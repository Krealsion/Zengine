# The Connections pane package: the other hosts connected to this Workshop, as a loadable weave
# reaching Workshop through the external pane protocol; Workshop compiles nothing for it. One
# target, composing text from a reading the guest door said: it links no socket, bridge or
# server (the door holds those, in the host), and no loom::kernel, so a pane that shows
# connections cannot make one.

if(NOT TARGET loom::kernel)
    message(STATUS "zengine: connections pane weave skipped — this Loom cannot host loadable weaves (no loom::kernel)")
    return()
endif()

zengine_weave(zengine-connections-pane pane.cpp)
target_link_libraries(zengine-connections-pane PRIVATE zengine-workshop-vocabulary
                                                       zengine-activation)
