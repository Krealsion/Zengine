# SPDX-License-Identifier: AGPL-3.0-only
# Copyright (c) 2026 Joshua DeMoss
if(NOT TARGET loom::kernel)
    return()
endif()
zengine_weave(zengine-demo-control control.cpp)
target_link_libraries(zengine-demo-control PRIVATE zengine-pane-vocabulary zengine-activation
    zengine-input-vocabulary zengine-warnings zengine-sanitize)
