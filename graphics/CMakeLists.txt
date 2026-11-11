# The Graphics package: the vocabulary an owner draws with (a node, an element record, seven
# operators, a canvas, a colour by name), the windows that show what owners draw -- the TUI
# window and the GUI window -- and the directory that introduces an area to its window and passes it
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

# The pool's names (`zengine::graphics::colors`, graphics/colors.hpp), the terminal's own look and
# the shipped themes as palettes (`zengine::graphics::themes`, graphics/themes.hpp): generated, and
# installed beside the vocabulary with the JSON they come from. A theme is its window file and its
# terminal file beside it.
set(ZENGINE_GRAPHICS_POOL ${CMAKE_CURRENT_SOURCE_DIR}/pool.json)
set(ZENGINE_GRAPHICS_TERMINAL ${CMAKE_CURRENT_SOURCE_DIR}/terminal.json)
set(ZENGINE_GRAPHICS_THEMES ${CMAKE_CURRENT_SOURCE_DIR}/themes/night.json
    ${CMAKE_CURRENT_SOURCE_DIR}/themes/day.json ${CMAKE_CURRENT_SOURCE_DIR}/themes/block.json)
set(ZENGINE_GRAPHICS_THEME_FILES ${ZENGINE_GRAPHICS_THEMES}
    ${CMAKE_CURRENT_SOURCE_DIR}/themes/night.terminal.json
    ${CMAKE_CURRENT_SOURCE_DIR}/themes/day.terminal.json
    ${CMAKE_CURRENT_SOURCE_DIR}/themes/block.terminal.json)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${ZENGINE_GRAPHICS_THEME_FILES})
zengine_names(zengine-graphics-vocabulary JSON ${ZENGINE_GRAPHICS_POOL}
              NAMESPACE zengine::graphics::colors HEADER graphics/colors.hpp)
zengine_palettes(zengine-graphics-vocabulary POOL ${ZENGINE_GRAPHICS_POOL}
                 TERMINAL ${ZENGINE_GRAPHICS_TERMINAL} THEMES ${ZENGINE_GRAPHICS_THEMES}
                 NAMESPACE zengine::graphics::themes HEADER graphics/themes.hpp)
set(ZENGINE_GRAPHICS_GENERATED
    ${CMAKE_CURRENT_BINARY_DIR}/zengine-names/zengine-graphics-vocabulary/graphics/colors.hpp
    ${CMAKE_CURRENT_BINARY_DIR}/zengine-names/zengine-graphics-vocabulary/graphics/themes.hpp
    PARENT_SCOPE)
set_property(GLOBAL APPEND PROPERTY ZENGINE_GENERATED_INCLUDES
             ${CMAKE_CURRENT_BINARY_DIR}/zengine-names/zengine-graphics-vocabulary)
set(ZENGINE_GRAPHICS_POOL ${ZENGINE_GRAPHICS_POOL} PARENT_SCOPE)
set(ZENGINE_GRAPHICS_THEMES ${ZENGINE_GRAPHICS_THEMES} PARENT_SCOPE)
set(ZENGINE_GRAPHICS_TERMINAL ${ZENGINE_GRAPHICS_TERMINAL} PARENT_SCOPE)
set(ZENGINE_GRAPHICS_THEME_FILES ${ZENGINE_GRAPHICS_THEME_FILES} PARENT_SCOPE)

if(NOT TARGET loom::kernel)
    message(STATUS "zengine: graphics weaves skipped — this Loom cannot host loadable weaves (no loom::kernel)")
    return()
endif()

# The directory, off the path a picture takes. Its manual is compiled in, and answered to whoever
# asks what a shape it accepts does.
zengine_weave(zengine-graphics directory.cpp MANUAL docs/zengine.graphics.md)
target_link_libraries(zengine-graphics PRIVATE zengine-graphics-vocabulary zengine-activation)

# The TUI window: it reads its own terminal on its own beat, paints with its own painter, and
# opens element code as operator providers are opened (operator/image.hpp), so it needs the dynamic
# loader too. Its manual is compiled in, and answered as the directory's is.
zengine_weave(zengine-window-tui window/window.cpp MANUAL docs/zengine.window.tui.md)
target_link_libraries(zengine-window-tui PRIVATE zengine-graphics-vocabulary
                                                      zengine-input-vocabulary
                                                      zengine-timer-vocabulary
                                                      zengine-activation zengine-operator)

# The GUI window, through SDL3: built where this configuration has SDL (cmake/ZengineSdl.cmake), as
# it is by default. Its manual is compiled in, as the TUI window's is, and so are its faces:
# each font's bytes become a translation unit in the build tree, nothing installed, staged or
# discovered at runtime (graphics/window/fonts/PROVENANCE.md) -- JetBrains Mono, regular and bold,
# and GNU Unifont and Unifont Upper, the last resort for every character the others lack.
if(ZENGINE_SDL_LIB)
    set(zengine_window_font_units "")
    foreach(face IN ITEMS "JetBrainsMono-Regular.ttf;kWindowFontBytes"
                          "JetBrainsMono-Bold.ttf;kWindowBoldFontBytes"
                          "Unifont-17.0.05.otf;kLastResortFontBytes"
                          "UnifontUpper-17.0.05.otf;kLastResortUpperFontBytes")
        list(GET face 0 file)
        list(GET face 1 name)
        set(unit "${CMAKE_CURRENT_BINARY_DIR}/zengine_window_font_${name}.cpp")
        add_custom_command(
            OUTPUT ${unit}
            COMMAND ${CMAKE_COMMAND}
                    -DZEN_EMBED_INPUT=${CMAKE_CURRENT_SOURCE_DIR}/window/fonts/${file}
                    -DZEN_EMBED_OUTPUT=${unit}
                    -DZEN_EMBED_NAME=${name}
                    -DZEN_EMBED_NAMESPACE=zengine::graphics::window
                    -P ${CMAKE_SOURCE_DIR}/cmake/EmbedBinary.cmake
            DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/window/fonts/${file}
                    ${CMAKE_SOURCE_DIR}/cmake/EmbedBinary.cmake
            COMMENT "zengine-window-sdl: embedding ${file} into the weave"
            VERBATIM)
        list(APPEND zengine_window_font_units ${unit})
    endforeach()
    # The faces' bytes as objects of their own, so the graphics suite, which builds the backend in
    # process, links the same faces the weave does. Millions of initializers of somebody else's
    # data: the warning discipline is about code this repository wrote.
    add_library(zengine-window-font OBJECT ${zengine_window_font_units})
    set_target_properties(zengine-window-font PROPERTIES POSITION_INDEPENDENT_CODE ON)
    target_compile_options(zengine-window-font PRIVATE -w)
    zengine_weave(zengine-window-sdl window/window_sdl.cpp MANUAL docs/zengine.window.gui.md)
    target_link_libraries(zengine-window-sdl PRIVATE zengine-graphics-vocabulary
                                                     zengine-input-vocabulary
                                                     zengine-timer-vocabulary
                                                     zengine-activation zengine-operator
                                                     zengine-window-font)
    zengine_sdl_weave(zengine-window-sdl)
    target_link_libraries(zengine-window-sdl PRIVATE ${ZENGINE_SDL_TEXT_LIB})
endif()
