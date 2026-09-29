# The activation cursor: whether a weave acts on an arriving `zen.Activated`
# (`#include "activation/activation.hpp"`). Header-only over the Loom's lifecycle vocabulary and
# the WeaveId type, so it exists even where no kernel does. Reference: docs/reference/activation.md.
add_library(zengine-activation INTERFACE)
target_include_directories(zengine-activation INTERFACE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/zengine>)
add_library(zengine::activation ALIAS zengine-activation)
target_link_libraries(zengine-activation INTERFACE loom::core loom::switchboard)
