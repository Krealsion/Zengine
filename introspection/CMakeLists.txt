# The Introspection package — the first Zengine tool whose whole value is a view
# of the Loom it is running in, and the first one that reaches Workshop as an
# ordinary external pane provider rather than as a panel Workshop was taught.
#
# Two header-only pieces and one loadable weave:
#
#   zengine-introspection-view   the durable pane identities and the pure projections
#                                (vocabulary.hpp, loaded.hpp, resolved.hpp). Portable —
#                                it links nothing and knows no bus, so the suite can ask
#                                it what a reading MEANS without loading anything.
#   zengine-introspection        the weave: when to observe, whom to believe, and
#                                the two sentences of the pane protocol.
#
# ---- INTR-1 ADDED A THIRD HEADER AND NO TARGET, WHICH IS THE HONEST SHAPE --------
#
# `resolved.hpp` projects `workshop/arrangement_vocabulary.hpp` — the two questions this
# tool asks the HOST and the two answers it gets back. That header is included the same
# way `workshop/pane_vocabulary.hpp` already is: by the `${CMAKE_SOURCE_DIR}` include
# path this target carries, with no new link edge, because it needs nothing this package
# does not already have. It is `<zen/weave/shape.hpp>` and three standard headers; the
# Surface vocabulary below already brings loom::core.
#
# A TARGET THAT ADDS NOTHING WOULD BE CEREMONY, and the alternative is worse than that:
# the vocabulary lives in `workshop/`, which CMake processes AFTER this directory, so a
# link edge here would name a target that does not exist yet. The dependency is real and
# is stated in prose rather than in a line that buys nothing.

# ---- SOURCE-1 ADDED A FOURTH HEADER AND ONE LINK EDGE ---------------------------
#
# `powers.hpp` is the Powers pane as a maker uses it — the two derived views, the
# search, the composite filter, the identity-held cursor, the row/control map and the
# retained sample — and it is still PURE: it knows no bus, so the suite can ask it
# what a maker would SEE and what a press would MEAN without loading anything.
#
# It holds a `component::TextBox`, which is the one new edge. That is the COMPOSER'S
# OWN edge, already shipped: the query field, its caret window, its selection, its
# undo and its clipboard vocabulary are HD-5's component, and re-spelling any of it
# here would be a fifth copy of the editing switch TEXT-0 collapsed. The component
# links nothing at all — not even loom::core — so the edge costs this package a
# header path and no dependency.
#
# `workshop/sample_vocabulary.hpp` rides the `${CMAKE_SOURCE_DIR}` include path the
# arrangement vocabulary already does, for the same reason and with the same absent
# link line: `workshop/` is processed AFTER this directory, so a link edge here would
# name a target that does not exist yet, and the dependency is real and is stated in
# prose rather than in a line that buys nothing.

# The view travels as a header-only interface target, the same shape timer's and
# input's vocabularies use. It needs the Surface vocabulary because its output IS
# `surface::SurfaceTextRow` — the pane protocol reuses that type rather than
# mirroring it, so a provider's row carries the same semantic role every
# first-party row does.
add_library(zengine-introspection-view INTERFACE)
target_include_directories(zengine-introspection-view INTERFACE ${CMAKE_SOURCE_DIR})
target_link_libraries(zengine-introspection-view INTERFACE zengine-surface-vocabulary
                                                           zengine-component)

# The weave itself needs a Loom that can HOST loadable weaves — the same honest
# question every package here asks: does this install export loom::kernel?
if(NOT TARGET loom::kernel)
    message(STATUS "zengine: introspection weave skipped — this Loom cannot host loadable weaves (no loom::kernel)")
    return()
endif()

zengine_weave(zengine-introspection introspection.cpp)
target_link_libraries(zengine-introspection PRIVATE zengine-introspection-view zengine-activation)
# It names `input::scan`'s constants directly, which is what makes the numbers in
# `PaneKey` mean something here rather than being compared against literals — the
# Composer's own edge, for the Composer's own reason.
target_link_libraries(zengine-introspection PRIVATE zengine-input-vocabulary)
# `zen.ListLoaded` and the Weave Manager's role name live in loom::kernel's
# headers. This links the target for its DECLARATIONS only: the weave asks the
# Manager a question by message, exactly as snake's operator does, and holds no
# `Kernel&`, no `LifecycleAuthority` and no reach to the control door.
target_link_libraries(zengine-introspection PRIVATE loom::kernel)
