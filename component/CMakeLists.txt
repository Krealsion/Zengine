# The Component package — reusable pieces of a maker-facing tool, owning their own
# semantic state and knowing nothing about the medium that shows them.
#
# It exists because HD-5 measured two working Workshop tools needing the same editing
# machinery: the Terminal's command line and an Inspector property draft. Before that
# measurement there was one implementation and one consumer, and extracting it would have
# renamed a class and deleted nothing — which is exactly why HD-4 traced the second consumer
# and declined. A component here is EARNED by repeated working behaviour, never by a list of
# widgets a GUI toolkit is expected to contain.
#
# WHY NOT ui/. That package owns exactly ONE distinction — what a maker AUTHORED versus what a
# viewport MAKES of it — with a compile-time fence and two compile-negative tests holding the
# halves apart. A TextBox is neither: it is not authored document content (it has no wire
# form, nothing serializes it, it is not poke-inspectable) and it is not a resolved
# observation of content. Filing it there would put a third category inside a package whose
# whole value is that it has exactly two, kept apart.
#
# WHY NOT workshop/. A component that lived in Workshop would belong to Workshop, and the
# next visual application would reinvent it — the same argument that created ui/ in W-1.
#
# WHY NOT the Loom. Nothing here crosses the bus, is hosted, or is loaded. `loom::Widget` is
# the OTHER model (intent plus relationship, resolved by a renderer, for an operator console)
# and W-1 already measured that it is neither relocatable nor able to express what this side
# needs.
#
# NO KERNEL, NO WEAVE, AND — uniquely among this repository's packages — NOT EVEN loom::core.
# The component holds a std::string and two indices. It has no shape, no schema and no
# serialized form, so it needs nothing to declare one with, and the absence of that link is
# the enforcement of "a component is not content".
#
# Header-only interface target, the pattern ui/, input/, surface/ and workshop/ use:
# consumers spell `#include "component/text_box.hpp"`, and the suite includes exactly what
# ships.
add_library(zengine-component INTERFACE)
target_include_directories(zengine-component INTERFACE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/zengine>)
add_library(zengine::component ALIAS zengine-component)
