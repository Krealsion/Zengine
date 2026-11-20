# Workshop law — layouts

Register `WL-LAYOUT`: several desks, one of them live. One law per heading; cite by ID. Router:
[`../workshop.md`](../workshop.md).

Retired: WL-LAYOUT-03, WL-LAYOUT-04, WL-LAYOUT-05, WL-LAYOUT-06, WL-LAYOUT-07, WL-LAYOUT-09, WL-LAYOUT-11, WL-LAYOUT-12, WL-LAYOUT-13, WL-LAYOUT-14

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

## WL-LAYOUT-08 — `kMaxLayouts` refuses a ninth rather than dropping one

LAW — The ceiling is a bound on work, not a claim about the row: the tab run is composed against whatever the row has and says what it could not paint, so raising the number is a number change.

WHY — `agents/decisions/a-layout-is-a-lifted-value.md`

## WL-LAYOUT-10 — Renaming is a layout operation; saving is a file operation

LAW — Renaming writes no file; saving writes the active layout's desk and names nothing; restoring reads into the active layout only; the association follows a success, never an intention.

MEANS
- both file gestures act on the active layout's association, else on the host's `--setup` path;
- the configured path is the acquisition door, never a default association;
- a save among a batch of edits writes its file once every edit is judged: a refused batch writes no file, a failed write refuses the batch, and one batch saves one file.

WHY — `agents/decisions/a-layout-is-a-lifted-value.md`

## Do not assume

- That a layout is a new kind of thing, or that `setup.active` is an index: a layout is a
  `Setup`, and `active` is still the live desk every consumer reads (WL-LAYOUT-01).
- That switching layouts reloads a provider, or that a pane in two layouts is two panes.

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

### WL-LAYOUT-08
`workshop/setup.hpp` kMaxLayouts
`tests/test_workshop_screen.cpp` "a new layout appends however far into the run you stand"
`tests/test_workshop_persistence.cpp` "a current run this Workshop could not have made is refused as
CURRENT data"

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
