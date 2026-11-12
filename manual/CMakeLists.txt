# A weave's manual as the weave carries it (`#include "manual/manual.hpp"`) and the door a role
# answers for the shapes it accepts (`manual/vocabulary.hpp`). Header-only over loom::core, so it
# exists where no kernel does; a page is compiled into a target by cmake/ZengineManual.cmake, which
# the package installs, so a stranger's weave carries its manual as the house's do.
# Reference: manual/docs/manual.md.
add_library(zengine-manual INTERFACE)
target_include_directories(zengine-manual INTERFACE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/zengine>)
target_link_libraries(zengine-manual INTERFACE loom::core)
add_library(zengine::manual ALIAS zengine-manual)
