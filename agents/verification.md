# Agent law — Verification

Routed detail behind [`AGENTS.md`](../AGENTS.md) — the lanes past the commands, the population
contract in full, and the repository's self-checks. The lane commands themselves are in the
core [`AGENTS.md`](../AGENTS.md#build--test-canonical-wsl-consumes-an-installed-loom); public
reference: [`../docs/contributing/build-and-test.md`](../docs/contributing/build-and-test.md).
Phase tags like (W-3a) are provenance markers into this repository's history; the law here is
current.

## The lanes, and what each can see

- **Run `tests/verify.cmake`, not a bare `ctest`**, whenever a result is going to be quoted. A
  bare `ctest` still works and still runs the tests; what it cannot tell you is whether the
  population that ran is the population this repository meant to run. Pass extra CTest flags
  with `-DZEN_CTEST_ARGS=-V`.
- **Stranger-by-default is deliberate** (`ZEN_LOOM_DEV=OFF`): an unexported-surface mistake
  must fail on every machine, not only in CI.
- **Per-repo green**: Zengine's lane never re-runs Loom's suite — state which repo's green you
  proved.
- **Weave libraries go through `zengine_weave()`**, which delegates the reloadable lifetime to
  the Loom's `loom_weave_build_contract()` (KERN-05). Do not reintroduce a private compiler
  flag for it here. A provider that is not a weave goes through `zengine_provider()`
  ([`operators.md`](operators.md)).
- **The package is a third kind of evidence, and neither ordinary lane can ask its question.**
  Both reach Zengine through its own build tree, where every header is on the include path and
  every artifact is staged; a requirement the *package* fails to carry is invisible there. Run
  `tests/package/run.cmake` after touching any public header, any exported target's link line,
  or `cmake/ZengineInstall.cmake`. It is not a CTest entry and `verify.cmake` will not run it
  for you. See [`packaging.md`](packaging.md).
- Suites are separate binaries (`zengine-timer-tests` etc.); ctest runs them all, plus
  compile-negative targets judged on their diagnostics.

## The sanitizer lane is a SECOND kind of evidence (W-3a)

`-DZENGINE_SANITIZE=ON` (ASan + UBSan, non-recovering) runs the same population under
instrumentation; CI runs it on every push and PR. It is not a second correctness lane and it
does not replace `verify.cmake` — the ordinary lane asks whether the intended population ran
and passed, which cannot see a defect whose symptom is that *no answer changes*. Measured, both
ways round:

```text
a Placed bound into a temporary Scene   ordinary PASSES   ASan  heap-use-after-free
resolve_extent without its guard        ordinary PASSES   UBSan signed integer overflow
```

- Both defects shipped green on the ordinary lane before this lane existed — one in committed
  test code, one signed-overflow UB in shared `ui/` code on the ordinary press path. That is
  why this lane exists.
- **A new target lists `zengine-sanitize` beside `zengine-warnings`.** Omitting it does not
  fail anything — it silently drops that target out of the witness, which is the one way this
  lane degrades.
- It instruments what this repo AUTHORS, not the Loom it consumes (the Loom runs the same lane
  over itself). ASan's allocator is process-wide, so a Loom allocation misused by Zengine code
  is still caught.
- It runs the **full** population, SDL skin included — the verifier prints
  `gates active: always;sdl`, and every floor is the one the ordinary lane clears. Do not
  lower a floor or drop a gate to buy instrumentation. The `sdl` gate opens the embedded
  typeface through SDL_ttf and FreeType against a real renderer, so this lane is where a leak
  or a misuse in that lifetime would be named.

## The population contract (C4, POP-01/POP-02)

A green here means the intended test population existed and ran. Four things have to hold, and
`tests/verify.cmake` requires all four:

```text
the declared CTest entries exist, exactly — no more, no fewer
each doctest surface selects at least its declared case floor
a doctest run that selects zero cases is a FAILURE
the tests themselves pass
```

- `tests/test_population.txt` is the **expectation** — entry, kind, gate, and either a case
  floor or the diagnostic a compile-negative test must be judged on. It lives in the source
  tree precisely so that deleting a registration cannot delete the expectation with it.
  Adding, renaming or removing a CTest entry is a deliberate edit to that file.
- Register tests through `zengine_doctest_test()` / `zengine_compile_test()` /
  `zengine_program_test()` (top-level `CMakeLists.txt`), never a bare `add_test()`. The
  helpers record what kind of evidence the entry is; the verifier refuses an entry it finds
  registered and unrecorded.
- `tests/doctest_main.cpp` is the one `main()` every runtime suite links. Stock doctest exits
  **0** on `--test-case=<no match>` ("Status: SUCCESS!"); this one exits **70** and says
  `EMPTY TEST POPULATION`. The verifier re-proves that on every run, per binary, with a filter
  that matches nothing.
- Floors are **minimums** anchored to a measured baseline. Additions are free; a deletion is a
  red. Do not lower a floor to make a deletion pass. The per-suite values live in
  `tests/test_population.txt` and nowhere else — a convenience copy of them kept here went
  stale twice before that rule was made. A second copy of a contract is not a convenience; it
  is a second answer.
- **Moving evidence between suites does not lower the source suite's floor.** When a
  vocabulary relocates to another package, what its operations DO becomes the new suite's
  claim, and the old suite keeps a case for each proving its own answers come from there — so
  the old floor rises or holds. A relocation that made the old floor fall would have moved the
  guarantee out of watch, not out of the file.
- **Assertion totals are evidence to report, never a population, never an acceptance oracle,
  and not coverage.** The figure is configuration-dependent (gated suites carry fewer cases
  where SDL is off), so it travels with the lane it was measured on, and it is dated because
  nothing enforces it: no contract file holds assertion counts, and a phase that adds a case
  moves the number without anything noticing. The repository's history shows every failure
  mode of reading it as more: convenience copies of it decayed in place (twice, including the
  arithmetic); a single property-sweep case can move it by ten thousand while pinning ONE law;
  a phase of distinct laws can move it by hundreds while changing far more of the repository;
  and cases moving between suites make one suite's total fall while the repository's rises.
  Report the total with its date and lane; read the per-suite floors in
  `tests/test_population.txt` as the contract.
- Configuration-dependent populations are **declared**, not absorbed: the SDL-gated cases are
  their own manifest rows, so a suite's floor is the SUM of the rows whose gate is active and
  the Windows lane's `-DZENGINE_SDL_SKIN=OFF` simply drops the `sdl` row, with no slack in
  either configuration.
- The verifier verifies the **configured build tree it is handed**; producing a current one is
  the job of whoever configures and builds.

## The suites need a Loom that can host weaves (POP-03)

Every suite but `smoke` drives real weave libraries through the real kernel, so
`BUILD_TESTING=ON` (the default) **requires a Loom exporting `loom::kernel`** — always present
on Linux; on Windows only under the Loom's opt-in `LOOM_ENABLE_WINDOWS_KERNEL`, which
`-DZEN_LOOM_DEV=ON` sets for you.

Against a kernel-less package, `tests/` **fails configuration** with an actionable message. It
used to `return()` quietly, and `ctest` then printed "100% tests passed" over the one surviving
smoke test — from the *supported default* Windows Loom package, not from a typo. See
`../Loom/docs/laws/population-laws.md` (sibling checkout; substrate truth is the Loom's).

`-DBUILD_TESTING=OFF` is the supported library-only configuration: it gates the tests and
nothing else. Against a kernel-full Loom it still builds every weave library and registers no
tests; against a kernel-less one it configures and builds the kernel-independent surface (the
activation cursor and the header-only vocabularies).

## The repository checks its own documentation and vocabulary

- **`doc_links`** (kind `script`; `tests/check_doc_links.cmake`). Every relative link in a
  current-facing `*.md` and its `#anchor`, plus every repository-relative `*.md` path written
  in a first-party C/C++ comment under any package directory or `tests/`, must resolve — a
  broken one is a RED in the official lane. Markdown is swept from the repository root, so a
  new documentation folder is covered the moment it exists; a comment's reference is resolved
  against the **repository root**, because a comment moves with its code. Excluded by written
  rule: `docs/history/` (frozen), `reference/` (the pre-Zen quarry), vendored and build trees.
  A reference above the repository root — including anything under `../Loom/` — is counted and
  declined: this repository is verified as a standalone clone.
- **`package_vocabulary`** (kind `script`; `tests/check_package_vocabulary.cmake`). An
  **artifact** is the physical loadable file; a **weave** and a **provider** are runtime
  surfaces one may expose, and the installed package holds both kinds, so its public variables
  name the physical thing. The retired spellings may appear in exactly one file — the checker
  that declares them — and the check asserts both halves of that. It does **not** police the
  word *weave*: `zengine_weave()`, `WeaveId`, the weave ABI and weave-only guides all mean
  weave and must not be renamed. Its self-test makes the predicate say **yes** to a token in
  the tree and **no** to one that is absent before it answers.

## Platform traps

- **MinGW objects need `-mbig-obj` on `workshop.cpp`'s object, and that is not optional.** It
  instantiates enough templates that its `-g` debug COMDATs sit near COFF's 32-bit
  section-relative limit; one more inline function in a header it includes fails the link with
  `relocation truncated to fit: IMAGE_REL_AMD64_SECREL against .debug_frame$...`, which reads
  like a broken repository and is not one. `zengine-warnings` carries the flag under
  `if(MINGW)`. Do not remove it because a build happens to link without it today.
