# The line is a window

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the law it
supports is in [text-box](../workshop/text-box.md).

**Context.** A command longer than the pane lost its tail to `detail::fit` and lost the caret
with it: the published column ran past the row, where the graphical plan refuses to draw a bar
and the cell projection refuses to insert its mark. Reproduced at the default window — 109 bytes
into 83 columns, `plan_caret.present == false` at End — and in a cell medium, where no `_`
appeared anywhere (`18cb118`, "Keep the Terminal caret in sight on a long line").

**Decision.** The editable line is a window onto its text: `first_visible` beside text and caret,
with `0 ≤ first_visible ≤ caret ≤ size()` on a character boundary after every operation, and
the caret inside the window after `keep_caret_visible(N)`. The capacity is never guessed —
`terminal_input_place(sc).columns`, the number the painter cuts by and a press is answered against
— and `refresh_terminal` reconciles once per repaint, above the participant check. The left edge
snaps forwards, the right cut is a byte cut, and one column is reserved for the caret on both
media. The measurers take the box itself.

**Alternatives considered.**
- *Snapping the left edge backwards* — rejected: it carries the right edge back and pushes the
  caret one to three columns off the row; no `character_boundary_at_or_before` was ever
  committed (`git log -S` finds nothing), and the forward snap mirrors the rule a press already
  used.
- *Snapping the right cut to a character boundary* — rejected: it would shorten the row under a
  caret column computed from the window, which is how a cell medium's caret falls off its row.
- *A capacity that branches on the medium* — rejected: the two media would scroll to different
  places for a reason invisible in either projection.
- *A hidden-content marker, a scrollbar, wheel or drag scrolling, a scroll command* — none: the
  width would come out of the same one capacity.
- *Reconciling below the participant return* — rejected: a maker can type into a pane with no
  participant mounted.
- *Defaulting `first_visible` in the caret helpers* — rejected: the compiler named every call
  site, the parameter doing its job; the helpers now take the box.

**Consequences.** A resize needs no path of its own, because a new extent causes a repaint. A
press on a scrolled line lands in the full authored string. `visible_selection` is the only span
arithmetic and the selection measurers add exactly the prose offset the caret measurers add.
`pasteable_line` flattens foreign bytes into one line for the byte-equals-column grid's sake.

**Laws supported.** [WL-TEXT-03](../workshop/text-box.md), [WL-TEXT-04](../workshop/text-box.md),
[WL-TEXT-05](../workshop/text-box.md), [WL-TEXT-13](../workshop/text-box.md).
