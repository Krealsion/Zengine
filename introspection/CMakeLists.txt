# The Introspection package — the first Zengine tool whose whole value is a view
# of the Loom it is running in, and the first one that reaches Workshop as an
# ordinary external pane provider rather than as a panel Workshop was taught.
#
# Two header-only pieces and one loadable weave:
#
#   zengine-introspection-view   the durable pane identity and the pure projection
#                                (vocabulary.hpp, loaded.hpp). Portable — it links
#                                nothing and knows no bus, so the suite can ask it
#                                what a reading MEANS without loading anything.
#   zengine-introspection        the weave: when to observe, whom to believe, and
#                                the two sentences of the pane protocol.

# The view travels as a header-only interface target, the same shape timer's and
# input's vocabularies use. It needs the Surface vocabulary because its output IS
# `surface::SurfaceTextRow` — the pane protocol reuses that type rather than
# mirroring it, so a provider's row carries the same semantic role every
# first-party row does.
add_library(zengine-introspection-view INTERFACE)
target_include_directories(zengine-introspection-view INTERFACE ${CMAKE_SOURCE_DIR})
target_link_libraries(zengine-introspection-view INTERFACE zengine-surface-vocabulary)

# The weave itself needs a Loom that can HOST loadable weaves — the same honest
# question every package here asks: does this install export loom::kernel?
if(NOT TARGET loom::kernel)
    message(STATUS "zengine: introspection weave skipped — this Loom cannot host loadable weaves (no loom::kernel)")
    return()
endif()

zengine_weave(zengine-introspection introspection.cpp)
target_link_libraries(zengine-introspection PRIVATE zengine-introspection-view zengine-activation)
# `zen.ListLoaded` and the Weave Manager's role name live in loom::kernel's
# headers. This links the target for its DECLARATIONS only: the weave asks the
# Manager a question by message, exactly as snake's operator does, and holds no
# `Kernel&`, no `LifecycleAuthority` and no reach to the control door.
target_link_libraries(zengine-introspection PRIVATE loom::kernel)
