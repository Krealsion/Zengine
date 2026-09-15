# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
#
# THE DEVELOPMENT RUNTIME'S RULES: made when absent, reused while what it copied is current, and
# otherwise refused and left exactly as it is (WL-CODE-07).
#
# A configured build tree generates `<build>/workshop/development-runtime.cmake`, which sets that
# tree's facts and includes this file; the development launch runs that script before it starts
# anything, and a maker may run it by hand. Only a suite runs this file directly, with facts of
# its own. The facts:
#
#   zengine_build            the build tree the copies come from
#   zengine_source           its checkout, said and never read
#   zengine_configuration    the configuration those copies were built as
#   zengine_copies           every file a runtime holds, by absolute path
#   zengine_replaceable      those of them a runtime changes itself: the pane artifacts a promotion
#                            writes there, which a development build rebuilds in the tree too
#   zengine_default_runtime  the runtime directory when ZEN_RUNTIME names none
#   zengine_launch           the launcher, for the words (may be empty)
#
# and ZEN_RUNTIME. The manifest is written last, so a directory holding one is a runtime this file
# finished making; what it records -- the tree, the configuration, each copy's name and the digest
# of each copy nothing but a new build changes -- is what a later run checks before reusing it.
cmake_minimum_required(VERSION 3.16)

foreach(zengine_fact IN ITEMS zengine_build zengine_source zengine_configuration zengine_copies
                              zengine_replaceable zengine_default_runtime)
    if(NOT DEFINED ${zengine_fact})
        message(FATAL_ERROR "zengine: ${zengine_fact} is not set -- this file is run by the "
            "development-runtime.cmake a configured build tree generates; nothing was copied.")
    endif()
endforeach()
if(NOT DEFINED ZEN_RUNTIME OR ZEN_RUNTIME STREQUAL "")
    set(ZEN_RUNTIME "${zengine_default_runtime}")
endif()
get_filename_component(zengine_runtime "${ZEN_RUNTIME}" ABSOLUTE)
set(zengine_manifest "${zengine_runtime}/zengine-development-runtime.txt")
set(zengine_configuration_said "${zengine_configuration}")
if(zengine_configuration_said STREQUAL "")
    set(zengine_configuration_said "(none)")
endif()
string(CONCAT zengine_elsewhere "Name another directory for this build tree's runtime: "
    "-DZEN_RUNTIME=<dir> to this script, or --runtime <dir> to zengine-workshop-develop.")
string(CONCAT zengine_keep_it "To keep this runtime and make one from what the tree has built "
    "now, rename or move this directory (its promotions and reloads stay in it) and launch "
    "again, or name another directory: -DZEN_RUNTIME=<dir> to this script, or --runtime <dir> "
    "to zengine-workshop-develop.")

# ---- the copies, by the name each has in a runtime --------------------------------------
set(zengine_names "")
set(zengine_fixed_names "")
foreach(zengine_file IN LISTS zengine_copies)
    get_filename_component(zengine_name "${zengine_file}" NAME)
    if(zengine_name IN_LIST zengine_names)
        message(FATAL_ERROR "zengine: two of this build tree's copies are named ${zengine_name}, "
            "and a runtime holds one file per name -- nothing was copied.")
    endif()
    list(APPEND zengine_names "${zengine_name}")
    set("zengine_from_${zengine_name}" "${zengine_file}")
    if(zengine_file IN_LIST zengine_replaceable)
        set("zengine_kind_${zengine_name}" "replaceable")
    else()
        set("zengine_kind_${zengine_name}" "fixed")
        list(APPEND zengine_fixed_names "${zengine_name}")
    endif()
endforeach()

# ---- a runtime already there: reused only while it is this tree's, whole and current ------
if(EXISTS "${zengine_manifest}")
    file(STRINGS "${zengine_manifest}" zengine_lines)
    set(zengine_made_from "")
    set(zengine_made_format "")
    set(zengine_made_configuration "")
    set(zengine_made_at "an unrecorded time")
    set(zengine_recorded "")
    foreach(zengine_line IN LISTS zengine_lines)
        if(zengine_line MATCHES "^build tree: (.*)$")
            set(zengine_made_from "${CMAKE_MATCH_1}")
        elseif(zengine_line MATCHES "^format: (.*)$")
            set(zengine_made_format "${CMAKE_MATCH_1}")
        elseif(zengine_line MATCHES "^configuration: (.*)$")
            set(zengine_made_configuration "${CMAKE_MATCH_1}")
        elseif(zengine_line MATCHES "^made: (.*)$")
            set(zengine_made_at "${CMAKE_MATCH_1}")
        elseif(zengine_line MATCHES "^copy: ([^ ]+) (.+)$")
            list(APPEND zengine_recorded "${CMAKE_MATCH_2}")
            set("zengine_recorded_${CMAKE_MATCH_2}" "${CMAKE_MATCH_1}")
        endif()
    endforeach()

    if(NOT zengine_made_from STREQUAL zengine_build)
        message(FATAL_ERROR "zengine: ${zengine_runtime} is a development runtime made from "
            "the build tree ${zengine_made_from}, not from ${zengine_build} -- nothing was "
            "copied or changed. ${zengine_elsewhere}")
    endif()
    if(NOT zengine_made_format STREQUAL "2")
        message(FATAL_ERROR "zengine: ${zengine_runtime} was made from this build tree by an "
            "earlier Zengine, which recorded too little to tell whether it is whole and still "
            "current -- nothing was copied or changed. ${zengine_keep_it}")
    endif()
    if(NOT zengine_made_configuration STREQUAL zengine_configuration_said)
        message(FATAL_ERROR "zengine: ${zengine_runtime} was made from this build tree's "
            "${zengine_made_configuration} configuration, not its ${zengine_configuration_said} "
            "one -- nothing was copied or changed. ${zengine_keep_it}")
    endif()

    set(zengine_added "")
    set(zengine_gone "")
    foreach(zengine_name IN LISTS zengine_names)
        if(NOT zengine_name IN_LIST zengine_recorded)
            list(APPEND zengine_added "${zengine_name}")
        elseif(zengine_kind_${zengine_name} STREQUAL "replaceable"
               AND NOT zengine_recorded_${zengine_name} STREQUAL "replaceable")
            list(APPEND zengine_added "${zengine_name} (now a pane)")
        elseif(zengine_kind_${zengine_name} STREQUAL "fixed"
               AND zengine_recorded_${zengine_name} STREQUAL "replaceable")
            list(APPEND zengine_added "${zengine_name} (no longer a pane)")
        endif()
    endforeach()
    foreach(zengine_name IN LISTS zengine_recorded)
        if(NOT zengine_name IN_LIST zengine_names)
            list(APPEND zengine_gone "${zengine_name}")
        endif()
    endforeach()
    if(zengine_added OR zengine_gone)
        list(JOIN zengine_added ", " zengine_added_said)
        list(JOIN zengine_gone ", " zengine_gone_said)
        message(FATAL_ERROR "zengine: ${zengine_runtime} was made when this build tree's "
            "configuration copied other files (now: ${zengine_added_said}; no longer: "
            "${zengine_gone_said}) -- nothing was copied or changed. ${zengine_keep_it}")
    endif()

    set(zengine_missing "")
    foreach(zengine_name IN LISTS zengine_names)
        if(NOT EXISTS "${zengine_runtime}/${zengine_name}")
            list(APPEND zengine_missing "${zengine_name}")
        endif()
    endforeach()
    if(zengine_missing)
        list(JOIN zengine_missing ", " zengine_missing_said)
        message(FATAL_ERROR "zengine: ${zengine_runtime} is incomplete: ${zengine_missing_said} "
            "is not there, and nothing is copied into a runtime once it is made -- nothing was "
            "copied or changed. ${zengine_keep_it}")
    endif()

    set(zengine_changed "")
    foreach(zengine_name IN LISTS zengine_fixed_names)
        if(NOT EXISTS "${zengine_from_${zengine_name}}")
            message(FATAL_ERROR "zengine: ${zengine_from_${zengine_name}} is not there, so this "
                "build tree cannot say whether ${zengine_runtime} is still current -- build the "
                "tree first (cmake --build ${zengine_build}); nothing was copied or changed.")
        endif()
        file(SHA256 "${zengine_from_${zengine_name}}" zengine_digest)
        if(NOT zengine_digest STREQUAL zengine_recorded_${zengine_name})
            list(APPEND zengine_changed "${zengine_name}")
        endif()
    endforeach()
    if(zengine_changed)
        list(JOIN zengine_changed ", " zengine_changed_said)
        message(FATAL_ERROR "zengine: ${zengine_runtime} was made from this build tree at "
            "${zengine_made_at}, and the tree has built ${zengine_changed_said} anew since, so "
            "it would still run the copies it took then -- nothing was copied, changed or "
            "launched. ${zengine_keep_it}")
    endif()

    message(STATUS "zengine: reusing the development runtime at ${zengine_runtime}")
    message(STATUS "zengine:   made at ${zengine_made_at} from the build tree ${zengine_build}; "
                   "its host, services, plans and catalogs are what the tree builds now, and what "
                   "it promoted and reloaded is kept")
    return()
endif()

# ---- no runtime there: make one, into nothing that is already something -----------------
if(EXISTS "${zengine_runtime}")
    if(NOT IS_DIRECTORY "${zengine_runtime}")
        message(FATAL_ERROR "zengine: ${zengine_runtime} is a file, not a directory -- nothing "
            "was copied. ${zengine_elsewhere}")
    endif()
    file(GLOB zengine_present "${zengine_runtime}/*")
    if(zengine_present)
        message(FATAL_ERROR "zengine: ${zengine_runtime} is not empty and is not a development "
            "runtime this script made -- nothing was copied. Choose an empty or new directory.")
    endif()
endif()
foreach(zengine_file IN LISTS zengine_copies)
    if(NOT EXISTS "${zengine_file}")
        message(FATAL_ERROR "zengine: ${zengine_file} is not there -- build the tree first "
            "(cmake --build ${zengine_build}); nothing was copied.")
    endif()
endforeach()
file(MAKE_DIRECTORY "${zengine_runtime}")
set(zengine_record "")
foreach(zengine_name IN LISTS zengine_names)
    file(COPY "${zengine_from_${zengine_name}}" DESTINATION "${zengine_runtime}"
         FOLLOW_SYMLINK_CHAIN)
    if(zengine_kind_${zengine_name} STREQUAL "fixed")
        file(SHA256 "${zengine_runtime}/${zengine_name}" zengine_digest)
    else()
        set(zengine_digest "replaceable")
    endif()
    string(APPEND zengine_record "copy: ${zengine_digest} ${zengine_name}\n")
endforeach()
string(TIMESTAMP zengine_now "%Y-%m-%d %H:%M:%S UTC" UTC)
file(WRITE "${zengine_manifest}"
    "A Zengine development runtime. Nothing a build does writes here: a reload stages into it and\n"
    "a promotion writes its pane files. A launch reuses it while the build tree's host, services,\n"
    "plans and catalogs are the ones copied below; otherwise it is refused and left as it is.\n"
    "format: 2\n"
    "build tree: ${zengine_build}\n"
    "source tree: ${zengine_source}\n"
    "configuration: ${zengine_configuration_said}\n"
    "made: ${zengine_now}\n"
    "catalog: ${zengine_runtime}/development-build-recipes.json\n"
    "${zengine_record}")
message(STATUS "zengine: development runtime made at ${zengine_runtime}")
message(STATUS "zengine:   from the build tree ${zengine_build} (source ${zengine_source})")
if(DEFINED zengine_launch AND NOT zengine_launch STREQUAL "")
    message(STATUS "zengine:   launch it with ${zengine_launch} -- in CLion, the Develop Workshop "
                   "run configuration")
endif()
