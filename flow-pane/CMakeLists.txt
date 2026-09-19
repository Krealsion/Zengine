# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
if(NOT TARGET loom::kernel)
    return()
endif()
zengine_weave(zengine-flow-pane pane.cpp)
target_include_directories(zengine-flow-pane PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(zengine-flow-pane PRIVATE zengine-pane-vocabulary zengine-message-draft
    zengine-activation zengine-component zengine-input-vocabulary loom::terminal)
