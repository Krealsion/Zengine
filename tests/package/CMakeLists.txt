# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
#
# The stranger: an unrelated CMake project that consumes Zengine as an installed package. What is
# not in it is the evidence: no path into a Zengine source or build tree, no named .so or .dll, no
# compile option or C++ standard borrowed from the library, no find_package(loom). If one becomes
# necessary the package has stopped carrying its requirements, and this goes red. run.cmake copies
# this directory outside both repositories before configuring it.

cmake_minimum_required(VERSION 3.16)
project(kitchen LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(zengine 0.1 CONFIG REQUIRED)

# ---- every exported target, compiled and run -------------------------------------------
add_executable(witness-surfaces public_surface.cpp)
target_link_libraries(witness-surfaces
    PRIVATE
        zengine::inventory
        zengine::activation
        zengine::maker
        zengine::flow
        zengine::timer
        zengine::surface
        zengine::input
        zengine::ui
        zengine::component
        zengine::operator
        zengine::operator-consumer
        zengine::pane
        zengine::neovim
        zengine::source-transfer)

# ---- a stranger uses the pane menu, the presenter seam and the list helpers, live ---------
# pane_menu_consumer.cpp runs the protocol public_surface.cpp only includes, from `zengine::pane`,
# `zengine::component` and the Loom bus alone, out of tree: a helper the package stopped
# installing, or one that stopped working against the installed headers, fails here.
add_executable(witness-pane-menu pane_menu_consumer.cpp)
target_link_libraries(witness-pane-menu PRIVATE zengine::pane zengine::component loom::switchboard)

# ---- one capability, end to end: a weave that orders time ------------------------------
#
# loom_weave_build_contract() arrives with the Loom package, which zengineConfig.cmake found
# for us. It is not optional and forgetting it is silent -- it applies whatever this platform
# needs for a loadable image's statics to die with the image.
add_library(oven SHARED oven.cpp)
target_link_libraries(oven PRIVATE zengine::timer loom::switchboard)
loom_weave_build_contract(oven)
set_target_properties(oven PROPERTIES PREFIX "" OUTPUT_NAME oven)

add_executable(kitchen-host kitchen_host.cpp)
target_link_libraries(kitchen-host PRIVATE zengine::timer loom::switchboard loom::kernel)

# The Timer SERVICE is a Zengine artifact, and this is where it comes from: the package says
# where its own artifacts live, so nothing here reaches into a build tree to fetch one.
if(NOT ZENGINE_ARTIFACT_DIR)
    message(FATAL_ERROR
        "kitchen: this zengine package carries no loadable artifacts "
        "(ZENGINE_RUNTIME_ARTIFACTS is empty), so there is no Timer service to bake with. It "
        "was installed from a build against a Loom with no kernel.")
endif()
add_custom_command(TARGET kitchen-host POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${ZENGINE_ARTIFACT_DIR}/zengine-timer${CMAKE_SHARED_LIBRARY_SUFFIX}"
            "$<TARGET_FILE_DIR:kitchen-host>"
    COMMENT "kitchen: taking the Timer service from the installed zengine package")
add_dependencies(kitchen-host oven)

# ---- what the artifact list is, from outside ---------------------------------------------
# The list is heterogeneous: `zengine-timer` is a weave the host hands to the Kernel, and
# `zengine-operators-basic` a provider it opens directly, with no participant in it. So the only
# noun true of every member is `artifact`, and a variable renamed after one surface goes red here.
foreach(expected zengine-timer zengine-operators-basic zengine-neovim-editor
                 zengine-guest-vocabulary zengine-inventory)
    if(NOT expected IN_LIST ZENGINE_RUNTIME_ARTIFACTS)
        message(FATAL_ERROR
            "kitchen: '${expected}' is not in ZENGINE_RUNTIME_ARTIFACTS "
            "(${ZENGINE_RUNTIME_ARTIFACTS}). The package's advertised artifacts are not the "
            "ones it was supposed to install.")
    endif()
    if(NOT EXISTS "${ZENGINE_ARTIFACT_DIR}/${expected}${CMAKE_SHARED_LIBRARY_SUFFIX}")
        message(FATAL_ERROR
            "kitchen: the package advertises '${expected}' but there is no such file in "
            "ZENGINE_ARTIFACT_DIR (${ZENGINE_ARTIFACT_DIR}). A list of stems that names a "
            "file nobody installed is worse than an empty one.")
    endif()
endforeach()
message(STATUS
    "kitchen: the installed package advertises ${ZENGINE_RUNTIME_ARTIFACTS} in "
    "${ZENGINE_ARTIFACT_DIR} -- weaves and a provider in one list, which is why they are "
    "called artifacts")

# Flow authored, compiled and loaded entirely against the installed package.
add_executable(flow-author flow_author.cpp)
target_link_libraries(flow-author PRIVATE zengine::flow zengine::maker)
set(flow_source "${CMAKE_CURRENT_BINARY_DIR}/flow-generated.cpp")
add_custom_command(OUTPUT "${flow_source}"
    COMMAND $<TARGET_FILE:flow-author> "${flow_source}"
    DEPENDS flow-author VERBATIM)
add_library(flow-native SHARED "${flow_source}")
target_link_libraries(flow-native PRIVATE zengine::flow loom::core loom::switchboard)
loom_weave_build_contract(flow-native)
set_target_properties(flow-native PROPERTIES PREFIX "")
add_executable(flow-host flow_host.cpp)
target_link_libraries(flow-host PRIVATE zengine::flow loom::kernel)
add_dependencies(flow-host flow-native)

add_executable(witness-message-drafts message_draft_consumer.cpp)
target_link_libraries(witness-message-drafts PRIVATE zengine::message-draft zengine::flow zengine::pane)
