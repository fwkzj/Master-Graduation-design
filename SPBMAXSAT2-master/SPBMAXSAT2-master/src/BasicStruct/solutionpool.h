#ifndef _SOLUTIONPOOL_H_
#define _SOLUTIONPOOL_H_

#include <algorithm>

using namespace std;

class Solution{
public:
    vector<int> assignment; // variable assignments, index 0 unused
    long long cost = __LONG_LONG_MAX__;        // cost of the solution
    bool feasible = false; // whether the solution is feasible
    Solution() {}
    Solution(int num_vars) : assignment(num_vars + 1, -1), cost(__LONG_LONG_MAX__) {}
    Solution(const vector<int>& assign, long long c, bool feas)
        : assignment(assign), cost(c), feasible(feas) {}
    // Comparison operator for sorting solutions by cost
    bool operator<(const Solution &other) const {
        return cost < other.cost;
    }
};

class SolutionPool {
public:
    Instance* instance;          // Pointer to the problem instance
    vector<Solution> solutions;  // Pool of solutions
    int maxSize;                // Maximum size of the solution pool

    SolutionPool() : instance(nullptr), maxSize(0) {
        solutions.clear();
    }

    SolutionPool(Instance* inst, int max_pool_size)
        : instance(inst), maxSize(max_pool_size) {}

    void PushSolutionByCost(const Solution &sol) {
        for(const auto &existing_sol : solutions) {
            if (existing_sol.cost == sol.cost) {
                return; // Duplicate cost, do not add
            }
            if (existing_sol.assignment == sol.assignment) {
                return; // Duplicate solution, do not add
            }
        }
        if (solutions.size() < maxSize) {
            solutions.push_back(sol);
            push_heap(solutions.begin(), solutions.end());
        } else if (sol.cost < solutions.front().cost) {
            pop_heap(solutions.begin(), solutions.end());
            solutions.back() = sol;
            push_heap(solutions.begin(), solutions.end());
        }
    }

    void test_print() {
        cout << "Solution Pool (size " << solutions.size() << "):" << endl;
        for (const auto &sol : solutions) {
            cout << "Cost: " << sol.cost << ", Feasible: " << sol.feasible << endl;
        }
    }

    bool is_empty() const {
        return solutions.empty();
    }

    bool is_full() const {
        return solutions.size() >= maxSize;
    }

    int size() const {
        return static_cast<int>(solutions.size());
    }

    int DiffNum(const Solution &sol1, const Solution &sol2) const {
        int diff_count = 0;
        for (size_t v = 1; v < sol1.assignment.size(); ++v) {
            if (sol1.assignment[v] != sol2.assignment[v]) {
                diff_count++;
            }
        }
        return diff_count;
    }
};

#endif // _SOLUTIONPOOL_H_