# Every program this project builds runs in the UTF-8 code page on Windows, its tests included: a
# manifest says so, and `argv`, the environment and every narrow path are then UTF-8 there as on
# every other platform. What a toolchain's own manifest says, the execution level and the systems
# it supports, stays. Method: agents/verification/platforms.md (VM-PLAT-16).

# Under MinGW-w64: the manifest linked into every program, as an object library compiled by
# CMake's resource compiler. The toolchain may link a default manifest of its own, and a program's
# own replaces it, so this one is the default with the code page added; a toolchain that links
# none gets the code page alone.
function(zengine_mingw_code_page fragment)
    file(READ "${fragment}" manifest)
    execute_process(COMMAND "${CMAKE_CXX_COMPILER}" -print-file-name=default-manifest.o
                    OUTPUT_VARIABLE default OUTPUT_STRIP_TRAILING_WHITESPACE
                    RESULT_VARIABLE asked ERROR_QUIET)
    set(kept "the toolchain links no manifest of its own")
    set(configured_from "")
    if(asked EQUAL 0 AND IS_ABSOLUTE "${default}" AND EXISTS "${default}")
        # The default's text is one printable run of its object, from `<?xml` to `</assembly>`.
        file(STRINGS "${default}" runs NEWLINE_CONSUME)
        set(theirs "")
        foreach(run IN LISTS runs)
            string(FIND "${run}" "<?xml" at)
            if(at GREATER -1)
                string(SUBSTRING "${run}" ${at} -1 theirs)
                break()
            endif()
        endforeach()
        string(FIND "${theirs}" "</assembly>" theirs_end)
        string(FIND "${manifest}" "<application" ours_begin)
        string(FIND "${manifest}" "</assembly>" ours_end)
        if(theirs_end EQUAL -1 OR ours_begin EQUAL -1 OR ours_end EQUAL -1)
            message(FATAL_ERROR "zengine: cannot read the manifest ${default} links into every program")
        endif()
        string(SUBSTRING "${theirs}" 0 ${theirs_end} head)
        math(EXPR length "${ours_end} - ${ours_begin}")
        string(SUBSTRING "${manifest}" ${ours_begin} ${length} added)
        set(manifest "${head}  ${added}</assembly>\n")
        set(kept "the toolchain's own kept: ${default}")
        set(configured_from "${default}")
    endif()
    # The manifest is written into the resource script, one quoted line each, so the resource
    # compiler opens no file by a path it would read in its own code page; and the script is
    # written only when its text changes, so a configure relinks nothing it did not change.
    string(REPLACE "\r" "" quoted "${manifest}")
    string(REGEX REPLACE "\n$" "" quoted "${quoted}")
    string(REPLACE "\\" "\\\\" quoted "${quoted}")
    string(REPLACE "\"" "\"\"" quoted "${quoted}")
    string(REPLACE "\n" "\\n\"\n\"" quoted "${quoted}")
    set(rc "${PROJECT_BINARY_DIR}/zengine-code-page.rc")
    file(WRITE "${rc}.new" "1 24\nBEGIN\n\"${quoted}\\n\"\nEND\n")
    configure_file("${rc}.new" "${rc}" COPYONLY)
    add_library(zengine-code-page OBJECT "${rc}")
    set_property(DIRECTORY "${PROJECT_SOURCE_DIR}" APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
                 "${fragment}" ${configured_from})
    set(zengine_code_page_kept "${kept}" PARENT_SCOPE)
endfunction()

# zengine_program_code_page() -- give the manifest to every executable this project has defined;
# called once, after the last. Under MSVC the linker writes a manifest of its own and CMake merges
# a listed one into it. A directory of another project (a sibling Loom, a fetched dependency) is
# not walked: its programs keep what that project gives them.
function(zengine_program_code_page)
    if(NOT WIN32)
        return()
    endif()
    set(fragment "${PROJECT_SOURCE_DIR}/cmake/utf8-code-page.manifest")
    if(MSVC)
        set(source "${fragment}")
        set(said "merged into the linker's own")
    elseif(MINGW)
        zengine_mingw_code_page("${fragment}")
        set(source "$<TARGET_OBJECTS:zengine-code-page>")
        set(said "${zengine_code_page_kept}")
    else()
        message(STATUS "zengine: no program manifest for this Windows toolchain, so its programs "
                       "run in the system's code page")
        return()
    endif()
    set(programs 0)
    set(dirs "${PROJECT_SOURCE_DIR}")
    while(dirs)
        list(GET dirs 0 dir)
        list(REMOVE_AT dirs 0)
        get_directory_property(project DIRECTORY "${dir}" DEFINITION PROJECT_NAME)
        if(NOT project STREQUAL PROJECT_NAME)
            continue()
        endif()
        get_property(subdirs DIRECTORY "${dir}" PROPERTY SUBDIRECTORIES)
        list(APPEND dirs ${subdirs})
        get_property(targets DIRECTORY "${dir}" PROPERTY BUILDSYSTEM_TARGETS)
        foreach(target IN LISTS targets)
            get_target_property(type ${target} TYPE)
            if(type STREQUAL "EXECUTABLE")
                target_sources(${target} PRIVATE "${source}")
                math(EXPR programs "${programs} + 1")
            endif()
        endforeach()
    endwhile()
    message(STATUS "zengine: ${programs} programs run in the UTF-8 code page (${said})")
endfunction()
