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

    // Shorten the local-search cutoff without discarding accumulated
    // state. set_settings() resets the persistent worker and with it the
    // adaptive clause weights, so an embedder that only needs to trim the
    // next window -- because the end-to-end budget is nearly spent -- has
    // to go through here instead.
    void set_cutoff_time(double seconds);

    // Diagnostics of the most recent improve_with_persistent_weights() call.
    // The cutoff bounds only the search phase, so these separate the work the
    // window actually bought (steps, search seconds) from the fixed cost paid
    // around it (worker setup, certificate re-check).
    long long last_step_count() const { return last_step_count_; }
    double last_init_seconds() const { return last_init_seconds_; }
    int last_tries() const { return last_tries_; }
    double last_search_seconds() const { return last_search_seconds_; }
    double last_setup_seconds() const { return last_setup_seconds_; }
    double last_verify_seconds() const { return last_verify_seconds_; }

    // Number of variables in the original WCNF, i.e. the length an assignment
    // vector must have (minus the unused element 0). An embedder that keeps its
    // own copy of the instance has to size its vectors by this, not by its own
    // variable count: the two differ as soon as the embedder adds variables of
    // its own (CASH's relaxation and sorter variables).
    int num_vars() const { return backend_.originInstance.num_vars; }

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
    long long last_step_count_ = 0;
    double last_init_seconds_ = 0.0;
    int last_tries_ = 0;
    double last_search_seconds_ = 0.0;
    double last_setup_seconds_ = 0.0;
    double last_verify_seconds_ = 0.0;

    void begin_search();
    void reset_persistent_worker();
    void validate_initial_solution(const std::vector<int> &initial_solution) const;
    Solution checked_result() const;
};

} // namespace spbmaxsat

#endif // SPBMAXSAT_LOCAL_SEARCH_SOLVER_H
