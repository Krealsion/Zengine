# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
add_library(zengine-inventory INTERFACE)
add_library(zengine::inventory ALIAS zengine-inventory)
target_compile_features(zengine-inventory INTERFACE cxx_std_20)
target_include_directories(zengine-inventory INTERFACE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/zengine>)
target_link_libraries(zengine-inventory INTERFACE loom::core)
