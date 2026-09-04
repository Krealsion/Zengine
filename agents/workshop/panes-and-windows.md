# Workshop law — panes and windows

Register `WL-PANE`: the three places, the overlay slots, the default a maker lays an override
over, and the seven states. One law per heading; cite by ID. Router:
[`../workshop.md`](../workshop.md). What crosses the pane seam is the protocol's law, in
[`../panes.md`](../panes.md); this register holds Workshop's side only.

## WL-PANE-01 — Three places, and only one of them is the screen's

LAW — Three places: the side region is the screen's, place-fixed with no override; the overlay stack and the top band are the maker's. One predicate is that exclusion, and every consumer asks it.

MEANS
- `project_pane`'s gate, `take_pane_hold` and `paint_pane_affordances` spend one sentence;
- `kinds_placed_in` pins the side region and the top band at one kind each, at compile time.

PROVEN BY — `workshop/panel.hpp` `place_is_authorable`, `kSideRegion`, `kOverlayStack`,
`kTopBand`, `kinds_placed_in`, `placement`, `kPanelCatalog`, `placement_of`; `workshop/screen.hpp`
`project_pane`, `paint_pane_affordances`; `workshop/weave.hpp` `take_pane_hold`;
`tests/test_workshop_panels.cpp` case `"a panel kind declares its place, and the place resolves to
bounds"`; `tests/test_workshop_screen.cpp` case `"WUX-12/SC-3: authored geometry moves the Layouts
pane, and the tabs with it"`.
WHY — `agents/decisions/three-places.md`

## WL-PANE-03 — A band-anchored or authored pane spends no reactive slot

LAW — A band-anchored pane takes no stack slot, and an authored place spends no reactive slot and cannot wait for one; `waiting` means only that the reactive default ran out of tiles.

MEANS
- both `seat_panes` and `bounds_of`'s slot counter, `stack_slots_that_fit`, say it;
- an oversubscribed authored setup keeps the extra reference, waiting for room.

PROVEN BY — `workshop/setup.hpp` `seat_panes`, `Reconciled::waiting`, `StackCapacity`,
`Seating`; `workshop/screen.hpp` `bounds_of`, `stack_slots_that_fit`; `workshop/panel.hpp`
`Panels::waiting_for_room`; `tests/test_workshop_panes_window.cpp` case `"WIND-2: an authored
place spends no reactive slot, and cannot wait for one"`; `tests/test_workshop_panes_seam.cpp`
case `"an oversubscribed authored setup keeps the extra reference, waiting for room"`.
WHY — `agents/decisions/three-places.md`

## WL-PANE-04 — A wider room is shared by the pane and the maker

LAW — An overlay slot is `kStackW + (room_w - kStackW)/2` wide, floored — the minimum's 48 plus half the room's surplus — while its column, row, height and gap are untouched.

MEANS
- at 79 columns the surplus is one and the odd column stays the maker's;
- a width edit never buys a slot: `stack_slots_that_fit` reads `y` and `h` only.

PROVEN BY — `workshop/screen.hpp` `placement_bounds`, `kStackW`, `stack_slots_that_fit`,
`kStackX`; `tests/test_workshop_panels.cpp` case `"WIND-1: the side region keeps its reservation
and the stack takes half the surplus"`, case `"WIND-1: the minimum composition is byte-identical,
and a width buys no slot"`.
WHY — `agents/decisions/half-the-surplus.md`

## WL-PANE-05 — Every cell a slot gains is paint and pointer alike

LAW — The frame painter fills the whole rectangle, occupancy owns all of it, and a press inside it is answered with the panel's sentence rather than reaching the workspace.

MEANS
- a drag begun on the workspace still walks under the panel and releases normally;
- `room_w > kStackW` implies `x + w < room_w`: columns of the panel's rows stay reachable.

PROVEN BY — `workshop/screen.hpp` `paint_panel_frame`, `occupied_at`, `take_hold`, `kNoKind`,
`Occupancy::what`; `workshop/weave.hpp` `on(PointerMoved)`; `tests/test_workshop_screen.cpp` case
`"WIND-1: the columns the panel took are its own, and the band is the maker's"`, case `"a gesture
that began on the workspace is not interrupted by a panel"`, case `"a visible panel occupies the
pointer space it covers"`.
WHY — `agents/decisions/half-the-surplus.md`

## WL-PANE-06 — An external pane's room follows its slot

LAW — Workshop's body for an external pane is its slot less its header rows, and the fitted room over that body is granted to the provider whenever the body changes.

MEANS
- a dragged edge, a widened room and a hidden title all reach the provider by this one door;
- what a grant carries, and that an unchanged capacity sends none, is the protocol's law.

PROVEN BY — `workshop/screen.hpp` `external_body_place`, `kExternalHeaderRows`,
`paint_external`; `workshop/weave.hpp` `refresh_external_rooms`; `surface/region.hpp`
`fit_region`; `workshop/panel.hpp` `ExternalPane`; `tests/test_workshop_panes_seam.cpp` case
`"WIND-1: an external grant follows the widened body through fit_region"`, case `"opening an
external pane grants exactly the fit_region room, authored as Workshop"`.
WHY — `agents/decisions/half-the-surplus.md`

## WL-PANE-07 — `panels.open` is never reordered

LAW — The open list is seated by walking the setup list in the setup's order, and a reactive slot is counted over that same list; no ordering operation writes what seating and slot-counting read.

MEANS
- that is the whole of "raising a pane cannot move it";
- ordering changes paint order and nothing else.

PROVEN BY — `workshop/setup.hpp` `seat_panes`, `reconcile`, `Reconciled`, `Setup`, `add_pane`,
`Seating`; `workshop/screen.hpp` `bounds_of`; `workshop/panel.hpp` `Panels::open`;
`tests/test_workshop_panes_window.cpp` case `"WIND-2: ordering changes paint order and NOTHING
else"`; `tests/test_workshop_persistence.cpp` case `"reconciling opens what the setup names, in
the setup's order"`.
WHY — `agents/decisions/front-is-a-permutation.md`

## WL-PANE-08 — The override is spent in the overlay stack and the top band only

LAW — A side-region row's authored geometry is retained in the file, never rewritten and never spent, and arrangement refuses to author one, naming the reservation it hit.

MEANS
- a movable Info would change the resolved size of objects in a maker's document;
- the Layouts pane's authored geometry is spent: it moves, and its tabs with it.

PROVEN BY — `workshop/screen.hpp` `project_pane`, `PaneProjection`, `pane_geometry_typeable`;
`workshop/panel.hpp` `place_is_authorable`; `workshop/weave.hpp` `arrange_geometry_ready`;
`tests/test_workshop_screen.cpp` case `"WUX-12/SC-3: authored geometry moves the Layouts pane, and
the tabs with it"`; `tests/test_workshop_panes_window.cpp` case `"WIND-2: a pixel axis is
setup-valid, projection-refused, and never falls back"`.
WHY — `agents/decisions/three-places.md`

## WL-PANE-09 — The host clips and never rewrites

LAW — `bounds_of` answers the visible rectangle — resolved, then intersected with the canvas — and `PanelBounds::resolved` carries the unclipped ask; an off-room pane is recoverable.

MEANS
- every consumer that reads an empty rectangle as "nowhere" is correct for an off-room pane;
- a wholly off-room pane is painted by nobody and its intent is not rewritten.

PROVEN BY — `workshop/screen.hpp` `bounds_of`, `PanelBounds`, `PanelBounds::rect`,
`PaneProjection`; `tests/test_workshop_panes_window.cpp` case `"WIND-2: a partly off-room pane is
clipped, and its intent is not rewritten"`, case `"WIND-2: a wholly off-room pane is off-room,
recoverable, and painted by nobody"`.
WHY — `agents/decisions/three-places.md`

## WL-PANE-10 — Seven states, one classifier, one precedence

LAW — `closed`, `unresolved`, `refused`, `waiting`, `off-room`, `covered`, `open`: a unit outranks a want of room, `covered` is coverage by the union of what is in front, one visible cell is `open`.

MEANS
- a pane with a pixel axis and no tile left is `refused`, because a taller window would not help;
- two panes that each cover half of a third leave nothing of it showing;
- the state column is eleven cells (`kPaneStateCols`), because `unresolved` is ten bytes.

PROVEN BY — `workshop/screen.hpp` `pane_state_of`, `pane_state`, `pane_state_word`,
`kPaneStateCols`, `PaneProjection`, `pane_state_remedy`, `pane_is_covered`; `workshop/weave.hpp`
`unresolved_note`; `workshop/panel.hpp` `Panels::waiting_for_room`;
`tests/test_workshop_panes_window.cpp` case `"WIND-2: a refused pane is refused rather than
waiting, and it still SEATS"`, case `"WIND-2: two panes that each cover HALF of a third leave
nothing of it showing"`, case `"WIND-2: coverage is the UNION of what is in front, not containment
by one pane"`.
WHY — `agents/decisions/three-places.md`

## WL-PANE-11 — An authored place is absolute, and each axis is independent

LAW — An authored place is an absolute canvas position on the fine lattice, never an offset from the default; a place edit freezes no size, and a default width still follows the half-share.

PROVEN BY — `workshop/setup.hpp` `author_pane_place`, `author_pane_size`, `PanePlace`;
`workshop/screen.hpp` `project_pane`; `tests/test_workshop_panes_window.cpp` case `"WIND-2: an
authored place is absolute canvas position, not an offset from the default"`, case `"WIND-2: each
axis is independent -- a place edit freezes no size, and back"`, case `"WIND-2: a default width
still follows the WIND-1 half-share after a place edit"`.
WHY — `agents/decisions/setup-format-v3.md`

## WL-PANE-12 — The picker keeps presence, and arrangement never touches it

LAW — `inventory_rows` is the catalog union every reference the setup names, `picker_population()` is the one inventory every picker consumer spends, and selecting an open row removes it.

MEANS
- an unresolved row carries `kNoPaneKind`, so nothing can present it as the Builder;
- arrangement binds no toggle, adds nothing and offers nothing.

PROVEN BY — `workshop/setup.hpp` `inventory_rows`, `CatalogRow`; `workshop/weave.hpp`
`picker_population`, `choose_panel`, `command`, `picker_move`, `toggle_participation`,
`arrangeable`; `workshop/panel.hpp` `kNoPaneKind`; `tests/test_workshop_screen.cpp` case
`"WIND-2a: the picker can reach and remove an unresolved row"`;
`tests/test_workshop_panes_window.cpp` case `"ARR-0: participation stays the picker's; arrangement
does not add or offer"`; `tests/test_workshop_panels.cpp` case `"selecting an open kind REMOVES
it, and says what was not touched"`.
WHY — `agents/decisions/three-places.md`

## WL-PANE-13 — A panel is a kind and nothing else, and a kind has one instance

LAW — An open panel carries a kind and nothing else; a kind is open once or not at all, per-kind view state lives beside the stack, and selecting an open kind removes it through the same door.

MEANS
- a second copy of a tool's status in each instance would need a policy about several instances;
- removing a panel touches nothing behind it: no message reaches the office.

PROVEN BY — `workshop/panel.hpp` `Panel`, `Panels::has`, `open_panel`, `close_panel`;
`tests/test_workshop_panels.cpp` case `"selecting an open kind REMOVES it, and says what was not
touched"`, case `"a panel opens from the picker, is removed, and opens again"`.
WHY — `agents/decisions/three-places.md`

## WL-PANE-14 — The picker is a mode, not a panel

LAW — The `+ panel` picker has no instance, no catalog row and nothing presents it: opening it opens nothing, choosing closes it, and Escape or its own key dismisses it having opened and removed nothing.

MEANS
- it cannot be opened from itself; its name is the words on the hint that opens it.

PROVEN BY — `workshop/panel.hpp` `PanelPicker`, `kPickerName`; `tests/test_workshop_panels.cpp`
case `"a panel opens from the picker, is removed, and opens again"`, case `"the picker can be
dismissed without opening anything, two ways"`.
WHY — `agents/decisions/three-places.md`

## WL-PANE-15 — The picker covers the whole first slot

LAW — The picker opens over the stack's first slot and paints a whole panel's worth of rows whatever it lists, so nothing shows through it, and while it is open a press on that slot is the picker's.

MEANS
- three rows over a nine-row panel left six rows of another panel reading as one box: measured.

PROVEN BY — `workshop/screen.hpp` `kPickerRows`, `picker_bounds`;
`tests/test_workshop_screen.cpp` case `"the picker occupies the slot it opens over, and answers
for it while it is there"`.
WHY — `agents/decisions/three-places.md`

## WL-PANE-16 — A pane with a room and no answer says waiting, never unavailable

LAW — A pane whose room was granted and answered by nothing valid says `kExternalWaiting`: a fact about this panel, never about the provider; `unavailable` is never said, because silence proves no fate.

MEANS
- Loom gives Workshop no participant-visible unload notification;
- an unload is said as waiting, and a reload recovers the view.

PROVEN BY — `workshop/screen.hpp` `kExternalWaiting`; `tests/test_workshop_panes_seam.cpp` case
`"silence is waiting, and Workshop never says unavailable"`; `tests/test_workshop_screen.cpp` case
`"INTR-0: unload and reload -- waiting is said, and a reload recovers the view"`.
WHY — `agents/decisions/a-presentation-owns-no-facts.md`

## Do not assume

- That docking exists — it is absent and refused.
- That `kinds_placed_in` has a runtime witness — its pins are compile-time only (WL-PANE-01).
