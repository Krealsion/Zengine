# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
add_library(zengine-message-draft INTERFACE)
add_library(zengine::message-draft ALIAS zengine-message-draft)
target_compile_features(zengine-message-draft INTERFACE cxx_std_20)
target_include_directories(zengine-message-draft INTERFACE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/zengine>)
target_link_libraries(zengine-message-draft INTERFACE loom::core loom::terminal)
