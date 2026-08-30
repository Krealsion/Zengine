# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
#
# ZENGINE AS AN INSTALLABLE CMAKE PACKAGE (PKG-0).
#
# One file, holding the whole public consumer surface, because that surface is a contract and
# a contract should be readable in one sitting. Everything an unrelated project can see of
# this repository is decided here: which targets it may link, which headers it receives, which
# loadable artifacts arrive beside them, and how it finds the Loom underneath.
#
# WHAT THE PACKAGE IS FOR. Before this file, Zengine shipped no install() rule at all: an
# external project reached the vocabularies by pointing an include directory at a Zengine
# SOURCE tree and took its `.so`/`.dll` artifacts out of a Zengine BUILD tree. That works and
# it is not a package -- the consumer has to know where two directories on somebody else's
# machine are, and neither of them is stable. This repository already refuses that arrangement
# from the other side: it consumes the Loom through find_package by default, precisely so an
# unexported surface fails everywhere rather than only for guests. Asking of a guest what the
# house declines to accept from its own dependency is the asymmetry this closes.
#
# THE SHAPE FOLLOWS THE LOOM'S, deliberately (lowercase package name, `zengine::` namespace,
# lib/cmake/<name>/, configure_package_config_file + write_basic_package_version_file,
# EXPORT_NAME set to the in-tree ALIAS spelling). A second house style would be one more thing
# for a consumer of both to hold, and there is no way in which Zengine's topology needs one.

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

# A DEV-MODE BUILD MAY NOT PRODUCE A PACKAGE, and this refuses rather than warns.
#
# With ZEN_LOOM_DEV=ON the Loom arrives by add_subdirectory, so `loom::core` is an in-tree
# target rather than an imported one. Exporting Zengine's targets from that build would write
# `loom::core` into zengineTargets.cmake with nothing to resolve it, and zengineConfig.cmake's
# find_dependency(loom) would look for an installed Loom package the build never used. The
# result configures on a machine that happens to have one installed and fails on every other,
# which is the worst of the available failures: it looks like a working package.
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

# ---- The exported targets, and what linking each one grants ------------------------------
#
# The test every row below had to pass is "what user-facing capability does linking this
# grant" -- not "does something else in the tree need it". Eight passed. What did NOT is at
# the bottom of this file, with the reason, because a boundary that only records its inside
# is half a boundary.
#
#   zengine::activation         read your own zen.Activated as a cursor -- lineage and
#                               deduplication -- so a weave can tell its own first breath
#                               from a later one. Every package below reads one.
#   zengine::timer              speak the Timer protocol (the locked shapes and the
#                               zengine.timer role), and declare an authored rhythm with
#                               the TimedWeave layer.
#   zengine::surface            publish visual intent, and read the geometry vocabulary a
#                               skin resolves it against: cells, regions, pointing, and the
#                               terminal's own size.
#   zengine::input              receive the locked input shapes, translate a raw byte
#                               stream into them, and build a reader on the Input layer.
#   zengine::ui                 author placement and extent, and read what a viewport
#                               resolved -- the one distinction that package owns.
#   zengine::component          a medium-independent editable text box: text, caret,
#                               character-safe edits, a horizontal window.
#   zengine::operator           hold and evaluate named typed operators, mount a provider
#                               image, and dress a catalog for an artifact that has none.
#   zengine::operator-consumer  spend a host's operator truth from inside a loaded image --
#                               the C table and the handle, and deliberately no catalog.
#
# EXPORT_NAME is what makes `zengine::surface` mean the same thing from this tree and from an
# installed prefix. Without it the house would link `zengine-surface-vocabulary` and a guest
# would link something else, and the two could quietly come apart.
set(ZENGINE_EXPORTED_TARGETS
    zengine-activation
    zengine-timer-vocabulary
    zengine-surface-vocabulary
    zengine-input-vocabulary
    zengine-ui-vocabulary
    zengine-component
    zengine-operator
    zengine-operator-consumer)

set_target_properties(zengine-activation          PROPERTIES EXPORT_NAME activation)
set_target_properties(zengine-timer-vocabulary    PROPERTIES EXPORT_NAME timer)
set_target_properties(zengine-surface-vocabulary  PROPERTIES EXPORT_NAME surface)
set_target_properties(zengine-input-vocabulary    PROPERTIES EXPORT_NAME input)
set_target_properties(zengine-ui-vocabulary       PROPERTIES EXPORT_NAME ui)
set_target_properties(zengine-component           PROPERTIES EXPORT_NAME component)
set_target_properties(zengine-operator            PROPERTIES EXPORT_NAME operator)
set_target_properties(zengine-operator-consumer   PROPERTIES EXPORT_NAME operator-consumer)

# Every one of them is an INTERFACE target, so nothing is installed here but the target
# definitions themselves; the headers go below and there is no library to place.
install(TARGETS ${ZENGINE_EXPORTED_TARGETS} EXPORT zengineTargets)

# ---- The public headers, named one at a time ---------------------------------------------
#
# EXPLICIT LISTS RATHER THAN install(DIRECTORY <pkg>/ FILES_MATCHING PATTERN "*.hpp"), and the
# difference is not tidiness. A package directory holds its implementation beside its
# vocabulary, and a pattern would ship both: `surface/skin.hpp` and `skin_tui.hpp` are how the
# shipped skins are built, they include `snake/vocabulary.hpp` -- a game's shapes -- and
# installing them would either break the install tree's own header closure or drag the demo
# in behind it. `input/translate_sdl.hpp` would arrive without SDL. `timer/timer_weave.hpp`
# and `timer/normalize.hpp` are the Timer SERVICE, and shipping them would advertise "write
# your own Timer service" as a supported external path that nothing has yet measured.
#
# So each package names what it publishes, the destination keeps the package directory, and
# the include root is <prefix>/include/zengine -- which is what makes the documented spelling
# `#include "timer/vocabulary.hpp"` the same sentence in this tree and out of it.
#
# The closure was checked rather than assumed: every quoted include in the set below resolves
# inside the set, and tests/package is the witness that says so on every run.
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
set(zengine_public_headers_component  component/text_box.hpp)
set(zengine_public_headers_operator   operator/operator.hpp
                                      operator/catalog.hpp
                                      operator/source.hpp
                                      operator/primitives.hpp
                                      operator/host.hpp
                                      operator/host_abi.h
                                      operator/host_surface.hpp
                                      operator/image.hpp
                                      operator/provider.hpp
                                      operator/provider_abi.h
                                      operator/provider_host.hpp)

foreach(pkg IN ITEMS activation timer surface input ui component operator)
    install(FILES ${zengine_public_headers_${pkg}}
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/zengine/${pkg})
endforeach()

# ---- The loadable artifacts a consumer's host actually opens -----------------------------
#
# ARTIFACT IS THE NOUN, and it is the only one that is true of all of them (QR-5). An artifact
# is the physical loadable unit -- one file on disk. WEAVE and PROVIDER are runtime SURFACES an
# artifact may expose, and they are not the same claim: four of the five below are weaves the
# Kernel loads into the bus, and `zengine-operators-basic` is a PROVIDER -- opened directly by a
# host, with no WeaveId, role, grant or manifest, and no participant in it at all (PROV-0, and
# `zengine_provider()` in the top-level CMakeLists is where the difference is enforced). A
# future artifact may expose both surfaces, or a third nobody has written yet.
#
# None of them is a library a consumer links: each is a FILE a host names by path and the loader
# opens. So they install as files rather than as exported targets -- an imported target for one
# would offer a link line that must never be written, and on Windows would ask for an import
# library nothing should ever consume.
#
# They land in one directory, spelled the same on every platform, and the package config hands
# a consumer both that directory (ZENGINE_ARTIFACT_DIR) and the stems that are in it
# (ZENGINE_RUNTIME_ARTIFACTS). Hand-copying an artifact out of a build tree is the thing this
# replaces.
#
# WHICH ONES. The artifacts whose runtime closure this install actually owns. Each links the
# Loom statically and needs nothing else at load time, so a copied prefix keeps working.
#
# NOT the SDL-backed pair (zengine-skin-sdl, zengine-input-sdl). When no SDL3 is installed
# this build FETCHES one and links it as a build-tree library; installing the weave without
# it would ship an artifact that cannot load, and installing a fetched SDL beside it would
# make this package a distributor of somebody else's. Truthfully absent beats quietly broken.
#
# NOT zengine-introspection or zengine-composer: both exist to be shown INSIDE a Workshop run
# and there is no external-pane installation story to arrive through yet. NOT the snake
# artifacts (a worked example, built for this tree's own suites), and NOT anything under
# tests/ -- the virtual Timers, the probes and the provider fixtures are evidence, and a
# fixture installed as production content is a lie about what this package ships.
set(ZENGINE_INSTALL_ARTIFACTDIR ${CMAKE_INSTALL_LIBDIR}/zengine)

set(zengine_installable_artifacts
    zengine-timer                # weave
    zengine-input                # weave
    zengine-skin-tui-classic     # weave
    zengine-skin-tui-block       # weave
    zengine-operators-basic)     # provider, not a weave (PROV-0)

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

# ---- What is deliberately NOT in the package, and why -------------------------------------
#
# zengine-workshop-vocabulary, zengine-workshop-load
#     Workshop's own surface. An office-authored external pane is a real seam and it is
#     documented, but the guide states in its own words that no installation or plugin path
#     exists for one yet -- so exporting the headers would advertise a road with no end.
#     It joins the package on the day a pane can arrive through one.
#
# zengine-builder-vocabulary
#     The Builder package ships no artifact of its own and its one consumer is the Workshop
#     host that compiles it in. Nothing outside can spend it without also being Workshop.
#
# zengine-introspection-view, zengine-composer-vocabulary, zengine-composer-draft
#     The header halves of two Workshop panes, and they follow their weaves.
#
# zengine-warnings, zengine-sanitize
#     This repository's build discipline, not a capability. They are PRIVATE on every target
#     that takes them, so they ride in nobody's link interface and a consumer never meets
#     them. (The Loom exports its equivalents because ITS static libraries carry them
#     transitively; Zengine's do not, and copying the decision would have been ceremony.)
#
# zengine-workshop, zengine-snake
#     Executables. Workshop in particular compiles ZENGINE_BUILDER_BUILD_DIR -- an absolute
#     path into the build tree that produced it -- into the binary for its Builder panel, so
#     installing it today would ship a developer machine's directory layout inside a public
#     artifact. That is a Workshop repair, not a packaging one.
