#ifndef _SOLVER_H_
#define _SOLVER_H_

#include "Worker/lsworker.h"
#include "BasicStruct/instance.h"
#include "BasicStruct/settings.h"
#include "BasicStruct/solutionpool.h"

using namespace std;

class Solver
{
public:
    const char *file_name;
    Instance originInstance;
    Settings settings;
    Solution best_sol = Solution();
    SolutionPool solpool;
    void BuildInstance(const char *filename);
    Solver();
    ~Solver();

    bool ParseParameters(int argc, char **argv);
    
    void Solve();

    void OnlySearch();

    void Solve_test();

    void UpdateBestSolution(const vector<int> &solution, long long cost);

    void SimplePrint();

    long long VerifyBestSolution();
private:
    void LocalSearchWithGen();
};

#endif // _SOLVER_H_
