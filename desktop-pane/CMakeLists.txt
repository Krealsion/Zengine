# The Desktop package — the application's own default behaviour, as a loadable weave rather
# than four things compiled into the Workshop host. It arrives through the external pane
# protocol exactly as the other pane weaves do, and Workshop compiles nothing for it.
#
# ONE TARGET AND NO PLATFORM HALF, for the Attention pane's reason: everything this weave does
# is compose text from values it was told and ask the host for operations the host owns.
#
# WHAT IT NEEDS is what the thinnest pane needs, plus nothing: the weave-authoring surface
# (loom::core + loom::switchboard, through zengine_weave), the pane protocol and the desktop
# seam (workshop-vocabulary), the key numbers (input-vocabulary) and the activation cursor.
#
# IT LINKS NO loom::kernel. It never touches the control door or the Manager and could not --
# nothing it can say reaches either, which is the point of it being a participant rather than
# the host.

# The weave needs a Loom that can HOST loadable weaves — the same honest question every
# package here asks.
if(NOT TARGET loom::kernel)
    message(STATUS "zengine: desktop weave skipped — this Loom cannot host loadable weaves (no loom::kernel)")
    return()
endif()

zengine_weave(zengine-desktop-pane pane.cpp)
target_link_libraries(zengine-desktop-pane PRIVATE zengine-workshop-vocabulary
                                                   zengine-input-vocabulary
                                                   zengine-activation
                                                   zengine-component)
