# The UI package — authored intent in, resolved geometry out, and the answer to
# "which authored object is under this cell".
#
# It owns ONE distinction and nothing else: what a maker authored versus what a
# viewport makes of it. Two headers keep the halves apart (vocabulary.hpp is the
# authored side and contains no resolved number; layout.hpp is the observation).
#
# W-1 created it because W-0's Workshop was keeping that answer privately, and a
# second visual application would have had to invent its own. What did NOT create
# it is a relocation of the Loom's loom::Widget / px_layout: that is a different
# model (relational stack layout for an operator console, with a compile-time
# fence AGAINST the authored placement this package needs), it has four live Loom
# consumers that cannot follow it here, and W-1-RB records the measurement.
#
# NO KERNEL DEPENDENCY, and no weave. This is vocabulary and pure functions —
# nothing here sends, receives, or loads anything, so it exists on every
# configuration, including one whose Loom cannot host loadable weaves. That is
# also why there is no `if(NOT TARGET loom::kernel) return()` below: there is
# nothing here for a kernel to host.
#
# It links loom::core for zen/weave/shape.hpp alone: the authored side is
# ordinary Zen content (gated, schema-carrying, serializable, poke-inspectable),
# exactly like every other package's vocabulary. The resolved side deliberately
# is not — see the wire-form assertion in layout.hpp.
#
# Header-only interface target, the pattern input/, surface/ and workshop/ use:
# consumers spell `#include "ui/vocabulary.hpp"` / `"ui/layout.hpp"`, and the
# suite includes exactly what ships.
add_library(zengine-ui-vocabulary INTERFACE)
target_include_directories(zengine-ui-vocabulary INTERFACE ${CMAKE_SOURCE_DIR})
target_link_libraries(zengine-ui-vocabulary INTERFACE loom::core)
