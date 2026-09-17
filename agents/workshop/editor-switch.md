# Workshop law — switching the Editor

Register `WL-SWITCH`: an office with authored choices, and what realization records about which
one holds it. One law per heading; cite by ID. Router: [`../workshop.md`](../workshop.md).
Document custody is [`editor.md`](editor.md); an open is [`opening.md`](opening.md); prepared
replacement is Loom's law, never restated here.

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
`"a switch is recorded only for an authored choice, and a refused record moves nothing"`.
WHY — `agents/decisions/an-editor-is-replaced-not-routed.md`

## Do not assume

- That anything in this tree moves an office between its choices yet: the record is witnessed by
  the realization owner's own rig, which records a switch directly.
