# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
#
# THE COMPILE-JUDGED LANE'S OWN TARGETS, AND THEIR OWN BUILD TREE.
#
# A compile test's evidence is a build: it drives `cmake --build <tree> --target <t>` and
# reads what the compiler said. That makes it a WRITER of whichever build tree it names, and
# for as long as these targets lived in the main tree, that tree was `${CMAKE_BINARY_DIR}` --
# so eight independent proofs each held a claim on the one build tree everything else in the
# repository also depends on. Serially nobody noticed. Under `ctest -j`, with a CMake
# regeneration owed, all eight raced it: each nested build re-checked the globbed directories,
# re-ran CMake, and re-entered the FetchContent sub-builds, and the losers reported
# `configure_file` errors and `ninja: error: rebuilding 'build.ninja': subcommand failed`
# instead of the diagnostic they exist to judge.
#
# So the fixtures live here, in a directory of their own, and are configured into a build tree
# of their own. Nothing else is built there and nothing else writes there; the main build tree
# is left to its one rightful owner, whoever runs `cmake --build` on it.
#
# THIS FILE IS ENTERED TWICE OVER, IN TWO DIFFERENT TREES, AND ONLY EVER ADDS TARGETS. It is
# reached from the top-level `CMakeLists.txt` under `ZENGINE_COMPILE_FIXTURES`, which
# `tests/CMakeLists.txt` turns on for the fixture tree and nothing turns on for the main one.
# The fixture tree is the same project configured with its dependencies and its suites switched
# off -- which is what makes it cheap (measured at 1.1 s), what makes it incapable of populating
# a dependency, and, most of the point, what means the warnings, the sanitizer flags and the
# package vocabularies below are the SAME definitions the rest of the repository compiles
# against rather than a second copy of them that could drift.
#
# EXCLUDE_FROM_ALL SURVIVES THE MOVE, and still for its original reason: most of these are
# supposed to fail, so an ordinary `cmake --build` of the fixture tree must not trip over them
# either. Each is built by name, by the one test that judges it.

# ---- The activation wall (owned by the Timer suite) -------------------------------------
#
# ONE SOURCE, FIVE TARGETS: four refusals and the positive control that keeps "the build
# refused" from being satisfied by a fixture that never compiled. `compile_negative/
# activation_collision.cpp` says at length which case is which.
set(ZENGINE_CN_NAMES collision hook missing-using cursor missing-using-no-binding)
set(ZENGINE_CN_IDS 1 2 3 4 5)
foreach(case_name case_id IN ZIP_LISTS ZENGINE_CN_NAMES ZENGINE_CN_IDS)
    add_executable(zengine-cn-${case_name} EXCLUDE_FROM_ALL
        activation_collision.cpp)
    target_link_libraries(zengine-cn-${case_name}
        PRIVATE loom::switchboard loom::core zengine-timer-vocabulary zengine-warnings
                zengine-sanitize)
    target_include_directories(zengine-cn-${case_name} PRIVATE ${CMAKE_SOURCE_DIR})
    target_compile_definitions(zengine-cn-${case_name} PRIVATE ZENGINE_CN_CASE=${case_id})
endforeach()

# ---- The authored/resolved fence (owned by the UI suite) --------------------------------
#
# The Timer pattern, pointed at a different wall: two refusals and their control, from one
# source (`compile_negative/ui_fence.cpp`) with the violation compiled in or out.
set(ZENGINE_UI_CN_NAMES extent resolved ok)
set(ZENGINE_UI_CN_IDS 1 2 3)
foreach(case_name case_id IN ZIP_LISTS ZENGINE_UI_CN_NAMES ZENGINE_UI_CN_IDS)
    add_executable(zengine-cn-ui-${case_name} EXCLUDE_FROM_ALL
        ui_fence.cpp)
    target_link_libraries(zengine-cn-ui-${case_name}
        PRIVATE loom::core zengine-ui-vocabulary zengine-warnings zengine-sanitize)
    target_include_directories(zengine-cn-ui-${case_name} PRIVATE ${CMAKE_SOURCE_DIR})
    target_compile_definitions(zengine-cn-ui-${case_name} PRIVATE ZENGINE_CN_CASE=${case_id})
endforeach()
