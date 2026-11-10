# The Persist package: the file doors every durable file shares -- one bounded read, one safe write,
# and what a write came to. Header-only, read by Workshop's files and by the theme reader alike.
add_library(zengine-persist INTERFACE)
target_include_directories(zengine-persist INTERFACE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/zengine>)
add_library(zengine::persist ALIAS zengine-persist)
