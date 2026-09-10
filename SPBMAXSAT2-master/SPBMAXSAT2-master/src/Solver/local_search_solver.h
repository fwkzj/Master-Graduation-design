#ifndef SPBMAXSAT_LOCAL_SEARCH_SOLVER_H
#define SPBMAXSAT_LOCAL_SEARCH_SOLVER_H

#include <string>
#include <memory>
#include <vector>

#include "BasicStruct/instance.h"
#include "BasicStruct/settings.h"
#include "BasicStruct/solutionpool.h"
#include "Solver/solver.h"

namespace spbmaxsat {

// A small synchronous, single-threaded facade for embedding SPB-MaxSAT.
// Assignment vectors are 1-based: element 0 is unused.
class LocalSearchSolver {
public:
    explicit LocalSearchSolver(const std::string &wcnf_file,
                               const Settings &settings = Settings());
    explicit LocalSearchSolver(const Instance &instance,
                               const Settings &settings = Settings());

    LocalSearchSolver(const LocalSearchSolver &) = delete;
    LocalSearchSolver &operator=(const LocalSearchSolver &) = delete;

    void set_settings(const Settings &settings);
    const Settings &get_settings() const;

    // Run one local-search worker with a decimation-generated initial state.
    Solution solve();

    // Improve a complete or partial assignment. Values must be 0, 1, or -1;
    // -1 entries are randomly completed before local search starts.
    Solution improve(const std::vector<int> &initial_solution);

    // Improve a fresh complete or partial assignment while retaining the
    // adaptive clause weights accumulated by previous calls on this object.
    // The assignment-dependent state is rebuilt for every call.  This is the
    // interface used by the CASH/SPB coordinator: the returned solution is
    // still checked against the original WCNF before it is exposed.
    Solution improve_with_persistent_weights(
        const std::vector<int> &initial_solution);

    // Run the existing solution-pool and crossover pipeline.
    Solution solve_with_crossover();

    const SolutionPool &solution_pool() const;

private:
    Solver backend_;
    std::unique_ptr<::LSworker> persistent_worker_;
    bool persistent_weights_initialized_ = false;

    void begin_search();
    void reset_persistent_worker();
    void validate_initial_solution(const std::vector<int> &initial_solution) const;
    Solution checked_result() const;
};

} // namespace spbmaxsat

#endif // SPBMAXSAT_LOCAL_SEARCH_SOLVER_H
