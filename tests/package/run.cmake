# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
#
# THE INSTALLED-PACKAGE WITNESS DRIVER (PKG-0).
#
#   cmake -DZEN_BUILD_DIR=<a configured, built Zengine build tree> \
#         -DZEN_WORK=<scratch dir outside both repositories> \
#         [-DZEN_CONFIG=Debug] [-DZEN_GENERATOR=...] [-DZEN_CMAKE_ARGS=...] \
#         -P tests/package/run.cmake
#
# It installs Zengine into an isolated prefix, audits the prefix, copies the stranger project
# OUT of this repository, builds and runs it against the prefix alone, and then tries to break
# it in the two ways that would mean the package is not really self-contained.
#
# WHY IT IS NOT A CTEST ENTRY. Everything the ordinary lane runs is inside one build tree;
# this one installs, relocates and configures a second, unrelated project, and folding that
# into `ctest` would put a nested CMake build inside a test binary's population. It is one
# command a human runs and the same one CI runs, so a local check and the hosted lane cannot
# come to mean different things.
#
# WHAT IT ASKS, in order:
#   1. does Zengine install into an isolated prefix at all
#   2. does the installed package name any path back to the machine that built it
#   3. does any installed public material assume this project's development environment
#   4. does an unrelated project outside both trees configure with find_package(zengine)
#   5. do all eight exported targets compile and run from the installed headers
#   6. does a real weave build, load and drive the installed Timer service -- and does the
#      successful run look successful, while the same program's real failure still speaks
#   7. does the same package still work after the prefix is MOVED
#   8. CANARY: with one installed header removed, does the stranger go RED -- or does it
#      quietly find the header in the source tree that is still sitting right there

foreach(v ZEN_BUILD_DIR ZEN_WORK)
    if(NOT DEFINED ${v})
        message(FATAL_ERROR "package witness: -D${v}=... is required")
    endif()
endforeach()
if(NOT DEFINED ZEN_CONFIG OR ZEN_CONFIG STREQUAL "")
    set(ZEN_CONFIG Debug)
endif()

get_filename_component(here "${CMAKE_CURRENT_LIST_DIR}" ABSOLUTE)
get_filename_component(repo "${here}/../.." ABSOLUTE)
get_filename_component(build_dir "${ZEN_BUILD_DIR}" ABSOLUTE)
get_filename_component(work "${ZEN_WORK}" ABSOLUTE)

# The whole claim is "outside the source tree", so this refuses to be run somewhere that would
# make it untrue by construction.
string(FIND "${work}" "${repo}/" work_inside)
if(work_inside EQUAL 0)
    message(FATAL_ERROR
        "package witness: ZEN_WORK is inside the Zengine repository (${work}). The stranger "
        "must be built outside it, or 'it does not need the source tree' is unproven.")
endif()

function(zen_run label)
    execute_process(COMMAND ${ARGN} RESULT_VARIABLE rc)
    if(NOT rc EQUAL 0)
        message(FATAL_ERROR "package witness: ${label} FAILED (exit ${rc})")
    endif()
    message(STATUS "package witness: ${label} ok")
endfunction()

# The generator is CHOSEN, not assumed: -DZEN_GENERATOR=... wins, else Ninja when it is on
# PATH, else CMake's platform default. Hard-coding one would make this witness unrunnable on
# an ordinary Makefiles host, and a proof a consumer cannot run is not much of a proof.
set(gen_args "")
if(DEFINED ZEN_GENERATOR AND NOT ZEN_GENERATOR STREQUAL "")
    set(gen_args -G "${ZEN_GENERATOR}")
else()
    find_program(ZEN_NINJA ninja)
    if(ZEN_NINJA)
        set(gen_args -G Ninja)
    endif()
endif()

# ---- 1. install into an isolated prefix ------------------------------------------------
set(prefix "${work}/prefix")
file(REMOVE_RECURSE "${prefix}")
zen_run("install into an isolated prefix" ${CMAKE_COMMAND} --install "${build_dir}"
        --config "${ZEN_CONFIG}" --prefix "${prefix}")

if(NOT EXISTS "${prefix}/lib/cmake/zengine/zengineConfig.cmake")
    message(FATAL_ERROR
        "package witness: no zengineConfig.cmake in ${prefix}. The build tree was configured "
        "with ZENGINE_INSTALL=OFF, or the install rules did not run.")
endif()

# ---- 2. does the package name the machine that built it? -------------------------------
#
# The failure this catches is an absolute path -- to the checkout, to the build tree, to a
# developer's home -- baked into a generated package file, which works perfectly on the
# machine that produced it and nowhere else.
file(GLOB package_files "${prefix}/lib/cmake/zengine/*.cmake")
foreach(f IN LISTS package_files)
    file(READ "${f}" text)
    foreach(forbidden "${repo}" "${build_dir}")
        string(FIND "${text}" "${forbidden}" at)
        if(NOT at EQUAL -1)
            get_filename_component(name "${f}" NAME)
            message(FATAL_ERROR
                "package witness: ${name} contains the absolute path '${forbidden}'. An "
                "installed package that names the machine that built it is not a package.")
        endif()
    endforeach()
endforeach()
message(STATUS "package witness: no build-machine paths in the installed package ok")

# ---- 3. does installed public material assume this project's development environment? ---
#
# Public material only -- the headers a consumer reads and the package files CMake executes.
# Not the repository's own comments, where a phase name or a workspace path is legitimate
# internal history. The list is the class a stranger cannot make sense of: this workspace's
# layout, its private siblings, and the way its work is organised.
set(forbidden_words
    "playground/" "reportback" "Zen/private" "zen-night-lab"
    "warm executor" "cold executor" "memory graph")
file(GLOB_RECURSE public_material "${prefix}/include/*" "${prefix}/lib/cmake/zengine/*")
set(leaks "")
foreach(f IN LISTS public_material)
    file(READ "${f}" text)
    foreach(word IN LISTS forbidden_words)
        string(TOLOWER "${text}" lowered)
        string(TOLOWER "${word}" lowered_word)
        string(FIND "${lowered}" "${lowered_word}" at)
        if(NOT at EQUAL -1)
            file(RELATIVE_PATH rel "${prefix}" "${f}")
            list(APPEND leaks "${rel}: ${word}")
        endif()
    endforeach()
endforeach()
if(leaks)
    string(REPLACE ";" "\n  " pretty "${leaks}")
    message(FATAL_ERROR
        "package witness: installed public material assumes this project's development "
        "environment:\n  ${pretty}")
endif()
message(STATUS "package witness: installed public material assumes no development environment ok")

# ---- the Loom prefix this build consumed, read from the build it consumed it in ---------
#
# The stranger needs it on CMAKE_PREFIX_PATH because zengineConfig.cmake resolves the Loom
# through find_dependency, and a dependency has to be findable to be found. Read out of the
# build tree's own cache rather than passed in, so this driver cannot be handed a different
# Loom from the one the artifacts were built against.
file(READ "${build_dir}/CMakeCache.txt" cache)
string(REGEX MATCH "CMAKE_PREFIX_PATH:[A-Z]+=([^\n]*)" _m "${cache}")
set(loom_prefix "${CMAKE_MATCH_1}")
if(loom_prefix STREQUAL "")
    message(FATAL_ERROR
        "package witness: ${build_dir}/CMakeCache.txt has no CMAKE_PREFIX_PATH, so the Loom "
        "this package depends on cannot be located for the stranger.")
endif()

# ---- 4-6. the stranger, built outside both repositories --------------------------------
set(stranger_src "${work}/kitchen")
file(REMOVE_RECURSE "${stranger_src}")
file(MAKE_DIRECTORY "${stranger_src}")
file(GLOB fixture "${here}/CMakeLists.txt" "${here}/*.cpp" "${here}/*.hpp")
file(COPY ${fixture} DESTINATION "${stranger_src}")

# A build of the copy, against the prefix and the Loom and nothing else. ZEN_CMAKE_ARGS
# carries only what selects a toolchain; if a compile option is ever needed here, the package
# has stopped carrying its own requirements and this is supposed to fail.
function(zen_stranger_build label src bin prefixes)
    execute_process(
        COMMAND ${CMAKE_COMMAND} -S "${src}" -B "${bin}" ${gen_args}
                "-DCMAKE_BUILD_TYPE=${ZEN_CONFIG}"
                "-DCMAKE_PREFIX_PATH=${prefixes}"
                ${ZEN_CMAKE_ARGS}
        RESULT_VARIABLE configure_rc)
    if(NOT configure_rc EQUAL 0)
        set(${label}_rc ${configure_rc} PARENT_SCOPE)
        return()
    endif()
    execute_process(COMMAND ${CMAKE_COMMAND} --build "${bin}" --config "${ZEN_CONFIG}"
                    RESULT_VARIABLE build_rc)
    set(${label}_rc ${build_rc} PARENT_SCOPE)
endfunction()

set(stranger_bin "${work}/kitchen-build")
file(REMOVE_RECURSE "${stranger_bin}")
zen_stranger_build(primary "${stranger_src}" "${stranger_bin}" "${prefix};${loom_prefix}")
if(NOT primary_rc EQUAL 0)
    message(FATAL_ERROR
        "package witness: the stranger project failed to configure or build against the "
        "installed package alone (exit ${primary_rc})")
endif()
message(STATUS "package witness: stranger configures and builds outside both trees ok")

function(zen_find_program_in out name dir)
    find_program(found "${name}" PATHS "${dir}" "${dir}/${ZEN_CONFIG}" NO_DEFAULT_PATH)
    if(NOT found)
        message(FATAL_ERROR "package witness: ${name} was not produced in ${dir}")
    endif()
    set(${out} "${found}" PARENT_SCOPE)
    unset(found CACHE)
endfunction()

zen_find_program_in(surfaces witness-surfaces "${stranger_bin}")
zen_run("every exported target, from the installed headers" "${surfaces}")

zen_find_program_in(kitchen kitchen-host "${stranger_bin}")
get_filename_component(kitchen_dir "${kitchen}" DIRECTORY)
zen_run("a weave drives the installed Timer service" "${kitchen}" "${kitchen_dir}")

# ---- 6b. and the same program's nearby genuine failure (FRIC-0) -------------------------
#
# The arm above passes only if the successful run reported NO refusal, which on its own is
# a claim about volume and would be satisfied by diagnostics that had stopped working. This
# is the other half: the identical program with the Timer service left out, which fails for
# real and must still say so precisely -- the shape, and the office it was addressed to.
# Together they are the only pair that can tell a quieter runtime from a blinder one.
zen_run("a genuine failure in the same program is still reported, with its destination"
        "${kitchen}" "${kitchen_dir}" --no-timer)

# ---- 7. the same package, moved --------------------------------------------------------
#
# An install prefix that only works where it was written is not relocatable, and the way that
# happens is an absolute path generated into it. Step 2 reads the package files for one; this
# asks the question the way a consumer would.
set(moved "${work}/prefix-moved")
file(REMOVE_RECURSE "${moved}")
file(RENAME "${prefix}" "${moved}")
set(moved_bin "${work}/kitchen-build-moved")
file(REMOVE_RECURSE "${moved_bin}")
zen_stranger_build(relocated "${stranger_src}" "${moved_bin}" "${moved};${loom_prefix}")
if(NOT relocated_rc EQUAL 0)
    message(FATAL_ERROR
        "package witness: the package stopped working after its prefix was moved to "
        "${moved} (exit ${relocated_rc}). Something in it names where it was installed.")
endif()
zen_find_program_in(moved_kitchen kitchen-host "${moved_bin}")
get_filename_component(moved_dir "${moved_kitchen}" DIRECTORY)
zen_run("the same package works from a moved prefix" "${moved_kitchen}" "${moved_dir}")

# ---- 8. CANARY: remove one installed header ---------------------------------------------
#
# The failure mode this discriminates is the one that produced this phase: a consumer that
# looks like it uses the package while actually reading Zengine's source tree. The Zengine
# checkout is fully present and readable during this step -- if the stranger still builds with
# `timer/vocabulary.hpp` deleted from the prefix, it is finding it somewhere else, and every
# green above meant nothing.
set(canary "${work}/prefix-canary")
file(REMOVE_RECURSE "${canary}")
file(COPY "${moved}/" DESTINATION "${canary}")
file(REMOVE "${canary}/include/zengine/timer/vocabulary.hpp")
if(EXISTS "${canary}/include/zengine/timer/vocabulary.hpp")
    message(FATAL_ERROR "package witness: the canary could not remove the header it needs to")
endif()
set(canary_bin "${work}/kitchen-build-canary")
file(REMOVE_RECURSE "${canary_bin}")
zen_stranger_build(canary "${stranger_src}" "${canary_bin}" "${canary};${loom_prefix}")
if(canary_rc EQUAL 0)
    message(FATAL_ERROR
        "package witness: CANARY DID NOT FIRE. The stranger built with "
        "include/zengine/timer/vocabulary.hpp deleted from the prefix, so it is not reading "
        "the installed headers -- it is finding Zengine's source tree, which is still present "
        "at ${repo}. Every result above is void.")
endif()
message(STATUS "package witness: canary fired -- a missing installed header is fatal ok")

message(STATUS "package witness: PASSED")
