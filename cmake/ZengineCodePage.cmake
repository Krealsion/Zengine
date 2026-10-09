# Every program this project builds runs in the UTF-8 code page on Windows, its tests included: a
# manifest says so, and `argv`, the environment and every narrow path are then UTF-8 there as on
# every other platform. What a toolchain's own manifest says, the execution level and the systems
# it supports, stays. Method: agents/verification/platforms.md (VM-PLAT-16). The package installs
# this file beside its config, so a program built against it takes the code page by the same
# function (docs/getting-started.md).

set_property(GLOBAL PROPERTY ZENGINE_CODE_PAGE_MANIFEST
             "${CMAKE_CURRENT_LIST_DIR}/utf8-code-page.manifest")

# Under MinGW-w64: the manifest linked into every program, as one object CMake's resource compiler
# builds in the build tree. The toolchain may link a default manifest of its own, and a program's
# own replaces it, so this one is the default with the code page added; a toolchain that links
# none gets the code page alone.
function(zengine_mingw_code_page)
    get_property(fragment GLOBAL PROPERTY ZENGINE_CODE_PAGE_MANIFEST)
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
    # compiler opens no other file; and the script is written only when its text changes, so a
    # configure relinks nothing it did not change.
    string(REPLACE "\r" "" quoted "${manifest}")
    string(REGEX REPLACE "\n$" "" quoted "${quoted}")
    string(REPLACE "\\" "\\\\" quoted "${quoted}")
    string(REPLACE "\"" "\"\"" quoted "${quoted}")
    string(REPLACE "\n" "\\n\"\n\"" quoted "${quoted}")
    set(rc "${CMAKE_CURRENT_BINARY_DIR}/zengine-code-page.rc")
    file(WRITE "${rc}.new" "1 24\nBEGIN\n\"${quoted}\\n\"\nEND\n")
    configure_file("${rc}.new" "${rc}" COPYONLY)
    # The resource compiler reads its command line in its own code page, which may lack a letter
    # of the build directory's name, so it runs in that directory and is given the script and the
    # object by their own names.
    set(object "${CMAKE_CURRENT_BINARY_DIR}/zengine-code-page.obj")
    separate_arguments(flags NATIVE_COMMAND "${CMAKE_RC_FLAGS}")
    add_custom_command(OUTPUT "${object}"
                       COMMAND "${CMAKE_RC_COMPILER}" ${flags} -O coff zengine-code-page.rc
                               zengine-code-page.obj
                       DEPENDS "${rc}"
                       WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
                       COMMENT "Building the UTF-8 code page manifest every program links"
                       VERBATIM)
    add_custom_target(zengine-code-page DEPENDS "${object}")
    add_library(zengine-code-page-object OBJECT IMPORTED GLOBAL)
    set_property(TARGET zengine-code-page-object PROPERTY IMPORTED_OBJECTS "${object}")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${fragment}" ${configured_from})
    set_property(GLOBAL PROPERTY ZENGINE_CODE_PAGE_KEPT "${kept}")
endfunction()

# zengine_code_page(<target>...) -- give each program the manifest naming UTF-8 its active code
# page on Windows; elsewhere nothing. MSVC's linker writes a manifest of its own and CMake merges
# a listed one into it. Under MinGW-w64 the object is one of the program's own, which CMake names
# relative to the build directory, as the linker too reads its command line in its own code page.
function(zengine_code_page)
    if(NOT WIN32)
        return()
    endif()
    if(NOT MSVC AND NOT MINGW)
        message(STATUS "zengine: no program manifest for this Windows toolchain, so ${ARGN} run "
                       "in the system's code page")
        return()
    endif()
    get_property(fragment GLOBAL PROPERTY ZENGINE_CODE_PAGE_MANIFEST)
    if(MINGW AND NOT TARGET zengine-code-page)
        zengine_mingw_code_page()
    endif()
    foreach(target IN LISTS ARGN)
        if(MSVC)
            target_sources(${target} PRIVATE "${fragment}")
        else()
            target_sources(${target} PRIVATE "$<TARGET_OBJECTS:zengine-code-page-object>")
            add_dependencies(${target} zengine-code-page)
        endif()
    endforeach()
endfunction()

# zengine_program_code_page() -- give the manifest to every executable this project has defined;
# called once, after the last. A directory of another project (a sibling Loom, a fetched
# dependency) is not walked: its programs keep what that project gives them.
function(zengine_program_code_page)
    if(NOT WIN32)
        return()
    endif()
    if(NOT MSVC AND NOT MINGW)
        message(STATUS "zengine: no program manifest for this Windows toolchain, so its programs "
                       "run in the system's code page")
        return()
    endif()
    set(programs "")
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
                list(APPEND programs ${target})
            endif()
        endforeach()
    endwhile()
    zengine_code_page(${programs})
    if(MSVC)
        set(said "merged into the linker's own")
    else()
        get_property(said GLOBAL PROPERTY ZENGINE_CODE_PAGE_KEPT)
    endif()
    list(LENGTH programs count)
    message(STATUS "zengine: ${count} programs run in the UTF-8 code page (${said})")
endfunction()
