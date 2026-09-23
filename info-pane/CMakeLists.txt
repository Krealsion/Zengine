# The Info pane package — the PANES list and one pane's PROPERTIES, as a loadable weave rather
# than C++ inside the Workshop host. It arrives through the external pane protocol exactly as
# introspection/, composer/, files/, builder-pane/ and attention-pane/ do, and Workshop compiles
# nothing for it.
#
# ⚠ WHAT IT INSPECTS IS NOT IN THIS PACKAGE, AND THAT IS THE MIGRATION'S DECISION. The desk and
# the rows over it stay in `workshop/`, with the host that owns them: this package links
# `zengine-workshop-vocabulary` for the pane protocol and the inspection seam's shapes, and
# nothing that could read or write a desk. What it can do to a pane's properties is ask. (It
# inspected the prototype object document the same way until that document retired.)
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
                                                zengine-activation zengine::inventory zengine::message-draft)
