# The Timer: the messages that give a weave time, what a source claims of the host's one wait
# (`source_wait.hpp`), and one loadable weave, the only keeper of time, which that wait sends the
# time.

# The messages are a header-only interface target, as Input's are, and a program includes
# `timer/vocabulary.hpp`. They are only ZEN_SHAPE structs, so the target exists even where the
# installed Loom has no kernel.
add_library(zengine-timer-vocabulary INTERFACE)
target_include_directories(zengine-timer-vocabulary INTERFACE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/zengine>)
add_library(zengine::timer ALIAS zengine-timer-vocabulary)
# What the installed headers need, each named: vocabulary.hpp includes <zen/weave/shape.hpp>,
# binding.hpp includes <zen/weave.hpp> and the activation cursor, and source_wait.hpp includes
# <zen/host/wait_set.hpp>. Leaving one out would show nothing in this tree, but from an installed
# package it decides whether a program that uses the Timer compiles.
target_link_libraries(zengine-timer-vocabulary INTERFACE loom::core zengine::activation)

# The weave itself needs a Loom that can host loadable weaves, which every package checks the same
# way: does the installed Loom export loom::kernel?
if(NOT TARGET loom::kernel)
    message(STATUS "zengine: timer weave skipped — this Loom cannot host loadable weaves (no loom::kernel)")
    return()
endif()

# The weave carries the Timer's manual and answers a shape's section of it to anyone who asks
# (`ShapeDocumentRequested`, manual/vocabulary.hpp). Every target that builds `TimerWeave` carries
# the page too, because timer_weave.hpp answers from it.
zengine_weave(zengine-timer timer.cpp MANUAL docs/zengine.timer.md)
# The Timer reads its own activation, and it runs its delay rule rather than writing it in C++. It
# builds the rule and supplies it to a host (`zengine-operator`, which a local Timer also builds its
# own catalog with), and it can use a host's operators (`zengine-operator-consumer`). The second is
# named on its own because a library that both supplies operators and uses them should show both.
# Both are header-only INTERFACE targets.
target_link_libraries(zengine-timer
    PRIVATE zengine-activation zengine-operator zengine-operator-consumer)
