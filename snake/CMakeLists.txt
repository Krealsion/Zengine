# The snake package — the first game panel: a World, two Drawers, a Score
# weave (each a real .so the kernel loads, unloads, and swaps), and the
# playable host that drives the three live-evolution moments.
#
# Everything here needs a Loom install that can HOST .so weaves; the honest
# question is "does this install export loom::kernel?" (it does on Linux, and
# deliberately does not on Windows — the kernel is dlopen/POSIX ground).
if(NOT TARGET loom::kernel)
    message(STATUS "zengine: snake package skipped — this Loom cannot host .so weaves (no loom::kernel)")
    return()
endif()

# A loadable weave: PIC shared library, exact filename (no 'lib' prefix), linked
# against the exported substrate. export.hpp is header-only, so a weave library
# needs only core + switchboard — the kernel target is the HOST'S dependency.
#
# -fno-gnu-unique is LOAD-BEARING, not hygiene. The maker path (ZEN_SHAPE +
# WeaveBase) instantiates loom's inline templates — schema_of<T>'s function-local
# statics — with vague linkage, which GCC emits as STB_GNU_UNIQUE symbols. glibc
# resolves unique symbols through a PROGRAM-WIDE table that ignores RTLD_LOCAL
# and can outlive a dlclose'd library, so the second .so sharing a vocabulary
# header silently aliased the FIRST one's destroyed statics: swapping the classic
# drawer for the block drawer crashed in describe() with a use-after-free into
# the unloaded library's heap (ASan: freed by the dlclose'd .so's static
# destructors). The Loom's own fixtures never hit this because their schemas are
# hand-built in anonymous namespaces (internal linkage is never unique); these
# are the first weave libraries built the maker way. -fno-gnu-unique demotes the
# statics to ordinary vague linkage, restoring what RTLD_LOCAL already promises:
# each library owns its own statics, whole lives, no sharing. GCC-only flag;
# pinned by the unload-reload regression cases in the snake suite.
function(snake_weave target source)
    add_library(${target} SHARED ${source})
    target_link_libraries(${target} PRIVATE loom::core loom::switchboard zengine-warnings)
    target_compile_options(${target} PRIVATE $<$<CXX_COMPILER_ID:GNU>:-fno-gnu-unique>)
    set_target_properties(${target} PROPERTIES PREFIX "" OUTPUT_NAME ${target})
endfunction()

snake_weave(snake-world-v1 world.cpp)
snake_weave(snake-world-v2 world.cpp)
target_compile_definitions(snake-world-v2 PRIVATE SNAKE_WORLD_V2)
snake_weave(snake-drawer-classic drawer_classic.cpp)
snake_weave(snake-drawer-block drawer_block.cpp)
snake_weave(snake-score score.cpp)

# The playable host. It resolves the .so files beside its own binary, so the
# weaves' runtime output directory is pinned to the host's.
add_executable(zengine-snake play.cpp)
target_link_libraries(zengine-snake
    PRIVATE loom::kernel loom::switchboard loom::core zengine-warnings)
set_target_properties(zengine-snake snake-world-v1 snake-world-v2 snake-drawer-classic
                      snake-drawer-block snake-score
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
