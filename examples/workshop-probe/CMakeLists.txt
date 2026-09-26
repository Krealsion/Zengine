# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
#
# The Workshop probe as a stranger builds it: one loadable weave for the supplied Loom host, from
# the installed packages alone (`find_package(loom)`, `find_package(zengine)`), with no include
# directory into either repository. Its product, `zengine-workshop-probe.<so|dll>`, is named by a
# `loom-boot.json` row; its journey is in probe.hpp, its console words in the external-host guide.
#   cmake -S examples/workshop-probe -B build/probe -DCMAKE_PREFIX_PATH="<loom>;<zengine>"

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
