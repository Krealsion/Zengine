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

# ---- The consumer half of the operator-host seam (OPH-0) --------------------
#
# WHY IT IS ITS OWN TARGET, and the shape is the claim. SEM-0's stranger could
# only be shown independent by grep, because it received `const op::Catalog&` and
# the package it linked contained the Catalog. This one links a package that DOES
# NOT CONTAIN ONE: `host_abi.h` is a C table and `host.hpp` is a codec and a
# handle, and neither names `op::Catalog`, `OperatorDef`, `make_operator` or a
# primitive. A dynamically loaded consumer that links this and nothing else has
# no way to reach the host's operator truth except by asking for it -- not
# because a link line forbade it, but because the truth lives in another image
# and this package cannot construct one to stand in for it.
#
# It links loom::core and not loom::kernel: `zen/kernel/abi.h` (for ZenByteSink
# and the export decoration) and `zen/kernel/schema_codec.hpp` (for zen.SchemaDesc)
# are HEADERS, reachable from the exported include path, exactly as every weave
# in this repository reaches `zen/kernel/export.hpp` while linking core and
# switchboard only. The kernel LIBRARY is the host's dependency, never a loaded
# artifact's.
#
# Portable: no kernel, no weave, no bus, no SDL. A consumer of it exists wherever
# a Loom header does.
add_library(zengine-operator-consumer INTERFACE)
target_include_directories(zengine-operator-consumer INTERFACE ${CMAKE_SOURCE_DIR})
target_link_libraries(zengine-operator-consumer INTERFACE loom::core)

# The HOST side of the same seam rides zengine-operator, because dressing a
# catalog in a C table is something only an image that HAS a catalog can do. The
# link is one-directional and says so: the host knows the consumer's descriptor
# codec (they must agree on the bytes, and one definition of them is the whole
# point), and the consumer knows nothing of the host.
target_link_libraries(zengine-operator INTERFACE zengine-operator-consumer)
if(UNIX)
    # `OperatorOffer` resolves the optional symbol from the same image the Kernel
    # opens, and `mount_provider` opens a provider artifact outright; both need the
    # dynamic loader. On glibc 2.34+ dl* lives in libc and this is a no-op; on older
    # ones it is not, and CMake's own answer is the one that knows which.
    target_link_libraries(zengine-operator INTERFACE ${CMAKE_DL_LIBS})
endif()

# ---- The BASIC OPERATOR PROVIDER, and it is not a weave (PROV-0) ------------
#
# WHAT THIS TARGET IS FOR. Before PROV-0 a host manufactured its operator vocabulary by
# calling a package's authoring function, which compiled one artifact's semantics into
# the host and made "a newly loaded artifact brings different semantics" inexpressible.
# Powers now arrive from PROVIDERS, and this is the first one: an ordinary shared library
# supplying `math.max` and `logic.select_int` to whoever mounts it.
#
# `zengine_provider` RATHER THAN `zengine_weave`, and the difference is the point.
# There is no `loom::switchboard` on this line, because there is no participant in this
# image -- no WeaveId, no role, no grant, no manifest, no bus. A host opens the file
# itself and resolves ONE optional symbol, exactly as `OperatorOffer` already resolves the
# consumer's; the Kernel is not involved and `zen_weave_abi` is not exported. That is
# `provider != weave` as a fact about an artifact this repository ships, rather than as a
# sentence in a report.
#
# It links the operator package because a provider AUTHORS: `make_operator` derives the
# schemas, `ProviderDefinitions` serializes them, and the C table carries the bytes. What
# it does NOT do is hold anybody's resolution -- that object belongs to the host, and this
# image has no way to reach one.
if(TARGET loom::kernel)
    zengine_provider(zengine-operators-basic basic_provider.cpp)
    target_link_libraries(zengine-operators-basic PRIVATE zengine-operator)
else()
    message(STATUS "zengine: basic operator provider skipped -- this Loom cannot host loadable images (no loom::kernel)")
endif()
