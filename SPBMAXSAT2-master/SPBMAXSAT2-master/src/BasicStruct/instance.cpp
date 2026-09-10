#include "BasicStruct/instance.h"

#include <cstdlib>
#include <stdexcept>

Instance::Instance()
{
    // Initialize pointers to null
    var_lit = nullptr;
    var_lit_count = nullptr;
    clause_lit = nullptr;
    clause_lit_count = nullptr;
    unit_clause = nullptr;
    unit_soft_clause = nullptr;
    org_clause_weight = nullptr;
    var_neighbor = nullptr;
    var_neighbor_count = nullptr;
    soft_clause_num_index = nullptr;
    temp_lit = nullptr;

    // Initialize counts and properties
    num_vars = 0;
    num_clauses = 0;
    num_hclauses = 0;
    num_sclauses = 0;
    problem_weighted = 0;
    top_clause_weight = 0;
    total_soft_weight = 0;
    total_hard_length = 0;
    total_soft_length = 0;
    fixed_soft_cost = 0;
    has_empty_hard_clause = false;
    unit_clause_count = 0;
    unit_soft_clause_count = 0;
}

Instance::~Instance()
{
    free_memory();
}

// Deep copy constructor
Instance::Instance(const Instance &other)
{
    // Copy scalar fields
    problem_weighted   = other.problem_weighted;
    num_vars           = other.num_vars;
    num_clauses        = other.num_clauses;
    num_hclauses       = other.num_hclauses;
    num_sclauses       = other.num_sclauses;
    top_clause_weight  = other.top_clause_weight;
    total_soft_weight  = other.total_soft_weight;
    total_hard_length  = other.total_hard_length;
    total_soft_length  = other.total_soft_length;
    fixed_soft_cost    = other.fixed_soft_cost;
    has_empty_hard_clause = other.has_empty_hard_clause;
    unit_clause_count  = other.unit_clause_count;
    unit_soft_clause_count = other.unit_soft_clause_count;

    // Allocate top-level arrays based on num_vars/num_clauses
    allocate_memory();

    // Copy clause-related structures
    for (int c = 0; c < num_clauses; ++c)
    {
        clause_lit_count[c] = other.clause_lit_count[c];

        if (clause_lit_count[c] > 0 && other.clause_lit[c] != nullptr)
        {
            clause_lit[c] = new lit[clause_lit_count[c] + 1];
            for (int i = 0; i <= clause_lit_count[c]; ++i)
            {
                clause_lit[c][i] = other.clause_lit[c][i];
            }
        }
        else
        {
            clause_lit[c] = nullptr;
        }

        org_clause_weight[c] = other.org_clause_weight[c];
    }

    // Copy unit clauses
    for (int i = 0; i < unit_clause_count; ++i)
    {
        unit_clause[i] = other.unit_clause[i];
    }

    // Copy unit soft clauses
    for (int i = 0; i < unit_soft_clause_count; ++i)
    {
        unit_soft_clause[i] = other.unit_soft_clause[i];
    }
    // Copy soft clause indices
    for (int i = 0; i < num_sclauses; ++i)
    {
        soft_clause_num_index[i] = other.soft_clause_num_index[i];
    }

    // Copy variable literal arrays
    for (int v = 1; v <= num_vars; ++v)
    {
        var_lit_count[v] = other.var_lit_count[v];
        if (var_lit_count[v] > 0 && other.var_lit[v] != nullptr)
        {
            var_lit[v] = new lit[var_lit_count[v] + 1];
            for (int i = 0; i <= var_lit_count[v]; ++i)
            {
                var_lit[v][i] = other.var_lit[v][i];
            }
        }
        else
        {
            var_lit[v] = nullptr;
        }
    }

    // Copy neighbor information if available
    for (int v = 1; v <= num_vars; ++v)
    {
        var_neighbor_count[v] = other.var_neighbor_count[v];
        if (var_neighbor_count[v] > 0 && other.var_neighbor[v] != nullptr)
        {
            var_neighbor[v] = new int[var_neighbor_count[v]];
            for (int i = 0; i < var_neighbor_count[v]; ++i)
            {
                var_neighbor[v][i] = other.var_neighbor[v][i];
            }
        }
        else
        {
            var_neighbor[v] = nullptr;
        }
    }

    // temp_lit is only used as scratch space; no need to copy contents.
}

// Deep copy assignment
Instance &Instance::operator=(const Instance &other)
{
    if (this != &other)
    {
        // Release existing resources
        free_memory();

        // Copy scalar fields
        problem_weighted   = other.problem_weighted;
        num_vars           = other.num_vars;
        num_clauses        = other.num_clauses;
        num_hclauses       = other.num_hclauses;
        num_sclauses       = other.num_sclauses;
        top_clause_weight  = other.top_clause_weight;
        total_soft_weight  = other.total_soft_weight;
        total_hard_length  = other.total_hard_length;
        total_soft_length  = other.total_soft_length;
        fixed_soft_cost    = other.fixed_soft_cost;
        has_empty_hard_clause = other.has_empty_hard_clause;
        unit_clause_count  = other.unit_clause_count;
        unit_soft_clause_count = other.unit_soft_clause_count;

        // Allocate top-level arrays based on new sizes
        allocate_memory();

        // Copy clause-related structures
        for (int c = 0; c < num_clauses; ++c)
        {
            clause_lit_count[c] = other.clause_lit_count[c];

            if (clause_lit_count[c] > 0 && other.clause_lit[c] != nullptr)
            {
                clause_lit[c] = new lit[clause_lit_count[c] + 1];
                for (int i = 0; i <= clause_lit_count[c]; ++i)
                {
                    clause_lit[c][i] = other.clause_lit[c][i];
                }
            }
            else
            {
                clause_lit[c] = nullptr;
            }

            org_clause_weight[c] = other.org_clause_weight[c];
        }

        // Copy unit clauses
        for (int i = 0; i < unit_clause_count; ++i)
        {
            unit_clause[i] = other.unit_clause[i];
        }

        // Copy unit soft clauses
        for (int i = 0; i < unit_soft_clause_count; ++i)
        {
            unit_soft_clause[i] = other.unit_soft_clause[i];
        }
        // Copy soft clause indices
        for (int i = 0; i < num_sclauses; ++i)
        {
            soft_clause_num_index[i] = other.soft_clause_num_index[i];
        }

        // Copy variable literal arrays
        for (int v = 1; v <= num_vars; ++v)
        {
            var_lit_count[v] = other.var_lit_count[v];
            if (var_lit_count[v] > 0 && other.var_lit[v] != nullptr)
            {
                var_lit[v] = new lit[var_lit_count[v] + 1];
                for (int i = 0; i <= var_lit_count[v]; ++i)
                {
                    var_lit[v][i] = other.var_lit[v][i];
                }
            }
            else
            {
                var_lit[v] = nullptr;
            }
        }

        // Copy neighbor information if available
        for (int v = 1; v <= num_vars; ++v)
        {
            var_neighbor_count[v] = other.var_neighbor_count[v];
            if (var_neighbor_count[v] > 0 && other.var_neighbor[v] != nullptr)
            {
                var_neighbor[v] = new int[var_neighbor_count[v]];
                for (int i = 0; i < var_neighbor_count[v]; ++i)
                {
                    var_neighbor[v][i] = other.var_neighbor[v][i];
                }
            }
            else
            {
                var_neighbor[v] = nullptr;
            }
        }
        // temp_lit remains scratch space
    }
    return *this;
}

Instance::Instance(Instance&& other) noexcept {
    // Steal the data from other
    var_lit = other.var_lit;
    var_lit_count = other.var_lit_count;
    clause_lit = other.clause_lit;
    clause_lit_count = other.clause_lit_count;
    unit_clause = other.unit_clause;
    unit_soft_clause = other.unit_soft_clause;
    org_clause_weight = other.org_clause_weight;
    var_neighbor = other.var_neighbor;
    var_neighbor_count = other.var_neighbor_count;
    soft_clause_num_index = other.soft_clause_num_index;
    temp_lit = other.temp_lit;

    num_vars = other.num_vars;
    num_clauses = other.num_clauses;
    num_hclauses = other.num_hclauses;
    num_sclauses = other.num_sclauses;
    problem_weighted = other.problem_weighted;
    top_clause_weight = other.top_clause_weight;
    total_soft_weight = other.total_soft_weight;
    total_hard_length = other.total_hard_length;
    total_soft_length = other.total_soft_length;
    fixed_soft_cost = other.fixed_soft_cost;
    has_empty_hard_clause = other.has_empty_hard_clause;
    unit_clause_count = other.unit_clause_count;
    unit_soft_clause_count = other.unit_soft_clause_count;
    // Reset other to a valid, destructible state
    other.var_lit = nullptr;
    other.var_lit_count = nullptr;
    other.clause_lit = nullptr;
    other.clause_lit_count = nullptr;
    other.unit_clause = nullptr;
    other.unit_soft_clause = nullptr;
    other.org_clause_weight = nullptr;
    other.var_neighbor = nullptr;
    other.var_neighbor_count = nullptr;
    other.soft_clause_num_index = nullptr;
    other.temp_lit = nullptr;

    other.num_vars = 0;
    other.num_clauses = 0;
    other.num_hclauses = 0;
    other.num_sclauses = 0;
    other.problem_weighted = 0;
    other.top_clause_weight = 0;
    other.total_soft_weight = 0;
    other.total_hard_length = 0;
    other.total_soft_length = 0;
    other.fixed_soft_cost = 0;
    other.has_empty_hard_clause = false;
    other.unit_clause_count = 0;
    other.unit_soft_clause_count = 0;
}

Instance& Instance::operator=(Instance&& other) noexcept {
    if (this != &other) {
        // Free existing resources
        free_memory();

        // Steal the data from other
        var_lit = other.var_lit;
        var_lit_count = other.var_lit_count;
        clause_lit = other.clause_lit;
        clause_lit_count = other.clause_lit_count;
        unit_clause = other.unit_clause;
        unit_soft_clause = other.unit_soft_clause;
        org_clause_weight = other.org_clause_weight;
        var_neighbor = other.var_neighbor;
        var_neighbor_count = other.var_neighbor_count;
        soft_clause_num_index = other.soft_clause_num_index;
        temp_lit = other.temp_lit;

        num_vars = other.num_vars;
        num_clauses = other.num_clauses;
        num_hclauses = other.num_hclauses;
        num_sclauses = other.num_sclauses;
        problem_weighted = other.problem_weighted;
        top_clause_weight = other.top_clause_weight;
        total_soft_weight = other.total_soft_weight;
        total_hard_length = other.total_hard_length;
        total_soft_length = other.total_soft_length;
        fixed_soft_cost = other.fixed_soft_cost;
        has_empty_hard_clause = other.has_empty_hard_clause;
        unit_clause_count = other.unit_clause_count;
        unit_soft_clause_count = other.unit_soft_clause_count;
        // Reset other to a valid, destructible state
        other.var_lit = nullptr;
        other.var_lit_count = nullptr;
        other.clause_lit = nullptr;
        other.clause_lit_count = nullptr;
        other.unit_clause = nullptr;
        other.unit_soft_clause = nullptr;
        other.org_clause_weight = nullptr;
        other.var_neighbor = nullptr;
        other.var_neighbor_count = nullptr;
        other.soft_clause_num_index = nullptr;
        other.temp_lit = nullptr;
        
        other.num_vars = 0;
        other.num_clauses = 0;
        other.num_hclauses = 0;
        other.num_sclauses = 0;
        other.problem_weighted = 0;
        other.top_clause_weight = 0;
        other.total_soft_weight = 0;
        other.total_hard_length = 0;
        other.total_soft_length = 0;
        other.fixed_soft_cost = 0;
        other.has_empty_hard_clause = false;
        other.unit_clause_count = 0;
        other.unit_soft_clause_count = 0;
    }
    return *this;
}

ReducedInstance Instance::reduce(const vector<int> &assignment) const
{
    ReducedInstance result;

    // Record which variables were fixed in the given assignment
    // -1 means unfixed, 0/1 means fixed value.
    result.fixed_assignment.assign(num_vars + 1, -1);
    for (int v = 1; v <= num_vars; ++v)
    {
        int val = (v < (int)assignment.size()) ? assignment[v] : -1;
        if (val != -1)
            result.fixed_assignment[v] = val;
    }

    // Shortcut: empty instance or no clauses
    if (num_clauses == 0 || num_vars == 0)
    {
        // result.reduced stays as default-constructed empty Instance
        result.old2new.assign(num_vars + 1, 0);
        result.new2old.assign(1, 0); // index 0 unused
        result.base_cost = fixed_soft_cost;
        result.unsat = has_empty_hard_clause;
        return result;
    }

    // Temporary structures to store simplified clauses
    struct TempClause
    {
        long long weight;
        bool isHard;
        vector<pair<int, int>> lits; // (var, sense)
    };

    vector<TempClause> tempClauses;
    tempClauses.reserve(num_clauses);

    vector<int> varUsed(num_vars + 1, 0);

    auto get_value = [&](int v) -> int {
        if (v <= 0 || v > num_vars)
            return -1;
        if ((size_t)v >= assignment.size())
            return -1;
        return assignment[v]; // expected -1, 0, or 1
    };

    result.base_cost = fixed_soft_cost;
    result.unsat = has_empty_hard_clause;
    if (result.unsat)
    {
        result.old2new.assign(num_vars + 1, 0);
        result.new2old.assign(1, 0);
        return result;
    }

    // First pass: simplify clauses under the partial assignment
    for (int c = 0; c < num_clauses; ++c)
    {
        long long weight = org_clause_weight[c];
        bool isHard = (weight == top_clause_weight);

        bool clauseSatisfied = false;
        vector<pair<int, int>> newLits;
        newLits.reserve(clause_lit_count[c]);

        for (int i = 0; i < clause_lit_count[c]; ++i)
        {
            int v = clause_lit[c][i].var_num;
            int sense = clause_lit[c][i].sense; // 1 for positive, 0 for negative

            int val = get_value(v); // -1, 0, or 1

            if (val == -1)
            {
                // Unassigned literal stays in the reduced clause
                newLits.emplace_back(v, sense);
                varUsed[v] = 1;
            }
            else
            {
                bool litTrue = (val == sense);
                if (litTrue)
                {
                    // Clause is satisfied, can be removed entirely
                    clauseSatisfied = true;
                    break;
                }
                // otherwise this literal is false and dropped
            }
        }

        if (clauseSatisfied)
        {
            // Satisfied clause contributes no further cost in reduced problem
            continue;
        }

        if (newLits.empty())
        {
            // Clause becomes empty: all literals are false under current assignment
            if (isHard)
            {
                // Hard clause conflict => whole instance UNSAT under this partial assignment
                result.unsat = true;
                // No need to build a reduced instance; return early.
                result.old2new.assign(num_vars + 1, 0);
                result.new2old.assign(1, 0);
                return result;
            }
            else
            {
                // Soft clause is forced unsatisfied; add its weight to base_cost
                result.base_cost += weight;
            }
            continue;
        }

        // Non-empty simplified clause goes into the reduced instance
        TempClause tc;
        tc.weight = weight;
        tc.isHard = isHard;
        tc.lits.swap(newLits);
        tempClauses.push_back(std::move(tc));
    }

    // Build variable mapping old -> new (only variables that still appear and are unassigned)
    result.old2new.assign(num_vars + 1, 0);
    result.new2old.clear();
    result.new2old.push_back(0); // index 0 unused to keep 1-based vars

    int newVarCount = 0;
    for (int v = 1; v <= num_vars; ++v)
    {
        int val = (v < (int)assignment.size()) ? assignment[v] : -1;
        if (val == -1 && varUsed[v])
        {
            ++newVarCount;
            result.old2new[v] = newVarCount;
            result.new2old.push_back(v);
        }
        if (val == -1 && !varUsed[v])
        {
            result.fixed_assignment[v] = 0;
        }
    }

    // If no clauses remain, the reduced instance is trivially satisfied
    if (tempClauses.empty())
    {
        // Keep reduced as an empty instance; mappings/base_cost already set
        return result;
    }

    // Second pass: actually construct the reduced Instance
    Instance reducedInst;

    reducedInst.problem_weighted = problem_weighted;
    reducedInst.num_vars = newVarCount;
    reducedInst.num_clauses = (int)tempClauses.size();
    reducedInst.num_hclauses = 0;
    reducedInst.num_sclauses = 0;
    reducedInst.top_clause_weight = top_clause_weight;
    reducedInst.total_soft_weight = 0;
    reducedInst.total_hard_length = 0;
    reducedInst.total_soft_length = 0;
    reducedInst.fixed_soft_cost = 0;
    reducedInst.has_empty_hard_clause = false;
    reducedInst.unit_clause_count = 0;
    reducedInst.unit_soft_clause_count = 0;
    // Allocate internal arrays based on new sizes
    reducedInst.allocate_memory();

    // Initialize pointer arrays similarly to build_instance
    for (int c = 0; c < reducedInst.num_clauses; ++c)
    {
        reducedInst.clause_lit_count[c] = 0;
        reducedInst.clause_lit[c] = nullptr;
    }
    for (int v = 1; v <= reducedInst.num_vars; ++v)
    {
        reducedInst.var_lit_count[v] = 0;
        reducedInst.var_lit[v] = nullptr;
        reducedInst.var_neighbor[v] = nullptr;
        reducedInst.var_neighbor_count[v] = 0;
    }

    int softIndex = 0;

    // First, allocate clause_lit arrays and accumulate var_lit counts
    for (int c = 0; c < reducedInst.num_clauses; ++c)
    {
        const TempClause &tc = tempClauses[c];
        int len = (int)tc.lits.size();

        reducedInst.clause_lit_count[c] = len;
        reducedInst.clause_lit[c] = new lit[len + 1];
        reducedInst.org_clause_weight[c] = tc.weight;

        for (int i = 0; i < len; ++i)
        {
            int oldV = tc.lits[i].first;
            int sense = tc.lits[i].second;
            int newV = result.old2new[oldV];

            reducedInst.clause_lit[c][i].clause_num = c;
            reducedInst.clause_lit[c][i].var_num = newV;
            reducedInst.clause_lit[c][i].sense = sense;

            reducedInst.var_lit_count[newV]++;
        }
        // sentinel
        reducedInst.clause_lit[c][len].var_num = 0;
        reducedInst.clause_lit[c][len].clause_num = -1;

        if (len == 1)
        {
            reducedInst.unit_clause[reducedInst.unit_clause_count++] = reducedInst.clause_lit[c][0];
            if (!tc.isHard)
            {
                reducedInst.unit_soft_clause[reducedInst.unit_soft_clause_count++] = reducedInst.clause_lit[c][0];
            }
        }

        if (tc.isHard)
        {
            reducedInst.num_hclauses++;
            reducedInst.total_hard_length += len;
        }
        else
        {
            reducedInst.num_sclauses++;
            reducedInst.total_soft_length += len;
            reducedInst.total_soft_weight += tc.weight;
            reducedInst.soft_clause_num_index[softIndex++] = c;
        }
    }

    // Build var_lit arrays from clause_lit
    for (int v = 1; v <= reducedInst.num_vars; ++v)
    {
        reducedInst.var_lit[v] = new lit[reducedInst.var_lit_count[v] + 1];
        reducedInst.var_lit_count[v] = 0; // reset as write pointer
    }

    for (int c = 0; c < reducedInst.num_clauses; ++c)
    {
        for (int i = 0; i < reducedInst.clause_lit_count[c]; ++i)
        {
            int v = reducedInst.clause_lit[c][i].var_num;
            int idx = reducedInst.var_lit_count[v]++;
            reducedInst.var_lit[v][idx] = reducedInst.clause_lit[c][i];
        }
    }

    for (int v = 1; v <= reducedInst.num_vars; ++v)
    {
        reducedInst.var_lit[v][reducedInst.var_lit_count[v]].clause_num = -1;
    }

    result.reduced = std::move(reducedInst);
    return result;
}

void Instance::infer_header_without_p_line(const char *filename,
                                           int &out_num_vars,
                                           int &out_num_clauses,
                                           long long &out_top_weight)
{
    ifstream file(filename);
    if (!file)
        throw runtime_error(string("cannot reopen WCNF file: ") + filename);

    int max_var = 0;
    long long clause_count = 0;
    long long max_soft_weight = 0;

    string line;
    while (getline(file, line))
    {
        istringstream input(line);
        string tag;
        // Standardized instances spell their metadata block as "c{", "c}" and
        // "c-----" rather than a bare "c", so match the comment prefix instead
        // of the exact token. No clause can start with 'c' or 'p': a clause
        // line begins with a digit, a '-', or the 'h' hard-clause marker.
        if (!(input >> tag) || tag[0] == 'c' || tag[0] == 'p')
            continue;

        ++clause_count;

        if (tag != "h")
        {
            long long weight = 0;
            size_t consumed = 0;
            try
            {
                weight = stoll(tag, &consumed);
            }
            catch (const exception &)
            {
                throw runtime_error("invalid WCNF clause weight");
            }
            if (consumed != tag.size() || weight <= 0)
                throw runtime_error("invalid soft-clause weight");
            if (weight > max_soft_weight)
                max_soft_weight = weight;
        }

        long long literal_value = 0;
        while (input >> literal_value)
        {
            if (literal_value == 0)
                break;
            const int variable = abs(static_cast<int>(literal_value));
            if (variable > max_var)
                max_var = variable;
        }
    }

    out_num_vars = max_var;
    out_num_clauses = static_cast<int>(clause_count);
    // Strictly above every soft weight, so the weight-based classification in
    // the clause pass marks exactly the 'h' clauses as hard.
    out_top_weight = max_soft_weight + 1;
}

void Instance::build_instance(const char *filename)
{
    ifstream header_file(filename);
    if (!header_file)
        throw runtime_error(string("cannot open WCNF file: ") + filename);

    string line;
    bool header_found = false;
    int declared_clauses = 0;

    while (getline(header_file, line))
    {
        istringstream input(line);
        string tag;
        if (!(input >> tag) || tag[0] == 'c')
            continue;

        if (tag != "p")
            continue;

        string format;
        if (header_found || !(input >> format >> num_vars >> declared_clauses >> top_clause_weight) ||
            format != "wcnf" || num_vars < 0 || declared_clauses < 0 || top_clause_weight <= 0)
            throw runtime_error("invalid WCNF header");

        header_found = true;
    }

    if (!header_found)
    {
        // Standardized instances have no "p wcnf" line; recover the same
        // information from the clause block itself.
        long long inferred_top_weight = 1;
        infer_header_without_p_line(filename, num_vars, declared_clauses,
                                    inferred_top_weight);
        top_clause_weight = inferred_top_weight;
    }

    num_clauses = declared_clauses;
    allocate_memory();

    for (int c = 0; c < num_clauses; ++c)
    {
        clause_lit_count[c] = 0;
        clause_lit[c] = nullptr;
    }
    for (int v = 1; v <= num_vars; ++v)
    {
        var_lit_count[v] = 0;
        var_lit[v] = nullptr;
        var_neighbor[v] = nullptr;
        var_neighbor_count[v] = 0;
    }

    problem_weighted = 0;
    num_hclauses = 0;
    num_sclauses = 0;
    unit_clause_count = 0;
    unit_soft_clause_count = 0;
    total_soft_weight = 0;
    total_soft_length = 0;
    total_hard_length = 0;
    fixed_soft_cost = 0;
    has_empty_hard_clause = false;

    ifstream clause_file(filename);
    if (!clause_file)
        throw runtime_error(string("cannot reopen WCNF file: ") + filename);

    int declared_clause_count = 0;
    int stored_clause_count = 0;
    vector<int> seen_literal(num_vars + 1, 0);

    while (getline(clause_file, line))
    {
        istringstream input(line);
        string tag;
        // Standardized instances spell their metadata block as "c{", "c}" and
        // "c-----" rather than a bare "c", so match the comment prefix instead
        // of the exact token. No clause can start with 'c' or 'p': a clause
        // line begins with a digit, a '-', or the 'h' hard-clause marker.
        if (!(input >> tag) || tag[0] == 'c' || tag[0] == 'p')
            continue;

        ++declared_clause_count;
        if (declared_clause_count > declared_clauses)
            throw runtime_error("WCNF contains more clauses than declared");

        bool hard_clause = tag == "h";
        long long weight = top_clause_weight;
        if (!hard_clause)
        {
            try
            {
                size_t consumed = 0;
                weight = stoll(tag, &consumed);
                if (consumed != tag.size() || weight <= 0)
                    throw runtime_error("invalid soft-clause weight");
            }
            catch (const exception &)
            {
                throw runtime_error("invalid WCNF clause weight");
            }
            hard_clause = weight == top_clause_weight;
        }

        vector<int> literals;
        bool tautology = false;
        bool terminated = false;
        long long literal_value = 0;
        while (input >> literal_value)
        {
            if (literal_value == 0)
            {
                terminated = true;
                break;
            }
            if (literal_value < -num_vars || literal_value > num_vars)
                throw runtime_error("literal is outside the declared variable range");

            const int literal = static_cast<int>(literal_value);
            const int variable = abs(literal);
            if (seen_literal[variable] == 0)
            {
                seen_literal[variable] = literal;
                literals.push_back(literal);
            }
            else if (seen_literal[variable] != literal)
            {
                tautology = true;
            }
        }

        if (!terminated)
            throw runtime_error("WCNF clause is missing its terminating zero");

        for (size_t i = 0; i < literals.size(); ++i)
            seen_literal[abs(literals[i])] = 0;

        if (tautology)
            continue;

        if (literals.empty())
        {
            if (hard_clause)
                has_empty_hard_clause = true;
            else
            {
                fixed_soft_cost += weight;
                total_soft_weight += weight;
                if (weight != 1)
                    problem_weighted = 1;
            }
            continue;
        }

        const int c = stored_clause_count++;
        clause_lit_count[c] = static_cast<int>(literals.size());
        clause_lit[c] = new lit[clause_lit_count[c] + 1];
        org_clause_weight[c] = weight;

        for (int i = 0; i < clause_lit_count[c]; ++i)
        {
            const int literal = literals[i];
            const int v = abs(literal);
            clause_lit[c][i].clause_num = c;
            clause_lit[c][i].var_num = v;
            clause_lit[c][i].sense = literal > 0 ? 1 : 0;
            ++var_lit_count[v];
        }
        clause_lit[c][clause_lit_count[c]].var_num = 0;
        clause_lit[c][clause_lit_count[c]].clause_num = -1;

        if (hard_clause)
        {
            ++num_hclauses;
            total_hard_length += clause_lit_count[c];
        }
        else
        {
            ++num_sclauses;
            soft_clause_num_index[num_sclauses - 1] = c;
            total_soft_weight += weight;
            total_soft_length += clause_lit_count[c];
            if (weight != 1)
                problem_weighted = 1;
        }

        if (clause_lit_count[c] == 1)
        {
            unit_clause[unit_clause_count++] = clause_lit[c][0];
            if (!hard_clause)
                unit_soft_clause[unit_soft_clause_count++] = clause_lit[c][0];
        }
    }

    if (declared_clause_count != declared_clauses)
        throw runtime_error("WCNF clause count does not match the header");

    num_clauses = stored_clause_count;
    for (int v = 1; v <= num_vars; ++v)
    {
        var_lit[v] = new lit[var_lit_count[v] + 1];
        var_lit_count[v] = 0;
    }
    for (int c = 0; c < num_clauses; ++c)
    {
        for (int i = 0; i < clause_lit_count[c]; ++i)
        {
            const int v = clause_lit[c][i].var_num;
            var_lit[v][var_lit_count[v]++] = clause_lit[c][i];
        }
    }
    for (int v = 1; v <= num_vars; ++v)
        var_lit[v][var_lit_count[v]].clause_num = -1;
}

void Instance::allocate_memory()
{
    int malloc_var_length = num_vars + 10;
    int malloc_clause_length = num_clauses + 10;

    unit_clause = new lit[malloc_clause_length];
    unit_soft_clause = new lit[malloc_clause_length];
    var_lit = new lit *[malloc_var_length];
    var_lit_count = new int[malloc_var_length];
    clause_lit = new lit *[malloc_clause_length];
    clause_lit_count = new int[malloc_clause_length];

    var_neighbor = new int *[malloc_var_length];
    var_neighbor_count = new int[malloc_var_length];

    org_clause_weight = new long long[malloc_clause_length];
    
    temp_lit = new int[malloc_var_length];

    soft_clause_num_index = new int[malloc_clause_length];
}

void Instance::free_memory()
{
    if (clause_lit != nullptr) {
        for (int i = 0; i < num_clauses; i++)
            delete[] clause_lit[i];
    }

    if (var_lit != nullptr) {
        for (int i = 1; i <= num_vars; ++i)
        {
            delete[] var_lit[i];
        }
    }
    
    if (var_neighbor != nullptr) {
        for (int i = 1; i <= num_vars; ++i)
        {
            delete[] var_neighbor[i];
        }
    }

    delete[] var_lit;
    delete[] var_lit_count;
    delete[] clause_lit;
    delete[] clause_lit_count;

    delete[] var_neighbor;
    delete[] var_neighbor_count;

    delete[] org_clause_weight;
    
    delete[] temp_lit;

    delete[] soft_clause_num_index;
    
    delete[] unit_clause;
    delete[] unit_soft_clause;
}

void Instance::print_info()
{
    cout << "c Instance Information:" << endl;
    cout << "c Number of variables: " << num_vars << endl;
    cout << "c Number of clauses: " << num_clauses << endl;
    cout << "c Number of hard clauses: " << num_hclauses << endl;
    cout << "c Number of soft clauses: " << num_sclauses << endl;
    cout << "c Problem weighted: " << problem_weighted << endl;
    cout << "c Top clause weight: " << top_clause_weight << endl;
    cout << "c Total soft weight: " << total_soft_weight << endl;
    cout << "c Total hard length: " << total_hard_length << endl;
    cout << "c Total soft length: " << total_soft_length << endl;
    cout << "c Unit clause count: " << unit_clause_count << endl;
    double unit_soft_ratio = (num_sclauses == 0) ? 0.0 : ((double)unit_soft_clause_count / (double)num_sclauses);
    cout << "c Unit soft clause count: " << unit_soft_clause_count << " (" << unit_soft_ratio * 100.0 << "%)" << endl;
    // print first few clauses (constraints) 
}

long long Instance::verify_solution(const vector<int> &assignment) const
{
    if (has_empty_hard_clause)
        return -1;

    long long total_cost = fixed_soft_cost;

    if(assignment.size() < (size_t)num_vars + 1) {
        cout << "c Warning: assignment size " << assignment.size() << " is not sufficient for " << num_vars << " variables." << endl;
        return -1; // Indicate error
    }

    for(int i = 1; i <= num_vars; ++i) {
        if(assignment[i] != 0 && assignment[i] != 1) {
            cout << "c Warning: variable " << i << " has invalid assignment value " << assignment[i] << "." << endl;
            return -1; // Indicate error
        }
    }

    for (int c = 0; c < num_clauses; ++c)
    {
        bool clause_satisfied = false;
        for (int i = 0; i < clause_lit_count[c]; ++i)
        {
            int v = clause_lit[c][i].var_num;
            int sense = clause_lit[c][i].sense; // 1 for true, 0 for false

            if (v >= (int)assignment.size()){
                cout << "c Warning: clause " << c << " references variable " << v << " which is out of bounds in the assignment." << endl;
                return -1; // Indicate error
            }

            int val = assignment[v]; // expected 0 or 1

            if (val == sense)
            {
                clause_satisfied = true;
                break;
            }
        }
        if (!clause_satisfied)
        {
            if(org_clause_weight[c] == top_clause_weight) {
                return -1; // Hard clause unsatisfied
            } else {
                total_cost += org_clause_weight[c];
            }
        }
    }

    return total_cost;
}
