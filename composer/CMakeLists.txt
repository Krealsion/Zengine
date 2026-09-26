# The Composer package: the tool in which a maker writes a message, from a target's own
# accepted vocabulary, to that target's own office. `zengine-composer-vocabulary` is the durable
# pane identity and links nothing; `zengine-composer-draft` is the draft model and projection
# (draft.hpp, view.hpp), with no bus, so the suite can ask what a draft composes to and what a
# maker would see; `zengine-composer` is the weave.

# It links loom::terminal, the first loadable weave in either repository to do so: that target
# carries the typed composer (`describe_schema`, `compose_message`, `assemble`) MSG-1 built for
# a Workshop pane. Loom's reloadable-weave contract sweep walks Loom's build graph, not Zengine's,
# so it does not see this link.

# The durable names link nothing at all: a host's boot list needs the stem and the office and
# must not thereby acquire the typed composer, the Schema vocabulary or a text editor.
add_library(zengine-composer-vocabulary INTERFACE)
target_include_directories(zengine-composer-vocabulary INTERFACE ${CMAKE_SOURCE_DIR})

add_library(zengine-composer-draft INTERFACE)
target_include_directories(zengine-composer-draft INTERFACE ${CMAKE_SOURCE_DIR})
target_link_libraries(zengine-composer-draft INTERFACE zengine-composer-vocabulary
                                                       zengine-surface-vocabulary
                                                       zengine-component
                                                       zengine::message-draft
                                                       loom::core
                                                       loom::terminal)

# The weave itself needs a Loom that can HOST loadable weaves — the same honest
# question every package here asks: does this install export loom::kernel?
if(NOT TARGET loom::kernel)
    message(STATUS "zengine: composer weave skipped — this Loom cannot host loadable weaves (no loom::kernel)")
    return()
endif()

zengine_weave(zengine-composer composer.cpp)
target_link_libraries(zengine-composer PRIVATE zengine-composer-draft zengine-activation)
# The two vocabularies it SPEAKS someone else's sentences in: Workshop's pane
# protocol (which brings the surface rows with it) and Introspection's one fact.
# It names `input::scan`'s constants directly, which is what makes the numbers in
# `PaneKey` mean something here rather than being compared against literals.
target_link_libraries(zengine-composer PRIVATE zengine-workshop-vocabulary
                                               zengine-introspection-view
                                               zengine-input-vocabulary)
