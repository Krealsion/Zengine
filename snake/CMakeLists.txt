# The snake package — the first game panel: a World, a Score weave, and a
# Controls adapter (each a real .so the kernel loads, unloads, and swaps),
# plus the playable host that drives the live-evolution moments. Drawing left
# this package in the Surface migration: the world publishes SnakeVisual, the
# score weave and the host's operator publish SurfaceText, and the Surface
# package's skins do every bit of the painting.
#
# Everything here needs a Loom install that can HOST .so weaves; the honest
# question is "does this install export loom::kernel?" (it does on Linux, and
# deliberately does not on Windows — the kernel is dlopen/POSIX ground —
# unless the explicit development/demo backend is enabled).
if(NOT TARGET loom::kernel)
    message(STATUS "zengine: snake package skipped — this Loom cannot host .so weaves (no loom::kernel)")
    return()
endif()

# The weave-library pattern lives in the top-level zengine_weave() — one definition
# for every package — and the reloadable-lifetime law it applies is the Loom's
# (loom_weave_build_contract, KERN-05), not this repo's to reimplement.
zengine_weave(snake-world-v1 world.cpp)
zengine_weave(snake-world-v2 world.cpp)
target_compile_definitions(snake-world-v2 PRIVATE SNAKE_WORLD_V2)
zengine_weave(snake-score score.cpp)
target_link_libraries(snake-score PRIVATE zengine-surface-vocabulary)

# The controls adapter: snake's own input binding — it accepts the Input
# package's KeyPressed and speaks the world's SnakeTurn. The one place where
# "which keys steer" lives; the world stays a keyboard-ignorant rule machine.
zengine_weave(snake-controls controls.cpp)
target_link_libraries(snake-controls PRIVATE zengine-input-vocabulary)

# The clock adapter: snake's own time binding — the controls move, pointed at
# time. It asks the Timer package for the 120ms beat and speaks the world's
# SnakeTick; the one place where "how fast the world runs" lives, and the
# world stays a clock-ignorant rule machine.
zengine_weave(snake-clock clock.cpp)
target_link_libraries(snake-clock PRIVATE zengine-timer-vocabulary zengine-activation)

# The playable host. It resolves the .so files beside its own binary, so every
# weave it loads — the package's own, the Input producer, and the Surface
# skins — is pinned to the host's runtime directory (deployment is the host's
# concern: those packages do not know who plays them).
add_executable(zengine-snake play.cpp)
target_link_libraries(zengine-snake
    PRIVATE loom::kernel loom::switchboard loom::core zengine-input-vocabulary
            zengine-surface-vocabulary zengine-timer-vocabulary zengine-warnings)
set(zengine_snake_runtime zengine-snake snake-world-v1 snake-world-v2 snake-score
    snake-controls snake-clock zengine-input zengine-timer
    zengine-skin-tui-classic zengine-skin-tui-block)
if(TARGET zengine-skin-sdl)
    list(APPEND zengine_snake_runtime zengine-skin-sdl)
endif()
set_target_properties(${zengine_snake_runtime}
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
