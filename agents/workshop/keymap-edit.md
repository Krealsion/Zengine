# Workshop law — editing a binding

Register `WL-KEY`, the editing half: how one action's keys change while Workshop runs, and what
the keymap file is told. The keymap's own laws — the one truth, admission, the file, the
declarations — are in [`keyboard.md`](keyboard.md), and the ids are one series. One law per
heading; cite by ID. Router: [`../workshop.md`](../workshop.md).

## WL-KEY-17 — An edit is a candidate map judged whole, applied live, then written

LAW — An office asks to set, add, remove, disable or reset an id's keys; the host judges the candidate as a file at load; a refusal changes nothing, in that law's words; an accepted edit is live, then written

MEANS
- removing the last key disables the action aloud; reset restores the declared default;
- a spelled gesture is parsed by the host's grammar, for a chord no pane can capture;
- the answer tells `applied` from `written`; a later declaration or file read is its own event.

DOES NOT MEAN
- that a presenter rebinds: the Hotkeys pane offers, captures or takes a spelling, and asks.

PROVEN BY — `workshop/desktop_seam_vocabulary.hpp` `KeymapEditRequested`,
`KeymapEditAnswered`; `workshop/weave.hpp` `on(KeymapEditRequested)`;
`workshop/weave_desktop.cpp` `on(KeymapEditRequested)`, `authored_spelling`;
`workshop/keymap.hpp` `apply_overrides`, `join_app_rows`, `join_pane_rows`;
`desktop-pane/pane.cpp` `ask_edit`, `keys_chose`, `keys_key`, `keys_action`, `Capture`,
`Typing`; `desktop-pane/vocabulary.hpp` `kMenuModifyPress`, `kMenuModifyType`, `kMenuAddPress`,
`kMenuAddType`, `kMenuRemove`, `kMenuDisable`, `kMenuReset`;
`tests/test_workshop_panes_desktop.cpp` case `"WL-KEY-17: right-click a binding, Modify (press a
key): the change is live at once, written to the file, listed in the table, and the floor
teaches it; the keys stayed where they were"`, case `"WL-KEY-17: Add a key, Remove one, remove
the last (disabled, aloud), Reset -- by menu and by the same door from the keyboard; two
consecutive writes; the table lists every key"`, case `"WL-KEY-17: a collision refuses the edit
in the collision law's own words, and nothing changes -- not the live map, not the file, not the
rows"`.
WHY — `agents/decisions/one-binding-truth.md`

## WL-KEY-18 — A write is judged against the bytes this host read, and a live change is said

LAW — An accepted edit is written only to a file whose bytes this host read or last wrote; an isolated run, a refused file, a failed write or another hand's change leave it live for this run, and said.

MEANS
- the baseline is refreshed after every write of this host's own, so consecutive edits write;
- a file refused at load refuses every edit: never a write over a file the maker has not repaired;
- rows are written as values in authored order; unknown and retired rows are kept.

PROVEN BY — `workshop/weave.hpp` `keymap_bytes_`, `keymap_file_present_`, `keymap_bad_`;
`workshop/weave_handlers.cpp` `load_keymap`; `workshop/weave_desktop.cpp`
`on(KeymapEditRequested)`; `workshop/screen.hpp` `kKeymapUnwrittenKey`; `workshop/persist.hpp`
`write_file`, `read_file`; `workshop/keymap_persist.hpp` `to_text`;
`tests/test_workshop_panes_desktop.cpp` case `"WL-KEY-18: persistence -- an isolated run applies
live and says nothing was written; a file changed by another hand is not overwritten; a file
refused at launch refuses every edit"`, case `"WL-KEY-18: a version-1 file is imported
explicitly, unrelated and unknown rows are preserved as values in order, and the next write is
version 2"`.
WHY — `agents/decisions/one-binding-truth.md`

## Do not assume

- That the JSON's layout is preserved: the values and their order are; the file is rewritten
  in the serializer's own form, and the guide says so.
