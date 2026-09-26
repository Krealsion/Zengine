# Workshop, seen from another Loom host: `zengine-guest-vocabulary`, a loadable weave the
# external host boots to declare Workshop's guest-facing vocabulary (vocabulary_weave.hpp), and
# `tools/workshop`, a Python tool package for a Loom session's run manager
# (tools/workshop/loom-tool.json). Both are installed (lib/zengine, share/zengine/loom-tools);
# Loom's session tooling runs them. The artifact needs a Loom that can host loadable weaves.

if(NOT TARGET loom::kernel)
    message(STATUS "zengine: guest vocabulary weave skipped -- this Loom cannot host loadable weaves (no loom::kernel)")
    return()
endif()

zengine_weave(zengine-guest-vocabulary vocabulary_weave.cpp)
target_link_libraries(zengine-guest-vocabulary
    PRIVATE zengine-input-vocabulary zengine-surface-vocabulary zengine::inventory
            zengine-builder-vocabulary)
target_include_directories(zengine-guest-vocabulary PRIVATE ${CMAKE_SOURCE_DIR})

# Built without the sanitizer: its host is an installed Loom session host, uninstrumented, and an
# instrumented image cannot load into an uninstrumented process -- under ZENGINE_SANITIZE the
# sanitizer lane would have to shut the `session` gate. It only registers shapes, so it gives up
# little; Workshop, which that host drives, stays instrumented.
get_target_property(zengine_guest_links zengine-guest-vocabulary LINK_LIBRARIES)
list(REMOVE_ITEM zengine_guest_links zengine-sanitize)
set_target_properties(zengine-guest-vocabulary PROPERTIES LINK_LIBRARIES "${zengine_guest_links}")
