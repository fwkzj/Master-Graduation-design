#include "BasicStruct/instance.h"

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

} // namespace

int main(int argc, char *argv[])
{
    require(argc == 2, "expected a directory for temporary test data");

    const std::string path = std::string(argv[1]) + "/standard_weighted.wcnf";
    {
        std::ofstream output(path.c_str());
        output << "c standard weighted MaxSAT instance\n";
        output << "p wcnf 2 3 10\n";
        output << "10 1 0\n";
        output << "3 -1 0\n";
        output << "5 2 0\n";
    }

    Instance instance;
    instance.build_instance(path.c_str());

    require(instance.num_vars == 2, "header variable count must be retained");
    require(instance.num_clauses == 3, "header clause count must be retained");
    require(instance.top_clause_weight == 10, "header top weight must be retained");
    require(instance.num_hclauses == 1, "top-weight clause must be hard");
    require(instance.num_sclauses == 2, "non-top clauses must be soft");
    require(instance.total_soft_weight == 8, "soft weights must be summed exactly");

    std::vector<int> assignment(3, 0);
    assignment[1] = 1;
    assignment[2] = 1;
    require(instance.verify_solution(assignment) == 3,
            "standard WCNF assignment must receive its soft-clause cost");

    return 0;
}
