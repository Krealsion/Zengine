# The activation cursor, which decides whether a weave acts on an arriving `zen.Activated`
# (`#include "activation/activation.hpp"`). It is header-only over Loom's lifecycle vocabulary and
# its WeaveId type, so it exists even where no kernel does. Its front page is activation/README.md.
add_library(zengine-activation INTERFACE)
target_include_directories(zengine-activation INTERFACE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/zengine>)
add_library(zengine::activation ALIAS zengine-activation)
target_link_libraries(zengine-activation INTERFACE loom::core loom::switchboard)
