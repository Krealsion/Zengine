# Workshop law — the Info controls

Register `WL-CTRL`: the footer of controls, and the grounds the structural rows sit on. One law
per heading; cite by ID. Router: [`../workshop.md`](../workshop.md).

## WL-CTRL-01 — The third run is a footer of controls, reserved off the budget

LAW — `[ Create ]` and `[ Delete ]` are two pressable rows reserved by one subtraction from the budget before either list is offered anything, anchored to the foot.

MEANS
- spare room falls between the properties and the controls, never under the hand aiming at them;
- `InfoBodyPlace::action_row` is where the reserved rows are; painter and press both ask it.

DOES NOT MEAN
- that the controls are a third claimant on `share_body_rows` — a fixed demand is not a list.

PROVEN BY — `workshop/screen.hpp` `kActionRows`, `action_row`, `info_body_place`,
`share_body_rows`, `kActionCreate`, `kActionDelete`; `tests/test_workshop_panels.cpp` case `"HD-8:
the footer is reserved off the budget, and every HD-7 property survives it"`, case `"HD-8: the
controls are the last two rows of the body, at every extent and size"`, case `"HD-8: growing the
panel gives the lists more room and the footer exactly two rows"`.
WHY — `agents/decisions/a-footer-not-a-third-list.md`

## WL-CTRL-02 — The body publishes exactly `capacity` rows

LAW — Spare rows are written blank because a region's rows are positional and the controls are at the end; `prose_row_of_action`/`action_at_prose_row` are inverses and there is no third copy.

DOES NOT MEAN
- that fewer than `capacity` rows are ever published — spare rows are blank, never absent.

PROVEN BY — `workshop/screen.hpp` `prose_row_of_action`, `action_at_prose_row`;
`tests/test_workshop_panels.cpp` case `"HD-8: the action row maps are inverses, and nothing else
is a control"`, case `"HD-8: a resize moves the footer and changes no document and no draft"`.
WHY — `agents/decisions/a-footer-not-a-third-list.md`

## WL-CTRL-03 — Availability is two reasons, one bit, two owners

LAW — Two reasons, two owners: a live draft is the application's refusal, made before the operation with its own sentence; no target is the document's, the press going through and the document refusing.

MEANS
- a control never invents a reason: it defers to whoever owns the refusal;
- availability predicts no refusal: dependents and a spent mint stay the document's to say.

DOES NOT MEAN
- that this is a `disabled` flag — a flag would collapse two facts with two owners.

PROVEN BY — `workshop/screen.hpp` `kDraftLive`, `kNoTarget`, `Availability`,
`action_availability`, `draft_live`; `workshop/weave.hpp` `actions_press`, `finish_draft_first`;
`workshop/document.hpp` `remove`; `tests/test_workshop_panels.cpp` case `"HD-8: availability is
two reasons, one bit, and no prediction of a refusal"`, case `"HD-8: availability is not a
prediction of what the document will say"`, case `"HD-8: an unavailable Delete presents as
unavailable and mutates nothing"`.
WHY — `agents/decisions/a-footer-not-a-third-list.md`

## WL-CTRL-04 — Unavailable is said in characters

LAW — `[ Create ]` is pressable and `( Delete )` is not, the same width either way; the muted role is the second signal and never the only one, because a terminal has no ground to tint.

PROVEN BY — `workshop/screen.hpp` `Availability`, `say_row`, `action_row_text`;
`tests/test_workshop_panels.cpp` case `"HD-8: unavailable is said in CHARACTERS, so a colourless
medium reads it too"`.
WHY — `agents/decisions/a-footer-not-a-third-list.md`

## WL-CTRL-05 — The controls do not own the acts

LAW — Pressing Create or Delete calls the operation `n` or `d` is bound to, so both gestures converge on one write, one selection rule and one sentence; the controls own no act.

MEANS
- there is no callback, command id or action registry — a switch over two indices of a table;
- `component::Button` was not extracted: a label, a bit and a bracket keep no invariant.

DOES NOT MEAN
- that the controls take keys — they are pointer-only, and no focus framework exists for them.

PROVEN BY — `workshop/weave.hpp` `actions_press`, `create_object`, `delete_object`;
`tests/test_workshop_panels.cpp` case `"HD-8: pressing Create is the SAME operation the `n` key
performs"`, case `"HD-8: pressing Delete is the SAME operation the `d` key performs"`, case
`"HD-8: a live property draft survives both controls, with no implicit commit"`.
WHY — `agents/decisions/a-component-is-earned.md`

## WL-CTRL-06 — The structural rows sit on a ground

LAW — The properties heading is accent on muted, a pressable control fill on muted, an unavailable one muted on no ground, every other row on none; the ground is the row's own background.

MEANS
- the two consumers use one ground by agreement, not by a `kSectionGround` constant;
- an unavailable control loses the ground entirely, so the ground means actionable, not present.

PROVEN BY — `workshop/screen.hpp` `say_row`; `surface/vocabulary.hpp` `kAccent`, `kMuted`,
`kFill`, `kNone`, `SurfaceTextRow`, `background`; `tests/test_workshop_document.cpp` case
`"HD-9: an available control sits on a ground and an unavailable one does not"`, case `"HD-9: no
other row of the body was given a ground"`, case `"HD-9: a live draft takes the ground off BOTH
controls, and gives it back"`.
WHY — `agents/decisions/a-footer-not-a-third-list.md`

## WL-CTRL-07 — The ground is presentation and moved no geometry

LAW — The grounded strip of a prose row is exactly the pixel partition the press inverse inverts — the row's own line height from the body's origin — and the text inset margin names no control.

PROVEN BY — `workshop/screen.hpp` `say_row`; `surface/pointing.hpp` `prose_row_of_pixel`;
`surface/region.hpp` `kTextInsetPx`; `tests/test_workshop_document.cpp` case `"HD-9: the
grounded strip is exactly the prose row a press resolves to"`, case `"HD-9: a ground changed no
composition, no row index and no hit mapping"`, case `"HD-9: the ground reaches the whole row in
a CELL medium, not just its characters"`.
WHY — `agents/decisions/a-footer-not-a-third-list.md`
