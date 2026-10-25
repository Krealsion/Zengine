# Workshop law — the session, restored

Register `WL-SESSION`, its second file: what a session holds, and how it comes back — the run of
layouts and their associations, the restore that runs once. The window's facts are Workshop's own
file, in [`window-file.md`](window-file.md). One law per heading; cite by ID. The files and their domains are in [`session.md`](session.md). Router:
[`../workshop.md`](../workshop.md).

Retired: WL-SESSION-11, WL-SESSION-12.

## WL-SESSION-05 — The session holds the run of layouts, each with its id and association

LAW — Version <!-- value session_persist::kFormatVersion -->12<!-- /value --> holds the weaver's order whole, each entry the id the layouts' owner minted, a desk and its link, the live layout's position, and the next id; the owner alone writes it.

MEANS
- an empty path is the absence with exactly one spelling; `link_in` refuses the half-association;
- an association remembers a whole desk, not a hash; a restore never re-reads what it refers to;
- an id is never reused or shown: a new layout or a copy takes the next, and a session before version 12 numbers its layouts in order.

WHY — `agents/decisions/a-layout-is-a-lifted-value.md`

## WL-SESSION-06 — The run's own admission is four questions plus the link

LAW — Admitting a run is five questions and the link: non-empty, in the ceiling, the live one in range, ids held once below the next, each desk and link legal; refusals of current data, never a conversion.

MEANS
- the file is a header line and a line per layout, each its own decode; a line refused, or lines not the header's count, refuse it whole;
- the ceiling is `kMaxHeaderLineBytes` and `kMaxLayouts` lines of `kMaxLayoutLineBytes`, two desks and a path: what this build writes it reads;
- each line decodes inside Loom's `kMaxDecodedCells`, and `kMaxPaneSettingsPerDesk` leaves a layout's room for a field on every row.

WHY — `agents/decisions/a-layout-is-a-lifted-value.md`

## WL-SESSION-14 — The restore runs once per process, and answers four things

LAW — The restore runs once per process, guarded by a flag, because a window offers itself again when it is started again; the answer carries present, outcome, honoured and declined.

MEANS
- the flag is set before the file is opened, so a refusal is final too;
- `load_file` asks `exists` first, so a first launch is not an error and stays silent;
- its sentence takes the unit the first window's offer names, unless another was said since.

WHY — `agents/decisions/three-ownership-domains.md`

## WL-SESSION-16 — Neither direction opens a setup file

LAW — Closing writes a session and leaves the standalone artifact byte-identical; restoring reads no setup file; the session carries the associations without reading what they refer to.

WHY — `agents/decisions/three-ownership-domains.md`

## WL-SESSION-17 — A restore returns the desks and the room, not what a weaver was doing

LAW — Selection, keyboard focus, the document and every other Workshop-global fact are this run's; a restored layout paints identically except for which pane wears the focus ink.

MEANS
- a loaded pane's own state is its own: this file remembers the reference and its settings, never the pane's state.

WHY — `agents/decisions/three-ownership-domains.md`

## Proven by

### WL-SESSION-05
`workshop/layouts/session_persist.hpp` WorkshopSession::layouts WorkshopSession::active
kFormatVersion WorkshopLayout WorkshopSetupLink link_in layouts_in WorkshopSession to_link in_layout
half_a_link WorkshopSessionHeader header_of to_text bad_id
`workshop/layouts/run.hpp` Layout next_id position_of
`workshop/layouts/store.cpp` place_of
`tests/test_workshop_persistence.cpp` "a whole layout run round-trips exactly, active in the middle"
"every position and every association combination round-trips" "a current-version session with half
an association is refused" "a session's file is a header line and a line for each layout, each
decoded alone"

### WL-SESSION-06
`workshop/layouts/session_persist.hpp` WorkshopSession::active link_in layouts_in kMaxSessionBytes
kMaxLinkPathBytes kMaxLayoutLineBytes kMaxHeaderLineBytes lines_of lines_in count_disagrees
one_value WorkshopSessionHeader::layout_count no_layouts too_many_layouts
`workshop/setup_persist.hpp` setup_in kMaxSetupBytes kMaxPaneSettingSpelledBytes
kMaxSetupLockSpelledBytes
`workshop/setup.hpp` kMaxPaneSettingsPerDesk kMaxLayouts
`workshop/decoded_cells.hpp` kDecodedCellBudget decoded_cells
`tests/test_workshop_persistence.cpp` "a current run this Workshop could not have made is refused as
CURRENT data" "a maximal legal session is still one this build can read back" "a session may hold as
much as it may hold, and be read back" "a session's file is refused whole where a line is refused or
the lines are not the header's count" "a maximal session decodes a line at a time, each line with
room for a field on every row" "a version-11 session written as one value is refused, its file being
a line a layout" "a header counting more layouts than a Workshop keeps is refused before a line is
read"

### WL-SESSION-14
`workshop/weave_session.cpp` restore_last_session restore_notice_text spell_restore_notice
restore_window say_restored
`workshop/layouts/session_persist.hpp` LoadedSession::present LoadedSession::outcome LoadedSession
load_file
`graphics/vocabulary.hpp` WindowOffered
`workshop/layouts/owner.cpp` on(LayoutEditRequested)
`workshop/layouts/store.cpp` restore
`tests/test_workshop_persistence.cpp` "the room is taken back only ONCE, however often a window
offers itself" "a first launch is not an error, and needs no file to exist" "a malformed session
costs the desk and nothing else" "the restore notice says the room in the unit the medium names:
pixels in a window, cells in a terminal"

### WL-SESSION-16
`workshop/weave_session.cpp` restore_last_session save_last_session restore_window
`workshop/layouts/store.cpp` restore
`tests/test_workshop_persistence.cpp` "an automatic save never touches the file a weaver named" "a
restored session never touches the file a weaver named, either" "the whole run and every association
come back after a restart"

### WL-SESSION-17
`workshop/weave_session.cpp` restore_last_session restore_window
`workshop/layouts/store.cpp` restore
`tests/test_workshop_persistence.cpp` "a session file holds the run and nothing of the window, and
nothing runtime" "the position that comes back is the one the weaver stood on" "the weaver is told
once, in the pane's own durable names"
