# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
#
# `zengine_wasm_weave(<target> <source> STEM <stem> LINKS <library>... [OUTPUT_DIRECTORY <dir>])`
# compiles a weave's one source for wasm32 with the Loom package's loom_wasm_weave() into
# `<stem>.wasm` (`HostContext::kModuleSuffix`) in OUTPUT_DIRECTORY, by default the caller's binary
# directory. LINKS are header-only Zengine libraries: a module carries only what it compiles.
# Zengine's headers are read from `_ZENGINE_WASM_INCLUDE`, this source tree or the installed one.

if(COMMAND zengine_wasm_weave)
    return()
endif()

if(NOT _ZENGINE_WASM_INCLUDE)
    get_filename_component(_ZENGINE_WASM_INCLUDE "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
endif()
set(_ZENGINE_WASM_INCLUDE "${_ZENGINE_WASM_INCLUDE}" CACHE INTERNAL "")

function(zengine_wasm_weave target source)
    cmake_parse_arguments(PARSE_ARGV 2 zw "" "STEM;OUTPUT_DIRECTORY" "LINKS")
    if(NOT COMMAND loom_wasm_weave)
        message(FATAL_ERROR "zengine_wasm_weave(${target}): this Loom package builds no "
                            "WebAssembly module (it was built without LOOM_ENABLE_WASM)")
    endif()
    if(NOT zw_STEM OR NOT zw_LINKS)
        message(FATAL_ERROR "zengine_wasm_weave(${target}): STEM and LINKS are required")
    endif()
    set(pending ${zw_LINKS})
    set(seen "")
    while(pending)
        list(POP_FRONT pending lib)
        if(lib IN_LIST seen OR lib MATCHES "^\\$<" OR lib MATCHES "^loom::(core|switchboard)$")
            continue() # a generator expression is a native build's; Loom's core rides every module
        endif()
        list(APPEND seen ${lib})
        if(NOT TARGET ${lib})
            message(FATAL_ERROR "zengine_wasm_weave(${target}): '${lib}' is not a target")
        endif()
        get_target_property(aliased ${lib} ALIASED_TARGET)
        if(aliased)
            set(lib ${aliased})
        endif()
        get_target_property(kind ${lib} TYPE)
        if(NOT kind STREQUAL "INTERFACE_LIBRARY")
            message(FATAL_ERROR
                "zengine_wasm_weave(${target}): '${lib}' is a compiled library (${kind}), and a "
                "module carries only what it compiles; a weave built boxed links header-only "
                "libraries")
        endif()
        get_target_property(next ${lib} INTERFACE_LINK_LIBRARIES)
        if(next)
            list(APPEND pending ${next})
        endif()
    endwhile()
    get_filename_component(entry "${source}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    if(zw_OUTPUT_DIRECTORY)
        get_filename_component(out_dir "${zw_OUTPUT_DIRECTORY}" ABSOLUTE
                               BASE_DIR "${CMAKE_CURRENT_BINARY_DIR}")
    else()
        set(out_dir "${CMAKE_CURRENT_BINARY_DIR}")
    endif()
    set(module "${out_dir}/${zw_STEM}.wasm")
    loom_wasm_weave(${target} OUTPUT "${module}" SOURCES "${entry}"
                    INCLUDE_DIRECTORIES "${_ZENGINE_WASM_INCLUDE}")
    set_target_properties(${target} PROPERTIES ZENGINE_WEAVE_SOURCE "${entry}"
                                               ZENGINE_WASM_MODULE "${module}")
endfunction()
