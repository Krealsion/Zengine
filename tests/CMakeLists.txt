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

#   zengine-workshop-hello  WP-0's external witness: a real dynamic weave that offers
#                           Workshop one read-only pane under its own Loom office. It is a
#                           FIXTURE and not a product -- no host boots it and Workshop ships
#                           two panes, neither of which is this. It goes through
#                           `zengine_weave` for the reason every other one does, and it
#                           carries `zengine-warnings` and `zengine-sanitize` so the
#                           standing ASan+UBSan lane sees the new target rather than
#                           silently omitting it (AGENTS.md: that is the one way this lane
#                           degrades).
zengine_weave(zengine-workshop-hello weavelib/workshop_hello.cpp)
target_link_libraries(zengine-workshop-hello
    PRIVATE zengine-workshop-vocabulary zengine-surface-vocabulary zengine-activation
            zengine-warnings zengine-sanitize)

# doctest_main.cpp rides every runtime suite: one shared main() that refuses a run which
# selected zero test cases (POP-01 / COLD-2 C-4). Stock doctest calls that SUCCESS.
add_executable(zengine-tests test_snake.cpp doctest_main.cpp)
target_link_libraries(zengine-tests
    PRIVATE loom::kernel loom::switchboard loom::core zengine-surface-vocabulary
            zengine-warnings zengine-sanitize)
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

zengine_doctest_test(snake zengine-tests)

# The Timer suite: time message-shaped — the contract pins, every schedule
# over a fake clock (deterministic, no wall waits), the real .so re-winding
# itself on the real clock, and the migration chains: the world ticking, the
# input weave polling, and the skin servicing its medium with nobody pumping
# them (the three obligations the host used to carry).
add_executable(zengine-timer-tests test_timer.cpp doctest_main.cpp)
target_link_libraries(zengine-timer-tests
    PRIVATE loom::kernel loom::switchboard loom::core zengine-timer-vocabulary
            zengine-input-vocabulary zengine-surface-vocabulary zengine-warnings
            zengine-sanitize)
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

zengine_doctest_test(timer zengine-timer-tests)

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
        PRIVATE loom::switchboard loom::core zengine-timer-vocabulary zengine-warnings
                zengine-sanitize)
    target_include_directories(zengine-cn-${case_name} PRIVATE ${CMAKE_SOURCE_DIR})
    target_compile_definitions(zengine-cn-${case_name} PRIVATE ZENGINE_CN_CASE=${case_id})
endforeach()

# zengine_compile_test() is the single registration path for this lane: it adds the entry,
# attaches the PASS_REGULAR_EXPRESSION that does the judging, and records in the build tree
# that this entry is a diagnostic-judged one. tests/test_population.txt then requires
# exactly that -- so a case whose diagnostic quietly went away is a red, not a test that
# has become "the compiler returned non-zero, therefore PASS" (C4).
zengine_compile_test(timer_raw_activation_refused zengine-cn-collision "on_timed_activation")

zengine_compile_test(timer_activation_hook_compiles zengine-cn-hook)

# TWO SPELLINGS OF ONE REFUSAL, because the compilers genuinely refuse it differently
# (MSVC-0). GCC reaches the authored static_assert in timer/binding.hpp and prints the
# sentence that tells a maker what to add. MSVC does NOT: its requires-expression accepts
# `s.on(f, m)` there, so the guard passes and the hidden handler surfaces one layer later
# as the raw conversion failure instead. The WALL holds identically on both -- the weave
# cannot be built either way, which is the property under test -- but the GUIDANCE is
# GCC-only, and that difference is recorded as pressure rather than smoothed over here.
# Each alternative is specific enough to disappear if the wall ever stopped working.
zengine_compile_test(timer_missing_using_refused zengine-cn-missing-using
                     "using TimedWeave::on|cannot convert argument 1 from 'const zengine::timer::TimerFired'")

# The hook runs INSIDE an accepted activation, so it must not be able to accept
# again. `activation()` hands back a const& and `accept` is non-const, so the
# attempt cannot compile — and this is what keeps that true, since a mutation
# widening the accessor to a mutable reference is otherwise invisible: no
# runtime case can produce an input the language refuses to express.
# GCC says "discards qualifiers", MSVC says "Conversion loses qualifiers" -- the same
# const-correctness refusal, two house styles. Both compilers DO reach it (unlike the
# case above), so one pattern covers them without weakening the claim.
zengine_compile_test(timer_cursor_bypass_refused zengine-cn-cursor
                     "(discards|loses) qualifiers")

# The Input suite: its own executable so each package's green stays its own
# count (the snake suite's numbers must not move when input's do). It speaks
# the timer vocabulary now: the weave's own beat is part of its contract.
add_executable(zengine-input-tests test_input.cpp doctest_main.cpp)
target_link_libraries(zengine-input-tests
    PRIVATE loom::kernel loom::switchboard loom::core zengine-input-vocabulary
            zengine-timer-vocabulary zengine-warnings zengine-sanitize)
target_include_directories(zengine-input-tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
    ${CMAKE_SOURCE_DIR}/snake)

target_compile_definitions(zengine-input-tests PRIVATE
    "INPUT_SO=\"$<TARGET_FILE:zengine-input>\""
    "CONTROLS_SO=\"$<TARGET_FILE:snake-controls>\""
    "WORLD_V1_SO=\"$<TARGET_FILE:snake-world-v1>\"")

zengine_doctest_test(input zengine-input-tests)

# THE SDL READER IS A CONDITIONAL SUBPOPULATION, declared the same way the SDL skin's is.
#
# The SDL TRANSLATION is pure and lives in the always rows -- the Windows stranger lane
# builds no SDL and still pins every key, modifier, coordinate and close mapping. What
# rides this gate is the one case that cannot be pure: the test process pushes events into
# a REAL SDL queue and a separately dlopen'ed zengine-input-sdl.so polls them out, which is
# the positive evidence that the two SDL-backed weaves share ONE SDL (a static SDL would
# give each of them their own queue and this case would see nothing). It therefore links
# SDL itself, which is the whole trick.
if(TARGET zengine-input-sdl)
    target_compile_definitions(zengine-input-tests PRIVATE
        INPUT_HAS_SDL=1
        "INPUT_SDL_SO=\"$<TARGET_FILE:zengine-input-sdl>\""
        # The window Skin too: one of the cases runs BOTH SDL weaves at once and
        # pumps the Skin between a push and a read, which is the interleaving that
        # would prove the queue has two owners if it had.
        "SKIN_SO_SDL=\"$<TARGET_FILE:zengine-skin-sdl>\"")
    target_link_libraries(zengine-input-tests PRIVATE zengine-surface-vocabulary
                                                      ${ZENGINE_SDL_LIB})
    set_tests_properties(input PROPERTIES
        ENVIRONMENT "SDL_VIDEO_DRIVER=dummy;SDL_VIDEODRIVER=dummy")
    set_property(GLOBAL APPEND PROPERTY ZENGINE_ACTIVE_GATES "sdl")
endif()

# The Surface suite: the skin pattern — golden terminal bytes, the pure SDL
# plan (pinned on every lane, SDL built or not), the hello handshake, surface
# ownership, and the linkage pins riding the skin pair. Where the SDL skin is
# built, the same intent drives it under SDL's dummy video driver (set both
# here and in the process, belt to braces).
add_executable(zengine-surface-tests test_surface.cpp doctest_main.cpp)
target_link_libraries(zengine-surface-tests
    PRIVATE loom::kernel loom::switchboard loom::core zengine-surface-vocabulary
            zengine-timer-vocabulary zengine-warnings zengine-sanitize)
target_include_directories(zengine-surface-tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party)

target_compile_definitions(zengine-surface-tests PRIVATE
    "WORLD_V1_SO=\"$<TARGET_FILE:snake-world-v1>\""
    "SCORE_SO=\"$<TARGET_FILE:snake-score>\""
    "SKIN_SO_TUI_CLASSIC=\"$<TARGET_FILE:zengine-skin-tui-classic>\""
    "SKIN_SO_TUI_BLOCK=\"$<TARGET_FILE:zengine-skin-tui-block>\"")
zengine_doctest_test(surface zengine-surface-tests)

# THE SDL SKIN IS A CONDITIONAL SUBPOPULATION, AND IT IS DECLARED, NOT ABSORBED.
#
# Cases in test_surface.cpp live behind `#if defined(SURFACE_HAS_SDL)`, so this suite is
# smaller where the SDL skin was not built -- and the Windows stranger lane builds it with
# -DZENGINE_SDL_SKIN=OFF, so both numbers are real. One floor everywhere would have bought
# that portability by giving the Linux lane that many cases of downward slack; the `sdl` gate
# states the difference instead, and tests/test_population.txt sums the rows whose gates are
# active. The two values are written there, once, next to the rows that produce them.
#
# ONE `if`, THREE CONSEQUENCES, on purpose. The compile definition that admits the case, the
# environment the case needs, and the gate the floor is derived from all hang off the same
# condition, so they cannot come to disagree about whether SDL is here. C3 paid for the
# general form of this: moving a check's gate from a name to a derivation moves the
# blindness onto the derivation -- a second, independent way of asking "is SDL built?" would
# be a second thing that can be wrong, and the way it would be wrong is silently downward.
if(TARGET zengine-skin-sdl)
    target_compile_definitions(zengine-surface-tests PRIVATE
        SURFACE_HAS_SDL=1
        "SKIN_SO_SDL=\"$<TARGET_FILE:zengine-skin-sdl>\"")
    # THE SUITE REACHES THE SAME SDL THE SKIN DOES, and that is a measurement rather than a
    # convenience. SDL's event queue is process-global with exactly one owner, and the whole
    # reason cmake/ZengineSdl.cmake fetches a SHARED SDL3 is that a weave loaded with
    # RTLD_LOCAL and an archived copy would be talking to a different queue. Linking the suite
    # to the same shared library is what lets a case put events on THE queue and then ask
    # whether the skin took any -- which cannot be asked from outside SDL at all.
    target_link_libraries(zengine-surface-tests PRIVATE ${ZENGINE_SDL_LIB})
    set_tests_properties(surface PROPERTIES
        ENVIRONMENT "SDL_VIDEO_DRIVER=dummy;SDL_VIDEODRIVER=dummy")
    set_property(GLOBAL APPEND PROPERTY ZENGINE_ACTIVE_GATES "sdl")
endif()

# The UI suite: the authored/resolved distinction, on its own, with no
# application around it. The shapes' declared schemas, the compile-time fence
# shown to DISCRIMINATE (not merely to be true of Element), resolution as a
# function of a viewport — including for values no setter would have accepted,
# because authored content arrives from the wire and from a poke — and hit
# testing answering with the authored identity.
#
# Its own executable, so this package's green stays its own count: the Workshop
# suite's numbers must not move when the vocabulary's do, which is the whole
# reason the vocabulary is no longer inside Workshop.
#
# It needs no kernel and no weave library. This package is vocabulary and
# arithmetic; loom::core is here for zen/weave/shape.hpp and the schema
# derivation the contract case checks.
add_executable(zengine-ui-tests test_ui.cpp doctest_main.cpp)
target_link_libraries(zengine-ui-tests
    PRIVATE loom::core zengine-ui-vocabulary zengine-warnings zengine-sanitize)
target_include_directories(zengine-ui-tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party)

zengine_doctest_test(ui zengine-ui-tests)

# The Component suite: the first foundational component, on its own.
#
# It links  and NOTHING ELSE -- not even loom::core, which every other
# suite here needs for zen/weave/shape.hpp. That is the claim rather than an economy: a
# TextBox is a std::string and two indices, it has no wire form, nothing serializes it and
# nothing hosts it, so a suite that had to boot anything to test it would be testing
# something other than a component.
#
# Its own executable for the same reason the UI suite has one: this package's green stays its
# own count, so the Workshop suite's numbers cannot move when the component's do. HD-5 moved
# four cases here out of Workshop -- what a TextBox DOES is this suite's claim, and that the
# Terminal's command line and a property draft get their answers from it is Workshop's.
add_executable(zengine-component-tests test_component.cpp doctest_main.cpp)
target_link_libraries(zengine-component-tests
    PRIVATE zengine-component zengine-warnings zengine-sanitize)
target_include_directories(zengine-component-tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party)

zengine_doctest_test(component zengine-component-tests)

# ---- The fence's compile-negative lane (owned by the UI suite) ---------------
#
# The authored/resolved fence is a COMPILE-TIME refusal, so a runtime suite
# cannot prove it fires: the whole point is that the offending type never
# becomes an object. The smallest honest proof is to try to build one and read
# the diagnostic — the Timer suite's pattern, pointed at a different wall.
#
# EXCLUDE_FROM_ALL, so the ordinary build never trips over the cases that must
# fail; each test drives `cmake --build --target` itself, and each asserts the
# DIAGNOSTIC rather than the exit code. The positive control is the same source
# with the violation removed, and it must BUILD.
set(ZENGINE_UI_CN_NAMES extent resolved ok)
set(ZENGINE_UI_CN_IDS 1 2 3)
foreach(case_name case_id IN ZIP_LISTS ZENGINE_UI_CN_NAMES ZENGINE_UI_CN_IDS)
    add_executable(zengine-cn-ui-${case_name} EXCLUDE_FROM_ALL
        compile_negative/ui_fence.cpp)
    target_link_libraries(zengine-cn-ui-${case_name}
        PRIVATE loom::core zengine-ui-vocabulary zengine-warnings zengine-sanitize)
    target_include_directories(zengine-cn-ui-${case_name} PRIVATE ${CMAKE_SOURCE_DIR})
    target_compile_definitions(zengine-cn-ui-${case_name} PRIVATE ZENGINE_CN_CASE=${case_id})
endforeach()

zengine_compile_test(ui_authored_extent_required zengine-cn-ui-extent
                     "a resolvable dimension must be an authored Extent")

zengine_compile_test(ui_resolved_geometry_refused zengine-cn-ui-resolved
                     "no resolved rectangle may live on an authored element")

zengine_compile_test(ui_authored_element_compiles zengine-cn-ui-ok)

# The Workshop suite: the maker-facing surface, proven headless. The typed
# property connection (read, commit, the two ways a commit fails), the document's
# refusals, and whole screens asserted as SurfaceCanvas values.
#
# After W-1 it also proves the INTEGRATION the relocation created: that the
# geometry a maker sees comes from the UI package and from nowhere else — the
# painted rectangle, the inspector's resolved reading and the answer to a click
# are all derived from one ui::Scene, so they cannot come to disagree. What
# resolution and hit testing DO is the ui suite's claim; that Workshop gets its
# answers from there is this one's.
#
# Workshop's OWN weave is still mounted in-process, so there is still no artifact of
# Workshop's for the kernel to load — but since WP-0 this suite DOES define a `_SO` path,
# and the comment that used to say otherwise is corrected rather than deleted. The path is
# a PROVIDER's: `zengine-workshop-hello` is a real dynamic library the suite loads through
# the real Kernel and Manager, because the claim WP-0 has to make is about the real ABI and
# load path and a mock loader would prove nothing about either.
#
# What it also needs is the surface vocabulary and the terminal rasterizer, because one case
# runs a real painted screen through the medium's own pure function.
#
# INTR-0 gave it a SECOND `_SO` path, and the two are different KINDS of artifact rather
# than two of a kind. `zengine-workshop-hello` is a fixture this directory builds, whose
# whole job is to be the smallest thing that exercises the protocol. `zengine-introspection`
# is a PRODUCT — a package beside timer/ and input/, staged and booted by the real Workshop
# host — and the suite loads the very library a maker runs. So a case here that reads its
# rows is reading the shipped tool's answer, not a stand-in's, which is the only way the
# claim "every displayed fact is true" can be tested at all.
add_executable(zengine-workshop-tests test_workshop.cpp doctest_main.cpp)
target_link_libraries(zengine-workshop-tests
    PRIVATE loom::kernel loom::switchboard loom::core loom::terminal
            zengine-workshop-vocabulary
            zengine-surface-vocabulary zengine-ui-vocabulary
            zengine-introspection-view zengine-warnings
            zengine-sanitize)
target_include_directories(zengine-workshop-tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
target_compile_definitions(zengine-workshop-tests PRIVATE
    "WORKSHOP_SO_HELLO=\"$<TARGET_FILE:zengine-workshop-hello>\""
    "WORKSHOP_SO_INTROSPECTION=\"$<TARGET_FILE:zengine-introspection>\"")
add_dependencies(zengine-workshop-tests zengine-workshop-hello zengine-introspection)

zengine_doctest_test(workshop zengine-workshop-tests)

# The Builder suite: a name, a command, the line between them, and the line
# between a build and the turn that asked for it. It is the one suite here that
# starts REAL CHILD PROCESSES -- success, failure, a program that is not there,
# and (ASYNC-1) one that is deliberately still running while the bus goes on
# working -- because that is the effect the package exists to bound, and a mocked
# process would prove nothing about a boundary whose whole subject is a real one.
#
# EVERY RECIPE IT RUNS IS `cmake -E ...` OR `cmake -P slow_build.cmake`, CMake's
# own portable shim and this repository's own script: no shell, no /bin/sh, no
# .bat, and no assumption about what else this machine has installed. The cmake it
# uses is THE one that configured this tree, handed over the way the Workshop
# host's recipe is handed over -- resolving `cmake` from PATH inside the test
# would be testing this machine's PATH.
#
# THE SLOW SCRIPT IS HANDED OVER BY ABSOLUTE PATH, for the same reason: a test
# that resolved it relative to its working directory would be testing where CTest
# happened to start it. `slow_build.cmake` says in its own header why a
# deliberately slow, deliberately chatty child is what an overlap claim needs.
#
# It links the Timer's vocabulary since ASYNC-1, because the runner is a
# `TimedWeave` now and the suite drives its beat the way the service would --
# through a real `TimerFired`, so what the cases exercise is the production path
# and not a second one written for them.
#
# It links no weave library and needs no kernel of its own: both Builder weaves are
# mounted in-process, exactly as Workshop's own weave is. loom::kernel rides along
# because this whole directory is gated on a Loom that can host weaves (POP-03).
add_executable(zengine-builder-tests test_builder.cpp doctest_main.cpp)
target_link_libraries(zengine-builder-tests
    PRIVATE loom::switchboard loom::core loom::history zengine-builder-vocabulary
            zengine-timer-vocabulary
            zengine-warnings zengine-sanitize)
target_include_directories(zengine-builder-tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
target_compile_definitions(zengine-builder-tests PRIVATE
    "ZENGINE_TEST_CMAKE=\"${CMAKE_COMMAND}\""
    "ZENGINE_TEST_SLOW_SCRIPT=\"${CMAKE_CURRENT_SOURCE_DIR}/slow_build.cmake\"")

zengine_doctest_test(builder zengine-builder-tests)

# The Trust-Gate liveness probes: what the substrate actually does when the
# TIMER SERVICE ITSELF is swapped / reloaded / double-wound / joined late. The
# durable deliverable of the audit phase's Stage 5, ratified onto main by R1 —
# read the suite's own header for why a file that pins UNWANTED behavior lives
# here, and what R2 is expected to flip.
add_executable(zengine-audit-probes test_audit_probes.cpp doctest_main.cpp)
target_link_libraries(zengine-audit-probes
    PRIVATE loom::kernel loom::switchboard loom::core zengine-timer-vocabulary
            zengine-warnings zengine-sanitize)
target_include_directories(zengine-audit-probes PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
    ${CMAKE_SOURCE_DIR}/snake)

target_compile_definitions(zengine-audit-probes PRIVATE
    "TIMER_SO=\"$<TARGET_FILE:zengine-timer>\""
    "SNAKE_CLOCK_SO=\"$<TARGET_FILE:snake-clock>\""
    "WORLD_V1_SO=\"$<TARGET_FILE:snake-world-v1>\"")

zengine_doctest_test(audit_probes zengine-audit-probes)

# ---- The documentation-link contract (VOLATILE-2) --------------------------------------
#
# The only entry here that interrogates the SOURCE TREE rather than a build: it asks whether
# every repo-local documentation reference this repository makes still resolves -- markdown
# links and their #anchors, and the repository-relative `docs/...md` paths written in
# first-party C/C++ comments.
#
# A CTest entry rather than a step in verify.cmake, so a bare `ctest` carries it too, and
# because the population contract is what keeps it registered: `doc_links` is a declared row
# in tests/test_population.txt, so deleting this registration is a red rather than a silently
# smaller lane. That is the composition C4 built the inventory for.
zengine_script_test(doc_links ${CMAKE_CURRENT_SOURCE_DIR}/check_doc_links.cmake)
