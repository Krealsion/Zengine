# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
#
# The population contract, included by tests/verify.cmake alone: did the population this
# repository claims to verify exist in the build that was tested (VM-POP-01)? Three sources agree:
# tests/test_population.txt, what must be registered and the only one a deleted registration
# cannot reach; `ctest -N`, what is; and <build>/zengine-test-population.cmake, what each
# registration declared itself to be, a description that never relaxes the manifest.

cmake_minimum_required(VERSION 3.16)

# ---- asking a doctest binary about itself ----------------------------------------------

# How many cases the binary selects with the entry's own arguments (none: every case). 0 is a
# legitimate answer to a QUERY (nothing ran), and it is exactly the answer the floor exists to
# catch.
function(zengine_case_count exe out_var)
    execute_process(COMMAND "${exe}" ${ARGN} --count
                    OUTPUT_VARIABLE captured ERROR_VARIABLE errors RESULT_VARIABLE rc)
    if(NOT rc EQUAL 0)
        message(FATAL_ERROR
            "population: `${exe} ${ARGN} --count` failed (exit ${rc}). A test binary that cannot "
            "even be asked for its inventory has no population to report.\n${captured}${errors}")
    endif()
    if(NOT captured MATCHES "filters: ([0-9]+)")
        message(FATAL_ERROR
            "population: could not read a case count out of `${exe} --count`. doctest's "
            "--count output shape changed; this check must be repaired rather than "
            "removed.\n${captured}")
    endif()
    set(${out_var} "${CMAKE_MATCH_1}" PARENT_SCOPE)
endfunction()

# The canary: every floor is read out of the binary that must refuse an empty population, so each
# run proves the refusal is still compiled in, with a filter that matches nothing, beside the
# entry's own arguments. Without it, a binary back on a stock doctest main would report its
# counts and pass every floor (VM-POP-04).
function(zengine_assert_refuses_empty_population entry exe)
    execute_process(COMMAND "${exe}" ${ARGN} "--test-case=zengine_no_such_case_population_canary"
                    OUTPUT_VARIABLE captured ERROR_VARIABLE errors RESULT_VARIABLE rc)
    if(rc EQUAL 70 AND "${captured}${errors}" MATCHES "EMPTY TEST POPULATION")
        return()
    endif()
    message(FATAL_ERROR
        "population: '${entry}' (${exe}) does NOT refuse an empty population.\n"
        "  Asked it to run a test case that does not exist; expected exit 70 and an EMPTY "
        "TEST POPULATION diagnostic, got exit ${rc}.\n"
        "  Stock doctest calls a run that selected zero cases 'Status: SUCCESS!' and exits 0 "
        "-- a pass over nothing. This binary must link tests/doctest_main.cpp, which is "
        "what turns that into a failure. Every case floor this check reports is read out of "
        "this same binary, so a binary that no longer knows 'nothing ran' is not success "
        "cannot be trusted to answer for its own population either.\n${captured}${errors}")
endfunction()

# The cases `exe` selects with the given arguments, each as `file:line name` (doctest's XML list).
function(zengine_case_list exe out_var)
    execute_process(COMMAND "${exe}" ${ARGN} --reporters=xml --list-test-cases
                    OUTPUT_VARIABLE captured ERROR_VARIABLE errors RESULT_VARIABLE rc)
    if(NOT rc EQUAL 0)
        message(FATAL_ERROR
            "population: `${exe} ${ARGN} --list-test-cases` failed (exit ${rc}).\n${errors}")
    endif()
    # A list item keeps no `;`, and an unbalanced bracket would stop CMake splitting the list.
    string(REPLACE ";" "," captured "${captured}")
    string(REPLACE "[" "(" captured "${captured}")
    string(REPLACE "]" ")" captured "${captured}")
    # doctest leaves an empty attribute out, and a case outside every suite has an empty one.
    set(one_case
        "<TestCase name=\"([^\"]*)\"( testsuite=\"[^\"]*\")? filename=\"([^\"]*)\" line=\"([0-9]+)\"")
    string(REGEX MATCHALL "${one_case}" found "${captured}")
    set(listed "")
    foreach(one IN LISTS found)
        string(REGEX REPLACE "^${one_case}$" "\\3:\\4 \\1" one "${one}")
        string(REPLACE "&quot;" "\"" one "${one}")
        string(REPLACE "&apos;" "'" one "${one}")
        string(REPLACE "&lt;" "<" one "${one}")
        string(REPLACE "&gt;" ">" one "${one}")
        string(REPLACE "&amp;" "&" one "${one}")
        list(APPEND listed "${one}")
    endforeach()
    set(${out_var} "${listed}" PARENT_SCOPE)
endfunction()

# Names the cases of a program its entries do not select exactly once, when each entry selects by
# one `--source-file`: those no entry's patterns match, and those an entry shares with another.
function(zengine_cases_not_once exe entries out_var)
    set(all "")
    foreach(entry IN LISTS entries)
        list(LENGTH ZENGINE_ARGS_${entry} n)
        if(NOT n EQUAL 1 OR NOT "${ZENGINE_ARGS_${entry}}" MATCHES "^--source-file=(.+)$")
            string(CONCAT why "  (not named: '${entry}' does not select by one --source-file "
                              "alone, so list each entry's cases with --list-test-cases)\n")
            set(${out_var} "${why}" PARENT_SCOPE)
            return()
        endif()
        set(patterns_${entry} "${CMAKE_MATCH_1}")
        list(APPEND all "${CMAKE_MATCH_1}")
    endforeach()
    string(REPLACE ";" "," all "${all}")
    set(named "")
    zengine_case_list("${exe}" none "--source-file-exclude=${all}")
    foreach(one IN LISTS none)
        string(APPEND named "  no entry selects: ${one}\n")
    endforeach()
    foreach(entry IN LISTS entries)
        set(others "")
        foreach(other IN LISTS entries)
            if(NOT other STREQUAL entry)
                list(APPEND others "${patterns_${other}}")
            endif()
        endforeach()
        string(REPLACE ";" "," others "${others}")
        zengine_case_list("${exe}" mine ${ZENGINE_ARGS_${entry}})
        zengine_case_list("${exe}" alone ${ZENGINE_ARGS_${entry}} "--source-file-exclude=${others}")
        if(alone)
            list(REMOVE_ITEM mine ${alone})
        endif()
        foreach(one IN LISTS mine)
            string(APPEND named "  '${entry}' and another entry select: ${one}\n")
        endforeach()
    endforeach()
    set(${out_var} "${named}" PARENT_SCOPE)
endfunction()

# ---- the contract ------------------------------------------------------------------------
#
#   manifest    tests/test_population.txt
#   build_dir   the configured CTest build tree
#   registered  the entry names `ctest -N` reported, as a CMake list

function(zengine_check_population manifest build_dir registered)
    set(record "${build_dir}/zengine-test-population.cmake")
    if(NOT EXISTS "${record}")
        message(FATAL_ERROR
            "population: '${build_dir}' has no zengine-test-population.cmake. That file is "
            "written at configure time by Zengine/CMakeLists.txt and says which gates this "
            "configuration has and what each registered test declared itself to be. Without "
            "it there is no way to tell a doctest surface from a compile-negative one, so "
            "there is no honest population question to ask. Reconfigure the build tree.")
    endif()
    if(NOT EXISTS "${manifest}")
        message(FATAL_ERROR "population: the expected-population manifest is missing: ${manifest}")
    endif()
    include("${record}")

    if(NOT DEFINED ZENGINE_ACTIVE_GATES OR ZENGINE_ACTIVE_GATES STREQUAL "")
        message(FATAL_ERROR
            "population: ${record} declares no active gates. Every supported configuration "
            "has at least `always`; an empty gate set would silently deactivate every row of "
            "the manifest and leave nothing to expect.")
    endif()

    # ---- read what must exist ----------------------------------------------------------

    file(STRINGS "${manifest}" manifest_lines)
    set(entries "")           # every entry named, in file order
    set(expected "")          # ...that an active gate reaches
    set(declared_absent "")
    set(problems "")

    foreach(line IN LISTS manifest_lines)
        string(REGEX REPLACE "#.*$" "" line "${line}")
        string(REPLACE "\t" " " line "${line}")
        string(STRIP "${line}" line)
        if(line STREQUAL "")
            continue()
        endif()
        if(NOT line MATCHES "^([^ ]+)[ ]+([^ ]+)[ ]+([^ ]+)[ ]*(.*)$")
            message(FATAL_ERROR
                "population: malformed manifest line (want `<entry> <kind> <gate> [detail]`): ${line}")
        endif()
        set(entry "${CMAKE_MATCH_1}")
        set(kind "${CMAKE_MATCH_2}")
        set(gate "${CMAKE_MATCH_3}")
        string(STRIP "${CMAKE_MATCH_4}" detail)

        # A semicolon would be eaten by CMake's own list expansion somewhere downstream --
        # quietly, and in the direction of a weaker check. Refused at the door instead.
        if(detail MATCHES ";")
            message(FATAL_ERROR
                "population: '${entry}' has a semicolon in its detail field. CMake treats "
                "that as a list separator, so the value would not survive intact to the "
                "comparison. Spell the pattern without one.")
        endif()

        if(NOT kind MATCHES "^(doctest|compile-negative|compile-positive|program|script)$")
            message(FATAL_ERROR
                "population: '${entry}' declares an unknown kind '${kind}'. The kinds are "
                "doctest, compile-negative, compile-positive, program and script; each "
                "carries a different population question, so a new one is a deliberate edit "
                "to tests/check_population.cmake as well as to the manifest.")
        endif()

        list(FIND entries "${entry}" known)
        if(known EQUAL -1)
            list(APPEND entries "${entry}")
            set(kind_${entry} "${kind}")
            set(detail_${entry} "${detail}")
            set(floor_${entry} 0)
            set(present_${entry} 0)
            set(rows_${entry} "")
            set(formula_${entry} "")
        else()
            if(NOT kind STREQUAL "${kind_${entry}}")
                message(FATAL_ERROR
                    "population: '${entry}' is declared as both '${kind_${entry}}' and "
                    "'${kind}'. One entry, one kind.")
            endif()
            if(NOT kind STREQUAL "doctest")
                message(FATAL_ERROR
                    "population: '${entry}' carries more than one row, but only a doctest "
                    "floor is summed over gates. A ${kind} entry either exists or does not.")
            endif()
        endif()
        list(APPEND rows_${entry} "${gate}")

        if(kind STREQUAL "doctest")
            if(NOT detail MATCHES "^[0-9]+$")
                message(FATAL_ERROR
                    "population: doctest entry '${entry}' has a non-numeric case floor: '${detail}'")
            endif()
        elseif(kind STREQUAL "compile-negative")
            if(detail STREQUAL "")
                message(FATAL_ERROR
                    "population: compile-negative entry '${entry}' declares no diagnostic. "
                    "The diagnostic is what makes it evidence rather than an observation "
                    "that a build failed; an entry without one is not this kind.")
            endif()
        elseif(NOT detail STREQUAL "")
            message(FATAL_ERROR
                "population: ${kind} entry '${entry}' carries a detail field ('${detail}'), "
                "and that kind has nothing to put in it.")
        endif()

        list(FIND ZENGINE_ACTIVE_GATES "${gate}" gate_active)
        if(gate_active EQUAL -1)
            continue()
        endif()
        set(present_${entry} 1)
        if(kind STREQUAL "doctest")
            math(EXPR floor_${entry} "${floor_${entry}} + ${detail}")
            if(formula_${entry} STREQUAL "")
                set(formula_${entry} "${detail} ${gate}")
            else()
                set(formula_${entry} "${formula_${entry}} + ${detail} ${gate}")
            endif()
        endif()
    endforeach()

    foreach(entry IN LISTS entries)
        if(present_${entry})
            list(APPEND expected "${entry}")
        else()
            string(REPLACE ";" "/" why "${rows_${entry}}")
            list(APPEND declared_absent "${entry} (gate '${why}' off here)")
        endif()
    endforeach()

    # ---- the inventory, exact ------------------------------------------------------------

    set(sorted_expected ${expected})
    set(sorted_registered ${registered})
    list(SORT sorted_expected)
    list(SORT sorted_registered)

    foreach(entry IN LISTS sorted_expected)
        list(FIND sorted_registered "${entry}" idx)
        if(idx EQUAL -1)
            string(CONCAT msg
                "MISSING: CTest entry '${entry}' is declared in the manifest but is NOT "
                "registered in this build -- deleted, renamed, or gated out of a "
                "configuration that is supposed to have it. Missing tests are absence of "
                "evidence, never successful evidence.")
            list(APPEND problems "${msg}")
        endif()
    endforeach()
    foreach(entry IN LISTS sorted_registered)
        list(FIND sorted_expected "${entry}" idx)
        if(idx EQUAL -1)
            string(CONCAT msg
                "UNEXPECTED: CTest entry '${entry}' is registered in this build but is NOT "
                "declared in ${manifest}. Declare it (kind, gate, and whatever its kind "
                "requires) so the inventory stays a contract rather than a description.")
            list(APPEND problems "${msg}")
        endif()
    endforeach()

    # ---- each kind's contract, for the entries that are there ----------------------------

    set(report "")
    set(programs "")
    foreach(entry IN LISTS sorted_expected)
        list(FIND sorted_registered "${entry}" idx)
        if(idx EQUAL -1)
            continue()
        endif()

        set(kind "${kind_${entry}}")
        if(NOT DEFINED ZENGINE_KIND_${entry})
            string(CONCAT msg
                "UNRECORDED: CTest entry '${entry}' is registered, but the build recorded "
                "nothing about what kind of evidence it is. Register it through "
                "zengine_doctest_test / zengine_compile_test / zengine_program_test rather "
                "than a bare add_test(), so the population question it answers is stated "
                "where the registration is.")
            list(APPEND problems "${msg}")
            continue()
        endif()
        if(NOT kind STREQUAL "${ZENGINE_KIND_${entry}}")
            string(CONCAT msg
                "KIND: CTest entry '${entry}' is declared '${kind}' in the manifest, but "
                "the build registered it as '${ZENGINE_KIND_${entry}}'. The kind decides "
                "which population question is asked of it, so the two must agree.")
            list(APPEND problems "${msg}")
            continue()
        endif()

        set(exe "")
        if(kind STREQUAL "doctest" OR kind STREQUAL "program")
            set(exe "${ZENGINE_EXE_${entry}}")
            if(NOT EXISTS "${exe}")
                string(CONCAT msg
                    "BINARY: CTest entry '${entry}' is registered, but its program does not "
                    "exist: ${exe}. Build the tree before verifying it.")
                list(APPEND problems "${msg}")
                continue()
            endif()
        endif()

        if(kind STREQUAL "doctest")
            set(minimum "${floor_${entry}}")
            zengine_assert_refuses_empty_population("${entry}" "${exe}" ${ZENGINE_ARGS_${entry}})
            zengine_case_count("${exe}" count ${ZENGINE_ARGS_${entry}})
            # Gathered by program, for the whole below.
            string(MD5 program "${exe}")
            if(NOT DEFINED entries_of_${program})
                list(APPEND programs "${exe}")
            endif()
            list(APPEND entries_of_${program} "${entry}")
            set(count_${entry} "${count}")
            if(count LESS minimum)
                string(CONCAT msg
                    "FLOOR: ${entry} selected ${count} cases, below declared floor "
                    "${minimum}. Test cases were removed or compiled out. A deliberate "
                    "decrease in evidence is an edit to ${manifest}, not a smaller green.")
                list(APPEND problems "${msg}")
            endif()
            string(APPEND report
                   "  ${entry}: doctest, ${count} cases (floor ${minimum} = ${formula_${entry}})\n")
        elseif(kind STREQUAL "compile-negative")
            if(NOT DEFINED ZENGINE_DIAGNOSTIC_${entry})
                string(CONCAT msg
                    "DIAGNOSTIC: compile-negative entry '${entry}' carries no diagnostic "
                    "pattern in this build. Without one it passes on any non-zero build "
                    "exit, which is 'the compiler complained about something' and not "
                    "'the compiler refused this for the stated reason'.")
                list(APPEND problems "${msg}")
            elseif(NOT "${ZENGINE_DIAGNOSTIC_${entry}}" STREQUAL "${detail_${entry}}")
                string(CONCAT msg
                    "DIAGNOSTIC: compile-negative entry '${entry}' is judged on "
                    "'${ZENGINE_DIAGNOSTIC_${entry}}', but ${manifest} requires "
                    "'${detail_${entry}}'. Changing what a refusal must say is a reviewed "
                    "edit, not a build-file detail.")
                list(APPEND problems "${msg}")
            else()
                string(APPEND report
                       "  ${entry}: compile-negative, judged on \"${detail_${entry}}\"\n")
            endif()
        elseif(kind STREQUAL "compile-positive")
            if(DEFINED ZENGINE_DIAGNOSTIC_${entry})
                string(CONCAT msg
                    "CONTROL: '${entry}' is the positive control -- it must BUILD -- but "
                    "this build judges it on the output pattern "
                    "'${ZENGINE_DIAGNOSTIC_${entry}}'. PASS_REGULAR_EXPRESSION makes CTest "
                    "ignore the exit status, so a control judged on output could be "
                    "satisfied by a fixture that never compiled.")
                list(APPEND problems "${msg}")
            else()
                string(APPEND report "  ${entry}: compile-positive, judged on building\n")
            endif()
        elseif(kind STREQUAL "program")
            string(APPEND report "  ${entry}: program, ${exe}\n")
        else()
            string(APPEND report "  ${entry}: script, run over the source tree\n")
        endif()
    endforeach()

    # ---- one program, several entries: every case once ------------------------------------
    # Each entry's floor holds its own part, so a case no entry selects would leave every floor
    # green: the program's unfiltered count must be the sum of its entries' (VM-POP-22).
    foreach(exe IN LISTS programs)
        string(MD5 program "${exe}")
        list(LENGTH entries_of_${program} shared)
        if(shared LESS 2)
            continue()
        endif()
        zengine_case_count("${exe}" whole)
        set(sum 0)
        set(each "")
        foreach(entry IN LISTS entries_of_${program})
            math(EXPR sum "${sum} + ${count_${entry}}")
            string(APPEND each "    ${entry}: ${count_${entry}}\n")
        endforeach()
        string(APPEND report
               "  ${exe}: ${whole} cases, run by ${shared} entries summing to ${sum}\n")
        if(NOT sum EQUAL whole)
            zengine_cases_not_once("${exe}" "${entries_of_${program}}" named)
            string(CONCAT msg
                "WHOLE: ${exe} holds ${whole} cases, and the ${shared} entries that run it select "
                "${sum}. A case no entry selects runs nowhere while every floor stays green, and a "
                "case two entries select runs twice. Each entry's count:\n${each}${named}")
            list(APPEND problems "${msg}")
        endif()
    endforeach()

    # ---- the report --------------------------------------------------------------------

    list(LENGTH expected expected_count)
    list(LENGTH registered registered_count)
    message(STATUS "population: gates active: ${ZENGINE_ACTIVE_GATES}")
    message(STATUS "population: expected CTest entries: ${expected_count}")
    message(STATUS "population: actual CTest entries: ${registered_count}")
    if(NOT report STREQUAL "")
        message("${report}")
    endif()
    if(NOT declared_absent STREQUAL "")
        message(STATUS "population: DECLARED ABSENT in this configuration (not run, and not passed):")
        foreach(absent IN LISTS declared_absent)
            message("  ${absent}")
        endforeach()
    endif()

    if(NOT problems STREQUAL "")
        set(text "")
        foreach(problem IN LISTS problems)
            string(APPEND text "  - ${problem}\n")
        endforeach()
        message(FATAL_ERROR
            "population: the verified population does not match the declared one.\n${text}"
            "  (contract: ${manifest})")
    endif()

    message(STATUS
        "population: OK -- ${expected_count} declared CTest entries all present, correctly "
        "kinded, and above their floors")
endfunction()
