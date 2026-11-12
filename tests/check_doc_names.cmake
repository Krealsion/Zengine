# SPDX-License-Identifier: AGPL-3.0-only
# Copyright (c) 2026 Joshua DeMoss
#
# The `doc_names` entry (docs/contributing/build-and-test.md): does each name a router or a
# much-read page gives in backticks still name something -- a file or folder of this tree, or, where
# a router names a suite, a CTest entry? doc_links holds the Markdown links and law_register the law
# ids and the cases; a name in backticks is this check's.
#   cmake -P tests/check_doc_names.cmake    (from the repository root, or -DZEN_REPO=<repo>)

cmake_minimum_required(VERSION 3.16)

if(NOT DEFINED ZEN_REPO OR ZEN_REPO STREQUAL "")
    set(ZEN_REPO "${CMAKE_CURRENT_LIST_DIR}/..")
endif()
get_filename_component(ZEN_REPO "${ZEN_REPO}" ABSOLUTE)
if(NOT EXISTS "${ZEN_REPO}/AGENTS.md")
    message(FATAL_ERROR "doc-names: '${ZEN_REPO}' is not this repository's root "
                        "(no AGENTS.md). Pass -DZEN_REPO=<repository root>.")
endif()

# ---- scope ------------------------------------------------------------------------------------
# The pages read: every router, the registers and their routers under agents/, and the pages a
# contributor is sent to. A decision record tells what was tried and names what retired, and a
# guide names the files its reader makes, so neither is read.
set(ZEN_NAMES_PAGES "(^|/)AGENTS[.]md$" "^agents/" "^docs/contributing/" "^docs/README[.]md$"
    "^CONTRIBUTING[.]md$")
set(ZEN_NAMES_NOT_PAGES "^agents/decisions/")
# A name is read as a path when it ends in `/` or in one of these, opens with no `/`, and holds
# nothing but a path's characters, `*` and a `{a,b}` choice; anything else in backticks is code,
# and no name.
set(ZEN_NAMES_EXTENSIONS md hpp h ipp cpp c cc cxx cmake txt json py sh cmd yml yaml xml in png svg
    ttf otf)
# The folders whose files a name may be the tail of: every row of tests/folders.txt but the quarry,
# whose files are named from the root or not at all.
set(ZEN_NAMES_NOT_TAILS quarry)
# The section of the root router whose bullets name suites, as folder_map reads it.
set(ZEN_NAMES_ROOT_SECTION "Read this, then route")
# The kinds a row of tests/doc_names.txt may give a name the tree does not hold.
set(ZEN_NAMES_KINDS written generated installed loom)

string(ASCII 2 ZEN_NAMES_STX)   # [
string(ASCII 3 ZEN_NAMES_ETX)   # ]
string(ASCII 4 ZEN_NAMES_EOT)   # ;
string(REPLACE ";" "|" ZEN_NAMES_EXT_RX "${ZEN_NAMES_EXTENSIONS}")
set(ZEN_NAMES_PATH_RX "^[A-Za-z0-9_.*{][A-Za-z0-9_.+*{},/-]*(/|[.](${ZEN_NAMES_EXT_RX}))$")

# ---- the readings, one place each so the self-test exercises the real ones --------------------

# Markdown as one string no character of which reshapes a CMake list (VM-CHECK-03): `\` dropped,
# `[` and `]` swapped, and `;` kept as a control character, since it ends a witness list.
function(zen_names_swap text out)
    string(REPLACE "\r" "" text "${text}")
    string(REPLACE "\\" "" text "${text}")
    string(REPLACE ";" "${ZEN_NAMES_EOT}" text "${text}")
    string(REPLACE "[" "${ZEN_NAMES_STX}" text "${text}")
    string(REPLACE "]" "${ZEN_NAMES_ETX}" text "${text}")
    set(${out} "${text}" PARENT_SCOPE)
endfunction()

# Every name a `{a,b}` choice stands for.
function(zen_names_choices name out)
    set(work "${name}")
    set(found "")
    while(work)
        list(GET work 0 item)
        list(REMOVE_AT work 0)
        if(item MATCHES "^([^{}]*)[{]([^{}]*)[}](.*)$")
            set(before "${CMAKE_MATCH_1}")
            set(body "${CMAKE_MATCH_2}")
            set(after "${CMAKE_MATCH_3}")
            string(REPLACE "," ";" parts "${body}")
            foreach(part IN LISTS parts)
                list(APPEND work "${before}${part}${after}")
            endforeach()
        else()
            list(APPEND found "${item}")
        endif()
    endwhile()
    set(${out} "${found}" PARENT_SCOPE)
endfunction()

# Is `tail` the end of a line of the tree's list (ZEN_NAMES_TREE: one `/path` a line), after
# `lead`? A folder's name ends in `/` and is found as the start of what follows it; a `*` stands
# for any run of a file name's characters.
function(zen_names_in_tree lead tail out)
    set(end "\n")
    if(tail MATCHES "/$")
        set(end "")
    endif()
    if(tail MATCHES "[*]")
        string(REPLACE "." "[.]" rx "${lead}${tail}")
        string(REPLACE "+" "[+]" rx "${rx}")
        string(REPLACE "*" "[^/\n]*" rx "${rx}")
        string(REGEX MATCH "${rx}${end}" hit "${ZEN_NAMES_TREE}")
        if(hit STREQUAL "")
            set(${out} FALSE PARENT_SCOPE)
        else()
            set(${out} TRUE PARENT_SCOPE)
        endif()
        return()
    endif()
    string(FIND "${ZEN_NAMES_TREE}" "${lead}${tail}${end}" at)
    if(at EQUAL -1)
        set(${out} FALSE PARENT_SCOPE)
    else()
        set(${out} TRUE PARENT_SCOPE)
    endif()
endfunction()

# Does a path name something? Read from the root, then from the page's folder `dir`; a name that
# opens with no top-level folder is also the tail of a path in the tree (`board.hpp`,
# `snake/world.cpp`), and one opening with `_` or `*` the tail of a file's name (`_desktop.cpp`
# after `tests/test_workshop_panes_button.cpp`). `./` and `../` are read from `dir` alone, and a
# name that climbs above the root is another repository's, not asked. Each name of a `{a,b}`
# choice is asked. `out` is empty when it resolves, else the first name that does not.
function(zen_names_resolve name dir out)
    set(${out} "" PARENT_SCOPE)
    zen_names_choices("${name}" choices)
    foreach(one IN LISTS choices)
        set(anchored FALSE)
        if(one MATCHES "^[.][.]?/")
            get_filename_component(abs "${dir}/${one}" ABSOLUTE BASE_DIR "/zen-names-root")
            if(NOT abs MATCHES "^/zen-names-root/(.+)$")
                continue()
            endif()
            set(rest "${CMAKE_MATCH_1}")
            if(one MATCHES "/$")
                string(APPEND rest "/")
            endif()
            set(one "${rest}")
            set(anchored TRUE)
        endif()
        zen_names_in_tree("\n/" "${one}" hit)
        if(NOT hit AND NOT anchored AND NOT dir STREQUAL "")
            zen_names_in_tree("\n/${dir}/" "${one}" hit)
        endif()
        if(NOT hit AND NOT anchored)
            string(REGEX REPLACE "/.*$" "" first "${one}")
            string(REGEX REPLACE "/$" "" inner "${one}")
            if(one MATCHES "^[_*]")
                zen_names_in_tree("" "${one}" hit)
            elseif(NOT inner MATCHES "/" OR NOT first IN_LIST ZEN_NAMES_TOP)
                zen_names_in_tree("/" "${one}" hit)
            endif()
        endif()
        if(NOT hit)
            set(${out} "${one}" PARENT_SCOPE)
            return()
        endif()
    endforeach()
endfunction()

# Is an unresolved name one tests/doc_names.txt holds: a row of that name, or a row ending in `/`
# that the name opens with? The row is marked used.
function(zen_names_excepted name out)
    set(${out} FALSE PARENT_SCOPE)
    foreach(row IN LISTS ZEN_NAMES_ROWS)
        string(LENGTH "${row}" len)
        string(LENGTH "${name}" name_len)
        set(held FALSE)
        if(row STREQUAL name)
            set(held TRUE)
        elseif(row MATCHES "/$" AND name_len GREATER len)
            string(SUBSTRING "${name}" 0 ${len} head)
            if(head STREQUAL row)
                set(held TRUE)
            endif()
        endif()
        if(held)
            set_property(GLOBAL APPEND PROPERTY ZEN_NAMES_USED "${row}")
            set(${out} TRUE PARENT_SCOPE)
            return()
        endif()
    endforeach()
endfunction()

# The suites a line names outside its parentheses: each backticked lowercase identifier. A
# parenthesis left open is cut, and `open` says so for the line after.
function(zen_names_suites text open out out_open)
    if(open)
        string(FIND "${text}" ")" at)
        if(at EQUAL -1)
            set(${out} "" PARENT_SCOPE)
            set(${out_open} TRUE PARENT_SCOPE)
            return()
        endif()
        math(EXPR at "${at} + 1")
        string(SUBSTRING "${text}" ${at} -1 text)
    endif()
    string(REGEX REPLACE "[(][^()]*[)]" "" text "${text}")
    string(REGEX REPLACE "[(][^()]*[)]" "" text "${text}")
    set(still FALSE)
    string(FIND "${text}" "(" at)
    if(NOT at EQUAL -1)
        string(SUBSTRING "${text}" 0 ${at} text)
        set(still TRUE)
    endif()
    string(REGEX MATCHALL "`[a-z][a-z0-9_]*`" spans "${text}")
    string(REPLACE "`" "" spans "${spans}")
    set(${out} "${spans}" PARENT_SCOPE)
    set(${out_open} ${still} PARENT_SCOPE)
endfunction()

# One page, read: each problem, `<page>:<line>: `<name>` <what>`, appended to the global
# ZEN_NAMES_PROBLEMS, the names read counted in ZEN_NAMES_READ. A fenced block is code and is not
# read; a span left open at a line's end closes on the next. A router (`router` names the section
# whose bullets name suites) names a suite at the head of such a bullet, before its dash, and in
# `## Law` after `witnessed by`, to the item's end or its first `;`, or in the last cell of a
# table whose last heading says so.
function(zen_names_page label dir router text)
    string(REPLACE "\n" ";" lines "${text}")
    set(n 0)
    set(read 0)
    set(fence FALSE)
    set(carry FALSE)
    set(section "")
    set(witness FALSE)
    set(paren FALSE)
    set(witness_table FALSE)
    foreach(line IN LISTS lines)
        math(EXPR n "${n} + 1")
        if(line MATCHES "^[ \t]*```")
            if(fence)
                set(fence FALSE)
            else()
                set(fence TRUE)
            endif()
            set(carry FALSE)
            continue()
        endif()
        if(fence)
            continue()
        endif()
        if(line MATCHES "^## (.*)$")
            string(STRIP "${CMAKE_MATCH_1}" section)
            set(witness FALSE)
            set(witness_table FALSE)
        endif()
        if(line MATCHES "^[ \t]*$")
            set(carry FALSE)
            set(witness FALSE)
            set(paren FALSE)
            set(witness_table FALSE)
            continue()
        endif()

        # ---- suites, where a router names them ------------------------------------------------
        set(suites "")
        if(NOT router STREQUAL "")
            if(section STREQUAL router AND line MATCHES "^- `")
                string(REGEX REPLACE " (—|--) .*$" "" head "${line}")
                zen_names_suites("${head}" FALSE suites ignored)
            elseif(section STREQUAL "Law")
                if(line MATCHES "^[|]")
                    set(witness FALSE)
                    if(line MATCHES "witnessed by [|][ \t]*$")
                        set(witness_table TRUE)
                    elseif(witness_table AND NOT line MATCHES "^[|][-| :]+$")
                        string(REGEX REPLACE "[ \t]*[|][ \t]*$" "" cell "${line}")
                        string(REGEX REPLACE "^.*[|]" "" cell "${cell}")
                        zen_names_suites("${cell}" FALSE suites ignored)
                    endif()
                else()
                    if(line MATCHES "^(- |#)")
                        set(witness FALSE)
                        set(paren FALSE)
                    endif()
                    set(span "")
                    if(witness)
                        set(span "${line}")
                    else()
                        string(FIND "${line}" "witnessed by" at)
                        if(NOT at EQUAL -1)
                            math(EXPR at "${at} + 12")
                            string(SUBSTRING "${line}" ${at} -1 span)
                            set(witness TRUE)
                            set(paren FALSE)
                            if(span STREQUAL "")
                                set(span " ")
                            endif()
                        endif()
                    endif()
                    if(NOT span STREQUAL "")
                        zen_names_suites("${span}" ${paren} suites paren_after)
                        # a `;` outside a parenthesis ends the list; one inside it is the note's
                        set(flat "${span}")
                        if(paren)
                            string(FIND "${flat}" ")" at)
                            if(at EQUAL -1)
                                set(flat "")
                            else()
                                string(SUBSTRING "${flat}" ${at} -1 flat)
                            endif()
                        endif()
                        string(REGEX REPLACE "[(][^()]*[)]" "" flat "${flat}")
                        string(REGEX REPLACE "[(].*$" "" flat "${flat}")
                        string(FIND "${flat}" "${ZEN_NAMES_EOT}" at)
                        if(NOT at EQUAL -1)
                            string(SUBSTRING "${flat}" 0 ${at} flat)
                            zen_names_suites("${flat}" FALSE suites ignored)
                            set(witness FALSE)
                            set(paren_after FALSE)
                        endif()
                        set(paren ${paren_after})
                    endif()
                endif()
            endif()
        endif()
        foreach(suite IN LISTS suites)
            math(EXPR read "${read} + 1")
            if(NOT suite IN_LIST ZEN_NAMES_POPULATION)
                set_property(GLOBAL APPEND PROPERTY ZEN_NAMES_PROBLEMS
                    "${label}:${n}: `${suite}` is named as a suite, and tests/test_population.txt declares no such CTest entry")
            endif()
        endforeach()

        # ---- paths ----------------------------------------------------------------------------
        if(NOT line MATCHES "`")
            continue()
        endif()
        set(rest "${line}")
        if(carry)
            string(FIND "${rest}" "`" at)
            math(EXPR at "${at} + 1")
            string(SUBSTRING "${rest}" ${at} -1 rest)
            set(carry FALSE)
        endif()
        string(REGEX MATCHALL "`[^`]+`" spans "${rest}")
        string(REGEX REPLACE "`[^`]+`" "" left "${rest}")
        if(left MATCHES "`")
            set(carry TRUE)
        endif()
        foreach(span IN LISTS spans)
            string(REPLACE "`" "" name "${span}")
            if(NOT name MATCHES "${ZEN_NAMES_PATH_RX}" OR name MATCHES "[.][.][.]")
                continue()
            endif()
            math(EXPR read "${read} + 1")
            zen_names_resolve("${name}" "${dir}" missing)
            if(missing STREQUAL "")
                continue()
            endif()
            zen_names_excepted("${name}" excepted)
            if(excepted)
                continue()
            endif()
            set(what "names no file or folder of this tree")
            if(NOT missing STREQUAL name)
                set(what "names no file or folder of this tree (${missing})")
            endif()
            set_property(GLOBAL APPEND PROPERTY ZEN_NAMES_PROBLEMS "${label}:${n}: `${name}` ${what}")
        endforeach()
    endforeach()
    get_property(total GLOBAL PROPERTY ZEN_NAMES_READ)
    math(EXPR total "0${total} + ${read}")
    set_property(GLOBAL PROPERTY ZEN_NAMES_READ ${total})
endfunction()

# ---- the self-test (VM-CHECK-01): a canary page over a canary tree ----------------------------
# Each line of the canary holds names that resolve beside names that do not, and the reading must
# name exactly the second kind, by page, line and name, before the tree is asked anything.
set(ZEN_NAMES_TREE "\n/tests/kept.cpp\n/tests/test_panes_button.cpp\n/tests/test_panes_code.cpp\n/part/pane.cpp\n/part/snake/board.hpp\n/agents/law.md\n/quarry-free.txt\n")
set(ZEN_NAMES_TOP tests part agents)
set(ZEN_NAMES_POPULATION kept_suite other_suite)
set(ZEN_NAMES_ROWS "made.json" "include/zengine/")
string(CONCAT st_page
    "# canary\n"                                                                              # 1
    "\n"
    "`tests/kept.cpp`, `tests/gone.cpp`, `pane.cpp`, `board.hpp`, `snake/board.hpp`, a `word`\n"  # 3
    "`tests/test_panes_button.cpp`, `_code.cpp`, `_gone.cpp`, `tests/test_panes_{button,code}.cpp`\n"  # 4
    "`tests/test_panes_{button,lost}.cpp`, `tests/test_*.cpp`, `tests/lost_*.cpp`, `snake/`, `lost/`\n"  # 5
    "`../agents/law.md`, `../agents/lost.md`, `../../other/page.md`, `made.json`, `stray.json`\n"  # 6
    "`include/zengine/pane/`, `tests/board.hpp`, `-DZEN=tests/x.cpp`, `a(b).cpp`, a span `left\n"  # 7
    "open.md` then `part/pane.cpp`\n"                                                           # 8
    "```\n"
    "`tests/fenced.cpp`\n"                                                                      # 10
    "```\n"
    "\n"
    "## Law\n"                                                                                # 13
    "\n"
    "- (r)(r.md) -- words; witnessed by `kept_suite` (a part: `o`; and\n"                       # 15
    "  `free`), then `lost_suite`; the `open` power\n"                                         # 16
    "\n"
    "| register | witnessed by |\n"
    "|---|---|\n"
    "| (r)(r.md) | `kept_suite` (`note`), `gone_suite` |\n"                                   # 20
    "\n"
    "## Suites\n"                                                                             # 22
    "\n"
    "- `kept_suite`, `absent_suite` -- what `after_dash` is\n"                                 # 24
    "A `prose_word` names no suite.\n")
zen_names_swap("${st_page}" st_text)
set_property(GLOBAL PROPERTY ZEN_NAMES_PROBLEMS "")
set_property(GLOBAL PROPERTY ZEN_NAMES_USED "")
set_property(GLOBAL PROPERTY ZEN_NAMES_READ 0)
zen_names_page("canary/AGENTS.md" "part" "Suites" "${st_text}")
get_property(st_problems GLOBAL PROPERTY ZEN_NAMES_PROBLEMS)
set(st_want
    "canary/AGENTS.md:3: `tests/gone.cpp` names no file or folder of this tree"
    "canary/AGENTS.md:4: `_gone.cpp` names no file or folder of this tree"
    "canary/AGENTS.md:5: `tests/test_panes_{button,lost}.cpp` names no file or folder of this tree (tests/test_panes_lost.cpp)"
    "canary/AGENTS.md:5: `tests/lost_*.cpp` names no file or folder of this tree"
    "canary/AGENTS.md:5: `lost/` names no file or folder of this tree"
    "canary/AGENTS.md:6: `../agents/lost.md` names no file or folder of this tree (agents/lost.md)"
    "canary/AGENTS.md:6: `stray.json` names no file or folder of this tree"
    "canary/AGENTS.md:7: `tests/board.hpp` names no file or folder of this tree"
    "canary/AGENTS.md:16: `lost_suite` is named as a suite, and tests/test_population.txt declares no such CTest entry"
    "canary/AGENTS.md:20: `gone_suite` is named as a suite, and tests/test_population.txt declares no such CTest entry"
    "canary/AGENTS.md:24: `absent_suite` is named as a suite, and tests/test_population.txt declares no such CTest entry")
if(NOT st_problems STREQUAL st_want)
    string(REPLACE ";" "\n  " st_shown "${st_problems}")
    string(REPLACE ";" "\n  " st_wanted "${st_want}")
    message(FATAL_ERROR "doc-names: SELF-TEST FAILED -- the canary page read as\n  ${st_shown}\nwant\n  ${st_wanted}")
endif()
get_property(st_used GLOBAL PROPERTY ZEN_NAMES_USED)
if(NOT st_used STREQUAL "made.json;include/zengine/")
    message(FATAL_ERROR "doc-names: SELF-TEST FAILED -- the canary used the rows '${st_used}', "
                        "want 'made.json;include/zengine/'")
endif()
# The same page read as no router names no suite.
set_property(GLOBAL PROPERTY ZEN_NAMES_PROBLEMS "")
zen_names_page("canary/page.md" "part" "" "${st_text}")
get_property(st_problems GLOBAL PROPERTY ZEN_NAMES_PROBLEMS)
if(st_problems MATCHES "suite")
    message(FATAL_ERROR "doc-names: SELF-TEST FAILED -- a page that is no router was read for suites")
endif()
set_property(GLOBAL PROPERTY ZEN_NAMES_PROBLEMS "")
set_property(GLOBAL PROPERTY ZEN_NAMES_USED "")
set_property(GLOBAL PROPERTY ZEN_NAMES_READ 0)

# ---- the tree ---------------------------------------------------------------------------------
file(STRINGS "${ZEN_REPO}/tests/folders.txt" folder_rows REGEX "^[^#]")
set(ZEN_NAMES_TOP "")
foreach(row IN LISTS folder_rows)
    if(row MATCHES "^([^ \t]+)[ \t]+[a-z]+[ \t]*$")
        list(APPEND ZEN_NAMES_TOP "${CMAKE_MATCH_1}")
    endif()
endforeach()
set(tree_files "")
set(page_files "")
foreach(folder IN LISTS ZEN_NAMES_TOP)
    if(folder IN_LIST ZEN_NAMES_NOT_TAILS OR NOT IS_DIRECTORY "${ZEN_REPO}/${folder}")
        continue()
    endif()
    file(GLOB_RECURSE found RELATIVE "${ZEN_REPO}" "${ZEN_REPO}/${folder}/*")
    list(APPEND tree_files ${found})
endforeach()
file(GLOB root_files RELATIVE "${ZEN_REPO}" "${ZEN_REPO}/*" "${ZEN_REPO}/.*")
foreach(entry IN LISTS root_files)
    if(NOT IS_DIRECTORY "${ZEN_REPO}/${entry}")
        list(APPEND tree_files "${entry}")
    endif()
endforeach()
list(LENGTH tree_files tree_count)
if(tree_count EQUAL 0)
    message(FATAL_ERROR "doc-names: no file read under the folders tests/folders.txt names; an "
                        "empty tree resolves nothing (VM-POP-01)")
endif()
foreach(file IN LISTS tree_files)
    set(is_page FALSE)
    if(file MATCHES "[.]md$")
        foreach(rx IN LISTS ZEN_NAMES_PAGES)
            if(file MATCHES "${rx}")
                set(is_page TRUE)
            endif()
        endforeach()
        foreach(rx IN LISTS ZEN_NAMES_NOT_PAGES)
            if(file MATCHES "${rx}")
                set(is_page FALSE)
            endif()
        endforeach()
    endif()
    if(is_page)
        list(APPEND page_files "${file}")
    endif()
endforeach()
string(REPLACE ";" "\n/" ZEN_NAMES_TREE "${tree_files}")
set(ZEN_NAMES_TREE "\n/${ZEN_NAMES_TREE}\n")
# The quarry is named from the root: its folder and its catalog stand in the list, its files do not.
foreach(folder IN LISTS ZEN_NAMES_NOT_TAILS)
    file(GLOB found RELATIVE "${ZEN_REPO}" "${ZEN_REPO}/${folder}/*")
    foreach(entry IN LISTS found)
        if(NOT IS_DIRECTORY "${ZEN_REPO}/${entry}")
            string(APPEND ZEN_NAMES_TREE "/${entry}\n")
        endif()
    endforeach()
endforeach()

file(STRINGS "${ZEN_REPO}/tests/test_population.txt" pop_rows REGEX "^[^#]")
set(ZEN_NAMES_POPULATION "")
foreach(row IN LISTS pop_rows)
    string(REGEX REPLACE "#.*$" "" row "${row}")
    string(STRIP "${row}" row)
    if(row MATCHES "^([^ \t]+)[ \t]")
        list(APPEND ZEN_NAMES_POPULATION "${CMAKE_MATCH_1}")
    endif()
endforeach()
list(REMOVE_DUPLICATES ZEN_NAMES_POPULATION)
list(LENGTH ZEN_NAMES_POPULATION population_count)
list(LENGTH page_files page_count)
if(population_count EQUAL 0 OR page_count EQUAL 0)
    message(FATAL_ERROR "doc-names: ${page_count} pages and ${population_count} CTest entries read; "
                        "an empty population is a failure, not a pass (VM-POP-01)")
endif()

# ---- the names the tree does not hold ---------------------------------------------------------
set(problems "")
set(ZEN_NAMES_ROWS "")
file(STRINGS "${ZEN_REPO}/tests/doc_names.txt" name_rows REGEX "^[^#]")
foreach(row IN LISTS name_rows)
    if(NOT row MATCHES "^([^ \t]+)[ \t]+([a-z]+)[ \t]*$")
        list(APPEND problems "tests/doc_names.txt: '${row}' is not `name kind`")
        continue()
    endif()
    set(row_name "${CMAKE_MATCH_1}")
    set(row_kind "${CMAKE_MATCH_2}")
    if(NOT row_kind IN_LIST ZEN_NAMES_KINDS)
        string(REPLACE ";" ", " kinds "${ZEN_NAMES_KINDS}")
        list(APPEND problems "tests/doc_names.txt: ${row_name}'s kind '${row_kind}' is none of ${kinds}")
    endif()
    if(row_name IN_LIST ZEN_NAMES_ROWS)
        list(APPEND problems "tests/doc_names.txt names ${row_name} twice")
    endif()
    list(APPEND ZEN_NAMES_ROWS "${row_name}")
endforeach()

# ---- the pages --------------------------------------------------------------------------------
foreach(page IN LISTS page_files)
    file(READ "${ZEN_REPO}/${page}" text)
    zen_names_swap("${text}" text)
    get_filename_component(dir "${page}" DIRECTORY)
    set(router "")
    if(page STREQUAL "AGENTS.md")
        set(router "${ZEN_NAMES_ROOT_SECTION}")
    elseif(page MATCHES "(^|/)AGENTS[.]md$")
        set(router "Suites")
    endif()
    zen_names_page("${page}" "${dir}" "${router}" "${text}")
endforeach()
get_property(found GLOBAL PROPERTY ZEN_NAMES_PROBLEMS)
list(APPEND problems ${found})
get_property(used GLOBAL PROPERTY ZEN_NAMES_USED)
foreach(row IN LISTS ZEN_NAMES_ROWS)
    if(NOT row IN_LIST used)
        list(APPEND problems "tests/doc_names.txt: no page read names ${row} unresolved -- a row the tree or the pages no longer need is removed")
    endif()
endforeach()

get_property(read GLOBAL PROPERTY ZEN_NAMES_READ)
list(LENGTH ZEN_NAMES_ROWS row_count)
message(STATUS "doc-names: ${read} names read in ${page_count} pages against ${tree_count} files and "
               "${population_count} CTest entries; ${row_count} names the tree does not hold")
if(read EQUAL 0)
    message(FATAL_ERROR "doc-names: no name read; an empty population is a failure, not a pass (VM-POP-01)")
endif()
if(problems)
    list(LENGTH problems problem_count)
    string(REPLACE ";" "\n  " shown "${problems}")
    message(FATAL_ERROR "doc-names FAILED: ${problem_count} problem(s):\n  ${shown}\n"
                        "A name a page gives in backticks is a file or folder of this tree, read from the "
                        "root, from the page's folder or as the tail of a path, or a row of "
                        "tests/doc_names.txt; a suite a router names is a CTest entry.")
endif()
message(STATUS "doc-names: PASSED -- every path a router or a much-read page names is in the tree or "
               "in tests/doc_names.txt, and every suite a router names is a CTest entry")
