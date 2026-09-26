# The Attention pane package: what is true right now, as a loadable weave reaching Workshop
# through the external pane protocol; Workshop compiles nothing for it. One target, no platform
# half, and the thinnest of the pane weaves: no builder vocabulary, component or filesystem
# half, and no office's ask -- it hears one publication and says rows. It links no loom::kernel:
# nothing it can say reaches the control door or the Manager.

# The weave needs a Loom that can HOST loadable weaves — the same honest question every
# package here asks.
if(NOT TARGET loom::kernel)
    message(STATUS "zengine: attention pane weave skipped — this Loom cannot host loadable weaves (no loom::kernel)")
    return()
endif()

zengine_weave(zengine-attention-pane pane.cpp)
target_link_libraries(zengine-attention-pane PRIVATE zengine-workshop-vocabulary
                                                     zengine-input-vocabulary
                                                     zengine-activation)
