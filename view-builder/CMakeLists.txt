# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
# The View Builder: a pane beside Flow where a view is made by hand and run through the view host
# (docs/workshop/view-builder.md). A weave loaded by a plan row, like the Flow pane.
if(NOT TARGET loom::kernel)
    return()
endif()
zengine_weave(zengine-view-builder pane.cpp)
target_include_directories(zengine-view-builder PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(zengine-view-builder PRIVATE zengine-view zengine-flow zengine-pane-vocabulary
    zengine-message-draft zengine-activation zengine-component zengine-input-vocabulary loom::terminal)
