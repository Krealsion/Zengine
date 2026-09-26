# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
#
# The `doc_links` entry (docs/contributing/build-and-test.md): does each repo-local reference
# resolve -- a current-facing markdown file's links and anchors, a first-party source comment's
# `.md` path read from the repository root -- and does no current-facing file name a path outside
# this repository? Frozen history, the reference/ quarry and paths above the root are not asked.
#   cmake -P tests/check_doc_links.cmake    (from the repository root, or -DZEN_REPO=<repo>)

cmake_minimum_required(VERSION 3.16)

if(NOT DEFINED ZEN_REPO OR ZEN_REPO STREQUAL "")
    set(ZEN_REPO "${CMAKE_CURRENT_LIST_DIR}/..")
endif()
get_filename_component(ZEN_REPO "${ZEN_REPO}" ABSOLUTE)
if(NOT EXISTS "${ZEN_REPO}/AGENTS.md")
    message(FATAL_ERROR
        "doc-links: '${ZEN_REPO}' does not look like this repository's root (no AGENTS.md). "
        "Pass -DZEN_REPO=<repository root>.")
endif()

# ---- scope, declared here so a standalone clone carries its own rule -----------------
# Markdown is swept from the repository root, not a list of doc directories, so a new folder is
# covered the moment it exists; anything narrowing is a written exclusion.

# Frozen or generated. Matched against the repository-relative path of every candidate.
set(ZEN_DOC_EXCLUDE
    "^build(-[^/]*)?/"       # build/ and build-*/, as .gitignore names them; not builder/
    "^cmake-build"
    "^_install"
    "^\\.git/"
    "^\\.git$"              # a worktree's gitdir pointer: one line naming another tree, not a document
    "^out/"                  # a build tree name .gitignore also names
    "^\\.idea/"              # editor state, gitignored: it holds this machine's paths by design
    "^\\.vscode/"
    "^\\.claude/"            # harness state, gitignored
    "^docs/history/"         # frozen: describes the tree at its source commit
    "^reference/"            # the pre-Zen engine, kept as a quarry and not live
    "third_party/")          # vendored

# First-party C/C++, CMake and the population manifest, whose comments are read: the roots
# source_comments holds, so a new package is a root in both. A root that is a file is read alone.
set(ZEN_DOC_SOURCE_ROOTS
    CMakeLists.txt activation attention-pane builder builder-pane cmake component composer
    connections-pane demo-control desktop-pane editor-pane examples external-host files flow
    flow-host flow-pane info-pane input introspection inventory inventory-pane maker
    menu-presenter message-draft neovim neovim-editor operator smoke snake source-transfer surface
    terminal-pane tests timer ui workshop)
set(ZEN_DOC_SOURCE_GLOBS *.h *.hpp *.ipp *.c *.cc *.cpp *.cxx CMakeLists.txt *.cmake *.cmake.in
    test_population.txt)
# The globs as one name pattern, so each root is walked once: a walk per glob reads every
# directory once per glob, which a 9p mount charges for.
set(ZEN_DOC_SOURCE_NAMES "")
set(separator "")
foreach(glob IN LISTS ZEN_DOC_SOURCE_GLOBS)
    string(REPLACE "." "\\." name "${glob}")
    string(REPLACE "*" "[^/]*" name "${name}")
    string(APPEND ZEN_DOC_SOURCE_NAMES "${separator}${name}")
    set(separator "|")
endforeach()
set(ZEN_DOC_SOURCE_NAMES "(^|/)(${ZEN_DOC_SOURCE_NAMES})$")

# The document the self-test interrogates. Every repository has one, it is current-facing by
# definition, and it carries headings.
set(ZEN_DOC_SELFTEST_FILE "AGENTS.md")

# ---- paths outside this repository ----------------------------------------------------
# The external-reader rule made mechanical: these spellings leaked once and were reworded out --
# the maintainers' workspace and its report and prompt directories, its drive and mount, a
# harness's scratch directory, a build root beyond the tree, a home. Every current-facing text
# file is read whole and raw; the two files declaring the spellings, this one and the package
# witness's word list, alone may carry them, and the self-test checks this one carries each.
set(ZEN_DOC_OUTSIDE_SPELLINGS
    "Zen/" "reportbacks/" "/mnt/g/" "G:/" "programming/cpp" "scratchpad" "zen-build"
    "Temp/claude" "/home/joshua" "Users/Joshua" "G:\\")
# The backslash spelling is last, since `\` before a list separator escapes it; the self-test
# counts the list against this number, so a reorder is a red.
set(ZEN_DOC_OUTSIDE_SPELLING_COUNT 11)
set(ZEN_DOC_OUTSIDE_DECLARERS tests/check_doc_links.cmake tests/package/run.cmake)
set(ZEN_DOC_TEXT_EXTENSIONS md h hpp ipp c cc cpp cxx cmake txt json in yml yaml py sh)

# ---- the slug, GitHub's convention ---------------------------------------------------
# Lowercase, drop the punctuation GitHub drops, keep word characters and hyphens, and turn EACH
# whitespace character into one hyphen (`## POP-01 -- a law` anchors as `pop-01----a-law`): a
# lenient slug would accept anchors GitHub does not serve.
function(zen_doc_slug text out)
    string(STRIP "${text}" s)
    string(TOLOWER "${s}" s)
    string(REGEX REPLACE "[`*]" "" s "${s}")
    string(REGEX REPLACE "[^a-z0-9_ \t-]" "" s "${s}")
    string(REGEX REPLACE "[ \t]" "-" s "${s}")
    set(${out} "${s}" PARENT_SCOPE)
endfunction()

# Reading a file without letting its punctuation reshape a CMake list (VM-CHECK-03): `;` and `\`
# are dropped at the door. Neither can change an answer: the heading slug discards both, as
# GitHub does, and no path or anchor contains either.
function(zen_doc_read_markdown path out)
    file(READ "${path}" content)
    string(REPLACE "\r" "" content "${content}")
    string(REPLACE ";" "" content "${content}")
    string(REPLACE "\\" "" content "${content}")
    set(${out} "${content}" PARENT_SCOPE)
endfunction()

# The same, plus C/C++ escape removal -- `\X` pairs go first so that `\"` cannot close a
# string literal downstream, and so that a `\`-continued line joins the next one exactly as
# the language says it does (a `//` comment really does continue across one).
function(zen_doc_read_source path out)
    file(READ "${path}" content)
    string(REPLACE "\r" "" content "${content}")
    string(REPLACE ";" "" content "${content}")
    string(REGEX REPLACE "\\\\." "" content "${content}")
    string(REPLACE "\\" "" content "${content}")
    set(${out} "${content}" PARENT_SCOPE)
endfunction()

# Every comment in a C/C++ translation unit, as one blob: whole-content extraction, since a file
# split into a list of lines arrives with its tail welded. String literals go first, bounded to a
# line, so the `//` in a URL opens no comment; character literals stay, since `'/'` cannot forge
# a marker and a `'[^']*'` pattern would pair apostrophes in prose. Its limits all read more text,
# never less (a `//` inside a block comment matches again, a one-line raw string may be stripped
# as an ordinary one), so a reference is at worst seen twice and the caller de-duplicates.
function(zen_doc_comments content out)
    string(REGEX REPLACE "\"[^\"\n]*\"" "" code "${content}")
    string(REGEX MATCHALL "//[^\n]*" line_comments "${code}")
    string(REGEX MATCHALL "/\\*([^*]|\\*+[^*/])*\\*+/" block_comments "${code}")
    set(${out} "${line_comments} ${block_comments}" PARENT_SCOPE)
endfunction()

# Every comment in a CMake file, read as zen_doc_read_source leaves it: a `#` outside a quoted
# argument runs to its line's end, and a quoted argument may span lines, so each line is walked
# quote by quote and its comment kept whole, quotes and all. The brackets are swapped before the
# split (VM-CHECK-03); no path holds one. A bracket argument or bracket comment is not read.
function(zen_doc_cmake_comments content out)
    string(REPLACE "[" "(" content "${content}")
    string(REPLACE "]" ")" content "${content}")
    string(REPLACE "\n" ";" lines "${content}")
    set(found "")
    set(in_quote FALSE)
    foreach(rest IN LISTS lines)
        while(NOT rest STREQUAL "")
            if(in_quote)
                string(FIND "${rest}" "\"" close)
                if(close EQUAL -1)
                    break()
                endif()
                math(EXPR close "${close} + 1")
                string(SUBSTRING "${rest}" ${close} -1 rest)
                set(in_quote FALSE)
            endif()
            string(FIND "${rest}" "#" hash)
            string(FIND "${rest}" "\"" open)
            if(NOT hash EQUAL -1 AND (open EQUAL -1 OR hash LESS open))
                string(SUBSTRING "${rest}" ${hash} -1 comment)
                string(APPEND found " ${comment}")
                break()
            elseif(open EQUAL -1)
                break()
            endif()
            math(EXPR open "${open} + 1")
            string(SUBSTRING "${rest}" ${open} -1 rest)
            set(in_quote TRUE)
        endwhile()
    endforeach()
    set(${out} "${found}" PARENT_SCOPE)
endfunction()

# Every comment in the population manifest, as tests/check_population.cmake reads it: a `#`
# anywhere opens one, and no quote protects it.
function(zen_doc_manifest_comments content out)
    string(REGEX MATCHALL "#[^\n]*" comments "${content}")
    set(${out} "${comments}" PARENT_SCOPE)
endfunction()

# A source's kind, from its name alone: cmake, manifest or cxx.
function(zen_doc_source_kind rel out)
    set(kind cxx)
    if(rel MATCHES "(^|/)(CMakeLists\\.txt|[^/]*\\.cmake|[^/]*\\.cmake\\.in)$")
        set(kind cmake)
    elseif(rel MATCHES "(^|/)test_population\\.txt$")
        set(kind manifest)
    endif()
    set(${out} ${kind} PARENT_SCOPE)
endfunction()

# Every heading slug in a markdown file; an empty answer is meaningful (the self-test requires a
# real document to yield a real heading). Memoised by resolved path, since a hub document is the
# target of dozens of anchors, on a GLOBAL property: a function cannot write its caller's scope,
# and a cache threaded through each call would sit in the predicate the self-test exercises.
function(zen_doc_headings path out)
    string(MAKE_C_IDENTIFIER "${path}" key)
    get_property(cached GLOBAL PROPERTY "zen_doc_headings_${key}" SET)
    if(cached)
        get_property(slugs GLOBAL PROPERTY "zen_doc_headings_${key}")
        set(${out} "${slugs}" PARENT_SCOPE)
        return()
    endif()
    set(slugs "")
    if(EXISTS "${path}")
        zen_doc_read_markdown("${path}" content)
        string(REGEX MATCHALL "\n#+[ \t][^\n]*" heads "\n${content}")
        foreach(head IN LISTS heads)
            string(REGEX REPLACE "^\n#+[ \t]" "" head "${head}")
            zen_doc_slug("${head}" s)
            list(APPEND slugs "${s}")
        endforeach()
    endif()
    set_property(GLOBAL PROPERTY "zen_doc_headings_${key}" "${slugs}")
    set(${out} "${slugs}" PARENT_SCOPE)
endfunction()

# ---- the predicate, in one place so the self-test exercises the real one -------------
#
# Sets ${out} to one of: ok | outside | broken:<reason>. `outside` is a real answer and not
# a failure: it is how a cross-repository or absolute reference is counted and declined.
function(zen_doc_verdict base_dir target out)
    if(target MATCHES "^[A-Za-z][A-Za-z0-9+.-]*:" OR target MATCHES "^//")
        set(${out} "outside" PARENT_SCOPE)   # a URL, or a protocol-relative one
        return()
    endif()
    string(FIND "${target}" "#" hash)
    if(hash EQUAL 0)
        set(${out} "outside" PARENT_SCOPE)   # same-file anchor; not a path claim
        return()
    endif()
    set(fragment "")
    set(path "${target}")
    if(NOT hash EQUAL -1)
        string(SUBSTRING "${target}" 0 ${hash} path)
        math(EXPR after "${hash} + 1")
        string(SUBSTRING "${target}" ${after} -1 fragment)
    endif()
    if(path STREQUAL "")
        set(${out} "outside" PARENT_SCOPE)
        return()
    endif()
    if(IS_ABSOLUTE "${path}")
        set(${out} "outside" PARENT_SCOPE)
        return()
    endif()

    get_filename_component(resolved "${base_dir}/${path}" ABSOLUTE)
    string(FIND "${resolved}" "${ZEN_REPO}/" inside)
    if(NOT inside EQUAL 0)
        set(${out} "outside" PARENT_SCOPE)   # above the repository root: not ours to demand
        return()
    endif()

    if(NOT EXISTS "${resolved}")
        set(${out} "broken:no such path" PARENT_SCOPE)
        return()
    endif()
    if(fragment STREQUAL "")
        set(${out} "ok" PARENT_SCOPE)
        return()
    endif()
    if(NOT resolved MATCHES "\\.md$")
        set(${out} "ok" PARENT_SCOPE)        # a fragment on a non-markdown target: not ours to slug
        return()
    endif()
    zen_doc_headings("${resolved}" slugs)
    zen_doc_slug("${fragment}" wanted)
    list(FIND slugs "${wanted}" found)
    if(found EQUAL -1)
        set(${out} "broken:no heading anchors to that fragment" PARENT_SCOPE)
        return()
    endif()
    set(${out} "ok" PARENT_SCOPE)
endfunction()

# ---- the self-test (VM-CHECK-01): make it say NO, and make it say YES ------------------

set(selftest_doc "${ZEN_REPO}/${ZEN_DOC_SELFTEST_FILE}")
if(NOT EXISTS "${selftest_doc}")
    message(FATAL_ERROR
        "doc-links: SELF-TEST cannot run -- '${ZEN_DOC_SELFTEST_FILE}' is missing, so there "
        "is no live document to prove the checker against. A checker that has not been made "
        "to say NO has not passed.")
endif()

zen_doc_verdict("${ZEN_REPO}" "${ZEN_DOC_SELFTEST_FILE}" v)
if(NOT v STREQUAL "ok")
    message(FATAL_ERROR
        "doc-links: SELF-TEST FAILED -- the checker rejected '${ZEN_DOC_SELFTEST_FILE}', "
        "which exists (${v}). It has started refusing live documents, which is a different "
        "defect and not a safer one.")
endif()

zen_doc_verdict("${ZEN_REPO}" "docs/zen-no-such-document-doclinks-selftest.md" v)
if(NOT v MATCHES "^broken:")
    message(FATAL_ERROR
        "doc-links: SELF-TEST FAILED -- a path that does not exist was accepted (${v}). "
        "Every green this check reports would be the same green a broken detector reports.")
endif()

zen_doc_headings("${selftest_doc}" selftest_slugs)
list(LENGTH selftest_slugs selftest_heading_count)
if(selftest_heading_count EQUAL 0)
    message(FATAL_ERROR
        "doc-links: SELF-TEST FAILED -- no headings were read out of "
        "'${ZEN_DOC_SELFTEST_FILE}'. With an empty heading set every anchor would look "
        "broken, or (worse, in a future edit) every anchor would be waved through.")
endif()
list(GET selftest_slugs 0 selftest_slug)
zen_doc_verdict("${ZEN_REPO}" "${ZEN_DOC_SELFTEST_FILE}#${selftest_slug}" v)
if(NOT v STREQUAL "ok")
    message(FATAL_ERROR
        "doc-links: SELF-TEST FAILED -- the anchor '#${selftest_slug}', slugified from a "
        "heading read out of '${ZEN_DOC_SELFTEST_FILE}' moments ago, was rejected (${v}).")
endif()
zen_doc_verdict("${ZEN_REPO}"
                "${ZEN_DOC_SELFTEST_FILE}#zen-no-such-anchor-doclinks-selftest" v)
if(NOT v MATCHES "^broken:")
    message(FATAL_ERROR
        "doc-links: SELF-TEST FAILED -- an anchor that no heading produces was accepted "
        "(${v}) on a file that does exist. Path checking alone would then be the whole "
        "check, silently.")
endif()

# The outside-path predicate: the INDICES (into ZEN_DOC_OUTSIDE_SPELLINGS) of every spelling
# the text carries, empty when it names nothing outside the tree. Indices rather than the
# spellings themselves, so the backslash one can never reach a CMake list.
function(zen_doc_outside_hits text out)
    set(hits "")
    set(i 0)
    foreach(spelling IN LISTS ZEN_DOC_OUTSIDE_SPELLINGS)
        string(FIND "${text}" "${spelling}" at)
        if(NOT at EQUAL -1)
            list(APPEND hits "${i}")
        endif()
        math(EXPR i "${i} + 1")
    endforeach()
    set(${out} "${hits}" PARENT_SCOPE)
endfunction()

list(LENGTH ZEN_DOC_OUTSIDE_SPELLINGS outside_spelling_count)
if(NOT outside_spelling_count EQUAL ZEN_DOC_OUTSIDE_SPELLING_COUNT)
    message(FATAL_ERROR
        "doc-links: SELF-TEST FAILED -- ZEN_DOC_OUTSIDE_SPELLINGS holds ${outside_spelling_count} "
        "elements and ${ZEN_DOC_OUTSIDE_SPELLING_COUNT} were written. A spelling has welded into "
        "its neighbour (a trailing backslash before the list separator), and the welded pair "
        "would match nothing.")
endif()
zen_doc_outside_hits("the matrix is in Zen/reportbacks/X-evidence.md, on G:/ and /mnt/g/" outside_yes)
zen_doc_outside_hits("see docs/reference/messaging.md, and /home/you/my-thing in the guide" outside_no)
file(READ "${CMAKE_CURRENT_LIST_FILE}" outside_own_text)
zen_doc_outside_hits("${outside_own_text}" outside_own)
list(LENGTH outside_yes n_outside_yes)
list(LENGTH outside_own n_outside_own)
if(NOT n_outside_yes EQUAL 4 OR NOT outside_no STREQUAL ""
   OR NOT n_outside_own EQUAL outside_spelling_count)
    message(FATAL_ERROR
        "doc-links: SELF-TEST FAILED -- the outside-path predicate found ${n_outside_yes} of 4 "
        "planted spellings, '${outside_no}' in a clean sentence, and ${n_outside_own} of "
        "${outside_spelling_count} in the file that declares them. Every 'no leak' below would "
        "then be meaningless.")
endif()

# The comment readers: a path inside a quoted CMake argument, one that spans lines included, is
# not a comment's, and one quoted inside a comment is; in the manifest a quote protects nothing.
string(CONCAT cmake_sample "set(x \"a # no/one.md\n# no/two.md \") # yes/three.md \"yes/four.md\"\n"
                           "# yes/five.md\nset(y \"z\") # yes/six.md\n")
zen_doc_cmake_comments("${cmake_sample}" cmake_seen)
string(REGEX MATCHALL "[a-z]+/[a-z]+\\.md" cmake_seen "${cmake_seen}")
zen_doc_manifest_comments("x compile-negative always \"a # yes/seven.md\"\n# yes/eight.md\n"
                          manifest_seen)
string(REGEX MATCHALL "[a-z]+/[a-z]+\\.md" manifest_seen "${manifest_seen}")
if(NOT cmake_seen STREQUAL "yes/three.md;yes/four.md;yes/five.md;yes/six.md"
   OR NOT manifest_seen STREQUAL "yes/seven.md;yes/eight.md")
    message(FATAL_ERROR
        "doc-links: SELF-TEST FAILED -- the CMake comment reader found '${cmake_seen}' and the "
        "manifest reader '${manifest_seen}'; a reader that drops a comment or reads an argument "
        "leaves a broken reference unseen, or invents one.")
endif()
foreach(name_case "a/b/x.hpp;TRUE" "a/b/CMakeLists.txt;TRUE" "a/b/x.cmake.in;TRUE"
                  "a/test_population.txt;TRUE" "a/b/x_hpp;FALSE" "a/b/x.hpp.orig;FALSE"
                  "a/b/notes.txt;FALSE")
    list(GET name_case 0 name_rel)
    list(GET name_case 1 name_want)
    set(name_named FALSE)
    if(name_rel MATCHES "${ZEN_DOC_SOURCE_NAMES}")
        set(name_named TRUE)
    endif()
    if(NOT name_named STREQUAL name_want)
        message(FATAL_ERROR
            "doc-links: SELF-TEST FAILED -- the source name '${name_rel}' is ${name_named}, want "
            "${name_want}: the name pattern and ZEN_DOC_SOURCE_GLOBS have come apart.")
    endif()
endforeach()

message(STATUS
    "doc-links: self-test OK -- a missing path and a missing anchor are both refused, a "
    "live document and one of its own headings are both accepted; four planted outside paths "
    "are found, a clean sentence is not, and this file carries every declared spelling; CMake "
    "and manifest comments are read as their own readers read them, and the source names "
    "match the globs")

# ---- gathering the two populations -----------------------------------------------------

function(zen_doc_excluded rel out)
    foreach(pattern IN LISTS ZEN_DOC_EXCLUDE)
        if(rel MATCHES "${pattern}")
            set(${out} 1 PARENT_SCOPE)
            return()
        endif()
    endforeach()
    set(${out} 0 PARENT_SCOPE)
endfunction()

# A build tree is excluded and a package named like one is not.
foreach(excluded_case "build/x.md;1" "build-san/x.md;1" "cmake-build-debug/x.md;1"
                      "builder/weave.hpp;0" "builder-pane/pane.cpp;0" "tests/third_party/x.h;1")
    list(GET excluded_case 0 excluded_rel)
    list(GET excluded_case 1 excluded_want)
    zen_doc_excluded("${excluded_rel}" excluded_got)
    if(NOT excluded_got EQUAL excluded_want)
        message(FATAL_ERROR
            "doc-links: SELF-TEST FAILED -- '${excluded_rel}' is excluded ${excluded_got}, want "
            "${excluded_want}: an exclusion that swallows a package silently empties its root.")
    endif()
endforeach()

# Excluded top-level directories are pruned before the walk rather than filtered after: the same
# result, without enumerating a fetched dependency tree on a slow filesystem; anything not
# excluded is still walked, so a new folder is covered at once. Root files come from a plain
# `file(GLOB)`: `file(GLOB_RECURSE)` on `<repo>/README.md` walks the whole repository.
file(GLOB root_md RELATIVE "${ZEN_REPO}" "${ZEN_REPO}/*.md")

set(md_globs "")
set(pruned "")
file(GLOB top_level RELATIVE "${ZEN_REPO}" "${ZEN_REPO}/*")
foreach(entry IN LISTS top_level)
    if(IS_DIRECTORY "${ZEN_REPO}/${entry}")
        zen_doc_excluded("${entry}/" skip)
        if(skip)
            list(APPEND pruned "${entry}")
        else()
            list(APPEND md_globs "${ZEN_REPO}/${entry}/*.md")
        endif()
    endif()
endforeach()

set(nested_md "")
if(md_globs)
    file(GLOB_RECURSE nested_md RELATIVE "${ZEN_REPO}" ${md_globs})
endif()
set(all_md ${root_md} ${nested_md})

set(md_files "")
set(md_excluded 0)
foreach(rel IN LISTS all_md)
    zen_doc_excluded("${rel}" skip)
    if(skip)
        math(EXPR md_excluded "${md_excluded} + 1")
    else()
        list(APPEND md_files "${rel}")
    endif()
endforeach()

# Each source root is walked once, and must yield a file this check reads: a root emptied by an
# exclusion would be reported as read.
set(src_files "")
foreach(root IN LISTS ZEN_DOC_SOURCE_ROOTS)
    if(IS_DIRECTORY "${ZEN_REPO}/${root}")
        file(GLOB_RECURSE found RELATIVE "${ZEN_REPO}" "${ZEN_REPO}/${root}/*")
        list(FILTER found INCLUDE REGEX "${ZEN_DOC_SOURCE_NAMES}")
    elseif(EXISTS "${ZEN_REPO}/${root}")
        set(found "${root}")
    else()
        message(FATAL_ERROR "doc-links: the source root '${root}' names nothing in ${ZEN_REPO}")
    endif()
    set(kept 0)
    foreach(rel IN LISTS found)
        zen_doc_excluded("${rel}" skip)
        if(NOT skip)
            list(APPEND src_files "${rel}")
            math(EXPR kept "${kept} + 1")
        endif()
    endforeach()
    if(kept EQUAL 0)
        message(FATAL_ERROR
            "doc-links: the source root '${root}' yields no file to read -- none matches "
            "ZEN_DOC_SOURCE_GLOBS, or ZEN_DOC_EXCLUDE takes every one -- so its comments would "
            "be reported as read when none was.")
    endif()
endforeach()
list(REMOVE_DUPLICATES src_files)
list(SORT src_files)

# Population 3: every current-facing file of a text kind -- root files by a plain glob, then
# everything under each unpruned top-level directory. Text is decided by extension, plus the
# extensionless and dot-named files at the root (LICENSE, .gitignore, .gitattributes).
file(GLOB root_any RELATIVE "${ZEN_REPO}" "${ZEN_REPO}/*")
set(any_globs "")
foreach(entry IN LISTS top_level)
    if(IS_DIRECTORY "${ZEN_REPO}/${entry}" AND NOT entry IN_LIST pruned)
        list(APPEND any_globs "${ZEN_REPO}/${entry}/*")
    endif()
endforeach()
set(nested_any "")
if(any_globs)
    file(GLOB_RECURSE nested_any RELATIVE "${ZEN_REPO}" ${any_globs})
endif()
set(text_files "")
foreach(rel IN LISTS root_any nested_any)
    if(IS_DIRECTORY "${ZEN_REPO}/${rel}")
        continue()
    endif()
    zen_doc_excluded("${rel}" skip)
    if(skip)
        continue()
    endif()
    get_filename_component(name "${rel}" NAME)
    get_filename_component(ext "${rel}" LAST_EXT)
    string(REGEX REPLACE "^\\." "" ext "${ext}")
    if(ext STREQUAL "" OR name MATCHES "^\\.[A-Za-z]+$")
        if(NOT rel MATCHES "/")
            list(APPEND text_files "${rel}")
        endif()
    elseif(ext IN_LIST ZEN_DOC_TEXT_EXTENSIONS)
        list(APPEND text_files "${rel}")
    endif()
endforeach()
list(REMOVE_DUPLICATES text_files)

list(LENGTH md_files md_count)
list(LENGTH src_files src_count)
if(md_count EQUAL 0 OR src_count EQUAL 0)
    message(FATAL_ERROR
        "doc-links: the sweep found ${md_count} markdown file(s) and ${src_count} first-party "
        "source file(s). An expectation of nothing is satisfied by anything (POP-01), so an "
        "empty population is a failure here and not a quiet pass -- check ZEN_REPO and the "
        "exclusion rules at the top of this file.")
endif()

# ---- population 1: markdown links ------------------------------------------------------
#
# Link targets are resolved relative to the FILE, which is how markdown itself resolves them
# and how they render on the forge.

set(problems "")
set(checked 0)
set(outside 0)

foreach(rel IN LISTS md_files)
    set(path "${ZEN_REPO}/${rel}")
    get_filename_component(base "${path}" DIRECTORY)
    zen_doc_read_markdown("${path}" content)
    string(REGEX MATCHALL "\\[[^]]*\\]\\([^)\r\n \t]+\\)" links "${content}")
    foreach(link IN LISTS links)
        if(NOT link MATCHES "\\(([^)]+)\\)$")
            continue()
        endif()
        set(target "${CMAKE_MATCH_1}")
        zen_doc_verdict("${base}" "${target}" v)
        if(v STREQUAL "outside")
            math(EXPR outside "${outside} + 1")
        elseif(v MATCHES "^broken:(.*)$")
            list(APPEND problems "${rel}: ${CMAKE_MATCH_1} -> ${target}")
            math(EXPR checked "${checked} + 1")
        else()
            math(EXPR checked "${checked} + 1")
        endif()
    endforeach()
endforeach()

if(checked EQUAL 0)
    message(FATAL_ERROR
        "doc-links: ${md_count} markdown files yielded ZERO repo-local links to check. Either "
        "the extractor stopped recognising markdown link syntax or every document lost its "
        "cross-references; both are failures, and neither is a green.")
endif()
set(md_checked "${checked}")

# ---- population 2: repository-relative doc paths in first-party source comments ---------
# Comment text only -- C/C++, CMake and the population manifest's -- resolved against the
# repository root rather than the file: a comment travels with its code, and a file-relative
# reference would break on the next move. A file naming no `.md` is skipped whole.

set(src_scanned 0)
set(src_refs 0)

foreach(rel IN LISTS src_files)
    set(path "${ZEN_REPO}/${rel}")
    zen_doc_source_kind("${rel}" kind)
    if(kind STREQUAL "manifest")
        zen_doc_read_markdown("${path}" content)
    else()
        zen_doc_read_source("${path}" content)
    endif()
    string(FIND "${content}" ".md" mentions)
    if(mentions EQUAL -1)
        continue()
    endif()
    math(EXPR src_scanned "${src_scanned} + 1")

    if(kind STREQUAL "manifest")
        zen_doc_manifest_comments("${content}" comment_text)
    elseif(kind STREQUAL "cmake")
        zen_doc_cmake_comments("${content}" comment_text)
    else()
        zen_doc_comments("${content}" comment_text)
    endif()
    string(REGEX MATCHALL "[A-Za-z0-9_.][A-Za-z0-9_./-]*\\.md(#[A-Za-z0-9_-]+)?"
           refs "${comment_text}")
    if(refs)
        list(REMOVE_DUPLICATES refs)
    endif()
    foreach(ref IN LISTS refs)
        zen_doc_verdict("${ZEN_REPO}" "${ref}" v)
        if(v STREQUAL "outside")
            math(EXPR outside "${outside} + 1")
        elseif(v MATCHES "^broken:(.*)$")
            string(CONCAT problem "${rel}: ${CMAKE_MATCH_1} -> ${ref}"
                   "   (a source comment's reference is resolved against the"
                   " repository root, not against the file)")
            list(APPEND problems "${problem}")
            math(EXPR src_refs "${src_refs} + 1")
        else()
            math(EXPR src_refs "${src_refs} + 1")
        endif()
    endforeach()
endforeach()

# ---- population 3: paths outside this repository ---------------------------------------
#
# Every current-facing text file, read whole and raw. A hit names the file, the line and the
# spelling; the remedy is to say the thing in words, never to widen the exclusions. The two
# declaring files are exempt by written rule and counted as such.

set(outside_read 0)
set(outside_exempt 0)
set(outside_leaks 0)
foreach(rel IN LISTS text_files)
    if(rel IN_LIST ZEN_DOC_OUTSIDE_DECLARERS)
        math(EXPR outside_exempt "${outside_exempt} + 1")
        continue()
    endif()
    file(READ "${ZEN_REPO}/${rel}" raw)
    math(EXPR outside_read "${outside_read} + 1")
    zen_doc_outside_hits("${raw}" hits)
    foreach(i IN LISTS hits)
        list(GET ZEN_DOC_OUTSIDE_SPELLINGS ${i} spelling)
        string(FIND "${raw}" "${spelling}" at)
        string(SUBSTRING "${raw}" 0 ${at} before)
        string(REGEX MATCHALL "\n" newlines "${before}")
        list(LENGTH newlines line0)
        math(EXPR line "${line0} + 1")
        string(REPLACE "\\" "<backslash>" shown "${spelling}")
        string(CONCAT problem "${rel}:${line}: names a path outside this repository ('${shown}')"
               "   (the external-reader rule, docs/contributing/repository-conventions.md:"
               " say the thing in words)")
        list(APPEND problems "${problem}")
        math(EXPR outside_leaks "${outside_leaks} + 1")
    endforeach()
endforeach()
if(outside_read EQUAL 0)
    message(FATAL_ERROR
        "doc-links: the sweep read ZERO current-facing text files for outside paths. An "
        "expectation of nothing is satisfied by anything; check the text-kind list and the "
        "exclusion rules at the top of this file.")
endif()

# ---- the report ------------------------------------------------------------------------

list(LENGTH pruned pruned_count)
message(STATUS "doc-links: ${md_count} markdown files (${md_excluded} excluded by rule, "
               "${pruned_count} top-level directories pruned), ${md_checked} repo-local "
               "links checked")
message(STATUS "doc-links: ${src_count} first-party source files (C/C++, CMake and the "
               "population manifest), ${src_scanned} carrying a .md reference, ${src_refs} "
               "comment references checked")
message(STATUS "doc-links: ${outside} references counted and declined (external URL, "
               "same-file anchor, or above the repository root)")
message(STATUS "doc-links: ${outside_read} current-facing text files read whole for a path "
               "outside this repository (${outside_spelling_count} spellings; "
               "${outside_exempt} declaring files exempt by rule) -- ${outside_leaks} found")

if(NOT problems STREQUAL "")
    list(LENGTH problems problem_count)
    set(text "")
    foreach(problem IN LISTS problems)
        string(APPEND text "  ${problem}\n")
    endforeach()
    message(FATAL_ERROR
        "doc-links FAILED: ${problem_count} broken repo-local documentation reference(s) or "
        "path(s) outside this repository.\n"
        "${text}\n"
        "  Fix the reference, or -- if the document is deliberately frozen history -- widen "
        "ZEN_DOC_EXCLUDE in this file with a written rule rather than silencing one path. A "
        "path outside the repository is reworded into words, never excluded.")
endif()

message(STATUS "doc-links: PASSED -- every repo-local documentation reference resolves, and no "
               "current-facing file names a path outside this repository")
