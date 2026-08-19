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
`SurfaceTextRegion` v2 and `SurfaceCanvas` **v3** (v5 since WIND-2a) — the latter two gained no field of their
own and changed anyway, because their wire identity is computed from the types they carry.
`project_text_regions` therefore returns `ProjectedRow{label, background}` rather than bare
labels; the ground travels **unresolved** through the cell projection and each medium answers
for itself.

**A canvas with no ground emits not one background byte**, which is why every pre-existing
terminal golden is unmoved — the assertion is in `test_surface.cpp`, over a canvas built by
hand for exactly that question, and it is the thing to re-check if this ever grows. It is
deliberately NOT asserted over a whole Workshop screen any more: HD-2 gave the Terminal's
completion list a `kMuted` ground for its selected row and HD-9 gave the Info panel three, so a
Workshop canvas now carries background bytes as a matter of course.

## A region may have a caret, and it is said in PROSE (HD-3)

`SurfaceTextRegion` carries `caret_row`/`caret_col` — a row and a column into the rows the
region carries, never a pixel and never a canvas cell. That is what lets each medium answer
with the metric it already resolved: `plan_caret` (`surface/skin_sdl_plan.hpp`) turns them
into a `kCaretWidthPx` bar off the same `RegionFit` the rows were positioned with, and
`project_text_regions` *inserts* `kCaretGlyph` at the same column, which for a caret at the
end of a line is byte-for-byte the row the Terminal used to append for itself. `kNoCaret` is
**negative** for `role::kNone`'s reason — a row index is non-negative by construction, so an
absence cannot collide with a row anybody meant. It made `SurfaceTextRegion` **v3** and
`SurfaceCanvas` **v4**; the canvas had by then changed three times and never gained a field.
WIND-2a is the first time it gained and lost some -- see the plane section below.

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

## The Info panel BODY, resolved once (HD-5, widened by HD-6 and again by HD-7)

`info_body_place(panel_bounds, screen, document, session)` (`workshop/screen.hpp`) is the whole
Info panel body — where it is, how many rows of the ACTIVE medium's type fit in it, how those
rows are shared between the OBJECTS list and the property list, how wide a value may be, and
which members each window is showing. The painter, the caret, `refresh_inspector`, both
vertical windows, `info_press` and `objects_press` all call it. Do not add a
`click_property_bounds()` beside a `paint_property_bounds()` here, and do not add an
`object_row_bounds()` beside either.

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

## Both of the Info panel's lists are rows of ONE region (HD-7)

`kListRows = 5` and `kRowsY = 8` are **gone**. How many objects the panel shows and where
`PROPERTIES` begins under them are answers about the room the active medium reports, and a
constant cannot hold either: on the pristine tree the list was five rows at 78×25, at 120×40
and at 240×80 while the property body beside it went from nine rows to sixty-four.

```text
row 0 .. objects_rows-1        the OBJECTS list, its markers included
row objects_rows               `PROPERTIES` -- a heading that MOVES with the composition
the next properties_rows       the property list, its markers included
everything after that          spare, and it is allowed to stay spare
```

- **One region rather than two, and the reason is arithmetic.** Splitting the panel's CELLS
  between two regions needs to know how many cells a run of rows costs, which is `fit_region`
  read backwards — a second arithmetic beside the one function that turns a metric into a
  capacity. One region asks `fit_region` once, gets a budget in PROSE ROWS, and spends it. The
  two sections then cannot overlap: they are disjoint runs of one budget, not two rectangles
  somebody has to keep apart.
- **`share_body_rows` is the whole composition policy**, and it is max-min fair sharing:
  *each list is given the rows its own population needs; what neither needs stays spare; and
  what they cannot both have is shared equally, with any part of a half a list does not need
  going to the other.* Four things follow and each is pinned as a property over every budget
  from 0 to 200: a list that fits gets exactly what it needs, spare room stays spare, growing
  the panel never shrinks either list, and **the 50/50 case is a consequence rather than a
  decision** — it is what "share what is contested equally" produces when both want more than
  half, and it stops the moment either wants less.
- **`OBJECTS` stays chrome on the panel's row 0** and `PROPERTIES` is a row of the body. That
  asymmetry is not an oversight: row 0 is SHARED with the screen's own `shift+space terminal`
  hint, and a region owns its interior. `OBJECTS` names the panel's column; `PROPERTIES` names
  a section inside the body whose position moves.
- **`prose_row_in_window`/`item_at_prose_row` are the one copy of the row arithmetic**, called
  twice. They own no items, no selection, no capacity and no keys — a helper, deliberately not
  a component, and deliberately not called `List`.
- **An object row is fitted WHOLE** (`object_row_text`). There is no `kObjectNameCols` beside
  the property row's mark and label columns, because an object row has no fixed column after
  the name to protect: cutting the row at the body's width cuts exactly the name and leaves the
  mark and the identity intact by construction. The identity comes before the name because a
  name is not an identity here — every object `n` makes is called `panel`.
- **A press on a visible object row selects it, in command mode only** (`objects_press`). The
  mode law: *while a property draft is live, a press on the object list changes no selection
  and says so.* Changing objects rebuilds the inspector rows, which is what a live draft cannot
  survive, and the three answers a press could give instead are three different sentences about
  a maker's unfinished work that nothing has measured a preference between. HD-6 refused the
  mirror of this question (a press does not BEGIN an edit) for the same reason.
- **`list_window`'s `rows < 3` branch is reachable now.** It was unreachable at `kListRows = 5`;
  a short panel gives a list a share of one or two rows, and what a maker then reads is
  `... 20 more` where the names would be — this place cannot show you an object AND tell you
  what it is hiding, so it tells you.

## The Info panel's third run is a FOOTER of controls, not a third list (HD-8)

The body's row budget now carries three runs, and the last of them is two rows a maker can
press: `[ Create ]` and `[ Delete ]`, the pointer's way to the acts `n` and `d` have always
performed.

```text
row 0 .. objects_rows-1        the OBJECTS list, its markers included
row objects_rows               `PROPERTIES`
the next properties_rows       the property list, its markers included
... spare, and still allowed to stay spare ...
capacity-kActionRows .. end    the controls -- anchored to the FOOT
```

- **The reservation is ONE subtraction and it happens before either list is offered anything**
  (`info_body_place`: `share_body_rows(capacity - 1 - kActionRows, ...)`). Controls are a
  FIXED demand and the lists are VARIABLE ones, so they are not a third claimant on
  `share_body_rows` — sharing is what two parties do when both want more than there is, and a
  control wants exactly one row at every size this panel has. There is no `-2 for buttons`
  anywhere else in the file; `InfoBodyPlace::action_row` is where the reserved rows are, and
  the painter and the press both ask for that number. Every HD-7 property survives the
  reduction, because a budget reduced by a constant is still a budget.
- **The footer is anchored to the FOOT, so the spare room falls between the properties and the
  controls.** Anchoring it to the row the property list happened to stop at is equally
  deterministic and puts the target somewhere new every time a maker selects an object with a
  different property count. A control that moves under the hand aiming at it is worse than an
  empty strip above it.
- **The body now publishes exactly `capacity` rows**, with the spare ones written as blank
  rows, because a region's rows are positional and the controls are at the end. That is the
  padding the object list's own share already used; what it costs is measured (a whole `paint`
  of a 74-row TUI body is 4.6 µs) and what it buys is that a second region — which would need
  to know how many CELLS a run of prose rows costs, i.e. `fit_region` read backwards — is not
  needed. HD-7 refused that arithmetic and HD-8 did not reintroduce it.
- **The minimum body grew by `kActionRows`** (`kInfoBodyMinRows + kActionRows`), and at the
  minimum graphical screen with six objects the two lists lost one row each — 4/5 became 3/4.
  That is the honest price and it is paid where a maker can see it, rather than by hiding the
  controls at small sizes.

**Availability is TWO REASONS, ONE BIT and TWO OWNERS**, and that distinction is the phase's
finding rather than a shape it inherited. The presentation needs one bit; the routing needs the
distinction, because the two reasons belong to different parts of the tool:

```text
kDraftLive   the APPLICATION owns it -- `actions_press` refuses BEFORE the operation, because
             `create_object`/`delete_object` know nothing about a live draft and would rebuild
             the inspector's rows out from under one. The sentence is `kFinishDraftFirst`, the
             one HD-7 wrote for a press on the object list.
kNoTarget    the DOCUMENT owns it -- the press goes THROUGH. `doc::remove` already refuses
             `no such object`, changes nothing and says so, so holding it back here would be a
             second copy of a refusal that exists and a second sentence for one state.
```

So: **a control never invents a reason; it defers to whoever owns the refusal, and holds a
press back only when nobody downstream would.** That is why this is not a `disabled` flag —
a flag collapses a fact the application must act on and a fact the document must speak for.

**And availability is not a prediction of every refusal.** An object something else measures
against cannot be deleted (`doc::remove`'s dependents policy) and a document can arrive from a
file with its mint spent. Neither makes a control unavailable: answering them in the
presentation would put a copy of the document's policy on the paint path, re-run every frame.
Availability is whether the act has a TARGET and whether the maker is FREE to act.

- **Unavailable is said in CHARACTERS**: `[ Create ]` is pressable and `( Delete )` is not, the
  same width either way. The muted role is the second signal and never the only one — the
  object list's `> ` mark, one run down, for the same reason: a terminal has no ground to tint.
  `[ ... ]` is also this tool's existing word for a pressable thing (`[ Build ]`, BLD-0); HD-8
  is the phase where one of them finally is.
- **The controls do not own the acts.** `actions_press` calls `create_object()` and
  `delete_object()` — the operations `command()` binds `n` and `d` to — so the two gestures
  converge on one document write, one selection rule and one sentence. There is no callback, no
  command id, no action registry and no `std::function`: the whole thing is a switch over two
  indices of a table. `n` and `d` are unchanged and a control does not know they exist.
- **`prose_row_of_action` / `action_at_prose_row` are inverses and there is no third copy**, the
  same pair the two lists have. A press is never rounded to a Workshop cell, and the footer is
  where that error would be largest.
- **Pointer order:** the terminal overlay (a MODE), then the active property editor, then the
  action controls, then the object list, then the panel's occupancy, then the workspace. The
  three runs inside the body are disjoint by construction, so the ordering among them is
  written down for HD-5's reason rather than because two of them could both answer.
- **`component::Button` was NOT extracted, and that is the reported result.** What Create and
  Delete share is a label, a bit, a bracket convention and a row — presentation with no
  invariant to keep. Consumer #2 cost four lines (an index, a label arm, an availability arm,
  a press arm) and no new geometry, input or paint path, which is what says the shape is a
  TABLE rather than a component. See `Zen/reportbacks/HD-8-RB.md` for the full comparison
  against the TextBox standard. Do not add `Button` until something owns a rule.
- **No focus framework, and none was earned.** The controls are pointer-only, keyboard command
  routing is untouched, and a `TextBox` still owns typing while editing. There is no
  keyboard-activation gesture for a control, so there is nothing for two owners to want.

## The Info panel's structural rows sit on a GROUND (HD-9)

Two rows of the Info body are set on something now, and they are the first consumers of
`SurfaceTextRow::background` outside the Terminal's completion list:

```text
`PROPERTIES`              role kAccent   background kMuted    a section BOUNDARY
[ Create ] / [ Delete ]   role kFill     background kMuted    a control that can be pressed
( Create ) / ( Delete )   role kMuted    background kNone     present, and not pressable now
every other row           role as before background kNone     whatever the region sits on
```

- **`surface/` did not change, and that is the phase's result.** No field, no role, no wire
  version, no renderer line. HD-2 shipped the capability and it stayed dormant in this panel
  for seven phases until two real consumers asked; what HD-9 spends is the vocabulary that was
  already there. (HD-7's and HD-8's notes that the field had *never* been used were wrong even
  when written — `completion_rows` has set its selected candidate on `kMuted` since HD-2.)
- **A ground is the WHOLE ROW in both media, and neither was taught that.** The cell
  projection pads every row to the region's width and carries the ground onto the padding
  (`project_one_text_region`); the SDL renderer fills a strip spanning the region's whole
  viewport (`skin_sdl_text.hpp`). Both were written that way by HD-2 for a *selected row*, and
  a control and a heading turn out to want exactly the same sentence: **this row, all of it.**
- **The two consumers use ONE ground, and that is agreement rather than sharing.** Each
  medium's palette offers exactly one ground that every ink reads on — `sgr_bg_for_role` says
  so in its own comment and `ink_for_role` bears it out — so a publisher that wants a legible
  grounded row has one honest choice. **Never pair a role with its own ground**: `kFill` on
  `kFill` is white on white in a terminal, and nothing refuses it. What tells the heading from
  the controls is what each already carried: accent ink and a section's name against fill ink
  and a bracketed verb. There is no `kSectionGround` constant, because two decisions that land
  on one value are not one decision.
- **An unavailable control loses the ground entirely rather than getting a quieter one.** That
  is what makes the ground mean *actionable* instead of *a control is here*, and it keeps
  availability out of being a matter of degree. HD-8's characters are untouched and still the
  first signal — `[ … ]` is what a medium with no ground reads, and `say_row`'s ground is the
  third signal after the brackets and the role, never a replacement for either.
- **The ground is presentation and it moved no geometry.** `kActionRows`, `share_body_rows`,
  the heading's row, the footer's foot anchor, all three inverse pairs, `info_body_at` and
  `actions_press` are byte-identical. The grounded strip a maker sees for prose row `i` is
  `[origin_y + i*line_px, origin_y + (i+1)*line_px)`, which is the identical partition
  `prose_row_of_pixel` inverts — pinned, boundary pixels included. Horizontally the slab is the
  region's viewport and the target is `0 <= column <= fit.columns` inside it, so a
  `kTextInsetPx` margin of the slab names no control; that margin predates HD-9 and the ground
  is merely the first thing that made it visible.
- **`component::Button` is still not earned** and neither is any interaction abstraction. A
  ground is a value on a row.

## The reserved column is nobody's to spend (HD-10)

The terminal pane's right edge is the **workspace's** right edge — `Screen::room_w` — and not the
screen's. Two expressions in `screen_of`, and they are the whole of the phase:

```text
pane_want    kTerminalWantW + (w - kScreenMinW)/2     G-2's half-share, unchanged
terminal_w   min(pane_want, room_w)                   the room is the ceiling
terminal_x   room_w - terminal_w                      anchored to the ROOM's corner
```

- **The defect this repairs is HD-9's, and it predates HD-9 by eight phases.** At *every* extent
  this composition lays out, the pane covered the whole 28-column width of the side region and
  between 8 and 37 of its rows — so the Info panel published its properties and its
  `[ Create ]` / `[ Delete ]` footer, and a later region cleared those cells to
  `kCanvasBackground` at the same moment. The eraser was the same colour as the canvas, so the
  panel read as *stopped* rather than *covered*, and a maker could not tell omitted from hidden
  from destroyed.
- **Nothing escaped its grant, and paint order was not the fault.** The Info body region is
  exactly its granted bounds less the heading row (`info_body_place`: `region_x = panel.x`,
  `region_w = panel.w`). Two *rectangles* claimed the same cells, and publication order was
  merely what decided the argument.
- **`screen_of` already knew the answer and was not asking itself.** It computes
  `room_w = panel_x - kPanelGap` for the workspace three lines above where it placed the pane,
  and `placement_bounds` puts the overlay stack inside the same number — asserted since PNL-1
  (`kMinStack.x + kMinStack.w == kMinScreen.room_w`). The pane was the one placement in the file
  older than that discipline. HD-10 invented no law; it brought the last presentation under the
  one that was already written down.
- **The law is about the RESERVATION, not about overlap.** Three overlaps remain and all three
  are intentional, each inside one owner's room: the completion list over the pane's transcript,
  the picker over the stack slot beneath it, and the pane itself over the workspace and the
  bottom band (which the screen answers for by not painting the notice or the help lines at all
  while the pane is open — the precedent this repair was modelled on). What is forbidden is
  reaching into the column the screen subtracted for somebody else. A test that forbade overlap
  generally would forbid all three.
- **Said twice, on purpose.** `terminal_x + terminal_w == room_w` and
  `terminal_x + terminal_w <= panel_x - kPanelGap` are both asserted, at `kMinScreen` in the type
  system and over eleven extents × three metrics in the suite. They are not redundant: a pane
  whose right edge sits exactly on `panel_x` shares no *cell* with the side region and is still
  wrong, and that mutation was measured passing the cell count while both edge assertions
  reddened.
- **`kTerminalMinW` is now `kTerminalWantW`, because the value stopped being a floor.** At 78, 79
  and 80 columns the room is narrower than 56 and the pane gets the room; the want and the room
  agree from 94 columns up, so the ceiling binds only where there is no half to take. The price
  is eight cells of pane at the minimum extent, and it is visible: the pane's standing statement
  (`SUBMITTED = authored; a sender is not told its fate`, 51 characters) is elided by
  `detail::fit` below 81 columns. That is pinned in both directions — the *answer* to `help` is
  still asserted whole, the legend's cut is asserted as present, and the extent where it stops
  being cut is named.
- **The Info panel did not move, reflow or shrink.** Every field of `info_body_place` and every
  published row — text, role and HD-9's grounds — is byte-identical with the pane open and with
  it closed, at every extent. The footer is a *fixed* demand anchored to the foot (HD-8) and a
  mode opening is not a reason for a target to move under the hand aiming at it.
- **One overlap between independent presentations is left, and it is measured rather than
  hoped:** at `kScreenMinH` exactly, the pane's top row is the overlay stack's first slot's last
  row — one row, and at the minimum width the slot's full width. (Nothing at any greater height
  *for the first slot*; a second slot is reachable since WP-0 put runtime panes in this stack,
  and it is where the worst case lives. WIND-1 widened the slot and therefore this overlap, and
  swept it: **504 shared cells** at worst, e.g. slot 1 at 640×26, against 432 before — bounded
  because the pane's left edge moves right at exactly the rate the slot's right edge does, so the
  contested columns never exceed `kTerminalWantW` at any extent. A full-width slot would make the
  overlap grow with the supported surface, reaching 3,033 cells at the 640-column maximum, which
  is why it is a half-share.) Both are overlays in the room the workspace has — the stack grows
  down from the top-left, the pane up from the bottom-right.
  Repairing it would mean reserving the stack's rows from the pane, which is a *second*
  reservation `screen_of` does not make and which would tie the pane's height to `kStackRows`.
  Pinned as a case so it is a known fact.
- **The composition answer is medium-independent by construction**, not by parity work: it is
  settled in canvas cells before any metric is consulted, and a metric only ever changes how much
  prose fits inside a placement it did not choose. Measured at 98×60 with the shipped face, the
  pane's clear rectangle ends at device pixel 816 and the Info body's viewport begins at 840; on
  the pristine tree the pane cleared 384..1176 and the body sat inside it.

## A wider room is shared by the pane and the maker (WIND-1)

An overlay-stack slot is no longer the same rectangle at every extent. `placement_bounds`
resolves its width to `kStackW + (sc.room_w - kStackW)/2` — the minimum composition's 48 cells
plus **half** the room's surplus over that, floored — while its column, its row, its height and
the blank row between slots are untouched.

```text
x       kStackX == 0                        unchanged
y       kStackY + clamped_slot*10           unchanged
width   48 + floor((room_w - 48)/2)         WIND-1
height  kStackRows == 9                     unchanged
```

- **It is `screen_of`'s own rule, not a new policy.** `pane_want = kTerminalWantW + (w -
  kScreenMinW)/2` is G-2's half-share for the terminal pane, three lines up; the base here is
  `kStackW` and the surplus is measured against `kMinScreen.room_w`, which the file has asserted
  are the same number since PNL-1. One expression, no threshold, no cap, no new constant.
- **The FLOOR is load-bearing.** At 79 columns the room is 49 and the surplus is exactly one;
  rounding up would spend it and leave nothing reachable. Floored, the odd column stays the
  maker's. `kMinScreen` is byte-identical — `{0,1,48,9}`, every `static_assert` unmoved.
- **A width is not a height.** `stack_slots_that_fit` reads `y` and `h`, so the vertical capacity
  is identical on the narrowest screen and the widest at every height. A width edit must not buy
  a slot, and the suite sweeps that.
- **Every added cell is paint AND pointer.** `paint_panel_frame` fills the whole rectangle,
  `occupied_at` owns the whole rectangle, and a press inside it is answered with the panel's
  sentence rather than reaching `take_hold`. So the honest half of the phrase is the *other*
  half: `room_w > kStackW` implies `x + w < room_w`, i.e. columns of the panel's own rows are
  still reachable at every extent above the minimum (1, 9, 21, 61, 281 at 79, 96, 120, 200, 640
  columns of surface). A full-width slot leaves zero, which is what this width was chosen
  against. A drag begun in that band still walks under the panel and releases normally (PNL-2).
- **An external pane's room follows it.** `external_body_place` is the slot less its header row
  and `refresh_external_rooms` grants `fit_region`'s answer over it, so a *wider surface* now
  moves a provider's budget where only a text metric could before — `8×48` at the minimum,
  `8×109` at 200×60; `5×71` and `5×163` under the 8×18 face. It is republished exactly when the
  capacity changes and never otherwise, and every grant clears the retained rows first, so a
  dragged window edge briefly shows `(waiting for the provider)`.

Setup bytes, the provider protocol, `PaneRef` identity, pane ordering, selection, and every
public API were untouched by WIND-1 itself. What it left absent — an authored size, a maker
override, a saved order — is what WIND-2 spends, below; docking is still absent and still refused.

## The code authors a default; the maker authors an override (WIND-2)

Setup format **version 2**. Each pane row carries a durable `PaneRef` plus the smallest authored
difference from the developer's answer, and nothing else:

```text
place  {mode, x, y}       mode: default | cells          absolute canvas cells, never an offset
width  {mode, amount}     mode: default | cells | pixels  per axis, independently
height {mode, amount}     mode: default | cells | pixels
front  integer            a permutation of 0..n-1 over ALL rows, 0 back-most
```

- **`default` is a VALUE, and its unused numbers must be zero.** Loom's admission refuses an
  unknown field and has no optional, so absence cannot be spelled by omitting one; and a magic
  coordinate is a value a maker could otherwise mean. Requiring the zeros is what gives absent
  intent exactly ONE canonical spelling — a file carrying `{"mode":"default","x":7}` would
  round-trip a number that means nothing, and the first reader to wonder what it meant would be
  right to.
- **The mode is a WORD in the file and a closed set**, `persist.hpp`'s own shipped decision about
  an extent mode, applied to the setup: the in-memory 0/1/2 are arbitrary and a renumber would
  silently change every saved arrangement. An unrecognised word refuses the WHOLE candidate and
  names both what it found and what would have worked. A PLACE has two words and a SIZE has three
  — `pixels` offered to a place is a word that field's vocabulary does not have.
- **The version and the envelope's shape version are ONE NUMBER**, with a `static_assert` making
  that a compile error to break. That is what buys the ORDERING the format needs: a version-1
  file's bytes claim `WorkshopSetup v1`, so the gate refuses it on the CLAIM before it has read a
  single pane row — and a version-1 file can therefore never be reported as *a pane row is missing
  `place`*, which is a true sentence about a false cause. `from_text`'s preflight reads that
  claimed version and says it in Workshop's own words, by number. The `format_version` FIELD is
  still checked afterwards, for the forgery only a reader of this format would produce.
- **`pixels` is declared, valid on every medium, and refused at PROJECTION on all of them.** No
  medium here publishes a trustworthy per-axis device-pixel scale for a canvas cell, and both
  near-misses are traps: `RegionFit::graphical()` identifies a medium that sets real *type* (a
  window whose font failed to open publishes `{w,h,0,0}` and still lays its canvas out at
  `kCanvasCellPx`), and `kCanvasCellPx` is ONE Skin's layout number that `surface/pointing.hpp`
  forbids Workshop to hold as a standing fact — a pointer may spend it only because the event
  carries `input::space::kPixels`, a stamp on that moment, and `SurfaceExtent` carries no such
  stamp. So the refusal is WHOLE, per `doc::resize`'s and `PaneContent`'s law: a pane with either
  axis in pixels is **not presented**, never presented at the default width with an honoured
  height. **Do not add a per-axis fallback**; it is exactly the silent default this refuses.
- **`front` is a canonical rank and never an accumulating counter.** `max + 1` is an operation
  TRACE: alternating `front(A)`/`front(B)` produces the same two semantic orders forever while the
  integers grow, so a legal gesture would eventually fail on a setup for which a bounded spelling
  always existed. A permutation of `0..n-1` is *unique* for a given order, so reset writes bytes
  identical to a setup that was never reordered — measured, not argued — and ten thousand
  alternating operations never leave the bound. There is no tie, so the resolved order needs no
  secondary key at all.
- **The rank is over ALL authored rows, including unresolved ones.** The presented order is that
  permutation RESTRICTED to what was seated, and a restriction of a total order is a total order —
  so a pane that stops resolving keeps its exact place for free and gets it back, with no byte of
  the file changing either way.
- **`panels.open` is NEVER reordered, and that is the whole of "raising a pane cannot move it".**
  `seat_panes` walks the setup LIST, `reconcile` assigns `panels.open` from that answer, and
  `bounds_of` counts a reactive slot over `panels.open`. No ordering operation writes anything any
  of the three reads. It is the absence of a write, not a rule somebody maintains. `presentation_order`
  is the one new pure function; paint walks it ascending and `occupied_at` descending.
- **An authored place spends no reactive slot and cannot WAIT for one.** A pane the maker put
  somewhere is not in the tiling, so asking the stack's slot arithmetic whether there is room for
  it is asking the wrong question — which narrows `waiting` to exactly what it has always meant:
  the reactive default ran out of tiles. Both `seat_panes` and `bounds_of`'s slot counter say it,
  and both are pinned.
- **The override is spent on `kOverlayStack` and nowhere else.** `screen_of` reserves the side
  column whether or not Info is open and `room_w` is what every share of the workspace resolves
  against, so a movable Info would change the resolved size of objects in a maker's document —
  PNL-0's refusal, unchanged. A side-region row's authored geometry is retained in the file, never
  rewritten, and never spent; management refuses to author one and says which reservation it hit.
- **The host clips and never rewrites.** `bounds_of` answers with the VISIBLE rectangle (resolved,
  then intersected with the canvas), so every consumer that already read an empty rectangle as
  "nowhere" is correct for an off-room pane with no branch of its own; `PanelBounds::resolved`
  carries the unclipped ask for the state classifier, which has to tell *partly cut off* from *not
  on this screen at all*.
- **Seven states, one classifier, and a precedence.** `closed`, `unresolved`, `refused`,
  `waiting`, `off-room`, `covered`, `open`. A UNIT outranks a want of room: a pane with a pixel
  axis and no tile left is `refused`, because a taller window would give it the tile and it still
  would not be presented. `covered` means every visible cell is behind the **union** of what is in
  front — two panes that each cover half of a third leave nothing showing, and a test that asked
  "is it inside some ONE pane" would call that `open`. One visible cell is enough to be `open`.
- **The state column is ELEVEN cells now, up from eight**, because `detail::pad` truncates and
  `unresolved` is ten bytes — an eight-column field would have presented it as `unresolv`. The
  price is measured and visible: at the 78×22 minimum a picker row two cells longer than the slot
  is fitted and the cut is marked.
- **The picker and pane management share one list and NOT one purpose.** `inventory_rows` is the
  combined catalog UNION every reference the setup names, so an unresolved pane finally has a row
  and can be removed by the gesture that removes any other; an unresolved row carries
  `kNoPaneKind` (negative, for `role::kNone`'s reason) so nothing can present it as the Builder.
  The picker keeps PRESENCE — selecting an open row removes it, PNL-0 — and management owns
  ARRANGEMENT and **binds no toggle at all**.
- **`w` enters pane management from command mode**, paying the `swallow_text_` rule once, after
  which its own keys need no modifier: `tab`/`up` select, `m` move, `s` size, `f`/`b` front/back,
  `r`/`l` raise/lower one, `0` reset (`p` place, `w` width, `h` height, `o` order), `esc` back one
  level. It is the sixth mode, below the Terminal and above ordinary command handling.
- **AN EDGE NAMES AN AXIS AND A DIRECTION — IT IS NOT AN ANCHOR.** A resize writes size and never
  place, so a pane's top-left corner is its authored place and stays where the maker put it
  whichever edge is pulled; what the left edge buys over the right one is the DIRECTION a hand
  means. Making the left edge move the place would turn one gesture into two authored writes and
  put a refused height beside a moved corner — the exact refusal-beside-a-successful-write
  `doc::resize` exists to refuse.
- **Escape is BACK, not cancel.** Every immediate-commit gesture in this application is reversible
  only by performing the inverse, and there is no undo. The help says `esc back`.
- **One press claims one gesture until release.** `PaneGesture` holds an identity, an edge and the
  size at the moment of the press — no rectangle and no live position, `Drag`'s own law — so
  crossing another pane, crossing the Terminal, and reordering mid-drag change nothing about who
  is being moved, and every motion proposes `base + (pointer - press)` rather than accumulating.
  **Management owns the pointer while it is open**, the Terminal's own shape; outside it nothing
  changed, so a selected pane behind another claims no press and no selection auto-raises.
- **Pane rectangles are CANVAS cells.** Do not pass one through `workspace_cell_x/y`; that
  conversion belongs to authored document objects.

## The front the host hits is the front the medium paints (WIND-2a)

A canvas is an ordered list of **planes** now, and that is the whole of the depth model:

```text
SurfaceLayer v1        rects[], labels[], texts[]   -- a nested value, never a message
SurfaceCanvas v5       width, height, layers[]      -- layers[0] back-most

inside one plane       rects in list order, then labels over them, then regions over those
between two planes     the complete earlier plane, then the complete later one over it
```

- **The defect it repairs was WIND-2's, and it was a MISMATCH rather than a missing feature.**
  WIND-2 authored a canonical `front` rank and correctly walked it ascending to paint and
  descending to hit. It could not make either shipped Skin execute it: the canvas held three
  ROOT lists, so painter's order was global across KINDS -- every rect, then every label, then
  every text region. Place the Builder over the Info column and send it to front, and
  `occupied_at` answered `Builder` while the terminal drew Info's prose in the same cell.
  Measured, both directions: with Info in front the picture was right, with the Builder in
  front it was wrong -- which is why a case that reversed a vector and asked about two cells
  each covered by ONE pane had passed.
- **`SurfaceLayer` is a nested value and not a surface message.** Nothing sends one, nothing
  grants one, and it is in no ordinary vocabulary -- it exists because `SurfaceCanvas` carries
  a list of them, exactly as `SurfaceTextRow` exists because a region carries a list of those.
- **It is not a compositor and must not become one.** No transform, no opacity, no blending, no
  clipping tree, no layer identity/name/handle/key, no numeric z or depth, no sorting, no ties,
  no epochs, no accumulating counter, no hit testing, no window-manager behaviour. A layer is a
  position in a vector; the publisher supplies an already-ordered list and the Skin executes it.
  **No primitive gained a field** -- `SurfaceRect`, `SurfaceLabel`, `SurfaceTextRow` and
  `SurfaceTextRegion` are byte-identical, which is what makes this an ORDERING change.
- **A clean break, on purpose.** There is no `SurfaceCanvas::rects`/`labels`/`texts`, no v4
  reader, no dual writer, no implicit base layer beside the explicit ones, and no
  `project_text_regions(const SurfaceCanvas&)` -- a canvas-wide projection IS the flattener the
  phase removed, so the overload does not exist and cannot be called. Zero layers is a valid
  blank picture; so is a plane with any empty subset of its three lists.
- **Both media execute planes, and the SDL edge no longer decides the order.** `canvas_body`
  rasterizes one whole plane at a time into the same two grids it always used.
  `plan_canvas(canvas, metric, surface)` returns one `PlanLayer{quads, regions}` per plane and
  the edge walks it -- the old shape handed the edge two canvas-wide lists, which is the same
  two global bands in a different type. `plan_layer_quads` / `plan_layer_regions` are the
  per-plane halves every lane pins.

**Workshop's plane sequence, and it is the whole layout of the screen:**

```text
the workspace       its backdrop, the scene, the size handle
one plane per pane  presentation_order(setup, panels), ascending by canonical `front`
the affordances     over the selected pane's own content, so no handle is hidden
picker / management over the panes they cover -- a provider's text cannot bury the row that
                    recovers it
the screen's chrome the shared top row and the bottom band
the Terminal        the final modal plane
```

- **The screen's chrome is in FRONT of the panes, and that is a decision with a reason.** Row 0
  is shared with the side region by design (HD-7: `OBJECTS` names the panel's column and
  `shift+space terminal` names a mode, on one row neither owns outright), and the bottom band is
  where the tool SPEAKS. A panel backdrop drawn over either would erase the notice that just told
  a maker what happened. Panes are in front of the DOCUMENT, which is what `occupied_at` has
  answered since PNL-2; they are not in front of the tool's own voice.
- `presentation_order` is still the one order helper, paint still walks it ascending and
  `occupied_at` its exact reverse. Nothing derives hit order from canvas layers, and Surface
  learns nothing about panes, setup, selection, rooms or input. **No layer fact persists.**

**The five other review findings, each a one-owner repair:**

- **A resize begins from the RESOLVED size** (`managed_bounds`). `rect` is the visible
  intersection and owns painting, occupancy, coverage, affordance placement and whether geometry
  is reachable; `resolved` is the unclipped ask and is what a first edit captures. WIND-2 captured
  the visible one: a default pane resolving to 89 cells with four on screen answered one rightward
  step by authoring **five**. The affordance stays on the visible boundary -- that is where the
  eye and the hand are -- and its delta applies to the resolved size, for the key and the pointer
  alike.
- **`end_held_gestures()` is the one release owner**, called by the Terminal branch, the
  management branch and the ordinary path. A gesture begins under one mode and is released under
  another, so whichever mode answers a release first must end them all; WIND-2's Terminal branch
  ended only the document's, and a pane gesture survived with the button up and followed the
  pointer afterwards. It says nothing -- what to tell a maker is the caller's, because the answer
  genuinely differs. It is not a capture framework: two records and one function.
- **`forget_removed_selection()` clears on MEMBERSHIP, never on presentation.** A pane that
  becomes waiting, refused, covered, off-room or **unresolved** keeps its selection -- every one
  is a pane the setup still names and whose management row is still reachable, which is WIND-2's
  recovery claim. A reference LEAVING the setup clears the selection, the submode, the edge and
  the gesture. It runs inside `apply_setup`, the one door membership changes through.
- **One picker inventory.** `picker_population()` is `inventory_rows(active, panels)` and the
  painter, the cursor bound, the Return action and the cursor repair all spend it. WIND-2 widened
  the PAINTER to the union and left the bound and the action on `combined_catalog`, so an
  unresolved row was painted and unreachable -- a maker could see the row the phase had added for
  them and could not remove it without editing the file.
- **`pane_unit_projectable(authored)` takes no placement.** A pane with either axis in pixels is
  projection-refused in every current medium, **Info included**; the old spelling exempted every
  non-overlay placement, so a setup could carry `240px` for Info and Info went on being presented
  at the developer's width, silently ignoring it. Fixed placement is not permission to present an
  unsupported unit as understood. Authored bytes are exact through the refusal, and reset and
  order still recover. A unit outranks a reservation in `manage_geometry_ready`, the same
  precedence `pane_state_of` already spends between a unit and a want of room.
- **`w` is on screen before the mode is entered.** Row 0 carries one label,
  `[+ panel]  p  [window]  w` -- 25 cells at column 24, ending at 48, one clear of `OBJECTS` at
  the minimum composition's column 50. One string rather than two labels, because what has to be
  true is a fact about the whole run and a single string cannot be half-moved.

## A press-chain bool means CONSUMED, and the Terminal's does not (QR-2)

The three handlers under `if (b.pressed)` in `WorkshopWeave::on(const input::PointerButton&)`
answer exactly one question, and it is a routing question:

```text
true   this layer CONSUMED the press -- stop routing
false  this layer did NOT consume it -- carry on to the next
```

Nothing else. Not acceptance, not success, not "something changed", and not target identity.
**A consumed press does not have to change anything; it only has to have reached the layer that
owns what the press means.** `info_press` used to answer *the caret MOVED*, which agrees with
that contract for exactly as long as every press landing on a live draft also moves it — so a
maker pressing where the caret already was fell through the whole chain and got
`Info is here -- nothing under it can be taken hold of` written over the notice they were
reading. The failure was a NAME, not a missing type: a three-valued disposition would have
hidden it under a plausible shape, and INT-R0's two-cases-per-word test earns exactly two words.

- **A deliberate `false` is a decision and reads as one.** `objects_press` still declines a
  press on the row of the object that is ALREADY selected, so the panel answers it as it
  answers every other press on that rectangle. That is the same *shape* `info_press` had by
  accident, which is why naming the bit — rather than making the two symmetrical — was the
  repair. Both dispositions are now pinned by a case of their own.
- **`terminal_press` is NOT part of this contract and must not be unified with it.** Its bool
  is *is a repaint owed*; consumption inside the overlay was already decided one layer up by
  the MODE, and a `false` there means "consumed, and nothing moved" — the opposite of a `false`
  in the chain. The caller names the result `repaint_needed` at the one place both kinds of
  bool are in view. Two questions with two answers each are not one question.
- **`info_body_at` (`workshop/screen.hpp`) is the resolve-and-locate preamble, owned once.**
  `bounds_of` → is Info open? → `info_body_place` → `prose_at` → is the position understood?
  was six lines inside each of the three handlers; a fourth pressable place would have copied
  it again. It answers **where** and nothing about what that means: no routing priority, no
  property/action/object semantics, no refusal and no consumption policy — each handler still
  asks its own inverse (`property_row_hit`, `action_press_at`, `object_press_at`) of the place
  it returns. `present` is the conjunction *panel open ∧ body resolved ∧ position understood*,
  one bit because it is one fact about the press: it named nothing here.
- **The body is resolved ONCE PER PRESS, in the route, beside the canvas point** — the chain
  already carried one resolved pointer fact (`PointedAt`) and now carries two, rather than each
  handler re-resolving (up to three resolutions of one body for one press). Holding it across
  the chain is sound for a stated reason and not an assumed one: **every one of the three
  handlers changes nothing on the paths where it declines**, so a "not mine" cannot have moved
  the picture the next handler is about to ask about. Keep that true when adding a fourth.
- No `Disposition`, no `InteractionResult`, no `Handled/Refused/Ignored`, no target enum and no
  interaction package. The richer answers already exist where richer answers matter (`Written`,
  `Handled`, `Commit`, `Availability`, `Occupancy`) and they are all on SEMANTIC paths; the
  bare bool survives only on the routing path, which is the one place it is adequate.

## A press on an external pane crosses the seam as a place, never as a meaning (SEL-0)

`PanePressed{pane, row, column}` is the fifth and last shape of the pane protocol. It is the
`PaneRoom` budget read backwards — a place in the lattice Workshop already granted, and nothing
that would let a provider locate itself on a screen.

```text
occupied_at   -> Occupancy{occupied, what, kind}     ONE geometry walk, topmost first
                 is_runtime_kind(kind)?              -> external_press, and Workshop says NOTHING
external_press_at(panels, setup, screen, kind, space, x, y) -> ExternalPressAt{named, row, column}
                 bounds_of -> external_body_place -> prose_at -> minus kExternalHeaderRows
                 named == false  =>  no sentence. The press was still the pane's.
```

- **`Occupancy` carries the KIND it met since SEL-0, and that is the same answer rather than a
  second one.** The one caller asks a further question of the walk that already decided what is on
  top; resolving the pane again to locate the press would be two geometries for one press, which is
  what the `bounds_of`-for-both rule exists to refuse. `kNoKind = -1` for the picker, negative for
  `role::kNone`'s reason. Nothing switches on a built-in kind — the question asked is
  `is_runtime_kind`, which is about which SEAM owns the press.
- **Consumed by occupancy, before anything is sent.** A pane that owns visible room owns pointer
  refusal for that room, and nothing waits for the provider: there is no reply shape, `consumed`
  never crosses the wire (WP-R0), and a press that named no row is consumed identically and simply
  travels no further. The Terminal overlay and pane management still take every press whole, one
  layer up, and the picker still answers first inside `occupied_at`.
- **Workshop says NOTHING on the notice line for an external pane, and that inverts the rule three
  lines above it in `on(PointerButton)`.** `<name> is here -- nothing under it can be taken hold
  of` is TRUE of a built-in and would be a claim about an OUTCOME here, made before the outcome
  exists. What a press on a provider's row means is that provider's vocabulary; the answer arrives
  later as ordinary `PaneContent`. INT-R0's rule decides it: a refusal belongs to the deepest layer
  whose vocabulary contains the reason, and this layer's does not.
- **The header row is subtracted in BOTH directions or in neither.** `external_body_place` reserves
  `kExternalHeaderRows` out of the fit before a provider is told its budget, so the row a provider
  means by 0 is the region's prose row 1. Forgetting the subtraction on the way back is the
  off-by-one that would be invisible until a pane had more than one selectable row.
- **A row that fits no prose is not a row.** Anything outside `[0, rows) × [0, columns)` — the
  header, the pixel remainder under the last prose line of a graphical medium, an unrecognised
  `space` — is refused rather than clamped. Rounding to a nearest row hands a provider a press at a
  place it never wrote to.
- **Workshop holds no selection, no focus and no memory of the press.** No `Workshop::selected_*`,
  no pane focus, no capture, no record of which pane a maker touched last, and no repaint on the
  forwarding path: Workshop's picture did not change, and if the provider answers, its own handler
  repaints. Nothing here reads `ExternalPane::shown` and nothing may — the moment Workshop looks at
  a provider's rows to decide what a press means, the seam has stopped being one.
- **A provider interprets the press against what it is CURRENTLY SHOWING.** `project_loaded`
  returns the row-to-entry map beside the rows it built (HD-3's one-measurer rule reaching
  interaction), the provider retains that value and drops it on every room grant, and a press costs
  one lookup and no observation. **A provider that re-queried its source to interpret a press would
  let a maker select something they were never shown** — silently, and only sometimes.
- **The fact a pane publishes carries DATA and no authority.** `LoadedSelected{pane, library, role}`
  is an occurrence, not a transition, and a listener that hears it has acquired nothing: a grant is
  per `(shape, version, target)` and a value in a message is not one. Values may flow; authority
  must not flow implicitly with them.

## The terminal is a medium with a SIZE, and the Sink is what holds it (TUI-0)

`TuiMedium::extent()` asks its `Sink`. A Sink is now anything with `write(std::string_view)` **and**
`TerminalSize size() const` — required, not detected, because a Sink that quietly lacked the method
would be a Sink whose terminal is permanently unmeasurable, which is an ordinary honest state a pipe
reaches every day; the mistake would look exactly like the truth on every lane, forever.

```text
native_terminal_size()   surface/terminal_size.hpp -- the ONE #if defined(_WIN32) any of this needs
TuiTerminal::size()      gated on the same `ok_` the alternate-screen CLAIM is
tui_canvas_extent()      pure: a terminal size -> what a CANVAS fits in it
SkinT::report_extent     unchanged: publish on change, never publish "no opinion"
```

- **The medium answers about a CANVAS, not about the terminal.** `kTuiReservedRows` is 3: two for
  the status and score slots (`kTuiCanvasTopRow`, consulted from `pointing.hpp` rather than
  restated) and one because `canvas_body` ends **every** row with CRLF, so a canvas whose last row
  lands on the terminal's last row feeds past the bottom — and a feed there SCROLLS, taking the two
  slots with it. Buying that row back by not feeding after the last row would move every terminal
  golden in this repository; a row is cheaper than a byte-exact projection.
- **`{0,0}` still means "no opinion" and is still never published.** An unmeasurable terminal and a
  terminal with no room left both reach it — two sentences the medium cannot tell apart to its
  shell, and both correctly become SILENCE. Silence leaves Workshop on `kScreenMinW`/`kScreenMinH`,
  which is why every golden, every CI run and every redirected run is byte-for-byte what it was.
  **Nothing manufactures a 78×22 terminal**; that number is a composition minimum standing because
  nobody offered anything else, and it stays legible as a different kind of fact.
- **Non-positive is the absence, and there is no `bool measured` beside it.** A flag would be a
  marker that fails OPEN — a console API answering successfully with a degenerate window would be
  believed by anything testing the flag instead of the number. `TerminalSize::measured()` is that
  test, written once.
- **`GetConsoleScreenBufferInfo`'s `srWindow`, never `dwSize`.** `dwSize` is the scrollback buffer —
  9,001 rows on a stock console — and a medium that believed it would report a nine-thousand-row
  surface into a thirty-row window. Measured: with the buffer set to 120×40 behind a 120×30 window,
  the reported room stayed 27 canvas rows.
- **A shrinking canvas hands its rows back.** `canvas()` appends one erase-below when the canvas it
  just drew is shorter than the one before it — the cursor is already one row past the last row
  written, so it erases precisely the difference. A steady frame writes the bytes it always wrote.
- **Workshop received no new code at all** *(in TUI-0 — HD-7 then changed the panel this bullet
  names)*. `adopt_screen`, `screen_of`, the Inspector body place, `list_window` and
  `component::TextBox` were byte-identical; a terminal that grew is the same message a window
  that grew has published since G-2.

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

## Is the rectangle mine? (TYPE-0, answered in three by TYPE-1)

Which text primitive a publisher reaches for is one question, and it is not about importance or
about how the text looks. It is about who owns the room:

```text
SurfaceTextRegion       the rectangle is MINE. It clears its whole bounds before a row is drawn
  ground=kGroundOwn     -- spaces in a character medium, its own fill in a graphical one -- so
  (the default)         nothing may be under it. Ordinary tool prose, headings, lists, controls,
                        status. The only shape a medium sets in REAL TYPE.

SurfaceTextRegion       the rectangle is SOMEBODY ELSE'S and I am writing ON it. Same bounds,
  ground=kGroundBeneath same fit, same rows, same real type -- and no padding and no fill, so
                        material published beneath shows wherever a glyph does not. The maker's
                        name written across an authored object is the consumer that earned it.

SurfaceLabel            it is not a rectangle at all: this CELL is the meaning. One glyph over
                        the ring that already fills it, a row shared with another publisher's
                        sentence. One cell per byte in every medium, and NOT deprecated.
```

- **`ground` IS NOT A ROW'S `background`, and the two must not be read as one field.** A row that
  names no background defers to its region; a region has nothing to defer to, so its two answers
  are about OWNERSHIP rather than about ink. That is why the default here is the OPPOSITE of the
  default there, and why `kGroundBeneath` is not spelled `role::kNone`. Inside a `kGroundBeneath`
  region a row that DOES name a background still gets its strip, at the region's full width, in
  both media -- a row claiming a ground is claiming those cells, and padding is how a character
  medium says so.
- **It is not transparency, alpha or compositing.** No blend, no opacity, no order of its own,
  no second rectangle. The region is in exactly the plane its publisher put it in (WIND-2a) and
  `kGroundBeneath` removes one fill. Everything a medium already knew about painter's order still
  decides what "beneath" is.
- **The character medium needed no change at all**, and that is the proof the cell projection was
  already honest: a cell's ground has always been "whatever the terminal is wearing", so the whole
  of the difference there is that the projection stops PADDING. The run of cells a
  `kGroundBeneath` region produces is byte-for-byte the run a `SurfaceLabel` at the same origin
  produced, which is why no terminal picture in this repository moved -- measured across seven
  Workshop states, byte-identical.
- **A row of such a region with nothing to draw is not a row.** No bytes, no caret and no ground
  of its own means no cell is written, so no `ProjectedRow` is produced -- which is also what
  keeps a name over a four-cell-tall object ONE projected row rather than four.
- **One field, three version numbers**: `SurfaceTextRegion` v4, `SurfaceLayer` v2,
  `SurfaceCanvas` v6. A layer IS a list of regions and a canvas IS a list of layers, so their
  wire identity moved without either gaining a field. `SurfaceRect`, `SurfaceLabel` and
  `SurfaceTextRow` are byte-identical.

- **A one-cell row cannot be semantic text, whatever it means.** A canvas cell is
  `kCanvasCellPx` = 12 device pixels and this repository's face has an 18-pixel line, so
  `fit_region` answers zero rows for a one-cell region and hands it back to the cell projection
  (HD-5). `floor((12h - 2*kTextInsetPx) / 18)` is the whole table: **h=1 → 0 rows, h=2 and h=3 →
  1, h=4 and h=5 → 2, h=7 → 4, h=9 → 5.** So a migration is always of a RUN of rows, never of one
  label, and `12h - 4 == 18k` has no integer solutions — a region's viewport never exactly fits
  its rows, which is why a row's ground cannot stand in for a rectangle's.
- **`panel_prose_place(b, sc)` + `panel_prose_region(b)` is the one call for a panel whose whole
  rectangle is its own** (the picker, the pane-management surface, an external pane). It returns
  the prose rows and columns the ACTIVE medium fits, and the painter spends them. Its cell
  projection is byte-for-byte what `paint_panel_row` wrote, so **no terminal picture moved**.
- **`paint_panel_row` has one consumer left: the Builder.** Its nine rows are a fixed composition
  against a nine-cell slot and the face holds five, so migrating it would drop four rows —
  `[ Build ]` among them. Fixed-row panels need a row-budget composition before they can be
  semantic text; that is a design phase, not a typography one.
- **The screen's own band is 5 cells for 4 sentences and cannot migrate either**, for the same
  arithmetic: 5 cells hold 3 rows of the face. The NOTICE is the one piece of it that could,
  because the spare row beneath it is already reserved and painted by nobody — two cells, one
  prose row, nothing moved (`kNoticeRows`).
- **The workspace object's name is a `kGroundBeneath` region, and the two things it is NOT were
  built and run live twice** -- once by TYPE-0 and once again by TYPE-1 to re-measure them. An
  ordinary region over the object's rect turns every object into an empty dark box; rows carrying
  the object's role as a GROUND leave a `12h - 4 - 18*rows` pixel band the strips cannot reach
  (10 px at h=4) AND replace `glyph_for_role`'s `#` with a background colour, which is the exact
  thing that constant exists to refuse. **Its bounds are the OBJECT'S OWN RECTANGLE since QR-3**
  -- `min(object width, workspace right edge - x)` by `the object's own height`, each floored at
  one cell -- so the name is bounded by the material it names and `fit_region` is what makes a
  one-cell object fall back to cells with no `if (h < N)` written anywhere. Until QR-3 the width
  half was `workspace right edge - x`, and that is what the next entry is about.
- **A name is bounded by the material it names, and that is QR-3's repair of TYPE-1's one
  measured product cost.** TYPE-1 gave the name the room from the object's origin to the
  workspace's right edge, and in a medium that paints roles as ink every character past the
  object's own edge was invisible: the name is `kMuted` so it reads on the object's `kFill` body,
  and the workspace backdrop is ALSO `kMuted`. Measured on the pristine tree: a 6-cell object
  with a 32-byte name planned a 564 px region over 72 px of material, so 9 characters were
  legible and 23 were the backdrop's exact colour. No role fixes that -- nothing this medium has
  reads on both a `kFill` body and a `kMuted` backdrop, and a fifth role is what
  `surface/vocabulary.hpp` refuses -- so the answer is the BOUND, and a name that does not fit
  its object says so with `detail::fit`'s mark rather than fading into the backdrop. The authored
  name is untouched by any of it and widening the object reveals more of the same bytes. See
  `Zen/reportbacks/TYPE-1-RB.md` for the cost and `Zen/reportbacks/QR-3-RB.md` for the repair.

## The first tool that reaches Workshop as a stranger (INTR-0)

`introspection/` builds `zengine-introspection`, an ordinary loadable weave beside timer/ and
input/, and it is the first thing in this repository whose pane arrives entirely through the
external protocol. **Workshop compiled nothing for it**: `weave.hpp`, `panel.hpp` and `screen.hpp`
do not name it, no `panel::k*` was minted, and the picker learned its row from a live offer.

```text
PaneRef      zengine.introspection / loaded         the durable pair a saved setup names
office       zengine.introspection                  the only address anything reaches it by
stem         zengine-introspection                  a line in the HOST'S boot list, and that
                                                    line is the remainder of the plugin story
```

- **The fact it shows is the KERNEL's, and there is no second copy of it.** `zen.ListLoaded` to
  the Weave Manager, relayed to `zen.ListLibraries` at the control door, answered from
  `Kernel::loaded()` — a live map, never a cache. The provider holds no `known_weaves_`, keeps no
  diff, derives no arrival or departure, and stores no timestamp. `introspection/loaded.hpp` is
  the pure half (parse the answer, spend the budget) and links nothing, so what a reading MEANS is
  provable over a value.
- **The order is the kernel's.** `Kernel::loaded()` walks a `std::map` keyed by library name, so
  the answer is name-ordered, stable across runs and independent of boot order. Nothing sorts it
  here; a view that reordered a list its owner already ordered would then window it by a rule the
  owner never applied.
- **The wire form has no escaping**, and it is the one thing to know before reading it: the door
  joins on `,` and `@` and emits no delimiter of its own, so a library name containing either is
  unrecoverable in principle. `parse_loaded` splits on the LAST `@` — the ambiguity's better half,
  not a solution — and says so where the reading happens.
- **`zen.ListLoaded` is not the load capability.** A Loom grant is per `(shape, version, target)`,
  so asking what is loaded is exactly that one question; `LoadWeave`/`SwapWeave`/`ReloadWeave`/
  `UnloadLibrary`/`UnloadRole` are absent from this weave's Emit set and from every send it makes.
  The suite pins that **from a bus tap**, not from the declaration, because `Emit<...>` is
  informational in this Loom and `Kernel::load` binds `allow_any()` to every library it opens —
  so a declaration proves nothing on its own and is not quoted as though it did.
- **THE PANE'S ONE BEAT IS THE ROOM GRANT.** Loom gives a participant no arrival or departure
  event, so there is nothing to subscribe to and nothing here polls or times out. It re-reads when
  the pane opens, when a valid re-offer refreshes it, and when the resolved prose capacity moves —
  and the last row of the pane says `snapshot`, because between two grants that is what it is.
- **A COUNT WITH AN UNSTATED POPULATION IS THE DEFECT THIS VIEW IS SHAPED AROUND.** `ListLoaded`
  enumerates kernel-loaded libraries, so every in-process weave — Workshop itself, the Builder, the
  runner, the terminal participant, the Manager, the control door — is outside it and cannot be
  spoken about. So `in-process weaves are not in the kernel's map` is reserved out of the row
  budget BEFORE the list is offered anything but its first row (HD-8's reservation argument, in a
  second place). Do not make that line conditional on spare room.
- **An entry and its omission marker are ONE demand on the budget**, and getting that wrong is the
  arithmetic defect the suite caught: reserving a single row for "the list" bought a row the marker
  then took, so a four-row body spent two rows on notes and named no weave at all. Showing PART of
  a list obliges saying how much was hidden.

**And the picker now MARKS a name it cut** (the phase's one adjacent repair). `picker_entry_text`
runs the name through `detail::fit` before `detail::pad`: fit for the truth, pad for the alignment,
and `kPaneStateCols` has not moved. `kPickerNameCols` is 10 and admission allows 32, so for two
phases the cut was arithmetic that never fired — Workshop's own names are `Builder` and `Info` —
and it fires the moment a name belongs to a party this build never compiled. `pad` still truncates
in silence and that is still right for a column whose longest word is a constant somebody checked;
the STATE column is padded and not fitted for exactly that reason.

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
- Assertion totals (**91,496** over the **nine** doctest binaries, SDL lane, measured
  2026-08-18 after WIND-2b, which added no case at all and 55 assertions to one existing
  Workshop case) are evidence to report. They are **not** a population, never an
  acceptance oracle, and not coverage. The count of suites said "seven" here until HD-2
  counted them, which is the same decay this bullet warns about arriving in the sentence
  that warns about it — and HD-4 found the *arithmetic* had decayed the same way: the
  figure written after HD-3 summed seven of the eight, leaving `audit_probes` out of a
  total that said eight. It is the sum of all of them, named so the next phase can
  reproduce it: `zengine-surface-tests` 6,962 · `zengine-workshop-tests` 74,697 ·
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
  number without anything noticing. **HD-7 is the clearest demonstration yet that
  the figure is not an oracle:** it added fifteen cases and moved the total from
  36,130 to 48,738, because two of those cases assert a *property* over every
  budget from zero to two hundred. HD-8 then added sixteen cases and 9,975 assertions
  the same way, for the same reason, and the figure crossed fifty thousand without
  anything in this repository being any better tested for the crossing. HD-10 added nine
  cases and 6,078 assertions, again for the same reason -- four of them sweep eleven extents
  against three metrics, and one walks every width from 78 to 200 -- and the honest reading is
  still that the phase pinned one law, not six thousand facts. WIND-1 is the same story once
  more and more starkly: **four** cases moved Workshop's total from 48,355 to 63,747, because
  one of them states the width law over every width the composition lays out at three heights
  and another sweeps every extent against every seated slot. The phase pinned one expression.
  WIND-2 then added **thirty-eight** cases for 10,755 assertions -- a ratio of about 280 to 1,
  where WIND-1's was 3,850 to 1 -- and it is the same lesson read from the other end: a phase
  whose claims are mostly DISTINCT LAWS rather than SWEPT DOMAINS moves this number hardly at
  all while changing far more of the repository. Ten thousand of those assertions come from ONE
  case, the bounded-ordering loop, which is worth knowing before anybody reads the delta as a
  measure of anything at all. WIND-2a is the smallest delta of the lot and the plainest reading
  of this bullet: **seventeen** new cases -- a whole vocabulary version, both media's execution
  order and six behavioural repairs -- moved the figure by **403**, because what they assert is
  LAWS rather than swept domains. Nobody should read 403 as "less was proved here than by
  WIND-1's four cases and fifteen thousand".
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
