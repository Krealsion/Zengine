# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
if(NOT TARGET loom::kernel)
    return()
endif()
# Its manual is compiled in: the pane's, and its office's shapes, answered to whoever asks.
zengine_weave(zengine-demo-control control.cpp MANUAL ../docs/zengine.demo.md)
target_link_libraries(zengine-demo-control PRIVATE zengine-pane-vocabulary zengine-activation
    zengine-input-vocabulary zengine-warnings zengine-sanitize)
