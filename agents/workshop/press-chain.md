# Workshop law — the press chain

Register `WL-PRESS`: what a routing bool means, where the body is resolved, and the pointer
order. One law per heading; cite by ID. Router: [`../workshop.md`](../workshop.md).

## WL-PRESS-01 — A press-chain bool means CONSUMED

LAW — The handlers under `if (b.pressed)` answer one routing question — true: consumed, stop; false: not mine, carry on — and a consumed press need not change anything at all.

MEANS
- it only has to have reached the layer that owns what the press means;
- consumed and not-consumed are told apart by where, never by what changed.

DOES NOT MEAN
- that a `Disposition`, `InteractionResult` or target enum is wanted on the routing path.

MEANS
- ⭐ every built-in's arm is gone; a pane's press says whether it named a row.

WHY — `agents/decisions/a-routing-bool-is-not-a-disposition.md`

## WL-PRESS-02 — A bool that is not the chain's is not unified with it

LAW — `layouts_press`'s bool is "this was a tab" and is not unified with the chain's CONSUMED: one shape, two questions, told apart by name.

MEANS
- ⭐ the deliberate `false` this law was written over is gone with `objects_press`;
- so is `terminal_press`'s "a repaint is owed", which left with the overlay.

WHY — `agents/decisions/a-routing-bool-is-not-a-disposition.md`

## WL-PRESS-03 — A pane's own inverse answers WHERE and nothing about meaning

LAW — A resolved owner's inverse is asked once per press, beside the canvas point, and answers where it landed in that pane's body — never what it means; a decline changes nothing, so holding it is sound.

MEANS
- ⭐ this law was `info_body_at`'s, then `pane_editor_at`'s; every inverse left is a pane's;
- an external pane's is asked before the press writes the keyboard, which moves a hidden title.

WHY — `agents/decisions/a-routing-bool-is-not-a-disposition.md`

## WL-PRESS-04 — Nothing asks a geometry question above occupancy

LAW — The order is arrangement, the open contextual surface, then pane occupancy over `effective_pane_order`, then the resolved pane's own inverse, then the bare room.

MEANS
- a new pane-internal gesture belongs in the resolved-owner arm, never above the walk;
- a pane in front of another takes the press, whichever of them owns a control at that cell;
- in a graphics window the chain answers Workshop's own area (`on(AreaButtonChanged)`): a pane's area is its own, a moment over it never relayed (`placed_in_window`).

WHY — `agents/decisions/a-routing-bool-is-not-a-disposition.md`

## WL-PRESS-05 — `band_tab_at` is the Layouts pane's local inverse

LAW — The tab inverse is spent only once occupancy has named the Layouts pane, and its spans come from the run's own composition against the pane's body, so no unpainted tab is ever answered.

WHY — `agents/decisions/a-routing-bool-is-not-a-disposition.md`

## WL-PRESS-06 — A secondary press is the pane's first, then state-local first refusal

LAW — A secondary press in a pane's area reaches it from the window, held per button, Workshop keeping its continuation; a doorless pane's right press opens the host's menu; chrome and modes are the host's.

MEANS
- the release is the hold's area's wherever the pointer is, and the window's word ends it lost; a pass-back cites the window's number;
- a continuation is eligible while its hand's newest, unspent, its pane on the desk; a release is no act;
- arrangement leaves on one, consumed whole.

DOES NOT MEAN
- that a replaced holder is promised its release: the window addresses the area's office, not an incarnation;
- that the host sees what the pane made of the press: only a pass-back or a menu asked says.

WHY — `agents/decisions/a-routing-bool-is-not-a-disposition.md`

## WL-PRESS-07 — A press acts on the element it landed on

LAW — A press acts on the element the window drew under the hand, over its whole face, and is refused in the pane's own words only where its newest drawing shows another element there, or none.

MEANS
- the window says what it drew there: the area, the part named there, that area's drawing number;
- the same element is the part a pane names across its redraws; unnamed, what the row means, by its number;
- the same element redrawn -- words, colour, hover, a row elsewhere moved -- takes it; a drop is judged likewise.

DOES NOT MEAN
- that a press waits on a drawing, or that two offices' messages arrive in one order;
- that a pane naming nothing and numbering nothing is guarded: it reads a press against its newest drawing.

WHY — `agents/decisions/a-press-names-the-element-it-landed-on.md`

## Do not assume

- That the bare bool is inadequate — the richer answers (`Written`, `Handled`, `Commit`,
  `Availability`, `Occupancy`) all live on semantic paths; only the routing path is a bool.

## Proven by

### WL-PRESS-01
`workshop/weave_pointer.cpp` pointer_button
`workshop/weave_handlers.cpp` area_pressed
`tests/test_workshop_panes_input.cpp` "management chrome gets first refusal, and a mode takes the
press whole"
`tests/test_workshop_screen.cpp` "a press that lands on a pane begins nothing, so a hand that leaves
it drags nothing"

### WL-PRESS-02
`workshop/weave_pointer.cpp` layouts_press
`tests/test_workshop_panes_input.cpp` "management chrome gets first refusal, and a mode takes the
press whole"

### WL-PRESS-03
`workshop/screen.hpp` ProseAt
`workshop/screen_external.cpp` external_press_at
`workshop/weave_pointer.cpp` pointer_button
`tests/test_workshop_panes_input.cpp` "a press in the body names the row under the header, in both
media"
`tests/test_workshop_panes_files.cpp` "with pane titles hidden, a first press on the row painted
gamma selects gamma once every delivery it caused has settled, and a later press opens gamma"

### WL-PRESS-04
`workshop/weave_pointer.cpp` pointer_button
`workshop/screen_chrome.cpp` occupied_at
`workshop/screen.hpp` Occupancy::kind ExternalPressAt kNoKind
`workshop/screen_external.cpp` external_press_at
`tests/test_workshop_screen.cpp` "a pane in front of the Layouts pane takes the press"
`tests/test_workshop_panes_window.cpp` "outside arrangement, an addressed pane behind another clicks
through nothing"
`tests/test_workshop_panes_input.cpp` "management chrome gets first refusal, and a mode takes the
press whole"
`workshop/weave_handlers.cpp` on(AreaButtonChanged) on(AreaPressed)
`tests/test_workshop_desk.cpp` "a press the graphics window gives Workshop's own area is the press it is, held to its own
release, and one it gives a pane's area is the pane's, Workshop told where it landed" "Workshop's own area of the window answers each button as itself: a press on the room says
so, a right press asks its menu about the room or a tab, re-asking it while one is open, a press
on the gear anchors there, and a tab carried along the run reorders it"

### WL-PRESS-07
`graphics/vocabulary.hpp` AreaButtonChanged Drawn
`graphics/canvas.hpp` part_at
`graphics/window/window.hpp` say_under
`workshop/pane_drawing.hpp` press_moved WindowDrawing::moved WindowPointer
`builder/builder-pane/pane.cpp` press
`editor/editor-pane/transfers.hpp` on_window
`tests/test_workshop_panes_canvas.cpp` "a pane's drawings judge a press by what the window drew
under the hand: a named part redrawn takes it over its whole face, another part or none refuses it,
and an unnamed place takes it only under the drawing's number"
`tests/test_graphics.cpp` "a button's moment says what the window drew under the hand when it
landed -- the area, the part named there and its drawer's number for the drawing -- and a press held
through its owner's next drawing keeps its number for its moves and its release, wherever it lands"
"a pane on the drawing facet is pressed over a drawn button's whole face, its edge included, and a
press the window landed on another part than it shows there is refused"
`tests/test_workshop_panes_desktop.cpp` "WL-DESK-14: a press queued behind a change of the list is
refused, never resolved against the moved rows"
`tests/test_workshop_panes_input.cpp` "a Loaded press the window landed before the wheel moved its
rows selects nothing"
`tests/test_workshop_panes_files.cpp` "a press the window landed on a Files row its listing has since
moved is refused in words and spends nothing, and one landed on the same entry redrawn takes"
`tests/test_workshop_inventory_info.cpp` "a value released on Flow before its carry is answered
carries the window's word from its release, though Flow repainted meanwhile"

### WL-PRESS-05
`workshop/screen_chrome.cpp` occupied_at
`workshop/screen_layouts.cpp` band_tab_at band_status layouts_body
`workshop/panes.hpp` kLayouts
`workshop/weave_pointer.cpp` layouts_press
`tests/test_workshop_screen.cpp` "a press answers a painted tab and nothing else on the band" "an
omitted tab has no span and cannot be pressed" "no press outside the painted run reaches a layout"

### WL-PRESS-06
`workshop/pane_vocabulary.hpp` PanePassRequested
`workshop/weave.hpp` SecondaryContinuation end_lost_holds
`workshop/weave_external.cpp` end_lost_holds
`workshop/weave_handlers.cpp` on(AreaHoldEnded)
`graphics/vocabulary.hpp` AreaHoldEnded
`tests/test_workshop_desk.cpp` "a press Workshop holds in its own area of the window ends when the
window says it ended the hold, and not before: an area Workshop places while it holds is the
window's to judge"
`workshop/weave_code.cpp` on(DispatchRefused)
`workshop/weave_pointer.cpp` pointer_button
`workshop/weave_session.cpp` apply_setup
`workshop/weave_arrange.cpp` enter_arrange_pane
`tests/test_workshop_panes_button.cpp` "WL-PRESS-06: a right press over a pane whose holder hears
the window is delivered, consumed and opens no menu; the chrome stays the host's" "WL-PRESS-06: a right press in a doorless pane's body opens the host's pane menu at the
press, sends the provider nothing and takes no keys; its chrome opens the same menu" "WL-PRESS-06:
the release is the pressing pane's wherever the pointer is, and leaks into no other pane"
"WL-PRESS-06: closing the pane AFTER the release invalidates the continuation on its own -- the
review's first integration finding" "WL-PRESS-06: a press of a button the host believes is down ends
the old hold aloud, never silently" "WL-PRESS-06: a holder replaced while the button is down -- the
release goes to the ROLE's current holder, who never saw the press; the old holder is told
nothing"
`tests/test_workshop_host.cpp` "one right press exits Arrange; only the NEXT one opens context" "a
mode that owns the pointer answers a right press its own way"
`workshop/weave_handlers.cpp` on(AreaPressed)
`tests/test_workshop_desk.cpp` "a press in a pane's area of the window is the pane's, and under the window's number Workshop
tells it where the keys were, hands its press back, opens its menu and carries what it drags out
or picks up"
