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

PROVEN BY — `workshop/weave_pointer.cpp` `on(PointerButton)`; `workshop/weave_external.cpp`
`external_press`; `tests/test_workshop_panes_input.cpp` case `"SEL-0: management chrome gets
first refusal, and a mode takes the press whole"`; `tests/test_workshop_screen.cpp` case `"a
press that lands on a panel begins nothing, so a hand that leaves it drags nothing"`.
WHY — `agents/decisions/a-routing-bool-is-not-a-disposition.md`

## WL-PRESS-02 — A bool that is not the chain's is not unified with it

LAW — `layouts_press`'s bool is "this was a tab" and is not unified with the chain's CONSUMED: one shape, two questions, told apart by name.

MEANS
- ⭐ the deliberate `false` this law was written over is gone with `objects_press`;
- so is `terminal_press`'s "a repaint is owed", which left with the overlay (VD-24).

PROVEN BY — `workshop/weave_pointer.cpp` `layouts_press`;
`tests/test_workshop_panes_input.cpp` case `"SEL-0: management chrome gets first refusal, and a
mode takes the press whole"`.
WHY — `agents/decisions/a-routing-bool-is-not-a-disposition.md`

## WL-PRESS-03 — A pane's own inverse answers WHERE and nothing about meaning

LAW — A resolved owner's inverse is asked once per press, beside the canvas point, and answers where it landed in that pane's body — never what it means; a decline changes nothing, so holding it is sound.

MEANS
- ⭐ this law was `info_body_at`'s, then `pane_editor_at`'s; every inverse left is a pane's;
- an external pane's is asked before the press writes the keyboard, which moves a hidden title.

PROVEN BY — `workshop/screen.hpp` `ProseAt`; `workshop/screen_external.cpp`
`external_press_at`; `workshop/weave_pointer.cpp` `on(PointerButton)`;
`tests/test_workshop_panes_input.cpp` case `"SEL-0: a press in the body names the row under the
header, in both media"`; `tests/test_workshop_panes_files.cpp` case `"with pane titles hidden, a
first press on the row painted gamma selects gamma once every delivery it caused has settled,
and a later press opens gamma"`.
WHY — `agents/decisions/a-routing-bool-is-not-a-disposition.md`

## WL-PRESS-04 — Nothing asks a geometry question above occupancy

LAW — The order is arrangement, the open contextual surface, then pane occupancy over `effective_pane_order`, then the resolved pane's own inverse, then the bare room.

MEANS
- a new pane-internal gesture belongs in the resolved-owner arm, never above the walk;
- a pane in front of another takes the press, whichever of them owns a control at that cell.

PROVEN BY — `workshop/weave_pointer.cpp` `on(PointerButton)`; `workshop/weave_external.cpp`
`external_press`; `workshop/screen_chrome.cpp` `occupied_at`; `workshop/screen.hpp`
`Occupancy::kind`, `ExternalPressAt`, `kNoKind`; `workshop/screen_external.cpp`
`external_press_at`; `tests/test_workshop_screen.cpp` case `"WUX-12/SC-5+SC-7: a pane in front
of the Layouts pane takes the press"`, case `"WIND-2: outside arrangement, an addressed pane
behind another clicks through nothing"`; `tests/test_workshop_panes_input.cpp` case `"SEL-0:
management chrome gets first refusal, and a mode takes the press whole"`.
WHY — `agents/decisions/a-routing-bool-is-not-a-disposition.md`

## WL-PRESS-05 — `band_tab_at` is the Layouts pane's local inverse

LAW — The tab inverse is spent only once occupancy has named the Layouts pane, and its spans come from the run's own composition against the pane's body, so no unpainted tab is ever answered.

PROVEN BY — `workshop/screen_chrome.cpp` `occupied_at`; `workshop/screen_layouts.cpp`
`band_tab_at`, `band_status`, `layouts_body`; `workshop/panel.hpp` `kLayouts`;
`workshop/weave_pointer.cpp` `layouts_press`; `tests/test_workshop_screen.cpp` case
`"WUX-9/SC-9: a press answers a painted tab and nothing else on the band"`, case
`"WUX-9/SC-8+SC-9: an omitted tab has no span and cannot be pressed"`, case `"QR-14/SC-5: no
press outside the painted run reaches a layout"`.
WHY — `agents/decisions/a-routing-bool-is-not-a-disposition.md`

## WL-PRESS-06 — A secondary press is the pane's first, then state-local first refusal

LAW — A secondary press in a pane's body reaches a `PaneButton` holder, consumed under a hold and a continuation per button; a doorless body is empty; chrome and modes stay the host's.

MEANS
- the release is the hold's pane's wherever the pointer is; loss and arbitration end it `lost`;
- a continuation is eligible while newest, unspent, its pane on the desk, only its release since;
- arrangement leaves on one, consumed whole; the Terminal still means nothing by it.

DOES NOT MEAN
- that a refused press holds: the tap attributes it; the host drops it; the release is silent;
- that a replaced holder is promised its release: the host addresses the role, not an incarnation.

PROVEN BY — `workshop/pane_vocabulary.hpp` `PaneButton`, `PanePassRequested`;
`workshop/weave.hpp` `SecondaryHold`, `SecondaryContinuation`, `external_button`,
`external_release`, `end_lost_holds`, `end_refused_button`; `workshop/weave_external.cpp`
`external_button`, `external_release`, `end_lost_holds`, `end_refused_button`;
`workshop/weave_code.cpp` `on(DispatchRefused)`; `workshop/weave_pointer.cpp` `on(PointerButton)`;
`workshop/weave_session.cpp` `apply_setup`; `workshop/pane_menu.hpp` `HeldButton`;
`workshop/weave_arrange.cpp` `enter_arrange_pane`; `tests/test_workshop_panes_button.cpp` case
`"WL-PRESS-06: a right press over a pane whose holder has the door is delivered, consumed, opens
no menu, and names the admitted picture; the chrome stays the host's"`, case `"WL-PRESS-06: a
doorless pane's body is empty by default -- a right press there opens no menu and takes no keys;
its chrome still opens the host's menu"`, case `"WL-PRESS-06: the
release is the pressing pane's wherever the pointer is, and leaks into no other pane"`, case
`"WL-PRESS-06: closing the pane AFTER the release invalidates the continuation on its own -- the
review's first integration finding"`, case `"WL-PRESS-06: a press of a button the host believes
is down ends the old hold aloud, never silently"`, case `"WL-PRESS-06: a press Loom refuses is
settled -- the custody it recorded is dropped, so the physical release sends nothing and no
second refusal follows; the failure stands on the tap"`;
`tests/test_workshop_panels.cpp` case `"ARR-0/SC-7: one right
press exits Arrange; only the NEXT one opens context"`, case `"CTX-0/ARR-0: a mode that owns the
pointer answers a right press its own way"`.
WHY — `agents/decisions/a-routing-bool-is-not-a-disposition.md`

## Do not assume

- That the bare bool is inadequate — the richer answers (`Written`, `Handled`, `Commit`,
  `Availability`, `Occupancy`) all live on semantic paths; only the routing path is a bool.
