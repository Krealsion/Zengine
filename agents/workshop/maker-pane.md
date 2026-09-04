# Workshop law — the maker's pane

Register `WL-MAKER`: a pane may exist because a maker described one. One law per heading; cite by
ID. Router: [`../workshop.md`](../workshop.md).

## WL-MAKER-01 — The first pane whose interior is authored data

LAW — A definition is a name, a list of text regions — each an id, a kind, a place, a size and a line of text — and a mint: the first pane implementation whose interior is authored data; one is open.

MEANS
- one admitted kind (`region_kind::kText`), ids minted and never reused, geometry in sub-units;
- geometry is relative to the pane's interior, never to the canvas.

DOES NOT MEAN
- that this is the ontology of pane: a built-in's interior is code, a provider's behind the seam;
- that a widget set, controls, anchors, fill, nesting or a second renderer may grow on this value.

PROVEN BY — `workshop/pane_definition.hpp` `PaneDefinition`, `TextRegion`, `region_kind::kText`,
`MakerPane`, `kMaxRegions`, `kMaxMakerPaneNameLen`, `kRegionSubMax`; `workshop/panel.hpp`
`Panels::maker`; `tests/test_workshop_panels_creator.cpp` case `"WUX-14/SC-6: a definition is a
name and a list of text regions with stable ids"`, case `"WUX-14/SC-7: the whole-definition law
refuses what no door could have made"`, case `"WUX-14/SC-12: a code-backed subject's interior is a
read-only capture, and an unresolved one is nothing to inspect"`.
WHY — `agents/decisions/one-way-a-pane-can-be-implemented.md`

## WL-MAKER-03 — The identity is minted from the name and from nothing else

LAW — The maker's panes live in a Workshop-owned provider namespace no office may offer a pane in, and a reference resolves to the maker's kind exactly when its pane name equals the open definition's name.

MEANS
- `admit_pane_offer` refuses an offer in that namespace; nothing is addressed to it;
- a definition not open leaves every row naming it retained and `unresolved`, as a stranger's;
- there is no singleton `Defined` ref whose meaning follows the open file.

PROVEN BY — `workshop/panel.hpp` `kMakerPaneProvider`, `zengine.workshop.maker`,
`kMakerPaneKind`; `workshop/setup.hpp` `admit_pane_offer`, `resolve_pane`, `maker_pane_ref`;
`workshop/pane_definition.hpp` `PaneDefinition::name`; `tests/test_workshop_panels_creator.cpp`
case `"WUX-14/SC-4: a maker pane's identity is its name under Workshop's namespace -- not a
singleton that follows the open file"`, case `"WUX-14/SC-16+SC-17: save, quit, relaunch -- the
same pane returns on the same layout by its reference; remove the file and the row is kept
unresolved"`.
WHY — `agents/decisions/one-way-a-pane-can-be-implemented.md`

## WL-MAKER-04 — `kMakerPaneKind` is a handle, not an identity

LAW — `kMakerPaneKind` is a third kind class beside built-ins and runtime handles; the resolution table takes the whole `Panels`, or a maker's pane would count as unresolved beneath a pane they can see.

MEANS
- `is_maker_kind`, `kind_takes_keyboard` and `placement_of` are the arms; nothing else switches;
- `resolve_pane`, `resolvable`, `seat_panes`, `unresolved_panes`, `setup_rest_text` take `Panels`.

PROVEN BY — `workshop/panel.hpp` `kMakerPaneKind`, `is_maker_kind`, `kind_takes_keyboard`,
`placement_of`, `kFirstRuntimeKind`; `workshop/setup.hpp` `resolve_pane`, `resolvable`,
`seat_panes`, `unresolved_panes`, `resolve_builtin_pane`; `workshop/screen.hpp` `setup_rest_text`;
`tests/test_workshop_panels_creator.cpp` case "WUX-14/SC-1+SC-3+SC-9: `n` in the Pane Manager
makes a named pane from data, and it lives on the desk exactly as every other pane does", case
`"WUX-14/SC-19: a run with no maker pane is the run it always was"`.
WHY — `agents/decisions/one-way-a-pane-can-be-implemented.md`

## WL-MAKER-05 — The pane on the desk is the preview; there is no second renderer

LAW — The desk pane is the preview and there is no second renderer: the maker's pane is one more arm of the ordinary pane painter — bounds, interior, each region presented, then owned-ground regions.

MEANS
- `present_region`: interior origin plus authored place, clipped, then `fit_region_subs`;
- a region authored at 126 px sits at pixel 126 of the interior; a terminal reads `~10 cells`;
- too small for the face is the face's own answer; nothing rewrites the authored number to fit.

PROVEN BY — `workshop/screen.hpp` `paint_panels`, `bounds_of`, `pane_inside`,
`paint_maker_pane`, `present_region`, `fit_region_subs`, `clip_to_fine`; `surface/vocabulary.hpp`
`kGroundOwn`; `tests/test_workshop_panels_creator.cpp` case `"WUX-14/SC-8: a region is placed
relative to the pane's INTERIOR and painted through the ordinary pane path in cells"`, case
`"WUX-14/SC-8: one authored fine value, read in pixels on the window and projected to cells on a
terminal, and looking writes nothing back"`, case `"WUX-14/SC-8: a region too small for the face
is the face's own answer, and the authored value is not rewritten to fit"`.
WHY — `agents/decisions/one-way-a-pane-can-be-implemented.md`

## WL-MAKER-06 — Looking never authors

LAW — Looking never authors: every interior row is read at display time, the resolved and shown rows are the presentation re-run, and the region mark is derived from that resolution and writes nothing.

MEANS
- `paint_creator_region_mark` is a later plane: an accent rect at the exact resolved bounds;
- proven by byte identity of `to_text` across faces, extents and repaints.

PROVEN BY — `workshop/screen.hpp` `maker_region`, `present_region`, `paint_creator_region_mark`;
`workshop/pane_definition_persist.hpp` `to_text`, `to_file`;
`tests/test_workshop_panels_creator.cpp` case `"WUX-14/SC-10: the Pane Creator marks the region it
is editing on the pane itself, from the same resolution, and writes nothing"`, case `"WUX-14/SC-8:
one authored fine value, read in pixels on the window and projected to cells on a terminal, and
looking writes nothing back"`.
WHY — `agents/decisions/one-way-a-pane-can-be-implemented.md`

## WL-MAKER-07 — One owner door per fact

LAW — One owner door per fact: a region's text and its four numbers go through the definition's own doors, a number read in the face's unit and refused, never clamped, per axis; `-` is refused in words.

MEANS
- a region has no default mode: those are ordinary values the maker reads and retypes;
- the manager's rows are adapters, the definition's doors are the law, Info owns none of it.

PROVEN BY — `workshop/screen.hpp` `write_region_text`, `write_region_axis`, `parse_face_amount`;
`workshop/pane_definition.hpp` `set_region_text`, `author_region_axis`, `check_maker_pane_name`;
`tests/test_workshop_panels_creator.cpp` case `"WUX-14/SC-11: Text and the four numbers are edited
through the definition's doors, refused in words, and clamped never"`.
WHY — `agents/decisions/one-way-a-pane-can-be-implemented.md`

## WL-MAKER-08 — The lifecycle is the source editor's

LAW — One session-owned open definition, dirty derived by comparison with its saved copy; a dirty definition refuses a new pane, a replacing open and an orderly quit, and there is one open door.

MEANS
- `close_panel` never touches `Panels::maker`; a never-saved pane is dirty by arithmetic;
- refusals name `s` and `ctrl+d`; `open_maker_pane` is spent by startup and nothing else yet;
- a refused file at the host's path is a wall (`kPaneWallKey`, `pane_refused_`) the save honours.

PROVEN BY — `workshop/weave.hpp` `quit`, `open_maker_pane`, `save_maker_pane`,
`discard_maker_pane_edits`, `WorkshopWeave::pane_refused_`, `new_maker_pane`,
`HostContext::pane_path`, `host_pane_path`, `load_pane_definition`; `workshop/screen.hpp`
`kPaneWallKey`; `workshop/pane_definition_persist.hpp` `LoadedDefinition`; `workshop/panel.hpp`
`Panels::maker`; `workshop/pane_definition.hpp` `MakerPane`;
`tests/test_workshop_panels_creator.cpp` case `"WUX-14/SC-14: dirty pane truth refuses the quit, a
second new pane and a replacing open until the maker saves or discards"`, case `"WUX-14/SC-14: the
discard door puts a saved pane back to its file, and closes a pane that was never saved while
keeping its row"`, case `"WUX-14/SC-15: a malformed file cannot replace a live definition, and a
refused file is never written over"`.
WHY — `agents/decisions/one-way-a-pane-can-be-implemented.md`

## WL-MAKER-09 — Startup order is the relaunch story

LAW — The pane definition is loaded before the last session is restored, because the restore seats only what resolves at that moment; the session carries the row and no byte of the interior.

PROVEN BY — `workshop/weave.hpp` `apply_setup`, `load_pane_definition`,
`restore_last_session`, `WorkshopWeave::pane_loaded_`; `tests/test_workshop_panels_creator.cpp`
case `"WUX-14/SC-16+SC-17: save, quit, relaunch -- the same pane returns on the same layout by its
reference; remove the file and the row is kept unresolved"`, case `"WUX-14/SC-9: the maker's pane
is edited, ordered and removed by the doors every pane has, and comes back through the session by
its reference"`.
WHY — `agents/decisions/one-way-a-pane-can-be-implemented.md`

## WL-MAKER-10 — The pane file, and what it cannot say

LAW — The pane file is `zengine-workshop-pane` version 1: `WorkshopPaneDefinition v1`/`WorkshopPaneRegion v1`, a derived 64 KiB ceiling, the family's safe write; what the file cannot say is the enforcement.

MEANS
- a name, a mint, and per region an id, a kind word, four numbers and a line of text, by field;
- both headers are tripwired against every bus, kernel, grant, operator and keymap spelling;
- an office in the maker namespace hears no `PaneRoom`, `PanePressed`, `PaneKey` or `PaneWheel`.

PROVEN BY — `workshop/pane_definition_persist.hpp` `zengine-workshop-pane`, `kFormatVersion`,
`WorkshopPaneDefinition`, `WorkshopPaneRegion`, `kMaxPaneDefinitionBytes`, `kMaxRegionFileBytes`,
`from_text`; `tests/test_workshop_panels_creator.cpp` case `"WUX-14/SC-13: the pane file
round-trips, refuses by number and by shape, and holds nothing but the definition"`, case
`"WUX-14/SC-18: the definition and its file are structurally unable to act"`, case `"WUX-14/SC-18:
loading a definition mounts nothing, offers nothing and sends nothing through the provider seam"`.
WHY — `agents/decisions/one-way-a-pane-can-be-implemented.md`

## WL-MAKER-11 — The Pane Creator is the maker-facing workflow

LAW — `n` in the Pane Manager opens a name prompt, Return makes the pane, `s` saves and `ctrl+d` discards; the default region is authored the moment it exists.

MEANS
- `kNewRegionX/Y/W/H` are 0, 0, 24 cells, 2 cells: two tall so the face sets one row of type;
- at the minimum composition a new pane lands `waiting`, says so, and stays the editable subject.

PROVEN BY — `workshop/keymap.hpp` `KeyContext::kPaneNaming`; `workshop/screen.hpp`
`Session::pane_naming`, `PaneNaming`; `workshop/weave.hpp` `new_maker_pane`, `save_maker_pane`,
`discard_maker_pane_edits`, `naming_line`; `workshop/pane_definition.hpp` `kNewRegionX`;
`tests/test_workshop_panels_creator.cpp` case `"WUX-14: the name prompt refuses a bad name in
words and keeps it, cancels cleanly, and swallows its own trigger"`, case `"WUX-14: at the minimum
composition a new pane lands waiting, is still the subject, and is still editable"`.
WHY — `agents/decisions/one-way-a-pane-can-be-implemented.md`

## WL-MAKER-12 — The code-backed answer is a capture

LAW — For every subject that is not the maker's pane, the interior is one read-only row: a code-backed capture, the provider's own, or `unresolved -- nothing to inspect`.

DOES NOT MEAN
- that anything is decompiled or inferred: no controls, no pretence.

PROVEN BY — `workshop/screen.hpp` `interior_capture_text`;
`tests/test_workshop_panels_creator.cpp` case `"WUX-14/SC-12: a code-backed subject's interior is
a read-only capture, and an unresolved one is nothing to inspect"`.
WHY — `agents/decisions/one-way-a-pane-can-be-implemented.md`

## WL-MAKER-13 — A maker-made pane's name is a durable key and a display name at once

LAW — A definition's name is present, at most `kMaxMakerPaneNameLen` bytes, plain ASCII with no space or control character, and has no `/`, so `provider/name` stays one token in a notice and a file.

PROVEN BY — `workshop/pane_definition.hpp` `check_maker_pane_name`, `kMaxMakerPaneNameLen`;
`tests/test_workshop_panels_creator.cpp` case `"WUX-14/SC-7: the whole-definition law refuses what
no door could have made"`, case `"WUX-14/SC-13: the pane file round-trips, refuses by number and
by shape, and holds nothing but the definition"`.
WHY — `agents/decisions/a-name-is-judged-in-bytes.md`

## Do not assume

- That a pane made by the Pane Creator is an external pane, or that `Panels::maker` is a
  presentation's copy: it is Workshop-owned authored material presented through the ordinary pane
  path, and `close_panel` never touches it (WL-MAKER-03, WL-MAKER-08).
