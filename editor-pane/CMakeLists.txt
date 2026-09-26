# The Editor pane package: the one source document a maker edits, as a loadable weave reaching
# Workshop through the external pane protocol; Workshop compiles nothing for it. The document
# lives here (`editor.hpp`: the buffer, the byte law, the tab geometry), so a keystroke never
# crosses the seam; what crosses is a source request, a preparation, a quit answer, rows and a
# caret. One target, no platform half: the weave composes text from the buffer it holds and
# reads or writes one file through `workshop/persist.hpp`'s atomic writer.

# The weave needs a Loom that can HOST loadable weaves — the same honest question every
# package here asks.
if(NOT TARGET loom::kernel)
    message(STATUS "zengine: editor pane weave skipped — this Loom cannot host loadable weaves (no loom::kernel)")
    return()
endif()

zengine_weave(zengine-editor-pane pane.cpp)
target_link_libraries(zengine-editor-pane PRIVATE zengine-workshop-vocabulary
                                                  zengine-component zengine-input-vocabulary
                                                  zengine-activation zengine-source-transfer)
