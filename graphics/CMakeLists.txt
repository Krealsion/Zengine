# The Graphics package: the vocabulary an owner draws with (a node, an element record, seven
# operators, a canvas), the terminal window that shows what owners draw, the directory that
# introduces an area to its window, and two skins as painter code a window opens.

# The vocabulary and its helpers travel as a header-only interface target, as Surface's do:
# consumers spell `#include "graphics/vocabulary.hpp"`. The seams' C headers are among them.
add_library(zengine-graphics-vocabulary INTERFACE)
target_include_directories(zengine-graphics-vocabulary INTERFACE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>)
add_library(zengine::graphics ALIAS zengine-graphics-vocabulary)
target_link_libraries(zengine-graphics-vocabulary INTERFACE loom::core zengine-surface-vocabulary)

if(NOT TARGET loom::kernel)
    message(STATUS "zengine: graphics weaves skipped — this Loom cannot host loadable weaves (no loom::kernel)")
    return()
endif()

# The directory, off the path a picture takes.
zengine_weave(zengine-graphics directory.cpp)
target_link_libraries(zengine-graphics PRIVATE zengine-graphics-vocabulary zengine-activation)

# The terminal window: it reads its own terminal on its own beat and opens skins and element code
# as operator providers are opened (operator/image.hpp), so it needs the dynamic loader too.
zengine_weave(zengine-window-terminal window/window.cpp)
target_link_libraries(zengine-window-terminal PRIVATE zengine-graphics-vocabulary
                                                      zengine-input-vocabulary
                                                      zengine-timer-vocabulary
                                                      zengine-activation zengine-operator)

# The skins: painter code, not weaves -- no participant lives in either image, and a window opens
# the file itself and resolves one symbol.
zengine_provider(zengine-paint-classic paint_classic.cpp)
target_link_libraries(zengine-paint-classic PRIVATE zengine-graphics-vocabulary)
zengine_provider(zengine-paint-block paint_block.cpp)
target_link_libraries(zengine-paint-block PRIVATE zengine-graphics-vocabulary)
