# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
if(TARGET loom::kernel)
    # Its manual is compiled in, and answered to whoever asks for it.
    zengine_weave(zengine-inventory-pane pane.cpp MANUAL ../docs/zengine.inventory-pane.md)
    target_link_libraries(zengine-inventory-pane PRIVATE zengine-workshop-vocabulary
        zengine::inventory zengine-activation zengine-input-vocabulary)
endif()
