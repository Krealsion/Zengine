# SPDX-License-Identifier: AGPL-3.0-only
# Copyright (c) 2026 Joshua DeMoss
if(TARGET loom::kernel)
    zengine_weave(zengine-inventory-pane pane.cpp)
    target_link_libraries(zengine-inventory-pane PRIVATE zengine-workshop-vocabulary
        zengine::inventory zengine-activation zengine-input-vocabulary)
endif()
