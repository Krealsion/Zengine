# The Input package — the floor games sit on. One vocabulary (the five locked
# shapes + the drive message), one loadable weave that is the sole producer of
# those shapes and the only code that talks to the platform.

# The vocabulary travels as a header-only interface target so consumers (the
# snake controls adapter, the host, the suites) spell the include the same way:
#   #include "input/vocabulary.hpp"
# It is portable — shapes are just ZEN_SHAPE structs — so it exists even where
# no kernel does.
add_library(zengine-input-vocabulary INTERFACE)
target_include_directories(zengine-input-vocabulary INTERFACE ${CMAKE_SOURCE_DIR})

# The weave itself needs a Loom that can HOST loadable weaves (same honest
# question snake asks: does this install export loom::kernel?).
if(NOT TARGET loom::kernel)
    message(STATUS "zengine: input weave skipped — this Loom cannot host loadable weaves (no loom::kernel)")
    return()
endif()

# The weave arranges its own heartbeat now: it accepts the Timer package's
# hello and beat, so it speaks that vocabulary too.
zengine_weave(zengine-input input.cpp)
target_link_libraries(zengine-input PRIVATE zengine-timer-vocabulary)
