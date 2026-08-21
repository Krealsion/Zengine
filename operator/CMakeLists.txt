# The Operator package — a named, typed, composable semantic truth, and the one
# place that evaluates one.
#
# It exists because SEM-0 measured a rule with TWO surfaces. The Timer's delay
# normalization decides what a maker's `delay_ms = -500, repeat = true` actually
# means, and AAF-R0 found that a second surface — anything previewing what a
# Timer would do with a draft — has to know the same thing and had no way to
# learn it. Before that measurement there was one consumer and one three-line
# helper, and lifting it would have renamed a function and deleted nothing.
#
# WHAT AN OPERATOR IS FOR, precisely, so this package does not grow into a logic
# framework: an ordinary internal helper stays an ordinary C++ function until TWO
# OR MORE SURFACES need the same rule and would otherwise each hold a copy. That
# is the line, and `timer.normalize_delay` is the first thing in this repository
# to cross it.
#
# WHY NOT the Loom. The Loom owns Schema and Value truth, loaded-code lifetime,
# and the generic dynamic mechanisms — and this package spends every one of them
# without adding to any. What an operator MEANS, how one composes, and which
# rules exist are Zengine's, exactly as the Timer's vocabulary is: moving them
# down would put an application concept inside the substrate because the
# substrate happens to own the types it is written in.
#
# WHY NOT timer/. A rule the Timer OWNS is a rule only the Timer can share, and
# the whole finding is that it must be shareable without compiling against the
# Timer. `math.max` filed under `timer/` would say the opposite.
#
# Header-only interface target, the pattern ui/, input/, component/ and
# surface/ use: consumers spell `#include "operator/catalog.hpp"`. Unlike
# zengine-component it DOES link loom::core, and the difference is real rather
# than incidental — a component holds a string and two indices and has no wire
# form at all, while an operator's signature IS a pair of loom::Schemas and its
# argument pack goes through loom::admit. The link is the enforcement of "there
# is no second type system here".
#
# Portable: no kernel, no weave, no bus. It exists where no kernel does.
add_library(zengine-operator INTERFACE)
target_include_directories(zengine-operator INTERFACE ${CMAKE_SOURCE_DIR})
target_link_libraries(zengine-operator INTERFACE loom::core)
