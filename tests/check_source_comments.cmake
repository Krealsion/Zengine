# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
#
# THE SOURCE-COMMENT CHECK -- the `source_comments` CTest entry.
#
# One question: do the comments under the scoped roots still meet the source comment standard
# (docs/contributing/repository-conventions.md, "Source comment conventions")? Three findings,
# each naming where the text belongs:
#
#   a long block      more than ZEN_COMMENT_BLOCK_LIMIT comment lines in a row -- a law pointer,
#                     a package's law line (`// Workshop law:`) and the SPDX pair not counted --
#                     outside an installed header (the headers cmake/ZengineInstall.cmake's code
#                     installs), which documents a public API and is exempt from this one rule
#   a removal note    a star or "was here": what was removed belongs to Git history
#   a private id      an upper-case token ending in a dash and a number (`XY-12`) whose family is
#                     not a public law family (ZEN_COMMENT_ID_FAMILIES) or a standard's name
#
# POPULATION: every C/C++ source, CMakeLists.txt and *.cmake under ZEN_COMMENT_ROOTS; an empty
# population is a red. A comment line is one whose first characters are `//` (C++) or `#` (CMake,
# outside a quoted argument); a trailing comment is read for the second and third findings. Not
# caught: a multi-line `/* */` comment, a removal note or a phase named in other words.
#
#   cmake -P tests/check_source_comments.cmake              (from the repository root)
#   cmake -DZEN_REPO=<repo> -P tests/check_source_comments.cmake

cmake_minimum_required(VERSION 3.16)

if(NOT DEFINED ZEN_REPO OR ZEN_REPO STREQUAL "")
    set(ZEN_REPO "${CMAKE_CURRENT_LIST_DIR}/..")
endif()
get_filename_component(ZEN_REPO "${ZEN_REPO}" ABSOLUTE)
if(NOT EXISTS "${ZEN_REPO}/AGENTS.md")
    message(FATAL_ERROR "source-comments: '${ZEN_REPO}' is not this repository's root "
                        "(no AGENTS.md). Pass -DZEN_REPO=<repository root>.")
endif()

# ---- scope -----------------------------------------------------------------------------
# A later phase widens the standard's reach by adding a root here.
set(ZEN_COMMENT_ROOTS workshop surface builder introspection timer operator editor-pane)
set(ZEN_COMMENT_GLOBS *.h *.hpp *.ipp *.inl *.c *.cc *.cpp *.cxx CMakeLists.txt *.cmake)
set(ZEN_COMMENT_BLOCK_LIMIT 6)
# The public id families a comment may cite: the law registers' (WL, MW, VM, TIMER) and Loom's
# published law families. A standard's name shaped like an id is not one.
set(ZEN_COMMENT_ID_FAMILIES WL MW VM TIMER ANS GATE HANDOFF KERN LIFE MSG POP PR SENSE)
set(ZEN_COMMENT_NOT_IDS UTF-8 UTF-16 UTF-32 MPL-2 FNV-1a SHA-1 SHA-256 ISO-8601)

# The four characters a CMake list cannot hold, swapped for control characters no source
# carries (VM-CHECK-03), and restored only for printing.
string(ASCII 1 ZEN_SOH)
string(ASCII 2 ZEN_STX)
string(ASCII 3 ZEN_ETX)
string(ASCII 4 ZEN_EOT)
set(ZEN_STAR "⭐")

function(zen_comments_swap text out)
    string(REPLACE "\r" "" text "${text}")
    string(REPLACE ";" "${ZEN_SOH}" text "${text}")
    string(REPLACE "[" "${ZEN_STX}" text "${text}")
    string(REPLACE "]" "${ZEN_ETX}" text "${text}")
    string(REPLACE "\\" "${ZEN_EOT}" text "${text}")
    set(${out} "${text}" PARENT_SCOPE)
endfunction()

function(zen_comments_show text out)
    string(REPLACE "${ZEN_SOH}" ";" text "${text}")
    string(REPLACE "${ZEN_STX}" "[" text "${text}")
    string(REPLACE "${ZEN_ETX}" "]" text "${text}")
    string(REPLACE "${ZEN_EOT}" "\\" text "${text}")
    set(${out} "${text}" PARENT_SCOPE)
endfunction()

# The findings about one comment's text: a removal note, and every private id.
function(zen_comments_judge where comment out)
    set(found "")
    string(TOLOWER "${comment}" lower)
    string(FIND "${comment}" "${ZEN_STAR}" star)
    if(NOT star EQUAL -1 OR lower MATCHES "(was|were) here|used to be here|what used to be")
        list(APPEND found "${where}: a removal note -- what was removed belongs to Git history, not the source")
    endif()
    string(REGEX REPLACE "[^A-Za-z0-9_-]+" ";" words "${comment}")
    foreach(word IN LISTS words)
        if(NOT word MATCHES "^[A-Z][A-Z0-9]*(-[A-Z0-9]+)*-[0-9]+[a-z]?$")
            continue()
        endif()
        if(word IN_LIST ZEN_COMMENT_NOT_IDS)
            continue()
        endif()
        string(REGEX REPLACE "^([A-Z0-9]+)-.*$" "\\1" family "${word}")
        if(NOT family IN_LIST ZEN_COMMENT_ID_FAMILIES)
            list(APPEND found "${where}: `${word}` is a phase name or private id -- say the reason in words -- an open seam belongs in the report's Pressure section and a law in its register")
        endif()
    endforeach()
    set(${out} "${found}" PARENT_SCOPE)
endfunction()

# Every finding in one file's swapped text. `kind` is cxx or cmake; `exempt` is TRUE for an
# installed header, which is spared the block length and nothing else.
function(zen_comments_scan rel kind exempt content out)
    set(findings "")
    string(REPLACE "\n" ";" lines "${content}")
    set(n 0)
    set(block 0)
    set(block_start 0)
    set(in_quote FALSE)
    foreach(line IN LISTS lines)
        math(EXPR n "${n} + 1")
        set(comment "")
        set(whole FALSE)
        if(kind STREQUAL "cmake")
            # Quoted arguments may span lines; a `#` inside one is text, not a comment.
            set(rest "${line}")
            if(in_quote)
                if(rest MATCHES "^([^\"${ZEN_EOT}]|${ZEN_EOT}.)*\"(.*)$")
                    set(rest "${CMAKE_MATCH_2}")
                    set(in_quote FALSE)
                else()
                    set(rest "")
                endif()
            endif()
            if(NOT in_quote)
                string(REGEX REPLACE "\"([^\"${ZEN_EOT}]|${ZEN_EOT}.)*\"" "\"\"" bare "${rest}")
                string(FIND "${bare}" "#" hash)
                string(FIND "${bare}" "\"" open)
                if(NOT hash EQUAL -1 AND (open EQUAL -1 OR hash LESS open))
                    string(SUBSTRING "${bare}" ${hash} -1 comment)
                    string(SUBSTRING "${bare}" 0 ${hash} before)
                    if(before MATCHES "^[ \t]*$" AND rest STREQUAL line)
                        set(whole TRUE)
                    endif()
                elseif(NOT open EQUAL -1)
                    set(in_quote TRUE)
                endif()
            endif()
        else()
            if(line MATCHES "^[ \t]*//")
                set(comment "${line}")
                set(whole TRUE)
            else()
                string(REGEX REPLACE "\"([^\"${ZEN_EOT}]|${ZEN_EOT}.)*\"" "\"\"" bare "${line}")
                string(REGEX REPLACE "'([^'${ZEN_EOT}]|${ZEN_EOT}.)'" "''" bare "${bare}")
                string(FIND "${bare}" "//" slash)
                if(NOT slash EQUAL -1)
                    string(SUBSTRING "${bare}" ${slash} -1 comment)
                endif()
            endif()
        endif()
        if(whole)
            if(block EQUAL 0)
                set(block_start ${n})
            endif()
            if(NOT line MATCHES "^[ \t]*// [A-Z]+-[A-Z]+-[0-9]" AND
               NOT line MATCHES "^[ \t]*(//|#) *([A-Z][A-Za-z]* law:|SPDX-License-Identifier:|Copyright \\(c\\))")
                math(EXPR block "${block} + 1")
            endif()
        else()
            if(block GREATER ZEN_COMMENT_BLOCK_LIMIT AND NOT exempt)
                list(APPEND findings "${rel}:${block_start}: a comment block of ${block} lines (at most ${ZEN_COMMENT_BLOCK_LIMIT}) -- keep what a reader needs in a line and move the reasoning to its owner, a decision record or a reference page, and law to its register")
            endif()
            set(block 0)
        endif()
        if(NOT comment STREQUAL "" AND
           NOT comment MATCHES "^[ \t]*(//|#) *(SPDX-License-Identifier:|Copyright \\(c\\))")
            zen_comments_judge("${rel}:${n}" "${comment}" judged)
            list(APPEND findings ${judged})
        endif()
    endforeach()
    if(block GREATER ZEN_COMMENT_BLOCK_LIMIT AND NOT exempt)
        list(APPEND findings "${rel}:${block_start}: a comment block of ${block} lines at the end of the file")
    endif()
    set(${out} "${findings}" PARENT_SCOPE)
endfunction()

# The headers an install file installs, read from its code alone: its comments name headers it
# deliberately does not ship. A C header (`.h`) is as public as a C++ one.
function(zen_comments_installed text out)
    zen_comments_swap("${text}" text)
    string(REPLACE "\n" ";" lines "${text}")
    set(code "")
    foreach(line IN LISTS lines)
        string(REGEX REPLACE "\"([^\"${ZEN_EOT}]|${ZEN_EOT}.)*\"" "\"\"" bare "${line}")
        string(FIND "${bare}" "#" hash)
        if(NOT hash EQUAL -1)
            string(SUBSTRING "${bare}" 0 ${hash} line)
        endif()
        string(APPEND code " ${line}")
    endforeach()
    string(REGEX MATCHALL "[A-Za-z0-9_/.-]+\\.(hpp|h)" headers "${code}")
    set(${out} "${headers}" PARENT_SCOPE)
endfunction()

# ---- the population ----------------------------------------------------------------------
set(population "")
foreach(root IN LISTS ZEN_COMMENT_ROOTS)
    foreach(glob IN LISTS ZEN_COMMENT_GLOBS)
        file(GLOB_RECURSE found RELATIVE "${ZEN_REPO}" "${ZEN_REPO}/${root}/${glob}")
        list(APPEND population ${found})
    endforeach()
endforeach()
list(REMOVE_DUPLICATES population)
list(SORT population)
list(LENGTH population population_count)

file(READ "${ZEN_REPO}/cmake/ZengineInstall.cmake" install_text)
zen_comments_installed("${install_text}" installed)
list(REMOVE_DUPLICATES installed)
set(installed_here "")
foreach(header IN LISTS installed)
    if(header IN_LIST population)
        list(APPEND installed_here "${header}")
    endif()
endforeach()
list(LENGTH installed_here installed_count)

# ---- the self-test (VM-CHECK-01) --------------------------------------------------------
# Every predicate says NO where it must and YES where it must, before the tree is read.
function(zen_comments_expect label kind text want)
    zen_comments_swap("${text}" swapped)
    zen_comments_scan("self-test" ${kind} FALSE "${swapped}" got)
    list(LENGTH got count)
    if(NOT count EQUAL want)
        zen_comments_show("${got}" shown)
        message(FATAL_ERROR "source-comments: self-test '${label}' found ${count}, want ${want}: ${shown}")
    endif()
endfunction()
set(six "// one\n// two\n// three\n// four\n// five\n// six\n")
zen_comments_expect("a seven-line block" cxx "${six}// seven\nint x;\n" 1)
zen_comments_expect("six lines, a pointer and the law line" cxx
    "// Workshop law: agents/workshop/x.md\n${six}// WL-KEY-15 -- agents/workshop/keyboard.md\nint x;\n" 0)
zen_comments_expect("a removal note" cxx "// ${ZEN_STAR} the old arm WAS HERE\nint x;\n" 1)
zen_comments_expect("a pane that is gone" cxx "// the pane is gone\nint x;\n" 0)
zen_comments_expect("a private id" cxx "// see VD-27 for why\nint x;\n" 1)
zen_comments_expect("public ids and standards" cxx "// WL-KEY-15, ANS-03, UTF-8, button-1\nint x;\n" 0)
zen_comments_expect("an id in a trailing comment" cxx "int x; // P-WORK-22\n" 1)
zen_comments_expect("an id in a literal" cxx "const char* s = \"VD-27\"; // fine\n" 0)
string(REPLACE "//" "#" hashes "${six}")
zen_comments_expect("a CMake block" cmake "${hashes}# seven\nset(x 1)\n" 1)
zen_comments_expect("hashes inside a quoted argument" cmake "set(x \"\n${hashes}# seven\n\")\n" 0)
zen_comments_expect("a package's law line" cxx "// Files law: agents/workshop/files.md\n${six}int x;\n" 0)
zen_comments_installed("# `a/x.hpp` is not shipped\nset(h a/y.hpp # nor a/w.hpp\n    a/z.h)\n" got)
if(NOT got STREQUAL "a/y.hpp;a/z.h")
    message(FATAL_ERROR "source-comments: self-test 'installed headers are read from code' "
                        "found '${got}', want 'a/y.hpp;a/z.h'")
endif()

list(GET ZEN_COMMENT_ROOTS 0 first_root)
if(population_count EQUAL 0)
    message(FATAL_ERROR "source-comments: the population is empty -- no source under ${ZEN_COMMENT_ROOTS}")
endif()
if(installed_count EQUAL 0)
    message(FATAL_ERROR "source-comments: no installed header read from cmake/ZengineInstall.cmake")
endif()
# A pointer line read from the tree now is one the block count skips.
set(pointer "")
foreach(rel IN LISTS population)
    file(STRINGS "${ZEN_REPO}/${rel}" hit REGEX "^[ \t]*// [A-Z]+-[A-Z]+-[0-9]" LIMIT_COUNT 1)
    if(hit)
        set(pointer "${hit}")
        break()
    endif()
endforeach()
if(pointer STREQUAL "")
    message(FATAL_ERROR "source-comments: no law pointer found under ${first_root} to self-test against")
endif()
zen_comments_expect("a pointer read from the tree" cxx "${six}${pointer}\nint x;\n" 0)

# ---- the tree --------------------------------------------------------------------------
set(findings "")
set(lines_read 0)
foreach(rel IN LISTS population)
    file(READ "${ZEN_REPO}/${rel}" content)
    zen_comments_swap("${content}" content)
    set(kind cxx)
    if(rel MATCHES "(CMakeLists\\.txt|\\.cmake)$")
        set(kind cmake)
    endif()
    set(exempt FALSE)
    if(rel IN_LIST installed_here)
        set(exempt TRUE)
    endif()
    zen_comments_scan("${rel}" ${kind} ${exempt} "${content}" found)
    list(APPEND findings ${found})
    string(REGEX MATCHALL "\n" breaks "${content}")
    list(LENGTH breaks count)
    math(EXPR lines_read "${lines_read} + ${count}")
endforeach()

list(LENGTH findings finding_count)
message(STATUS "source-comments: ${population_count} files under ${ZEN_COMMENT_ROOTS} (C/C++ and "
               "CMake), ${lines_read} lines read; ${installed_count} installed headers exempt from "
               "the ${ZEN_COMMENT_BLOCK_LIMIT}-line block rule; self-test passed")
if(finding_count GREATER 0)
    zen_comments_show("${findings}" shown)
    string(REPLACE ";" "\n  " shown "${shown}")
    message(FATAL_ERROR "source-comments: ${finding_count} findings:\n  ${shown}")
endif()
message(STATUS "source-comments: PASSED -- no long block, removal note or private id")
