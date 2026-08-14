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
- It runs the **full** population, SDL skin included — the verifier prints
  `gates active: always;sdl`, and every floor is the one the ordinary lane
  clears. Do not lower a floor or drop a gate to buy instrumentation.
- Since HD-1 that includes a real font engine: the `sdl` gate opens the embedded
  typeface through SDL_ttf and FreeType against a real renderer, so this lane is
  where a leak or a misuse in that lifetime would be named. It was, at the point
  of writing, clean.

## A row may sit on something, and it is one field (HD-2)

`SurfaceTextRow` carries `background`, a semantic role defaulting to `role::kNone` — the
**absence** of a ground, negative on purpose so the unknown-role fallback (`kFill`) can never
swallow it and a later fifth role cannot collide with it. Nothing passes it to a Skin's
role→ink table; a consumer tests for it first. It made `SurfaceTextRow` v2,
`SurfaceTextRegion` v2 and `SurfaceCanvas` **v3** — the latter two gained no field of their
own and changed anyway, because their wire identity is computed from the types they carry.
`project_text_regions` therefore returns `ProjectedRow{label, background}` rather than bare
labels; the ground travels **unresolved** through the cell projection and each medium answers
for itself.

**A canvas with no ground emits not one background byte**, which is why every pre-existing
terminal golden is unmoved — the assertion is in `test_surface.cpp`, and it is the thing to
re-check if this ever grows.

## A region may have a caret, and it is said in PROSE (HD-3)

`SurfaceTextRegion` carries `caret_row`/`caret_col` — a row and a column into the rows the
region carries, never a pixel and never a canvas cell. That is what lets each medium answer
with the metric it already resolved: `plan_caret` (`surface/skin_sdl_plan.hpp`) turns them
into a `kCaretWidthPx` bar off the same `RegionFit` the rows were positioned with, and
`project_text_regions` *inserts* `kCaretGlyph` at the same column, which for a caret at the
end of a line is byte-for-byte the row the Terminal used to append for itself. `kNoCaret` is
**negative** for `role::kNone`'s reason — a row index is non-negative by construction, so an
absence cannot collide with a row anybody meant. It made `SurfaceTextRegion` **v3** and
`SurfaceCanvas` **v4**; the canvas has now changed three times and never gained a field.

**A caret is an insertion point, so it is a bar and never a block**, and it is not a
selection, not a focus fact and not a clock. Two regions on one canvas may each carry one.

**The geometry that draws a thing and the geometry that hits it must be the same geometry.**
`terminal_input_place` (`workshop/screen.hpp`) is the pane's editable line resolved once, and
`paint_terminal`, the caret and `terminal_press` all call it. `completion_first_shown` was
lifted out of `completion_rows` for the same reason — a second copy of the list's windowing
is right until the first scroll, which is to say wrong only when nobody is looking. Do not
add a `click_*_bounds()` beside a `paint_*_bounds()` here.

**`scan::kHome`/`kDelete`/`kEnd` are NAMES for values that already arrived**, not new reach.
`translate_sdl.hpp` passes SDL's scancode through untranslated, so the SDL backend has always
delivered them unnamed; the POSIX terminal backend drops their CSI sequences and the Win32
console backend maps their VKs to `kUnknown`. Neither was widened, and a constant here is not
a claim that every backend can produce one — `translate.hpp` remains where each backend's
honest reach is written.

**Do not save and restore a renderer viewport through `SDL_GetRenderViewport` alone.** SDL
keeps two states and that call flattens them: a renderer with no viewport of its own answers
with the whole target's rectangle, and setting *that* back makes the viewport explicit, after
which SDL stops growing it when the output does. HD-1 shipped that; the symptom is a window
dragged larger whose picture is still clipped to the old size, one frame after the part drawn
inside a region has already reflowed. Ask `SDL_RenderViewportSet` first and restore `nullptr`
when it says false (`surface/skin_sdl_text.hpp`, pinned in the `sdl` gate).

## The graphical Skin carries a typeface (HD-1)

`ZENGINE_SDL_SKIN=ON` now fetches **three** pinned-and-checksummed dependencies,
not one: SDL3, SDL_ttf, and — because SDL_ttf hard-requires FreeType and its
release tarball deliberately does not bundle it — FreeType, extracted into
SDL_ttf's own `external/freetype` before its subdirectory is added.
`cmake/ZengineSdl.cmake` owns that assembly, states why both of SDL_ttf's own
doors were measured shut on the lanes this repository builds on, and fails with
an actionable message if the two contents' download stamps ever fall out of step.

The face itself (`surface/fonts/JetBrainsMono-Regular.ttf`, SIL OFL 1.1) is
**bundled and distributed**, unlike the fetched libraries: `cmake/EmbedBinary.cmake`
turns its bytes into a translation unit compiled into `zengine-skin-sdl`. Nothing
is installed, staged or discovered at runtime. Provenance and obligations:
`surface/fonts/PROVENANCE.md`, `THIRD_PARTY_NOTICES.md`.

**MinGW objects need `-mbig-obj` here, and that is not optional.** `workshop.cpp`
instantiates enough templates that its `-g` debug COMDATs sat just under COFF's
32-bit section-relative limit; HD-1 added inline arithmetic to a header it
includes and the link failed with `relocation truncated to fit:
IMAGE_REL_AMD64_SECREL against .debug_frame$...`, which reads like a broken
repository and is not one. `zengine-warnings` carries the flag under `if(MINGW)`.
Do not remove it because a build happens to link without it today.

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
- Floors are **minimums** anchored to a measured baseline. Additions are free; a
  deletion is a red. Do not lower a floor to make a deletion pass. The per-suite
  values live in `tests/test_population.txt` and nowhere else — this file used to
  carry a convenience copy of them, which had gone stale in three entries before
  BL-VER-04 re-derived it and was stale again by VOLATILE-2. A second copy of a
  contract is not a convenience; it is a second answer.
- **Moving evidence between suites does not lower the source suite's floor.**
  W-1 relocated the authored/resolved vocabulary out of Workshop into `ui/` and
  Workshop's floor went **up**, 20 → 22: what `resolve`/`hit` DO became the
  `ui` suite's claim, and Workshop kept a case for each proving its own answers
  come from there. A relocation that made the old floor fall would have moved
  the guarantee out of watch, not out of the file.
- Assertion totals (~31,675 over the **eight** doctest suites, SDL lane, measured
  2026-08-14 after HD-3) are evidence to report. They are **not** a population, never an
  acceptance oracle, and not coverage. The count of suites said "seven" here until HD-2
  counted them, which is the same decay this bullet warns about arriving in the sentence
  that warns about it. The figure is configuration-dependent —
  the two gated suites carry fewer cases where SDL is off — so it travels with
  the lane it was measured on, and it is dated because nothing enforces it: no
  contract file holds assertion counts, and a phase that adds a case moves this
  number without anything noticing.
- Configuration-dependent populations are **declared**, not absorbed: the
  SDL-gated cases in `test_surface.cpp` — and, since G-1, in `test_input.cpp` —
  are their own manifest rows, so a suite's floor is the SUM of the rows whose
  gate is active and the Windows lane's `-DZENGINE_SDL_SKIN=OFF` simply drops
  the `sdl` row, with no slack in either configuration. The numbers themselves
  live in `tests/test_population.txt` and nowhere else — a worked example here
  went stale the first time a phase added an SDL-gated case, which is the same
  lesson as the per-suite floors two bullets up.
- The verifier verifies the **configured build tree it is handed**; producing a
  current one is the job of whoever configures and builds.
- **Documentation references are checked too** (`doc_links`, kind `script` — the
  one entry that reads the source tree rather than a build). Every relative link
  in a current-facing `*.md` and its `#anchor`, plus every repository-relative
  `*.md` path written in a first-party C/C++ comment under any package
  directory or `tests/`, must resolve — a broken one is a RED in the official
  lane. `docs/reference/foo.md` is the usual form, but the check is not limited
  to `docs/`: `README.md#surface--the-surface-package` is checked, anchor and
  all, which is what makes a README section a citable owner. A comment's
  reference is resolved against the **repository root**,
  because a comment moves with its code. Excluded by written rule:
  `docs/history/` (frozen), `reference/` (the pre-Zen quarry), vendored and build
  trees. A reference above the repository root — including anything under
  `../Loom/` — is counted and declined: this repository is verified as a
  standalone clone, and a stranger has no sibling to look at.
  `tests/check_doc_links.cmake`.

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
