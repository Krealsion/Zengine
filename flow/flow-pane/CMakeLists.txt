# SPDX-License-Identifier: AGPL-3.0-only
# Copyright (c) 2026 Joshua DeMoss
if(NOT TARGET loom::kernel)
    return()
endif()
# Flow's own colours, a slot each, generated from slots.json as a stranger's pane generates its own
# (cmake/ZengineNames.cmake); the pane and the suites that read its look include them.
add_library(zengine-flow-slots INTERFACE)
target_link_libraries(zengine-flow-slots INTERFACE zengine-graphics-vocabulary)
zengine_names(zengine-flow-slots JSON ${CMAKE_CURRENT_SOURCE_DIR}/slots.json
              NAMESPACE zengine::flow_pane::slots HEADER flow/flow-pane/slots.hpp)
# Its manual is compiled in, and answered to whoever asks for it.
zengine_weave(zengine-flow-pane pane.cpp MANUAL ../docs/zengine.flow.md)
target_include_directories(zengine-flow-pane PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(zengine-flow-pane PRIVATE zengine-pane-vocabulary zengine-message-draft
    zengine-activation zengine-component zengine-input-vocabulary zengine-graphics-vocabulary
    zengine-flow-slots zengine-settings-vocabulary
    loom::terminal)
