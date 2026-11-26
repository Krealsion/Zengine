# The laws of attention (WL-ATTN)

**Laws.** The numbered rules kept about what is true right now: by Workshop, which holds and ranks
the conditions, and by the Attention pane, which shows them. A thing that happened and a thing that
is true are two surfaces, and these laws keep them two. Each is a rule that more than one file
relies on, and a comment or a page that relies on one names it by its number; a rule about one
declaration is in that declaration's comment. The conditions themselves and how loud each is are
[what earns attention](conditions.md), and the pane's keys and rows are [its
manual](zengine.attention.md). The files and cases that prove each law are listed in
[`tests/laws/attention.txt`](../../tests/laws/attention.txt), which the `law_register` check reads.

## WL-ATTN-01 — An utterance and a condition are two surfaces

An utterance is a sentence about a moment that passed, kept in the notices record (WL-NOTE-02). A
condition is a fact that is true when it is read, held under a key or derived from a live owner.

What this means:

- An utterance is never taken back: the record lets the oldest go past its bound, and nothing else
  ends one.
- A condition disappears because it resolved, never because something else was said.

Why: [a condition has a lifetime](../../agents/decisions/a-condition-has-a-lifetime.md).

## WL-ATTN-03 — `attention_conditions` is a pure projection

`attention_conditions` reads the held conditions and the derived owners, ranks them, and owns
nothing. It is the one population every consumer on Workshop's side of the seam spends.

What this means:

- What a weaver has hidden is taken away on the pane's side, and nowhere here.
- So the glance says what is true, and the list says what this weaver is looking at.

Why: [a condition has a lifetime](../../agents/decisions/a-condition-has-a-lifetime.md).

## WL-ATTN-04 — A derived condition stays derived

A derived condition is never copied into the held conditions; its owner's next truth clears it,
with no call to take it back.

What this means:

- The project frontier is a condition while it waits, read from its owner at each repaint, and
  nothing after.
- A pane's drawing that a window refuses is no condition of Workshop's: the window tells the
  drawer (`DrawnRefused`), which says so in its own picture.

What it does not mean: that Workshop judges what a pane draws, or marks a refused drawing. It draws
no pane's body.

Why: [a condition has a lifetime](../../agents/decisions/a-condition-has-a-lifetime.md).

## WL-ATTN-07 — Ranking is `ranks_before`: loudness, then key

`attention_rank` is the one place this application says one role is more urgent than another. Its
order is total, and an unknown role ranks last.

Why: [a condition has a lifetime](../../agents/decisions/a-condition-has-a-lifetime.md).

## WL-ATTN-08 — Hiding is the pane's, scoped to the statement and not to the key

The Attention pane remembers the key and a stamp of the statement it hid, so a condition whose
content changes is shown again. A hiding outlives no condition.

What this means:

- It is never written to a file: it crosses a same-shape reload in the pane's state, and reaches no
  file.
- Hiding is not resolving: Workshop still holds the condition, derives it and says it.
- So a list whose every current condition is hidden says they are hidden, never the all-clear.

Why: [a condition has a lifetime](../../agents/decisions/a-condition-has-a-lifetime.md).

## WL-ATTN-10 — A condition names an action and holds no power

A condition names an action by its catalog id, which Workshop turns into words against the
effective keymap. The id never crosses to the pane, and no loudness opens or arranges anything.

What this means:

- The pane is handed a sentence, and could not press an action if it were given one.
- Nothing puts this pane on a screen but a weaver opening it from the Pane Manager.

Why: [a condition has a lifetime](../../agents/decisions/a-condition-has-a-lifetime.md).

## WL-ATTN-11 — The condition path holds no timer, no callback and no history

`workshop/attention.hpp` includes exactly `graphics/colors.hpp`, the pool's names. Nothing in the
path schedules, calls back, records or persists, and showing a condition implies no history.

What this means:

- The form that crosses the seam is the seam's; the internal type crosses nothing.
- An observer can see what Workshop says, and that grants nobody the authority to observe.

Why: [a condition has a lifetime](../../agents/decisions/a-condition-has-a-lifetime.md).
