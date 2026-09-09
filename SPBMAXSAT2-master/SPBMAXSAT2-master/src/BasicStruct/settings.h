#ifndef _SETTINGS_H_
#define _SETTINGS_H_

#include <iostream>

// A structure to hold all solver settings.
struct Settings
{
    // Default values are set here.
    float rdprob = 0.5;
    int hd_count_threshold = 5;
    float rwprob = 0.1;
    float smooth_probability = 0.01;
    float soft_smooth_probability = 0.01;
    double softclause_weight_threshold = 1.0;
    double h_inc = 1.0;
    double s_inc = 1.0;
    int coe_soft_clause_weight = 2;
    int cutoff_time = 300;
    int solution_pool_size = 15;

    Settings() = default;
};

inline void print_settings(const Settings &settings){
    std::cout << "c Solver Settings:" << std::endl;
    std::cout << "c rdprob: " << settings.rdprob << std::endl;
    std::cout << "c hd_count_threshold: " << settings.hd_count_threshold << std::endl;
    std::cout << "c rwprob: " << settings.rwprob << std::endl;
    std::cout << "c smooth_probability: " << settings.smooth_probability << std::endl;
    std::cout << "c soft_smooth_probability: " << settings.soft_smooth_probability << std::endl;
    std::cout << "c softclause_weight_threshold: " << settings.softclause_weight_threshold << std::endl;
    std::cout << "c h_inc: " << settings.h_inc << std::endl;
    std::cout << "c s_inc: " << settings.s_inc << std::endl;
    std::cout << "c coe_soft_clause_weight: " << settings.coe_soft_clause_weight << std::endl;
    std::cout << "c cutoff_time: " << settings.cutoff_time << " seconds" << std::endl;
    std::cout << "c solution_pool_size: " << settings.solution_pool_size << std::endl;
}

#endif // _SETTINGS_H_
