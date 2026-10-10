# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
if(NOT TARGET loom::kernel)
    return()
endif()
zengine_weave(zengine-demo-control control.cpp)
target_link_libraries(zengine-demo-control PRIVATE zengine-pane-vocabulary zengine-activation
    zengine-warnings zengine-sanitize)
