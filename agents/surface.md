# Agent law — Surface and media

Routed detail behind [`AGENTS.md`](../AGENTS.md), for tasks touching `surface/` — the drawing
vocabulary, the two shipped media (character cells and SDL), text regions, grounds, planes, and
what a medium may know. Public reference: [`../docs/reference/surface.md`](../docs/reference/surface.md).
Phase tags like (HD-2) are provenance markers into this repository's history; the law here is
current.

## A row may sit on something, and it is one field (HD-2)

`SurfaceTextRow` carries `background`, a semantic role defaulting to `role::kNone` — the
**absence** of a ground, negative on purpose so the unknown-role fallback (`kFill`) can never
swallow it and a later fifth role cannot collide with it. Nothing passes it to a Skin's
role→ink table; a consumer tests for it first. `project_text_regions` returns
`ProjectedRow{label, background}` rather than bare labels; the ground travels **unresolved**
through the cell projection and each medium answers for itself.

**A canvas with no ground emits not one background byte** — the assertion is in
`test_surface.cpp`, over a canvas built by hand for exactly that question, and it is the thing
to re-check if this ever grows. It is deliberately NOT asserted over a whole Workshop screen:
the Terminal's completion list, the Info panel and other consumers set grounds as a matter of
course, so a Workshop canvas carries background bytes.

**A ground is the WHOLE ROW in both media.** The cell projection pads every row to the region's
width and carries the ground onto the padding (`project_one_text_region`); the SDL renderer
fills a strip spanning the region's whole viewport (`skin_sdl_text.hpp`). A control and a
heading want exactly the same sentence as a selected row: **this row, all of it.**

**Each medium's palette offers exactly one ground that every ink reads on** — `sgr_bg_for_role`
says so in its own comment and `ink_for_role` bears it out — so a publisher that wants a legible
grounded row has one honest choice. **Never pair a role with its own ground**: `kFill` on
`kFill` is white on white in a terminal, and nothing refuses it.

## A region may have a caret, and it is said in PROSE (HD-3)

`SurfaceTextRegion` carries `caret_row`/`caret_col` — a row and a column into the rows the
region carries, never a pixel and never a canvas cell. That is what lets each medium answer
with the metric it already resolved: `plan_caret` (`surface/skin_sdl_plan.hpp`) turns them
into a `kCaretWidthPx` bar off the same `RegionFit` the rows were positioned with, and
`project_text_regions` *inserts* `kCaretGlyph` at the same column, which for a caret at the
end of a line is byte-for-byte the row the Terminal used to append for itself. `kNoCaret` is
**negative** for `role::kNone`'s reason — a row index is non-negative by construction, so an
absence cannot collide with a row anybody meant.

**A caret is an insertion point, so it is a bar and never a block**, and it is not a focus
fact and not a clock. Two regions on one canvas may each carry one.

## A region may have a selected range, and each medium answers in its own voice (TEXT-0)

`SurfaceTextRegion` carries `sel_begin_row/col` and `sel_end_row/col` — two caret-like
positions in **reading order** (begin inclusive, end exclusive, `kNoSelection` = none), in the
region's own prose lattice. The per-row arithmetic is ONE function both media consume:
`selection_span_of_row` (`surface/region.hpp`) — begin row from `sel_begin_col` to its own
end, middle rows whole, end row to `sel_end_col`, everything clamped into the text that
exists, and a range that is absent, empty or not in reading order answers the empty span for
every row. Do not re-derive a span at a call site; two spellings of it is a highlight that
covers different characters in different media.

- **The character medium answers in REVERSE VIDEO over exactly the selected cells** — a
  fourth grid in `canvas_body` beside glyph/role/ground, emitted as `\x1b[7m`/`\x1b[27m`
  runs. It composes with any ink and any ground, `\x1b[0m` resets it (so it is re-stated
  after a reset exactly as a ground is), and a canvas with no selection emits not one byte of
  it — the goldens are the proof.
- **The graphical medium answers with a BAND under the glyphs** — `kSelectionBand`
  (`skin_sdl_plan.hpp`), one `PlanSelectionBand` per touched row, resolved from the SAME
  `RegionFit` that placed the rows and drawn after the row grounds, before the text, so
  glyphs keep their ink and sit on it. The bitmap face paints a selected cell's clear in the
  band's ink, `kGroundBeneath` rows included — a selection must not vanish because the
  material under it is somebody else's.
- **The cell projection carries the span on `ProjectedRow` (`sel_begin`/`sel_end`)**, in the
  projected label's own bytes: the inserted caret glyph shifts a span at or after it, sits
  INSIDE the highlight when the caret is strictly inside the range, and the region-width cut
  cuts highlights exactly as it cuts text.
- **Which end the caret is at is not restated** — the caret fields already say it — and the
  pair is NOT per-span styling: one range, meaning selection. The role vocabulary is still
  closed.
- The selection made `SurfaceTextRegion` v5, `SurfaceLayer` v3, `SurfaceCanvas` v7 — the same
  compose-upward bump every region field has cost.

## The Medium owns the platform clipboard, in both directions (TEXT-0, repaired by QR-11)

**Clipboard read follows paste intent.** The system clipboard is ambient host state that may
have nothing to do with this application; permission to use its text when a maker asks to
paste is not permission to observe it continuously. Nothing in the process watches it — no
mirror of it exists, the SDL Input reader has no clipboard business at all (the clipboard
event class is in its ignored set, and a source tripwire in the input suite keeps its files
clean of the read calls) — and the ONE road foreign clipboard text has onto the bus is the
answer to a paste's own ask.

- **The write** (TEXT-0): `ClipboardCopy{text}` is intent (a maker copied this), a
  publication because several unrelated parties mirror it. The active Skin executes it —
  `SDL_SetClipboardText` on the SDL medium; the OSC 52 set-clipboard sequence
  (`tui_clipboard_sequence`, base64 and all) written to the stream a terminal Skin already
  owns, honoured where the terminal supports it and harmless where not, with **no claim ever
  made** that the system took it. Every text-holding participant mirrors the same
  publication, which is what keeps copy-here-paste-there true in-process on media whose
  platform cannot answer. Mirrors never echo, and a mirror means exactly *the freshest copy
  said IN this process* — never the platform's state.
- **The read** (QR-11): `ClipboardTextRequested{}` is a SEND to `kSkinRole`, made because a
  maker pressed paste and for no other reason; the Skin reads the Medium at that moment and
  ANSWERS (`mail.answer`) with `ClipboardText{readable, text}`. `readable=false` is a
  terminal medium's standing truth (no truthful terminal route reads a system clipboard —
  the OSC 52 query is disabled almost everywhere), and the asker then falls back to its
  in-process mirror; `readable=true` with empty text is the SDL medium saying the platform
  holds no text, which a paste honours by inserting nothing. The two are separate fields
  because collapsing them would make an empty platform clipboard paste stale mirror text.
  The asker settles the answer with its own `loom::AskBook` **and** `answers_ask()`, and
  applies it to the draft that requested the paste or discards it whole
  ([`workshop.md`](workshop.md#editing-text-is-a-component-and-it-belongs-to-no-consumer-hd-5-text-0)).

`clipboard_copy` and `clipboard_text` are both REQUIRED Medium methods, the Sink's own rule
for the Sink's own reason: a Medium that quietly lacked either would be one on which the
gesture silently reaches nothing, and the mistake would look exactly like the truth on every
lane. `clipboard_text`'s nullopt is not a failure — it is the honest cannot-say.

## The Medium owns the desktop placement, in both directions (WUX-3)

The window's place on the desktop is the medium's second answer-only fact, beside the
extent — and unlike the extent it is spoken in the MEDIUM'S OWN desktop units, which a
publisher may remember and hand back and may never interpret. That custody split is the
whole design: authored geometry stays on the medium-independent lattice (WUX-2), and
desktop truth never enters it.

- **The report** (`SurfacePlacement{x, y, maximized}`): published on change, on the same
  beat that notices a dragged edge — and `x`/`y` are always the NORMAL window's top-left
  (the SDL medium samples them only while unmaximized and remembers them across a
  maximized stretch, `skin_sdl.cpp`), with `maximized` the current state beside them.
  Absence is SILENCE, not zeroes — `(0,0)` is a real place on every desktop, so the
  medium answers `std::optional` and the shell (`SkinT::report_placement`) publishes
  nothing for nullopt. Every terminal medium answers nullopt forever: the emulator owns
  that window and tells its guests nothing.
- **Placement is reported BEFORE the extent at every report site**, and the order is
  load-bearing: a maximize changes both facts in one gesture, and a consumer keeping the
  normal window's room must hear the state before the size or it files the maximized
  room under the wrong state. The reports run after the beat's `SDL_PumpEvents`, so the
  flags and the drawable are one settled picture.
- **The offer** (`SurfacePlacementRemembered{x, y, maximized}`): a publisher restoring a
  session sends its remembered placement to `kSkinRole`, once. It is a want, not an
  instruction — the medium can see the displays that exist NOW and the publisher cannot,
  so the judgment is the medium's: `placement_within` (skin_sdl_plan.hpp, pure, pinned on
  every lane) restores a reachable position VERBATIM (partial overhangs are intent — the
  test is a hand's width, `kPlacementGraspPx`, of the window's top strip on some single
  display's usable area), clamps a stranded one into the nearest display's usable bounds
  top-left-first, and answers NOTHING with no display truth — an uninformed move is a
  blind replay, refused. The maximize is applied after the position, so unmaximize lands
  where the offer put the frame. What comes back to the publisher is the truth through
  the ordinary report, never an echo.
- `placement()` and `place()` are REQUIRED Medium methods, the clipboard pair's rule for
  the clipboard pair's reason; the terminal's honest pair is one line each
  (`skin_tui.hpp`).
- **This is not a window manager.** No monitor identity, no fullscreen, no z-order, no
  multi-window vocabulary, no size instruction (the window's size remains the canvas
  conversation's, WUX-0's floor law included).

## Which text primitive: who owns the room (TYPE-0, answered in three by TYPE-1)

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
  both media — a row claiming a ground is claiming those cells, and padding is how a character
  medium says so.
- **It is not transparency, alpha or compositing.** No blend, no opacity, no order of its own,
  no second rectangle. The region is in exactly the plane its publisher put it in (see the plane
  model below) and `kGroundBeneath` removes one fill. Everything a medium already knew about
  painter's order still decides what "beneath" is.
- **The character medium's cell ground has always been "whatever the terminal is wearing"**, so
  the whole of the difference there is that the projection stops PADDING. The run of cells a
  `kGroundBeneath` region produces is byte-for-byte the run a `SurfaceLabel` at the same origin
  produced.
- **A row of such a region with nothing to draw is not a row.** No bytes, no caret and no ground
  of its own means no cell is written, so no `ProjectedRow` is produced — which is also what
  keeps a name over a four-cell-tall object ONE projected row rather than four.
- **One field, three version numbers**: the ground made `SurfaceTextRegion` v4, `SurfaceLayer`
  v2, `SurfaceCanvas` v6. A layer IS a list of regions and a canvas IS a list of layers, so
  their wire identity moved without either gaining a field.
- **A one-cell row cannot be semantic text, whatever it means.** A canvas cell is
  `kCanvasCellPx` = 12 device pixels and this repository's face has an 18-pixel line, so
  `fit_region` answers zero rows for a one-cell region and hands it back to the cell projection.
  `floor((12h - 2*kTextInsetPx) / 18)` is the whole table: **h=1 → 0 rows, h=2 and h=3 → 1,
  h=4 and h=5 → 2, h=7 → 4, h=9 → 5.** So a migration is always of a RUN of rows, never of one
  label, and `12h - 4 == 18k` has no integer solutions — a region's viewport never exactly fits
  its rows, which is why a row's ground cannot stand in for a rectangle's.

## The canvas is an ordered list of planes (WIND-2a)

```text
SurfaceLayer           rects[], labels[], texts[]   -- a nested value, never a message
SurfaceCanvas          width, height, layers[]      -- layers[0] back-most

inside one plane       rects in list order, then labels over them, then regions over those
between two planes     the complete earlier plane, then the complete later one over it
```

- **`SurfaceLayer` is a nested value and not a surface message.** Nothing sends one, nothing
  grants one, and it is in no ordinary vocabulary — it exists because `SurfaceCanvas` carries
  a list of them, exactly as `SurfaceTextRow` exists because a region carries a list of those.
- **It is not a compositor and must not become one.** No transform, no opacity, no blending, no
  clipping tree, no layer identity/name/handle/key, no numeric z or depth, no sorting, no ties,
  no epochs, no accumulating counter, no hit testing, no window-manager behaviour. A layer is a
  position in a vector; the publisher supplies an already-ordered list and the Skin executes it.
- **There is no canvas-root primitive list and no canvas-wide projection.** No
  `SurfaceCanvas::rects`/`labels`/`texts`, no implicit base layer beside the explicit ones, and
  no `project_text_regions(const SurfaceCanvas&)` — a canvas-wide projection IS a flattener
  that would re-impose global paint order across kinds, so the overload does not exist and
  cannot be called. Zero layers is a valid blank picture; so is a plane with any empty subset
  of its three lists. The defect this model repaired was exactly a three-root-list canvas whose
  painter's order was global across KINDS — every rect, then every label, then every text
  region — so a pane sent to front could be hit first and still painted under another's prose.
- **Both media execute planes, and the SDL edge does not decide the order.** `canvas_body`
  rasterizes one whole plane at a time into the same two grids it always used.
  `plan_canvas(canvas, metric, surface)` returns one `PlanLayer{quads, regions}` per plane and
  the edge walks it. `plan_layer_quads` / `plan_layer_regions` are the per-plane halves every
  lane pins.
- **No layer fact persists**, and Surface knows nothing about panes, setup, selection, rooms or
  input. What order a consumer publishes its planes in is that consumer's law — Workshop's is in
  [`workshop.md`](workshop.md#the-plane-sequence-is-the-layout-of-the-screen-wind-2a).

## The lattice is fine, and each medium floors at its own grain (WUX-2)

`kCellSubs` (vocabulary.hpp) is the canvas lattice's resolution: 48 sub-units per cell, a
fixed vocabulary constant that never moves with a medium. The geometry shapes carry their
coordinates as whole cells plus a `sub_*` remainder in `[0, kCellSubs)` — the floor
decomposition, one spelling per value — so every earlier publisher's zeros mean exactly what
its silence always meant, and a publisher with a finer fact (pane arrangement is the one
that earned it) says it on the same lattice everything else is drawn on.

- **ONE QUANTIZATION LAW.** A consumer whose device unit is `g` sub-units — a terminal cell
  (`kCellGrainSubs` = 48), the shipped skin's pixel (`kPixelGrainSubs` = 4) — presents a fine
  span `[L, R)` on device units `[floor(L/g), floor(R/g))`. The SDL plan applies it per EDGE
  through `px_of_subs` (quads, labels, and `fit_region`'s viewport — one arithmetic, so a
  pane's backdrop, its prose and its hit answer are one picture); the cell projection and the
  terminal rasterizer apply it at the cell grain (covered cells; a label's anchor floors).
  Exact-cell geometry therefore lands on exactly the cells and pixels it always did.
- **THE HIT LAW IS THE PAINT LAW READ BACKWARDS.** `sub_span_contains` floors BOTH sides by
  the pointer's own grain before comparing, so the hand meets exactly the device units the
  rectangle paints — comparing the raw sub-position instead is wrong by up to one device unit
  at a fractional edge, which is a pane whose visible edge and interactive edge disagree.
- **A GARBAGE REMAINDER READS AS ZERO** (`sub_rem`): a value outside `[0, kCellSubs)` is a
  number nobody could mean, and the safe reading is the whole-cell picture — the same posture
  an unknown ground takes, for the same reason.
- **THE POINTER'S FINE TWIN rides beside its cell projection**: `subs_of_pixel` /
  `canvas_subs_of_window_pixels` / `canvas_subs_of_terminal_cells` (pointing.hpp), with the
  reporting medium's grain travelling beside the position — a terminal says a cell's corner
  at grain 48, a window says a pixel at grain 4, and a consumer that spends cells and one
  that spends subs are reading ONE measurement.
- **What stayed coarse, deliberately**: `SurfaceExtent` (the medium's room is whole cells —
  the honest coarse fact), the canvas's own `width`/`height`, and the prose lattice (rows,
  columns, carets, selections — a region's interior is the metric's business, not the
  lattice's).

## A region too small for the face is a CELL region (HD-5)

`fit_region` falls back to the region's own cell bounds when a real metric yields no rows or no
columns, and `plan_canvas`/`plan_text_regions` partition on `fit_region(r, metric).graphical()`
rather than on the metric alone. Before this, a region one cell tall was in **neither** list and
was drawn by nobody — reachable, and reached. `fit_region`'s answer is byte-for-byte the one a
faceless medium gets, so no canvas painted in a character medium moved. A body still too short
for one line of the face still resolves to cells and is still drawn by exactly one of the two
lists — grant more room rather than special-casing past this.

## The terminal is a medium with a SIZE, and the Sink is what holds it (TUI-0)

`TuiMedium::extent()` asks its `Sink`. A Sink is anything with `write(std::string_view)` **and**
`TerminalSize size() const` — required, not detected, because a Sink that quietly lacked the
method would be a Sink whose terminal is permanently unmeasurable, which is an ordinary honest
state a pipe reaches every day; the mistake would look exactly like the truth on every lane,
forever.

```text
native_terminal_size()   surface/terminal_size.hpp -- the ONE #if defined(_WIN32) any of this needs
TuiTerminal::size()      gated on the same `ok_` the alternate-screen CLAIM is
tui_canvas_extent()      pure: a terminal size -> what a CANVAS fits in it
SkinT::report_extent     publish on change, never publish "no opinion"
```

- **The medium answers about a CANVAS, not about the terminal.** `kTuiReservedRows` is 3: two for
  the status and score slots (`kTuiCanvasTopRow`, consulted from `pointing.hpp` rather than
  restated) and one because `canvas_body` ends **every** row with CRLF, so a canvas whose last row
  lands on the terminal's last row feeds past the bottom — and a feed there SCROLLS, taking the two
  slots with it. Buying that row back by not feeding after the last row would move every terminal
  golden in this repository; a row is cheaper than a byte-exact projection.
- **`{0,0}` means "no opinion" and is never published.** An unmeasurable terminal and a
  terminal with no room left both reach it — two sentences the medium cannot tell apart to its
  shell, and both correctly become SILENCE. Silence leaves Workshop on `kScreenMinW`/`kScreenMinH`.
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

## The graphical Skin carries a typeface (HD-1)

`ZENGINE_SDL_SKIN=ON` fetches **three** pinned-and-checksummed dependencies, not one: SDL3,
SDL_ttf, and — because SDL_ttf hard-requires FreeType and its release tarball deliberately does
not bundle it — FreeType, extracted into SDL_ttf's own `external/freetype` before its
subdirectory is added. `cmake/ZengineSdl.cmake` owns that assembly, states why both of SDL_ttf's
own doors were measured shut on the lanes this repository builds on, and fails with an
actionable message if the two contents' download stamps ever fall out of step.

The face itself (`surface/fonts/JetBrainsMono-Regular.ttf`, SIL OFL 1.1) is **bundled and
distributed**, unlike the fetched libraries: `cmake/EmbedBinary.cmake` turns its bytes into a
translation unit compiled into `zengine-skin-sdl`. Nothing is installed, staged or discovered at
runtime. Provenance and obligations: `surface/fonts/PROVENANCE.md`, `THIRD_PARTY_NOTICES.md`.

## The SDL renderer viewport has two states (HD-1, pinned HD-4)

**Do not save and restore a renderer viewport through `SDL_GetRenderViewport` alone.** SDL
keeps two states and that call flattens them: a renderer with no viewport of its own answers
with the whole target's rectangle, and setting *that* back makes the viewport explicit, after
which SDL stops growing it when the output does. The symptom is a window dragged larger whose
picture is still clipped to the old size, one frame after the part drawn inside a region has
already reflowed. Ask `SDL_RenderViewportSet` first and restore `nullptr` when it says false
(`surface/skin_sdl_text.hpp`, pinned in the `sdl` gate).

## Input scan names are NAMES for values that already arrived (HD-4)

`scan::kHome`/`kDelete`/`kEnd` are names, not new reach. `translate_sdl.hpp` passes SDL's
scancode through untranslated, so the SDL backend has always delivered them unnamed; the POSIX
terminal backend drops their CSI sequences and the Win32 console backend maps their VKs to
`kUnknown`. A constant here is not a claim that every backend can produce one —
`translate.hpp` remains where each backend's honest reach is written.

## Do not assume

- A metric identifies a graphical medium — `RegionFit::graphical()` is the partition, and a
  window whose font failed to open publishes `{w,h,0,0}` and still lays its canvas out at
  `kCanvasCellPx`.
- A sub-cell remainder is device pixels — it is 1/`kCellSubs` of a CANVAS cell, a
  medium-independent fraction; the shipped skin's pixel happens to be four of them (12
  divides 48), which is an alignment worth having, not a contract a future medium must meet.
- `fit_region`'s 6-argument form takes sub-units — it takes CELLS, exactly as it always has;
  the sub-unit entry is `fit_region_subs`, and the shape overload routes through it so a
  fine region fits at its fine place.
- `kCanvasCellPx` is a standing fact Workshop may hold — `surface/pointing.hpp` forbids it; a
  pointer may spend it only because the event carries `input::space::kPixels`, a stamp on that
  moment. `SurfaceExtent` carries no such stamp.
- A fifth semantic role can be added when a consumer wants one — `surface/vocabulary.hpp`
  refuses it; the vocabulary is deliberately closed.
