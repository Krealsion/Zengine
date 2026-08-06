# The minimal consumer. It links ONLY the Loom's exported public surface — if this builds,
# a stranger's build of the same shape builds too.

add_executable(zengine-smoke smoke.cpp)
target_link_libraries(zengine-smoke PRIVATE loom::core)

# Registered as a `program`: it carries no doctest population, so the contract the
# verifier holds it to is that the entry exists and this command still runs. No case
# floor is invented for it (C4).
zengine_program_test(smoke zengine-smoke)
