# Workshop law — the Pane Manager

Register `WL-PED`: the Pane Manager has a subject, and the subject is not the selection. One law
per heading; cite by ID. Router: [`../workshop.md`](../workshop.md).

## WL-PED-01 — The Pane Manager is the built-in whose subject is a pane

LAW — The Pane Manager is the keyboard-taking built-in whose subject is an ordinary Workshop pane; its durable key is `pane-editor`, and its symbols, key and ids are the older name's, unchanged.

MEANS
- a key is a promise to every file that names it; the symbols and ids are history, not product;
- it replaces nothing: Info still inspects document objects behind rows that share labels;
- `kPickerNameCols` is thirteen so `Pane Manager` fits; a name must be strictly shorter than it.

PROVEN BY — `workshop/panel.hpp` `kPaneEditor`, `pane-editor`, `kPanelCatalog`;
`workshop/keymap.hpp` `pane-editor`; `workshop/screen.hpp` `kPickerNameCols`, `picker_entry_text`;
`tests/test_workshop_panels_creator.cpp` case `"WUX-14/SC-2: the WUX-13 surface is the Pane
Manager, and its durable key did not move"`; `tests/test_workshop_panels.cpp` case `"WUX-13/SC-2:
the Pane Editor is a built-in, and its list is the picker's population"`;
`tests/test_workshop_screen.cpp` case `"INTR-0 bounded extension: a provider's long name is MARKED
in the picker, not cut"`.
WHY — `agents/decisions/a-subject-is-not-a-selection.md`

## WL-PED-02 — The subject is a `PaneRef` on the session, never derived from the selection

LAW — The subject is written by `choose_subject` and nothing else, never derived from `Panels::selected`; pressing into the manager selects the manager and retargets nothing.

MEANS
- that is what lets the manager be its own subject: choose it, type into its `X`, and it moves;
- it is not persisted: a subject is interaction state, not a preference riding an artifact.

PROVEN BY — `workshop/screen.hpp` `PaneEditor`, `pane_editor`; `workshop/weave.hpp`
`choose_subject`, `pane_editor_press`; `tests/test_workshop_panels.cpp` case `"WUX-13/SC-1: the
subject is chosen, and interacting inside the editor does not retarget it"`, case `"WUX-13/SC-15:
the Pane Editor can be its own subject, and its own rows do not retarget it"`, case
`"WUX-13/SC-13: a Pane Editor edit survives a restart through the session, and the subject does
not"`.
WHY — `agents/decisions/a-subject-is-not-a-selection.md`

## WL-PED-03 — The subject stands, and one rule clears it

LAW — The subject stands through a layout switch, its pane closing and its provider going away; the one clearing rule is `repair_pane_editor_subject`, asked at a gesture and never on paint.

MEANS
- it clears when the ref is in neither `inventory_rows` nor the active setup;
- `forget_removed_selection` deliberately does not touch it.

PROVEN BY — `workshop/weave.hpp` `repair_pane_editor_subject`, `forget_removed_selection`;
`workshop/setup.hpp` `inventory_rows`; `tests/test_workshop_panels.cpp` case `"WUX-13: the subject
stands across a layout switch, and clears only when nothing names it"`, case `"WUX-13/SC-9: a
closed pane and an unresolved row are subjects with honest facts"`.
WHY — `agents/decisions/a-subject-is-not-a-selection.md`

## WL-PED-04 — Every row reads fresh and nothing writes on paint

LAW — The panes list is the picker's own population, read fresh at every paint and gesture; every row reads fresh and nothing writes on paint; a section heading is the one widening of the row vocabulary.

MEANS
- rows close over the session and the subject, rebuilt only when the subject changes;
- `RESOLVED` rows call `bounds_of` and `pane_state_of` at the moment they are read.

PROVEN BY — `workshop/screen.hpp` `paint_pane_editor`, `pane_editor_rows`; `workshop/setup.hpp`
`inventory_rows`; `workshop/property.hpp` `section`; `workshop/weave.hpp` `rebuild_subject_rows`;
`tests/test_workshop_panels.cpp` case `"WUX-13/SC-2: the Pane Editor is a built-in, and its list
is the picker's population"`, case `"WUX-13/SC-4+SC-5: the subject's rows say identity, then
AUTHORED, then RESOLVED"`, case `"WUX-13/SC-8: looking never authors"`.
WHY — `agents/decisions/a-subject-is-not-a-selection.md`

## WL-PED-05 — Every write is an existing door

LAW — No door of its own: a typed axis goes through the gesture door (a reset door for `-`), the order keys through the contextual seam, participation through the picker's toggle, a place through reseating.

MEANS
- there is no `PaneEditor` setter and no rectangle held anywhere;
- `toggle_participation` is `choose_panel`'s body quarried out: picker and manager are one door;
- `pane_window_base` is `managed_window_base`'s body: typed axes measure from the hands' window.

PROVEN BY — `workshop/screen.hpp` `write_pane_axis`, `pane_window_base`; `workshop/setup.hpp`
`author_pane_window`, `reset_pane_place`; `workshop/weave.hpp` `apply_setup`, `spend_pane_action`,
`toggle_participation`, `choose_panel`, `managed_window_base`, `editing_key`;
`tests/test_workshop_panels.cpp` case `"WUX-13/SC-6+SC-11: a typed place moves Layouts through the
gesture door, and its tabs follow"`, case `"WUX-13/SC-6: a typed place reseats the stack through
`apply_setup`"`, case `"WUX-13/SC-12: moving, resizing and closing Layouts through the editor
leaves the reservation alone"`.
WHY — `agents/decisions/a-subject-is-not-a-selection.md`

## WL-PED-06 — A typed value is refused, never clamped

LAW — A typed amount is a whole number in the face's own unit, refused and never clamped: the other face's word is refused rather than converted, and the inverse to sub-units on that grain is a ceiling.

MEANS
- the setup's own checks then judge the fine value; the draft stays open with the maker's text;
- `pane_geometry_typeable` is the arrangement's admission less one refusal: off-room is typeable.

PROVEN BY — `workshop/screen.hpp` `parse_face_amount`, `subs_of_device_amount`,
`pane_geometry_typeable`, `geometry_unit`, `FaceAmount`; `surface/region.hpp` `device_of_subs`;
`tests/test_workshop_panels.cpp` case `"WUX-13: a typed amount is read and written in the face's
own unit"`, case `"WUX-13/SC-7: a typed value that is not admissible is refused, and the authored
row is untouched"`.
WHY — `agents/decisions/a-subject-is-not-a-selection.md`

## WL-PED-07 — `kDraft` is one context for two inspectors

LAW — The manager's draft shares Info's draft context — one context for two inspectors — and the draft under the keys is resolved by the same chain, so a change of document selection cannot touch it.

MEANS
- `draft_live` and `pane_editor_draft_live` are two questions; Info's refusals skip the manager.

PROVEN BY — `workshop/screen.hpp` `keyboard_context_beneath_menu`, `pane_editor_has_keyboard`,
`draft_live`, `pane_editor_draft_live`; `workshop/weave.hpp` `editing_row`;
`workshop/keymap.hpp` `kDraft`, `kPaneEditor`; `tests/test_workshop_panels.cpp` case
`"WUX-13/SC-1: the subject is chosen, and interacting inside the editor does not retarget it"`,
case `"WUX-13/SC-10: editing a pane in a layout related to a current Setup makes it modified"`.
WHY — `agents/decisions/a-subject-is-not-a-selection.md`

## WL-PED-08 — Both lists scroll under the wheel, and the wheel moves no keys

LAW — The list under the pointer scrolls through `pane_editor_move_in`, the keys' own bounded step; the wheel moves a cursor and never the keys, and Escape sheds the selection, not the subject.

MEANS
- a section heading is stepped over, never onto; `on_rows` and the subject are untouched.

PROVEN BY — `workshop/weave.hpp` `pane_editor_move_in`, `pane_editor_wheel`;
`workshop/screen.hpp` `on_rows`; `tests/test_workshop_panels.cpp` case `"QR-18/SC-5: the Pane
Editor's two lists are reached by the wheel past their windows"`, case `"QR-18/SC-1+SC-3: Escape
clears the ordinary selection last, and the Pane Editor's subject stands"`.
WHY — `agents/decisions/a-subject-is-not-a-selection.md`

## Do not assume

- That the Pane Manager's subject is the selected pane, or that choosing a subject selects it:
  neither (WL-PED-02).
- That the Pane Manager is a document editor, a Surface primitive, a general property inspector,
  a wiring editor or a safe mode. Recovery is the picker, `-` here or `0` in the arrangement, the
  default desk and `--isolated`.
