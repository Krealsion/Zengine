# Workshop law — layouts

Register `WL-LAYOUT`: several desks, one of them live. One law per heading; cite by ID. Router:
[`../workshop.md`](../workshop.md).

## WL-LAYOUT-01 — A layout is a desk plus its optional relationship to one Setup file

LAW — The setup state is the live desk every consumer reads, its association, the shelf of inactive layouts as values only, the live one's position, and the rename editor; a layout is a desk plus its link.

MEANS
- no pane, provider, room or selection belongs to a shelved layout;
- `Layout` is desk + link, one struct rather than two parallel vectors whose indices could drift.

WHY — `agents/decisions/a-layout-is-a-lifted-value.md`

## WL-LAYOUT-02 — There is no `on_file` and no `saved()`

LAW — Every layout owns its own association, one function decides the verdict — `none`, `current` or `modified` — and the verdict is a claim about Workshop's knowledge, never the disk.

MEANS
- no stat, no reload, no watcher: a paint path never goes near a filesystem.

WHY — `agents/decisions/a-layout-is-a-lifted-value.md`

## WL-LAYOUT-03 — The run is the shelf with one element lifted out

LAW — Activating puts the lifted value back at its position, takes out the destination and moves nothing else, never a swap; adding appends a blank desk; removing takes the next neighbour.

MEANS
- a swap reorders the run at every look: green on the first hop, red on the second;
- `add_layout` makes `default_setup()` with no association: new means new;
- removing the last layout takes the previous neighbour instead.

WHY — `agents/decisions/a-layout-is-a-lifted-value.md`

## WL-LAYOUT-04 — Duplicate copies the desk and clears the association

LAW — Duplicating copies the desk, inserts after its source, makes it live and always clears the association; moving changes order only; renaming writes one name and no file.

MEANS
- the copy keeps the name: duplicate names are legal, and position is the identity;
- move and duplicate go through the inverse pair; the live position is computed, never searched.

WHY — `agents/decisions/a-layout-is-a-lifted-value.md`

## WL-LAYOUT-05 — A switch is `restore_setup` minus the file read

LAW — A switch activates, then applies the desk through the one door membership changes through, then says one sentence and repaints — a restore minus the file read, so it behaves exactly as a restore.

WHY — `agents/decisions/a-layout-is-a-lifted-value.md`

## WL-LAYOUT-06 — Per-layout is the value's own fields and nothing else

LAW — Participation, place, extent, front order, a pane's settings, the name, the locks, pins and baseline are per-layout; all else is one Workshop-global truth a switch does not copy, clear or revalidate.

MEANS
- the catalog, providers and their state (the Editor's document among them), the marks;
- recipes, the project anchor, the clipboard, the keymap, the window, selection and keyboard.

DOES NOT MEAN
- that a pane's own state is a setting: a switch hands a pane its settings and copies none of it.

WHY — `agents/decisions/a-layout-is-a-lifted-value.md`

## WL-LAYOUT-07 — One pane in two layouts is one pane and one provider

LAW — Leaving a layout withdraws the presentation (`close_kind`; no unload, nothing sent), entering one re-seats it and re-earns its room, and an inactive layout is an unread value, never walked.

MEANS
- a pane in both layouts at the same prose capacity hears no grant and no ask;
- ...and is handed its settings only when the two layouts keep different ones (WL-SETTING-04).

WHY — `agents/decisions/a-layout-is-a-lifted-value.md`

## WL-LAYOUT-08 — `kMaxLayouts` refuses a ninth rather than dropping one

LAW — The ceiling is a bound on work, not a claim about the row: the tab run is composed against whatever the row has and says what it could not paint, so raising the number is a number change.

WHY — `agents/decisions/a-layout-is-a-lifted-value.md`

## WL-LAYOUT-09 — The files did not move

LAW — `s` writes the live layout to a setup file and `r` reads one into the live layout; neither touches the shelf, a Setup file still means one desk, and the session carries the whole run.

WHY — `agents/decisions/a-layout-is-a-lifted-value.md`

## WL-LAYOUT-10 — Renaming is a layout operation; saving is a file operation

LAW — Renaming writes no file; saving writes the active layout's desk and names nothing; restoring reads into the active layout only; the association follows a success, never an intention.

MEANS
- both file gestures act on the active layout's association, else on the host's `--setup` path;
- the configured path is the acquisition door, never a default association;
- a save among a batch of edits writes its file once every edit is judged: a refused batch writes no file, a failed write refuses the batch, and one batch saves one file.

WHY — `agents/decisions/a-layout-is-a-lifted-value.md`

## WL-LAYOUT-11 — The shared-artifact law

LAW — When Workshop successfully learns what a Setup file holds, `adopt_known_setup` updates the baseline of every association to that path, compared by bytes, and establishes nothing elsewhere.

DOES NOT MEAN
- that a path is canonicalised before it is compared — associations compare by bytes.

WHY — `agents/decisions/a-layout-is-a-lifted-value.md`

## WL-LAYOUT-12 — The run leaves the state through one inverse pair

LAW — The run leaves the state as a new vector with the live value back in place and returns by lifting one out again, one inverse pair; the durable owner never touches the shelf or the position.

WHY — `agents/decisions/a-layout-is-a-lifted-value.md`

## WL-LAYOUT-13 — The layout gestures are ordinary rows in command mode

LAW — `layout.next` (`.`), `layout.previous` (`,`), `layout.new` (`=`), `layout.remove` (`^w`): three unshifted printables and one plain ctrl chord, because the POSIX wire carries nothing else in that family.

MEANS
- removal is the one chord because discarding a layout cannot be undone;
- `x` is refused: it once closed the Builder, and a weaver's hand may still mean that.

WHY — `agents/decisions/a-layout-is-a-lifted-value.md`

## Do not assume

- That a layout is a new kind of thing, or that `setup.active` is an index: a layout is a
  `Setup`, and `active` is still the live desk every consumer reads (WL-LAYOUT-01).
- That switching layouts reloads a provider, or that a pane in two layouts is two panes
  (WL-LAYOUT-07).

## Proven by

### WL-LAYOUT-01
`workshop/setup.hpp` Setup
`workshop/layouts/run.hpp` SetupState::active SetupState SetupState::active_link SetupState::shelved
SetupState::active_at Layout SetupLink
`workshop/screen.hpp` Session::setup
`workshop/layouts/owner.cpp` send_live on(LayoutEditRequested)
`workshop/live_desk.hpp` LiveDesk
`workshop/weave_session.cpp` on(LiveLayout) install_live ask_layout on(LayoutEditAnswered)
`tests/test_workshop_screen.cpp` "a layout is a Setup and the run is the shelf plus the live value"
`tests/test_workshop_persistence.cpp` "the run and the lifted-active representation are one fact"

### WL-LAYOUT-02
`workshop/layouts/run.hpp` link_status SetupLink SetupLink::known
`workshop/layouts/store.cpp` link_note
`workshop/setup.hpp` setup_link
`tests/test_workshop_screen.cpp` "the three verdicts, and what makes a fresh desk `none`"
`tests/test_workshop_persistence.cpp` "the standing verdict performs no filesystem read"

### WL-LAYOUT-03
`workshop/setup.hpp` default_setup
`workshop/layouts/run.hpp` SetupState::active_at activate_layout add_layout remove_layout SetupState
`workshop/weave_session.cpp` new_layout drop_layout
`tests/test_workshop_screen.cpp` "switching never reorders the run, and the live value never
doubles" "new is BLANK and appended, and it is a value of its own" "removing takes the next
neighbour, the previous only at the end"

### WL-LAYOUT-04
`workshop/layouts/run.hpp` duplicate_layout move_layout rename_layout layout_run install_layout_run
`workshop/weave_session.cpp` commit_layout_rename duplicate_layout shift_layout
`tests/test_workshop_screen.cpp` "duplicate copies the desk exactly and always clears the
association" "rename writes one layout's name and touches nothing else" "moving a layout changes
order and nothing else"

### WL-LAYOUT-05
`workshop/weave_session.cpp` switch_layout apply_setup
`workshop/layouts/run.hpp` activate_layout
`workshop/layouts/owner.cpp` on(SetupApplyRequested)
`tests/test_workshop_host.cpp` "a switch returns membership, geometry and front order as authored"
`tests/test_workshop_panes_window.cpp` "leaving a layout withdraws a presentation and unloads
nothing"

### WL-LAYOUT-06
`workshop/setup.hpp` Setup SetupPane::settings check_layout_own
`workshop/weave_session.cpp` switch_layout
`tests/test_workshop_host.cpp` "a switch touches no Workshop-global fact" "a new layout is blank and
duplicates no Workshop-global state"
`tests/test_workshop_panes_settings.cpp` "a layout switch hands a pane its new layout's settings
once, and only when they differ"

### WL-LAYOUT-07
`workshop/weave_session.cpp` apply_setup
`workshop/panes.hpp` close_kind
`workshop/layouts/run.hpp` SetupState::shelved
`tests/test_workshop_panes_window.cpp` "a pane in two layouts is one pane, one provider, one room"
"an inactive layout's rows are dormant, not maintained"
`tests/test_workshop_panes_settings.cpp` "a layout switch hands a pane its new layout's settings
once, and only when they differ"

### WL-LAYOUT-08
`workshop/setup.hpp` kMaxLayouts
`tests/test_workshop_screen.cpp` "a new layout appends however far into the run you stand"
`tests/test_workshop_persistence.cpp` "a current run this Workshop could not have made is refused as
CURRENT data"

### WL-LAYOUT-09
`workshop/weave_session.cpp` switch_layout save_setup restore_setup
`workshop/layouts/session_persist.hpp` WorkshopLayout
`tests/test_workshop_persistence.cpp` "`s` writes the live layout and leaves the shelf alone" "`r`
restores into the live layout and clears no shelf" "the whole layout run rides the session, and
comes back"

### WL-LAYOUT-10
`workshop/keymap.hpp` layout.rename setup.name setup.restore
`workshop/weave_session.cpp` open_layout_rename commit_layout_rename save_setup restore_setup
`workshop/weave.hpp` HostContext::setup_path
`workshop/layouts/run.hpp` rename_layout
`workshop/live_desk.hpp` LayoutNaming
`tests/test_workshop_host.cpp` "a double-click on a tab renames THAT layout, and writes no file"
`tests/test_workshop_persistence.cpp` "`s` establishes the association only after a successful
write" "`r` establishes on success and changes nothing on refusal"
`workshop/layouts/store.cpp` LayoutStore::edit LayoutStore::edit_one
`tests/test_workshop_screen.cpp` "a batch's setup file is written once every edit in it is judged, and not at all when one is
refused" "a batch whose setup file cannot be written is refused whole, the run as it was"

### WL-LAYOUT-11
`workshop/layouts/run.hpp` adopt_known_setup SetupLink
`workshop/weave_session.cpp` save_setup restore_setup
`tests/test_workshop_persistence.cpp` "two layouts sharing one artifact cannot both claim `current`"
`tests/test_workshop_screen.cpp` "switching carries the association, sharing keeps it honest"

### WL-LAYOUT-12
`workshop/layouts/run.hpp` SetupState::shelved SetupState::active_at layout_run install_layout_run
`tests/test_workshop_persistence.cpp` "the run and the lifted-active representation are one fact"
"installing a run touches nothing else the session owns" "every position in the run is a position a
session can be saved at"

### WL-LAYOUT-13
`workshop/keymap.hpp` layout.next layout.previous layout.new layout.remove
`input/translate.hpp` terminal_byte_scancode
`tests/test_workshop_host.cpp` "four ordinary command-mode actions reach the layout shelf" "the
layout gestures stay in command mode"
