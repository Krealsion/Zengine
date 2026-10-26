# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
# The notices record: what Workshop said, kept for the run, as a loadable weave. It needs a Loom
# that can host loadable weaves; the pane that walks it is `notices-pane/`.
if(NOT TARGET loom::kernel)
    message(STATUS "zengine: notices weave skipped — this Loom cannot host loadable weaves (no loom::kernel)")
    return()
endif()

# Its manual is compiled in: the office answers a shape's section from this page
# (`ShapeDocumentRequested`, manual/vocabulary.hpp).
zengine_weave(zengine-notices notices.cpp MANUAL docs/zengine.notices.md)
target_link_libraries(zengine-notices PRIVATE zengine-workshop-vocabulary zengine-activation)
