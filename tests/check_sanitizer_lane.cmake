# SPDX-License-Identifier: AGPL-3.0-only
# Copyright (c) 2026 Joshua DeMoss
#
# The `sanitizer_lane` entry (VM-LANE-27, VM-POP-22): the runners' shares and programs, a part's
# pieces, the time budget and the build's memory, each judgement refusing or warning on its fixed
# case and passing a clean sample; then every entry tests/test_population.txt declares, in one of
# the job's shares.
#   cmake -DZEN_REPO=<source tree> [-DZEN_WORK=<folder>] -P tests/check_sanitizer_lane.cmake

cmake_minimum_required(VERSION 3.21)

if(NOT DEFINED ZEN_REPO)
    get_filename_component(ZEN_REPO "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
endif()
include("${ZEN_REPO}/tests/sanitizer_lane.cmake")
include("${ZEN_REPO}/tests/check_population.cmake")

set(failures "")
set(passed 0)
# A judgement's problems, held to the case: `none` for a clean sample, else each pattern found.
function(zen_lane_expect case problems)
    set(wanted ${ARGN})
    string(REPLACE ";" " | " said "${problems}")
    if(wanted STREQUAL "none")
        if(NOT problems STREQUAL "")
            set(failures "${failures}  - ${case}: refused a clean sample: ${said}\n" PARENT_SCOPE)
            return()
        endif()
    else()
        foreach(pattern IN LISTS wanted)
            string(FIND "${problems}" "${pattern}" at)
            if(at EQUAL -1)
                set(failures "${failures}  - ${case}: wanted '${pattern}', said: ${said}\n" PARENT_SCOPE)
                return()
            endif()
        endforeach()
    endif()
    math(EXPR passed "${passed} + 1")
    set(passed ${passed} PARENT_SCOPE)
endfunction()

# The job's own settings, kept while the fixed cases set their own.
foreach(name RUNNERS ENTRIES_panes ENTRIES_rest)
    set(real_${name} "${ZEN_SANITIZER_${name}}")
endforeach()

# ---- the shares ------------------------------------------------------------------------------

set(ZEN_SANITIZER_RUNNERS one two)
set(registered x_1 x_2 y z)
set(ZEN_SANITIZER_ENTRIES_one x_*)
set(ZEN_SANITIZER_ENTRIES_two * EXCEPT x_*)
zen_sanitizer_shares("${registered}" problems)
zen_lane_expect("every entry in one share" "${problems}" none)
if(NOT share_one STREQUAL "x_1;x_2" OR NOT share_two STREQUAL "y;z")
    string(APPEND failures "  - every entry in one share: the shares read '${share_one}' and '${share_two}'\n")
endif()

set(ZEN_SANITIZER_ENTRIES_two y)
zen_sanitizer_shares("${registered}" problems)
zen_lane_expect("an entry in neither share" "${problems}" "z is in no runner's share")

set(ZEN_SANITIZER_ENTRIES_two *)
zen_sanitizer_shares("${registered}" problems)
zen_lane_expect("an entry in both shares" "${problems}" "x_1 is in the shares of one and two"
                "x_2 is in the shares of one and two")

set(ZEN_SANITIZER_ENTRIES_two z*)
zen_sanitizer_shares("${registered}" problems)
zen_lane_expect("a runner that runs nothing, and an entry it leaves" "${problems}"
                "y is in no runner's share")

set(ZEN_SANITIZER_ENTRIES_two x_[12] EXCEPT x_*)
zen_sanitizer_shares("${registered}" problems)
zen_lane_expect("a pattern of other characters" "${problems}" "names 'x_[12]'")

# ---- the programs ------------------------------------------------------------------------------

set(ZEN_SANITIZER_ENTRIES_two * EXCEPT x_*)
zen_sanitizer_shares("${registered}" problems)
set(ZENGINE_TARGET_x_1 prog-x)
set(ZENGINE_TARGET_x_2 prog-x)
set(ZENGINE_TARGET_y prog-y)
set(ZENGINE_SANITIZER_BUILDS_one prog-x)
set(ZENGINE_SANITIZER_BUILDS_two prog-y prog-z)
zen_sanitizer_programs(problems)
zen_lane_expect("each runner builds its entries' programs" "${problems}" none)

set(ZENGINE_SANITIZER_BUILDS_one prog-z)
zen_sanitizer_programs(problems)
zen_lane_expect("a runner that does not build its entry's program" "${problems}"
                "x_1 runs in one, which does not build its program prog-x")

set(ZENGINE_SANITIZER_BUILDS_one prog-x)
set(ZENGINE_SANITIZER_BUILDS_two prog-x prog-y)
set(ZENGINE_TARGET_y prog-x)
zen_sanitizer_programs(problems)
zen_lane_expect("a program whose entries run in two runners" "${problems}"
                "prog-x runs in one and in two (y)")
unset(ZENGINE_TARGET_x_1)
unset(ZENGINE_TARGET_x_2)
unset(ZENGINE_TARGET_y)

# ---- the pieces ------------------------------------------------------------------------------

set(cases_part "a.cpp:1 first" "a.cpp:9 second" "b.cpp:4 third")
set(cases_part_1 "a.cpp:1 first")
set(cases_part_2 "a.cpp:9 second" "b.cpp:4 third")
zengine_pieces_each_once(part "part_1;part_2" problems)
zen_lane_expect("each case in one piece" "${problems}" none)

set(cases_part_2 "b.cpp:4 third")
zengine_pieces_each_once(part "part_1;part_2" problems)
zen_lane_expect("a case in no piece of its part" "${problems}" "a.cpp:9 second is in no piece of part")

set(cases_part_1 "a.cpp:1 first" "a.cpp:9 second")
set(cases_part_2 "a.cpp:9 second" "b.cpp:4 third")
zengine_pieces_each_once(part "part_1;part_2" problems)
zen_lane_expect("a case in two pieces" "${problems}"
                "a.cpp:9 second is in the pieces part_1 and part_2 of part")

set(cases_part_1 "a.cpp:1 first" "c.cpp:2 stray")
set(cases_part_2 "a.cpp:9 second" "b.cpp:4 third")
zengine_pieces_each_once(part "part_1;part_2" problems)
zen_lane_expect("a piece's case its part does not hold" "${problems}"
                "c.cpp:2 stray is in the piece part_1 and not in its part part")

# ---- the budget ------------------------------------------------------------------------------

string(CONCAT junit
    "<testsuite name=\"Linux\" tests=\"3\">\n"
    "  <testcase name=\"fast\" classname=\"fast\" time=\"599.95\" status=\"run\">\n"
    "  <testcase name=\"slow\" classname=\"slow\" time=\"600.001\" status=\"run\">\n"
    "  <testcase name=\"whole\" classname=\"whole\" time=\"12\" status=\"run\">\n"
    "</testsuite>\n")
set(github "$ENV{GITHUB_ACTIONS}")
set(ENV{GITHUB_ACTIONS} "")
zen_sanitizer_times("${junit}" one 600 said over)
if(NOT over EQUAL 1)
    string(APPEND failures "  - an entry past the budget: ${over} warned, wanted 1 (slow)\n")
endif()
zen_lane_expect("an entry past the budget warns" "${said}"
                "sanitizer-lane: warning: slow took 600.00 s in the runner one, past the budget of 600 s"
                "the longest slow 600.00 s, fast 599.95 s, whole 12.00 s")
string(FIND "${said}" "warning: fast" fast_warned)
if(NOT fast_warned EQUAL -1)
    string(APPEND failures "  - an entry inside the budget warned: ${said}\n")
endif()
set(ENV{GITHUB_ACTIONS} true)
zen_sanitizer_times("${junit}" one 600 said over)
zen_lane_expect("an entry past the budget marks a GitHub run" "${said}"
                "::warning title=Sanitizer entry time::slow took 600.00 s")
set(ENV{GITHUB_ACTIONS} "${github}")
zen_sanitizer_times("${junit}" one 700 said over)
if(NOT over EQUAL 0)
    string(APPEND failures "  - every entry inside the budget: ${over} warned, wanted none\n")
endif()

# ---- the build's memory ----------------------------------------------------------------------

# A runner's build as the build step runs it, at a fixed case's limit: a tree whose
# `zengine-sanitizer-one` starts a process holding 128 MB, built under the memory watch in a
# GitHub run. A peak past the limit warns the run, one under it does not. The watch reads /proc,
# so these cases run on Linux, with a python3.
find_program(python NAMES python3)
if(NOT CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux" OR NOT python)
    message(STATUS "sanitizer-lane-check: the build's memory cases need Linux and a python3, and "
                   "go unrun here")
else()
    if(NOT DEFINED ZEN_WORK OR ZEN_WORK STREQUAL "")
        set(ZEN_WORK "${CMAKE_CURRENT_BINARY_DIR}/sanitizer-lane-work")
    endif()
    file(REMOVE_RECURSE "${ZEN_WORK}")
    file(WRITE "${ZEN_WORK}/hold.py"
        "import time\n"
        "held = bytearray(128 * 1024 * 1024)\n"
        "held[::4096] = b'x' * len(held[::4096])\n"
        "time.sleep(3)\n")
    file(WRITE "${ZEN_WORK}/tree/CMakeLists.txt"
        "cmake_minimum_required(VERSION 3.21)\n"
        "project(held LANGUAGES NONE)\n"
        "add_custom_target(zengine-sanitizer-one\n"
        "    COMMAND \"${python}\" \"${ZEN_WORK}/hold.py\")\n")
    file(WRITE "${ZEN_WORK}/build.cmake"
        "include(\"${ZEN_REPO}/tests/sanitizer_lane.cmake\")\n"
        "set(ZEN_SANITIZER_BUILD_PEAK_GB \${limit})\n"
        "zen_sanitizer_build(one 1 rc)\n"
        "message(\"build exit \${rc}\")\n")
    execute_process(COMMAND ${CMAKE_COMMAND} -S "${ZEN_WORK}/tree" -B "${ZEN_WORK}/tree/build"
                    RESULT_VARIABLE rc OUTPUT_VARIABLE text ERROR_VARIABLE text)
    if(NOT rc EQUAL 0)
        string(APPEND failures
               "  - the build's memory: its tree did not configure (exit ${rc}): ${text}\n")
    else()
        foreach(limit 0.05 1)
            execute_process(
                COMMAND ${CMAKE_COMMAND} -E env GITHUB_ACTIONS=true
                        ${CMAKE_COMMAND} "-DZEN_BUILD_DIR=${ZEN_WORK}/tree/build"
                        "-Dlimit=${limit}" -P "${ZEN_WORK}/build.cmake"
                RESULT_VARIABLE rc OUTPUT_VARIABLE said ERROR_VARIABLE said)
            # Shown as words, so a failure printed under GitHub Actions marks nothing.
            string(REPLACE "::warning" "[warning]" said "${said}")
            set(said_${limit} "")
            if(NOT rc EQUAL 0 OR NOT said MATCHES "build exit 0")
                string(APPEND failures "  - the build's memory at ${limit} GB: the build failed "
                                       "(exit ${rc}): ${said}\n")
            elseif(NOT said MATCHES "lane-memory: peak ([0-9.]+) GB" OR CMAKE_MATCH_1 STREQUAL "0.0")
                string(APPEND failures "  - the build's memory at ${limit} GB: no peak holding "
                                       "the 128 MB its build held: ${said}\n")
            else()
                set(said_${limit} "${said}")
            endif()
        endforeach()
        if(NOT said_0.05 STREQUAL "")
            zen_lane_expect("a build's peak past its limit warns the run" "${said_0.05}"
                            "[warning] title=Sanitizer build memory: past 0.05 GB::The peak, ")
        endif()
        if(NOT said_1 STREQUAL "")
            if(said_1 MATCHES "warning")
                string(APPEND failures "  - a build's peak under its limit warned: ${said_1}\n")
            else()
                math(EXPR passed "${passed} + 1")
            endif()
        endif()
    endif()
    file(REMOVE_RECURSE "${ZEN_WORK}")
endif()

# ---- the job's own shares, over every entry the manifest declares --------------------------

foreach(name RUNNERS ENTRIES_panes ENTRIES_rest)
    set(ZEN_SANITIZER_${name} "${real_${name}}")
endforeach()
file(STRINGS "${ZEN_REPO}/tests/test_population.txt" rows REGEX "^[A-Za-z0-9_]+[ \t]")
set(declared "")
foreach(row IN LISTS rows)
    string(REGEX MATCH "^[A-Za-z0-9_]+" entry "${row}")
    list(APPEND declared "${entry}")
endforeach()
list(REMOVE_DUPLICATES declared)
list(LENGTH declared declared_count)
if(declared_count EQUAL 0)
    message(FATAL_ERROR "sanitizer-lane-check: tests/test_population.txt declares no entry; an empty "
                        "population is a failure, not a pass (VM-POP-01)")
endif()
zen_sanitizer_shares("${declared}" problems)
zen_lane_expect("every declared entry in one of the job's shares" "${problems}" none)

if(NOT failures STREQUAL "")
    message(FATAL_ERROR "sanitizer-lane-check: FAILED\n${failures}")
endif()
set(counts "")
foreach(runner IN LISTS ZEN_SANITIZER_RUNNERS)
    list(LENGTH share_${runner} n)
    list(APPEND counts "${runner} ${n}")
endforeach()
string(REPLACE ";" ", " counts "${counts}")
message(STATUS "sanitizer-lane-check: PASSED -- ${passed} cases; the ${declared_count} declared "
               "entries in the job's shares: ${counts}")
