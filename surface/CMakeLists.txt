# The Surface package: visual intent in, output out. The vocabulary, and the Skins -- the
# replaceable weaves that claim the surface and paint it: two terminal skins (classic and block,
# the old snake drawers' looks) and one SDL skin, a real window.

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

# ---- The SDL skin (a real window) --------------------------------------------------------
#
# Gated, on by default. Only the two SDL-backed weaves see SDL; the vocabulary, the terminal
# skins and the pure planner (skin_sdl_plan.hpp) stay SDL-free. With no display the skin loads
# and says why it cannot draw. Acquiring SDL is cmake/ZengineSdl.cmake's, because two packages
# share it: a static copy in each weave would be two event queues.
if(ZENGINE_SDL_LIB)
    # ---- The face, carried rather than found -------------------------------------------
    #
    # The font's bytes become a translation unit in the build tree, compiled into the weave:
    # nothing installed, staged or discovered at runtime (surface/fonts/PROVENANCE.md). It
    # depends on the font, and the generated array is never checked in beside its source.
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
