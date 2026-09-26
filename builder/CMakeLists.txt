# The Builder package, the one Zengine package whose subject is an effect -- starting an
# operating-system process -- split so its authority stays readable: vocabulary.hpp (the shapes,
# which cannot spell a command), recipe.hpp (authored recipes, pure data), weave.hpp (the tool),
# generate.hpp (a recipe as one process, and a single source's generated project), runner.hpp
# and run.hpp (the only code in Zengine that starts and holds a process).

# One interface target, not three: which header a consumer names carries the split (Workshop
# names only the vocabulary), and the grant the host writes enforces it, never the build graph.
# Header-only and kernel-free; it links loom::core, and the Timer's vocabulary and activation
# cursor for the runner's beat.
add_library(zengine-builder-vocabulary INTERFACE)
target_include_directories(zengine-builder-vocabulary INTERFACE ${CMAKE_SOURCE_DIR})
target_link_libraries(zengine-builder-vocabulary INTERFACE loom::core
                                                          zengine-timer-vocabulary
                                                          zengine-activation)
