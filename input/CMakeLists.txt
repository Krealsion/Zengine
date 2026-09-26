# The Input package: one vocabulary (the input moments and the drive message) and the loadable
# weaves that produce them -- the only code that talks to the platform's input.

# The vocabulary travels as a header-only interface target so consumers (the
# snake controls adapter, the host, the suites) spell the include the same way:
#   #include "input/vocabulary.hpp"
# It is portable — shapes are just ZEN_SHAPE structs — so it exists even where
# no kernel does.
add_library(zengine-input-vocabulary INTERFACE)
target_include_directories(zengine-input-vocabulary INTERFACE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/zengine>)
add_library(zengine::input ALIAS zengine-input-vocabulary)
# vocabulary.hpp includes <zen/weave/shape.hpp>, and input_weave.hpp -- the layer a
# backend author builds a reader on -- includes timer/binding.hpp for the poll rhythm it
# declares. Both stated, for the same reason the Timer's are.
target_link_libraries(zengine-input-vocabulary INTERFACE loom::core zengine::timer zengine::component)

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

# The SDL Input reader: a second artifact, since which surface a run is shown on is a launch
# decision (`--input <stem>`), not a platform fact; the terminal/console reader builds everywhere.
# It is the only Input weave linking the Surface vocabulary (a close request rides SDL's one
# queue, forwarded as the surface fact it is), and it shares SDL with the window Skin:
# cmake/ZengineSdl.cmake requires a shared SDL3 and owns the staging half (`zengine_sdl_weave`).
if(ZENGINE_SDL_LIB)
    zengine_weave(zengine-input-sdl input_sdl.cpp)
    target_link_libraries(zengine-input-sdl PRIVATE zengine-timer-vocabulary zengine-activation
                                                    zengine-surface-vocabulary)
    zengine_sdl_weave(zengine-input-sdl)
endif()
