# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
#
# The Zengine population contract (C4, closing COLD-2 C-4). Included by tests/verify.cmake,
# which is the only thing that runs it; it defines one function and executes nothing on its
# own.
#
# It answers the question a green CTest run cannot answer for itself: *did the test
# population this repository claims to verify actually exist in the build that was tested?*
# It executes no test cases. `--count` is a doctest QUERY mode, so nothing runs and the whole
# check costs a fraction of a second.
#
# THREE SOURCES, and the contract is that they agree:
#
#   tests/test_population.txt                  what MUST be registered   (source tree)
#   `ctest -N`                                 what IS registered        (CTest)
#   <build>/zengine-test-population.cmake      what each registration declared itself to be,
#                                              and which gates this configuration has (build)
#
# Only the first can survive the deletion of a registration, which is why it is a written
# file and not a derivation. The third is a DESCRIPTION -- it never relaxes the first.

cmake_minimum_required(VERSION 3.16)

# ---- asking a doctest binary about itself ----------------------------------------------

# How many cases the binary selects with no filter. 0 is a legitimate answer to a QUERY
# (nothing ran), and it is exactly the answer the floor exists to catch.
function(zengine_case_count exe out_var)
    execute_process(COMMAND "${exe}" --count
                    OUTPUT_VARIABLE captured ERROR_VARIABLE errors RESULT_VARIABLE rc)
    if(NOT rc EQUAL 0)
        message(FATAL_ERROR
            "population: `${exe} --count` failed (exit ${rc}). A test binary that cannot even "
            "be asked for its inventory has no population to report.\n${captured}${errors}")
    endif()
    if(NOT captured MATCHES "filters: ([0-9]+)")
        message(FATAL_ERROR
            "population: could not read a case count out of `${exe} --count`. doctest's "
            "--count output shape changed; this check must be repaired rather than "
            "removed.\n${captured}")
    endif()
    set(${out_var} "${CMAKE_MATCH_1}" PARENT_SCOPE)
endfunction()

# THE CANARY. Every floor below is read out of the same binary that is supposed to refuse an
# empty population, so this proves the refusal is still compiled into it -- with a filter
# that cannot match anything, which is the mutation itself, run for real on every
# verification. Without it, a binary that quietly went back to a stock doctest main would
# still report its case counts happily and the floors would all pass; the thing that had
# stopped working is the thing nothing was asking about. (C3's lesson, one repository over:
# a check whose gate is a derivation goes blind exactly where the derivation does.)
function(zengine_assert_refuses_empty_population entry exe)
    execute_process(COMMAND "${exe}" "--test-case=zengine_no_such_case_population_canary"
                    OUTPUT_VARIABLE captured ERROR_VARIABLE errors RESULT_VARIABLE rc)
    if(rc EQUAL 70 AND "${captured}${errors}" MATCHES "EMPTY TEST POPULATION")
        return()
    endif()
    message(FATAL_ERROR
        "population: '${entry}' (${exe}) does NOT refuse an empty population.\n"
        "  Asked it to run a test case that does not exist; expected exit 70 and an EMPTY "
        "TEST POPULATION diagnostic, got exit ${rc}.\n"
        "  Stock doctest calls a run that selected zero cases 'Status: SUCCESS!' and exits 0 "
        "(COLD-2 C-4). This binary must be linked against tests/doctest_main.cpp, which is "
        "what turns that into a failure. Every case floor this check reports is read out of "
        "this same binary, so a binary that no longer knows 'nothing ran' is not success "
        "cannot be trusted to answer for its own population either.\n${captured}${errors}")
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

        if(NOT kind MATCHES "^(doctest|compile-negative|compile-positive|program)$")
            message(FATAL_ERROR
                "population: '${entry}' declares an unknown kind '${kind}'. The kinds are "
                "doctest, compile-negative, compile-positive and program; each carries a "
                "different population question, so a new one is a deliberate edit to "
                "tests/check_population.cmake as well as to the manifest.")
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

    # ---- 1. inventory, exact -----------------------------------------------------------

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

    # ---- 2 + 3. the per-kind contract, for the entries that are actually there ----------

    set(report "")
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
            zengine_assert_refuses_empty_population("${entry}" "${exe}")
            zengine_case_count("${exe}" count)
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
        else()
            string(APPEND report "  ${entry}: program, ${exe}\n")
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
