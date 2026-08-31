# What needs your attention

**Workshop as a product.** Workshop says two different kinds of thing out loud, and telling
them apart is the whole of this page:

| | |
|---|---|
| **something happened** | `committed Width = 40%`, `removed Info`, `released #12`. It was true at one instant and it is a report about the past. It goes on the **notice row** in the bottom band, and the next thing Workshop says replaces it. |
| **something is true** | your keymap file could not be read; a pane you authored is off the screen; the project is waiting on an artifact you have not built. It is still true when you read it, and it is still true an hour later. It goes to **attention**. |

The difference matters because the two need opposite lifetimes. A report about the past is
finished the moment it is said; a standing truth has to disappear **when it stops being true**
and at no other moment. A sentence somebody said once cannot do that.

## The compact indicator

When at least one thing currently deserves your attention, Workshop puts one line where the
active medium can always show it:

- in a **graphical window**, a compact box in the top-right corner of the picture — and the
  same line in the window title;
- in a **terminal**, the second reserved row at the top of the screen.

It carries the most serious condition and an honest count of the rest:

```text
keymap refused -- default bindings stand (+2 more)
```

The order is decided by how loud each condition currently is, then by its identity — never by
which one arrived last, so the line does not re-shuffle itself when something unrelated
changes. When nothing deserves attention the line is **empty**, and the box is not drawn at
all.

## The view — `Ctrl`+`a`

`Ctrl`+`a` opens the full list: every condition that is currently true, in the words of
whatever owns it, with the cursor on one of them.

| key | does |
|---|---|
| `↑` `↓` | move the cursor |
| `d` | hide the condition the cursor is on |
| `Esc` or `Ctrl`+`a` | close |

The row under the cursor also shows its owner's own explanation — the loader's refusal
sentence, the reason a pane update did not fit, what the project is waiting on — and, where
there is something you could do about it, the gesture that does it, spelled from your
effective keymap. Reading a condition never performs anything: the view lists a gesture and
you press it somewhere else.

`Ctrl`+`a` works wherever nothing on screen is taking text, which is the same rule
`Ctrl`+`c` follows. Inside the terminal line, a property draft, a setup name or a focused
pane, `Ctrl`+`a` is the text field's own **select all** and stays that way.

Opening the view is always **your** gesture. Nothing Workshop discovers — however serious —
opens a window, steals the keyboard or interrupts what you were doing.

## Hiding is not fixing

`d` hides one condition from the indicator and from the view. It changes **nothing** about
what is true:

- the condition is still true, and whatever owns it still holds it;
- the wall it describes is still standing — a refused preferences file is still refused, and
  Workshop still will not overwrite it;
- **if the condition materially changes, it comes back.** A dismissal is scoped to the exact
  statement you hid, so a wall that changes its reason is a new statement and is visible
  again. That is deliberate: hiding a condition should silence the thing you read, not the
  next thing you have not.

Hiding lasts as long as the run. Nothing about it is written to a file, and a fresh Workshop
starts by showing you everything that is true.

## What makes a condition go away

Exactly one thing: **it stops being true.** Fix the keymap file and the wall is gone at the
next launch. Send a pane a valid update and its refusal is gone immediately. Reset a pane's
place and its off-screen condition is gone the moment the place is reset. Build the frontier
and the project stops waiting.

Nothing has to be un-said, no sentence has to be overwritten, and no timer has to expire.
There are no toasts, nothing fades, and nothing disappears on its own while you are still
looking at it.

## What earns attention today

| condition | how loud |
|---|---|
| your keymap file exists and could not be read — the default bindings stand | an alert |
| your preferences file exists and could not be read — Workshop will not overwrite it | an alert |
| an older local keymap or session file is being shadowed by the one under your user directory | worth acting on, not urgent |
| a pane sent Workshop an update it could not keep | an alert |
| a pane you authored is resolvable and **no part of it is on the screen** — refused, waiting for room, or off the canvas | worth acting on |
| the project is stopped at an artifact waiting to be built | informative — waiting is not a failure |

Some true things are deliberately **not** here. A pane you closed is your own choice and lives
in the [picker](panes.md). A pane the setup names that this run cannot resolve is already
counted on Workshop's first row, all day. A pane that is behind another one is still on the screen,
and stacking is what arranging *is*. Attention is for what you would otherwise not find out.

## What this is not

- **not a notification history.** A condition that stopped being true has nothing to show. If
  you want a record of what happened, that is `--log` and `--dump`
  ([getting started](getting-started.md#launch-it)).
- **not a message queue.** Nothing accumulates, nothing is unread, and there is no badge
  counting things you have not looked at — only things that are true right now.
- **not timed.** Nothing expires, nothing auto-dismisses, and nothing animates.
- **not a decision.** Workshop never asks you a question you did not open.

## See also

- [Hotkeys and the keymap](hotkeys.md) — the one binding truth, and how to remap `Ctrl`+`a`.
- [Panes](panes.md) — the picker, management, and how a pane comes to be off the screen.
- [Setups](setups.md) — the files Workshop reads and writes, and which ones can refuse.
- [What does not work yet](limitations.md).
