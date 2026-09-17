# Workshop law — switching the Editor

Register `WL-SWITCH`: an office with authored choices, and one switch of `zengine.editor`
between them with the document carried across. One law per heading; cite by ID. Router:
[`../workshop.md`](../workshop.md). Document custody is [`editor.md`](editor.md); an open is
[`opening.md`](opening.md); what crosses the seam is the protocol's law in
[`../panes.md`](../panes.md); prepared replacement is Loom's law, never restated here.

## WL-SWITCH-01 — A plan authors an office's choices, and exactly one of them starts

LAW — `choices` name the artifacts that may hold a role, each name and artifact once; exactly one row of `artifacts` loads the role and is a choice, and the rest load only when a switch asks.

MEANS
- an artifact is a choice for one office; its start row asks for weave participation alone;
- a plan with no choices is written as version 1, byte for byte; choices are version 2;
- a version-2 file with no choices, and a version-1 file carrying them, are refused.

DOES NOT MEAN
- that a host names an artifact: only the plan confers participation.

PROVEN BY — `workshop/load_plan.hpp` `check_choices`, `ChoiceIntent`, `LoadPlan::choices`,
`check_plan`; `workshop/load_persist.hpp` `WorkshopLoadChoice`, `to_text`, `from_text`;
`tests/test_workshop_load.cpp` case `"a plan authoring no choices is written as version 1, byte
for byte what it always was"`, case `"choices are written as version 2 and read back as the same
plan, byte for byte"`, case `"a version-2 file with no choices is refused, and a version-1 file
cannot carry choices"`, case `"the choice law: one start holder that is a choice, one office per
artifact, one spelling per word"`.
WHY — `agents/decisions/an-editor-is-replaced-not-routed.md`

## WL-SWITCH-02 — Realization records which choice holds the office, and every reader follows it

LAW — After Loom moved the office, realization records the choice's weave and image; the row it left is `switched` and refuses a reload in words, and arrangement and provenance follow the weave.

MEANS
- a switch back resolves the plan's own row again; a choice outside `artifacts` gets a kept row;
- a record for a stem that is no choice, or made during a load, is refused and moves nothing.

PROVEN BY — `workshop/load_execute.hpp` `record_choice_holder`, `choice_holder`, `image_of`,
`reload_refusal`, `ResolvedArtifact::switched_to`; `workshop/arrangement.hpp` `kSwitchedToken`;
`introspection/resolved.hpp` `kSwitchedRow`; `tests/test_workshop_load.cpp` case `"a recorded
switch moves an office between its authored choices, and every reader follows the weave"`, case
`"a switch is recorded only for an authored choice, and a refused record moves nothing"`;
`tests/test_workshop_editor_switch.cpp` case `"a switch between two authored Editors carries the
document, its unsaved edits and its caret, and moves the office"`.
WHY — `agents/decisions/an-editor-is-replaced-not-routed.md`

## WL-SWITCH-03 — One coordinator owns one switch, and every ending is an answer

LAW — The native `EditorSwitchCoordinator` owns one switch's intent, stage, consent and outcome over Loom's prepared replacement: one at a time, no timeout, each ending answering the ask that waits.

MEANS
- the choice in office is `already-active` and loads nothing; an unloadable one moves nothing;
- a request while one is under way is refused in words; silence stays pending and cancellable;
- a request replaces one awaiting consent, whose confirmation is then answered `superseded`.

DOES NOT MEAN
- that the coordinator holds a document, a row or a room: the editors and the desk do.

PROVEN BY — `workshop/editor_switch.hpp` `EditorSwitchCoordinator`, `on(EditorSwitchRequested)`,
`on(EditorSwitchCancelled)`, `on(EditorSwitchStatusRequested)`, `answer_superseded`,
`load_and_warm`, `on_beat`, `abandon`; `workshop/editor_switch_vocabulary.hpp`
`EditorSwitchAnswered`, `EditorSwitchProgress`; `tests/test_workshop_editor_switch.cpp` case
`"asking for the choice that already holds the office is harmless: nothing is loaded"`, case `"a
choice the plan does not author, and one whose artifact will not load, are refused with nothing
moved"`, case `"a switch waiting on a silent candidate is pending, published, refuses a second
switch, and cancels cleanly"`, case `"a newer request replaces a switch awaiting consent, and the
replaced switch's confirmation is answered superseded"`.
WHY — `agents/decisions/an-editor-is-replaced-not-routed.md`

## WL-SWITCH-04 — The incumbent authors the document at the boundary and holds still until told

LAW — Asked at the boundary, the editor in office answers the exact transfer and applies no input until it hears the outcome; what it refused is counted, and said when it resumes or reported with the switch.

MEANS
- judging holds nothing, and the incumbent keeps editing while the candidate starts;
- an open, relay or paste in flight refuses a switch; an open or quit while held is refused;
- when nothing moved before the commitment, the incumbent is told, resumes, and says so.

PROVEN BY — `editor-pane/pane.cpp` `on(EditorHandoffJudgeRequested)`,
`on(EditorHandoffRequested)`, `on(EditorHandoffEnded)`, `on(EditorRetireRequested)`,
`held_still`, `handoff_refusal`, `transfer_now`; `workshop/editor_switch.hpp` `end_handoff`,
`on(EditorRetired)`; `tests/test_workshop_editor_switch.cpp` case `"a keystroke that reaches the
incumbent after its boundary is refused, counted, and reported with the switch"`, case `"a
destination that refuses the document leaves the incumbent editing, resumed and told"`, case `"a
switch waiting on a silent candidate is pending, published, refuses a second switch, and cancels
cleanly"`.
WHY — `agents/decisions/an-editor-is-replaced-not-routed.md`

## WL-SWITCH-05 — The candidate adopts sealed, the admission activates it, nothing rolls back

LAW — A sealed candidate adopts the transfer or refuses it naming why, and the commit is its activation; a successor that does not prove it serves is `failed-after-commit`, and the retired editor is kept.

MEANS
- the successor's document generation passes the incumbent's; traffic queued behind reaches it;
- the retired editor is unloaded once the successor serves; a kept one goes at the next switch.

PROVEN BY — `editor-pane/pane.cpp` `on(EditorAdoptRequested)`, `on(EditorLiveRequested)`,
`after_delivery`; `workshop/editor_switch.hpp` `on(EditorAdopted)`, `on(EditorLive)`,
`release_retired`, `release_retained`; `tests/test_workshop_editor_switch.cpp` case `"a switch
between two authored Editors carries the document, its unsaved edits and its caret, and moves the
office"`, case `"a keystroke queued behind the commitment reaches the successor, ahead of nothing
it could miss"`, case `"a successor that holds the office and does not serve is a failure after
the commitment, and the retired Editor is kept"`.
WHY — `agents/decisions/an-editor-is-replaced-not-routed.md`

## WL-SWITCH-06 — Losses are agreed to by their digest, and judged again at the boundary

LAW — A switch that would lose something loads nothing until `EditorSwitchConfirmed` carries the digest of exactly those losses; losses that moved by the boundary discard the candidate and ask again.

MEANS
- the digest names the losses, not the document: typing that loses nothing more keeps it good;
- a consent is for one switch; a wrong one is answered with the right one; nothing is saved.

PROVEN BY — `workshop/editor_switch.hpp` `on(EditorHandoffJudged)`,
`on(EditorSwitchConfirmed)`, `on(EditorHandoffOffered)`;
`workshop/editor_handoff_vocabulary.hpp` `handoff_digest`, `EditorHandoffJudged`;
`tests/test_workshop_editor_switch.cpp` case `"a switch that would lose something loads nothing
until the maker consents, and a consent the losses moved past is asked for again"`.
WHY — `agents/decisions/an-editor-is-replaced-not-routed.md`

## WL-SWITCH-07 — A maker switches from the Terminal, and the desk shows the switch meanwhile

LAW — The host mounts the coordinator over its managed pane's office and grants its Terminal the four switch asks, to that office alone; a pending switch is a standing condition saying how to stop it.

MEANS
- the documented lines are typed against the host's own widening, `let_terminal_switch_editors`;
- the desk keeps a condition only when the switch's office said it, and drops it when it ends;
- every answer is said on the notice line; a question for consent shows the line that confirms it.

DOES NOT MEAN
- that the host names an editor artifact: its managed pane's office, and the plan's choices.

PROVEN BY — `workshop/workshop.cpp` `EditorSwitchHost`, `let_terminal_switch_editors`,
`editor_switch_grant`; `workshop/editor_switch.hpp` `let_terminal_switch_editors`, `progress`,
`publish_said`;
`workshop/weave_managed.cpp` `on(EditorSwitchProgress)`; `tests/test_workshop_editor_switch.cpp`
case `"the documented Terminal lines, typed through Workshop's own door, ask, confirm and switch,
and the desk shows the switch while it stands"`.
WHY — `agents/decisions/an-editor-is-replaced-not-routed.md`

## Do not assume

- That these rigs ran Neovim: the switch suite runs builds of the standard Editor, three of
  which fail on purpose; the Neovim-backed choice is witnessed by its own suites.
- That a switch survives a reload of the coordinator: it is native, and its hot reload is not
  supported.
