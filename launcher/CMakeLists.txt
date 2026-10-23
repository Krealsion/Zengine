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
# feature's `docs/tutorials/`, the first of a name winning. The package (ZENGINE_PACKAGE) has none.
set(zengine_launcher_python "")
set(zengine_launcher_demo_script "")
set(zengine_launcher_build "")
set(zengine_launcher_loom_prefix "")
set(zengine_launcher_demos_folder "")
set(zengine_launcher_setups "")
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
                endif()
            endforeach()
        endforeach()
    endif()
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
