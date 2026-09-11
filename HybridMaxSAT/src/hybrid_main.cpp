#include "hybrid_coordinator.h"

#include <atomic>
#include <chrono>
#include <climits>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <sstream>
#include <thread>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unistd.h>
#include <vector>

#include "Global.h"
#include "Main.h"
#include "Main_utils.h"
#include "PbParser.h"
#include "Solver/local_search_solver.h"

#ifdef USE_SCIP
// Defined in CASH's Main_utils.cc; declared here because CASH only exposes it
// to its own option parser.
extern double opt_scip_cpu;
// Defined in CASH's Main_utils.cc and ScipSolver.cc respectively. See
// apply_maxsat_options() for why the embedder has to set the first one, and
// fill_from_solver() for what the second one reports.
extern bool opt_embedded_runner;
extern std::atomic<bool> opt_scip_proved_optimal;
#endif

namespace {

// ---------------------------------------------------------------------------
// Configuration

enum class Config { CASH, SPB, Hybrid, HybridNoInference, HybridNatural,
                    HybridAdaptive };

const char *config_name(Config config)
{
    switch (config)
    {
    case Config::CASH: return "CASH";
    case Config::SPB: return "SPB";
    case Config::Hybrid: return "Hybrid";
    case Config::HybridNoInference: return "HybridNoInference";
    case Config::HybridNatural: return "HybridNatural";
    case Config::HybridAdaptive: return "HybridAdaptive";
    }
    return "unknown";
}

bool parse_config(const std::string &text, Config &config)
{
    if (text == "CASH") { config = Config::CASH; return true; }
    if (text == "SPB") { config = Config::SPB; return true; }
    if (text == "Hybrid") { config = Config::Hybrid; return true; }
    if (text == "HybridNoInference")
    {
        config = Config::HybridNoInference;
        return true;
    }
    if (text == "HybridNatural")
    {
        config = Config::HybridNatural;
        return true;
    }
    if (text == "HybridAdaptive")
    {
        config = Config::HybridAdaptive;
        return true;
    }
    return false;
}

// The CASH/SPB interleaving windows. 15/3 is the protocol the experiment
// measures; the defaults are overridable from the command line so the handoff
// can also be exercised on demand without first waiting out a full window.
const int kCashWindowSeconds = 15;
const int kSpbWindowSeconds = 3;

struct Options {
    Config config = Config::Hybrid;
    unsigned int seed = 20260909U;
    int budget_seconds = 600;
    int cash_window_seconds = kCashWindowSeconds;
    int spb_window_seconds = kSpbWindowSeconds;
    // Seconds CASH's SCIP component may run before CASH's own CDCL loop gets a
    // turn. Zero keeps CASH's default, which is no limit at all.
    double scip_seconds = 0.0;
    std::string instance_id;
    std::string input_path;
    std::string events_path;
};

// ---------------------------------------------------------------------------
// Output helpers

std::string int_to_string(const Int &value)
{
    char *text = toString(value);
    const std::string result(text);
    xfree(text);
    return result;
}

// Int_MAX is Int's own infinity, and toString() renders it "+oo". As a *lower*
// bound that reads backwards (an infinite lower bound), and for a configuration
// that simply does not compute the bound it is not true at all -- the SPB-only
// baseline never derives a lower bound, so it has 0 as its trivial bound and
// nothing more. Records therefore use "n/a" for "this configuration did not
// produce this value", which is distinct from any numeric bound.
std::string bound_text(const Int &value)
{
    return value == Int_MAX ? std::string("n/a") : int_to_string(value);
}

// Minimal JSON string escaping for the fields we emit (paths on this
// filesystem contain neither quotes nor control characters, but the record is
// meant to be machine-parsed, so do not rely on that).
// Same monotonic clock the coordinator uses, so a deadline computed here is
// directly comparable with the one `cash_deadline_reached` tests.
double steady_now()
{
    return std::chrono::duration<double>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
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

// Everything the experiment record needs beyond the per-round events.
struct RunSummary {
    bool cash_proved_optimal = false;
    // The optimality proof came from CASH's ILP component rather than from its
    // CDCL search. Still a proof, so `cash_proved_optimal` is also true; this
    // field records which component produced it, because the two say different
    // things about the hybrid protocol (a SCIP proof in the first seconds means
    // the protocol never got to interleave at all).
    bool scip_proved_optimal = false;
    Int final_lb = Int_MAX;
    Int final_ub = Int_MAX;
    Int final_incumbent = Int_MAX;
    bool has_spb_certificate = false;
    long long spb_certificate_ub = -1;
    // Cadical consultations of the deadline terminator. See the header.
    long long cash_deadline_polls = 0;
    // The SCIP limit actually applied, which may be derived from the window.
    double scip_seconds = 0.0;
    std::string exit_reason = "completed";
};

// The two ways a run can end with an optimality proof are reported apart,
// because "CASH stopped after proving the optimum itself" and "SCIP answered
// the instance before the protocol could interleave" are different findings,
// even though both mean the instance is solved.
const char *optimal_exit_reason(const RunSummary &summary)
{
    return summary.scip_proved_optimal ? "optimum_scip" : "optimum";
}

void fill_from_solver(MsSolver &solver, RunSummary &summary)
{
    summary.final_lb = solver.LB_goalvalue;
    summary.final_ub = solver.UB_goalvalue;
    summary.final_incumbent = solver.best_goalvalue;
    summary.cash_proved_optimal =
        !solver.asynch_interrupt &&
        solver.best_goalvalue != Int_MAX &&
        solver.LB_goalvalue == solver.best_goalvalue;
#ifdef USE_SCIP
    summary.scip_proved_optimal = opt_scip_proved_optimal.load();
    if (summary.scip_proved_optimal && solver.best_goalvalue != Int_MAX)
    {
        // SCIP solved the ILP to proven optimality, so its dual bound equals
        // its primal bound: the optimum is known on both sides, not merely
        // attained. CASH's own LB_goalvalue is not updated by that path and
        // would otherwise be reported alongside an optimum, which reads as an
        // unbounded gap that the run did not actually have.
        summary.cash_proved_optimal = true;
        summary.final_lb = solver.best_goalvalue;
        summary.final_ub = solver.best_goalvalue;
        summary.final_incumbent = solver.best_goalvalue;
    }
#endif
    summary.cash_deadline_polls = hybridmaxsat::cash_deadline_polls();
}

std::string build_run_complete_line(const Options &options,
                                    const RunSummary &summary,
                                    long long start_epoch,
                                    long long end_epoch,
                                    std::string exit_reason)
{
    std::ostringstream line;
    line << "{\"event\":\"run_complete\""
         << ",\"instance\":\"" << json_escape(options.instance_id) << "\""
         << ",\"config\":\"" << config_name(options.config) << "\""
         << ",\"seed\":" << options.seed
         << ",\"budget_seconds\":" << options.budget_seconds
         << ",\"start_epoch\":" << start_epoch
         << ",\"end_epoch\":" << end_epoch
         << ",\"wall_seconds\":" << (end_epoch - start_epoch)
         << ",\"cash_proved_optimal\":"
         << (summary.cash_proved_optimal ? "true" : "false")
         << ",\"scip_proved_optimal\":"
         << (summary.scip_proved_optimal ? "true" : "false")
         << ",\"final_lb\":\"" << bound_text(summary.final_lb) << "\""
         << ",\"final_ub\":\"" << bound_text(summary.final_ub) << "\""
         << ",\"final_incumbent\":\""
         << bound_text(summary.final_incumbent) << "\""
         << ",\"spb_certificate_ub\":"
         << (summary.has_spb_certificate ?
             std::to_string(summary.spb_certificate_ub) : "null")
         << ",\"cash_deadline_polls\":" << summary.cash_deadline_polls
         << ",\"cash_window_seconds\":" << options.cash_window_seconds
         << ",\"spb_window_seconds\":" << options.spb_window_seconds
         << ",\"scip_seconds\":" << summary.scip_seconds
         << ",\"exit_reason\":\"" << exit_reason << "\"}";
    return line.str();
}

// Exactly one `run_complete` per run. The normal path and the watchdog both
// race to publish; the atomic exchange decides the single winner, so a run
// stopped at its deadline cannot also emit the record the solver was about to
// write, and vice versa.
std::atomic<bool> g_published{false};
std::atomic<bool> g_finished{false};

bool publish_record(const std::string &line)
{
    if (g_published.exchange(true))
        return false;
    // CASH's reportf() flushes after every call, but std::cout output from this
    // file may still be buffered; flush before appending the record so the two
    // writers do not interleave mid-line.
    std::cout.flush();
    const std::string terminated = line + "\n";
    const ssize_t ignored =
        ::write(STDOUT_FILENO, terminated.data(), terminated.size());
    (void)ignored;
    return true;
}

void write_run_complete(const Options &options,
                        const RunSummary &summary,
                        long long start_epoch,
                        long long end_epoch,
                        const std::string &exit_reason)
{
    publish_record(build_run_complete_line(
        options, summary, start_epoch, end_epoch, exit_reason));
}

// ---------------------------------------------------------------------------
// Budget enforcement and the run record
//
// Three independent mechanisms, in order of preference:
//
//  1. The CaDiCaL terminator (`hybridmaxsat::set_cash_deadline`), which makes
//     CASH return from its SAT call at the deadline so the run ends through the
//     normal path with the solver's real bounds.
//  2. The watchdog below, which guarantees a record even if the solver never
//     yields.
//  3. SIGTERM from the harness, for a process that is wedged beyond both.
//
// Every path writes its record through publish_record(), so a run produces
// exactly one `run_complete` line whichever way it ends. A run stopped at its
// budget is a completed run; the distinction is carried by `exit_reason`, not
// by the process status.

Options g_options;
RunSummary g_summary;
long long g_start_epoch = 0;

// The SCIP limit that was actually applied, published by
// apply_scip_limit() so the watchdog can report it too. A watchdog
// record used to carry scip_seconds = 0, which is indistinguishable
// from "no limit was applied" and was the only reliable way to tell
// such a record apart from a normal one.
std::atomic<double> g_applied_scip_seconds{0.0};

// Enforces the end-to-end budget independently of the solver. CASH's own
// alarms were measured not to interrupt a running SAT call, so a run that the
// terminator cannot reach would otherwise outlive its budget with no record at
// all. This fires a little after the deadline so a graceful stop -- which
// carries the solver's real bounds -- wins whenever it can.
// Set by the hybrid configuration once its coordinator exists, so a
// run the watchdog has to end still reports the best SPB model the run
// produced. Both the normal path and the watchdog fill the record
// through backfill_spb_certificate(), so they cannot drift apart.
hybridmaxsat::HybridCoordinator *g_watchdog_coordinator = nullptr;

void backfill_spb_certificate(RunSummary &summary)
{
    if (g_watchdog_coordinator == nullptr)
        return;

    long long cost = -1;
    if (!g_watchdog_coordinator->certificate_snapshot(cost))
        return;
    summary.has_spb_certificate = true;
    summary.spb_certificate_ub = cost;
    // CASH can end a run without ever holding a model -- its SAT calls
    // are interrupted at each window boundary -- while SPB did find one.
    // The best feasible value the run produced is what the
    // cross-configuration comparison needs, so the certificate backfills
    // it. `cash_proved_optimal` is untouched: only CASH may claim that.
    const Int certificate(static_cast<int64_t>(summary.spb_certificate_ub));
    if (summary.final_ub == Int_MAX || certificate < summary.final_ub)
        summary.final_ub = certificate;
    if (summary.final_incumbent == Int_MAX)
        summary.final_incumbent = certificate;
}

void start_watchdog(int budget_seconds, int grace_seconds)
{
    std::thread([budget_seconds, grace_seconds]() {
        std::this_thread::sleep_for(
            std::chrono::seconds(budget_seconds + grace_seconds));
        if (g_finished.load())
            return;

        const long long end_epoch = static_cast<long long>(std::time(NULL));
        try
        {
            RunSummary summary;
            // Deliberately NOT reading pb_solver's bounds. They are
            // `Int`, an arbitrary-precision type the solver thread keeps
            // mutating, and reading one from here produced a 424248-digit
            // lower bound in runs/iter1_debug50. The signal path already
            // refuses the same read for the same reason. The only bound
            // this path reports is the SPB certificate below, which is a
            // plain integer copied out under a lock.
            summary.scip_seconds = g_applied_scip_seconds.load();
            summary.cash_deadline_polls = hybridmaxsat::cash_deadline_polls();
            backfill_spb_certificate(summary);
            // A reason of its own, not "budget": reaching here means the
            // run outlived its budget plus the work the normal path may
            // still have been finishing, which is a finding about the
            // protocol rather than a run that stopped on schedule.
            publish_record(build_run_complete_line(
                g_options, summary, g_start_epoch, end_epoch,
                "budget_watchdog"));
        }
        catch (...)
        {
            // Deliberately not read from a signal handler, but here throwing is
            // allowed -- CASH's bounds are arbitrary precision, so converting
            // one to a decimal string can fail. A missing record would be worse
            // than a record without bounds, so fall back to the latter.
            publish_record(build_run_complete_line(
                g_options, RunSummary(), g_start_epoch, end_epoch,
                "budget_watchdog_no_bounds"));
        }
        _Exit(0);
    }).detach();
}

// Whether CASH's ILP component has already proved the instance optimal. The
// signal path reports both proof flags from this one value, because on that
// path a SCIP proof is the only proof there can be: it is published before the
// run can be interrupted, whereas CASH's own bound comparison is only made
// after `maxsat_solve()` returns. A relaxed load of a lock-free bool is a
// single read -- no allocation and no throw, unlike the `Int` conversions the
// handler below has to avoid.
bool scip_proved_optimal_now()
{
#ifdef USE_SCIP
    return opt_scip_proved_optimal.load(std::memory_order_relaxed);
#else
    return false;
#endif
}

// The bounds are deliberately reported as "n/a" here rather than read from
// `pb_solver`. CASH stores them as `Int`, a bignum type whose `tolong()` both
// allocates (`xstrdup`) and *throws* `Exception_IntOverflow` when the value
// does not fit an int64 -- neither of which is legal in a signal handler, and
// the throw would escape into `terminate()`, aborting the process before the
// record is written at all. Since this path only runs when the graceful stop
// could not be reached, "unknown" is the honest answer.
void emit_signal_record()
{
    // A run that finished normally already wrote its record; returning without
    // writing a second one keeps "one run, one record" true.
    if (g_published.exchange(true))
        return;

    // Only async-signal-safe operations from here on: format into a local
    // buffer and hand it to write() directly.
    char buffer[1024];
    const long long end_epoch = static_cast<long long>(std::time(NULL));
    const int written = std::snprintf(
        buffer, sizeof(buffer),
        "{\"event\":\"run_complete\",\"instance\":\"%s\",\"config\":\"%s\""
        ",\"seed\":%u,\"budget_seconds\":%d,\"start_epoch\":%lld"
        ",\"end_epoch\":%lld,\"wall_seconds\":%lld,\"cash_proved_optimal\":%s"
        ",\"scip_proved_optimal\":%s"
        ",\"final_lb\":\"n/a\",\"final_ub\":\"n/a\",\"final_incumbent\":\"n/a\""
        ",\"spb_certificate_ub\":null,\"exit_reason\":\"budget_signal\"}\n",
        g_options.instance_id.c_str(), config_name(g_options.config),
        g_options.seed, g_options.budget_seconds, g_start_epoch, end_epoch,
        end_epoch - g_start_epoch,
        scip_proved_optimal_now() ? "true" : "false",
        scip_proved_optimal_now() ? "true" : "false");

    if (written > 0)
    {
        const size_t length = static_cast<size_t>(written) < sizeof(buffer)
            ? static_cast<size_t>(written) : sizeof(buffer) - 1;
        const ssize_t ignored = ::write(STDOUT_FILENO, buffer, length);
        (void)ignored;
    }
    _Exit(0);
}

void on_budget_signal(int)
{
    emit_signal_record();
}

// ---------------------------------------------------------------------------
// Shared setup

// CASH hands the instance to its SCIP component before its CDCL loop starts,
// and that first call runs *synchronously* with no time limit of its own
// (`ScipSolver.cc`: `if (opt_scip_cpu > 0) SCIPsetRealParam(scip, "limits/time",
// opt_scip_cpu)`). On the larger MSE instances SCIP neither finishes presolving
// nor is interrupted, so the CDCL loop -- and with it every callback the hybrid
// protocol hangs off -- is never reached at all. Bounding SCIP is what gives the
// CASH component a turn in the first place.
double apply_scip_limit(double seconds)
{
#ifdef USE_SCIP
    opt_scip_cpu = seconds;
#endif
    g_applied_scip_seconds.store(seconds);
    return seconds;
}

// CASH reaches its persistent state through the global `pb_solver` pointer;
// Main.cc sets it the same way before parsing.
void prepare_cash_globals(MsSolver &solver)
{
    pb_solver = &solver;
    time(&wall_clock_time);
}

// Mirror the MaxSAT branch of Main.cc so every configuration solves the same
// transformed problem.
void apply_maxsat_options()
{
    opt_maxsat = true;
    opt_satlive = false;
    opt_model_out = false;
    opt_satisfiable_out = false;
    opt_maxsat_msu = true;
    opt_minimization = 1;
    opt_seq_thres = 4;
    opt_convert = ct_Sorters;
    opt_reuse_sorters = false;
#ifdef USE_SCIP
    // CASH's standalone CLI ends a solved run by calling std::_Exit(20/30) from
    // whichever thread noticed, which is uncatchable and therefore also skips
    // the record this runner still has to write. On MSE23W that fires whenever
    // SCIP proves the instance optimal within its time limit -- in the first
    // 50-instance debug sample it hit 2 instances across every CASH-based
    // configuration, always at about 5 seconds. Embedded mode makes CASH return
    // from the solve call instead, so the run ends through the normal path with
    // opt_scip_proved_optimal set. Runs that do not set it are indistinguishable
    // from non-embedded ones.
    opt_embedded_runner = true;
#endif
}

// ---------------------------------------------------------------------------
// CASH-only baseline

// The solver and the end-to-end stopping rule are exactly those of the CASH
// window inside the hybrid protocol -- the only difference is that SPB is never
// invoked.
//
// The budget is NOT enforced through CASH's `-cpu-lim`. That flag arms
// RLIMIT_CPU at only a *quarter* of the budget (MsSolver.cc: `limitTime(
// start_solving_cpu + (opt_cpu_lim - start_solving_cpu)/4)`) expecting the
// solver loop to absorb that first signal and raise the limit. The absorb is
// guarded by a one-shot `first_time` flag, which `opt_scip_delay = 0` burns on
// a much earlier 1-second Cadical alarm, so on hard instances the quarter limit
// lands unabsorbed and CASH stops at roughly budget/4. Measured on
// MSE23W/hs-timetabling: `-cpu-lim=45` stops at 23s wall. It is also CPU time,
// not wall time, and CASH's SCIP thread makes the two diverge.
//
// The budget is therefore wall-clock and identical for all four configurations:
// the callback stops the run at the deadline, and the harness (scripts/
// run_batch.py) enforces the same deadline as a hard backstop for the case
// where CASH never returns to a scheduling point.
class CashOnlyCallback final : public HybridMaxSatCallback {
  public:
    CashOnlyCallback(int budget_seconds, const std::string &events_path,
                     const std::string &instance_id)
        : budget_seconds_(budget_seconds)
        , instance_id_(instance_id)
        , start_wall_(std::chrono::steady_clock::now())
    {
        event_log_.open(events_path.c_str());
    }

    // The deadline is the whole budget, never a window: each SAT call runs
    // until the run is over, so nothing restarts Cadical mid-search the way the
    // hybrid's 15-second handoff does. This baseline must be CASH exactly as it
    // ships, but it still returns at the deadline so the run ends through the
    // normal path and its record carries the solver's real bounds.
    int (*cash_terminator())(void *) override
    {
        return &hybridmaxsat::cash_deadline_reached;
    }

    bool should_run(double) override { return false; }

    bool should_stop(double) override
    {
        // As in the hybrid coordinator: a SCIP proof ends the instance, rather
        // than leaving CASH to re-derive it until the budget runs out.
        if (scip_proved_optimal_now())
            return true;
        return std::chrono::duration<double>(
            std::chrono::steady_clock::now() - start_wall_).count() >=
            budget_seconds_;
    }

    bool find_upper_bound(const std::vector<int> &, const Int &, const Int &,
                          Int &) override
    {
        return false;
    }

    // The baseline never hands off, but it still publishes its bounds at every
    // scheduling point, so its LB/UB trajectory can be compared with the hybrid
    // run on the same instance under the same clock.
    void on_scheduling_point(const Int &lower_bound,
                             const Int &upper_bound) override
    {
        if (!event_log_.is_open())
            return;

        const double elapsed = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - start_wall_).count();
        if (wrote_progress_ && elapsed - last_progress_ < 5.0)
            return;
        last_progress_ = elapsed;
        wrote_progress_ = true;

        event_log_ << "{\"event\":\"progress\",\"instance\":\""
                   << json_escape(instance_id_)
                   << "\",\"t\":" << elapsed
                   << ",\"lb\":" << (lower_bound == Int_MAX ? -1 : tolong(lower_bound))
                   << ",\"ub\":" << (upper_bound == Int_MAX ? -1 : tolong(upper_bound))
                   << ",\"round\":0}\n";
        event_log_.flush();
    }

  private:
    int budget_seconds_;
    std::string instance_id_;
    std::ofstream event_log_;
    double last_progress_ = 0.0;
    bool wrote_progress_ = false;
    std::chrono::steady_clock::time_point start_wall_;
};

void run_cash(const Options &options, RunSummary &summary)
{
    // This baseline is the hybrid protocol's CASH window with SPB removed, so
    // it gets the same SCIP bound as the hybrid's CASH component. That is what
    // makes the four configurations an ablation rather than four unrelated
    // solvers: CASH and Hybrid then differ only by the SPB rounds. Leaving SCIP
    // unlimited instead would be a different baseline -- on the larger MSE
    // instances SCIP alone then consumes the whole budget and CASH's own CDCL
    // loop, the component this work is about, never runs.
    summary.scip_seconds = apply_scip_limit(
        options.scip_seconds > 0 ? options.scip_seconds
                                 : options.cash_window_seconds);

    MsSolver cash_solver(false, opt_preprocess);
    prepare_cash_globals(cash_solver);

    // The budget covers parsing as well, matching the coordinator's clock.
    hybridmaxsat::set_cash_deadline(steady_now() + options.budget_seconds);
    CashOnlyCallback callback(options.budget_seconds, options.events_path,
                              options.instance_id);
    cash_solver.set_hybrid_callback(&callback);

    parse_WCNF_file(const_cast<char *>(options.input_path.c_str()), cash_solver);
    cash_solver.maxsat_solve(PbSolver::sc_Minimize);

    fill_from_solver(cash_solver, summary);
    summary.exit_reason = summary.cash_proved_optimal
                              ? optimal_exit_reason(summary)
                              : "budget";
}

// ---------------------------------------------------------------------------
// SPB-only baseline

void run_spb(const Options &options, RunSummary &summary)
{
    // SPB's local search draws from the C global generator, so seeding it here
    // makes an otherwise non-deterministic pipeline reproducible.
    std::srand(options.seed);

    Settings settings;
    settings.cutoff_time = options.budget_seconds;
    // Keep the default pool size (15). `Solver::Solve()` bails out of the
    // generation-based search unless the pool holds at least two solutions, so
    // a smaller pool silently reduces this baseline to the initial local search
    // and skips crossover altogether.

    spbmaxsat::LocalSearchSolver spb(options.input_path, settings);
    const Solution result = spb.solve_with_crossover();

    if (result.feasible)
    {
        summary.has_spb_certificate = true;
        summary.spb_certificate_ub = result.cost;
        // A local-search upper bound is never an optimality proof; CASH's exact
        // reasoning is the only component allowed to make that claim.
        summary.final_ub = Int((int64_t)result.cost);
        // The certificate *is* this run's incumbent: it is the model the run
        // would hand out. `final_lb` stays "n/a" -- local search proves nothing
        // on the lower side, and 0 would be the trivial bound, not a result.
        summary.final_incumbent = summary.final_ub;
    }
    summary.cash_proved_optimal = false;
    summary.exit_reason = "budget";
}

// ---------------------------------------------------------------------------
// Hybrid and Hybrid-NoInference

void run_hybrid(const Options &options, RunSummary &summary)
{
    hybridmaxsat::HybridSchedule schedule;
    schedule.cash_window_seconds = options.cash_window_seconds;
    schedule.spb_window_seconds = options.spb_window_seconds;
    schedule.total_budget_seconds = options.budget_seconds;
    schedule.random_seed = options.seed;
    schedule.no_inference = options.config == Config::HybridNoInference;
    // HybridNatural is the same protocol without the forced interrupt:
    // everything else about the schedule is identical, which is what
    // makes the pair an ablation of the preemption rather than of the
    // protocol.
    schedule.preemptive = options.config != Config::HybridNatural &&
                          options.config != Config::HybridAdaptive;
    // HybridAdaptive is HybridNatural plus the progress-based window.
    schedule.adaptive = options.config == Config::HybridAdaptive;

    // A CASH window is cash_window_seconds long, so SCIP may not overrun it:
    // that is the whole point of the handoff. The default (0) means the SCIP
    // call would run until it solved the instance or the budget killed it, and
    // SPB would never be asked for a bound.
    summary.scip_seconds = apply_scip_limit(
        options.scip_seconds > 0 ? options.scip_seconds
                                 : options.cash_window_seconds);

    MsSolver cash_solver(false, opt_preprocess);
    prepare_cash_globals(cash_solver);

    hybridmaxsat::HybridCoordinator coordinator(options.input_path, schedule);
    coordinator.set_event_log(options.events_path, options.instance_id);
    cash_solver.set_hybrid_callback(&coordinator);
    // Lets the watchdog report the same SPB certificate this path does.
    g_watchdog_coordinator = &coordinator;

    parse_WCNF_file(const_cast<char *>(options.input_path.c_str()), cash_solver);
    cash_solver.maxsat_solve(PbSolver::sc_Minimize);

    // Rounds are flushed as they complete; this catches any trailing event.
    coordinator.flush_pending_events();

    fill_from_solver(cash_solver, summary);
    backfill_spb_certificate(summary);
    summary.exit_reason = summary.cash_proved_optimal
                              ? optimal_exit_reason(summary)
                              : "budget";
}

// ---------------------------------------------------------------------------

Options parse_options(int argc, char *argv[])
{
    Options options;
    std::vector<std::string> positional;

    for (int i = 1; i < argc; ++i)
    {
        const std::string argument(argv[i]);
        std::string name = argument;
        std::string value;
        bool has_inline_value = false;

        const std::size_t equals = argument.find('=');
        if (argument.compare(0, 2, "--") == 0 && equals != std::string::npos)
        {
            name = argument.substr(0, equals);
            value = argument.substr(equals + 1);
            has_inline_value = true;
        }

        const bool is_flag = name.compare(0, 2, "--") == 0;
        if (!is_flag)
        {
            positional.push_back(argument);
            continue;
        }

        if (!has_inline_value)
        {
            if (i + 1 >= argc)
                throw std::invalid_argument("missing value for " + name);
            value = argv[++i];
        }

        if (name == "--config")
        {
            if (!parse_config(value, options.config))
                throw std::invalid_argument("unknown config: " + value);
        }
        else if (name == "--seed")
        {
            options.seed = static_cast<unsigned int>(std::stoul(value));
        }
        else if (name == "--budget")
        {
            options.budget_seconds = std::stoi(value);
            if (options.budget_seconds <= 0)
                throw std::invalid_argument("--budget must be positive");
        }
        else if (name == "--cash-window")
        {
            options.cash_window_seconds = std::stoi(value);
            if (options.cash_window_seconds <= 0)
                throw std::invalid_argument("--cash-window must be positive");
        }
        else if (name == "--spb-window")
        {
            options.spb_window_seconds = std::stoi(value);
            if (options.spb_window_seconds <= 0)
                throw std::invalid_argument("--spb-window must be positive");
        }
        else if (name == "--scip-cpu")
        {
            options.scip_seconds = std::stod(value);
            if (options.scip_seconds < 0)
                throw std::invalid_argument("--scip-cpu must not be negative");
        }
        else if (name == "--instance-id")
        {
            options.instance_id = value;
        }
        else
        {
            throw std::invalid_argument("unknown option: " + name);
        }
    }

    if (positional.size() != 2)
        throw std::invalid_argument(
            "expected <input.wcnf> <events.jsonl>, got " +
            std::to_string(positional.size()) + " positional argument(s)");

    options.input_path = positional[0];
    options.events_path = positional[1];
    if (options.instance_id.empty())
        options.instance_id = options.input_path;
    return options;
}

void print_usage()
{
    std::cerr << "usage: hybridmaxsat [--config CASH|SPB|Hybrid|HybridNoInference|HybridNatural|HybridAdaptive]\n"
              << "                    [--seed N] [--budget SECONDS]\n"
              << "                    [--cash-window SECONDS] [--spb-window SECONDS]\n"
              << "                    [--scip-cpu SECONDS]\n"
              << "                    [--instance-id RELATIVE_PATH]\n"
              << "                    <input.wcnf> <events.jsonl>" << std::endl;
}

} // namespace

int main(int argc, char *argv[])
{
    try
    {
        g_options = parse_options(argc, argv);
    }
    catch (const std::exception &error)
    {
        std::cerr << "hybridmaxsat: " << error.what() << std::endl;
        print_usage();
        return 2;
    }

    g_start_epoch = static_cast<long long>(std::time(NULL));

    try
    {
        apply_maxsat_options();

        // Last-resort backstop, for a process the watchdog cannot reach. It
        // may be replaced by CASH's own CaDiCaL signal handler, which re-raises
        // the signal with the default disposition, so nothing depends on it.
        std::signal(SIGTERM, on_budget_signal);

        // The watchdog is a backstop for a process that outlives its
        // budget. Its grace only has to cover what the normal path may
        // still be finishing at the deadline -- at most one SPB window,
        // the longest step that cannot be cut short -- and it has to stay
        // below the harness's own grace (run_batch.py HARNESS_GRACE) so
        // the run still writes its own record instead of being signalled.
        const int watchdog_grace =
            g_options.config == Config::SPB ? 5
                                            : g_options.spb_window_seconds + 3;
        start_watchdog(g_options.budget_seconds, watchdog_grace);

        // The coordinator owns the round log for the hybrid configurations; the
        // other two still produce the file so that every run directory has the
        // same shape.
        if (g_options.config == Config::CASH || g_options.config == Config::SPB)
        {
            std::ofstream empty(g_options.events_path.c_str());
            if (!empty)
                throw std::runtime_error(
                    "cannot open JSONL output file: " + g_options.events_path);
        }

        switch (g_options.config)
        {
        case Config::CASH:
            run_cash(g_options, g_summary);
            break;
        case Config::SPB:
            run_spb(g_options, g_summary);
            break;
        case Config::Hybrid:
        case Config::HybridNoInference:
        case Config::HybridNatural:
        case Config::HybridAdaptive:
            run_hybrid(g_options, g_summary);
            break;
        }

        // This run finished on its own; the watchdog must not also publish.
        g_finished = true;

        const long long end_epoch = static_cast<long long>(std::time(NULL));
        write_run_complete(g_options, g_summary, g_start_epoch, end_epoch,
                           g_summary.exit_reason);
    }
    catch (const std::exception &error)
    {
        std::cerr << "hybridmaxsat error: " << error.what() << std::endl;
        return 1;
    }

    return 0;
}
