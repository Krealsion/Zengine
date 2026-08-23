# The Timer package — time, message-shaped. One vocabulary (the four locked
# shapes + the three named additions), one loadable weave that is the sole
# authority for them and the only place in the running system that reads the
# OS clock or sleeps.

# The vocabulary travels as a header-only interface target, same as input's:
# consumers spell `#include "timer/vocabulary.hpp"`. Portable — shapes are
# just ZEN_SHAPE structs — so it exists even where no kernel does.
add_library(zengine-timer-vocabulary INTERFACE)
target_include_directories(zengine-timer-vocabulary INTERFACE ${CMAKE_SOURCE_DIR})

# The weave itself needs a Loom that can HOST loadable weaves (the same
# honest question every package asks: does this install export loom::kernel?).
if(NOT TARGET loom::kernel)
    message(STATUS "zengine: timer weave skipped — this Loom cannot host loadable weaves (no loom::kernel)")
    return()
endif()

zengine_weave(zengine-timer timer.cpp)
# The service authors its beat chain from its own activation, so it reads one.
# It also EXECUTES a semantic rule rather than privately implementing one, so it
# consumes the operator package — see timer/normalize.hpp for what that buys and
# what it costs.
#
# TWO OPERATOR TARGETS, AND THE PAIR IS THE POINT (CAT-0, sharpened by PROV-0).
# This artifact is both halves of OPH-0's distinction and it is the first thing in
# the repository to be both: it AUTHORS its delay composition and exports it across
# the provider seam (`zengine-operator`, which carries `make_operator`, `Builder`
# and the contribution codec, and which is also what a fallback Timer assembles a
# local vocabulary with), and it CONSUMES a host's runtime operator surface
# (`zengine-operator-consumer`, the C table and the handle). Naming the consumer
# explicitly rather than leaning on the transitive edge is deliberate: a provider
# that also consumes should say so on its own line. Neither target is a library --
# both are header-only INTERFACE targets -- so this adds no shared-object
# dependency.
#
# WHAT IT NO LONGER SUPPLIES is the interesting half. Since PROV-0 this image
# contributes ONE power, `timer.normalize_delay`, and the two arithmetic leaves that
# rule is built from arrive from `zengine-operators-basic` -- a separate artifact,
# not a weave, mounted by the host. So the export table here names three symbols and
# the vocabulary behind one of them is exactly one operator wide.
target_link_libraries(zengine-timer
    PRIVATE zengine-activation zengine-operator zengine-operator-consumer)
