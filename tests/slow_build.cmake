# A deliberately slow, chatty stand-in for a build: that a build outlives the turn that started it
# is measurable only against a child that runs as long as it is told and speaks at intervals. Run
# as `cmake -P` by the cmake that configured this tree, so the suite needs no shell; each line is
# printed by a child whose exit flushes it, since `message()` into a pipe is block-buffered and
# could deliver every line at exit.
#   STEPS  lines to say, a pause between each;  PAUSE  seconds per pause;  FAIL  exit non-zero

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
