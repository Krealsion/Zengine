# Workshop law — the desk, said whole

Register `WL-READ`: the desk read in one turn, a pane's reading paged under its stamp, what covers
a pane and what is said of it, the rate desk reads are answered at, the words Workshop says of its
own surfaces, the notice that the desk moved, the stamp that names what a pane shows, and the window
a pane drawing in an area of its own is read from. The shapes are public in
[workshop panes](../../workshop/docs/workshop-panes.md); the reader that writes them on an agent's
host is [Drive Workshop from another host](../../external-host/docs/external-host.md)'s. One law
per heading; cite by ID. Router: [`../workshop.md`](../workshop.md).

Retired: WL-READ-01, WL-READ-04, WL-READ-06, WL-READ-07, WL-READ-08

## WL-READ-02 — A pane's reading stands on its stamp and pages under it

LAW — A pane's reading names its stamp, the place it shows in and its text measure, says its words and parts from `from` within one reply, and no word of what is in flight.

MEANS
- a page asked under a stamp the pane no longer stands on is refused as stale;
- a cover moving between pages leaves the stamp alone: a reader compares `total` and `covered`, and a `from` past a shrunk reading is refused;
- in flight is a window waiting for the pane's drawing, or a reading asked and not yet answered, said so at any page; a pane whose role nobody holds is no reading, and says it waits.

WHY — `agents/decisions/the-desk-is-said-whole-on-the-agents-host.md`

## WL-READ-03 — Desk reads are a few a second, each asker

LAW — Workshop answers at most four `DeskReadRequested` a second from one asker, and refuses the next in words naming when it may read again.

WHY — `agents/decisions/the-desk-is-said-whole-on-the-agents-host.md`

## WL-READ-05 — A reading says what is visible, and points only where a press lands

LAW — A pane's reading says no word or part of a covered place, counting them and naming the cover, and gives a point only where a press lands on what it names.

MEANS
- a cover is a pane in front, the menu, arranging, or the band -- the foot band over Layouts too -- each by Workshop's own order, as it arranges each;
- while a menu is open a primary press outside it reaches nothing beside it -- it closes the menu, or is refused -- so nothing there has a point.

WHY — `agents/decisions/the-desk-is-said-whole-on-the-agents-host.md`

## Do not assume

- That a reading is consistent across panes: each pane's stands on its own stamp, and the desk on
  its number; the reader says which panes were read on another picture than the desk read named.
- That a fingerprint orders pictures: it says whether two are the same, never which came later.
- That the window's words are a pane's own: what another role draws in front is left out, what
  Workshop draws in front is Workshop's cover.

## Proven by

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

### WL-READ-05
`workshop/pane_view.hpp` PaneCover
`workshop/weave_desk_read.cpp` covers_of pane_reading area_reading layouts_reading
`tests/test_workshop_desk.cpp` "a covered pane says its visible words only, and what covers it: a
pane in front, Workshop's menu, arranging -- and beside a menu nothing has a point" "a pane's words
are covered while a menu is open or arranging, no point a press would land on, and the desk still
answers" "Layouts and the band are read as Workshop draws them: tabs as named parts, the band's
words with no point"
`tests/test_graphics.cpp` "an area's arranger reads what the TUI window shows of it: its text
by row in the area's pixels, each part by its box and the cell a press reaches it by, what another's
area in front paints over left out and its own hiding nothing, and asked again since its version,
the same"
