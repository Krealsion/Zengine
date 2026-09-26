# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
#
# The compile-judged lane's targets, entered from the top-level CMakeLists.txt under
# ZENGINE_COMPILE_FIXTURES, which tests/CMakeLists.txt turns on for the fixtures' own tree and
# nothing turns on for the main one (VM-LANE-17, VM-LANE-18): the same project, so the same
# warnings, sanitizer flags and vocabularies. EXCLUDE_FROM_ALL, since most must fail: each is
# built by name, by the one test that judges it.

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
