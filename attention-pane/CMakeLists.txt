# The Attention pane package — what is true right now, as a loadable weave rather than chrome
# inside the Workshop host. It arrives through the external pane protocol exactly as
# introspection/, composer/, files/ and builder-pane/ do, and Workshop compiles nothing for it.
#
# ONE TARGET, AND NO PLATFORM HALF. Everything this pane does is compose text from values it
# was told, so the weave is one translation unit and there is nothing for a suite to share a
# compilation of. The Builder pane's own CMakeLists says the same thing for the same reason.
#
# IT IS THE THINNEST OF THE THREE, AND THE SHAPE OF THAT IS THE POINT: it links no builder
# vocabulary, no component, no filesystem half, and it holds no office's ask. It hears one
# publication and says rows. What it needs is the weave-authoring surface (loom::core +
# loom::switchboard, through zengine_weave), the pane protocol and the conditions shape
# (workshop-vocabulary), the key numbers (input-vocabulary) and the activation cursor.
#
# IT LINKS NO loom::kernel, like the Files and Builder weaves and unlike introspection: it
# never touches the control door or the Manager, and could not — nothing it can say reaches
# either.

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
