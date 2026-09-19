# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
#
# THE WORKSHOP PROBE, AS A STRANGER BUILDS IT.
#
# An unrelated CMake project that produces one loadable weave for the supplied Loom host, out of
# the installed packages and nothing else: `find_package(loom)` for the weave surface and the link
# envelope, `find_package(zengine)` for the Input and Surface vocabularies. No include directory
# points into either repository, and no header this file reaches is unpublished.
#
#     cmake -S examples/workshop-probe -B build/probe -DCMAKE_PREFIX_PATH="<loom prefix>;<zengine prefix>"
#     cmake --build build/probe
#
# The product is `zengine-workshop-probe.<so|dll>`, which a `loom-boot.json` row names. The
# journey it runs, and the console words that start it, are in probe.hpp and in
# docs/workshop/external-host.md.

cmake_minimum_required(VERSION 3.16)
project(workshop-probe LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(loom REQUIRED)
find_package(zengine 0.1 CONFIG REQUIRED)

add_library(zengine-workshop-probe SHARED probe.cpp)
target_link_libraries(zengine-workshop-probe
    PRIVATE loom::core loom::switchboard zengine::input zengine::surface)
set_target_properties(zengine-workshop-probe PROPERTIES PREFIX "")
loom_weave_build_contract(zengine-workshop-probe)
