# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
#
# Zengine's official lane (VM-POP-01, VM-LANE-01): did the test population this repository means
# to run exist and pass? It refuses a missing build tree, a zero-entry selection (asked of `ctest
# -N` and refused by --no-tests=error, two independent guards), a population the manifest does not
# declare, and a failing test; it verifies the tree it is handed, not its currency (VM-POP-09).
#   cmake -DZEN_BUILD_DIR=build [-DZEN_CTEST_ARGS=-V] -P tests/verify.cmake

cmake_minimum_required(VERSION 3.18) # --no-tests=error

if(NOT DEFINED ZEN_BUILD_DIR)
    message(FATAL_ERROR "verify: -DZEN_BUILD_DIR=<build dir> is required")
endif()
if(NOT EXISTS "${ZEN_BUILD_DIR}/CTestTestfile.cmake")
    message(FATAL_ERROR
        "verify: '${ZEN_BUILD_DIR}' is not a configured CTest build directory "
        "(no CTestTestfile.cmake). Configure and build first. Zengine's suites need a Loom "
        "that exports loom::kernel; -DBUILD_TESTING=OFF configures no tests at all, and this "
        "lane has nothing to verify in that mode.")
endif()

# ---- guard 1: something must actually be registered --------------------------------------

execute_process(
    COMMAND ${CMAKE_CTEST_COMMAND} --test-dir "${ZEN_BUILD_DIR}" -N
    OUTPUT_VARIABLE listing RESULT_VARIABLE list_rc)
if(NOT list_rc EQUAL 0)
    message(FATAL_ERROR "verify: could not list the registered tests (exit ${list_rc}).\n${listing}")
endif()
if(NOT listing MATCHES "Total Tests: ([0-9]+)")
    message(FATAL_ERROR "verify: could not read a test count out of `ctest -N`.\n${listing}")
endif()
set(selected "${CMAKE_MATCH_1}")
if(selected EQUAL 0)
    message(FATAL_ERROR
        "verify: this build tree registers ZERO CTest entries. CTest would have printed "
        "\"No tests were found!!!\" and exited 0; this lane calls that what it is -- a "
        "question that was never asked, not an answer.")
endif()

# The entry names, for the inventory contract. `ctest -N` prints one `Test #<n>: <name>` line
# per registered entry.
string(REPLACE "\r" "" listing "${listing}")
string(REPLACE "\n" ";" listing_lines "${listing}")
set(registered "")
foreach(line IN LISTS listing_lines)
    if(line MATCHES "^ *Test +#[0-9]+: +(.+)$")
        string(STRIP "${CMAKE_MATCH_1}" name)
        list(APPEND registered "${name}")
    endif()
endforeach()
list(LENGTH registered registered_count)
if(NOT registered_count EQUAL selected)
    message(FATAL_ERROR
        "verify: `ctest -N` reported ${selected} tests but named ${registered_count} of them. "
        "This lane could not read the inventory it is supposed to check, and a population "
        "check that cannot see the population is not evidence.\n${listing}")
endif()
message(STATUS "verify: ${selected} CTest entries registered")

# ---- guard 2: the population must be the one this repository declared --------------------
# Checked here, outside the population it checks: as a CTest entry, deleting the entry would
# delete the question with it.

include("${CMAKE_CURRENT_LIST_DIR}/check_population.cmake")
zengine_check_population("${CMAKE_CURRENT_LIST_DIR}/test_population.txt"
                         "${ZEN_BUILD_DIR}" "${registered}")

# ---- guard 3: run them, and let CTest refuse a zero too ----------------------------------

execute_process(
    COMMAND ${CMAKE_CTEST_COMMAND} --test-dir "${ZEN_BUILD_DIR}"
            --no-tests=error --output-on-failure ${ZEN_CTEST_ARGS}
    RESULT_VARIABLE run_rc)
if(NOT run_rc EQUAL 0)
    message(FATAL_ERROR "verify: FAILED (ctest exit ${run_rc}) over ${selected} registered entries")
endif()

message(STATUS "verify: PASSED -- ${selected} declared CTest entries registered, populated and executed")
