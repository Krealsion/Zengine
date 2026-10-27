# The launcher (launcher/docs/zengine.launcher.md): `zengine-launcher`, the weave that starts an app
# or a demo from a catalog fixed here, detached, and keeps nothing of it.

# The words, and the catalog's configured facts (`launcher/configured.hpp`, written below): portable,
# no kernel and no loader.
add_library(zengine-launcher-vocabulary INTERFACE)
target_include_directories(zengine-launcher-vocabulary INTERFACE ${CMAKE_SOURCE_DIR}
                                                                 ${CMAKE_CURRENT_BINARY_DIR}/generated)
target_link_libraries(zengine-launcher-vocabulary INTERFACE loom::core)

# ---- the catalog, fixed at configure ---------------------------------------------------------
# The demo service is this checkout's `external-host/demo.py`, run by a Python 3.8 or newer with
# the Loom this tree builds against, where that Loom carries its session tools; otherwise there is
# none, and the launcher starts no demo. Its setups are found as `demo.py list` finds them: each
# folder holding a `setup.json` in the tool package's collection, then in `examples/`, then in each
# feature's `docs/tutorials/`, the first of a name winning. A package carries its own (below).
set(zengine_launcher_in_package false)
set(zengine_launcher_python "")
set(zengine_launcher_demo_script "")
set(zengine_launcher_build "")
set(zengine_launcher_loom_prefix "")
set(zengine_launcher_demos_folder "")
set(zengine_launcher_setups "")
set(zengine_launcher_setup_lines "")
if(LOOM_SESSION_TOOLS AND LOOM_HOST_PROGRAM AND NOT ZENGINE_PACKAGE)
    if(ZENGINE_SESSION_PYTHON)
        set(Python3_EXECUTABLE "${ZENGINE_SESSION_PYTHON}")
    endif()
    find_package(Python3 3.8 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        set(zengine_launcher_python "${Python3_EXECUTABLE}")
        set(zengine_launcher_demo_script "${CMAKE_SOURCE_DIR}/external-host/demo.py")
        set(zengine_launcher_build "${CMAKE_BINARY_DIR}")
        get_filename_component(zengine_launcher_loom_prefix "${LOOM_HOST_PROGRAM}" DIRECTORY)
        get_filename_component(zengine_launcher_loom_prefix "${zengine_launcher_loom_prefix}"
                               DIRECTORY)
        set(zengine_launcher_demos_folder "${CMAKE_SOURCE_DIR}/external-host/tools/workshop/setups")
        file(GLOB zengine_launcher_tutorials LIST_DIRECTORIES true
             "${CMAKE_SOURCE_DIR}/*/docs/tutorials")
        list(SORT zengine_launcher_tutorials)
        set(zengine_launcher_named "")
        foreach(root IN ITEMS "${zengine_launcher_demos_folder}" "${CMAKE_SOURCE_DIR}/examples"
                              ${zengine_launcher_tutorials})
            file(GLOB children LIST_DIRECTORIES true "${root}/*")
            list(SORT children)
            foreach(child IN LISTS children)
                get_filename_component(name "${child}" NAME)
                if(EXISTS "${child}/setup.json" AND NOT name IN_LIST zengine_launcher_named)
                    list(APPEND zengine_launcher_named "${name}")
                    string(APPEND zengine_launcher_setups "{\"${name}\", \"${child}\"}, ")
                    # ...and its own title and one line, as `demo.py list` says them, where this
                    # CMake reads JSON (3.19 and later); otherwise a listing says its name alone.
                    set(title "")
                    set(summary "")
                    if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.19)
                        file(READ "${child}/setup.json" setup_json)
                        string(JSON title ERROR_VARIABLE no_title GET "${setup_json}" title)
                        string(JSON summary ERROR_VARIABLE no_summary GET "${setup_json}" summary)
                        if(no_title)
                            set(title "")
                        endif()
                        if(no_summary)
                            set(summary "")
                        endif()
                    endif()
                    foreach(said IN ITEMS title summary)
                        string(REPLACE "\\" "\\\\" ${said} "${${said}}")
                        string(REPLACE "\"" "\\\"" ${said} "${${said}}")
                        string(REGEX REPLACE "[\r\n\t]" " " ${said} "${${said}}")
                    endforeach()
                    string(APPEND zengine_launcher_setup_lines
                           "{\"${name}\", \"${title}\", \"${summary}\"}, ")
                endif()
            endforeach()
        endforeach()
    endif()
endif()

# ---- the package's own demo service ----------------------------------------------------------
# `demo.py` beside the tool packages (cmake/ZengineInstall.cmake) and the setups there a package
# starts: the tool package's own and the tutorials an install exports, none built against a
# development runtime. Each is named relative to the package, found above the running program; the
# interpreter is found on the PATH at each listing and start (launcher/catalog.hpp, `find_python`).
if(ZENGINE_PACKAGE AND LOOM_SESSION_TOOLS AND TARGET zengine-guest-vocabulary)
    set(zengine_launcher_in_package true)
    set(zengine_launcher_tools "${CMAKE_INSTALL_DATADIR}/zengine/loom-tools")
    set(zengine_launcher_demo_script "${zengine_launcher_tools}/demo.py")
    set(zengine_launcher_demos_folder "${zengine_launcher_tools}/workshop/setups")
    file(GLOB zengine_launcher_children LIST_DIRECTORIES true
         "${CMAKE_SOURCE_DIR}/external-host/tools/workshop/setups/*")
    find_package(Python3 3.8 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        file(GLOB zengine_launcher_tutorials LIST_DIRECTORIES true
             "${CMAKE_SOURCE_DIR}/*/docs/tutorials/*")
        list(APPEND zengine_launcher_children ${zengine_launcher_tutorials})
    endif()
    list(SORT zengine_launcher_children)
    set(zengine_launcher_named "")
    foreach(child IN LISTS zengine_launcher_children)
        get_filename_component(name "${child}" NAME)
        if(NOT EXISTS "${child}/setup.json" OR name IN_LIST zengine_launcher_named)
            continue()
        endif()
        file(READ "${child}/setup.json" setup_json)
        if(setup_json MATCHES "\"runtime\"[ \t\r\n]*:[ \t\r\n]*\"development\"")
            continue()
        endif()
        list(APPEND zengine_launcher_named "${name}")
        string(APPEND zengine_launcher_setups
               "{\"${name}\", \"${zengine_launcher_demos_folder}/${name}\"}, ")
        set(title "")
        set(summary "")
        if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.19)
            string(JSON title ERROR_VARIABLE no_title GET "${setup_json}" title)
            string(JSON summary ERROR_VARIABLE no_summary GET "${setup_json}" summary)
            if(no_title)
                set(title "")
            endif()
            if(no_summary)
                set(summary "")
            endif()
        endif()
        foreach(said IN ITEMS title summary)
            string(REPLACE "\\" "\\\\" ${said} "${${said}}")
            string(REPLACE "\"" "\\\"" ${said} "${${said}}")
            string(REGEX REPLACE "[\r\n\t]" " " ${said} "${${said}}")
        endforeach()
        string(APPEND zengine_launcher_setup_lines "{\"${name}\", \"${title}\", \"${summary}\"}, ")
    endforeach()
endif()
configure_file(configured.hpp.in ${CMAKE_CURRENT_BINARY_DIR}/generated/launcher/configured.hpp
               @ONLY)

# What may be started, and how: header-only, so the suite judges as the weave does.
add_library(zengine-launcher-catalog INTERFACE)
target_link_libraries(zengine-launcher-catalog INTERFACE zengine-launcher-vocabulary
                                                         zengine-app-folder)

if(NOT TARGET loom::kernel)
    return()
endif()
zengine_weave(zengine-launcher weave.cpp MANUAL docs/zengine.launcher.md)
target_link_libraries(zengine-launcher PRIVATE zengine-launcher-catalog zengine-activation)
