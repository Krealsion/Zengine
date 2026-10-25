# Workshop law — the window file

Register `WL-SESSION`, its third file: the window's own facts -- the viewport, the desktop
placement, the normal room, the OS window's room -- in Workshop's window file beside the session,
which Workshop alone reads and writes. One law per heading; cite by ID. The files and their
domains are in [`session.md`](session.md). Router: [`../workshop.md`](../workshop.md).

## WL-SESSION-21 — The window's facts are Workshop's own file, beside the session

LAW — Workshop keeps the window's facts in its window file, `<stem>.window<ext>` beside the session (`path_beside`), and is its one writer; the layouts' owner keeps the session and writes no window file.

MEANS
- while no window file stands, a session before version 12 is read for them once (`import_from_session`), and the next close writes the window file;
- the file is one value of its own version, refused whole as current data; a refused one opens at the default size, said, and the desk comes back;
- it is written on the orderly close, before Workshop asks the owner to save the run, and not where the session could not be read.

DOES NOT MEAN
- that a window fact rides in the session: the session holds the run of layouts (WL-SESSION-05).

WHY — `agents/decisions/three-ownership-domains.md`

## WL-SESSION-07 — The viewport is the window file's, and is declined, never clamped

LAW — The viewport, in canvas pixels, is the window file's; a viewport outside the screen's minimum-to-maximum band on either axis is declined, never clamped.

MEANS
- Workshop then opens at its floor and names the value;
- a restored window is the weaver's chosen size, to the pixel;
- whether a size fits the current display is not a question Workshop can put to anybody.

WHY — `agents/decisions/the-first-picture-is-the-floor.md`

## WL-SESSION-08 — The desktop placement is remembered opaque and judged by the medium

LAW — The medium reports where its normal window sits (`SurfacePlacement`); Workshop hands the last report back once at restore, and the medium brings the window back whole on a display (`placement_within`).

MEANS
- a run whose medium reports no placement, every terminal, retains the remembered value;
- a maximized restore is shaped, repositioned, then re-maximized: the medium's ordering;
- desktop placement is not canvas geometry: no desktop unit enters authored intent.

WHY — `agents/decisions/the-first-picture-is-the-floor.md`

## WL-SESSION-09 — The saved viewport is the normal window's

LAW — `Session::normal_w/h` tracks the screen except while this run's medium says the window is maximized, so a maximized close writes the room the weaver chose with `maximized` beside it.

MEANS
- a maximized flag merely restored from the file never gates a placement-less run's tracking;
- the OS window says a maximize or a restore-down before the room it brings, though its desktop sizes it first.

WHY — `agents/decisions/the-first-picture-is-the-floor.md`

## WL-SESSION-20 — The OS window comes back shaped to its own room

LAW — The window file keeps the OS window's normal room beside its placement; a restore in it shapes it to that room (`WindowShaped`), a smaller one taken, then places it.

MEANS
- a maximized one is shaped first and maximized after, so restoring it down returns that room;
- a run in another window writes the room back as it read it, and a terminal keeps its own size;
- a room outside the screen's band is declined, never clamped, and said; a session before version 10 imports none.

DOES NOT MEAN
- that Workshop judges the display: the window keeps a shaped size within it (`shaped_within`).

WHY — `agents/decisions/the-first-picture-is-the-floor.md`

## Proven by

### WL-SESSION-21
`workshop/window_persist.hpp` path_beside WorkshopWindow kFormatVersion from_text save_file
load_file import_from_session import_from_session_text kSessionPixelsVersion
`workshop/weave_session.cpp` restore_last_session save_last_session
`tests/test_workshop_persistence.cpp` "an older session's room is Workshop's window file's once:
read while none stands, and never again once one does" "a session file holds the run and nothing of
the window, and nothing runtime" "reading an old session does not rewrite it; the next close does"
`tests/test_workshop_panes_editor.cpp` "an orderly close writes the window file and the session,
and a refused quit writes neither"

### WL-SESSION-07
`workshop/window_persist.hpp` viewport_honoured declined_viewport WorkshopViewport LoadedWindow
`workshop/screen.hpp` kScreenMinW kScreenMaxW kScreenMinH kScreenMaxH
`tests/test_workshop_persistence.cpp` "a hostile room is declined, and the desk still comes back"
"the band a room is honoured in is the one the screen is honest at"

### WL-SESSION-08
`workshop/weave.hpp` SurfacePlacementRemembered SurfacePlacement
`workshop/weave_handlers.cpp` on(SurfacePlacement)
`graphics/vocabulary.hpp` SurfacePlacement SurfacePlacementRemembered
`graphics/window/window_sdl.hpp` placement_within
`workshop/window_persist.hpp` kPlacementNone WorkshopPlacement Placement placement_in
`workshop/screen.hpp` Session::placement_known Session::place_x Session::place_y
Session::place_maximized
`tests/test_workshop_persistence.cpp` "a window file with a placement round-trips
byte-identically" "the placement's words are judged; its coordinates are not" "the desk remembers
where its window sat, and offers it back" "a run whose medium reports no placement RETAINS the
remembered one"
`tests/test_graphics.cpp` "the OS window's clipboard and its place on the desktop are its backend's: a
copy handed on, the clipboard read only when asked, its placement read, and a remembered place"

### WL-SESSION-09
`workshop/screen.hpp` Session::normal_w
`workshop/weave_handlers.cpp` normal_w on(SurfacePlacement)
`workshop/weave.hpp` WorkshopWeave::medium_placed_
`graphics/window/window.hpp` service_pump
`tests/test_graphics.cpp` "the OS window's clipboard and its place on the desktop are its backend's: a
copy handed on, the clipboard read only when asked, its placement read, and a remembered place"
`tests/test_workshop_persistence.cpp` "a maximized close remembers the NORMAL room beside the
maximized state" "unmaximizing reopens the gate, and the normal room tracks again" "a restored
maximized flag alone does not gate this run's viewport"

### WL-SESSION-20
`workshop/window_persist.hpp` Placement::room_w Placement::room_h kSessionWindowRoomVersion
`workshop/screen.hpp` Session::window_room_w Session::window_room_h
`workshop/weave.hpp` WorkshopWeave::RestoredWindow WorkshopWeave::restored_window_
`workshop/weave_session.cpp` restore_window shape_restored_window save_last_session
`workshop/grant.cpp` workshop_grant
`graphics/window/window_sdl_backend.hpp` ProcessSdlBackend::shape
`tests/test_workshop_persistence.cpp` "the window file keeps the OS window's own room beside its
placement, and a run in the terminal window writes it back as it read it, as it does the placement"
"a restore in the OS window shapes it to its own room, a smaller one taken, then places it; a
maximized one is shaped before it is maximized; the terminal window keeps its own"
