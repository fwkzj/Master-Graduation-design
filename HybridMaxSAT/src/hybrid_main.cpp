#include "hybrid_coordinator.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

#include "Global.h"
#include "Main.h"
#include "Main_utils.h"
#include "PbParser.h"

namespace {

std::string int_to_string(const Int &value)
{
    char *text = toString(value);
    const std::string result(text);
    xfree(text);
    return result;
}

const char *bound_result_name(HybridBoundResult result)
{
    switch (result)
    {
    case HybridBoundResult::Accepted: return "accepted";
    case HybridBoundResult::NotImproved: return "not_improved";
    case HybridBoundResult::InvalidNegative: return "invalid_negative";
    case HybridBoundResult::InvalidUnitConversion: return "invalid_unit_conversion";
    case HybridBoundResult::InvalidBelowLowerBound: return "invalid_below_lb";
    }
    return "unknown";
}

void write_events(const std::string &path,
                  const std::string &instance,
                  const std::vector<hybridmaxsat::RoundEvent> &events)
{
    std::ofstream output(path.c_str());
    if (!output)
        throw std::runtime_error("cannot open JSONL output file");

    for (const hybridmaxsat::RoundEvent &event : events)
    {
        output << "{\"event\":\"spb_round\",\"instance\":\"" << instance
               << "\",\"round\":" << event.round
               << ",\"cash_cpu_before_spb\":" << event.cash_cpu_before_spb
               << ",\"propagated_original_variables\":"
               << event.propagated_original_variables
               << ",\"cash_ub_before_spb\":" << event.cash_ub_before_spb
               << ",\"spb_ub\":" << event.spb_ub
               << ",\"cash_ub_after_spb\":" << event.cash_ub_after_spb
               << ",\"cash_bound_result\":\""
               << bound_result_name(event.cash_bound_result) << "\"}\n";
    }
}

} // namespace

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        std::cerr << "usage: hybridmaxsat <input.wcnf> <events.jsonl>" << std::endl;
        return 2;
    }

    const std::string input_path(argv[1]);
    const std::string events_path(argv[2]);

    try
    {
        // Match the CASH MaxSAT path while retaining control of final output.
        opt_maxsat = true;
        opt_satlive = false;
        opt_model_out = false;
        opt_satisfiable_out = false;
        opt_maxsat_msu = true;
        opt_minimization = 1;
        opt_seq_thres = 4;
        opt_convert = ct_Sorters;
        opt_reuse_sorters = false;

        MsSolver cash_solver(false, opt_preprocess);
        parse_WCNF_file(const_cast<char *>(input_path.c_str()), cash_solver);

        hybridmaxsat::HybridCoordinator coordinator(input_path);
        cash_solver.set_hybrid_callback(&coordinator);
        cash_solver.maxsat_solve(PbSolver::sc_Minimize);

        write_events(events_path, input_path, coordinator.round_events());

        const bool cash_proved_optimal =
            !cash_solver.asynch_interrupt &&
            cash_solver.best_goalvalue != Int_MAX &&
            cash_solver.LB_goalvalue == cash_solver.best_goalvalue;

        std::cout << "{\"event\":\"run_complete\",\"instance\":\""
                  << input_path
                  << "\",\"cash_proved_optimal\":"
                  << (cash_proved_optimal ? "true" : "false")
                  << ",\"final_lb\":\"" << int_to_string(cash_solver.LB_goalvalue)
                  << "\",\"final_ub\":\"" << int_to_string(cash_solver.UB_goalvalue)
                  << "\",\"final_incumbent\":\""
                  << int_to_string(cash_solver.best_goalvalue)
                  << "\",\"spb_certificate_ub\":"
                  << (coordinator.has_spb_certificate() ?
                      std::to_string(coordinator.best_spb_certificate().cost) : "null")
                  << "}" << std::endl;
    }
    catch (const std::exception &error)
    {
        std::cerr << "hybridmaxsat error: " << error.what() << std::endl;
        return 1;
    }

    return 0;
}
