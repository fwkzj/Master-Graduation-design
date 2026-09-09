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

include(CTest)
if(BUILD_TESTING)
    add_executable(instance_parser_test tests/instance_parser_test.cpp)
    target_link_libraries(instance_parser_test PRIVATE spbmaxsat)
    add_test(
        NAME instance_parser_standard_wcnf
        COMMAND instance_parser_test ${CMAKE_CURRENT_BINARY_DIR}
    )

    add_executable(persistent_interface_test tests/persistent_interface_test.cpp)
    target_link_libraries(persistent_interface_test PRIVATE spbmaxsat)
    add_test(
        NAME persistent_local_search_interface
        COMMAND persistent_interface_test ${CMAKE_CURRENT_BINARY_DIR}
    )
endif()
