# Workshop law — the Info controls

Register `WL-CTRL`: the footer of controls, and the grounds the structural rows sit on. One law
per heading; cite by ID. Router: [`../workshop.md`](../workshop.md).

**Where these controls live.** They were `workshop/screen_info.cpp`'s `action_row_text` and
`say_row`, painted by a built-in panel. They are `info-pane/pane.cpp`'s now, said as two rows
into the room the pane was granted. The two reasons, the two owners and the characters are
unchanged; what moved is that the party making the application's own refusal is a loaded
image, and the party making the document's is still the host.

## WL-CTRL-01 — The last two rows are a footer of controls, reserved off the budget

LAW — `[ Create ]` and `[ Delete ]` are two pressable rows reserved by one subtraction from the granted rows before either list is offered anything, and they are said last.

MEANS
- spare room falls between the properties and the controls, never under the hand aiming at them;
- one composition pass places them and records the rows, so painter and press cannot disagree.

DOES NOT MEAN
- that the controls are a third claimant on `share_body_rows` — a fixed demand is not a list.

PROVEN BY — `info-pane/pane.cpp` `kActionCount`, `kActionCreate`, `kActionDelete`, `say`,
`share_body_rows`; `tests/test_workshop_panes_info.cpp` case `"INFO-WEAVE: the two controls are
the last rows of the body, and say their own availability in characters"`, case `"INFO-WEAVE: a
room too short for the body invents none of it"`.
WHY — `agents/decisions/a-footer-not-a-third-list.md`

## WL-CTRL-02 — The pane publishes rows it can place, and never more than the room

LAW — Spare rows are written blank because a publication is positional and the controls are at the end; `mark` and `placed` are inverses over one composition, and `finish` truncates to the granted rows.

DOES NOT MEAN
- that a row is dropped to make space — the room is a wall, never a budget to negotiate.

PROVEN BY — `info-pane/pane.cpp` `mark`, `placed`, `finish`, `composed_`;
`workshop/weave.hpp` `WorkshopWeave::judge_content`; `tests/test_workshop_panes_info.cpp` case
`"INFO-WEAVE: a room too short for the body invents none of it"`, case `"INFO-WEAVE: pressing
Delete is the SAME operation the `d` key performs"`.
WHY — `agents/decisions/a-footer-not-a-third-list.md`

## WL-CTRL-03 — Availability is two reasons, one bit, two owners

LAW — Two reasons, two owners: a live draft is the pane's own refusal, made before the operation with its own sentence; no target is the document's, the ask going through and the document refusing.

MEANS
- a control never invents a reason: it defers to whoever owns the refusal;
- availability predicts no refusal: dependents and a spent mint stay the document's to say.

DOES NOT MEAN
- that this is a `disabled` flag — a flag would collapse two facts with two owners.

PROVEN BY — `info-pane/pane.cpp` `Availability`, `action_availability`, `press_action`,
`available`; `workshop/document_seam_vocabulary.hpp` `DocumentActed::refusal`;
`workshop/document.hpp` `remove`; `tests/test_workshop_panes_info.cpp` case `"INFO-WEAVE: a live
draft holds both controls back, and the reason is the maker's"`, case `"INFO-WEAVE: the two
controls are the last rows of the body, and say their own availability in characters"`.
WHY — `agents/decisions/a-footer-not-a-third-list.md`

## WL-CTRL-04 — Unavailable is said in characters

LAW — `[ Create ]` is pressable and `( Delete )` is not, the same width either way; the muted role is the second signal and never the only one, because a terminal has no ground to tint.

PROVEN BY — `info-pane/pane.cpp` `action_row_text`, `action_label`, `Availability`;
`tests/test_workshop_panes_info.cpp` case `"INFO-WEAVE: the two controls are the last rows of
the body, and say their own availability in characters"`.
WHY — `agents/decisions/a-footer-not-a-third-list.md`

## WL-CTRL-05 — The controls do not own the acts

LAW — Pressing Create or Delete asks for the operation `n` or `d` is bound to, so both gestures converge on one write, one selection rule and one sentence; the controls own no act and no document.

MEANS
- there is no callback, command id or action registry — a switch over two indices of a table;
- `component::Button` was not extracted: a label, a bit and a bracket keep no invariant.

DOES NOT MEAN
- that the controls take keys — they are pointer-only, and no focus framework exists for them.

PROVEN BY — `info-pane/pane.cpp` `press_action`, `ask`; `workshop/document_seam_vocabulary.hpp`
`kDocumentCreate`, `kDocumentDelete`; `workshop/weave_seam.cpp` `on(DocumentActRequested)`;
`tests/test_workshop_panes_info.cpp` case `"INFO-WEAVE: pressing Create is the SAME operation
the `n` key performs"`, case `"INFO-WEAVE: pressing Delete is the SAME operation the `d` key
performs"`, case `"INFO-WEAVE: a live draft holds both controls back, and the reason is the
maker's"`.
WHY — `agents/decisions/a-component-is-earned.md`

## WL-CTRL-06 — The structural rows sit on a ground

LAW — The properties heading is accent on muted, a pressable control fill on muted, an unavailable one muted on no ground, every other row on none; the ground is the row's own background.

MEANS
- the ground crosses the seam as a row's own field, so the pane names it and the host draws it;
- an unavailable control loses the ground entirely, so the ground means actionable, not present.

PROVEN BY — `info-pane/pane.cpp` `say`, `say_properties`; `surface/vocabulary.hpp` `kAccent`,
`kMuted`, `kFill`, `kNone`, `SurfaceTextRow`, `SurfaceTextRow::background`;
`tests/test_workshop_panes_info.cpp` case `"INFO-WEAVE: the two controls are the last rows of
the body, and say their own availability in characters"`.
WHY — `agents/decisions/a-footer-not-a-third-list.md`

## WL-CTRL-07 — The ground is presentation and moved no geometry

LAW — A row's ground is the host's to draw at the row the pane published it on, so the grounded strip is exactly the prose row the press inverse inverts, and no row index or hit mapping moved for it.

PROVEN BY — `workshop/screen_external.cpp` `paint_external`; `surface/pointing.hpp`
`prose_row_of_pixel`; `surface/region.hpp` `kTextInsetPx`;
`tests/test_workshop_panes_info.cpp` case `"INFO-WEAVE: a press on an object row selects it,
through the document's own door"`.
WHY — `agents/decisions/a-footer-not-a-third-list.md`
