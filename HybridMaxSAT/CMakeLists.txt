cmake_minimum_required(VERSION 3.10)
project(HybridMaxSAT LANGUAGES CXX)

# The hybrid layer includes MsSolver.h / SatSolver.h, so its translation units
# must be compiled with the same backend macros as the CASH library to keep the
# class layouts (and therefore the ABI) identical.
add_definitions(-include Global.h -include Main.h -D_FILE_OFFSET_BITS=64)
add_definitions(-DCADICAL -DUSE_SCIP -DNDEBUG -D__STDC_LIMIT_MACROS)
add_compile_options(-pthread)

# This directory is included by the server-side superproject after the two
# solver libraries have been configured. It deliberately does not build either
# solver locally; see agent.md for the S122-only execution rule.
add_library(hybridmaxsat_protocol STATIC
    src/hybrid_coordinator.cpp)

target_compile_features(hybridmaxsat_protocol PUBLIC cxx_std_11)
target_include_directories(hybridmaxsat_protocol
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../CASHWMaxSAT-DisjCad-S6/code/uwrmaxsat
        ${CMAKE_CURRENT_SOURCE_DIR}/../CASHWMaxSAT-DisjCad-S6/code/uwrmaxsat/ADTs
        ${CMAKE_CURRENT_SOURCE_DIR}/../CASHWMaxSAT-DisjCad-S6/code/cominisatps
        ${CMAKE_CURRENT_SOURCE_DIR}/../CASHWMaxSAT-DisjCad-S6/code/cominisatps/minisat
        ${CMAKE_CURRENT_SOURCE_DIR}/../SPBMAXSAT2-master/SPBMAXSAT2-master/src
        ${cadical_SOURCE_DIR}
        ${scip_SOURCE_DIR}
        ${scip_BUILD_DIR}/scip)

# The server superproject supplies the SPB and CASH library targets. Keeping
# this target separate prevents the original solver command-line programs from
# being linked into the hybrid protocol library.
target_link_libraries(hybridmaxsat_protocol PUBLIC spbmaxsat)

add_executable(hybridmaxsat
    src/hybrid_main.cpp)
target_link_libraries(hybridmaxsat PRIVATE
    hybridmaxsat_protocol
    minisatp-lib-static
    ${cadical_LIBRARY}
    ${scip_LIBRARY}
    ${soplex_LIBRARY}
    ${z_LIBRARY}
    gmp
    pthread)
