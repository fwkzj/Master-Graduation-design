#include "Solver/solver.h"
#include "Worker/lsworker.h"
#include "BasicStruct/solutionpool.h"
#include "Crossover/crossover.h"
#include <assert.h>

Solver::Solver() {}

Solver::~Solver() {}

void Solver::BuildInstance(const char *filename)
{
    originInstance.build_instance(filename);
}

bool Solver::ParseParameters(int argc, char **argv)
{
    int i = 0;
    int temp_para = 0;
    // cout << "c test:" << endl;
    for (i = 1; i < argc; i++)
    {
        if (0 == strcmp(argv[i], "-rdprob"))
        {
            i++;
            if (i >= argc)
                return false;
            sscanf(argv[i], "%f", &settings.rdprob);
        }
        else if (0 == strcmp(argv[i], "-bms_num"))
        {
            i++;
            if (i >= argc)
                return false;
            sscanf(argv[i], "%d", &settings.hd_count_threshold);
        }
        else if (0 == strcmp(argv[i], "-rwprob"))
        {
            i++;
            if (i >= argc)
                return false;
            sscanf(argv[i], "%f", &settings.rwprob);
        }
        else if (0 == strcmp(argv[i], "-hard_sp"))
        {
            i++;
            if (i >= argc)
                return false;
            sscanf(argv[i], "%f", &settings.smooth_probability);
        }
        else if (0 == strcmp(argv[i], "-soft_sp"))
        {
            i++;
            if (i >= argc)
                return false;
            sscanf(argv[i], "%f", &settings.soft_smooth_probability);
            cout << "c ssp: " << settings.soft_smooth_probability << endl;
        }
        else if (0 == strcmp(argv[i], "-soft_weight_threshold"))
        {
            i++;
            if (i >= argc)
                return false;
            sscanf(argv[i], "%lf", &settings.softclause_weight_threshold);
            cout << "c soft_weight_threshold: " << settings.softclause_weight_threshold << endl;
        }
        else if (0 == strcmp(argv[i], "-h_inc"))
        {
            i++;
            if (i >= argc)
                return false;
            sscanf(argv[i], "%lf", &settings.h_inc);
        }
        else if (0 == strcmp(argv[i], "-s_inc"))
        {
            i++;
            if (i >= argc)
                return false;
            sscanf(argv[i], "%lf", &settings.s_inc);
        }
        else if (0 == strcmp(argv[i], "-coe"))
        {
            i++;
            if (i >= argc)
                return false;
            sscanf(argv[i], "%d", &settings.coe_soft_clause_weight);
        }
        else if (0 == strcmp(argv[i], "-cutoff"))
        {
            i++;
            if (i >= argc)
                return false;
            sscanf(argv[i], "%d", &settings.cutoff_time);
        }
        else if (0 == strcmp(argv[i], "-pool_size"))
        {
            i++;
            if (i >= argc)
                return false;
            sscanf(argv[i], "%d", &settings.solution_pool_size);
            if (settings.solution_pool_size < 1)
                settings.solution_pool_size = 1;
        }
    }
    //print_settings(settings);
    util::set_global_cutoff(settings.cutoff_time);
    return true;
}

void Solver::Solve()
{
    Instance trans = originInstance;
    LSworker lsworker(std::move(trans));
    lsworker.set_solver(this);
    const int pool_size = settings.solution_pool_size > 0 ? settings.solution_pool_size : 1;
    solpool = SolutionPool(&originInstance, pool_size);
    lsworker.settings();
    lsworker.settings(settings);
    lsworker.set_cutoff_time(settings.cutoff_time*0.5);
    lsworker.local_search_with_decimation();
    if(util::global_timeout()){
        cout << "c timeout after initial local search." << endl;
        return;
    }
    int turns = 0;
    while(util::global_timeout() == false){
        if(solpool.size() < 2){
            cout << "c stop generation search: solution pool has " << solpool.size() << " solution(s)." << endl;
            break;
        }
        cout << "c Generation-based Local Search, turn " << turns+1 << endl;
        LocalSearchWithGen();
        turns++;
    }
}

void Solver::OnlySearch()
{
    Instance trans = originInstance;
    LSworker lsworker(std::move(trans));
    lsworker.set_solver(this);
    const int pool_size = settings.solution_pool_size > 0 ? settings.solution_pool_size : 1;
    solpool = SolutionPool(&originInstance, pool_size);
    lsworker.settings();
    lsworker.settings(settings);
    lsworker.local_search_with_decimation();
}

void Solver::Solve_test()
{
    Instance trans = originInstance;
    LSworker lsworker(std::move(trans));
    lsworker.set_solver(this);
    lsworker.settings();
    lsworker.settings(settings);
    Solution a = lsworker.local_search_with_decimation();
    cout << "c test solution cost: " << a.cost << endl;
    cout << "c test solution cost verified: " << lsworker.verify_solution(a.assignment) << endl;
    vector<int> sol = a.assignment;
    vector<int> sol2;
    sol2.clear();
    sol2.push_back(-1); // dummy for index 0
    for(int i = 1;i < a.assignment.size();i++){
        if(i % 2){
            sol[i] = -1;
            sol2.push_back(a.assignment[i]);
        }
    }

    ReducedInstance reducedInst = originInstance.reduce(sol);
    vector<int> expanded_solution = reducedInst.expand_solution(sol2);
    cout << "c expanded solution cost verified: " << originInstance.verify_solution(expanded_solution) << endl;
    for(size_t i = 1;i < expanded_solution.size();i++){
        if(expanded_solution[i] != a.assignment[i]){
            cout << "c mismatch at var " << i << ": " << expanded_solution[i] << " vs " << a.assignment[i] << endl;
        }
    }
    cout << reducedInst.base_cost << endl;
    cout << reducedInst.base_cost + reducedInst.reduced.verify_solution(sol2) << endl;
    cout << "c original instance vars: " << a.assignment.size() - 1 << endl;
    cout << "c reduced instance vars: " << reducedInst.reduced.num_vars << endl;
    return;
}

void Solver::LocalSearchWithGen(){
    int pool_size = solpool.size();
    if(pool_size < 2){
        cout << "c Skipping generation search: solution pool has " << pool_size << " solution(s)." << endl;
        return;
    }

    Crossover crossover(originInstance);
    vector<int> shuffle_indices;
    for(int i = 0; i < pool_size; ++i){
        shuffle_indices.push_back(i);
    }
    random_shuffle(shuffle_indices.begin(), shuffle_indices.end());
    for(int i = 0; i < 5; ++i){
        cout << "c Local Search with worker: " << i+1 << endl;
        Solution parent1 = solpool.solutions[shuffle_indices[i % pool_size]];
        Solution parent2 = solpool.solutions[shuffle_indices[(i+1) % pool_size]];
        cout << "c Parent costs: " << parent1.cost << ", " << parent2.cost << endl;
        Offspring child = crossover.Crossover3(parent1, parent2);
        cout << "c Child fixed count: " << child.fixed_count << endl;
        vector<int> init_solution = child.assignment;
        for(int v = 1; v <= originInstance.num_vars; ++v){
            if(!child.isfixed[v]){
                init_solution[v] = -1;
            }
        }
        ReducedInstance reducedInst = originInstance.reduce(init_solution);
        if(reducedInst.unsat){
            cout << "c Skipping child due to hard contradiction after fixing." << endl;
            continue;
        }

        int reduced_vars = reducedInst.new2old.size() - 1;
        cout << "c Child reduce stats: base_cost " << reducedInst.base_cost
             << ", reduced_vars " << reduced_vars
             << "/" << originInstance.num_vars << endl;

        if(best_sol.feasible && reducedInst.base_cost >= best_sol.cost){
            cout << "c Skipping child due to high base cost: " << reducedInst.base_cost << endl;
            continue;
        }
        init_solution.clear();
        init_solution.resize(reduced_vars + 1);
        for(int v = 1; v <= reduced_vars; ++v){
            int old_idx = reducedInst.new2old[v];
            init_solution[v] = child.assignment[old_idx];
        }
        cout << "c reduced instance vars: " << reducedInst.reduced.num_vars << endl;
        LSworker lsworker(std::move(reducedInst.reduced));
        lsworker.set_solver(this);
        lsworker.settings();
        lsworker.settings(settings);
        lsworker.set_cutoff_time(5);
        lsworker.set_reduce_instance(&reducedInst);
        //lsworker.enable_debug_mode();
        lsworker.local_search_with_init_solution(init_solution, reducedInst.base_cost);
    }
}

void Solver::UpdateBestSolution(const vector<int> &solution, long long cost){
    if(cost < 0){
        cout << "c Warning: negative cost solution ignored." << cost << endl;
        return;
    }
    if(!best_sol.feasible || cost < best_sol.cost){
        best_sol = Solution(solution, cost, true);
    }
}

long long Solver::VerifyBestSolution(){
    if(best_sol.feasible){
        return originInstance.verify_solution(best_sol.assignment);
    } else {
        return -1; // no feasible solution
    }
}

void Solver::SimplePrint(){
    if(VerifyBestSolution() == best_sol.cost)
        cout << "c bestcost " << best_sol.cost << endl;
}
