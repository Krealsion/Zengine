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
target_link_libraries(zengine-timer-virtual PRIVATE zengine-timer-vocabulary zengine-activation zengine-operator zengine-operator-consumer)

zengine_weave(zengine-timer-virtual-v2 weavelib/timer_virtual.cpp)
target_link_libraries(zengine-timer-virtual-v2 PRIVATE zengine-timer-vocabulary zengine-activation zengine-operator zengine-operator-consumer)

zengine_weave(zengine-timer-declines weavelib/timer_virtual.cpp)
target_link_libraries(zengine-timer-declines PRIVATE zengine-timer-vocabulary zengine-activation zengine-operator zengine-operator-consumer)
target_compile_definitions(zengine-timer-declines PRIVATE ZENGINE_TIMER_DECLINES=1)

zengine_weave(zengine-probe-oneshot weavelib/oneshot_probe.cpp)
target_link_libraries(zengine-probe-oneshot PRIVATE zengine-timer-vocabulary zengine-activation)
target_include_directories(zengine-probe-oneshot PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/weavelib)

zengine_weave(zengine-probe-required weavelib/oneshot_probe.cpp)
target_link_libraries(zengine-probe-required PRIVATE zengine-timer-vocabulary zengine-activation)
target_include_directories(zengine-probe-required PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/weavelib)
target_compile_definitions(zengine-probe-required PRIVATE PROBE_REQUIRE_PRESERVATION=1)

#   zengine-operator-consumer-weave  OPH-0's dynamic stranger: a real loadable weave
#                           that spends the HOST's operator truth through the injected
#                           surface and cannot reach the catalog behind it. Its link
#                           line is the phase's product claim and is meant to be read:
#                           `zengine-operator-consumer` (the C table, the descriptor
#                           codec and the handle) plus the ask vocabulary, and NOT
#                           `zengine-operator` -- so no `op::Catalog`, no `OperatorDef`,
#                           no `make_operator` and no primitive is reachable from this
#                           image at all. SEM-0's fence had to be a grep because its
#                           stranger linked the package that contained the class; this
#                           one links a package that does not contain one.
#
#   ...-legacy              THE SAME SOURCE, minus one declaration. A weave that never
#                           heard of operators: it exports no consumer symbol, receives
#                           no offer, and loads and runs exactly as it would have before
#                           this seam existed. What separates the two artifacts is
#                           `ZENGINE_OPERATOR_CONSUMER();` and nothing else, which is
#                           what makes "optional" checkable rather than asserted.
#
#   ...-abi                 THE SAME SOURCE, exporting the surface at a version this
#                           host does not speak -- an artifact from another era. The
#                           host must REFUSE the handoff and name both numbers rather
#                           than call through a table whose shape it is guessing at.
#
#   zengine-workshop-hello  WP-0's external witness: a real dynamic weave that offers
#                           Workshop one read-only pane under its own Loom office. It is a
#                           FIXTURE and not a product -- no host boots it and Workshop ships
#                           two panes, neither of which is this. It goes through
#                           `zengine_weave` for the reason every other one does, and it
#                           carries `zengine-warnings` and `zengine-sanitize` so the
#                           standing ASan+UBSan lane sees the new target rather than
#                           silently omitting it (AGENTS.md: that is the one way this lane
#                           degrades).
foreach(oph_variant IN ITEMS "" "-legacy" "-abi")
    zengine_weave(zengine-operator-consumer-weave${oph_variant}
                  weavelib/operator_consumer_weave.cpp)
    # THE LINK LINE IS THE CLAIM. `zengine-operator-consumer` and the ask
    # vocabulary; `zengine-operator` is deliberately absent, and adding it would
    # be the edit that quietly lets this image hold a catalog of its own.
    target_link_libraries(zengine-operator-consumer-weave${oph_variant}
        PRIVATE zengine-operator-consumer zengine-warnings zengine-sanitize)
    target_include_directories(zengine-operator-consumer-weave${oph_variant}
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/weavelib)
endforeach()
target_compile_definitions(zengine-operator-consumer-weave-legacy PRIVATE OPH_STRANGER_LEGACY=1)
target_compile_definitions(zengine-operator-consumer-weave-abi PRIVATE OPH_STRANGER_ABI=1)

# ---- PROV-0's provider artifacts, and NOT ONE OF THEM IS A WEAVE ------------
#
# `zengine_provider` rather than `zengine_weave`, and the absence of `loom::switchboard`
# on these lines is the claim: there is no participant in any of these images. No
# `zen_weave_abi`, no WeaveId, no role, no grant, no manifest and no bus. A host opens
# the file, resolves ONE optional symbol, and reads definitions out of it.
#
#   zengine-provider-a        the three-level chain, `prov.function.3` = x * 2
#   zengine-provider-b        `prov.function.3` ALONE, = x + 100 -- same signature, so
#                             nothing structural notices the substitution
#   zengine-provider-b-wrong  `prov.function.3` at Text -> Bool: a different power
#                             wearing the same name, which an overlay must refuse
#   zengine-provider-abi      the surface at a version this host does not speak
#   zengine-provider-min      `math.max` supplied as a MIN -- SEM-0's instrument, as an
#                             artifact, for the REAL Timer override witness
#
# One source for the first four (the weavelib pattern), because the difference under
# test is one branch and everything else must be identical.
set(ZENGINE_PROV_NAMES a b b-wrong abi)
set(ZENGINE_PROV_FLAGS PROV_CHAIN_A PROV_CHAIN_B PROV_CHAIN_WRONG PROV_CHAIN_ABI)
foreach(prov_name prov_flag IN ZIP_LISTS ZENGINE_PROV_NAMES ZENGINE_PROV_FLAGS)
    zengine_provider(zengine-provider-${prov_name} weavelib/provider_chain.cpp)
    target_link_libraries(zengine-provider-${prov_name} PRIVATE zengine-operator)
    # PROV_CHAIN_A is the default arm and the source never tests for it; defining it
    # anyway keeps this loop from needing a branch, and a flag that selects the
    # already-default behaviour is honest about being one.
    target_compile_definitions(zengine-provider-${prov_name} PRIVATE ${prov_flag}=1)
endforeach()

zengine_provider(zengine-provider-min weavelib/provider_min.cpp)
target_link_libraries(zengine-provider-min PRIVATE zengine-operator)

# SOURCE-0's artifact: one zero-input SOURCE and one ordinary operator, from one
# provider. It is a separate file rather than a flag on `provider_chain.cpp` because
# what it demonstrates is not a variation of the chain -- it is that a contribution with
# NO input ports crosses the existing codec, mounts, and can be sampled, out of an image
# the host opened for itself. Its Source counts its own spends, because the host's
# `op::invocations()` is a vague-linkage static and cannot see a body over there.
zengine_provider(zengine-provider-source weavelib/provider_source.cpp)
target_link_libraries(zengine-provider-source PRIVATE zengine-operator)

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
            zengine-input-vocabulary zengine-surface-vocabulary zengine-operator
            zengine-operator-stranger zengine-warnings zengine-sanitize)
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
#
# ---- THE FIXTURES' OWN BUILD TREE, CONFIGURED AT BUILD TIME (ZOOM-P1) --------
#
# The targets themselves are in tests/compile_negative/CMakeLists.txt now, and
# they are built HERE — in a second, small build tree of this same project — and
# never in ${CMAKE_BINARY_DIR}. `cmake --build` makes its caller responsible for
# bringing that tree's build system up to date, so eight compile tests naming the
# main tree were eight independent proofs holding a claim on it; run in parallel
# with a regeneration owed, they raced each other through CMake and reported
# `configure_file` errors where a diagnostic should have been.
#
# CONFIGURED AT BUILD TIME AND BUILT AT TEST TIME — the split the Builder suite's
# fixture tree already uses, and for the reason its header gives: the expensive,
# environment-sensitive half belongs where the environment is already known, and
# what the test does at run time is the one thing it is about. A nested CONFIGURE
# inside a ctest entry is exactly what went wrong here.
#
# WHAT IS SWITCHED OFF IS AS DELIBERATE AS WHAT IS PASSED THROUGH:
#
#   BUILD_TESTING=OFF        no suites, and no fixture tree of its own — this
#                            tree is the leaf, not another branch.
#   ZENGINE_SDL_SKIN=OFF     no FetchContent AT ALL, so no compile test can
#                            inherit a dependency population. That was the
#                            second half of the defect: the failing entries were
#                            re-entering the SDL3 sub-build, outside this tree
#                            and shared between every build directory of the same
#                            name.
#   ZENGINE_INSTALL=OFF      this tree ships nothing, so it declares no package —
#                            and in dev mode it MUST NOT, because a build that
#                            takes the Loom from the sibling source cannot honestly
#                            export targets naming loom::core
#                            (cmake/ZengineInstall.cmake refuses that pair, and
#                            would refuse this configure with it).
#   SUPPRESS_REGENERATION=ON no `RERUN_CMAKE` edge in the generated build files,
#                            so `cmake --build` on this tree CANNOT re-run CMake.
#                            The custody is structural, not a matter of timing.
#
# Everything that decides what the compiler is handed IS passed through — the
# generator, the compiler, its launcher, the flags, the build type, the sanitizer
# switch and the very Loom this tree resolved — because a fixture compiled under
# some other configuration would be proving a wall that this build does not have.
#
# IT HAPPENS DURING THIS CONFIGURE, not during a build, and that was measured
# rather than assumed. Preparing it as a build step (the shape the Builder suite's
# fixture tree uses, since a suite BINARY can depend on one) left a hole this lane
# has no target to close: configure, `--build --target zengine-ui-tests`, run a
# compile test, and the entry fails with `compile-fixtures is not a directory` —
# a developer being asked for a ceremony they were never told about. Configuring
# here costs 1.1 s and means the tree exists the moment configure returns, for
# every build order anyone might use. It also makes the ownership exact: the
# fixtures' configuration is written by the parent's configure and by nothing
# else, so it can neither drift behind it nor be produced by a test.
set(zengine_cn_tree ${CMAKE_CURRENT_BINARY_DIR}/compile-fixtures)

set(zengine_cn_args -G "${CMAKE_GENERATOR}")
if(CMAKE_MAKE_PROGRAM)
    list(APPEND zengine_cn_args "-DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}")
endif()
if(CMAKE_GENERATOR_PLATFORM)
    list(APPEND zengine_cn_args -A "${CMAKE_GENERATOR_PLATFORM}")
endif()
if(CMAKE_GENERATOR_TOOLSET)
    list(APPEND zengine_cn_args -T "${CMAKE_GENERATOR_TOOLSET}")
endif()
foreach(passthrough CMAKE_CXX_COMPILER CMAKE_CXX_COMPILER_LAUNCHER CMAKE_CXX_FLAGS
                    CMAKE_BUILD_TYPE CMAKE_CONFIGURATION_TYPES)
    if(DEFINED ${passthrough})
        list(APPEND zengine_cn_args "-D${passthrough}=${${passthrough}}")
    endif()
endforeach()
# The Loom this tree actually resolved, by whichever of the two routes it took —
# the exported package at the directory find_package() settled on, or the sibling
# source tree. A fixture that found a DIFFERENT Loom would be judging a different
# wall, so neither route is left to be re-decided down there.
if(ZEN_LOOM_DEV)
    list(APPEND zengine_cn_args "-DZEN_LOOM_DEV=ON")
else()
    list(APPEND zengine_cn_args "-Dloom_DIR=${loom_DIR}")
endif()

execute_process(
    COMMAND ${CMAKE_COMMAND} -S ${CMAKE_SOURCE_DIR} -B ${zengine_cn_tree}
            ${zengine_cn_args}
            "-DZENGINE_COMPILE_FIXTURES=ON"
            "-DZENGINE_SANITIZE=${ZENGINE_SANITIZE}"
            "-DZENGINE_SDL_SKIN=OFF"
            "-DZENGINE_INSTALL=OFF"
            "-DBUILD_TESTING=OFF"
            "-DCMAKE_SUPPRESS_REGENERATION=ON"
    OUTPUT_VARIABLE zengine_cn_out ERROR_VARIABLE zengine_cn_out
    RESULT_VARIABLE zengine_cn_rc)
# Loud here rather than as a puzzling test failure later: a compile test whose tree
# was never configured fails on `is not a directory`, which names the symptom and
# not one thing about the cause.
if(NOT zengine_cn_rc EQUAL 0)
    message(FATAL_ERROR
        "zengine: could not configure the compile-judged lane's fixture build tree at\n"
        "  ${zengine_cn_tree}\n"
        "The compile tests build their fixtures there — never in this build tree — so "
        "without it they have nothing to judge.\n${zengine_cn_out}")
endif()
message(STATUS "zengine: compile-lane fixtures configured in ${zengine_cn_tree}")

# Read by zengine_compile_test(), which refuses to register an entry without it.
set(ZENGINE_COMPILE_FIXTURE_TREE ${zengine_cn_tree})

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

# THE SAME DEFECT WITH NO BINDING DECLARED, and it is a separate entry because it was a
# separate outcome. The visibility wall was anchored in `timers()`, so a weave that takes
# this layer for its activation half and places its schedules with the raw protocol -- the
# documented answer whenever the delay is runtime data -- never instantiated the sentence
# that names the fix: measured at 72 lines of template soup mentioning neither
# `using TimedWeave::on;` nor the hiding. Both walls are constructor-anchored now. Same two
# spellings as the case above and for the same reason (MSVC-0): GCC reaches the authored
# static_assert, MSVC's requires-expression accepts the probe and the wall lands one layer
# out as the conversion failure. The WALL is the property under test on both.
zengine_compile_test(timer_missing_using_no_binding_refused zengine-cn-missing-using-no-binding
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
    "WORLD_V1_SO=\"$<TARGET_FILE:snake-world-v1>\""
    # QR-11's tripwire reads the SDL reader's own sources: "this reader never touches the
    # clipboard" is a claim about what code does NOT contain, so it cannot be proved by
    # running it, and it must hold on every lane -- including the ones that build no SDL.
    "INPUT_SDL_READER_CPP=\"${CMAKE_SOURCE_DIR}/input/input_sdl.cpp\""
    "INPUT_SDL_TRANSLATE_HPP=\"${CMAKE_SOURCE_DIR}/input/translate_sdl.hpp\"")

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

# ---- The Operator suite, and the stranger that reads a rule it did not write ----------
#
# `zengine-operator-stranger` is its own target on purpose. It is SEM-0's
# independent consumer, and what makes it independent is visible here: its link
# line names `zengine-operator` and nothing else -- no timer vocabulary, no timer
# weave, no `timer/normalize.hpp`. It is handed a catalog and a name and reads
# every port off the operator's own schema.
#
# The fence is honest rather than absolute, and the limit is worth writing down:
# these are header-only packages, so a link line cannot stop a later edit from
# including sideways. What it CAN say -- and what a reviewer can check in one
# grep -- is that this translation unit names no timer symbol and no timer
# string, which is the claim §10 actually makes.
add_library(zengine-operator-stranger STATIC operator_stranger.cpp)
target_link_libraries(zengine-operator-stranger
    PUBLIC zengine-operator
    PRIVATE zengine-warnings zengine-sanitize)
target_include_directories(zengine-operator-stranger PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# OPH-0 adds a second file to this suite, and the split between them is the
# difference between the two claims. `test_operator.cpp` asks what an operator IS
# and what it takes for two consumers to spend one, and needs no kernel at all.
# `test_operator_host.cpp` asks whether a consumer in ANOTHER IMAGE can spend the
# host's, and needs the real Kernel, the real Weave Manager and three real
# artifacts -- so this suite now links loom::kernel and loom::switchboard, which
# until this phase it deliberately did not.
# PROV-0 adds a FOURTH file to the same binary, on the same terms: the population
# contract counts ENTRIES, and a phase that registered a new CTest entry for a new
# tier of an existing suite would be splitting the inventory in order to record a
# file. What it asks is the next question in the line -- `test_operator.cpp` asks what
# an operator is, `test_operator_host.cpp` whether a loaded stranger can spend a
# host's, `test_operator_canonical.cpp` whether the Timer and that stranger spend ONE,
# and `test_operator_provider.cpp` where the host's own powers came from.
# SOURCE-0 adds a FIFTH on the same terms again, asking the next question in that line:
# which entries can be spent with nothing supplied (`test_operator_source.cpp`), what
# sampling one costs, and what it costs to enumerate one WITHOUT sampling it.
add_executable(zengine-operator-tests test_operator.cpp test_operator_host.cpp
                                     test_operator_canonical.cpp test_operator_provider.cpp
                                     test_operator_source.cpp
                                     doctest_main.cpp)
target_link_libraries(zengine-operator-tests
    PRIVATE loom::kernel loom::switchboard loom::core zengine-operator
            zengine-operator-stranger zengine-timer-vocabulary zengine-warnings
            zengine-sanitize)
target_include_directories(zengine-operator-tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
    ${CMAKE_CURRENT_SOURCE_DIR}/weavelib)

target_compile_definitions(zengine-operator-tests PRIVATE
    # CAT-0's canary drives the SHIPPED Timer artifact beside OPH-0's stranger in
    # one process, because "one live catalog" is a claim about the two of them
    # together and neither one alone can carry it.
    "TIMER_SO=\"$<TARGET_FILE:zengine-timer>\""
    # The production host's SOURCE, for CAT-0's ownership tripwire. Workshop's
    # `main()` claims a terminal, so no case here can run it; what a case CAN do is
    # read the arrangement off the file, which is the same defence in depth the
    # no-privileged-wind and clock-binding tripwires already provide next door.
    "WORKSHOP_HOST_CPP=\"${CMAKE_SOURCE_DIR}/workshop/workshop.cpp\""
    # ...and since LOAD-0, the file the host's ordering law MOVED INTO. The offer that
    # used to bracket the Timer's boot in `workshop.cpp` is not there any more --
    # `workshop.cpp` names no artifact at all -- so the tripwire that read it follows
    # the code to `load_execute.hpp`, which is where mount-before-offer-before-load is
    # now written.
    "WORKSHOP_LOAD_EXECUTE_HPP=\"${CMAKE_SOURCE_DIR}/workshop/load_execute.hpp\""
    "OPH_STRANGER_SO=\"$<TARGET_FILE:zengine-operator-consumer-weave>\""
    "OPH_STRANGER_LEGACY_SO=\"$<TARGET_FILE:zengine-operator-consumer-weave-legacy>\""
    "OPH_STRANGER_ABI_SO=\"$<TARGET_FILE:zengine-operator-consumer-weave-abi>\""
    # A REAL PRE-EXISTING ARTIFACT, untouched by this phase and built by another
    # package's rules. The `-legacy` fixture proves that dropping one declaration
    # from an operator-aware source leaves an ordinary weave; this one proves the
    # thing that actually matters -- that a weave written before this seam existed
    # meets the offer path and is simply not a consumer.
    "OPH_UNTOUCHED_WEAVE_SO=\"$<TARGET_FILE:zengine-workshop-hello>\""
    # The stranger's SOURCE, for the independence tripwire. The fence is a fact
    # about what this translation unit can name, and only reading the file can
    # tell whether it still names nothing from the host's side of the seam --
    # the same defence-in-depth shape as snake's no-privileged-wind tripwire.
    "OPH_STRANGER_CPP=\"${CMAKE_CURRENT_SOURCE_DIR}/weavelib/operator_consumer_weave.cpp\""
    # ---- PROV-0's artifacts ----------------------------------------------------
    #
    # THE SHIPPED BASIC PROVIDER, not a fixture: `zengine-operators-basic` is a
    # production artifact this host mounts, and the suite drives the same file the
    # Workshop does. A fixture standing in for it would prove the seam and not the
    # product.
    "PROVIDER_BASIC_SO=\"$<TARGET_FILE:zengine-operators-basic>\""
    "PROVIDER_A_SO=\"$<TARGET_FILE:zengine-provider-a>\""
    "PROVIDER_B_SO=\"$<TARGET_FILE:zengine-provider-b>\""
    "PROVIDER_WRONG_SO=\"$<TARGET_FILE:zengine-provider-b-wrong>\""
    "PROVIDER_ABI_SO=\"$<TARGET_FILE:zengine-provider-abi>\""
    "PROVIDER_MIN_SO=\"$<TARGET_FILE:zengine-provider-min>\""
    # ---- SOURCE-0's artifact ---------------------------------------------------
    #
    # A provider whose batch is one zero-input SOURCE and one ordinary operator. It
    # is the only way to ask whether a definition with NO input ports survives the
    # real encode/decode across a real module boundary -- everything else about that
    # claim can be, and is, asked in one process by the codec case beside it.
    "PROVIDER_SOURCE_SO=\"$<TARGET_FILE:zengine-provider-source>\""
    # ...and the Source seam's own SOURCE, for the Sense/message fence. `operator/source.hpp`
    # must stay a projection over the catalog and nothing else: no bus, no Sense, no kernel,
    # no loader. This suite links `loom::switchboard` for the OPH-0 tier, so no link line can
    # say that -- only reading the file can, exactly as the stranger fence next door does.
    "OPERATOR_SOURCE_HPP=\"${CMAKE_SOURCE_DIR}/operator/source.hpp\""
    # OPH-0's loaded stranger, under a name of this tier's own: the real Timer
    # override witness needs a consumer in a THIRD image to agree with the Timer,
    # and a stranger that shares no line of code with it is the only thing that can.
    "PROV_STRANGER_SO=\"$<TARGET_FILE:zengine-operator-consumer-weave>\""
    # ...and a real pre-existing weave, untouched by this phase and built by another
    # package's rules, so "most artifacts are not providers" is measured on one.
    "PROV_UNTOUCHED_WEAVE_SO=\"$<TARGET_FILE:zengine-workshop-hello>\"")

zengine_doctest_test(operator zengine-operator-tests)

# The Composer suite: what a maker has AUTHORED, what it composes to, and what
# they would see (MSG-0).
#
# It links no switchboard, and that absence is the claim. `composer/draft.hpp`
# and `composer/view.hpp` know nothing about a bus, a Kernel or a medium, so a
# case here asks a VALUE what a draft means and what a pane would show. Where the
# fact comes FROM -- a real target's real `zen.AcceptedShapes`, through the real
# load path, into the real pane protocol -- is the Workshop suite's claim, for
# INTR-0's reason exactly: a seam is proved where the seam is.
#
# It does link loom::terminal, because the typed composer IS the thing under
# test: every refusal this suite reads back is `compose_message`'s, in its words,
# and a case that asserted a sentence written in Zengine would be asserting a
# second copy of a judgement it does not own.
add_executable(zengine-composer-tests test_composer.cpp doctest_main.cpp)
target_link_libraries(zengine-composer-tests
    PRIVATE zengine-composer-draft zengine-warnings zengine-sanitize)
target_include_directories(zengine-composer-tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party)

zengine_doctest_test(composer zengine-composer-tests)

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
#
# These targets are defined in tests/compile_negative/CMakeLists.txt beside the
# Timer suite's, and built in the fixtures' own build tree — never in this one.
# The block that configures that tree, and why it exists, is up with the Timer
# lane that needed it first.
zengine_compile_test(ui_authored_extent_required zengine-cn-ui-extent
                     "a resolvable dimension must be an authored Extent")

zengine_compile_test(ui_resolved_geometry_refused zengine-cn-ui-resolved
                     "no resolved rectangle may live on an authored element")

zengine_compile_test(ui_authored_element_compiles zengine-cn-ui-ok)

# ---- The Workshop suites -------------------------------------------------------------
#
# EIGHT BINARIES, NOT ONE, AND THE BOUNDARIES ARE THE SOURCE'S OWN. Workshop's cases had
# grown to a single thirty-thousand-line translation unit: thirty seconds to compile
# before one assertion could run, and nineteen seconds of test time CTest could only ever
# give to one worker. Neither number is about how much Workshop proves -- it is the same
# population -- and both are about it being presented to the machine as one indivisible
# job. So the file was cut along the seams its own headings already had:
#
#   workshop_document      the authored material and the maker's hands on it
#   workshop_screen        composition and geometry -- what is painted where
#   workshop_panels        the panels Workshop ships, and the attention surface
#   workshop_panes         the external pane seam, from both sides
#   workshop_persistence   what survives a process
#   workshop_load          which artifacts are in the room at all
#   workshop_editor        the built-in source editor and its document
#   workshop_files         the project browser, its bounds and its root
#
# THE COUNT IN THAT FIRST SENTENCE IS THE ONE THING HERE THAT DATES: the split's argument
# is about boundaries, not about how many there are, and each later phase that earns a suite
# adds a line above rather than restating the case for cutting.
#
# `tests/workshop_support.hpp` holds what more than one of them needs -- the fixtures, the
# canvas readers, the `Live` and `PaneRig` rigs -- and says in its own header what may go
# there and what may not. `test_workshop_load.cpp` does not include it: its rigs are its
# own, because a Timer in a rig that drains to empty never returns and every one of its
# cases owns a Switchboard and a Kernel of its own.
#
# WHAT IS PROVEN IS UNCHANGED. Every case moved verbatim into the suite that owns its
# subject; `tests/test_population.txt` carries a floor per suite where it carried one, and
# at the split their sum was what the one was.
#
# The Workshop surface is proven headless. That is not a limitation of the suites, it is a
# property of the design: the authored objects are plain data, the operations on them are
# functions that can refuse, the inspector is a list of values, and the screen is a
# SurfaceCanvas returned by a function. What headless cannot prove is that a Skin carries
# the canvas to a human's eyes -- that is the Surface suite's job, plus the live run.
#
# After W-1 they also prove the INTEGRATION the relocation created: that the
# geometry a maker sees comes from the UI package and from nowhere else -- the
# painted rectangle, the inspector's resolved reading and the answer to a click
# are all derived from one ui::Scene, so they cannot come to disagree. What
# resolution and hit testing DO is the ui suite's claim; that Workshop gets its
# answers from there is these ones'.
#
# Workshop's OWN weave is still mounted in-process, so there is still no artifact of
# Workshop's for the kernel to load -- but since WP-0 these suites DO define `_SO` paths,
# and the comment that used to say otherwise is corrected rather than deleted. The first is
# a PROVIDER's: `zengine-workshop-hello` is a real dynamic library they load through
# the real Kernel and Manager, because the claim WP-0 has to make is about the real ABI and
# load path and a mock loader would prove nothing about either.
#
# What they also need is the surface vocabulary and the terminal rasterizer, because one case
# runs a real painted screen through the medium's own pure function.
#
# INTR-0 gave a SECOND `_SO` path, and the two are different KINDS of artifact rather
# than two of a kind. `zengine-workshop-hello` is a fixture this directory builds, whose
# whole job is to be the smallest thing that exercises the protocol. `zengine-introspection`
# is a PRODUCT -- a package beside timer/ and input/, staged and booted by the real Workshop
# host -- and the suites load the very library a maker runs. So a case here that reads its
# rows is reading the shipped tool's answer, not a stand-in's, which is the only way the
# claim "every displayed fact is true" can be tested at all.
#
# MSG-0 gave a THIRD `_SO` path, and it is a PRODUCT rather than a fixture -- the
# same distinction INTR-0 drew for the second. `zengine-composer` is the tool under
# test, and the cases that name it load the very library a maker runs: what a maker sees is
# read off the published canvas at the rectangle Workshop's own painter used, so
# nothing about the pane is asserted from the inside.
#
# THERE IS NO `_SO` FOR THE TIMER IN THE FIVE SUITES ABOVE `workshop_load`, AND THAT
# ABSENCE IS MEASURED RATHER THAN CHOSEN. The shipped Timer service re-arms its own beat
# inside its own handler and `Switchboard::drain_until_idle()` does what it says, so
# loading it into a rig whose every gesture drains never returns -- it hung, at the load.
# The Timer's real SHAPES are still what these cases compose (see `TimerSeat` in
# test_workshop_panes.cpp), and the real SERVICE is exercised where a real service can be:
# in `workshop_load`, whose rigs drain in bounded turns, and in the live run.
set(ZENGINE_WORKSHOP_LIBS
    loom::kernel loom::switchboard loom::core loom::terminal
    zengine-workshop-vocabulary zengine-workshop-load
    zengine-surface-vocabulary zengine-ui-vocabulary
    zengine-introspection-view zengine-composer-vocabulary
    zengine-timer-vocabulary zengine-warnings
    zengine-sanitize)

# The artifacts `workshop_support.hpp` itself names, so every suite that includes it needs
# them. The three fixtures stand in for the things a plan can point at and get wrong;
# `zengine-operators-basic` is what the shipped plan actually names.
set(ZENGINE_WORKSHOP_DEFS
    "WORKSHOP_SO_HELLO=\"$<TARGET_FILE:zengine-workshop-hello>\""
    "WORKSHOP_SO_INTROSPECTION=\"$<TARGET_FILE:zengine-introspection>\""
    "PROVIDER_BASIC_SO=\"$<TARGET_FILE:zengine-operators-basic>\""
    # SEM-0's substitution instrument as an ARTIFACT: `math.max` supplied as a MIN at
    # the same signature.
    "PROVIDER_MIN_SO=\"$<TARGET_FILE:zengine-provider-min>\""
    # ...and INTR-1's GENERICITY witness, which needs powers this repository's
    # projections and panes have never heard of. `zengine-provider-a` supplies
    # `prov.function.1/2/3` -- two composites over one native leaf -- under a provider
    # identity no source outside `tests/` names. A projection that had to be edited to
    # show them would not be a projection over the store.
    "PROVIDER_A_SO=\"$<TARGET_FILE:zengine-provider-a>\""
    # ...and SOURCE-1's evaluation witness, which needs a real cross-image SOURCE.
    # `zengine-provider-source` supplies `prov.source.spends` (zero maker inputs, and a
    # body that COUNTS ITS OWN SPENDS in its own image) beside an ordinary
    # parameterized operator. `op::invocations()` is a vague-linkage static and cannot
    # see a body running in another image, so a Source that answers 1, then 2, then 3
    # is the only instrument that can tell an evaluation from a cached answer across
    # the provider ABI.
    "PROVIDER_SOURCE_SO=\"$<TARGET_FILE:zengine-provider-source>\"")
set(ZENGINE_WORKSHOP_FIXTURES
    zengine-workshop-hello zengine-introspection
    zengine-operators-basic zengine-provider-min zengine-provider-a
    zengine-provider-source)

# One Workshop suite: the shared link line, the shared artifact paths, the shared
# third_party include, plus whatever this one alone needs. It exists so the six agree by
# construction -- a second copy of a link line is a second answer -- and so that adding a
# seventh is one call rather than forty lines.
function(zengine_workshop_suite entry source)
    cmake_parse_arguments(WS "" "" "DEFINITIONS;NEEDS;LINKS" ${ARGN})
    set(target zengine-workshop-${entry}-tests)
    add_executable(${target} ${source} doctest_main.cpp)
    target_link_libraries(${target} PRIVATE ${ZENGINE_WORKSHOP_LIBS} ${WS_LINKS})
    target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
    # THE SUITE'S OWN NAME, because the six now run at once and a temporary directory has
    # to belong to one of them (`workshop_temp_root` in workshop_support.hpp).
    target_compile_definitions(${target} PRIVATE
        "ZENGINE_WORKSHOP_SUITE=\"${entry}\"" ${ZENGINE_WORKSHOP_DEFS} ${WS_DEFINITIONS})
    add_dependencies(${target} ${ZENGINE_WORKSHOP_FIXTURES} ${WS_NEEDS})
    zengine_doctest_test(workshop_${entry} ${target})
endfunction()

zengine_workshop_suite(document test_workshop_document.cpp
    DEFINITIONS "WORKSHOP_SO_COMPOSER=\"$<TARGET_FILE:zengine-composer>\""
    NEEDS zengine-composer)

zengine_workshop_suite(screen test_workshop_screen.cpp)

# The SOURCE EDITOR: the multiline buffer, the byte law, the Builder's edit-source
# door, save/dirty truth, the viewport and the no-silent-loss floor. Its temporary
# directories are its own, exactly as the family rule requires.
zengine_workshop_suite(editor test_workshop_editor.cpp)

# The PROJECT BROWSER: enumeration and its bound, the directories-first order, the
# name admission, the root boundary, the press/wheel routing and the path from a row to
# the editor's one door. Its temporary directories are its own, and the project root it
# browses is one of them -- no case here reads the machine's real working directory.
#
# ...AND WHO OWNS THE COMPLETED RECIPE CATALOG (PROJ-0), which is the same subject one
# noun over: this suite already asks what a project-relative source MEANS, and the answer
# now has a session-long owner. The HOST'S SOURCE comes with it, because "there is ONE
# completed catalog in this process" is a claim about an arrangement no rig can run --
# `main()` claims a terminal -- and the tripwires next door read it for the same reason.
#
# ...AND SINCE PROJ-1 THE OWNER'S OWN HEADER COMES WITH IT, because the completion and the
# custody the host used to spell now live in `install_recipes` -- one seam the launch and a
# maker's live choice both go through. "There is one recipe policy" is a claim about that
# arrangement, and the two files have to be read together to make it.
zengine_workshop_suite(files test_workshop_files.cpp
    DEFINITIONS "WORKSHOP_HOST_CPP=\"${CMAKE_SOURCE_DIR}/workshop/workshop.cpp\""
                "WORKSHOP_RECIPES_HPP=\"${CMAKE_SOURCE_DIR}/workshop/recipes.hpp\"")

# `loom::history` IS LINKED BY THIS SUITE AND BY NOTHING IN WORKSHOP -- AND BY NONE OF THE
# OTHER FIVE -- and the asymmetry is the claim. The current-condition lifecycle must not
# reach the Recorder or the Logger, and "it does not" is only a measurement if a Recorder
# and a Logger are ACTUALLY ATTACHED to the bus while a condition is established, read,
# hidden and resolved. So this suite attaches both and reads their counters; the Workshop
# package still links neither, which is where the property is actually enforced.
zengine_workshop_suite(panels test_workshop_panels.cpp
    LINKS loom::history
    DEFINITIONS "WORKSHOP_ATTENTION_HPP=\"${CMAKE_SOURCE_DIR}/workshop/attention.hpp\"")

# ---- INTR-1's SOURCE TRIPWIRES -------------------------------------------------------
#
# FIVE FILES READ AS FILES, and the pair of claims is why. Workshop's four say
# "this host was TAUGHT no pane" -- neither of the two INTR-1 added, nor the one
# INTR-0 added -- which no rig can prove, because every rig here would stay green
# if Workshop grew a special case that happened to agree with the generic path.
# The two projections say "no view branches on an identity", which is the whole
# genericity feature: a provider added later must appear without an edit.
#
# They are TRIPWIRES rather than proofs, exactly as `test_operator_provider.cpp`
# declares its own to be. Workshop's `main()` claims a terminal and no case here
# can run it; what a source read adds is that a claim every case rests on cannot
# quietly stop being true.
zengine_workshop_suite(panes test_workshop_panes.cpp
    NEEDS zengine-composer
    DEFINITIONS "WORKSHOP_SO_COMPOSER=\"$<TARGET_FILE:zengine-composer>\""
                "WORKSHOP_HOST_CPP=\"${CMAKE_SOURCE_DIR}/workshop/workshop.cpp\""
                "WORKSHOP_WEAVE_HPP=\"${CMAKE_SOURCE_DIR}/workshop/weave.hpp\""
                "WORKSHOP_SCREEN_HPP=\"${CMAKE_SOURCE_DIR}/workshop/screen.hpp\""
                "WORKSHOP_PANEL_HPP=\"${CMAKE_SOURCE_DIR}/workshop/panel.hpp\""
                "WORKSHOP_ARRANGEMENT_HPP=\"${CMAKE_SOURCE_DIR}/workshop/arrangement.hpp\""
                "INTROSPECTION_RESOLVED_HPP=\"${CMAKE_SOURCE_DIR}/introspection/resolved.hpp\""
                # SOURCE-1's structural half of "browsing does not evaluate": the pane's
                # own two sources, read as text, so that "this image cannot reach an
                # evaluator" is checked rather than trusted -- and the two host files
                # whose narrowness the phase's whole authority argument rests on.
                "INTROSPECTION_CPP=\"${CMAKE_SOURCE_DIR}/introspection/introspection.cpp\""
                "INTROSPECTION_POWERS_HPP=\"${CMAKE_SOURCE_DIR}/introspection/powers.hpp\""
                "WORKSHOP_SAMPLE_DOOR_HPP=\"${CMAKE_SOURCE_DIR}/workshop/sample_door.hpp\""
                "INTROSPECTION_CMAKE=\"${CMAKE_SOURCE_DIR}/introspection/CMakeLists.txt\"")

zengine_workshop_suite(persistence test_workshop_persistence.cpp
    DEFINITIONS "WORKSHOP_HOST_CPP=\"${CMAKE_SOURCE_DIR}/workshop/workshop.cpp\"")

# LOAD-0's suite asks which artifacts are in the room at all, where the five above ask what
# a maker sees and does. It is the one that STAGES artifacts into a real directory, because
# the claim "filesystem presence is not load authority" cannot be made against a lookup
# table -- with a table, "not in the plan" and "not reachable" are the same thing and the
# interesting question cannot be asked. It copies these files into `WORKSHOP_LOAD_STAGE_DIR`
# itself and then resolves stems against it with the HOST's own rule, so the directory holds
# more artifacts than any plan in the file names.
#
# THE STAGE DIRECTORY IS THIS SUITE'S ALONE. No other Workshop suite names it, which is what
# lets all six run as concurrent CTest entries: two processes staging over one directory
# would be two owners of it.
#
# SIX PATHS, AND FOUR OF THEM ARE PRODUCTION ARTIFACTS. `zengine-operators-basic`
# and `zengine-timer` are what the shipped plan actually names, so a case here
# drives the very files a maker runs; the three fixtures stand in for the things a
# plan can point at and get wrong.
zengine_workshop_suite(load test_workshop_load.cpp
    NEEDS zengine-timer zengine-provider-abi zengine-operator-consumer-weave-abi
    DEFINITIONS "WORKSHOP_LOAD_STAGE_DIR=\"${CMAKE_CURRENT_BINARY_DIR}/load-plan-stage\""
                "TIMER_SO=\"$<TARGET_FILE:zengine-timer>\""
                # An ordinary weave that is not a provider and not an operator consumer,
                # built by another phase's rules and untouched by this one.
                "PLAIN_WEAVE_SO=\"$<TARGET_FILE:zengine-workshop-hello>\""
                # A provider surface at a version this host does not speak, and a CONSUMER
                # surface at a version this host does not speak. Two different refusals;
                # both must remain distinguishable from "an ordinary artifact".
                "PROVIDER_ABI_SO=\"$<TARGET_FILE:zengine-provider-abi>\""
                "BROKEN_CONSUMER_SO=\"$<TARGET_FILE:zengine-operator-consumer-weave-abi>\""
                # The two plan files this repository SHIPS, read as files rather than as a
                # description of files.
                "WORKSHOP_DEFAULT_PLAN=\"${CMAKE_SOURCE_DIR}/workshop/default-load-plan.json\""
                "WORKSHOP_GRAPHICAL_PLAN=\"${CMAKE_SOURCE_DIR}/workshop/graphical-load-plan.json\"")

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

# ---- THE FIXTURE BUILD TREE, CONFIGURED AT BUILD TIME (BLD-1) --------------------------
#
# A `CMakeTargetRecipe` names a CONFIGURED CMake build tree and a target in it, so the
# suite needs one. `tests/buildfixture/` is that project and says at length why it exists;
# what happens here is that it is configured ONCE, when this test tree is built, into a
# directory beside the suite binary.
#
# CONFIGURED AT BUILD TIME AND BUILT AT TEST TIME, and the split is deliberate. A nested
# CMake CONFIGURE inside a ctest entry is what `tests/package/run.cmake` refuses to be (its
# header says why); a nested `cmake --build` is not the same thing at all -- it is exactly
# what a build recipe IS, and running it is the whole point of the cases. So the expensive,
# environment-sensitive half happens where the environment is already known, and what the
# suite does at run time is the one thing production does.
#
# THE GENERATOR IS PASSED THROUGH RATHER THAN DEFAULTED. This build tree knows which
# generator and make program it was configured with; the fixture inherits both, so the
# suite cannot end up driving a generator this machine's CMake merely defaults to. The
# fixture declares `LANGUAGES NONE`, so no compiler is named, checked or needed.
#
# TWO TREES, BECAUSE ONE CASE RUNS TWO BUILDS AT ONCE. A build tree has ONE owner while a
# build is in it: `cmake --build` drives a generator that keeps private bookkeeping beside
# the manifest, and two of them entered concurrently can collide there and abort before the
# target is reached. Measured on Ninja, whose `.ninja_log` recompaction writes one fixed
# temp name per directory: two builds of one tree lost a child in 2 of 400, and in 9 of 40
# once the log was over the recompaction threshold; the same pairs against two trees lost
# none of 400. The case that runs two operations at once therefore gets a second tree, so
# the concurrency it is measuring is Builder's and not the generator's.
set(zengine_fixture_src ${CMAKE_CURRENT_SOURCE_DIR}/buildfixture)
set(zengine_fixture_tree ${CMAKE_CURRENT_BINARY_DIR}/build-fixture)
set(zengine_fixture_tree_b ${CMAKE_CURRENT_BINARY_DIR}/build-fixture-b)
# ONE ARTIFACTS DIRECTORY IS STILL SHARED, and that is not the same hazard: the fixture's
# successes end in `cmake -E make_directory` and `cmake -E touch`, both of which two
# processes may do to one path at the same time. What may not be shared is the tree a
# generator is running in.
set(zengine_fixture_artifacts ${CMAKE_CURRENT_BINARY_DIR}/build-fixture-artifacts)
set(zengine_fixture_gen -G "${CMAKE_GENERATOR}")
if(CMAKE_MAKE_PROGRAM)
    list(APPEND zengine_fixture_gen "-DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}")
endif()
if(CMAKE_GENERATOR_PLATFORM)
    list(APPEND zengine_fixture_gen -A "${CMAKE_GENERATOR_PLATFORM}")
endif()
if(CMAKE_GENERATOR_TOOLSET)
    list(APPEND zengine_fixture_gen -T "${CMAKE_GENERATOR_TOOLSET}")
endif()
set(zengine_fixture_stamps)
foreach(zengine_fixture_one ${zengine_fixture_tree} ${zengine_fixture_tree_b})
    add_custom_command(OUTPUT ${zengine_fixture_one}/CMakeCache.txt
        COMMAND ${CMAKE_COMMAND} -S ${zengine_fixture_src} -B ${zengine_fixture_one}
                ${zengine_fixture_gen}
                "-DZENGINE_FIXTURE_ARTIFACTS=${zengine_fixture_artifacts}"
                "-DZENGINE_FIXTURE_SUFFIX=${CMAKE_SHARED_LIBRARY_SUFFIX}"
                "-DZENGINE_FIXTURE_SLOW=${CMAKE_CURRENT_SOURCE_DIR}/slow_build.cmake"
        DEPENDS ${zengine_fixture_src}/CMakeLists.txt
        COMMENT "builder suite: configuring the fixture build tree a recipe can point at")
    list(APPEND zengine_fixture_stamps ${zengine_fixture_one}/CMakeCache.txt)
endforeach()
add_custom_target(zengine-build-fixture DEPENDS ${zengine_fixture_stamps})
add_dependencies(zengine-builder-tests zengine-build-fixture)

target_compile_definitions(zengine-builder-tests PRIVATE
    "ZENGINE_TEST_CMAKE=\"${CMAKE_COMMAND}\""
    "ZENGINE_TEST_SLOW_SCRIPT=\"${CMAKE_CURRENT_SOURCE_DIR}/slow_build.cmake\""
    "ZENGINE_TEST_FIXTURE_TREE=\"${zengine_fixture_tree}\""
    "ZENGINE_TEST_FIXTURE_TREE_B=\"${zengine_fixture_tree_b}\""
    "ZENGINE_TEST_FIXTURE_ARTIFACTS=\"${zengine_fixture_artifacts}\""
    "ZENGINE_TEST_ARTIFACT_SUFFIX=\"${CMAKE_SHARED_LIBRARY_SUFFIX}\""
    # ---- The two halves of the package, READ AS SOURCE FILES (BLD-1) --------------
    #
    # A property that is true because of what the code does NOT contain cannot be
    # proved by running the code. BOOT-0's tripwire over `load_execute.hpp` is the
    # precedent; these are the same question asked of the two participants a build
    # conversation passes through.
    "ZENGINE_TEST_TOOL_SOURCE=\"${CMAKE_SOURCE_DIR}/builder/weave.hpp\""
    "ZENGINE_TEST_RUNNER_SOURCE=\"${CMAKE_SOURCE_DIR}/builder/runner.hpp\"")

zengine_doctest_test(builder zengine-builder-tests)

# ---- The BUILD WITNESS: a host, and deliberately NOT a CTest entry (BLD-1) --------------
#
# `tests/build/witness.cpp` is Workshop's Builder wiring with the picture removed -- the same
# two weaves, the same grants, the same realization owner, the same ordinary host loop --
# driven by arguments instead of by keys, so a script can build one authored recipe and read
# what happened. `tests/build/run.cmake` is the driver that installs Zengine into a scratch
# prefix, writes a one-file weave OUTSIDE this repository, and puts the whole route through
# it including its canaries.
#
# WHY IT IS NOT A CTEST ENTRY. It installs a package, configures two nested CMake projects
# and runs a real compiler. `tests/package/run.cmake`'s header draws exactly this line and
# says why: a nested CONFIGURE inside a ctest entry is a second build system inside a test
# binary's population. It is one command a human runs and the same one CI runs.
#
# ⚠ IT IS STILL BUILT BY THE ORDINARY BUILD, and that is on purpose: a witness that only
# compiles when somebody remembers to run its lane is a witness that rots. What it PROVES
# needs the lane; that it still COMPILES is checked on every build of this tree.
add_executable(zengine-build-witness build/witness.cpp)
target_link_libraries(zengine-build-witness
    PRIVATE loom::kernel loom::switchboard loom::core
            zengine-builder-vocabulary zengine-workshop-load zengine-operator
            zengine-timer-vocabulary
            zengine-warnings zengine-sanitize)
target_compile_definitions(zengine-build-witness PRIVATE
    # THE ONE PROGRAM IT MAY START, by absolute path, chosen by the party that composed
    # the process -- `workshop.cpp`'s rule, unchanged, in the host that stands in for it.
    "ZENGINE_BUILDER_CMAKE=\"${CMAKE_COMMAND}\"")

# The Trust-Gate liveness probes: what the substrate actually does when the
# TIMER SERVICE ITSELF is swapped / reloaded / double-wound / joined late. The
# durable deliverable of the audit phase's Stage 5, ratified onto main by R1 —
# read the suite's own header for why a file that pins UNWANTED behavior lives
# here, and what R2 is expected to flip.
add_executable(zengine-audit-probes test_audit_probes.cpp doctest_main.cpp)
target_link_libraries(zengine-audit-probes
    PRIVATE loom::kernel loom::switchboard loom::core zengine-timer-vocabulary
            zengine-operator zengine-warnings zengine-sanitize)
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

# ---- The package-vocabulary contract (QR-5) --------------------------------------------
#
# The second entry that interrogates the SOURCE TREE rather than a build, and it is here for
# the same reason as the first: nothing in a build can notice a word.
#
# PKG-0 shipped the installed package's artifact directory and artifact list under names that
# said `weave`. Four of the five artifacts are weaves; `zengine-operators-basic` is a PROVIDER
# and explicitly not one (PROV-0), so the public noun was false of its own contents. An
# artifact is the physical loadable file; weave and provider are surfaces it may expose, and
# the package names the physical thing. This entry keeps the retired spellings from returning
# through a config, a doc, a fixture or a comment -- the one thing a compiler will never
# complain about.
zengine_script_test(package_vocabulary
                    ${CMAKE_CURRENT_SOURCE_DIR}/check_package_vocabulary.cmake)
