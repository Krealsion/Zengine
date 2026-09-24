# A held change is owned until it is answered

**Decision record.** One decision, its alternatives, and why this one. The law it supports is in
[neovim-transfers](../workshop/neovim-transfers.md).

**Context.** The Neovim-backed Editor asks Neovim for a drop's insertion and a location's cursor
within a bound, beside Neovim's fast mode. Measured at `3e96a7c` with the loaded pane and menu: a
command's Terminal line chosen while Neovim sat in an unfinished `g` was said "nothing was
inserted", and the Escape that ended the wait inserted it, modified. msgpack-RPC has no cancel,
and Neovim runs a request it held once its wait ends, in the order sent, after keys already typed.

**Decision.** A change Neovim leaves unanswered is held, not refused. `Host::ask` says
`Outstanding` and hands the late answer, or Neovim's end, to the caller exactly once. The Editor
keeps one held change, sent once with its whole target -- buffer, changedtick, cell and screen
row, or line and its saved text -- which Neovim checks again when it runs it. The notice and the
status row say what waits; keys still reach Neovim; another drop, an open and a switch refuse
until the change is said: in, refused because its target moved, or ended with Neovim.

**Alternatives considered.**
- *Tried: reading silence as a refusal* -- the change ran later, after the Editor had said it did
  not; pinned by case `"a command chosen from the drop's menu while Neovim waits for input is
  held, never refused: typing still reaches Neovim, another drop, an open and a switch wait for
  it, and it goes in once Neovim stops waiting, said once, one undo taking it back"`.
- *Argued: cancelling or expiring the request* -- a cancel asked later is queued behind it, and a
  deadline needs a clock both processes share and still a later answer to know the outcome.
- *Argued: a longer bound or a retry* -- silence proves no fate, and a retry is a second write.
- *Argued: asking the mode first* -- Neovim can begin waiting between the question and the change.

**Consequences.** A Neovim that never stops waiting keeps the change held and says so; Escape, an
answer or the end of Neovim settles it. The adoption, an open's preparation and its showing are
not held this way: `Host::ask` with a `late` is their hook.

**Laws supported.** [WL-NVIM-13](../workshop/neovim-transfers.md).
