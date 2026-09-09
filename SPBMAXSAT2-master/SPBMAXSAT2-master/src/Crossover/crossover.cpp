#include "BasicStruct/instance.h"
#include "Crossover/crossover.h"
#include "BasicStruct/solutionpool.h"

Offspring Crossover::CrossoverSolutions(const Solution& parent1, const Solution& parent2) {
    int num_vars = oriInst.num_vars;
    Offspring child(num_vars);

    int crossover_point = num_vars / 2;

    for (int v = 1; v <= num_vars; ++v) {
        if (v <= crossover_point) {
            child.assignment[v] = parent1.assignment[v];
        } else {
            child.assignment[v] = parent2.assignment[v];
        }
    }

    for(int v = 1; v <= num_vars; ++v) {
        if (parent1.assignment[v] == parent2.assignment[v]) {
            child.isfixed[v] = true;
            child.fixed_count++;
        } else {
            child.isfixed[v] = false;
        }
    }

    return child;
}

Offspring Crossover::Crossover1(const Solution& parent1, const Solution& parent2) {
    int num_vars = oriInst.num_vars;
    Offspring child(num_vars);

    int crossover_point = num_vars / 2;

    for(int v = 1; v <= num_vars; ++v) {
        if (parent1.assignment[v] == parent2.assignment[v]) {
            child.isfixed[v] = true;
            child.fixed_count++;
        } else {
            child.isfixed[v] = false;
        }
    }

    vector<int> clause_visit_count(oriInst.num_clauses + 1, 0);

    for(int v = 1; v <= num_vars; ++v) {
        if (!child.isfixed[v]) {
            for(int w = 0; w < oriInst.var_lit_count[v]; w++) {
                int clause = oriInst.var_lit[v][w].clause_num;
                clause_visit_count[clause]++;
            }
        } 
    }

    for (int c = 0; c < oriInst.num_clauses; c++){
        if(clause_visit_count[c] > 1) {
            for (int w = 0; w < oriInst.clause_lit_count[c]; w++)
            {
                int var = oriInst.clause_lit[c][w].var_num;
                if(child.isfixed[var]) {
                    child.isfixed[var] = false;
                    child.fixed_count--;
                }
            }
        }
    }

    for(int v = 1; v <= num_vars; ++v) {
        if (child.isfixed[v]) {
            child.assignment[v] = parent1.assignment[v];
        } else {
            // Randomly choose from either parent
            if (rand() % 2 == 0) {
                child.assignment[v] = parent1.assignment[v];
            } else {
                child.assignment[v] = parent2.assignment[v];
            }
        }
    }

    return child;
}

Offspring Crossover::Crossover2(const Solution& parent1, const Solution& parent2) {
    int num_vars = oriInst.num_vars;
    Offspring child(num_vars);

    for(int v = 1; v <= num_vars; ++v) {
        if (parent1.assignment[v] == parent2.assignment[v]) {
            child.isfixed[v] = true;
            child.fixed_count++;
        } else {
            child.isfixed[v] = false;
        }
    }

    for(int v = 0; v < oriInst.unit_soft_clause_count; v++) {
        int var = oriInst.unit_soft_clause[v].var_num;
        int clause = oriInst.unit_soft_clause[v].clause_num;
        bool sense = oriInst.unit_soft_clause[v].sense;
        if(child.isfixed[var]) {
            child.isfixed[var] = false;
            child.fixed_count--;
        }else{
            for(int w = 0; w < oriInst.clause_lit_count[clause]; w++) {
                int other_var = oriInst.clause_lit[clause][w].var_num;
                if(child.isfixed[other_var]) {
                    child.isfixed[var] = false;
                    child.fixed_count--;
                }
            }
        }
    }

    for(int v = 1; v <= num_vars; ++v) {
        if (child.isfixed[v]) {
            child.assignment[v] = parent1.assignment[v];
        } else {
            // Randomly choose from either parent
            if (rand() % 2 == 0) {
                child.assignment[v] = parent1.assignment[v];
            } else {
                child.assignment[v] = parent2.assignment[v];
            }
        }
    }

    return child;
}

Offspring Crossover::Crossover3(const Solution& parent1, const Solution& parent2) {
    int num_vars = oriInst.num_vars;
    Offspring child(num_vars);

    if(oriInst.unit_soft_clause_count == 0) {
        // No soft unit clauses, fallback to Crossover1
        return Crossover1(parent1, parent2);
    }

    vector<bool> is_soft_unit(num_vars + 1, false);
    for(int v = 0; v < oriInst.unit_soft_clause_count; v++)
    {
        int var = oriInst.unit_soft_clause[v].var_num;
        is_soft_unit[var] = true;
    }

    vector<int> soft_unit_neighbor;
    vector<bool> is_soft_unit_neighbor(num_vars + 1, false);
    vector<int> visit_counter(num_vars + 1, 0);
    vector<int> clause_visited(oriInst.num_clauses + 1, 0);
    vector<bool> clause_flag(oriInst.num_clauses + 1, false);
    vector<bool> is_same_unit(num_vars + 1, false);
    // fix common vars
    for(int v = 1; v <= num_vars; ++v) {
        if (parent1.assignment[v] == parent2.assignment[v]) {
            child.isfixed[v] = true;
            child.fixed_count++;
        } else {
            child.isfixed[v] = false;
        }
    }
    // if too few fixed vars, do simple crossover
    if (child.fixed_count <= num_vars / 5)
    {
        for (int v = 1; v <= num_vars; ++v)
        {
            if (child.isfixed[v])
            {
                child.assignment[v] = parent1.assignment[v];
            }
            else
            {
                // Randomly choose from either parent
                if (rand() % 2 == 0)
                {
                    child.assignment[v] = parent1.assignment[v];
                }
                else
                {
                    child.assignment[v] = parent2.assignment[v];
                }
            }
        }
        return child;
    }
    //for all soft unit clauses, if its var is fixed, mark it
    //if not fixed, mark all its neighbor clauses
    for(int v = 0; v < oriInst.unit_soft_clause_count; v++) {
        int var = oriInst.unit_soft_clause[v].var_num;
        if(child.isfixed[var]) {
            is_same_unit[var] = true;
        }else{
            for(int w = 0; w <  oriInst.var_lit_count[var]; w++) {
                int c = oriInst.var_lit[var][w].clause_num;
                clause_visited[c]++;
            }
        }
    }
    //visit all clauses visited by soft unit vars
    for (int c = 0; c < oriInst.num_clauses; c++){
        if(clause_flag[c]) continue;
        if(clause_visited[c] > 0) {
            clause_flag[c] = true;
            for (int w = 0; w < oriInst.clause_lit_count[c]; w++)
            {
                int var = oriInst.clause_lit[c][w].var_num;
                visit_counter[var] += clause_visited[c];
            }
        }
    }

    for(int v = 1;v <= num_vars; ++v) {
        if(visit_counter[v] > 0) {
            is_soft_unit_neighbor[v] = true;
            soft_unit_neighbor.push_back(v);
        }
    }

    clause_visited.assign(oriInst.num_clauses + 1, 0);

    // unfix all soft unit vars and visit their neighbor clauses
    for(int i = 0; i < soft_unit_neighbor.size(); i++) {
        int var = soft_unit_neighbor[i];
        if(child.isfixed[var]) {
            child.isfixed[var] = false;
            child.fixed_count--;
        }
        for(int c = 0;c < oriInst.var_lit_count[var]; c++) {
            int clause = oriInst.var_lit[var][c].clause_num;
            clause_visited[clause]++;
        }
    }
    
    // unfix all vars in clauses visited by more than one soft unit neighbor
    for (int c = 0; c < oriInst.num_clauses; c++){
        if(clause_flag[c]) continue;
        if(clause_visited[c] > 1) {
            clause_flag[c] = true;
            for (int w = 0; w < oriInst.clause_lit_count[c]; w++)
            {
                int var = oriInst.clause_lit[c][w].var_num;
                if(child.isfixed[var]) {
                    child.isfixed[var] = false;
                    child.fixed_count--;
                }
            }
        }
    }

    //unfix soft_unit vars whose clauses are visited(connected) by other vars
    for(int v = 0;v < oriInst.unit_soft_clause_count; v++) {
        int var = oriInst.unit_soft_clause[v].var_num;
        if(child.isfixed[var]) {
            for(int c = 0;c < oriInst.var_lit_count[var]; c++) {
                int clause = oriInst.var_lit[var][c].clause_num;
                if(clause_flag[clause]) {
                    child.isfixed[var] = false;
                    child.fixed_count--;
                    break;
                }
            }
        }
    }

    for(int v = 1; v <= num_vars; ++v) {
        if (child.isfixed[v]) {
            child.assignment[v] = parent1.assignment[v];
        } else {
            // Randomly choose from either parent
            if (rand() % 2 == 0) {
                child.assignment[v] = parent1.assignment[v];
            } else {
                child.assignment[v] = parent2.assignment[v];
            }
        }
    }

    return child;
}


Offspring Crossover::Crossover4(const Solution& parent1, const Solution& parent2) {
    int num_vars = oriInst.num_vars;
    Offspring child(num_vars);

    for(int v = 1; v <= num_vars; ++v) {
        if (parent1.assignment[v] == parent2.assignment[v]) {
            child.isfixed[v] = true;
            child.fixed_count++;
        } else {
            child.isfixed[v] = false;
        }
    }

    vector<int> clause_visit_count(oriInst.num_clauses + 1, 0);
    vector<int> var_visit_count(num_vars + 1, 0);
    for(int v = 0; v < oriInst.unit_soft_clause_count; v++) {
        int var = oriInst.unit_soft_clause[v].var_num;
        int clause = oriInst.unit_soft_clause[v].clause_num;
        bool sense = oriInst.unit_soft_clause[v].sense;
        if(child.isfixed[var]) {
            child.isfixed[var] = false;
            child.fixed_count--;
        }else{
            for(int w = 0;w < oriInst.var_lit_count[var]; w++) {
                int c = oriInst.var_lit[var][w].clause_num;
                clause_visit_count[c]++;
            }
        }
    }

    for (int c = 0; c < oriInst.num_clauses; c++){
        for (int w = 0; w < oriInst.clause_lit_count[c]; w++)
        {
            int var = oriInst.clause_lit[c][w].var_num;
            var_visit_count[var]++;
        }
    }
    struct VarCount {
        int var;
        int count;
    };
    vector<VarCount> var_count_list;
    for(int v = 1; v <= num_vars; ++v) {
        if(var_visit_count[v] > 0) {
            var_count_list.push_back({v, var_visit_count[v]});
        }
    }
    sort(var_count_list.begin(), var_count_list.end(), [](const VarCount& a, const VarCount& b) {
        return a.count > b.count;
    });
    for(size_t i = 0; i < var_count_list.size() && i < oriInst.num_vars/2; i++) {
        int var = var_count_list[i].var;
        if(child.isfixed[var]) {
            child.isfixed[var] = false;
            child.fixed_count--;
        }
    }

    for(int v = 1; v <= num_vars; ++v) {
        if (child.isfixed[v]) {
            child.assignment[v] = parent1.assignment[v];
        } else {
            // Randomly choose from either parent
            if (rand() % 2 == 0) {
                child.assignment[v] = parent1.assignment[v];
            } else {
                child.assignment[v] = parent2.assignment[v];
            }
        }
    }

    return child;
}
