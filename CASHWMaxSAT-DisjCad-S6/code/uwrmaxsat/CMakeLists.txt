cmake_minimum_required(VERSION 3.5 FATAL_ERROR)

project(uwrmaxsat)

#--------------------------------------------------------------------------------------------------
# Configurable options:

option(STATIC_BINARIES "Link binaries statically." ON)
option(USE_SORELEASE   "Use SORELEASE in shared library filename." ON)

#--------------------------------------------------------------------------------------------------
# Library version:

set(MINISATP_SOMAJOR   1)
set(MINISATP_SOMINOR   1)
set(MINISATP_SORELEASE 0)

# Compute VERSION and SOVERSION:
if(USE_SORELEASE)
  set(MINISATP_VERSION ${MINISATP_SOMAJOR}.${MINISATP_SOMINOR}.${MINISATP_SORELEASE})
else()
  set(MINISATP_VERSION ${MINISATP_SOMAJOR}.${MINISATP_SOMINOR})
endif()
set(MINISATP_SOVERSION ${MINISATP_SOMAJOR})

#--------------------------------------------------------------------------------------------------
# Compile flags:

add_definitions(-include Global.h -include Main.h -D_FILE_OFFSET_BITS=64)
add_definitions(-DCADICAL -DUSE_SCIP -DNDEBUG -D__STDC_LIMIT_MACROS)
add_compile_options(-pthread)

# We require C++11 or higher.

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

#--------------------------------------------------------------------------------------------------
# Dependencies:

find_library(GMP_LIBRARY gmp)
#include_directories(${GMP_INCLUDE_DIR})

include_directories(${minisat_SOURCE_DIR} ${minisat_SOURCE_DIR}/minisat)
include_directories(${minisatp_SOURCE_DIR})
include_directories(${minisatp_SOURCE_DIR}/ADTs)
include_directories(${cadical_SOURCE_DIR})
include_directories(${scip_SOURCE_DIR})
include_directories(${scip_BUILD_DIR}/scip)

#--------------------------------------------------------------------------------------------------
# Build Targets:

set(MINISATP_LIB_SOURCES
    ADTs/FEnv.cc
    ADTs/File.cc
    ADTs/Global.cc
    Debug.cc
    Hardware_adders.cc
    Hardware_clausify.cc
    Hardware_reuse_sorters.cc
    Hardware_sorters.cc
    Main_utils.cc
    PbParser.cc
    PbSolver.cc
    PbSolver_convert.cc
    PbSolver_convertAdd.cc
    PbSolver_convertBdd.cc
    PbSolver_convertSort.cc
    Pre_separator.cc
    MsSolver.cc
    SatSolver.cc
    ScipSolver.cc
    System.cc
    ipamir.cc)

add_library(minisatp-lib-static STATIC ${MINISATP_LIB_SOURCES})

target_link_libraries(minisatp-lib-static minisat-lib-static ${GMP_LIBRARY} ${cadical_LIBRARY} ${scip_LIBRARY} ${soplex_LIBRARY} ${z_LIBRARY} pthread)

set_target_properties(minisatp-lib-static PROPERTIES OUTPUT_NAME "uwrmaxsat")

add_executable(uwrmaxsat Main.cc)
target_link_libraries(uwrmaxsat minisatp-lib-static)


#--------------------------------------------------------------------------------------------------
# Installation targets:
