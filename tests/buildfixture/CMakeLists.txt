# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
#
# The Builder suite's own CMake project: a real configured build tree with real targets, the only
# thing a `CMakeTargetRecipe` can point at, since a recipe names inputs to a mechanism and never a
# program. `LANGUAGES NONE`, so it configures in milliseconds with no compiler: custom targets that
# succeed, fail, take a measured time and speak as they go (tests/slow_build.cmake), or leave one
# known file. A parameterisation is a target, since `cmake --build --target` takes no arguments.

cmake_minimum_required(VERSION 3.16)
project(zengine-build-fixture NONE)

# Handed in by the suite's own CMakeLists at configure time. Named rather than derived,
# because a fixture that guessed where the suite expects its artifact would be a fixture
# that can silently stop being about the same file.
foreach(required ZENGINE_FIXTURE_ARTIFACTS ZENGINE_FIXTURE_SUFFIX ZENGINE_FIXTURE_SLOW
                 ZENGINE_FIXTURE_DIAGNOSTIC)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "build fixture: -D${required}=... is required")
    endif()
endforeach()

# ---- the one shape every succeeding target has ------------------------------------------
# A succeeding target produces its own artifact, named after itself, in the directory the suite
# handed in: `outcome::kSucceeded` means the process exited zero and the recipe's file is there.
# The touch is last, so a target whose command fails stops before it and leaves nothing behind.
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

# A build that fails the way a COMPILER fails: a diagnostic with UTF-8 quotes, a CR LF line, a
# path holding a space and a command echo longer than one `BuildOutput`, then a non-zero exit.
# It is what the output reader has to carry whole, and `diagnostic.txt` beside this tree holds
# the same bytes for a case to compare (`tests/diagnostic_build.cmake` says what is in them).
# Also not `fixture_target`: it fails, so it leaves no artifact.
add_custom_target(fixture-diagnostic
    COMMAND ${CMAKE_COMMAND} "-DOUT=${CMAKE_CURRENT_BINARY_DIR}/diagnostic.txt"
            -P "${ZENGINE_FIXTURE_DIAGNOSTIC}"
    COMMAND ${CMAKE_COMMAND} -E false)

# The slow, chatty ones, each a parameterisation of tests/slow_build.cmake at the numbers its
# cases were written against.
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
