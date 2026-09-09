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

    const std::string hard_keyword_path = std::string(argv[1]) + "/hard_keyword.wcnf";
    {
        std::ofstream output(hard_keyword_path.c_str());
        output << "p wcnf 1 2 100\n";
        output << "h 1 0\n";
        output << "7 -1 0\n";
    }

    Instance hard_keyword_instance;
    hard_keyword_instance.build_instance(hard_keyword_path.c_str());

    require(hard_keyword_instance.num_hclauses == 1,
            "h-prefixed clause must be classified as hard");
    require(hard_keyword_instance.num_sclauses == 1,
            "h-prefixed clause must not contribute a soft clause");
    require(hard_keyword_instance.total_soft_weight == 7,
            "h-prefixed clause must not contribute to the soft objective");

    std::vector<int> hard_keyword_assignment(2, 0);
    hard_keyword_assignment[1] = 1;
    require(hard_keyword_instance.verify_solution(hard_keyword_assignment) == 7,
            "h-prefixed clause must remain hard during solution verification");

    hard_keyword_assignment[1] = 0;
    require(hard_keyword_instance.verify_solution(hard_keyword_assignment) == -1,
            "violating an h-prefixed clause must reject the assignment");

    const std::string empty_soft_path = std::string(argv[1]) + "/empty_soft.wcnf";
    {
        std::ofstream output(empty_soft_path.c_str());
        output << "p wcnf 1 2 100\n";
        output << "6 0\n";
        output << "4 1 0\n";
    }

    Instance empty_soft_instance;
    empty_soft_instance.build_instance(empty_soft_path.c_str());
    require(empty_soft_instance.fixed_soft_cost == 6,
            "empty soft clauses must be represented as fixed objective cost");
    require(empty_soft_instance.num_clauses == 1,
            "empty soft clauses must not enter the local-search clause stacks");

    std::vector<int> empty_soft_assignment(2, 0);
    empty_soft_assignment[1] = 1;
    require(empty_soft_instance.verify_solution(empty_soft_assignment) == 6,
            "fixed soft cost must be included in verified objective values");

    const std::string empty_hard_path = std::string(argv[1]) + "/empty_hard.wcnf";
    {
        std::ofstream output(empty_hard_path.c_str());
        output << "p wcnf 1 1 100\n";
        output << "h 0\n";
    }

    Instance empty_hard_instance;
    empty_hard_instance.build_instance(empty_hard_path.c_str());
    require(empty_hard_instance.has_empty_hard_clause,
            "empty hard clauses must mark the instance infeasible");
    require(empty_hard_instance.verify_solution(empty_soft_assignment) == -1,
            "empty hard clauses must reject every assignment");

    return 0;
}
