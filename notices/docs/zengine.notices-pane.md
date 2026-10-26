# zengine.notices-pane -- Notices

**Manual.** The pane that shows what Workshop said, the newest at the bottom: each saved file, each
refusal, each thing a command of yours did. It is a *weave*: a piece of Workshop that is loaded,
replaced and spoken to by messages, built as `zengine-notices-pane`. Other weaves reach it by its
*office*, the name it answers to, `zengine.notices-pane`. The notices are the
[notices record](zengine.notices.md)'s; the pane keeps only where you are reading.

## About

Workshop says what happened when it happens -- `saved setup "Working"`, `nothing to save`, a
command refused and why -- and the [notices record](zengine.notices.md) keeps each thing said, the
oldest let go past its `keep` setting. This pane shows the record. It is pure information: no row
is selected, nothing is unread, and nothing here asks you anything; it shows what was said and
moves the view.

A notice that is said while the pane is closed is in the record, and in the pane when it opens.
What is true now, and stays true until it stops being so, is
[Attention](../../attention/docs/zengine.attention.md)'s, not this pane's.

## Use

A fresh desk has the pane at the room's bottom left; open it again from the Pane Manager,
<!-- key desktop.panes -->`ctrl+p`<!-- /key -->. While the view follows the newest, each new notice
appears at the bottom. Turn the wheel over the pane, or press into it and use
[`notices.up`](#noticesup) and [`notices.down`](#noticesdown), to read older ones: a notch of the
wheel and a key move the view by the same notice. Coming back to the newest, the view follows it
again. A right press on the pane opens Workshop's menu for it. Each command is a row a weaver's
keymap file can move under its id.

## Commands

### `notices.up`

Moves the view one notice older.

- **Takes:** the view. Default key <!-- key notices.up -->`up`<!-- /key -->; a notch of the wheel
  away from you asks it too.
- **Answers:** the view ends one notice earlier, and the row below it says how many are newer,
  `... 1 newer`, from the page the pane asks the record for
  ([`NoticesRequested`](zengine.notices.md#noticesrequested)). With every older notice already
  shown, nothing moves.
- **Refuses:** nothing.
- **Example:** press <!-- key notices.up -->`up`<!-- /key --> twice to read the notice said two
  before the newest.

### `notices.down`

Moves the view one notice newer.

- **Takes:** the view. Default key <!-- key notices.down -->`down`<!-- /key -->; a notch of the
  wheel toward you asks it too.
- **Answers:** the view ends one notice later, from the page the pane asks the record for
  ([`NoticesRequested`](zengine.notices.md#noticesrequested)); at the newest it follows each new
  notice again. At the newest, nothing moves.
- **Refuses:** nothing.
- **Example:** press <!-- key notices.down -->`down`<!-- /key --> to come back toward the newest.

## Follow

The pane asks the record for a page with
[`NoticesRequested`](zengine.notices.md#noticesrequested), as many notices as its room has rows,
and asks again each time the record says it moved (`NoticesMoved`), its room changes or the view
moves; it reads an answer to its latest ask alone. It says nothing unasked.

## Shows

### `notice`

One notice, as Workshop said it, wrapped to the pane's width with its later rows indented, in the
alert's colour when it was a refusal. Its first row is named `notice:<number>`, by the notice's
number in the run.

### `earlier`

Above the notices, how many kept notices are older than those shown, `... 4 earlier`; at the
oldest kept, when the record has let some go, how many, `... 37 let go -- the record keeps 100`.

### `newer`

Below the notices, while the view does not end at the newest, how many are newer, `... 2 newer`.

### `waiting`

`waiting for the notices record` until the record has answered, and `nothing said yet` while it
keeps nothing.

## See also

- [The notices record](zengine.notices.md): what it keeps, for how long, and its `keep` setting.
- [Attention](../../attention/docs/zengine.attention.md): what is true right now.
- [`notices/notices-pane/vocabulary.hpp`](../notices-pane/vocabulary.hpp): the office, the pane
  and the ids its keys answer to.
