# Workshop law — the window file

Register `WL-SESSION`, its third file: the window's own facts -- the viewport, the desktop
placement, the normal room, the GUI window's room -- in Workshop's window file beside the session,
which Workshop alone reads and writes. One law per heading; cite by ID. The files and their
domains are in [`session.md`](session.md). Router: [`../workshop.md`](../workshop.md).

Retired: WL-SESSION-08, WL-SESSION-09, WL-SESSION-20

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
