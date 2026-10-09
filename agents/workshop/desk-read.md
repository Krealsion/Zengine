# Workshop law — the desk, said whole

Register `WL-READ`: the desk read in one turn, a pane's reading paged under its stamp, what covers
a pane and what is said of it, the rate desk reads are answered at, and the words Workshop says of
its own surfaces. The shapes are public in [workshop panes](../../docs/reference/workshop-panes.md);
the reader that writes them on an agent's host is
[Drive Workshop from another host](../../docs/workshop/external-host.md)'s. One law per heading; cite by
ID. Router: [`../workshop.md`](../workshop.md).

## WL-READ-01 — The desk is said in one turn, paged by pane

LAW — Workshop answers `DeskReadRequested` with `DeskView` v3, every presented pane's stamp front to back, and as many of those panes' `PaneView` v4 readings, in that order, as one reply holds.

MEANS
- a stamp past the last reading names a pane its reader asks alone, and nothing is held between asks;
- one reply is one decoded value by Loom's rule (`decoded_cells`), in at most a quarter of Loom's frame in the larger of its native and JSON serializations (`reply_bytes`);
- a reading is carried whole or only named by its stamp.

PROVEN BY — `workshop/pane_view.hpp` `DeskReadRequested`, `DeskRead`; `workshop/decoded_cells.hpp`
`kDecodedCellBudget`, `decoded_cells`; `workshop/reply_bytes.hpp` `kReplyByteBudget`, `reply_bytes`;
`workshop/weave_inspection.cpp` `on(DeskReadRequested)`, `page_of`;
`tests/test_workshop_desk_read.cpp` case `"the decode budget a reading is filled to is Loom's own:
a value of exactly that many cells decodes, and one cell more is refused"`, case `"a desk past one
value's budget reads whole: the panes past it are named by their stamps and read alone"`, case `"a
picture at the canvas text limit under overlapping parts reads whole, each page and the desk read
inside one reply's bytes"`, case `"a capture guest reads a picture at the canvas text limit whole
over the bridge, page by page, and its connection answers the next ask"`.
WHY — `agents/decisions/the-desk-is-said-whole-on-the-agents-host.md`

## WL-READ-02 — A pane's reading stands on its stamp and pages under it

LAW — A pane's `PaneView` v4 names its holder, incarnation, room grant and picture, says its words and parts from `from` within one decoded value and one reply's bytes, and no word of a picture in flight.

MEANS
- a page asked under a stamp the pane no longer stands on is refused as stale, whatever its picture;
- a cover moving between pages leaves the stamp alone: a reader compares `total` and `covered`, and a `from` past a shrunk reading is refused;
- a picture in flight -- handed out and not aimed at, or a room out no picture has answered -- is said so at any page.

PROVEN BY — `workshop/pane_view.hpp` `PaneStamp`, `v4::PaneViewRequested`, `v4::PaneView`;
`workshop/weave.hpp` `HostContext::incarnation_of`; `workshop/weave_inspection.cpp`
`pane_reading`, `stamp_of`, `on(PaneViewRequested)`, `page_of`; `workshop/reply_bytes.hpp`
`kReplyByteBudget`; `tests/test_workshop_desk.cpp` case `"a
stamp names holder, incarnation, room and picture: a reading after t or a reload in place is stale
even at an equal picture number"`, case `"a pane whose newest picture is in flight is read as in
flight, with no word, and the desk still reads"`; `tests/test_workshop_desk_read.cpp` case `"a
pane at its full canvas budgets reads whole, paged by index under one stamp, and a page asked after
its picture moved is refused as stale"`, case `"a picture at the canvas text limit under
overlapping parts reads whole, each page and the desk read inside one reply's bytes"`.
WHY — `agents/decisions/the-desk-is-said-whole-on-the-agents-host.md`

## WL-READ-03 — Desk reads are a few a second, each asker

LAW — Workshop answers at most four `DeskReadRequested` a second from one asker, and refuses the next in words naming when it may read again.

PROVEN BY — `workshop/weave.hpp` `kDeskReadsPerSecond`; `workshop/weave_inspection.cpp`
`take_desk_read`; `tests/test_workshop_desk.cpp` case `"past four desk reads in one second an asker
is refused in words, and reads again a second later"`.
WHY — `agents/decisions/the-desk-is-said-whole-on-the-agents-host.md`

## WL-READ-04 — Workshop says its own words, and names its own surfaces' parts

LAW — `DeskView` v3 says the band's words where drawn, with no point, the status slot with no place, and a number moving when any of it does; Layouts reads as painted, tabs named `layout:<name>`, `layout:+`.

MEANS
- a layout's name is spelled `%XX` past printable ASCII and for `%`, `#` and `+`, and a name two layouts share is followed by `#` and its place.

PROVEN BY — `workshop/pane_view.hpp` `v3::DeskView`; `workshop/weave_inspection.cpp`
`desk_view_v3`, `layouts_reading`, `layout_part_names`; `tests/test_workshop_desk.cpp` case `"a desk
read's words stand where the medium draws them, Workshop's own band words included, in a window and
in a terminal"`, case `"a part's listed point presses that part, one in the middle of a row too, and
Layouts' tabs press the layout they name"`, case `"Layouts and the band are read as Workshop draws
them: tabs as named parts, the band's words with no point"`, case `"the desk number moves when
anything the desk says moves, and holds while nothing does"`.
WHY — `agents/decisions/the-desk-is-said-whole-on-the-agents-host.md`

## WL-READ-05 — A reading says what is visible, and points only where a press lands

LAW — A pane's `PaneView` v4 says no word or part of a covered place, counting them and naming the cover, and gives a point only where a press lands on what it names.

MEANS
- a cover is a pane in front, the menu, arranging, the refused mark, or the band -- the foot band over Layouts too;
- while a menu is open a primary press outside it reaches nothing beside it -- it closes the menu, or is refused -- so nothing there has a point.

PROVEN BY — `workshop/pane_view.hpp` `PaneCover`; `workshop/weave_inspection.cpp` `covers_of`,
`pane_reading`, `layouts_reading`; `tests/test_workshop_desk.cpp` case `"a covered pane says its
visible words only, and what covers it: a pane in front, Workshop's menu, arranging -- and beside a
menu nothing has a point"`, case `"Layouts and the band are read as Workshop draws them: tabs as
named parts, the band's words with no point"`, case `"with pane titles hidden, a refused picture's
mark is Workshop's own: the pane view waits while it covers the picture, a press on it reaches no
provider and a right press opens Workshop's menu, and the picture beside it takes a press as
before, a press held or not"`.
WHY — `agents/decisions/the-desk-is-said-whole-on-the-agents-host.md`

## Do not assume

- That an earlier `PaneView` says a covered pane's visible words: versions 1 to 3 still refuse it.
- That a reading is consistent across panes: each pane's stands on its own stamp, and the desk on
  its number; the reader says which panes stand on a later picture.
