#include "Worker/lsworker.h"
#include "BasicStruct/solutionpool.h"
#include "Solver/solver.h"
#include <assert.h>
LSworker::LSworker(Solver *solver)
    : solver(solver)
{
}

void LSworker::allocate_memory()
{
    
    int malloc_var_length = inst.num_vars + 10;
    int malloc_clause_length = inst.num_clauses + 10;

    score = new double[malloc_var_length];
    time_stamp = new long long[malloc_var_length];
    neighbor_flag = new int[malloc_var_length];
    temp_neighbor = new int[malloc_var_length];

    clause_weight = new double[malloc_clause_length];
    tuned_org_clause_weight = new double[malloc_clause_length];
    sat_count = new int[malloc_clause_length];
    sat_var = new int[malloc_clause_length];
    // clause_selected_count = new long long[malloc_clause_length];
    best_soft_clause = new int[malloc_clause_length];

    hardunsat_stack = new int[malloc_clause_length];
    index_in_hardunsat_stack = new int[malloc_clause_length];
    softunsat_stack = new int[malloc_clause_length];
    index_in_softunsat_stack = new int[malloc_clause_length];

    // soft_clause_weight_upper_bound = new long long[malloc_clause_length];

    unsatvar_stack = new int[malloc_var_length];
    index_in_unsatvar_stack = new int[malloc_var_length];
    unsat_app_count = new int[malloc_var_length];

    goodvar_stack = new int[malloc_var_length];
    already_in_goodvar_stack = new int[malloc_var_length];

    cur_soln = new int[malloc_var_length];
    best_soln = new int[malloc_var_length];
    local_opt_soln = new int[malloc_var_length];

    large_weight_clauses = new int[malloc_clause_length];
    soft_large_weight_clauses = new int[malloc_clause_length];
    already_in_soft_large_weight_stack = new int[malloc_clause_length];

    best_array = new int[malloc_var_length];
}

void LSworker::free_memory(){
    delete[] score;

    delete[] time_stamp;
    delete[] neighbor_flag;
    delete[] temp_neighbor;

    delete[] clause_weight;
    delete[] tuned_org_clause_weight;
    delete[] sat_count;
    delete[] sat_var;
    // delete[] clause_selected_count;
    delete[] best_soft_clause;

    delete[] hardunsat_stack;
    delete[] index_in_hardunsat_stack;
    delete[] softunsat_stack;
    delete[] index_in_softunsat_stack;

    delete[] unsatvar_stack;
    delete[] index_in_unsatvar_stack;
    delete[] unsat_app_count;

    delete[] goodvar_stack;
    delete[] already_in_goodvar_stack;

    // delete [] fix;
    delete[] cur_soln;
    delete[] best_soln;
    delete[] local_opt_soln;

    delete[] large_weight_clauses;
    delete[] soft_large_weight_clauses;
    delete[] already_in_soft_large_weight_stack;

    delete[] best_array;
}

LSworker::~LSworker(){
    free_memory();
}


LSworker::LSworker(Instance &&instance)
{
    inst = std::move(instance);
    allocate_memory();
}

void LSworker::settings(){
    cutoff_time = 300;
    max_tries = 100000000;
    max_flips = 200000000;
    max_non_improve_flip = 10000000;
    large_clause_count_threshold = 0;
    soft_large_clause_count_threshold = 0;

    if (1 == inst.problem_weighted) // Weighted Partial MaxSAT
    {
        coe_soft_clause_weight = 3000;
        if (0 != inst.num_hclauses)
        {
            hd_count_threshold = 97;
            rdprob = 0.036;
            rwprob = 0.48;
            h_inc = 28;
            soft_increase_ratio = 1.001;
            avg_soft_weight =  double(inst.total_soft_weight) / inst.num_sclauses;
            for (int i = 0; i < inst.num_sclauses; ++i)
            {
                int c = inst.soft_clause_num_index[i];
                tuned_org_clause_weight[c] = (double)inst.org_clause_weight[c] / avg_soft_weight;
            }
        }
        else
        {
            softclause_weight_threshold = 0;
            soft_smooth_probability = 1E-3;
            hd_count_threshold = 22;
            rdprob = 0.036;
            rwprob = 0.48;
            s_inc = 1.0;
            for (int i = 0; i < inst.num_sclauses; ++i)
            {
                int c = inst.soft_clause_num_index[i];
                tuned_org_clause_weight[c] = inst.org_clause_weight[c];
            }
        }
    }
    else // Unweighted Partial Maxsat
    {
        avg_soft_weight =  1;
        for (int i = 0; i < inst.num_sclauses; ++i)
        {
            int c = inst.soft_clause_num_index[i];
            tuned_org_clause_weight[c] = 1;
        }
        h_inc = 1;
        s_inc = 1;
        if (0 != inst.num_hclauses)
        {
            hd_count_threshold = 53;
            coe_soft_clause_weight = 1;
            rdprob = 0.079;
            rwprob = 0.087;
            soft_increase_ratio = 1.00072;
        }
        else
        {
            hd_count_threshold = 94;
            coe_soft_clause_weight = 397;
            rdprob = 0.007;
            rwprob = 0.047;
            soft_smooth_probability = 0.002;
            softclause_weight_threshold = 550;
        }
    }
}

void LSworker::settings(Settings &settings){
    local_soln_feasible = 1;
    cutoff_time = settings.cutoff_time;
    hd_count_threshold = settings.hd_count_threshold;
    rdprob = settings.rdprob;
    rwprob = settings.rwprob;
    smooth_probability = settings.smooth_probability;
    soft_smooth_probability = settings.soft_smooth_probability;
    softclause_weight_threshold = settings.softclause_weight_threshold;
    h_inc = settings.h_inc;
    s_inc = settings.s_inc;
    coe_soft_clause_weight = settings.coe_soft_clause_weight;
}

void LSworker::push_best_solution_to_solver(){
    if(best_soln_feasible == 0)return;
    if(reduceInst != nullptr){
        vector<int> best_soln_vec = reduceInst->expand_solution(vector<int>(best_soln, best_soln + inst.num_vars + 1));
        solver->UpdateBestSolution(best_soln_vec, opt_unsat_weight + reduceInst->base_cost);
        solver->solpool.PushSolutionByCost(Solution(best_soln_vec, opt_unsat_weight + reduceInst->base_cost, best_soln_feasible ));
    } else {
        vector<int> best_soln_vec = vector<int>(best_soln, best_soln + inst.num_vars + 1);
        solver->UpdateBestSolution(best_soln_vec, opt_unsat_weight);
        solver->solpool.PushSolutionByCost(Solution(best_soln_vec, opt_unsat_weight, best_soln_feasible ));
    }
}