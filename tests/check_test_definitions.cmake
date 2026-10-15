# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
#
# Does a test compile carry a path of its checkout or build tree in a definition's value, which a
# compiler cache keys on and never rewrites (VM-CHECK-16)? Reads the tree's compile_commands.json,
# or ZEN_COMPILE_COMMANDS (`ninja -t compdb` writes one), for every compile whose object is under
# `tests/CMakeFiles/` (its `output`, else its `-o` or `/Fo`); tests/verify.cmake runs it.
#   cmake -DZEN_BUILD_DIR=<tree> [-DZEN_COMPILE_COMMANDS=<file>] -P tests/check_test_definitions.cmake

cmake_minimum_required(VERSION 3.16)

# The characters a list fears, swapped for control characters no command holds (VM-CHECK-03).
string(ASCII 1 ZEN_SOH) # was ;
string(ASCII 2 ZEN_STX) # was [
string(ASCII 3 ZEN_ETX) # was ]

# Text as the check reads it: forward slashes, no quotes or escapes, one slash at a time, and a
# slash an escape left before a drive gone. Paths are compared without case (`zen_defs_of`).
function(zen_defs_normal text out)
    string(REPLACE ";" "${ZEN_SOH}" text "${text}")
    string(REPLACE "[" "${ZEN_STX}" text "${text}")
    string(REPLACE "]" "${ZEN_ETX}" text "${text}")
    string(REPLACE "\\" "/" text "${text}")
    string(REPLACE "\"" "" text "${text}")
    string(REGEX REPLACE "/+" "/" text "${text}")
    string(REGEX REPLACE "(^|[ =])/([A-Za-z]:/)" "\\1\\2" text "${text}")
    set(${out} "${text}" PARENT_SCOPE)
endfunction()

# Every definition in one normalized compile command whose value begins with `source/` or
# `build/`, as `name=value` items, and how many definitions it read. A definition is `-DNAME=` or
# `/DNAME=`; its value runs to the next definition, so a path holding a space is read whole.
function(zen_defs_of text source build out_found out_read)
    string(REGEX REPLACE "[ \t]([-/]D[A-Za-z_][A-Za-z0-9_]*=)" ";\\1" pieces "${text}")
    set(found "")
    set(read 0)
    foreach(piece IN LISTS pieces)
        if(NOT piece MATCHES "^[-/]D([A-Za-z_][A-Za-z0-9_]*)=(.*)$")
            continue()
        endif()
        math(EXPR read "${read} + 1")
        set(name "${CMAKE_MATCH_1}")
        set(value "${CMAKE_MATCH_2}")
        foreach(root IN ITEMS "${source}/" "${build}/")
            string(LENGTH "${root}" root_len)
            string(SUBSTRING "${value}" 0 ${root_len} start)
            string(TOLOWER "${start}" start)
            string(TOLOWER "${root}" root)
            if(start STREQUAL root)
                string(REGEX REPLACE "/?[ \t]+[-/][A-Za-z].*$|/$" "" shown "${value}")
                list(APPEND found "${name}=${shown}")
                break()
            endif()
        endforeach()
    endforeach()
    set(${out_found} "${found}" PARENT_SCOPE)
    set(${out_read} ${read} PARENT_SCOPE)
endfunction()

# The verdict over a compile database's text: each test compile carrying a tree path, as
# `<object>: name=value`, and how many test compiles and definitions were read.
function(zen_defs_verdict json source build out_problems out_compiles out_defs)
    zen_defs_normal("${json}" plain)
    zen_defs_normal("${source}" source)
    zen_defs_normal("${build}" build)
    string(REGEX MATCHALL "{[^{}]*}" entries "${plain}")
    set(problems "")
    set(compiles 0)
    set(defs 0)
    foreach(entry IN LISTS entries)
        if(NOT entry MATCHES "(output: |[ \t]-o |[ \t]/Fo)([^ ,}]*tests/CMakeFiles/[^ ,}]*)")
            continue()
        endif()
        set(object "${CMAKE_MATCH_2}")
        math(EXPR compiles "${compiles} + 1")
        zen_defs_of("${entry}" "${source}" "${build}" found read)
        math(EXPR defs "${defs} + ${read}")
        foreach(f IN LISTS found)
            list(APPEND problems "${object}: ${f}")
        endforeach()
    endforeach()
    set(${out_problems} "${problems}" PARENT_SCOPE)
    set(${out_compiles} ${compiles} PARENT_SCOPE)
    set(${out_defs} ${defs} PARENT_SCOPE)
endfunction()

# ---- the self-test (VM-CHECK-01): make it say NO, and make it say YES -------------------------
# Two test compiles, as GCC's and MSVC's commands are written into the database, and a product's.
string(CONCAT st_json
    "[\n{\n  \"directory\": \"/b/tests\",\n"
    "  \"command\": \"/usr/bin/c++ -DGOOD=::zengine::testing::tree_path(\\\\\\\"GOOD\\\\\\\") -DFLAG=1 "
    "-DBAD=\\\\\\\"/s/rc/x.cpp\\\\\\\" -DSPACED=\\\\\\\"/b/my dir/y\\\\\\\" -I/s/rc "
    "-o tests/CMakeFiles/t.dir/a.cpp.o -c /s/rc/tests/a.cpp\",\n"
    "  \"file\": \"/s/rc/tests/a.cpp\"\n},\n"
    "{\n  \"directory\": \"C:/B\",\n"
    "  \"command\": \"cl.exe /nologo -DWIN=\\\\\\\"C:/B/tests/w.dll\\\\\\\" /DOK=\\\\\\\"elsewhere\\\\\\\" "
    "/Fotests\\\\CMakeFiles\\\\t.dir\\\\b.cpp.obj /c C:\\\\S\\\\b.cpp\",\n"
    "  \"file\": \"C:/S/b.cpp\",\n  \"output\": \"tests\\\\CMakeFiles\\\\t.dir\\\\b.cpp.obj\"\n},\n"
    "{\n  \"directory\": \"/b/workshop\",\n"
    "  \"command\": \"/usr/bin/c++ -DPRODUCT=\\\\\\\"/b/my dir/w\\\\\\\" -c /s/rc/workshop/w.cpp\",\n"
    "  \"file\": \"/s/rc/workshop/w.cpp\",\n  \"output\": \"workshop/CMakeFiles/w.dir/w.cpp.o\"\n}\n]")
zen_defs_verdict("${st_json}" "/s/rc" "C:/B" st_win_problems st_win_compiles st_win_defs)
zen_defs_verdict("${st_json}" "/s/rc" "/b/my dir" st_problems st_compiles st_defs)
list(LENGTH st_problems st_n)
list(LENGTH st_win_problems st_win_n)
if(NOT st_n EQUAL 2 OR NOT st_win_n EQUAL 2 OR NOT st_compiles EQUAL 2 OR NOT st_defs EQUAL 6
   OR NOT st_problems MATCHES "BAD=/s/rc/x.cpp" OR NOT st_problems MATCHES "SPACED=/b/my dir/y"
   OR NOT st_win_problems MATCHES "WIN=C:/B/tests/w.dll"
   OR st_problems MATCHES "GOOD|FLAG|OK|PRODUCT" OR st_win_problems MATCHES "GOOD|FLAG|OK|PRODUCT")
    message(FATAL_ERROR
        "test-definitions: SELF-TEST FAILED -- over two test compiles and a product's it read "
        "${st_compiles} test compiles and ${st_defs} definitions, and refused '${st_problems}' "
        "under a build tree with a space and '${st_win_problems}' under a drive (want BAD and "
        "SPACED, then BAD and WIN). A check that misreads a command passes every tree, or "
        "refuses every one.")
endif()
message(STATUS "test-definitions: self-test OK -- a checkout path and a build path refused, on "
               "GCC's spelling and MSVC's, a space and a drive letter read; a name, a flag, a path "
               "outside the tree and a product's compile let be")

# ---- the tree ----------------------------------------------------------------------------------
if(NOT DEFINED ZEN_BUILD_DIR OR ZEN_BUILD_DIR STREQUAL "")
    message(FATAL_ERROR "test-definitions: -DZEN_BUILD_DIR=<configured tree> is required")
endif()
get_filename_component(ZEN_BUILD_DIR "${ZEN_BUILD_DIR}" ABSOLUTE)
if(NOT EXISTS "${ZEN_BUILD_DIR}/CMakeCache.txt")
    message(FATAL_ERROR "test-definitions: '${ZEN_BUILD_DIR}' is not a configured tree")
endif()
file(STRINGS "${ZEN_BUILD_DIR}/CMakeCache.txt" home REGEX "^CMAKE_HOME_DIRECTORY:")
string(REGEX REPLACE "^[^=]*=" "" home "${home}")
if(NOT DEFINED ZEN_COMPILE_COMMANDS OR ZEN_COMPILE_COMMANDS STREQUAL "")
    set(ZEN_COMPILE_COMMANDS "${ZEN_BUILD_DIR}/compile_commands.json")
endif()
if(NOT EXISTS "${ZEN_COMPILE_COMMANDS}")
    message(FATAL_ERROR
        "test-definitions: no compile commands at '${ZEN_COMPILE_COMMANDS}'. tests/CMakeLists.txt "
        "exports them for a Makefile or Ninja generator; with another, name a file `ninja -t "
        "compdb` or the generator wrote, as -DZEN_COMPILE_COMMANDS=<file>.")
endif()
file(READ "${ZEN_COMPILE_COMMANDS}" json)
zen_defs_verdict("${json}" "${home}" "${ZEN_BUILD_DIR}" problems compiles defs)
if(compiles EQUAL 0)
    message(FATAL_ERROR
        "test-definitions: '${ZEN_COMPILE_COMMANDS}' holds no compile of a test target: an empty "
        "population is a red, never a pass.")
endif()
list(LENGTH problems n)
if(n GREATER 0)
    string(REPLACE ";" "\n    " shown "${problems}")
    message(FATAL_ERROR
        "test-definitions: ${n} definition(s) in test compiles carry a path of the checkout "
        "(${home}) or the build tree (${ZEN_BUILD_DIR}): give the target the path through "
        "zengine_test_definitions, which hands its cases the name and writes the path into the "
        "tree's own unit.\n    ${shown}")
endif()
message(STATUS "test-definitions: PASSED -- ${compiles} test compiles and ${defs} definitions read, "
               "none carrying a path of ${home} or ${ZEN_BUILD_DIR}")
