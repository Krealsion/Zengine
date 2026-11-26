# The Terminal's laws (WL-TERM)

**Laws.** The numbered rules the Terminal pane keeps, each a rule that more than one file relies
on; a comment or a page that relies on one names it by its number. A rule about one declaration is
in that declaration's comment, not here. The pane's messages and keys are in [its
manual](zengine.terminal.md). The participant the pane presents, the door onto it and its
completer are Workshop's, and their laws are Workshop's too. The files and cases that prove each
law here are listed in [`tests/laws/terminal.txt`](../../tests/laws/terminal.txt), which the
`law_register` check reads.

## WL-TERM-01 — The Terminal is a pane, and nothing global reaches it

The Terminal is a loaded weave offered by the role `zengine.terminal`, arranged like any other
pane. No key, chord or contextual row of Workshop's own opens it or acts on it.

What this means:

- A weaver opens it from the Pane Manager, and reaches its keys by pressing into it.
- Its five action ids and their gestures keep the spellings weavers' overrides name, so an
  override still reaches them.
- It wears a pane's boundary, so what it covers it covers legibly.

What it does not mean: that the pane holds the participant. Workshop mounts the participant and
holds the pointer that reaches it (WL-TERM-02).

Why: [the Terminal is a participant](../../agents/decisions/the-terminal-is-a-participant.md).

## WL-TERM-08 — The image that presents a participant cannot reach one

The pane's image, `zengine-terminal-pane`, includes no header that declares `loom::TerminalSession`
or its transcript, links neither Workshop's logic nor Loom's terminal, and keeps no copy of the
record.

What it does not mean: that the pane stands with nothing of the participant's beside it. The
door onto the participant, its completer and the picture of the record are Workshop's
(`workshop/terminal/`), and the pane includes none of them.

Why: [the Terminal is a participant](../../agents/decisions/the-terminal-is-a-participant.md).

## WL-TERM-11 — A completion answer applies to the line and caret it was asked about

An ask records the intent, the line and the caret it is about. An answer that comes back to
another is neither shown nor accepted, and the list is asked for again only when the intent
standing then wants one.

What this means:

- The correlation says which question an answer is to, never that the question still stands.
- An edit, a caret move, a dismissal, a clear, a submit, a recall and its lock each end one.
- The list's selection still survives a recomputation of the same question (WL-TERM-05).

What it does not mean: that a weaver never sees the gap. The ask and its answer are one turn of
Workshop's drain; what a medium draws inside that turn is another question, and is not measured.

Why: [the Terminal is a participant](../../agents/decisions/the-terminal-is-a-participant.md).
