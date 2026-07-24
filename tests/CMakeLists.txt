# The Zengine suites. Same gate as the packages they prove: no hosting Loom,
# no snake/input tests (the smoke test remains the portable floor).
#
# third_party/doctest.h is the same single-header doctest the Loom's suite
# vendors (MIT; https://github.com/doctest/doctest) — copied, not shared, so
# this repo builds as the stranger it is.
if(NOT TARGET loom::kernel)
    message(STATUS "zengine: snake/input suites skipped — this Loom cannot host .so weaves (no loom::kernel)")
    return()
endif()

add_executable(zengine-tests test_snake.cpp)
target_link_libraries(zengine-tests
    PRIVATE loom::kernel loom::switchboard loom::core zengine-warnings)
target_include_directories(zengine-tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
    ${CMAKE_SOURCE_DIR}/snake)

# The suites drive the REAL libraries through the REAL kernel; the build hands
# them their paths the same way the Loom's own harness does.
target_compile_definitions(zengine-tests PRIVATE
    "SNAKE_SO_WORLD_V1=\"$<TARGET_FILE:snake-world-v1>\""
    "SNAKE_SO_WORLD_V2=\"$<TARGET_FILE:snake-world-v2>\""
    "SNAKE_SO_DRAWER_CLASSIC=\"$<TARGET_FILE:snake-drawer-classic>\""
    "SNAKE_SO_DRAWER_BLOCK=\"$<TARGET_FILE:snake-drawer-block>\""
    "SNAKE_SO_SCORE=\"$<TARGET_FILE:snake-score>\"")

add_test(NAME snake COMMAND zengine-tests)

# The Input suite: its own executable so each package's green stays its own
# count (the snake suite's numbers must not move when input's do).
add_executable(zengine-input-tests test_input.cpp)
target_link_libraries(zengine-input-tests
    PRIVATE loom::kernel loom::switchboard loom::core zengine-input-vocabulary zengine-warnings)
target_include_directories(zengine-input-tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
    ${CMAKE_SOURCE_DIR}/snake)

target_compile_definitions(zengine-input-tests PRIVATE
    "INPUT_SO=\"$<TARGET_FILE:zengine-input>\""
    "CONTROLS_SO=\"$<TARGET_FILE:snake-controls>\""
    "WORLD_V1_SO=\"$<TARGET_FILE:snake-world-v1>\"")

add_test(NAME input COMMAND zengine-input-tests)
