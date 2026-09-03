# A component is earned

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the law it
supports is in [text-box](../workshop/text-box.md).

**Context.** Two Workshop tools reached the same editing machinery from opposite ends: the
Terminal's command line had text, a caret, character-safe edits, a window and a pointer, and an
Inspector property draft had the text and the edits and nothing else. The first trace declined
to extract, correctly: a TextBox then would have renamed `TerminalInput` and deleted nothing.
The day the draft needed the caret, the window and the pointer arithmetic was the day extracting
was the smaller repair (`2931651`, "Extract the editable line into a TextBox component and give
the Inspector one"). With four consumers, each spelling one six-case key mapping, the ordinary
expectations of an editable line had stopped being per-consumer projects (`235141b`).

**Decision.** `component::TextBox` owns text, caret, anchor and window as one state with the
operations as its only door. A consumer owns the capacity (an argument), where its prose begins,
the `Clipboard` and its custody, and what the text means — Return, Escape and Tab are never the
component's. `consume()` is the routing bool at the component boundary and declining is
`default:`. The history is the draft's and dies with it. `zengine-component` links nothing. A
word has one definition and three compositions. `component::Button` is not extracted.

**Alternatives considered.**
- *Extracting at the first duplicate* — declined then, done when it deleted something
  (`2931651`; `erase_one_character`, which erased from the end, went with it).
- *A `Button` component for Create and Delete* — not extracted: what they share is a label, a
  bit, a bracket convention and a row, presentation with no invariant to keep, and consumer #2
  cost four lines (`5f8fab8`).
- *A focus flag, a filter, a max length, a multiline mode, a blink* — refused: the pre-Zen
  `Zen::TextBox` in `reference/` had most of those and could not move its caret.
- *The box learning application chords to refuse* — rejected: declining is `default:`, and a
  chord carrying Alt or Super is never the box's; pinned by case `"component: consume owns
  exactly the editing vocabulary and declines the rest"`.
- *An application-wide undo* — refused; `set`/`clear` wipe the history.
- *Linking `loom::core`* — rejected: the absence of the link is the enforcement that a component
  is not content; the key identities are spelled locally and pinned against the input wire.
- *Three word implementations kept in agreement* — replaced by one scan composed three ways
  (`1bc34e6`).

**Consequences.** A consumed gesture need not change anything — copy with nothing selected is
consumed. `kEditingVocabulary` is swept against `consume` both ways, so a help surface can show
the vocabulary without re-spelling it. A fifth absence a competent user trips on gets the same
test, not a reflex extraction.

**Laws supported.** [WL-CTRL-05](../workshop/info-controls.md),
[WL-TEXT-01](../workshop/text-box.md), [WL-TEXT-02](../workshop/text-box.md),
[WL-TEXT-06](../workshop/text-box.md), [WL-TEXT-07](../workshop/text-box.md),
[WL-TEXT-11](../workshop/text-box.md), [WL-TEXT-12](../workshop/text-box.md).
