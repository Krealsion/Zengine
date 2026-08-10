# The Surface package — visual intent in, output out. One tiny vocabulary
# (SurfaceText + SurfaceReady + the zengine.skin role), and the Skins: the
# replaceable weaves that claim the actual surface and paint. Two terminal
# skins (classic / block — the old snake drawers' looks, now living where
# drawing lives) and one SDL skin (a real window; the proof the intent never
# leaked a TUI assumption).

# The vocabulary travels as a header-only interface target, same as input's:
# consumers spell `#include "surface/vocabulary.hpp"`. Portable — it exists
# even where no kernel does.
add_library(zengine-surface-vocabulary INTERFACE)
target_include_directories(zengine-surface-vocabulary INTERFACE ${CMAKE_SOURCE_DIR})

# The skins are loadable weaves; they need a hosting Loom (the same honest
# question every package asks: does this install export loom::kernel?).
if(NOT TARGET loom::kernel)
    message(STATUS "zengine: surface skins skipped — this Loom cannot host loadable weaves (no loom::kernel)")
    return()
endif()

# Skins ask the Timer package for their own beat (skin.hpp), so every skin
# speaks that vocabulary alongside its own.
zengine_weave(zengine-skin-tui-classic skin_tui_classic.cpp)
target_link_libraries(zengine-skin-tui-classic PRIVATE zengine-surface-vocabulary
                                                       zengine-timer-vocabulary
                                                       zengine-activation)
zengine_weave(zengine-skin-tui-block skin_tui_block.cpp)
target_link_libraries(zengine-skin-tui-block PRIVATE zengine-surface-vocabulary
                                                     zengine-timer-vocabulary
                                                     zengine-activation)

# ---- The SDL skin (a real window; the agnosticism proof) --------------------------------
#
# GATED, but ON by default. Only the two SDL-backed weaves see SDL — the vocabulary, the
# terminal skins, and the pure frame-planner (skin_sdl_plan.hpp, pinned by the suite on
# every lane) stay SDL-free. With no display the skin still loads and runs — SDL_Init
# fails, the medium stays disabled and SAYS WHY — and the suite drives the whole path
# under SDL's dummy driver.
#
# ACQUIRING SDL IS NO LONGER THIS PACKAGE'S JOB. It was, while this was the only target
# that linked it; G-1's SDL Input reader made SDL a repository-level dependency that two
# packages must SHARE (a static copy in each weave library is two event queues — see
# cmake/ZengineSdl.cmake, which owns the decision, the shared-library requirement and its
# refusal message).
if(ZENGINE_SDL_LIB)
    zengine_weave(zengine-skin-sdl skin_sdl.cpp)
    target_link_libraries(zengine-skin-sdl PRIVATE zengine-surface-vocabulary
                                                   zengine-timer-vocabulary zengine-activation)
    zengine_sdl_weave(zengine-skin-sdl)
endif()
