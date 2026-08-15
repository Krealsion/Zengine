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

## The editable line is a WINDOW onto the command (HD-4)

`TerminalInput` owns `first_visible` beside its text and caret, and the three move together
because the operations are the only way any of them changes. The capacity is never guessed: it
is `terminal_input_place(sc).columns`, the same number the painter cuts the slice with and the
same one a press is answered against.

```text
always, after every operation      0 <= first_visible <= caret <= size()
                                   first_visible is on a character boundary
after keep_caret_visible(N)        caret - first_visible <= N
                                   first_visible <= max(0, size() - N)
```

- **`refresh_terminal` reconciles it, once per repaint, above the `attached` early return.**
  That is the guarantee the whole design rests on — the window a press is answered with is the
  window the last repaint drew, and a resize needs no path of its own because a new extent
  causes a repaint. Do not move the call below the return: `paint_terminal` draws the pane
  whenever it is OPEN and `terminal_key` edits the line on the same condition, so a maker can
  type into a pane with no participant mounted.
- **The left edge snaps FORWARDS** (`character_boundary_at_or_after`, `workshop/property.hpp`).
  Snapping backwards is the obvious spelling and it is wrong here: it carries the window's
  right edge back with it and pushes the caret one to three columns off the row. The
  right-hand cut is a **byte** cut, as `detail::fit` and `project_text_regions` have always
  been — snapping it back would shorten the row under a caret column computed from the window,
  which is how a cell medium's caret falls off its own row.
- **`terminal_caret_column`/`terminal_caret_of_column` take `first_visible` and it is not
  defaulted.** A default would let a call site keep HD-3's two-argument spelling and be
  silently correct until the first line long enough to scroll. When the compiler stopped every
  such site, that was the parameter doing its job.
- **`kTerminalCaretCols` is one column of the row the line may not use.** A caret is *between*
  characters, so the one after the last character of a full row needs somewhere to be: a
  window has `kTextInsetPx` and `plan_caret` puts the bar at column `fit.columns` legitimately,
  but `project_text_regions` inserts a *character* and then cuts at the region's width, so a
  cell medium has to be given a cell. One rule for both media on purpose — a capacity that
  branched on the medium would make the two scroll to different places for a reason invisible
  in either projection.
- **No marker, no gesture.** There is no `<`/`>` hidden-content indicator, no scrollbar, no
  wheel or drag scrolling and no scroll command; the viewport moves only because the caret did.
  Adding one is not free: its width would have to come out of the same one capacity.

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

## Editing text is a COMPONENT now, and it belongs to neither consumer (HD-5)

`zengine::component::TextBox` (`component/text_box.hpp`) owns text, a caret and a horizontal
window as one state, with the operations as the only door. `workshop::TerminalInput` is gone —
it moved out whole and was renamed once — and `workshop::Row` holds one too, so the Terminal's
command line and an Inspector property draft are two instances of one implementation.

```text
what the component owns      text, caret, first_visible; the character walk; the four
                             caret-follow rules; the slice; column <-> byte, through the window
what a consumer owns         the capacity (an ARGUMENT), where its prose begins, and what the
                             text MEANS -- submit, parse, validate, commit, refuse, complete
```

- **`zengine-component` links nothing**, not even `loom::core`. A TextBox has no wire form,
  nothing serializes it and nothing hosts it; the absence of that link is the enforcement.
- **The character helpers moved with it.** `is_continuation_byte`, `character_before`,
  `character_after`, `character_boundary` and `character_boundary_at_or_after` were
  `workshop/property.hpp`'s and are now the component's, because both consumers that spend them
  ARE the component. `erase_one_character` was **deleted** — it erased from the END of a line,
  which is the only edit a draft with no caret could make.
- **`terminal_caret_column`/`terminal_caret_of_column` take the BOX**, not two indices. HD-4
  made `first_visible` non-defaulted so no call site could keep the old spelling and be
  silently right until the first line long enough to scroll; taking the component makes that
  hazard unsayable — there is no argument left to forget, and no way to pair a caret from one
  line with a window from another.
- **Do not give the component a focus flag, a filter, a selection, a clipboard or a blink.**
  Neither consumer has asked, and the pre-Zen `Zen::TextBox` in `reference/` had four of those
  and could not move its caret.

## The Inspector's property BODY, resolved once (HD-5, widened by HD-6)

`inspector_body_place(panel_bounds, screen, session)` (`workshop/screen.hpp`) is the whole
property body — where it is, how many rows of the ACTIVE medium's type fit in it, how wide a
value may be, and which properties the window is showing. The painter, the caret,
`refresh_inspector`, the vertical window and `info_press` all call it. Do not add a
`click_property_bounds()` beside a `paint_property_bounds()` here.

- **The body is ONE region and a property row is one of its rows**, mark and name included.
  HD-5's shape was the other way round — the mark and the name were labels beside a
  one-cell-tall region carrying only the value — and that is the shape that could not hold a
  line of the face (`(12 - 2*inset) / 18` = **zero** rows). A property row could not simply be
  given two cells: a two-cell region covers the property beneath it. Taking the room once, for
  all the rows together, is what let `fit_region` answer the whole question in one equation.
- **Nothing in Workshop multiplies a font metric.** `fit_region` returns `rows` and `columns`
  with `kTextInsetPx` already inside them, so there is no Inspector row height, no second font
  path and no `22px` constant. 25 cells of body is 16 rows of an 18-pixel face and 25 rows of a
  cell medium — two honest projections of one body (`value_columns` moves with them:
  `fit.columns - kPropertyMarkCols - kPropertyLabelCols - kPropertyCaretCols`).
- **`kPropertyCaretCols` is one column of the row the value may not use**, for
  `kTerminalCaretCols`' reason: a caret is *between* characters and a cell medium has no
  half-cells.
- **The vertical window is `list_window`, the OBJECTS list's own function** — the same three
  rules (a population that fits is shown whole; the focused row is always in the window; every
  omission is counted on its own side and spends a row of the budget) and the same wording
  (`omitted_text`). It is derived every paint and stored nowhere: there is no scroll offset,
  no session field and no scroll gesture. `completion_first_shown` is deliberately NOT the
  same function — that list anchors to the tail and never says `before`, because its heading
  already reads `3-5 of 9`.
- **`inspector_focus` is the row that must stay visible: the editing row, else the cursor.**
  They are the same row today by a reachability argument, and the function exists anyway —
  HD-5's lesson about orderings that rest on reachability proofs, one refactor from being
  silently wrong.
- **`prose_row_of_property` and `property_at_prose_row` are inverses and there is no third
  copy.** The painter positions the caret with the first and a press resolves with the second,
  so a scrolled body cannot land a click one row off the caret. A press is never rounded to a
  Workshop cell: an 18-pixel row against a 12-pixel cell would name the wrong property for
  most of the body.
- **A resting value is FITTED and a live draft is WINDOWED**, and the difference is the point:
  `detail::fit` marks what it cut (`the-quick-brow...`) because a committed value has no caret
  to tell a maker it moved; `TextBox::visible` does not, because a draft has one (HD-4's rule).
  Before HD-6 a committed value simply ran off the canvas — 14 characters, unmarked, measured.
- **`refresh_inspector` reconciles the draft's window once per repaint**, beside
  `refresh_terminal` and for the same reason: the window a press is answered with must be the
  window the last repaint drew, and a resize is not an edit. At most one row is ever editing —
  `begin_edit` is reachable only from command mode, which is exactly the state in which no row
  is being edited.
- **A `SurfaceExtent` must not drop a live draft.** `refocus_keeping_draft` rebuilds the rows
  (the resolved row closes over the extent) and hands the draft, its refusal and the cursor
  back. Every *other* `rebuild_rows` caller follows a change of selection or of document, where
  dropping it is right — `Name` is a row every object has, so a draft carried across a selection
  would arrive on a different object's property wearing the same label.
- **The Info panel publishes a region on every paint now**, so a test that wants the Terminal's
  pane must ask for it by its PLACE (`Screen::terminal_x`/`terminal_y`) and not as `texts[0]`.
  Painter's order is: panels first, then the overlay, then the completion list.

## A region too small for the face is a CELL region (HD-5)

`fit_region` falls back to the region's own cell bounds when a real metric yields no rows or no
columns, and `plan_canvas`/`plan_text_regions` partition on `fit_region(r, metric).graphical()`
rather than on the metric alone. Before this, a region one cell tall was in **neither** list and
was drawn by nobody — reachable, and reached, by the Inspector's editable row. `fit_region`'s
answer is byte-for-byte the one a faceless medium gets, so no canvas painted in a character
medium moved.

HD-6 did **not** special-case the Inspector past this. It succeeded by granting the body enough
room, not by lying to the Surface layer, and a body still too short for one line of the face
still resolves to cells and is still drawn by exactly one of the two lists.

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
- Assertion totals (**35,278** over the **nine** doctest binaries, SDL lane, measured
  2026-08-14 after HD-6) are evidence to report. They are **not** a population, never an
  acceptance oracle, and not coverage. The count of suites said "seven" here until HD-2
  counted them, which is the same decay this bullet warns about arriving in the sentence
  that warns about it — and HD-4 found the *arithmetic* had decayed the same way: the
  figure written after HD-3 summed seven of the eight, leaving `audit_probes` out of a
  total that said eight. It is the sum of all of them, named so the next phase can
  reproduce it: `zengine-surface-tests` 6,226 · `zengine-workshop-tests` 19,215 ·
  `zengine-component-tests` 2,153 · `zengine-builder-tests` 4,330 · `zengine-input-tests` 1,374 ·
  `zengine-timer-tests` 1,380 · `zengine-tests` (snake) 364 · `zengine-ui-tests` 164 ·
  `zengine-audit-probes` 72. HD-5 added a NINTH binary and Workshop's own total FELL by 1,318
  while the repository's rose by 871, which is the sharpest illustration this bullet has of why
  the figure is not an oracle: four cases moved out of Workshop into the new component suite,
  so a suite losing assertions and a repository gaining them are the same event.
  The figure is configuration-dependent —
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
