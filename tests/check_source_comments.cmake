# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
#
# The `source_comments` entry: do the comments under the roots below meet the source comment
# standard (docs/contributing/repository-conventions.md)? A long block, a removal note or a private
# id is a red that names where the text belongs; an empty population is a red. It cannot see a
# multi-line `/* */` comment, or history and phase names put in other words.
#   cmake -P tests/check_source_comments.cmake    (from the repository root, or -DZEN_REPO=<repo>)

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
# The roots held: a directory is read whole, a file alone; a new package adds its root. Vendored
# code is never held, nor a golden a suite compares byte for byte, whose comments are the
# generator's text for a maker. A pending file is not held yet, and is struck from that list as
# it meets the standard: the list only shrinks.
set(ZEN_COMMENT_ROOTS
    CMakeLists.txt cmake examples tests workshop
    activation attention-pane builder builder-pane component composer connections-pane
    demo-control desktop-pane editor-pane external-host files flow flow-host flow-pane info-pane
    input introspection inventory inventory-pane maker menu-presenter message-draft neovim
    neovim-editor operator smoke snake source-transfer surface terminal-pane timer ui)
set(ZEN_COMMENT_EXCLUDED tests/third_party/ tests/source_transfer_ensure_timer.generated.hpp
    tests/source_transfer_string_bytes.generated.hpp)
set(ZEN_COMMENT_PENDING
    tests/doctest_main.cpp tests/editor_transfer_story.hpp tests/flow_fixture.hpp
    tests/flow_generate.cpp tests/guest_journey.cpp tests/inventory_story.hpp
    tests/launch_fixture_driver.cpp tests/launch_fixture_host.cpp tests/lifecycle_door.hpp
    tests/maker_author.cpp tests/maker_fixture.hpp tests/neovim_environment.hpp
    tests/neovim_fixture.cpp tests/operator_fixture.hpp tests/operator_stranger.cpp
    tests/operator_stranger.hpp tests/source_transfer_cpp_witness.cpp
    tests/source_transfer_samples.hpp tests/test_audit_probes.cpp tests/test_component.cpp
    tests/test_composer.cpp tests/test_editor.cpp tests/test_files.cpp tests/test_flow.cpp
    tests/test_flow_graph.cpp tests/test_flow_pane.cpp tests/test_flow_runtime.cpp
    tests/test_flow_view.cpp tests/test_guest_vocabulary.cpp tests/test_inventory.cpp
    tests/test_maker.cpp tests/test_message_draft.cpp tests/test_neovim.cpp
    tests/test_neovim_live.cpp tests/test_operator.cpp tests/test_operator_canonical.cpp
    tests/test_operator_host.cpp tests/test_operator_migration.cpp tests/test_operator_provider.cpp
    tests/test_operator_source.cpp tests/test_snake.cpp tests/test_source_transfer.cpp
    tests/test_ui.cpp tests/workshop_switch_rig.hpp tests/test_workshop_panes_actions.cpp
    tests/test_workshop_panes_attention.cpp tests/test_workshop_panes_builder.cpp
    tests/test_workshop_panes_button.cpp tests/test_workshop_panes_canvas.cpp
    tests/test_workshop_panes_code.cpp tests/test_workshop_panes_desktop.cpp
    tests/test_workshop_panes_editor.cpp tests/test_workshop_panes_files.cpp
    tests/test_workshop_panes_info.cpp tests/test_workshop_panes_input.cpp
    tests/test_workshop_panes_introspection.cpp tests/test_workshop_panes_opening.cpp
    tests/test_workshop_panes_output.cpp tests/test_workshop_panes_sampling.cpp
    tests/test_workshop_panes_seam.cpp tests/test_workshop_panes_terminal.cpp
    tests/test_workshop_panes_window.cpp tests/test_workshop_demo.cpp
    tests/test_workshop_document.cpp tests/test_workshop_editor_switch.cpp
    tests/test_workshop_editor_transfers.cpp tests/test_workshop_files.cpp
    tests/test_workshop_guests.cpp tests/test_workshop_info_views.cpp
    tests/test_workshop_inventory_folders.cpp tests/test_workshop_inventory_info.cpp
    tests/test_workshop_load.cpp tests/test_workshop_neovim.cpp
    tests/test_workshop_neovim_transfers.cpp tests/test_workshop_panels.cpp
    tests/test_workshop_panels_creator.cpp tests/test_workshop_persistence.cpp
    tests/test_workshop_probe.cpp tests/test_workshop_screen.cpp)
set(ZEN_COMMENT_GLOBS *.h *.hpp *.ipp *.inl *.c *.cc *.cpp *.cxx CMakeLists.txt *.cmake
    *.cmake.in test_population.txt)
# A long block is more comment lines in a row than this, a law pointer, a package's law line
# (`// Workshop law:`) and the SPDX pair not counted, outside an installed header: one that
# cmake/ZengineInstall.cmake's code installs documents a public API and is spared this rule alone.
set(ZEN_COMMENT_BLOCK_LIMIT 6)
# A private id is an upper-case token ending in a dash and a number, outside the families a
# comment may cite: the law registers' (WL, MW, VM, TIMER) and Loom's published law families,
# whose numbers have two digits: a family's letters with a one-digit number are a phase tag. A
# standard's name shaped like an id is not one.
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

# The findings about one comment's text: a removal note (a star, or a note that something stood
# here once: what was removed is Git's to keep), and every private id.
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
        if(NOT family IN_LIST ZEN_COMMENT_ID_FAMILIES OR word MATCHES "-[0-9][a-z]?$")
            list(APPEND found "${where}: `${word}` is a phase name or private id -- say the reason in words -- an open seam belongs in the report's Pressure section and a law in its register")
        endif()
    endforeach()
    set(${out} "${found}" PARENT_SCOPE)
endfunction()

# Every finding in one file's swapped text. `kind` is cxx, cmake or manifest; `exempt` is TRUE for
# an installed header. A comment line starts with `//`, or with `#` outside a quoted CMake argument;
# a trailing comment is judged for the removal note and the private id.
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
                # A closed argument leaves no quote behind: a `"` still found opens one.
                string(REGEX REPLACE "\"([^\"${ZEN_EOT}]|${ZEN_EOT}.)*\"" "__" bare "${rest}")
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
        elseif(kind STREQUAL "manifest")
            # tests/check_population.cmake strips `#.*$` from every line: no quote protects a `#`.
            string(FIND "${line}" "#" hash)
            if(NOT hash EQUAL -1)
                string(SUBSTRING "${line}" ${hash} -1 comment)
                string(SUBSTRING "${line}" 0 ${hash} before)
                if(before MATCHES "^[ \t]*$")
                    set(whole TRUE)
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

# Whether a file under a root is held: neither under an excluded prefix nor pending.
function(zen_comments_held rel out)
    set(held TRUE)
    foreach(prefix IN LISTS ZEN_COMMENT_EXCLUDED)
        string(FIND "${rel}" "${prefix}" at)
        if(at EQUAL 0)
            set(held FALSE)
        endif()
    endforeach()
    if(rel IN_LIST ZEN_COMMENT_PENDING)
        set(held FALSE)
    endif()
    set(${out} ${held} PARENT_SCOPE)
endfunction()

function(zen_comments_kind rel out)
    set(kind cxx)
    if(rel MATCHES "(^|/)(CMakeLists\\.txt|[^/]*\\.cmake|[^/]*\\.cmake\\.in)$")
        set(kind cmake)
    elseif(rel MATCHES "(^|/)test_population\\.txt$")
        set(kind manifest)
    endif()
    set(${out} ${kind} PARENT_SCOPE)
endfunction()

# ---- the population ----------------------------------------------------------------------
# The globs as one name pattern, so each root is walked once: a walk per glob reads every
# directory once per glob, which a 9p mount charges for.
set(ZEN_COMMENT_NAMES "")
set(separator "")
foreach(glob IN LISTS ZEN_COMMENT_GLOBS)
    string(REPLACE "." "\\." name "${glob}")
    string(REPLACE "*" "[^/]*" name "${name}")
    string(APPEND ZEN_COMMENT_NAMES "${separator}${name}")
    set(separator "|")
endforeach()
set(ZEN_COMMENT_NAMES "(^|/)(${ZEN_COMMENT_NAMES})$")
set(found_under_roots "")
foreach(root IN LISTS ZEN_COMMENT_ROOTS)
    if(IS_DIRECTORY "${ZEN_REPO}/${root}")
        file(GLOB_RECURSE found RELATIVE "${ZEN_REPO}" "${ZEN_REPO}/${root}/*")
        list(FILTER found INCLUDE REGEX "${ZEN_COMMENT_NAMES}")
        list(APPEND found_under_roots ${found})
    elseif(EXISTS "${ZEN_REPO}/${root}")
        list(APPEND found_under_roots "${root}")
    else()
        message(FATAL_ERROR "source-comments: the root '${root}' names nothing in ${ZEN_REPO}")
    endif()
endforeach()
list(REMOVE_DUPLICATES found_under_roots)
list(SORT found_under_roots)
set(population "")
foreach(rel IN LISTS found_under_roots)
    zen_comments_held("${rel}" held)
    if(held)
        list(APPEND population "${rel}")
    endif()
endforeach()
list(LENGTH population population_count)
list(LENGTH ZEN_COMMENT_PENDING pending_count)

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
zen_comments_expect("a phase tag in a law family's letters" cxx "// the MSG-0 reading\nint x;\n" 1)
zen_comments_expect("an id in a trailing comment" cxx "int x; // P-WORK-22\n" 1)
zen_comments_expect("an id in a literal" cxx "const char* s = \"VD-27\"; // fine\n" 0)
string(REPLACE "//" "#" hashes "${six}")
zen_comments_expect("a CMake block" cmake "${hashes}# seven\nset(x 1)\n" 1)
zen_comments_expect("hashes inside a quoted argument" cmake "set(x \"\n${hashes}# seven\n\")\n" 0)
zen_comments_expect("a block after a closed argument" cmake "set(x \"a\")\n${hashes}# seven\nset(y 1)\n" 1)
zen_comments_expect("a package's law line" cxx "// Files law: agents/workshop/files.md\n${six}int x;\n" 0)
zen_comments_expect("a manifest block" manifest "${hashes}# seven\nui doctest always 22\n" 1)
zen_comments_expect("a quote protects no manifest comment" manifest
    "x compile-negative always \"a # VD-27\"\n" 1)
zen_comments_installed("# `a/x.hpp` is not shipped\nset(h a/y.hpp # nor a/w.hpp\n    a/z.h)\n" got)
if(NOT got STREQUAL "a/y.hpp;a/z.h")
    message(FATAL_ERROR "source-comments: self-test 'installed headers are read from code' "
                        "found '${got}', want 'a/y.hpp;a/z.h'")
endif()
function(zen_comments_expect_path rel want_held want_kind)
    set(ZEN_COMMENT_EXCLUDED a/vendored/)
    set(ZEN_COMMENT_PENDING a/pending.cpp)
    zen_comments_held("${rel}" held)
    zen_comments_kind("${rel}" kind)
    if(NOT held STREQUAL want_held OR NOT kind STREQUAL want_kind)
        message(FATAL_ERROR "source-comments: self-test '${rel}' is held ${held} as ${kind}, "
                            "want ${want_held} as ${want_kind}")
    endif()
endfunction()
zen_comments_expect_path(a/pending.cpp FALSE cxx)
zen_comments_expect_path(a/vendored/x.h FALSE cxx)
zen_comments_expect_path(b/a/vendored/x.h TRUE cxx)
zen_comments_expect_path(CMakeLists.txt TRUE cmake)
zen_comments_expect_path(cmake/zengineConfig.cmake.in TRUE cmake)
zen_comments_expect_path(tests/test_population.txt TRUE manifest)
function(zen_comments_expect_name rel want)
    set(named FALSE)
    if(rel MATCHES "${ZEN_COMMENT_NAMES}")
        set(named TRUE)
    endif()
    if(NOT named STREQUAL want)
        message(FATAL_ERROR "source-comments: self-test name '${rel}' is ${named}, want ${want}")
    endif()
endfunction()
zen_comments_expect_name(a/b/x.hpp TRUE)
zen_comments_expect_name(a/b/x.cmake.in TRUE)
zen_comments_expect_name(a/test_population.txt TRUE)
zen_comments_expect_name(a/b/x_hpp FALSE)
zen_comments_expect_name(a/b/x.hpp.orig FALSE)
zen_comments_expect_name(a/b/notes.txt FALSE)

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
    message(FATAL_ERROR "source-comments: no law pointer found in the population to self-test against")
endif()
zen_comments_expect("a pointer read from the tree" cxx "${six}${pointer}\nint x;\n" 0)

# ---- the tree --------------------------------------------------------------------------
set(findings "")
foreach(rel IN LISTS ZEN_COMMENT_PENDING)
    if(NOT EXISTS "${ZEN_REPO}/${rel}")
        list(APPEND findings "${rel}: pending, but no such file -- strike it from ZEN_COMMENT_PENDING")
    endif()
endforeach()
set(lines_read 0)
foreach(rel IN LISTS population)
    file(READ "${ZEN_REPO}/${rel}" content)
    zen_comments_swap("${content}" content)
    zen_comments_kind("${rel}" kind)
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
message(STATUS "source-comments: ${population_count} files held under ${ZEN_COMMENT_ROOTS} (C/C++, "
               "CMake and the population manifest), ${pending_count} pending, ${lines_read} lines "
               "read; ${installed_count} installed headers exempt from the "
               "${ZEN_COMMENT_BLOCK_LIMIT}-line block rule; self-test passed")
if(finding_count GREATER 0)
    zen_comments_show("${findings}" shown)
    string(REPLACE ";" "\n  " shown "${shown}")
    message(FATAL_ERROR "source-comments: ${finding_count} findings:\n  ${shown}")
endif()
message(STATUS "source-comments: PASSED -- no long block, removal note or private id")
