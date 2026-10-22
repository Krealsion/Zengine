# The tally app (examples/tally-app/README.md): an app's folder, written by hand, that `zengine-app`
# runs with no Workshop; and the tool that writes its view and its flow.

add_executable(zengine-tally-app-fixture fixture.cpp)
target_include_directories(zengine-tally-app-fixture PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(zengine-tally-app-fixture PRIVATE zengine-view zengine-flow zengine-operator
                                                        zengine-warnings zengine-sanitize)

# ---- the folder, staged beside the app host with its launchers --------------------------------
# A copy of the folder in `apps/tally/` beside `zengine-app`, and the two launchers an export
# writes: each starts that program on the folder it stands in, `run.cmd` in a console of its own.
if(NOT DEFINED ZENGINE_APP_DIR)
    return()
endif()
set(zengine_tally_app_dir ${ZENGINE_APP_DIR}/apps/tally)
set(zengine_tally_app_staged "")
foreach(file IN ITEMS app-plan.json app.json tally.view tally.flow)
    set(stamp ${CMAKE_CURRENT_BINARY_DIR}/staged-${file}.stamp)
    add_custom_command(OUTPUT ${stamp}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${zengine_tally_app_dir}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${CMAKE_CURRENT_SOURCE_DIR}/${file} ${zengine_tally_app_dir}
        COMMAND ${CMAKE_COMMAND} -E touch ${stamp}
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${file}
        COMMENT "tally app: staging ${file} beside the app host")
    list(APPEND zengine_tally_app_staged ${stamp})
endforeach()
add_custom_target(zengine-tally-app-staging DEPENDS ${zengine_tally_app_staged})
add_dependencies(zengine-app zengine-tally-app-staging)
file(GENERATE OUTPUT ${zengine_tally_app_dir}/run.cmd
     CONTENT "@start \"tally\" \"$<SHELL_PATH:$<TARGET_FILE:zengine-app>>\" \"%~dp0.\"\r\n")
# ...executable where this CMake can say so (3.19); `sh run.sh` runs it either way.
set(zengine_tally_app_run_sh "#!/bin/sh\nexec \"$<TARGET_FILE:zengine-app>\" \"$(dirname \"$0\")\"\n")
if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.19)
    file(GENERATE OUTPUT ${zengine_tally_app_dir}/run.sh CONTENT "${zengine_tally_app_run_sh}"
         FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE
                          WORLD_READ WORLD_EXECUTE)
else()
    file(GENERATE OUTPUT ${zengine_tally_app_dir}/run.sh CONTENT "${zengine_tally_app_run_sh}")
endif()
