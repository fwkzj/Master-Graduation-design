#ifndef _LSWORKER_H_
#define _LSWORKER_H_

#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <queue>
#include <stdio.h>
#include <stdlib.h>

#include "BasicStruct/lit.h"
#include "BasicStruct/instance.h"
#include "BasicStruct/settings.h"
#include "BasicStruct/solutionpool.h"
#include "Crossover/crossover.h"
#include "Util/timer.h"

using namespace std;

#define mypop(stack) stack[--stack##_fill_pointer]
#define mypush(item, stack) stack[stack##_fill_pointer++] = item

const float MY_RAND_MAX_FLOAT = 10000000.0;
const int MY_RAND_MAX_INT = 10000000;
const float BASIC_SCALE = 0.0000001; //1.0f/MY_RAND_MAX_FLOAT;

class Solver;

class LSworker
{
  private:
	/***********non-algorithmic information ****************/
	bool debug_mode = false;
	Instance inst;
	Solver* solver;
	ReducedInstance *reduceInst = nullptr;
	//steps and time
	long long total_step = 0;
	int tries;
	int max_tries;
	unsigned int max_flips;
	unsigned int max_non_improve_flip;
	unsigned int step;

	int print_time;
	int cutoff_time;
	int prioup_time;
	double opt_time;
	double opt_time_gap = 1000000;
	/**********end non-algorithmic information*****************/

	/* Information about the variables. */
	double *score;
	long long *time_stamp;
	int *neighbor_flag;
	int *temp_neighbor;

	/* Information about the clauses */
	long long max_soft_weight;
	long long min_soft_weight;
	double *clause_weight;
	double *tuned_org_clause_weight;
	double coe_tuned_weight;
	int *sat_count;
	int *sat_var;

	double avg_soft_weight;
	double max_soft_clause_weight;
	double soft_increase_ratio;		//delta

	// long long *clause_selected_count;
	int *best_soft_clause;

	//unsat clauses stack
	int *hardunsat_stack;		   //store the unsat clause number
	int *index_in_hardunsat_stack; //which position is a clause in the unsat_stack
	int hardunsat_stack_fill_pointer;

	int *softunsat_stack;		   //store the unsat clause number
	int *index_in_softunsat_stack; //which position is a clause in the unsat_stack
	int softunsat_stack_fill_pointer;

	//variables in unsat clauses
	int *unsatvar_stack;
	int unsatvar_stack_fill_pointer;
	int *index_in_unsatvar_stack;
	int *unsat_app_count; //a varible appears in how many unsat clauses

	//good decreasing variables (dscore>0 and confchange=1)
	int *goodvar_stack;
	int goodvar_stack_fill_pointer;
	int *already_in_goodvar_stack;

	/* Information about solution */
	int *cur_soln; //the current solution, with 1's for True variables, and 0's for False variables
	int *best_soln;
	int *local_opt_soln;
	int best_soln_feasible = 0; //when find a feasible solution, this is marked as 1.
	int local_soln_feasible;
	int hard_unsat_nb;
	long long soft_unsat_weight;
	long long opt_unsat_weight;
	long long local_opt_unsat_weight;

	//clause weighting
	int *large_weight_clauses;
	int large_weight_clauses_count;
	int large_clause_count_threshold;

	int *soft_large_weight_clauses;
	int *already_in_soft_large_weight_stack;
	int soft_large_weight_clauses_count;
	int soft_large_clause_count_threshold;

	//tem data structure used in algorithm
	int *best_array;
	int best_count;

	//parameters used in algorithm
	float rwprob;
	float rdprob;
	float smooth_probability;
	float soft_smooth_probability;
	int hd_count_threshold;
	double h_inc;
	double s_inc;
	double h_inc_1;
	double h_inc_2;
	double s_inc_1;
	double s_inc_2;
	double softclause_weight_threshold;
	float random_prob;
	int coe_soft_clause_weight;
	//long long *soft_clause_weight_upper_bound;

	//function used in algorithm
	void allocate_memory();

	void hard_increase_weights();
	void soft_increase_weights();
	void soft_smooth_weights();
	void update_clause_weights();
	void unsat(int clause);
	void sat(int clause);
	void init(vector<int> &init_solution, bool preserve_clause_weights = false);
	void flip(int flipvar);
	void update_goodvarstack1(int flipvar);
	void update_goodvarstack2(int flipvar);
	int pick_var();
	void soft_increase_weights_not_partial();

  public:
	LSworker(Solver *solver);
	~LSworker();
	LSworker(Instance &&instance);
	void settings();
	void settings(Settings &settings);
	void set_solver(Solver *s){solver = s;}
	void set_reduce_instance(ReducedInstance *rInst){reduceInst = rInst;}
	void set_cutoff_time(int time){cutoff_time = time;}
	void free_memory();

	Solution local_search_with_decimation();
	void local_search_with_init_solution(vector<int> &init_solution,
		int basic_cost = 0, bool preserve_clause_weights = false);

	int get_best_cost(){ return opt_unsat_weight; }
	int verify_solution(const vector<int> &assignment){ return inst.verify_solution(assignment); }

	void enable_debug_mode(){ debug_mode = true; }
	void push_best_solution_to_solver();
	void simple_print(char *filename);
	void print_best_solution();
	bool verify_sol();
};

#endif
