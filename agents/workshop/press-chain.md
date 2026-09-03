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

PROVEN BY — `workshop/weave.hpp` `info_press`, `objects_press`, `actions_press`, `take_hold`;
`tests/test_workshop_document.cpp` case `"QR-2: a press where the caret already is is CONSUMED,
and the panel never answers"`, case `"QR-2: consumed and not-consumed are told apart by WHERE,
not by what changed"`.
WHY — `agents/decisions/a-routing-bool-is-not-a-disposition.md`

## WL-PRESS-02 — A deliberate `false` is a decision

LAW — `objects_press` declines a press on the already-selected object's row so the panel answers it; `terminal_press`'s bool is "a repaint is owed" and is never unified with the chain.

PROVEN BY — `workshop/weave.hpp` `objects_press`, `terminal_press`, `repaint_needed`;
`tests/test_workshop_document.cpp` case `"QR-2: a press on the ALREADY selected object row is
deliberately not consumed"`.
WHY — `agents/decisions/a-routing-bool-is-not-a-disposition.md`

## WL-PRESS-03 — `info_body_at` is the resolve-and-locate preamble, owned once

LAW — It answers where and nothing about meaning, and the body is resolved once per press beside the canvas point; holding it is sound because a declining handler changes nothing.

MEANS
- each handler asks its own inverse: `property_row_hit`, `action_press_at`, `object_press_at`.

PROVEN BY — `workshop/screen.hpp` `info_body_at`, `property_row_hit`, `action_press_at`,
`object_press_at`, `terminal_input_hit`, `InfoBodyAt`, `InfoBodyAt::present`,
`files_row_of_body_row`; `tests/test_workshop_document.cpp` case `"QR-2: the body's
resolve-and-locate is ONE answer, and it is the painter's"`, case `"QR-2: no press inside the Info
body begins a workspace gesture, on any row"`.
WHY — `agents/decisions/a-routing-bool-is-not-a-disposition.md`

## WL-PRESS-04 — Nothing asks a geometry question above occupancy

LAW — The order is the terminal overlay, arrangement, the open contextual surface, then pane occupancy over `effective_pane_order`, then the resolved pane's own inverse, then the workspace.

MEANS
- a new pane-internal gesture belongs in the resolved-owner arm, never above the walk;
- a pane in front of an Info control takes the point; a pane in front of the tabs takes the press.

PROVEN BY — `workshop/weave.hpp` `take_hold`, `external_press`, `info_press`,
`on(PointerButton)`; `workshop/screen.hpp` `occupied_at`, `Occupancy::kind`, `ExternalPressAt`,
`external_press_at`; `tests/test_workshop_screen.cpp` case `"WUX-12/SC-5+SC-7: a pane in front of
the Layouts pane takes the press"`, case `"WUX-12/SC-6: a pane in front of an Info control takes
the point"`; `tests/test_workshop_panes_input.cpp` case `"SEL-0: management chrome gets first
refusal, and a mode takes the press whole"`.
WHY — `agents/decisions/a-routing-bool-is-not-a-disposition.md`

## WL-PRESS-05 — `band_tab_at` is the Layouts pane's local inverse

LAW — The tab inverse is spent only once occupancy has named the Layouts pane, and its spans come from the run's own composition against the pane's body, so no unpainted tab is ever answered.

PROVEN BY — `workshop/screen.hpp` `occupied_at`, `band_tab_at`, `band_status`, `layouts_body`;
`workshop/panel.hpp` `kLayouts`; `workshop/weave.hpp` `layouts_press`;
`tests/test_workshop_screen.cpp` case `"WUX-9/SC-9: a press answers a painted tab and nothing else
on the band"`, case `"WUX-9/SC-8+SC-9: an omitted tab has no span and cannot be pressed"`, case
`"QR-14/SC-5: no press outside the painted run reaches a layout"`.
WHY — `agents/decisions/a-routing-bool-is-not-a-disposition.md`

## WL-PRESS-06 — A secondary press is state-local first refusal

LAW — The active interaction that can truthfully interpret a secondary press receives it; one consumed gesture performs one transition, and there is no global Back action or keymap row.

MEANS
- ordinary Workshop opens or re-targets the contextual surface; the open surface re-targets;
- either arrangement scope, the reset prompt included, leaves on one, consumed whole;
- the Terminal still means nothing by it.

PROVEN BY — `workshop/weave.hpp` `take_hold`, `enter_arrange_pane`;
`tests/test_workshop_panels.cpp` case `"ARR-0/SC-6: every arrangement level claims the press; the
menu keeps its own"`, case `"ARR-0/SC-7: one right press exits Arrange; only the NEXT one opens
context"`, case `"CTX-0/ARR-0: a mode that owns the pointer answers a right press its own
way"`.
WHY — `agents/decisions/a-routing-bool-is-not-a-disposition.md`

## Do not assume

- That the bare bool is inadequate — the richer answers (`Written`, `Handled`, `Commit`,
  `Availability`, `Occupancy`) all live on semantic paths; only the routing path is a bool.
