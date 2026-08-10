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
target_link_libraries(zengine-input PRIVATE zengine-timer-vocabulary zengine-activation)

# ---- The SDL Input reader (G-1) ---------------------------------------------------------
#
# A SECOND artifact rather than a second reader inside the one above, because which
# surface a run is shown on is a LAUNCH decision and not a platform fact: the same Linux
# box runs the terminal Workshop and the graphical one, and `--input <stem>` is what
# chooses. The terminal/console reader is untouched and still builds everywhere.
#
# It is the only weave in the Input package that links the SURFACE vocabulary, and the
# reason is in translate_sdl.hpp: SDL carries window lifecycle and input on one queue, so
# the queue's owner is the only thing that can see a close request — which it forwards as
# the surface fact it is, never as a synthesized key.
#
# It shares SDL with the window Skin, and it must: two weave libraries each holding their
# own archived SDL would be two event queues. cmake/ZengineSdl.cmake requires a shared
# SDL3, refuses a static one out loud, and owns the $ORIGIN/staging half through
# zengine_sdl_weave().
if(ZENGINE_SDL_LIB)
    zengine_weave(zengine-input-sdl input_sdl.cpp)
    target_link_libraries(zengine-input-sdl PRIVATE zengine-timer-vocabulary zengine-activation
                                                    zengine-surface-vocabulary)
    zengine_sdl_weave(zengine-input-sdl)
endif()
