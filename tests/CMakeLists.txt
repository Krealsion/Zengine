# The Zengine suites. Same gate as the packages they prove: no hosting Loom,
# no snake/input/surface tests (the smoke test remains the portable floor).
#
# third_party/doctest.h is the same single-header doctest the Loom's suite
# vendors (MIT; https://github.com/doctest/doctest) — copied, not shared, so
# this repo builds as the stranger it is.
if(NOT TARGET loom::kernel)
    message(STATUS "zengine: snake/input/surface suites skipped — this Loom cannot host .so weaves (no loom::kernel)")
    return()
endif()

add_executable(zengine-tests test_snake.cpp)
target_link_libraries(zengine-tests
    PRIVATE loom::kernel loom::switchboard loom::core zengine-surface-vocabulary
            zengine-warnings)
target_include_directories(zengine-tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
    ${CMAKE_SOURCE_DIR}/snake)

# The suites drive the REAL libraries through the REAL kernel; the build hands
# them their paths the same way the Loom's own harness does. Since the Surface
# migration the painting scenery is the skin pair, not package-local drawers.
target_compile_definitions(zengine-tests PRIVATE
    "SNAKE_SO_WORLD_V1=\"$<TARGET_FILE:snake-world-v1>\""
    "SNAKE_SO_WORLD_V2=\"$<TARGET_FILE:snake-world-v2>\""
    "SNAKE_SO_SCORE=\"$<TARGET_FILE:snake-score>\""
    "SKIN_SO_TUI_CLASSIC=\"$<TARGET_FILE:zengine-skin-tui-classic>\""
    "SKIN_SO_TUI_BLOCK=\"$<TARGET_FILE:zengine-skin-tui-block>\""
    # The host's SOURCE, for the no-privileged-wind tripwire (R2A-2). A root
    # Drive is inert now, so its return cannot be caught by behaviour — only by
    # reading the file. Same shape as the console's geometry-name tripwire:
    # defense in depth against a claim quietly becoming false, not a proof of
    # unrepresentability.
    "HOST_PLAY_CPP=\"${CMAKE_SOURCE_DIR}/snake/play.cpp\"")

add_test(NAME snake COMMAND zengine-tests)

# The Timer suite: time message-shaped — the contract pins, every schedule
# over a fake clock (deterministic, no wall waits), the real .so re-winding
# itself on the real clock, and the migration chains: the world ticking, the
# input weave polling, and the skin servicing its medium with nobody pumping
# them (the three obligations the host used to carry).
add_executable(zengine-timer-tests test_timer.cpp)
target_link_libraries(zengine-timer-tests
    PRIVATE loom::kernel loom::switchboard loom::core zengine-timer-vocabulary
            zengine-input-vocabulary zengine-surface-vocabulary zengine-warnings)
target_include_directories(zengine-timer-tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party)

target_compile_definitions(zengine-timer-tests PRIVATE
    "TIMER_SO=\"$<TARGET_FILE:zengine-timer>\""
    "SNAKE_CLOCK_SO=\"$<TARGET_FILE:snake-clock>\""
    "WORLD_V1_SO=\"$<TARGET_FILE:snake-world-v1>\""
    "INPUT_SO=\"$<TARGET_FILE:zengine-input>\""
    "SKIN_SO_TUI_CLASSIC=\"$<TARGET_FILE:zengine-skin-tui-classic>\""
    "SKIN_SO_TUI_BLOCK=\"$<TARGET_FILE:zengine-skin-tui-block>\"")

add_test(NAME timer COMMAND zengine-timer-tests)

# The Input suite: its own executable so each package's green stays its own
# count (the snake suite's numbers must not move when input's do). It speaks
# the timer vocabulary now: the weave's own beat is part of its contract.
add_executable(zengine-input-tests test_input.cpp)
target_link_libraries(zengine-input-tests
    PRIVATE loom::kernel loom::switchboard loom::core zengine-input-vocabulary
            zengine-timer-vocabulary zengine-warnings)
target_include_directories(zengine-input-tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
    ${CMAKE_SOURCE_DIR}/snake)

target_compile_definitions(zengine-input-tests PRIVATE
    "INPUT_SO=\"$<TARGET_FILE:zengine-input>\""
    "CONTROLS_SO=\"$<TARGET_FILE:snake-controls>\""
    "WORLD_V1_SO=\"$<TARGET_FILE:snake-world-v1>\"")

add_test(NAME input COMMAND zengine-input-tests)

# The Surface suite: the skin pattern — golden terminal bytes, the pure SDL
# plan (pinned on every lane, SDL built or not), the hello handshake, surface
# ownership, and the linkage pins riding the skin pair. Where the SDL skin is
# built, the same intent drives it under SDL's dummy video driver (set both
# here and in the process, belt to braces).
add_executable(zengine-surface-tests test_surface.cpp)
target_link_libraries(zengine-surface-tests
    PRIVATE loom::kernel loom::switchboard loom::core zengine-surface-vocabulary
            zengine-timer-vocabulary zengine-warnings)
target_include_directories(zengine-surface-tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party)

target_compile_definitions(zengine-surface-tests PRIVATE
    "WORLD_V1_SO=\"$<TARGET_FILE:snake-world-v1>\""
    "SCORE_SO=\"$<TARGET_FILE:snake-score>\""
    "SKIN_SO_TUI_CLASSIC=\"$<TARGET_FILE:zengine-skin-tui-classic>\""
    "SKIN_SO_TUI_BLOCK=\"$<TARGET_FILE:zengine-skin-tui-block>\"")
if(TARGET zengine-skin-sdl)
    target_compile_definitions(zengine-surface-tests PRIVATE
        SURFACE_HAS_SDL=1
        "SKIN_SO_SDL=\"$<TARGET_FILE:zengine-skin-sdl>\"")
endif()

add_test(NAME surface COMMAND zengine-surface-tests)
if(TARGET zengine-skin-sdl)
    set_tests_properties(surface PROPERTIES
        ENVIRONMENT "SDL_VIDEO_DRIVER=dummy;SDL_VIDEODRIVER=dummy")
endif()

# The Trust-Gate liveness probes: what the substrate actually does when the
# TIMER SERVICE ITSELF is swapped / reloaded / double-wound / joined late. The
# durable deliverable of the audit phase's Stage 5, ratified onto main by R1 —
# read the suite's own header for why a file that pins UNWANTED behavior lives
# here, and what R2 is expected to flip.
add_executable(zengine-audit-probes test_audit_probes.cpp)
target_link_libraries(zengine-audit-probes
    PRIVATE loom::kernel loom::switchboard loom::core zengine-timer-vocabulary
            zengine-warnings)
target_include_directories(zengine-audit-probes PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
    ${CMAKE_SOURCE_DIR}/snake)

target_compile_definitions(zengine-audit-probes PRIVATE
    "TIMER_SO=\"$<TARGET_FILE:zengine-timer>\""
    "SNAKE_CLOCK_SO=\"$<TARGET_FILE:snake-clock>\""
    "WORLD_V1_SO=\"$<TARGET_FILE:snake-world-v1>\"")

add_test(NAME audit_probes COMMAND zengine-audit-probes)
