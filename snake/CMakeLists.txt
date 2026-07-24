# The snake package — the first game panel: a World, two Drawers, a Score
# weave, and a Controls adapter (each a real .so the kernel loads, unloads,
# and swaps), plus the playable host that drives the three live-evolution
# moments.
#
# Everything here needs a Loom install that can HOST .so weaves; the honest
# question is "does this install export loom::kernel?" (it does on Linux, and
# deliberately does not on Windows — the kernel is dlopen/POSIX ground —
# unless the explicit development/demo backend is enabled).
if(NOT TARGET loom::kernel)
    message(STATUS "zengine: snake package skipped — this Loom cannot host .so weaves (no loom::kernel)")
    return()
endif()

# The weave-library pattern (and the load-bearing -fno-gnu-unique law) lives in
# the top-level zengine_weave() — one definition for every package.
zengine_weave(snake-world-v1 world.cpp)
zengine_weave(snake-world-v2 world.cpp)
target_compile_definitions(snake-world-v2 PRIVATE SNAKE_WORLD_V2)
zengine_weave(snake-drawer-classic drawer_classic.cpp)
zengine_weave(snake-drawer-block drawer_block.cpp)
zengine_weave(snake-score score.cpp)

# The controls adapter: snake's own input binding — it accepts the Input
# package's KeyPressed and speaks the world's SnakeTurn. The one place where
# "which keys steer" lives; the world stays a keyboard-ignorant rule machine.
zengine_weave(snake-controls controls.cpp)
target_link_libraries(snake-controls PRIVATE zengine-input-vocabulary)

# The playable host. It resolves the .so files beside its own binary, so every
# weave it loads — the package's own, and the Input package's producer — is
# pinned to the host's runtime directory (deployment is the host's concern:
# the input package does not know who plays it).
add_executable(zengine-snake play.cpp)
target_link_libraries(zengine-snake
    PRIVATE loom::kernel loom::switchboard loom::core zengine-input-vocabulary zengine-warnings)
set_target_properties(zengine-snake snake-world-v1 snake-world-v2 snake-drawer-classic
                      snake-drawer-block snake-score snake-controls zengine-input
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
