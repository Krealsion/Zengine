cmake_minimum_required(VERSION 3.16)

project(zengine
    VERSION 0.1.0
    DESCRIPTION "Zen's default set of weaves — the Loom's first external consumer"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---- How this repo gets the Loom ------------------------------------------------------
#
# Two paths, same target names either way (the Loom's export sets EXPORT_NAME to match its
# in-tree ALIASes — loom::core, loom::switchboard, loom::kernel — so the override is a
# genuine drop-in and the two paths cannot silently come to mean different things):
#
#   ZEN_LOOM_DEV=ON  (the default) — the SIBLING path: add_subdirectory(../Loom). The
#       working-tree default, and the turnkey Windows path: configure, press play.
#   ZEN_LOOM_DEV=OFF — the STRANGER'S path: find_package() against an installed, exported
#       Loom, consuming it exactly as a third party would.
#
# The stranger's path remains the PROOF lane even though it is no longer the default: the
# WSL verification lanes (`build`, `build-san`) are configured with it deliberately, so a
# mistaken dependency on an unexported target still breaks here, in the house, before it
# breaks for a guest. Dev mode makes the WHOLE Loom build tree reachable — convenient for
# editing the two together, silent about export mistakes — which is exactly why the proof
# lanes stay on the other path. (The default was flipped from OFF once the exported surface
# had a real consumer suite pinning it; the discipline moved from the default into the lanes.)

option(ZEN_LOOM_DEV
       "Consume the Loom from the sibling ../Loom source tree instead of an installed package"
       ON)

if(ZEN_LOOM_DEV)
    message(STATUS "zengine: Loom from sibling source ../Loom  [DEV OVERRIDE]")
    # A dependency's proof rides its version: Zengine's lane does not re-run the Loom's suite,
    # so the sibling build contributes its libraries and nothing else. ZEN_SDL is silenced for
    # the same reason — the SDL skin is Zengine-destined, not something this build needs today.
    set(ZEN_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
    set(ZEN_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(ZEN_SDL            OFF CACHE BOOL "" FORCE)
    if(WIN32)
        # The demos need the kernel, and on Windows the kernel exists only as the Loom's
        # explicit development/demo backend (no isolation — the Loom prints its banner and
        # Kernel::containment_note() says so at every surface). Dev mode on Windows IS the
        # demo workflow, so default the option ON here — a plain CACHE set, so an explicit
        # -DLOOM_ENABLE_WINDOWS_KERNEL=OFF on the command line still wins.
        set(LOOM_ENABLE_WINDOWS_KERNEL ON CACHE BOOL
            "Build the Windows kernel backend (LoadLibrary). Development/demo only — no isolation.")
    endif()
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../Loom ${CMAKE_CURRENT_BINARY_DIR}/_loom)
else()
    find_package(loom 0.1 REQUIRED)
    message(STATUS "zengine: Loom from installed package at ${loom_DIR}")
endif()

# ---- House warnings for Zengine's own sources ------------------------------------------
#
# The Loom's exported loom::warnings is deliberately $<LINK_ONLY:> so its -Werror never
# leaks onto a consumer's sources; a consumer that wants the same discipline declares its
# own. Zengine does — the same list, the same -Werror, applied to every target this repo
# authors. (GNU/Clang only: those are the lanes that exist here — WSL/GCC and MinGW.)
add_library(zengine-warnings INTERFACE)
target_compile_options(zengine-warnings INTERFACE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic -Wshadow -Wconversion
    -Wsign-conversion -Werror>)

# ---- The loadable-weave pattern --------------------------------------------------------
#
# A loadable weave: PIC shared library, exact filename (no 'lib' prefix), linked against
# the exported substrate. export.hpp is header-only, so a weave library needs only core +
# switchboard — the kernel target is the HOST'S dependency. Every package's weaves are
# built through this one function so the flag below is a law, not a convention.
#
# -fno-gnu-unique is LOAD-BEARING, not hygiene. The maker path (ZEN_SHAPE + WeaveBase)
# instantiates loom's inline templates — schema_of<T>'s function-local statics — with
# vague linkage, which GCC emits as STB_GNU_UNIQUE symbols. glibc resolves unique symbols
# through a PROGRAM-WIDE table that ignores RTLD_LOCAL and can outlive a dlclose'd
# library, so the second .so sharing a vocabulary header silently aliased the FIRST one's
# destroyed statics: swapping the classic drawer for the block drawer crashed in
# describe() with a use-after-free into the unloaded library's heap (ASan: freed by the
# dlclose'd .so's static destructors). The Loom's own fixtures never hit this because
# their schemas are hand-built in anonymous namespaces (internal linkage is never
# unique); snake's were the first weave libraries built the maker way. -fno-gnu-unique
# demotes the statics to ordinary vague linkage, restoring what RTLD_LOCAL already
# promises: each library owns its own statics, whole lives, no sharing. GCC-only flag;
# pinned by the unload-reload regression cases in the snake suite.
function(zengine_weave target source)
    add_library(${target} SHARED ${source})
    target_link_libraries(${target} PRIVATE loom::core loom::switchboard zengine-warnings)
    target_compile_options(${target} PRIVATE $<$<CXX_COMPILER_ID:GNU>:-fno-gnu-unique>)
    set_target_properties(${target} PROPERTIES PREFIX "" OUTPUT_NAME ${target})
endfunction()

# ---- The consumers --------------------------------------------------------------------
#
# smoke: the minimal stranger (links only the exported surface; its refusal test is the
# proof it is real). input: the Input package — the floor games sit on (the sole producer
# of the locked input shapes). snake: the first game panel — the Stage 2 vertical slice,
# and the hosting consumer that pulled loom::kernel onto the Loom's export surface; now
# also the Input package's first consumer. tests: the Zengine suites (the smoke test
# stays its own tiny proof; the snake and input suites drive the real .so weaves through
# the real kernel). Order matters only where vocabulary flows: input's interface target
# must exist before snake consumes it.

enable_testing()
add_subdirectory(smoke)
add_subdirectory(input)
add_subdirectory(snake)
add_subdirectory(tests)
