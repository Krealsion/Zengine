# zengine.terminal -- the Terminal

**Manual.** The pane you talk to the weaves of a running Workshop through. You type a line and it
becomes one message on this Workshop's bus; the pane recalls the commands you have run, reads back
through everything the session has said, and offers, as you type, what can come next. It is a
*weave*: a piece of Workshop that is loaded, replaced and spoken to by messages, built as
`zengine-terminal-pane`. Other weaves reach it by its *office*, the name it answers to,
`zengine.terminal`, and it offers one pane, `terminal`. Hiding its legend in one layout, keeping a
reply in Inventory, finding a power from the line and taking a completion are walked through in
[Terminal](terminal.md).

## About

What you type reaches a *participant*: one identity on this Workshop's bus, with its own vocabulary
and only the authority Workshop gives it. Workshop mounts and holds it; the pane shows its record
and the line you type on, and asks Workshop for the rest: to run a line, to say what could follow
it, to pick up a value. The header names whose record it shows, `TERMINAL -- weave #3`, or `TERMINAL
-- no participant was mounted on this bus`. What the pane and Workshop exchange is declared in
[the terminal seam](../../workshop/terminal_seam_vocabulary.hpp).

A line is a verb, an address, a shape, its version, then `field=value` arguments, read by Loom's own
command grammar:

```text
send @zengine.powers FindPowers 2 text=larger
ask  @zengine.editor-switch EditorSwitchStatusRequested 1
```

`send` authors one message; `ask` authors one and remembers it until Loom's answer arrives. An
address is `#12` for one weave, `@office` for whoever holds that office when the message is
delivered, or `*` for everyone that accepts the shape. It is not a shell: it starts no process, and
knows no `cd` and no pipeline.

The record shows each entry wrapped across as many rows as it needs: `> ` a command you typed,
`!! ` a refusal, `-- ` a notice, `^ FindPowers v2 -> @zengine.powers  SUBMITTED` a message authored,
`v <shape> v<n> from #4` one received, and the same with `  [Loom: answers ask 3]` after it, the
answer to an ask. `SUBMITTED` means authored: a sender is not told its fate.

## Use

Press <!-- key desktop.terminal -->`ctrl+t`<!-- /key --> from anywhere in Workshop, even inside
another pane, and the pane opens or comes forward with your keys in it; the Pane Manager's
`Terminal` row does the same. Its keys act only while it holds your keys. Type a line and run it
([`terminal.submit`](#terminalsubmit)), or run it as typed while a candidate is chosen
([`terminal.submit-early`](#terminalsubmit-early)); ask what can come next
([`terminal.complete`](#terminalcomplete)); walk back through what you ran
([`terminal.previous`](#terminalprevious)); read back through the record
([`terminal.scroll-up`](#terminalscroll-up)); and leave ([`terminal.back`](#terminalback)).

Every other key is the line's own: characters, the caret keys, `Home` and `End`, erasing, undo,
`Shift` with the caret keys to select, and the clipboard. A press on the line places the caret, and
a second press in the same word selects it; a press does not sweep a selection. Pasted text is
flattened onto one line, and refused whole when it would not draw: `the clipboard holds bytes
outside plain ASCII, which this line cannot carry truthfully -- nothing was pasted`.

A press on a submitted, received or answer row picks up a copy of its value
([`entry`](#entry)).

Each layout keeps the pane's three settings in its row, written from Info's SETTINGS rows or by
message ([`PaneSettingCommitRequested`](../../workshop/docs/zengine.layouts.md#panesettingcommitrequested)):
`legend`, pinned, whether the shortcut legend bar shows ([`legend`](#legend)); `wheel-rows`, how
many rows of the record a wheel notch reads; and `prompt`, the line's mark, one of `>`, `$`, `%`,
`#` and `:` ([`line`](#line)). What each takes is said beside it in Info. A value a setting
cannot use, one a hand-written setup file kept, leaves its default in force, and the pane tells
Workshop, which says it with the row: `it cannot use <key> -- it takes <what it takes>`.

## Commands

### `terminal.submit`

Runs the line: the whole line, as typed, goes to the participant, which records it and authors the
message it says. With the list's cursor moved to a candidate -- by
[`terminal.previous`](#terminalprevious), [`terminal.next`](#terminalnext) or a press on its row
-- it takes that candidate instead, as [`terminal.complete`](#terminalcomplete) does, and runs
nothing; [`terminal.submit-early`](#terminalsubmit-early) runs the line as typed then. On a
recalled command it keeps that line for editing instead, and runs nothing.

- **Takes:** the line, or the candidate the cursor moved to. Default key
  <!-- key terminal.submit -->`return`<!-- /key -->.
- **Answers:** a candidate taken is written as [`terminal.complete`](#terminalcomplete) writes it,
  and the next press runs the line unless you move into the list again. A line run empties, and
  the view follows the newest output again. Workshop is asked to
  author it ([`TerminalActRequested`](../../workshop/terminal_seam_vocabulary.hpp)), and what it
  came to is on the record: `> ` your line, then `^ ... SUBMITTED`, or the participant's notice when
  it authored nothing, such as `` this pane speaks two verbs, and `ask` takes the same form as
  `send`: send <addr> <Shape> <version> [args] -- an address is #12 for one weave, @office for
  whoever holds a role, or * for everyone ``. An `ask`'s answer arrives later, as a `v` row.
- **Refuses:** an empty line authors nothing. Over the top row, beside the line: `no terminal
  participant is mounted on this bus -- nothing was authored`. A send the participant may not make,
  or to an address nothing holds now, is refused by Loom, and its outcome and detail are a notice on
  the record; it is never sent anywhere else.
- **Example:** type `ask @zengine.powers FindPowers 1 text=larger`, press
  <!-- key terminal.submit -->`return`<!-- /key -->.

### `terminal.submit-early`

Runs the line as typed, whatever the list shows: a candidate the cursor moved to is not taken. On a
recalled command it keeps that line for editing instead, and runs nothing, as
[`terminal.submit`](#terminalsubmit) does.

- **Takes:** the line. Default key <!-- key terminal.submit-early -->`shift+return`<!-- /key -->
  (`ctrl+e` from a POSIX terminal, which cannot tell Shift on Return).
- **Answers:** as [`terminal.submit`](#terminalsubmit) answers a line it runs: the line empties,
  the view follows the newest output, Workshop is asked to author it
  ([`TerminalActRequested`](../../workshop/terminal_seam_vocabulary.hpp)), and the record says what
  the line came to.
- **Refuses:** an empty line authors nothing; the rest as [`terminal.submit`](#terminalsubmit).
- **Example:** type `send @zengine.powers FindPowers 2 text=larger `, press
  <!-- key terminal.next -->`down`<!-- /key --> to read the fields it could still name, then
  <!-- key terminal.submit-early -->`shift+return`<!-- /key --> to run the line as you typed it.

### `terminal.complete`

One key for "help me here": with a list on screen it takes the candidate you are on, and with none
it asks for one. On a recalled command it keeps that line for editing instead, and asks nothing.
Typing asks too: the list follows the word you are in.

- **Takes:** the line, at its end. Default key <!-- key terminal.complete -->`tab`<!-- /key -->.
- **Answers:** Workshop is asked what can follow the line
  ([`TerminalCompletionRequested`](../../workshop/terminal_seam_vocabulary.hpp)), read at the moment
  you ask and kept by nobody; the list stands above the line ([`candidate`](#candidate)). At the
  verb, `send` and `ask`. At the address, under `where it goes -- on this bus now; not permission,
  nor a promise at send`: `*`, each office held now, `@zengine.powers   held by #12 now; reaches
  whoever holds it when sent`, then each weave by id, `#12   @zengine.powers; accepts
  ShapeDocumentRequested, FindPowers, FindPowers +1`. Then the shapes the participant knows, under
  `shapes this terminal KNOWS -- knowing one is not authority to send it`, their versions, and
  field names, never values; over the arguments, the verdict on the line, `FindPowers v2 -- ready
  to submit` or what is missing. Taking a candidate replaces the word you were typing.
- **Refuses:** with the caret inside the line, `completion follows the END of the line -- this
  caret is inside it`, and no candidate. A heading with no candidates is an answer:
  `no verb begins with 'x'; this pane speaks two`.
- **Example:** type `send @zen`, press <!-- key terminal.complete -->`tab`<!-- /key --> on
  `@zengine.powers`.

### `terminal.previous`

On an empty line, recalls the last command you ran; while recalling, the one before it. With a list
on screen, moves up the list instead, and [`terminal.submit`](#terminalsubmit) then takes the
candidate it stands on.

- **Takes:** an empty line with no list asked for, or a recalled command; otherwise the list.
  Default key <!-- key terminal.previous -->`up`<!-- /key -->.
- **Answers:** the command stands on the line, and the row above it says where you are, `history 2
  of 7 -- Enter/Tab: edit, Esc: back`. History is the commands the participant recorded, newest
  first, never its answers, notices or refusals, kept as long as its record keeps them; a command
  run twice in a row is one step. While recalling, [`terminal.submit`](#terminalsubmit),
  [`terminal.submit-early`](#terminalsubmit-early) or [`terminal.complete`](#terminalcomplete)
  keeps the line for editing and does nothing else; any other key the line takes keeps it too,
  then does its own work once; [`terminal.back`](#terminalback) puts back the line you had before.
- **Refuses:** `no command to recall yet`; at the oldest, `the oldest command`, or `the oldest kept;
  12 older entries dropped for good` when the record let older ones go.
- **Example:** press <!-- key terminal.previous -->`up`<!-- /key --> twice, then
  <!-- key terminal.submit -->`return`<!-- /key --> to keep that command, and again to run it.

### `terminal.next`

While recalling, the newer command; past the newest, the line you had before the first recall,
whole. With a list on screen, moves down the list instead, and
[`terminal.submit`](#terminalsubmit) then takes the candidate it stands on.

- **Takes:** a recalled command; otherwise the list. Default key
  <!-- key terminal.next -->`down`<!-- /key -->.
- **Answers:** the newer command, and the history row, as [`terminal.previous`](#terminalprevious).
- **Refuses:** on an empty line, `nothing newer -- Up recalls the last command`.
- **Example:** after recalling too far, press <!-- key terminal.next -->`down`<!-- /key -->.

### `terminal.back`

Sheds one layer a press, the most specific first: a recall goes back to the line before it, a list
is dismissed and the line kept, a line is cleared, and with nothing of its own left the pane is put
down.

- **Takes:** the recall, the list or the line. Default key
  <!-- key terminal.back -->`escape`<!-- /key -->.
- **Answers:** the layer goes. A dismissed list stays away for the word you were in and comes back
  at the next. With nothing left, the pane tells Workshop it did not spend the key, and Workshop
  puts the pane down: your keys go back to Workshop, the band stops saying typing goes to the
  Terminal, and the pane keeps its record, its place and its size. Nothing closes.
- **Refuses:** nothing.
- **Example:** from a half-typed line, <!-- key terminal.back -->`escape`<!-- /key --> clears it,
  and <!-- key terminal.back -->`escape`<!-- /key --> again hands the keys back.

### `terminal.scroll-up`

Reads older output: the view moves up a page, keeping one row of the page before in sight. Reading
moves nothing else: not the line, a recalled command, the list or a refusal beside the line.

- **Takes:** the record. Default key <!-- key terminal.scroll-up -->`ctrl+up`<!-- /key -->; the
  wheel over the pane reads the layout's `wheel-rows` a notch.
- **Answers:** the view stays where you leave it while new output arrives, and the rows around it
  say what is out of sight ([`newest`](#newest)). A long entry is read by scrolling: its beginning
  is above its end.
- **Refuses:** nothing; at the oldest kept row nothing moves.
- **Example:** press <!-- key terminal.scroll-up -->`ctrl+up`<!-- /key --> to read what an earlier
  ask answered.

### `terminal.scroll-down`

Reads newer output, a page down; reaching the newest row, the view follows it again.

- **Takes:** the record. Default key <!-- key terminal.scroll-down -->`ctrl+down`<!-- /key -->.
- **Answers:** the view moves, as [`terminal.scroll-up`](#terminalscroll-up).
- **Refuses:** nothing; following the newest, nothing moves.
- **Example:** <!-- key terminal.scroll-down -->`ctrl+down`<!-- /key -->.

### `terminal.oldest`

The oldest row the record keeps.

- **Takes:** the record. Default key <!-- key terminal.oldest -->`ctrl+home`<!-- /key -->.
- **Answers:** the view moves to the record's first kept row; entries dropped for good are past
  any reading.
- **Refuses:** nothing.
- **Example:** <!-- key terminal.oldest -->`ctrl+home`<!-- /key -->.

### `terminal.newest`

Back to the newest output, which the view then follows. A press on the row below the view does the
same, and so does running a line.

- **Takes:** the record. Default key <!-- key terminal.newest -->`ctrl+end`<!-- /key -->.
- **Answers:** the view follows the newest row again.
- **Refuses:** nothing.
- **Example:** <!-- key terminal.newest -->`ctrl+end`<!-- /key -->.

## Follow

The pane's record is `TranscriptShown`, which Workshop publishes from its office,
`zengine.workshop`, to any, each time the participant's record changed; a weave that wants the
record follows that rather than the pane ([the terminal seam](../../workshop/terminal_seam_vocabulary.hpp)).
The pane says nothing else unasked but its picture, to Workshop, `ClipboardCopy` when you copy
from the line, and to the layouts' owner `PaneSettingsAsked`, for its row when it arrives, and
`PaneSettingUnused` when a layout hands it a setting it cannot use.

## Shows

### `line`

The line you type on, always the bottom row: the `prompt` mark, `>` unless the layout keeps
another, a space and your text, or, empty, `>    Tab: what can this terminal say?  Up:
recall a command`. While you recall, the row above it is the history row.
A refusal stands over the top row until your next act.

### `legend`

Under the header, `SUBMITTED = authored; a sender is not told its fate`. It is the pane's
`legend` setting, on unless a layout keeps it off; a value it cannot use leaves the
row shown, saying why, `legend takes on or off, not <the value> -- on`.

### `entry`

A submitted, received or answer row, whose value you can pick up: a press drags a copy, a right
press offers `Pick up a copy`. Notices: `Picking up transcript value`, `Placing transcript copy`;
refused, `That transcript picture changed; try again`, `A transcript pickup is pending`, or
`This transcript value is no longer retained, or is only local text`.

### `newest`

While you read away from the end, the row below the view, `... 9 more rows below -- press here for
the newest`. Above the view, `... 37 more rows above; 12 older entries dropped for good`: rows above
can be scrolled to, entries dropped for good cannot.

### `candidate`

One row of the list above the line, `> ` before the one you are on, then what it is and means. The
heading says what the list is and, when cut, which slice, `1-5 of 12`. A press on a row makes it the
one you are on. Once you have moved to a row, by a press or the arrows,
[`terminal.submit`](#terminalsubmit) takes it, and the heading ends `-- Enter: take it, Shift+Enter:
run the line` where the pane has room for it after the list's own words.

## Files

The pane reads and writes no file. Your command history is the participant's record, and lasts this
run: a new launch starts with none.

## See also

- [Terminal](terminal.md): keep a command or a reply in Inventory, find a power, hide the legend in
  one layout, and take a completion or run the line as typed.
- [The terminal seam](../../workshop/terminal_seam_vocabulary.hpp), the shapes the pane and Workshop
  exchange, and [the pane protocol](../../workshop/pane_vocabulary.hpp).
