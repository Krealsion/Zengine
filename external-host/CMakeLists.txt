# Workshop, seen from ANOTHER Loom host -- the external host's side of the crossing.
#
#   zengine-guest-vocabulary   a loadable weave the external host boots: Workshop's guest-facing
#                              vocabulary, declared there so a Python tool's asks can be encoded
#                              and Workshop's answers re-admitted (vocabulary_weave.hpp)
#   tools/workshop             a tool package for a Loom session's run manager: journeys against
#                              a running Workshop, written in Python (tools/workshop/loom-tool.json)
#
# Both are installed: the artifact with the others in lib/zengine, the package in
# share/zengine/loom-tools/workshop (cmake/ZengineInstall.cmake). Loom's session tooling runs
# them; this repository ships nothing of the session itself.
#
# THE ARTIFACT NEEDS A LOOM THAT CAN HOST LOADABLE WEAVES, like every weave here.

if(NOT TARGET loom::kernel)
    message(STATUS "zengine: guest vocabulary weave skipped -- this Loom cannot host loadable weaves (no loom::kernel)")
    return()
endif()

zengine_weave(zengine-guest-vocabulary vocabulary_weave.cpp)
target_link_libraries(zengine-guest-vocabulary
    PRIVATE zengine-input-vocabulary zengine-surface-vocabulary)
target_include_directories(zengine-guest-vocabulary PRIVATE ${CMAKE_SOURCE_DIR})

# BUILT WITHOUT THE SANITIZER -- the one weave here that is, because its host is not this
# repository's. An INSTALLED Loom session host boots it, uninstrumented, and an instrumented image
# cannot load into an uninstrumented process: under ZENGINE_SANITIZE it would stop loading where
# it is used, and the sanitizer lane would have to shut the `session` gate to buy instrumentation
# (tests/CMakeLists.txt). What it gives up is instrumenting declarations: it accepts nothing, says
# nothing and runs nothing of its own beyond registering shapes. Workshop, on the far side of that
# journey, stays instrumented.
get_target_property(zengine_guest_links zengine-guest-vocabulary LINK_LIBRARIES)
list(REMOVE_ITEM zengine_guest_links zengine-sanitize)
set_target_properties(zengine-guest-vocabulary PROPERTIES LINK_LIBRARIES "${zengine_guest_links}")
