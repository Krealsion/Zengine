# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
#
# The installed-package witness's package arm (run.cmake -DZEN_PACKAGE=ON): the package a stranger
# downloads, judged where run.cmake installed it (`prefix`), stripped. Its files are there, the SDL
# it built among them; `--version` names the commit this checkout is at; no image names the build
# machine's source or build folder; and Workshop starts from the prefix on the OS window's plan and
# on the terminal's, with an environment naming nothing installed, each row loaded.

if(WIN32)
    set(exe ".exe")
else()
    set(exe "")
endif()
set(workshop "${prefix}/bin/zengine-workshop${exe}")

# ---- the files a stranger opens first, and the SDL it built ----------------------------------
set(package_files bin/zengine-workshop${exe} bin/zengine-app${exe} bin/default-load-plan.json
    bin/graphical-load-plan.json README.txt BUILD-INFO.txt NOTICE.md LICENSE
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
foreach(plan IN ITEMS graphical-load-plan.json default-load-plan.json)
    set(project "${work}/package-start-${plan}")
    file(REMOVE_RECURSE "${project}")
    file(MAKE_DIRECTORY "${project}")
    execute_process(COMMAND ${CMAKE_COMMAND} -E env "PATH=${bare_path}" SDL_VIDEO_DRIVER=offscreen
                            "${workshop}" --isolated --load-plan "${prefix}/bin/${plan}"
                    WORKING_DIRECTORY "${project}" INPUT_FILE "${work}/package-start-input.txt"
                    TIMEOUT 20 RESULT_VARIABLE start_rc OUTPUT_VARIABLE start_said ERROR_VARIABLE start_said)
    if(NOT start_said MATCHES "operators: [0-9]+ resolvable" OR start_said MATCHES "unavailable: "
       OR start_said MATCHES "refused")
        message(FATAL_ERROR "package witness: Workshop from the package on ${plan} did not load every "
                            "row (${start_rc}):\n${start_said}")
    endif()
    message(STATUS "package witness: Workshop starts from the package on ${plan}, every row loaded ok")
endforeach()
