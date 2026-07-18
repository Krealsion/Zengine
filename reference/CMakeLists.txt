cmake_minimum_required(VERSION 3.24)
project(Zen LANGUAGES CXX)

# --- Standard ---
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- Options ---
option(ZEN_ENABLE_WARNINGS "Enable strict warnings" ON)
option(ZEN_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)
option(ZEN_ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(ZEN_ENABLE_UBSAN "Enable UBSanitizer" OFF)
option(ZEN_ENABLE_LSAN "Enable LeakSanitizer" OFF)

# --- Library Target ---
add_library(Zen STATIC)
add_library(Zen::Zen ALIAS Zen)

# --- Public include root (future-proof) ---
target_include_directories(Zen PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/game_engine_code/engine/>
    $<INSTALL_INTERFACE:include>
        Resources
)

# --- Dependencies ---
find_package(SDL3 REQUIRED CONFIG)
find_package(SDL3_image REQUIRED CONFIG)
find_package(SDL3_ttf REQUIRED CONFIG)
find_package(Threads REQUIRED)
find_package(rttr CONFIG REQUIRED)

include(FetchContent)
FetchContent_Declare(
    json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)
FetchContent_MakeAvailable(json)

target_link_libraries(Zen
    PUBLIC
        SDL3::SDL3
        SDL3_image::SDL3_image
        SDL3_ttf::SDL3_ttf
        nlohmann_json::nlohmann_json
    PRIVATE
        Threads::Threads
        RTTR::Core
)

# --- Engine Sources ---
add_subdirectory(src)

target_sources(Zen PRIVATE Resources/generated/config.cpp)

set(ZEN_CONFIG_SCHEMA ${CMAKE_SOURCE_DIR}/Resources/config/default.json)
set(ZEN_CONFIG_GEN_DIR ${CMAKE_BINARY_DIR}/generated)

set(ZEN_CONFIG_GEN_H ${ZEN_CONFIG_GEN_DIR}/config.h)
set(ZEN_CONFIG_GEN_CPP ${ZEN_CONFIG_GEN_DIR}/config.cpp)

add_custom_command(
        OUTPUT ${ZEN_CONFIG_GEN_H} ${ZEN_CONFIG_GEN_CPP}
        COMMAND zen_codegen
        ${ZEN_CONFIG_SCHEMA}
        ${ZEN_CONFIG_GEN_DIR}
        DEPENDS ${ZEN_CONFIG_SCHEMA}
        COMMENT "Generating Zen config bindings"
)

add_custom_target(zen_config_codegen ALL
        DEPENDS ${ZEN_CONFIG_GEN_H} ${ZEN_CONFIG_GEN_CPP}
)

include_directories(${ZEN_CONFIG_GEN_DIR})