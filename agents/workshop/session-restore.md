# Workshop law — the session, restored

Register `WL-SESSION`, its second file: what a session holds, and how it comes back — the run of
layouts and their associations, the room and the window, the restore that runs once. One law per
heading; cite by ID. The files and their domains are in [`session.md`](session.md). Router:
[`../workshop.md`](../workshop.md).

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

PROVEN BY — `workshop/weave_session.cpp` `restore_last_session`; `workshop/weave_run.cpp`
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
