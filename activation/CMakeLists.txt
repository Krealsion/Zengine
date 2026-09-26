# The activation cursor: how Zengine reads `zen.Activated`, written once. Header-only and tiny --
# a two-field cursor and the comparison rule (lineage and deduplication, not authentication; see
# the header) -- so every package reads an activation the same way
# (`#include "activation/activation.hpp"`). It depends only on the Loom's lifecycle vocabulary
# and the WeaveId type, so it exists even where no kernel does.
add_library(zengine-activation INTERFACE)
target_include_directories(zengine-activation INTERFACE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/zengine>)
add_library(zengine::activation ALIAS zengine-activation)
target_link_libraries(zengine-activation INTERFACE loom::core loom::switchboard)
