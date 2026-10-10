# A weave's manual as the weave carries it (`#include "manual/manual.hpp"`) and the door an office
# answers for the shapes it accepts (`manual/vocabulary.hpp`). Header-only over loom::core, so it
# exists where no kernel does; a page is compiled into a target by cmake/ZengineManual.cmake.
# Reference: manual/docs/manual.md.
add_library(zengine-manual INTERFACE)
target_include_directories(zengine-manual INTERFACE ${CMAKE_SOURCE_DIR})
target_link_libraries(zengine-manual INTERFACE loom::core)
