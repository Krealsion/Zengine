# Verification method — walls

Register `VM-WALL`: compile-time walls and source tripwires — where a wall is anchored, what it
probes, how a refusal is judged, and what a grep over the source can honestly claim. One method
per heading; cite by ID. Router: [`../verification.md`](../verification.md).

## VM-WALL-01 — Anchor a wall where every instance runs

METHOD — Anchor a compile-time wall where every instance runs — a constructor — never in an optional call; a check the author may never instantiate is a check that is not there.
BECAUSE — the visibility assert sat in an optional call, so a weave that followed the layer's
own guidance for a runtime delay never instantiated it and got seventy-two lines of template soup.
SEEN — `tests/CMakeLists.txt` `timer_missing_using_no_binding_refused`; `timer/binding.hpp`.

## VM-WALL-02 — Probe the one shape a derived class may never claim

METHOD — Probe the one shape a derived class may never claim; a probe on a shape a correct author may legitimately redeclare is satisfied by their own handler and the wall stays silent.
BECAUSE — the probe asked for a handler a raw-protocol consumer legitimately writes, so a
correct author's own handler satisfied it and the missing `using` stayed invisible.
SEEN — `timer/binding.hpp`; `tests/CMakeLists.txt` `timer_missing_using_no_binding_refused`.

## VM-WALL-03 — Ask whether this compiler can answer the probe

METHOD — Ask of every wall whether THIS compiler can answer its probe for the shape a correct author writes: deducing from an overload set is not portable, and a relocated wall is low-risk only once every toolchain ran.
BECAUSE — a probe that deduced a class from an overload set fired on correct code under MSVC,
and nothing in the repository compiled there for six commits.
SEEN — `tests/CMakeLists.txt` `timer_missing_using_refused`; `tests/test_population.txt`.

## VM-WALL-04 — One compile-negative case per reachable path, with its control

METHOD — One compile-negative case per reachable path per supported toolchain, judged on its diagnostic text and paired with a positive control that must compile, or a fixture that never built also "refuses".
BECAUSE — "the build failed" is satisfied by any unrelated breakage; the diagnostic text is what
makes a refusal evidence, and the positive control is what says the fixture built at all.
SEEN — `CMakeLists.txt` `zengine_compile_test`; `tests/CMakeLists.txt`
`timer_activation_hook_compiles`; `tests/check_population.cmake`.

## VM-WALL-05 — A detection probe deduces only what it asks about

METHOD — A detection probe deduces only the thing it asks about: more template parameters than the question needs makes it ambiguous, and the check abstains on exactly the cases it exists to refuse.
BECAUSE — a probe with more template parameters than the question matched everything, went
ambiguous and reported not detectable, so the negative fixture compiled clean; the hand-proven
canary is what caught it, before any matrix.
SEEN — `timer/binding.hpp`.

## VM-WALL-06 — Two guards that defer to each other leave a hole

METHOD — Two guards that each defer to the other leave a hole between them: a wall that abstains needs the other diagnostic reachable in the same situations.
BECAUSE — a guard that abstains and delegates to the other diagnostic needs that diagnostic
reachable in the same situations; two that each defer leave the case between them to nobody.
SEEN — nowhere yet

## VM-WALL-07 — A deleted overload is proven in an ordinary case

METHOD — A deleted overload is proven by `static_assert(!std::is_constructible_v<...>)` in an ordinary case, without paying for a compile-negative entry.
BECAUSE — binding a long-lived reference to a temporary is a use-after-free whose first symptom
is nonsense output; the compiler is the only party that can catch it in time, and an ordinary case
can ask it without a compile-negative entry.
SEEN — `tests/test_builder.cpp` case `"PROJ-0: a build participant cannot be composed over a
temporary catalog"`.

## VM-WALL-08 — A defect nothing can observe gets a wall where the choice is made

METHOD — A defect nothing can observe gets a wall where the choice is made, not a test: when the falsifier for a repair comes back green, refuse the configuration that carried the defect.
BECAUSE — restoring the ownership passed serially, passed in parallel on a warm tree and was
invisible downstream, since a listing prints entry names and not commands; nothing could have
noticed it come back, so the guard went where the choice is made.
SEEN — `CMakeLists.txt` `zengine_compile_test`, `ZENGINE_COMPILE_FIXTURE_TREE`.

## VM-WALL-09 — A detector keys on a marker's value, never on a name's presence

METHOD — A detector keys on a marker's VALUE, never on a name's presence — a presence check fails open, in the widening direction; write the negative twin beside every marker check.
BECAUSE — a presence check was satisfied by a hand-written false constant and by a state field
that merely bore the name, exposing every field; a flag would be believed where a number is read.
SEEN — `surface/terminal_size.hpp` `measured`; `tests/check_law_register.cmake`.

## VM-WALL-10 — A source tripwire is a pure string check

METHOD — A source tripwire is a pure string check: a comment can trip it (reword the prose, never weaken the check), it forbids stems and not roles, and it matches a token, not a substring.
BECAUSE — a role in a grant says who may be spoken to and cannot become a load; the first draft
went red on exactly that, and a comment can trip the same check by saying the stem.
SEEN — `tests/test_workshop_load.cpp` case `"an artifact stem may not climb out of the host's
artifact directory"`, case `"a stem or a role that is not a NAME is refused"`;
`tests/test_population.txt`.

## VM-WALL-11 — A tripwire beside a behavioural case is defence in depth

METHOD — A source tripwire with prose stripped is defence in depth beside a behavioural case, and the manifest says which rows are tripwires and which are proofs.
BECAUSE — the reference member present, every by-value spelling absent and the deleted overload
present are three facts a grep can state; naming it defence in depth keeps it from reading as
behaviour, which a value declared in `main` can never be.
SEEN — `tests/test_population.txt`.

## VM-WALL-12 — When the bytes' meaning changed, only a tripwire can catch a retained branch

METHOD — When a version moves with no field moving — the bytes' MEANING changed — nothing but a source tripwire can catch a retained branch: grow its forbidden-token list per retired version and say in the case why.
BECAUSE — a retained branch for a retired version compiles, admits and behaves correctly for
every file that does not depend on the distinction, so no case fails and no id mismatches.
SEEN — `tests/test_workshop_persistence.cpp` case `"MIG-0/SC-8: the session reader owns no
historical shape and no conversion"`.
