# Workshop law — panes and windows

Register `WL-PANE`: the three places, the overlay slots, the default a weaver lays an override
over, and the seven states. One law per heading; cite by ID. Router:
[`../workshop.md`](../workshop.md). What crosses the pane seam is the protocol's law, in
[`../panes.md`](../panes.md); this register holds Workshop's side only.

## WL-PANE-01 — Three places, and every one of them is the weaver's

LAW — Three places: the right column, the overlay stack and the top band. Each is a named rectangle a pane resolves into, none is reserved out of the room, and an authored override is spent in all three.

MEANS
- `project_pane` lays an override over whatever rectangle a place answered, for every place;
- `kinds_placed_in` pins the right column and the top band at one catalog DEFAULT each.

DOES NOT MEAN
- that two panes may not stand in one place — a desk that says so gets what it asked for.

PROVEN BY — `workshop/panes.hpp` `kSideRegion`, `kOverlayStack`,
`kTopBand`, `kinds_placed_in`, `placement`, `kBuiltinPanes`, `placement_of`;
`workshop/screen_chrome.cpp` `project_pane`; `workshop/screen_reveal.cpp`
`paint_pane_affordances`; `workshop/weave_arrange.cpp` `take_pane_hold`;
`tests/test_workshop_host.cpp` case `"a pane kind declares its place, and the place resolves to
bounds"`; `tests/test_workshop_screen.cpp` case `"authored geometry moves the Layouts pane, and
the tabs with it"`.
WHY — `agents/decisions/the-room-is-the-screen.md`

## WL-PANE-03 — A band-anchored or authored pane spends no reactive slot

LAW — A band-anchored pane takes no stack slot, and an authored place spends no reactive slot and cannot wait for one; `waiting` means that the reactive stack lacks height for the next pane.

MEANS
- both `seat_panes` and `bounds_of` spend each preferred height (or the fallback) plus the gap;
- an oversubscribed authored setup keeps the extra reference, waiting for room.

PROVEN BY — `workshop/setup.hpp` `seat_panes`, `Reconciled::waiting`, `StackCapacity`,
`Seating`; `workshop/screen_chrome.cpp` `bounds_of`; `workshop/screen.hpp` `stack_slots_that_fit`;
`workshop/panes.hpp` `Panes::waiting_for_room`; `tests/test_workshop_panes_window.cpp` case
`"an authored place spends no reactive slot, and cannot wait for one"`;
`tests/test_workshop_panes_seam.cpp` case `"an oversubscribed authored setup keeps the extra
reference, waiting for room"`.
WHY — `agents/decisions/three-places.md`

## WL-PANE-04 — A wider room is shared by the pane and the weaver

LAW — Without a preferred size, an overlay slot is `kStackW + (room_w - kStackW)/2` wide, floored — the minimum's 48 plus half the room's surplus — while its column, row, height and gap are untouched.

MEANS
- at 79 columns the surplus is one and the odd column stays the weaver's;
- a width edit never buys a slot: `stack_slots_that_fit` reads `y` and `h` only.

PROVEN BY — `workshop/screen.hpp` `placement_bounds`, `kStackW`, `stack_slots_that_fit`,
`kStackX`; `tests/test_workshop_host.cpp` case `"the right column keeps its width and the stack
takes half the surplus"`, case `"the half-share pays at the bottom of the range too, and
buys no slot"`.
WHY — `agents/decisions/half-the-surplus.md`

## WL-PANE-05 — Every cell a slot gains is paint and pointer alike

LAW — The frame painter fills the whole rectangle, occupancy owns all of it, and a press inside it is answered with the pane's sentence rather than reaching the bare room.

MEANS
- a press on a pane begins nothing, so a hand that leaves it drags nothing;
- `room_w > kStackW` implies `x + w < room_w`: columns of the pane's rows stay reachable.

PROVEN BY — `workshop/screen_pane_state.cpp` `paint_pane_frame`; `workshop/screen_chrome.cpp`
`occupied_at`; `workshop/screen.hpp` `kNoKind`, `Occupancy::what`; `workshop/weave_pointer.cpp`
`on(PointerMoved)`; `tests/test_workshop_screen.cpp` case `"the columns the pane took are its
own, and the band is the weaver's"`, case `"a press that lands on a pane begins nothing,
so a hand that leaves it drags nothing"`, case `"a visible pane occupies the pointer space it
covers"`.
WHY — `agents/decisions/half-the-surplus.md`

## WL-PANE-06 — An external pane's room follows its slot

LAW — Workshop's body for an external pane is its slot less its header rows, and the fitted room over that body is granted to the provider whenever the body changes.

MEANS
- a dragged edge, a widened room and a hidden title all reach the provider by this one door;
- what a grant carries, and that an unchanged capacity sends none, is the protocol's law.

PROVEN BY — `workshop/screen_external.cpp` `external_body_place`, `paint_external`;
`workshop/screen.hpp` `kExternalHeaderRows`; `workshop/weave_external.cpp`
`refresh_external_rooms`; `surface/region.hpp` `fit_region`; `workshop/panes.hpp` `ExternalPane`;
`tests/test_workshop_panes_seam.cpp` case `"an external grant follows the widened body through
fit_region"`, case `"opening an external pane grants exactly the fit_region room, authored
as Workshop"`.
WHY — `agents/decisions/half-the-surplus.md`

## WL-PANE-07 — `panes.open` is never reordered

LAW — The open list is seated by walking the setup list in the setup's order, and a reactive slot is counted over that same list; no ordering operation writes what seating and slot-counting read.

MEANS
- that is the whole of "raising a pane cannot move it";
- ordering changes paint order and nothing else.

PROVEN BY — `workshop/setup.hpp` `seat_panes`, `reconcile`, `Reconciled`, `Setup`, `add_pane`,
`Seating`; `workshop/screen_chrome.cpp` `bounds_of`; `workshop/panes.hpp` `Panes::open`;
`tests/test_workshop_panes_window.cpp` case `"ordering changes paint order and NOTHING else"`;
`tests/test_workshop_persistence.cpp` case `"reconciling opens what the setup names, in
the setup's order"`.
WHY — `agents/decisions/front-is-a-permutation.md`

## WL-PANE-08 — The override is spent in every place, and the refusals are blind

LAW — An authored row's geometry is spent wherever the pane stands; every refusal `arrange_geometry_ready` still makes belongs to a pane with no rectangle, so none can be reached by pointing.

MEANS
- absent, unresolved, sized in pixels or off the screen — each is invisible as well as refused;
- the right column's pane is arranged by the keys and the hand that arrange every other.

DOES NOT MEAN
- that a refusal cannot be reached at all: a captured subject can be made unreachable after it.

PROVEN BY — `workshop/screen_chrome.cpp` `project_pane`; `workshop/screen.hpp` `PaneProjection`;
`workshop/screen_pane_subject.cpp` `pane_geometry_typeable`;
`workshop/weave_arrange.cpp` `arrange_geometry_ready`;
`tests/test_workshop_screen.cpp` case `"every pane a weaver can point at can be arranged, and the
refusals are blind"`; `tests/test_workshop_panes_window.cpp`
case `"a pixel axis is setup-valid, projection-refused, and never falls back"`.
WHY — `agents/decisions/the-room-is-the-screen.md`

## WL-PANE-09 — The host clips and never rewrites

LAW — `bounds_of` answers the visible rectangle — resolved, then intersected with the canvas — and `PaneBounds::resolved` carries the unclipped ask; an off-room pane is recoverable.

MEANS
- every consumer that reads an empty rectangle as "nowhere" is correct for an off-room pane;
- a wholly off-room pane is painted by nobody and its intent is not rewritten.

PROVEN BY — `workshop/screen_chrome.cpp` `bounds_of`; `workshop/screen.hpp` `PaneBounds`,
`PaneBounds::rect`, `PaneProjection`; `tests/test_workshop_panes_window.cpp` case `"a partly
off-room pane is clipped, and its intent is not rewritten"`, case `"a wholly off-room pane
is off-room, recoverable, and painted by nobody"`.
WHY — `agents/decisions/three-places.md`

## WL-PANE-10 — Seven states, one classifier, one precedence

LAW — `closed`, `unresolved`, `refused`, `waiting`, `off-room`, `covered`, `open`: a unit outranks a want of room, `covered` is coverage by the union of what is in front, one visible cell is `open`.

MEANS
- a pane with a pixel axis and no tile left is `refused`, because a taller window would not help;
- two panes that each cover half of a third leave nothing of it showing.

PROVEN BY — `workshop/screen_pane_state.cpp` `pane_state_of`, `pane_state_word`,
`pane_state_remedy`, `pane_is_covered`; `workshop/screen.hpp` `pane_state`,
`PaneProjection`; `workshop/weave_session.cpp` `unresolved_note`; `workshop/panes.hpp`
`Panes::waiting_for_room`; `tests/test_workshop_panes_window.cpp` case `"a refused pane is
refused rather than waiting, and it still SEATS"`, case `"two panes that each cover HALF of a
third leave nothing of it showing"`, case `"coverage is the UNION of what is in front,
not containment by one pane"`.
WHY — `agents/decisions/three-places.md`

## WL-PANE-11 — An authored place is absolute, and each axis is independent

LAW — An authored place is absolute on the fine lattice. A place edit freezes no size; each unauthored dimension follows the accepted preference or its placement fallback.

PROVEN BY — `workshop/setup.hpp` `author_pane_place`, `author_pane_size`, `PanePlace`;
`workshop/screen_chrome.cpp` `project_pane`; `tests/test_workshop_panes_window.cpp` case `"an
authored place is absolute canvas position, not an offset from the default"`, case `"each axis
is independent -- a place edit freezes no size, and back"`, case `"a default width still
takes half the surplus after a place edit"`.
WHY — `agents/decisions/setup-format-v3.md`

## WL-PANE-12 — Presence is the desk's, through two doors, and arrangement never touches it

LAW — `inventory_rows` is the catalog union every reference the setup names, the one list presence spends; a launch opens or focuses a row, and a close takes one off whether or not anything offers it.

MEANS
- an unresolved row carries `kNoPaneKind`, so nothing can present it as the Builder;
- arrangement binds no toggle, adds nothing and offers nothing.

PROVEN BY — `workshop/setup.hpp` `inventory_rows`, `CatalogRow`; `workshop/weave_desktop.cpp`
`launch_pane`, `close_pane`; `workshop/weave_arrange.cpp` `arrangeable`; `workshop/panes.hpp`
`kNoPaneKind`; `tests/test_workshop_screen.cpp` case `"the close door can reach and remove an
unresolved row"`; `tests/test_workshop_panes_window.cpp` case `"participation stays the
doors'; arrangement does not add or offer"`; `tests/test_workshop_panes_seam.cpp` case
`"closing a waiting row removes the intent, exactly as closing an open one does"`.
WHY — `agents/decisions/three-places.md`

## WL-PANE-13 — An open pane is a kind and nothing else, and a kind has one instance

LAW — An open pane carries a kind and nothing else; a kind is open once or not at all, per-kind view state lives beside the stack, and a close removes it through the one door.

MEANS
- a second copy of a tool's status in each instance would need a policy about several instances;
- removing a pane touches nothing behind it: no message reaches the office.

PROVEN BY — `workshop/panes.hpp` `OpenPane`, `Panes::has`, `open_kind`, `close_kind`;
`tests/test_workshop_host.cpp` case `"a built-in pane needs no weave, and opening one speaks to
no office"`; `tests/test_workshop_panes_actions.cpp` case `"WL-DESK-12: a close takes a pane off
the desk and leaves its provider holding; a close of a pane that is not there is refused and
opens nothing"`.
WHY — `agents/decisions/three-places.md`

## WL-PANE-14 — RETIRED: the picker was a mode, not a pane

WHY — `agents/decisions/three-places.md`

## WL-PANE-15 — RETIRED: the picker covered the whole first slot

WHY — `agents/decisions/three-places.md`

## WL-PANE-16 — A pane with a room and no answer says waiting, never unavailable

LAW — A pane whose room was granted and answered by nothing valid says `kExternalWaiting`: a fact about this pane, never about the provider; `unavailable` is never said, because silence proves no fate.

MEANS
- Loom gives Workshop no participant-visible unload notification;
- an unload is said as waiting, and a reload recovers the view.

PROVEN BY — `workshop/screen.hpp` `kExternalWaiting`; `tests/test_workshop_panes_seam.cpp` case
`"silence is waiting, and Workshop never says unavailable"`; `tests/test_workshop_screen.cpp` case
`"unload and reload -- waiting is said, and a reload recovers the view"`.
WHY — `agents/decisions/a-presentation-owns-no-facts.md`

## Do not assume

- That docking exists — it is absent and refused.
- That `kinds_placed_in` has a runtime witness — its pins are compile-time only (WL-PANE-01).
- That two panes cannot stand in one place — a desk may say so, and one covers the other
  (WL-PANE-01).

## WL-PANE-17 — Preferred body space is resolved by Workshop

LAW — A preferred size budgets body text plus title and chrome in the current medium, bounded by available workspace; an authored dimension wins, and seating and drawing spend the same default height.

MEANS
- a first accepted offer fixes the runtime preference; refresh cannot resize it;
- old providers retain their fallback, and small authored dimensions remain legal.

PROVEN BY — `workshop/setup.hpp` `preferred_extent`, `seat_panes`, `admit_pane_offer`;
`workshop/screen.hpp` `stack_capacity`; `workshop/screen_chrome.cpp` `project_pane`, `bounds_of`;
`tests/test_workshop_demo.cpp` case
`"pane comfort budgets body text with chrome in both real medium metrics"`,
case `"pane comfort survives offer refresh and rejects malformed preferences"`,
case `"pane comfort seating and drawing spend the same vertical space"`.
WHY — `agents/decisions/preferred-pane-space.md`
