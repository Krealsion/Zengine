# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
#
# The text checks and the documentation lane's reach (AGENTS.md, the documentation lane). Each
# check is `tests/check_<name>.cmake`, run as `cmake -DZEN_REPO=<root> -P` by CTest and the lane
# alike; tests/CMakeLists.txt registers them from this list, so the two cannot drift.
set(ZEN_TEXT_CHECKS doc_links package_vocabulary law_register source_comments code_values folder_map
    manual)

# Besides Markdown and the images under a docs/ folder, what a documentation-only change may
# touch: the text checks themselves and the files only they read.
set(ZEN_TEXT_ONLY_FILES tests/code_values.txt tests/folders.txt)

# The folders whose first-party source doc_links and source_comments read: each row of
# tests/folders.txt whose kind holds code a build compiles or a lane runs, and the root
# CMakeLists.txt beside them. An empty answer is refused: it would read nothing and pass.
set(ZEN_SOURCE_KINDS feature host library examples build tests)
function(zen_source_roots repo out)
    file(STRINGS "${repo}/tests/folders.txt" rows REGEX "^[^#]")
    set(roots "")
    foreach(row IN LISTS rows)
        if(row MATCHES "^([^ \t]+)[ \t]+([a-z]+)[ \t]*$" AND CMAKE_MATCH_2 IN_LIST ZEN_SOURCE_KINDS)
            list(APPEND roots "${CMAKE_MATCH_1}")
        endif()
    endforeach()
    if(roots STREQUAL "")
        message(FATAL_ERROR "text checks: tests/folders.txt names no folder of a kind holding code "
                            "(${ZEN_SOURCE_KINDS}), so no source would be read")
    endif()
    set(${out} CMakeLists.txt ${roots} PARENT_SCOPE)
endfunction()

# Markdown a compiled test reads, so a change to it is the official lane's: the hotkeys guide's
# keymap example, applied by the document suite as a weaver copies it; and the page a test image of
# the Builder pane carries, which the panes suite reloads in place.
set(ZEN_COMPILED_READS_DOCS workshop/docs/hotkeys.md tests/weavelib/reworded-builder-pane.md)

# ...and every manual, by its name and place rather than a list: a page `<folder>/docs/<office>.md`
# is compiled into the target that carries it (cmake/ZengineManual.cmake), so an edit to one builds
# that target, and tests/check_manual.cmake refuses a manual no target carries.
set(ZEN_COMPILED_READS_MANUALS "(^|/)docs/[a-z0-9-]+([.][a-z0-9-]+)+[.]md$")

# A C/C++ change qualifies not even when only its comments change: compiled tests read source as
# text (the Editor's source law over each pane's entry file, the tripwires over workshop/), and a
# character in a comment can turn one of them red.
set(ZEN_COMMENT_ONLY_QUALIFIES OFF)
