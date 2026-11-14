# The realization owner (realization/README.md): the typed load plan and its law, the plan's file
# format, the owner that performs it, and what it claims. Header-only, and a participant both hosts
# mount: Workshop and the app host each put one on their bus (`realization::mount`). It links no
# kernel -- the host hands in the bus and the Weave Manager's WeaveId -- so it is portable.
add_library(zengine-realization INTERFACE)
target_include_directories(zengine-realization INTERFACE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/zengine>)
# The builder's words are the owner's account (`RealizationAsked`, `ArtifactRealized`); their
# header installs beside it, and their target stays in this tree.
target_link_libraries(zengine-realization INTERFACE zengine::operator zengine::persist
                                                    zengine::activation zengine::manual
                                                    zengine::graphics
                                                    $<BUILD_INTERFACE:zengine-builder-vocabulary>
                                                    loom::core loom::switchboard)
# ...and the owner's manual, compiled into whatever mounts it: it answers a shape's section from
# this page (`ShapeDocumentRequested`, manual/vocabulary.hpp).
zengine_manual(zengine-realization INTERFACE docs/zengine.realization.md)
add_library(zengine::realization ALIAS zengine-realization)
