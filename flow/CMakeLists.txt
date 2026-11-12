# Flow authoring and generated-runtime support; usable without Workshop.
add_library(zengine-flow INTERFACE)
add_library(zengine::flow ALIAS zengine-flow)
target_compile_features(zengine-flow INTERFACE cxx_std_20)
target_include_directories(zengine-flow INTERFACE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/zengine>)
target_link_libraries(zengine-flow INTERFACE zengine-maker zengine-operator-consumer
    zengine-operator zengine-message-draft loom::core loom::switchboard)

# The Flow host (`flow-host/runtime.hpp`, `RuntimeHost`): the session host Workshop and `zengine-app`
# both mount, exported as `zengine::flow-host` with its role's manual compiled in, so a host that
# mounts it links this and nothing else of either host's.
add_library(zengine-flow-host INTERFACE)
add_library(zengine::flow-host ALIAS zengine-flow-host)
target_link_libraries(zengine-flow-host INTERFACE zengine-flow zengine-activation zengine-timer-vocabulary
    zengine-manual)
zengine_manual(zengine-flow-host INTERFACE docs/zengine.flow.host.md)

if(TARGET loom::kernel)
    add_executable(zengine-flow-tool main.cpp)
    set_target_properties(zengine-flow-tool PROPERTIES OUTPUT_NAME zengine-flow)
    target_link_libraries(zengine-flow-tool PRIVATE zengine-flow loom::kernel
        zengine-warnings zengine-sanitize)
else()
    message(STATUS "Flow workbench skipped: Loom package does not export loom::kernel")
endif()
