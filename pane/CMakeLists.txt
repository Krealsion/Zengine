# The Pane package: the contract between a pane and the host that arranges it -- the shapes a pane
# and its host exchange, and the helpers a pane may draw its rows and picture and seat itself with
# -- with the presenter seam beside it. Header-only, and exported on its own (`zengine::pane`,
# cmake/ZengineInstall.cmake), so a pane built against the installed package can arrive by a plan
# row. In-tree panes and hosts reach it by include path, `pane/vocabulary.hpp`.
add_library(zengine-pane-vocabulary INTERFACE)
target_include_directories(zengine-pane-vocabulary INTERFACE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/zengine>)
add_library(zengine::pane ALIAS zengine-pane-vocabulary)
target_link_libraries(zengine-pane-vocabulary INTERFACE zengine-graphics-vocabulary loom::core)
