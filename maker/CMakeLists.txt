# The Maker package -- a Loom weave built from a maker's DEFINITION rather than from a class.
#
# WHAT IT IS FOR. Every weave in this repository so far is C++: a ZEN_SHAPE state, a class
# with handlers, a build. A maker at the Workshop's editor has no compiler in the loop, so the
# thing they make must be data -- a definition (the state schema, the accepted and emitted
# shapes, the triggers as compositions over the host's operator catalog) and a state (their own
# value at their own schema) -- and something must register one Loom weave per definition and
# run it. This package is that interpreter: `maker::Weave` implements the raw `loom::Weave`
# contract over a schema decoded at runtime, with `loom::construct_blind` and the schema codec
# doing what a static struct cannot.
#
# WHAT IT DELIBERATELY IS NOT. Not a second type system (the seven Loom kinds close the maker
# path; a keyed table is a list of entries, a one-of is several optional fields), not a second
# evaluator (a trigger's body is a composition spent through the host's ONE catalog, at spend),
# not a second lifecycle (a behaviour edit is `swap_state`; a schema edit is a prepared
# replacement with the conversion authored as data in the successor and spent as a MIG-0
# operator by the coordinator). Nothing in the Loom changed for it.
#
# THE NAME. `maker` is the word this tree already uses for a thing a maker made from data
# (`agents/workshop/maker-pane.md`). The verbs carry their terminating conditions:
# `register_definition` returns after registration, `apply_behaviour_edit` after the swap,
# `begin_schema_edit` after scheduling -- nothing in this package pumps the bus.
#
# Header-only INTERFACE target in operator/'s shape; consumers spell `#include "maker/weave.hpp"`.
# It links what its headers use, on its own lines: loom::core (Schema, Value, the codec),
# loom::switchboard (the weave contract, the bus, the prepared-replacement handle) and
# zengine-operator (the catalog, the composition wire form, the migration convention). It needs
# no loom::kernel: a definition is registered natively, never loaded, and the coordinator's
# handle is the kernel-less constructor. Not exported this phase -- the Workshop consumes it
# in-tree in the panel phase, which decides export.
add_library(zengine-maker INTERFACE)
target_include_directories(zengine-maker INTERFACE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>)
target_link_libraries(zengine-maker INTERFACE loom::core)
target_link_libraries(zengine-maker INTERFACE loom::switchboard)
target_link_libraries(zengine-maker INTERFACE zengine-operator)
