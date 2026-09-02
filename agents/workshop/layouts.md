# Workshop law — layouts

Register `WL-LAYOUT`: several desks, one of them live. One law per heading; cite by ID. Router:
[`../workshop.md`](../workshop.md).

## WL-LAYOUT-01 — A layout is a desk plus its optional relationship to one Setup file

LAW — The setup state is the live desk every consumer reads, its association, the shelf of inactive layouts as values only, the live one's position, and the rename editor; a layout is a desk plus its link.

MEANS
- no panel, provider, room or selection belongs to a shelved layout;
- `Layout` is desk + link, one struct rather than two parallel vectors whose indices could drift.

PROVEN BY — `workshop/setup.hpp` `Setup`, `active`, `SetupState`, `active_link`, `shelved`,
`active_at`, `naming`, `Layout`, `SetupLink`; `tests/test_workshop_screen.cpp` case
`"WUX-9/SC-2: a layout is a Setup and the run is the shelf plus the live value"`;
`tests/test_workshop_persistence.cpp` case `"WUX-10/SC-9: the run and the lifted-active
representation are one fact"`.
WHY — `agents/decisions/a-layout-is-a-lifted-value.md`

## WL-LAYOUT-02 — There is no `on_file` and no `saved()`

LAW — Every layout owns its own association, one function decides the verdict — `none`, `current` or `modified` — and the verdict is a claim about Workshop's knowledge, never the disk.

MEANS
- no stat, no reload, no watcher: a paint path never goes near a filesystem.

PROVEN BY — `workshop/setup.hpp` `link_status`, `SetupLink`, `known`;
`tests/test_workshop_screen.cpp` case `"WUX-11/SC-7: the three verdicts, and what makes a fresh
desk `none`"`; `tests/test_workshop_persistence.cpp` case `"WUX-11/SC-7: the standing verdict
performs no filesystem read"`.
WHY — `agents/decisions/a-layout-is-a-lifted-value.md`

## WL-LAYOUT-03 — The run is the shelf with one element lifted out

LAW — Activating puts the lifted value back at its position, takes out the destination and moves nothing else, never a swap; adding appends a blank desk; removing takes the next neighbour.

MEANS
- a swap reorders the run at every look: green on the first hop, red on the second;
- `add_layout` makes `default_setup()` with no association: new means new;
- removing the last layout takes the previous neighbour instead.

PROVEN BY — `workshop/setup.hpp` `active_at`, `activate_layout`, `add_layout`,
`remove_layout`, `default_setup`; `tests/test_workshop_screen.cpp` case `"WUX-9/SC-3: switching
never reorders the run, and the live value never doubles"`, case `"WUX-11/SC-1: new is BLANK and
appended, and it is a value of its own"`, case `"WUX-9/SC-11: removing takes the next neighbour,
the previous only at the end"`.
WHY — `agents/decisions/a-layout-is-a-lifted-value.md`

## WL-LAYOUT-04 — Duplicate copies the desk and clears the association

LAW — Duplicating copies the desk, inserts after its source, makes it live and always clears the association; moving changes order only; renaming writes one name and no file.

MEANS
- the copy keeps the name: duplicate names are legal, and position is the identity;
- move and duplicate go through the inverse pair; the live position is computed, never searched.

PROVEN BY — `workshop/setup.hpp` `duplicate_layout`, `move_layout`, `rename_layout`,
`layout_run`, `install_layout_run`; `tests/test_workshop_screen.cpp` case `"WUX-11/SC-2:
duplicate copies the desk exactly and always clears the association"`, case `"WUX-11/SC-3:
rename writes one layout's name and touches nothing else"`, case `"WUX-11/SC-4: moving a layout
changes order and nothing else"`.
WHY — `agents/decisions/a-layout-is-a-lifted-value.md`

## WL-LAYOUT-05 — A switch is `restore_setup` minus the file read

LAW — A switch activates, then applies the desk through the one door membership changes through, then says one sentence and repaints — a restore minus the file read, so it behaves exactly as a restore.

PROVEN BY — `workshop/weave.hpp` `switch_layout`, `apply_setup`; `workshop/setup.hpp`
`activate_layout`; `tests/test_workshop_panels.cpp` case `"WUX-9/SC-4: a switch returns
membership, geometry and front order as authored"`; `tests/test_workshop_panes_window.cpp` case
`"WUX-9/SC-6: leaving a layout withdraws a presentation and unloads nothing"`.
WHY — `agents/decisions/a-layout-is-a-lifted-value.md`

## WL-LAYOUT-06 — Per-layout is the value's own fields and nothing else

LAW — Participation, authored place, extent, front order and the name are per-layout; everything else is one Workshop-global truth a switch does not copy, clear or revalidate.

MEANS
- the catalog, providers and their state, the Editor's document, the browser's location, marks;
- recipes, the project anchor, the clipboard, the keymap, the window, selection and keyboard.

PROVEN BY — `workshop/setup.hpp` `Setup`; `workshop/weave.hpp` `switch_layout`;
`tests/test_workshop_panels.cpp` case `"WUX-9/SC-5: a switch touches no Workshop-global fact"`,
case `"WUX-11/SC-1: a new layout is blank and duplicates no Workshop-global state"`.
WHY — `agents/decisions/a-layout-is-a-lifted-value.md`

## WL-LAYOUT-07 — One pane in two layouts is one pane and one provider

LAW — Leaving a layout withdraws the presentation (`close_panel`; no unload, nothing sent), entering one re-seats it and re-earns its room, and an inactive layout is an unread value, never walked.

MEANS
- a pane in both layouts at the same prose capacity hears nothing: no grant, no ask.

PROVEN BY — `workshop/weave.hpp` `apply_setup`; `workshop/panel.hpp` `close_panel`;
`workshop/setup.hpp` `shelved`;
`tests/test_workshop_panes_window.cpp` case `"WUX-9/SC-6: a pane in two layouts is one pane, one
provider, one room"`, case `"WUX-9/SC-15: an inactive layout's rows are dormant, not
maintained"`.
WHY — `agents/decisions/a-layout-is-a-lifted-value.md`

## WL-LAYOUT-08 — `kMaxLayouts` refuses a ninth rather than dropping one

LAW — The ceiling is a bound on work, not a claim about the row: the tab run is composed against whatever the row has and says what it could not paint, so raising the number is a number change.

PROVEN BY — `workshop/setup.hpp` `kMaxLayouts`; `tests/test_workshop_screen.cpp` case
`"WUX-9/SC-3+SC-11: a new layout appends however far into the run you stand"`;
`tests/test_workshop_persistence.cpp` case `"WUX-10/SC-12: a current run this Workshop could not
have made is refused as CURRENT data"`.
WHY — `agents/decisions/a-layout-is-a-lifted-value.md`

## WL-LAYOUT-09 — The files did not move

LAW — `s` writes the live layout to a setup file and `r` reads one into the live layout; neither touches the shelf, a Setup file still means one desk, and the session carries the whole run.

PROVEN BY — `workshop/weave.hpp` `switch_layout`; `workshop/session_persist.hpp`
`WorkshopLayout`; `tests/test_workshop_persistence.cpp` case `"WUX-9/SC-12: `s` writes the live
layout and leaves the shelf alone"`, case `"WUX-9/SC-12: `r` restores into the live layout and
clears no shelf"`, case `"WUX-10/SC-13: the whole layout run rides the session, and comes
back"`.
WHY — `agents/decisions/a-layout-is-a-lifted-value.md`

## WL-LAYOUT-10 — Renaming is a layout operation; saving is a file operation

LAW — Renaming writes no file; saving writes the active layout's desk and names nothing; restoring reads into the active layout only; the association follows a success, never an intention.

MEANS
- both file gestures act on the active layout's association, else on the host's `--setup` path;
- the configured path is the acquisition door, never a default association.

PROVEN BY — `workshop/keymap.hpp` `layout.rename`, `setup.name`, `setup.restore`;
`workshop/weave.hpp` `open_layout_rename`; `tests/test_workshop_panels.cpp` case `"WUX-11/SC-3: a
double-click on a tab renames THAT layout, and writes no file"`;
`tests/test_workshop_persistence.cpp` case `"WUX-11/SC-9: `s` establishes the association only
after a successful write"`, case `"WUX-11/SC-10+SC-11: `r` establishes on success and changes
nothing on refusal"`.
WHY — `agents/decisions/a-layout-is-a-lifted-value.md`

## WL-LAYOUT-11 — The shared-artifact law

LAW — When Workshop successfully learns what a Setup file holds, `adopt_known_setup` updates the baseline of every association to that path, compared by bytes, and establishes nothing elsewhere.

DOES NOT MEAN
- that a path is canonicalised before it is compared — associations compare by bytes.

PROVEN BY — `workshop/setup.hpp` `adopt_known_setup`; `tests/test_workshop_persistence.cpp` case
`"WUX-11/SC-12: two layouts sharing one artifact cannot both claim `current`"`;
`tests/test_workshop_screen.cpp` case `"WUX-11/SC-6+SC-12: switching carries the association,
sharing keeps it honest"`.
WHY — `agents/decisions/a-layout-is-a-lifted-value.md`

## WL-LAYOUT-12 — The run leaves the state through one inverse pair

LAW — The run leaves the state as a new vector with the live value back in place and returns by lifting one out again, one inverse pair; the durable owner never touches the shelf or the position.

PROVEN BY — `workshop/setup.hpp` `shelved`, `active_at`, `layout_run`, `install_layout_run`;
`tests/test_workshop_persistence.cpp` case `"WUX-10/SC-9: the run and the lifted-active
representation are one fact"`, case `"WUX-10/SC-9: installing a run touches nothing else the
session owns"`, case `"WUX-10/SC-8: every position in the run is a position a session can be
saved at"`.
WHY — `agents/decisions/a-layout-is-a-lifted-value.md`

## WL-LAYOUT-13 — The layout gestures are ordinary rows in command mode

LAW — `layout.next` (`.`), `layout.previous` (`,`), `layout.new` (`=`), `layout.remove` (`^w`): three unshifted printables and one plain ctrl chord, because the POSIX wire carries nothing else in that family.

MEANS
- removal is the one chord because discarding a layout cannot be undone;
- `x` is refused: it once closed the Builder, and a maker's hand may still mean that.

PROVEN BY — `workshop/keymap.hpp` `layout.next`, `layout.previous`, `layout.new`,
`layout.remove`; `input/translate.hpp` `terminal_byte_scancode`; `tests/test_workshop_panels.cpp`
case `"WUX-9/SC-10: four ordinary command-mode actions reach the layout shelf"`, case
`"WUX-9/SC-10: the layout gestures stay in command mode"`.
WHY — `agents/decisions/a-layout-is-a-lifted-value.md`

## Do not assume

- That a layout is a new kind of thing, or that `setup.active` is an index: a layout is a
  `Setup`, and `active` is still the live desk every consumer reads (WL-LAYOUT-01).
- That switching layouts reloads a provider, or that a pane in two layouts is two panes
  (WL-LAYOUT-07).
