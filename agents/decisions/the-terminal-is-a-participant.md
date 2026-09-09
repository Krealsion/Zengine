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
- *Trusting the correlation alone for a completion answer* — replaced: it identifies the
  question and says nothing about whether it still stands, and three paths ended one without
  saying so; pinned by case `"TERM-W20: a completion answer about a line that is gone is
  neither shown nor taken"`.
- *Cancelling an outstanding completion in each early return instead of measuring the answer
  against the line* — refused: it repairs the paths that exist and not the property, and it
  leaves the coalescing window (an answer for a line the maker has typed past) still usable.
- *Keeping a superseded list on screen until its replacement arrives* — refused: a list under a
  line it is not about is a wrong answer whether or not anybody presses Tab. The cost is a
  publication with no list, delivered to the Skin in the same turn as its replacement; whether
  a medium draws both is that medium's business and is not measured here.
- *Cancelling a paste on any keystroke* — refused: an edit is the same draft, and `set`/`clear`
  are the two doors that end one; pinned by case `"TERM-W21b: an edit is not a new draft, and a
  submit is"`.

**What "could not migrate" is a claim about.** The measurement is exact and its scope is
narrower than the sentence it is often shortened to. `loom::TerminalSession::handle` sends
nothing by construction and `accepted_schemas()` returns the three answer doors its host
declared, so **no message in the interface this participant has today drives it**, and under
this work's constraint — the Loom is fenced and no substrate sentence is added — it therefore
could not have moved. That is a fact about the current message interface plus a scope rule; it
is not a claim that no participant of this kind could ever migrate. A Loom that gave the
session a driven door, or a host that wrapped it in a weave that owns the channel, would change
the answer, and either is a Loom conversation rather than a Workshop one. Say the bound with
the claim wherever it is repeated.

**Draft lifetime is not settled by calling one thing a composition.** The reload keeps the
terminal line and drops Info's property draft and the Builder's role line, and the argument
first written for that — a composition is not a write in flight — names a real difference but
does not by itself carry the conclusion. Four questions are worth asking of any draft. **Two of
them separate these drafts and two do not**, and the two that do not were first written down
here as though they did; the correction is kept in view because a rule that only ever agrees
with the decision it was written for is not a rule.

| question | terminal line | property draft | separates? |
|---|---|---|---|
| *recoverability* — what do the maker's edits survive as? | nothing; the text was only here | nothing either. Reopening the property gives its COMMITTED value back, never the uncommitted edits | **no** — what differs is how far back a maker lands, not whether their work survives |
| *user effort* | minutes: address, shape, version, named arguments, assembled with the completer | usually one value, typed in one go | **yes**, by degree |
| *target identity* — does it name something that can change under it? | yes. `#12` and `@office` are targets, resolved at SUBMIT | yes: an object and a label, resolved at commit | **no** — both name targets |
| *replacement semantics* — what does keeping it risk? | nothing until an explicit submit: inert text on a row the maker reads | a commit writes into a live document | **yes**, and it carries the decision |

**What the terminal line's execution-time behaviour actually guarantees**, read rather than
assumed: `submit_terminal_line` resolves the address at submit and, when the send or ask does
not succeed, records the outcome as a notice on the participant's own transcript — so a stale
`#12` fails where the maker is looking. It does **not** guarantee the address still means what
it meant when the line was typed: a role resolves to whoever holds it then, so a kept line can
reach a successor. That is a real consequence of keeping the draft and is named rather than
argued away. Preserving inert text is a different act from executing it, and a third from
validating what it points at.

**The open question is left open**: nothing here decides what a draft with a MIXED answer
does — high effort and a commit that writes, or low effort and inert — and the first one that
appears is the phase that decides it, not this record.

**A refusal is a row of the budget.** The pane spends its rows input-line first, and the door's
refusal is the answer to the gesture the maker just made — so it is budgeted with the rows and
not added after they are composed. A row added afterwards has to take one back, and the row it
takes back is the last one composed, which is the input line: the sentence appeared and the
line it was about vanished, with the caret (WL-TERM-10, found in review). In one row of room
there is no row for a notice that is not the maker's own line, and the line keeps it.

**An answer applies to the line it was asked about.** Three operations cross the new seam as
requests — the act, the completion and the paste — and correlation tells this pane which
question an answer is to, never that the question still stands. A completion request records
the line and the caret it asks about (WL-TERM-11); a paste records the draft that asked
(WL-TEXT-09, the check Workshop used to make for this line and the pane now makes for itself).
Both were found in review, and both are the same finding: what used to be a function call is a
round trip now, and a maker keeps typing across it.

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
[WL-TERM-09](../workshop/terminal.md), [WL-TERM-10](../workshop/terminal.md),
[WL-TERM-11](../workshop/terminal.md).
