# Verification method — lanes

Register `VM-LANE`: running the lanes — what each can see, when it runs in parallel, and the
tree a compile-judged entry writes. One method per heading; cite by ID. Router:
[`../verification.md`](../verification.md).

## VM-LANE-01 — Quote the lane, never a bare ctest

METHOD — Quote `tests/verify.cmake`, never a bare `ctest`, whenever a result is going to be quoted: the bare run cannot say whether the population that ran is the one this repository meant to run.
BECAUSE — a bare `ctest` accepts a selector that matched nothing and prints a full pass over it;
the lane checks the inventory, the floors and the empty-population refusal before it believes a
pass.
SEEN — `tests/verify.cmake`; `AGENTS.md`.

## VM-LANE-02 — Stranger-by-default is deliberate

METHOD — Stranger-by-default (`ZEN_LOOM_DEV=OFF`) is deliberate: an unexported-surface mistake must fail on every machine, not only in CI.
BECAUSE — a sibling checkout puts every Loom header on the include path, so a use of an
unexported surface compiles there and lands on the first stranger.
SEEN — `CMakeLists.txt` `ZEN_LOOM_DEV`; `.github/workflows/ci.yml` `ZEN_LOOM_DEV`.

## VM-LANE-03 — A green names its repository, configuration and compiler

METHOD — A green names the repository, the configuration and the compiler; Zengine's lane never re-runs Loom's suite, so "green" never silently means green in one of two.
BECAUSE — a dependency's proof rides its version: even under the sibling override this
repository's entries are its own, so a bare "green" cannot say which repository it is about.
SEEN — `.github/workflows/ci.yml`; `AGENTS.md`.

## VM-LANE-04 — Windows is two standard libraries

METHOD — Windows is two standard libraries and a Windows claim names the one it was measured on: MinGW-w64/libstdc++ is REQUIRED, MSVC's STL is ADVISORY, and neither is evidence for the other.
BECAUSE — measured on the Files browser, libstdc++ reports a directory junction as a directory
unfollowed and MSVC's STL reports the junction; the MinGW lane was red on exactly that difference.
SEEN — `.github/workflows/ci.yml` `continue-on-error`; `tests/test_workshop_files.cpp`, the
comment on the WL-FILES-14 second-clause case.

## VM-LANE-05 — A job under continue-on-error is red only in the jobs list

METHOD — A job under `continue-on-error` is red only in the run's jobs list, never in its conclusion: read a CI run job by job before quoting it.
BECAUSE — a lane was red for every run after the Files browser began marking junctions, and the
run's conclusion stayed green throughout; a red that cannot fail the run is not a lane.
SEEN — `.github/workflows/ci.yml` `continue-on-error`.

## VM-LANE-06 — CI runs on pull requests and on main

METHOD — CI runs on pull requests and on pushes to `main`; a branch push alone gets no run, so a claim of "CI green" names the pull request or the merge that ran it.
BECAUSE — the workflow's trigger is `pull_request` and `push` to `main` and nothing else; a
green quoted from a branch push is a green that never ran.
SEEN — `.github/workflows/ci.yml`.

## VM-LANE-07 — The sanitizer lane is a second kind of evidence

METHOD — The sanitizer lane is a second KIND of evidence: the same full population under ASan+UBSan, for defects whose symptom is that no answer changes; it never replaces the ordinary lane.
BECAUSE — a bound into a temporary scene and an unguarded extent overflow both passed the
ordinary lane green and were named only under instrumentation.
SEEN — `CMakeLists.txt` `ZENGINE_SANITIZE`; `.github/workflows/ci.yml` `ZENGINE_SANITIZE`.

## VM-LANE-08 — A new target lists zengine-sanitize beside zengine-warnings

METHOD — A new target lists `zengine-sanitize` beside `zengine-warnings`; omitting it fails nothing and silently drops the target out of the sanitizer witness, which is the one way that lane degrades.
BECAUSE — the interface target carries the instrumentation flags; a target that does not link it
builds uninstrumented and green, so the lane shrinks without a red.
SEEN — `CMakeLists.txt` `zengine-sanitize`, `zengine-warnings`; `workshop/CMakeLists.txt`
`zengine-sanitize`.

## VM-LANE-09 — The sanitizer lane runs the full population

METHOD — The sanitizer lane runs the full population, SDL gate included (`gates active: always;sdl`); never lower a floor or drop a gate to buy instrumentation.
BECAUSE — the `sdl` gate opens the embedded typeface through SDL_ttf and FreeType against a real
renderer, so this lane is where a leak or a misuse in that lifetime would be named.
SEEN — `tests/verify.cmake`; `tests/check_population.cmake`.

## VM-LANE-10 — The package witness is a third kind

METHOD — The package witness is a third kind: both lanes reach Zengine through its own build tree, so a requirement the package fails to carry is invisible to them; it is not a CTest entry.
BECAUSE — in the build tree every header is on the include path and every artifact is staged;
the witness installs, builds an unrelated project outside the tree, repeats against a moved
prefix, then deletes a header to prove it read the prefix.
SEEN — `tests/package/run.cmake`; `agents/packaging.md`.

## VM-LANE-11 — The lane may run in parallel on Linux/GCC

METHOD — The lane may run in parallel on Linux/GCC with `-DZEN_CTEST_ARGS=-j<n>`, same proofs; Windows stays serial because `timer` fails there under `-j` on a weave-load assertion, an open defect.
BECAUSE — no CTest entry writes this build tree, so the entries cannot race it; the `timer`
failure is measured at the same rate before and after the compile-test repair (two of six runs
after, four of six before), so it is a separate defect.
SEEN — `tests/verify.cmake` `ZEN_CTEST_ARGS`; `AGENTS.md`.

## VM-LANE-12 — The filesystem decides parallel safety

METHOD — What decides parallel safety is the build tree's FILESYSTEM, not the platform: quote the filesystem with the parallel number, and fall back to serial before suspecting your own change.
BECAUSE — the same `timer` assertion fails the parallel sanitizer lane four of four on a
9p-mounted tree and zero of three on an ext4 tree built from the same source; serial is green on
both, and it is load-sensitive.
SEEN — nowhere yet

## VM-LANE-13 — A parallel lane's floor is its longest entry

METHOD — A parallel lane's floor is its LONGEST ENTRY, not its total, and one case can be the floor: ask CTest which entry is longest and doctest which case, before optimizing anything.
BECAUSE — the lane took as long as its one longest entry whatever the job count, and inside that
entry one property sweep was most of the time; nothing else moved the number.
SEEN — nowhere yet

## VM-LANE-14 — A speedup is not adopted until the full lane has run under it

METHOD — A speedup is not adopted until the FULL lane has run under it, on a named repository, configuration and compiler; the attractive ones are the ones that cost proof.
BECAUSE — a faster linker cut the inner loop and segfaulted the weave suites; parallel CTest
passed every single run and failed three of three once a regeneration was pending.
SEEN — nowhere yet

## VM-LANE-15 — One parallel run is not evidence of parallel safety

METHOD — One run of a parallel lane is not evidence that the lane is parallel-safe: repeat it, and repeat it under the condition that makes it fail (a CMake regeneration owed).
BECAUSE — the compile-judged entries raced the shared tree only when CMake owed it a
regeneration, which a warm tree never does; the single-shot green was true and meaningless.
SEEN — nowhere yet

## VM-LANE-16 — A verdict is the check's own exit code

METHOD — A verdict is the check's own exit code, written to a file and tested, never read through a pipe; the rule covers every lane whose verdict is quoted, not only harnesses.
BECAUSE — a lane's exit read zero through a pipe while the verifier was failing on a missing
directory; a pipe reports the last command and lets the FAILED line scroll past.
SEEN — `tests/verify.cmake` `RESULT_VARIABLE`; `.github/workflows/ci.yml` `PIPESTATUS`.

## VM-LANE-17 — A test that builds is a writer of the tree it names

METHOD — A test whose evidence is a build is a WRITER of the tree it names; give it a tree nothing else depends on, and never `${CMAKE_BINARY_DIR}`.
BECAUSE — a build command first brings the tree's build system up to date, unbounded work the
entry then owns; eight entries naming the shared tree failed five to seven of twenty-two under a
parallel run, three of three.
SEEN — `tests/CMakeLists.txt` `compile-fixtures`; `CMakeLists.txt` `zengine_compile_test`.

## VM-LANE-18 — The fixture tree is this same project

METHOD — The fixture tree is this same project entered under `ZENGINE_COMPILE_FIXTURES`, so fixtures compile as an in-tree target would; a private fixture project is a second copy of the contract.
BECAUSE — one warnings target, one sanitizer target, one vocabulary per package, and the
compiler, its launcher, the flags, the build type and the resolved Loom all pass through; a second
copy of a contract is a second answer.
SEEN — `tests/CMakeLists.txt` `ZENGINE_COMPILE_FIXTURES`; `CMakeLists.txt`
`ZENGINE_COMPILE_FIXTURES`.

## VM-LANE-19 — The private tree and the lock protect different halves

METHOD — The fixture tree has no FetchContent, cannot re-run CMake, and takes one writer at a time; the private tree and the lock protect different halves, and a lock alone leaves the ownership.
BECAUSE — a lock alone went green three of three while all eight entries still re-checked the
shared tree and one still ran CMake; a lock that makes a race serial has not moved the custody.
SEEN — `tests/CMakeLists.txt` `CMAKE_SUPPRESS_REGENERATION`; `CMakeLists.txt` `RESOURCE_LOCK`.

## VM-LANE-20 — The fixture tree is prepared at configure

METHOD — The fixture tree is prepared at configure, not as a build step, because this lane has no target a build-time dependency could hang from.
BECAUSE — configure, build one suite, run a compile test: a build-time preparation leaves that
sequence failing on a directory that does not exist.
SEEN — `tests/CMakeLists.txt` `ZENGINE_COMPILE_FIXTURES`.

## VM-LANE-21 — One owner per build tree holds inside one process too

METHOD — One owner per build tree holds inside one test process too: a case that runs two builds at once gives each its own tree, because the hazard is the generator's bookkeeping and differs between generators.
BECAUSE — Ninja recompacts its log through one fixed temp name per directory, so two builds
entered at once in one tree collide; measured nine of forty over the threshold, two of four
hundred on a settled log, none with a tree each, none under Makefiles.
SEEN — `tests/CMakeLists.txt` `build-fixture-b`; `tests/test_builder.cpp` case `"output is
attributed to its own operation, and two can run at once"`.

## VM-LANE-22 — Ask who ran CMake over the shared tree

METHOD — Ask who ran CMake over the shared tree (`ctest -V`: Re-running CMake, Configuring done), not whether it went red; a green on a warm dependency tree says nothing about the cold one.
BECAUSE — the locked version was green in twenty seconds and had been six hundred once, from
inheriting a cold dependency population; same code, same lock, two orders of magnitude apart.
SEEN — nowhere yet
