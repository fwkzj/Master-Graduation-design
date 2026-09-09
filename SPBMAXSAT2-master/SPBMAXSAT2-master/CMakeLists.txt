cmake_minimum_required(VERSION 3.10)
project(SPB-MaxSAT)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Include directories (use src as include root)
include_directories(${CMAKE_SOURCE_DIR}/src)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(-O3)
endif()

# Reusable solver library (everything except the CLI entry point).
set(SPBMAXSAT_SOURCES
    src/BasicStruct/instance.cpp
    src/BasicStruct/solutionpool.cpp
    src/Solver/solver.cpp
    src/Solver/local_search_solver.cpp
    src/Worker/deci.cpp
    src/Worker/lsworker_base.cpp
    src/Worker/lsworker_search.cpp
    src/Worker/lsworker_heur.cpp
    src/Crossover/crossover.cpp
)

# External solvers can link this target after add_subdirectory(...).
add_library(spbmaxsat STATIC ${SPBMAXSAT_SOURCES})
target_include_directories(spbmaxsat PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Command-line solver.
add_executable(solver src/main.cpp)
target_link_libraries(solver PRIVATE spbmaxsat)
