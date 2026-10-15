# The snake package: the first game -- a World that keeps its own rules, keys and beat and draws
# into a terminal window, its board as the game's own element code, a late-loaded Score weave, and
# the playable host that drives the live-evolution moments (examples/snake/README.md).

# Everything here needs a Loom that can host .so weaves, which is to say one exporting
# loom::kernel: Linux does; Windows does only with the development backend enabled.
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
foreach(world snake-world-v1 snake-world-v2)
    target_link_libraries(${world} PRIVATE zengine-graphics-vocabulary zengine-input-vocabulary
                                           zengine-timer-vocabulary zengine-activation)
endforeach()
zengine_weave(snake-score score.cpp)
target_link_libraries(snake-score PRIVATE zengine-graphics-vocabulary zengine-input-vocabulary)

# The board's code: element code a window opens, not a weave -- no participant lives in it.
zengine_provider(snake-board board.cpp)
target_link_libraries(snake-board PRIVATE zengine-graphics-vocabulary)

# The playable host. It resolves every library beside its own binary, so every weave and every
# piece of code it names -- the package's own, the window, the directory, the skins and the Timer
# -- is pinned to the host's runtime directory (deployment is the host's concern: those packages
# do not know who plays them).
add_executable(zengine-snake play.cpp)
target_link_libraries(zengine-snake
    PRIVATE loom::kernel loom::switchboard loom::core zengine-graphics-vocabulary
            zengine-input-vocabulary zengine-timer-vocabulary zengine-warnings zengine-sanitize)
set(zengine_snake_runtime zengine-snake snake-world-v1 snake-world-v2 snake-score snake-board
    zengine-graphics zengine-window-terminal zengine-paint-classic zengine-paint-block
    zengine-timer)
set_target_properties(${zengine_snake_runtime}
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
