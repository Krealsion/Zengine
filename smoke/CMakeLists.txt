# The minimal consumer. It links ONLY the Loom's exported public surface — if this builds,
# a stranger's build of the same shape builds too.

add_executable(zengine-smoke smoke.cpp)
target_link_libraries(zengine-smoke PRIVATE loom::core)

add_test(NAME smoke COMMAND zengine-smoke)
