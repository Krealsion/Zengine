# The Builder pane package: the weaver's seat at the build, as a loadable weave reaching Workshop
# through the external pane protocol; Workshop compiles nothing for it. One target, no platform
# half: it composes text from values it was told. It is a folder of its own inside `builder/`,
# whose tool package is header-only and needs no kernel: the `return()` below ends this file
# alone, so a Loom that cannot host weaves still exports the vocabulary Workshop compiles against.

# It links no loom::kernel: it asks offices (the tool, the project, the plan, the opening
# office) and hears answers, and never touches the control door or the Manager.

# The weave needs a Loom that can HOST loadable weaves — the same honest question every
# package here asks.
if(NOT TARGET loom::kernel)
    message(STATUS "zengine: builder pane weave skipped — this Loom cannot host loadable weaves (no loom::kernel)")
    return()
endif()

# Its manual is compiled in, and answered to whoever asks for it.
zengine_weave(zengine-builder-pane pane.cpp MANUAL ../docs/zengine.builder-pane.md)
target_link_libraries(zengine-builder-pane PRIVATE zengine-workshop-vocabulary
                                                   zengine-builder-vocabulary
                                                   zengine-component zengine-input-vocabulary
                                                   zengine-activation)
