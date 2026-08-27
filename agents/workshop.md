# Agent law — Workshop

Routed detail behind [`AGENTS.md`](../AGENTS.md), for tasks touching `workshop/` and
`component/` — the screen's composition, panes and authored windows, the Info panel, the
terminal pane, interaction routing, and session persistence. The external pane seam and the
tools that arrive through it are [`panes.md`](panes.md); the Surface vocabulary itself is
[`surface.md`](surface.md). Phase tags like (HD-7) are provenance markers into this
repository's history; the law here is current.

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
  reservation in `manage_geometry_ready`, the same precedence `pane_state_of` spends between a
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
  anything any of the three reads. `presentation_order` is the one pure order helper; paint
  walks it ascending and `occupied_at` descending.
- **An authored place spends no reactive slot and cannot WAIT for one.** A pane the maker put
  somewhere is not in the tiling, so `waiting` means exactly what it always meant: the reactive
  default ran out of tiles. Both `seat_panes` and `bounds_of`'s slot counter say it, and both
  are pinned.
- **The override is spent on `kOverlayStack` and nowhere else.** `screen_of` reserves the side
  column whether or not Info is open and `room_w` is what every share of the workspace resolves
  against, so a movable Info would change the resolved size of objects in a maker's document —
  PNL-0's refusal, unchanged. A side-region row's authored geometry is retained in the file,
  never rewritten, and never spent; management refuses to author one and says which reservation
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
- **EVERY RESIZE EDGE PRESERVES ITS OPPOSITE ANCHOR (WUX-2, reversing WIND-2's rule).** The
  edge a hand pulls follows the hand; the edge opposite holds still: a top pull changes `y`
  and the height TOGETHER so the bottom edge stays put (`pane_window_proposal` — the START
  tree's measured top-edge defect, where the bottom edge moved instead, is pinned dead), and
  a corner holds the corner across from it. Right/bottom pulls anchor the place by NOT
  writing it — a default place stays reactive — while left/top pulls author place and size as
  ONE transaction through `author_pane_window`, every part judged before any part written, so
  a refused height can never leave a moved corner behind (`doc::resize`'s both-before-either
  law, widened to the axis pair a place is). WIND-2's old objection — that a left edge moving
  the place is two writes for one gesture — is answered by making it one door rather than by
  refusing the geometry a hand plainly means.
- **Escape is BACK, not cancel.** Every immediate-commit gesture in this application is
  reversible only by performing the inverse, and there is no undo. The help says `esc back`.
- **`w` enters pane management from command mode**, paying the `swallow_text_` rule once, after
  which its own keys need no modifier: `tab`/`up` select, `m` move, `s` size, `f`/`b`
  front/back, `r`/`l` raise/lower one, `0` reset (`p` place, `w` width, `h` height, `o`
  order), `esc` back one level. It is the sixth mode, below the Terminal and above ordinary
  command handling. **`w` is on screen before the mode is entered** — as `w window` among the
  band legend's pairs and in the full hotkey view, both projections of the one keymap (the
  row-0 label that used to carry it is retired with the row, WUX-1).
- **The picker and pane management share one list and NOT one purpose.** `inventory_rows` is
  the combined catalog UNION every reference the setup names, so an unresolved pane has a row
  and can be removed by the gesture that removes any other; an unresolved row carries
  `kNoPaneKind` (negative, for `role::kNone`'s reason) so nothing can present it as the
  Builder. The picker keeps PRESENCE — selecting an open row removes it (PNL-0) — and
  management owns ARRANGEMENT and **binds no toggle at all**. **One picker inventory:**
  `picker_population()` is `inventory_rows(active, panels)` and the painter, the cursor bound,
  the Return action and the cursor repair all spend it — widening only the painter to the union
  leaves a row a maker can see and cannot reach.
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

## The plane sequence is the layout of the screen (WIND-2a)

The canvas is an ordered list of planes ([`surface.md`](surface.md#the-canvas-is-an-ordered-list-of-planes-wind-2a));
Workshop's publication order is the whole depth story:

```text
the workspace       its backdrop, the scene, the size handle
one plane per pane  presentation_order(setup, panels), ascending by canonical `front`
the affordances     over the selected pane's own content, so no handle is hidden
picker / management over the panes they cover -- a provider's text cannot bury the row that
                    recovers it
the screen's chrome the bottom band, one budget-composed region (WUX-1)
the Terminal        the final modal plane
```

- **The screen's chrome is in FRONT of the panes, and that is a decision with a reason.** The
  bottom band is where the tool SPEAKS: a panel backdrop drawn over it would erase the notice
  that just told a maker what happened, and since WUX-1 the band OWNS its whole rectangle (one
  region, `kGroundOwn`), so a pane a maker authors over it is covered by it — the rule the
  notice region alone used to carry. Panes are in front of the DOCUMENT, which is what
  `occupied_at` has answered since PNL-2; they are not in front of the tool's own voice.
- **The shared top row is RETIRED (WUX-1).** Canvas row 0 carried four one-cell voices — the
  workspace extent, the picker/window hints, the terminal hint — each structurally unable to
  hold a row of a real face. The facts moved rather than died: the extent is the band's
  `workspace_text` row, and the gestures are ordinary keymap rows said by the band's legend
  and the full hotkey view. The workspace did NOT grow (its extent is what a share resolves
  against; a chrome retirement must not resize a maker's document), so row 0 is empty canvas
  now, the side region's reservation and every other constant unmoved.
- `presentation_order` is the one order helper; paint walks it ascending and `occupied_at` its
  exact reverse. Nothing derives hit order from canvas layers.

## Gestures: press, hold, release (WIND-2, WIND-2a)

- **One press claims one gesture until release.** `PaneGesture` holds an identity, an edge and
  the size at the moment of the press — no rectangle and no live position, `Drag`'s own law —
  so crossing another pane, crossing the Terminal, and reordering mid-drag change nothing about
  who is being moved, and every motion proposes `base + (pointer - press)` rather than
  accumulating. **Management owns the pointer while it is open**, the Terminal's own shape;
  outside it nothing changed, so a selected pane behind another claims no press and no
  selection auto-raises.
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
  management branch and the ordinary path. A gesture begins under one mode and is released
  under another, so whichever mode answers a release first must end them all — ending only one
  kind leaves a gesture alive with the button up, following the pointer afterwards. It says
  nothing; what to tell a maker is the caller's, because the answer genuinely differs. It is
  not a capture framework: three records and one function.
- **`forget_removed_selection()` clears on MEMBERSHIP, never on presentation.** A pane that
  becomes waiting, refused, covered, off-room or unresolved keeps its selection — every one is
  a pane the setup still names and whose management row is still reachable. A reference LEAVING
  the setup clears the selection, the submode, the edge and the gesture. It runs inside
  `apply_setup`, the one door membership changes through.

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
  whole rectangle is its own** (the picker, the pane-management surface, an external pane). It
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
  `band_region`, screen.hpp): budget ≥5 (a character medium) reads the setup line, the
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
- **The body is resolved ONCE PER PRESS, in the route, beside the canvas point.** Holding it
  across the chain is sound for a stated reason and not an assumed one: **every handler changes
  nothing on the paths where it declines**, so a "not mine" cannot have moved the picture the
  next handler is about to ask about. Keep that true when adding another.
- No `Disposition`, no `InteractionResult`, no `Handled/Refused/Ignored`, no target enum and no
  interaction package. The richer answers already exist where richer answers matter
  (`Written`, `Handled`, `Commit`, `Availability`, `Occupancy`) and they are all on SEMANTIC
  paths; the bare bool survives only on the routing path, which is the one place it is
  adequate.
- **Pointer order:** the terminal overlay (a MODE), then the active property editor, then the
  action controls, then the object list, then the panel's occupancy, then the workspace.

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
  `editable_text_has_keyboard()` mirror is `context_takes_text(ctx)`. Pane management answers
  as its submode (`kManageSelect/Move/Size/Reset`) because the sub-switches are different
  vocabularies. It is resolved fresh, stored nowhere; there is no context stack, no
  registration, no focus framework.
- **Matching is exact.** A binding matches the observed modifier bits exactly; the old
  subset aliases (Ctrl+N created, Alt+Q quit) are removed and behaviorally falsified. One
  family spelled two ways (`hjkl` / `Shift+hjkl`, `b` / `Shift+b`) is two declared actions.
- **Two declaration-only activity classes:** `kGlobal` rows are answered above every mode
  (`document.save`, `document.open`, `workshop.terminal` = `ctrl+t`, `workshop.hotkeys` =
  `ctrl+k`); `kNoText` is `workshop.quit`'s (`ctrl+c`) — active exactly where no editable
  text has the keyboard, TEXT-0's law as a declarable fact. `on(KeyPressed)`'s head answers
  ONLY rows declared in those classes (`above_mode_action`); an action's ordinary context row
  — quit's own `q` — travels the chain, which is what keeps the hotkey view's modal swallow
  ahead of it. `shift+space` is GONE, not aliased: it could never arrive from the POSIX
  backend, which is the remapping capability's own motivating defect.
- **An action may own several rows** (`workshop.quit`'s `^c` + `q`; `manage.next`'s tab +
  down; `manage.done`'s esc in three submodes); an override moves all of an action's rows.
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
- **The full hotkey view** (`Session::hotkeys`, `paint_hotkeys`, `hotkeys_bounds` — the
  stack COLUMN, floor to ceiling, one row above the setup line; a single slot was measured
  too small) is a projection, not an owner: it lists the context BENEATH it, grouped by
  owning layer, with the component's editing vocabulary shown from
  `component::kEditingVocabulary` and marked not remappable, and a focused pane described
  only as ownership — Workshop is never told a provider's bindings and must not guess. It is
  keys-modal while open (its toggle and bare Escape close it; Escape is deliberately NOT a
  keymap action — a modal surface's structural way out must not be authorable into a
  lockout); the pointer chain is untouched, the picker's own precedent.
- **The printable-trigger swallow is derived from the binding** (`expected_text_of`), armed
  centrally in `on(KeyPressed)` when the keymap consumed a text-faced gesture, cleared by
  the very next key or text. No site hard-codes an expected character, and nothing swallows
  an unrelated later one. The correspondence is the US-layout face with case-folded letters
  — the same honest reach the old three hard-coded sites had.
- **What deliberately does not exist:** no callback or `std::function` in the keymap, no
  command bus, no registry object, no provider-contributed declarations (the pane seam still
  has no shape for wanted keys), no TextBox remapping, no sequences/leaders/macros, no new
  wire vocabulary — KEY-0 added zero bus shapes.

## The keyboard goes where the maker last pressed (MSG-0)

`Panels::keyboard` is a PRESS's memory: which external pane a maker last pressed into.
`keyboard_pane(panels)` is the ANSWER, resolved fresh at every spend — open, runtime kind, room
granted; the same three `external_press` already requires.

- **ONE LINE DECIDES WHERE THE KEYBOARD GOES**, at the top of the pressed branch, before any
  layer answers: `keyboard = occupied ∧ is_runtime_kind ? kind : kNoPaneKind`. Putting it in
  the routing arms would be four decisions about one fact, and the fourth is the one nobody
  adds. `occupied_at` sits beside `info_body_at` in the route to make that possible — the same
  hoist, one question further on, licensed by the same argument.
- **The candidate is never cleared and the target is never stored.** A pane that closes, stops
  resolving, or loses its room stops being the answer with nothing to clear; if it comes back,
  so does the keyboard. `bounds_of`'s discipline applied to a focus.
- **The modes above it never reach that line**, so opening the Terminal or pane management
  leaves the candidate exactly where it was and closing it hands the keys straight back. The
  priority reads: the above-mode actions (the keymap's `kGlobal` rows — save, open, the
  terminal toggle, the hotkey view — and quit's `kNoText` chord exactly where nothing takes
  text, TEXT-0), then the five modes, then a focused pane, then a live property draft, then
  `command()` — spelled once, in `keyboard_context` (KEY-0).
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
  (command mode, the picker, pane management) and travels the chain everywhere text has the
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
- **PANE TITLES ARE A PRESENTATION PREFERENCE WITH A KEY (WUX-1):** `workshop.pane-titles`,
  default bare `t` in command mode, flips `Session::pane_titles` — runtime state only, not
  persisted (the maker-config domain is a later phase's; do not park it in the keymap file).
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

## The desk comes back on its own, and the window with it (WUX-0)

Workshop writes the desk it was arranged into and the room it was in when it closes, and reads
them back when it starts. Four files, four promises — the third is the one nobody types, and
the fourth (KEY-0) is the one nobody but the maker writes:

```text
--document   workshop.json           what a maker MADE
--setup      workshop-setup.json     a desk they NAMED -- `s` writes it, `r` reads it
--session    workshop-session.json   the desk they were USING, and the room it was in
--keymap     workshop-keymap.json    the maker's HAND -- hand-edited overrides + the legend
```

- **ONE REPRESENTATION OF A DESK, TWO FILES.** `session_persist::WorkshopSession` nests
  `setup_persist::WorkshopSetup` as a field rather than paraphrasing it, and
  `setup_persist::setup_in` is the one function that turns a written setup into a live one. A
  desk cannot be legal in one file and illegal in the other. Do NOT add a second desk format
  because one save is automatic.
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
  most `kCanvasCellPx - 1` pixels short on each axis. Position and maximized state are NOT
  persisted and are not an oversight — no message in the Surface vocabulary carries either, in
  either direction.
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
- **NEITHER DIRECTION TOUCHES THE NAMED SETUP FILE.** Closing writes a session and leaves
  `workshop-setup.json` byte-identical; restoring a session reads no setup file at all.
  `setup.on_file` is deliberately NOT written by a session restore — it is this run's copy of
  what is in the SETUP file, and this run has not read that file — so a restored session still
  says `UNSAVED`, meaning what it has always meant here.

## Do not assume

- Nothing Workshop persists is read at launch — the DESK and the window's size are,
  automatically. The **document** still is not.
- Workshop can restore the window it had — it restores the window's SIZE, floored to whole
  cells. It is never told the position or the maximized state and cannot ask.
- A session save can be trusted after a crash — it is written on an orderly close and nowhere
  else. A killed Workshop loses the session it was in.
- The last session and a named setup are the same thing saved twice — they are two promises in
  two files, and an automatic save that could land on `--setup` would rewrite a maker's named
  desk every time they closed the window.
- Hiding pane titles can hide where typing goes — it cannot: the pane holding the keyboard
  keeps its title and its `> ` mark whatever the preference says (WUX-1).
- Docking exists — it is still absent and still refused.
