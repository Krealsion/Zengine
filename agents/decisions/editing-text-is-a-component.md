# Editing text is a component

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the law it
supports is in [text-box](../workshop/text-box.md).

**Context.** Two Workshop tools reached the same editing machinery from opposite ends: the
Terminal's command line had text, a caret, character-safe edits, a window and a pointer, and an
Inspector property draft had the text and the edits and nothing else. Every tool that takes a
line of text owes a weaver the same things — a caret that moves by character and by word, a
selection, a window that keeps the caret in view, an undo — and a line that has only some of
them is a lesser line wherever it appears (`2931651`, "Extract the editable line into a TextBox
component and give the Inspector one").

**Decision.** `component::TextBox` owns text, caret, anchor and window as one state with the
operations as its only door. A consumer owns the capacity (an argument), where its prose begins,
the `Clipboard` and its custody, and what the text means — Return, Escape and Tab are never the
component's. `consume()` is the routing bool at the component boundary and declining is
`default:`. The history is the draft's and dies with it. `zengine-component` links nothing. A
word has one definition and three compositions.

**Alternatives considered.**
- *Each tool keeping its own line* — tried, and replaced: four consumers each spelled one six-case
  key mapping (`235141b`), and `erase_one_character`, which erased from the end, went with the
  extraction (`2931651`).
- *A `Button` component for Create and Delete* — not made: what they share is a label, a bit, a
  bracket convention and a row, presentation with no invariant to keep (`5f8fab8`). Labelled
  controls are a strip a pane packs (`component/control_strip.hpp`).
- *A focus flag, a filter, a max length, a multiline mode, a blink* — refused: the pre-Zen
  `Zen::TextBox` in `quarry/` had most of those and could not move its caret.
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
the vocabulary without re-spelling it. What an editable line owes a weaver is built into the box,
whole, rather than left for each consumer to add.

**Since.** This record first argued that a component is extracted only when duplication makes it
the smaller repair. The founder retired that rule, 2026-10-31: a piece is built for the uses the
direction sets, whole and with care. The decision stands on its invariant: one state, one door.

**Laws supported.** [WL-TEXT-01](../workshop/text-box.md), [WL-TEXT-02](../workshop/text-box.md),
[WL-TEXT-06](../workshop/text-box.md), [WL-TEXT-07](../workshop/text-box.md),
[WL-TEXT-11](../workshop/text-box.md), [WL-TEXT-12](../workshop/text-box.md).
