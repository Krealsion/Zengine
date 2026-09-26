# The Terminal pane package: the transcript a maker reads and the line they compose messages on,
# as a loadable weave reaching Workshop through the external pane protocol; Workshop compiles
# nothing for it. The participant (`loom::TerminalSession`) stays with the host, which mounts it:
# this package links the pane protocol and the terminal seam's shapes
# (`zengine-workshop-vocabulary`) and nothing that could construct or speak through one.

# The weave needs a Loom that can HOST loadable weaves — the same honest question every
# package here asks.
if(NOT TARGET loom::kernel)
    message(STATUS "zengine: terminal pane weave skipped — this Loom cannot host loadable weaves (no loom::kernel)")
    return()
endif()

zengine_weave(zengine-terminal-pane pane.cpp)
target_link_libraries(zengine-terminal-pane PRIVATE zengine-workshop-vocabulary
                                                    zengine-component zengine-input-vocabulary
                                                    zengine-activation)
