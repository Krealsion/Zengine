target_include_directories(Zen PUBLIC /types/)

add_subdirectory(logic)
add_subdirectory(graphics)
add_subdirectory(graphics_3d)
add_subdirectory(input)
add_subdirectory(state_management)
add_subdirectory(types)
add_subdirectory(ui)
target_sources(Zen PRIVATE
        config_manager.cpp
        config_code_gen.cpp
        timer.cpp
        logger.cpp
)
add_executable(generate_configs config_code_gen.cpp logic/utils.cpp)
target_link_libraries(generate_configs
        PUBLIC
        Zen
        SDL3::SDL3
        nlohmann_json::nlohmann_json
)
