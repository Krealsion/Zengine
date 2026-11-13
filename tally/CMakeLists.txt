# The bus tally (tally/README.md): where a running bus spends its time, off Loom's own stamps, by a
# host's observer. Header-only; Workshop, the app host and the snake each keep one and say it at
# the end.
add_library(zengine-tally INTERFACE)
target_include_directories(zengine-tally INTERFACE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/zengine>)
target_link_libraries(zengine-tally INTERFACE zengine::graphics zengine::input loom::switchboard)
