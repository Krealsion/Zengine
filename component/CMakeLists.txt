# The Component package: reusable pieces of a maker-facing tool that own their own semantic
# state and know nothing about the medium showing them, earned by repeated working behaviour,
# never by a list of widgets (docs/reference/component.md). Not ui/, whose one distinction is
# authored content versus a viewport's observation of it; not workshop/, or the next visual
# application would reinvent them; not the Loom, since nothing here crosses the bus.

# No kernel, no weave, and not even loom::core: a component has no shape, schema or serialized
# form, and the absence of that link is the enforcement of "a component is not content".
# Header-only; consumers spell `#include "component/text_box.hpp"`.
add_library(zengine-component INTERFACE)
target_include_directories(zengine-component INTERFACE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/zengine>)
add_library(zengine::component ALIAS zengine-component)
