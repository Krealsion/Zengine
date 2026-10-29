# The row is its own scrub track

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the law it
supports is in [pointer](../workshop/pointer.md).

**Context.** `fit` and `fit_path` bound a line and mark the cut, and a weaver had no way to read
what was cut short of widening a pane. A timed marquee needs a repaint with no event behind it,
and this application publishes a canvas only when it has been told something (`1bc34e6`, "Let
arranging choose the pane, a double-click choose the word, and a pointer read what a row cut").

**Decision.** `Session::reveal` is presentation only, and `detail::reveal_shown` returns the
revealed window only when surface, item, a non-zero offset and the string all agree — the guard
is the reset. The item is the identity, never the prose row. The pointer's column is the offset:
the row's left edge is the value's start, the right edge its end, everything between
proportional and monotone, the head marked as the tail is. The first consumer set is four rows.
A mode or a held gesture owns the pointer and the reveal does not. The terminal cannot report a
hover.

**Alternatives considered.**
- *A timed marquee* — rejected structurally: no beat reaches Workshop, and asking the Timer
  service for one would be Loom participation for a presentation.
- *Binding the reveal to a prose row* — rejected: a row follows whatever scrolled into it, the
  neighbouring-row defect.
- *Widening the external pane protocol to ask for a longer text* — rejected: a provider's
  already-shortened text is not recovered.
- *A registry of revealable rows* — refused: a fifth consumer is one `reveal_shown` call at the
  painter and one arm in the resolver.
- *Moving the terminal to any-motion tracking (`1003`)* — rejected: it would price every idle
  motion in every session; `1002` is documented as a medium fact in
  `workshop/docs/limitations.md`.

**Consequences.** Eligibility is `rest != full`, so a value that fits is perfectly still. A live
draft is excluded — it is windowed against its own caret. Nothing durable holds the reveal, and
no file, setup, document, provider or value is touched. On a terminal the gesture does not exist.

**Reversed in the host, and returned in a pane.** Workshop's reveal is gone: its whole consumer set
was the Info pane's two rows, and the Info pane became a loaded weave, which sends rows it has
already cut, so the unfitted string the reveal depended on never reaches the host. The alternative
this record rejected — widening the pane protocol to ask for a longer text — is still refused.
The decision lives where the value does: Files holds its paths, and the pane protocol's facets hand
it the resting pointer, so its path rows are their own scrub track (WL-FILES-20), by the same rule
-- the pointer's column the offset, the left edge the start and the right the end. The host's four
retired laws are in [pointer](../workshop/pointer.md); the one about a medium stands.

**Laws supported.** [WL-FILES-20](../workshop/files.md), [WL-PTR-09](../workshop/pointer.md).
