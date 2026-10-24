# Workshop law — the desk, said whole

Register `WL-READ`: the desk read in one turn, a pane's reading paged under its stamp, what covers
a pane and what is said of it, the rate desk reads are answered at, the words Workshop says of its
own surfaces, the notice that the desk moved, the stamp that names what a pane shows, and the window
a pane drawing in an area of its own is read from. The shapes are public in
[workshop panes](../../workshop/docs/workshop-panes.md); the reader that writes them on an agent's
host is [Drive Workshop from another host](../../external-host/docs/external-host.md)'s. One law
per heading; cite by ID. Router: [`../workshop.md`](../workshop.md).

## WL-READ-01 — The desk is said in one turn, paged by pane

LAW — Workshop answers `DeskReadRequested` with `DeskRead`: `DeskView`, every presented pane's stamp front to back, and as many of those panes' readings, in that order, as one reply holds.

MEANS
- a stamp past the last reading names a pane its reader asks alone, and nothing is held between asks;
- one reply is one decoded value (`decoded_cells`) in a quarter of Loom's frame at most, native or JSON (`reply_bytes`); a reading is carried whole or only named by its stamp;
- a read that must hear from the window first (WL-READ-08) is answered once it has, or refused in words.

WHY — `agents/decisions/the-desk-is-said-whole-on-the-agents-host.md`

## WL-READ-02 — A pane's reading stands on its stamp and pages under it

LAW — A pane's reading names its stamp, the place it shows in and its text measure, says its words and parts from `from` within one reply, and no word of what is in flight.

MEANS
- a page asked under a stamp the pane no longer stands on is refused as stale;
- a cover moving between pages leaves the stamp alone: a reader compares `total` and `covered`, and a `from` past a shrunk reading is refused;
- in flight is a window waiting for the pane's drawing, or a reading asked and not yet answered, said so at any page; a pane whose office nobody holds is no reading, and says it waits.

WHY — `agents/decisions/the-desk-is-said-whole-on-the-agents-host.md`

## WL-READ-03 — Desk reads are a few a second, each asker

LAW — Workshop answers at most four `DeskReadRequested` a second from one asker, and refuses the next in words naming when it may read again.

WHY — `agents/decisions/the-desk-is-said-whole-on-the-agents-host.md`

## WL-READ-04 — Workshop says its own words, and names its own surfaces' parts

LAW — `DeskView` says the band's words where drawn, with no point, and a number moving when any of it does; Layouts reads as painted, tabs named `layout:<name>`, `layout:+`.

MEANS
- a layout's name is spelled `%XX` past printable ASCII and for `%`, `#` and `+`, and a name two layouts share is followed by `#` and its place.

WHY — `agents/decisions/the-desk-is-said-whole-on-the-agents-host.md`

## WL-READ-05 — A reading says what is visible, and points only where a press lands

LAW — A pane's reading says no word or part of a covered place, counting them and naming the cover, and gives a point only where a press lands on what it names.

MEANS
- a cover is a pane in front, the menu, arranging, or the band -- the foot band over Layouts too -- each by Workshop's own order, as it arranges each;
- while a menu is open a primary press outside it reaches nothing beside it -- it closes the menu, or is refused -- so nothing there has a point.

WHY — `agents/decisions/the-desk-is-said-whole-on-the-agents-host.md`

## WL-READ-06 — The desk says when it moved

LAW — Workshop publishes `DeskStamps`, the desk number and each presented pane's stamp front to back, at the end of a delivery that moved any, so the newest stands for each earlier one.

MEANS
- an unchanged repaint publishes nothing; a desk read finding the number moved publishes; the window saying an area settled past what Workshop took asks it again (`AreaSettled`);
- it is the office's own, to any listener: a guest follows it through the relay as `latest`, with `capture` (WL-GUEST-11);
- a notice's `cause` stands only until a later notice replaces it.

DOES NOT MEAN
- that a holder leaving between Workshop's deliveries is said before its next delivery or desk read;
- that a notice names each change since the last one a reader took: it names where the desk stands.

WHY — `agents/decisions/the-desk-says-when-it-moved.md`

## WL-READ-07 — A stamp names what a pane shows, where

LAW — A stamp names a pane's holder, incarnation, the window's incarnation, and a fingerprint of what the pane shows and where, so the same picture drawn again keeps it and a moved pane moves it.

MEANS
- the fingerprint is over what the window says the area shows (`AreaShown`) and the place it shows it in;
- an area the window waits on is named none, so the pane's first drawing moves the stamp.

DOES NOT MEAN
- that a stamp moving changes the reading: a caret, a colour or a selection moves it and no word.

WHY — `agents/decisions/the-desk-says-when-it-moved.md`

## WL-READ-08 — A pane that draws in an area of its own is read from the window

LAW — A pane drawing in a window area Workshop placed is read as the window says the area shows, its fingerprint over that answer, asked as it settles; any other pane, and Layouts, as Workshop draws it.

MEANS
- a read waits for the window's answer (`AreaShowAsked`) to an ask made after it, a few at once and two seconds at most, refused in words past that or when the window starts again;
- a guide mark on such a pane is drawn from that answer too (WL-GUIDE-02);
- a pane's menu is read so from its presenter's area (WL-CTX-09), and a desk read waits on it too.

WHY — `agents/decisions/a-pane-is-read-from-the-window-that-shows-it.md`

## Do not assume

- That a reading is consistent across panes: each pane's stands on its own stamp, and the desk on
  its number; the reader says which panes were read on another picture than the desk read named.
- That a fingerprint orders pictures: it says whether two are the same, never which came later.
- That the window's words are a pane's own: what another office draws in front is left out, what
  Workshop draws in front is Workshop's cover.

## Proven by

### WL-READ-01
`workshop/pane_view.hpp` DeskReadRequested DeskRead
`workshop/decoded_cells.hpp` kDecodedCellBudget decoded_cells
`workshop/reply_bytes.hpp` kReplyByteBudget reply_bytes
`workshop/weave_desk_read.cpp` on(DeskReadRequested) desk_read_now
`workshop/desk_read.hpp` page_of
`tests/test_workshop_desk_read.cpp` "the decode budget a reading is filled to is Loom's own: a value
of exactly that many cells decodes, and one cell more is refused" "a desk past one value's budget
reads whole: the panes past it are named by their stamps and read alone" "a picture of long words
under a part a row, as many as a window keeps, reads whole, each page and the desk read inside one
reply's bytes" "a capture guest reads a high-text picture whole over the bridge, page by page, and
its connection answers the next ask"

### WL-READ-02
`workshop/pane_view.hpp` PaneStamp PaneViewRequested PaneView
`workshop/weave.hpp` HostContext::incarnation_of
`workshop/weave_desk_read.cpp` pane_reading unshown_reading stamp_of on(PaneViewRequested) pane_page
`workshop/desk_read.hpp` page_of
`workshop/reply_bytes.hpp` kReplyByteBudget
`tests/test_workshop_desk.cpp` "a stamp names holder, incarnation and what the pane shows where: a
reading after t or a reload in place is stale even for the same picture" "a pane whose provider
left waits for one, and no reading says it is in flight"
`tests/test_workshop_desk_read.cpp` "a pane at a window's budgets reads whole under one stamp, and
from any index of it, and a page asked after its picture moved is refused as stale" "a picture of
long words under a part a row, as many as a window keeps, reads whole, each page and the desk read
inside one reply's bytes"

### WL-READ-03
`workshop/weave.hpp` kDeskReadsPerSecond
`workshop/weave_desk_read.cpp` take_desk_read
`tests/test_workshop_desk.cpp` "past four desk reads in one second an asker is refused in words, and
reads again a second later"

### WL-READ-04
`workshop/pane_view.hpp` DeskView
`workshop/weave_desk_read.cpp` desk_view layouts_reading
`workshop/desk_read.hpp` layout_part_names
`tests/test_workshop_desk.cpp` "a desk read's words stand where the medium draws them, Workshop's
own band words included, in a window and in a terminal" "a part's listed point presses that part,
one in the middle of a row too, and Layouts' tabs press the layout they name" "Layouts and the band
are read as Workshop draws them: tabs as named parts, the band's words with no point" "the desk
number moves when anything the desk says moves, and holds while nothing does"

### WL-READ-05
`workshop/pane_view.hpp` PaneCover
`workshop/weave_desk_read.cpp` covers_of pane_reading area_reading layouts_reading
`tests/test_workshop_desk.cpp` "a covered pane says its visible words only, and what covers it: a
pane in front, Workshop's menu, arranging -- and beside a menu nothing has a point" "a pane's words
are covered while a menu is open or arranging, no point a press would land on, and the desk still
answers" "Layouts and the band are read as Workshop draws them: tabs as named parts, the band's
words with no point"
`tests/test_graphics.cpp` "an area's arranger reads what the terminal window shows of it: its text
by row in the area's pixels, each part by its box and the cell a press reaches it by, what another's
area in front paints over left out and its own hiding nothing, and asked again since its version,
the same"

### WL-READ-06
`workshop/pane_view.hpp` DeskStamps
`workshop/weave_desk_read.cpp` publish_desk_stamps presented_refs on(AreaSettled)
`workshop/weave_managed.cpp` after_delivery
`tests/test_workshop_desk_read.cpp` "a keystroke gives at least one notice, the last naming the
pane's final picture, with no ask between" "moving only the selection, closing a pane or opening
arranging gives exactly one notice, with a new desk number" "two panes changing in one turn both
reach the follower: the newest notice names both new pictures"

### WL-READ-07
`workshop/fingerprint.hpp` Fingerprint
`workshop/weave_desk_read.cpp` stamp_now pane_reading shown_fingerprint placed_fingerprint
`tests/test_workshop_desk_read.cpp` "a stamp names what a pane shows: the same picture drawn again
keeps it and sends no notice, and every change moves it, rows included" "a canvas pane moved and
drawn again the same ends on a notice naming its stamp in its new place, read with its words" "a page under a stamp naming its picture by fingerprint is stale only when the holder, the
incarnation, the room or what the pane shows moved: the same picture sent again keeps it" "Layouts' stamp names what it shows: a new
layout's tab and the naming line's caret each move it, and a desk seated again the same keeps it"

### WL-READ-08
`graphics/vocabulary.hpp` AreaShowAsked AreaShown AreaSettled
`graphics/window/window.hpp` tell_settled
`workshop/weave_desk_read.cpp` area_reading on(AreaShown) on(AreaSettled) finish_desk_waits
wait_on_areas shown_words_and_parts presented_menu_reading presented_area
`tests/test_workshop_panes_button.cpp` "WL-HAND-06: the shipped presenter names each row's line by
its id, and a press at that name's point chooses it"
`tests/test_workshop_desk.cpp` "a pane drawing in an area of its own is read as the window says it
shows there: its words, its parts with the point a press reaches them by, and a stamp that moves
when it draws again" "a weaver's press in a pane's area of the window ends a guest's guide mark
standing there, and the press is the pane's"
`tests/test_graphics.cpp` "the window tells an area's arranger, unasked, each time what the area
shows settles at a new version, and once a version, when it follows the area, and where presses
landed and that the pointer left; an area it placed without following it is told none of it" "an area's arranger reads what the terminal window shows of it: its text
by row in the area's pixels, each part by its box and the cell a press reaches it by, what another's
area in front paints over left out and its own hiding nothing, and asked again since its version,
the same" "an area whose owner was told a room it must draw whole reads waiting until the owner
draws, and its owner's drawing moves its version"
