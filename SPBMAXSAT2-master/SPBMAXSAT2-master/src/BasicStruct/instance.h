#ifndef _INSTANCE_H_
#define _INSTANCE_H_

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <cstring>
#include <limits>
#include "BasicStruct/lit.h"

using namespace std;

struct ReducedInstance;

class Instance
{
public:
    int problem_weighted;

    // size of instance
    int num_vars;
    int num_clauses;
    int num_hclauses;
    int num_sclauses;

    // information about the clauses
    long long top_clause_weight;
    long long total_soft_weight;
    long long total_hard_length;
    long long total_soft_length;

    // Literals and Clauses
    lit **var_lit;
    int *var_lit_count;
    lit **clause_lit;
    int *clause_lit_count;

    // Unit clauses
    lit *unit_clause;
    int unit_clause_count;

    lit *unit_soft_clause;
    int unit_soft_clause_count;
    // Clause weights
    long long *org_clause_weight;

    // Neighbors
    int **var_neighbor;
    int *var_neighbor_count;

    // Soft clauses
    int *soft_clause_num_index;

    // Temp data for building
    int *temp_lit;

    // Constructor and Destructor
    Instance();
    ~Instance();

    // Copy semantics (deep copy)
    Instance(const Instance &other);
    Instance &operator=(const Instance &other);

    // Move semantics
    Instance(Instance&& other) noexcept;
    Instance& operator=(Instance&& other) noexcept;

    // Methods
    void build_instance(const char *filename);
    void print_info();

    // Reduce current instance under a partial assignment.
    // assignment[v] in {0,1} means fixed value, -1 means unassigned.
    // The result contains a smaller instance plus variable mappings and
    // the accumulated base_cost of soft clauses that are already forced unsatisfied.
    ReducedInstance reduce(const vector<int> &assignment) const;
    
    long long verify_solution(const vector<int> &assignment) const;

private:
    void allocate_memory();
    void free_memory();
};

struct ReducedInstance
{
    Instance reduced;         // reduced sub-instance
    vector<int> old2new;      // size = original num_vars + 1, 0 means removed/fixed
    vector<int> new2old;      // size = reduced num_vars + 1, new index -> old index
    vector<int> fixed_assignment; // size = original num_vars + 1, -1 for unfixed, 0/1 for fixed
    long long base_cost;      // fixed cost from already-unsatisfied soft clauses
    bool unsat;               // true if reduction detects hard contradiction

    ReducedInstance() : base_cost(0), unsat(false) {}

    // Expand a solution on the reduced instance (1..reduced.num_vars)
    // back to a full solution on the original instance (1..original num_vars).
    // Values from fixed_assignment are kept for variables that were fixed
    // when reduce() was called, and values from reduced_solution are filled
    // for remaining variables according to new2old mapping.
    vector<int> expand_solution(const vector<int> &reduced_solution) const
    {
        int original_num_vars = 0;
        if (!old2new.empty())
            original_num_vars = static_cast<int>(old2new.size()) - 1;

        vector<int> full_solution(original_num_vars + 1, -1);

        // Start from fixed_assignment if available
        if (!fixed_assignment.empty())
        {
            int limit = static_cast<int>(fixed_assignment.size());
            if (limit > original_num_vars + 1)
                limit = original_num_vars + 1;
            for (int v = 0; v < limit; ++v)
            {
                full_solution[v] = fixed_assignment[v];
            }
        }

        // Overlay values from reduced solution via new2old mapping
        int new_vars = static_cast<int>(new2old.size()) - 1; // since new2old[0] is dummy
        for (int new_v = 1; new_v <= new_vars; ++new_v)
        {
            int old_v = new2old[new_v];
            if (old_v <= 0 || old_v > original_num_vars)
                continue;
            if (static_cast<size_t>(new_v) >= reduced_solution.size())
                continue;
            full_solution[old_v] = reduced_solution[new_v];
        }

        return full_solution;
    }
};

#endif // _INSTANCE_H_
