# The activation cursor — how Zengine reads `zen.Activated`, written once.
#
# Header-only and deliberately tiny: a two-field cursor plus the comparison rule
# (lineage + deduplication, NOT authentication — see the header). It travels as
# an interface target like the package vocabularies do, so Timer, Input, Surface
# and Snake all read an activation the same way; consumers spell
# `#include "activation/activation.hpp"`.
#
# Portable — it depends on nothing but the Loom's lifecycle vocabulary and the
# WeaveId type — so it exists even where no kernel does.
add_library(zengine-activation INTERFACE)
target_include_directories(zengine-activation INTERFACE ${CMAKE_SOURCE_DIR})
target_link_libraries(zengine-activation INTERFACE loom::core loom::switchboard)
