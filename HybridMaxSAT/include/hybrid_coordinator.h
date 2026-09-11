#ifndef HYBRIDMAXSAT_COORDINATOR_H
#define HYBRIDMAXSAT_COORDINATOR_H

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <mutex>
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
    // Handoff policy. True is the protocol as originally specified: the
    // coordinator arms CaDiCaL's terminator at the next window boundary, so
    // a SAT call that would outlive the window is cut short and SPB gets its
    // turn on schedule. Measured on MSE23W that policy is destructive --
    // CASH's CDCL search does not recover from the cut and loses far more
    // optimality proofs than SPB can win back -- so the switch exists to run
    // the same protocol without it. False arms the terminator with the
    // end-to-end budget only, which restricts SPB to scheduling points CASH
    // reached on its own; a window then reads as a minimum amount of CASH
    // time between two handoffs rather than a hard preemption point.
    bool preemptive = true;
    // Selective scheduling (A3). Measured on runs/full23w_fix: an accepted
    // round is followed by another gain 32.8% of the time, a feasible round
    // that did not improve only 4.6%, an infeasible round 1.8%. So the handoff
    // keeps its base cadence only while it keeps paying; the first miss buys
    // one retry four windows later, and a second miss stops handoffs for the
    // rest of the run and gives the time back to CASH.
    bool selective = false;
    // Progress-based scheduling. When true the gap before the next
    // handoff doubles for every consecutive round in which SPB produced
    // nothing CASH could use, and resets the moment one of its bounds is
    // accepted. The observables are exactly the ones the round log
    // already publishes -- feasibility and strict improvement -- so the
    // schedule stays deterministic and reproducible from the event
    // stream. See note_round_outcome().
    bool adaptive = false;
};

struct RoundEvent {
    int round = 0;
    double cash_seconds_before_spb = 0.0;
    int propagated_original_variables = 0;
    long long cash_ub_before_spb = -1;
    long long spb_ub = -1;
    long long cash_ub_after_spb = -1;
    HybridBoundResult cash_bound_result = HybridBoundResult::NotImproved;
    // Evidence that the window ran a real search instead of only paying the
    // fixed cost around it: flips actually executed, and how the wall time
    // inside the call split between setup, search and certificate re-check.
    // Bounds on both sides of the handoff: what CASH held when SPB started and
    // what it held once the round was folded in. The LB is CASH own; a round
    // can only move the UB.
    // Seconds left on the deadline CASH was actually running under when this
    // round started. Negative means the armed deadline had already expired, so
    // every SAT call of the window was aborted on entry.
    double cash_deadline_in = 0.0;
    std::string schedule_reason;
    double next_handoff_in = 0.0;
    long long cash_lb_before_spb = -1;
    long long cash_lb_after_spb = -1;
    long long spb_steps = 0;
    int spb_tries = 0;
    double spb_init_seconds = 0.0;
    double spb_call_seconds = 0.0;
    double spb_search_seconds = 0.0;
    double spb_setup_seconds = 0.0;
    double spb_verify_seconds = 0.0;
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
                          const Int &cash_lower_bound,
                          const Int &cash_upper_bound,
                          Int &candidate_upper_bound) override;
    void on_upper_bound_result(HybridBoundResult result,
                               const Int &cash_lower_bound) override;

    // CASH reports its own bounds at every scheduling point, so the run keeps
    // an LB/UB trajectory even across the windows in which no handoff happens.
    void on_scheduling_point(const Int &cash_lower_bound,
                             const Int &cash_upper_bound) override;

    // Write each round to `path` as soon as it completes, instead of holding
    // every event until the run ends. A run that stops on the end-to-end
    // budget can end through CASH's SIGXCPU backstop, which leaves no chance
    // to write anything afterwards; flushing per round keeps the round data.
    void set_event_log(const std::string &path, const std::string &instance_id);
    void flush_pending_events();

    const std::vector<RoundEvent> &round_events() const;
    const Solution &best_spb_certificate() const;
    bool has_spb_certificate() const;
    // Copy the best verified SPB cost out under the lock the writer
    // holds. The watchdog runs on its own thread and may look at the
    // certificate while CASH's thread is replacing it, so it must not
    // touch best_spb_certificate() directly. Returns false when no
    // verified model has been produced yet.
    bool certificate_snapshot(long long &cost) const;

  private:
    HybridSchedule schedule_;
    spbmaxsat::LocalSearchSolver spb_solver_;
    // Wall-clock seconds since the protocol started. The `cash_cpu_time`
    // values CASH passes in are ignored on purpose; see HybridSchedule.
    void ensure_started();
    // Arm CaDiCaL's deadline terminator. Under the preemptive policy this is
    // the next window boundary; under the non-preemptive one it is only ever
    // the end-to-end budget.
    void arm_deadline();
    // Fold one round's outcome into the adaptive schedule. A no-op
    // unless schedule_.adaptive is set.
    void note_round_outcome(bool productive);
    double wall_elapsed() const;
    bool started_ = false;
    double instance_start_wall_ = 0.0;
    double next_spb_wall_ = 0.0;
    // Wall clock at the end of the most recent SPB round; the adaptive
    // schedule measures its next gap from there, not from whenever
    // CASH got around to reporting the round's verdict.
    double last_round_end_wall_ = 0.0;
    // Consecutive rounds SPB could not improve. Drives the backoff.
    int barren_rounds_ = 0;
    // Set once the selective policy has given up on handing off.
    bool stopped_ = false;
    // Why the schedule moved where it did; copied into the round record.
    std::string last_schedule_reason_;
    // Throttle for the progress records; one every few seconds is plenty to
    // show whether CASH is closing the gap or standing still.
    double last_progress_wall_ = -1.0e18;
    int round_ = 0;
    std::vector<RoundEvent> events_;
    Solution best_spb_certificate_;
    // Guards best_spb_certificate_ between CASH's thread and the
    // watchdog; see certificate_snapshot().
    mutable std::mutex certificate_mutex_;
    std::ofstream event_log_;
    std::string event_log_instance_id_;
    std::size_t written_events_ = 0;
};

} // namespace hybridmaxsat

#endif // HYBRIDMAXSAT_COORDINATOR_H
