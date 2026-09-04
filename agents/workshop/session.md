# Workshop law — the session

Register `WL-SESSION`: the durable files, the session, the room and the window. One law per
heading; cite by ID. Router: [`../workshop.md`](../workshop.md).

## WL-SESSION-01 — Workshop writes the desk, the room and the window, and reads them back

LAW — The maker-facing files live in three ownership domains: project (`--document`, `--setup`, `--pane`), configuration (`--keymap`, `--prefs`), state (`--session`, `--marks`); plans ship beside the binary.

MEANS
- project files follow the launch directory; configuration follows the maker; state, the machine;
- a mark is an absolute path, so the marks file is state, and its own file, not a prefs field.

DOES NOT MEAN
- that the document is read at launch: it is not; the desk, the window and the preferences are.

PROVEN BY — `workshop/user_paths.hpp` `resolve_durable_path`; `workshop/weave.hpp`
`HostContext::document_path`, `HostContext::setup_path`, `HostContext::session_path`,
`HostContext::marks_path`, `HostContext::pane_path`, `HostContext::keymap_path`,
`HostContext::prefs_path`; `workshop/workshop.cpp` `Arguments`;
`tests/test_workshop_persistence.cpp` case `"WUX-3: the two Windows roots are the platform's own
conventions"`, case `"WUX-3: the two XDG roots, and their home fallbacks"`, case `"WUX-3: the host
resolves the maker's files through the one precedence"`.
WHY — `agents/decisions/three-ownership-domains.md`

## WL-SESSION-02 — The precedence is pinned and has one spelling

LAW — The precedence has one spelling: an explicit path the maker typed, then `--isolated`, then the per-user default; `--isolated` resolves the per-user defaults to the designed empty-path absence.

MEANS
- `--isolated` is the flag every witness harness and executor live run must carry;
- an environment with no resolvable root is the same absence, said once on the banner.

PROVEN BY — `workshop/user_paths.hpp` `resolve_durable_path`; `workshop/workshop.cpp`
`Arguments::session`; `tests/test_workshop_persistence.cpp` case `"WUX-3: one precedence --
explicit path, then isolation, then the default"`, case `"WUX-3: the host resolves the maker's
files through the one precedence"`.
WHY — `agents/decisions/three-ownership-domains.md`

## WL-SESSION-03 — The legacy transition is one rule and converges by existence

LAW — `import_legacy_file`: a per-user default whose file does not exist, beside a pre-root-flip local file that does, imports the local bytes once, safe-written and unjudged, and never fires again.

MEANS
- an existing user-root file always wins; the legacy file is never deleted, moved or rewritten.

PROVEN BY — `workshop/user_paths.hpp` `import_legacy_file`, `LegacyImport`;
`workshop/workshop.cpp` `import_legacy_file`; `workshop/weave.hpp` `HostContext::transition_note`;
`tests/test_workshop_persistence.cpp` case `"WUX-3: a legacy-only file is imported once, and the
original is left in place"`, case `"WUX-3: an existing user-root file always wins over a legacy
file"`, case `"WUX-3: repeated launches converge -- the import can never fire twice"`, case
`"WUX-3: no legacy file, no destination -- the import does nothing, silently"`.
WHY — `agents/decisions/three-ownership-domains.md`

## WL-SESSION-04 — One representation of a desk, two files

LAW — The session's written shape nests the setup's written shape as a field, and one function turns a written setup into a live one, whichever file it came from; an automatic save never lands on `--setup`.

MEANS
- a desk cannot be legal in one file and illegal in the other;
- the three files are three formats, and each refuses the others.

PROVEN BY — `workshop/session_persist.hpp` `WorkshopSession`, `setup_in`;
`workshop/setup_persist.hpp` `WorkshopSetup`, `setup_in`; `workshop/weave.hpp`
`HostContext::session_path`; `tests/test_workshop_persistence.cpp` case `"WUX-0 F: an automatic
save never touches the file a maker named"`, case `"WUX-0 F: a restored session never touches the
file a maker named, either"`, case `"WUX-0 F: the three files are three formats, and each refuses
the others"`.
WHY — `agents/decisions/three-ownership-domains.md`

## WL-SESSION-05 — The session holds the run of layouts with their associations

LAW — Version 6 holds the maker's order whole, the live layout in it and its position beside it, each entry a desk plus its link; position is a layout's whole identity, and no id is minted.

MEANS
- an empty path is the absence with exactly one spelling; `link_in` refuses the half-association;
- an association remembers a whole desk, not a hash; a restore never re-reads what it refers to.

PROVEN BY — `workshop/session_persist.hpp` `WorkshopSession::layouts`,
`WorkshopSession::active`, `kFormatVersion`, `WorkshopLayout`, `WorkshopSetupLink`, `link_in`,
`layouts_in`, `WorkshopSession`, `to_link`, `in_layout`, `half_a_link`;
`tests/test_workshop_persistence.cpp` case `"WUX-10/SC-8: a whole layout run round-trips exactly,
active in the middle"`, case `"WUX-11/SC-14: every position and every association combination
round-trips"`, case `"WUX-11/SC-15: a current-version session with half an association is
refused"`.
WHY — `agents/decisions/a-layout-is-a-lifted-value.md`

## WL-SESSION-06 — The run's own admission is four questions plus the link

LAW — Admitting a run is four questions plus the link: non-empty, within the ceiling, the live one in range, every desk and link legal by its own law; refusals of current data, never a conversion search.

MEANS
- the ceiling is `kMaxLayouts` * (2 * `kMaxSetupBytes` + `kMaxLinkPathBytes`): two desks, a path;
- a session this build writes is never one it refuses to read.

PROVEN BY — `workshop/session_persist.hpp` `WorkshopSession::active`, `link_in`, `layouts_in`,
`kMaxSessionBytes`, `kMaxLinkPathBytes`, `kMaxLayouts`, `no_layouts`; `workshop/setup_persist.hpp`
`setup_in`, `kMaxSetupBytes`; `tests/test_workshop_persistence.cpp` case `"WUX-10/SC-12: a current
run this Workshop could not have made is refused as CURRENT data"`, case `"WUX-11/SC-14: a maximal
legal session is still one this build can read back"`, case `"WUX-10: a session may hold as much
as it may hold, and be read back"`.
WHY — `agents/decisions/a-layout-is-a-lifted-value.md`

## WL-SESSION-07 — The viewport is one level above the desk, and is declined, never clamped

LAW — The viewport, in canvas cells, sits one level above the desk in the file; a viewport outside the screen's minimum-to-maximum band on either axis is declined, never clamped.

MEANS
- Workshop then opens at its floor and names the value;
- a restored window is the maker's chosen size floored to whole cells;
- whether a size fits the current display is not a question Workshop can put to anybody.

PROVEN BY — `workshop/session_persist.hpp` `WorkshopLayout::desk`, `viewport_honoured`,
`WorkshopViewport`, `LoadedSession`; `workshop/screen.hpp` `kScreenMinW`, `kScreenMaxW`,
`kScreenMinH`, `kScreenMaxH`; `tests/test_workshop_persistence.cpp` case `"WUX-0 E: a hostile room
is declined, and the desk still comes back"`, case `"WUX-0 E: the band a room is honoured in is
the one the screen is honest at"`, case `"WUX-0: a session file holds the desk and the room, and
nothing runtime"`.
WHY — `agents/decisions/the-first-picture-is-the-floor.md`

## WL-SESSION-08 — The desktop placement is remembered opaque and judged by the medium

LAW — The medium reports where its normal window sits (`SurfacePlacement`), Workshop remembers the last report and hands it back once at restore, where the medium validates it (`placement_within`).

MEANS
- a run whose medium reports no placement, every terminal, retains the remembered value;
- a maximized restore repositions, re-grows, then re-maximizes: the medium's ordering;
- desktop placement is not canvas geometry: no desktop unit enters authored intent.

PROVEN BY — `workshop/weave.hpp` `SurfacePlacementRemembered`, `SurfacePlacement`;
`workshop/weave_handlers.cpp` `on(SurfacePlacement)`; `surface/vocabulary.hpp` `SurfacePlacement`,
`SurfacePlacementRemembered`; `surface/skin_sdl_plan.hpp` `placement_within`;
`workshop/session_persist.hpp` `kPlacementNone`, `WorkshopPlacement`, `Placement`;
`workshop/screen.hpp` `Session::placement_known`, `Session::place_x`, `Session::place_y`,
`Session::place_maximized`; `tests/test_workshop_persistence.cpp` case `"WUX-3: a session with a
placement round-trips byte-identically"`, case `"WUX-3: the placement's words are judged; its
coordinates are not"`, case `"WUX-3: the desk remembers where its window sat, and offers it
back"`, case `"WUX-3: a run whose medium reports no placement RETAINS the remembered one"`;
`tests/test_surface.cpp` case `"WUX-3: placement is reported BEFORE the extent, at every door"`.
WHY — `agents/decisions/the-first-picture-is-the-floor.md`

## WL-SESSION-09 — The saved viewport is the normal window's

LAW — `Session::normal_w/h` tracks the screen except while this run's medium says the window is maximized, so a maximized close writes the room the maker chose with `maximized` beside it.

MEANS
- a maximized flag merely restored from the file never gates a placement-less run's tracking.

PROVEN BY — `workshop/screen.hpp` `Session::normal_w`; `workshop/weave_handlers.cpp` `normal_w`,
`on(SurfacePlacement)`; `workshop/weave.hpp` `WorkshopWeave::medium_placed_`;
`tests/test_workshop_persistence.cpp` case `"WUX-3: a maximized close remembers the NORMAL room
beside the maximized state"`, case `"WUX-3: unmaximizing reopens the gate, and the normal room
tracks again"`, case `"WUX-3: a restored maximized flag alone does not gate this run's viewport"`.
WHY — `agents/decisions/the-first-picture-is-the-floor.md`

## WL-SESSION-11 — The first picture of a run is Workshop's floor

LAW — The SDL medium makes a run's first picture the window's minimum, once, so `on(SurfaceReady)` repaints at the minimum extent and then takes the session back.

MEANS
- seeding the extent before the first canvas would leave a maker unable to shrink the window.

PROVEN BY — `workshop/weave_session.cpp` `restore_last_session`; `workshop/weave_save.cpp`
`repaint`; `surface/skin_sdl.cpp` `SDL_SetWindowMinimumSize`;
`tests/test_workshop_persistence.cpp` case `"WUX-0: the FIRST picture of a run is the floor, and
the room is the second"`.
WHY — `agents/decisions/the-first-picture-is-the-floor.md`

## WL-SESSION-12 — The room, and then the desk into it

LAW — The desk is seated against the restored room's capacity, so the room is taken back before the restored desk is installed; reversing the two leaves a pane waiting for room it already had.

PROVEN BY — `workshop/weave_session.cpp` `restore_last_session`, `apply_setup`;
`workshop/weave_handlers.cpp` `adopt_screen`; `workshop/screen.hpp` `stack_capacity`;
`tests/test_workshop_persistence.cpp` case `"WUX-0: the desk is seated against the RESTORED room,
not the default one"`.
WHY — `agents/decisions/the-first-picture-is-the-floor.md`

## WL-SESSION-13 — One door writes the session, and only on an orderly close

LAW — The quit key, the interrupt chord and the medium's close request all reach one quit, which saves the session before it stops the bus; no autosave, no dirty tracking, no background writer.

MEANS
- crash durability is not claimed: `write_file` does not fsync; a killed run loses its session.

PROVEN BY — `workshop/weave_save.cpp` `quit`; `workshop/weave_session.cpp` `save_last_session`;
`workshop/weave_handlers.cpp` `on(SurfaceCloseRequested)`; `workshop/weave.hpp`
`HostContext::session_path`; `workshop/persist.hpp` `write_file`; `surface/vocabulary.hpp`
`SurfaceCloseRequested`; `workshop/session_persist.hpp` `save_file`;
`tests/test_workshop_persistence.cpp` case `"WUX-0 B: the second session replaces the first, room
and desk both"`, case `"WUX-0: a write that fails leaves the last good session where it was"`;
`tests/test_workshop_document.cpp` case `"the native close request reaches the quit policy `q`
already had"`.
WHY — `agents/decisions/three-ownership-domains.md`

## WL-SESSION-14 — The restore runs once per process, and answers four things

LAW — The restore runs once per process, guarded by a flag, because the surface's hello arrives again when a Skin is replaced; the answer carries present, outcome, honoured and declined.

MEANS
- the flag is set before the file is opened, so a refusal is final too;
- `load_file` asks `exists` first, so a first launch is not an error and stays silent.

PROVEN BY — `workshop/weave_session.cpp` `restore_last_session`; `workshop/weave.hpp`
`WorkshopWeave::restored_`; `workshop/session_persist.hpp` `LoadedSession::present`,
`LoadedSession::outcome`, `LoadedSession`, `load_file`, `LoadedSession::honoured`,
`LoadedSession::declined`; `surface/vocabulary.hpp` `SurfaceReady`;
`tests/test_workshop_persistence.cpp` case `"WUX-0: the room is taken back only ONCE, however
often a surface says hello"`, case `"WUX-0 C: a first launch is not an error, and needs no file to
exist"`, case `"WUX-0 D: a malformed session costs the desk and nothing else"`.
WHY — `agents/decisions/three-ownership-domains.md`

## WL-SESSION-15 — A session this run could not read is never written over

LAW — A session this run could not read is never written over: the save checks the refusal first; a declined viewport is not a refusal; the standing consequence is a condition, true all run, with an action.

PROVEN BY — `workshop/weave.hpp` `WorkshopWeave::session_refused_`; `workshop/weave_session.cpp`
`save_last_session`, `kSessionWallKey`; `tests/test_workshop_persistence.cpp` case `"MIG-0/SC-13:
a session this run could not read is never written over"`, case `"MIG-0/SC-13: the file survives
the run that could not read it, and opens later"`.
WHY — `agents/decisions/three-ownership-domains.md`

## WL-SESSION-16 — Neither direction opens a setup file

LAW — Closing writes a session and leaves the standalone artifact byte-identical; restoring reads no setup file; the session carries the associations without reading what they refer to.

PROVEN BY — `workshop/weave_session.cpp` `restore_last_session`, `save_last_session`;
`tests/test_workshop_persistence.cpp` case `"WUX-0 F: an automatic save never touches the file a
maker named"`, case `"WUX-0 F: a restored session never touches the file a maker named, either"`,
case `"WUX-11/SC-14: the whole run and every association come back after a restart"`.
WHY — `agents/decisions/three-ownership-domains.md`

## WL-SESSION-17 — A restore returns the desks and the room, not what a maker was doing

LAW — Selection, keyboard focus, the document, the browser's location and every other Workshop-global fact are this run's; a restored layout paints identically except for which pane wears the focus ink.

PROVEN BY — `workshop/weave_session.cpp` `restore_last_session`;
`tests/test_workshop_persistence.cpp` case `"WUX-0: a session file holds the desk and the room,
and nothing runtime"`, case `"WUX-10/SC-13: the position that comes back is the one the maker
stood on"`; `tests/test_workshop_files.cpp` case `"PROJ-2: marks survive a restart, and the
browsing location deliberately does not"`.
WHY — `agents/decisions/three-ownership-domains.md`

## WL-SESSION-18 — Roots are made on the first write, and a project directory is never invented

LAW — The per-user roots are created on the first write and never on a read, so a run that persists nothing leaves no trace; a project file into a missing directory is refused, not given one.

MEANS
- the session's close, the prefs toggle and the legacy import write through the making door;
- `--document`, `--setup` and `--pane` paths do not: a missing directory there is a maker's typo.

PROVEN BY — `workshop/persist.hpp` `write_file_making_room`;
`tests/test_workshop_persistence.cpp` case `"WUX-0 C: a first launch is not an error, and needs no
file to exist"`, case `"a save into a place that does not exist refuses before it writes
anything"`.
WHY — `agents/decisions/three-ownership-domains.md`

## Do not assume

- That a session save can be trusted after a crash: it is written on an orderly close and
  nowhere else (WL-SESSION-13).
- That the last session and a named setup are the same thing saved twice: two promises, two
  files (WL-SESSION-04).
