# The Composer package — the tool in which a maker WRITES a message, from a
# target's own accepted vocabulary, to that target's own office.
#
# Two header-only pieces and one loadable weave, the shape introspection/ already
# uses:
#
#   zengine-composer-vocabulary  the durable pane identity, and nothing else. It
#                            links NOTHING -- the absence is the enforcement.
#   zengine-composer-draft   the draft model and the projection (draft.hpp,
#                            view.hpp). It knows no bus, so the suite can ask it
#                            what a DRAFT composes to and what a maker would SEE
#                            without loading anything.
#   zengine-composer         the weave: whom to believe, when to ask, and what a
#                            key means.
#
# IT LINKS loom::terminal, AND IT IS THE FIRST LOADABLE WEAVE IN EITHER REPOSITORY
# TO DO SO. That target carries the typed composer — `describe_schema`,
# `compose_message`, `assemble` — which MSG-1 built precisely so that "a future
# Workshop pane" could show a message to a person before sending it. Loom's own
# CMakeLists predicted this consumer by name and left a note where the contract
# would be needed:
#
#     "DELIBERATELY NOT loom_weave_build_contract() ... nothing loadable links the
#      terminal core today. When a loadable weave first links this target, the
#      sweep will derive the requirement from the build graph and say so."
#
# The sweep in question walks LOOM's build graph, and this weave is in Zengine's —
# so it will not fire, and the cost is measured here instead. See
# `Zen/reportbacks/MSG-0-RB.md` for the measurement and what it means for reloading
# this particular weave. Nothing about it is hidden behind a flag.
# The two durable names travel on their own and LINK NOTHING AT ALL, which is a
# narrower offer than introspection's view makes. A host's boot list needs the stem
# and the office and must not thereby acquire the typed composer, the Schema
# vocabulary or a text editor -- and the absence of those links is what says so.
add_library(zengine-composer-vocabulary INTERFACE)
target_include_directories(zengine-composer-vocabulary INTERFACE ${CMAKE_SOURCE_DIR})

add_library(zengine-composer-draft INTERFACE)
target_include_directories(zengine-composer-draft INTERFACE ${CMAKE_SOURCE_DIR})
target_link_libraries(zengine-composer-draft INTERFACE zengine-composer-vocabulary
                                                       zengine-surface-vocabulary
                                                       zengine-component
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
