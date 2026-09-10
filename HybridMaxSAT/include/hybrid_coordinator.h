#ifndef HYBRIDMAXSAT_COORDINATOR_H
#define HYBRIDMAXSAT_COORDINATOR_H

#include <cstdint>
#include <string>
#include <vector>

#include "MsSolver.h"
#include "Solver/local_search_solver.h"

namespace hybridmaxsat {

struct HybridSchedule {
    int cash_window_seconds = 15;
    int spb_window_seconds = 3;
    int total_budget_seconds = 600;
    unsigned int random_seed = 20260909U;
};

struct RoundEvent {
    int round = 0;
    double cash_cpu_before_spb = 0.0;
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

    bool should_run(double cash_cpu_time) override;
    bool should_stop(double cash_cpu_time) override;
    bool find_upper_bound(const std::vector<int> &partial_assignment,
                          const Int &current_upper_bound,
                          Int &candidate_upper_bound) override;
    void on_upper_bound_result(HybridBoundResult result) override;

    const std::vector<RoundEvent> &round_events() const;
    const Solution &best_spb_certificate() const;
    bool has_spb_certificate() const;

  private:
    HybridSchedule schedule_;
    spbmaxsat::LocalSearchSolver spb_solver_;
    double instance_start_cpu_;
    double next_spb_cpu_;
    int round_ = 0;
    std::vector<RoundEvent> events_;
    Solution best_spb_certificate_;
};

} // namespace hybridmaxsat

#endif // HYBRIDMAXSAT_COORDINATOR_H
