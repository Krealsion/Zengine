# The Builder package — a name, a command, and the line between them.
#
# It is the first Zengine package whose subject is an EFFECT rather than a
# picture: building something means starting an operating-system process, which
# nothing in this repository had ever done. So the package is deliberately five
# headers with a boundary drawn through the middle of them:
#
#   vocabulary.hpp  the shapes and the two offices. No command can be spelled in
#                   it, which is what makes "the panel cannot send a command" a
#                   property of the types rather than of a check.
#   recipe.hpp      the AUTHORED RECIPE (BLD-1): what a maker may write down
#                   about how one artifact is produced, its two kinds, and its
#                   law. Pure data and pure predicates — no process, no file, no
#                   bus — which is why the tool, the runner and the file codec
#                   can all name it without any of them acquiring the others'
#                   reach.
#   weave.hpp       the TOOL. Ordinary: it holds recipe NAMES and the ARTIFACT
#                   each produces, remembers what happened, and publishes that.
#                   Three grant rules, no reach.
#   generate.hpp    a recipe, turned into ONE process — and, for a single-source
#                   recipe, the tiny CMake project written around the maker's
#                   `.cpp`. It is where "Zengine does not drive a compiler"
#                   becomes text a suite can assert on.
#   runner.hpp      the RUNNER. It holds the host's authored recipes, is the
#                   only weave that starts a process, and since ASYNC-1 HOLDS the
#                   processes it started; run.hpp is the platform code it calls,
#                   and the only such code in Zengine.
#
# ONE INTERFACE TARGET, NOT THREE, and that is a decision about consumers rather
# than a shortcut. A consumer that wants to PRESENT a build (Workshop) includes
# only `builder/vocabulary.hpp`; a consumer that wants to RUN one (a host)
# includes runner.hpp as well. Splitting the target would suggest the header
# boundary is enforced by the build graph, and it is not — the enforcement is the
# grant the host writes, which is where authority in this system actually lives.
# What DOES carry the split is which header each consumer names, and Workshop's
# sources name exactly one of them.
#
# Header-only, like every other package vocabulary here, and portable in the same
# way: shapes are ZEN_SHAPE structs and run.hpp is POSIX or Win32 with nothing in
# between. It needs no kernel — there is no loadable weave in this package, and
# both weaves are mounted in-process by the host that wants them, exactly as
# Workshop's own weave is.
#
# It links loom::core for zen/weave/shape.hpp and zen/weave.hpp: the shapes are
# ordinary Zen content and both weaves are ordinary WeaveBase weaves.
#
# ...and the Timer's vocabulary and activation cursor since ASYNC-1, because the
# runner is now a `TimedWeave`: it asks for a repeating beat while it holds live
# operations and gives it back when it does not. That is one more package this
# ONE target carries, which is the cost of the single-target decision above and
# is worth naming rather than leaving to be discovered -- a presentation that
# includes only `builder/vocabulary.hpp` still compiles against nothing it does
# not use, and the enforcement of which half a consumer may reach was never the
# build graph's job in the first place. It is the grant the host writes.
add_library(zengine-builder-vocabulary INTERFACE)
target_include_directories(zengine-builder-vocabulary INTERFACE ${CMAKE_SOURCE_DIR})
target_link_libraries(zengine-builder-vocabulary INTERFACE loom::core
                                                          zengine-timer-vocabulary
                                                          zengine-activation)
