# The Info pane package: the panes list and one pane's properties, plus typed value views, as a
# loadable weave reaching Workshop through the external pane protocol; Workshop compiles nothing
# for it. The desk it inspects stays in `workshop/` with the host that owns it: this package
# links the pane protocol and the inspection seam's shapes (`zengine-workshop-vocabulary`) and
# nothing that could read or write a desk, so what it can do to a pane's properties is ask.

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
