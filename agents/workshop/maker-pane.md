# Workshop law — a pane a weaver made

Register `WL-MAKER`: a pane a weaver makes from data is a described view, and Workshop holds none
of it. One law per heading; cite by ID. Router: [`../workshop.md`](../workshop.md). How a view is
read, run and made is [`../view.md`](../view.md).

Retired: WL-MAKER-01, WL-MAKER-03, WL-MAKER-04, WL-MAKER-05, WL-MAKER-06, WL-MAKER-07, WL-MAKER-08, WL-MAKER-09, WL-MAKER-10, WL-MAKER-13, WL-MAKER-14.

## WL-MAKER-11 — The Pane Manager's way to make a pane is the View Builder

LAW — The desktop Pane Manager's `n` (`launcher.new`) shows the View Builder through the host's launch door: a weaver's own pane is a described view, and Workshop holds no kind or definition for it.

MEANS
- the desktop spells `zengine.view.builder/view-builder`, an application's choice, as Terminal's;
- a Workshop without a View Builder answers in the launch door's words, and nothing is made;
- a keymap row for `pane-creator.new` moves `launcher.new`; the Creator's other ids are retired.

DOES NOT MEAN
- that Workshop runs views: the view host runs them, granted what each description implies.

PROVEN BY — `desktop-pane/vocabulary.hpp` `kActionNew`; `desktop-pane/pane.cpp` `pane_rows`,
`kViewBuilderOffice`, `kViewBuilderPane`, `launch`; `workshop/keymap.hpp` `kRenamedActions`,
`kRetiredActions`, `kToViewBuilder`, `join_pane_rows`; `tests/test_workshop_panes_actions.cpp`
case `"WL-MAKER-11: the shipped Pane Manager's `n` shows the View Builder through the host's
launch door, and a Workshop without one says so in the door's words"`, case `"a keymap row
written for the Pane Creator's `n` moves the Pane Manager's, and one for its other acts is kept
and said retired"`.
WHY — `agents/decisions/a-pane-made-from-data-is-a-view.md`

## WL-MAKER-12 — A pane's interior is a capture

LAW — Every pane subject's interior is one read-only row: a code-backed capture, the provider's own, or `unresolved -- nothing to inspect`; Info edits no region of any pane.

DOES NOT MEAN
- that anything is decompiled or inferred: no controls, no pretence.

PROVEN BY — `workshop/screen_pane_subject.cpp` `interior_capture_text`, `pane_subject_rows`;
`tests/test_workshop_host.cpp` case `"a code-backed subject's interior is a read-only capture,
and an unresolved one is nothing to inspect"`.
WHY — `agents/decisions/a-pane-made-from-data-is-a-view.md`

## WL-MAKER-15 — A desk naming a Pane Creator pane names its view

LAW — A saved desk naming `zengine.workshop.maker/<name>` is read as naming `<view name>/view`, the pane its file offers read as a view, keeping the row's place, size and front; the load says so once.

MEANS
- one rule names the view for the file's reader and the desk's (`view_name_of_creator_pane`);
- the setup file, a session's every desk and every remembered link meet it, before the setup law;
- no format version moves: a `PaneRef` is opaque data, rewritten as a pane that changed hands is.

DOES NOT MEAN
- that Workshop runs the view: the View Builder runs it again at a launch ([view](../view.md)).

PROVEN BY — `workshop/pane_migration.hpp` `convert_retired_panes`, `view_of_creator_pane`,
`kViewPane`, `Converted::creator`, `converted_note`; `view/creator_pane.hpp`
`kCreatorPaneProvider`, `view_name_of_creator_pane`; `workshop/setup_persist.hpp` `setup_in`;
`tests/test_workshop_persistence.cpp` case `"WL-MAKER-15: a desk naming a pane the Pane Creator
made names its view, where the pane stood, and the load says so once"`.
WHY — `agents/decisions/a-pane-made-from-data-is-a-view.md`

## Do not assume

- That a pane made from data is Workshop's: it is a view the view host runs in the office its
  description names, and Workshop seats it as it seats any provider's pane (WL-MAKER-11,
  WL-MAKER-15).
