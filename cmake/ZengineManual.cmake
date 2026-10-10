# A weave's manual: one page split at its headings by the one grammar here, which the build embeds
# with and tests/check_manual.cmake judges with. `zengine_weave(... MANUAL <page>)` or
# `zengine_manual(<target> <scope> <page>)` writes `manuals/<office>.hpp` beside the build, defining
# `zengine::manual::pages::<office>::kManual`; `zengine_manual_split(<page> <prefix>)` reads one in
# a check. Reference: manual/docs/manual.md.

if(CMAKE_VERSION VERSION_LESS 3.18)
    message(FATAL_ERROR "ZengineManual.cmake: needs CMake 3.18 or later (string(HEX)); this is "
                        "${CMAKE_VERSION}")
endif()

set(ZENGINE_MANUAL_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")

# The bounds, read from their owner. A bound renamed or gone stops the split: a page judged by a
# number this file guessed would be judged by nobody's law.
function(zengine_manual_bounds)
    file(READ "${ZENGINE_MANUAL_ROOT}/manual/manual.hpp" owner)
    foreach(name kMaxManualKeyBytes kMaxManualSectionBytes kMaxManualFollowBytes
                 kMaxManualHeardBytes kMaxManualSections kMaxManualBytes)
        string(REGEX MATCH "${name} = ([0-9]+);" found "${owner}")
        if(found STREQUAL "")
            message(FATAL_ERROR "ZengineManual.cmake: manual/manual.hpp declares no ${name}")
        endif()
        set(ZEN_MANUAL_${name} "${CMAKE_MATCH_1}" PARENT_SCOPE)
    endforeach()
endfunction()

# A heading's key: lowercase, a space a dash, nothing else kept.
function(zengine_manual_slug heading out)
    string(TOLOWER "${heading}" s)
    string(REPLACE " " "-" s "${s}")
    string(REGEX REPLACE "[^a-z0-9-]" "" s "${s}")
    set(${out} "${s}" PARENT_SCOPE)
endfunction()

# Where a `##` heading may stand: its rank, which never goes down through a page, and 0 for a
# heading the form does not know. `Commands in <mode>` (or `on`) may repeat.
function(zengine_manual_rank heading out)
    set(rank 0)
    if(heading STREQUAL "About")
        set(rank 1)
    elseif(heading STREQUAL "Use")
        set(rank 2)
    elseif(heading STREQUAL "Commands")
        set(rank 3)
    elseif(heading MATCHES "^Commands (in|on) [A-Za-z0-9 '-]+$")
        set(rank 4)
    elseif(heading STREQUAL "Hears")
        set(rank 5)
    elseif(heading STREQUAL "Follow")
        set(rank 6)
    elseif(heading STREQUAL "Shows")
        set(rank 7)
    elseif(heading STREQUAL "Files")
        set(rank 8)
    elseif(heading STREQUAL "See also")
        set(rank 9)
    endif()
    set(${out} ${rank} PARENT_SCOPE)
endfunction()

# THE PAGE AS SECTIONS. Sets, in the caller's scope: <prefix>_OFFICE, _NAME, _ID (the SHA-256 of
# the page with its line endings as LF), _BYTES, _COUNT, and for each section i from 0: _KEY_<i>,
# _GROUP_<i>, _TEXT_<i> (its Markdown, its heading left out, blank lines at either end trimmed) and
# _LINE_<i>; and _REFUSALS, every way the page is out of form, each naming its line. A page with
# refusals is never embedded.
function(zengine_manual_split page prefix)
    file(READ "${page}" text)
    zengine_manual_split_text("${text}" "${prefix}")
    foreach(field OFFICE NAME ID BYTES COUNT REFUSALS)
        set(${prefix}_${field} "${${prefix}_${field}}" PARENT_SCOPE)
    endforeach()
    if(${prefix}_COUNT GREATER 0)
        math(EXPR last "${${prefix}_COUNT} - 1")
        foreach(i RANGE ${last})
            foreach(field KEY GROUP TEXT LINE)
                set(${prefix}_${field}_${i} "${${prefix}_${field}_${i}}" PARENT_SCOPE)
            endforeach()
        endforeach()
    endif()
endfunction()

# ...and a page given as its text, as a check judges its own samples.
function(zengine_manual_split_text text prefix)
    zengine_manual_bounds()
    string(REPLACE "\r\n" "\n" text "${text}")
    string(SHA256 id "${text}")
    string(LENGTH "${text}" page_bytes)
    # Control bytes a page may not carry (a tab among them); a line break is the one allowed.
    string(ASCII 1 2 3 4 5 6 7 8 9 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31
                 127 controls)

    set(refusals "")
    set(count 0)
    set(keys "")
    set(office "")
    set(name "")
    set(rank 0)
    set(group "")          # the `##` heading the open section stands under
    set(key "lead")        # the open section's key; the lead until the first `##`
    set(key_line 1)
    set(kind "lead")       # lead, group, command, heard
    set(body "")
    set(labels "")         # a command's four labelled lines, as found
    set(fence OFF)
    set(sections_seen "")

    if(page_bytes GREATER ZEN_MANUAL_kMaxManualBytes)
        list(APPEND refusals "the page is ${page_bytes} bytes, past ${ZEN_MANUAL_kMaxManualBytes}")
    endif()

    # Close the open section: trim it, judge it, and keep it unless it is a group's empty opening.
    macro(zengine_manual_close)
        string(REGEX REPLACE "^\n+" "" body "${body}")
        string(REGEX REPLACE "[ \n]+$" "" body "${body}")
        string(LENGTH "${body}" bytes)
        set(bound ${ZEN_MANUAL_kMaxManualSectionBytes})
        if(key STREQUAL "follow")
            set(bound ${ZEN_MANUAL_kMaxManualFollowBytes})
        elseif(kind STREQUAL "heard")
            set(bound ${ZEN_MANUAL_kMaxManualHeardBytes})
        endif()
        if(bytes GREATER bound)
            list(APPEND refusals "line ${key_line}: `${key}` is ${bytes} bytes, past ${bound}")
        endif()
        if(kind STREQUAL "command")
            if(NOT labels STREQUAL "Takes;Answers;Refuses;Example")
                string(REPLACE ";" ", " found_labels "${labels}")
                list(APPEND refusals "line ${key_line}: `${key}` needs its four lines in order, **Takes:**, **Answers:**, **Refuses:**, **Example:** (found: ${found_labels})")
            endif()
        endif()
        if(bytes EQUAL 0 AND NOT kind STREQUAL "group")
            list(APPEND refusals "line ${key_line}: `${key}` has no words")
        endif()
        if(bytes GREATER 0 OR NOT kind STREQUAL "group")
            set(${prefix}_KEY_${count} "${key}" PARENT_SCOPE)
            set(${prefix}_GROUP_${count} "${group}" PARENT_SCOPE)
            set(${prefix}_TEXT_${count} "${body}" PARENT_SCOPE)
            set(${prefix}_LINE_${count} "${key_line}" PARENT_SCOPE)
            math(EXPR count "${count} + 1")
        endif()
        set(body "")
        set(labels "")
    endmacro()

    string(LENGTH "${text}" total)
    set(at 0)
    set(line_no 0)
    while(at LESS total)
        string(SUBSTRING "${text}" ${at} -1 tail)
        string(FIND "${tail}" "\n" nl)
        if(nl EQUAL -1)
            set(line "${tail}")
            set(at ${total})
        else()
            string(SUBSTRING "${tail}" 0 ${nl} line)
            math(EXPR at "${at} + ${nl} + 1")
        endif()
        math(EXPR line_no "${line_no} + 1")

        string(REGEX MATCH "[${controls}]" control "${line}")
        if(NOT control STREQUAL "")
            list(APPEND refusals "line ${line_no}: a control character, which a page may not carry")
        endif()

        if(line_no EQUAL 1)
            if(line MATCHES "^# ([a-z0-9][a-z0-9-]*(\\.[a-z0-9][a-z0-9-]*)+) -- ([^\"\\\\]+)$")
                set(office "${CMAKE_MATCH_1}")
                set(name "${CMAKE_MATCH_3}")
            else()
                list(APPEND refusals "line 1: the title is not `# <office> -- <the name a weaver sees>`")
            endif()
            continue()
        endif()

        if(line MATCHES "^```")
            if(fence)
                set(fence OFF)
            else()
                set(fence ON)
            endif()
        endif()

        if(NOT fence AND line MATCHES "^## (.*)$")
            set(heading "${CMAKE_MATCH_1}")
            zengine_manual_close()
            zengine_manual_rank("${heading}" next)
            if(next EQUAL 0)
                list(APPEND refusals "line ${line_no}: `## ${heading}` is not a heading a manual has")
            elseif(next LESS rank OR (next EQUAL rank AND NOT next EQUAL 4))
                list(APPEND refusals "line ${line_no}: `## ${heading}` is out of order")
            else()
                set(rank ${next})
            endif()
            list(APPEND sections_seen "${heading}")
            set(group "${heading}")
            zengine_manual_slug("${heading}" key)
            list(FIND keys "${key}" again)
            if(NOT again EQUAL -1)
                list(APPEND refusals "line ${line_no}: `## ${heading}` stands twice")
            endif()
            list(APPEND keys "${key}")
            set(key_line ${line_no})
            if(heading MATCHES "^(Commands|Commands (in|on) .*|Hears|Shows)$")
                set(kind "group")
            else()
                set(kind "section")
            endif()
        elseif(NOT fence AND line MATCHES "^### (.*)$")
            set(heading "${CMAKE_MATCH_1}")
            zengine_manual_close()
            set(key_line ${line_no})
            if(group MATCHES "^(Commands|Commands (in|on) .*)$")
                set(kind "command")
            elseif(group MATCHES "^(Hears|Shows)$")
                set(kind "heard")
            else()
                set(kind "section")
                list(APPEND refusals "line ${line_no}: a `###` section stands only under Commands, Hears or Shows, not under `${group}`")
            endif()
            if(heading MATCHES "^`([A-Za-z0-9_.-]+)`$")
                set(key "${CMAKE_MATCH_1}")
                string(LENGTH "${key}" key_bytes)
                if(key_bytes GREATER ZEN_MANUAL_kMaxManualKeyBytes)
                    list(APPEND refusals "line ${line_no}: the key `${key}` is past ${ZEN_MANUAL_kMaxManualKeyBytes} bytes")
                endif()
            else()
                set(key "line-${line_no}")
                list(APPEND refusals "line ${line_no}: a section's heading is its key in backticks and nothing else")
            endif()
            list(FIND keys "${key}" again)
            if(NOT again EQUAL -1)
                list(APPEND refusals "line ${line_no}: `${key}` has two sections")
            endif()
            list(APPEND keys "${key}")
        elseif(NOT fence AND line MATCHES "^# ")
            list(APPEND refusals "line ${line_no}: a page has one `#` heading, its title")
        else()
            if(kind STREQUAL "command" AND line MATCHES "^- \\*\\*(Takes|Answers|Refuses|Example):\\*\\*")
                list(APPEND labels "${CMAKE_MATCH_1}")
            endif()
            string(APPEND body "${line}\n")
        endif()
    endwhile()
    zengine_manual_close()

    list(LENGTH sections_seen seen)
    set(first "")
    set(second "")
    if(seen GREATER 0)
        list(GET sections_seen 0 first)
    endif()
    if(seen GREATER 1)
        list(GET sections_seen 1 second)
    endif()
    if(NOT first STREQUAL "About" OR NOT second STREQUAL "Use")
        list(APPEND refusals "the page opens with `## About` and `## Use`")
    endif()
    if(count GREATER ZEN_MANUAL_kMaxManualSections)
        list(APPEND refusals "the page has ${count} sections, past ${ZEN_MANUAL_kMaxManualSections}")
    endif()

    set(${prefix}_OFFICE "${office}" PARENT_SCOPE)
    set(${prefix}_NAME "${name}" PARENT_SCOPE)
    set(${prefix}_ID "${id}" PARENT_SCOPE)
    set(${prefix}_BYTES "${page_bytes}" PARENT_SCOPE)
    set(${prefix}_COUNT "${count}" PARENT_SCOPE)
    set(${prefix}_REFUSALS "${refusals}" PARENT_SCOPE)
endfunction()

# Bytes as a C++ initializer: `0x23,0x20,...` -- spelled as numbers, so no character of the page
# meets a compiler's source encoding.
function(zengine_manual_bytes value out)
    string(HEX "${value}" hex)
    string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," hex "${hex}")
    string(REGEX REPLACE "((0x[0-9a-f][0-9a-f],){24})" "\\1\n    " hex "${hex}")
    set(${out} "${hex}" PARENT_SCOPE)
endfunction()

# A TARGET CARRIES A PAGE: its sections, compiled in. A page out of form stops the configure,
# naming each refusal, so no build embeds a page the check would refuse.
function(zengine_manual target scope page)
    get_filename_component(page "${page}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    zengine_manual_split("${page}" zm)
    if(zm_REFUSALS)
        string(REPLACE ";" "\n  " said "${zm_REFUSALS}")
        message(FATAL_ERROR "zengine_manual: ${page} is out of form:\n  ${said}")
    endif()
    string(MAKE_C_IDENTIFIER "${zm_OFFICE}" ident)
    file(RELATIVE_PATH shown "${PROJECT_SOURCE_DIR}" "${page}")

    set(arrays "")
    set(table "")
    zengine_manual_bytes("${zm_NAME}" name_bytes)
    math(EXPR last "${zm_COUNT} - 1")
    foreach(i RANGE ${last})
        zengine_manual_bytes("${zm_TEXT_${i}}" bytes)
        string(LENGTH "${zm_TEXT_${i}}" n)
        string(APPEND arrays "inline constexpr char kText${i}[] = {\n    ${bytes}0x00};\n")
        string(APPEND table "    {\"${zm_KEY_${i}}\", \"${zm_GROUP_${i}}\", std::string_view(kText${i}, ${n})},\n")
    endforeach()
    string(LENGTH "${zm_NAME}" name_n)

    set(dir "${PROJECT_BINARY_DIR}/zengine-manuals/${target}")
    file(GENERATE OUTPUT "${dir}/manuals/${zm_OFFICE}.hpp" CONTENT
"// GENERATED by cmake/ZengineManual.cmake from ${shown} -- do not edit and do not check in: the
// page is the source, and the configure that wrote this rewrites it when the page changes.
//
// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#pragma once

#include \"manual/manual.hpp\"

#include <string_view>

namespace zengine::manual::pages::${ident} {

inline constexpr char kName[] = {
    ${name_bytes}0x00};
${arrays}
inline constexpr Section kSections[] = {
${table}};

inline constexpr Manual kManual{\"${zm_OFFICE}\", std::string_view(kName, ${name_n}),
                                \"${zm_ID}\", kSections, ${zm_COUNT}};

} // namespace zengine::manual::pages::${ident}
")
    target_include_directories(${target} ${scope} "${dir}")
    target_link_libraries(${target} ${scope} zengine-manual)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
                 "${page}" "${ZENGINE_MANUAL_ROOT}/manual/manual.hpp")
endfunction()
