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
