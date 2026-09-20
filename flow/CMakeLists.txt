# Flow authoring and generated-runtime support; usable without Workshop.
add_library(zengine-flow INTERFACE)
add_library(zengine::flow ALIAS zengine-flow)
target_compile_features(zengine-flow INTERFACE cxx_std_20)
target_include_directories(zengine-flow INTERFACE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/zengine>)
target_link_libraries(zengine-flow INTERFACE zengine-maker zengine-operator-consumer
    zengine-operator zengine-message-draft loom::core loom::switchboard)

if(TARGET loom::kernel)
    add_executable(zengine-flow-tool main.cpp)
    set_target_properties(zengine-flow-tool PROPERTIES OUTPUT_NAME zengine-flow)
    target_link_libraries(zengine-flow-tool PRIVATE zengine-flow loom::kernel
        zengine-warnings zengine-sanitize)
else()
    message(STATUS "Flow workbench skipped: Loom package does not export loom::kernel")
endif()
