# zengine.attention -- Attention

**Manual.** The pane that shows what is true right now and worth your attention: a keymap file
Workshop could not read, an update a pane sent that Workshop could not keep, a pane of yours that is
off the screen. It is a *weave*: a piece of Workshop that is loaded, replaced and spoken to by
messages, built as `zengine-attention-pane`. Other weaves reach it by its *office*, the name it
answers to, `zengine.attention`. What is true is Workshop's to say; the pane shows it, and keeps only
which of it you have chosen to hide.

## About

Workshop says two kinds of thing. *Something happened* (`hid Info -- the Pane Manager shows it
again; ...`) was true at one instant: it is a report about the past, and the next thing Workshop
says replaces it. *Something is true* (your keymap file could not be read, the project is waiting on
an artifact you have not built) is still true when you read it, and an hour later. The second kind
is a *condition*, and this pane is where conditions are shown.

Each condition is Workshop's own reading of its state, in the words of whatever owns it: a short
line, an explanation, how loud it is, and, where there is something you could do, what to press
somewhere else, spelled from your own keymap. Which conditions there are, how loud each is and their
order (the loudest first, then by key, never by which arrived last) are Workshop's, declared in
[`workshop/attention.hpp`](../../workshop/attention.hpp). Among them: a keymap, preferences or
session file that exists and could not be read; an older local file shadowed by the one under your
user directory; an update a pane sent that Workshop could not keep; a tool the load plan marks
optional that this run could not load; a pane you placed whose place is off the screen; the project
stopped at an artifact waiting to be built; a guests file that names this a development host, or
whose rows reach more or less here than their names.

A condition goes away for one reason: it stops being true, and Workshop stops saying it. Nothing has
to be un-said, nothing expires, nothing fades. The pane is not a history (a condition that stopped
being true has nothing to show), not a queue (nothing is unread and nothing is counted as unseen),
and not a question: it opens nothing, asks for no keys and asks you nothing. Reading a condition
performs nothing.

## Use

Open it from the Pane Manager, <!-- key desktop.panes -->`ctrl+p`<!-- /key -->, and put it where you
want it on the desk. Only you put it there: no condition, however loud, opens it, arranges it or
takes your keys. Its keys act only while it holds your keys: press into it first. Move the
cursor with [`attention.up`](#attentionup) and [`attention.down`](#attentiondown); the row under it
opens its explanation. Hide the one you have read with [`attention.dismiss`](#attentiondismiss).

The [glance](#glance) leads the pane, under a [notice](#notice) when there is one, and a pane one
row tall is the glance alone. A right press on the pane opens Workshop's menu for it. Each command is a row a weaver's keymap file can move
under its id; the pane spells no key of its own, so the keys a weaver sees in the band's legend and
the hotkey view are that weaver's.

## Commands

### `attention.up`

Moves the cursor up one condition, to the one before it in the list.

- **Takes:** the conditions you have not hidden, and the cursor. Default key
  <!-- key attention.up -->`up`<!-- /key -->.
- **Answers:** the cursor's row, `> ` before its words, moves up and its explanation opens beneath
  it; at the first row nothing moves. A [notice](#notice) standing on the first row is spent.
- **Refuses:** nothing.
- **Example:** with the cursor on the second condition, press
  <!-- key attention.up -->`up`<!-- /key --> to read the first one's explanation again.

### `attention.down`

Moves the cursor down one condition, to the one after it in the list.

- **Takes:** the conditions you have not hidden, and the cursor. Default key
  <!-- key attention.down -->`down`<!-- /key -->.
- **Answers:** the cursor's row moves down and its explanation opens beneath it; at the last row
  nothing moves. A [notice](#notice) standing on the first row is spent.
- **Refuses:** nothing.
- **Example:** press <!-- key attention.down -->`down`<!-- /key --> to read the second condition's
  explanation.

### `attention.dismiss`

Hides the condition the cursor is on from the list. Hiding is not fixing: the condition is still
true, whatever owns it still holds it, and the [glance](#glance) still counts it.

- **Takes:** the condition under the cursor. Default key
  <!-- key attention.dismiss -->`d`<!-- /key -->.
- **Answers:** the condition leaves the list and the [notice](#notice) says
  `hidden -- keymap refused -- default bindings stand is still true`. What is hidden is that exact
  statement: when the condition materially changes (its line, its explanation, how loud it is, or
  what it suggests) it is listed again, and when it stops being true the hiding goes with it, so if
  it becomes true later you see it. With every condition hidden, the list says
  `all conditions hidden -- 2 are still true`. Hiding is held in the pane's memory: it lasts while
  the pane's weave runs, through a reload of it in place, and is written to no file, so a fresh
  Workshop shows everything true.
- **Refuses:** nothing; with nothing listed, nothing is hidden and no notice is said.
- **Example:** press <!-- key attention.dismiss -->`d`<!-- /key --> on a condition you have read and
  will act on later.

## Follow

The pane's rows follow Workshop's `StandingConditions` v1, published by `zengine.workshop` each time
what is true changes, and to a pane newly offered: every condition true now, in Workshop's order,
an empty list when nothing is. Each publication replaces the last one whole; the pane hears it only
from `zengine.workshop`. An observer follows the same publication instead of reading the pane
([`workshop/attention_seam_vocabulary.hpp`](../../workshop/attention_seam_vocabulary.hpp)); what
you hide is the pane's own and is said to no one.

## Shows

### `glance`

The first row: the loudest condition true and a count of the rest, `keymap refused -- default
bindings stand (+2 more)`, in that condition's colour. It counts what you have hidden too. Before
Workshop has said anything it reads `ATTENTION (waiting)`; when nothing is true,
`nothing needs your attention right now`.

### `condition`

One row per condition you have not hidden, its short line in its owner's words and colour, `> `
before the cursor's; the row is named `condition:<key>`, by the condition's key. A list taller
than the room says what it left out, `... 3 earlier` and `... 4 more`.

### `explanation`

Beneath the cursor's row, indented: the condition's explanation, wrapped, then what you could
press elsewhere, `try: w arrange desk`. Cut short, it ends `... 2 more`; below four rows of list
there is none.

### `notice`

What your last [`attention.dismiss`](#attentiondismiss) did, on the first row above the glance,
until your next press of a command here.

## See also

- [What earns attention](conditions.md): every condition Workshop says, and how loud it is.
- [`zengine.builder-pane`](../../builder/docs/zengine.builder-pane.md#builderfrontier): its
  `builder.frontier` builds what the project is waiting on.
- [`attention/vocabulary.hpp`](../vocabulary.hpp): the office, the pane and the ids its keys answer
  to.
