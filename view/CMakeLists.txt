# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
# The view package: a view described as data, the renderer that draws it on its pane's canvas and
# in a graphics window, and the host that registers each view as its own participant
# (view/docs/view.md). Header-only and internal: Workshop composes the host, and the View Builder
# authors descriptions.
add_library(zengine-view INTERFACE)
target_include_directories(zengine-view INTERFACE $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>)
target_compile_features(zengine-view INTERFACE cxx_std_20)
target_link_libraries(zengine-view INTERFACE zengine-maker zengine-pane-vocabulary
    zengine-component zengine-input-vocabulary
    zengine-graphics-vocabulary loom::core loom::switchboard)
# ...and the view host's manual, compiled into whatever mounts the host: its office answers a
# shape's section from this page (`ShapeDocumentRequested`, manual/vocabulary.hpp).
zengine_manual(zengine-view INTERFACE docs/zengine.view.host.md)
