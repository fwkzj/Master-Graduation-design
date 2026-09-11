cmake_minimum_required(VERSION 3.10)
project(MasterGraduationDesign LANGUAGES CXX)

enable_testing()

set(minisat_SOURCE_DIR
    ${CMAKE_CURRENT_SOURCE_DIR}/CASHWMaxSAT-DisjCad-S6/code/cominisatps)
set(minisatp_SOURCE_DIR
    ${CMAKE_CURRENT_SOURCE_DIR}/CASHWMaxSAT-DisjCad-S6/code/uwrmaxsat)

# CASHWMaxSAT-DisjCad uses Cadical as its SAT backend and SCIP for its exact
# optimization.  Their static libraries are built natively (see
# CASHWMaxSAT-DisjCad-S6/code/starexec_build) before CMake configures; these
# variables point the CMake targets at the produced headers and archives.
set(cadical_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/CASHWMaxSAT-DisjCad-S6/code/cadical/src)
set(cadical_BUILD_DIR  ${CMAKE_CURRENT_SOURCE_DIR}/CASHWMaxSAT-DisjCad-S6/code/cadical/build)
set(scip_SOURCE_DIR    ${CMAKE_CURRENT_SOURCE_DIR}/CASHWMaxSAT-DisjCad-S6/code/scipoptsuite-8.1.0/scip/src)
set(scip_BUILD_DIR     ${CMAKE_CURRENT_SOURCE_DIR}/CASHWMaxSAT-DisjCad-S6/code/scipoptsuite-8.1.0/build)

find_library(cadical_LIBRARY NAMES cadical    PATHS ${cadical_BUILD_DIR}       NO_DEFAULT_PATH)
find_library(scip_LIBRARY    NAMES scip       PATHS ${scip_BUILD_DIR}/lib      NO_DEFAULT_PATH)
find_library(soplex_LIBRARY  NAMES soplex-pic PATHS ${scip_BUILD_DIR}/lib      NO_DEFAULT_PATH)
find_library(z_LIBRARY       NAMES z)

add_subdirectory(CASHWMaxSAT-DisjCad-S6/code/cominisatps)
add_subdirectory(CASHWMaxSAT-DisjCad-S6/code/uwrmaxsat)
add_subdirectory(SPBMAXSAT2-master/SPBMAXSAT2-master)
add_subdirectory(HybridMaxSAT)
