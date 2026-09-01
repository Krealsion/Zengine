# Agent law — Workshop

Routed detail behind [`AGENTS.md`](../AGENTS.md), for tasks touching `workshop/` and
`component/` — the screen's composition, panes and authored windows, the Info panel, the
terminal pane, interaction routing, and session persistence. The external pane seam and the
tools that arrive through it are [`panes.md`](panes.md); the Surface vocabulary itself is
[`surface.md`](surface.md). Phase tags like (HD-7) are provenance markers into this
repository's history; the law here is current.

**Where a case goes.** Workshop's tests are seven suites, one per area — document, screen,
panels, panes, persistence, load, editor — and a new case belongs to the one whose subject it
proves. [`verification.md`](verification.md) names them, says what shared support may be, and
says why a temporary directory belongs to a suite.

## One geometry draws a thing and hits it (HD-3)

**The geometry that draws a thing and the geometry that hits it must be the same geometry.**
`terminal_input_place` (`workshop/screen.hpp`) is the pane's editable line resolved once, and
`paint_terminal`, the caret and `terminal_press` all call it. `completion_first_shown` was
lifted out of `completion_rows` for the same reason — a second copy of the list's windowing
is right until the first scroll, which is to say wrong only when nobody is looking. Do not
add a `click_*_bounds()` beside a `paint_*_bounds()` anywhere in Workshop.

## The reserved column is nobody's to spend (HD-10)

The terminal pane's right edge is the **workspace's** right edge — `Screen::room_w` — and not the
screen's. Two expressions in `screen_of`:

```text
pane_want    kTerminalWantW + (w - kScreenMinW)/2     G-2's half-share
terminal_w   min(pane_want, room_w)                   the room is the ceiling
terminal_x   room_w - terminal_w                      anchored to the ROOM's corner
```

- **The law is about the RESERVATION, not about overlap.** Reaching into the column the screen
  subtracted for somebody else is forbidden; overlaps *inside one owner's room* are intentional
  and remain: the completion list over the pane's transcript, the picker over the stack slot
  beneath it, and the pane itself over the workspace and the bottom band (which the screen
  answers for by not painting the notice or the help lines at all while the pane is open). A
  test that forbade overlap generally would forbid all three.
- **Said twice, on purpose.** `terminal_x + terminal_w == room_w` and
  `terminal_x + terminal_w <= panel_x - kPanelGap` are both asserted, at `kMinScreen` in the type
  system and over eleven extents × three metrics in the suite. They are not redundant: a pane
  whose right edge sits exactly on `panel_x` shares no *cell* with the side region and is still
  wrong, and that mutation was measured passing the cell count while both edge assertions
  reddened.
- **`kTerminalWantW` is a want, not a floor.** At 78–80 columns the room is narrower and the
  pane gets the room; the want and the room agree from 94 columns up. The price is eight cells
  of pane at the minimum extent, and it is visible: the pane's standing statement is elided by
  `detail::fit` below 81 columns — pinned in both directions (the *answer* to `help` is still
  asserted whole, the legend's cut is asserted as present, and the extent where it stops being
  cut is named).
- **One overlap between independent presentations remains, measured rather than hoped:** the
  pane's top rows against the overlay stack's lowest reachable slot — bounded because the pane's
  left edge moves right at exactly the rate the slot's right edge does, so the contested columns
  never exceed `kTerminalWantW` at any extent (504 shared cells at the measured worst). Both are
  overlays in the room the workspace has — the stack grows down from the top-left, the pane up
  from the bottom-right. Repairing it would mean a *second* reservation `screen_of` does not
  make, tying the pane's height to `kStackRows`. Pinned as a case so it is a known fact.
- **The composition answer is medium-independent by construction**: it is settled in canvas
  cells before any metric is consulted, and a metric only ever changes how much prose fits
  inside a placement it did not choose.

## A wider room is shared by the pane and the maker (WIND-1)

An overlay-stack slot resolves its width to `kStackW + (sc.room_w - kStackW)/2` — the minimum
composition's 48 cells plus **half** the room's surplus over that, floored — while its column,
its row, its height and the blank row between slots are untouched.

- **It is `screen_of`'s own rule**: the terminal pane's half-share, with `kStackW` as the base
  and the surplus measured against `kMinScreen.room_w` (asserted equal to `kStackW` since
  PNL-1). One expression, no threshold, no cap, no new constant.
- **The FLOOR is load-bearing.** At 79 columns the surplus is exactly one; rounding up would
  spend it and leave nothing reachable. Floored, the odd column stays the maker's. `kMinScreen`
  is byte-identical.
- **A width is not a height.** `stack_slots_that_fit` reads `y` and `h`, so the vertical
  capacity is identical on the narrowest screen and the widest at every height. A width edit
  must not buy a slot, and the suite sweeps that.
- **Every added cell is paint AND pointer.** `paint_panel_frame` fills the whole rectangle,
  `occupied_at` owns the whole rectangle, and a press inside it is answered with the panel's
  sentence rather than reaching `take_hold`. The honest half of the phrase: `room_w > kStackW`
  implies `x + w < room_w` — columns of the panel's own rows stay reachable at every extent
  above the minimum. A full-width slot leaves zero, which is what this width was chosen
  against. A drag begun in that band still walks under the panel and releases normally (PNL-2).
- **An external pane's room follows it.** `external_body_place` is the slot less its header row
  and `refresh_external_rooms` grants `fit_region`'s answer over it — republished exactly when
  the capacity changes and never otherwise, and every grant clears the retained rows first, so
  a dragged window edge briefly shows `(waiting for the provider)`.

## The code authors a default; the maker authors an override (WIND-2, lattice WUX-2)

Setup format **version 3**. Each pane row carries a durable `PaneRef` plus the smallest authored
difference from the developer's answer, and nothing else — with the geometry on the FINE
LATTICE since WUX-2: sub-cell units of 1/`surface::kCellSubs` (48) of a canvas cell, so a
pixel of hand is authorable and a cell boundary is an exact multiple:

```text
place  {mode, x, y}       mode: default | subcells          absolute canvas position on the
                                                            fine lattice, never an offset
width  {mode, amount}     mode: default | subcells | pixels  per axis, independently
height {mode, amount}     mode: default | subcells | pixels
front  integer            a permutation of 0..n-1 over ALL rows, 0 back-most
```

- **A VERSION-2 WHOLE-CELL FILE STILL LOADS (WUX-2's one legacy reader).** Its bytes are
  admitted against the RETAINED v2 shapes (`setup_persist::v2`, same wire names and
  versions) and its cells are mapped exactly (x 48) onto the fine lattice, so an old desk
  resolves to the identical pixels and characters it always did; the next explicit save
  writes v3. `cells` is v2's word and is NOT in v3's vocabulary; the session file marched
  with the nested desk (session v2, its own v1 legacy road). Every other version is refused
  by its number. This is one namespace and one exact multiply, not a migration framework —
  the clean-break stance stands for every other transition.

- **`default` is a VALUE, and its unused numbers must be zero.** Loom's admission refuses an
  unknown field and has no optional, so absence cannot be spelled by omitting one; and a magic
  coordinate is a value a maker could otherwise mean. Requiring the zeros is what gives absent
  intent exactly ONE canonical spelling — a file carrying `{"mode":"default","x":7}` would
  round-trip a number that means nothing, and the first reader to wonder what it meant would be
  right to.
- **The mode is a WORD in the file and a closed set** — the in-memory 0/1/2 are arbitrary and a
  renumber would silently change every saved arrangement. An unrecognised word refuses the
  WHOLE candidate and names both what it found and what would have worked. A PLACE has two
  words and a SIZE has three — `pixels` offered to a place is a word that field's vocabulary
  does not have.
- **The version and the envelope's shape version are ONE NUMBER**, with a `static_assert`
  making that a compile error to break. That is what buys the ORDERING the format needs: a
  version-1 file's bytes claim `WorkshopSetup v1`, so the gate refuses it on the CLAIM before
  it has read a single pane row — and a version-1 file can therefore never be reported as *a
  pane row is missing `place`*, a true sentence about a false cause. `from_text`'s preflight
  reads the claimed version and says it in Workshop's own words, by number. The
  `format_version` FIELD is still checked afterwards, for the forgery only a reader of this
  format would produce.
- **`pixels` is declared, valid on every medium, and refused at PROJECTION on all of them.**
  WUX-2 deliberately did not light it: sub-cell units refine the medium-independent lattice,
  which is a different thing from a device pixel becoming authored truth. No
  medium here publishes a trustworthy per-axis device-pixel scale for a canvas cell, and both
  near-misses are traps ([`surface.md`](surface.md#do-not-assume)). The refusal is WHOLE, per
  `doc::resize`'s and `PaneContent`'s law: a pane with either axis in pixels is **not
  presented**, never presented at the default width with an honoured height, **Info included**
  — fixed placement is not permission to present an unsupported unit as understood. Authored
  bytes are exact through the refusal, and reset and order still recover. **Do not add a
  per-axis fallback**; it is exactly the silent default this refuses. A unit outranks a
  reservation in `arrange_geometry_ready`, the same precedence `pane_state_of` spends between a
  unit and a want of room. `pane_unit_projectable(authored)` takes no placement.
- **`front` is a canonical rank and never an accumulating counter.** `max + 1` is an operation
  TRACE: alternating `front(A)`/`front(B)` produces the same two semantic orders forever while
  the integers grow, so a legal gesture would eventually fail on a setup for which a bounded
  spelling always existed. A permutation of `0..n-1` is *unique* for a given order, so reset
  writes bytes identical to a setup that was never reordered — measured, not argued. There is
  no tie, so the resolved order needs no secondary key at all.
- **The rank is over ALL authored rows, including unresolved ones.** The presented order is
  that permutation RESTRICTED to what was seated, and a restriction of a total order is a total
  order — so a pane that stops resolving keeps its exact place for free and gets it back, with
  no byte of the file changing either way.
- **`panels.open` is NEVER reordered, and that is the whole of "raising a pane cannot move
  it".** `seat_panes` walks the setup LIST, `reconcile` assigns `panels.open` from that answer,
  and `bounds_of` counts a reactive slot over `panels.open`. No ordering operation writes
  anything any of the three reads. `effective_pane_order` is the one FOREGROUND order helper
  (WUX-5); paint walks it ascending and `occupied_at` descending, and `presentation_order` is
  the authored base it lifts the selection out of.
- **An authored place spends no reactive slot and cannot WAIT for one.** A pane the maker put
  somewhere is not in the tiling, so `waiting` means exactly what it always meant: the reactive
  default ran out of tiles. Both `seat_panes` and `bounds_of`'s slot counter say it, and both
  are pinned.
- **The override is spent on `kOverlayStack` and nowhere else.** `screen_of` reserves the side
  column whether or not Info is open and `room_w` is what every share of the workspace resolves
  against, so a movable Info would change the resolved size of objects in a maker's document —
  PNL-0's refusal, unchanged. A side-region row's authored geometry is retained in the file,
  never rewritten, and never spent; arrangement refuses to author one and says which reservation
  it hit.
- **The host clips and never rewrites.** `bounds_of` answers with the VISIBLE rectangle
  (resolved, then intersected with the canvas), so every consumer that already read an empty
  rectangle as "nowhere" is correct for an off-room pane with no branch of its own;
  `PanelBounds::resolved` carries the unclipped ask for the state classifier, which has to tell
  *partly cut off* from *not on this screen at all*.
- **Seven states, one classifier, and a precedence.** `closed`, `unresolved`, `refused`,
  `waiting`, `off-room`, `covered`, `open`. A UNIT outranks a want of room: a pane with a pixel
  axis and no tile left is `refused`, because a taller window would give it the tile and it
  still would not be presented. `covered` means every visible cell is behind the **union** of
  what is in front — two panes that each cover half of a third leave nothing showing, and a
  test that asked "is it inside some ONE pane" would call that `open`. One visible cell is
  enough to be `open`. The state column is ELEVEN cells (`detail::pad` truncates and
  `unresolved` is ten bytes).
- **A resize begins from the RESOLVED WINDOW** (`managed_bounds`) — place beside size since
  WUX-2, captured at the press. `rect` is the visible intersection and owns painting,
  occupancy, coverage, affordance placement and whether geometry is reachable; `resolved` is
  the unclipped ask and is what a first edit captures. Capturing the visible one makes a
  default pane resolving to 89 cells with four on screen answer one rightward step by
  authoring **five cells' worth**. The affordance stays on the visible boundary — that is
  where the eye and the hand are — and its delta applies to the resolved window, for the key
  and the pointer alike.
- **EVERY RESIZE EDGE PRESERVES ITS OPPOSITE ANCHOR (WUX-2, reversing WIND-2's rule), AND
  INDEPENDENT AXES SETTLE INDEPENDENTLY (WUX-2a).** The edge a hand pulls follows the hand;
  the edge opposite holds still: a top pull changes `y` and the height TOGETHER so the
  bottom edge stays put (`pane_window_proposal` — WUX-2's START tree measured the bottom
  edge moving instead, pinned dead), and a corner holds the corner across from it.
  Right/bottom pulls anchor the place by NOT writing it — a default place stays reactive —
  while a left/top pull authors place and size as one AXIS-LOCAL transaction: the
  position+extent pair its anchor couples settles together or not at all, so a refused
  height can never leave a moved top edge behind (`doc::resize`'s both-before-either law,
  scoped to the axis the anchor actually couples). The OTHER axis is not part of that
  transaction: a move or corner gesture blocked on one axis — a drag past the left wall —
  still settles the other axis's legal proposal, the refused axis keeps its own value
  rather than clamping (refuse-never-clamp, per axis), and an axis the gesture did not
  change is not a proposal at all, so a refused single-axis step writes nothing and cannot
  author a reactive place as a side effect. `author_pane_window` (setup.hpp) is the one
  gesture door owning that settlement; `author_pane_place`/`author_pane_size` remain the
  VALUE doors, atomic whole, for a value stated as one thing. WIND-2's old objection — that
  a left edge moving the place is two writes for one gesture — is answered by making each
  axis one door-judged transaction rather than by refusing the geometry a hand plainly
  means.
- **A PANE'S GEOMETRY IS SAID IN THE ACTIVE FACE'S UNIT (WUX-6).** `pane_window_text` takes
  `Session::cell_px` and `subcell_text`'s exact mixed number (`10+1/2`) is retired — see
  [the WUX-6 section](#a-maker-reads-their-pane-in-the-unit-their-face-reported-wux-6). The
  authored bytes and the refusal of `pixels` at projection are unchanged by any of it.
- **Escape is BACK, not cancel.** Every immediate-commit gesture in this application is
  reversible only by performing the inverse, and there is no undo. The help says `esc back`.
- **ARRANGEMENT IS TWO SCOPES AND ONE VOCABULARY (ARR-0).** `PaneArrange{open, desk, pane,
  resetting}` replaced the old selector-with-submodes: moving and resizing a pane are ONE
  maker intent, so there is no Move step, no Size step and no edge-picking step. The
  ONE-PANE scope (`desk == false`, entered by the context menu's `arrange` row or the
  desk's Return, admission BEFORE binding via `arrange_geometry_ready`) is bound to
  exactly `pane`: its body moves it, its ring sizes it, and a press anywhere else is
  consumed with the sentence naming the state — another pane cannot be drawn in. The DESK
  (`desk == true`, `w` from command mode paying the `swallow_text_` rule once; id still
  `workshop.manage`, label `arrange desk`) opens with NO pane addressed — a selection
  prerequisite is exactly what it retired — and every arrangeable pane answers the pointer
  directly, topmost first: a press takes hold AND makes that pane the keyboard's target.
  Keys in both scopes: arrows place one cell, shift+arrows pull the extent one cell
  anchored at the place (`pane_edge::kBottomRight` through the same proposal door — the
  other six anchors are the pointer's, on the handles), `=`/`-` the COARSE step through that
  same door (WUX-6), `f`/`b`/`r`/`l` order, `d` remove,
  `0` the reset prompt (`resetting`; `p`/`w`/`h`/`o`, `esc` back), `esc` leave; the desk
  adds `tab`/`shift+tab` stepping (over `arrangeable()` — every setup row, the recovery
  invariant) and Return. The ROSTER PANEL IS RETIRED: the state's visible statement is the
  affordance RINGS on the panes themselves (`paint_pane_affordances` — one-pane scope
  accent; desk muted over the set, accent on the target; a held size drag accents the
  dragged edge from `pane_drag`), the band's generated legend, and `arrange_status()` on
  the notice line, which carries the pane's STATE word so an invisible pane is recoverable
  by ear: step to it, read what it is, reset it.
- **The picker keeps PRESENCE and arrangement never touches it.** `inventory_rows` is
  the combined catalog UNION every reference the setup names, so an unresolved pane has a
  row and can be removed by the gesture that removes any other; an unresolved row carries
  `kNoPaneKind` (negative, for `role::kNone`'s reason) so nothing can present it as the
  Builder. Selecting an open picker row removes it (PNL-0); arrangement binds no toggle,
  adds nothing and offers nothing — participation and arrangement are separate concerns.
  **One picker inventory:** `picker_population()` is `inventory_rows(active, panels)` and
  the painter, the cursor bound, the Return action and the cursor repair all spend it —
  widening only the painter to the union leaves a row a maker can see and cannot reach.
- **Pane rectangles are SUB-UNITS of the canvas lattice (`FineRect`, WUX-2)** — a distinct
  type from `ui::Rect` so the compiler separates the two lattices; convert only through
  `fine_of_cells` / `cells_covered` (the cell-grain quantization law), and never pass one
  through `workspace_cell_x/y` — that conversion belongs to authored document objects, which
  stay whole-cell. `PanelBounds` carries fine rectangles; screen furniture, the document and
  every placement default stay `ui::Rect` cells and enter the fine lattice exactly, at
  `project_pane`, by one multiply.
- **ONE QUANTIZATION LAW, EVERY CONSUMER, EVERY GRAIN (WUX-2).** A presenter whose device
  unit is `g` sub-units (a terminal cell 48, the shipped skin's pixel 4) shows a fine span
  `[L, R)` on device units `[floor(L/g), floor(R/g))`; hit testing floors by the SAME grain
  (`sub_span_contains`, `FineRect::contains_at`, the pointer's grain riding `PointedAt`), so
  the first painted pixel of a fractional edge answers the hand and the pixel before it does
  not — what you see is what you can grab, as an identity rather than an intention. The TUI
  quantizes at ITS projection (`canvas_body`, the cell projection) and never writes back:
  exact-cell values stay exact, sub-cell values resolve deterministically, and any number of
  frames rewrites nothing.

## A maker reads their pane in the unit their FACE reported (WUX-6)

One authored value, on one medium-independent lattice, read through whichever face the maker
is sitting at:

```text
authored     what the maker chose -- sub-units of a canvas cell (WUX-2), the setup's own
             number, unchanged by any of this
projected    the nearest thing the ACTIVE medium can say, in the device unit that medium
             itself reported -- `Session::cell_px`
```

- **THE UNIT IS THE MEDIUM'S ANSWER, NEVER WORKSHOP'S.** `SurfaceExtent::cell_px`
  ([`surface.md`](surface.md#the-lattice-is-fine-and-each-medium-floors-at-its-own-grain))
  arrives on the same message the room and the face metric do; `adopt_screen` takes it beside
  them, `Session::cell_px` holds it, and **zero is "my device unit IS the cell"** — every
  terminal, and any run no medium has spoken to. Workshop may not derive it: an application
  holding one Skin's layout number is correct only for as long as it has one medium
  (`surface/pointing.hpp`), which is the whole reason WUX-2 declared `pixels` and refused it
  at projection. A change of unit ALONE is a change, so a window that opens its canvas late
  does not leave a maker reading cells until something unrelated moves.
- **ONE DERIVATION, AND IT IS NOT A UNIT SYSTEM.** `geometry_unit` (the word),
  `geometry_spelling` (the number and whether it is EXACT), `geometry_amount_text`,
  `fine_rect_text`, `pane_window_text` — five small pure functions in `screen.hpp` over
  `surface::device_of_subs`, which is the same arithmetic the shipped face paints and
  hit-tests by. No unit type, no registry, no per-medium table, and **no second conversion
  constant anywhere in Workshop**.
- **A PROJECTION WEARS `~`, AND THE LINE NAMES THE REASON ONCE.** A whole-cell value is exact
  on every medium; a value authored at a window's pixel grain is exact in pixels and in
  general is not exact in cells. `~34x~19 cells (~ projected)` and `417x233 px` are the same
  authored value. The mark is ASCII because the shipped face's letterform is 0x20–0x7E, and
  the clause appears only when something on that line actually is a projection — the
  distinction is inspectable, never permanently lectured. This is WUX-2's SC-9 (never present
  a rounded value as the stored one) kept, with the exact mixed number `10+1/2` retired: it
  was exact and unreadable on a window, and `126` is exact there.
- **LOOKING IS NOT AUTHORING.** No readout path writes: not the spelling, not the notice, not
  a repaint, not a save. Proven by byte identity — a session that crosses both media reading
  a geometry no terminal can say writes the same session file, byte for byte, as one that
  never crossed. The unit itself reaches no durable file (`session_persist`'s own rule about
  the text metric), and a session RESTORE hands this run's unit straight back rather than
  resetting it to the character reading.
- **`arrange_status` ALSO SAYS WHERE A PANE THE MAKER DID NOT PLACE ACTUALLY IS.** A reactive
  axis's authored text is `-`, which is true and is not a rectangle, so a window still partly
  the code's answer is followed by ` -- now @x,y WxH <unit>` from `managed_bounds().resolved`
  (the unclipped ask — the same rectangle a gesture measures from). A fully authored window
  gets no such clause: there the authored text already IS the rectangle.
- **AN AXIS AUTHORED IN `pixels` KEEPS ITS OWN INLINE `px`** whatever the medium is, because
  that is the unit the maker's file says. On the shipped face that reads `483x220px px`, which
  is redundant and true; the state word beside it is `refused`, because `pixels` is still the
  unit no medium here presents.

## The coarse step is the fine step with a bigger delta (WUX-6)

`=` grows the addressed pane and `-` shrinks it, by `kCoarseStepCells` (4) on **both** axes,
in both arranging scopes.

- **IT IS `arrange_grow`, THE SAME FUNCTION A SHIFTED ARROW SPENDS** — through
  `pane_window_proposal` anchored at `kBottomRight`, through `author_pane_window`. So it
  cannot move the pane it resizes, cannot author a reactive place as a side effect, moves no
  other pane, performs no collision avoidance, measures no content, and meets the identical
  per-axis settlement: a shrink that would take the width below one cell keeps the width and
  still shortens the height (refuse-never-clamp, per axis, WUX-2a). There is **no second
  geometry owner** and no second proposal.
- **FOUR IS MEASURED, NOT CHOSEN.** It must be unmistakably coarser than the one-cell step
  beside it, and one press must clear the tightest measured pane on the shipped desk with
  room to spare: a default stack pane's external body is `kStackRows - 2*kChromeCells - 1` =
  6 rows, the Compose form needs 8, and a step of two would land on exactly eight. A
  `static_assert` beside the constant is what keeps that true when `kStackRows` or
  `kChromeCells` moves.
- **IT CLOSES WUX-5's COMPOSE FINDING AT THE DESK.** `composer/view.hpp` is byte-identical:
  the Composer's composition priorities were not touched, and what changed is that a maker
  can give the pane the room in one ordinary key. Witnessed off the published canvas — Submit
  and no fields at the developer's default, the whole form and its Submit after one grow, and
  a real `StartTimer` submitted from the grown pane.
- **IT IS ORDINARY ACTION VOCABULARY.** `manage.grow` / `manage.shrink` are durable ids a
  maker's keymap file rebinds, declared in both scopes so one override moves both, said by
  the band's legend and the full hotkey view — and painted into **no** pane's chrome, which
  is WUX-5's rule and still holds. `=` and `-` are the two keys a hand already reads as
  bigger and smaller, and both are plain printable ASCII, which `ctrl+shift+<letter>` is not
  from a POSIX terminal.

## A pane has an edge, and the edge is inside the pane (WUX-5, thinned by WUX-8)

Every ordinary pane and every framed transient surface draws a visible boundary, and the
boundary is subtracted from the rectangle it already had:

```text
pane_outer          bounds_of's answer -- authored, dragged, hit-tested, ringed. UNCHANGED.
   +-- chrome       ONE UNIT OF THE ACTIVE FACE on every side
   +-- pane_inner   pane_inside(outer, sc).rect -- what a painter, a press or a room may spend
```

- **THE PANE OWNS THAT IT HAS CHROME; THE FACE OWNS HOW FINELY IT CAN BE DRAWN (WUX-8).** A
  terminal spends one canvas cell, because that is the smallest boundary a character medium
  can show. The shipped window spends **one device pixel**. Both are presentations of the
  same authored rectangle: `chrome_grain(sc)` is `surface::subs_of_one_device(sc.cell_px)`,
  and `cell_px` is the number the MEDIUM reported (`SurfaceExtent`, WUX-6) carried onto
  `Screen` beside the text metric. Workshop may not derive it — an application holding one
  Skin's layout number is correct only for as long as it has one medium
  ([`surface.md`](surface.md#the-lattice-is-fine-and-each-medium-floors-at-its-own-grain)).
  Chrome thickness is **presentation**: it is on no shape, in no file, and nothing about it
  is authored.
- **...AND THE FACE'S UNIT IS THE UNIT IT PRESENTS *THIS INTERIOR* IN**, which is the half
  that keeps the thin boundary honest. A boundary nobody can see is not a boundary. Where a
  face sets a pane's interior in its own type, that interior is a PIXEL viewport and a pixel
  of chrome is visible ink. Where it describes the same interior in CELLS — every terminal, a
  window whose font never opened, a window whose face is too tall for this pane (HD-5) — the
  interior is projected onto covered cells, a sub-cell inset is projected away, and the body
  would spill back over its own left and top edge leaving a ring on two sides. `pane_inside`
  therefore resolves the interior **and its presentation together** and pays the cell wherever
  the cell is what will be drawn. Two candidates at most, and it cannot oscillate: the cell
  inset is the smaller interior, so a rectangle that held no row of the face at the finer
  inset holds none at the coarser one either.
- **THE BACKDROP IS THE BORDER**, which is one rectangle rather than five: `paint_panel_frame`
  pushes the outer rect and the body region drawn over it OWNS its ground (`kGroundOwn` clears
  the whole of its bounds in both media), so what remains visible is exactly the ring. The
  ring IS `b` minus `pane_inside(b, sc).rect` — there is no border arithmetic anywhere and no
  thickness on the paint call to get wrong, which is why the thin boundary needed **no
  painter**: a face that draws the interior in pixels leaves a one-pixel ring of that same
  rect, and a face that draws it in cells leaves a one-cell ring of it.
- **ONE SUBTRACTION, INSIDE THE THREE BODY RESOLUTIONS.** `external_body_place`,
  `info_body_place` and `panel_prose_place` each call `pane_inside` on the rectangle they are
  handed, so the painter, the press inverse and the room a provider is granted are one
  geometry BY CONSTRUCTION — there is no call site that could spend the outer rectangle for
  prose, and none that could re-fit the interior a second time (`PaneInside` carries the
  `RegionFit` beside the rectangle for exactly that reason). `pane_interior(outer, chrome_subs)`
  is the raw subtraction underneath, and its thickness is a REQUIRED argument: a default there
  would be the forgotten call site that pays a cell on a face that can draw a pixel.
- **`chrome_outer_of` RESERVES THE CELL ON EVERY FACE, and that is not an oversight.** It is
  the subtraction read backwards for the one surface sized by its own content (the contextual
  popup), and what it answers is ONE whole-cell `ui::Rect` shown on whichever face draws it —
  so the boundary it must make room for is the coarsest any face can spend, `kChromeCells`. A
  graphical face draws a thinner ring inside that reservation and hands the difference to the
  popup's own interior; a popup sized for pixels would cut a row off itself the moment a
  terminal drew it, and its PLACEMENT would depend on the face.
- **SELECTION CHANGES THE INK AND NOT ONE NUMBER.** Ordinary and selected chrome have
  identical geometry — same outer rect, same body rect, same row/column capacity, same press
  inverse — so a maker pointing at a pane never makes its contents jump.
- **THREE CHROME ROLES, FROM THE CLOSED VOCABULARY** (`surface/vocabulary.hpp` refuses a
  fifth): `kPaneChrome` = `kFill` (ordinary material, the word an authored object's body
  already speaks), `kPaneChromeSelected` = `kAccent` (the one being pointed at — the same word
  the document's selection ring speaks, so the desk and the document say selection once), and
  `kTransientChrome` = `kMuted` (quiet furniture, in front of everything already).
  ⚠ In a character medium `kMuted` is the workspace backdrop's own glyph, so a transient
  surface's edge over bare workspace is drawn by the hole its interior clears rather than by
  its ring; the shipped face has three distinct inks. Do not repair that with a fifth role or
  a per-medium palette.
- **WHAT IT COSTS, measured, and it is now TWO answers.** On a terminal every stack pane's
  interior is 46x7 where the slot is 48x9, so the Builder drops its lowest-priority row at the
  default height, the picker windows two entries sooner, and the Info panel's lists each lose a
  row — each of those is `list_window`'s or the Builder's own composition priority doing its
  declared job, and the Compose pane's FORM does not fit the default at all (see
  [`panes.md`](panes.md); WUX-6 closed that at the DESK with one coarse grow, and
  `kCoarseStepCells`' `static_assert` keeps the two numbers tied together — it is written
  against `kChromeCells`, the coarsest case, which is the honest floor for a claim about
  *every* face). On the shipped window the same authored 48x9 slot is 576x108 device pixels
  and its interior is **574x106**, one pixel a side instead of twelve — which at an 18-pixel
  line is **five** body rows where the cell inset gave four. That extra room is an honest
  consequence and nothing pads it back: no provider, no priority and no form was touched to
  spend it.

## Three places, and only one of them is the screen's (PNL-1, third place WUX-12)

```text
kSideRegion   the reserved column beside the workspace   THE SCREEN'S: place-fixed, no override
kOverlayStack over the workspace, stacked, reactive      the maker's
kTopBand      the reserved rows at the top, full width   the maker's
```

- **`place_is_authorable(where)` IS THE EXCLUSION, AND IT IS ONE SENTENCE**: the side region is
  the screen's; every other place resolves a developer default that an authored row lays over,
  per axis. Four consumers spend it — `project_pane`'s override gate, `take_pane_hold`,
  `paint_pane_affordances` and the arrangement admission — and three of them used to say
  `== kOverlayStack`, which is the same set written as a list somebody has to extend.
- **EACH PLACE HOLDS THE PANES ITS GEOMETRY CAN SHARE**, asserted at compile time:
  `kinds_placed_in(kSideRegion) == 1` and `kinds_placed_in(kTopBand) == 1`, because two kinds
  in either resolve to the SAME rectangle and would paint over each other silently. The overlay
  stack carries no such assertion — stacking is what it is for.
- **A BAND-ANCHORED PANE TAKES NO STACK SLOT** (`bounds_of`, `panes_that_fit`): slots are the
  overlay stack's rationing and a pane that is not in the tiling must not push a reactive one
  down a row.

## A pane too small for a boundary draws none (WUX-8, third rung WUX-12)

`pane_inside` walks candidates from the finest boundary the face can draw to no boundary at
all, and stops at the first that leaves an interior: the face's own device unit, then the
CELL, then **zero**.

- **THE LAST RUNG IS WHAT LETS A TWO-CELL PANE EXIST.** The Layouts pane's developer default is
  `kTopRows` tall on every face; on a terminal one cell a side leaves zero rows of the two the
  band always had, and a pane that draws its boundary and nothing else has stopped being a
  presentation. On the shipped window a one-pixel edge still fits inside the same rectangle and
  is still drawn — measured: `chrome_subs == 0` on the cell medium, `subs_of_one_device` on the
  face.
- **IT CANNOT OSCILLATE**: each candidate is a LARGER interior than the last, so a rectangle
  that held no row at a finer inset holds none at a coarser one.
- **WHAT IT COSTS** is that a pane that small wears no selected-chrome ink, because there is no
  ring to colour. The maker's other answers — the arrangement handles, the desk's stepping, the
  notice, the picker — are untouched.

## The desk's front is the authored order plus one lift (WUX-5)

```text
authored/persisted pane order  +  the current selected-pane lift
    =  effective ordinary-pane presentation order
```

- **`Panels::selected` IS A PRESS'S MEMORY AND `selected_pane` IS THE ANSWER**, exactly
  `Panels::keyboard`/`keyboard_pane`'s shape one question wider — and written by the SAME line
  in `on(PointerButton)`, with the keyboard candidate DERIVED from it through the declared
  candidacy rather than the occupancy being tested twice. Session only: not in the setup, not
  in the document, not persisted, not restored, and `kNoPaneKind` is where every session
  starts. The Builder's edit-source door is its second writer, for the keyboard candidate's
  own reason.
- **ARRANGING A PANE IS CHOOSING IT, AND IT IS THE THIRD WRITER (WUX-7).**
  `enter_arrange_pane` sets `Panels::selected` from the ADDRESSED reference — after admission,
  never before, so a refusal still leaves the maker exactly where they were — and everything
  else follows from the fact that already existed: selected chrome, the lift in
  `effective_pane_order`, paint order, hit order. No `front` rank is touched, no
  arrangement-specific z-order exists, and there is no second foreground fact to keep true. It
  writes `Panels::selected` ALONE: where the keys go when the scope closes is a separate fact
  with a separate owner, and the two are not collapsed merely because they happen together.
  The contextual door spends the CAPTURED subject (`spend_context_choice`), so the pane that
  lifts is the pane the maker pointed at rather than whichever was topmost when they chose.
- **`effective_pane_order` (setup.hpp) IS THE ONE ANSWER**, and every consumer whose meaning
  is literally foreground order spends it: `paint_panels` ascending, `occupied_at` descending,
  `pane_is_covered`, and the arrangement desk's pointer walk. They cannot disagree because
  there is nothing to disagree about. `presentation_order` remains the AUTHORED base and is
  what persistence and `reset order` want; nothing that means "in front right now" may call it.
- **THE LIFT IS A ROTATION AND NEVER A WRITE.** No rank is read differently, none is written,
  `panels.open` is untouched, and nothing reaches a file. A selection that is not seated lifts
  nothing, so a closed or unresolved pane leaves no ghost foreground identity — `bounds_of`'s
  discipline, and the reason this needs no clearing path anywhere. `manage.front` remains the
  way to say *and I mean this permanently*.
- **THE TRANSIENT PLANES ARE STILL ABOVE THE PANES.** The lift orders the ordinary pane planes
  among themselves and reaches no further, so a selected pane is never drawn over the menu a
  maker just opened on it.

## The plane sequence is the layout of the screen (WIND-2a)

The canvas is an ordered list of planes ([`surface.md`](surface.md#the-canvas-is-an-ordered-list-of-planes-wind-2a));
Workshop's publication order is the whole depth story:

```text
the workspace       its backdrop, the scene, the size handle
one plane per pane  effective_pane_order(setup, panels) -- the canonical `front` ranks with
                    the selected pane lifted (WUX-5), ascending
the affordances     over the panes' own content, so no handle is hidden (ARR-0: the
                    rings ARE the arrangement state's visible statement)
picker / overlays   over the panes they cover -- a provider's text cannot bury the row that
                    recovers it
the screen's chrome the ONE band at the foot, a budget-composed region (WUX-1; the top band
                    became a pane at WUX-12 and is in the pane planes above)
the Terminal        the final modal plane
```

- **The screen's chrome is in FRONT of the panes, and that is a decision with a reason.** A
  band is where the tool SPEAKS: a panel backdrop drawn over one would erase the notice that
  just told a maker what happened, and since WUX-1 a band OWNS its whole rectangle (one region,
  `kGroundOwn`), so a pane a maker authors over it is covered by it. Panes are in front of the
  DOCUMENT, which is what `occupied_at` has answered since PNL-2; they are not in front of the
  tool's own voice.
- **⚠ AND THAT SENTENCE USED TO HAVE AN EXCEPTION IT CANNOT SURVIVE (WUX-12).** The TOP band
  was in this plane too, painted in front of every pane and answering presses on the layout
  tabs alone — so a pane a maker dragged under it was visually erased, still met the hand, and
  still classified `open` because coverage counts panes. See-here/press-there, at exactly the
  boundary HD-3 forbids. Both halves are gone: the tabs are a pane's interior and
  `occupied_at` answers that pane for those cells like any other. What is left in this plane is
  the foot, which occupies no pointer space at all — and the utterance channel's
  reachability is why it is still the screen's rather than converted beside the other one.
- **THE VERTICAL ORDER IS THREE REGIONS AND IT TILES THE SCREEN EXACTLY (QR-14):**

```text
rows 0 .. kTopRows-1               RESERVED, and by default the Layouts PANE stands on them --
                                   the selector, the setup's status, and the workspace fact
                                   under it where the medium fits a second row (WUX-12)
rows kWorkspaceY .. +room_h-1      the body: the workspace, the overlay stack, the side region
the last kBottomRows               the BOTTOM band -- the notice, then the legend
```

- **`kTopRows + kBottomRows == 6`, ASSERTED, AND THAT IS THE WHOLE OF WHAT QR-14 MAY SPEND.**
  It was 1 + 5 (a blank row 0, five band rows at the foot) and is 2 + 4. `room_h` is therefore
  byte-identical, which is the property that matters: the workspace's extent is what a share
  resolves against, so chrome that moves must not resize a maker's document — PNL-0's rule, and
  the same one WUX-1 obeyed by leaving row 0 empty rather than giving the room away.
- **THE OLD SHARED TOP ROW IS STILL RETIRED, AND ITS CELL IS SPENT NOW.** Canvas row 0 carried
  four one-cell voices — the workspace extent, the picker/window hints, the terminal hint —
  each structurally unable to hold a row of a real face. WUX-1 moved those facts into the band
  and left the cell empty; QR-14 spends it, plus one taken from the foot, on a band TWO cells
  tall, which is what a face needs for one row of type. ⚠ One cell would be zero face rows and
  bitmap glyphs — the exact defect WUX-1 retired that row over, and the reason `kTopRows` is
  not 1.
- **⚠ AND WHAT STANDS ON THE RESERVED ROWS IS NOT WHAT RESERVES THEM (WUX-12).** `screen_of`
  subtracts `kTopRows` unconditionally and cannot see a pane; `placement_bounds(kTopBand, ...)`
  turns the same rows into the rectangle the Layouts pane takes when its maker has said
  nothing. Two steps, two questions — how much room the workspace has is a fact about the
  SCREEN, and what is standing in it is the maker's. Move the pane away and the rows are empty
  and `room_h` is the number it was, which is Info's own pattern at the other edge.
- ⚠ **TWO MEASURED FACTS MOVED WITH THE BODY, and neither is a spelling change.** The side
  region begins under the full-width top band, so the Info column is `room_h` cells rather than
  one more (`share_body_rows` spends the smaller budget by its own policy). And the overlay
  stack, anchored to the workspace's top, is one row closer to the Terminal pane, which is
  anchored to the screen's foot: their bounded overlap is TWO rows at the minimum screen where
  it was one, and one row at the two heights above it where it was none.
- `effective_pane_order` is the one FOREGROUND order helper (WUX-5); paint walks it ascending
  and `occupied_at` its exact reverse. Nothing derives hit order from canvas layers, and
  nothing that means "in front right now" reads `presentation_order` -- that is the authored
  base, and it is what persistence and `reset order` want.

## Gestures: press, hold, release (WIND-2, WIND-2a)

- **One press claims one gesture until release.** `PaneGesture` holds an identity, an edge and
  the size at the moment of the press — no rectangle and no live position, `Drag`'s own law —
  so crossing another pane, crossing the Terminal, and reordering mid-drag change nothing about
  who is being moved, and every motion proposes `base + (pointer - press)` rather than
  accumulating. **Arrangement owns the pointer while it is open**, the Terminal's own shape;
  outside it nothing changed, so an addressed pane behind another claims no press and no
  address auto-raises.
- **A TEXT-SELECTION DRAG IS THE THIRD GESTURE RECORD (TEXT-0).** `Session::text_drag` holds
  WHICH editable line a press began sweeping (the Terminal's, or the live property draft) and
  nothing else — the anchor and caret live in the `TextBox` the press placed, and every motion
  re-resolves the CURRENT geometry through the same functions the press spent
  (`terminal_input_place` + `terminal_value_column`, or `info_body_at` +
  `property_value_column`) and hands the component a column (`drag_to_column`). The ROW is
  deliberately not re-tested mid-drag — a hand that wanders off the line keeps sweeping it by
  column, which is what keeps the selection stable — and a drag left of the slice steps one
  character per motion, the component's own leftward auto-scroll. A press begins it only on
  the paths that CONSUME the press (QR-2's mutate-nothing-on-decline rule holds); release
  ends the gesture and keeps the selection, silently — the selection on screen is the
  statement.
- **`end_held_gestures()` is the one release owner**, called by the Terminal branch, the
  arrangement branch and the ordinary path. A gesture begins under one mode and is released
  under another, so whichever mode answers a release first must end them all — ending only one
  kind leaves a gesture alive with the button up, following the pointer afterwards. It says
  nothing; what to tell a maker is the caller's, because the answer genuinely differs. It is
  not a capture framework: three records and one function.
- **`forget_removed_selection()` clears on MEMBERSHIP, never on presentation.** A pane that
  becomes waiting, refused, covered, off-room or unresolved stays addressed — every one is
  a pane the setup still names and still reachable by stepping. A reference LEAVING the
  setup clears the address and the gesture; the ONE-PANE scope closes with its pane (a
  state bound to exactly one pane is a state about nothing once it is gone — silently,
  because the removing operation's own sentence is on the notice line), while the desk
  stays open, its subject being the desk. It runs inside `apply_setup`, the one door
  membership changes through.

## The editable line is a WINDOW onto the command (HD-4)

`component::TextBox` owns `first_visible` beside its text and caret (see the component section
below), and the three move together because the operations are the only way any of them
changes. The capacity is never guessed: it is `terminal_input_place(sc).columns`, the same
number the painter cuts the slice with and the same one a press is answered against.

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
- **The left edge snaps FORWARDS** (`character_boundary_at_or_after`). Snapping backwards is
  the obvious spelling and it is wrong here: it carries the window's right edge back with it
  and pushes the caret one to three columns off the row. The right-hand cut is a **byte** cut,
  as `detail::fit` and `project_text_regions` have always been — snapping it back would shorten
  the row under a caret column computed from the window, which is how a cell medium's caret
  falls off its own row.
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

## Editing text is a COMPONENT, and it belongs to no consumer (HD-5, TEXT-0)

`zengine::component::TextBox` (`component/text_box.hpp`) owns text, a caret, a selection
anchor and a horizontal window as one state, with the operations as the only door. The
Terminal's command line, an Inspector property draft, the setup-name editor and the Composer's
field drafts are four instances of one implementation.

```text
what the component owns      text, caret, anchor, first_visible; the character walk; the four
                             caret-follow rules; the slice; column <-> byte, through the window;
                             the selection grammar (extend/collapse/replace); copy/cut/paste
                             over an owner-held Clipboard; a bounded local undo/redo; the
                             editing-KEY vocabulary: consume(scancode, modifiers, clip) -> bool
what a consumer owns         the capacity (an ARGUMENT), where its prose begins, the Clipboard
                             instance and its custody beyond the process, and what the text
                             MEANS -- submit, parse, validate, commit, refuse, complete.
                             Return, Escape and Tab are policy in every consumer and are never
                             in the component's vocabulary.
```

- **`consume()` is QR-2's bool at the component boundary**: true = the box's own vocabulary,
  stop routing; false = not mine, yours. A consumed gesture need not change anything — a copy
  with nothing selected is consumed, or "copy nothing" would fall through to whatever the
  application binds. Declining is `default:`, not knowledge: the box never learns an
  application chord to refuse it, and a chord carrying Alt or Super is never the box's. The
  owner's handler is `if (box.consume(k.scancode, k.modifiers, session.clipboard)) return;`
  followed by its policy switch — the shape all four consumers now spell instead of four
  copies of one six-case mapping.
- **The history is the DRAFT's and dies with it**: `set`/`clear` — how every consumer opens
  and closes a draft — wipe it, so undo can never resurrect a previous draft's text under a
  new label. Contiguous same-kind keystrokes coalesce; paste, cut and any selection
  replacement are one entry each. There is still no application-wide undo and none may grow
  from this. Since QR-11 the same two doors bump `TextBox::draft_epoch()` — the draft's
  identity made comparable, for the one owner question history cannot answer: is the box
  still holding the draft that asked for something a turn ago?
- **The clipboard is `Session::clipboard`, one for all of Workshop's own boxes** — the
  freshest copy said IN this process: the component writes it on copy/cut, `on(KeyPressed)`
  notices (`writes` compared around the whole chain, once) and publishes `ClipboardCopy`;
  copies from elsewhere land in it without bumping `writes` (mirrors never echo). It is NOT
  a mirror of the system clipboard — nothing watches that — and it is deliberately not
  persisted (WUX-0 keeps the desk, never the work-in-progress).
- **A PASTE IS A CONVERSATION, AND THE ANSWER BELONGS TO THE DRAFT THAT ASKED (QR-11).**
  `consume()`'s Ctrl+V bumps `Clipboard::paste_requests` instead of pasting — the value a
  paste means is the platform clipboard's CURRENT one, and only the owner can obtain it.
  The same one comparison around the chain notices; `paste_owner_now()` (the chain's
  mirror, `editable_text_has_keyboard`'s pattern) names the asking draft; Workshop opens
  the ask in its own `loom::AskBook` (capacity 4, refuse-new — the asker's half of the
  settlement law) and sends `ClipboardTextRequested` to `kSkinRole`
  ([`surface.md`](surface.md#the-medium-owns-the-platform-clipboard-in-both-directions-text-0-repaired-by-qr-11)).
  The answer is settled by `answers_ask()` plus the book, then applied through
  `TextBox::paste`/`Row::paste` ONLY if the initiating draft still stands — same owner,
  same `draft_epoch`, and for a property row the same object and label (a draft carried
  across a rebuild by `Row::resume` keeps its epoch, so an extent change mid-flight does
  not orphan its paste). Anything else discards the payload whole, mirror update included:
  a focus or mode change between request and answer must never redirect clipboard text
  into another box. On a medium that answers `readable=false` (the terminal) the paste
  falls back to the in-process mirror, which is what keeps copy-here-paste-there true
  there with no platform claim. An ask with nobody at the Skin's role stays open,
  bounded by the book — Loom has no unanswerability notice and Workshop does not pretend
  one. The four Session boxes ask through Workshop; the Composer, a provider, holds the
  same conversation itself (its own book, its own draft-generation identity).
- **`zengine-component` links nothing**, not even `loom::core`. A TextBox has no wire form,
  nothing serializes it and nothing hosts it; the absence of that link is the enforcement —
  which is why the vocabulary's key identities are spelled locally in `component::key`/`mod`
  and pinned against `input::scan`/`mod` in the input suite, translate_sdl.hpp's own pattern.
- **The editing vocabulary is DECLARED beside the switch since KEY-0**:
  `component::kEditingVocabulary` rows `{scancode, modifiers, label}` are exactly the
  gestures `consume` answers `true` to, swept against it in both directions by the component
  suite, so a consumer's contextual help can SHOW the vocabulary without re-spelling it. The
  rows carry no context, no command id and no remappability — the component still knows
  nothing of Workshop, and these gestures' executable truth remains `consume` alone.
- **A WORD HAS ONE DEFINITION AND THREE COMPOSITIONS (WUX-7).** `word_run_begin` /
  `word_run_end` are the maximal non-space run read backwards and forwards from a position;
  `word_before`/`word_after` add the separator walk a KEYBOARD gesture means, and
  `word_at(line, at)` is the two scans meeting at one position, which is what a POINTER means.
  A position with a separator on both sides is in no word (`WordSpan::present()` is false) and
  nothing invents the nearest one; a position on either EDGE of a run belongs to that run,
  which falls out of the two scans rather than being a case. `TextBox::select_word_at` is
  `place`'s other answer to one press — anchor at the start, caret at the end, `select_all`'s
  choice — and a position in no word places the caret and selects nothing. **The component
  still knows nothing about clicks:** whether two presses are one gesture is the consumer's,
  and this class gained a span and an operation and no timing at all.
- **The character helpers live with it**, and since TEXT-0 the word helpers too
  (`word_before`/`word_after` — space-delimited runs, deliberately a shell's word and not an
  editor's) and `pasteable_line` (what foreign clipboard bytes become in a one-line box: CRLF
  pairs and control bytes flatten to spaces, tabs included, for the byte=column grid's sake).
  There is deliberately no end-of-line eraser — that shape was deleted in HD-5.
- **`terminal_caret_column`/`terminal_caret_of_column` take the BOX**, not two indices — no
  argument left to forget, no way to pair a caret from one line with a window from another.
  The selection has the same one-measurer family: `TextBox::visible_selection(columns)` is
  the only span arithmetic, and `terminal_selection_columns`/`property_selection_columns`
  add exactly the prose offset the caret helpers add.
- **Do not give the component a focus flag, a filter, a max length, a multiline mode or a
  blink.** The pre-Zen `Zen::TextBox` in `reference/` had most of those and could not move
  its caret. Selection, clipboard and undo arrived in TEXT-0 because with four consumers the
  ordinary expectations had stopped being per-consumer projects; a fifth absence a competent
  user would trip on gets the same test, not a reflex extraction.

## Two presses are one gesture, and time is an argument (WUX-7)

`input::PointerButton` carries no click count and no timestamp on either backend, so a
double-click is Workshop's own interpretation and not a platform's — which is what keeps a TUI
and an SDL Workshop agreeing about a component's grammar.

```text
HostContext::interaction_now   a READING the host may wire (`frontier`'s seam). Empty means
                               `interaction_now_ms()` -- steady_clock, monotonic, no calendar,
                               never persisted, never on a wire. It owns "what time is it now"
                               and no policy at all (workshop/interaction_time.hpp).
Session::click                 what the LAST press on an editable line named: which line
                               (`text_drag_place`), which draft of it (`draft_epoch`), which
                               WORD, and when. No column, no row, no rectangle.
doubles_a_click(...)           pure, total, one place: armed + same line + same draft + same
                               word + within kDoubleClickMs (400).
```

- **ONE SEAM, BOTH EDITABLE LINES.** `press_selects_word` is spent by `terminal_press` and by
  `info_press`, because the Terminal's command line and the Inspector's live draft are two
  instances of one component and a maker's hand must mean the same thing in both. The Editor
  keeps its own multiline machinery (EDIT-0) and is deliberately not taught this; the
  Composer's fields are a provider's, and the pane protocol was not widened.
- **IT ARMS ON THE WAY OUT, ALWAYS.** The record is read only by a LATER press, so the click
  that pointed the keyboard at a pane, opened the draft, or landed in a word for the first time
  is an ordinary press with an arming beside it and can never be counted as its own second half.
  The completing press SPENDS the arming, which is why there is no triple-click.
- **A MODIFIER-BEARING PRESS NEITHER DOUBLES NOR ARMS**, and its ordinary behaviour is
  untouched — this path consumes nothing it did not already own.
- **THE INTERVAL IS A PRODUCT CONSTANT, NOT A PREFERENCE.** `kDoubleClickMs` is not read from
  a file, not asked of a desktop and not persisted. A per-platform answer would make one
  gesture mean two things depending on which medium a maker opened.
- **TIME IS AN ARGUMENT TO THE PREDICATE**, which is what makes every condition falsifiable by
  a case rather than by a stopwatch. `tests/workshop_support.hpp`'s `InteractionClock` is what
  the rigs wire, and its default step is past the interval — so a case's two presses are two
  deliberate aims unless it says `clock.together()`.

## A fitted row may be read past, and only under the pointer (WUX-7)

`fit`/`fit_path` bound a line and mark the cut; this is the other half — the maker POINTS at
the row and the same row shows a different part of the same string for as long as they do.

```text
Revealed (Session::reveal)   place + item + the WHOLE row as the painter held it + a byte
                             offset. Presentation only: no file, no setup, no document, no
                             provider, no value, and nothing durable anywhere.
detail::reveal_shown(...)    the painter's one call. Four things must agree -- surface, item,
                             a non-zero offset, and the STRING -- or the row's own resting
                             answer comes back. The guard IS the reset; there is no clearing
                             path anywhere (`bounds_of`'s discipline, spent on presentation).
reveal_at / reveal_for       the pointer's inverse: `occupied_at` first, then the SAME body
                             resolution, `prose_at`, and the same row->item inverses the
                             press path spends (`files_row_of_body_row`,
                             `object_at_prose_row`, `property_at_prose_row`).
```

- **THE ITEM IS THE IDENTITY, NEVER THE PROSE ROW.** A row moves with the window, the cursor
  and the pane's height; the item does not. A reveal bound to a row would follow the row onto
  whatever scrolled into it, which is exactly the neighbouring-row defect.
- **ELIGIBILITY IS `rest != full`** — what the painter WOULD show against what it is holding.
  A value that fits is perfectly still, and a provider's already-shortened text cannot be
  recovered: the external pane protocol was not widened to ask for a longer one.
- **THE POINTER'S COLUMN IS THE OFFSET, and that is the frame loop's honest answer.** A timed
  marquee needs a repaint with no event behind it, and this application publishes a canvas only
  when it has been told something — no beat reaches Workshop, and asking the Timer service for
  one would be Loom participation for a presentation. So the ROW is its own scrub track: the
  left edge is the value's start byte-for-byte, the right edge is its end, everything between
  is proportional, and moving back reverses it.
- **THE HEAD IS MARKED THE WAY THE TAIL IS**, so a still photograph of a revealed row never
  reads as a complete value, and `revealed_row` clamps the offset itself — "a value that fits
  never moves" is a property of the projection rather than of whoever computed the offset.
- **A MODE OR A HELD GESTURE OWNS THE POINTER AND THIS DOES NOT.** The Terminal, an arrangement
  scope, the contextual surface, a text drag, a document drag and a pane drag each mean the
  motion is theirs; the reveal is empty for all of them.
- **THE FIRST CONSUMER SET IS FOUR ROWS**: the Files pane's location header and its listed
  names, and the Info panel's object rows and RESTING property rows (a live draft is windowed
  against its own caret and is excluded — a pointer scrolling it would be a second window over
  one line). Adding a fifth is one `reveal_shown` call at the painter and one arm in the
  resolver; it is not a registry and must not become one.
- **⚠ THE TERMINAL CANNOT REPORT A HOVER.** `kTuiPointerOn` asks for `1002` — button-event
  tracking, which is press, release and drag — so an idle pointer reaches nobody there. That
  is a medium fact and it is documented as one
  ([`docs/workshop/limitations.md`](../docs/workshop/limitations.md)); do not repair it by
  moving the terminal to `1003` without pricing every idle motion in every session.

## The source editor: one document, session-owned, presented by a pane (EDIT-0)

`workshop/editor.hpp` owns the editor's machinery; `Session::editor` (`EditorState`) owns the
document; the Editor pane (`panel::kEditor`, overlay stack) is only its presentation.

```text
EditorState     path identity, the saved copy (dirty DERIVES by comparing lines), the
                line-ending convention, `doc_epoch` (which document is open, comparable),
                the viewport (first_row / first_col in DISPLAYED columns, follow flag)
EditorBuffer    lines, caret, anchor, preferred column, bounded snapshot undo (depth AND
                byte budget), `revision()` (moves with text/caret/selection and nothing
                else), and `consume(scancode, mods, clip)` -- the component's bool at the
                editor's boundary, its gestures DECLARED in `kEditorVocabulary` and swept
                against `consume` both ways by the suite
the weave       every refusal sentence and every file door: `open_source(path, mail)` (the
                ONE door), `edit_source` (the Builder's `e`: discovery, then that door),
                `save_source`, `discard_source_edits`, the quit guard
```

- **THE FIRST MULTILINE CONSUMER OWNS ITS MULTILINE MACHINERY.** `component::TextBox` is
  untouched and still exactly its four one-line consumers' width; the extraction trigger for
  a shared seam is named in editor.hpp (a second multiline consumer, two simultaneous views,
  or a replaceable backend). A future real Vim replaces `EditorBuffer` as a unit and must
  never need to replace path custody, save authority, or the pane presentation.
- **THE DOCUMENT IS SESSION STATE, AND THAT PLACEMENT IS THE NO-SILENT-LOSS FLOOR.** Hiding,
  moving, covering, re-ordering or REMOVING the pane touches presentation only; the picker's
  "nothing behind it was touched" sentence is true of the Editor because there is nothing in
  the pane to lose. What ordinary acts may not do: a different source is refused over a dirty
  buffer; an orderly quit (all three arrival doors) is refused with the two ways out named;
  the ONE deliberate discard door is `editor.discard` (`ctrl+d` -- a PLAIN chord because the
  POSIX wire cannot say ctrl+shift+letter, safe because `revert_to` keeps the history so
  one undo takes a slip back; a row in kEditor AND
  kCommand so the quit refusal names a reachable gesture). Process death still loses drafts,
  WUX-0's own split, and no crash recovery is claimed.
- **`^s` FOLLOWS THE KEYBOARD, AS TWO DECLARED IDENTITIES** — `document.save` (`kNoEditor`)
  and `editor.save` (`kEditor`); see the activity-class rule under KEY-0 above. `^o` stays
  global; `^c` is copy in the editor (`context_takes_text(kEditor)`), quit where nothing
  takes text — TEXT-0's law reaching the fifth text place unchanged.
- **THE ONE DOOR IS `open_source(path, mail)` AND IT TAKES A PATH (EDIT-1).** Every referrer
  arrives through it and none may bypass it: normalize (`persist::resolved_against` against
  `HostContext::project_dir`) → same-path reveal → dirty refusal → bounded read → `source_in`
  → trial-seat → install + `doc_epoch++` + viewport reset → focus + sentence. Two callers
  today — the Files pane hands it a row's path, and `edit_source` (`builder.edit-source`,
  `e`, command mode; unbound with no Builder panel exactly as `b`) keeps ONLY the Builder half:
  which recipe is chosen, the `cmake_target` refusal in the recipe file's own kind word, and
  the HOST's answer over the completed catalog (`HostContext::recipe_source` — the `frontier`
  seam's shape: a function, spent at the gesture, stored nowhere). A third referrer is a call,
  not a policy. `EditorState` holds NO acquisition provenance: the `recipe` field was
  write-only and went out with the factoring — the editor owns the document it has open, not
  the reason somebody asked for it.
- **⚠ IDENTITY IS A NORMALIZED SPELLING, NOT A FILESYSTEM OBJECT.** `e.path == path` is still
  a string comparison; what changed is that every entrant is normalized first (absolute
  against the project, `lexically_normal`, forward slashes), so `a.cpp` and `./a.cpp` cannot
  become two documents and two referrers cannot disagree about which file the dirty refusal
  is protecting. Nothing canonicalizes: Windows case-folding and hard links remain NAMED
  residuals, and claiming otherwise would need a filesystem question on every open.
- **THE SOURCE-BYTE LAW IS THE MEDIA'S HONEST REACH**: printable ASCII + tab; line breaks
  are structure. One convention per document (LF or CRLF, detected at open, spent on every
  inserted newline); a final newline is a final empty line, so `source_in`/`source_text`
  are exact inverses and round trips are an identity. Mixed endings, bare CR, control
  bytes and non-ASCII refuse WHOLE, naming the line, and the file is never rewritten —
  the ground is byte-columns vs glyph rendering (editor.hpp's header carries it). Typed
  and pasted text meet the same law at the weave's doors; a non-ASCII paste is refused
  rather than flattened.
- **TABS EXPAND AT PRESENTATION ONLY**, at a fixed four-column stop: `visual_col_of` /
  `byte_of_visual_col` / `expanded_slice` are the ONE measurer both the painter and every
  press/drag spend (a column inside a tab's span names the tab byte; its right edge the
  position after). The viewport's `first_col` is displayed columns; `kEditorCaretCols`
  reserves the caret's column of every body row, `kTerminalCaretCols`' rule.
- **THE VIEWPORT RECONCILES ONCE PER REPAINT** (`reconcile_editor_view`, in the
  `refresh_terminal` family): offsets always clamp; the caret is FOLLOWED when a gesture
  asked (`follow_caret` — every edit, navigation, placement) or the body's room changed
  (resize must not strand the caret), and deliberately NOT after the wheel, whose whole
  meaning is looking elsewhere. `on(PointerWheel)` is Workshop's first and only wheel
  consumer: modes keep their ownership, the TOPMOST occupancy must be the Editor, the
  header row is not the body, fractional notches accumulate. Project Files is the SECOND
  (EDIT-1) and it cost one more arm on the same topmost-occupancy answer — still no scroll
  framework and no provider wheel protocol. Its wheel moves the CURSOR, because a list
  derives its window from the cursor (`list_window`) and a second viewport would be a second
  answer to one question; the editor's keeps the caret still because a caret is a place in a
  document rather than a place you are looking.
- **A PASTE ANSWER LANDS WHERE THE MAKER ASKED OR NOWHERE** — QR-11's conversation with a
  stricter settlement: the pending record pins `doc_epoch` AND `buffer.revision()`, so a
  replaced document strands the payload silently (the dead draft's fate) and a document
  that merely MOVED — any edit or caret change between request and answer — gets a
  sentence (`paste again`) instead of a relocated paste.
- **THE PANE PAINTS ONE REGION** (`paint_editor`): a header row — dirty word FIRST, then
  `L:C/N`, then the path, ordered by what must survive `detail::fit`'s tail cut, with the
  `> ` keyboard mark, `external_header`'s convention — and the document through the
  viewport via `expanded_slice`, the caret and selection carried as the REGION's own so
  each medium answers in its voice. The body resolution IS `external_body_place` with
  `kEditorHeaderRows`; a second arithmetic for the same shape is the two-measurers defect.

## Where source comes from: the project, and the browser over the machine (EDIT-1, PROJ-2)

`HostContext::project_dir` is the launch directory, captured ONCE by the host
(`launch_project_dir()`; empty = the designed absence, said on the banner — and there are TWO
ways to reach it, a platform that will not report a working directory and one that reports a
directory this application cannot carry, joined deliberately because a maker meets one fact). It is
not `dir` — that is where the BINARY is, installation truth — and nothing derives one from
the other or from `--document`, `--recipes`, a workspace or a prefix. There is no
`--project`: one install serves two projects by being launched in two places, which is the
law `user_paths.hpp` already wrote down and which had, until now, no value behind it.

- **⚠⚠ A RELATIVE AUTHORED `single_source` MEANT TWO FILES, and the repair is ONE completion.**
  Editor and the runner's exists-preflight resolved the authored spelling against the PROCESS
  CWD while the generated project embedded it verbatim for CMake to resolve against the
  WORKSPACE. `recipe_persist::complete_recipes(recipes, host_dir, project_dir)` is the one
  place any host fact enters a recipe — `artifact_dir` and `workspace` from the install,
  `source` resolved against the PROJECT — and `main` calls it once, so "the file you edit is
  the file the build compiles" is structural rather than three parties agreeing. The FILE is
  never rewritten. **Falsifier that must stay green:** `workshop_files`' two-base case — the
  project and the workspace both holding `src/example.cpp` with different bytes; a green build
  that never arranged that proves nothing.
- **THE COMPLETED CATALOG HAS ONE SESSION OWNER, AND EVERY CONSUMER READS IT (PROJ-0).**
  `workshop::CurrentRecipes` (`workshop/recipes.hpp`) holds the output of that one completion —
  the recipes, the tool's reduced `RecipeView`s derived beside them from the same rows, and
  since PROJ-1 the authored FILE they came from, so none of the three can drift. `main` declares
  one ABOVE the HostContext, the bus, the Kernel and every weave, and that declaration order IS
  the lifetime proof; `hold()` assigns into members it already owns, so a replacement changes
  CONTENTS and never the objects consumers bound to. The four consumers are reads:
  `BuildRunnerWeave` and `BuilderWeave` take `const&` (and refuse an rvalue outright — a
  temporary catalog is a dangling one), `HostContext::recipe_source` captures the owner and asks
  it at the gesture, and the `AwaitingBuild` predicate asks it per row. ⚠ **The owner is not
  authorship**: there is no non-const reach and the recipe FILE stays authored truth — nothing
  completes host paths back into it.
- **THE SOURCE PATH IS A PARAMETER OF `hold()`, NOT A SETTER (PROJ-1).** Which recipes are in
  force and which file authored them are installed by one call, so "the path moved and the rows
  did not" has no spelling in this program. `install_recipes(owner, path, host_dir, project_dir,
  artifact_file)` beside it is the ONE seam that turns a file into that answer — read → parse →
  complete → hold, every step on a CANDIDATE in its own frame, so a refusal at any stage leaves
  all three answers exactly as they were. **The launch has no private path to the owner**: `main`
  wires `HostContext::use_recipes` over that function and then installs its OWN startup catalog
  through it, which is what keeps `--recipes` from becoming a second completion policy. A valid
  EMPTY catalog installs (a project with nothing to build is a project) and is not a failure;
  completion is TOTAL and adds no third refusal kind, so the distinctions a maker is offered are
  exactly `persist::read_file`'s and `recipe_persist::from_text`'s and no invented third.
- **THE FIRST LIVE CHOOSER IS `files.use-recipes` (`u`, `kFiles`).** The browser resolves a ROW
  to a path exactly as activation does, refuses a directory and a name its path custody cannot
  carry, and hands the path to `use_recipes`; every judgement about the BYTES is the recipe
  owner's, in its own words — no extension test, no filename convention, no sniffing.
  Same-path is a RELOAD and never a no-op (the durable file may have changed, and this is the
  application's whole live-refresh mechanism — no watcher, no timer, no poll). A dirty Editor
  buffer over the same path is NOT consumed and is NOT auto-saved: the durable file is the
  input. It needs no Builder panel; a successful swap republishes through the `StatusRequested`
  an opening panel already sends, and Workshop's own catalog handler is what ignores it when
  there is no panel.
  ⚠ **SINCE PROJ-2 THE CATALOG MAY LIVE ANYWHERE THE BROWSER CAN REACH, and completion did
  not move with it.** A foreign catalog's RELATIVE `single_source` still names a file under the
  ACTIVE PROJECT, because the host's closure passes `host.project_dir` regardless of where the
  bytes came from. Surprising the first time, correct, and pinned twice: at `install_recipes`,
  and at the live gesture from outside the project.
- **STANDING BUILDER INTENT SURVIVES BY RECIPE IDENTITY, NEVER BY ROW POSITION (PROJ-1).**
  `on(RecipeCatalog)` remembers the chosen recipe's NAME across the arrival: same identity →
  follow it to its new row with `picked` intact; identity gone → home, and `picked` released.
  There is deliberately no fallback to the old index, the artifact stem, a nearest row or a
  similar name. A replacement may INVALIDATE a choice and may not REINTERPRET one.
- **AN IN-FLIGHT BUILD IS THE OPERATION'S, NOT THE CATALOG'S.** `BuilderWeave` resolves the
  artifact FILE when the ask is accepted (`path_`, beside `before_`) and judges the ending
  against that, so a catalog replaced mid-build cannot re-aim, relabel or falsely fail an
  operation already running. The runner's `Held` was already owned facts. Nothing cancels or
  restarts a build because standing recipe truth changed; the NEXT ask reads the new catalog.
- **`Session::recipes_moved_to` IS A PROJECTION AND NOT AN OWNER.** Empty until a maker
  replaces a catalog; the Builder panel spends it on a row that exists exactly while the fact
  has moved — the `project` row's rule, because that panel seats nine facts in nine rows of a
  character medium and an unconditional tenth would spend the third `said` row of every session.
  It is on the `Session` and not on `BuilderPane` because `close_panel` forgets that pane whole.
  ⚠ **IT HOLDS THE OWNER'S OWN ABSOLUTE PATH, READ BACK RATHER THAN RECOMPOSED (PROJ-2).** It
  used to be the project-RELATIVE spelling, which was unambiguous only while the browser could
  not leave the project — and PROJ-2 removed exactly that premise, so a based spelling with no
  stated base became a wrong-looking name for the right file. The value is `RecipeSwap::path`,
  which is what the catalog owner is holding AFTER the attempt, so the screen and the owner
  cannot come to name two different files.
- **⚠ A PATH IS NOT A SENTENCE, AND `detail::fit` IS WRONG FOR ONE (PROJ-2).** A sentence
  front-loads its meaning, so cutting the tail and marking the cut keeps the useful half; a path
  BACK-loads it, so the same cut removes the filename. `detail::fit_path` is the measurer for
  the two consumers that meet this — the browser's location header and the Builder's catalog
  row — and its property is: enough ROOT CUE to say which filesystem, a mark where the middle
  was removed, and the tail cut at a component boundary. `path_root_cue` is purely lexical
  (`/`, `C:/`, `//server/`) because this runs at every repaint and the "proper" way to ask would
  involve exactly the accessors measured to throw. It changes no stored identity, and no pane
  widens to avoid a cut.
- **`workshop/files.hpp` is the browser's machinery; `Panels::files` is its state.** Rows are
  `{name, kind, linked, openable}` and NOTHING else — no resolved path, no recipe, no artifact,
  no build or editor state — so the browser cannot become a second owner of source truth. What
  a row denotes is derived at ACTIVATION from the current LOCATION plus the row's name, and
  `files.use-recipes` (PROJ-1) derives it the same way for the same reason.
- **⚠⚠ FOUR FACTS THAT COINCIDE AT LAUNCH AND ARE NOT THE SAME FACT (PROJ-2).** The single
  hardest thing to keep straight in this area, and the phase that separated them exists because
  the browser's location used to be *spelled in terms of* the project:

  ```text
  HostContext::project_dir   what a project-relative source spelling MEANS. One writer, in
                             `main`. Browsing, marking, jumping and choosing a foreign recipe
                             catalog all leave it exactly where it was — structurally, because
                             no expression in this application derives it from any of them.
  Session::marks.origin      where THIS RUN's navigation began. Generated once, never
                             persisted, never moved by browsing, and deliberately NOT renamed
                             "the project" merely because the two coincide today.
  FilesPane::current_dir     where somebody is looking. ONE absolute, lexically-normal,
                             generic-slash string; seeded from origin and owned here after.
  the operating system       what may actually be read. Never modelled, never claimed; a
                             directory this process may not read is an ordinary refusal.
  ```

- **PARENT IS LEXICAL AND STOPS AT THE FILESYSTEM, NOT AT THE PROJECT (PROJ-2).**
  `parent_path()` until the MEASURED fixed point `p.parent_path() == p`, which is what POSIX
  `/`, a Windows drive root and `//server/` all answer. ⚠ **`has_parent_path()` is TRUE at all
  three and is not a root test** — a boundary built on it never fires. Nothing canonicalizes,
  ever: going up from a linked directory returns the maker to where they walked IN from, and
  `weakly_canonical` would silently relocate them to a place they never navigated to.
- **A LINKED DIRECTORY IS MARKED AND ENTERABLE (PROJ-2 retired EDIT-1's refusal).** That refusal
  existed to keep the entered-name stack honest; there is no stack, so no property survived it,
  and the measured cost of keeping it anyway was six of the twenty-three directories at POSIX
  `/`. The MARK stays: a directory row is `linked` when following it says directory and NOT
  following it does not (`entry.symlink_status()`), which is standard C++ and catches whatever
  this platform reports a reparse point as.
  ⚠⚠ **Do not "simplify" that to `is_symlink()`.** MEASURED on Windows/MSVC (EDIT-1): a
  directory JUNCTION answers `is_symlink() == false` while `symlink_status().type()` is a
  platform extension that is not `directory` — so the disagreement test catches it and an
  `is_symlink` test would have walked straight past it. Probe and table:
  `Zen/reportbacks/EDIT-1-evidence.md`.
- **A LOCATION MARK IS A DESTINATION AND NOTHING ELSE (PROJ-2, `workshop/marks.hpp`).**
  `Session::marks` is the one owner, beside `panels` and deliberately OUTSIDE `FilesPane` —
  Files is the first CONSUMER, not the semantic owner, and a fact inside a pane is a fact
  `close_panel` can destroy. Three provenances as FLAGS rather than a kind, because one place is
  often known two ways at once: generated `origin`, durable `maker` marks, and host-reported
  `root`s. A mark confers no authority, no membership, no trust and no recipe base — nothing
  in the owner's surface could carry one if somebody tried.
- **THE TRAVERSAL SET IS BUILT AT THE GESTURE AND HELD NOWHERE.** Origin, then the maker's own
  marks (sorted bytewise — the browser's own order rule), then `host_filesystem_roots()` asked
  FRESH. One address is one stop however many provenances it wears. There is deliberately no
  standing "selected mark": the cycle is found from where the browser IS, so nothing can drift
  out of agreement with the location on screen.
- **`workshop/filesystem_roots.hpp` IS THE ONLY PLACE THIS REPOSITORY ASKS AN OS FOR ROOTS**, and
  it is `surface/terminal_size.hpp`'s shape one package over. Windows uses `GetLogicalDrives()`
  — one call, no string conversion, so no new narrow custody becomes load-bearing — and the
  drive TYPE is deliberately not asked: a drive with no media is reported, is not listable, and
  refuses in the filesystem's own words. ⚠ They are HOST-REPORTED roots and never "every
  reachable path": a UNC share is reachable by spelling and is in no drive list.
  ⚠⚠ **`files_has_keyboard` MAY NOT ASK FOR THEM.** It answers at every keystroke and every
  paint, so its "is there anything to do here" test is `listing.known || !current_dir.empty() ||
  marks.somewhere_to_go()` — all in memory. Asking an OS which drives exist at that rate is
  this file's own per-paint-population mistake in a smaller place. The residual (a run with no
  origin AND no marks declines the keyboard) is NAMED in `docs/workshop/limitations.md` rather
  than solved.
- **MAKER MARKS ARE DURABLE AND RIDE THE MACHINE-LOCAL ROOT (`workshop/marks_persist.hpp`).** An
  eighth durable artifact and its own file, for three reasons worth not re-deriving: the prefs
  header says in its own words that non-presentation facts belong somewhere with their own name;
  the prefs format has ONE version and no migration, so growing a field there would refuse every
  existing prefs file BY NUMBER and cost makers a preference they had stated; and a mark is an
  ABSOLUTE PATH, so it describes THIS machine's disks — the same criterion that already puts
  the viewport and the desktop placement under the state root rather than the config root.
  ⚠ **The FILE's claims refuse it whole; a ROW is refused alone and SAID.** Format word, version
  and shape are the family's law. A row that is not an absolute location this build can carry is
  skipped, and the skip is a standing CONDITION rather than a notice — because the next mark a
  maker makes writes the list without it. "Unusable" is a SPELLING test and never an existence
  one: a marked directory that is not there today is KEPT, and nothing here asks the filesystem
  anything at all.
  ⚠⚠ **`marks_refused_` is the prefs file's own load-bearing flag, not decoration.** This is a
  file Workshop WRITES, so restraint is not enough: without it, the first `m` a maker pressed
  would replace bytes this run could not read with an empty list.
- **⚠ A DURABLE SPELLING COMING BACK IN IS A CONVERSION TOO (`admit_location`, PROJ-2).** QR-12
  measured `path -> string` throwing on MSVC; `string -> path` refuses on the same platform for
  the same reason, and a hand-edited durable file is the one place bytes this build cannot widen
  can arrive from. Every write to `current_dir` and every persisted mark goes through that one
  door, so "absolute, lexically normal, carriable" holds after the seed, after an enter, after a
  parent and after a jump rather than at four sites that each have to remember.
- **⚠ FILENAMES ARE `std::string` EVERYWHERE, so admission is a PATH law and not a content law.**
  Names are taken as `u8string()` bytes; printable-ASCII names are exact and openable, and any
  other keeps its row, shows a `?`-marked projection (never an identity), and refuses
  ACTIVATION. What may be INSIDE a file stays the editor's question, answered at the door in
  the editor's words — no file-type registry, no extension list, and a `.png` is allowed to
  walk into the refusal that actually knows why.
- **⚠⚠ ASKING FOR A PATH'S BYTES IS ITSELF A CONVERSION THAT CAN THROW, AND `workshop/path_admission.hpp`
  IS THE ONLY PLACE ALLOWED TO ASK.** MEASURED on MSVC (ACP 1252): `string()`, `generic_string()`
  AND `u8string()` raise `filesystem_error` — `u8string()` for an ill-formed-UTF-16 filename
  (`CreateFileW` accepts an unpaired surrogate, so any program can leave one in a directory),
  `generic_string()` for a working directory outside the code page. Both took the process down,
  one out of the enumeration walk and one out of `main`. `admit_path` answers with a VALUE
  (`carried` + the spelling), `admit_filename` always answers with a name and an `exact` flag,
  and `launch_project_dir()` is the capture `main` runs so what a case proves is what ships.
  ⚠⚠ **`exact` is not redundant beside the byte test**: a refused name's `name` is a `?`
  PROJECTION, which is entirely printable ASCII, so `printable_ascii_name` alone would call it
  openable and hand a door a path naming a different file or no file. `openable`, never the
  bytes, is what `files_open` and `files_use_recipes` ask. Anything that later names a
  filesystem location a maker did not type asks this header rather than growing a
  `generic_string()` of its own — a second one is a second way for the process to die. Since
  PROJ-2 the header owns the OTHER DIRECTION too (`admit_location`), because a spelling read
  back out of a durable file is the same conversion aimed the other way.
- **A LISTING IS NOT A PER-PAINT POPULATION.** Every other population Workshop paints is in
  memory; this one is an OS walk. It is recomputed at open (the `Reconciled::opened` arm the
  Builder's `StatusRequested` already uses), enter, parent, `files.refresh`, and on a FINISHED
  build — gated on `build_news`, the fact this weave already derives, not on a status ARRIVING
  (the tool republishes its whole picture on every transition and on every panel open). No
  watcher, no timer, no poll, no protocol widened. External staleness between those moments is
  named in `docs/workshop/limitations.md`, not solved.
- **Bounds and order:** `kMaxListedEntries` stops the walk and the header says `stopped
  counting` — QR-4, never a total the walk never reached. Directories first, then files,
  bytewise over the admitted name bytes inside each class; no locale, no natural sort, no
  extension grouping, no configuration.
- **⚠ THE PICKER'S NAME COLUMN BOUNDS THIS BUILD'S OWN NAMES TOO.** `Files` is one word because
  `kPickerNameCols` is ten: widening it to hold a two-word name narrowed every SUMMARY by the
  same three cells and began cutting INTR-0's own measured sentence at 71 columns. A provider's
  sentence being readable is a product fact; a built-in's display name is a choice. Measured,
  reverted, recorded — do not re-widen it for a name.

## The Info panel BODY, resolved once (HD-5..HD-7)

`info_body_place(panel_bounds, screen, document, session)` (`workshop/screen.hpp`) is the whole
Info panel body — where it is, how many rows of the ACTIVE medium's type fit in it, how those
rows are shared between the OBJECTS list and the property list, how wide a value may be, and
which members each window is showing. The painter, the caret, `refresh_inspector`, both
vertical windows, `info_press` and `objects_press` all call it.

- **The body is ONE region and a property row is one of its rows**, mark and name included. A
  property row could not simply be given two cells of its own region: a two-cell region covers
  the property beneath it. Taking the room once, for all the rows together, is what lets
  `fit_region` answer the whole question in one equation.
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
  already reads `3-5 of 9`. Its `rows < 3` branch is reachable: a short panel gives a list a
  share of one or two rows, and what a maker then reads is `... 20 more` where the names would
  be — this place cannot show you an object AND tell you what it is hiding, so it tells you.
- **`inspector_focus` is the row that must stay visible: the editing row, else the cursor.**
  They are the same row today by a reachability argument, and the function exists anyway —
  orderings that rest on reachability proofs are one refactor from being silently wrong.
- **`prose_row_of_property` and `property_at_prose_row` are inverses and there is no third
  copy.** The painter positions the caret with the first and a press resolves with the second,
  so a scrolled body cannot land a click one row off the caret. A press is never rounded to a
  Workshop cell: an 18-pixel row against a 12-pixel cell would name the wrong property for
  most of the body. `prose_row_in_window`/`item_at_prose_row` are the one copy of the row
  arithmetic, called twice — helpers, deliberately not a component, and deliberately not
  called `List`.
- **A resting value is FITTED and a live draft is WINDOWED**, and the difference is the point:
  `detail::fit` marks what it cut (`the-quick-brow...`) because a committed value has no caret
  to tell a maker it moved; `TextBox::visible` does not, because a draft has one.
- **`refresh_inspector` reconciles the draft's window once per repaint**, beside
  `refresh_terminal` and for the same reason. At most one row is ever editing — `begin_edit` is
  reachable only from command mode, which is exactly the state in which no row is being edited.
- **A `SurfaceExtent` must not drop a live draft.** `refocus_keeping_draft` rebuilds the rows
  (the resolved row closes over the extent) and hands the draft, its refusal and the cursor
  back. Every *other* `rebuild_rows` caller follows a change of selection or of document, where
  dropping it is right — `Name` is a row every object has, so a draft carried across a
  selection would arrive on a different object's property wearing the same label.
- **The Info panel publishes a region on every paint**, so a test that wants the Terminal's
  pane must ask for it by its PLACE (`Screen::terminal_x`/`terminal_y`) and not as `texts[0]`.
- **`share_body_rows` is the whole composition policy**, and it is max-min fair sharing:
  *each list is given the rows its own population needs; what neither needs stays spare; and
  what they cannot both have is shared equally, with any part of a half a list does not need
  going to the other.* Four things follow and each is pinned as a property over every budget
  from 0 to 200: a list that fits gets exactly what it needs, spare room stays spare, growing
  the panel never shrinks either list, and **the 50/50 case is a consequence rather than a
  decision**.
- **One region rather than two, and the reason is arithmetic.** Splitting the panel's CELLS
  between two regions needs to know how many cells a run of rows costs, which is `fit_region`
  read backwards — a second arithmetic beside the one function that turns a metric into a
  capacity. One region asks `fit_region` once, gets a budget in PROSE ROWS, and spends it. The
  two sections then cannot overlap: they are disjoint runs of one budget.
- **`OBJECTS` is the panel region's first prose row since WUX-1** — reserved out of the PROSE
  budget by `info_body_place` before either list is offered anything (`kInfoHeadingRows`,
  `external_body_place`'s own ordering), so the region is the WHOLE panel and the heading is
  set in whatever type the active medium owns. Until the shared top row was retired it was an
  ordinary label kept out of the region, because the panel's cell row 0 also carried the
  screen's terminal hint and a region owns its interior; nothing shares the rectangle now.
  Body rows still begin at zero — `info_body_at` subtracts the heading on the way back, so a
  press on the heading names no body row and falls to the panel's occupancy answer.
  `PROPERTIES` is a row OF the body, a section whose position moves.
- **An object row is fitted WHOLE** (`object_row_text`). There is no fixed column after the
  name to protect: cutting the row at the body's width cuts exactly the name and leaves the
  mark and the identity intact by construction. The identity comes before the name because a
  name is not an identity here — every object `n` makes is called `panel`.
- **A press on a visible object row selects it, in command mode only** (`objects_press`). The
  mode law: *while a property draft is live, a press on the object list changes no selection
  and says so.* Changing objects rebuilds the inspector rows, which is what a live draft cannot
  survive. The mirror question (a press does not BEGIN an edit) was refused for the same
  reason.

## The Info panel's third run is a FOOTER of controls, not a third list (HD-8)

The body's row budget carries three runs, and the last of them is two rows a maker can press:
`[ Create ]` and `[ Delete ]`, the pointer's way to the acts `n` and `d` perform.

```text
row 0 .. objects_rows-1        the OBJECTS list, its markers included
row objects_rows               `PROPERTIES` -- a heading that MOVES with the composition
the next properties_rows       the property list, its markers included
... spare, and allowed to stay spare ...
capacity-kActionRows .. end    the controls -- anchored to the FOOT
```

- **The reservation is ONE subtraction and it happens before either list is offered anything**
  (`info_body_place`: `share_body_rows(capacity - 1 - kActionRows, ...)`). Controls are a
  FIXED demand and the lists are VARIABLE ones, so they are not a third claimant on
  `share_body_rows`. There is no `-2 for buttons` anywhere else;
  `InfoBodyPlace::action_row` is where the reserved rows are, and the painter and the press
  both ask for that number.
- **The footer is anchored to the FOOT, so the spare room falls between the properties and the
  controls.** Anchoring it to the row the property list happened to stop at puts the target
  somewhere new every time a maker selects an object with a different property count. A
  control that moves under the hand aiming at it is worse than an empty strip above it.
- **The body publishes exactly `capacity` rows**, with the spare ones written as blank rows,
  because a region's rows are positional and the controls are at the end. The cost is measured
  (a whole `paint` of a 74-row TUI body is 4.6 µs); what it buys is that a second region — and
  `fit_region` read backwards — is not needed.
- **Availability is TWO REASONS, ONE BIT and TWO OWNERS:**

```text
kDraftLive   the APPLICATION owns it -- `actions_press` refuses BEFORE the operation, because
             `create_object`/`delete_object` know nothing about a live draft and would rebuild
             the inspector's rows out from under one. The sentence is `kFinishDraftFirst`.
kNoTarget    the DOCUMENT owns it -- the press goes THROUGH. `doc::remove` already refuses
             `no such object`, changes nothing and says so; holding it back here would be a
             second copy of a refusal that exists and a second sentence for one state.
```

  **A control never invents a reason; it defers to whoever owns the refusal, and holds a press
  back only when nobody downstream would.** That is why this is not a `disabled` flag — a flag
  collapses a fact the application must act on and a fact the document must speak for. **And
  availability is not a prediction of every refusal**: an object something else measures
  against cannot be deleted (`doc::remove`'s dependents policy) and a document can arrive from
  a file with its mint spent; answering those in the presentation would put a copy of the
  document's policy on the paint path, re-run every frame. Availability is whether the act has
  a TARGET and whether the maker is FREE to act.
- **Unavailable is said in CHARACTERS**: `[ Create ]` is pressable and `( Delete )` is not,
  the same width either way. The muted role is the second signal and never the only one — a
  terminal has no ground to tint. `[ ... ]` is this tool's word for a pressable thing.
- **The controls do not own the acts.** `actions_press` calls `create_object()` and
  `delete_object()` — the operations `command()` binds `n` and `d` to — so the two gestures
  converge on one document write, one selection rule and one sentence. There is no callback,
  no command id, no action registry and no `std::function`: the whole thing is a switch over
  two indices of a table. `prose_row_of_action` / `action_at_prose_row` are inverses and there
  is no third copy.
- **`component::Button` was NOT extracted, and that is a standing decision.** What Create and
  Delete share is a label, a bit, a bracket convention and a row — presentation with no
  invariant to keep. A component here is earned by repeated working behaviour with an
  invariant to keep, never by a widget list (the TextBox standard). Do not add `Button` until
  something owns a rule. **No focus framework either**: the controls are pointer-only,
  keyboard command routing is untouched, and there is no keyboard-activation gesture for a
  control, so there is nothing for two owners to want.

## The Info panel's structural rows sit on a GROUND (HD-9)

```text
`PROPERTIES`              role kAccent   background kMuted    a section BOUNDARY
[ Create ] / [ Delete ]   role kFill     background kMuted    a control that can be pressed
( Create ) / ( Delete )   role kMuted    background kNone     present, and not pressable now
every other row           role as before background kNone     whatever the region sits on
```

- The capability is `SurfaceTextRow::background`, unchanged
  ([`surface.md`](surface.md#a-row-may-sit-on-something-and-it-is-one-field-hd-2)); what
  Workshop spends is vocabulary that was already there.
- **The two consumers use ONE ground, and that is agreement rather than sharing.** What tells
  the heading from the controls is what each already carried: accent ink and a section's name
  against fill ink and a bracketed verb. There is no `kSectionGround` constant, because two
  decisions that land on one value are not one decision.
- **An unavailable control loses the ground entirely rather than getting a quieter one.** That
  is what makes the ground mean *actionable* instead of *a control is here*, and it keeps
  availability out of being a matter of degree. The characters are still the first signal;
  `say_row`'s ground is the third, after the brackets and the role, never a replacement for
  either.
- **The ground is presentation and it moved no geometry.** The grounded strip a maker sees for
  prose row `i` is `[origin_y + i*line_px, origin_y + (i+1)*line_px)`, the identical partition
  `prose_row_of_pixel` inverts — pinned, boundary pixels included. Horizontally the slab is the
  region's viewport and the target is `0 <= column <= fit.columns` inside it, so a
  `kTextInsetPx` margin of the slab names no control.

## Semantic text in Workshop panels (TYPE-1, QR-3)

- **`panel_prose_place(b, sc)` + `panel_prose_region(b)` is the one call for a panel whose
  whole rectangle is its own** (the picker, the contextual popup, an external pane). It
  returns the prose rows and columns the ACTIVE medium fits, and the painter spends them. Its
  cell projection is byte-for-byte what `paint_panel_row` wrote.
- **The Builder is a region composed by explicit priority since WUX-1** (`paint_panel_row`,
  the cell-lattice row spelling, is gone with its last consumer). Each fact carries a
  distinct priority; the budget keeps the most important and the DISPLAY order never changes
  — header, recipe, `project` frontier while one is waiting (BLD-2's row, which under a
  constrained budget outlives everything but the header and the live `last` row), last,
  exit, ran, realize, `said…`. A character medium's nine-row budget selects every fact,
  byte-for-byte the panel BLD-2 shipped; the shipped face's five keep header/recipe/last/
  realize/said (frontier displacing said while waiting); the `said` block is wrapped into
  exactly the rows that survived, so its elision mark tells the truth about THIS budget.
  Dropped facts are dropped WHOLE — nothing substitutes, nothing reorders.
- **The band is ONE budget-composed region since WUX-1** (`band_bounds`/`band_fit`/
  `band_region`, screen.hpp): budget ≥5 (a character medium) reads the status row (the
  layout tabs and the setup's status — `band_status`, WUX-9), the
  notice, the `workspace WxH cells` fact and two legend rows — the five-cell composition
  every golden pinned, with the old spare row spent on the workspace fact; the shipped
  face's 3 reads status (workspace fact folded in after the hints), notice, one packed
  legend row; 2 drops the legend (the hotkey view remains the full claim surface); 1 keeps
  the notice while there is one, the identity line otherwise, and the name editor's row
  outranks both while a maker is mid-name. The name editor's caret and selection are the
  REGION's now — a bar and a band in a medium with a face, the same inserted glyph and
  reverse video as ever in the cell projection — and `setup_name_columns` measures against
  the band's own fit (one measurer). The layout is the budget's ALONE: the legend preference
  changes what the legend rows say, never where any other fact sits.
- **The workspace object's name is a `kGroundBeneath` region over the object's own
  rectangle.** The two things it is NOT were built and run live to measure them: an ordinary
  region over the object's rect turns every object into an empty dark box; rows carrying the
  object's role as a GROUND leave a pixel band the strips cannot reach AND replace
  `glyph_for_role`'s `#` with a background colour, which is the exact thing that constant
  exists to refuse.
- **A name is bounded by the material it names (QR-3).** Its bounds are
  `min(object width, workspace right edge - x)` by `the object's own height`, each floored at
  one cell — so the name is bounded by the object and `fit_region` is what makes a one-cell
  object fall back to cells with no `if (h < N)` written anywhere. Giving the name the room to
  the workspace's right edge instead is invisible in a medium that paints roles as ink: the
  name is `kMuted`, legible on the object's `kFill` body, and the workspace backdrop is ALSO
  `kMuted` — measured, a 32-byte name showed 9 legible characters and 23 in the backdrop's
  exact colour. No role fixes that, and a fifth role is refused, so the answer is the BOUND: a
  name that does not fit its object says so with `detail::fit`'s mark rather than fading into
  the backdrop. The authored name is untouched; widening the object reveals more of the same
  bytes. The bound belongs to the material being shown, not to the name.

## A press-chain bool means CONSUMED (QR-2)

The handlers under `if (b.pressed)` in `WorkshopWeave::on(const input::PointerButton&)` answer
exactly one question, and it is a routing question:

```text
true   this layer CONSUMED the press -- stop routing
false  this layer did NOT consume it -- carry on to the next
```

Nothing else. Not acceptance, not success, not "something changed", and not target identity.
**A consumed press does not have to change anything; it only has to have reached the layer that
owns what the press means.** An answer of *the caret MOVED* agrees with that contract for
exactly as long as every press landing on a live draft also moves it — the measured failure was
a maker pressing where the caret already was, falling through the whole chain, and getting
`Info is here -- nothing under it can be taken hold of` written over the notice they were
reading. The failure was a NAME, not a missing type: a three-valued disposition would have
hidden it under a plausible shape, and the two-cases-per-word test (INT-R0) earns exactly two
words.

- **A deliberate `false` is a decision and reads as one.** `objects_press` declines a press on
  the row of the object that is ALREADY selected, so the panel answers it as it answers every
  other press on that rectangle. Both dispositions are pinned by a case of their own.
- **`terminal_press` is NOT part of this contract and must not be unified with it.** Its bool
  is *is a repaint owed*; consumption inside the overlay was already decided one layer up by
  the MODE, and a `false` there means "consumed, and nothing moved" — the opposite of a `false`
  in the chain. The caller names the result `repaint_needed` at the one place both kinds of
  bool are in view. Two questions with two answers each are not one question.
- **`info_body_at` (`workshop/screen.hpp`) is the resolve-and-locate preamble, owned once.**
  It answers **where** and nothing about what that means: no routing priority, no
  property/action/object semantics, no refusal and no consumption policy — each handler still
  asks its own inverse (`property_row_hit`, `action_press_at`, `object_press_at`) of the place
  it returns. `present` is the conjunction *panel open ∧ body resolved ∧ position understood*.
  ⚠ It does NOT test whether the point is Info's to answer — occupancy does, one line up, and
  since WUX-12 it is asked first.
- **`band_tab_at` is the Layouts pane's local inverse and is asked the same way.** Its spans
  come from `band_status`' own composition, so a press cannot be answered against a tab this
  budget did not paint; and it is spent only once `occupied_at` has named `panel::kLayouts`,
  so a pane authored in front of the run takes the press. The inverse may stay specialised;
  the coordinate exception may not come back.
- **The body is resolved ONCE PER PRESS, in the route, beside the canvas point.** Holding it
  across the chain is sound for a stated reason and not an assumed one: **every handler changes
  nothing on the paths where it declines**, so a "not mine" cannot have moved the picture the
  next handler is about to ask about. Keep that true when adding another.
- No `Disposition`, no `InteractionResult`, no `Handled/Refused/Ignored`, no target enum and no
  interaction package. The richer answers already exist where richer answers matter
  (`Written`, `Handled`, `Commit`, `Availability`, `Occupancy`) and they are all on SEMANTIC
  paths; the bare bool survives only on the routing path, which is the one place it is
  adequate.
- **Pointer order (WUX-12):** the terminal overlay (a MODE), then arrangement (a mode), then
  the open contextual surface (a mode, CTX-0), then **ordinary pane occupancy**, then the
  resolved pane's OWN local inverse, then the workspace.

```text
transient/mode first refusal
        v
ordinary pane occupancy / front order      occupied_at, over effective_pane_order
        v
the resolved pane's own local inverse      external | editor | files | info's three | layouts
        v
workspace / background handling            take_hold
```

- **⭐ NOTHING ASKS A GEOMETRY QUESTION ABOVE OCCUPANCY ANY MORE.** WUX-12 removed the five
  that did: the two top-band arms (the left press's tab/create, and the right press's
  tab-as-subject) and Info's three internal arms (`info_press`, `actions_press`,
  `objects_press`), which ran BEFORE the walk and never consulted the effective order — so a
  pane authored over the side column and ranked in front of Info still lost presses on Info's
  control cells to Info. The inverses themselves are unchanged, including their order and
  their disjointness argument; what changed is WHERE they are asked. **A new pane-internal
  gesture belongs in the resolved-owner arm, never above the walk.**
- **A SECONDARY PRESS IS STATE-LOCAL FIRST REFUSAL (ARR-0).** The active interaction that
  can truthfully interpret a secondary press receives it; only an unclaimed one reaches
  the ordinary contextual opener. Concretely: ordinary Workshop opens/re-targets the
  contextual surface on a right press; an arrangement scope — either scope, the reset
  prompt included — LEAVES on one, consumed whole; the open contextual surface keeps its
  own established meaning (re-targeting); the Terminal still means nothing by it. **One
  consumed gesture performs one interaction transition**: the press that closes a state
  never also operates the state it revealed — no context menu opens from an
  arrangement-leaving press, and its release is dropped on the ordinary path as every
  non-primary release always was. This is each state's own local reading, NOT a global
  Back: there is no `right_click_back` action, no keymap row, and a future state that
  genuinely uses secondary press for something else is free to claim it.

## One executable binding truth (KEY-0)

`workshop/keymap.hpp` owns the split every keyboard question routes through:

```text
ACTION      stable dotted id + label + context + default gesture   constexpr kActionCatalog rows
BINDING     the gesture that requests it                           default + maker override
EXECUTION   the owner that performs it                             untouched -- each dispatch
                                                                   site switches on the action
                                                                   id and calls its own function
```

- **`Session::keymap` is the effective truth** (defaults + admitted overrides + the legend),
  read by dispatch, by every help surface, and by persistence. No surface spells an
  executable gesture as a literal any more: the band rows, the title hints, the setup line,
  the mode headings, the Builder header, the terminal header and prompt, the notice hints and
  the boot line are all projections through `hotkey_text`/`gesture_text`. Adding a gesture
  claim as a string literal is reintroducing the drift KEY-R0 measured in six places.
- **`keyboard_context(const Session&)` (screen.hpp) is the routing chain, spelled once.** It
  replaced the chain's five hand-kept spellings: `on(KeyPressed)` and `on(TextEntered)` both
  switch on it, `paste_owner_now()` derives from it, and the old
  `editable_text_has_keyboard()` mirror is `context_takes_text(ctx)`. Arrangement answers
  as its scope (`kArrangePane`/`kArrangeDesk`/`kArrangeReset`, ARR-0) because those are
  different vocabularies. It is resolved fresh, stored nowhere; there is no context stack,
  no registration, no focus framework. `keyboard_context_beneath_menu()` is the chain's
  tail below the contextual surface, split out because annotations need it as a value.
- **Matching is exact.** A binding matches the observed modifier bits exactly; the old
  subset aliases (Ctrl+N created, Alt+Q quit) are removed and behaviorally falsified. One
  family spelled two ways (`hjkl` / `Shift+hjkl`, `b` / `Shift+b`) is two declared actions.
- **Three declaration-only activity classes:** `kGlobal` rows are answered above every mode
  (`document.open`, `workshop.terminal` = `ctrl+t`, `workshop.hotkeys` = `ctrl+k`);
  `kNoText` is `workshop.quit`'s (`ctrl+c`) and `workshop.attention`'s (`ctrl+a`) — active
  exactly where no editable text has the keyboard, TEXT-0's law as a declarable fact; and
  `kNoEditor` is `document.save`'s (`ctrl+s`) — above every mode EXCEPT the source editor,
  where the same physical chord is `editor.save`'s own context row (EDIT-0). That is the
  `^c` resolution one chord over: a global colliding with an editing surface's meaning is
  resolved by following the keyboard, and the resolution is DECLARED so admission's
  collision law, the help surfaces and dispatch read one fact — the two save rows' contexts
  do not intersect, so no state ever has both. `on(KeyPressed)`'s head answers
  ONLY rows declared in those classes (`above_mode_action`); an action's ordinary context row
  — quit's own `q`, and `editor.save`'s `^s` — travels the chain, which is what keeps the
  hotkey view's modal swallow
  ahead of it. `shift+space` is GONE, not aliased: it could never arrive from the POSIX
  backend, which is the remapping capability's own motivating defect.
- **An action may own several rows** (`workshop.quit`'s `^c` + `q`; since ARR-0 the whole
  arrangement vocabulary — every gesture shared by the two scopes is one action with a row
  in each); an override moves all of an action's rows. **The Move/Size→Arrange migration
  preserved identities**: `workshop.manage` (label `arrange desk`), `manage.next`/
  `previous`, `place-*`, `pull-*`, the order/remove/reset family and `manage.close`/`done`
  all kept their ids so authored overrides keep working; `manage.arrange` is new (Return,
  kArrangeDesk — earned by the desk's narrowing, not minted for the menu); `manage.move`,
  `manage.size` and `manage.edge` are RETIRED — an authored row naming one is preserved
  byte-for-byte as an unknown id, exactly as admission has always treated ids it cannot
  spend. `manage.previous` defaults to `shift+tab`, which the POSIX backend now delivers
  (`ESC [ Z`, back-tab — input/translate.hpp learned the one CSI whose final IS its
  modifier, and `posix_gap` no longer flags shift on Tab).
- **The keymap file is the sixth durable artifact** (`keymap_persist.hpp`,
  `zengine-workshop-keymap` v1, `--keymap`, default `workshop-keymap.json`): defaults in
  code, authored differences only, absent ≡ defaults, hand-edited, never rewritten. Loaded
  once on the first `SurfaceReady` (the session restore's own moment), answered in words.
  Admission refuses — naming what a maker can fix — a gesture outside the grammar on a KNOWN
  action, an action authored twice, a same-context collision (both actions and the contested
  gesture named; the check runs over the EFFECTIVE map, so an override colliding with
  another action's default is caught), a bare printable on a global, and a component-owned
  chord on a global (the old "the TextBox never binds ^s/^o" discipline, checkable for the
  first time). An unknown action's row is PRESERVED byte-for-byte, gesture unjudged — the
  setup law's ACCEPTED clause; `Keymap::authored` is what a save writes back, so round-trips
  edit nothing. A known POSIX-gap gesture is accepted and the gap said once (`posix_gap`).
- **The legend** (`full`/`compact`/`hidden`, `default` = the code's answer) governs exactly
  the band's legend rows — two in a cell medium, one on the shipped face, however many the
  band's budget composition granted (WUX-1) — hidden blanks them, reclaims no geometry, and
  unbinds nothing — dispatch never reads it. The FULL rows fold four families (`hjkl move`,
  `shift+hjkl size`, `up/down row`, `[ ] workspace`) exactly while every member sits on the
  default that makes the folded word true (`help_pairs`); what a packed row cannot carry is
  marked, and the full hotkey view remains the complete list in every mode.
- **The full hotkey view** (`Session::hotkeys`, `paint_hotkeys`) opens **beside the selected
  pane** since WUX-5 and **fits what it says** since QR-17: `hotkeys_bounds(session, screen)`
  anchors it at that pane's visible OUTER top-left and sizes it from its own rows —
  `hotkeys_rows(session)` is the one composition (the heading, then the groups; painted row
  i IS composed row i), the longest row is the width and the row count the height, read
  into whole cells by `popup_bounds_at`: the contextual surface's own arithmetic (ARR-0),
  quarried out of `context_bounds` so the two surfaces share one measurer, one chrome and
  one clamp. It shifts the rectangle whole inside the band the overlay stack respects, and a
  list taller than the band keeps the band's height while the painter says what it cut.
  There is no fixed width, no floor-to-ceiling column and no `kPickerRows` floor any more: a
  small population is a small view, on both faces (a character medium's interior is exactly
  the content; the shipped face adds only the slack `chrome_outer_of`'s whole-cell
  reservation leaves it, WUX-8). It is DERIVED at every paint and every press — nothing is
  stored, so moving or resizing the pane moves the help on the next projection and no
  position reaches a file. With NO selection, and for a selected pane with no rectangle on
  this screen, it opens at `overlay_column(sc)`'s CORNER — where this view always opened —
  and nothing invents a selection to anchor to. ⚠ `attention_bounds` did NOT follow it: a
  condition is about the application and anchoring that list to whichever pane was last
  pressed would assert a relationship that does not exist; that view is still the column
  outright.
  The view is a projection, not an owner: it lists the context BENEATH it, grouped by owning
  layer, with the component's editing vocabulary shown from `component::kEditingVocabulary`
  and marked not remappable, and a focused pane described only as ownership — Workshop is
  never told a provider's bindings and must not guess. It is keys-modal while open (its
  toggle and bare Escape close it; Escape is deliberately NOT a keymap action — a modal
  surface's structural way out must not be authorable into a lockout); the pointer chain is
  untouched, the picker's own precedent — the view's rectangle, however small, is nobody's
  pointer space (`occupied_at` never reads it), and `screen_of` cannot see it.
- **The printable-trigger swallow is derived from the binding** (`expected_text_of`), armed
  centrally in `on(KeyPressed)` when the keymap consumed a text-faced gesture, cleared by
  the very next key or text. No site hard-codes an expected character, and nothing swallows
  an unrelated later one. The correspondence is the US-layout face with case-folded letters
  — the same honest reach the old three hard-coded sites had.
- **What deliberately does not exist:** no callback or `std::function` in the keymap, no
  command bus, no registry object, no provider-contributed declarations (the pane seam still
  has no shape for wanted keys), no TextBox remapping, no sequences/leaders/macros, no new
  wire vocabulary — KEY-0 added zero bus shapes.

## What can I do with this? The contextual surface (CTX-0, placed beside the hand by ARR-0)

A maker points at a thing — a pane, a document object, a LAYOUT TAB (WUX-11), or the empty
room — and Workshop lists the actions declared meaningful for that KIND of thing, in a bounded
popup BESIDE the press. Right-click opens it on the pointed subject (every backend already
delivered button 3; Workshop used to drop it); `workshop.context` (`a`, command mode) opens it
on the subject command mode can truthfully name: the selected object, else the room. Two laws
bound the whole surface:

```text
POINTING NAMES A SUBJECT FOR ONE REQUEST.    Opening captures a temporary subject and
SELECTION IS A STATE A MAKER ENTERED.        changes no selection, no keyboard candidate,
                                             no focus. Arrange is the one exception -- it
                                             binds the scope AND selects the subject
                                             (WUX-7) -- and only AFTER its explicit target
                                             passes admission (`enter_arrange_pane` -> the
                                             target-taking `arrange_geometry_ready`).
OPEN REMEMBERS AN IDENTITY.                  `ContextMenu` holds a `PaneRef`, an object id,
SPEND RE-ASKS ITS OWNER.                     a layout POSITION, or nothing -- never a
                                             rectangle, row or handle.
                                             The owner answers absence in its own words; a
                                             ref outside the setup gets ONE truthful
                                             absence sentence, not a geometry refusal.
                                             `load_document()` drops a captured OBJECT
                                             subject -- the one identity-aliasing door.
```

- **THE POPUP IS LOCAL AND ITS BOUNDS ARE DERIVED (ARR-0).** `ContextMenu` also captures
  the opening press's canvas CELL (`anchored`/`anchor_x`/`anchor_y` — the GESTURE's place,
  not the subject's; the keyboard entrance is `anchored == false` and opens at the overlay
  stack's corner, a deterministic placement rather than an invented pointer).
  `context_bounds(session, screen)` re-derives the rectangle at every paint and every
  press: extent from the LEVEL's own composition — **its rows and nothing else since
  WUX-5** — read backwards into whole cells through `surface::region_cells_for` (the one
  text measurer's inverse, living beside `fit_region` so a second inversion cannot drift by
  an inset), grown by the chrome through `chrome_outer_of` so the content lands inside the
  surface's own boundary, width capped at `kContextMaxCols` (= `kStackW`), then shifted to
  stay whole inside the band the overlay stack itself respects (`kStackY` to
  `kWorkspaceY + room_h`, inside the canvas) — the measurer, the chrome and the clamp are
  one function, `popup_bounds_at`, since QR-17, and the full hotkey view spends it too.
  Entering a group re-derives at the SAME anchor, so depth stays local. A level taller than
  the room keeps the room's height and `list_window` says what was cut.
- **THE FIRST ROW IS AN ACTION (WUX-5).** The surface used to open with two chrome rows — a
  title reading `ACTIONS -- ` and the captured subject's reference, and a hint restating the
  choose/back gestures — and both are gone, along with `kContextHeadingRows`,
  `context_row_budget` and `context_subject_text`. **Painted row i IS population row i now**,
  so the painter and `context_press_at` have no offset left to disagree about. The hint was a
  second cheat sheet: `context.choose`/`context.back` are ordinary keymap rows, so the band's
  generated legend already says them, in the maker's own bindings, for exactly as long as this
  surface is open. The title announced that a menu of actions contains actions and PAID FOR IT
  IN WIDTH — it was the widest string on most levels, so the removed rows were the width
  FLOOR and the popup shrinks to its content now, with no separately hard-coded narrower
  number. A group level lost no meaning either, which was checked rather than assumed: every
  label in `Order` and `Reset` says what it does on its own.
- **`kContextCatalog` (context.hpp) declares, and owns no power**: `{action id, subject
  bits, group}` referencing `kActionCatalog` ids — no callback, no label, no gesture, no
  availability. A stale reference is a compile error (`context_actions_resolve`). Groups
  are their names; an empty group cannot exist (a group entry exists only where a member
  declared it). `context_population` is the ONE population owner — painter, cursor bound,
  keyboard choose and pointer press all spend it. The pane's top level since ARR-0:
  `arrange`, `Order >` (front/back/raise/lower — renamed from `Arrange`, which an
  `arrange` action row one level up made a lie), `Reset >`, `remove`. **A LAYOUT TAB's, since
  WUX-11**: `layout.rename`, `layout.duplicate`, `Order >` (move-left/move-right — the same
  intent twice, so a maker reads them together), `layout.remove`.
- **A ROW MAY TEACH ITS SHORTCUT, AND ONLY A TRUTHFUL ONE (ARR-0).** `context_annotation`
  shows an entry's effective gesture (through `hotkey_text` — a remap moves it) exactly
  when the action owns a declared row `active_in` the context the maker RETURNS to when
  the surface closes (`keyboard_context_beneath_menu`) — so room rows teach their command
  keys over command mode, globals teach everywhere, a mode beneath that swallows bare keys
  suppresses the command annotations, and pane rows never annotate (their contexts are the
  arrangement scopes). **TWO semantic refinements, and they are the same rule:**
  `object.delete` is taught exactly when the captured object IS the selection, and a LAYOUT
  row exactly when the captured tab IS the active layout (WUX-11) — because `^w` closes the
  live layout while the menu's row closes the pointed one, and anything else teaches a key
  that acts on a different subject than the row it sits beside. ⚠ **And a row whose action
  answers to no key never annotates at all** (`is_bound`): the four tab operations are reached
  from this very menu, and a `?` there would teach a binding that does not exist.
  `context_row_text` is the one composition of label + annotation column; the painter and
  the width both spend it (HD-3).
- **Spending is one seam per subject kind**: `spend_pane_action(Act, PaneRef, Mail&)` (the
  one switch; `arrange_key` passes its addressed pane, the menu its captured ref — mode
  bookkeeping stays with the keyboard caller), `delete_object_at(id)` (`delete_selected`
  reused exactly when the id IS the selection, so its neighbour repair stays authoritative;
  a live draft holds the contextual delete back with `finish_draft_first`), the LAYOUT rows
  call the position-taking doors (`open_layout_rename`, `duplicate_layout`, `shift_layout`,
  `drop_layout` — each of which re-asks the run whether that position is still a layout), and
  the room's rows call their zero-target owners directly (deliberate one-line duplication — a
  zero-target call has no target to drift).
- **Paint is not policy**: the menu shows what is DECLARED for the subject kind and renders
  the captured identity (an identity, not an existence claim); the owner refuses at spend.
  No owner predicate runs on the paint path.
- **A mode with first refusal**: `KeyContext::kContext` sits at the top of the picker band
  (`keyboard_context`), and the pointer branch consumes every press while open — inside:
  navigate/choose through the painter's inverse (`context_press_at`, one composition with
  `paint_context` over the same derived bounds, HD-3); outside: dismissal, consumed whole;
  a further right press re-targets. `manage.remove` (`d`, both arrangement scopes)
  completes the arranging vocabulary and is the same owner arm the menu's remove row
  spends. The provider seam is untouched: no `PanePressed` for a second button, no
  provider-contributed rows, nothing crosses.

## A thing that HAPPENED and a thing that is TRUE are two surfaces (WUX-4)

Workshop says outward truth two structurally different ways, and using the wrong one is the
defect this section exists to prevent.

```text
AN UTTERANCE          `Session::notice`, written by `say(text, bad)`. About a moment that has
                      passed. Replaced by the next thing said, and retracted no other way.
                      `committed Width = 40%` · `removed Info` · `released #12`
A CONDITION           true when it is READ. Held under a key (`Session::conditions`,
                      workshop/attention.hpp) or DERIVED from a live owner. It disappears
                      because it resolved, never because something else was said.
```

- **`Session::notice` is the utterance row and nothing else now.** Its ~104 producers keep
  their job; what left it is the standing truths — the refused keymap file, the refused prefs
  file, a shadowed legacy file, a pane's refused update, a waiting frontier. The severity bit
  went with them: `speak_startup_notes` joins only the EVENT halves and says them `bad=false`.
- **`attention_conditions(session, frontier)` is a PURE PROJECTION in `paint`'s family.** It
  reads the held set and the derived owners, ranks, and owns nothing. `attention_shown` is the
  same list less what this session dismissed, and it is the ONE population the compact
  indicator, the view, the cursor bound and the dismissal all spend.
- **A derived condition must stay derived.** `ExternalPane::refusal`/`refusal_why`,
  `pane_state_of` and `ProjectFrontier` are correct by construction; copying one into
  `HeldConditions` to make it presentable buys the staleness it currently cannot have. The
  pane-content refusal is the case that proves it: the pane clears its refusal on the next
  valid content and the condition is gone with no retraction call anywhere on the path.
- **THREE PANE STATES EARN AMBIENT ATTENTION AND FOUR DO NOT**, and the exclusions are the
  judgement: `refused` / `waiting` / `off-room` (authored, resolvable, and no cell of it on
  the screen) are conditions; `closed` is the maker's own choice, `unresolved` is already
  counted on the band's status row all day, `covered` has something of it visible, `open` is
  nothing. The model may know more than the projection elects to surface.
- **The compact channel is the `kSlotScore` `SurfaceText` slot** — published on every repaint,
  BEFORE the canvas, because the SDL medium composes it INTO the picture it draws
  ([`surface.md`](surface.md#the-attention-chip-is-the-mediums-own-furniture-wux-4)). The
  loudest condition plus an honest `(+N more)`; **empty is the retraction**, so nothing has to
  be un-said when the last one resolves. No band row was taken.
- **Ranking is `ranks_before`: loudness, then key.** `surface::role`'s integers are a
  vocabulary and not a ladder, so `attention_rank` is the one place this application claims
  one role is more urgent than another — total, with an unknown role last.
- **DISMISSAL IS SCOPED TO THE STATEMENT, NOT THE KEY.** `AttentionView::dismissed` holds
  `{key, stamp}`; `Condition::stamp()` is an opaque comparison token over compact + detail +
  role + action. A condition whose content moves is a different statement, so a prior
  dismissal does not reach it and it is visible again with nobody clearing anything — the
  Terminal completion's `dismissed`/`dismissed_at` rule one layer out. Session-only, never
  persisted, and it changes no underlying truth.
- **`KeyContext::kAttention` is a MODE in the picker's place** — below the Terminal and the
  arrangement scopes, above a focused pane and a live draft — deliberately not keys-modal like the
  hotkey view, because its four gestures are real catalog rows the help surfaces and a
  maker's keymap file must be able to see. `workshop.attention` is a `kNoText` row on `^a`:
  the component owns that chord inside a text field and this class is exactly what tells the
  two apart, `workshop.quit`'s own shape.
- **A condition NAMES an action and holds no power.** `Condition::action` is an
  `ActionRow::id` or nothing; the view paints that row's CURRENT gesture through the effective
  keymap and stops there. There is no invocation path from the view, and adding one would need
  its own authority argument.
- **Nothing may open this surface but a maker's gesture.** No severity, no count and no
  condition becoming true reaches `toggle_attention`, and a modal is still earned by required
  maker intent rather than by diagnostic severity.
- **The condition path touches neither the Recorder nor the Logger.** `workshop/attention.hpp`
  includes exactly `surface/vocabulary.hpp`: no substrate header, no `ZEN_SHAPE`, so a
  condition has no wire form and cannot be observed, recorded, selected or persisted.
  Displaying one implies no history; if some condition ever deserves durable history, the
  host-authored diagnostic seam is a separate decision.

## The keyboard goes where the maker last pressed (MSG-0)

`Panels::keyboard` is a POINTING's memory: which keyboard-taking pane — an external pane, or
a built-in whose catalog row declares it takes keys — the maker last aimed the keys at.
`keyboard_pane(panels)` is the external ANSWER, resolved fresh at every spend — open, runtime
kind, room granted; the same three `external_press` already requires. `editor_has_keyboard`
and `files_has_keyboard` (screen.hpp) are the built-ins' twin resolutions, beside
`keyboard_context` because the router, each pane's header mark and the band all ask them.

- **CANDIDACY IS DECLARED; READINESS IS RESOLVED (EDIT-1).** `PanelKind::takes_keyboard` is a
  fact about a KIND and lives on its catalog row; whether that pane can take keys AT THIS
  INSTANT is live state its own resolver answers — the Editor needs a document open, Project
  Files needs a listing — and both resolve fresh and store nothing. Two questions, two owners.
  ⚠ Extraction trigger, recorded: while the Editor was the only such built-in the routing
  layer simply named it (`kind == panel::kEditor`); at the SECOND consumer that spelling
  became a disjunction somebody must remember to extend, in a file with no other reason to
  know which panes take keys. The declaration moved; nothing registered, and this is still
  not a focus framework.
- **ONE READING DECIDES BOTH, at the top of the pressed branch, before any layer answers**
  — the keyboard candidate and the SELECTION (WUX-5) are two facts off one press rather than
  two decisions about one press: `selected = occupied ? kind : kNoPaneKind`, then
  `keyboard = selected ≠ kNoPaneKind ∧ (is_runtime_kind ∨ takes_keyboard) ? selected
  : kNoPaneKind`. Every other built-in still clears the candidate and keeps the selection,
  which is the whole difference between the two: selection is about every pane, the keyboard
  candidate about the panes declared able to take keys. ⚠ The PRIOR answer
  is read one line ABOVE it (`files_has_keyboard`) because Project Files' press rule needs
  it: reading it after would make every first press look like a press in a pane the maker was
  already working in. Putting the line in
  the routing arms would be four decisions about one fact, and the fourth is the one nobody
  adds. `occupied_at` sits beside `info_body_at` in the route to make that possible — the same
  hoist, one question further on, licensed by the same argument. The candidate's SECOND
  writer is the Builder's edit-source door (EDIT-0): opening a source points the keys at the
  Editor it just filled, because an open that left the keyboard elsewhere would land the
  first keystroke in the wrong place.
- **⚠ THE PRESS THAT POINTS THE KEYS IS NOT AN ACT IN THE PANE.** Project Files activates a
  row on a press only when the pane ALREADY held the keyboard (EDIT-1) — without that, a maker
  aiming at a cold pane whose cursor happens to rest on the pointed row would open a file, or
  meet the dirty refusal, having done nothing but look. Two presses from cold is the price of
  "no single press replaces what is open". Double-click was not available to consider:
  `input::PointerButton` carries no click count, and widening the pointer wire for a file
  browser is the shape this repository refuses.
- **The candidate is never cleared and the target is never stored.** A pane that closes, stops
  resolving, or loses its room stops being the answer with nothing to clear; if it comes back,
  so does the keyboard. `bounds_of`'s discipline applied to a focus.
- **The modes above it never reach that line**, so opening the Terminal or an arrangement
  scope leaves the candidate exactly where it was and closing it hands the keys straight back. The
  priority reads: the above-mode actions (the keymap's `kGlobal` rows — open, the
  terminal toggle, the hotkey view — quit's and attention's `kNoText` chords exactly where
  nothing takes text, TEXT-0, and the document save's `kNoEditor` chord everywhere but the
  source editor, EDIT-0), then the six modes (the contextual surface joined the picker's
  band at its top, CTX-0), then a focused pane, then the source editor holding a document,
  then a live property draft, then
  `command()` — spelled once, in `keyboard_context` (KEY-0). The pane and the editor share
  the one candidate, so whichever was pointed at LAST answers, the pressed-into-last
  symmetry unchanged.
- **A focused pane sits ABOVE a live property draft**, and the symmetry is what decides it:
  both are PLACES reached by pressing into them, so the one that answers is the one the maker
  pressed into LAST. Pressing back into the Info body clears the candidate by the same line
  that set it. The draft is never cancelled, committed or touched by any of it.
- **THE PANE GETS EVERY BARE KEY, `q` INCLUDED, AND THAT IS THE DESIGN.** The global
  survivors are CHORDED and that is a rule rather than a coincidence — since KEY-0 an
  ENFORCED one (keymap admission refuses a bare printable on a global row): a bare printable
  cannot be global once anything on the screen can take text, which is the whole reason
  typing `p` into a field does not open the picker.
- **`^c` FOLLOWS THE KEYBOARD SINCE TEXT-0.** It quits exactly where nothing takes text
  (command mode, the picker, the arrangement scopes) and travels the chain everywhere text has the
  keyboard — the Terminal line, the name editor, a live property draft, and a focused runtime
  pane, which receives it as an ordinary `PaneKey` because a pane that takes every character
  is a place `^c` means copy at. The gate is `context_takes_text(keyboard_context(...))`
  since KEY-0 — a derivation of the one resolved context, where a hand-kept mirror predicate
  used to stand; the other above-mode chords stay above every mode, and quit stays one
  press-elsewhere (or `q`, or the close box) away. A consumed
  `^c` with nothing selected is still consumed — "copy nothing" must never quit the program.
- **So the screen says so, in two places, in CHARACTERS.** `external_header` marks the pane
  that has the keys (`> Loaded @…`, unmarked `  Loaded @…`, the same width either way), and
  the band's first legend row becomes `typing goes to <name> @<office> — press elsewhere
  for Workshop's keys`, the chords that still work following it — in the second legend row
  where the budget grants two, packed after the sentence where it grants one (WUX-1) —
  generated from the keymap's global rows since KEY-0, so the list that once had to be
  hand-kept truthful (`^c` left it with TEXT-0) is derived now; a band advertising a quit
  that would not happen is the lie this band exists to refuse.
  **`keyboard_pane` is in `panel.hpp` because the ROUTER and the PAINTER both ask it**; two
  answers would be a screen that says a maker is typing somewhere the keys do not go.
- **PANE TITLES ARE A PRESENTATION PREFERENCE WITH A KEY (WUX-1), DURABLE SINCE WUX-3:**
  `workshop.pane-titles`, default bare `t` in command mode, flips `Session::pane_titles` —
  and the toggle IS the maker stating the preference, so it is also the moment the prefs
  file is written (`prefs_persist.hpp`, the maker-configuration home the WUX-1 note
  deferred to — loaded at the first `SurfaceReady` before the first paint; no path means
  live-only, which is `--isolated`'s promise; a file that stands refused is spoken once
  and never overwritten).
  `external_title_rows(panels, kind, titles)` is the ONE resolution of how many header rows a
  pane's presentation reserves, and the painter, the press path (`external_press_at`) and the
  room grant (`refresh_external_rooms`) all spend its answer through
  `ExternalBodyPlace::header_rows` — a hidden title RETURNS its row to the provider through
  the ordinary grant-on-change door. **The pane holding the keyboard always keeps its title**:
  the header's `> ` mark is one of the two on-screen statements of where typing goes, and a
  presentation preference may hide ordinary chrome but may not recreate the measured MSG-0
  lie where keystrokes land somewhere the screen does not name. Focus moves, and the title
  follows it with nothing to clear — `keyboard_pane`'s own resolved-fresh discipline.
- What crosses the seam, and why Workshop never asks a provider whether it wants keys, is the
  pane protocol's law: [`panes.md`](panes.md).

## Several desks, one of them live (WUX-9; per-layout association WUX-11)

A maker's **layout** is a desk plus its optional relationship to one standalone Setup file. The
desk is a `Setup`: the plural cost one vector of the value that already existed and the
gestures that exchange it — no new geometry owner, no new membership notion, no per-medium
fork, no protocol widening.

```text
SetupState
    active        THE live desk, a `Setup`. Every consumer still reads this member.
    active_link   its SETUP ASSOCIATION -- the lifted half of the pair (WUX-11)
    shelved       the INACTIVE layouts as `Layout` (desk + link), values only: no panel,
                  provider, room or selection belongs to one
    active_at     where the live one sits in the maker's order
    naming        the one-line RENAME editor and the position it is about

SetupLink         path (empty = none) + `known`, the last value Workshop successfully knew
                  that artifact to hold. `link_status` derives none / current / modified.
Layout            desk + link -- the shelf's element and the RUN's element, one type
```

- **⚠ THERE IS NO `on_file` AND NO `saved()`.** One comparison copy for a whole Workshop was
  already the wrong shape the moment there were several desks: two layouts both read `UNSAVED`,
  or both read `saved`, against a value only one of them had anything to do with. Every layout
  owns its own association now, and `link_status(desk, link)` is the ONE place the question is
  decided. It is a claim about WORKSHOP'S KNOWLEDGE and never about the disk — no stat, no
  reload, no watcher, so it is not invalidated by another process editing the file, and a
  paint path never goes near a filesystem.
- **`Layout` IS NOT THE TYPE THE LIVE DESK IS**, and that asymmetry is the lift. `active` is a
  bare `Setup` so every reader in Workshop still reads a `Setup`; the lifted element is the
  pair `active` + `active_link`, and `Layout` is that same pair for the layouts that are not
  live. It is one struct rather than two parallel vectors because a shelf of desks beside a
  shelf of links is two containers whose indices somebody has to keep equal, and the first
  `erase` that forgets one is a layout wearing another layout's association.

- **`shelved` + `active_at` IS the run with exactly one element lifted out**, and that is what
  lets the order be STABLE while the live value stays in one member. `activate_layout` puts the
  lifted value back at `active_at`, takes out the destination, and moves nothing else — a SWAP
  is the shorter spelling and is wrong: it leaves the departing layout wherever the arriving one
  happened to sit, so the run reorders itself every time a maker looks at it. Measured by the
  suite's every-destination sweep, which is green for the swap on the first hop and red on the
  second. `add_layout` puts the ORIGINAL back and takes the appended position with a BLANK
  desk; `remove_layout(at)` erases one shelf element where `at` is not live, and where it IS
  live discards that value and takes the survivor now standing at `active_at` (the NEXT
  neighbour), or the one before it when the removed layout was last.
- **THE SIX VALUE OPERATIONS, AND WHICH OF THEM COPY (WUX-11).** `add_layout` makes a
  `default_setup()` with NO association — new means new, and copying is `duplicate_layout`'s
  job. `duplicate_layout(at)` copies that layout's desk (its NAME included: duplicate names are
  legal and position is the identity), inserts the copy directly after its source, makes it
  live, and **always clears the association** — an inherited one would have the copy claim an
  artifact it has never been written to, and the first `s` would overwrite the very file the
  maker duplicated in order not to touch. `move_layout(from,to)` and `duplicate_layout` are
  spelled THROUGH the inverse pair (`layout_run` → one erase and one insert → `install_layout_run`)
  rather than by index surgery on `shelved` + `active_at`, because that surgery is a third
  spelling of the lift in the one operation that crosses it; the live element's new position is
  COMPUTED from the erase and the insert, never searched for, since two layouts may hold equal
  values. `rename_layout(at,name)` writes one name and no file. `adopt_known_setup(path,known)`
  is the shared-artifact sweep.
- **A SWITCH IS `restore_setup` MINUS THE FILE READ.** `switch_layout` → `activate_layout` →
  `apply_setup` → one sentence → the repaint the gesture already earns. There is no tab-switch
  reconcile path and no teardown path for a removal; `apply_setup` remains the one door
  membership changes through, so `forget_removed_selection`, the seating, the per-opened asks
  and `refresh_external_rooms` all behave exactly as they do for a restore.
- **PER-LAYOUT IS THE VALUE'S OWN FIELDS AND NOTHING ELSE**: participation, authored place,
  authored extent, authored front order, the name. Everything else is one Workshop-global truth
  and a switch does not copy, clear or revalidate any of it — the catalog, every provider
  instance and its state, the Editor's document, the browser's location, the marks, the
  recipes, the project anchor, the clipboard, the keymap, the window, `Panels::selected` and
  `Panels::keyboard`. A selection whose pane is absent from the live layout lifts nothing and
  anchors nothing, by `selected_pane`'s own resolution (WUX-5), and means something again when
  its pane participates again. Per-layout selection or keyboard focus would be a second store
  of a fact that is already derived correctly.
- **ONE PANE IN TWO LAYOUTS IS ONE PANE, and one provider.** A second instance is unsayable:
  one `PaneRef` resolves to one runtime kind in one session catalog. Leaving a layout withdraws
  the PRESENTATION (`close_panel` — Workshop has no unload path, sends nothing and retracts no
  offer); entering one re-seats it and re-earns its room through the dragged-window-edge path.
  A pane in both layouts at the same prose capacity hears NOTHING — no grant, no ask — which is
  the room churn the suite measures the absence of, as a count.
- **AN INACTIVE LAYOUT IS AN UNREAD VALUE.** Its rows are never walked when the catalog changes:
  a new offer enters the runtime catalog and the picker for the run, and enters no layout's pane
  list unless a maker authors that participation. A dormant reference resolves at its layout's
  next activation with no byte of the value having moved — the shipped unresolved-reference
  machinery is the whole mechanism, and there is no catalog fanout and no zombie instance.
- **THE CEILING IS A BOUND ON WORK, NOT A CLAIM ABOUT THE ROW.** `kMaxLayouts` (8) refuses a
  ninth rather than dropping one; the tab run is composed against whatever the band's row has
  and says what it could not paint, so raising the number is a number change.
- **THE FILES DID NOT MOVE.** `s` writes the LIVE layout to a setup file and `r` reads one into
  the LIVE layout; neither touches the shelf, and a Setup file still means one desk. **THE
  SESSION CARRIES THE WHOLE RUN SINCE WUX-10 AND EVERY ASSOCIATION SINCE WUX-11** (version 6
  since WUX-12, below), so the set, the order, the names, which one was live and what each is related to all
  come back — and the ownership split is unchanged by the plural: the SESSION owns *these are
  the desks I was using on this machine*, the SETUP FILE owns *this is one desk I named*.
- **⭐ RENAMING IS A LAYOUT OPERATION; SAVING IS A FILE OPERATION (WUX-11, retiring P-WORK-12).**
  `s` used to open the name editor and write the artifact when it committed, so a maker fixing
  a typo in a tab had to accept a write to a named file they may not have meant to touch. Now:
  ```text
  layout.rename   double-click a tab, or the tab's contextual menu. Writes NO file.
  setup.name (s)  writes the active layout's desk. Names NOTHING. Identity kept so an
                  authored override keeps working -- `workshop.manage`'s precedent.
  setup.restore   reads into the active layout, and only it.
  ```
  Both file gestures act on the ACTIVE LAYOUT'S OWN ASSOCIATION where it has one, and on the
  host's configured `--setup` path where it does not — the configured path is the ACQUISITION
  DOOR, never a default association. **The association follows a SUCCESS and never an
  intention**: a failed write or a refused read leaves the desk, the file and every association
  exactly as they were, and a `none` layout still `none`.
- **⭐ THE SHARED-ARTIFACT LAW.** Two layouts may honestly refer to one Setup file. When
  Workshop successfully learns what that file now holds — because this run wrote it or read it
  — `adopt_known_setup` updates the baseline of EVERY association to that path, and each layout
  then answers `current` or `modified` by comparing its own desk. Advancing only the acting
  layout's baseline leaves the other claiming to match bytes that were just replaced, which is
  a status wrong about the only thing it is for. It ESTABLISHES nothing: a layout with no
  association, or one pointing elsewhere, is not touched. ⚠ Paths are compared BY BYTES and are
  never canonicalised — the artifact is the path as the host named it.
- **THE RUN LEAVES THIS FILE THROUGH ONE INVERSE PAIR, AND ONLY THAT PAIR** (WUX-10):
  `layout_run(const SetupState&)` puts the lifted value back at `active_at` and answers with a
  NEW vector (so no save can reorder what it is saving), and `install_layout_run(SetupState&,
  run, active)` lifts one back out. The durable owner never touches `shelved` or `active_at`,
  because a second spelling of the lift is a second thing to get wrong — and the pair's round
  trip is swept over every position by the persistence suite.

## The tab run is the left of the Layouts pane's first row (WUX-9, converted WUX-12)

```text
 Code >Build< Inspect +          setup: workshop-setup.json | modified | s save  r restore
 ^ the layout tabs    ^ create   ^ the ACTIVE LAYOUT's association, adjusted to the row's edge
```

- **⭐ THE STATUS IS THE ACTIVE LAYOUT'S ASSOCIATION, IN THREE SENTENCES (WUX-11).**
  `setup: none` — no artifact is associated; `setup: <artifact> | current` — this desk IS the
  last value Workshop knew that file to hold; `setup: <artifact> | modified` — associated, and
  it has since diverged. **`none` DOES NOT MEAN UNSAVED**: the session remembers every layout
  automatically, and an association is the optional explicit relationship a maker asked for with
  `s` or `r`. The word `UNSAVED` is retired here and must not come back as a synonym; the
  session file is never shown in this slot.
- **THE PATH IS WHAT ELIDES, AND THAT ORDERING IS THE WHOLE OF `setup_link_text`.** A row too
  narrow for everything must go on distinguishing the three verdicts; WHICH artifact is what it
  may stop showing. So the path meets `fit_path` against its own budget BEFORE the words are
  appended, rather than the sentence meeting `fit` afterwards — which would cut `modified` off
  the end and leave a maker reading a file name and no verdict. ⚠ And the path yields to the
  UNRESOLVED COUNT and the two hints as well (`path_columns` subtracts `rest`): taking the whole
  remainder for the path reads as generous and starved the dynamic truth behind it at the
  78-column minimum, measured by the suite.
- **THE RESERVATION IS THE WORDS AND THE MARKS, NOT THE PATH.** `kSetupStatusCols` is
  `" | setup: " + <a path elided to its mark> + " | modified"` plus the row's own cut mark, and
  it is DERIVED from those constants' own widths so the reservation and the words cannot drift.
- **THE STATUS IS ADJUSTED TO THE ROW'S RIGHT EDGE where the row still fits.** Combined with
  QR-15's equal-width marker that makes the right-hand sentence perfectly still: neither
  switching layouts nor adding one moves a cell of it.
- **`+` IS AN ACTION, NOT A DURABLE PSEUDO-LAYOUT.** One cell with a span of its own at the end
  of the run — not in `layout_count`, not in the maker's order, not steppable, and unknown to
  the session. It is the LAST thing paid for out of the run's budget, so the active tab's
  visibility and the association's reservation both outrank it; a row too narrow simply does
  not have one and the key is unaffected. Pressing it does exactly what `layout.new` does,
  refusal included.

- **⭐ AND SINCE WUX-12 THE ROW BELONGS TO A PANE, NOT TO A BAND.** The run, the association
  and the workspace fact are the built-in `Layouts` pane (`panel::kLayouts`,
  `placement::kTopBand`) — a catalog row, a setup row, authored fine-lattice geometry, a
  canonical front rank, ordinary paint through `paint_panels`, ordinary occupancy, ordinary
  coverage, picker recovery and session persistence. Nothing about the COMPOSITION moved: the
  same three facts, the same fold, the same degradation order, the same single source. What
  moved is who owns the rectangle, and therefore what a maker may do to it.
- **NO BAND ROW WAS ADDED AND THE BODY'S EXTENT NEVER MOVED.** WUX-9 put the run on the
  bottom band's status row, which was the row that already named the arrangement; QR-14 moved
  that row to the top of the screen, where a selector belongs, by re-homing reserved cells
  rather than reserving another (the plane-sequence section above holds the arithmetic). A
  sixth reserved row would resize the workspace every share resolves against (PNL-0's refusal
  class), which is what neither phase was allowed to spend.
- **⚠ THE RESERVATION IS NOT THE PANE, AND THAT SEPARATION IS WHAT MAKES THE CONVERSION SAFE.**
  `kTopRows` stays out of `room_h` whether or not any pane stands on those rows —
  `screen_of` cannot see a pane and must not learn to. Move, resize or remove `Layouts` and
  the rows go empty and every `%`-sized document object is the size it was. That is PNL-0's
  own rule (hiding Info leaves the column empty) applied at the other edge, and it is the one
  coupling this conversion was forbidden to make.
- **`band_status` IS THE ONE COMPOSITION** and it is what both consumers spend (HD-3):
  `paint_layouts` publishes its `text`, and `band_tab_at` answers a press out of its `tabs`.
  ⚠ Since WUX-12 the budget it is composed against is `layouts_body` — the PANE's interior and
  its fit — so a maker who narrows the pane narrows the run, and the omission markers, the
  association's reservation and `+` all degrade by the rules they already had. A tab's span is recorded as the row is written, so there is no second measurement to
  drift. `band_tab_row` says which prose row the run is on, or `kNoBandRow` — the name editor
  takes the identity row whole; and since WUX-12 a second, ordinary absence joins it, because
  the pane can be closed, off-room or unprojectable. What can NOT displace the run is a
  budget: the notice used to share this surface and outrank it at a one-row budget, and it
  lives at the foot now.
  ⚠ **`band_tab_at` RESOLVES AGAINST `layouts_body`** — the PANE's interior and its fit, which
  is where the painter publishes. A press answered from the rectangle the band used to own
  would be the stale one-row geometry QR-14 exists to make unsayable, and the suite sweeps
  EVERY cell of the screen against the inverse to say so.
- **THE MARKER BRACKETS THE LIVE NAME (`>name<`) AND EVERY OTHER TAB WEARS ` name `** — one
  presentation cell on each side, whichever tab it is, said in CHARACTERS because a band row
  carries ONE role for all of its bytes. Same width either way, deliberately: brackets around
  the live tab alone would slide the whole right side of the row two cells on every switch,
  which is HD-8's moving-target defect. Since QR-15 that width equality is the TYPE's rather
  than an agreement between two literals — `kLayoutLiveOpen`/`kLayoutLiveClose`/`kLayoutTabPad`
  are `char`, and `layout_tab_text` pushes exactly one, the name, and exactly one more, so a
  two-cell marker is unsayable without changing the type. They are spelled beside the run
  rather than shared with `kTypingHere` — two decisions that land on one value are not one
  decision (HD-9). WUX-9 spent both cells on the left (`> ` / `  `); a maker read the marker as
  attached to nothing in particular, which is the whole of what QR-15 repaired.
- **EVERY NAME IS PAINTED BARE** — the authored bytes, no quoting and no escaping. WUX-9 spent
  `quoted_setup_name` here because a layout name may hold spaces and `> my desk  other` leaves
  a reader guessing; the marker cells are that delimiter now, so the gap BETWEEN two tabs is
  two cells and a space INSIDE a name is one, and `Home >My Layout< Art` reads correctly.
  ⚠ **WHAT THAT COST, SAID PLAINLY (QR-15).** `quoted_setup_name` also made the identity one
  TOKEN a reader recovers the maker's bytes from, which is why WS-0a exists: a name honestly
  spelled `Ops" UNSAVED | decoy` can otherwise manufacture the delimiter this row uses between
  the run and the status, and to the NAKED EYE it now does. What survives is the half the
  machine spends — a tab's extent is `LayoutTab::column`/`columns`, recorded as the row is
  written, so where the identity ends is still known exactly and the press inverse is
  untouched. The notices still spend `quoted_setup_name`; only this run stopped, and the
  persistence suite pins both halves.
- **THE VISIBLE WINDOW IS DERIVED AND STORED NOWHERE** — `list_window`'s three rules over
  COLUMNS: a run that fits is painted whole with no marker; the live layout is always painted
  (cut with `detail::fit`'s mark rather than dropped, where even it alone will not fit); and
  everything omitted is counted on its own side out of the same budget (`layouts_omitted_text`
  — `<2` / `3>`, one function so the two markers cannot be worded by two hands). It grows
  outward from the live layout, right then left, alternating, so there is no offset to go stale
  after a switch or a removal and nothing reorders to keep the live tab first. Keyboard stepping
  traverses the WHOLE population, painted or not, and the window follows.
- **THE ASSOCIATION HAS THE ROW'S ONE RESERVATION.** The run is composed against the row's
  columns less `kSetupStatusCols`, so a run of long names can never be the reason a maker stops
  being told what their desk is related to. Everything after that sentence — the unresolved
  count and the two hints — degrades through `detail::fit` exactly as it always did, and the
  status half no longer says the NAME at all: the tabs carry it, and a row that said it twice
  would spend its scarcest resource on a repeat.
- **THE TABS AND THE `+` ARE THE ONLY POINTER SPACE THE BAND OWNS.** The arm sits at the TOP of
  the pressed branch — above every layer, because the band is painted in front of the panes
  (WIND-2a) — and above the line that writes `Panels::selected`/`keyboard`, because pressing a
  tab is not pointing at a pane and must not clear the pane a maker chose. A press on the
  status, on the blank between, on another band row, or on an omitted tab answers nothing and
  falls through exactly as it always has.
  - **A SECOND PRESS ON THE SAME TAB RENAMES IT (WUX-11).** `TabClickMemory` /
    `doubles_a_tab_click` — a SECOND record beside `ClickMemory` because it is a second
    population, not a second opinion: that one's identity is a line, a draft and a WORD, and a
    tab is none of those. What the two share is `kDoubleClickMs`, the arm-on-the-way-out
    discipline and the spend-the-arming rule, so there is no triple-click. The first press has
    already made the tab live, which is why the editor's subject and the live layout cannot
    disagree.
  - **A PRESS ALSO TAKES HOLD OF THE TAB.** `LayoutTabDrag` is the FOURTH gesture record beside
    the object drag, the pane drag and the text drag, for their own stated reason — and it holds
    nothing but whether it is active, because the press has just made that tab live, so what the
    hand carries is always `setup.active_at`. A motion re-asks the SAME inverse against the run
    as it is painted right now and calls `move_layout`; nothing is cached, nothing is
    reconciled, and a release ends the gesture wherever the hand is (`end_held_gestures`).
  - **A RIGHT PRESS ON A TAB NAMES IT AS A SUBJECT** — `context_subject::kLayout`, whose
    identity is the POSITION, captured at the press and re-judged by the owner at spend. Asking
    about a tab does NOT stand on it, which is what lets Close and the two reorder steps mean
    the tab that was pointed at. ⚠ `^w` is annotated beside Close only when the captured tab IS
    the active one, because otherwise the row and the key act on different layouts —
    `object.delete`'s own refinement, found by the live TUI witness.
- **THE GESTURES ARE ORDINARY KEY-0 ROWS**: `layout.next` (`.`), `layout.previous` (`,`),
  `layout.new` (`=`), `layout.remove` (`^w`). Three unshifted printables and one plain ctrl
  chord, and the selection criterion is narrow: the POSIX wire carries an unshifted printable
  and a shifted LETTER and nothing else in that family, so `<`, `>` and `+` — the conventional
  spellings — are bytes `terminal_byte_scancode` cannot name, and ctrl+shift+letter cannot be
  said at all. ⚠ Removal is the one CHORD and the asymmetry is deliberate: discarding a layout
  cannot be undone, so it may not be one slipped keystroke away in the mode where every other
  bare letter does something harmless. `x` was the obvious mnemonic and is refused — BLD-0 bound
  it to "close the Builder" and a later phase took that back on purpose, so a maker's hand may
  still mean the panel by it.
- **⚠ AND FOUR ROWS ANSWER TO NO KEY AT ALL (WUX-11)**: `layout.rename`, `layout.duplicate`,
  `layout.move-left`, `layout.move-right` declare `kNoGesture` and are reached from a tab's
  contextual menu (and rename from the double-click). The criterion above is the whole reason —
  four more of that free set spent on operations a maker reaches by pointing would be four
  gestures taken from whatever asks next. `kNoGesture` is `scan::kUnknown`, the one scancode
  that can never be a binding, and `is_bound` guards THREE places, each a different kind of
  wrong without it: **dispatch** (one unnamed key would otherwise request every unbound action
  at once), **admission's collision check** (two actions answering to no key are not two actions
  holding one gesture, and a whole keymap file would be refused for a clash that cannot be
  pressed), and **the surfaces that SPELL bindings** (the band legend and a menu annotation must
  not teach a key that does not exist; `gesture_text` answers `unbound`). A maker may still bind
  any of them in their own keymap file, and then every one of those surfaces spells it.

## The desk comes back on its own, and the window with it (WUX-0, roots WUX-3)

Workshop writes the desk it was arranged into, the room it was in, and where its window sat
when it closes, and reads them back when it starts. Six maker-facing files, six promises —
and since WUX-3 they live in three OWNERSHIP DOMAINS with three different default homes:

```text
PROJECT -- follows the project: the launch directory, or the path the maker typed
--document   workshop.json           what a maker MADE
--setup      workshop-setup.json     a desk they NAMED -- `s` writes it, `r` reads it

USER CONFIGURATION -- follows the maker: %APPDATA%\zengine-workshop | $XDG_CONFIG_HOME/...
--keymap     workshop-keymap.json    the maker's HAND -- hand-edited overrides + the legend
--prefs      workshop-prefs.json     the maker's EYES -- presentation preferences, written
                                     by Workshop at the moment a preference is stated
                                     (pane titles, WUX-3); a refused file is never
                                     overwritten

USER STATE -- follows the maker's MACHINE: %LOCALAPPDATA%\zengine-workshop | $XDG_STATE_HOME/...
--session    workshop-session.json   the desk they were USING, the room it was in, and
                                     where the window sat on the desktop
--marks      workshop-marks.json     the maker's PLACES — filesystem locations they asked
                                     to be able to come back to (PROJ-2), written by
                                     Workshop the moment one is marked; a refused file is
                                     never overwritten
```

⚠ **THE MARKS FILE IS STATE AND NOT CONFIGURATION, and the criterion is this block's own.**
A keymap and a presentation preference are meaningful on any machine a maker sits at; a mark is
an ABSOLUTE PATH, so it describes THIS machine's disks exactly as a viewport describes this
machine's window. It is also its own file rather than a field on the prefs, because the prefs
format has one version and no migration — a new field there refuses every existing prefs file
BY NUMBER (`marks_persist.hpp` states all three reasons).

(The load plan and build recipes stay a fourth kind — shipped defaults beside the
executable, authored per project when named.)

- **THE PRECEDENCE IS PINNED AND THERE IS ONE SPELLING OF IT** (`user_paths.hpp`,
  `resolve_durable_path`): an explicit path the maker typed, then `--isolated`, then the
  per-user default. `--isolated` is the whole-application promise *this run reads and
  writes none of my ordinary per-user configuration or session state* — it resolves the
  four per-user defaults to the weave's designed empty-path absence, exists because the
  root flip inverted accidental scratch-directory isolation into accidental danger, and is
  the flag every witness harness and executor live run must carry. Explicit paths outrank
  it, so an isolated witness that needs scratch persistence names its scratch files. An
  environment with no resolvable root is the same absence, said once on the banner —
  never a silent fallback to the launch directory.
- **THE LEGACY TRANSITION IS ONE RULE AND IT CONVERGES BY EXISTENCE** (`user_paths.hpp`,
  `import_legacy_file`; the host wires it for exactly the defaulted paths): a per-user
  default whose file does not exist yet, beside a pre-WUX-3 local file that does, imports
  the local bytes once — safe-written, unjudged (content is the loaders' law), directory
  created on that first write — and says so on the banner and the notice line. An
  existing user-root file ALWAYS wins; the legacy file is never deleted, moved or
  rewritten, and once the destination exists the rule can never fire again. A standing
  shadowed legacy file earns a standing note naming which file is read and how to end it.

- **ONE REPRESENTATION OF A DESK, TWO FILES.** `session_persist::WorkshopSession` nests
  `setup_persist::WorkshopSetup` as a field rather than paraphrasing it, and
  `setup_persist::setup_in` is the one function that turns a written setup into a live one. A
  desk cannot be legal in one file and illegal in the other. Do NOT add a second desk format
  because one save is automatic.
- **...AND SINCE WUX-10 THE SESSION HOLDS A RUN OF THEM, WITH THEIR ASSOCIATIONS SINCE WUX-11
  (version 6 since WUX-12, which moved the number without moving a field — see below).** `layouts` is the maker's order WHOLE — the live one IN it rather than lifted
  out of it — and `active` is which position that was. Each entry is a `WorkshopLayout`: the
  desk, and a `WorkshopSetupLink` holding the artifact's path and the last value Workshop knew
  it to contain. The wire shape is deliberately NOT the runtime container: `shelved` +
  `active_at` is how a running Workshop holds one live desk with no second copy of it, and what
  a saved session MEANS is *these desks, in this order, related to these files, and I was
  standing on that one*. Position remains a layout's whole identity (duplicate names are legal;
  **no durable layout id is minted**).
  - **AN EMPTY PATH IS THE ABSENCE AND IT HAS EXACTLY ONE SPELLING.** Loom's admission has no
    optional fields, so `none` has to be WRITTEN — and unlike the placement, which needs a
    `mode` word because 0,0 is a legal coordinate, "" is not a legal path, so the path IS the
    mode and no word is spent on it. `link_in` refuses the half-association (no path, a desk
    remembered anyway) because a value nobody means must not have two ways of being written;
    a non-empty path's `known` meets `setup_persist::setup_in`, the same whole law the layout's
    own desk just met.
  - **⚠ AN ASSOCIATION REMEMBERS A WHOLE DESK, NOT A HASH.** The comparison it feeds is exact
    equality with a live `Setup`, so carrying the value IS carrying the answer; a digest buys a
    smaller file and costs the ability to say why. **And a restore never re-reads what it refers
    to** — that would be exactly the automatic file access the standing status is defined not to
    perform, N opens of files a maker did not ask about, at every launch.
  - **THE RUN'S OWN ADMISSION IS FOUR QUESTIONS PLUS THE LINK** (`layouts_in`), and each is
    asked against a number this build already enforces elsewhere: non-empty (runtime's floor is
    the TYPE's — `active` is a value); within `kMaxLayouts` (the SAME number the `=` gesture
    refuses a ninth layout with); `active` in range; every contained desk legal by
    `setup_persist::setup_in`, whose sentence is quoted with the layout's position in front of
    it; and every link legal by `link_in`. All are refusals of CURRENT-version data and none of
    them may become a search for a conversion.
  - **THE READ CEILING IS DERIVED AND THAT IS LOAD-BEARING.** `kMaxSessionBytes` is
    `kMaxLayouts * (2 * setup_persist::kMaxSetupBytes + kMaxLinkPathBytes)` — a layout holds a
    desk AND the desk its association remembers, so the bound is TWO desks and a path, and a
    session this build writes must never be one it refuses to read. ⚠ `kMaxSetupBytes` carries
    an order of magnitude of slack over a real desk, so a MEASURED maximal file passes whichever
    multiplier is used: the case therefore asserts the bound against the FORMAT's own numbers
    (`kMaxSessionBytes >= kMaxLayouts * 2 * kMaxSetupBytes`), which is what a mutation back to
    one desk per layout reddens.
- **⚠ THE SESSION READER KNOWS ONE SHAPE, AND YESTERDAY BELONGS TO A CONVERSION (MIG-0).**
  `session_persist` carries `kFormatVersion` and no second number: the retained `v1`/`v2`
  shapes and their two roads are GONE, and `workshop/session_history.hpp` owns them — an
  artifact's material, shipped as `zengine-workshop-session-history`
  (`workshop/session_migration_provider.cpp`), an ordinary operator provider and not a weave.
  **⭐ WUX-10 SPENT THAT AND WUX-11 AND WUX-12 SPENT IT AGAIN, AND THAT IS WHAT IT LOOKS LIKE
  WORKING**: the format moved 3 → 4, then 4 → 5, then 5 → 6, and the cost in the reader was one
  number and one shape each time. The retired struct is copied VERBATIM into `session_history::v<n>` (its wire
  identity — name, version and content id — is what an old file's bytes claim, so a reordered
  field silently strands every one of them; a case pins ALL FIVE historical content ids, each
  with its provenance beside it — v4's `0xb621c9f3616c7bb1` measured off Zengine a39795e, and
  v5's `0x6f5b0dfc72bfa501` read off a session file the predecessor's own live witness left
  behind, which is a door corroborated by bytes rather than by a recompile). The edges RETARGET THEMSELVES: `conversions()` reads `current` off the reader's
  own schema, so `v*-to-v4` became `v*-to-v5` and then `v*-to-v6` with no string edited, the
  shipped plans needed no new row any of the three times, and `operator/migration.hpp` did not
  change. Each edge is DIRECT — `v1 -> v6` is one authored conversion whose body composes
  `session_v1_to_v3`, `session_v3_to_v4`, `session_v4_to_v5` and `session_v5_to_v6` in C++,
  which is what authored means; the catalog holds no `v1 -> v3` or `v3 -> v4` at all, so there
  is nothing to walk.
  - **⭐ VERSION 6 MOVED THE NUMBER WITHOUT MOVING A FIELD, and that is the case this whole seam
    was built for (WUX-12).** `v5::WorkshopSession`'s fields ARE the current shape's; what
    changed is what the same bytes MEAN. A version-5 desk with no `zengine.workshop/layouts`
    row is a maker who had the layout surface anyway, because nothing could remove it; a
    version-6 desk with no such row is a maker who took it off their desk. Two readings, one
    spelling — a version number is exactly the mechanism that tells them apart. ⚠ A retained
    v5 branch in the current reader would compile, admit and behave for every file that does
    not depend on the distinction, so nothing but the source tripwire catches it: the forbidden
    token list in `test_workshop_persistence.cpp` is the guard, and it now names v4 and v5.
  - **AND `v5 -> v6` MATERIALIZES THE FORMERLY IMPLICIT SURFACE, in BOTH copies of a desk.**
    Every layout that does not already name the Layouts pane gains one row — unauthored place,
    width and height (so `placement_bounds(kTopBand, ...)` answers the historical rectangle) and
    the front-most rank (the band was painted after every pane). `desk_v5_to_v6` also runs on a
    link's `known`, because that value is compared for EXACT equality to decide `current` or
    `modified`: converting the live desk and not its remembered copy would tell every maker
    their desk had drifted from a file because of an upgrade they did not make. ⚠ It does NOT
    touch the `known` of an unassociated layout — an empty path requires the canonical no-desk
    beside it and `link_in` refuses every other spelling. And a desk already at
    `kMaxSetupPanes` REFUSES rather than dropping either fact: dropping the surface says the
    maker removed it, dropping a pane says they never had it, and the file survives the
    refusal for a build that can say more.
  - **A v1/v2/v3 SESSION IS EXACTLY ONE LAYOUT, LIVE AT POSITION ZERO.** Those vintages could
    not say how many layouts a maker had, so the plurality is DEFAULTED and not inferred —
    `absent_placement()`'s argument, one field over. Never zero, never two, and every
    non-layout fact of that vintage crosses unchanged (v3's real placement included).
  - **AND EVERY HISTORICAL LAYOUT IS RELATED TO NOTHING (WUX-11).** A v4 session could not say
    that a desk came from a standalone artifact, because a v4 Workshop had one comparison copy
    for the whole application; so the truthful reading is *these desks, in this order, standing
    on that one, and no artifact is known for any of them*. `absent_link()` is the one canonical
    spelling, exactly as `absent_placement()` is one field over. ⚠ Inventing an association out
    of the host's configured `--setup` path would be this reader deciding something the maker
    never wrote down — which is precisely what an association IS, and precisely what those bytes
    cannot say.
  The reader's whole knowledge of history is one arm: *this shape's name at another version*
  is a historical claim, and it asks `op::migrate` for one live direct edge to
  `schema_of<WorkshopSession>()` ([operators.md](operators.md#a-conversion-is-an-operator-whose-signature-is-the-edge-mig-0)
  owns the convention). Adding a rung to THIS file the next time the format moves is the thing
  this move exists to prevent — change `kFormatVersion`, and author the edges in the provider.
  - **A conversion cannot skip a check.** `current_in` is this format's whole law and has
    exactly two callers: straight off the gate, and out of a conversion's answer. `format`
    crosses conversions UNTOUCHED so the reader's own sentence about a wrong format word is
    the one a maker reads either way; `format_version` cannot, so the converter judges the
    vintage it converts and this file judges its own (`forged_version`, which only a forgery
    produces — an envelope claiming THIS version over a body that says another).
  - **The catalog reaches the reader as a READING, never a power.** `HostContext::conversions`
    is a `const op::Catalog*` the host wires beside `frontier` and `interaction_now`;
    `nullptr` is ordinary and is what every fixture gets. Nothing a holder of it can do
    mounts, loads or realizes anything — and an old file's version claim is a lookup key that
    reaches no load door.
  - **⚠ THE ORDERING IS AUTHORED PLAN ORDER, and nothing else.** A provider-only row is
    performed synchronously inside `LoadExecutor::begin()`; the first WEAVE row opens a
    conversation and returns to the host. The session is read from `on(SurfaceReady)`, which
    cannot arrive until a Skin has loaded — a weave row. So a conversion row placed above the
    first weave row is live before one delivery has been made, which is why both shipped plans
    name it second. Do NOT add a retry, a pending posture or a demand-load to buy what row
    order already buys; a suite case pins the shipped plans' order for exactly this reason.
  - **Reading never rewrites.** A converted session is in-memory; the file first changes at the
    owner's ordinary close-time save, which writes the current shape — so a converter is
    needed only while yesterday's bytes still exist, and never twice for one file.
  - **⚠ AND A SESSION THIS RUN COULD NOT READ IS NEVER WRITTEN OVER** (`session_refused_`,
    checked in `save_last_session`; `marks_refused_`'s law, one durable fact over). Restraint
    on the READ path was always here; the session is a file Workshop WRITES on its way out, so
    without the flag an orderly close replaced bytes this run could not read with this run's
    default desk. MIG-0 makes that strictly worse than it was — the likeliest refusal now is a
    conversion this ARRANGEMENT does not carry, which a maker fixes by editing a plan, on a
    file that has to still be there. A DECLINED VIEWPORT is not a refusal: that file was read
    and its desk came back, so the run keeps its session. The standing consequence is a
    condition (`kSessionWallKey`), because the notice is about the launch and this is true all
    run and has a maker action.
- **THE VIEWPORT IS ONE LEVEL ABOVE THE DESK.** `{width, height}` in canvas cells, a sibling
  of `desk` and not a field of it: the same desk is worth having in a big window and in a small
  one, so how much room the surface had describes the APPLICATION rather than the arrangement.
  It is its own shape rather than `surface::SurfaceExtent`, which is a message free to grow a
  field whenever a medium has something new to say — and whose text metric would be a stale
  claim about a font the moment it was written down.
- **CELLS, BECAUSE CELLS ARE WHAT WORKSHOP KNOWS.** The window belongs to whichever Skin holds
  `zengine.skin`, behind a C ABI; the only thing it publishes about its room is `SurfaceExtent`
  and the only thing Workshop says back is how large a picture it wants. The fidelity is a
  bound, not a hope: a restored window is the maker's chosen size FLOORED TO WHOLE CELLS, at
  most `kCanvasCellPx - 1` pixels short on each axis.
- **THE DESKTOP PLACEMENT IS REMEMBERED OPAQUE AND JUDGED BY THE MEDIUM (WUX-3).** The
  placement pair (`surface::SurfacePlacement` / `SurfacePlacementRemembered`,
  [`surface.md`](surface.md#the-medium-owns-the-desktop-placement-in-both-directions-wux-3))
  closed the old deliberate omission: the medium reports where its NORMAL window sits (its
  own desktop units, maximized state beside it), Workshop remembers the last report in the
  session — coordinates it cannot interpret and does not try to — and hands it back once
  at restore, where the medium validates against the displays that exist NOW
  (`placement_within`: verbatim when reachable, clamped into the nearest usable area when
  stranded, untouched with no display truth). A run whose medium reports no placement —
  every terminal — RETAINS the remembered value rather than erasing it, and claims
  nothing. Desktop placement is not canvas geometry: WUX-2's lattice is untouched, and no
  desktop unit enters authored intent.
- **THE SAVED VIEWPORT IS THE NORMAL WINDOW'S (WUX-3).** `Session::normal_w/h` tracks the
  screen except while THIS run's medium says the window is maximized (the medium reports
  placement before extent on its beat so the gate closes first, `skin.hpp`), so a
  maximized close writes the room the maker chose with `maximized` beside it. A maximized
  flag merely restored from the file never gates a placement-less run's own resize tracking.
- **⚠ AND THE RESTORE IS THE SAME SENTENCE READ BACKWARDS, WHICH IS WHY IT WAS THE HALF THAT
  BROKE (QR-16).** Writing both facts down is not keeping them: restoring a maximized session
  REPOSITIONS the normal window, then re-grows it through the ordinary canvas conversation,
  and only then re-maximizes — and until QR-16 the medium re-maximized first, so the platform
  froze the normal rectangle at the floor `on(SurfaceReady)` had just created the window at
  and a maker unmaximized onto 78x22 instead of onto the room they chose. The ordering is the
  medium's and is stated there ([`surface.md`](surface.md#the-medium-owns-the-desktop-placement-in-both-directions-wux-3));
  what Workshop owes it is exactly what it already does — publish the floor picture, restore
  the room, offer the placement, repaint — because the restored room reaching `normal_w/h`
  depends on that second picture being REPORTED while the window is still normal. This is the
  same self-correction an unmaximized restore has always had; the maximize was jumping ahead
  of it. Measured live both ways, Windows/SDL.
- **⚠ THE FIRST PICTURE OF A RUN IS WORKSHOP'S FLOOR, AND THAT IS LOAD-BEARING.** A medium
  that has been told nothing has only a run's first picture to size itself from, and the SDL
  medium makes that size the window's MINIMUM, once, at creation (`SDL_SetWindowMinimumSize`
  in `surface/skin_sdl.cpp`). So `on(SurfaceReady)` repaints at the minimum extent and THEN
  takes the session back: seeding the remembered extent before the first canvas would come up
  at the right size and leave a maker unable ever to shrink their own window again. Measured on
  Windows/MinGW against a real window; a suite case pins the ordering.
- **THE ROOM, AND THEN THE DESK INTO IT.** `apply_setup` seats panes against
  `stack_capacity(screen_of(...))`, so how much of a desk can be presented is a fact about the
  screen. `adopt_screen` runs before `session_.setup.active = last.desk`; reversing the two
  leaves a pane waiting for room it already had (one case red, predicted and measured).
- **ONE DOOR WRITES IT, AND ONLY ON AN ORDERLY CLOSE.** `q`, `Ctrl+C` and
  `SurfaceCloseRequested` all reach `quit()`, which calls `save_last_session()` before it stops
  the bus. No autosave, no dirty tracking, no background writer, and CRASH DURABILITY IS NOT
  CLAIMED — `persist::write_file` does not fsync and says so.
- **ONCE PER PROCESS, guarded in the weave and not at the caller.** `SurfaceReady` arrives
  again whenever a Skin is replaced; a second restore would throw away an afternoon of
  arranging. The flag is set before the file is opened, so a refusal is final too.
- **FOUR ANSWERS, NOT ONE BOOLEAN** — and a first launch is the one that must stay silent.
  `LoadedSession` carries `present` (there was a file at all), `outcome` (it could be read and
  understood), `honoured` + `declined` (its viewport is one this Workshop opens at).
  `load_file` asks `std::filesystem::exists` BEFORE reading, because to `persist::read_file` a
  missing file and an unreadable one are both "cannot read".
- **A VIEWPORT OUTSIDE THE BAND IS DECLINED, NEVER CLAMPED.** `viewport_honoured` is
  `kScreenMinW..kScreenMaxW` by `kScreenMinH..kScreenMaxH`; outside it Workshop opens at its
  floor and names the value. Clamping 100000 to 640 would still open a window nobody chose, on
  a display Workshop cannot see — and whether a size fits the CURRENT DISPLAY is not a question
  Workshop can put to anybody, so this is a plausibility bound and is named as one.
- **NEITHER DIRECTION OPENS A SETUP FILE.** Closing writes a session and leaves the standalone
  artifact byte-identical; restoring a session reads no setup file at all. ⚠ **And since WUX-11
  the session carries the ASSOCIATIONS without reading what they refer to**: a restored layout
  comes back saying `current` or `modified` against the value this Workshop last successfully
  knew that artifact to hold, which is a fact about Workshop's own knowledge and therefore a
  fact a session may legitimately remember. What it must not become is a read.

## Do not assume

- Nothing Workshop persists is read at launch — the DESK, the window's size, the desktop
  placement, the keymap and the prefs are, automatically. The **document** still is not.
- Workshop restores only the window's size — since WUX-3 the SDL path round-trips the
  desktop position and the maximized state too, through the placement pair. The size is
  still floored to whole cells; the position is still never INTERPRETED by Workshop (the
  medium judges it against live displays); and a terminal run still claims no placement
  at all — it retains the remembered one unchanged.
- Workshop's per-user files follow the launch directory — since WUX-3 they follow the
  MAKER (`--keymap`/`--prefs` under the configuration root, `--session` and, since PROJ-2,
  `--marks` under the machine-local state root), and only the document, the setup and the
  shipped plan/recipes stay where they always were. A scratch-directory launch is therefore NOT
  isolated by accident any more: witness harnesses and executor runs must say
  `--isolated`.
- A session save can be trusted after a crash — it is written on an orderly close and nowhere
  else. A killed Workshop loses the session it was in.
- The last session and a named setup are the same thing saved twice — they are two promises in
  two files, and an automatic save that could land on `--setup` would rewrite a maker's named
  desk every time they closed the window.
- `session_persist` still reads old sessions, or an old file gets what it asks for — neither
  since MIG-0. The reader admits ONE version; an older file opens exactly when a conversion is
  already mounted and is refused, by number, when one is not. The standalone SETUP file is the
  other way round and deliberately so: `setup_persist` keeps its own `v2` reader, because a
  setup file is a maker's named artifact with no session to ride, and the session history is
  its only other consumer. It is the next historical reader a phase could move, not a
  violation of this one's scope.
- Hiding pane titles can hide where typing goes — it cannot: the pane holding the keyboard
  keeps its title and its `> ` mark whatever the preference says (WUX-1).
- Docking exists — it is still absent and still refused.
- The Files pane cannot leave the project, or a linked directory is refused, or the browser's
  location is project-relative — all three were EDIT-1's law and all three are retired
  (PROJ-2). Files owns one absolute location, parent stops at the FILESYSTEM's fixed point, and
  a link is marked and entered. What did NOT move is the project anchor: browsing, marking and
  choosing a foreign recipe catalog all leave `HostContext::project_dir` exactly where it was.
- A marked place is part of the project, or is trusted, or is buildable — a mark is a
  DESTINATION. It says one thing: somebody may want to come back here.
- The band is at the bottom, or canvas row 0 is empty — neither since QR-14. The layout
  selector and the setup's status are the screen's FIRST row; the notice and the legend
  are the last. Both are `fit_region`-composed regions, and `kTopRows + kBottomRows`
  is asserted equal to what the two reservations always summed to.
- A layout is a new kind of thing, or `setup.active` is now an index into one — neither. A
  layout IS a `Setup`, `active` is still THE live desk every consumer reads, and the shelf
  beside it holds values with no presentation, no provider and no room of their own.
- Switching layouts reloads a provider, or a pane in two layouts is two panes — it does
  neither. A switch withdraws and re-seats PRESENTATIONS; Workshop has no unload path,
  and an unchanged prose capacity means the provider hears nothing at all.
- The layout shelf is transient, or only the live layout comes back — neither since WUX-10.
  The session is version 6 and carries the whole run, the maker's order, every `Setup::name`
  and which position was live; a restart returns all of it and stands on the same one.
- A session file's `format_version` and a desk's are the same number — they have not been since
  WUX-10. A v6 session nests desks that still say 3, so a search for the bare field finds the
  NESTED one; the envelope's `"version":6` is what says which session format this is. **A
  session format move does NOT imply a desk format move** — WUX-12 moved the session to 6 and
  left the desk at 3, because the Layouts pane is an ordinary row in the existing `panes`
  array. ⚠ A standalone Setup file written before WUX-12 therefore does not name it, and
  restoring one removes it from that layout — which is what restoring a desk has always meant
  (the file's contents, exactly), and the picker is the way back. The SESSION is the automatic
  one and is the one that migrates.
- Restoring a session restores what a maker was DOING — it restores the desks and the room.
  Selection, keyboard focus, the document, the browser's location and every other
  Workshop-global fact are this run's (WUX-0's law, unchanged by the plural). Measured on a
  real screen: a restored layout paints identically except for which pane wears the focus ink.
