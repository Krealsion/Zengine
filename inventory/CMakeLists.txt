# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
add_library(zengine-inventory-vocabulary INTERFACE)
add_library(zengine::inventory ALIAS zengine-inventory-vocabulary)
target_compile_features(zengine-inventory-vocabulary INTERFACE cxx_std_20)
target_include_directories(zengine-inventory-vocabulary INTERFACE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/zengine>)
target_link_libraries(zengine-inventory-vocabulary INTERFACE loom::core)

add_executable(zengine-inventory-read read.cpp)
target_link_libraries(zengine-inventory-read PRIVATE zengine::inventory zengine-warnings zengine-sanitize)
install(TARGETS zengine-inventory-read RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if(TARGET loom::kernel)
    zengine_weave(zengine-inventory inventory.cpp)
    target_link_libraries(zengine-inventory PRIVATE zengine::inventory)
endif()
