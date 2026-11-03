# The Graphics package: the vocabulary an owner draws with (a node, an element record, seven
# operators, a canvas, a colour by name), the windows that show what owners draw -- the terminal
# window and the OS window -- and the directory that introduces an area to its window and passes it
# the palettes offered. The pool's names and the shipped themes are JSON, generated into C++ here.

# The vocabulary and its helpers travel as a header-only interface target, and install with the
# package (cmake/ZengineInstall.cmake): consumers spell
# `#include "graphics/vocabulary.hpp"`. The seams' C headers are among them. Its link line is what
# the headers include: the Loom's values and shapes, and the activation cursor a drawing weave
# keeps (`drawing.hpp`); the roles and the terminal's claim are its own headers. The text pieces'
# drawn faces draw from the component values (`elements/text_face.hpp`).
add_library(zengine-graphics-vocabulary INTERFACE)
target_include_directories(zengine-graphics-vocabulary INTERFACE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/zengine>)
add_library(zengine::graphics ALIAS zengine-graphics-vocabulary)
target_link_libraries(zengine-graphics-vocabulary INTERFACE loom::core zengine-activation
    zengine-component)

# The pool's names (`zengine::graphics::colors`, graphics/colors.hpp) and the shipped themes as
# palettes (`zengine::graphics::themes`, graphics/themes.hpp): generated, and installed beside the
# vocabulary with the JSON they come from.
set(ZENGINE_GRAPHICS_POOL ${CMAKE_CURRENT_SOURCE_DIR}/pool.json)
set(ZENGINE_GRAPHICS_THEMES ${CMAKE_CURRENT_SOURCE_DIR}/themes/classic.json
    ${CMAKE_CURRENT_SOURCE_DIR}/themes/block.json ${CMAKE_CURRENT_SOURCE_DIR}/themes/day.json
    ${CMAKE_CURRENT_SOURCE_DIR}/themes/night.json)
zengine_names(zengine-graphics-vocabulary JSON ${ZENGINE_GRAPHICS_POOL}
              NAMESPACE zengine::graphics::colors HEADER graphics/colors.hpp)
zengine_palettes(zengine-graphics-vocabulary POOL ${ZENGINE_GRAPHICS_POOL}
                 THEMES ${ZENGINE_GRAPHICS_THEMES}
                 NAMESPACE zengine::graphics::themes HEADER graphics/themes.hpp)
set(ZENGINE_GRAPHICS_GENERATED
    ${CMAKE_CURRENT_BINARY_DIR}/zengine-names/zengine-graphics-vocabulary/graphics/colors.hpp
    ${CMAKE_CURRENT_BINARY_DIR}/zengine-names/zengine-graphics-vocabulary/graphics/themes.hpp
    PARENT_SCOPE)
set(ZENGINE_GRAPHICS_POOL ${ZENGINE_GRAPHICS_POOL} PARENT_SCOPE)
set(ZENGINE_GRAPHICS_THEMES ${ZENGINE_GRAPHICS_THEMES} PARENT_SCOPE)

if(NOT TARGET loom::kernel)
    message(STATUS "zengine: graphics weaves skipped — this Loom cannot host loadable weaves (no loom::kernel)")
    return()
endif()

# The directory, off the path a picture takes. Its manual is compiled in, and answered to whoever
# asks what a shape it accepts does.
zengine_weave(zengine-graphics directory.cpp MANUAL docs/zengine.graphics.md)
target_link_libraries(zengine-graphics PRIVATE zengine-graphics-vocabulary zengine-activation)

# The terminal window: it reads its own terminal on its own beat, paints with its own painter, and
# opens element code as operator providers are opened (operator/image.hpp), so it needs the dynamic
# loader too. Its manual is compiled in, and answered as the directory's is.
zengine_weave(zengine-window-terminal window/window.cpp MANUAL docs/zengine.window.terminal.md)
target_link_libraries(zengine-window-terminal PRIVATE zengine-graphics-vocabulary
                                                      zengine-input-vocabulary
                                                      zengine-timer-vocabulary
                                                      zengine-activation zengine-operator)

# The OS window, through SDL3: built where this configuration has SDL (cmake/ZengineSdl.cmake), as
# it is by default. Its manual is compiled in, as the terminal window's is, and so is its face: the
# font's bytes become a translation unit in the build tree, nothing installed, staged or discovered
# at runtime (graphics/window/fonts/PROVENANCE.md).
if(ZENGINE_SDL_LIB)
    set(zengine_window_font_unit "${CMAKE_CURRENT_BINARY_DIR}/zengine_window_font.cpp")
    add_custom_command(
        OUTPUT ${zengine_window_font_unit}
        COMMAND ${CMAKE_COMMAND}
                -DZEN_EMBED_INPUT=${CMAKE_CURRENT_SOURCE_DIR}/window/fonts/JetBrainsMono-Regular.ttf
                -DZEN_EMBED_OUTPUT=${zengine_window_font_unit}
                -DZEN_EMBED_NAME=kWindowFontBytes
                -DZEN_EMBED_NAMESPACE=zengine::graphics::window
                -P ${CMAKE_SOURCE_DIR}/cmake/EmbedBinary.cmake
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/window/fonts/JetBrainsMono-Regular.ttf
                ${CMAKE_SOURCE_DIR}/cmake/EmbedBinary.cmake
        COMMENT "zengine-window-sdl: embedding JetBrains Mono into the weave"
        VERBATIM)
    # The face's bytes as objects of their own, so the graphics suite, which builds the backend in
    # process, links the same face the weave does. A quarter of a million initializers of somebody
    # else's data: the warning discipline is about code this repository wrote.
    add_library(zengine-window-font OBJECT ${zengine_window_font_unit})
    set_target_properties(zengine-window-font PROPERTIES POSITION_INDEPENDENT_CODE ON)
    target_compile_options(zengine-window-font PRIVATE -w)
    zengine_weave(zengine-window-sdl window/window_sdl.cpp MANUAL docs/zengine.window.sdl.md)
    target_link_libraries(zengine-window-sdl PRIVATE zengine-graphics-vocabulary
                                                     zengine-input-vocabulary
                                                     zengine-timer-vocabulary
                                                     zengine-activation zengine-operator
                                                     zengine-window-font)
    zengine_sdl_weave(zengine-window-sdl)
    target_link_libraries(zengine-window-sdl PRIVATE ${ZENGINE_SDL_TEXT_LIB})
endif()
