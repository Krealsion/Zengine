# SPDX-License-Identifier: AGPL-3.0-only
# Copyright (c) 2026 Joshua DeMoss
# The Notices pane, which walks the notices record, as a loadable weave.
if(TARGET loom::kernel)
    # Its manual is compiled in, and answered to whoever asks for it.
    zengine_weave(zengine-notices-pane pane.cpp MANUAL ../docs/zengine.notices-pane.md)
    target_link_libraries(zengine-notices-pane PRIVATE zengine-workshop-vocabulary
        zengine-activation zengine-input-vocabulary)
endif()
