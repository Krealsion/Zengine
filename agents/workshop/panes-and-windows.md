# Workshop law — panes and windows

Register `WL-PANE`: the two places, the room's points a word names, the overlay slots, the default a weaver lays an override
over, and the seven states. One law per heading; cite by ID. Router:
[`../workshop.md`](../workshop.md). What crosses the pane seam is the protocol's law, in
[`../panes.md`](../panes.md); this register holds Workshop's side only.

Retired: WL-PANE-14, WL-PANE-15, WL-PANE-01, WL-PANE-03, WL-PANE-04, WL-PANE-05, WL-PANE-06, WL-PANE-08, WL-PANE-10, WL-PANE-13, WL-PANE-16

## WL-PANE-07 — `panes.open` is never reordered

LAW — The open list is seated by walking the setup list in the setup's order, and a reactive slot is counted over that same list; no ordering operation writes what seating and slot-counting read.

MEANS
- that is the whole of "raising a pane cannot move it";
- ordering changes paint order and nothing else.

WHY — `agents/decisions/front-is-a-permutation.md`

## WL-PANE-09 — The host clips and never rewrites

LAW — `bounds_of` answers the visible rectangle — resolved, then intersected with the canvas — and `PaneBounds::resolved` carries the unclipped ask; an off-room pane is recoverable.

MEANS
- every consumer that reads an empty rectangle as "nowhere" is correct for an off-room pane;
- a wholly off-room pane is painted by nobody and its intent is not rewritten; a launch or a reveal of one keeps its place and says so on the band, with the two ways back;
- a pane too small to see is found as an off-room one is -- the Pane Manager, `tab`, Info -- and its words taken back there: how small it is the weaver's choice.

WHY — `agents/decisions/three-places.md`

## WL-PANE-11 — An authored place is a word measured from the room, and each axis is independent

LAW — An authored place is a word and its offsets, measured in the room under the top band on every face, inward from the edges it names and never negative; a place edit freezes no size.

MEANS
- what writes, shows or takes a place agrees: a drag, an arrow, Info's `X`, `Y` and `Window`;
- a pane above the room (Layouts, in its band) placed by one axis comes down to the room's top;
- `pixels` counts from the room's top-left, an anchored pane keeps its corner, edge or middle at every window size, and each unauthored dimension follows its default.

WHY — `agents/decisions/the-whole-pixel.md`

## WL-PANE-12 — Presence is the desk's, through two doors, and arrangement never touches it

LAW — `inventory_rows` is the catalog union every reference the setup names, the one list presence spends; a launch opens or focuses a row, and a close takes one off whether or not anything offers it.

MEANS
- an unresolved row carries `kNoPaneKind`, so nothing can present it as the Builder;
- arrangement binds no toggle, adds nothing and offers nothing.

WHY — `agents/decisions/three-places.md`

## Do not assume

- That a docked pane stands at its place — it stands against its edge's strip, over the room
  (WL-GEO-14).
- That `kinds_placed_in` has a runtime witness — its pins are compile-time only.
- That two panes cannot stand in one place — a desk may say so, and one covers the other.
- That a stacked pane waits for room — a spent column begins again at its top, so a launch
  lands in front of what it covers.

## WL-PANE-17 — Preferred body space is resolved by Workshop

LAW — A preferred size budgets body text, or a canvas body of whole pixels, plus title and chrome in the current medium, bounded by the workspace; an authored dimension wins.

MEANS
- a pixel body is granted exactly: to the pixel in a window, rounded up to cells in a terminal;
- `default` reads the first accepted offer; `fit` follows each re-offer at once, either way, save one the GUI window holds and says: within a pixel, on every axis, of what it stands on;
- the column spends the default height a pane is drawn at; old providers keep their fallback.

WHY — `agents/decisions/preferred-pane-space.md`

## Proven by

### WL-PANE-07
`workshop/setup.hpp` Setup add_pane
`workshop/setup_reconcile.hpp` seat_panes reconcile Reconciled Seating
`workshop/screen_chrome.cpp` bounds_of
`workshop/panes.hpp` Panes::open
`tests/test_workshop_panes_window.cpp` "ordering changes paint order and NOTHING else"
`tests/test_workshop_persistence.cpp` "reconciling opens what the setup names, in the setup's order"

### WL-PANE-09
`workshop/screen_chrome.cpp` bounds_of
`workshop/screen.hpp` PaneBounds PaneBounds::rect PaneProjection
`workshop/weave.hpp` off_this_screen
`workshop/weave_desktop.cpp` off_this_screen on(PaneLaunchRequested)
`workshop/weave_seam.cpp` on(PaneRevealRequested)
`tests/test_workshop_panes_window.cpp` "a partly off-room pane is clipped, and its intent is not
rewritten" "a wholly off-room pane is off-room, recoverable, and painted by nobody"
`tests/test_workshop_panes_info.cpp` "a pane its words leave too small to see, or off the room, is
still listed, reached by tab, inspected by Info and given its size back there"
`tests/test_workshop_panes_seam.cpp` "a pane launched where its own place stands off this screen
stays there, and the band says so and how to bring it back"
`tests/test_workshop_panes_canvas.cpp` "a view shown where its own place stands off this screen
stays there, and the band says so and how to bring it back"

### WL-PANE-11
`workshop/setup.hpp` author_pane_place author_pane_size author_pane_window PanePlace ui_place_of
`workshop/screen.hpp` canvas_of_room room_of_canvas
`workshop/screen_chrome.cpp` project_pane pane_part
`ui/vocabulary.hpp` place_anchor_x place_anchor_y
`tests/test_workshop_screen.cpp` "an anchored pane keeps its corner where its word puts it, at every
size of the window" "a centred pane stands in the middle of the room, and a fill runs to the room's
far edge"
`workshop/screen_pane_subject.cpp` pane_window_base
`tests/test_workshop_panes_window.cpp` "an authored place is measured from the room's top-left, not
an offset from the default" "each axis is independent -- a place edit freezes no size, and back" "a
default width still takes half the surplus after a place edit"
`tests/test_workshop_host.cpp` "a typed value that is not admissible is refused, and the authored
row is untouched"

### WL-PANE-12
`workshop/setup_reconcile.hpp` inventory_rows CatalogRow
`workshop/weave_desktop.cpp` launch_pane
`workshop/layouts/store.cpp` close
`workshop/weave_arrange.cpp` arrangeable
`workshop/panes.hpp` kNoPaneKind
`tests/test_workshop_screen.cpp` "the close door can reach and remove an unresolved row"
`tests/test_workshop_panes_window.cpp` "participation stays the doors'; arrangement does not add or
offer"

### WL-PANE-17
`workshop/setup_reconcile.hpp` preferred_extent fit_extent fit_offer within_a_pixel offered_extent admit_pane_offer PaneBody
`workshop/panes.hpp` PaneOfferedBody RuntimePane::latest RuntimePane::standing
`workshop/screen_pane_state.cpp` pane_fit_note
`pane/vocabulary.hpp` kMaxPaneBodyPx
`workshop/screen.hpp` stack_capacity
`workshop/screen_chrome.cpp` project_pane bounds_of pane_measure
`view/view.hpp` offered
`tests/test_workshop_demo.cpp` "pane comfort budgets body text with chrome in both real medium
metrics" "pane comfort survives offer refresh and rejects malformed preferences" "pane comfort: the
column spends each pane's preferred height, and begins again at its top when spent" "pane comfort:
`fit` follows a pane switching between two sizes each time, in both windows, and `default` keeps
the first" "pane comfort: a re-offer within one pixel of what `fit` stands on is held in the GUI window
alone, and the TUI window follows it" "pane comfort: a `fit` axis says what it stands on where that is not the pane's latest offer"
`tests/test_workshop_panes_canvas.cpp` "a view asks for its size in pixels and its pane grants
exactly that room: to the pixel in a window, to the cells that hold it in a terminal"
