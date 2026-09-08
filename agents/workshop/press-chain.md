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
- ⭐ its three handlers left with the Info panel; the Editor's arm answers now, and a pane's.

PROVEN BY — `workshop/weave_pointer.cpp` `on(PointerButton)`, `take_hold`;
`workshop/weave_editor.cpp` `editor_press`; `workshop/weave_pane_editor.cpp`
`pane_editor_press`; `tests/test_workshop_panes_input.cpp` case `"SEL-0: management chrome gets
first refusal, and a mode takes the press whole"`; `tests/test_workshop_screen.cpp` case `"a
press that lands on a panel begins nothing, so a hand that leaves it drags nothing"`.
WHY — `agents/decisions/a-routing-bool-is-not-a-disposition.md`

## WL-PRESS-02 — A bool that is not the chain's is not unified with it

LAW — `terminal_press`'s bool is "a repaint is owed" and `layouts_press`'s is "this was a tab", and neither is unified with the chain's CONSUMED: one shape, three questions, told apart by name.

MEANS
- ⭐ the deliberate `false` this law was written over is gone with `objects_press`.

PROVEN BY — `workshop/weave_terminal.cpp` `terminal_press`; `workshop/weave_pointer.cpp`
`repaint_needed`, `layouts_press`; `tests/test_workshop_panes_input.cpp` case `"SEL-0:
management chrome gets first refusal, and a mode takes the press whole"`.
WHY — `agents/decisions/a-routing-bool-is-not-a-disposition.md`

## WL-PRESS-03 — A pane's own inverse answers WHERE and nothing about meaning

LAW — A resolved owner's inverse is asked once per press, beside the canvas point, and answers where it landed in that pane's body — never what it means; a decline changes nothing, so holding it is sound.

MEANS
- ⭐ this law was `info_body_at`'s; `pane_editor_at` is the built-in that owns one now.

PROVEN BY — `workshop/screen_pane_editor.cpp` `pane_editor_at`, `pane_editor_body`;
`workshop/screen.hpp` `PaneEditorAt`, `PaneEditorAt::present`, `terminal_input_hit`, `ProseAt`;
`workshop/screen_external.cpp` `external_press_at`; `tests/test_workshop_panes_input.cpp` case
`"SEL-0: a press in the body names the row under the header, in both media"`.
WHY — `agents/decisions/a-routing-bool-is-not-a-disposition.md`

## WL-PRESS-04 — Nothing asks a geometry question above occupancy

LAW — The order is the terminal overlay, arrangement, the open contextual surface, then pane occupancy over `effective_pane_order`, then the resolved pane's own inverse, then the workspace.

MEANS
- a new pane-internal gesture belongs in the resolved-owner arm, never above the walk;
- a pane in front of another takes the press, whichever of them owns a control at that cell.

PROVEN BY — `workshop/weave_pointer.cpp` `take_hold`, `on(PointerButton)`;
`workshop/weave_external.cpp` `external_press`; `workshop/screen_chrome.cpp` `occupied_at`;
`workshop/screen.hpp` `Occupancy::kind`, `ExternalPressAt`, `kNoKind`;
`workshop/screen_external.cpp` `external_press_at`; `workshop/screen_gestures.cpp` `take_hold`;
`tests/test_workshop_screen.cpp` case `"WUX-12/SC-5+SC-7: a pane in front of the Layouts pane
takes the press"`, case `"WIND-2: outside arrangement, an addressed pane behind another clicks
through nothing"`; `tests/test_workshop_panes_input.cpp` case `"SEL-0: management chrome gets
first refusal, and a mode takes the press whole"`.
WHY — `agents/decisions/a-routing-bool-is-not-a-disposition.md`

## WL-PRESS-05 — `band_tab_at` is the Layouts pane's local inverse

LAW — The tab inverse is spent only once occupancy has named the Layouts pane, and its spans come from the run's own composition against the pane's body, so no unpainted tab is ever answered.

PROVEN BY — `workshop/screen_chrome.cpp` `occupied_at`; `workshop/screen_layouts.cpp`
`band_tab_at`, `band_status`, `layouts_body`; `workshop/panel.hpp` `kLayouts`;
`workshop/weave_editor.cpp` `layouts_press`; `tests/test_workshop_screen.cpp` case `"WUX-9/SC-9: a
press answers a painted tab and nothing else on the band"`, case `"WUX-9/SC-8+SC-9: an omitted tab
has no span and cannot be pressed"`, case `"QR-14/SC-5: no press outside the painted run reaches a
layout"`.
WHY — `agents/decisions/a-routing-bool-is-not-a-disposition.md`

## WL-PRESS-06 — A secondary press is state-local first refusal

LAW — The active interaction that can truthfully interpret a secondary press receives it; one consumed gesture performs one transition, and there is no global Back action or keymap row.

MEANS
- ordinary Workshop opens or re-targets the contextual surface; the open surface re-targets;
- either arrangement scope, the reset prompt included, leaves on one, consumed whole;
- the Terminal still means nothing by it.

PROVEN BY — `workshop/weave_pointer.cpp` `take_hold`; `workshop/weave_arrange.cpp`
`enter_arrange_pane`; `tests/test_workshop_panels.cpp` case `"ARR-0/SC-6: every arrangement level
claims the press; the menu keeps its own"`, case `"ARR-0/SC-7: one right press exits Arrange; only
the NEXT one opens context"`, case `"CTX-0/ARR-0: a mode that owns the pointer answers a right
press its own way"`.
WHY — `agents/decisions/a-routing-bool-is-not-a-disposition.md`

## Do not assume

- That the bare bool is inadequate — the richer answers (`Written`, `Handled`, `Commit`,
  `Availability`, `Occupancy`) all live on semantic paths; only the routing path is a bool.
