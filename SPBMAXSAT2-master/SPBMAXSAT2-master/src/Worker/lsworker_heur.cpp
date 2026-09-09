#include "Worker/lsworker.h"
#include "Worker/deci.h"
#include "BasicStruct/solutionpool.h"
#include "Solver/solver.h"
#include "Util/timer.h"
#include "Crossover/crossover.h"
#include <assert.h>

void LSworker::init(vector<int> &init_solution){
    soft_large_weight_clauses_count = 0;

    if (1 == inst.problem_weighted) // weighted partial MaxSAT
    {
        if (0 != inst.num_hclauses)
        {
            if ((0 == local_soln_feasible || 0 == best_soln_feasible))
            {
                for (int c = 0; c < inst.num_clauses; c++)
                {
                    already_in_soft_large_weight_stack[c] = 0;
                    if (inst.org_clause_weight[c] == inst.top_clause_weight)
                        clause_weight[c] = 1;
                    else
                        clause_weight[c] = 0;
                }
            }
            else
            {
                for (int c = 0; c < inst.num_clauses; c++)
                {
                    already_in_soft_large_weight_stack[c] = 0;
                    if (inst.org_clause_weight[c] == inst.top_clause_weight)
                        clause_weight[c] = 1;
                    else
                    {
                        clause_weight[c] = tuned_org_clause_weight[c];
                        if (clause_weight[c] > s_inc && already_in_soft_large_weight_stack[c] == 0)
                        {
                            already_in_soft_large_weight_stack[c] = 1;
                            soft_large_weight_clauses[soft_large_weight_clauses_count++] = c;
                        }
                    }
                }
            }
        }
        else
        {
            for (int c = 0; c < inst.num_clauses; c++)
            {
                already_in_soft_large_weight_stack[c] = 0;
                clause_weight[c] = tuned_org_clause_weight[c];
                if (clause_weight[c] > s_inc && already_in_soft_large_weight_stack[c] == 0)
                {
                    already_in_soft_large_weight_stack[c] = 1;
                    soft_large_weight_clauses[soft_large_weight_clauses_count++] = c;
                }
            }
        }
    }
    else // unweighted partial MaxSAT
    {
        for (int c = 0; c < inst.num_clauses; c++)
        {
            already_in_soft_large_weight_stack[c] = 0;

            if (inst.org_clause_weight[c] == inst.top_clause_weight)
                clause_weight[c] = 1;
            else
            {
                if ((0 == local_soln_feasible || 0 == best_soln_feasible) && inst.num_hclauses > 0)
                {
                    clause_weight[c] = 1;
                }
                else
                {
                    clause_weight[c] = coe_soft_clause_weight;
                    if (clause_weight[c] > 1 && already_in_soft_large_weight_stack[c] == 0)
                    {
                        already_in_soft_large_weight_stack[c] = 1;
                        soft_large_weight_clauses[soft_large_weight_clauses_count++] = c;
                    }
                }
            }
        }
    }

    if (init_solution.size() == 0)
    {
        for (int v = 1; v <= inst.num_vars; v++)
        {
            cur_soln[v] = rand() % 2;
            time_stamp[v] = 0;
            unsat_app_count[v] = 0;
        }
    }
    else
    {
        for (int v = 1; v <= inst.num_vars; v++)
        {
            cur_soln[v] = init_solution[v];
            if (cur_soln[v] != 0 && cur_soln[v] != 1)
                cur_soln[v] = rand() % 2;
            time_stamp[v] = 0;
            unsat_app_count[v] = 0;
        }
    }
    local_soln_feasible = 0;
    // init stacks
    hard_unsat_nb = 0;
    soft_unsat_weight = 0;
    hardunsat_stack_fill_pointer = 0;
    softunsat_stack_fill_pointer = 0;
    unsatvar_stack_fill_pointer = 0;
    large_weight_clauses_count = 0;

    /* figure out sat_count, sat_var and init unsat_stack */
    for (int c = 0; c < inst.num_clauses; ++c)
    {
        sat_count[c] = 0;
        for (int j = 0; j < inst.clause_lit_count[c]; ++j)
        {
            if (cur_soln[inst.clause_lit[c][j].var_num] == inst.clause_lit[c][j].sense)
            {
                sat_count[c]++;
                sat_var[c] = inst.clause_lit[c][j].var_num;
            }
        }
        if (sat_count[c] == 0)
        {
            unsat(c);
        }
    }

    /*figure out score*/
    for (int v = 1; v <= inst.num_vars; v++)
    {
        score[v] = 0.0;
        for (int i = 0; i < inst.var_lit_count[v]; ++i)
        {
            int c = inst.var_lit[v][i].clause_num;
            if (sat_count[c] == 0)
                score[v] += clause_weight[c];
            else if (sat_count[c] == 1 && inst.var_lit[v][i].sense == cur_soln[v])
                score[v] -= clause_weight[c];
        }
    }

    // init goodvars stack
    goodvar_stack_fill_pointer = 0;
    for (int v = 1; v <= inst.num_vars; v++)
    {
        if (score[v] > 0)
        {
            already_in_goodvar_stack[v] = goodvar_stack_fill_pointer;
            mypush(v, goodvar_stack);
        }
        else
            already_in_goodvar_stack[v] = -1;
    }
}


void LSworker::hard_increase_weights(){
    int i, c, v;
    for (i = 0; i < hardunsat_stack_fill_pointer; ++i)
    {
        c = hardunsat_stack[i];
        
        clause_weight[c] += h_inc;

        if (clause_weight[c] == (h_inc + 1))
            large_weight_clauses[large_weight_clauses_count++] = c;

        for (lit *p = inst.clause_lit[c]; (v = p->var_num) != 0; p++)
        {
            score[v] += h_inc;
            if (score[v] > 0 && already_in_goodvar_stack[v] == -1)
            {
                already_in_goodvar_stack[v] = goodvar_stack_fill_pointer;
                mypush(v, goodvar_stack);
            }
        }
    }
    return;
}

void LSworker::soft_increase_weights(){
    int i, c, v;

    if (1 == inst.problem_weighted)
    {
        for (i = 0; i < inst.num_sclauses; ++i)
        {
            c = inst.soft_clause_num_index[i];

            double inc = soft_increase_ratio * (clause_weight[c] + tuned_org_clause_weight[c]) - clause_weight[c];

            clause_weight[c] += inc;
            if (sat_count[c] <= 0) // unsat
            {
                for (lit *p = inst.clause_lit[c]; (v = p->var_num) != 0; p++)
                {
                    score[v] += inc;
                    if (score[v] > 0 && already_in_goodvar_stack[v] == -1)
                    {
                        already_in_goodvar_stack[v] = goodvar_stack_fill_pointer;
                        mypush(v, goodvar_stack);
                    }
                }
            }
            else if (sat_count[c] < 2) // sat
            {
                v = sat_var[c];
                score[v] -= inc;
                if (score[v] <= 0 && -1 != already_in_goodvar_stack[v])
                {
                    int index = already_in_goodvar_stack[v];
                    int last_v = mypop(goodvar_stack);
                    goodvar_stack[index] = last_v;
                    already_in_goodvar_stack[last_v] = index;
                    already_in_goodvar_stack[v] = -1;
                }
            }
        }
    }
    else
    {
        for (i = 0; i < inst.num_sclauses; ++i)
        {
            c = inst.soft_clause_num_index[i];

            double inc = soft_increase_ratio * (clause_weight[c] + s_inc) - clause_weight[c];

            clause_weight[c] += inc;

            if (sat_count[c] <= 0) // unsat
            {
                for (lit *p = inst.clause_lit[c]; (v = p->var_num) != 0; p++)
                {
                    score[v] += inc;
                    if (score[v] > 0 && already_in_goodvar_stack[v] == -1)
                    {
                        already_in_goodvar_stack[v] = goodvar_stack_fill_pointer;
                        mypush(v, goodvar_stack);
                    }
                }
            }
            else if (sat_count[c] < 2) // sat
            {
                v = sat_var[c];
                score[v] -= inc;
                if (score[v] <= 0 && -1 != already_in_goodvar_stack[v])
                {
                    int index = already_in_goodvar_stack[v];
                    int last_v = mypop(goodvar_stack);
                    goodvar_stack[index] = last_v;
                    already_in_goodvar_stack[last_v] = index;
                    already_in_goodvar_stack[v] = -1;
                }
            }
        }
    }
    return;
}

void LSworker::soft_smooth_weights()
{
    int i, clause, v;

    for (i = 0; i < soft_large_weight_clauses_count; i++)
    {
        clause = soft_large_weight_clauses[i];
        if (sat_count[clause] > 0)
        {
            clause_weight[clause] -= s_inc;
            if (clause_weight[clause] <= s_inc && already_in_soft_large_weight_stack[clause] == 1)
            {
                already_in_soft_large_weight_stack[clause] = 0;
                soft_large_weight_clauses[i] = soft_large_weight_clauses[--soft_large_weight_clauses_count];
                i--;
            }
            if (sat_count[clause] == 1)
            {
                v = sat_var[clause];
                score[v] += s_inc;
                if (score[v] > 0 && already_in_goodvar_stack[v] == -1)
                {
                    already_in_goodvar_stack[v] = goodvar_stack_fill_pointer;
                    mypush(v, goodvar_stack);
                }
            }
        }
    }
    return;
}

void LSworker::update_clause_weights()
{
    if (inst.num_hclauses > 0)
    {
        hard_increase_weights();

        if (soft_unsat_weight >= opt_unsat_weight){ //The SPB constraint is falsified
            soft_increase_weights();                //update w(SPB)
        }    
    }
    else
    {
        if (((rand() % MY_RAND_MAX_INT) * BASIC_SCALE) < soft_smooth_probability && soft_large_weight_clauses_count > soft_large_clause_count_threshold)
        {
            soft_smooth_weights();
        }
        else
        {
            soft_increase_weights_not_partial();
        }
    }
}

inline void LSworker::unsat(int clause)
{
	if (inst.org_clause_weight[clause] == inst.top_clause_weight)
	{
		index_in_hardunsat_stack[clause] = hardunsat_stack_fill_pointer;
		mypush(clause, hardunsat_stack);
		hard_unsat_nb++;
	}
	else
	{
		index_in_softunsat_stack[clause] = softunsat_stack_fill_pointer;
		mypush(clause, softunsat_stack);
		soft_unsat_weight += inst.org_clause_weight[clause];
	}
}

inline void LSworker::sat(int clause)
{
	int index, last_unsat_clause;

	if (inst.org_clause_weight[clause] == inst.top_clause_weight)
	{
		last_unsat_clause = mypop(hardunsat_stack);
		index = index_in_hardunsat_stack[clause];
		hardunsat_stack[index] = last_unsat_clause;
		index_in_hardunsat_stack[last_unsat_clause] = index;

		hard_unsat_nb--;
	}
	else
	{
		last_unsat_clause = mypop(softunsat_stack);
		index = index_in_softunsat_stack[clause];
		softunsat_stack[index] = last_unsat_clause;
		index_in_softunsat_stack[last_unsat_clause] = index;

		soft_unsat_weight -= inst.org_clause_weight[clause];
	}
}

void LSworker::flip(int flipvar)
{
	int i, v, c;
	int index;
	lit *clause_c;

	double org_flipvar_score = score[flipvar];
	//cout << "c org_flipvar_score: " <<org_flipvar_score << endl;
	cur_soln[flipvar] = 1 - cur_soln[flipvar];

	for (i = 0; i < inst.var_lit_count[flipvar]; ++i)
	{
		c = inst.var_lit[flipvar][i].clause_num;
		clause_c = inst.clause_lit[c];

		if (cur_soln[flipvar] == inst.var_lit[flipvar][i].sense)
		{
			++sat_count[c];
			if (sat_count[c] == 2) //sat_count from 1 to 2
			{
				score[sat_var[c]] += clause_weight[c];
				if (score[sat_var[c]] > 0 && -1 == already_in_goodvar_stack[sat_var[c]])
				{
					already_in_goodvar_stack[sat_var[c]] = goodvar_stack_fill_pointer;
					mypush(sat_var[c], goodvar_stack);
				}
			}
			else if (sat_count[c] == 1) // sat_count from 0 to 1
			{
				sat_var[c] = flipvar; //record the only true lit's var
				for (lit *p = clause_c; (v = p->var_num) != 0; p++)
				{
					score[v] -= clause_weight[c];
					if (score[v] <= 0 && -1 != already_in_goodvar_stack[v])
					{
						int index = already_in_goodvar_stack[v];
						int last_v = mypop(goodvar_stack);
						goodvar_stack[index] = last_v;
						already_in_goodvar_stack[last_v] = index;
						already_in_goodvar_stack[v] = -1;
					}
				}
				sat(c);
			}
		}
		else // cur_soln[flipvar] != cur_lit.sense
		{
			--sat_count[c];
			if (sat_count[c] == 1) //sat_count from 2 to 1
			{
				for (lit *p = clause_c; (v = p->var_num) != 0; p++)
				{
					if (p->sense == cur_soln[v])
					{
						score[v] -= clause_weight[c];
						if (score[v] <= 0 && -1 != already_in_goodvar_stack[v])
						{
							int index = already_in_goodvar_stack[v];
							int last_v = mypop(goodvar_stack);
							goodvar_stack[index] = last_v;
							already_in_goodvar_stack[last_v] = index;
							already_in_goodvar_stack[v] = -1;
						}
						sat_var[c] = v;
						break;
					}
				}
			}
			else if (sat_count[c] == 0) //sat_count from 1 to 0
			{
				for (lit *p = clause_c; (v = p->var_num) != 0; p++)
				{
					score[v] += clause_weight[c];
					if (score[v] > 0 && -1 == already_in_goodvar_stack[v])
					{
						already_in_goodvar_stack[v] = goodvar_stack_fill_pointer;
						mypush(v, goodvar_stack);
					}
				}
				unsat(c);
			} //end else if
		}	 //end else
	}

	//update information of flipvar
	score[flipvar] = -org_flipvar_score;
	if (score[flipvar] > 0 && already_in_goodvar_stack[flipvar] == -1)
	{
		already_in_goodvar_stack[flipvar] = goodvar_stack_fill_pointer;
		mypush(flipvar, goodvar_stack);
	}
	else if (score[flipvar] <= 0 && already_in_goodvar_stack[flipvar] != -1)
	{
		int index = already_in_goodvar_stack[flipvar];
		int last_v = mypop(goodvar_stack);
		goodvar_stack[index] = last_v;
		already_in_goodvar_stack[last_v] = index;
		already_in_goodvar_stack[flipvar] = -1;
	}
	//update_goodvarstack1(flipvar);
}

void LSworker::update_goodvarstack1(int flipvar)
{
	int v;
	//remove the vars no longer goodvar in goodvar stack
	for (int index = goodvar_stack_fill_pointer - 1; index >= 0; index--)
	{
		v = goodvar_stack[index];
		if (score[v] <= 0)
		{
			goodvar_stack[index] = mypop(goodvar_stack);
			already_in_goodvar_stack[v] = -1;
		}
	}

	//add goodvar
	for (int i = 0; i < inst.var_neighbor_count[flipvar]; ++i)
	{
		v = inst.var_neighbor[flipvar][i];
		if (score[v] > 0)
		{
			if (already_in_goodvar_stack[v] == -1)
			{
				already_in_goodvar_stack[v] = goodvar_stack_fill_pointer;
				mypush(v, goodvar_stack);
			}
		}
	}
}
void LSworker::update_goodvarstack2(int flipvar)
{
	if (score[flipvar] > 0 && already_in_goodvar_stack[flipvar] == -1)
	{
		already_in_goodvar_stack[flipvar] = goodvar_stack_fill_pointer;
		mypush(flipvar, goodvar_stack);
	}
	else if (score[flipvar] <= 0 && already_in_goodvar_stack[flipvar] != -1)
	{
		int index = already_in_goodvar_stack[flipvar];
		int last_v = mypop(goodvar_stack);
		goodvar_stack[index] = last_v;
		already_in_goodvar_stack[last_v] = index;
		already_in_goodvar_stack[flipvar] = -1;
	}
	int i, v;
	for (i = 0; i < inst.var_neighbor_count[flipvar]; ++i)
	{
		v = inst.var_neighbor[flipvar][i];
		if (score[v] > 0)
		{
			if (already_in_goodvar_stack[v] == -1)
			{
				already_in_goodvar_stack[v] = goodvar_stack_fill_pointer;
				mypush(v, goodvar_stack);
			}
		}
		else if (already_in_goodvar_stack[v] != -1)
		{
			int index = already_in_goodvar_stack[v];
			int last_v = mypop(goodvar_stack);
			goodvar_stack[index] = last_v;
			already_in_goodvar_stack[last_v] = index;
			already_in_goodvar_stack[v] = -1;
		}
	}
}

int LSworker::pick_var()
{
    int i, v;
    int best_var;
    int sel_c;
    lit *p;

    if (goodvar_stack_fill_pointer > 0)
    {
        int best_array_count = 0;
        if ((rand() % MY_RAND_MAX_INT) * BASIC_SCALE < rdprob)
            return goodvar_stack[rand() % goodvar_stack_fill_pointer];

        if (goodvar_stack_fill_pointer < hd_count_threshold)
        {
            best_var = goodvar_stack[0];

            for (i = 1; i < goodvar_stack_fill_pointer; ++i)
            {
                v = goodvar_stack[i];
                if (score[v] > score[best_var])
                {
                    best_var = v;
                }
                else if (score[v] == score[best_var])
                {
                    if (time_stamp[v] < time_stamp[best_var])
                    {
                        best_var = v;
                    }
                }
            }
            return best_var; // best_array[rand() % best_array_count];
        }
        else
        {
            best_var = goodvar_stack[rand() % goodvar_stack_fill_pointer];

            for (i = 1; i < hd_count_threshold; ++i)
            {
                v = goodvar_stack[rand() % goodvar_stack_fill_pointer];
                if (score[v] > score[best_var])
                {
                    best_var = v;
                }
                else if (score[v] == score[best_var])
                {
                    if (time_stamp[v] < time_stamp[best_var])
                    {
                        best_var = v;
                    }
                }
            }
            return best_var; // best_array[rand() % best_array_count];
        }
    }

    update_clause_weights();

    // 如果已经没有任何未满足的硬/软子句，则表示当前赋值已满足所有子句，返回 -1 让上层停止搜索
    if (hardunsat_stack_fill_pointer == 0 && softunsat_stack_fill_pointer == 0)
    {
        if (debug_mode)
            cout << "c no unsatisfied clauses, pick_var returns -1" << endl;
        return -1;
    }

    if (hardunsat_stack_fill_pointer > 0)
    {
        sel_c = hardunsat_stack[rand() % hardunsat_stack_fill_pointer];
    }
    else
    {
        if(debug_mode)cout << "c softunsat_stack_fill_pointer: " << softunsat_stack_fill_pointer << endl;
        while(1){
            sel_c = softunsat_stack[rand() % softunsat_stack_fill_pointer];
            if (inst.clause_lit_count[sel_c] != 0)
                break;
        }
    }
    if ((rand() % MY_RAND_MAX_INT) * BASIC_SCALE < rwprob){
        return inst.clause_lit[sel_c][rand() % inst.clause_lit_count[sel_c]].var_num;
    }
        

    best_var = inst.clause_lit[sel_c][0].var_num;
    p = inst.clause_lit[sel_c];
    for (p++; (v = p->var_num) != 0; p++)
    {
        if (score[v] > score[best_var])
            best_var = v;
        else if (score[v] == score[best_var])
        {
            if (time_stamp[v] < time_stamp[best_var])
                best_var = v;
        }
    }
    return best_var;
}


void LSworker::soft_increase_weights_not_partial()
{
    int i, c, v;

    if (1 == inst.problem_weighted)
    {
        for (i = 0; i < softunsat_stack_fill_pointer; ++i)
        {
            c = softunsat_stack[i];
            if (clause_weight[c] >= tuned_org_clause_weight[c] + softclause_weight_threshold)
                continue;
            else
                clause_weight[c] += s_inc;

            if (clause_weight[c] > s_inc && already_in_soft_large_weight_stack[c] == 0)
            {
                already_in_soft_large_weight_stack[c] = 1;
                soft_large_weight_clauses[soft_large_weight_clauses_count++] = c;
            }
            for (lit *p = inst.clause_lit[c]; (v = p->var_num) != 0; p++)
            {
                score[v] += s_inc;
                if (score[v] > 0 && already_in_goodvar_stack[v] == -1)
                {
                    already_in_goodvar_stack[v] = goodvar_stack_fill_pointer;
                    mypush(v, goodvar_stack);
                }
            }
        }
    }
    else
    {
        for (i = 0; i < softunsat_stack_fill_pointer; ++i)
        {
            c = softunsat_stack[i];
            if (clause_weight[c] >= coe_soft_clause_weight + softclause_weight_threshold)
                continue;
            else
                clause_weight[c] += s_inc;

            if (clause_weight[c] > s_inc && already_in_soft_large_weight_stack[c] == 0)
            {
                already_in_soft_large_weight_stack[c] = 1;
                soft_large_weight_clauses[soft_large_weight_clauses_count++] = c;
            }
            for (lit *p = inst.clause_lit[c]; (v = p->var_num) != 0; p++)
            {
                score[v] += s_inc;
                if (score[v] > 0 && already_in_goodvar_stack[v] == -1)
                {
                    already_in_goodvar_stack[v] = goodvar_stack_fill_pointer;
                    mypush(v, goodvar_stack);
                }
            }
        }
    }
    return;
}



