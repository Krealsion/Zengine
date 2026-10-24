# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
#
# The `configure_switches` entry: a configure given the retired `ZENGINE_SDL_SKIN`, or run in a
# tree that kept it, stops before it seeks anything, naming the switch and its fix; a configure
# without it says nothing of it. Each configure runs in a folder of its own beside this entry.
#   cmake [-DZEN_REPO=<repository root>] [-DZEN_WORK=<folder>] -P tests/check_configure_switches.cmake

cmake_minimum_required(VERSION 3.16)

if(NOT DEFINED ZEN_REPO OR ZEN_REPO STREQUAL "")
    set(ZEN_REPO "${CMAKE_CURRENT_LIST_DIR}/..")
endif()
get_filename_component(ZEN_REPO "${ZEN_REPO}" ABSOLUTE)
if(NOT EXISTS "${ZEN_REPO}/AGENTS.md")
    message(FATAL_ERROR "configure-switches: '${ZEN_REPO}' is not this repository's root "
                        "(no AGENTS.md). Pass -DZEN_REPO=<repository root>.")
endif()
if(NOT DEFINED ZEN_WORK OR ZEN_WORK STREQUAL "")
    set(ZEN_WORK "${CMAKE_CURRENT_BINARY_DIR}/configure-switches-work")
endif()

set(problems "")

# One configure in a fresh folder, a CMakeCache.txt written there first when `cache` is given. A
# Loom that is nowhere is named, so a configure that gets past the switch stops at the Loom at most.
function(configure_once name cache out_rc out_text)
    set(tree "${ZEN_WORK}/${name}")
    file(REMOVE_RECURSE "${tree}")
    file(MAKE_DIRECTORY "${tree}")
    if(NOT cache STREQUAL "")
        file(WRITE "${tree}/CMakeCache.txt" "${cache}\n")
    endif()
    execute_process(
        COMMAND ${CMAKE_COMMAND} -S "${ZEN_REPO}" -B "${tree}" -DZEN_LOOM_DEV=OFF
                "-Dloom_DIR=${tree}/no-loom-here" ${ARGN}
        RESULT_VARIABLE rc OUTPUT_VARIABLE text ERROR_VARIABLE text)
    string(REGEX REPLACE "[ \t\r\n]+" " " text "${text}")
    set(${out_rc} "${rc}" PARENT_SCOPE)
    set(${out_text} "${text}" PARENT_SCOPE)
endfunction()

# GIVEN TO THIS CONFIGURE: refused, by name, with the switch that replaces it.
configure_once(given "" rc text -DZENGINE_SDL_SKIN=OFF)
if(rc EQUAL 0 OR NOT text MATCHES "ZENGINE_SDL_SKIN is retired \\(given to this configure"
   OR NOT text MATCHES "-DZENGINE_TUI_ONLY=ON" OR NOT text MATCHES "-UZENGINE_SDL_SKIN")
    list(APPEND problems "a configure given -DZENGINE_SDL_SKIN=OFF was not refused by name with "
                         "its fix (exit ${rc}): ${text}")
endif()
if(text MATCHES "loom|compiler identification")
    list(APPEND problems "a configure given the retired switch sought a compiler or the Loom before "
                         "refusing it")
endif()

# KEPT BY A TREE an earlier configure made: refused, saying so.
configure_once(kept "ZENGINE_SDL_SKIN:BOOL=ON" rc text)
if(rc EQUAL 0 OR NOT text MATCHES "ZENGINE_SDL_SKIN is retired \\(kept by this build tree")
    list(APPEND problems "a tree keeping ZENGINE_SDL_SKIN was not refused by name (exit ${rc}): "
                         "${text}")
endif()

# ...AND CLEARED FROM IT, and in a configure that never named it: nothing said of it.
configure_once(cleared "ZENGINE_SDL_SKIN:BOOL=ON" rc text -UZENGINE_SDL_SKIN
               -DZENGINE_TUI_ONLY=ON)
if(text MATCHES "ZENGINE_SDL_SKIN")
    list(APPEND problems "a tree cleared with -UZENGINE_SDL_SKIN was still refused: ${text}")
endif()
configure_once(plain "" rc text)
if(text MATCHES "ZENGINE_SDL_SKIN")
    list(APPEND problems "a configure never naming the switch spoke of it: ${text}")
endif()

file(REMOVE_RECURSE "${ZEN_WORK}")
if(problems)
    list(JOIN problems "\n  " said)
    message(FATAL_ERROR "configure-switches FAILED:\n  ${said}")
endif()
message(STATUS "configure-switches: PASSED -- the retired ZENGINE_SDL_SKIN refused by name, given "
               "and kept, before a compiler is sought; cleared or never named, nothing said of it")
