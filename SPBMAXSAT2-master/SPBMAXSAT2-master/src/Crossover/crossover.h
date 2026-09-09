#ifndef _CROSSOVER_H_
#define _CROSSOVER_H_

#include "BasicStruct/instance.h"
#include "BasicStruct/solutionpool.h"

using namespace std;

class Offspring {
public:
    vector<int> assignment; // variable assignments, index 0 unused
    vector<bool> isfixed; // whether the variable is fixed from parents
    int fixed_count = 0;
    Offspring(int num_vars) : assignment(num_vars + 1, -1), isfixed(num_vars + 1, false) {}
};

class Crossover {
public:
    const Instance& oriInst;
    Crossover(const Instance& instance) : oriInst(instance) {}
    Offspring CrossoverSolutions(const Solution& parent1, const Solution& parent2);
    Offspring Crossover1(const Solution& parent1, const Solution& parent2);
    Offspring Crossover2(const Solution& parent1, const Solution& parent2);
    Offspring Crossover3(const Solution& parent1, const Solution& parent2);
    Offspring Crossover4(const Solution& parent1, const Solution& parent2);
};

#endif // _CROSSOVER_H_