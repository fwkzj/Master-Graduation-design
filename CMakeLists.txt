cmake_minimum_required(VERSION 3.10)
project(MasterGraduationDesign LANGUAGES CXX)

enable_testing()

set(minisat_SOURCE_DIR
    ${CMAKE_CURRENT_SOURCE_DIR}/CASHWMaxSAT-DisjCad-S6/code/cominisatps)
set(minisatp_SOURCE_DIR
    ${CMAKE_CURRENT_SOURCE_DIR}/CASHWMaxSAT-DisjCad-S6/code/uwrmaxsat)

add_subdirectory(CASHWMaxSAT-DisjCad-S6/code/cominisatps)
add_subdirectory(CASHWMaxSAT-DisjCad-S6/code/uwrmaxsat)
add_subdirectory(SPBMAXSAT2-master/SPBMAXSAT2-master)
add_subdirectory(HybridMaxSAT)
