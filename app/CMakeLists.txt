# The app host (app/docs/app.md): `zengine-app`, the host an app made in Workshop runs in with no
# Workshop; `zengine-app-weave`, the app's own weave; and the vocabulary between them.

# The words: portable, no kernel and no loader, so a suite reads them anywhere.
add_library(zengine-app-vocabulary INTERFACE)
target_include_directories(zengine-app-vocabulary INTERFACE ${CMAKE_SOURCE_DIR})
target_link_libraries(zengine-app-vocabulary INTERFACE zengine-maker loom::core)

# Everything else needs a Loom that hosts loadable weaves and installs its deciding half: an app's
# console decides in Loom's authority store, through Loom's `authority` command.
if(NOT TARGET loom::kernel OR NOT TARGET loom::host-policy)
    message(STATUS "zengine: app host skipped -- this Loom has no loom::kernel or no "
                   "loom::host-policy, so it cannot load an app's weaves under its console")
    return()
endif()

# ---- the app's own weave ------------------------------------------------------------------
zengine_weave(zengine-app-weave weave.cpp)
target_link_libraries(zengine-app-weave PRIVATE zengine-app-vocabulary zengine-view zengine-flow
                                                zengine-graphics-vocabulary zengine-activation)

# ---- the host, composed once for the program and the suite -----------------------------------
# Header-only (`host.hpp`), so the `app_host` suite composes the host the program does. The plan's
# executor is Workshop's (`zengine-workshop-load`), header-only and with no Workshop in it.
add_library(zengine-app-host INTERFACE)
target_link_libraries(zengine-app-host INTERFACE zengine-app-vocabulary zengine-workshop-load
                                                 zengine-view zengine-flow
                                                 zengine-timer-vocabulary
                                                 zengine-graphics-vocabulary
                                                 loom::kernel loom::host-policy
                                                 loom::switchboard loom::core)

add_executable(zengine-app main.cpp)
target_link_libraries(zengine-app PRIVATE zengine-app-host zengine-warnings zengine-sanitize)
# Where the installed package's artifacts stand from the installed program, a relative path, so
# the package names no machine's layout. A build stages its artifacts beside the program instead.
file(RELATIVE_PATH zengine_app_artifacts "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}"
     "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/zengine")
target_compile_definitions(zengine-app PRIVATE
    "ZENGINE_APP_ARTIFACTS=\"${zengine_app_artifacts}\"")

# ---- the artifacts an app's plan names, staged beside the program ----------------------------
# The host resolves a stem in its own directory, as Workshop does, so each is copied here by a rule
# of its own, on the artifact's clock. The OS window joins only where this configuration built it.
set(zengine_app_artifacts zengine-app-weave zengine-operators-basic zengine-graphics
                          zengine-window-terminal zengine-timer
                          zengine-paint-day zengine-paint-night
                          zengine-paint-classic zengine-paint-block)
if(TARGET zengine-window-sdl)
    list(APPEND zengine_app_artifacts zengine-window-sdl)
endif()
if(CMAKE_CONFIGURATION_TYPES)
    set(zengine_app_dir ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>)
else()
    set(zengine_app_dir ${CMAKE_CURRENT_BINARY_DIR})
endif()
set(zengine_app_staged "")
foreach(artifact IN LISTS zengine_app_artifacts)
    set(stamp ${CMAKE_CURRENT_BINARY_DIR}/staged-${artifact}.stamp)
    add_custom_command(OUTPUT ${stamp}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:${artifact}> ${zengine_app_dir}
        COMMAND ${CMAKE_COMMAND} -E touch ${stamp}
        DEPENDS ${artifact}
        COMMENT "app: staging ${artifact} beside the app host")
    list(APPEND zengine_app_staged ${stamp})
endforeach()
add_custom_target(zengine-app-staging DEPENDS ${zengine_app_staged})
add_dependencies(zengine-app zengine-app-staging)
# ...and the SDL runtime the OS window opens, where this configuration fetched one.
zengine_stage_sdl(zengine-app)
# Where the program stands, for the probe's launchers (examples/tally-app/CMakeLists.txt).
set(ZENGINE_APP_DIR ${zengine_app_dir} PARENT_SCOPE)
