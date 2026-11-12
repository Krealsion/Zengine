# SPDX-License-Identifier: AGPL-3.0-only
# Copyright (c) 2026 Joshua DeMoss
#
# The loadable-weave pattern, this tree's and the installed package's alike: a loadable weave is a
# PIC shared library with an exact filename (no `lib` prefix), linked against the exported
# substrate; the kernel is the host's dependency. The reloadable lifetime is the Loom's law
# (KERN-05), and loom_weave_build_contract() applies it. A weave takes the build discipline its
# folder names in ZENGINE_WEAVE_DISCIPLINE (none, for a stranger who names none).

# zengine_weave(<target> <source> [<more sources>...] [MANUAL <page>]): every weave goes through it,
# so the contract is a law here, not a convention.
function(zengine_weave target source)
    # ARGN, so a weave may carry more than its one .cpp (a generated source, an embedded
    # typeface); the first source is the one file that is the weave. `MANUAL <page>` names the
    # weave's manual, which it carries compiled in (cmake/ZengineManual.cmake).
    cmake_parse_arguments(PARSE_ARGV 2 zengine_weave "" "MANUAL" "")
    add_library(${target} SHARED ${source} ${zengine_weave_UNPARSED_ARGUMENTS})
    if(zengine_weave_MANUAL)
        zengine_manual(${target} PRIVATE "${zengine_weave_MANUAL}")
    endif()
    target_link_libraries(${target} PRIVATE loom::core loom::switchboard ${ZENGINE_WEAVE_DISCIPLINE})
    loom_weave_build_contract(${target})
    set_target_properties(${target} PROPERTIES PREFIX "" OUTPUT_NAME ${target})
    # ...AND THAT FILE IS WRITTEN DOWN, as the file this weave's code is read from. It is the
    # editing entry the development recipe catalog names for the weave (workshop/CMakeLists.txt),
    # so the entry is the build's own declaration and never a file guessed from a target's name.
    get_filename_component(zengine_weave_entry "${source}" ABSOLUTE
                           BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    set_property(TARGET ${target} PROPERTY ZENGINE_WEAVE_SOURCE "${zengine_weave_entry}")
    # ...AND A WEAVE OF THE TREE'S OWN SOURCE IS COUNTED, for the census of the shapes it publishes
    # (tests/test_shapes_census.cpp); a test's own weave, or one built from a source the build
    # writes -- under the build tree, wherever that tree stands -- publishes nothing of the tree's.
    string(FIND "${zengine_weave_entry}" "${PROJECT_SOURCE_DIR}/" zengine_weave_in_tree)
    string(FIND "${zengine_weave_entry}" "${PROJECT_SOURCE_DIR}/tests/" zengine_weave_in_tests)
    string(FIND "${zengine_weave_entry}" "${PROJECT_BINARY_DIR}/" zengine_weave_in_build)
    if(zengine_weave_in_tree EQUAL 0 AND NOT zengine_weave_in_tests EQUAL 0 AND
       NOT zengine_weave_in_build EQUAL 0)
        set_property(GLOBAL APPEND PROPERTY ZENGINE_PRODUCT_WEAVES ${target})
    endif()
endfunction()

# ---- ...and an artifact that is a provider, not a weave --------------------------------
# One line differs from zengine_weave(), and it is the claim: no loom::switchboard. A provider
# supplies operator definitions to a host that opened it; the Kernel never loads it and it has
# no participant. It still takes loom_weave_build_contract(): it is opened, held while its
# contributions are installed and closed, and an escaped static would survive an unmount.
function(zengine_provider target source)
    add_library(${target} SHARED ${source} ${ARGN})
    target_link_libraries(${target} PRIVATE loom::core ${ZENGINE_WEAVE_DISCIPLINE})
    loom_weave_build_contract(${target})
    set_target_properties(${target} PROPERTIES PREFIX "" OUTPUT_NAME ${target})
endfunction()


# ---- ...and the SDL a host's SDL-backed weaves need, beside it -----------------------------
# zengine_stage_sdl(<host target>) -- put the SDL runtime, ZENGINE_SDL_RUNTIME, beside a host that
# loads SDL-backed weaves; a no-op where SDL came from the system or there is none, so a host asks
# without an `if` of its own, which the lane with no SDL, the Windows stranger lane, is the least
# likely to test. This tree's runtime is cmake/ZengineSdl.cmake's, a package's its config's.
function(zengine_stage_sdl host)
    foreach(runtime IN LISTS ZENGINE_SDL_RUNTIME)
        add_custom_command(TARGET ${host} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${runtime} $<TARGET_FILE_DIR:${host}>
            COMMENT "${host}: staging ${runtime} beside the host")
    endforeach()
endfunction()
