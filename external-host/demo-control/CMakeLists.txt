# SPDX-License-Identifier: AGPL-3.0-only
# Copyright (c) 2026 Joshua DeMoss

# Workshop's own warnings and sanitizer, which its weaves here take (workshop/discipline.cmake).
include(${CMAKE_CURRENT_LIST_DIR}/../../workshop/discipline.cmake)
if(NOT TARGET loom::kernel)
    return()
endif()
# Its manual is compiled in: the pane's, and its role's shapes, answered to whoever asks.
zengine_weave(zengine-demo-control control.cpp MANUAL ../docs/zengine.demo.md)
target_link_libraries(zengine-demo-control PRIVATE zengine::pane zengine::activation
    zengine::input workshop-warnings workshop-sanitize)
