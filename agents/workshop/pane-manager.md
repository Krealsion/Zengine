# Workshop law — the Pane Manager

Register `WL-PED`: a pane as a subject, and what its rows write. The host's Pane Manager
built-in retired: its list is the desktop's Pane Manager (WL-DESK-04, WL-DESK-12) and a pane's
subject is an inspector's (WL-INFO-14). One law per heading; cite by ID. Router:
[`../workshop.md`](../workshop.md).

## WL-PED-01 — RETIRED: the Pane Manager was the built-in whose subject is a pane

WHY — `agents/decisions/a-subject-is-not-a-selection.md`

## WL-PED-02 — RETIRED: the subject was the manager's, written by one door

WHY — `agents/decisions/a-subject-is-not-a-selection.md`

## WL-PED-03 — RETIRED: the manager's subject stood, and one rule cleared it

WHY — `agents/decisions/a-subject-is-not-a-selection.md`

## WL-PED-04 — A pane subject's rows read fresh, and nothing writes on paint

LAW — A pane subject's rows say identity, then AUTHORED, then RESOLVED, each read when it is read; nothing writes on paint, and a section heading is the one widening of the row vocabulary.

MEANS
- rows close over the session and the subject, named anew when the pane, desk or layout moves;
- `RESOLVED` rows call `bounds_of` and `pane_state_of` at the moment they are read.

PROVEN BY — `workshop/screen_pane_subject.cpp` `pane_subject_rows`;
`workshop/weave_inspection.cpp` `refresh_inspected`; `workshop/property.hpp` `Row::section`;
`tests/test_workshop_panels.cpp` case `"WUX-13/SC-4+SC-5: the subject's rows say identity, then
AUTHORED, then RESOLVED"`, case `"WUX-13/SC-8: looking never authors"`.
WHY — `agents/decisions/a-subject-is-not-a-selection.md`

## WL-PED-05 — Every write is an existing door

LAW — A subject's row writes through a door the desk already has: a typed axis through the gesture door (a reset door for `-`), a place through reseating; presence is the launch and close doors'.

MEANS
- there is no subject setter and no rectangle held anywhere;
- `pane_window_base` is `managed_window_base`'s body: typed axes measure from the hands' window.

PROVEN BY — `workshop/screen_pane_subject.cpp` `write_pane_axis`, `pane_window_base`;
`workshop/setup.hpp` `author_pane_window`, `reset_pane_place`; `workshop/weave_session.cpp`
`apply_setup`; `workshop/weave_arrange.cpp` `managed_window_base`;
`workshop/weave_inspection.cpp` `on(PaneCommitRequested)`; `tests/test_workshop_panels.cpp` case
`"WUX-13/SC-6+SC-11: a typed place moves Layouts through the gesture door, and its tabs follow"`,
case `"WUX-13/SC-6: a typed place reseats the stack through `apply_setup`"`, case `"WUX-13/SC-12:
moving, resizing and closing Layouts through the doors leaves the reservation alone"`.
WHY — `agents/decisions/a-subject-is-not-a-selection.md`

## WL-PED-06 — A typed value is refused, never clamped

LAW — A typed amount is a whole number in the face's own unit, refused and never clamped: the other face's word is refused rather than converted, and the inverse to sub-units on that grain is a ceiling.

MEANS
- the setup's own checks then judge the fine value; a refusal goes back to the asker's draft;
- `pane_geometry_typeable` is the arrangement's admission less one refusal: off-room is typeable.

PROVEN BY — `workshop/screen_pane_state.cpp` `parse_face_amount`, `geometry_unit`;
`workshop/screen.hpp` `subs_of_device_amount`, `FaceAmount`; `workshop/screen_pane_subject.cpp`
`pane_geometry_typeable`; `surface/region.hpp` `device_of_subs`; `tests/test_workshop_panels.cpp`
case `"WUX-13: a typed amount is read and written in the face's own unit"`, case `"WUX-13/SC-7: a
typed value that is not admissible is refused, and the authored row is untouched"`.
WHY — `agents/decisions/a-subject-is-not-a-selection.md`

## WL-PED-07 — RETIRED: `kDraft` was the manager's draft context

WHY — `agents/decisions/a-subject-is-not-a-selection.md`

## WL-PED-08 — RETIRED: both lists scrolled under the wheel, and the wheel moved no keys

WHY — `agents/decisions/a-subject-is-not-a-selection.md`

## Do not assume

- That a pane's subject is the selected pane, or that naming a subject selects it: neither
  (WL-INFO-14).
- That the rows are a document editor, a Surface primitive, a general property inspector, a
  wiring editor or a safe mode. Recovery is the desktop's Pane Manager (`desktop.panes`), `-` in
  a row or `0` in the arrangement, the default desk and `--isolated`.
