#include "Solver/local_search_solver.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

void require(bool condition, const std::string &message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << std::endl;
        std::exit(1);
    }
}

void require_valid(const Solution &solution, const std::string &message)
{
    require(solution.feasible, message + ": a hard-feasible solution is required");
    require(solution.cost >= 0, message + ": objective must be non-negative");
}

} // namespace

int main(int argc, char *argv[])
{
    require(argc == 2, "expected a directory for temporary test data");

    const std::string path = std::string(argv[1]) + "/persistent_interface.wcnf";
    {
        std::ofstream output(path.c_str());
        output << "p wcnf 2 3 100\n";
        output << "100 1 0\n";
        output << "4 -1 2 0\n";
        output << "3 -2 0\n";
    }

    Settings settings;
    settings.cutoff_time = 1;
    settings.solution_pool_size = 1;
    spbmaxsat::LocalSearchSolver solver(path, settings);

    std::vector<int> first_initial(3, -1);
    first_initial[1] = 1;
    const Solution first = solver.improve_with_persistent_weights(first_initial);
    require_valid(first, "first persistent window");

    std::vector<int> second_initial(3, -1);
    second_initial[2] = 0;
    const Solution second = solver.improve_with_persistent_weights(second_initial);
    require_valid(second, "second persistent window");

    return 0;
}
