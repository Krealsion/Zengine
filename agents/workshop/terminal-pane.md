# Workshop law — the Terminal pane's line and record

Register `WL-TERM`, continued from [`terminal.md`](terminal.md): what the Terminal pane does with
the line a maker composes and the record it is shown. One law per heading; cite by ID. Router:
[`../workshop.md`](../workshop.md). The participant, the picture and the completer are
[`terminal.md`](terminal.md); what crosses the pane seam is [`../panes.md`](../panes.md).

## WL-TERM-12 — Up and Down walk the participant's commands while nothing is composed

LAW — While the line is empty with no list asked for, or while a recalled line is browsed, Up and Down walk the `command` entries of the record the pane is shown, newest first.

MEANS
- a run of one command is one step, and a position is `dropped` + index, which eviction keeps;
- past the newest, or on Escape, the draft from before the recall comes back whole;
- a command being composed, or a list asked for on an empty line, keeps Up and Down for the list.

DOES NOT MEAN
- a log the pane keeps: answers, notices and refusals are never recalled, and eviction bounds it;
- history across launches: the participant and its record belong to the run.

PROVEN BY — `terminal-pane/pane.cpp` `recallable`, `recall`, `composing_nothing`,
`cancel_recall`, `show_recalled`, `history_heading`, `Recall`;
`tests/test_workshop_panes_terminal.cpp` case `"history says where its ends are and past the
newest is the line before the recall"`, case `"Escape on a recalled line goes back to the line
before the recall and keeps the keys"`, case `"a command being composed keeps Up and Down for its
completion list and its draft is untouched"`, case `"history is the participant's commands only
and eviction bounds it"`.
WHY — `agents/decisions/history-is-the-participants-record.md`

## WL-TERM-13 — Enter and Tab lock a recalled line in, and that press does nothing else

LAW — Enter or Tab on a recalled line ends the recall and is spent: no submission, no candidate taken, no list asked; any other key the line takes ends the recall and then does its own work once.

MEANS
- the next Enter submits, and the next Tab asks for a list and the one after accepts;
- a key the line does not take ends no recall, and a paste asked for before it lands nowhere;
- a completion answer is taken only by the intent that asked, even when the bytes match again.

PROVEN BY — `terminal-pane/pane.cpp` `settle_recall`, `moved`, `offer_applies`,
`ask_completion`, `on(PaneActionRequested)`, `on(TerminalCompletionOffered)`;
`tests/test_workshop_panes_terminal.cpp` case `"on an empty line Up Up Enter leaves the older
command ready to edit and runs nothing until the next Enter runs it"`, case `"Tab on a recalled
line locks it in with no candidate taken and no list asked and the next two Tabs ask and then
accept"`, case `"any other key leaves a recall and does its own work once: typing or a caret key
or an erase or a shifted letter"`, case `"after a lock the recalled line is an ordinary draft
whose undo stops at the recalled text and whose copy reaches the process"`, case `"a paste asked
for before a recall does not land in the recalled line"`, case `"a completion answer asked before
a recall is neither shown on the recalled line nor taken by its lock though the bytes match"`.
WHY — `agents/decisions/history-is-the-participants-record.md`

## WL-TERM-14 — The record is read through a view of rows, anchored to an entry

LAW — The pane wraps every entry at its width and shows a view onto those rows that follows the newest row until the maker reads away, and then keeps the entry and wrapped row at its top.

MEANS
- new output leaves a scrolled view where it is, and a resize re-wraps under the same entry;
- Ctrl+Up and Ctrl+Down page, Ctrl+Home and Ctrl+End go to the ends, the wheel reads three rows;
- a press on the row below the view, or a submit, follows the newest again.

DOES NOT MEAN — that reading moves anything else: the line, a recall, a list and a refusal stay.

PROVEN BY — `terminal-pane/pane.cpp` `wrap_record`, `WrappedRecord`, `view_top`, `scroll`,
`page`, `Reading`, `on(PaneWheel)`, `on(PanePressed)`; `terminal-pane/vocabulary.hpp`
`kActionScrollUp`, `kActionScrollDown`, `kActionOldest`, `kActionNewest`;
`tests/test_workshop_panes_terminal.cpp` case `"a long entry is read whole by scrolling and no row
of it is clipped for good"`, case `"new output leaves a scrolled view where it is and counts
itself below while a following view shows it"`, case `"the newest output is one press on the row
below the view or Ctrl+End or a submit away"`, case `"a resize re-wraps under the entry being read
and the line and its caret stay usable"`, case `"a list growing under a scrolled view takes rows
from its bottom and leaves its top and the line"`, case `"the wheel reads the record three rows a
notch and moves nothing else"`, case `"reading keys leave a recall and the line as they were"`.
WHY — `agents/decisions/the-record-is-read-through-a-view.md`

## WL-TERM-15 — What the view is not showing is said in rows, above it and below it

LAW — The row above the view counts retained rows above it and states entries evicted for good apart; while the maker reads away, the row below the view counts the rows below it.

MEANS
- the unit is a row, so a wrapped entry cut at the top is never counted as a message;
- when the entry being read is evicted, the view moves to the oldest kept and says so once;
- in a room with no row for the one below, the row above says both.

PROVEN BY — `terminal-pane/pane.cpp` `omission_text`, `below_text`, `say`;
`tests/test_workshop_panes_terminal.cpp` case `"TERM-W9: the pane says what it is not showing, in
the two senses that differ"`, case `"a view scrolled into the middle says the rows above at its
top and the rows below at its bottom"`, case `"when the entry being read is evicted the view moves
to the oldest kept and says why"`, case `"in every small room the line is the last row and its
caret and press agree while reading and composing"`.
WHY — `agents/decisions/the-record-is-read-through-a-view.md`

## Do not assume

- That a reload keeps a recall or a scrolled view — it keeps the line before the recall, and
  follows the newest output again (WL-TERM-12, WL-TERM-14).
