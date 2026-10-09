# Workshop law — the desk, said whole

Register `WL-READ`: the desk read in one turn, a pane's reading paged under its stamp, what covers
a pane and what is said of it, the rate desk reads are answered at, the words Workshop says of its
own surfaces, the notice that the desk moved, and the stamp that names a picture by what it shows. The shapes are public in [workshop panes](../../docs/reference/workshop-panes.md);
the reader that writes them on an agent's host is
[Drive Workshop from another host](../../docs/workshop/external-host.md)'s. One law per heading; cite by
ID. Router: [`../workshop.md`](../workshop.md).

## WL-READ-01 — The desk is said in one turn, paged by pane

LAW — Workshop answers `DeskReadRequested` with `DeskView` v3, every presented pane's stamp front to back, and as many of those panes' readings, in that order, as one reply holds.

MEANS
- a stamp past the last reading names a pane its reader asks alone, and nothing is held between asks;
- one reply is one decoded value (`decoded_cells`) in a quarter of Loom's frame at most, native or JSON (`reply_bytes`); a reading is carried whole or only named by its stamp;
- the second version's stamps and `PaneView` v5 readings name a picture by fingerprint (WL-READ-07), the first's by number.

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

LAW — A pane's reading names its holder, incarnation, room grant and picture, says its words and parts from `from` within one reply, and no word of a picture in flight.

MEANS
- a page asked under a stamp the pane no longer stands on is refused as stale, whatever its picture;
- a cover moving between pages leaves the stamp alone: a reader compares `total` and `covered`, and a `from` past a shrunk reading is refused;
- a picture in flight -- a room out no picture has answered; at v4 a number not yet aimed at, at v5 what is shown not yet what the stamp names -- is said so at any page.

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

## WL-READ-06 — The desk says when it moved

LAW — Workshop publishes `DeskStamps`, the desk number and every presented pane's stamp front to back, at the end of each delivery that moved either, so the newest notice stands for each earlier one.

MEANS
- a repaint that moved nothing and a picture sent again unchanged publish nothing; a desk read finding the number moved without a notice publishes one;
- it is the office's own, to any listener: a guest follows it through the relay as `latest`, with `capture` (WL-GUEST-11);
- a notice's `cause` stands only until a later notice replaces it.

DOES NOT MEAN
- that a holder leaving between deliveries is said before Workshop's next repaint, fence or desk read;
- that a notice names each change since the last one a reader took: it names where the desk stands.

PROVEN BY — `workshop/pane_view.hpp` `DeskStamps`; `workshop/weave_inspection.cpp`
`publish_desk_stamps`, `presented_refs`; `workshop/weave_managed.cpp` `after_delivery`;
`workshop/weave_seam.cpp` `on(PictureFence)`; `tests/test_workshop_desk_read.cpp` case `"a keystroke
gives at least one notice, the last naming the pane's final picture, with no ask between"`, case
`"moving only the selection, closing a pane or opening arranging gives exactly one notice, with a new
desk number"`, case `"two panes changing in one turn both reach the follower: the newest notice names
both new pictures"`, case `"a notice names a picture only once it is aimed at: it is told after that
picture's fence has come round twice"`.
WHY — `agents/decisions/the-desk-says-when-it-moved.md`

## WL-READ-07 — A stamp names a picture by what it shows

LAW — A second-version stamp names a pane's picture by the fingerprint Workshop takes of what the pane sent and holds, so the same picture sent again keeps it; a fifth-version reading waits while it moves.

MEANS
- a canvas picture no press is stamped with -- a room not drawn, a preview, a managed opening's -- is named none, so the first taking a press, or refused, moves the stamp;
- a pane's own numbers stay where a press needs them: `PaneView` v5's `picture`, which may move while the stamp holds;
- the first `PaneStamp` and `PaneView` v4 still name the number, and say a picture in flight until its number is aimed at.

DOES NOT MEAN
- that a stamp moving changes the reading: a caret, a colour or a selection moves it and no word.

PROVEN BY — `workshop/fingerprint.hpp` `Fingerprint`; `workshop/panes.hpp` `picture_fingerprint`,
`PictureStamp`; `workshop/weave_seam.cpp` `fence_pictures`; `workshop/weave_inspection.cpp`
`stamp_now`, `pane_reading`; `tests/test_workshop_desk_read.cpp` case `"a stamp names what a pane
shows: the same picture sent again keeps it and sends no notice, and every change moves it, prose that
numbers no picture included"`, case `"a canvas pane moved and drawn again the same ends on a notice
naming its settled stamp, read with its words"`, case `"a picture sent again unchanged is read at once
under the fifth version, while the fourth says it in flight until its number is aimed at"`, case `"a
page under a stamp naming its picture by fingerprint is stale only when the holder, the incarnation,
the room or what the pane shows moved: the same picture sent again keeps it"`, case `"a refused
picture is told: its pane's stamp moves when Workshop refuses the picture its room asked for, and
the same refusal said again moves nothing"`, case `"Layouts' stamp names what it shows: a new
layout's tab and the naming line's caret each move it, and a desk seated again the same keeps it"`.
WHY — `agents/decisions/the-desk-says-when-it-moved.md`

## Do not assume

- That an earlier `PaneView` says a covered pane's visible words: versions 1 to 3 still refuse it.
- That a reading is consistent across panes: each pane's stands on its own stamp, and the desk on
  its number; the reader says which panes were read on another picture than the desk read named.
- That a fingerprint orders pictures: it says whether two are the same, never which came later.
