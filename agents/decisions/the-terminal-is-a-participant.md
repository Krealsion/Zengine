# The Terminal is a participant

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the law it
supports is in [terminal](../workshop/terminal.md).

**Context.** Workshop needed a terminal, and the risks were named before the pane existed: a
second Loom, a privileged Workshop protocol, a second command grammar, a weave mounted merely to
separate a pane, a focus framework (`5494e17`, "Present an ordinary TerminalSession through
Workshop"). What followed was measured in live pictures: a transcript row with no entry was not
written, so the backdrop's `.` showed through the pane (`982f9b9`, "Draw the overlay as a pane
rather than as a set of lines"); the vocabulary the host handed the participant was real, exact
and invisible (`16e6abc`, "Let the Terminal show what it knows"); a 5x5 letterform scaled into a
12-pixel cell read `weave` as `woave` (`b92079f`, "Let one region of the picture be finer than a
cell"); and the command line had no caret a press could place (`b30ab5d`).

**Decision.** The host mounts one `loom::TerminalSession` on the Switchboard it already has and
hands Workshop the non-owning pointer through the HostContext. `workshop.terminal` toggles a
modal overlay: while it is open the keys and the pointer belong to it, and closing it restores
every gesture. The pane presents an ordinary participant on Workshop's own bus, parsing with
Loom's own grammar. It is one bounded region placed in cells that fits entries and says what it
omits in the two senses that differ. The completer reads the line's slot and offers only what the
submitter runs; browsing candidates authors nothing; the completion list is a bounded region
inside the pane, never over the input line. Wrapping is a presentation act. A fresh skin clears
nothing. A press in the pane is the pane's.

**Alternatives considered.**
- *A second Loom, a privileged protocol, a second grammar, a separating weave, a focus
  framework* — refused: "while the terminal is open, the terminal has the input" is one sentence
  and one branch (`5494e17`); the two identities stay two, measured rather than asserted.
- *Painting from a live transcript* — rejected: snapshots taken inside a handler, so a canvas
  cannot read a participant the host has ended; pinned by case `"the pane's snapshot outlives
  the participant it came from"`.
- *Skipping empty transcript rows* — measured wrong in the first live rasterization; every row
  is written and the case rasterizes through the terminal medium's own pure function (`982f9b9`).
- *Offering address values, or verbs the submitter does not run* — refused: `#` and `@` are
  offered as forms with the reason beside them, because knowing a shape is not authority to send
  one; pinned by case `"an address offers the three forms and never pretends to know the
  values"`.
- *Shift+Space as the toggle* — retired: a POSIX terminal reports no modifier for Space at all
  (`7b64b73`).
- *Fitting lines rather than entries* — rejected; pinned by case `"a pane fits ENTRIES, not
  lines, and says what it could not show"`.
- *Clearing the presentation context on a fresh skin's hello* — measured, not blessed; pinned by
  case `"a fresh skin's hello does NOT clear the presentation context -- measured, not
  blessed"`.

**Consequences.** The participant's rule is narrower than Workshop's in the same shape: it may
say `SurfaceText` only to whoever holds the skin's office. Measured on the minimum window when
the region gained real type: 83 columns where the cell grid held 56, 8 rows where it held 13, and
a plan sixteen times cheaper. `SurfaceTextRow` gained `background` for the selected row. Opening
the pane mid-drag no longer strands a gesture; clicking a completion row selects it and Tab
accepts; Tab, Up and Down are unbound in this mode.

Wrapping earned its place by one measurement: the pane's own syntax notice is 111 characters and
the pane was 56 columns wide, so a maker who asked how to send a message read the first 53
characters and `...` -- the truncation was in the fitting, not in the room (`detail::wrap`,
`terminal_wrapped`).

`kCompletionMinRows` is one, measured rather than reasoned: with a floor of two, `send * s` showed
nothing at all, because no shape begins with a lowercase `s` and a heading with no candidate rows
was refused for being one row tall -- so the one sentence that tells a maker the vocabulary lacks
what they reached for never appeared.

**Laws supported.** [WL-TERM-01](../workshop/terminal.md), [WL-TERM-02](../workshop/terminal.md),
[WL-TERM-03](../workshop/terminal.md), [WL-TERM-04](../workshop/terminal.md),
[WL-TERM-05](../workshop/terminal.md), [WL-TERM-06](../workshop/terminal.md),
[WL-TERM-07](../workshop/terminal.md), [WL-TERM-08](../workshop/terminal.md),
[WL-TERM-09](../workshop/terminal.md).
