# The Files package: the project browser, its marks and the two files it writes, as a loadable
# weave reaching Workshop through the external pane protocol; Workshop compiles nothing for it.
# `zengine-files-os` is a small static library of the two platform bodies the pure half needs
# (`leaves_the_tree`, `host_filesystem_roots`), so the weave and the suite share one
# compilation of them; everything else is header-only.

# The weave links no loom::kernel: it asks offices (the project, the recipes, the opening
# office) and hears answers, and never touches the control door or the Manager.

add_library(zengine-files-os STATIC os.cpp)
set_target_properties(zengine-files-os PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_link_libraries(zengine-files-os PUBLIC zengine-workshop-vocabulary
                                       PRIVATE zengine-warnings zengine-sanitize)

# The weave needs a Loom that can HOST loadable weaves — the same honest question every
# package here asks.
if(NOT TARGET loom::kernel)
    message(STATUS "zengine: files weave skipped — this Loom cannot host loadable weaves (no loom::kernel)")
    return()
endif()

zengine_weave(zengine-files files.cpp)
target_link_libraries(zengine-files PRIVATE zengine-files-os zengine-workshop-vocabulary
                                            zengine-component zengine-input-vocabulary
                                            zengine-activation)
