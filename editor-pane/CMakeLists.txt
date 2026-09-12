# The Editor pane package — the one source document a maker edits, as a loadable weave rather
# than a built-in panel inside the Workshop host. It arrives through the external pane protocol
# exactly as introspection/, composer/, files/, builder-pane/, attention-pane/, info-pane/ and
# terminal-pane/ do, and Workshop compiles nothing for it.
#
# ⚠ THE DOCUMENT CAME WITH IT, AND THAT IS THIS MIGRATION'S MEASUREMENT. Every earlier pane
# left its subject in the host: the Builder tool, the object document, the terminal
# participant. The Editor's subject IS the source buffer, and a pane that only presented a
# buffer the host still owned would have needed every keystroke to cross the seam twice.
# So `editor.hpp` -- the buffer, the byte law, the tab geometry -- is in this package, the
# weave holds the one open document, and what crosses is a source request, a reveal, a
# quit answer, rows and a caret. The host keeps room, focus and the exit decision.
#
# ONE TARGET, AND NO PLATFORM HALF. Everything this pane does is compose text from a buffer it
# holds and read or write one file through `workshop/persist.hpp`'s atomic writer; the weave
# is one translation unit. It links `zengine-component` for the word and character helpers
# the buffer spends and the owner-held `Clipboard`, `zengine-input-vocabulary` for the key
# numbers, and `zengine-activation` for the attested first breath.

# The weave needs a Loom that can HOST loadable weaves — the same honest question every
# package here asks.
if(NOT TARGET loom::kernel)
    message(STATUS "zengine: editor pane weave skipped — this Loom cannot host loadable weaves (no loom::kernel)")
    return()
endif()

zengine_weave(zengine-editor-pane pane.cpp)
target_link_libraries(zengine-editor-pane PRIVATE zengine-workshop-vocabulary
                                                  zengine-component zengine-input-vocabulary
                                                  zengine-activation)
