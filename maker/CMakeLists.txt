# The Maker package: a Loom weave built from a maker's definition rather than from a class. A
# definition reaches the bus through admission, not a build, so an edit is a swap or a
# succession; `maker::Weave` implements the raw `loom::Weave` contract over a schema decoded at
# runtime. Not a second type system, evaluator or lifecycle: the seven Loom kinds close the maker
# path, triggers are compositions spent through the host's one catalog, and a schema edit is a
# prepared replacement whose conversion is data (docs/reference/maker-weave.md).

# Header-only, in operator/'s shape; it links what its headers use -- loom::core,
# loom::switchboard and zengine-operator -- and needs no loom::kernel, since a definition is
# registered natively. The verbs carry their terminating conditions, and nothing here pumps the
# bus. Exported as zengine::maker for independent hosts and Flow consumers.
add_library(zengine-maker INTERFACE)
target_include_directories(zengine-maker INTERFACE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/zengine>)
target_link_libraries(zengine-maker INTERFACE loom::core)
target_link_libraries(zengine-maker INTERFACE loom::switchboard)
target_link_libraries(zengine-maker INTERFACE zengine-operator)

add_library(zengine::maker ALIAS zengine-maker)
