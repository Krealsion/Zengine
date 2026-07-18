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
# The DEFAULT is the STRANGER'S path: find_package() against an installed, exported Loom.
# That is the entire point of the split — Zengine consumes the Loom exactly as a third party
# would, so every rough edge in the public surface hits the house before it hits a guest, and
# the dependency arrow stays structurally un-invertible (the Loom's build cannot see Zengine).
#
# ZEN_LOOM_DEV is the explicit opt-in for the sibling-source workflow, for when the Loom and
# Zengine are being edited together. Both paths expose the SAME target names (loom::core,
# loom::switchboard), because the Loom's export sets EXPORT_NAME to match its in-tree ALIASes
# — so the override is a genuine drop-in and the two paths cannot silently come to mean
# different things.
#
# Why the default is NOT the convenient one: in dev mode the WHOLE Loom build tree is
# reachable, including targets deliberately left out of the exported surface. A mistaken
# dependency on one of those compiles happily in dev mode and breaks for a real consumer.
# Defaulting to the exported path means that mistake surfaces here, in the house.

option(ZEN_LOOM_DEV
       "Consume the Loom from the sibling ../Loom source tree instead of an installed package"
       OFF)

if(ZEN_LOOM_DEV)
    message(STATUS "zengine: Loom from sibling source ../Loom  [DEV OVERRIDE]")
    # A dependency's proof rides its version: Zengine's lane does not re-run the Loom's suite,
    # so the sibling build contributes its libraries and nothing else. ZEN_SDL is silenced for
    # the same reason — the SDL skin is Zengine-destined, not something this build needs today.
    set(ZEN_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
    set(ZEN_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(ZEN_SDL            OFF CACHE BOOL "" FORCE)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../Loom ${CMAKE_CURRENT_BINARY_DIR}/_loom)
else()
    find_package(loom 0.1 REQUIRED)
    message(STATUS "zengine: Loom from installed package at ${loom_DIR}")
endif()

# ---- The smoke consumer ---------------------------------------------------------------
#
# One test proving the consumption works is this repo's whole green today. No ports live here
# yet; the atlas's port-sort lands them in later phases, in the right home from birth.

enable_testing()
add_subdirectory(smoke)
