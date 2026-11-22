# SPDX-License-Identifier: AGPL-3.0-only
# Copyright (c) 2026 Joshua DeMoss
#
# ONE COMMAND TO A READY BUILD (CONTRIBUTING.md, "Build and test"), on Linux and on Windows with
# MSYS2's MinGW-w64, from this checkout:
#
#   cmake -P tools/ready-build.cmake <build folder> [<Loom checkout>]
#
# Loom stands beside Zengine (`../Loom` unless named; cloned from GitHub when it is not there), is
# built and installed into the build folder, and Zengine is configured and built against it there,
# so the build folder holds `loom/` (Loom's tree), `loom-install/` (the installed package Zengine
# finds) and `zengine/` (Zengine's tree). It writes `CMakeUserPresets.json` here too, a `ready`
# preset naming that tree, its compilers and the Loom it found, so an IDE that reads CMake presets
# (CLion, Visual Studio Code's CMake Tools) opens the checkout ready to build. Run again, it moves
# each checkout forward to its upstream branch when it is clean and can fast-forward, says so when
# it cannot and leaves it, and rebuilds only what changed: an installed Loom file whose bytes did
# not change keeps its time, so Zengine rebuilds only what the new Loom changed.
#
# It needs what building Zengine needs and nothing more: CMake, Git, a C++ compiler on PATH (in the
# MSYS2 MinGW64 shell on Windows), and Ninja when it is there (CMake's default generator when not).
# Nothing is written outside the build folder, the Loom checkout and this checkout's presets file.
# It builds as many jobs at once as the memory available when it starts holds, at one heavy unit a
# job, and no more than the cores, and says the count it chose. `-DREADY_JOBS=<n>` before `-P` sets
# the count instead; `-DREADY_BUILD=OFF` configures both and installs Loom, and builds nothing of
# Zengine.

cmake_minimum_required(VERSION 3.21)

set(ready_loom_url "https://github.com/Krealsion/Loom.git")
get_filename_component(ready_zengine "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

# The arguments after the script: the build folder, then the Loom checkout.
set(ready_args "")
math(EXPR ready_last "${CMAKE_ARGC} - 1")
set(ready_seen_script FALSE)
foreach(i RANGE 1 ${ready_last})
    if(ready_seen_script)
        list(APPEND ready_args "${CMAKE_ARGV${i}}")
    elseif(CMAKE_ARGV${i} MATCHES "ready-build[.]cmake$")
        set(ready_seen_script TRUE)
    endif()
endforeach()
list(LENGTH ready_args ready_n)
if(ready_n LESS 1 OR ready_n GREATER 2)
    message(FATAL_ERROR "ready-build: name the build folder, and the Loom checkout if it is not "
                        "../Loom:\n  cmake -P tools/ready-build.cmake <build folder> [<Loom checkout>]")
endif()
list(GET ready_args 0 ready_build)
get_filename_component(ready_build "${ready_build}" ABSOLUTE)
if(ready_n EQUAL 2)
    list(GET ready_args 1 ready_loom)
    get_filename_component(ready_loom "${ready_loom}" ABSOLUTE)
else()
    get_filename_component(ready_loom "${ready_zengine}/../Loom" ABSOLUTE)
endif()
string(FIND "${ready_build}/" "${ready_zengine}/" ready_inside)
if(ready_inside EQUAL 0)
    message(FATAL_ERROR "ready-build: the build folder ${ready_build} is inside this checkout; name "
                        "one beside it, so the checkout holds only its sources")
endif()

find_program(ready_git git)
if(NOT ready_git)
    message(FATAL_ERROR "ready-build: no git on PATH; it fetches Loom and keeps both checkouts current")
endif()
# The job count: `-DREADY_JOBS=<n>` when given, else as many jobs as the memory available now holds
# at one heavy unit a job, and no more than the cores. A build tool's own count is the cores and
# more, and the Workshop suites' heaviest units take several GB each to compile, so at that count a
# cold build outgrows the memory of most machines; a job is counted at 4 GB, above the heaviest
# unit, with room for the rest of the machine.
set(ready_job_mb 4096)
cmake_host_system_information(RESULT ready_cores QUERY NUMBER_OF_LOGICAL_CORES)
cmake_host_system_information(RESULT ready_avail_mb QUERY AVAILABLE_PHYSICAL_MEMORY)
if(DEFINED READY_JOBS)
    if(NOT READY_JOBS MATCHES "^[1-9][0-9]*$")
        message(FATAL_ERROR "ready-build: READY_JOBS is the number of jobs at once, 1 or more, not "
                            "'${READY_JOBS}'")
    endif()
    set(ready_count "${READY_JOBS}")
    message(STATUS "ready-build: ${ready_count} jobs at once, as -DREADY_JOBS asks")
else()
    if(NOT ready_avail_mb GREATER 0 OR NOT ready_cores GREATER 0)
        set(ready_count 1)
        message(STATUS "ready-build: the memory available or the cores could not be read, so it "
                       "builds 1 job at once; -DREADY_JOBS=<n> sets the count")
    else()
        math(EXPR ready_count "${ready_avail_mb} / ${ready_job_mb}")
        if(ready_count LESS 1)
            set(ready_count 1)
        elseif(ready_count GREATER ready_cores)
            set(ready_count "${ready_cores}")
        endif()
        math(EXPR ready_avail_gb "${ready_avail_mb} / 1024")
        math(EXPR ready_job_gb "${ready_job_mb} / 1024")
        message(STATUS "ready-build: ${ready_count} jobs at once: ${ready_avail_gb} GB of memory "
                       "available, ${ready_job_gb} GB a job, ${ready_cores} cores; -DREADY_JOBS=<n> "
                       "sets the count")
    endif()
endif()
set(ready_jobs --parallel "${ready_count}")
find_program(ready_ninja ninja)
set(ready_generator "")
if(ready_ninja)
    set(ready_generator -G Ninja)
endif()

# Runs a command, its output shown, and stops on a failure, naming the step.
function(ready_run step)
    message(STATUS "ready-build: ${step}")
    execute_process(COMMAND ${ARGN} RESULT_VARIABLE rc)
    if(NOT rc EQUAL 0)
        message(FATAL_ERROR "ready-build: ${step} failed (exit ${rc}); nothing after it ran")
    endif()
endfunction()

# Moves a checkout forward to its upstream branch when it is clean and can fast-forward; says why
# when it is left as it is.
function(ready_update name dir)
    execute_process(COMMAND "${ready_git}" -C "${dir}" status --porcelain --untracked-files=no
                    OUTPUT_VARIABLE changed RESULT_VARIABLE rc OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(NOT rc EQUAL 0)
        message(FATAL_ERROR "ready-build: ${dir} is not a Git checkout of ${name}")
    endif()
    execute_process(COMMAND "${ready_git}" -C "${dir}" rev-parse --abbrev-ref --symbolic-full-name "@{upstream}"
                    OUTPUT_VARIABLE upstream RESULT_VARIABLE rc ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(NOT changed STREQUAL "")
        message(STATUS "ready-build: ${name} has uncommitted changes, so it is left at what it holds")
    elseif(NOT rc EQUAL 0)
        message(STATUS "ready-build: ${name}'s branch follows no upstream, so it is left at what it holds")
    else()
        execute_process(COMMAND "${ready_git}" -C "${dir}" pull --ff-only --quiet RESULT_VARIABLE rc)
        if(NOT rc EQUAL 0)
            message(STATUS "ready-build: ${name} cannot fast-forward to ${upstream}, so it is left at what it holds")
        endif()
    endif()
    execute_process(COMMAND "${ready_git}" -C "${dir}" log -1 "--format=%h %s"
                    OUTPUT_VARIABLE at OUTPUT_STRIP_TRAILING_WHITESPACE)
    message(STATUS "ready-build: ${name} at ${at}")
endfunction()

# ---- the checkouts ------------------------------------------------------------------------------
if(NOT EXISTS "${ready_loom}")
    ready_run("Loom cloned into ${ready_loom}" "${ready_git}" clone --quiet "${ready_loom_url}" "${ready_loom}")
elseif(NOT EXISTS "${ready_loom}/CMakeLists.txt" OR NOT EXISTS "${ready_loom}/include/zen")
    message(FATAL_ERROR "ready-build: ${ready_loom} is not a Loom checkout; name one, or a folder "
                        "that does not exist yet to clone it there")
endif()
ready_update(Loom "${ready_loom}")
ready_update(Zengine "${ready_zengine}")

# ---- Loom, built and installed into the build folder --------------------------------------------
set(ready_loom_tree "${ready_build}/loom")
set(ready_prefix "${ready_build}/loom-install")
set(ready_kernel "")
if(CMAKE_HOST_WIN32)
    set(ready_kernel -DLOOM_ENABLE_WINDOWS_KERNEL=ON)   # every suite but smoke loads its weaves
endif()
set(ready_configure_loom "${CMAKE_COMMAND}" -S "${ready_loom}" -B "${ready_loom_tree}")
if(NOT EXISTS "${ready_loom_tree}/CMakeCache.txt")
    list(APPEND ready_configure_loom ${ready_generator})
endif()
list(APPEND ready_configure_loom -DCMAKE_BUILD_TYPE=Debug -DZEN_BUILD_TESTS=OFF -DZEN_BUILD_EXAMPLES=OFF
     "-DCMAKE_INSTALL_PREFIX=${ready_prefix}" ${ready_kernel})
ready_run("Loom configured in ${ready_loom_tree}" ${ready_configure_loom})
ready_run("Loom built" "${CMAKE_COMMAND}" --build "${ready_loom_tree}" ${ready_jobs})
ready_run("Loom installed into ${ready_prefix}" "${CMAKE_COMMAND}" --install "${ready_loom_tree}")

# ---- Zengine, configured and built against it ---------------------------------------------------
set(ready_tree "${ready_build}/zengine")
set(ready_configure "${CMAKE_COMMAND}" -S "${ready_zengine}" -B "${ready_tree}")
if(NOT EXISTS "${ready_tree}/CMakeCache.txt")
    list(APPEND ready_configure ${ready_generator})
endif()
list(APPEND ready_configure -DCMAKE_BUILD_TYPE=Debug "-DCMAKE_PREFIX_PATH=${ready_prefix}")
ready_run("Zengine configured in ${ready_tree}" ${ready_configure})
if(NOT DEFINED READY_BUILD OR READY_BUILD)
    ready_run("Zengine built" "${CMAKE_COMMAND}" --build "${ready_tree}" ${ready_jobs})
endif()

# ---- the preset an IDE opens --------------------------------------------------------------------
# The tree's own generator and compilers, read from its cache, so the IDE builds the tree this
# command made rather than configuring another.
file(STRINGS "${ready_tree}/CMakeCache.txt" ready_cache
     REGEX "^(CMAKE_GENERATOR|CMAKE_C_COMPILER|CMAKE_CXX_COMPILER|CMAKE_MAKE_PROGRAM):[A-Z]+=")
set(ready_vars "")
set(ready_gen "")
foreach(line IN LISTS ready_cache)
    if(line MATCHES "^([A-Z_]+):[A-Z]+=(.*)$")
        if(CMAKE_MATCH_1 STREQUAL "CMAKE_GENERATOR")
            set(ready_gen "${CMAKE_MATCH_2}")
        elseif(NOT CMAKE_MATCH_2 STREQUAL "")
            string(APPEND ready_vars ",\n        \"${CMAKE_MATCH_1}\": \"${CMAKE_MATCH_2}\"")
        endif()
    endif()
endforeach()
set(ready_presets "${ready_zengine}/CMakeUserPresets.json")
set(ready_mark "\"zengine.ready-build\"")
if(EXISTS "${ready_presets}")
    file(READ "${ready_presets}" ready_old)
    string(FIND "${ready_old}" "${ready_mark}" ready_ours)
    if(ready_ours EQUAL -1)
        message(STATUS "ready-build: ${ready_presets} is yours, so it is left as it is; the tree to "
                       "open is ${ready_tree}, against CMAKE_PREFIX_PATH=${ready_prefix}")
        set(ready_presets "")
    endif()
endif()
if(ready_presets)
    file(WRITE "${ready_presets}" "{
  \"version\": 3,
  \"vendor\": { ${ready_mark}: { \"written by\": \"tools/ready-build.cmake\" } },
  \"configurePresets\": [
    {
      \"name\": \"ready\",
      \"displayName\": \"Ready build (${ready_tree})\",
      \"generator\": \"${ready_gen}\",
      \"binaryDir\": \"${ready_tree}\",
      \"cacheVariables\": {
        \"CMAKE_BUILD_TYPE\": \"Debug\",
        \"CMAKE_PREFIX_PATH\": \"${ready_prefix}\"${ready_vars}
      }
    }
  ],
  \"buildPresets\": [ { \"name\": \"ready\", \"configurePreset\": \"ready\" } ]
}
")
    message(STATUS "ready-build: ${ready_presets} names the tree as the `ready` preset")
endif()
message(STATUS "ready-build: ready -- ${ready_tree}, against the Loom installed in ${ready_prefix}")
