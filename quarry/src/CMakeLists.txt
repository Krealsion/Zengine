cmake_minimum_required(VERSION 3.20)
project(ZenKernel LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


# Core kernel code → static library (or OBJECT if you prefer no .lib file)
add_library(kernel_core STATIC
    kernel.cpp
)

target_include_directories(kernel_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} zengine)

# The actual kernel executable links against the core
add_executable(kernel
    main.cpp
)

target_link_libraries(kernel PRIVATE kernel_core)

# Bootstrap DLL also links against the core (gets definitions)
add_library(bootstrap SHARED bootstrap.cpp)

target_link_libraries(bootstrap PRIVATE kernel_core)

# Windows needs these for dynamic loading (dlopen equivalent is already in kernel)
if(WIN32)
    target_link_libraries(kernel PRIVATE kernel32)
    target_link_libraries(bootstrap PRIVATE kernel32)
endif()

add_library(console SHARED console.cpp)
target_link_libraries(console PRIVATE kernel_core)
target_include_directories(console PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Windows extra
if(WIN32)
    target_link_libraries(console PRIVATE kernel32)
endif()

add_subdirectory(apps/builder)
add_subdirectory(zengine)
