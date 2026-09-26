cmake_minimum_required(VERSION 3.16)

project(zengine
    VERSION 0.1.0
    DESCRIPTION "Zen's default set of weaves — the Loom's first external consumer"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# The platform's own answer to "where does a package put its headers, libraries and
# programs" (CMAKE_INSTALL_INCLUDEDIR / LIBDIR / BINDIR). Included here rather than beside
# the install rules because every exported target's INSTALL_INTERFACE include path is
# written in those terms, and those are declared by the packages themselves.
include(GNUInstallDirs)

# ---- how this repository gets the Loom ------------------------------------------------
# Two routes, the same target names either way (the Loom's export matches its in-tree ALIASes):
# find_package() against an installed Loom, the stranger's route and the default, so an
# unexported-surface mistake fails on every machine (VM-LANE-02); or ZEN_LOOM_DEV=ON,
# add_subdirectory(../Loom), for editing the two trees together.

option(ZEN_LOOM_DEV
       "Consume the Loom from the sibling ../Loom source tree instead of an installed package"
       OFF)

if(ZEN_LOOM_DEV)
    message(STATUS "zengine: Loom from sibling source ../Loom  [DEV OVERRIDE]")
    # A dependency's proof rides its version: Zengine's lane does not re-run the Loom's suite,
    # so the sibling build contributes its libraries and nothing else. Zengine's own SDL skin
    # lives in surface/ and fetches its own SDL3; the Loom declares no SDL option of its own.
    set(ZEN_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
    set(ZEN_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    if(WIN32)
        # The demos need the kernel, which on Windows is the Loom's development backend (no
        # isolation, and the Loom says so): dev mode on Windows is the demo workflow, so the
        # option defaults ON here, and an explicit -DLOOM_ENABLE_WINDOWS_KERNEL=OFF still wins.
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

# ---- big objects on COFF --------------------------------------------------------------
# A COFF object holds a limited number of sections, and a heavily templated unit built with `-g`
# nears it; MinGW's `-mbig-obj` and MSVC's `/bigobj` lift it, for every source, because the
# ceiling is the toolchain's and the next header to grow meets it elsewhere (VM-PLAT-01). A second
# MinGW ceiling, the section-name string table, no flag lifts: another source under the same
# suite does (VM-PLAT-02, VM-PLAT-03). ELF has neither, so the Linux lanes ask for nothing.
if(MINGW)
    target_compile_options(zengine-warnings INTERFACE -Wa,-mbig-obj)
endif()
if(MSVC)
    target_compile_options(zengine-warnings INTERFACE /bigobj)
endif()

# ---- the standing sanitizer witness ---------------------------------------------------
# A second kind of evidence, the same population under ASan and UBSan (VM-LANE-07); every target
# lists zengine-sanitize beside zengine-warnings (VM-LANE-08). -fno-sanitize-recover=all makes a
# diagnostic a red rather than a line that scrolls past. It instruments this repository's
# targets, not the Loom, which runs the same lane over itself; the flags are unconditional, so
# an unsupported compiler fails loudly instead of passing having watched nothing.
option(ZENGINE_SANITIZE
       "Build Zengine's own targets with AddressSanitizer + UndefinedBehaviorSanitizer"
       OFF)

add_library(zengine-sanitize INTERFACE)
if(ZENGINE_SANITIZE)
    target_compile_options(zengine-sanitize INTERFACE
        -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all)
    target_link_options(zengine-sanitize INTERFACE
        -fsanitize=address,undefined)
endif()

# ---- the loadable-weave pattern -------------------------------------------------------
# A loadable weave is a PIC shared library with an exact filename (no `lib` prefix), linked
# against the exported substrate; the kernel is the host's dependency. The reloadable lifetime is
# the Loom's law (KERN-05), and loom_weave_build_contract() applies it: every package's weaves
# go through this function, so the contract is a law here, not a convention.
function(zengine_weave target source)
    # ARGN, so a weave may carry more than its one .cpp (a generated source, an embedded
    # typeface); the first source is the one file that is the weave.
    add_library(${target} SHARED ${source} ${ARGN})
    target_link_libraries(${target} PRIVATE loom::core loom::switchboard zengine-warnings
                                            zengine-sanitize)
    loom_weave_build_contract(${target})
    set_target_properties(${target} PROPERTIES PREFIX "" OUTPUT_NAME ${target})
    # ...AND THAT FILE IS WRITTEN DOWN, as the file this weave's code is read from. It is the
    # editing entry the development recipe catalog names for the weave (workshop/CMakeLists.txt),
    # so the entry is the build's own declaration and never a file guessed from a target's name.
    get_filename_component(zengine_weave_entry "${source}" ABSOLUTE
                           BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    set_property(TARGET ${target} PROPERTY ZENGINE_WEAVE_SOURCE "${zengine_weave_entry}")
endfunction()

# ---- ...and an artifact that is a provider, not a weave --------------------------------
# One line differs from zengine_weave(), and it is the claim: no loom::switchboard. A provider
# supplies operator definitions to a host that opened it; the Kernel never loads it and it has
# no participant. It still takes loom_weave_build_contract(): it is opened, held while its
# contributions are installed and closed, and an escaped static would survive an unmount.
function(zengine_provider target source)
    add_library(${target} SHARED ${source} ${ARGN})
    target_link_libraries(${target} PRIVATE loom::core zengine-warnings zengine-sanitize)
    loom_weave_build_contract(${target})
    set_target_properties(${target} PROPERTIES PREFIX "" OUTPUT_NAME ${target})
endfunction()

# ---- the SDL dependency, acquired once for every package that needs it -----------------
# The window Skin and the SDL Input reader must share one SDL, or the reader polls a queue the
# window never posts to: cmake/ZengineSdl.cmake owns that and refuses a static SDL out loud.
# Included before any package, since both ask it for the same target.
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/ZengineSdl.cmake)

# ---- testing is a real switch and a real requirement ----------------------------------
# The suites need a Loom that can host weaves, and tests/ refuses one that cannot rather than
# vanishing (VM-POP-10). The requirement is the tests', not the package's: -DBUILD_TESTING=OFF
# builds the kernel-independent surface against a kernel-less Loom.
option(BUILD_TESTING "Build Zengine's test suites (they require a Loom exporting loom::kernel)" ON)

# ---- every registered test says what kind of evidence it is ---------------------------
# tests/verify.cmake checks the population against three sources (VM-POP-01): the manifest, what
# must be registered; `ctest -N`, what is; and the record written below, what each registration
# declared itself to be. An entry registered and unrecorded is refused (VM-POP-03).

# zengine_record_test(<entry> <kind> [<one extra line>]). The third argument is taken whole: a
# semicolon inside a recorded value would otherwise be eaten by CMake's list expansion.
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

# A plain program with no doctest population: the contract is that the entry exists and CTest
# still runs the real command; no cases are invented for it. Arguments after the target are the
# program's own (the guest journey names the host it launches); the record keeps the executable.
function(zengine_program_test entry target)
    add_test(NAME ${entry} COMMAND ${target} ${ARGN})
    zengine_record_test(${entry} program "set(ZENGINE_EXE_${entry} \"$<TARGET_FILE:${target}>\")")
endfunction()

# A repository-owned CMake script run over the source tree, its own kind because none of the
# other questions is askable of it: no binary to exist, no cases to count, no diagnostic to
# judge. It reads the source tree, not the build, so it is registered in any configuration.
function(zengine_script_test entry script)
    add_test(NAME ${entry} COMMAND ${CMAKE_COMMAND}
             -DZEN_REPO=${CMAKE_SOURCE_DIR} -P ${script})
    zengine_record_test(${entry} script)
endfunction()

# A compile-judged test builds an EXCLUDE_FROM_ALL fixture and reads the compiler:
#   zengine_compile_test(<entry> <target> <required diagnostic>)  the negative case
#   zengine_compile_test(<entry> <target>)                        its positive control
# PASS_REGULAR_EXPRESSION judges the output, so a build broken for another reason proves nothing
# (VM-WALL-04). The fixtures build in a tree of their own, one at a time (VM-LANE-17,
# VM-LANE-19), and this function registers no entry without that tree.
function(zengine_compile_test entry target)
    if(NOT ZENGINE_COMPILE_FIXTURE_TREE)
        message(FATAL_ERROR
            "zengine_compile_test(${entry}): no ZENGINE_COMPILE_FIXTURE_TREE. A compile test "
            "builds a target to read the compiler, which makes it a writer of whatever build "
            "tree it names -- and the one tree it must never name is this one.")
    endif()
    # Refused here, because the defect is silent: entries pointed at ${CMAKE_BINARY_DIR} pass
    # serially and fail in parallel only when a regeneration is owed, and `ctest -N` prints
    # names, not commands, so nothing downstream would see it.
    if(ZENGINE_COMPILE_FIXTURE_TREE STREQUAL CMAKE_BINARY_DIR)
        message(FATAL_ERROR
            "zengine_compile_test(${entry}): the fixture tree is this build tree.\n"
            "A compile test proves its point by BUILDING something, so the entry is a writer "
            "of the tree it names. Eight of them naming this one makes eight independent "
            "proofs into eight competing owners of the tree the whole repository shares -- and "
            "under `ctest -j`, with a CMake regeneration owed, they race it: 5-7 of 22 entries "
            "failing on configure_file errors and a half-written build.ninja. The fixtures have "
            "a tree of their own; see tests/CMakeLists.txt.")
    endif()
    add_test(NAME ${entry}
             COMMAND ${CMAKE_COMMAND} --build ${ZENGINE_COMPILE_FIXTURE_TREE} --target ${target})
    set_tests_properties(${entry} PROPERTIES RESOURCE_LOCK zengine_compile_fixture_tree)
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


# ---- the packages ----------------------------------------------------------------------
# Order matters only where a package's file reads another's target as it is read: activation
# first, timer before its consumers, surface before input (the SDL reader carries the Surface
# vocabulary's window-lifecycle fact), builder before workshop, and each package workshop/ stages
# or a boot plan names before workshop/. A link to a later target resolves at generate time and
# does not constrain the order; `if(TARGET ...)` and `$<TARGET_FILE:...>` are answered when read.

add_subdirectory(activation)
add_subdirectory(operator)
add_subdirectory(maker)
add_subdirectory(message-draft)
add_subdirectory(inventory)
add_subdirectory(source-transfer)
add_subdirectory(flow)
add_subdirectory(timer)
add_subdirectory(surface)
add_subdirectory(input)
# The Neovim hosting library: header-only, after the input vocabulary its key notation names,
# and before every weave -- the Neovim editor weave includes it, and nothing it includes is a weave.
add_subdirectory(neovim)
add_subdirectory(ui)
add_subdirectory(component)
add_subdirectory(builder)
add_subdirectory(introspection)
add_subdirectory(composer)
add_subdirectory(files)
add_subdirectory(builder-pane)
add_subdirectory(attention-pane)
add_subdirectory(connections-pane)
add_subdirectory(external-host)
add_subdirectory(demo-control)
add_subdirectory(desktop-pane)
add_subdirectory(menu-presenter)
add_subdirectory(flow-pane)
add_subdirectory(info-pane)
add_subdirectory(inventory-pane)
add_subdirectory(terminal-pane)
add_subdirectory(editor-pane)
add_subdirectory(neovim-editor)
add_subdirectory(snake)
add_subdirectory(workshop)

# ---- the compile-judged lane's fixtures -------------------------------------------------
# OFF here and ON only in the small tree tests/CMakeLists.txt configures for the compile tests,
# which reads the definitions above through this same project (VM-LANE-18); a compile test
# writes the tree it builds, so its fixtures never build in this one (VM-LANE-17).
option(ZENGINE_COMPILE_FIXTURES
       "Add the compile-judged lane's fixture targets (the compile tests' own build tree sets this)"
       OFF)
if(ZENGINE_COMPILE_FIXTURES)
    add_subdirectory(tests/compile_negative)
endif()

if(BUILD_TESTING)
    enable_testing()

    # `always` is the gate every supported test configuration carries, and the subdirectories
    # append the conditional ones: a population legitimately absent is declared absent that way
    # rather than by a lowered floor (VM-POP-08).
    set_property(GLOBAL PROPERTY ZENGINE_ACTIVE_GATES "always")

    add_subdirectory(smoke)
    add_subdirectory(tests)

    # The build tree's record of what it registered, for tests/verify.cmake. It is written
    # here, once, after every subdirectory has had its say -- and it is a DESCRIPTION. The
    # requirement lives in tests/test_population.txt, in the source tree, where deleting a
    # registration cannot reach it.
    get_property(zengine_active_gates GLOBAL PROPERTY ZENGINE_ACTIVE_GATES)
    # A set, not a tally: several suites declare the gate they ride, and a gate named twice is
    # still one gate.
    list(REMOVE_DUPLICATES zengine_active_gates)
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

# ---- the package a stranger installs and finds -----------------------------------------
# Last, since it names every target it exports and stages the loadable artifacts by
# $<TARGET_FILE:...>. The whole public consumer surface is decided in that one file, with the
# reason each package is or is not in it.
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/ZengineInstall.cmake)
