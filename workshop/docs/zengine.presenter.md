# zengine.presenter -- the menu presenter

**Manual.** The menu presenter shows a pane's menu. It is the weave in the office
`zengine.presenter` (an office is the name a weave answers to), built as `zengine-menu-presenter`
and loaded by a row of the load plan like any weave. It has no pane of its own: when a pane asks
Workshop for a menu, Workshop hands the menu here, and this presenter lays out its lines, moves the
cursor and tells the pane which row you chose. You can reload it in place, or load another
presenter in its office ([replacing it](panes.md#replacing-the-menu-presenter)).

## About

A pane offers rows (an id and a label each) and asks Workshop to present them
(`PaneMenuRequested`, [`workshop/pane_vocabulary.hpp`](../pane_vocabulary.hpp)). Three parties
share a menu, each owning one thing. The pane owns what its rows mean and performs every
operation. Workshop owns your keys and presses while a menu is open, the popup and where it
opens, one menu at a time, and the standard rows it adds beneath the pane's own (its own pane
menu, which it spends itself). The presenter owns how the menu is shown and how long it lives,
and answers the pane exactly once for each menu: chosen, dismissed, withdrawn by Workshop, or
refused. It performs nothing a row means, reads no pane's subject and holds no pane's authority.

The menu it has open is its state, `HeldMenu` v2: the menu's number, who asked, the rows, the
cursor, the room and the standard rows. A reload keeps it, so a reloaded presenter shows the open
menu again and answers it under the same number. How it laid the lines out is not state: a new
image lays them out afresh. The seam between Workshop and whatever holds this office is
[`workshop/presenter_vocabulary.hpp`](../presenter_vocabulary.hpp), installed with the pane
protocol.

## Use

A pane asks for its menu when you right-press it, or press the key it declares for its menu (the
Builder pane's is [`builder.menu`](../../builder/docs/zengine.builder-pane.md#buildermenu)), and
Workshop hands the menu here. The pane's rows come first, then a rule naming the pane, `  -- Builder --`, then Workshop's
standard rows. The row under the cursor is drawn `> label`, every other `  label`. A menu taller
than its room is windowed, with `  ... 3 earlier` above and `  ... 5 more` below.

Move with the keys your keymap gives an open menu's up and down ([hotkeys](hotkeys.md)): they pass
over the rule and stop at the ends. Its choose key chooses the row under the cursor, and its back
key dismisses the menu. A press of the primary button on a row chooses that row; a press outside
the popup dismisses the menu and does nothing else; a release means nothing. A standard row you
choose is spent by Workshop on the pane, and the pane is told nothing was chosen.

## Commands

### `MenuGranted`

`MenuGranted` v2 asks the presenter to present one pane's menu, as menu number `menu`.

- **Takes:** `menu`, Workshop's number for this menu; `office` and `pane`, the pane that asked, as
  Loom authenticated it; `subject`, the pane's own word, echoed back; `rows`, the pane's rows;
  `room_rows` and `room_columns`, the most the popup can show now; `standard`, Workshop's standard
  rows for that pane; `pane_name`, the pane's name as you know it, for the rule.
- **Answers:** shows the menu to Workshop (`MenuShown` v2, [Follow](#follow)); later, when you
  choose or dismiss it, closes it (`MenuClosed` v2) and answers the pane with `PaneMenuAnswered`
  under the pane's own correlation.
- **Refuses:** an offer it cannot show is refused in these words, given straight back to Workshop
  and answered to the pane unchosen: `nothing to present -- the request offered no rows`; `too many
  rows to present` (more than `kMaxPaneMenuRows`); `a row's id is empty or too long, or its label is
  too long`; `no room to present a menu here`. A grant from anyone but Workshop, or with no number,
  is ignored. A grant while another menu is open answers the older one `replaced by a newer menu`.
- **Example:** a pane sends `PaneMenuRequested` with two rows; Workshop grants them here as menu 4,
  and this presenter shows `> Rename` and `  Delete`, then the rule and Workshop's rows.

## Hears

On every activation, Loom's `zen.Activated`, it tells Workshop it holds the office and which menu
it carries (`PresenterReady`), and shows that menu again if it carries one. Every other word it
heeds comes from Workshop, `zengine.workshop`, about a menu with a number; anyone else's is
ignored.

### `MenuInput`

`MenuInput` v1, from Workshop: you did something to menu `menu`: a key (its verb: up, down, choose
or back), a press on a line of the picture you saw, or a press outside. A press aimed at lines it
has since replaced is ignored. For a menu it does not hold, it hands the act back: `this image does
not hold that menu`.

### `MenuWithdrawn`

`MenuWithdrawn` v1, from Workshop: Workshop ended menu `menu`, and `why` says how. The presenter
answers the pane unchosen in those words. For a menu it does not hold, it hands it back as it does
an act.

## Follow

It speaks to Workshop and to the pane that asked, never to everyone, always as `zengine.presenter`.
To Workshop: `PresenterReady` v1 on every activation; `MenuShown` v2 each time the lines change, a
line naming the row it shows by the row's id, `picture` moving when a line would now choose
something else; `MenuClosed` v2 when a menu ends, `chosen` or not, with the standard row chosen if
one was; `MenuReturned` v1 for a menu it was handed and does not hold. To the pane: one
`PaneMenuAnswered` per menu, chosen with the row's id, or not chosen with why: `dismissed`, `a
standard row was chosen`, Workshop's words, or a refusal. Workshop shows the open menu on the desk;
a pane learns its menu's end only from that one answer.

## See also

- [Replacing the menu presenter](panes.md#replacing-the-menu-presenter), and
  [`examples/numbered-presenter`](../../examples/numbered-presenter/presenter.cpp), another presenter.
- [The menu presenter, and replacing it](workshop-panes.md#the-menu-presenter-and-replacing-it):
  the seam in full.
- [`workshop/presenter_vocabulary.hpp`](../presenter_vocabulary.hpp), the seam's shapes, and
  [`workshop/pane_vocabulary.hpp`](../pane_vocabulary.hpp), the pane protocol's.
