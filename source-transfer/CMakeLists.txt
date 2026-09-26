# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
#
# Text and file locations as typed material: what an editor hands Inventory, and what a drop on
# an editor means. `vocabulary.hpp` (the four shapes) is exported as `zengine::source-transfer`,
# so a pane or tool outside this tree can make and recognize the same material; the conversions
# beside it are the editors' own and stay in the tree.
add_library(zengine-source-transfer-vocabulary INTERFACE)
add_library(zengine::source-transfer ALIAS zengine-source-transfer-vocabulary)
target_compile_features(zengine-source-transfer-vocabulary INTERFACE cxx_std_20)
target_include_directories(zengine-source-transfer-vocabulary INTERFACE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/zengine>)
target_link_libraries(zengine-source-transfer-vocabulary INTERFACE loom::core)

add_library(zengine-source-transfer INTERFACE)
target_include_directories(zengine-source-transfer INTERFACE ${CMAKE_SOURCE_DIR})
target_link_libraries(zengine-source-transfer INTERFACE zengine-source-transfer-vocabulary
                                                        zengine::inventory zengine-message-draft
                                                        loom::terminal)
