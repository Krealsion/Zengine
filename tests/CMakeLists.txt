# The Zengine suites; third_party/doctest.h is doctest's single header (MIT), vendored so this
# repository builds as the stranger it is. Every suite but smoke needs a Loom that can host
# weaves, and a Loom that cannot is refused here rather than skipped (VM-POP-10).
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

# ---- the suites' own weave libraries ---------------------------------------------------
# Fixtures built as every weave here is (zengine_weave, the Loom's reloadable-weave contract) and
# loaded through the real kernel. Where one source makes several libraries, the difference under
# test is one definition and everything else is identical. timer_virtual.cpp is the shipped Timer
# over a clock whose nap advances virtual time: its claims are about durations.
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

# The operator stranger, three ways from one source: as itself, spending the host's operators
# through the injected surface; -legacy, without its consumer declaration, a weave that never
# heard of operators and still loads and runs; -abi, exporting a surface version the host does
# not speak, which the host must refuse, naming both versions.
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

# ---- the operator providers, none of them a weave ----------------------------------------
# `zengine_provider` and no `loom::switchboard`: a host opens the file, resolves one optional
# symbol and reads definitions out of it. provider_chain.cpp builds four: a, the three-level
# chain; b, its last level alone at the same signature with another answer; b-wrong, that name at
# another signature, which an overlay must refuse; abi, a surface version the host does not
# speak. provider_min.cpp supplies `math.max` as a minimum, for the Timer override witness.
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

# A zero-input source and an ordinary operator from one provider, a file of its own because what
# it shows is that a contribution with no inputs crosses the codec, mounts and can be sampled.
# Its source counts its own spends: the host's `op::invocations()` is a vague-linkage static and
# cannot see a body in another image.
zengine_provider(zengine-provider-source weavelib/provider_source.cpp)
target_link_libraries(zengine-provider-source PRIVATE zengine-operator)

# ---- the migration providers, over an invented history ----------------------------------
# provider_migration.cpp builds four roads between three versions of `mig_fixture::Rung`
# (migration_family.hpp): chain, `v1 -> v2 -> v3` with no direct edge; direct, the authored
# `v1 -> v3`; direct-alt, that edge answering differently, as a lawful overlay would; composed,
# that edge as a composition. The history is invented because the real session history holds
# no such arrangement on purpose; the persistence suite runs the real one as itself.
set(ZENGINE_MIG_NAMES chain direct direct-alt composed)
set(ZENGINE_MIG_FLAGS MIG_CHAIN MIG_DIRECT MIG_DIRECT_ALT MIG_COMPOSED)
foreach(mig_name mig_flag IN ZIP_LISTS ZENGINE_MIG_NAMES ZENGINE_MIG_FLAGS)
    zengine_provider(zengine-provider-mig-${mig_name} weavelib/provider_migration.cpp)
    target_link_libraries(zengine-provider-mig-${mig_name} PRIVATE zengine-operator)
    # MIG_CHAIN is the default arm, defined anyway so the loop needs no branch, as above.
    target_compile_definitions(zengine-provider-mig-${mig_name} PRIVATE ${mig_flag}=1)
endforeach()

zengine_weave(zengine-workshop-hello weavelib/workshop_hello.cpp)
target_link_libraries(zengine-workshop-hello
    PRIVATE zengine-workshop-vocabulary zengine-surface-vocabulary zengine-activation
            zengine-warnings zengine-sanitize)

# THE MAKER'S EXAMPLE PANE, compiled here so it cannot rot (examples are compiled,
# docs/contributing/repository-conventions.md) and loaded by the Workshop suites that follow a
# pane to its source. It links what a stranger's recipe links -- the exported pane protocol,
# the surface and input vocabularies, the activation cursor -- and not Workshop's own
# vocabulary, so a header it needed from outside that set would fail here first.
zengine_weave(zengine-example-tally ${CMAKE_SOURCE_DIR}/examples/tally-pane/tally.cpp)
target_link_libraries(zengine-example-tally
    PRIVATE zengine-pane-vocabulary zengine-surface-vocabulary zengine-input-vocabulary
            zengine-activation zengine-warnings zengine-sanitize)

# ...AND THE SECOND EXAMPLE, THE GUARD: the game-like consumer of the second button, which
# blocks while the right button is held and never asks for a menu. Compiled here on Tally's
# terms and loaded by the button suite, so the shipped helper (`workshop/pane_menu.hpp`) is
# exercised from a pane that links only what a stranger's recipe links.
zengine_weave(zengine-example-guard ${CMAKE_SOURCE_DIR}/examples/guard-pane/guard.cpp)
target_link_libraries(zengine-example-guard
    PRIVATE zengine-pane-vocabulary zengine-surface-vocabulary zengine-input-vocabulary
            zengine-activation zengine-warnings zengine-sanitize)

# ...AND THE THIRD, A SECOND PRESENTER: the ordinary replacement of the shipped one. It holds
# `zengine.presenter` in its place, presents a pane's menu its own way (numbered rows, a digit to
# choose, a choice on the release) and keeps the same reload state, so the suites load it instead
# of the shipped presenter and reload it in the shipped one's place while a menu is open. It
# links only what a stranger's recipe links, so a presenter needing more fails here first.
zengine_weave(zengine-example-numbered-presenter
              ${CMAKE_SOURCE_DIR}/examples/numbered-presenter/presenter.cpp)
target_link_libraries(zengine-example-numbered-presenter
    PRIVATE zengine-pane-vocabulary zengine-surface-vocabulary zengine-input-vocabulary
            zengine-activation zengine-component zengine-warnings zengine-sanitize)

# A PANE PROVIDER BUILT AGAINST THE PUBLISHED PROTOCOL ALONE: its own image, compiled from
# weavelib/legacy_pane_protocol.hpp (the pane-action protocol exactly as `84b6bc0` published
# it) and deliberately NOT from zengine-workshop-vocabulary -- the whole claim is that a party
# nobody rebuilt still registers, declares and dispatches with the current host, and a case
# reads its source to check the include is absent. Loaded by the actions suite (through the
# real Workshop) and the load suite (through the real reload).
zengine_weave(zengine-legacy-pane weavelib/legacy_pane.cpp)
target_link_libraries(zengine-legacy-pane
    PRIVATE zengine-surface-vocabulary zengine-input-vocabulary zengine-activation
            zengine-warnings zengine-sanitize)

# A loaded stand-in for the Editor that cannot apply a published claim (WL-OPEN-02): it holds
# `zengine.editor`, offers the pane, claims a document identity and prepares and offers like the
# real image, and, armed through its exposed state, fails the showing of its claim once. It shows
# what the real manager and Workshop do when a real loaded owner fails through the real ABI.
zengine_weave(zengine-failing-editor weavelib/failing_editor.cpp)
target_link_libraries(zengine-failing-editor
    PRIVATE zengine-workshop-vocabulary zengine-surface-vocabulary zengine-activation
            zengine-warnings zengine-sanitize)

# The real Editor built once more with one deliberate throw (WL-OPEN-02): the exact source of
# editor-pane/pane.cpp with ZENGINE_EDITOR_TEST_THROW_ON_B_CPP defined, whose showing hook throws
# once, before anything is activated, for a published path ending in `/b.cpp`. A reload of its
# record into the normal image is the repair the cases witness; the normal image defines none of
# it. Loaded by one suite, named in no host's boot list, never installed.
zengine_weave(zengine-editor-throwing ${PROJECT_SOURCE_DIR}/editor-pane/pane.cpp)
target_compile_definitions(zengine-editor-throwing PRIVATE ZENGINE_EDITOR_TEST_THROW_ON_B_CPP=1)
target_link_libraries(zengine-editor-throwing PRIVATE zengine-workshop-vocabulary zengine-component
                                                     zengine-input-vocabulary zengine-activation)

# THE EDITOR SWITCH'S OTHER CHOICES, for suite `workshop_editor_switch`: the standard Editor's own
# source built under other stems, so a switch runs between two real Editors with no Neovim at all
# -- one plain, three that fail on purpose at exactly one step (a candidate that never answers
# its warm-up, one that refuses every adoption, one that holds the office and says it does not
# serve), and one whose switch away loses something that moves with its document, so consent can
# be asked for and made stale. Each definition compiles into its own image only.
foreach(switch_variant IN ITEMS "b" "silent" "refusing" "not-live" "losing")
    zengine_weave(zengine-editor-pane-${switch_variant} ${PROJECT_SOURCE_DIR}/editor-pane/pane.cpp)
    target_link_libraries(zengine-editor-pane-${switch_variant}
        PRIVATE zengine-workshop-vocabulary zengine-component zengine-input-vocabulary
                zengine-activation)
endforeach()
target_compile_definitions(zengine-editor-pane-silent PRIVATE ZENGINE_EDITOR_TEST_SILENT_WARM=1)
target_compile_definitions(zengine-editor-pane-refusing PRIVATE ZENGINE_EDITOR_TEST_REFUSE_ADOPT=1)
target_compile_definitions(zengine-editor-pane-not-live PRIVATE ZENGINE_EDITOR_TEST_NOT_LIVE=1)
target_compile_definitions(zengine-editor-pane-losing PRIVATE ZENGINE_EDITOR_TEST_LOSSES=1)

# doctest_main.cpp rides every runtime suite: one shared main() that refuses a run selecting zero
# test cases, which stock doctest calls a success (VM-POP-04).
add_executable(zengine-tests test_snake.cpp doctest_main.cpp)
target_link_libraries(zengine-tests
    PRIVATE loom::kernel loom::switchboard loom::core zengine-surface-vocabulary
            zengine-warnings zengine-sanitize)
target_include_directories(zengine-tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
    ${CMAKE_SOURCE_DIR}/snake)

# The suites drive the real libraries through the real kernel, and the build hands them their
# paths as the Loom's own harness does.
target_compile_definitions(zengine-tests PRIVATE
    "SNAKE_SO_WORLD_V1=\"$<TARGET_FILE:snake-world-v1>\""
    "SNAKE_SO_WORLD_V2=\"$<TARGET_FILE:snake-world-v2>\""
    "SNAKE_SO_SCORE=\"$<TARGET_FILE:snake-score>\""
    "SKIN_SO_TUI_CLASSIC=\"$<TARGET_FILE:zengine-skin-tui-classic>\""
    "SKIN_SO_TUI_BLOCK=\"$<TARGET_FILE:zengine-skin-tui-block>\""
    # The host's source, for the tripwire that no privileged wind returns: a root Drive is
    # inert, so only reading the file can catch one (VM-WALL-11).
    "HOST_PLAY_CPP=\"${CMAKE_SOURCE_DIR}/snake/play.cpp\""
    # The clock adapter's source, for the binding tripwire: hand-written ceremony and the
    # binding behave identically, so only the file tells which is there (VM-WALL-11).
    "CLOCK_CPP=\"${CMAKE_SOURCE_DIR}/snake/clock.cpp\"")

zengine_doctest_test(snake zengine-tests)

# The Timer suite: the contract pins, every schedule over a fake clock, the real library
# re-winding itself on the real clock, and the world, the input weave and the skin kept moving
# with nobody pumping them.
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
    # The continuity cases: virtual time, real everything else.
    "TIMER_VIRTUAL_SO=\"$<TARGET_FILE:zengine-timer-virtual>\""
    "PROBE_ONESHOT_SO=\"$<TARGET_FILE:zengine-probe-oneshot>\""
    "PROBE_REQUIRED_SO=\"$<TARGET_FILE:zengine-probe-required>\""
    # The prepared candidate, and the one that refuses to become the Timer.
    "TIMER_VIRTUAL_V2_SO=\"$<TARGET_FILE:zengine-timer-virtual-v2>\""
    "TIMER_DECLINES_SO=\"$<TARGET_FILE:zengine-timer-declines>\"")

zengine_doctest_test(timer zengine-timer-tests)

# ---- the compile-negative lane: the Timer suite's walls -----------------------------------
# A compile-time wall is proved by building the offending weave and judging its diagnostic,
# beside a control that must build (VM-WALL-04). The fixtures (tests/compile_negative/) build in a
# tree of their own, configured here and built by each test, never this tree (VM-LANE-17,
# VM-LANE-18); it fetches nothing and cannot re-run CMake (VM-LANE-19), and it is prepared at
# configure because no target could carry it as a build step (VM-LANE-20).
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
# The Loom this tree resolved, by either route: a fixture that found another Loom would be
# judging another wall (VM-LANE-18).
if(ZEN_LOOM_DEV)
    list(APPEND zengine_cn_args "-DZEN_LOOM_DEV=ON")
else()
    list(APPEND zengine_cn_args "-Dloom_DIR=${loom_DIR}")
endif()

# The fixture tree is the leaf: no tests, no SDL fetch and no regeneration (VM-LANE-19), and no
# install, which cmake/ZengineInstall.cmake would refuse beside a sibling Loom in dev mode.
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

# zengine_compile_test() is this lane's one registration path: it adds the entry, attaches the
# PASS_REGULAR_EXPRESSION that judges it and records it as diagnostic-judged, which the manifest
# then requires, so a diagnostic that quietly went away is a red (VM-WALL-04).
zengine_compile_test(timer_raw_activation_refused zengine-cn-collision "on_timed_activation")

zengine_compile_test(timer_activation_hook_compiles zengine-cn-hook)

# Two spellings of one refusal: GCC reaches the authored static_assert in timer/binding.hpp and
# prints the sentence that tells a maker what to add; MSVC's requires-expression accepts the
# probe, and the wall surfaces one layer out as the conversion failure. The wall holds on both,
# the guidance is GCC's alone, and each alternative would disappear if the wall stopped working.
zengine_compile_test(timer_missing_using_refused zengine-cn-missing-using
                     "using TimedWeave::on|cannot convert argument 1 from 'const zengine::timer::TimerFired'")

# The same defect in a weave that declares no binding and places its schedules with the raw
# protocol, the documented answer when a delay is runtime data: the wall is anchored in the
# constructor so such a weave meets it too (VM-WALL-01). The two spellings are the case above's.
zengine_compile_test(timer_missing_using_no_binding_refused zengine-cn-missing-using-no-binding
                     "using TimedWeave::on|cannot convert argument 1 from 'const zengine::timer::TimerFired'")

# The hook runs inside an accepted activation, so it must not accept again: `activation()` hands
# back a const& and `accept` is non-const. Only this case keeps that true, since no runtime case
# can produce an input the language refuses. GCC says "discards qualifiers" and MSVC "Conversion
# loses qualifiers": both reach the refusal, so one pattern covers them.
zengine_compile_test(timer_cursor_bypass_refused zengine-cn-cursor
                     "(discards|loses) qualifiers")

# The Input suite, its own executable so each package's count stays its own; it links the Timer
# vocabulary because the weave's own beat is part of its contract.
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
    # The SDL reader's own sources, for the tripwire that it never touches the clipboard: a
    # claim about what code does not contain holds on every lane, those without SDL included,
    # and running the code cannot prove it (VM-WALL-11).
    "INPUT_SDL_READER_CPP=\"${CMAKE_SOURCE_DIR}/input/input_sdl.cpp\""
    "INPUT_SDL_TRANSLATE_HPP=\"${CMAKE_SOURCE_DIR}/input/translate_sdl.hpp\"")

zengine_doctest_test(input zengine-input-tests)

# The SDL reader is a conditional subpopulation (VM-POP-08). The SDL translation is pure and
# counts in the always row; what rides `sdl` is the one case that cannot be pure: the process
# pushes events into a real SDL queue and a separately loaded zengine-input-sdl polls them out,
# the evidence that the two SDL weaves share one SDL. So the suite links SDL itself.
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

# ---- the Neovim hosting library ----------------------------------------------------------
# Two suites, split as `sdl` splits: `neovim` is the arithmetic, framing and custody, and the
# conversation owner walked through every failing start by a scriptable fake Neovim, on every
# lane; `neovim_live` is what only a real Neovim can say. The live one is registered only when a
# Neovim is named at configure time, and naming one that is not there is an error, not a skip.
add_executable(zengine-neovim-fixture neovim_fixture.cpp)
target_link_libraries(zengine-neovim-fixture PRIVATE zengine-neovim zengine-warnings zengine-sanitize)

add_executable(zengine-neovim-tests test_neovim.cpp doctest_main.cpp)
target_link_libraries(zengine-neovim-tests
    PRIVATE zengine-neovim zengine-builder-vocabulary zengine-warnings zengine-sanitize)
target_include_directories(zengine-neovim-tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
target_compile_definitions(zengine-neovim-tests PRIVATE
    "NEOVIM_FIXTURE=\"$<TARGET_FILE:zengine-neovim-fixture>\"")
add_dependencies(zengine-neovim-tests zengine-neovim-fixture)
zengine_doctest_test(neovim zengine-neovim-tests)

set(ZENGINE_NEOVIM_PROGRAM "" CACHE FILEPATH
    "A Neovim (0.11 or newer) for the neovim-gated live suites; empty registers none")
if(ZENGINE_NEOVIM_PROGRAM)
    if(NOT EXISTS "${ZENGINE_NEOVIM_PROGRAM}")
        message(FATAL_ERROR
            "zengine: ZENGINE_NEOVIM_PROGRAM names ${ZENGINE_NEOVIM_PROGRAM}, which does not exist. "
            "The `neovim` gate opens for a Neovim that is there; clear the variable to run without it.")
    endif()
    add_executable(zengine-neovim-live-tests test_neovim_live.cpp doctest_main.cpp)
    target_link_libraries(zengine-neovim-live-tests
        PRIVATE zengine-neovim zengine-warnings zengine-sanitize)
    target_include_directories(zengine-neovim-live-tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
    target_compile_definitions(zengine-neovim-live-tests PRIVATE
        "NEOVIM_PROGRAM=\"${ZENGINE_NEOVIM_PROGRAM}\"")
    zengine_doctest_test(neovim_live zengine-neovim-live-tests)
    set_property(GLOBAL APPEND PROPERTY ZENGINE_ACTIVE_GATES "neovim")
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

# The SDL skin's cases are a conditional subpopulation, declared, not absorbed (VM-POP-08):
# test_surface.cpp holds them behind SURFACE_HAS_SDL, and the Windows stranger lane builds no SDL
# skin. One `if` sets the compile definition that admits them, the environment they need and the
# gate their floor hangs on, so the three cannot disagree about whether SDL is here: a second way
# of asking would be a second thing that can be wrong, silently downward.
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

# The UI suite: the authored/resolved distinction with no application around it, in its own
# executable so its count stays its own. It needs no kernel and no weave library: loom::core is
# here for zen/weave/shape.hpp and the schema derivation the contract case checks.
add_executable(zengine-ui-tests test_ui.cpp doctest_main.cpp)
target_link_libraries(zengine-ui-tests
    PRIVATE loom::core zengine-ui-vocabulary zengine-warnings zengine-sanitize)
target_include_directories(zengine-ui-tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party)

zengine_doctest_test(ui zengine-ui-tests)

# The Component suite links zengine-component and nothing else, not even loom::core: a TextBox is
# a std::string and two indices with no wire form, so a suite that had to boot anything to test
# it would test something else. Its own executable, so its count stays its own.
add_executable(zengine-component-tests test_component.cpp doctest_main.cpp)
target_link_libraries(zengine-component-tests
    PRIVATE zengine-component zengine-warnings zengine-sanitize)
target_include_directories(zengine-component-tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party)

zengine_doctest_test(component zengine-component-tests)

# ---- the Editor buffer suite: the multiline document as values -------------------------
# The buffer, the source byte law, the tab geometry and the paste flattening are
# editor-pane/editor.hpp's; what they do is this suite's claim, in its own executable with an
# include set that can name nothing of the host. That the Editor pane gets its answers from it
# is the panes suite's claim.
add_executable(zengine-editor-tests test_editor.cpp doctest_main.cpp)
target_link_libraries(zengine-editor-tests
    PRIVATE zengine-component zengine-workshop-vocabulary zengine-input-vocabulary
            zengine-warnings zengine-sanitize)
target_include_directories(zengine-editor-tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party)

zengine_doctest_test(editor zengine-editor-tests)

# ---- the Operator suite, and the stranger that reads a rule it did not write -------------
# `zengine-operator-stranger` is its own target so its link line can show its independence: it
# names `zengine-operator` and nothing else, and reads every port off the operator's own schema.
# These are header-only packages, so a link line cannot stop an edit including sideways; what a
# reviewer can check is that the file names no timer symbol and no timer string.
add_library(zengine-operator-stranger STATIC operator_stranger.cpp)
target_link_libraries(zengine-operator-stranger
    PUBLIC zengine-operator
    PRIVATE zengine-warnings zengine-sanitize)
target_include_directories(zengine-operator-stranger PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# One entry, six files, each the next question: what an operator is (test_operator.cpp, no
# kernel); whether a consumer in another image can spend the host's (_host); whether the Timer and
# that stranger spend one catalog (_canonical); where the host's powers came from (_provider);
# what a zero-input source costs to sample and enumerate (_source); and what makes a power a
# conversion (_migration). The population counts entries, not files (VM-POP-12).
add_executable(zengine-operator-tests test_operator.cpp test_operator_host.cpp
                                     test_operator_canonical.cpp test_operator_provider.cpp
                                     test_operator_source.cpp test_operator_migration.cpp
                                     doctest_main.cpp)
target_link_libraries(zengine-operator-tests
    PRIVATE loom::kernel loom::switchboard loom::core zengine-operator
            zengine-operator-stranger zengine-timer-vocabulary zengine-warnings
            zengine-sanitize)
target_include_directories(zengine-operator-tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
    ${CMAKE_CURRENT_SOURCE_DIR}/weavelib)

target_compile_definitions(zengine-operator-tests PRIVATE
    # The shipped Timer beside the loaded stranger in one process: "one live catalog" is a
    # claim about the two together, which neither alone can carry.
    "TIMER_SO=\"$<TARGET_FILE:zengine-timer>\""
    # The production host's source, for the ownership tripwire: Workshop's `main()` claims a
    # terminal, so no case can run it, and the arrangement is read off the file (VM-WALL-11).
    "WORKSHOP_HOST_CPP=\"${CMAKE_SOURCE_DIR}/workshop/workshop.cpp\""
    # ...and the executor, where mount-before-offer-before-load is written, for the same one.
    "WORKSHOP_LOAD_EXECUTE_HPP=\"${CMAKE_SOURCE_DIR}/workshop/load_execute.hpp\""
    "OPH_STRANGER_SO=\"$<TARGET_FILE:zengine-operator-consumer-weave>\""
    "OPH_STRANGER_LEGACY_SO=\"$<TARGET_FILE:zengine-operator-consumer-weave-legacy>\""
    "OPH_STRANGER_ABI_SO=\"$<TARGET_FILE:zengine-operator-consumer-weave-abi>\""
    # A real weave written before the operator seam, built by another package's rules: it meets
    # the offer path and is simply not a consumer, which the -legacy fixture alone cannot show.
    "OPH_UNTOUCHED_WEAVE_SO=\"$<TARGET_FILE:zengine-workshop-hello>\""
    # The stranger's source, for the independence tripwire: only reading the file can tell
    # whether it still names nothing from the host's side of the seam (VM-WALL-11).
    "OPH_STRANGER_CPP=\"${CMAKE_CURRENT_SOURCE_DIR}/weavelib/operator_consumer_weave.cpp\""
    # The shipped basic provider, not a fixture: the suite drives the production file the
    # Workshop mounts, since a stand-in would prove the seam and not the product.
    "PROVIDER_BASIC_SO=\"$<TARGET_FILE:zengine-operators-basic>\""
    "PROVIDER_A_SO=\"$<TARGET_FILE:zengine-provider-a>\""
    "PROVIDER_B_SO=\"$<TARGET_FILE:zengine-provider-b>\""
    "PROVIDER_WRONG_SO=\"$<TARGET_FILE:zengine-provider-b-wrong>\""
    "PROVIDER_ABI_SO=\"$<TARGET_FILE:zengine-provider-abi>\""
    "PROVIDER_MIN_SO=\"$<TARGET_FILE:zengine-provider-min>\""
    # The zero-input source's provider: only a real module boundary shows that a definition with
    # no input ports survives the real encode and decode; the codec case beside it asks the rest.
    "PROVIDER_SOURCE_SO=\"$<TARGET_FILE:zengine-provider-source>\""
    # The Source seam's own source, for its fence: operator/source.hpp stays a projection over
    # the catalog, with no bus, Sense, kernel or loader. This suite links the switchboard, so no
    # link line can say that; only the file can (VM-WALL-11).
    "OPERATOR_SOURCE_HPP=\"${CMAKE_SOURCE_DIR}/operator/source.hpp\""
    # The loaded stranger again: the real Timer override witness needs a consumer in a third
    # image to agree with the Timer, and a stranger sharing no code with it is the only one.
    "PROV_STRANGER_SO=\"$<TARGET_FILE:zengine-operator-consumer-weave>\""
    # ...and a real weave built by another package's rules, so "most artifacts are not
    # providers" is measured on one.
    "PROV_UNTOUCHED_WEAVE_SO=\"$<TARGET_FILE:zengine-workshop-hello>\""
    # Four real images over one invented history: which roads exist means mounted from a file,
    # not published by the suite into its own catalog. The signature questions need no image and
    # are asked in process beside them.
    "PROVIDER_MIG_CHAIN_SO=\"$<TARGET_FILE:zengine-provider-mig-chain>\""
    "PROVIDER_MIG_DIRECT_SO=\"$<TARGET_FILE:zengine-provider-mig-direct>\""
    "PROVIDER_MIG_ALT_SO=\"$<TARGET_FILE:zengine-provider-mig-direct-alt>\""
    "PROVIDER_MIG_COMPOSED_SO=\"$<TARGET_FILE:zengine-provider-mig-composed>\""
    # The migration seam's own source, for its fence: operator/migration.hpp stays a projection
    # over the catalog, with no image, loader, plan or filesystem; only the file can say so
    # (VM-WALL-11).
    "OPERATOR_MIGRATION_HPP=\"${CMAKE_SOURCE_DIR}/operator/migration.hpp\"")
add_dependencies(zengine-operator-tests
    zengine-provider-mig-chain zengine-provider-mig-direct
    zengine-provider-mig-direct-alt zengine-provider-mig-composed)

zengine_doctest_test(operator zengine-operator-tests)

# The Composer suite: what a maker authored, what it composes to and what they would see. It
# links no switchboard: composer/draft.hpp and composer/view.hpp know no bus, Kernel or medium,
# and where a fact comes from is the Workshop suite's claim, since a seam is proved where it is.
# It links loom::terminal because every refusal read back here is `compose_message`'s, in its words.
add_executable(zengine-composer-tests test_composer.cpp doctest_main.cpp)
target_link_libraries(zengine-composer-tests
    PRIVATE zengine-composer-draft zengine-warnings zengine-sanitize)
target_include_directories(zengine-composer-tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party)

zengine_doctest_test(composer zengine-composer-tests)

# ---- the Files suite: the pure half, in the package that owns it ------------------------
# The listing, the marks and their durable file, linked against `zengine-files-os` (the two
# platform bodies) and not `zengine-workshop-logic`, so a green proves the pure half portable.
# The weave's interaction over the pane protocol is proved in `workshop_panes`.
add_executable(zengine-files-tests test_files.cpp doctest_main.cpp)
target_link_libraries(zengine-files-tests
    PRIVATE zengine-files-os zengine-workshop-vocabulary zengine-warnings zengine-sanitize)
target_include_directories(zengine-files-tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
# ...AND TWO BUILD FILES, READ AS TEXT. "This image links no host target" is a claim about
# the build graph, and the build graph is what these two files say -- so the suite reads
# them rather than asserting a consequence that would still hold for a while after the
# edge was added.
target_compile_definitions(zengine-files-tests PRIVATE
    "FILES_CMAKE=\"${CMAKE_SOURCE_DIR}/files/CMakeLists.txt\""
    "WORKSHOP_CMAKE=\"${CMAKE_SOURCE_DIR}/workshop/CMakeLists.txt\"")

zengine_doctest_test(files_weave zengine-files-tests)

# ---- the Maker suite: a weave from a maker's definition --------------------------------
# Whether one weave registered from data (state schema, accepted and emitted shapes, triggers
# composed over the host's catalog) behaves as a native one: accepts, spends, writes back, emits,
# is inspected and edited live, and survives a process as two files. It links the switchboard and
# not the kernel: a definition is registered, never loaded. `zengine-maker-author` is the fresh
# process the two-files case runs through `std::system`, asserting its status first.
add_executable(zengine-maker-author maker_author.cpp)
target_link_libraries(zengine-maker-author
    PRIVATE zengine-maker loom::switchboard loom::core zengine-warnings zengine-sanitize)

add_executable(zengine-maker-tests test_maker.cpp doctest_main.cpp)
target_link_libraries(zengine-maker-tests
    PRIVATE zengine-maker loom::switchboard loom::core zengine-warnings zengine-sanitize)
target_include_directories(zengine-maker-tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
add_dependencies(zengine-maker-tests zengine-maker-author)
target_compile_definitions(zengine-maker-tests PRIVATE
    "MAKER_AUTHOR_EXE=\"$<TARGET_FILE:zengine-maker-author>\""
    # The definition header's SOURCE, for the no-author-field tripwire: a property that is true
    # because of what a file does NOT contain is read off the file (VM-WALL-10).
    "MAKER_DEFINITION_HPP=\"${CMAKE_SOURCE_DIR}/maker/definition.hpp\"")

zengine_doctest_test(maker zengine-maker-tests)

# Generated native rules and the ordinary Loom weave in one artifact. These are
# generated during the build from the same public authoring API as the workbench.
add_executable(zengine-flow-fixture-generator flow_generate.cpp)
target_link_libraries(zengine-flow-fixture-generator PRIVATE zengine-flow zengine-warnings zengine-sanitize)
set(flow_generated_dir "${CMAKE_CURRENT_BINARY_DIR}/flow-generated")
set(flow_generated_sources "")
foreach(stem IN ITEMS thermostat boundaries high_water empty)
    list(APPEND flow_generated_sources "${flow_generated_dir}/${stem}.cpp")
endforeach()
add_custom_command(OUTPUT ${flow_generated_sources}
    COMMAND $<TARGET_FILE:zengine-flow-fixture-generator> "${flow_generated_dir}"
    DEPENDS zengine-flow-fixture-generator
    VERBATIM)
foreach(stem IN ITEMS thermostat boundaries high_water empty)
    zengine_weave(zengine-flow-${stem} "${flow_generated_dir}/${stem}.cpp")
    target_link_libraries(zengine-flow-${stem} PRIVATE zengine-flow)
endforeach()
add_executable(zengine-flow-tests test_flow.cpp test_flow_runtime.cpp test_flow_graph.cpp test_flow_view.cpp doctest_main.cpp)
target_link_libraries(zengine-flow-tests PRIVATE zengine-flow loom::kernel zengine-warnings zengine-sanitize)
target_include_directories(zengine-flow-tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
target_compile_definitions(zengine-flow-tests PRIVATE
    "FLOW_THERMOSTAT=\"$<TARGET_FILE:zengine-flow-thermostat>\""
    "FLOW_BOUNDARIES=\"$<TARGET_FILE:zengine-flow-boundaries>\""
    "FLOW_HIGH_WATER=\"$<TARGET_FILE:zengine-flow-high_water>\""
    "FLOW_EMPTY=\"$<TARGET_FILE:zengine-flow-empty>\"")
add_dependencies(zengine-flow-tests zengine-flow-thermostat zengine-flow-boundaries
    zengine-flow-high_water zengine-flow-empty)
zengine_doctest_test(flow zengine-flow-tests)
add_executable(zengine-flow-pane-tests test_flow_pane.cpp doctest_main.cpp)
target_link_libraries(zengine-flow-pane-tests PRIVATE zengine-flow zengine-activation loom::kernel zengine-warnings zengine-sanitize)
target_include_directories(zengine-flow-pane-tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
target_compile_definitions(zengine-flow-pane-tests PRIVATE "FLOW_PANE_ARTIFACT=\"$<TARGET_FILE:zengine-flow-pane>\"")
add_dependencies(zengine-flow-pane-tests zengine-flow-pane)
zengine_doctest_test(flow_pane zengine-flow-pane-tests)

add_executable(zengine-message-draft-tests test_message_draft.cpp doctest_main.cpp)
target_link_libraries(zengine-message-draft-tests PRIVATE zengine-message-draft zengine-warnings zengine-sanitize)
target_include_directories(zengine-message-draft-tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
zengine_doctest_test(message_draft zengine-message-draft-tests)

# ---- THE SOURCE-TRANSFER SUITE: the material editors exchange with Inventory, as pure functions --
# The byte laws, the Terminal line (proved by Loom's lexer), reading a dropped pair, the capture
# pairs and the C++ generator, whose exact outputs -- for a saved EnsureTimer, and for a value whose
# strings hold NULs (`source_transfer_samples.hpp`) -- are the golden files the program test below
# compiles against the installed Loom package and invokes.
add_executable(zengine-source-transfer-tests test_source_transfer.cpp doctest_main.cpp)
target_link_libraries(zengine-source-transfer-tests
    PRIVATE zengine-source-transfer zengine-timer-vocabulary zengine-warnings zengine-sanitize)
target_include_directories(zengine-source-transfer-tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
target_compile_definitions(zengine-source-transfer-tests PRIVATE
    "SOURCE_TRANSFER_GOLDEN=\"${CMAKE_CURRENT_SOURCE_DIR}/source_transfer_ensure_timer.generated.hpp\""
    "SOURCE_TRANSFER_BYTES_GOLDEN=\"${CMAKE_CURRENT_SOURCE_DIR}/source_transfer_string_bytes.generated.hpp\"")
zengine_doctest_test(source_transfer zengine-source-transfer-tests)

add_executable(zengine-source-transfer-cpp-witness source_transfer_cpp_witness.cpp)
target_link_libraries(zengine-source-transfer-cpp-witness
    PRIVATE loom::core zengine-timer-vocabulary zengine-warnings zengine-sanitize)
target_include_directories(zengine-source-transfer-cpp-witness PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
zengine_program_test(source_transfer_cpp zengine-source-transfer-cpp-witness)

# ---- THE INVENTORY SUITE: the byte-envelope codec and the weave's own Set/Get/CaptureDescribe --
#
# inventory/codec.hpp, inventory/vocabulary.hpp, inventory/weave.hpp -- exercised on an
# in-process Switchboard with a trusted grant, so this suite proves the weave's own contract.
# Header-only, so it needs no kernel, exactly like the guest vocabulary suite beside it. What an
# actual GUEST's "inventory" power reaches is test_workshop_guests.cpp's.
add_executable(zengine-inventory-tests test_inventory.cpp doctest_main.cpp)
target_link_libraries(zengine-inventory-tests
    PRIVATE loom::kernel loom::switchboard loom::core zengine::inventory zengine-warnings zengine-sanitize)
target_include_directories(zengine-inventory-tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
target_compile_definitions(zengine-inventory-tests PRIVATE
    "INVENTORY_SO=\"$<TARGET_FILE:zengine-inventory>\"")
add_dependencies(zengine-inventory-tests zengine-inventory)
zengine_doctest_test(inventory zengine-inventory-tests)



# ---- the UI fence's compile-negative lane ----------------------------------------------
# The authored/resolved fence is proved as the Timer's walls are (VM-WALL-04): each case builds an
# offending type in the fixtures' own tree (tests/compile_negative/) and is judged on its
# diagnostic, beside a control that must build.
zengine_compile_test(ui_authored_extent_required zengine-cn-ui-extent
                     "a resolvable dimension must be an authored Extent")

zengine_compile_test(ui_resolved_geometry_refused zengine-cn-ui-resolved
                     "no resolved rectangle may live on an authored element")

zengine_compile_test(ui_authored_element_compiles zengine-cn-ui-ok)

# ---- the Workshop suites --------------------------------------------------------------
# One entry per subject (VM-POP-11), an entry holding several sources where one object cannot
# (VM-POP-12), and tests/workshop_support.hpp what several need (VM-POP-13). They prove Workshop
# headless, its screen a returned canvas; a Skin carrying it to a maker is the Surface suite's
# and the live run's. A case that loads a product loads the library a maker runs. Only
# `workshop_load` loads the real Timer: in a rig that drains to idle its beat never returns.
set(ZENGINE_WORKSHOP_LIBS
    loom::kernel loom::switchboard loom::core loom::terminal
    # The vocabulary is the headers and the logic their bodies, compiled once
    # (workshop/CMakeLists.txt): a suite that included the bodies would emit them all again.
    zengine-workshop-vocabulary zengine-workshop-logic zengine-workshop-load
    zengine-surface-vocabulary zengine-ui-vocabulary
    zengine-introspection-view zengine-composer-vocabulary
    zengine-timer-vocabulary zengine-warnings
    zengine-sanitize)

# The artifacts `workshop_support.hpp` itself names, so every suite that includes it needs
# them. The three fixtures stand in for the things a plan can point at and get wrong;
# `zengine-operators-basic` is what the shipped plan actually names.
set(ZENGINE_WORKSHOP_DEFS
    # Workshop's presentation sources as a directory, not a list, which fails open: a body moved
    # into a new file is not on it. `presentation_sources()` (workshop_support.hpp) walks it,
    # asserts a floor on what it found and excludes the host's side of the seam by name.
    "WORKSHOP_SOURCE_DIR=\"${CMAKE_SOURCE_DIR}/workshop\""
    "WORKSHOP_SO_HELLO=\"$<TARGET_FILE:zengine-workshop-hello>\""
    # ...AND THE MAKER'S EXAMPLE PANE, with its source: the Edit Code cases load the image
    # and open the file, so the path a recipe names is the file this tree compiled.
    "WORKSHOP_SO_TALLY=\"$<TARGET_FILE:zengine-example-tally>\""
    "TALLY_SOURCE=\"${CMAKE_SOURCE_DIR}/examples/tally-pane/tally.cpp\""
    "WORKSHOP_SO_GUARD=\"$<TARGET_FILE:zengine-example-guard>\""
    "GUARD_SOURCE=\"${CMAKE_SOURCE_DIR}/examples/guard-pane/guard.cpp\""
    # ...AND THE TWO PRESENTERS: the shipped one every menu case loads, and the example that
    # replaces it (loaded in its place, and reloaded over it while a menu is open).
    "WORKSHOP_SO_MENU_PRESENTER=\"$<TARGET_FILE:zengine-menu-presenter>\""
    "WORKSHOP_SO_NUMBERED_PRESENTER=\"$<TARGET_FILE:zengine-example-numbered-presenter>\""
    "WORKSHOP_SO_INTROSPECTION=\"$<TARGET_FILE:zengine-introspection>\""
    # The loaded panes are shared definitions, not one suite's: `PaneRig::artifact_path`
    # (workshop_support.hpp) resolves a plan row's stem to a file in every suite with the rig.
    "WORKSHOP_SO_FILES=\"$<TARGET_FILE:zengine-files>\""
    "WORKSHOP_SO_BUILDER_PANE=\"$<TARGET_FILE:zengine-builder-pane>\""
    "WORKSHOP_SO_ATTENTION_PANE=\"$<TARGET_FILE:zengine-attention-pane>\""
    # The Info pane: the shipped desk names it (`default_setup`), so here a fresh Workshop's
    # own second pane resolves.
    "WORKSHOP_SO_INFO_PANE=\"$<TARGET_FILE:zengine-info-pane>\""
    "WORKSHOP_SO_CONNECTIONS_PANE=\"$<TARGET_FILE:zengine-connections-pane>\""
    "WORKSHOP_SO_DESKTOP_PANE=\"$<TARGET_FILE:zengine-desktop-pane>\""
    "WORKSHOP_SO_TERMINAL_PANE=\"$<TARGET_FILE:zengine-terminal-pane>\""
    "WORKSHOP_SO_EDITOR_PANE=\"$<TARGET_FILE:zengine-editor-pane>\""
    # ...AND THE PROVIDER BUILT AGAINST THE PUBLISHED PROTOCOL ALONE, with its source, which a
    # case reads to check that the current vocabulary is nowhere in it.
    "WORKSHOP_SO_LEGACY_PANE=\"$<TARGET_FILE:zengine-legacy-pane>\""
    "LEGACY_PANE_SOURCE=\"${CMAKE_CURRENT_SOURCE_DIR}/weavelib/legacy_pane.cpp\""
    "WORKSHOP_SO_FAILING_EDITOR=\"$<TARGET_FILE:zengine-failing-editor>\""
    # ...and the real Editor built with one deliberate throw.
    "WORKSHOP_SO_EDITOR_THROWING=\"$<TARGET_FILE:zengine-editor-throwing>\""
    "PROVIDER_BASIC_SO=\"$<TARGET_FILE:zengine-operators-basic>\""
    # `math.max` supplied as a minimum at the same signature: the substitution instrument.
    "PROVIDER_MIN_SO=\"$<TARGET_FILE:zengine-provider-min>\""
    # The genericity witness: `zengine-provider-a` supplies `prov.function.1/2/3` under a
    # provider identity no source outside tests/ names, so a projection that had to be edited
    # to show them would not be a projection over the store.
    "PROVIDER_A_SO=\"$<TARGET_FILE:zengine-provider-a>\""
    # The evaluation witness needs a real cross-image source: `prov.source.spends` counts its
    # own spends in its own image, since `op::invocations()` is a vague-linkage static that
    # cannot see a body there, so answers of 1, 2, 3 tell an evaluation from a cached answer.
    "PROVIDER_SOURCE_SO=\"$<TARGET_FILE:zengine-provider-source>\"")
set(ZENGINE_WORKSHOP_FIXTURES
    zengine-workshop-hello zengine-introspection zengine-files zengine-builder-pane
    zengine-attention-pane zengine-info-pane zengine-desktop-pane zengine-terminal-pane
    zengine-connections-pane
    zengine-legacy-pane
    zengine-failing-editor zengine-editor-throwing
    zengine-operators-basic zengine-provider-min zengine-provider-a
    zengine-provider-source zengine-example-tally zengine-example-guard
    zengine-menu-presenter zengine-example-numbered-presenter)

# One Workshop suite: the shared link line, artifact paths and includes plus what this one alone
# needs, so the suites agree by construction (a second copy of a link line is a second answer).
# `SOURCES` is a list because a suite is not a file (VM-POP-12): every source gets the target's
# definitions, includes and link line. Both arms fail closed, since a mistyped keyword would
# otherwise be dropped silently and a test family go missing while everything reports green.
function(zengine_workshop_suite entry)
    cmake_parse_arguments(WS "" "" "SOURCES;DEFINITIONS;NEEDS;LINKS" ${ARGN})
    if(NOT WS_SOURCES)
        message(FATAL_ERROR
            "zengine_workshop_suite(${entry}): no SOURCES. A suite with no source would "
            "build an empty binary that selects zero cases.")
    endif()
    if(WS_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR
            "zengine_workshop_suite(${entry}): unrecognized argument(s) "
            "'${WS_UNPARSED_ARGUMENTS}'. Sources go after SOURCES; a source left outside it "
            "would not be compiled and its cases would vanish from the population.")
    endif()
    set(target zengine-workshop-${entry}-tests)
    add_executable(${target} ${WS_SOURCES} doctest_main.cpp)
    target_link_libraries(${target} PRIVATE ${ZENGINE_WORKSHOP_LIBS} ${WS_LINKS})
    target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
    # The suite's own name: the suites run at once, and a temporary directory belongs to one
    # of them (VM-POP-14).
    target_compile_definitions(${target} PRIVATE
        "ZENGINE_WORKSHOP_SUITE=\"${entry}\"" ${ZENGINE_WORKSHOP_DEFS} ${WS_DEFINITIONS})
    add_dependencies(${target} ${ZENGINE_WORKSHOP_FIXTURES} ${WS_NEEDS})
    zengine_doctest_test(workshop_${entry} ${target})
endfunction()

# THE EDITOR SWITCH, IN A LIVE WORKSHOP WITH NO NEOVIM: the real Editor and copies of it under
# other stems (above), switched through the real coordinator and Loom's prepared replacement.
zengine_workshop_suite(editor_switch SOURCES test_workshop_editor_switch.cpp
    DEFINITIONS "WORKSHOP_SO_EDITOR_SWITCH_B=\"$<TARGET_FILE:zengine-editor-pane-b>\""
                "WORKSHOP_SO_EDITOR_SWITCH_SILENT=\"$<TARGET_FILE:zengine-editor-pane-silent>\""
                "WORKSHOP_SO_EDITOR_SWITCH_REFUSING=\"$<TARGET_FILE:zengine-editor-pane-refusing>\""
                "WORKSHOP_SO_EDITOR_SWITCH_NOT_LIVE=\"$<TARGET_FILE:zengine-editor-pane-not-live>\""
                "WORKSHOP_SO_EDITOR_SWITCH_LOSING=\"$<TARGET_FILE:zengine-editor-pane-losing>\""
    NEEDS zengine-editor-pane zengine-editor-pane-b zengine-editor-pane-silent
          zengine-editor-pane-refusing zengine-editor-pane-not-live zengine-editor-pane-losing)

# THE NEOVIM-BACKED EDITOR IN A LIVE WORKSHOP. Always: the choice with no Neovim, and with a fake
# one that fails at start in each way a real one can (`neovim-fixture`). Behind the `neovim` gate:
# the real Neovim named at configure time, carrying documents both ways, editing, opening,
# quitting, the clipboard and the refusals only a real Neovim can produce.
set(zengine_neovim_suite_defs
    "WORKSHOP_SO_NEOVIM_EDITOR=\"$<TARGET_FILE:zengine-neovim-editor>\""
    "NEOVIM_FIXTURE=\"$<TARGET_FILE:zengine-neovim-fixture>\"")
if(ZENGINE_NEOVIM_PROGRAM)
    list(APPEND zengine_neovim_suite_defs "NEOVIM_PROGRAM=\"${ZENGINE_NEOVIM_PROGRAM}\"")
endif()
# ...and, in its second source, the Neovim-backed Editor in Workshop's typed carry, through the same
# loaded Inventory, pane and menu presenter the standard Editor's transfer cases use
# (`editor_transfer_story.hpp`); every case there rides the `neovim` gate.
zengine_workshop_suite(neovim SOURCES test_workshop_neovim.cpp test_workshop_neovim_transfers.cpp
    DEFINITIONS ${zengine_neovim_suite_defs}
                "WORKSHOP_SO_INVENTORY_PANE=\"$<TARGET_FILE:zengine-inventory-pane>\""
                "WORKSHOP_SO_INVENTORY=\"$<TARGET_FILE:zengine-inventory>\""
    NEEDS zengine-editor-pane zengine-neovim-editor zengine-neovim-fixture zengine-inventory
          zengine-inventory-pane)

zengine_workshop_suite(document SOURCES test_workshop_document.cpp
    DEFINITIONS "WORKSHOP_SO_COMPOSER=\"$<TARGET_FILE:zengine-composer>\""
    NEEDS zengine-composer)

zengine_workshop_suite(screen SOURCES test_workshop_screen.cpp)

# THE DEVELOPMENT LAUNCH AS SEPARATE PROCESSES (WL-CODE-08). Two launches of one runtime meet only
# across processes, so the cases that pin who holds one start this driver -- the same `launch`
# through the same `real_world` doors as `zengine-workshop-develop`, with a case's facts instead of
# a tree's -- and a stand-in host that counts itself into its project directory and waits for the
# case to let it go. Neither is a weave, an artifact, or anything a build runs.
add_executable(zengine-launch-fixture-host launch_fixture_host.cpp)
target_link_libraries(zengine-launch-fixture-host PRIVATE zengine-warnings zengine-sanitize)
add_executable(zengine-launch-fixture-driver launch_fixture_driver.cpp)
target_link_libraries(zengine-launch-fixture-driver
    PRIVATE zengine-builder-vocabulary zengine-warnings zengine-sanitize)

# The project browser: enumeration and its bound, the order, name admission, the root boundary,
# press and wheel routing and a row's path to the Editor, all under temporary roots of its own.
# Also the recipe catalog's one owner, read as text beside the host's source since no rig can
# run `main()`, and who holds a development runtime (WL-CODE-08). The shipped catalog is read
# beside the host's executable, a definition and not a NEEDS: it is in place before that link,
# which the maker's MinGW toolchain cannot finish (WL-PROJ-15).
zengine_workshop_suite(files SOURCES test_workshop_files.cpp
    NEEDS zengine-launch-fixture-host zengine-launch-fixture-driver
    DEFINITIONS "WORKSHOP_HOST_CPP=\"${CMAKE_SOURCE_DIR}/workshop/workshop.cpp\""
                "WORKSHOP_RECIPES_HPP=\"${CMAKE_SOURCE_DIR}/workshop/recipes.hpp\""
                "WORKSHOP_HOST_DIR=\"$<TARGET_FILE_DIR:zengine-workshop>\""
                # THE DEVELOPMENT CATALOG THIS TREE GENERATED, and the two trees it must name, so
                # a case reads the build's own answer about its panes (WL-CODE-06).
                "WORKSHOP_DEVELOPMENT_RECIPES=\"$<TARGET_FILE_DIR:zengine-workshop>/development-build-recipes.json\""
                "ZENGINE_SOURCE_DIR=\"${CMAKE_SOURCE_DIR}\""
                "ZENGINE_BINARY_DIR=\"${CMAKE_BINARY_DIR}\""
                # ...and the runtime script beside it, run by this tree's own CMake (WL-CODE-07),
                # with the rules it includes, which a case runs over copies of its own.
                "WORKSHOP_DEVELOPMENT_RUNTIME_SCRIPT=\"$<TARGET_FILE_DIR:zengine-workshop>/development-runtime.cmake\""
                "WORKSHOP_DEVELOPMENT_RUNTIME_RULES=\"${CMAKE_SOURCE_DIR}/workshop/prepare-runtime.cmake\""
                "WORKSHOP_CMAKE_PROGRAM=\"${CMAKE_COMMAND}\""
                "WORKSHOP_LAUNCH_FIXTURE_HOST=\"$<TARGET_FILE:zengine-launch-fixture-host>\""
                "WORKSHOP_LAUNCH_FIXTURE_DRIVER=\"$<TARGET_FILE:zengine-launch-fixture-driver>\"")

# This suite links `loom::history` and Workshop's logic does not: that the condition lifecycle
# never reaches the Recorder or the Logger is measured only with both attached while a condition
# is established, read, hidden and resolved. The Pane Creator is a second source (VM-POP-12);
# its cases read two sources as text, since no running case can show that presentation data
# reaches no door through which it could act (VM-WALL-11).
zengine_workshop_suite(panels
    SOURCES test_workshop_panels.cpp
            test_workshop_panels_creator.cpp
    LINKS loom::history
    DEFINITIONS "WORKSHOP_ATTENTION_HPP=\"${CMAKE_SOURCE_DIR}/workshop/attention.hpp\""
                "WORKSHOP_PANE_DEFINITION_HPP=\"${CMAKE_SOURCE_DIR}/workshop/pane_definition.hpp\""
                "WORKSHOP_PANE_DEFINITION_PERSIST_HPP=\"${CMAKE_SOURCE_DIR}/workshop/pane_definition_persist.hpp\"")

# ---- the panes suite's source tripwires -------------------------------------------------
# The presentation's sources and the host say "this host was taught no pane", and the two
# projections "no view branches on an identity": no rig proves either, since every rig would stay
# green beside a special case that agreed with the generic path. Tripwires, not proofs
# (VM-WALL-11): Workshop's `main()` claims a terminal, and a source read keeps a claim every
# case rests on from quietly going false.
zengine_workshop_suite(panes
    LINKS loom::history
    SOURCES test_workshop_demo.cpp
            test_workshop_panes_seam.cpp
            test_workshop_panes_window.cpp
            test_workshop_panes_button.cpp
            test_workshop_panes_canvas.cpp
            test_workshop_panes_desktop.cpp
            test_workshop_panes_input.cpp
            test_workshop_panes_introspection.cpp
            test_workshop_panes_sampling.cpp
            test_workshop_panes_actions.cpp
            test_workshop_panes_files.cpp
            test_workshop_panes_builder.cpp
            test_workshop_panes_attention.cpp
            test_workshop_panes_info.cpp
            test_workshop_inventory_info.cpp
            test_workshop_inventory_folders.cpp
            test_workshop_info_views.cpp
            test_workshop_panes_terminal.cpp
            test_workshop_panes_editor.cpp
            test_workshop_editor_transfers.cpp
            test_workshop_panes_opening.cpp
            test_workshop_panes_code.cpp
            test_workshop_panes_output.cpp
    NEEDS zengine-composer zengine-inventory zengine-inventory-pane
    DEFINITIONS "WORKSHOP_SO_INVENTORY_PANE=\"$<TARGET_FILE:zengine-inventory-pane>\""
                "WORKSHOP_SO_INVENTORY=\"$<TARGET_FILE:zengine-inventory>\"" "WORKSHOP_SO_COMPOSER=\"$<TARGET_FILE:zengine-composer>\""
                "WORKSHOP_HOST_CPP=\"${CMAKE_SOURCE_DIR}/workshop/workshop.cpp\""
                "WORKSHOP_ARRANGEMENT_HPP=\"${CMAKE_SOURCE_DIR}/workshop/arrangement.hpp\""
                "INTROSPECTION_RESOLVED_HPP=\"${CMAKE_SOURCE_DIR}/introspection/resolved.hpp\""
                # The Introspection pane's two sources, read as text so "this image cannot
                # reach an evaluator" is checked, not trusted, and the two host files the
                # authority argument rests on.
                "INTROSPECTION_CPP=\"${CMAKE_SOURCE_DIR}/introspection/introspection.cpp\""
                "INTROSPECTION_POWERS_HPP=\"${CMAKE_SOURCE_DIR}/introspection/powers.hpp\""
                "WORKSHOP_SAMPLE_DOOR_HPP=\"${CMAKE_SOURCE_DIR}/workshop/sample_door.hpp\""
                "INTROSPECTION_CMAKE=\"${CMAKE_SOURCE_DIR}/introspection/CMakeLists.txt\""
                # The Builder pane's own two files, read as text for the same reason: what
                # the build graph says about a loaded image, and the five names a
                # presentation may not spell, are claims a source read can keep and a
                # runtime case cannot.
                "BUILDER_PANE_CMAKE=\"${CMAKE_SOURCE_DIR}/builder-pane/CMakeLists.txt\""
                "BUILDER_PANE_SOURCE=\"${CMAKE_SOURCE_DIR}/builder-pane/pane.cpp\""
                # ...and the Info pane's own source, for the claim only a source read can
                # keep: that the image which shows a maker's document holds no document.
                "INFO_PANE_SOURCE=\"${CMAKE_SOURCE_DIR}/info-pane/pane.cpp\""
                # ...and the Terminal pane's source and build file, for the claim only a
                # source read can keep: that the image which presents a Loom participant
                # cannot construct, own, reach or author through one.
                "TERMINAL_PANE_SOURCE=\"${CMAKE_SOURCE_DIR}/terminal-pane/pane.cpp\""
                "TERMINAL_PANE_CMAKE=\"${CMAKE_SOURCE_DIR}/terminal-pane/CMakeLists.txt\""
                # ...and the Editor pane's source and build file, for the claim only a source
                # read can keep: that the image which holds a maker's document cannot reach
                # the host's session, screen or weave.
                "EDITOR_PANE_SOURCE=\"${CMAKE_SOURCE_DIR}/editor-pane/pane.cpp\""
                "EDITOR_PANE_CMAKE=\"${CMAKE_SOURCE_DIR}/editor-pane/CMakeLists.txt\""
                # ...and the golden the C++ generator must write for a saved EnsureTimer, which a
                # drop on a C++ document inserts (the `source_transfer` suite pins the same bytes).
                "SOURCE_TRANSFER_GOLDEN_FOR_PANES=\"${CMAKE_CURRENT_SOURCE_DIR}/source_transfer_ensure_timer.generated.hpp\"")

zengine_workshop_suite(persistence SOURCES test_workshop_persistence.cpp
    DEFINITIONS "WORKSHOP_HOST_CPP=\"${CMAKE_SOURCE_DIR}/workshop/workshop.cpp\""
        # The shipped session history artifact, not a fixture: a maker's Workshop mounts this
        # file and these cases open the same one, since a stand-in would prove the seam only.
        "SESSION_HISTORY_SO=\"$<TARGET_FILE:zengine-workshop-session-history>\""
        # ...and the session reader's own SOURCE, for the ownership tripwire. That the
        # current owner compiles in no historical shape is a fact about what a translation
        # unit NAMES, and only reading the file can carry it -- the same defence in depth as
        # the operator suite's stranger and Source fences.
        "WORKSHOP_SESSION_PERSIST_HPP=\"${CMAKE_SOURCE_DIR}/workshop/session_persist.hpp\""
    NEEDS zengine-workshop-session-history)

# The load suite asks which artifacts are in the room at all. It stages them into a real
# directory of its own (so the suites can run at once), since "filesystem presence is not load
# authority" cannot be asked of a lookup table, and resolves stems there by the host's own rule.
# `zengine-operators-basic` and `zengine-timer` are what the shipped plan names, so its cases
# drive the files a maker runs; the fixtures stand in for what a plan can get wrong.
zengine_workshop_suite(load SOURCES test_workshop_load.cpp
    NEEDS zengine-timer zengine-provider-abi zengine-operator-consumer-weave-abi
          zengine-operator-consumer-weave zengine-introspection zengine-legacy-pane
    DEFINITIONS "WORKSHOP_LOAD_STAGE_DIR=\"${CMAKE_CURRENT_BINARY_DIR}/load-plan-stage\""
                "TIMER_SO=\"$<TARGET_FILE:zengine-timer>\""
                # Two more real images: a good consumer weave, whose reload must say `Offered`
                # again, and one with another state shape, for the changed-shape refusal.
                "CONSUMER_SO=\"$<TARGET_FILE:zengine-operator-consumer-weave>\""
                "INTROSPECTION_SO=\"$<TARGET_FILE:zengine-introspection>\""
                # An ordinary weave, neither a provider nor an operator consumer.
                "PLAIN_WEAVE_SO=\"$<TARGET_FILE:zengine-workshop-hello>\""
                # The custody witness: the real Editor image, reloaded in place with a maker's
                # document aboard.
                "EDITOR_PANE_SO=\"$<TARGET_FILE:zengine-editor-pane>\""
                # The provider built against the published protocol alone, reloaded in place
                # through the same path with the state it had.
                "LEGACY_PANE_SO=\"$<TARGET_FILE:zengine-legacy-pane>\""
                # A provider surface at a version this host does not speak, and a CONSUMER
                # surface at a version this host does not speak. Two different refusals;
                # both must remain distinguishable from "an ordinary artifact".
                "PROVIDER_ABI_SO=\"$<TARGET_FILE:zengine-provider-abi>\""
                "BROKEN_CONSUMER_SO=\"$<TARGET_FILE:zengine-operator-consumer-weave-abi>\""
                # The two plan files this repository SHIPS, read as files rather than as a
                # description of files.
                "WORKSHOP_DEFAULT_PLAN=\"${CMAKE_SOURCE_DIR}/workshop/default-load-plan.json\""
                "WORKSHOP_GRAPHICAL_PLAN=\"${CMAKE_SOURCE_DIR}/workshop/graphical-load-plan.json\"")

# The Builder suite starts real child processes (success, failure, a missing program, and one
# still running while the bus works), since the package exists to bound that effect. Every recipe
# is `cmake -E ...` or `cmake -P slow_build.cmake` through the CMake that configured this tree,
# the slow script by absolute path: no shell, and no test of this machine's PATH or of where CTest
# started. The runner is a `TimedWeave`, driven through a real `TimerFired`; both Builder weaves
# are mounted in-process, so it needs no kernel of its own.
add_executable(zengine-builder-tests test_builder.cpp doctest_main.cpp)
target_link_libraries(zengine-builder-tests
    PRIVATE loom::switchboard loom::core loom::history zengine-builder-vocabulary
            zengine-timer-vocabulary
            zengine-warnings zengine-sanitize)
target_include_directories(zengine-builder-tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party)

# ---- the Builder's fixture build tree ----------------------------------------------------
# A `CMakeTargetRecipe` names a configured tree and a target, so tests/buildfixture/ is
# configured once, when this tree builds, and built at test time: a nested configure inside a
# CTest entry is what tests/package/run.cmake refuses to be. The generator is passed through,
# not defaulted. Two trees, because a tree has one owner while a build is in it and one case
# runs two builds at once (VM-LANE-21).
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
                "-DZENGINE_FIXTURE_DIAGNOSTIC=${CMAKE_CURRENT_SOURCE_DIR}/diagnostic_build.cmake"
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
    # ...and the bytes `fixture-diagnostic` writes before it prints them, so a case compares what
    # the output reader kept with what the build said, byte for byte.
    "ZENGINE_TEST_DIAGNOSTIC_OUT=\"${zengine_fixture_tree}/diagnostic.txt\""
    "ZENGINE_TEST_FIXTURE_ARTIFACTS=\"${zengine_fixture_artifacts}\""
    "ZENGINE_TEST_ARTIFACT_SUFFIX=\"${CMAKE_SHARED_LIBRARY_SUFFIX}\""
    # The package's two halves, read as source: a property true because of what code does not
    # contain cannot be proved by running it (VM-WALL-11), asked of the two participants a
    # build conversation passes through.
    "ZENGINE_TEST_TOOL_SOURCE=\"${CMAKE_SOURCE_DIR}/builder/weave.hpp\""
    "ZENGINE_TEST_RUNNER_SOURCE=\"${CMAKE_SOURCE_DIR}/builder/runner.hpp\""
    # ...and the production host, read the same way for the durable-record case: its journal
    # selection sits inside `main()`, which claims a terminal and no case can run, and a copy
    # of the selection would prove what a Logger does, not what Workshop chose.
    "ZENGINE_TEST_HOST_SOURCE=\"${CMAKE_SOURCE_DIR}/workshop/workshop.cpp\"")

zengine_doctest_test(builder zengine-builder-tests)

# ---- the guests suite: who may connect to this Workshop, and what each may then say --------
# The guest door and its policy file over a real loopback socket, with the real Input weave (over
# a fake reader) on the bus and Loom's client on the far side, in this process. It links the
# guests library the host links and nothing of Workshop's screen, standalone as the Builder suite
# is: no rig, window or plan, only a bus, a socket and the door.
add_executable(zengine-workshop-guests-tests test_workshop_guests.cpp doctest_main.cpp)
target_link_libraries(zengine-workshop-guests-tests
    PRIVATE loom::bridge loom::switchboard loom::core zengine-workshop-guests
            zengine-input-vocabulary zengine-surface-vocabulary zengine-timer-vocabulary
            zengine::inventory zengine-warnings zengine-sanitize)
target_include_directories(zengine-workshop-guests-tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
zengine_doctest_test(workshop_guests zengine-workshop-guests-tests)

# ---- THE GUEST VOCABULARY SUITE: what an external host boots to speak Workshop's guest shapes --
#
# external-host/vocabulary_weave.hpp, registered natively on a bus: every guest-facing shape
# resolves by identity once it is registered, none before; a divergent definition is refused at
# the wall; unregistered, it takes the shapes with it. Header-only, so it needs no kernel.
add_executable(zengine-guest-vocabulary-tests test_guest_vocabulary.cpp doctest_main.cpp)
target_link_libraries(zengine-guest-vocabulary-tests
    PRIVATE loom::switchboard loom::core zengine-input-vocabulary zengine-surface-vocabulary
            zengine-warnings zengine-sanitize)
target_include_directories(zengine-guest-vocabulary-tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party ${CMAKE_SOURCE_DIR})
zengine_doctest_test(guest_vocabulary zengine-guest-vocabulary-tests)

# ---- the probe suite: the reusable journey's orchestration and assertions ------------------
# The real probe weave (examples/workshop-probe) on one bus, with the real Input weave behind a
# scripted link that keeps the link's contract: when the probe may say PASS, which answers may
# move it, what it closes before it stops, and which inventory row is its session. The crossing
# is Loom's `bridge` suite's; the door, the Skin and the socket are `workshop_guests`' and the
# journey's.
add_executable(zengine-workshop-probe-tests test_workshop_probe.cpp doctest_main.cpp)
target_link_libraries(zengine-workshop-probe-tests
    PRIVATE loom::kernel loom::bridge loom::switchboard loom::core zengine-workshop-guests
            zengine-input-vocabulary zengine-surface-vocabulary zengine-timer-vocabulary
            zengine-warnings zengine-sanitize)
target_include_directories(zengine-workshop-probe-tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
zengine_doctest_test(workshop_probe zengine-workshop-probe-tests)

# ---- the guest journey, across two real processes --------------------------------------
# A real `zengine-workshop` launched with a guests file and the shipped terminal plan beside it,
# and this program as the other host: admitted by credential, refused by a wrong one, an input
# session, the desktop's chord injected, the desktop asked what it accepts, and a picture of the
# real terminal Skin read for the Pane Manager the chord opened. A `program` entry.
add_executable(zengine-guest-journey guest_journey.cpp)
target_link_libraries(zengine-guest-journey
    PRIVATE loom::bridge loom::switchboard loom::core
            zengine-input-vocabulary zengine-surface-vocabulary
            zengine-warnings zengine-sanitize)
add_dependencies(zengine-guest-journey zengine-workshop)
zengine_program_test(guest_journey zengine-guest-journey
    $<TARGET_FILE:zengine-workshop>
    $<TARGET_FILE_DIR:zengine-workshop>/default-load-plan.json
    ${CMAKE_BINARY_DIR}/guest-journey-work)
set_tests_properties(guest_journey PROPERTIES TIMEOUT 120)

# ---- the Workshop journey from a Loom session: the `session` gate --------------------------
# tests/session/workshop_journey.py drives this tree's Workshop, a session host from the installed
# Loom (its run manager and external-host/vocabulary_weave.hpp) linked to it twice, and the
# Workshop tool package as named runs; every claim is read from its owner. The gate opens on the
# session tooling the Loom package says it carries (LOOM_SESSION_TOOLS) and a Python 3.8 or
# newer, found or named by ZENGINE_SESSION_PYTHON; it stays open under ZENGINE_SANITIZE.
set(ZENGINE_SESSION_PYTHON "" CACHE FILEPATH
    "A Python (3.8 or newer) for the session-gated journey; empty finds one")
if(LOOM_SESSION_TOOLS)
    if(ZENGINE_SESSION_PYTHON)
        if(NOT EXISTS "${ZENGINE_SESSION_PYTHON}")
            message(FATAL_ERROR
                "zengine: ZENGINE_SESSION_PYTHON names ${ZENGINE_SESSION_PYTHON}, which does not "
                "exist. The `session` gate opens for a Python that is there; clear the variable "
                "to find one instead.")
        endif()
        set(Python3_EXECUTABLE "${ZENGINE_SESSION_PYTHON}")
    endif()
    find_package(Python3 3.8 COMPONENTS Interpreter)
    if(Python3_Interpreter_FOUND)
        # The population record names the program from the top-level directory, where an
        # imported target found here is otherwise invisible.
        set_target_properties(Python3::Interpreter PROPERTIES IMPORTED_GLOBAL TRUE)
        zengine_program_test(demo_recipes Python3::Interpreter
            ${CMAKE_CURRENT_SOURCE_DIR}/session/demo_recipes.py)
        zengine_program_test(workshop_journey Python3::Interpreter
            ${CMAKE_CURRENT_SOURCE_DIR}/session/workshop_journey.py
            --workshop $<TARGET_FILE:zengine-workshop>
            --plan $<TARGET_FILE_DIR:zengine-workshop>/default-load-plan.json
            --loom-host ${LOOM_HOST_PROGRAM}
            --loom-runs ${LOOM_RUNS_ARTIFACT}
            --loom-runtime ${LOOM_SESSION_RUNTIME}
            --vocabulary $<TARGET_FILE:zengine-guest-vocabulary>
            --tools ${CMAKE_SOURCE_DIR}/external-host/tools/workshop
            --inventory-reader $<TARGET_FILE:zengine-inventory-read>
            --work ${CMAKE_BINARY_DIR}/workshop-journey-work
            --evidence ${CMAKE_BINARY_DIR}/workshop-journey.json)
        set_tests_properties(workshop_journey PROPERTIES TIMEOUT 600)
        # THE TOWER-DEFENSE STORY'S OWN CUSTODY (examples/tower-defense/story.py), on the same real
        # processes: roots launched by the story's `launch` from this tree, its commands run as a
        # maker runs them. A run whose wait ran out keeps its handle until it is seen to settle;
        # `stop` believes a process gone only when it is seen to end, and forces one only once its
        # identity is confirmed. The driver, their parent, asks the operating system itself.
        zengine_program_test(story_journey Python3::Interpreter
            ${CMAKE_CURRENT_SOURCE_DIR}/session/story_journey.py
            --story ${CMAKE_SOURCE_DIR}/examples/tower-defense/story.py
            --workshop $<TARGET_FILE:zengine-workshop>
            --loom-host ${LOOM_HOST_PROGRAM}
            --loom-runs ${LOOM_RUNS_ARTIFACT}
            --loom-runtime ${LOOM_SESSION_RUNTIME}
            --vocabulary $<TARGET_FILE:zengine-guest-vocabulary>
            --work ${CMAKE_BINARY_DIR}/story-journey-work
            --evidence ${CMAKE_BINARY_DIR}/story-journey.json)
        set_tests_properties(story_journey PROPERTIES TIMEOUT 600)
        set_property(GLOBAL APPEND PROPERTY ZENGINE_ACTIVE_GATES "session")
        # `workshop/nvim-edit` AGAINST A REAL NEOVIM needs both halves: this session tooling and
        # the Neovim the `neovim` gate names. The two together are one gate of their own,
        # `session-neovim`, so a lane with only one of them declares this entry absent rather
        # than running half of it.
        if(ZENGINE_NEOVIM_PROGRAM)
            zengine_program_test(neovim_journey Python3::Interpreter
                ${CMAKE_CURRENT_SOURCE_DIR}/session/neovim_journey.py
                --neovim ${ZENGINE_NEOVIM_PROGRAM}
                --story ${CMAKE_SOURCE_DIR}/examples/tower-defense/story.py
                --workshop $<TARGET_FILE:zengine-workshop>
                --loom-host ${LOOM_HOST_PROGRAM}
                --loom-runs ${LOOM_RUNS_ARTIFACT}
                --loom-runtime ${LOOM_SESSION_RUNTIME}
                --vocabulary $<TARGET_FILE:zengine-guest-vocabulary>
                --work ${CMAKE_BINARY_DIR}/neovim-journey-work
                --evidence ${CMAKE_BINARY_DIR}/neovim-journey.json)
            set_tests_properties(neovim_journey PROPERTIES TIMEOUT 900)
            set_property(GLOBAL APPEND PROPERTY ZENGINE_ACTIVE_GATES "session-neovim")
        endif()
    else()
        message(STATUS "zengine: the `session` gate is shut: no Python 3.8 or newer was found "
                       "(ZENGINE_SESSION_PYTHON names one)")
    endif()
else()
    message(STATUS "zengine: the `session` gate is shut: the Loom this tree consumes names no "
                   "session tooling (LOOM_SESSION_TOOLS)")
endif()

# ---- the build witness: a host, and deliberately not a CTest entry ------------------------
# tests/build/witness.cpp is Workshop's Builder wiring without the picture, driven by arguments
# so tests/build/run.cmake can install Zengine into a scratch prefix and build a one-file weave
# outside this repository, canaries included. Not an entry: it configures nested projects, as
# tests/package/run.cmake's header explains. The ordinary build still compiles it, so it cannot
# rot unseen.
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

# The liveness probes: what the substrate does when the Timer service itself is swapped,
# reloaded, double-wound or joined late. The suite's header says why a file that pins unwanted
# behaviour lives here.
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

# ---- the source-tree checks ------------------------------------------------------------
# Four entries read the source tree rather than a build -- doc_links, package_vocabulary,
# law_register and source_comments -- each stating its contract in its own header. They are CTest
# entries so a bare `ctest` carries them, and rows in tests/test_population.txt, so deleting one
# is a red rather than a smaller lane.
zengine_script_test(doc_links ${CMAKE_CURRENT_SOURCE_DIR}/check_doc_links.cmake)

zengine_script_test(package_vocabulary
                    ${CMAKE_CURRENT_SOURCE_DIR}/check_package_vocabulary.cmake)

zengine_script_test(law_register ${CMAKE_CURRENT_SOURCE_DIR}/check_law_register.cmake)

zengine_script_test(source_comments ${CMAKE_CURRENT_SOURCE_DIR}/check_source_comments.cmake)
