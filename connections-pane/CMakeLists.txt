# The Connections pane package -- the other hosts connected to this Workshop, as a loadable weave.
# It arrives through the external pane protocol exactly as the Attention pane does, and Workshop
# compiles nothing for it.
#
# ONE TARGET AND NO PLATFORM HALF, for the Attention pane's reason: everything this weave does is
# compose text from a reading the guest door said. It links no socket, no bridge and no server --
# the door holds those, in the host -- so a pane that shows connections cannot make one.
#
# IT LINKS NO loom::kernel, like every pane weave: it never touches the control door or the
# Manager and could not.

if(NOT TARGET loom::kernel)
    message(STATUS "zengine: connections pane weave skipped — this Loom cannot host loadable weaves (no loom::kernel)")
    return()
endif()

zengine_weave(zengine-connections-pane pane.cpp)
target_link_libraries(zengine-connections-pane PRIVATE zengine-workshop-vocabulary
                                                       zengine-activation)
