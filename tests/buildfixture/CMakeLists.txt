# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
#
# THE BUILDER SUITE'S OWN CMAKE PROJECT -- a real configured build tree with real targets,
# which is the only thing a `CMakeTargetRecipe` can point at (BLD-1).
#
# WHY IT HAD TO EXIST. BLD-0's suite handed the runner a `BuildRecipe` holding a program
# and an argument vector, so a case could say "run cmake -E echo hello" and measure what
# came back. BLD-1 deleted that possibility on purpose: a recipe names INPUTS to a
# mechanism (a configured build tree and a target, or a source file and its packages) and
# there is no field anywhere in which one could name a program. That is the phase's
# central authority claim -- and its immediate cost is that the suite can no longer invent
# a command either.
#
# So the suite gets what a maker gets: a real CMake project, configured once at BUILD
# time, whose targets it drives through the ordinary production path
#
#     Recipe -> prepare() -> cmake --build <tree> --target <t> -> a child process
#
# Every case below therefore measures the path Workshop actually uses, which the old
# fixture could not claim: it measured the runner with a command nothing in production
# could produce.
#
# `LANGUAGES NONE`, AND THAT IS WHAT MAKES IT CHEAP AND PORTABLE. There is no compiler
# check, no ABI probe and no toolchain requirement -- configuring this takes milliseconds
# on every platform this repository builds for, and it needs no compiler even to exist.
# What it holds is custom targets, which is exactly what a suite needs: a thing that
# succeeds, a thing that fails, a thing that takes a measurable amount of time and says
# so as it goes, and a thing that produces a FILE at a path the suite knows.
#
# THE SLOW ONES ARE THE SAME SCRIPT ASYNC-1 WROTE (`tests/slow_build.cmake`), for its
# reason: a claim that a build outlives the turn that started it cannot be measured
# against a child that finishes before anybody can look. The targets are named by their
# parameters because `cmake --build --target` carries no arguments -- a parameterisation
# is a target here, which is a small price for driving the real path.
#
# NOTHING HERE COMPILES ANYTHING and none of it is a second build system. It is a set of
# processes with known durations, known chatter, known exit statuses and one known file.

cmake_minimum_required(VERSION 3.16)
project(zengine-build-fixture NONE)

# Handed in by the suite's own CMakeLists at configure time. Named rather than derived,
# because a fixture that guessed where the suite expects its artifact would be a fixture
# that can silently stop being about the same file.
foreach(required ZENGINE_FIXTURE_ARTIFACTS ZENGINE_FIXTURE_SUFFIX ZENGINE_FIXTURE_SLOW)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "build fixture: -D${required}=... is required")
    endif()
endforeach()

# ---- The one shape every succeeding target has ------------------------------------------
#
# A target that succeeds PRODUCES ITS OWN ARTIFACT, named after itself, in the directory the
# suite handed in. That is not decoration: `outcome::kSucceeded` in BLD-1 means the process
# exited zero AND the file the recipe names is there, so a fixture whose successes produced
# nothing would make every "it built" case in this suite assert `kNoArtifact` instead --
# which is a real outcome with its own case below and is not what those cases are about.
#
# THE TOUCH IS LAST, and that is what makes a FAILING target leave nothing behind: a custom
# target's commands stop at the first non-zero exit, so the artifact of a build that broke
# is never written by the build that broke.
function(fixture_target name)
    add_custom_target(${name}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${ZENGINE_FIXTURE_ARTIFACTS}"
        ${ARGN}
        COMMAND ${CMAKE_COMMAND} -E touch
                "${ZENGINE_FIXTURE_ARTIFACTS}/${name}${ZENGINE_FIXTURE_SUFFIX}")
endfunction()

# A build that succeeds at once, says one thing, and produces its artifact.
fixture_target(fixture-quick
    COMMAND ${CMAKE_COMMAND} -E echo "fixture: nothing to build, said quickly")

# ...and one that ALSO produces a second, perfectly valid-looking artifact beside its own.
# It is the no-scanning witness: a recipe still identifies only its own authored product,
# and a neighbour it never named can neither satisfy it nor confuse it.
fixture_target(fixture-neighbour
    COMMAND ${CMAKE_COMMAND} -E echo "fixture: and a neighbour nobody asked for"
    COMMAND ${CMAKE_COMMAND} -E touch
            "${ZENGINE_FIXTURE_ARTIFACTS}/fixture-bystander${ZENGINE_FIXTURE_SUFFIX}")

# A build that succeeds and produces NOTHING. The recipe that names it declares an artifact
# anyway, which is how "a process exited zero and the artifact is absent" becomes a case
# rather than a worry. It is the one target here that is deliberately not `fixture_target`.
add_custom_target(fixture-empty
    COMMAND ${CMAKE_COMMAND} -E echo "fixture: this build makes no artifact at all")

# A build that fails, quickly, having said something first. Also not `fixture_target`: what
# it must never do is leave a file behind, because a failed build that produced its artifact
# would make the stale-artifact question unanswerable.
add_custom_target(fixture-broken
    COMMAND ${CMAKE_COMMAND} -DSTEPS=1 -DPAUSE=0.05 -DFAIL=1 -P "${ZENGINE_FIXTURE_SLOW}")

# The slow, chatty ones. Each is one parameterisation of ASYNC-1's script, and the numbers
# are the ones its cases were written against. A parameterisation is a TARGET here, because
# `cmake --build --target` carries no arguments -- which is a small price for driving the
# path production drives.
fixture_target(fixture-slow5
    COMMAND ${CMAKE_COMMAND} -DSTEPS=5 -DPAUSE=0.15 -P "${ZENGINE_FIXTURE_SLOW}")
fixture_target(fixture-slow4
    COMMAND ${CMAKE_COMMAND} -DSTEPS=4 -DPAUSE=0.15 -P "${ZENGINE_FIXTURE_SLOW}")
fixture_target(fixture-slow2
    COMMAND ${CMAKE_COMMAND} -DSTEPS=2 -DPAUSE=0.1 -P "${ZENGINE_FIXTURE_SLOW}")
fixture_target(fixture-chatty6
    COMMAND ${CMAKE_COMMAND} -DSTEPS=6 -DPAUSE=0.05 -P "${ZENGINE_FIXTURE_SLOW}")
fixture_target(fixture-forever
    COMMAND ${CMAKE_COMMAND} -DSTEPS=20 -DPAUSE=0.5 -P "${ZENGINE_FIXTURE_SLOW}")
