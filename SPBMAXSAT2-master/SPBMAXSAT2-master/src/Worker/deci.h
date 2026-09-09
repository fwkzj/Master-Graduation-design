#ifndef _DECI_H_
#define _DECI_H_

#include "BasicStruct/lit.h"
#include <vector>

using namespace std;

class Decimation
{
  public:
    Decimation(lit **ls_var_lit, int *ls_var_lit_count, lit **ls_clause_lit, long long *ls_org_clause_weight, long long ls_top_clause_weight);

    void make_space(int max_c, int max_v);
    void free_memory();
    void init(int *ls_local_opt, int *ls_global_opt, lit *ls_unit_clause, int ls_unit_clause_count, int *ls_clause_lit_count);
    void push_unit_clause_to_queue(lit tem_l);
    void assign(int v, int sense);
    void remove_unassigned_var(int v);
    void hunit_propagation();
    void sunit_propagation();
    void random_propagation();
    void unit_prosess();
    bool choose_sense(int v);

    vector<int> fix;

    int num_vars;
    int num_clauses;

    long long *h_true_score;
    long long *h_false_score;
    long long *hscore;
    long long *s_true_score;
    long long *s_false_score;
    long long *sscore;

    lit **clause_lit;
    lit **var_lit;
    int *var_lit_count;

    int *local_opt;
    int *global_opt;
    long long *org_clause_weight;
    long long top_clause_weight;

    lit *hunit_clause_queue;
    int *sense_in_hunit_clause_queue;
    int hunit_beg_pointer;
    int hunit_end_pointer;

    lit *sunit_clause_queue;
    int *sense_in_sunit_clause_queue;
    int sunit_beg_pointer;
    int sunit_end_pointer;

    int *unassigned_var;
    int *index_in_unassigned_var;
    int unassigned_var_count;

    int *clause_delete;
    int *clause_lit_count;
};

#endif
