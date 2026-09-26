# The UI package: authored intent in, resolved geometry out, and the answer to "which authored
# object is under this cell". It owns one distinction: what a maker authored (vocabulary.hpp,
# which holds no resolved number) versus what a viewport makes of it (layout.hpp). Not a
# relocation of the Loom's `loom::Widget` / px_layout, a different model (relational stack
# layout for an operator console) that fences out the authored placement this package needs.
# Reference: docs/reference/ui.md.

# No kernel dependency and no weave: vocabulary and pure functions, present in every
# configuration. It links loom::core for zen/weave/shape.hpp alone -- the authored side is
# ordinary Zen content; the resolved side deliberately is not (layout.hpp's wire-form
# assertion). Header-only; consumers spell `#include "ui/vocabulary.hpp"` / `"ui/layout.hpp"`.
add_library(zengine-ui-vocabulary INTERFACE)
target_include_directories(zengine-ui-vocabulary INTERFACE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/zengine>)
add_library(zengine::ui ALIAS zengine-ui-vocabulary)
target_link_libraries(zengine-ui-vocabulary INTERFACE loom::core)
