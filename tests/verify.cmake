# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
#
# Zengine's official lane (VM-POP-01, VM-LANE-01): did the test population this repository means
# to run exist and pass? It refuses a missing build tree, a zero-entry selection (asked of `ctest
# -N` and refused by --no-tests=error, two independent guards), a population the manifest does not
# declare, and a failing test; it verifies the tree it is handed, not its currency (VM-POP-09).
#   cmake -DZEN_BUILD_DIR=build [-DZEN_CTEST_ARGS=-V] [-DZEN_CTEST_APART=<entry>;...] -P tests/verify.cmake

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

# ---- guard 2b: no test compile carries a path of the tree --------------------------------
# Such a value keeps a compiler cache from serving the unit to a second tree; the check says how
# to give a case the path another way (check_test_definitions.cmake).
execute_process(
    COMMAND ${CMAKE_COMMAND} "-DZEN_BUILD_DIR=${ZEN_BUILD_DIR}"
            -P "${CMAKE_CURRENT_LIST_DIR}/check_test_definitions.cmake"
    RESULT_VARIABLE defs_rc OUTPUT_VARIABLE defs_said ERROR_VARIABLE defs_said)
if(NOT defs_rc EQUAL 0)
    message(FATAL_ERROR "verify: the test compiles' definitions refused (exit ${defs_rc}).\n${defs_said}")
endif()
string(REGEX MATCH "test-definitions: PASSED[^\n]*" defs_passed "${defs_said}")
message(STATUS "verify: ${defs_passed}")

# ---- guard 3: run them, and let CTest refuse a zero too ----------------------------------

# ZEN_CTEST_APART names entries that run alone, one after another, once the rest have run with
# ZEN_CTEST_ARGS (VM-LANE-11): every name must be a registered entry, and both runs always run.
set(apart_regex "")
foreach(name IN LISTS ZEN_CTEST_APART)
    if(NOT name MATCHES "^[A-Za-z0-9_]+$" OR NOT name IN_LIST registered)
        message(FATAL_ERROR
            "verify: ZEN_CTEST_APART names '${name}', which is not a registered entry; an entry "
            "kept apart is kept apart by its exact name.")
    endif()
    string(APPEND apart_regex "|${name}")
endforeach()

if(apart_regex STREQUAL "")
    execute_process(
        COMMAND ${CMAKE_CTEST_COMMAND} --test-dir "${ZEN_BUILD_DIR}"
                --no-tests=error --output-on-failure ${ZEN_CTEST_ARGS}
        RESULT_VARIABLE run_rc)
    if(NOT run_rc EQUAL 0)
        message(FATAL_ERROR "verify: FAILED (ctest exit ${run_rc}) over ${selected} registered entries")
    endif()
else()
    string(SUBSTRING "${apart_regex}" 1 -1 apart_regex)
    execute_process(
        COMMAND ${CMAKE_CTEST_COMMAND} --test-dir "${ZEN_BUILD_DIR}"
                --no-tests=error --output-on-failure ${ZEN_CTEST_ARGS} -E "^(${apart_regex})$"
        RESULT_VARIABLE run_rc)
    message(STATUS "verify: the entries kept apart, alone: ${ZEN_CTEST_APART}")
    execute_process(
        COMMAND ${CMAKE_CTEST_COMMAND} --test-dir "${ZEN_BUILD_DIR}"
                --no-tests=error --output-on-failure ${ZEN_CTEST_ARGS} -j1 -R "^(${apart_regex})$"
        RESULT_VARIABLE apart_rc)
    if(NOT run_rc EQUAL 0 OR NOT apart_rc EQUAL 0)
        message(FATAL_ERROR "verify: FAILED (ctest exit ${run_rc}, and ${apart_rc} for the entries "
                            "kept apart) over ${selected} registered entries")
    endif()
endif()

message(STATUS "verify: PASSED -- ${selected} declared CTest entries registered, populated and executed")
