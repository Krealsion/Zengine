# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
#
# The `package_vocabulary` entry (docs/contributing/build-and-test.md): does any current-facing
# file still spell a retired public package variable? The list lives here and nowhere else, so
# this file is the one place the spellings appear, and it must carry every one. It checks exact
# spellings, not the word "weave", which the repository rightly uses throughout.
#   cmake [-DZEN_REPO=<repo>] -P tests/check_package_vocabulary.cmake   (from the repository root)

cmake_minimum_required(VERSION 3.16)

if(NOT DEFINED ZEN_REPO OR ZEN_REPO STREQUAL "")
    set(ZEN_REPO "${CMAKE_CURRENT_LIST_DIR}/..")
endif()
get_filename_component(ZEN_REPO "${ZEN_REPO}" ABSOLUTE)
if(NOT EXISTS "${ZEN_REPO}/AGENTS.md")
    message(FATAL_ERROR
        "package-vocabulary: '${ZEN_REPO}' does not look like this repository's root (no "
        "AGENTS.md). Pass -DZEN_REPO=<repository root>.")
endif()

# ---- what may no longer be written, and what replaced it -------------------------------
#
# The internal spellings are here beside the public ones on purpose: they are what the public
# names are GENERATED from, so a reintroduction through `ZengineInstall.cmake` would put the
# false noun back into every installed package without ever writing the public name in the tree.
set(ZEN_PKG_RETIRED
    "ZENGINE_WEAVE_DIR"           "ZENGINE_ARTIFACT_DIR"
    "ZENGINE_WEAVES"              "ZENGINE_RUNTIME_ARTIFACTS"
    "ZENGINE_INSTALL_WEAVEDIR"    "ZENGINE_INSTALL_ARTIFACTDIR"
    "ZENGINE_INSTALLED_WEAVES"    "ZENGINE_INSTALLED_ARTIFACTS")

# The token the self-test requires the sweep to FIND. It is the current public variable, so
# this doubles as a check that the package config template is still in scope.
set(ZEN_PKG_SENTINEL "ZENGINE_RUNTIME_ARTIFACTS")

# ---- scope: everything current-facing, narrowed only by written rule --------------------
#
# Swept from the repository root rather than from a list of directories, for the reason
# doc_links gives: a new folder must be covered the moment it exists, and an omission that
# quietly reduces coverage is the failure this file exists to prevent.
set(ZEN_PKG_EXCLUDE
    "^build(-[^/]*)?/"       # build/ and build-*/, as .gitignore names them; not builder/
    "^cmake-build"
    "^_install"
    "^\\.git/"
    "^docs/history/"         # frozen: describes the tree at its source commit
    "^reference/"            # the pre-Zen engine, kept as a quarry and not live
    "third_party/")          # vendored

set(ZEN_PKG_GLOBS
    *.md *.txt *.cmake *.in *.yml *.yaml
    *.h *.hpp *.ipp *.c *.cc *.cpp *.cxx)

function(zen_pkg_excluded rel out)
    foreach(pattern IN LISTS ZEN_PKG_EXCLUDE)
        if(rel MATCHES "${pattern}")
            set(${out} 1 PARENT_SCOPE)
            return()
        endif()
    endforeach()
    set(${out} 0 PARENT_SCOPE)
endfunction()

# ---- the predicate, in one place so the self-test exercises the real one ----------------
#
# Sets ${out} to the number of times `token` appears in `text`. Whole-content matching rather
# than a line walk: check_doc_links.cmake measured that CMake stops honouring `;` as an element
# boundary past a few dozen of them, so a file split into lines silently welds its tail into
# one element. There is no list here to go wrong.
function(zen_pkg_count text token out)
    string(REGEX MATCHALL "${token}" hits "${text}")
    list(LENGTH hits n)
    set(${out} "${n}" PARENT_SCOPE)
endfunction()

# A file's text, with the two characters that would make CMake reinterpret it removed. The
# check is for an exact identifier, and neither `;` nor `\` can occur inside one.
function(zen_pkg_read path out)
    file(READ "${path}" content)
    string(REPLACE ";" "" content "${content}")
    string(REPLACE "\\" "" content "${content}")
    set(${out} "${content}" PARENT_SCOPE)
endfunction()

# ---- the self-test (VM-CHECK-01), before any answer ------------------------------------
zen_pkg_count("a line naming ${ZEN_PKG_SENTINEL} in it" "${ZEN_PKG_SENTINEL}" selftest_yes)
if(NOT selftest_yes EQUAL 1)
    message(FATAL_ERROR
        "package-vocabulary: SELF-TEST FAILED -- the predicate did not find a token that is "
        "plainly present. It cannot be trusted to report a token that is absent.")
endif()
zen_pkg_count("a line with no package variable in it at all" "ZENGINE_WEAVE_DIR" selftest_no)
if(NOT selftest_no EQUAL 0)
    message(FATAL_ERROR
        "package-vocabulary: SELF-TEST FAILED -- the predicate found a token that is plainly "
        "absent, so every 'not found' below would be meaningless.")
endif()

# ---- the sweep --------------------------------------------------------------------------
# Two globs, for the reason doc_links records: root files from a plain glob, since
# `file(GLOB_RECURSE)` on a root-level expression walks the whole repository once per pattern;
# everything else from recursive globs over the surviving top-level directories.
set(root_globs "")
foreach(g IN LISTS ZEN_PKG_GLOBS)
    list(APPEND root_globs "${ZEN_REPO}/${g}")
endforeach()
file(GLOB root_files RELATIVE "${ZEN_REPO}" ${root_globs})

file(GLOB top_entries RELATIVE "${ZEN_REPO}" "${ZEN_REPO}/*")
set(nested_globs "")
set(pruned "")
foreach(entry IN LISTS top_entries)
    if(IS_DIRECTORY "${ZEN_REPO}/${entry}")
        zen_pkg_excluded("${entry}/" skip)
        if(skip)
            list(APPEND pruned "${entry}")
        else()
            foreach(g IN LISTS ZEN_PKG_GLOBS)
                list(APPEND nested_globs "${ZEN_REPO}/${entry}/${g}")
            endforeach()
        endif()
    endif()
endforeach()

set(nested_files "")
if(nested_globs)
    file(GLOB_RECURSE nested_files RELATIVE "${ZEN_REPO}" ${nested_globs})
endif()

set(files "")
set(excluded 0)
foreach(rel IN LISTS root_files nested_files)
    zen_pkg_excluded("${rel}" skip)
    if(skip)
        math(EXPR excluded "${excluded} + 1")
    else()
        list(APPEND files "${rel}")
    endif()
endforeach()
list(REMOVE_DUPLICATES files)
list(LENGTH files file_count)

# A sweep that found nothing is not a clean repository, it is a broken sweep. The floor is
# deliberately far below the real count -- it is a smoke test for the glob, not a population.
if(file_count LESS 100)
    message(FATAL_ERROR
        "package-vocabulary: the sweep found only ${file_count} files under ${ZEN_REPO}. That "
        "is not this repository, so a clean result would mean nothing.")
endif()

# ---- the answer -------------------------------------------------------------------------
#
# This script's own repository-relative path, computed rather than written, so renaming the
# file cannot silently turn the exception into a hole somewhere else.
file(RELATIVE_PATH zen_pkg_self "${ZEN_REPO}" "${CMAKE_CURRENT_LIST_FILE}")
set(self_seen 0)

set(offences "")
set(sentinel_seen 0)
foreach(rel IN LISTS files)
    zen_pkg_read("${ZEN_REPO}/${rel}" text)

    zen_pkg_count("${text}" "${ZEN_PKG_SENTINEL}" seen)
    if(seen GREATER 0)
        math(EXPR sentinel_seen "${sentinel_seen} + 1")
    endif()

    set(i 0)
    list(LENGTH ZEN_PKG_RETIRED retired_len)
    while(i LESS retired_len)
        math(EXPR j "${i} + 1")
        list(GET ZEN_PKG_RETIRED ${i} dead)
        list(GET ZEN_PKG_RETIRED ${j} live)
        # The retired names are prefixes of nothing and suffixes of nothing here, but
        # ZENGINE_WEAVES would also match inside ZENGINE_WEAVES_SOMETHING; the count is of the
        # bare identifier, which is what a config would actually write.
        zen_pkg_count("${text}" "${dead}" n)
        if(rel STREQUAL zen_pkg_self)
            # The declaration itself. It must be here -- see below.
            if(n GREATER 0)
                math(EXPR self_seen "${self_seen} + 1")
            endif()
        elseif(n GREATER 0)
            list(APPEND offences "${rel}: ${dead} (${n}x) -- use ${live}")
        endif()
        math(EXPR i "${i} + 2")
    endwhile()
endforeach()

# The exception, verified. Every retired spelling must be present in this file; if one is not,
# the list has lost an entry and this check would pass over a tree that still writes it.
list(LENGTH ZEN_PKG_RETIRED retired_len)
math(EXPR retired_pairs "${retired_len} / 2")
if(NOT self_seen EQUAL retired_pairs)
    message(FATAL_ERROR
        "package-vocabulary: this script declares ${retired_pairs} retired spellings but only "
        "${self_seen} of them appear in ${zen_pkg_self} itself. The declaration and the search "
        "have come apart, so a clean result would be vacuous.")
endif()

# The sentinel must be found in the tree, not merely in a string this script wrote. If the
# current public variable appears nowhere, the sweep is looking at the wrong files and the
# absence of the retired ones proves nothing.
if(sentinel_seen EQUAL 0)
    message(FATAL_ERROR
        "package-vocabulary: ${ZEN_PKG_SENTINEL} appears in none of the ${file_count} files "
        "swept. The current package vocabulary is missing, or this sweep is not reading this "
        "repository -- either way a clean result would be a false green.")
endif()

if(offences)
    string(REPLACE ";" "\n  " pretty "${offences}")
    message(FATAL_ERROR
        "package-vocabulary: retired public package variables are still written here.\n\n"
        "  ${pretty}\n\n"
        "An ARTIFACT is the physical loadable file. A WEAVE and a PROVIDER are runtime "
        "surfaces an artifact may expose -- and this package installs one of each, so a "
        "variable named after a surface is false of its own contents. Use the "
        "artifact spelling; the concepts `zengine_weave()`, WeaveId and the weave ABI are not "
        "affected and must not be renamed.")
endif()

list(LENGTH pruned pruned_count)
message(STATUS
    "package-vocabulary: PASSED -- ${file_count} files swept (${excluded} excluded, "
    "${pruned_count} directories pruned), ${sentinel_seen} naming ${ZEN_PKG_SENTINEL}, "
    "${retired_pairs} retired spellings declared and found only in ${zen_pkg_self}")
