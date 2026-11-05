# SPDX-License-Identifier: AGPL-3.0-only
# Copyright (c) 2026 Joshua DeMoss
#
# The sanitizer lane as CI's sanitizer job runs it, from here alone: the official lane
# (tests/verify.cmake) over an ASan + UBSan tree, its CTest arguments and the entries it keeps
# apart below, under tests/lane_memory.py, which says the lane's peak and warns past the limit.
#   cmake -DZEN_BUILD_DIR=build-san -P tests/sanitizer_lane.cmake

cmake_minimum_required(VERSION 3.16)

# The limit is a standard GitHub runner's 16 GB less a quarter spare (VM-LANE-27).
set(ZEN_SANITIZER_PEAK_GB 12)
set(ZEN_SANITIZER_CTEST_ARGS -j4)
# The largest peaks measured alone: beside the rest, four at a time, they take the lane past the
# limit (VM-LANE-11), and their memory grows for as long as they run.
set(ZEN_SANITIZER_APART workshop_panes_project workshop_panes_info)

if(NOT DEFINED ZEN_BUILD_DIR OR ZEN_BUILD_DIR STREQUAL "")
    message(FATAL_ERROR "sanitizer-lane: pass -DZEN_BUILD_DIR=<an ASan + UBSan build tree>")
endif()
foreach(own ZEN_CTEST_ARGS ZEN_CTEST_APART)
    if(DEFINED ${own})
        message(FATAL_ERROR "sanitizer-lane: ${own} is this lane's own, set in this file; run "
                            "tests/verify.cmake for a lane with arguments of yours")
    endif()
endforeach()

string(REPLACE ";" "\\;" apart "${ZEN_SANITIZER_APART}")
set(verify "${CMAKE_COMMAND}" "-DZEN_BUILD_DIR=${ZEN_BUILD_DIR}"
    "-DZEN_CTEST_ARGS=${ZEN_SANITIZER_CTEST_ARGS}" "-DZEN_CTEST_APART=${apart}"
    -P "${CMAKE_CURRENT_LIST_DIR}/verify.cmake")
find_program(ZEN_PYTHON3 NAMES python3)
if(ZEN_PYTHON3)
    execute_process(COMMAND "${ZEN_PYTHON3}" "${CMAKE_CURRENT_LIST_DIR}/lane_memory.py"
                            --limit-gb ${ZEN_SANITIZER_PEAK_GB} --title "Sanitizer lane memory"
                            -- ${verify}
                    RESULT_VARIABLE rc)
else()
    message(STATUS "sanitizer-lane: no python3, so the lane runs without its memory said")
    execute_process(COMMAND ${verify} RESULT_VARIABLE rc)
endif()
if(NOT rc EQUAL 0)
    message(FATAL_ERROR "sanitizer-lane: FAILED (exit ${rc})")
endif()
