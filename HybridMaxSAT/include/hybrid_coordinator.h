#ifndef HYBRIDMAXSAT_COORDINATOR_H
#define HYBRIDMAXSAT_COORDINATOR_H

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

#include "MsSolver.h"
#include "Solver/local_search_solver.h"

namespace hybridmaxsat {

// Absolute wall-clock time at which CASH's current SAT call must return.
//
// CASH's own budget mechanism cannot be used for this. `-cpu-lim` arms
// RLIMIT_CPU at a quarter of the budget and expects the solver loop to absorb
// that first signal, which does not happen on hard instances; and the Cadical
// alarm that `SimpSolver::limitTime` installs was measured not to interrupt a
// running call at all. Without an external interrupt a single SAT call runs for
// the whole budget, so the coordinator is never consulted again and every
// configuration silently degenerates into "CASH only".
//
// Cadical polls its connected terminator regularly and returns as soon as it
// says yes, so the call comes back at the deadline with CASH's bounds intact
// instead of being killed from outside.
void set_cash_deadline(double absolute_wall_seconds);
// Terminator handed to Cadical. Returns non-zero once the deadline has passed.
int cash_deadline_reached(void *state);
// How many times Cadical consulted the terminator. Zero across a long run means
// the terminator is not being polled at all, which is the difference between "a
// single SAT call outran the window" and "the interrupt never reached the
// solver"; the two look identical in the round count alone.
long long cash_deadline_polls();

// Windows and budget are WALL-clock seconds. They are deliberately not the
// CPU time CASH passes into the callbacks: CASH runs its SCIP optimizer on a
// second thread, so process CPU time advances faster than wall time and a
// CPU-based window would silently shrink the protocol. The end-to-end budget
// must also match SPB's own cutoff (`Settings::cutoff_time`, a wall clock), or
// the four configurations would not be comparable.
struct HybridSchedule {
    int cash_window_seconds = 15;
    int spb_window_seconds = 3;
    int total_budget_seconds = 600;
    unsigned int random_seed = 20260909U;
    // Ablation switch: when true the CASH-propagated values are discarded and
    // SPB always starts from a fully random assignment (all -1 entries).
    bool no_inference = false;
};

struct RoundEvent {
    int round = 0;
    double cash_seconds_before_spb = 0.0;
    int propagated_original_variables = 0;
    long long cash_ub_before_spb = -1;
    long long spb_ub = -1;
    long long cash_ub_after_spb = -1;
    HybridBoundResult cash_bound_result = HybridBoundResult::NotImproved;
};

// Owns one SPB object for one instance. The object retains adaptive clause
// weights across rounds, but each callback supplies a new partial assignment.
class HybridCoordinator final : public HybridMaxSatCallback {
  public:
    HybridCoordinator(const std::string &wcnf_path,
                      const HybridSchedule &schedule = HybridSchedule());

    int (*cash_terminator())(void *) override;
    bool should_run(double cash_cpu_time) override;
    bool should_stop(double cash_cpu_time) override;
    // `cash_assignment` is 1-based over CASH's whole variable set, which is a
    // superset of the original WCNF variables; the coordinator trims it to the
    // part SPB knows before handing it over.
    bool find_upper_bound(const std::vector<int> &cash_assignment,
                          const Int &current_upper_bound,
                          Int &candidate_upper_bound) override;
    void on_upper_bound_result(HybridBoundResult result) override;

    // Write each round to `path` as soon as it completes, instead of holding
    // every event until the run ends. A run that stops on the end-to-end
    // budget can end through CASH's SIGXCPU backstop, which leaves no chance
    // to write anything afterwards; flushing per round keeps the round data.
    void set_event_log(const std::string &path, const std::string &instance_id);
    void flush_pending_events();

    const std::vector<RoundEvent> &round_events() const;
    const Solution &best_spb_certificate() const;
    bool has_spb_certificate() const;

  private:
    HybridSchedule schedule_;
    spbmaxsat::LocalSearchSolver spb_solver_;
    // Wall-clock seconds since the protocol started. The `cash_cpu_time`
    // values CASH passes in are ignored on purpose; see HybridSchedule.
    void ensure_started();
    double wall_elapsed() const;
    bool started_ = false;
    double instance_start_wall_ = 0.0;
    double next_spb_wall_ = 0.0;
    int round_ = 0;
    std::vector<RoundEvent> events_;
    Solution best_spb_certificate_;
    std::ofstream event_log_;
    std::string event_log_instance_id_;
    std::size_t written_events_ = 0;
};

} // namespace hybridmaxsat

#endif // HYBRIDMAXSAT_COORDINATOR_H
