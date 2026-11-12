# The weave the shipped recipe catalog builds: Workshop's own example, built here and never loaded
# or staged by the host (`workshop/CMakeLists.txt`, the shipped recipe catalog).
include(${CMAKE_CURRENT_LIST_DIR}/../discipline.cmake)
zengine_weave(zengine-workshop-recipe-example example.cpp)
