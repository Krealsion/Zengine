# SPDX-License-Identifier: AGPL-3.0-only
# Copyright (c) 2026 Joshua DeMoss

# The desk's geometry and its gestures' transitions, as a library: functions of their arguments,
# holding no state and no authority, so a host keeps only the wiring and this package's own suite
# (`desk_gestures`) proves the arithmetic with no host built. Its link line is what its headers
# include: the cell lattice (`graphics/pixels.hpp`) and which medium a pointer reported in
# (`input/vocabulary.hpp`).
add_library(zengine-desk-gestures STATIC gestures.cpp)
add_library(zengine::desk-gestures ALIAS zengine-desk-gestures)
target_compile_features(zengine-desk-gestures PUBLIC cxx_std_20)
target_include_directories(zengine-desk-gestures PUBLIC $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>)
set_target_properties(zengine-desk-gestures PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_link_libraries(zengine-desk-gestures PUBLIC zengine::graphics zengine::input
                                            PRIVATE zengine-warnings zengine-sanitize)
