# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
#
# ZENGINE'S OFFICIAL VERIFICATION LANE (C4, closing COLD-2 C-4).
#
# Run this, not a bare `ctest`, when a Zengine result is going to be quoted as evidence:
#
#   cmake -DZEN_BUILD_DIR=build -P tests/verify.cmake
#   cmake -DZEN_BUILD_DIR=build -DZEN_CTEST_ARGS=-V -P tests/verify.cmake
#
# It is the answer to one question -- *did the test population Zengine intended to run exist
# and pass?* -- and it refuses four different ways of answering it dishonestly:
#
#   1. no configured build tree at all;
#   2. a CTest selection of ZERO entries. CTest's own default treats that as success:
#
#          $ ctest -R "^no_such_test$" ; echo $?
#          No tests were found!!!
#          0
#
#      so the lane asks `ctest -N` for the count itself AND passes --no-tests=error, two
#      guards that fail independently;
#   3. a population that does not match tests/test_population.txt -- an entry missing, an
#      entry present that nothing declared, a doctest surface below its case floor, a
#      compile-negative test that stopped judging its diagnostic;
#   4. a failing test.
#
# Green requires all four. COLD-2 measured what the third one costs when it is absent:
# deleting one whole TEST_CASE from test_input.cpp left `ctest --no-tests=error` reporting
# "100% tests passed ... out of 10", and asking a suite for a test case that does not exist
# printed "Status: SUCCESS!" and exited 0.
#
# THE POPULATION CHECK LIVES HERE, OUTSIDE THE POPULATION IT CHECKS -- deliberately. If it
# were a CTest entry, deleting that entry would delete the question along with the answer,
# and the remaining nine tests would pass. (C3 paid for the general form of this next door:
# a check gated on a derivation goes blind exactly where the derivation does.) So the
# expectation is a source file, the check is this script, and neither of them can be removed
# by editing a registration.
#
# WHAT THIS LANE DOES NOT PROVE. It verifies the configured build tree it is handed. It does
# not prove that tree is current with the sources -- producing a correctly configured, freshly
# built tree is the job of whoever configures and builds, and a failed reconfigure that left
# a stale tree standing would be verified faithfully as the stale thing it is. Same honest
# limit as the Loom's own lane; not fixed here, and not hidden either.

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
