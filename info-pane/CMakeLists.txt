# The Info pane package — the OBJECTS list and the PROPERTIES inspector, as a loadable weave
# rather than C++ inside the Workshop host. It arrives through the external pane protocol
# exactly as introspection/, composer/, files/, builder-pane/ and attention-pane/ do, and
# Workshop compiles nothing for it.
#
# ⚠ THE DOCUMENT IS NOT IN THIS PACKAGE AND THAT IS THE MIGRATION'S DECISION. `document.hpp`,
# `property.hpp` and `persist.hpp` stay in `workshop/`, with the host that owns them: this
# package links `zengine-workshop-vocabulary` for the pane protocol and the document SEAM's
# shapes, and nothing that could construct, read or write a `WorkshopDoc`. What it can do to a
# document is ask.
#
# ONE TARGET, AND NO PLATFORM HALF. Everything this pane does is compose text from values it
# was told; the weave is one translation unit. It links `zengine-component` for the editable
# line a property draft is typed into — the same one the Builder's role line and the browser's
# authoring field spend.

# The weave needs a Loom that can HOST loadable weaves — the same honest question every
# package here asks.
if(NOT TARGET loom::kernel)
    message(STATUS "zengine: info pane weave skipped — this Loom cannot host loadable weaves (no loom::kernel)")
    return()
endif()

zengine_weave(zengine-info-pane pane.cpp)
target_link_libraries(zengine-info-pane PRIVATE zengine-workshop-vocabulary
                                                zengine-component zengine-input-vocabulary
                                                zengine-activation)
