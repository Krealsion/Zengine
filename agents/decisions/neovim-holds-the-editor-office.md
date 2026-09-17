# Neovim holds the Editor's office

**Decision record.** One decision, its alternatives, and why this one. The law it supports is in
[neovim](../workshop/neovim.md).

**Context.** A maker wants to edit in Neovim inside a running Workshop, and from a Loom that has
no Workshop, without losing what the Editor's office promises: that Files, the Builder and Edit
Code reach the editor, that an open is one commitment, that the quit asks, and that a switch
carries the document.

**Decision.** Neovim is another implementation of the Editor, authored as a load plan choice for
`zengine.editor` and loaded as its own weave. It holds the office and the Editor's pane key, runs
one Neovim per incarnation, and speaks every door the standard Editor speaks. Its screen crosses
in the pane protocol's existing words -- ASCII rows under one status row, one caret, one range --
and a maker's keys and text cross back as Neovim input; `^s` and `^o` stand in for Workshop's own
rows while its pane holds the keys. Questions to Neovim are bounded and asked beside its fast mode;
the flow is never waited on. From a Loom with no Workshop the same weave starts Neovim listening,
and Neovim's own interface attaches from a second terminal.

**Alternatives considered.**
- *A terminal emulator pane running `nvim`* -- refused: the office's facts (the document, its
  identity, a transfer) would be scraped from a picture rather than asked.
- *New Surface vocabulary for semantic spans* -- deferred: the existing protocol carries a usable
  editor now, and spans are the named next seam.
- *Neovim's interface inside `loom-host`'s console* -- refused: the console would be taken from
  the maker; a second terminal leaves it where it is.
- *Carrying a live Neovim across a reload of this image* -- deferred: a reload is refused while
  Neovim runs, in words, so nothing is lost silently.

**Consequences.** `document.open` became ownable, as `document.save` is. The Lua module is
installed per Neovim and touches no maker configuration; the profile is `clean` unless the maker
opts in. A copy and a paste cross through the Skin exactly as the standard Editor's do.

**Laws supported.** [WL-NVIM-01](../workshop/neovim.md), [WL-NVIM-02](../workshop/neovim.md),
[WL-NVIM-03](../workshop/neovim.md), [WL-NVIM-04](../workshop/neovim.md),
[WL-NVIM-05](../workshop/neovim.md), [WL-NVIM-06](../workshop/neovim.md),
[WL-NVIM-07](../workshop/neovim.md), [WL-NVIM-08](../workshop/neovim.md).
