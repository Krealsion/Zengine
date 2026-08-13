# A deliberately slow, deliberately chatty stand-in for a build.
#
# ASYNC-1's central claim is that a build outlives the turn that started it and
# that Zen keeps working while it runs. A claim like that cannot be measured
# against a child that finishes before anybody can look at it: `cmake -E echo`
# exits in milliseconds, so a suite driven by one would be measuring a race and
# calling it a property. This script is the controlled recipe that makes the
# overlap undeniable -- it takes as long as it is told to, and it says something
# at intervals rather than all at once, which is what an incremental-output claim
# needs in order to be falsifiable.
#
# IT IS A CMAKE SCRIPT AND NOT A SHELL SCRIPT, and that is the same restraint the
# package it tests is built on: it is run as `cmake -P`, by THE cmake that
# configured this tree, so the suite needs no shell, no /bin/sh, no .bat, and no
# assumption about what else this machine has installed -- on either platform
# this repository builds for.
#
# EACH LINE IS PRINTED BY A CHILD PROCESS, and that is deliberate rather than
# roundabout. `message()` writes to a stdout that is block-buffered when it is a
# pipe, so a script that used it could legitimately deliver every one of its
# lines at exit -- which would make an "output arrives while it runs" test pass
# or fail on a buffering detail rather than on the thing it is testing. A short
# child process that exits after each line is flushed by its own exit, so what
# reaches the pipe reaches it when the line was written.
#
#   STEPS   how many things to say, with a pause between each
#   PAUSE   how long each pause is, in seconds
#   FAIL    when true, exit non-zero after saying everything -- the failing build
#
# Nothing here compiles anything, and nothing about it is a second build system:
# it is a stand-in for the DURATION and the CHATTER of a real build, which are
# the only two properties the tests using it care about.

if(NOT DEFINED STEPS)
    set(STEPS 3)
endif()
if(NOT DEFINED PAUSE)
    set(PAUSE 0.2)
endif()

foreach(step RANGE 1 ${STEPS})
    execute_process(COMMAND ${CMAKE_COMMAND} -E echo "slow build: step ${step} of ${STEPS}")
    execute_process(COMMAND ${CMAKE_COMMAND} -E sleep ${PAUSE})
endforeach()
execute_process(COMMAND ${CMAKE_COMMAND} -E echo "slow build: done")

if(FAIL)
    message(FATAL_ERROR "slow build: this one was asked to fail")
endif()
