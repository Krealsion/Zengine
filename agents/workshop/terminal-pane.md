# Workshop law — the Terminal pane's line and record

Register `WL-TERM`, continued from [`terminal.md`](terminal.md): what the Terminal pane does with
the line a weaver composes, the record it is shown, and the setting its layout keeps for it. One law per heading; cite by ID. Router:
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

WHY — `agents/decisions/history-is-the-participants-record.md`

## WL-TERM-13 — Enter and Tab lock a recalled line in, and that press does nothing else

LAW — Enter or Tab on a recalled line ends the recall and is spent: no submission, no candidate taken, no list asked; any other key the line takes ends the recall and then does its own work once.

MEANS
- the next Enter submits, and the next Tab asks for a list and the one after accepts;
- a key the line does not take ends no recall, and a paste asked for before it lands nowhere;
- a completion answer is taken only by the intent that asked, even when the bytes match again.

WHY — `agents/decisions/history-is-the-participants-record.md`

## WL-TERM-14 — The record is read through a view of rows, anchored to an entry

LAW — The pane wraps every entry at its width and shows a view onto those rows that follows the newest row until the weaver reads away, and then keeps the entry and wrapped row at its top.

MEANS
- new output leaves a scrolled view where it is, and a resize re-wraps under the same entry;
- Ctrl+Up and Ctrl+Down page, Ctrl+Home and Ctrl+End go to the ends, the wheel reads three rows;
- a press on the row below the view, or a submit, follows the newest again.

DOES NOT MEAN — that reading moves anything else: the line, a recall, a list and a refusal stay.

WHY — `agents/decisions/the-record-is-read-through-a-view.md`

## WL-TERM-15 — What the view is not showing is said in rows, above it and below it

LAW — The row above the view counts retained rows above it and states entries evicted for good apart; while the weaver reads away, the row below the view counts the rows below it.

MEANS
- the unit is a row, so a wrapped entry cut at the top is never counted as a message;
- when the entry being read is evicted, the view moves to the oldest kept and says so once;
- in a room with no row for the one below, the row above says both.

WHY — `agents/decisions/the-record-is-read-through-a-view.md`

## WL-TERM-19 — The Terminal's legend is a setting each layout keeps

LAW — The Terminal declares one setting, `legend`, a flag on by default; handed it off, its legend row's place goes to the record, and handed a value it cannot use, the legend row stays and says why.

MEANS
- it takes what it is handed whole and holds it in plain members, so a reload is handed it again;
- the legend row is its part `legend`, there while it shows and gone while it does not;
- every row below it moves when it shows or hides, so no press aimed at the last picture lands.

DOES NOT MEAN
- the band's legend preference (WL-KEY-09): that is the keymap file's, over Workshop's own rows.

WHY — `agents/decisions/a-setting-is-a-typed-value-its-row-keeps.md`

## Do not assume

- That a reload keeps a recall or a scrolled view — it keeps the line before the recall, and
  follows the newest output again (WL-TERM-12, WL-TERM-14).

## Proven by

### WL-TERM-12
`terminal/pane.cpp` composing_nothing Recall
`terminal/history_and_completion.hpp` recallable recall cancel_recall show_recalled history_heading
`tests/test_workshop_panes_terminal.cpp` "history says where its ends are and past the newest is the
line before the recall" "Escape on a recalled line goes back to the line before the recall and keeps
the keys" "a command being composed keeps Up and Down for its completion list and its draft is
untouched" "history is the participant's commands only and eviction bounds it"

### WL-TERM-13
`terminal/pane.cpp` on(PaneActionRequested) on(TerminalCompletionOffered)
`terminal/history_and_completion.hpp` settle_recall moved offer_applies ask_completion
`tests/test_workshop_panes_terminal.cpp` "on an empty line Up Up Enter leaves the older command
ready to edit and runs nothing until the next Enter runs it" "Tab on a recalled line locks it in
with no candidate taken and no list asked and the next two Tabs ask and then accept" "any other key
leaves a recall and does its own work once: typing or a caret key or an erase or a shifted letter"
"after a lock the recalled line is an ordinary draft whose undo stops at the recalled text and whose
copy reaches the process" "a paste asked for before a recall does not land in the recalled line" "a
completion answer asked before a recall is neither shown on the recalled line nor taken by its lock
though the bytes match"

### WL-TERM-14
`terminal/pane.cpp` Reading wheel press
`terminal/view.hpp` wrap_record WrappedRecord view_top scroll page
`terminal/vocabulary.hpp` kActionScrollUp kActionScrollDown kActionOldest kActionNewest
`tests/test_workshop_panes_terminal.cpp` "a long entry is read whole by scrolling and no row of it
is clipped for good" "new output leaves a scrolled view where it is and counts itself below while a
following view shows it" "the newest output is one press on the row below the view or Ctrl+End or a
submit away" "a resize re-wraps under the entry being read and the line and its caret stay usable"
"a list growing under a scrolled view takes rows from its bottom and leaves its top and the line"
"the wheel reads the record three rows a notch and moves nothing else" "reading keys leave a recall
and the line as they were"

### WL-TERM-15
`terminal/view.hpp` omission_text below_text say
`tests/test_workshop_panes_terminal.cpp` "the pane says what it is not showing, in the two senses
that differ" "a view scrolled into the middle says the rows above at its top and the rows below at
its bottom" "when the entry being read is evicted the view moves to the oldest kept and says why"
"in every small room the line is the last row and its caret and press agree while reading and
composing"

### WL-TERM-19
`terminal/pane.cpp` on(ws::PaneSettings) legend_setting offered legend_shown_ legend_problem_
named_rows
`terminal/view.hpp` say
`terminal/vocabulary.hpp` kSettingLegend
`tests/test_workshop_panes_legend.cpp` "a layout keeps the Terminal's legend hidden while another
shows it, written through Info's rows" "a Terminal seated again is handed its layout's legend before
its room, and its first picture keeps it" "after a restart the Terminal's first picture keeps its
layout's legend, and so does a reload's" "a legend value the Terminal cannot use is kept as stored,
and said in its legend row" "a Terminal closed in a layout that hid its legend shows it again when
it is shown" "a press aimed at the Terminal's picture from before its legend hid is refused"
