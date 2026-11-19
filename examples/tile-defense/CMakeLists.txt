# The tile defense: a game drawn in tiles, sprites and points, one game's code shown in the GUI
# window and in the terminal, each its own way (examples/tile-defense/README.md).

# Its weaves are loaded by a kernel, which is to say a Loom exporting loom::kernel.
if(NOT TARGET loom::kernel)
    message(STATUS "zengine: tile defense skipped -- this Loom cannot host loadable weaves (no loom::kernel)")
    return()
endif()

zengine_weave(tile-defense game.cpp)
target_link_libraries(tile-defense PRIVATE zengine-graphics-vocabulary zengine-input-vocabulary
                                           zengine-timer-vocabulary zengine-activation)
zengine_weave(tile-defense-stage stage.cpp)
target_link_libraries(tile-defense-stage PRIVATE zengine-graphics-vocabulary zengine-activation)

# The host, its own weaves beside it, and the artifacts it loads staged beside it too: it resolves
# every stem in its own directory.
add_executable(zengine-tile-defense play.cpp)
target_link_libraries(zengine-tile-defense
    PRIVATE loom::kernel loom::switchboard loom::core loom::host-policy
            zengine-graphics-vocabulary zengine-timer-vocabulary zengine-warnings
            zengine-sanitize)
set_target_properties(zengine-tile-defense tile-defense tile-defense-stage
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
set(tile_defense_artifacts zengine-graphics zengine-window-tui zengine-timer)
if(TARGET zengine-window-sdl)
    list(APPEND tile_defense_artifacts zengine-window-sdl)
endif()
set(tile_defense_staged "")
foreach(artifact IN LISTS tile_defense_artifacts)
    set(stamp ${CMAKE_CURRENT_BINARY_DIR}/staged-${artifact}.stamp)
    add_custom_command(OUTPUT ${stamp}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:${artifact}> ${CMAKE_CURRENT_BINARY_DIR}
        COMMAND ${CMAKE_COMMAND} -E touch ${stamp}
        DEPENDS ${artifact}
        COMMENT "tile defense: staging ${artifact} beside its host")
    list(APPEND tile_defense_staged ${stamp})
endforeach()
add_custom_target(tile-defense-staging DEPENDS ${tile_defense_staged})
add_dependencies(zengine-tile-defense tile-defense-staging tile-defense tile-defense-stage)
# ...and the SDL runtime the GUI window opens, where this configuration fetched one.
zengine_stage_sdl(zengine-tile-defense)

# The bench: the game played to its busiest wave through each window in process, and what one
# picture costs said; with --pictures, pictures of both windows mid-wave. It mounts the windows'
# own C++, so it compiles their manuals in, and SDL's where this configuration built the GUI window.
add_executable(zengine-tile-defense-bench bench.cpp)
target_link_libraries(zengine-tile-defense-bench
    PRIVATE loom::kernel loom::switchboard loom::core loom::host-policy
            zengine-graphics-vocabulary zengine-input-vocabulary zengine-timer-vocabulary
            zengine-activation zengine-warnings zengine-sanitize)
zengine_manual(zengine-tile-defense-bench PRIVATE ${PROJECT_SOURCE_DIR}/graphics/docs/zengine.window.tui.md)
if(TARGET zengine-window-sdl)
    zengine_manual(zengine-tile-defense-bench PRIVATE ${PROJECT_SOURCE_DIR}/graphics/docs/zengine.window.gui.md)
    target_compile_definitions(zengine-tile-defense-bench PRIVATE TILE_DEFENSE_HAS_SDL=1)
    target_link_libraries(zengine-tile-defense-bench PRIVATE ${ZENGINE_SDL_LIB} ${ZENGINE_SDL_TEXT_LIB}
                                                             zengine-window-font)
endif()
set_target_properties(zengine-tile-defense-bench
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
add_dependencies(zengine-tile-defense-bench tile-defense-staging tile-defense-stage)
