# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
#
# The `attribution` CI job: no commit reachable from the ref may carry a Co-authored-by trailer
# whose value names `claude` or `anthropic`, read by git's own trailer parser, so prose about
# Claude stays legal and a human co-author survives; the predicate is the one Loom carries here.
# A CI job, not a CTest entry, since a source export has no history. -DZEN_RANGE=<base>..<head>
# narrows it: cmake -P tests/check_commit_attribution.cmake, from the repository root, no build.

cmake_minimum_required(VERSION 3.16)

find_program(GIT_EXECUTABLE NAMES git)
if(NOT GIT_EXECUTABLE)
    message(FATAL_ERROR
        "attribution: no git executable, so the history cannot be read. A check that "
        "cannot run has not passed.")
endif()

if(NOT DEFINED ZEN_REPO OR ZEN_REPO STREQUAL "")
    set(ZEN_REPO "${CMAKE_CURRENT_LIST_DIR}/..")
endif()
if(NOT DEFINED ZEN_RANGE OR ZEN_RANGE STREQUAL "")
    set(ZEN_RANGE "HEAD")
endif()

# ---- the predicate, in one place so the self-test exercises the real one ------------

# Sets ${out} to the offending trailer value, or "" if the commit is clean. The set is the vendor
# and the product family, not one model name the next release would slip past, and no wider,
# lest it refuse a human whose name collides with a product.
function(zen_attribution_verdict sha out)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" -C "${ZEN_REPO}" log -1
                "--format=%(trailers:key=Co-authored-by,valueonly,separator=%x1F)" "${sha}"
        OUTPUT_VARIABLE values OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE rc ERROR_VARIABLE err)
    if(NOT rc EQUAL 0)
        message(FATAL_ERROR "attribution: could not read trailers of ${sha} (exit ${rc}).\n${err}")
    endif()
    string(TOLOWER "${values}" folded)
    if(folded MATCHES "claude" OR folded MATCHES "anthropic")
        set(${out} "${values}" PARENT_SCOPE)
    else()
        set(${out} "" PARENT_SCOPE)
    endif()
endfunction()

# Manufactures a dangling commit carrying ${message} and returns its sha.
function(zen_throwaway_commit message out)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env
                GIT_AUTHOR_NAME=attribution-selftest GIT_AUTHOR_EMAIL=selftest@invalid
                GIT_COMMITTER_NAME=attribution-selftest GIT_COMMITTER_EMAIL=selftest@invalid
                "${GIT_EXECUTABLE}" -C "${ZEN_REPO}" commit-tree "${selftest_tree}" -m "${message}"
        OUTPUT_VARIABLE sha OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE rc ERROR_VARIABLE err)
    if(NOT rc EQUAL 0)
        message(FATAL_ERROR "attribution: could not build a self-test commit (exit ${rc}).\n${err}")
    endif()
    set(${out} "${sha}" PARENT_SCOPE)
endfunction()

# ---- the self-test (VM-CHECK-01): make it say NO, and make it say YES -----------------
# Two throwaway commit objects, one carrying the forbidden trailer and one a human co-author
# with prose naming Claude, must reach opposite verdicts. Both dangle -- no ref, no index, no
# working-tree change -- and git discards them at the next gc.

execute_process(
    COMMAND "${GIT_EXECUTABLE}" -C "${ZEN_REPO}" rev-parse "HEAD^{tree}"
    OUTPUT_VARIABLE selftest_tree OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
    message(FATAL_ERROR "attribution: '${ZEN_REPO}' is not a readable git repository.\n${err}")
endif()

zen_throwaway_commit(
    "Self-test: the wave HIST-2 removed\n\nCo-Authored-By: Claude Opus 5 <noreply@anthropic.com>"
    caught_sha)
zen_attribution_verdict("${caught_sha}" caught)
if(caught STREQUAL "")
    message(FATAL_ERROR
        "attribution: SELF-TEST FAILED -- the check did not catch a commit carrying the "
        "exact trailer HIST-2 removed from 9 commits. It would have reported a clean "
        "history over a dirty one, which is the only outcome worse than not running.")
endif()

zen_throwaway_commit(
    "Self-test: legitimate collaboration\n\nThis message discusses Claude in prose, which is not attribution.\n\nCo-authored-by: A Human <human@example.invalid>"
    clean_sha)
zen_attribution_verdict("${clean_sha}" wrongly_caught)
if(NOT wrongly_caught STREQUAL "")
    message(FATAL_ERROR
        "attribution: SELF-TEST FAILED -- the check flagged a commit whose only co-author "
        "is a human being (\"${wrongly_caught}\"). It has started refusing honest "
        "collaborators, which is a different defect and not a safer one.")
endif()

message(STATUS "attribution: self-test OK -- the check catches the forbidden trailer and spares a human co-author")

# ---- the real population ------------------------------------------------------------

execute_process(
    COMMAND "${GIT_EXECUTABLE}" -C "${ZEN_REPO}" rev-list "${ZEN_RANGE}"
    OUTPUT_VARIABLE listing OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
    message(FATAL_ERROR "attribution: could not list commits for '${ZEN_RANGE}' (exit ${rc}).\n${err}")
endif()

string(REPLACE "\n" ";" commits "${listing}")
list(REMOVE_ITEM commits "")
list(LENGTH commits commit_count)
if(commit_count EQUAL 0)
    message(FATAL_ERROR
        "attribution: '${ZEN_RANGE}' selected ZERO commits. An expectation of nothing is "
        "satisfied by anything (POP-01), so this is a failure and not a quiet pass -- "
        "check the range, and check that the clone carries history (fetch-depth: 0).")
endif()

set(offenders "")
foreach(sha IN LISTS commits)
    zen_attribution_verdict("${sha}" value)
    if(NOT value STREQUAL "")
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" -C "${ZEN_REPO}" log -1 --format=%s "${sha}"
            OUTPUT_VARIABLE subject OUTPUT_STRIP_TRAILING_WHITESPACE)
        list(APPEND offenders "  ${sha}  ${subject}\n      Co-authored-by: ${value}")
    endif()
endforeach()

if(NOT offenders STREQUAL "")
    list(LENGTH offenders offender_count)
    string(REPLACE ";" "\n" text "${offenders}")
    message(FATAL_ERROR
        "attribution FAILED: ${offender_count} of ${commit_count} commit(s) reachable from "
        "'${ZEN_RANGE}' record an AI assistant as co-author.\n${text}\n\n"
        "  Zengine records no AI co-authors. Remove the trailer from the commit message -- "
        "amend if it is the tip, otherwise rewrite the affected messages as HIST-2 did "
        "(message-only, final tree unchanged) and force-push with an exact lease.")
endif()

message(STATUS
    "attribution: PASSED -- ${commit_count} commits reachable from '${ZEN_RANGE}', none "
    "recording an AI co-author")
