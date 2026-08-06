cmake_minimum_required(VERSION 3.16)

project(zengine
    VERSION 0.1.0
    DESCRIPTION "Zen's default set of weaves — the Loom's first external consumer"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---- How this repo gets the Loom ------------------------------------------------------
#
# Two paths, same target names either way (the Loom's export sets EXPORT_NAME to match its
# in-tree ALIASes — loom::core, loom::switchboard, loom::kernel — so the override is a
# genuine drop-in and the two paths cannot silently come to mean different things):
#
#   ZEN_LOOM_DEV=OFF (the default) — the STRANGER'S path: find_package() against an
#       installed, exported Loom, consuming it exactly as a third party would.
#   ZEN_LOOM_DEV=ON  — the SIBLING path: add_subdirectory(../Loom). For editing the two
#       trees together, and the turnkey Windows path: one flag, configure, press play.
#
# WHY THE STRANGER'S PATH IS THE DEFAULT (re-affirmed deliberately, 2026-07-27). Because
# it is what makes an unexported-surface mistake fail on EVERY developer's machine instead
# of only in the verification lanes. Dev mode reaches the whole Loom build tree, so a
# target that was never exported still links — the build stays green here and breaks for
# the first guest who tries it. That is precisely the class of error this repo exists to
# catch first, and a default that hides it moves the discipline from "always on" to
# "on wherever someone remembered". It was ON for one lane-length experiment; the trust
# gate found the choice had drifted in without a decision, so it is now a decision:
# stranger by default, dev one flag away. (Between the two, target names are identical —
# the Loom's export sets EXPORT_NAME to match its in-tree ALIASes — so the override is a
# genuine drop-in and the two paths cannot silently come to mean different things.)

option(ZEN_LOOM_DEV
       "Consume the Loom from the sibling ../Loom source tree instead of an installed package"
       OFF)

if(ZEN_LOOM_DEV)
    message(STATUS "zengine: Loom from sibling source ../Loom  [DEV OVERRIDE]")
    # A dependency's proof rides its version: Zengine's lane does not re-run the Loom's suite,
    # so the sibling build contributes its libraries and nothing else. ZEN_SDL is silenced for
    # the same reason — that option builds the Loom's SDL2 UI-tree renderer (zen-ui-sdl, a
    # port still to come); Zengine's own SDL skin lives in surface/ and fetches its own SDL3.
    set(ZEN_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
    set(ZEN_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(ZEN_SDL            OFF CACHE BOOL "" FORCE)
    if(WIN32)
        # The demos need the kernel, and on Windows the kernel exists only as the Loom's
        # explicit development/demo backend (no isolation — the Loom prints its banner and
        # Kernel::containment_note() says so at every surface). Asking for dev mode on
        # Windows IS asking for the demo workflow, so default the option ON here — and note
        # this now sits downstream of an explicit -DZEN_LOOM_DEV=ON, so no default implies
        # another. A plain CACHE set, so an explicit -DLOOM_ENABLE_WINDOWS_KERNEL=OFF on the
        # command line still wins.
        set(LOOM_ENABLE_WINDOWS_KERNEL ON CACHE BOOL
            "Build the Windows kernel backend (LoadLibrary). Development/demo only — no isolation.")
    endif()
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../Loom ${CMAKE_CURRENT_BINARY_DIR}/_loom)
else()
    find_package(loom 0.1 REQUIRED)
    message(STATUS "zengine: Loom from installed package at ${loom_DIR}")
endif()

# Either path must hand over the weave-authoring surface. Checked here, out loud, rather
# than left to surface as "Unknown CMake command" from inside zengine_weave() a hundred
# lines later: a Loom too old to own the reloadable-weave contract is a Loom against
# which this repo's weaves cannot be built safely, and the honest answer is to say which
# thing is missing and why it matters — not to quietly build them without it.
if(NOT COMMAND loom_weave_build_contract)
    message(FATAL_ERROR
        "zengine: this Loom does not export loom_weave_build_contract(). That function "
        "carries the reloadable-weave build contract (KERN-05) — without it a weave's "
        "statics can outlive the image they belong to, and unload/reload aliases freed "
        "memory. Zengine will not build weaves against a Loom that cannot state it.")
endif()

# ---- House warnings for Zengine's own sources ------------------------------------------
#
# The Loom's exported loom::warnings is deliberately $<LINK_ONLY:> so its -Werror never
# leaks onto a consumer's sources; a consumer that wants the same discipline declares its
# own. Zengine does — the same list, the same -Werror, applied to every target this repo
# authors. (GNU/Clang only: those are the lanes that exist here — WSL/GCC and MinGW.)
add_library(zengine-warnings INTERFACE)
target_compile_options(zengine-warnings INTERFACE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic -Wshadow -Wconversion
    -Wsign-conversion -Werror>)

# ---- The loadable-weave pattern --------------------------------------------------------
#
# A loadable weave: PIC shared library, exact filename (no 'lib' prefix), linked against
# the exported substrate. export.hpp is header-only, so a weave library needs only core +
# switchboard — the kernel target is the HOST'S dependency. Every package's weaves are
# built through this one function so the flag below is a law, not a convention.
#
# THE RELOADABLE LIFETIME IS THE LOOM'S LAW, AND IT IS THE LOOM THAT APPLIES IT.
# loom_weave_build_contract() arrives with the package (KERN-05), and this helper
# delegates to it. Zengine keeps only what is genuinely Zengine's: which target, which
# source, this repo's warnings, and the no-prefix filename its hosts resolve by.
#
# This repo used to carry its own literal -fno-gnu-unique, because this repo is where
# the failure was FOUND. The maker path (ZEN_SHAPE + WeaveBase) instantiates loom's
# schema_of<T> function-local statics with vague linkage; GCC emits those as
# STB_GNU_UNIQUE on ELF; glibc resolves unique symbols through a PROGRAM-WIDE table that
# ignores RTLD_LOCAL and outlives dlclose — so the second .so sharing a vocabulary
# header silently aliased the FIRST one's destroyed statics, and swapping the classic
# drawer for the block drawer crashed in describe() with an ASan-confirmed
# use-after-free into the unloaded library's heap. The Loom's own fixtures never hit it
# because their schemas sit in anonymous namespaces (internal linkage is never unique);
# snake's were the first weave libraries built the maker way.
#
# Finding it here was right. KEEPING it here was not: a private second implementation of
# someone else's law means the guest who has not read this file gets nothing (COLD-1
# F-22), and it drifts — this copy had no platform guard, so it was injecting an
# ELF-only option into MinGW weave DLLs purely because that compiler accepts the
# spelling. The Loom's function asks the semantic question instead.
function(zengine_weave target source)
    add_library(${target} SHARED ${source})
    target_link_libraries(${target} PRIVATE loom::core loom::switchboard zengine-warnings)
    loom_weave_build_contract(${target})
    set_target_properties(${target} PROPERTIES PREFIX "" OUTPUT_NAME ${target})
endfunction()

# ---- Testing is a real switch, and it is a real requirement -----------------------------
#
# BUILD_TESTING is CMake's conventional name, and here it draws a line that used to be
# missing entirely (R2F-D / POP-03, closing COLD-1 F-25).
#
# Zengine's suites need a Loom that can HOST loadable weaves. Before this, tests/ answered a
# kernel-less Loom with `message(STATUS ...)` + `return()`: configure succeeded, the whole
# test tree silently evaporated, and `ctest` then printed "100% tests passed, 0 tests failed
# out of 1" over the one surviving smoke test. That is reachable from the SUPPORTED DEFAULT
# Windows Loom package — not from a typo — so the most-advertised green in this repo could be
# produced by a configuration that had deleted its own evidence. tests/CMakeLists.txt now
# refuses that configuration out loud.
#
# The switch is what keeps the refusal honest and narrow: the requirement belongs to the
# TESTS, not to the package. A consumer that wants only Zengine's kernel-independent surface
# — the activation cursor and the timer/input/surface vocabularies, all header-only — can
# still configure and build against a kernel-less Loom with -DBUILD_TESTING=OFF. R2F-D proves
# both halves: tests-on refuses, tests-off builds.
option(BUILD_TESTING "Build Zengine's test suites (they require a Loom exporting loom::kernel)" ON)

# ---- Every registered test says what kind of evidence it is (C4 / POP-01) ---------------
#
# Zengine's official verification lane is
#
#     cmake -DZEN_BUILD_DIR=<build> -P tests/verify.cmake
#
# and its first question is not "did the tests pass" but "is the population that ran the
# population this repository meant to run". Answering that needs THREE independent sources,
# and these helpers are one of them:
#
#   tests/test_population.txt   what MUST be registered. Source-owned, so deleting a
#                               registration cannot delete the expectation with it.
#   `ctest -N`                  what IS registered. CTest's own answer.
#   the record written below    what each registration DECLARED ITSELF TO BE -- a doctest
#                               binary with a case floor, a compile-negative test judged on
#                               a diagnostic, its positive control, or a plain program.
#
# A CTest entry cannot be a semantic surface here without going through one of these, because
# the verifier refuses an entry it finds registered and unrecorded. That is the point: the
# kind is what decides which population question is even askable (a compile-negative test has
# no test cases to count, and inventing a case floor for it would be a number that means
# nothing). COLD-2 C-4 measured what the absence cost -- deleting a whole TEST_CASE from
# test_input.cpp left `ctest --no-tests=error` reporting 10/10.
#
# The record is a build-tree file, written once after every subdirectory has registered, and
# it is deliberately NOT the expectation. It describes; test_population.txt requires.

# zengine_record_test(<entry> <kind> [<one extra line>]). The optional third argument is
# taken whole rather than as a list: a semicolon inside a recorded value would otherwise be
# eaten by CMake's own list expansion before it reached the file, which is a real and quiet
# failure mode this project has already paid for once next door (C3).
function(zengine_record_test entry kind)
    set(lines "set(ZENGINE_KIND_${entry} \"${kind}\")\n")
    if(ARGC GREATER 2)
        string(APPEND lines "${ARGV2}\n")
    endif()
    set_property(GLOBAL APPEND_STRING PROPERTY ZENGINE_TEST_RECORD "${lines}")
endfunction()

# A doctest-bearing runtime binary: the population question is "how many cases did it
# select", and the answer must clear the floor test_population.txt declares for it.
function(zengine_doctest_test entry target)
    add_test(NAME ${entry} COMMAND ${target})
    zengine_record_test(${entry} doctest "set(ZENGINE_EXE_${entry} \"$<TARGET_FILE:${target}>\")")
endfunction()

# A plain program with no doctest population -- today, the smoke test. The contract is that
# the entry exists and CTest still runs the real command; there are no cases to count and
# none are invented for it.
function(zengine_program_test entry target)
    add_test(NAME ${entry} COMMAND ${target})
    zengine_record_test(${entry} program "set(ZENGINE_EXE_${entry} \"$<TARGET_FILE:${target}>\")")
endfunction()

# A compile-judged test: it builds an EXCLUDE_FROM_ALL fixture and reads the compiler.
#
#   zengine_compile_test(<entry> <target> <required diagnostic>)  the negative case
#   zengine_compile_test(<entry> <target>)                        its positive control
#
# The diagnostic is what makes a compile-negative test evidence rather than an observation
# that something failed to build: CTest's PASS_REGULAR_EXPRESSION judges the OUTPUT, so a
# build that broke for an unrelated reason does not read as a proof, and a fixture that
# started compiling cleanly fails for producing no diagnostic at all. This helper is the only
# way to register one here, so "is the diagnostic still being judged" becomes a fact the
# build records and the verifier can require -- COLD-2 credited these tests for judging the
# diagnostic, and C4's job is to keep them from silently degrading into
# `the compiler returned non-zero, therefore PASS`.
function(zengine_compile_test entry target)
    add_test(NAME ${entry}
             COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target ${target})
    set(diagnostic "")
    if(ARGC GREATER 2)
        set(diagnostic "${ARGV2}")
    endif()
    if(diagnostic STREQUAL "")
        zengine_record_test(${entry} compile-positive)
    else()
        set_tests_properties(${entry} PROPERTIES PASS_REGULAR_EXPRESSION "${diagnostic}")
        # The record is read back as CMake source, so a backslash or a quote inside a
        # diagnostic pattern must survive the round trip intact.
        string(REPLACE "\\" "\\\\" quoted "${diagnostic}")
        string(REPLACE "\"" "\\\"" quoted "${quoted}")
        zengine_record_test(${entry} compile-negative
                            "set(ZENGINE_DIAGNOSTIC_${entry} \"${quoted}\")")
    endif()
endfunction()


# ---- The consumers --------------------------------------------------------------------
#
# smoke: the minimal stranger (links only the exported surface; its refusal test is the
# proof it is real). timer: the Timer package — time, message-shaped (the one weave that
# owns a clock and a nap; everyone else hears beats). input: the Input package — the
# floor games sit on (the sole producer of the locked input shapes). surface: the
# Surface package — visual intent in, output out (the skins own every terminal row and
# window pixel; consumers only publish). snake: the first game panel — the Stage 2
# vertical slice, and the hosting consumer that pulled loom::kernel onto the Loom's
# export surface; consumer of the floors, owner of no platform edge. tests: the Zengine
# suites (the smoke test stays its own tiny proof; the others drive the real .so weaves
# through the real kernel). Order matters only where vocabulary flows: activation's
# interface target must exist before the packages that read an activation, timer's before
# input/surface consume it, and theirs before snake.
#
# smoke and tests sit behind BUILD_TESTING because both are test artifacts; the packages
# themselves do not, and each still makes its own honest kernel decision.

add_subdirectory(activation)
add_subdirectory(timer)
add_subdirectory(input)
add_subdirectory(surface)
add_subdirectory(snake)

if(BUILD_TESTING)
    enable_testing()

    # `always` is the gate every supported test configuration carries; the subdirectories
    # append the conditional ones (today: `sdl`). A gate is how a population that is
    # legitimately absent gets DECLARED absent instead of quietly lowering a floor.
    set_property(GLOBAL PROPERTY ZENGINE_ACTIVE_GATES "always")

    add_subdirectory(smoke)
    add_subdirectory(tests)

    # The build tree's record of what it registered, for tests/verify.cmake. It is written
    # here, once, after every subdirectory has had its say -- and it is a DESCRIPTION. The
    # requirement lives in tests/test_population.txt, in the source tree, where deleting a
    # registration cannot reach it.
    get_property(zengine_active_gates GLOBAL PROPERTY ZENGINE_ACTIVE_GATES)
    get_property(zengine_test_record GLOBAL PROPERTY ZENGINE_TEST_RECORD)
    file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/zengine-test-population.cmake" CONTENT
"# GENERATED AT CONFIGURE TIME by Zengine/CMakeLists.txt. Do not edit; do not commit.
# What this build actually registered, and what each entry declared itself to be.
# Read by tests/verify.cmake, which requires it to agree with tests/test_population.txt.
set(ZENGINE_ACTIVE_GATES \"${zengine_active_gates}\")
${zengine_test_record}")
else()
    message(STATUS "zengine: BUILD_TESTING=OFF — library targets only, no suites registered")
endif()
