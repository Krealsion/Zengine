# The Introspection package: a view of the Loom it runs in, reaching Workshop as an ordinary
# external pane provider. `zengine-introspection-view` is the header-only pure half -- the
# durable names and the projections (vocabulary, loaded, resolved, powers) -- linking the
# Surface vocabulary (its rows are `SurfaceTextRow`) and the component (the query's TextBox).
# `zengine-introspection` is the weave.

# `workshop/arrangement_vocabulary.hpp` and `workshop/sample_vocabulary.hpp` ride the source
# include path with no link edge: workshop/ is processed after this directory, so an edge here
# would name a target that does not exist yet.
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
# `input::scan`'s constants, so the numbers in `PaneKey` mean something here.
target_link_libraries(zengine-introspection PRIVATE zengine-input-vocabulary)
# For the declarations of `zen.ListLoaded` and the Manager's role only: the weave asks by
# message and holds no `Kernel&` and no reach to the control door.
target_link_libraries(zengine-introspection PRIVATE loom::kernel)
