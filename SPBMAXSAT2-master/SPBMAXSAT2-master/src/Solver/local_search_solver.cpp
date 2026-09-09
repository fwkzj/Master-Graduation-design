#include "Solver/local_search_solver.h"

#include <stdexcept>
#include <utility>

#include "Util/timer.h"
#include "Worker/lsworker.h"

namespace spbmaxsat {

LocalSearchSolver::LocalSearchSolver(const std::string &wcnf_file,
                                     const Settings &settings)
{
    backend_.settings = settings;
    backend_.BuildInstance(wcnf_file.c_str());
}

LocalSearchSolver::LocalSearchSolver(const Instance &instance,
                                     const Settings &settings)
{
    backend_.settings = settings;
    // Copy-construct first, then move into the backend. This keeps ownership of
    // the caller's instance unchanged while avoiding another deep copy.
    backend_.originInstance = Instance(instance);
}

void LocalSearchSolver::set_settings(const Settings &settings)
{
    backend_.settings = settings;
}

const Settings &LocalSearchSolver::get_settings() const
{
    return backend_.settings;
}

void LocalSearchSolver::begin_search()
{
    util::start_global_timer();
    util::set_global_cutoff(backend_.settings.cutoff_time);
    backend_.best_sol = Solution();

    int pool_size = backend_.settings.solution_pool_size;
    if (pool_size < 1)
        pool_size = 1;
    backend_.solpool = SolutionPool(&backend_.originInstance, pool_size);
}

Solution LocalSearchSolver::solve()
{
    begin_search();

    Instance working_instance = backend_.originInstance;
    LSworker worker(std::move(working_instance));
    worker.set_solver(&backend_);
    worker.settings();
    worker.settings(backend_.settings);
    worker.set_cutoff_time(backend_.settings.cutoff_time);
    worker.local_search_with_decimation();

    return checked_result();
}

Solution LocalSearchSolver::improve(const std::vector<int> &initial_solution)
{
    validate_initial_solution(initial_solution);
    begin_search();

    Instance working_instance = backend_.originInstance;
    LSworker worker(std::move(working_instance));
    worker.set_solver(&backend_);
    worker.settings();
    worker.settings(backend_.settings);
    worker.set_cutoff_time(backend_.settings.cutoff_time);

    std::vector<int> working_solution = initial_solution;
    worker.local_search_with_init_solution(working_solution);

    return checked_result();
}

Solution LocalSearchSolver::solve_with_crossover()
{
    begin_search();
    backend_.Solve();
    return checked_result();
}

const SolutionPool &LocalSearchSolver::solution_pool() const
{
    return backend_.solpool;
}

void LocalSearchSolver::validate_initial_solution(
    const std::vector<int> &initial_solution) const
{
    const std::size_t expected = static_cast<std::size_t>(backend_.originInstance.num_vars) + 1;
    if (initial_solution.size() != expected)
        throw std::invalid_argument("initial solution must contain num_vars + 1 entries");

    for (std::size_t v = 1; v < initial_solution.size(); ++v)
    {
        const int value = initial_solution[v];
        if (value != -1 && value != 0 && value != 1)
            throw std::invalid_argument("initial solution values must be -1, 0, or 1");
    }
}

Solution LocalSearchSolver::checked_result() const
{
    const Solution result = backend_.best_sol;
    if (!result.feasible)
        return result;

    const long long verified_cost = backend_.originInstance.verify_solution(result.assignment);
    if (verified_cost < 0 || verified_cost != result.cost)
        throw std::runtime_error("local-search result failed verification");

    return result;
}

} // namespace spbmaxsat
