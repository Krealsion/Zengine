# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
#
# The text checks and the documentation lane's reach (AGENTS.md, the documentation lane). Each
# check is `tests/check_<name>.cmake`, run as `cmake -DZEN_REPO=<root> -P` by CTest and the lane
# alike; tests/CMakeLists.txt registers them from this list, so the two cannot drift.
set(ZEN_TEXT_CHECKS doc_links package_vocabulary law_register source_comments code_values)

# Besides Markdown and the images under docs/, what a documentation-only change may touch: the
# text checks themselves and the files only they read.
set(ZEN_TEXT_ONLY_FILES tests/code_values.txt)

# Markdown a compiled test reads, so a change to it is the official lane's: the hotkeys guide's
# keymap example, applied by the document suite as a weaver copies it.
set(ZEN_COMPILED_READS_DOCS docs/workshop/hotkeys.md)

# A C/C++ change qualifies not even when only its comments change: compiled tests read source as
# text (the Editor's source law over each pane's entry file, the tripwires over workshop/), and a
# character in a comment can turn one of them red.
set(ZEN_COMMENT_ONLY_QUALIFIES OFF)
