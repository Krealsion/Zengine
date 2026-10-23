# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
#
# Zengine as an installable CMake package: the whole public consumer surface in one file, which
# targets a project may link, which headers it receives, which artifacts arrive beside them and
# how it finds the Loom, with the reason each package is in or out (agents/packaging.md). Its
# shape follows the Loom's: `zengine::` names, lib/cmake/zengine/, a version file.

# ---- The switch ------------------------------------------------------------------------
#
# ON when this repository is the build, OFF when someone has add_subdirectory'd it: a
# consumer embedding Zengine in their own tree has their own install rules and should not
# have Zengine's fire inside them. Same predicate the Loom uses.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(zengine_default_install ON)
else()
    set(zengine_default_install OFF)
endif()
option(ZENGINE_INSTALL "Generate the install/export rules for find_package(zengine)"
       ${zengine_default_install})

# A dev-mode build may not produce a package, and this refuses rather than warns: with
# ZEN_LOOM_DEV=ON `loom::core` is an in-tree target, so the export would name it with nothing to
# resolve it and find_dependency(loom) would look for a Loom the build never used -- a package
# that configures only where some Loom happens to be installed.
if(ZENGINE_INSTALL AND ZEN_LOOM_DEV)
    message(FATAL_ERROR
        "zengine: ZENGINE_INSTALL and ZEN_LOOM_DEV are both ON, and the package that build "
        "would produce is not honest.\n"
        "WHY\n"
        "  Dev mode takes the Loom from the sibling source tree, so this build never consumed "
        "an installed Loom package -- but the exported targets name loom::core and the "
        "generated config calls find_dependency(loom). The package would only resolve on a "
        "machine that separately happens to have a Loom installed.\n"
        "WHAT TO DO\n"
        "  Install a Loom, then configure Zengine the stranger's way (the default):\n"
        "    cmake -S . -B build -DCMAKE_PREFIX_PATH=<loom prefix>\n"
        "  Or keep dev mode and turn the package off: -DZENGINE_INSTALL=OFF")
endif()

if(NOT ZENGINE_INSTALL)
    message(STATUS "zengine: install/export rules skipped (ZENGINE_INSTALL=OFF)")
    return()
endif()

include(CMakePackageConfigHelpers)

# ---- the exported targets ---------------------------------------------------------------
# A target is exported when linking it grants a user-facing capability, not because something in
# the tree needs it; agents/packaging.md says what each grants, and the end of this file what is
# out. EXPORT_NAME makes `zengine::surface` the same target from this tree and from a prefix.
set(ZENGINE_EXPORTED_TARGETS
    zengine-maker
    zengine-flow
    zengine-message-draft
    zengine-inventory-vocabulary
    zengine-source-transfer-vocabulary
    zengine-activation
    zengine-timer-vocabulary
    zengine-surface-vocabulary
    zengine-input-vocabulary
    zengine-ui-vocabulary
    zengine-component
    zengine-operator
    zengine-operator-consumer
    zengine-pane-vocabulary
    zengine-neovim-editor-vocabulary
    zengine-graphics-vocabulary)

set_target_properties(zengine-maker PROPERTIES EXPORT_NAME maker)
set_target_properties(zengine-flow PROPERTIES EXPORT_NAME flow)
set_target_properties(zengine-message-draft PROPERTIES EXPORT_NAME message-draft)
set_target_properties(zengine-inventory-vocabulary PROPERTIES EXPORT_NAME inventory)
set_target_properties(zengine-source-transfer-vocabulary PROPERTIES EXPORT_NAME source-transfer)

set_target_properties(zengine-activation          PROPERTIES EXPORT_NAME activation)
set_target_properties(zengine-timer-vocabulary    PROPERTIES EXPORT_NAME timer)
set_target_properties(zengine-surface-vocabulary  PROPERTIES EXPORT_NAME surface)
set_target_properties(zengine-input-vocabulary    PROPERTIES EXPORT_NAME input)
set_target_properties(zengine-ui-vocabulary       PROPERTIES EXPORT_NAME ui)
set_target_properties(zengine-component           PROPERTIES EXPORT_NAME component)
set_target_properties(zengine-operator            PROPERTIES EXPORT_NAME operator)
set_target_properties(zengine-operator-consumer   PROPERTIES EXPORT_NAME operator-consumer)
set_target_properties(zengine-pane-vocabulary     PROPERTIES EXPORT_NAME pane)
set_target_properties(zengine-neovim-editor-vocabulary PROPERTIES EXPORT_NAME neovim)
set_target_properties(zengine-graphics-vocabulary PROPERTIES EXPORT_NAME graphics)

# Every one of them is an INTERFACE target, so nothing is installed here but the target
# definitions themselves; the headers go below and there is no library to place.
install(TARGETS ${ZENGINE_EXPORTED_TARGETS} EXPORT zengineTargets)

# ---- the public headers, named one at a time --------------------------------------------
# Explicit lists, not an install(DIRECTORY) pattern: a package directory holds its implementation
# beside its vocabulary, and a pattern would ship both -- the skins with the game shapes they
# include, the SDL translation without SDL, the Timer service as a supported external path. The
# destination keeps the package directory, so `#include "timer/vocabulary.hpp"` reads the same
# from either side; tests/package checks that the set's includes close over it.
set(zengine_public_headers_activation activation/activation.hpp)
set(zengine_public_headers_timer      timer/vocabulary.hpp
                                      timer/binding.hpp)
set(zengine_public_headers_surface    surface/vocabulary.hpp
                                      surface/cells.hpp
                                      surface/region.hpp
                                      surface/pointing.hpp)
set(zengine_public_headers_input      input/vocabulary.hpp
                                      input/translate.hpp
                                      input/input_weave.hpp)
set(zengine_public_headers_ui         ui/vocabulary.hpp
                                      ui/layout.hpp)
set(zengine_public_headers_manual     manual/manual.hpp)
set(zengine_public_headers_component  component/motion.hpp
                                      component/text_box.hpp
                                      component/list_window.hpp
                                      component/row_map.hpp
                                      component/held_choice.hpp
                                      component/control_strip.hpp
                                      component/columns.hpp
                                      component/tab_strip.hpp)
set(zengine_public_headers_operator   operator/operator.hpp
                                      operator/catalog.hpp
                                      operator/fold.hpp
                                      operator/reference.hpp
                                      operator/source.hpp
                                      operator/migration.hpp
                                      operator/primitives.hpp
                                      operator/host.hpp
                                      operator/host_abi.h
                                      operator/host_surface.hpp
                                      operator/image.hpp
                                      operator/provider.hpp
                                      operator/provider_abi.h
                                      operator/provider_host.hpp)
# The pane protocol, spelled `#include "workshop/pane_vocabulary.hpp"` as the house does: the
# protocol, the helpers a pane draws its rows and picture in its area with, the facets a pane is
# built on (with the manual page they answer from), and the presenter seam. Nothing else under
# workshop/ is public. The guest seam beside it says what this Workshop reports of the hosts
# connected to it, so a probe on another host can ask for the inventory it is in.
set(zengine_public_headers_workshop   workshop/setup_control.hpp workshop/pane_operation.hpp workshop/pane_view.hpp
                                      workshop/pane_carry.hpp
                                      workshop/pane_settings.hpp
                                      workshop/pane_shortcuts.hpp
                                      workshop/pane_vocabulary.hpp
                                      workshop/pane_parts.hpp
                                      workshop/pane_causes.hpp
                                      workshop/pane_canvas_text.hpp
                                      workshop/pane_text_fit.hpp
                                      workshop/pane_text.hpp
                                      workshop/pane_rows.hpp
                                      workshop/pane_picture.hpp
                                      workshop/pane_drawing.hpp
                                      workshop/pane_canvas_drawing.hpp
                                      workshop/pane_document.hpp
                                      workshop/pane_facets.hpp
                                      workshop/pane_menu.hpp
                                      workshop/pane_escape.hpp
                                      workshop/presenter_vocabulary.hpp
                                      workshop/guest_seam_vocabulary.hpp)
# ...and the Neovim-backed Editor's asks, under its own directory for the same reason.
set(zengine_public_headers_editor/neovim-editor editor/neovim-editor/vocabulary.hpp)
# The graphics vocabulary an owner draws with, its roles, the elements, the two painters' code (for
# characters and for light), the two seams a skin or an element's code is written against and the
# pixel arithmetic; the windows and the directory are artifacts. Beside them, the terminal's claim
# and size.
set(zengine_public_headers_graphics graphics/vocabulary.hpp graphics/role.hpp graphics/ops.hpp
    graphics/implied.hpp graphics/painting.hpp graphics/term_paint.hpp graphics/light_paint.hpp
    graphics/canvas.hpp graphics/drawing.hpp graphics/elements.hpp graphics/element_abi.h
    graphics/element_export.hpp graphics/painter_abi.h graphics/painter_export.hpp
    graphics/pixels.hpp)
set(zengine_public_headers_graphics/window graphics/window/terminal_io.hpp)
set(zengine_public_headers_graphics/elements graphics/elements/button.hpp
    graphics/elements/checkbox.hpp graphics/elements/grid.hpp graphics/elements/icon.hpp
    graphics/elements/image.hpp graphics/elements/list.hpp graphics/elements/scroll_view.hpp
    graphics/elements/text_box.hpp)

set(zengine_public_headers_maker maker/definition.hpp maker/files.hpp maker/write.hpp
    maker/runtime.hpp maker/weave.hpp maker/vocabulary.hpp maker/succession.hpp)
set(zengine_public_headers_flow flow/native_abi.h flow/native.hpp flow/compiled.hpp
    flow/generate.hpp flow/project.hpp flow/graph.hpp flow/author.hpp flow/example.hpp
    flow/graph_edit.hpp flow/workspace.hpp flow/shape.hpp)
set(zengine_public_headers_message-draft message-draft/draft.hpp message-draft/library.hpp message-draft/transfer.hpp)
set(zengine_public_headers_inventory inventory/codec.hpp inventory/vocabulary.hpp inventory/grant.hpp)
install(FILES inventory/inventory-pane/vocabulary.hpp DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/zengine/inventory/inventory-pane)
set(zengine_public_headers_editor/source-transfer editor/source-transfer/vocabulary.hpp)
set(zengine_public_headers_flow/flow-host flow/flow-host/vocabulary.hpp)
set(zengine_public_headers_flow/flow-pane flow/flow-pane/vocabulary.hpp)
if(TARGET zengine-flow-tool)
    install(TARGETS zengine-flow-tool RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
install(TARGETS zengine-inventory-read RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
# The host an app runs in with no Workshop: it opens the artifacts below by a path relative to its
# own (app/CMakeLists.txt), so it names no machine's layout. Built where the Loom installs its
# deciding half.
if(TARGET zengine-app)
    install(TARGETS zengine-app RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

foreach(pkg IN ITEMS maker flow flow/flow-host flow/flow-pane message-draft inventory
                    editor/source-transfer activation timer surface input ui manual component
                    operator workshop editor/neovim-editor graphics graphics/elements
                    graphics/window)
    install(FILES ${zengine_public_headers_${pkg}}
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/zengine/${pkg})
endforeach()

# ---- the loadable artifacts a consumer's host opens --------------------------------------
# An artifact is the physical loadable file; weave and provider are surfaces it may expose
# (agents/packaging.md). None is linked: each is a file a host names by path, so they install as
# files, in one directory named by ZENGINE_ARTIFACT_DIR, their stems in
# ZENGINE_RUNTIME_ARTIFACTS. Only those whose runtime closure this install owns: each links the
# Loom statically and needs nothing else at load time.
set(ZENGINE_INSTALL_ARTIFACTDIR ${CMAKE_INSTALL_LIBDIR}/zengine)

# Out: the SDL weaves -- the Skin, the Input reader and the OS window (a fetched SDL is a build-tree
# library this install does not own, and shipping one would make this package another project's
# distributor); introspection and the
# composer, shown only inside a Workshop run; the snake example; every test fixture.
set(zengine_installable_artifacts
    zengine-app-weave            # weave: an app's own weave, which zengine-app runs
    zengine-timer                # weave
    zengine-input                # weave
    zengine-inventory            # weave
    zengine-skin-tui-classic     # weave
    zengine-skin-tui-block       # weave
    zengine-neovim-editor        # weave: the Editor's office held with a Neovim, and a Neovim
                                 # for a second terminal from a Loom with no Workshop; its one
                                 # runtime need beyond the Loom is a Neovim, found at run time
    zengine-operators-basic      # provider, not a weave
    zengine-guest-vocabulary     # weave for an EXTERNAL Loom host: Workshop's guest vocabulary
                                 # declared there, so a session's Python tools can speak it
    zengine-graphics             # weave: the graphics directory
    zengine-window-terminal      # weave: the window over the process's own terminal
    zengine-paint-classic        # provider, not a weave: painter code a window opens by path
    zengine-paint-block          # provider, not a weave: the same, in block glyphs
    zengine-paint-day            # provider, not a weave: painter code for light, a light ground
    zengine-paint-night)         # provider, not a weave: the same, a dark ground

set(ZENGINE_INSTALLED_ARTIFACTS "")
foreach(artifact IN LISTS zengine_installable_artifacts)
    # Gated on the target existing rather than on a platform: every one of these is built
    # only where the Loom can host loadable images, and `if(TARGET ...)` is that same honest
    # question asked once more. An install from a kernel-less Loom is a headers-only package,
    # and the config below says so out loud instead of leaving an empty directory.
    if(TARGET ${artifact})
        install(FILES $<TARGET_FILE:${artifact}> DESTINATION ${ZENGINE_INSTALL_ARTIFACTDIR})
        list(APPEND ZENGINE_INSTALLED_ARTIFACTS ${artifact})
    endif()
endforeach()

if(NOT ZENGINE_INSTALLED_ARTIFACTS)
    message(STATUS
        "zengine: no loadable artifacts to install -- this Loom cannot host them, so the "
        "package will carry public headers and exported targets only")
endif()

# ---- The Workshop tool packages, for a Loom session's run manager ------------------------------
# Python tools another Loom host's session runs against Workshop: `workshop`, and `desk`, the desk
# read and written as text (a package of its own: `workshop` holds Loom's 32 tools a package).
# DATA, not a target: a session's catalog names each directory and its operator approves it. They
# ride the condition of the vocabulary weave they need (a Loom that can host loadable weaves).
if(TARGET zengine-guest-vocabulary)
    install(DIRECTORY ${CMAKE_SOURCE_DIR}/external-host/tools/workshop
                      ${CMAKE_SOURCE_DIR}/external-host/tools/desk
            DESTINATION ${CMAKE_INSTALL_DATADIR}/zengine/loom-tools
            PATTERN "__pycache__" EXCLUDE
            PATTERN "*.pyc" EXCLUDE)
    # Each feature's tutorials (`<feature>/docs/tutorials/<name>/`) join the package's `setups/`, so
    # one starts from a prefix as a shipped setup does. A tutorial names shared files outside its
    # folder, so it is gathered by the setup's own export, which places and rewrites each of them.
    file(GLOB zengine_tutorial_setups CONFIGURE_DEPENDS
         "${CMAKE_SOURCE_DIR}/*/docs/tutorials/*/setup.json")
    set(zengine_tutorials "")
    foreach(zengine_tutorial_setup IN LISTS zengine_tutorial_setups)
        get_filename_component(zengine_tutorial "${zengine_tutorial_setup}" DIRECTORY)
        list(APPEND zengine_tutorials "${zengine_tutorial}")
    endforeach()
    if(zengine_tutorials)
        find_package(Python3 3.8 COMPONENTS Interpreter)
        if(NOT Python3_Interpreter_FOUND)
            message(STATUS "zengine: no Python 3.8 or newer was found to export the tutorials, so "
                           "the tool package will carry none (Python3_EXECUTABLE names one)")
            set(zengine_tutorials "")
        endif()
    endif()
    foreach(zengine_tutorial IN LISTS zengine_tutorials)
        get_filename_component(zengine_tutorial_name "${zengine_tutorial}" NAME)
        install(CODE "
            set(zengine_setup \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATADIR}/zengine/loom-tools/workshop/setups/${zengine_tutorial_name}\")
            message(STATUS \"Installing: \${zengine_setup}\")
            file(REMOVE_RECURSE \"\${zengine_setup}\")
            execute_process(COMMAND \"${Python3_EXECUTABLE}\" -B
                \"${CMAKE_SOURCE_DIR}/external-host/demo.py\" export --setup \"${zengine_tutorial}\"
                --to \"\${zengine_setup}\" OUTPUT_QUIET RESULT_VARIABLE zengine_exported)
            if(NOT zengine_exported EQUAL 0)
                message(FATAL_ERROR \"zengine: the tutorial ${zengine_tutorial_name} was not exported \"
                    \"into the package (exit \${zengine_exported})\")
            endif()
            file(GLOB_RECURSE zengine_setup_files \"\${zengine_setup}/*\")
            list(APPEND CMAKE_INSTALL_MANIFEST_FILES \${zengine_setup_files})")
    endforeach()
endif()

# ---- The package files -------------------------------------------------------------------
install(EXPORT zengineTargets
    FILE zengineTargets.cmake
    NAMESPACE zengine::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/zengine)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/zengineConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/zengineConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/zengine
    PATH_VARS ZENGINE_INSTALL_ARTIFACTDIR)

# SameMajorVersion, the Loom's spelling, and at 0.x it promises exactly what it says and no
# more: this is the 0.x line. The project has made no compatibility commitment yet and the
# version file is not the place to invent one -- a 1.0.0 written here to look finished would
# be a promise nothing in the repository has agreed to keep.
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/zengineConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)

# Beside them, the function a program built against the package calls to run in the UTF-8 code
# page on Windows, as every program this repository builds does, and the manifest it gives.
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/zengineConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/zengineConfigVersion.cmake
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/ZengineCodePage.cmake
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/utf8-code-page.manifest
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/zengine)

# ---- what is deliberately not in the package -----------------------------------------------
# Workshop's own vocabularies and load plan, the Builder's, and the pane weaves' header halves:
# Workshop being Workshop, which no stranger needs to offer it a pane (the pane protocol is in).
# zengine-warnings and zengine-sanitize: build discipline, PRIVATE on every target. The
# zengine-workshop and zengine-snake executables: Workshop compiles ZENGINE_BUILDER_CMAKE, the
# absolute path of the cmake that configured it, so installing it would ship a machine's layout.
