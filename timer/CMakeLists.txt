# The Timer package — time, message-shaped. One vocabulary (the four locked
# shapes + the three named additions), one loadable weave that is the sole
# authority for them and the only place in the running system that reads the
# OS clock or sleeps.

# The vocabulary travels as a header-only interface target, same as input's:
# consumers spell `#include "timer/vocabulary.hpp"`. Portable — shapes are
# just ZEN_SHAPE structs — so it exists even where no kernel does.
add_library(zengine-timer-vocabulary INTERFACE)
target_include_directories(zengine-timer-vocabulary INTERFACE ${CMAKE_SOURCE_DIR})

# The weave itself needs a Loom that can HOST loadable weaves (the same
# honest question every package asks: does this install export loom::kernel?).
if(NOT TARGET loom::kernel)
    message(STATUS "zengine: timer weave skipped — this Loom cannot host loadable weaves (no loom::kernel)")
    return()
endif()

zengine_weave(zengine-timer timer.cpp)
# The service authors its beat chain from its own activation, so it reads one.
# It also EXECUTES a semantic rule rather than privately implementing one, so it
# consumes the operator package — see timer/normalize.hpp for what that buys and
# what it costs.
#
# TWO OPERATOR TARGETS, AND THE PAIR IS THE POINT (CAT-0). This artifact is both
# halves of OPH-0's distinction and it is the first thing in the repository to be
# both: it AUTHORS the delay vocabulary (`zengine-operator`, which is what lets a
# host publish `timer::standard_operators()` and what a fallback Timer carries),
# and it CONSUMES a host's runtime operator surface (`zengine-operator-consumer`,
# the C table and the handle). Naming the consumer explicitly rather than leaning
# on the transitive edge is deliberate: a provider that also consumes should say
# so on its own line. Neither target is a library -- both are header-only
# INTERFACE targets -- so this adds no shared-object dependency.
target_link_libraries(zengine-timer
    PRIVATE zengine-activation zengine-operator zengine-operator-consumer)
