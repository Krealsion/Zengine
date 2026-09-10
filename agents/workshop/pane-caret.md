# Workshop law — a pane's caret

Register `WL-CARET`: the second sentence a pane may say about its rows — where its insertion
point is, and what it has selected — and what Workshop does with one. One law per heading;
cite by ID. Router: [`../workshop.md`](../workshop.md). What a pane publishes as ROWS is
[`panes-and-windows.md`](panes-and-windows.md) (`WL-PANE`) and [`../panes.md`](../panes.md);
the first pane to say a caret is the Terminal ([`terminal.md`](terminal.md)).

## WL-CARET-01 — A pane with a caret says where it is, beside its rows

LAW — `PaneCaret` is a second sentence a pane MAY say: where its caret is in the body lattice it was granted, and the range it has selected. It is not a field on `PaneContent`.

MEANS
- Workshop merges it into its region with its own header offset, which a press subtracts;
- `surface::kNoCaret` on the row is a pane saying it has none: ordinary speech;
- a caret spoken personally, or about a pane the office never offered, is nothing.

DOES NOT MEAN — that a pane asked for anything. There is no blink, shape, width, colour,
visibility, scroll request or claim on the keyboard: a pane said where, inside rows it
already sent, the insertion point of text it already wrote is.

PROVEN BY — `workshop/pane_vocabulary.hpp` `PaneCaret`; `workshop/panel.hpp`
`ExternalPane::caret_row`, `ExternalPane::caret_col`, `ExternalPane::sel_begin_row`;
`workshop/weave_seam.cpp` `on(PaneCaret)`; `workshop/screen_external.cpp` `paint_external`;
`tests/test_workshop_panes_seam.cpp` case `"CARET-1: a caret is judged against the CONTENT, and
merged with the header's offset"`, case `"CARET-4: a caret spoken personally, or about somebody
else's pane, is nothing"`.
WHY — `agents/decisions/a-presentation-owns-no-facts.md`

## WL-CARET-02 — A pane's caret is one door, and no caret is one call

LAW — `ExternalPane::clear_caret` is the only way a pane's caret and its selection are un-said, so "refused whole" is one call rather than six assignments somebody can write five of.

PROVEN BY — `workshop/panel.hpp` `ExternalPane::clear_caret`; `workshop/weave_seam.cpp`
`on(PaneCaret)`, `on(PaneContent)`; `tests/test_workshop_panes_seam.cpp` case `"CARET-3:
`kNoCaret` is a sentence, and shorter content drops a caret it outgrew"`.
WHY — `agents/decisions/a-presentation-owns-no-facts.md`

## WL-CARET-03 — A caret is judged against the content, and refused whole

LAW — `judge_caret` answers against the rows the pane last had ACCEPTED, never against the room it was granted, and a refusal leaves the pane with NO caret rather than with its previous one.

MEANS
- one past the last byte is legal: an insertion point sits there at a line's end;
- the rows survive a refused caret, because they were judged on their own;
- content and caret are two messages, so shorter rows drop a caret admitted against the old.

DOES NOT MEAN — that a stale caret is kept the way stale `PaneActions` rows are. Rows are
still a set of rows; a position that is wrong is read as a fact about where the maker is typing.

PROVEN BY — `workshop/weave.hpp` `judge_caret`; `workshop/weave_seam.cpp` `judge_caret`,
`on(PaneCaret)`; `tests/test_workshop_panes_seam.cpp` case `"CARET-2: a caret naming a row the
content does not have is refused WHOLE"`, case `"CARET-3: `kNoCaret` is a sentence, and shorter
content drops a caret it outgrew"`.
WHY — `agents/decisions/a-presentation-owns-no-facts.md`
