#include "hybrid_coordinator.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <stdexcept>

#include "Global.h"
#include "PbSolver.h"

#ifdef USE_SCIP
// Defined and published by CASH's ScipSolver.cc when SCIP solves the instance
// to proven optimality. Declared here rather than through a header because the
// rest of CASH has no reason to expose it.
extern std::atomic<bool> opt_scip_proved_optimal;
#endif

namespace hybridmaxsat {

namespace {

// Whether CASH's ILP component has already proved the instance optimal. Never
// true in a build without SCIP, where no such component runs.
bool scip_proved_optimal_now()
{
#ifdef USE_SCIP
    return opt_scip_proved_optimal.load(std::memory_order_relaxed);
#else
    return false;
#endif
}

// Monotonic wall clock. Deliberately not cpuTime(): CASH's SCIP optimizer runs
// on a second thread, so process CPU time drifts away from wall time and would
// make the window protocol drift with it.
double wall_now()
{
    using Clock = std::chrono::steady_clock;
    return std::chrono::duration<double>(Clock::now().time_since_epoch()).count();
}

// Read from the thread running CASH's SAT call and written from the thread
// driving the schedule; a relaxed atomic is enough because each writer only
// ever shortens the deadline.
std::atomic<double> g_cash_deadline{std::numeric_limits<double>::infinity()};

long long to_log_value(const Int &value)
{
    return value == Int_MAX ? -1 : tolong(value);
}

// Longest backoff the adaptive policy may reach, as a power of two of
// the base CASH window: 4 means a 15-second base tops out at 240
// seconds between handoffs.
const int kMaxBackoffRounds = 4;

// One progress record every this many seconds of protocol time. A record per
// scheduling point would be far more than needed to see whether the gap is
// closing, and the points can be dense.
const double kProgressIntervalSeconds = 5.0;

int count_propagated(const std::vector<int> &partial_assignment)
{
    int count = 0;
    for (std::size_t variable = 1; variable < partial_assignment.size(); ++variable)
        if (partial_assignment[variable] == 0 || partial_assignment[variable] == 1)
            ++count;
    return count;
}

const char *bound_result_name(HybridBoundResult result)
{
    switch (result)
    {
    case HybridBoundResult::Accepted: return "accepted";
    case HybridBoundResult::NotImproved: return "not_improved";
    case HybridBoundResult::InvalidNegative: return "invalid_negative";
    case HybridBoundResult::InvalidUnitConversion: return "invalid_unit_conversion";
    case HybridBoundResult::InvalidBelowLowerBound: return "invalid_below_lb";
    }
    return "unknown";
}

std::string json_escape(const std::string &text)
{
    std::string result;
    result.reserve(text.size() + 8);
    for (char raw : text)
    {
        switch (raw)
        {
        case '"':  result += "\\\""; break;
        case '\\': result += "\\\\"; break;
        case '\n': result += "\\n";  break;
        case '\r': result += "\\r";  break;
        case '\t': result += "\\t";  break;
        default:   result += raw;    break;
        }
    }
    return result;
}

} // namespace

void set_cash_deadline(double absolute_wall_seconds)
{
    const double current = g_cash_deadline.load(std::memory_order_relaxed);
    if (absolute_wall_seconds < current)
        g_cash_deadline.store(absolute_wall_seconds, std::memory_order_relaxed);
}

// NOTE: measured not to bound a long call in practice. On
// MSE23W/hs-timetabling a 20-second solve polled this exactly once, and the
// same instance never observed the Cadical alarm `SimpSolver::limitTime`
// installs. Both mechanisms are therefore kept only as a best-effort way to
// make an *easy* instance stop through the normal path; the end-to-end budget
// is enforced by the runner's watchdog and the harness timeout, and the window
// protocol only actually interleaves on instances where CASH's own loop
// iterates between SAT calls. The scheduler reports the per-configuration round
// count so this is visible in the results rather than silently assumed.
std::atomic<long long> g_cash_deadline_polls{0};

int cash_deadline_reached(void *)
{
    g_cash_deadline_polls.fetch_add(1, std::memory_order_relaxed);
    return wall_now() >= g_cash_deadline.load(std::memory_order_relaxed) ? 1 : 0;
}

long long cash_deadline_polls()
{
    return g_cash_deadline_polls.load(std::memory_order_relaxed);
}

HybridCoordinator::HybridCoordinator(const std::string &wcnf_path,
                                     const HybridSchedule &schedule)
    : schedule_(schedule)
    , spb_solver_(wcnf_path, [&schedule]() {
          Settings settings;
          settings.cutoff_time = schedule.spb_window_seconds;
          // Each round runs improve_with_persistent_weights(), which goes
          // straight to the local-search worker and never consults the solution
          // pool (only Solver::Solve()'s generation search does). A pool of one
          // therefore only avoids allocating storage nothing reads.
          settings.solution_pool_size = 1;
          return settings;
      }())
{
    if (schedule_.cash_window_seconds <= 0 ||
        schedule_.spb_window_seconds <= 0 ||
        schedule_.total_budget_seconds <= 0)
        throw std::invalid_argument("hybrid schedule durations must be positive");

    // SPB uses the C random generator internally.  A new initial assignment
    // is still generated each round because -1 values are handed to improve.
    std::srand(schedule_.random_seed);
}

void HybridCoordinator::ensure_started()
{
    // Constructing this object loads SPB's copy of the instance, which for the
    // largest MSE instances takes a while; CASH then parses the same file
    // again. The window protocol must not spend its budget on that, so the
    // clock starts at the first scheduling point instead -- the first thing
    // maxsat_solve() does once it is actually solving.
    if (!started_)
    {
        started_ = true;
        instance_start_wall_ = wall_now();
        next_spb_wall_ = instance_start_wall_ + schedule_.cash_window_seconds;
        arm_deadline();
    }
}

void HybridCoordinator::arm_deadline()
{
    const double budget_deadline =
        instance_start_wall_ + schedule_.total_budget_seconds;
    // Under the preemptive policy the current SAT call is pulled out of its
    // search at the next window boundary so SPB can start on schedule.
    // Under the non-preemptive one the terminator only ever fires at the
    // end-to-end budget, and SPB waits for a scheduling point CASH reached
    // by itself. See HybridSchedule::preemptive for why both exist.
    set_cash_deadline(schedule_.preemptive
                          ? std::min(budget_deadline, next_spb_wall_)
                          : budget_deadline);
}

double HybridCoordinator::wall_elapsed() const
{
    return wall_now() - instance_start_wall_;
}

int (*HybridCoordinator::cash_terminator())(void *)
{
    // CASH returns to the scheduling point at least once per CASH window, so a
    // long SAT call cannot swallow the whole budget and starve SPB. The
    // deadline itself is maintained by ensure_started() and find_upper_bound().
    return &cash_deadline_reached;
}

bool HybridCoordinator::should_run(double)
{
    ensure_started();
    return wall_elapsed() < schedule_.total_budget_seconds &&
           wall_now() >= next_spb_wall_;
}

bool HybridCoordinator::should_stop(double)
{
    // CASH consults should_stop() before should_run() in every iteration, so
    // this is also where the protocol clock is started.
    ensure_started();
    // Once SCIP has proved the instance optimal there is nothing left to
    // improve, and the schedule would otherwise keep handing CASH windows to
    // SPB for the rest of the budget. Stopping here is what makes "CASH proved
    // the optimum, so the instance ends" true for the ILP path too.
    if (scip_proved_optimal_now())
        return true;
    // The coordinator's own budget is a graceful stop; the run is ultimately
    // bounded by the harness. CASH ignores this while inside a SAT call, so it
    // is only a way to end a run that happens to reach a scheduling point.
    return wall_elapsed() >= schedule_.total_budget_seconds;
}

bool HybridCoordinator::find_upper_bound(
    const std::vector<int> &cash_assignment,
    const Int &cash_lower_bound,
    const Int &current_upper_bound,
    Int &candidate_upper_bound)
{
    RoundEvent event;
    event.round = ++round_;
    event.cash_seconds_before_spb = wall_elapsed();
    event.cash_lb_before_spb = to_log_value(cash_lower_bound);
    event.cash_ub_before_spb = to_log_value(current_upper_bound);

    // CASH's vector is sized by *its* variable count, which includes the
    // relaxation and sorter variables it added while converting the objective;
    // SPB parsed the original WCNF on its own and only knows the original
    // variables. SPB rejects a vector of the wrong length outright, so the
    // assignment is cut down to the original variables here. Both number them
    // in WCNF order starting at 1 (element 0 is unused), so the prefix is the
    // shared part and the trailing CASH-only variables are dropped.
    const std::size_t spb_size =
        static_cast<std::size_t>(spb_solver_.num_vars()) + 1;
    if (cash_assignment.size() < spb_size)
        throw std::runtime_error(
            "CASH reported " + std::to_string(cash_assignment.size() - 1) +
            " variables, fewer than the " +
            std::to_string(spb_size - 1) + " SPB parsed from the instance");

    std::vector<int> partial_assignment(cash_assignment.begin(),
                                        cash_assignment.begin() + spb_size);

    // In the NoInference ablation the CASH-propagated values are discarded and
    // SPB performs its own random completion for every variable. The event then
    // reports zero propagated variables, which is what SPB actually received.
    std::vector<int> random_assignment;
    const std::vector<int> *initial_assignment = &partial_assignment;
    if (schedule_.no_inference)
    {
        random_assignment.assign(partial_assignment.size(), -1);
        initial_assignment = &random_assignment;
    }
    event.propagated_original_variables = count_propagated(*initial_assignment);

    // Give SPB only what is left of the end-to-end budget, so the last round
    // cannot push the run past its own deadline and hand the record to the
    // watchdog. Every other round uses the configured window.
    const double remaining =
        schedule_.total_budget_seconds - wall_elapsed();
    spb_solver_.set_cutoff_time(
        std::min<double>(schedule_.spb_window_seconds, remaining));

    const double spb_call_start = wall_now();
    const Solution result =
        spb_solver_.improve_with_persistent_weights(*initial_assignment);
    const double spb_call_end = wall_now();
    event.spb_steps = spb_solver_.last_step_count();
    event.spb_init_seconds = spb_solver_.last_init_seconds();
    event.spb_tries = spb_solver_.last_tries();
    event.spb_search_seconds = spb_solver_.last_search_seconds();
    event.spb_setup_seconds = spb_solver_.last_setup_seconds();
    event.spb_verify_seconds = spb_solver_.last_verify_seconds();
    event.spb_call_seconds = spb_call_end - spb_call_start;
    last_round_end_wall_ = spb_call_end;
    next_spb_wall_ = last_round_end_wall_ + schedule_.cash_window_seconds;
    arm_deadline();

    // A round CASH never hears about cannot have changed its bounds, so the
    // after-values are the before-values. Leaving them unset would read as
    // unknown in the log and look like missing data.
    event.cash_lb_after_spb = event.cash_lb_before_spb;
    event.cash_ub_after_spb = event.cash_ub_before_spb;

    if (!result.feasible)
    {
        // CASH does not call on_upper_bound_result() for a rejected candidate,
        // so this round has to be flushed here or it would be lost.
        note_round_outcome(false);
        events_.push_back(event);
        flush_pending_events();
        return false;
    }

    // improve_with_persistent_weights() independently re-evaluates the model
    // using the original WCNF before returning this result.
    event.spb_ub = result.cost;
    {
        // The watchdog may ask for the certificate from its own thread
        // at any moment, including this one.
        std::lock_guard<std::mutex> guard(certificate_mutex_);
        if (!best_spb_certificate_.feasible ||
            result.cost < best_spb_certificate_.cost)
            best_spb_certificate_ = result;
    }

    events_.push_back(event);
    if (Int((int64_t)result.cost) >= current_upper_bound)
    {
        // A feasible model that does not beat the bound CASH already
        // holds costs exactly as much as no model at all: the window was
        // spent and nothing improved. CASH never reports this case back
        // to the callback, so it is folded in here.
        note_round_outcome(false);
        return false;
    }

    candidate_upper_bound = Int((int64_t)result.cost);
    return true;
}

void HybridCoordinator::note_round_outcome(bool productive)
{
    if (!schedule_.adaptive)
        return;

    if (productive)
        barren_rounds_ = 0;
    else if (barren_rounds_ < kMaxBackoffRounds)
        ++barren_rounds_;

    // Back off geometrically while SPB keeps returning nothing CASH can
    // use, so an unproductive handoff stops taking a fixed slice out of
    // every cycle; one accepted bound resets the window to its base.
    const double factor = std::pow(2.0, static_cast<double>(barren_rounds_));
    next_spb_wall_ =
        last_round_end_wall_ + schedule_.cash_window_seconds * factor;
}

void HybridCoordinator::on_scheduling_point(const Int &cash_lower_bound,
                                            const Int &cash_upper_bound)
{
    const double elapsed = started_ ? wall_elapsed() : 0.0;
    if (elapsed - last_progress_wall_ < kProgressIntervalSeconds)
        return;
    last_progress_wall_ = elapsed;

    if (!event_log_.is_open())
        return;

    event_log_ << "{\"event\":\"progress\",\"instance\":\""
               << json_escape(event_log_instance_id_)
               << "\",\"t\":" << elapsed
               << ",\"lb\":" << to_log_value(cash_lower_bound)
               << ",\"ub\":" << to_log_value(cash_upper_bound)
               << ",\"round\":" << round_ << "}\n";
    event_log_.flush();
}

void HybridCoordinator::on_upper_bound_result(HybridBoundResult result,
                                              const Int &cash_lower_bound)
{
    note_round_outcome(result == HybridBoundResult::Accepted);
    if (!events_.empty())
    {
        events_.back().cash_bound_result = result;
        events_.back().cash_lb_after_spb = to_log_value(cash_lower_bound);
        events_.back().cash_ub_after_spb =
            result == HybridBoundResult::Accepted ? events_.back().spb_ub :
            events_.back().cash_ub_before_spb;
    }
    flush_pending_events();
}

void HybridCoordinator::set_event_log(const std::string &path,
                                      const std::string &instance_id)
{
    event_log_.open(path.c_str());
    if (!event_log_)
        throw std::runtime_error("cannot open JSONL output file: " + path);
    event_log_instance_id_ = instance_id;
    written_events_ = 0;
}

void HybridCoordinator::flush_pending_events()
{
    if (!event_log_.is_open())
        return;

    const std::string instance = json_escape(event_log_instance_id_);
    for (; written_events_ < events_.size(); ++written_events_)
    {
        const RoundEvent &event = events_[written_events_];
        event_log_ << "{\"event\":\"spb_round\",\"instance\":\"" << instance
                   << "\",\"round\":" << event.round
                   << ",\"cash_seconds_before_spb\":"
                   << event.cash_seconds_before_spb
                   << ",\"propagated_original_variables\":"
                   << event.propagated_original_variables
                   << ",\"cash_lb_before_spb\":" << event.cash_lb_before_spb
                   << ",\"cash_ub_before_spb\":" << event.cash_ub_before_spb
                   << ",\"spb_ub\":" << event.spb_ub
                   << ",\"cash_lb_after_spb\":" << event.cash_lb_after_spb
                   << ",\"cash_ub_after_spb\":" << event.cash_ub_after_spb
                   << ",\"cash_bound_result\":\""
                   << bound_result_name(event.cash_bound_result) << "\""
                   << ",\"spb_steps\":" << event.spb_steps
                   << ",\"spb_tries\":" << event.spb_tries
                   << ",\"spb_init_seconds\":" << event.spb_init_seconds
                   << ",\"spb_call_seconds\":" << event.spb_call_seconds
                   << ",\"spb_search_seconds\":" << event.spb_search_seconds
                   << ",\"spb_setup_seconds\":" << event.spb_setup_seconds
                   << ",\"spb_verify_seconds\":" << event.spb_verify_seconds
                   << "}\n";
    }
    event_log_.flush();
}

const std::vector<RoundEvent> &HybridCoordinator::round_events() const
{
    return events_;
}

const Solution &HybridCoordinator::best_spb_certificate() const
{
    return best_spb_certificate_;
}

bool HybridCoordinator::has_spb_certificate() const
{
    return best_spb_certificate_.feasible;
}

bool HybridCoordinator::certificate_snapshot(long long &cost) const
{
    std::lock_guard<std::mutex> guard(certificate_mutex_);
    if (!best_spb_certificate_.feasible)
        return false;
    cost = static_cast<long long>(best_spb_certificate_.cost);
    return true;
}

} // namespace hybridmaxsat
