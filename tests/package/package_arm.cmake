# SPDX-License-Identifier: AGPL-3.0-only
# Copyright (c) 2026 Joshua DeMoss
#
# The installed-package witness's package arm (run.cmake -DZEN_PACKAGE=ON): the package a stranger
# downloads, judged where run.cmake installed it (`prefix`), stripped: its files and SDL; its
# commit and one release number in all it says; no image naming the build machine's folders; and
# Workshop started from the prefix as a weaver starts it, nothing beside it, each row loaded in
# the window each start means (no flag, `--tui`, a driver SDL lacks).

if(WIN32)
    set(exe ".exe")
else()
    set(exe "")
endif()
set(workshop "${prefix}/bin/zengine-workshop${exe}")

# ---- the files a stranger opens first, and the SDL it built ----------------------------------
set(package_files bin/zengine-workshop${exe} bin/zengine-app${exe} bin/default-load-plan.json
    README.txt BUILD-INFO.txt NOTICE.md LICENSE
    licenses/SDL3/LICENSE.txt licenses/SDL3_ttf/LICENSE.txt licenses/FreeType/FTL.TXT)
if(WIN32)
    list(APPEND package_files Workshop.cmd Workshop-terminal.cmd bin/SDL3.dll bin/SDL3_ttf.dll)
else()
    list(APPEND package_files workshop.sh workshop-terminal.sh bin/libSDL3.so.0 bin/libSDL3_ttf.so.0)
endif()
foreach(f IN LISTS package_files)
    if(NOT EXISTS "${prefix}/${f}")
        message(FATAL_ERROR "package witness: the package has no ${f}")
    endif()
endforeach()
message(STATUS "package witness: the package's launchers, first page, notices and SDL are there ok")

# ---- what it says it is ----------------------------------------------------------------------
execute_process(COMMAND git -C "${repo}" rev-parse HEAD OUTPUT_VARIABLE commit
                OUTPUT_STRIP_TRAILING_WHITESPACE RESULT_VARIABLE git_rc)
if(NOT git_rc EQUAL 0 OR commit STREQUAL "")
    message(FATAL_ERROR "package witness: the checkout at ${repo} names no commit")
endif()
execute_process(COMMAND "${workshop}" --version RESULT_VARIABLE version_rc
                OUTPUT_VARIABLE version_said ERROR_VARIABLE version_said)
if(NOT version_rc EQUAL 0 OR NOT version_said MATCHES "zengine commit ${commit}")
    message(FATAL_ERROR "package witness: `zengine-workshop --version` does not name the commit "
                        "${commit} (exit ${version_rc}):\n${version_said}")
endif()
file(READ "${prefix}/BUILD-INFO.txt" build_info)
if(NOT build_info MATCHES "zengine commit ${commit}")
    message(FATAL_ERROR "package witness: BUILD-INFO.txt does not name the commit ${commit}:\n${build_info}")
endif()
message(STATUS "package witness: --version and BUILD-INFO.txt name ${commit} ok")

# ...and one release number in all it says (the top-level CMakeLists.txt, "the release number"):
# Workshop, its CMake package, BUILD-INFO.txt and README.txt say the number this tree was
# configured as, and so do the Loom package it was built against and that Loom's host, which a
# package holds beside them.
file(READ "${build_dir}/CMakeCache.txt" arm_cache)
string(REGEX MATCH "\nZENGINE_PACKAGE_VERSION:[A-Z]+=([^\n]*)" _ "${arm_cache}")
set(number "${CMAKE_MATCH_1}")
string(REGEX MATCH "\nloom_DIR:[A-Z]+=([^\n]*)" _ "${arm_cache}")
set(loom_config_dir "${CMAKE_MATCH_1}")
if(NOT number MATCHES "^[0-9]+[.][0-9]+[.][0-9]+$" OR NOT EXISTS "${loom_config_dir}/loomConfigVersion.cmake")
    message(FATAL_ERROR "package witness: ${build_dir} names no release number X.Y.N ('${number}') "
                        "or no installed Loom ('${loom_config_dir}')")
endif()
set(wrong "")
string(REGEX MATCH "^[^\r\n]*" said "${version_said}")
if(NOT said STREQUAL "zengine ${number}")
    list(APPEND wrong "zengine-workshop --version: ${said}")
endif()
foreach(config "${prefix}/lib/cmake/zengine/zengineConfigVersion.cmake"
               "${loom_config_dir}/loomConfigVersion.cmake")
    file(READ "${config}" config_text)
    string(REGEX MATCH "set\\(PACKAGE_VERSION \"([^\"]*)\"\\)" _ "${config_text}")
    if(NOT CMAKE_MATCH_1 STREQUAL number)
        list(APPEND wrong "${config}: ${CMAKE_MATCH_1}")
    endif()
endforeach()
string(REGEX MATCH "^[^\r\n]*" said "${build_info}")
string(REGEX REPLACE "[ \t]+" " " said "${said}")
if(NOT said STREQUAL "zengine ${number}")
    list(APPEND wrong "BUILD-INFO.txt: ${said}")
endif()
file(STRINGS "${prefix}/README.txt" said LIMIT_COUNT 1)
string(FIND "${said}" "Zengine ${number} -- " said_at)
if(NOT said_at EQUAL 0)
    list(APPEND wrong "README.txt: ${said}")
endif()
get_filename_component(loom_host "${loom_config_dir}/../../../bin/loom-host${exe}" ABSOLUTE)
execute_process(COMMAND "${loom_host}" --version RESULT_VARIABLE host_rc
                OUTPUT_VARIABLE host_said ERROR_VARIABLE host_said)
string(FIND "${host_said}" "loom-host ${number}  (weave ABI v" host_at)
if(NOT host_rc EQUAL 0 OR NOT host_at EQUAL 0)
    list(APPEND wrong "${loom_host} --version (exit ${host_rc}): ${host_said}")
endif()
if(wrong)
    string(REPLACE ";" "\n  " pretty "${wrong}")
    message(FATAL_ERROR "package witness: the package is ${number}, and these say otherwise:\n  ${pretty}")
endif()
message(STATUS "package witness: Workshop, loom-host, both CMake packages, BUILD-INFO.txt and "
               "README.txt say ${number} ok")

# ---- no build machine in any image -----------------------------------------------------------
# The source and build folders, as this machine spells them, read in every printable run of every
# file in bin/ and lib/: a compile's __FILE__, a configured path, a RUNPATH.
set(needles "${repo}" "${build_dir}")
if(WIN32)
    foreach(n IN ITEMS "${repo}" "${build_dir}")
        file(TO_NATIVE_PATH "${n}" native)
        list(APPEND needles "${native}")
    endforeach()
endif()
file(GLOB_RECURSE images "${prefix}/bin/*" "${prefix}/lib/*")
set(named "")
foreach(f IN LISTS images)
    file(STRINGS "${f}" runs LENGTH_MINIMUM 8)
    foreach(n IN LISTS needles)
        string(FIND "${runs}" "${n}" at)
        if(NOT at EQUAL -1)
            file(RELATIVE_PATH rel "${prefix}" "${f}")
            list(APPEND named "${rel}: ${n}")
        endif()
    endforeach()
endforeach()
if(named)
    string(REPLACE ";" "\n  " pretty "${named}")
    message(FATAL_ERROR "package witness: the package names the machine that built it:\n  ${pretty}")
endif()
list(LENGTH images image_count)
message(STATUS "package witness: no source or build folder in the package's ${image_count} files ok")

# ---- started from the prefix, with nothing installed beside it -------------------------------
# A fresh folder its project, every per-user file off, PATH naming only the system, SDL on a
# driver that needs no display; it is ended when the wait runs out, having said what it loaded.
if(WIN32)
    set(bare_path "$ENV{SystemRoot}\\System32;$ENV{SystemRoot}")
else()
    set(bare_path "/usr/bin:/bin")
endif()
file(WRITE "${work}/package-start-input.txt" "")
# Each start: its name, its driver, its flag, the window it loads, and whether the GUI window gave way.
set(starts "window|offscreen||zengine-window-sdl|no"
           "tui|offscreen|--tui|zengine-window-tui|no"
           "no-display|zengine-no-such-driver||zengine-window-tui|yes")
foreach(start IN LISTS starts)
    string(REPLACE "|" ";" start "${start}")
    list(GET start 0 name)
    list(GET start 1 driver)
    list(GET start 2 flag)
    list(GET start 3 window)
    list(GET start 4 gave_way)
    set(project "${work}/package-start-${name}")
    file(REMOVE_RECURSE "${project}")
    file(MAKE_DIRECTORY "${project}")
    execute_process(COMMAND ${CMAKE_COMMAND} -E env "PATH=${bare_path}" SDL_VIDEO_DRIVER=${driver}
                            "${workshop}" --isolated ${flag}
                    WORKING_DIRECTORY "${project}" INPUT_FILE "${work}/package-start-input.txt"
                    TIMEOUT 20 RESULT_VARIABLE start_rc OUTPUT_VARIABLE start_said ERROR_VARIABLE start_said)
    set(loaded_all TRUE)
    if(NOT start_said MATCHES "operators: [0-9]+ resolvable" OR start_said MATCHES "unavailable: "
       OR start_said MATCHES "load plan refused" OR start_said MATCHES "plan was not completed"
       OR NOT start_said MATCHES "loaded: ${window} ")
        set(loaded_all FALSE)
    endif()
    if(gave_way STREQUAL "yes" AND NOT start_said MATCHES
       "`zengine-window-sdl` did not load, so `zengine-window-tui` stands in its row")
        set(loaded_all FALSE)
    endif()
    if(gave_way STREQUAL "no" AND start_said MATCHES "did not load, so")
        set(loaded_all FALSE)
    endif()
    if(NOT loaded_all)
        message(FATAL_ERROR "package witness: Workshop from the package (${name}) did not load every "
                            "row in ${window} (${start_rc}):\n${start_said}")
    endif()
    message(STATUS "package witness: Workshop starts from the package (${name}) in ${window}, every "
                   "row loaded ok")
endforeach()
