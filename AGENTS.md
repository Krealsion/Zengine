# AGENTS.md — Zengine

Docs router: **`docs/README.md`** (Timer guides/reference/laws live here;
substrate truth lives in `../Loom/docs/`, machine router
`../Loom/docs/CONTEXT.md`).

## Build / test (canonical: WSL; consumes an *installed* Loom)

```bash
# once, in ../Loom:  cmake --build build -j && cmake --install build --prefix build/_install
cmake -S . -B build -DCMAKE_PREFIX_PATH="$PWD/../Loom/build/_install"
cmake --build build -j"$(nproc)"
cmake -DZEN_BUILD_DIR=build -P tests/verify.cmake        # the official lane

# sanitizer lane: the same suites under ASan + UBSan (W-3a)
cmake -S . -B build-san -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_PREFIX_PATH="$PWD/../Loom/build/_install" -DZENGINE_SANITIZE=ON
cmake --build build-san -j"$(nproc)"
cmake -DZEN_BUILD_DIR=build-san -P tests/verify.cmake
```

- **Run `tests/verify.cmake`, not a bare `ctest`**, whenever a result is going
  to be quoted. A bare `ctest` still works and still runs the tests; what it
  cannot tell you is whether the population that ran is the population this
  repository meant to run (see below). Pass extra CTest flags with
  `-DZEN_CTEST_ARGS=-V`.
- Stranger-by-default is deliberate (`ZEN_LOOM_DEV=OFF`): an unexported-surface
  mistake must fail on every machine, not only in CI.
- Per-repo green: Zengine's lane never re-runs Loom's suite — state which
  repo's green you proved.
- Weave libraries go through `zengine_weave()`, which delegates the reloadable
  lifetime to the Loom's `loom_weave_build_contract()` (KERN-05). Do not
  reintroduce a private compiler flag for it here.
- Suites are separate binaries (`zengine-timer-tests` etc.); ctest runs them
  all, plus compile-negative targets judged on their diagnostics.

## The sanitizer lane is a SECOND kind of evidence (W-3a)

`-DZENGINE_SANITIZE=ON` (ASan + UBSan, non-recovering) runs the same population
under instrumentation; CI runs it on every push and PR. It is not a second
correctness lane and it does not replace `verify.cmake` — the ordinary lane asks
whether the intended population ran and passed, which cannot see a defect whose
symptom is that *no answer changes*. Measured, both ways round:

```text
a Placed bound into a temporary Scene   ordinary PASSES   ASan  heap-use-after-free
resolve_extent without its guard        ordinary PASSES   UBSan signed integer overflow
```

- W-2 shipped the first in committed test code; W-3 found signed-overflow UB in
  `ui::Rect::contains`, in shared `ui/` code on the ordinary press path. Both
  were called green by the ordinary lane. That is why this lane exists.
- **A new target lists `zengine-sanitize` beside `zengine-warnings`.** Omitting
  it does not fail anything — it silently drops that target out of the witness,
  which is the one way this lane degrades.
- It instruments what this repo AUTHORS, not the Loom it consumes (the Loom runs
  the same lane over itself). ASan's allocator is process-wide, so a Loom
  allocation misused by Zengine code is still caught.
- It runs the **full** population, SDL skin included (`gates active: always;sdl`,
  `surface` 22). Do not lower a floor or drop a gate to buy instrumentation.

## The population contract (C4, POP-01/POP-02)

A green here means the intended test population existed and ran. Four things
have to hold, and `tests/verify.cmake` requires all four:

```text
the declared CTest entries exist, exactly — no more, no fewer
each doctest surface selects at least its declared case floor
a doctest run that selects zero cases is a FAILURE
the tests themselves pass
```

- `tests/test_population.txt` is the **expectation** — entry, kind, gate, and
  either a case floor or the diagnostic a compile-negative test must be judged
  on. It lives in the source tree precisely so that deleting a registration
  cannot delete the expectation with it. Adding, renaming or removing a CTest
  entry is a deliberate edit to that file.
- Register tests through `zengine_doctest_test()` / `zengine_compile_test()` /
  `zengine_program_test()` (top-level `CMakeLists.txt`), never a bare
  `add_test()`. The helpers record what kind of evidence the entry is; the
  verifier refuses an entry it finds registered and unrecorded.
- `tests/doctest_main.cpp` is the one `main()` every runtime suite links.
  Stock doctest exits **0** on `--test-case=<no match>` ("Status: SUCCESS!");
  this one exits **70** and says `EMPTY TEST POPULATION`. The verifier
  re-proves that on every run, per binary, with a filter that matches nothing.
- Floors are **minimums** anchored to a measured baseline (`snake` 22,
  `timer` 78, `input` 24, `surface` 22 + 1 `sdl`, `ui` 22, `workshop` 119,
  `audit_probes` 4). Additions are free; a deletion is a red. Do not lower a
  floor to make a deletion pass. Read them from `tests/test_population.txt`,
  which is the contract; this list is a convenience and can go stale.
- **Moving evidence between suites does not lower the source suite's floor.**
  W-1 relocated the authored/resolved vocabulary out of Workshop into `ui/` and
  Workshop's floor went **up**, 20 → 22: what `resolve`/`hit` DO became the
  `ui` suite's claim, and Workshop kept a case for each proving its own answers
  come from there. A relocation that made the old floor fall would have moved
  the guarantee out of watch, not out of the file.
- Assertion totals (~2,450) are evidence to report. They are **not** a
  population, never an acceptance oracle, and not coverage.
- Configuration-dependent populations are **declared**, not absorbed: the one
  SDL-gated case in `test_surface.cpp` is its own manifest row, so the Windows
  lane's `-DZENGINE_SDL_SKIN=OFF` reads 16 and Linux reads 17 with no slack in
  either.
- The verifier verifies the **configured build tree it is handed**; producing a
  current one is the job of whoever configures and builds.

## The suites need a Loom that can host weaves (POP-03)

Every suite but `smoke` drives real weave libraries through the real kernel, so
`BUILD_TESTING=ON` (the default) **requires a Loom exporting `loom::kernel`** —
always present on Linux; on Windows only under the Loom's opt-in
`LOOM_ENABLE_WINDOWS_KERNEL`, which `-DZEN_LOOM_DEV=ON` sets for you.

Against a kernel-less package, `tests/` now **fails configuration** with an
actionable message. It used to `return()` quietly, and `ctest` then printed
"100% tests passed" over the one surviving smoke test — from the *supported
default* Windows Loom package, not from a typo (COLD-1 F-25).

`-DBUILD_TESTING=OFF` is the supported library-only configuration: it gates the
tests and nothing else. Against a kernel-full Loom it still builds every weave
library and registers no tests; against a kernel-less one it configures and
builds the kernel-independent surface (the activation cursor and the
timer/input/surface vocabularies — all header-only). See
`../Loom/docs/laws/population-laws.md`.

## Do not assume

- `TimedWeave` bindings reconcile immediately — they are authored, reconciled
  at activation and `TimerReady` only (TIMER-05).
- Timer handoff carries deadlines — it carries **remaining durations**
  (TIMER-03).
- `TimerReady` may precede the continuity decision (TIMER-04).
- A raw `on(const loom::Activated&)` is allowed on a `TimedWeave` — it is a
  compile-time refusal; use `on_timed_activation`.
- Loom laws stop applying here — they all do; see `../Loom/docs/laws/`.
