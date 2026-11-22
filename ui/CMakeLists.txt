# The UI package (ui/README.md): authored intent in, whole-pixel geometry out, and the answer to
# "which authored part is under this pixel". It owns one distinction: what a weaver authored
# (vocabulary.hpp, which holds no solved number) and what solving it in given bounds makes of it
# (layout.hpp). It is not Loom's `loom::Widget` and px_layout moved here: those are a different
# model, a relational stack layout for a weaver's console, which leaves out the authored placement
# this package needs.

# It has no kernel dependency and no weave: a vocabulary and pure functions, present in every
# configuration. It links loom::core for zen/weave/shape.hpp alone: the authored side is ordinary
# Zen content, and the solved side deliberately is not (layout.hpp's wire-form assertion). It is
# header-only; a program writes `#include "ui/vocabulary.hpp"` and `#include "ui/layout.hpp"`.
add_library(zengine-ui-vocabulary INTERFACE)
target_include_directories(zengine-ui-vocabulary INTERFACE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/zengine>)
add_library(zengine::ui ALIAS zengine-ui-vocabulary)
target_link_libraries(zengine-ui-vocabulary INTERFACE loom::core)
