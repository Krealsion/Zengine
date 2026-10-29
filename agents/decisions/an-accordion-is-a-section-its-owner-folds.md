# An accordion is a section its owner folds

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the laws it
supports are in [files](../workshop/files.md),
[desktop-presenting](../workshop/desktop-presenting.md) and [info-body](../workshop/info-body.md).

**Context.** Two panes had more than a weaver wants in view at once and nothing to fold it with:
Files' places -- the origin, the marks and the roots, reachable only one at a time by `n` -- and the
Hotkeys pane's keys, every pane's in one long table. One is a part of a pane, kept per layout like
any choice about how a pane presents itself; the other is content, which a layout does not own.

**Decision.** `component::Accordion` is one section -- a header that always shows and a body that
shows while it is open, or over what follows while the pointer rests on a shut one's header -- and
`IndependentAccordions` the group whose sections open and shut alone, one act shutting them all:
a value and its arithmetic in `component/`, with no medium. Its owner keeps the fold, and the
group borrows the owner's list of open sections rather than copying it. A part of a pane is the
pane's own word in its layout's row, which the pane commits itself and draws only as the row comes
back (Files' `places`); content is the pane's state, read there and written there (the Hotkeys
pane's sections, Info's nested levels). A tree is the accordion at every level (`shown_tree`).
A peek is presentation the pane draws from the resting pointer its facet hands it, and nothing
keeps it. No fold writes a size: the pane offers again, and a `fit` row follows.

**Alternatives considered.**
- *A part in `graphics/elements/`* -- not first: no pane draws with those parts, and a face the
  window draws would make the fold the window's state and the press never the pane's.
- *The fold kept by Workshop* -- rejected: only the pane knows its parts, and its row or its state
  already keeps what a pane keeps.
- *An exclusive group or a pair* -- not built: neither pane opens a section by shutting another.
- *A key that peeks* -- rejected: a key does what a press does, and a press opens.

**Consequences.** A POSIX terminal reports no resting pointer (WL-PTR-09), so there a section opens
only when pressed or keyed. A pane that does not hear rests is told nothing new and accepts nothing
new (WL-CANVAS-06).

**Laws supported.** [WL-DESK-21](../workshop/desktop-presenting.md),
[WL-FILES-18](../workshop/files.md), [WL-INFO-16](../workshop/info-body.md).
