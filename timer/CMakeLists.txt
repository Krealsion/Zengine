# The Timer package: time, message-shaped. The vocabulary, and one loadable weave that is its
# sole authority and the only place in the running system that reads the OS clock or sleeps.

# The vocabulary travels as a header-only interface target, same as input's:
# consumers spell `#include "timer/vocabulary.hpp"`. Portable — shapes are
# just ZEN_SHAPE structs — so it exists even where no kernel does.
add_library(zengine-timer-vocabulary INTERFACE)
target_include_directories(zengine-timer-vocabulary INTERFACE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/zengine>)
add_library(zengine::timer ALIAS zengine-timer-vocabulary)
# What the two shipped headers need, on their own lines: vocabulary.hpp includes
# <zen/weave/shape.hpp>, and binding.hpp <zen/weave.hpp> and the activation cursor. In this tree
# the omission would be invisible; from an installed package it decides whether a consumer compiles.
target_link_libraries(zengine-timer-vocabulary INTERFACE loom::core zengine::activation)

# The weave itself needs a Loom that can HOST loadable weaves (the same
# honest question every package asks: does this install export loom::kernel?).
if(NOT TARGET loom::kernel)
    message(STATUS "zengine: timer weave skipped — this Loom cannot host loadable weaves (no loom::kernel)")
    return()
endif()

zengine_weave(zengine-timer timer.cpp)
# The service reads its own activation, and executes its delay rule rather than implementing it:
# it authors the composition and exports it across the provider seam (`zengine-operator`, also
# what a fallback Timer assembles its local vocabulary with), and it consumes a host's operator
# surface (`zengine-operator-consumer`), named on its own line because a provider that also
# consumes should say so. Both are header-only INTERFACE targets.
target_link_libraries(zengine-timer
    PRIVATE zengine-activation zengine-operator zengine-operator-consumer)
