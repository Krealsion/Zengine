# The bus tally (tally/README.md): where a running bus spends its time, read off Loom's own stamps
# by an observer a host adds. It is header-only; Workshop, the app and the snake example each keep
# one and print it as they end.
add_library(zengine-tally INTERFACE)
target_include_directories(zengine-tally INTERFACE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/zengine>)
target_link_libraries(zengine-tally INTERFACE zengine::graphics zengine::input loom::switchboard)
