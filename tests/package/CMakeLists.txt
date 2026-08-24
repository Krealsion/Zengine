# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
#
# THE STRANGER. An unrelated CMake project that consumes Zengine as an installed package.
#
# WHAT MAKES THIS EVIDENCE IS WHAT IS NOT IN IT. There is no include directory pointing into a
# Zengine source tree, no path into a Zengine build tree, no named .so or .dll, no compile
# option, no CMAKE_CXX_STANDARD borrowed from the library, and no find_package(loom) -- the
# Loom arrives because zengineConfig.cmake asks for it. If any of those ever becomes necessary
# for this file to configure, the package has stopped carrying its own requirements and this
# fixture is supposed to go red rather than be helped.
#
# run.cmake copies this directory somewhere outside both repositories before configuring it,
# so "outside the source tree" is a fact about where the build happens and not a claim.

cmake_minimum_required(VERSION 3.16)
project(kitchen LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(zengine 0.1 CONFIG REQUIRED)

# ---- every exported target, compiled and run -------------------------------------------
add_executable(witness-surfaces public_surface.cpp)
target_link_libraries(witness-surfaces
    PRIVATE
        zengine::activation
        zengine::timer
        zengine::surface
        zengine::input
        zengine::ui
        zengine::component
        zengine::operator
        zengine::operator-consumer)

# ---- one capability, end to end: a weave that orders time ------------------------------
#
# loom_weave_build_contract() arrives with the Loom package, which zengineConfig.cmake found
# for us. It is not optional and forgetting it is silent -- it applies whatever this platform
# needs for a loadable image's statics to die with the image.
add_library(oven SHARED oven.cpp)
target_link_libraries(oven PRIVATE zengine::timer loom::switchboard)
loom_weave_build_contract(oven)
set_target_properties(oven PROPERTIES PREFIX "" OUTPUT_NAME oven)

add_executable(kitchen-host kitchen_host.cpp)
target_link_libraries(kitchen-host PRIVATE zengine::timer loom::switchboard loom::kernel)

# The Timer SERVICE is a Zengine artifact, and this is where it comes from: the package says
# where its own artifacts live, so nothing here reaches into a build tree to fetch one.
if(NOT ZENGINE_ARTIFACT_DIR)
    message(FATAL_ERROR
        "kitchen: this zengine package carries no loadable artifacts "
        "(ZENGINE_RUNTIME_ARTIFACTS is empty), so there is no Timer service to bake with. It "
        "was installed from a build against a Loom with no kernel.")
endif()
add_custom_command(TARGET kitchen-host POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${ZENGINE_ARTIFACT_DIR}/zengine-timer${CMAKE_SHARED_LIBRARY_SUFFIX}"
            "$<TARGET_FILE_DIR:kitchen-host>"
    COMMENT "kitchen: taking the Timer service from the installed zengine package")
add_dependencies(kitchen-host oven)

# ---- what the artifact list is, from outside (QR-5) ------------------------------------
#
# The stranger is the only party that can say what the PUBLIC vocabulary looks like, so the
# claim behind the noun is asserted here rather than described anywhere.
#
# The claim is that this list is HETEROGENEOUS. `zengine-timer` is a weave -- kitchen_host.cpp
# hands it to the Kernel and it arrives on the bus with a role. `zengine-operators-basic` is a
# PROVIDER: a host opens it directly, and it has no participant, WeaveId, role or grant in it
# at all. One list, two different runtime surfaces, so the only noun true of every member is
# `artifact`. If a later phase renames the variable back to something that says `weave`, this
# goes red with the reason attached.
foreach(expected zengine-timer zengine-operators-basic)
    if(NOT expected IN_LIST ZENGINE_RUNTIME_ARTIFACTS)
        message(FATAL_ERROR
            "kitchen: '${expected}' is not in ZENGINE_RUNTIME_ARTIFACTS "
            "(${ZENGINE_RUNTIME_ARTIFACTS}). The package's advertised artifacts are not the "
            "ones it was supposed to install.")
    endif()
    if(NOT EXISTS "${ZENGINE_ARTIFACT_DIR}/${expected}${CMAKE_SHARED_LIBRARY_SUFFIX}")
        message(FATAL_ERROR
            "kitchen: the package advertises '${expected}' but there is no such file in "
            "ZENGINE_ARTIFACT_DIR (${ZENGINE_ARTIFACT_DIR}). A list of stems that names a "
            "file nobody installed is worse than an empty one.")
    endif()
endforeach()
message(STATUS
    "kitchen: the installed package advertises ${ZENGINE_RUNTIME_ARTIFACTS} in "
    "${ZENGINE_ARTIFACT_DIR} -- weaves and a provider in one list, which is why they are "
    "called artifacts")
