# The Zengine suites. Same gate as the packages they prove: no hosting Loom,
# no snake/input/surface tests (the smoke test remains the portable floor).
#
# third_party/doctest.h is the same single-header doctest the Loom's suite
# vendors (MIT; https://github.com/doctest/doctest) — copied, not shared, so
# this repo builds as the stranger it is.
#
# THIS DIRECTORY REFUSES A LOOM IT CANNOT TEST AGAINST (R2F-D / POP-03, closing COLD-1 F-25).
#
# It used to say STATUS + return() here, which meant: testing was requested, the entire test
# tree quietly vanished, configure reported success, and `ctest` printed "100% tests passed"
# over the single surviving smoke test. The audit reached that state through the SUPPORTED
# DEFAULT Windows Loom package, so it was not a typo's fault and nothing would ever have
# noticed. An unsupported testability is now declared, not disguised.
if(NOT TARGET loom::kernel)
    message(FATAL_ERROR
"zengine: testing is enabled, but this Loom package cannot host loadable weaves.\n"
"WHAT IS MISSING\n"
"  The installed/consumed Loom does not export loom::kernel, so nothing here can load a\n"
"  weave. Every Zengine suite but the smoke test drives real weave libraries through the\n"
"  real kernel: snake, timer (plus its four compile-negative cases), input, surface and\n"
"  the audit probes. None of them can be built.\n"
"WHY THIS IS AN ERROR AND NOT A SKIP\n"
"  Configuring anyway would leave one CTest entry standing in for the whole suite, and\n"
"  `ctest` would then report '100% tests passed' over a test project whose tests had been\n"
"  deleted at configure time. A green that names a population which never existed is worse\n"
"  than no run at all, so the configuration is refused instead of quietly emptied.\n"
"HOW TO GET A LOOM THAT CAN HOST WEAVES\n"
"  Linux : the kernel is always built and exported. Install a normal Loom build and point\n"
"          CMAKE_PREFIX_PATH at the prefix.\n"
"  Windows: the kernel is the opt-in development/demo backend and carries NO isolation.\n"
"          Build and install the Loom with -DLOOM_ENABLE_WINDOWS_KERNEL=ON, or configure\n"
"          Zengine with -DZEN_LOOM_DEV=ON, which turns it on for you.\n"
"IF YOU ONLY WANT THE LIBRARY\n"
"  Configure with -DBUILD_TESTING=OFF. Zengine's kernel-independent surface (the activation\n"
"  cursor and the timer/input/surface vocabularies) builds fine against a kernel-less Loom.\n"
"  The kernel is a requirement of the TESTS, not of the package.")
endif()

# ---- the suite's own weave libraries (R2B-0) ------------------------------------------
#
# Fixtures, built exactly the way every other weave in this repo is (zengine_weave, so
# the Loom's reloadable-weave contract reaches them too), and loaded through the REAL
# kernel.
#
#   zengine-timer-virtual   the shipped TimerServiceT over a clock whose nap advances
#                           virtual time instead of sleeping. The continuity proof is a
#                           claim about DURATIONS, and proving it on a wall clock would
#                           mean sleeping five real seconds to learn less.
#   zengine-probe-oneshot   an ordinary binding consumer: one declared five-second
#                           one-shot, the binding API's DEFAULT order.
#   zengine-probe-required  the same source, one flag apart: preservation REQUIRED, no
#                           fallback. One source, two libraries — the world weavelib
#                           pattern, for the same reason (the difference under test is
#                           one declaration, so everything else must be identical).
#   zengine-timer-virtual-v2  the same service, a second REAL library — the prepared
#                             candidate of the R2B-3c keystone. One source, because the
#                             artifact that prepares must be the artifact that goes live.
#   zengine-timer-declines    a real artifact that loads, seals and is asked, and then
#                             refuses to become the Timer. The negative proof of R2B-3.
zengine_weave(zengine-timer-virtual weavelib/timer_virtual.cpp)
target_link_libraries(zengine-timer-virtual PRIVATE zengine-timer-vocabulary zengine-activation)

zengine_weave(zengine-timer-virtual-v2 weavelib/timer_virtual.cpp)
target_link_libraries(zengine-timer-virtual-v2 PRIVATE zengine-timer-vocabulary zengine-activation)

zengine_weave(zengine-timer-declines weavelib/timer_virtual.cpp)
target_link_libraries(zengine-timer-declines PRIVATE zengine-timer-vocabulary zengine-activation)
target_compile_definitions(zengine-timer-declines PRIVATE ZENGINE_TIMER_DECLINES=1)

zengine_weave(zengine-probe-oneshot weavelib/oneshot_probe.cpp)
target_link_libraries(zengine-probe-oneshot PRIVATE zengine-timer-vocabulary zengine-activation)
target_include_directories(zengine-probe-oneshot PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/weavelib)

zengine_weave(zengine-probe-required weavelib/oneshot_probe.cpp)
target_link_libraries(zengine-probe-required PRIVATE zengine-timer-vocabulary zengine-activation)
target_include_directories(zengine-probe-required PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/weavelib)
target_compile_definitions(zengine-probe-required PRIVATE PROBE_REQUIRE_PRESERVATION=1)

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
    "HOST_PLAY_CPP=\"${CMAKE_SOURCE_DIR}/snake/play.cpp\""
    # The clock adapter's SOURCE, for the binding tripwire (R2A-3). Hand-written
    # ceremony and the binding are behaviourally identical, so only reading the
    # file can tell which one is there. Defense in depth, same as above.
    "CLOCK_CPP=\"${CMAKE_SOURCE_DIR}/snake/clock.cpp\"")

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
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
    ${CMAKE_CURRENT_SOURCE_DIR}/weavelib)

target_compile_definitions(zengine-timer-tests PRIVATE
    "TIMER_SO=\"$<TARGET_FILE:zengine-timer>\""
    "SNAKE_CLOCK_SO=\"$<TARGET_FILE:snake-clock>\""
    "WORLD_V1_SO=\"$<TARGET_FILE:snake-world-v1>\""
    "INPUT_SO=\"$<TARGET_FILE:zengine-input>\""
    "SKIN_SO_TUI_CLASSIC=\"$<TARGET_FILE:zengine-skin-tui-classic>\""
    "SKIN_SO_TUI_BLOCK=\"$<TARGET_FILE:zengine-skin-tui-block>\""
    # The R2B-0 continuity lane: virtual time, real everything else.
    "TIMER_VIRTUAL_SO=\"$<TARGET_FILE:zengine-timer-virtual>\""
    "PROBE_ONESHOT_SO=\"$<TARGET_FILE:zengine-probe-oneshot>\""
    "PROBE_REQUIRED_SO=\"$<TARGET_FILE:zengine-probe-required>\""
    # The R2B-3c keystone: the prepared candidate, and the one that refuses.
    "TIMER_VIRTUAL_V2_SO=\"$<TARGET_FILE:zengine-timer-virtual-v2>\""
    "TIMER_DECLINES_SO=\"$<TARGET_FILE:zengine-timer-declines>\"")

add_test(NAME timer COMMAND zengine-timer-tests)

# ---- The compile-negative lane (owned by the Timer suite) --------------------
#
# The activation wall is a COMPILE-TIME refusal, so a runtime suite cannot prove
# it: the whole point is that the offending weave never becomes an artifact. The
# smallest honest proof is to try to build it and read the diagnostic.
#
# EXCLUDE_FROM_ALL, so the ordinary build never trips over the case that must
# fail; each test drives `cmake --build --target` itself. And each asserts the
# DIAGNOSTIC, not merely the exit code — a build that broke for an unrelated
# reason must not read as evidence. PASS_REGULAR_EXPRESSION is what does that:
# CTest then judges the output, so a target that started compiling cleanly would
# fail the test for producing no diagnostic at all.
#
# The positive control is the same source with the collision replaced by the
# supported hook, and it must BUILD — without it, "the build refused" would be
# satisfied by a fixture that never compiled for any reason.
set(ZENGINE_CN_NAMES collision hook missing-using cursor)
set(ZENGINE_CN_IDS 1 2 3 4)
foreach(case_name case_id IN ZIP_LISTS ZENGINE_CN_NAMES ZENGINE_CN_IDS)
    add_executable(zengine-cn-${case_name} EXCLUDE_FROM_ALL
        compile_negative/activation_collision.cpp)
    target_link_libraries(zengine-cn-${case_name}
        PRIVATE loom::switchboard loom::core zengine-timer-vocabulary zengine-warnings)
    target_include_directories(zengine-cn-${case_name} PRIVATE ${CMAKE_SOURCE_DIR})
    target_compile_definitions(zengine-cn-${case_name} PRIVATE ZENGINE_CN_CASE=${case_id})
endforeach()

add_test(NAME timer_raw_activation_refused
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target zengine-cn-collision)
set_tests_properties(timer_raw_activation_refused PROPERTIES
    PASS_REGULAR_EXPRESSION "on_timed_activation")

add_test(NAME timer_activation_hook_compiles
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target zengine-cn-hook)

add_test(NAME timer_missing_using_refused
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target zengine-cn-missing-using)
set_tests_properties(timer_missing_using_refused PROPERTIES
    PASS_REGULAR_EXPRESSION "using TimedWeave::on")

# The hook runs INSIDE an accepted activation, so it must not be able to accept
# again. `activation()` hands back a const& and `accept` is non-const, so the
# attempt cannot compile — and this is what keeps that true, since a mutation
# widening the accessor to a mutable reference is otherwise invisible: no
# runtime case can produce an input the language refuses to express.
add_test(NAME timer_cursor_bypass_refused
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target zengine-cn-cursor)
set_tests_properties(timer_cursor_bypass_refused PROPERTIES
    PASS_REGULAR_EXPRESSION "discards qualifiers")

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
