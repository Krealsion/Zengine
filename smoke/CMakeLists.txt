# The minimal consumer. It links ONLY the Loom's exported public surface — if this builds,
# a stranger's build of the same shape builds too.

add_executable(zengine-smoke smoke.cpp)
# zengine-sanitize and nothing else local: it is compile/link FLAGS, so the sanitizer lane
# has no hole where the one entry that runs on every configuration sits, and the claim above
# is untouched — a flags-only interface target adds no Loom surface to what this links. It
# still does not take zengine-warnings, which would be a real addition to what a stranger
# writes.
target_link_libraries(zengine-smoke PRIVATE loom::core zengine-sanitize)

# Registered as a `program`: it carries no doctest population, so the contract the
# verifier holds it to is that the entry exists and this command still runs. No case
# floor is invented for it (C4).
zengine_program_test(smoke zengine-smoke)
