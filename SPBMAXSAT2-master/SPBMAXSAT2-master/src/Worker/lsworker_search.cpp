#include "Worker/lsworker.h"
#include "Worker/deci.h"
#include "BasicStruct/solutionpool.h"
#include "Solver/solver.h"
#include "Util/timer.h"
#include "Crossover/crossover.h"
#include <assert.h>

Solution LSworker::local_search_with_decimation(){
    double ls_start_time = util::global_elapsed_seconds();
    Solution best_solution(inst.num_vars);
    Decimation deci(inst.var_lit, inst.var_lit_count, inst.clause_lit, inst.org_clause_weight, inst.top_clause_weight);
    deci.make_space(inst.num_clauses, inst.num_vars);
    total_step = 0;
    opt_unsat_weight = __LONG_LONG_MAX__;
    for (tries = 1; tries < max_tries; ++tries)
    {
        deci.init(local_opt_soln, best_soln, inst.unit_clause, inst.unit_clause_count, inst.clause_lit_count);
        deci.unit_prosess();
        init(deci.fix);
        if(util::global_timeout()){
            push_best_solution_to_solver();
            return best_solution;
        }
        // if(tries == 1 || best_soln_feasible==0||solver->solpool.size()<2){
        //     deci.init(local_opt_soln, best_soln, inst.unit_clause, inst.unit_clause_count, inst.clause_lit_count);
        //     deci.unit_prosess();
        //     init(deci.fix);
        // }
        // else{
        //     Crossover crossover(inst);
        //     int rd = rand() % solver->solpool.size();
        //     Solution parent1 = solver->solpool.solutions[rd % solver->solpool.size()];
        //     Solution parent2 = solver->solpool.solutions[(rd+1) % solver->solpool.size()];
        //     Offspring child = crossover.Crossover3(parent1, parent2);
        //     init(child.assignment);
        // }

        long long local_opt = __LONG_LONG_MAX__;
        max_flips = max_non_improve_flip;
        for (step = 1; step < max_flips; ++step)
        {
            if (hard_unsat_nb == 0)
            {
                if (best_soln_feasible == 0)
                {
                    best_soln_feasible = 1;
                    // break;
                }
                local_soln_feasible = 1;
                if (local_opt > soft_unsat_weight)
                {
                    local_opt = soft_unsat_weight;
                    max_flips = step + max_non_improve_flip;
                }
                if (soft_unsat_weight < opt_unsat_weight)
                {
                    opt_time_gap = util::global_elapsed_seconds() - opt_time;
                    opt_time = util::global_elapsed_seconds() ;
                    cout << "o " << soft_unsat_weight << " " << total_step << " " << tries << " " << opt_time << endl;
                    //cout << "o " << soft_unsat_weight << " " << opt_time << endl;
                    opt_unsat_weight = soft_unsat_weight;
                    for (int v = 1; v <= inst.num_vars; ++v)
                        best_soln[v] = cur_soln[v];
                    push_best_solution_to_solver();
                    // vector<int> best_soln_vec(inst.num_vars + 1);
                    // for (int v = 1; v <= inst.num_vars; ++v)
                    //     best_soln_vec[v] = best_soln[v];
                    // solver->UpdateBestSolution(best_soln_vec, opt_unsat_weight);
                    // for (int v = 1; v <= inst.num_vars; ++v)
                    //     best_solution.assignment[v] = best_soln[v];
                    // solver->solpool.PushSolutionByCost(Solution(best_soln_vec, opt_unsat_weight, best_soln_feasible));
                    
                    // if (opt_unsat_weight <= best_known || best_known == -1)
                    // {
                    //     cout << "c best solution found." << endl;
                    //     if (opt_unsat_weight < best_known)
                    //     {
                    //         cout << "c a better solution " << opt_unsat_weight << endl;
                    //     }
                    //     return;
                    // }
                }
                if (soft_unsat_weight == 0)
                {
                    push_best_solution_to_solver();
                    return best_solution;
                }
            }
            // if(goodvar_stack_fill_pointer==0) cout<<step<<": 0"<<endl;
            if (step % 1000 == 0)
            {
                if (util::global_timeout())
                {
                    push_best_solution_to_solver();
                    return best_solution;
                }
                if (util::global_elapsed_seconds() - opt_time > 3)
                {
                    // if (solver->solpool.size() >= 5 || solver->solpool.solutions.size() >= 3)
                    if (solver->solpool.size() >= 5)
                    {
                        double elapse_time = util::global_elapsed_seconds() - ls_start_time;
                        if (elapse_time >= cutoff_time)
                        {
                            push_best_solution_to_solver();
                            return best_solution;
                        }
                        else if (opt_unsat_weight == 0)
                        {
                            push_best_solution_to_solver();
                            return best_solution;
                        }
                    }
                }
            }
            int flipvar = pick_var();
            // 若已经没有未满足子句且 goodvar 栈也为空，pick_var 会返回 -1，此时结束当前尝试
            if (flipvar == -1){
                break;
            }
            flip(flipvar);
            time_stamp[flipvar] = step;
            total_step++;
        }
    }
    if (best_soln_feasible){
        best_solution.feasible = best_soln_feasible;
        best_solution.cost = opt_unsat_weight;
        for (int v = 1; v <= inst.num_vars; ++v)
            best_solution.assignment[v] = best_soln[v];
    }
    return best_solution;
}

void LSworker::local_search_with_init_solution(vector<int> &init_solution,
    int basic_cost, bool preserve_clause_weights){
    // 记录本次 reduced 实例局部搜索的起始时间
    double ls_start_time = util::global_elapsed_seconds();
    assert(init_solution.size() >= inst.num_vars + 1);
    total_step = 0;
    init_seconds_total = 0.0;
    opt_improvements = 0;
    target_reached = false;
    // opt_time is only written when a new incumbent appears; without this reset
    // a call that finds nothing returns whatever the previous call left there
    // (uninitialised on the first call, which serialised as -nan).
    opt_time = 0.0;
    opt_unsat_weight = __LONG_LONG_MAX__;
    // Each coordinator window starts from a new assignment and has an
    // independent incumbent.  Clause weights are the sole state optionally
    // retained across windows.
    best_soln_feasible = 0;
    local_soln_feasible = 0;
    for (tries = 1; tries < max_tries; ++tries)
    {
        const double init_start = util::global_elapsed_seconds();
        init(init_solution, preserve_clause_weights);
        init_seconds_total += util::global_elapsed_seconds() - init_start;
        long long local_opt = __LONG_LONG_MAX__;
        max_flips = max_non_improve_flip;
        for (step = 1; step < max_flips; ++step)
        {
            if (hard_unsat_nb == 0)
            {
                local_soln_feasible = 1;
                
                if (local_opt > soft_unsat_weight)
                {
                    local_opt = soft_unsat_weight;
                    max_flips = step + max_non_improve_flip;
                }
                if (soft_unsat_weight < opt_unsat_weight)
                {
                    opt_improvements++;
                    opt_time = util::global_elapsed_seconds() - ls_start_time;
                    if (target_cost >= 0 && soft_unsat_weight <= target_cost)
                    {
                        // Reached the bound CASH already proved: this model is
                        // optimal, so return it now instead of burning the rest
                        // of the window.
                        target_reached = true;
                        for (int v = 1; v <= inst.num_vars; ++v)
                            best_soln[v] = cur_soln[v];
                        push_best_solution_to_solver();
                        return;
                    }
                    //cout << "o " << soft_unsat_weight + basic_cost << " " << total_step << " " << tries << " " << opt_time << endl;
                    //cout << "o " << soft_unsat_weight << " " << opt_time << endl;
                    opt_unsat_weight = soft_unsat_weight;
                    for (int v = 1; v <= inst.num_vars; ++v)
                        best_soln[v] = cur_soln[v];
                    
                    // if (opt_unsat_weight <= best_known || best_known == -1)
                    // {
                    //     cout << "c best solution found." << endl;
                    //     if (opt_unsat_weight < best_known)
                    //     {
                    //         cout << "c a better solution " << opt_unsat_weight << endl;
                    //     }
                    //     return;
                    // }
                }
                if (best_soln_feasible == 0)
                {
                    best_soln_feasible = 1;
                    // break;
                }
                // reduced 实例上所有子句都满足（软代价为 0），可以直接结束当前尝试
                if (soft_unsat_weight == 0)
                {
                    push_best_solution_to_solver();
                    return;
                }
            }
            // if(goodvar_stack_fill_pointer==0) cout<<step<<": 0"<<endl;
            if (step % 1000 == 0)
            {
                if(util::global_timeout()){
                    push_best_solution_to_solver();
                    return;
                }
                double elapse_time = util::global_elapsed_seconds() - ls_start_time;
                if (elapse_time >= cutoff_time){
                    push_best_solution_to_solver();
                    return;
                }
                else if (opt_unsat_weight == 0){
                    push_best_solution_to_solver();
                    return;
                }
            }
            // cout << "c step " << step << ", hard unsat nb: " << hard_unsat_nb << ", soft unsat weight: " << soft_unsat_weight << endl;
            
            int flipvar = pick_var();
            
            if (flipvar == -1)
            {
                //cout << "c no more unsatisfied clauses, stop local search" << endl;
                break;
            }
            // cout << "c flip var: " << flipvar << endl;
            
            flip(flipvar);
            
            time_stamp[flipvar] = step;
            total_step++;
        }
    }
    push_best_solution_to_solver();
}
