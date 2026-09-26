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
    zengine-neovim-editor-vocabulary)

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
                                      surface/pointing.hpp
                                      surface/terminal_size.hpp)
set(zengine_public_headers_input      input/vocabulary.hpp
                                      input/translate.hpp
                                      input/input_weave.hpp)
set(zengine_public_headers_ui         ui/vocabulary.hpp
                                      ui/layout.hpp)
set(zengine_public_headers_component  component/motion.hpp
                                      component/text_box.hpp
                                      component/list_window.hpp
                                      component/row_map.hpp
                                      component/held_choice.hpp
                                      component/control_strip.hpp
                                      component/columns.hpp)
set(zengine_public_headers_operator   operator/operator.hpp
                                      operator/catalog.hpp
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
# The pane protocol, under its own directory so a stranger spells `#include
# "workshop/pane_vocabulary.hpp"` as the house does: the protocol, the helpers a pane may use
# and the presenter seam, so a stranger can build a menu presenter as well as a pane. Nothing
# else under workshop/ is public. The guest seam beside it says what this Workshop reports of
# the hosts connected to it, so a probe on another host can ask for the inventory it is in.
set(zengine_public_headers_workshop   workshop/setup_control.hpp workshop/pane_operation.hpp workshop/pane_view.hpp
                                      workshop/pane_carry.hpp
                                      workshop/pane_shortcuts.hpp
                                      workshop/pane_vocabulary.hpp
                                      workshop/pane_canvas_vocabulary.hpp
                                      workshop/pane_canvas_text.hpp
                                      workshop/pane_menu.hpp
                                      workshop/presenter_vocabulary.hpp
                                      workshop/guest_seam_vocabulary.hpp)
# ...and the Neovim-backed Editor's asks, under its own directory for the same reason.
set(zengine_public_headers_neovim-editor neovim-editor/vocabulary.hpp)

set(zengine_public_headers_maker maker/definition.hpp maker/files.hpp maker/write.hpp
    maker/runtime.hpp maker/weave.hpp maker/vocabulary.hpp maker/succession.hpp)
set(zengine_public_headers_flow flow/native_abi.h flow/native.hpp flow/compiled.hpp
    flow/generate.hpp flow/project.hpp flow/graph.hpp flow/author.hpp flow/example.hpp
    flow/graph_edit.hpp flow/workspace.hpp)
set(zengine_public_headers_message-draft message-draft/draft.hpp message-draft/library.hpp message-draft/transfer.hpp)
set(zengine_public_headers_inventory inventory/codec.hpp inventory/vocabulary.hpp inventory/grant.hpp)
install(FILES inventory-pane/vocabulary.hpp DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/zengine/inventory-pane)
set(zengine_public_headers_source-transfer source-transfer/vocabulary.hpp)
set(zengine_public_headers_flow-host flow-host/vocabulary.hpp)
set(zengine_public_headers_flow-pane flow-pane/vocabulary.hpp)
if(TARGET zengine-flow-tool)
    install(TARGETS zengine-flow-tool RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

foreach(pkg IN ITEMS maker flow flow-host flow-pane message-draft inventory source-transfer activation timer surface input ui component operator workshop neovim-editor)
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

# Out: the SDL pair (a fetched SDL is a build-tree library this install does not own, and
# shipping one would make this package another project's distributor); introspection and the
# composer, shown only inside a Workshop run; the snake example; every test fixture.
set(zengine_installable_artifacts
    zengine-timer                # weave
    zengine-input                # weave
    zengine-inventory            # weave
    zengine-skin-tui-classic     # weave
    zengine-skin-tui-block       # weave
    zengine-neovim-editor        # weave: the Editor's office held with a Neovim, and a Neovim
                                 # for a second terminal from a Loom with no Workshop; its one
                                 # runtime need beyond the Loom is a Neovim, found at run time
    zengine-operators-basic      # provider, not a weave
    zengine-guest-vocabulary)    # weave for an EXTERNAL Loom host: Workshop's guest vocabulary
                                 # declared there, so a session's Python tools can speak it

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

# ---- The Workshop tool package, for a Loom session's run manager -------------------------------
#
# Python tools that run against a running Workshop from another Loom host's session (Loom's
# `loom-host --serve` and its run manager). DATA, not a target: a session's catalog names the
# directory, its operator approves it, and nothing here is compiled or linked. It rides the same
# condition as the vocabulary weave it needs (a Loom that can host loadable weaves).
if(TARGET zengine-guest-vocabulary)
    install(DIRECTORY ${CMAKE_SOURCE_DIR}/external-host/tools/workshop
            DESTINATION ${CMAKE_INSTALL_DATADIR}/zengine/loom-tools
            PATTERN "__pycache__" EXCLUDE
            PATTERN "*.pyc" EXCLUDE)
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

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/zengineConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/zengineConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/zengine)

# ---- what is deliberately not in the package -----------------------------------------------
# Workshop's own vocabularies and load plan, the Builder's, and the pane weaves' header halves:
# Workshop being Workshop, which no stranger needs to offer it a pane (the pane protocol is in).
# zengine-warnings and zengine-sanitize: build discipline, PRIVATE on every target. The
# zengine-workshop and zengine-snake executables: Workshop compiles ZENGINE_BUILDER_CMAKE, the
# absolute path of the cmake that configured it, so installing it would ship a machine's layout.
