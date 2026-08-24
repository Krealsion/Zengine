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
target_include_directories(zengine-surface-vocabulary INTERFACE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/zengine>)
add_library(zengine::surface ALIAS zengine-surface-vocabulary)
# vocabulary.hpp includes <zen/weave/shape.hpp>; the geometry headers beside it
# (cells/region/pointing/terminal_size) need nothing else. Declared here for the same
# reason as the Timer's: a link that only ever worked transitively is not a declaration.
target_link_libraries(zengine-surface-vocabulary INTERFACE loom::core)

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
    # ---- The face, carried rather than found (HD-1) ----------------------------------
    #
    # The bytes of surface/fonts/JetBrainsMono-Regular.ttf become a C++ translation
    # unit at build time and are compiled INTO the weave. Three consequences worth
    # having: nothing is installed, nothing is staged, and nothing is discovered at
    # runtime -- so a Skin either has its face or does not, and the "does not" path
    # is a visible complaint plus the bitmap fallback rather than a font from
    # whichever machine happened to run it. See surface/fonts/PROVENANCE.md.
    #
    # Generated into the BUILD tree, and depending on the FONT rather than on a
    # timestamp: change the file and the unit regenerates; change nothing and it does
    # not. Checking the generated array in beside the file it came from would be a
    # second copy of the same truth, in the format that decays.
    set(zengine_font_unit "${CMAKE_CURRENT_BINARY_DIR}/zengine_skin_font.cpp")
    add_custom_command(
        OUTPUT ${zengine_font_unit}
        COMMAND ${CMAKE_COMMAND}
                -DZEN_EMBED_INPUT=${CMAKE_CURRENT_SOURCE_DIR}/fonts/JetBrainsMono-Regular.ttf
                -DZEN_EMBED_OUTPUT=${zengine_font_unit}
                -DZEN_EMBED_NAME=kSkinFontBytes
                -DZEN_EMBED_NAMESPACE=zengine::surface
                -P ${CMAKE_SOURCE_DIR}/cmake/EmbedBinary.cmake
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/fonts/JetBrainsMono-Regular.ttf
                ${CMAKE_SOURCE_DIR}/cmake/EmbedBinary.cmake
        COMMENT "zengine-skin-sdl: embedding JetBrains Mono into the weave"
        VERBATIM)

    zengine_weave(zengine-skin-sdl skin_sdl.cpp ${zengine_font_unit})
    target_link_libraries(zengine-skin-sdl PRIVATE zengine-surface-vocabulary
                                                   zengine-timer-vocabulary zengine-activation)
    zengine_sdl_weave(zengine-skin-sdl)
    target_link_libraries(zengine-skin-sdl PRIVATE ${ZENGINE_SDL_TEXT_LIB})
    # The generated unit is a quarter of a million initializers of somebody else's
    # data; the repository's warning discipline is about code this repository wrote.
    set_source_files_properties(${zengine_font_unit} PROPERTIES COMPILE_OPTIONS "-w")
endif()
