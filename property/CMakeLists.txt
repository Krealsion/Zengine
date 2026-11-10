# The Property package: a typed connection to one property of one thing, the text form a type is
# read and written in, and an inspector's row over a property. Header-only, read by Workshop's
# files, the editors, Files and Info alike; a write's outcome is the file library's (`persist/`).
add_library(zengine-property INTERFACE)
target_include_directories(zengine-property INTERFACE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/zengine>)
target_link_libraries(zengine-property INTERFACE zengine-persist)
add_library(zengine::property ALIAS zengine-property)
