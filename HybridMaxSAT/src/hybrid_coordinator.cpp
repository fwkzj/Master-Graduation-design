#include "hybrid_coordinator.h"

#include <cstdlib>
#include <stdexcept>

#include "Global.h"

namespace hybridmaxsat {

namespace {

long long to_log_value(const Int &value)
{
    return value == Int_MAX ? -1 : tolong(value);
}

int count_propagated(const std::vector<int> &partial_assignment)
{
    int count = 0;
    for (std::size_t variable = 1; variable < partial_assignment.size(); ++variable)
        if (partial_assignment[variable] == 0 || partial_assignment[variable] == 1)
            ++count;
    return count;
}

} // namespace

HybridCoordinator::HybridCoordinator(const std::string &wcnf_path,
                                     const HybridSchedule &schedule)
    : schedule_(schedule)
    , spb_solver_(wcnf_path, [&schedule]() {
          Settings settings;
          settings.cutoff_time = schedule.spb_window_seconds;
          settings.solution_pool_size = 1;
          return settings;
      }())
    , instance_start_cpu_(cpuTime())
    , next_spb_cpu_(instance_start_cpu_ + schedule.cash_window_seconds)
{
    if (schedule_.cash_window_seconds <= 0 ||
        schedule_.spb_window_seconds <= 0 ||
        schedule_.total_budget_seconds <= 0)
        throw std::invalid_argument("hybrid schedule durations must be positive");

    // SPB uses the C random generator internally.  A new initial assignment
    // is still generated each round because -1 values are handed to improve.
    std::srand(schedule_.random_seed);
}

bool HybridCoordinator::should_run(double cash_cpu_time)
{
    return cash_cpu_time < instance_start_cpu_ + schedule_.total_budget_seconds &&
           cash_cpu_time >= next_spb_cpu_;
}

bool HybridCoordinator::should_stop(double cash_cpu_time)
{
    return cash_cpu_time >= instance_start_cpu_ + schedule_.total_budget_seconds;
}

bool HybridCoordinator::find_upper_bound(
    const std::vector<int> &partial_assignment,
    const Int &current_upper_bound,
    Int &candidate_upper_bound)
{
    RoundEvent event;
    event.round = ++round_;
    event.cash_cpu_before_spb = cpuTime() - instance_start_cpu_;
    event.propagated_original_variables = count_propagated(partial_assignment);
    event.cash_ub_before_spb = to_log_value(current_upper_bound);

    const Solution result =
        spb_solver_.improve_with_persistent_weights(partial_assignment);
    next_spb_cpu_ = cpuTime() + schedule_.cash_window_seconds;

    if (!result.feasible)
    {
        events_.push_back(event);
        return false;
    }

    // improve_with_persistent_weights() independently re-evaluates the model
    // using the original WCNF before returning this result.
    event.spb_ub = result.cost;
    if (!best_spb_certificate_.feasible || result.cost < best_spb_certificate_.cost)
        best_spb_certificate_ = result;

    events_.push_back(event);
    if (Int(result.cost) >= current_upper_bound)
        return false;

    candidate_upper_bound = Int(result.cost);
    return true;
}

void HybridCoordinator::on_upper_bound_result(HybridBoundResult result)
{
    if (!events_.empty())
    {
        events_.back().cash_bound_result = result;
        events_.back().cash_ub_after_spb =
            result == HybridBoundResult::Accepted ? events_.back().spb_ub :
            events_.back().cash_ub_before_spb;
    }
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

} // namespace hybridmaxsat
