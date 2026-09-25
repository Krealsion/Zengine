# The Workshop package: the maker-facing application's vocabulary, its logic compiled once, and
# its host. Workshop's own weave is mounted in-process; the weaves the host loads are others'.
#
# The vocabulary is header-only, as input/ and surface/ are, and the suites include the headers
# the host does, so what the tests pin is what ships. It is portable: no kernel, no loader.

# Every edge is named on its own line, never inherited through a sibling. The builder and
# operator edges carry shapes and the catalog: no presentation header names builder/runner.hpp,
# builder/run.hpp, operator/provider_host.hpp or operator/image.hpp, so a presentation reaches
# neither process authority nor the dynamic loader (`zengine-workshop-load` loads).
add_library(zengine-workshop-vocabulary INTERFACE)
target_include_directories(zengine-workshop-vocabulary INTERFACE ${CMAKE_SOURCE_DIR})
target_link_libraries(zengine-workshop-vocabulary INTERFACE zengine-surface-vocabulary
                                                            zengine-ui-vocabulary
                                                            zengine-component
                                                            zengine-builder-vocabulary
                                                            zengine-operator
                                                            loom::core
                                                            loom::terminal)

# ---- The pane protocol, alone, as a package a stranger links -------------------------
#
# `workshop/pane_vocabulary.hpp` needs only the surface row type and <zen/weave/shape.hpp>, and is
# exported on its own (`zengine::pane`, cmake/ZengineInstall.cmake) so a pane built against the
# installed package can arrive by a plan row. In-tree panes reach it by include path.
add_library(zengine-pane-vocabulary INTERFACE)
target_include_directories(zengine-pane-vocabulary INTERFACE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/zengine>)
add_library(zengine::pane ALIAS zengine-pane-vocabulary)
target_link_libraries(zengine-pane-vocabulary INTERFACE zengine-surface-vocabulary loom::core)

# ---- The authored load plan: its shape, its file, its execution, and the observation door ----
#
# A second target, so the vocabulary stays portable: a load plan spends `op::MountMode`, opens
# images and sends `zen.LoadWeave` on a real bus. It links no kernel (the host hands in the bus
# and the Manager's WeaveId), so it sits outside the `loom::kernel` gate below.

# The observation door (`arrangement_vocabulary.hpp`, `arrangement.hpp`) is here because it reads
# this target's two subjects; `zengine-introspection` includes its vocabulary by include path
# (introspection/CMakeLists.txt). The builder vocabulary is named on its own line for
# `load_execute.hpp`'s two shapes, `builder::ArtifactBuilt` and `builder::ArtifactRealized`.
add_library(zengine-workshop-load INTERFACE)
target_include_directories(zengine-workshop-load INTERFACE ${CMAKE_SOURCE_DIR})
target_link_libraries(zengine-workshop-load INTERFACE zengine-workshop-vocabulary
                                                      zengine-builder-vocabulary
                                                      zengine-operator
                                                      loom::core
                                                      loom::switchboard)

# ---- The logic, compiled once ----------------------------------------------------------
#
# Static and linked, not included: a body in a header is paid for by every translation unit that
# reaches it (the COFF ceiling, top-level CMakeLists.txt). The warnings and the sanitizer are
# PRIVATE, so this library is inside the sanitizer witness (agents/verification.md) without
# exporting either.
add_library(zengine-workshop-logic STATIC
    screen_arrange.cpp
    screen_attention.cpp
    screen_bindings.cpp
    screen_chrome.cpp
    screen_compose.cpp
    screen_external.cpp
    screen_gestures.cpp
    screen_hotkeys.cpp
    screen_layouts.cpp
    screen_pane_subject.cpp
    screen_pane_state.cpp
    screen_reveal.cpp
    screen_terminal.cpp
    weave_arrange.cpp
    weave_desktop.cpp
    weave_document.cpp
    weave_canvas.cpp
    weave_external.cpp
    weave_operation.cpp
    weave_handlers.cpp
    weave_inspection.cpp
    weave_maker.cpp
    weave_pointer.cpp
    weave_run.cpp
    weave_seam.cpp
    weave_session.cpp
    weave_terminal.cpp
    # The presentation owner's half of a managed opening, the opening manager itself, and
    # the host's turn of the bus (the pump seam a native showing failure reaches).
    weave_managed.cpp
    weave_opening.cpp
    host_pump.cpp
    # The host's watch over the quit question's deliveries, and the book Workshop reads.
    quit_delivery.cpp
    # A pane's Edit Code: the host's answer about its code spent, the managed open asked for,
    # and the Builder told which recipe the opened source belongs to.
    weave_code.cpp
)
target_sources(zengine-workshop-logic PRIVATE grant.cpp)

target_link_libraries(zengine-workshop-logic PUBLIC zengine-workshop-vocabulary
                                             PRIVATE zengine-warnings zengine-sanitize)

# The host needs a Loom that can host .so weaves: it loads a Skin and an Input producer.
if(NOT TARGET loom::kernel)
    message(STATUS "zengine: workshop host skipped — this Loom cannot host .so weaves (no loom::kernel)")
    return()
endif()

# ---- The session history, as an artifact ---------------------------------------
#
# A provider, not a weave, as `zengine-operators-basic` is: no participant, so no switchboard. It
# links the vocabulary because a conversion names the shape it converts to. Not installed: it
# means nothing outside a Workshop run (cmake/ZengineInstall.cmake).
zengine_provider(zengine-workshop-session-history session_migration_provider.cpp)
target_link_libraries(zengine-workshop-session-history
    PRIVATE zengine-workshop-vocabulary zengine-operator)

# ---- The guests file and the door's policy, compiled once --------------------------------
#
# `guests.hpp` is the file a maker writes to admit another host and the grants its powers are
# worth; `guest_door.hpp` holds the listener over Loom's crossing. A library, so the guests suite
# can pin the policy without a window or a plan.
add_library(zengine-workshop-guests STATIC guests.cpp)
target_include_directories(zengine-workshop-guests PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(zengine-workshop-guests
    PUBLIC loom::bridge loom::switchboard loom::core
           zengine-input-vocabulary zengine-surface-vocabulary zengine-timer-vocabulary
           zengine-workshop-vocabulary zengine::inventory
    PRIVATE zengine-warnings zengine-sanitize)

add_executable(zengine-workshop workshop.cpp)
target_link_libraries(zengine-workshop
    PRIVATE loom::kernel loom::switchboard loom::core loom::terminal loom::history
            # ...and the crossing, through the guests library: the door this host mounts when a
            # guests file names other hosts, and the policy that file becomes.
            zengine-workshop-guests loom::bridge
            zengine-workshop-vocabulary
            # ...and the bodies, compiled once (the logic target above): this host reaches
            # every one of them, and its object is where the ceiling was measured.
            zengine-workshop-logic
            zengine-surface-vocabulary zengine-ui-vocabulary
            zengine-timer-vocabulary zengine-builder-vocabulary
            # The surface and timer vocabularies are here because this host writes grants and a
            # log selection in their shapes; what to load is the plan's, so no package is named
            # here to save a plan row. The operator package is the host half of the catalog this host
            # owns, which starts empty and is filled by mounting providers.
            zengine-operator
            # ...and the load plan: the typed plan, its codec and its executor.
            zengine-workshop-load
            zengine-warnings zengine-sanitize)

# ---- The one thing this host hands its build runner -------------------------------------
#
# ${CMAKE_COMMAND} is the cmake that configured this tree, by absolute path: one resolved from
# PATH could drive a different CMake against this tree, and a recipe naming a program would make
# every recipe catalog an arbitrary-execution document.
target_compile_definitions(zengine-workshop PRIVATE
    "ZENGINE_BUILDER_CMAKE=\"${CMAKE_COMMAND}\"")

# ---- The shipped recipe catalog, configured rather than compiled in ---------------------
#
# Its target, zengine-skin-tui-block, is an artifact always built here and never loaded or staged
# by this host: building an image the host has mapped changes code under a running program. Its
# `artifact_dir` is asked of the target, never guessed (snake stages that skin elsewhere), and
# `file(GENERATE)` below expands what `configure_file` copies as text.
set(ZENGINE_RECIPE_ARTIFACT_DIR "$<TARGET_FILE_DIR:zengine-skin-tui-block>")
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/default-build-recipes.json.in
               ${CMAKE_CURRENT_BINARY_DIR}/default-build-recipes.json.in2 @ONLY)

# The artifacts this host loads, staged beside its binary: the host resolves them from its own
# directory (workshop.cpp's exe_dir). Copied, not aimed here: snake aims the same files at its own
# directory, and each host takes a copy. The SDL skin joins only where this configuration built
# it; `--skin` selects one at launch, and staging makes the stem resolvable.
set(zengine_workshop_artifacts zengine-skin-tui-classic zengine-input zengine-timer zengine-inventory zengine-inventory-pane
                               zengine-introspection zengine-composer zengine-files
                               zengine-builder-pane zengine-attention-pane
                               zengine-desktop-pane zengine-connections-pane zengine-demo-control
                               zengine-menu-presenter zengine-flow-pane
                               zengine-info-pane zengine-terminal-pane
                               zengine-editor-pane zengine-neovim-editor
                               zengine-operators-basic
                               zengine-workshop-session-history)
if(TARGET zengine-skin-sdl)
    list(APPEND zengine_workshop_artifacts zengine-skin-sdl)
endif()
if(TARGET zengine-input-sdl)
    list(APPEND zengine_workshop_artifacts zengine-input-sdl)
endif()
# Each copy is a rule with its own OUTPUT and DEPENDS, on the artifact's clock: a POST_BUILD copy
# runs only when the host relinks, so an edited Skin went on loading stale. The host's directory
# is spelled out: `$<TARGET_FILE_DIR:zengine-workshop>` would make these rules depend on the host
# that depends on them.
if(CMAKE_CONFIGURATION_TYPES)
    set(zengine_workshop_dir ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>)
else()
    set(zengine_workshop_dir ${CMAKE_CURRENT_BINARY_DIR})
endif()
set(zengine_workshop_staged "")
foreach(artifact IN LISTS zengine_workshop_artifacts)
    set(stamp ${CMAKE_CURRENT_BINARY_DIR}/staged-${artifact}.stamp)
    add_custom_command(OUTPUT ${stamp}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:${artifact}> ${zengine_workshop_dir}
        COMMAND ${CMAKE_COMMAND} -E touch ${stamp}
        DEPENDS ${artifact}
        COMMENT "workshop: staging ${artifact} beside the host")
    list(APPEND zengine_workshop_staged ${stamp})
endforeach()
# ---- The authored load plans, which are not artifacts ---------------------------------
#
# Staged by the same stamp rule, since the host resolves its default plan from its own directory.
# The graphical plan only where its artifacts exist: a file that can only refuse is not offered.
# Not in `zengine_workshop_artifacts`: an artifact is a library this host may execute.
set(zengine_workshop_plans default-load-plan.json)
if(TARGET zengine-skin-sdl AND TARGET zengine-input-sdl)
    list(APPEND zengine_workshop_plans graphical-load-plan.json)
endif()
foreach(plan IN LISTS zengine_workshop_plans)
    set(stamp ${CMAKE_CURRENT_BINARY_DIR}/staged-${plan}.stamp)
    add_custom_command(OUTPUT ${stamp}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${CMAKE_CURRENT_SOURCE_DIR}/${plan} ${zengine_workshop_dir}
        COMMAND ${CMAKE_COMMAND} -E touch ${stamp}
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${plan}
        COMMENT "workshop: staging ${plan} beside the host")
    list(APPEND zengine_workshop_staged ${stamp})
endforeach()

add_custom_target(zengine-workshop-staging DEPENDS ${zengine_workshop_staged})
add_dependencies(zengine-workshop zengine-workshop-staging)

# ---- The shipped recipe catalog, written straight into the host's directory ------
#
# `file(GENERATE)`, not a staging stamp: the file has no source-tree original, and its layout is
# known only at generate time.
file(GENERATE OUTPUT ${zengine_workshop_dir}/default-build-recipes.json
     INPUT ${CMAKE_CURRENT_BINARY_DIR}/default-build-recipes.json.in2)

# ---- The development setup: Workshop's own panes as a project (WL-CODE-06, WL-CODE-07) ----
# Facts only this configuration knows, generated so no host code names a pane, target or path:
# where each pane builds and its editing entry. Nothing selects this catalog; a maker names it.
# Which weaves are panes is no build fact, so they are listed; which may reload is the
# realization owner's, at the reload.
set(zengine_workshop_pane_weaves zengine-attention-pane zengine-builder-pane zengine-desktop-pane
                                 zengine-connections-pane
                                 zengine-editor-pane
                                 zengine-neovim-editor zengine-files zengine-info-pane
                                 zengine-terminal-pane
                                 zengine-introspection zengine-composer
                                 # ...and the menu presenter, which draws nothing of its own but
                                 # is a weave a maker edits, builds and reloads like any pane
                                 zengine-menu-presenter zengine-flow-pane)
if(CMAKE_CONFIGURATION_TYPES)
    set(zengine_development_config "$<CONFIG>")
    set(zengine_development_runtime "${CMAKE_BINARY_DIR}/workshop-runtime/$<CONFIG>")
else()
    set(zengine_development_config "")
    set(zengine_development_runtime "${CMAKE_BINARY_DIR}/workshop-runtime")
endif()
set(zengine_development_rows "")
foreach(pane IN LISTS zengine_workshop_pane_weaves)
    if(NOT TARGET ${pane})
        continue()
    endif()
    if(NOT zengine_development_rows STREQUAL "")
        string(APPEND zengine_development_rows ",\n")
    endif()
    string(APPEND zengine_development_rows
        "      {\n"
        "        \"recipe\": \"${pane}\",\n"
        "        \"artifact\": \"${pane}\",\n"
        "        \"artifact_dir\": \"$<TARGET_FILE_DIR:${pane}>\",\n"
        "        \"cmake_target\": [\n"
        "          {\n"
        "            \"build_dir\": \"${CMAKE_BINARY_DIR}\",\n"
        "            \"target\": \"${pane}\",\n"
        "            \"config\": \"${zengine_development_config}\",\n"
        "            \"entry\": \"$<TARGET_PROPERTY:${pane},ZENGINE_WEAVE_SOURCE>\"\n"
        "          }\n"
        "        ],\n"
        "        \"single_source\": []\n"
        "      }")
endforeach()
file(GENERATE OUTPUT ${zengine_workshop_dir}/development-build-recipes.json CONTENT
"{
  \"zen\": 1,
  \"schema\": \"WorkshopRecipeFile\",
  \"version\": 2,
  \"fields\": {
    \"format\": \"zengine-build-recipes\",
    \"format_version\": \"2\",
    \"recipes\": [
${zengine_development_rows}
    ]
  }
}
")

# The development runtime: a Workshop run from this tree's host directory maps files a build
# stages there, so the one a maker edits itself with runs from a copy no build rule writes, made
# or reused by the launch below or `cmake [-DZEN_RUNTIME=<dir>] -P
# <build>/workshop/development-runtime.cmake`. A `-P` script, so no recipe can name it; its policy
# is `prepare-runtime.cmake`. A runtime changes its pane copies itself, so a rebuilt pane does not
# stale it, and a rebuilt host or service does.
set(zengine_development_copies "$<TARGET_FILE:zengine-workshop>")
set(zengine_development_replaceable "")
foreach(artifact IN LISTS zengine_workshop_artifacts)
    list(APPEND zengine_development_copies "$<TARGET_FILE:${artifact}>")
    if(artifact IN_LIST zengine_workshop_pane_weaves)
        list(APPEND zengine_development_replaceable "$<TARGET_FILE:${artifact}>")
    endif()
endforeach()
foreach(plan IN LISTS zengine_workshop_plans)
    list(APPEND zengine_development_copies "${CMAKE_CURRENT_SOURCE_DIR}/${plan}")
endforeach()
list(APPEND zengine_development_copies "${zengine_workshop_dir}/default-build-recipes.json"
                                       "${zengine_workshop_dir}/development-build-recipes.json"
                                       ${ZENGINE_SDL_RUNTIME})
string(REPLACE ";" "\"\n    \"" zengine_development_copy_lines "${zengine_development_copies}")
string(REPLACE ";" "\"\n    \"" zengine_development_replaceable_lines
               "${zengine_development_replaceable}")
file(GENERATE OUTPUT ${zengine_workshop_dir}/development-runtime.cmake CONTENT
"# Generated by Zengine's configure for ONE build tree. Do not edit: configuring rewrites it.
#
# Makes or reuses a development runtime: a copy of this build's Workshop that no build rule
# writes, so a pane rebuilt from its development catalog reloads into a running Workshop without
# the build touching an image that Workshop has mapped. See docs/workshop/develop-workshop.md.
#
#     cmake [-DZEN_RUNTIME=<dir>] -P ${zengine_workshop_dir}/development-runtime.cmake
cmake_minimum_required(VERSION 3.16)
set(zengine_build \"${CMAKE_BINARY_DIR}\")
set(zengine_source \"${CMAKE_SOURCE_DIR}\")
set(zengine_configuration \"$<CONFIG>\")
set(zengine_default_runtime \"${zengine_development_runtime}\")
set(zengine_launch \"$<TARGET_FILE:zengine-workshop-develop>\")
set(zengine_copies
    \"${zengine_development_copy_lines}\")
set(zengine_replaceable
    \"${zengine_development_replaceable_lines}\")
include(\"${CMAKE_CURRENT_SOURCE_DIR}/prepare-runtime.cmake\")
")

# ---- The development launch (WL-CODE-08) -----------------------------------------------
# What a maker runs to work in that runtime (CLion's shared "Develop Workshop" configuration names
# it): it claims the runtime, runs the script above and starts the runtime's host, or says why
# not. Nothing inside Workshop can launch it. It depends on the host, so an IDE's build refreshes
# what a runtime copies, and exists without SDL, refusing to launch, so every lane compiles it.
if("graphical-load-plan.json" IN_LIST zengine_workshop_plans)
    set(zengine_development_plan graphical-load-plan.json)
else()
    set(zengine_development_plan "")
endif()
add_executable(zengine-workshop-develop develop.cpp)
target_link_libraries(zengine-workshop-develop
    PRIVATE zengine-builder-vocabulary zengine-warnings zengine-sanitize)
add_dependencies(zengine-workshop-develop zengine-workshop)
target_compile_definitions(zengine-workshop-develop PRIVATE
    "ZENGINE_DEVELOP_CMAKE=\"${CMAKE_COMMAND}\""
    "ZENGINE_DEVELOP_SCRIPT=\"${zengine_workshop_dir}/development-runtime.cmake\""
    "ZENGINE_DEVELOP_BUILD=\"${CMAKE_BINARY_DIR}\""
    "ZENGINE_DEVELOP_RUNTIME=\"${zengine_development_runtime}\""
    "ZENGINE_DEVELOP_PROJECT=\"${CMAKE_BINARY_DIR}/workshop-project\""
    "ZENGINE_DEVELOP_HOST=\"$<TARGET_FILE_NAME:zengine-workshop>\""
    "ZENGINE_DEVELOP_PLAN=\"${zengine_development_plan}\""
    "ZENGINE_DEVELOP_CATALOG=\"development-build-recipes.json\"")

# And the shared SDL3 the two SDL-backed weaves link (cmake/ZengineSdl.cmake says why shared): on
# Windows a shared library is found in the application directory and nowhere else.
zengine_stage_sdl(zengine-workshop)
