# Three ownership domains

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the law it
supports is in [session](../workshop/session.md).

**Context.** Everything a desk is made of persisted, and nothing read it without being asked:
every session began by pressing `r`, and a 1440x820 Workshop reopened at 952x303 with the saved
desk unread on disk (`fba0dc2`, "Give a maker their desk back when they reopen Workshop"). The
per-user files then followed the launch directory, so a scratch-directory launch was isolated by
accident, and moving the defaults inverted that into accidental danger (`6790547`, the commit
that made Workshop an installed application). The session is a file Workshop writes on its way
out, and an orderly close replaced bytes this run could not read with this run's default desk
(`0cffe92`).

**Decision.** Three domains with three default homes: project files (`--document`, `--setup`,
`--pane`) follow the launch directory; configuration (`--keymap`, `--prefs`) follows the maker;
state (`--session`, `--marks`) follows the machine. The precedence has one spelling — an
explicit path, then `--isolated`, then the per-user default. The legacy transition is one rule
that converges by existence. One representation of a desk, two files: the session nests the
setup's written shape. One door writes the session, on an orderly close only. The restore runs
once per process and answers four things. A session this run could not read is never written over.
Neither direction opens a setup file. A restore returns the desks and the room, not what a maker
was doing.

**Alternatives considered.**
- *An automatic save landing on `--setup`, or a launch that reads it* — refused: it would
  rewrite a maker's named desk every time they closed the window; pinned by case `"WUX-0 F: an
  automatic save never touches the file a maker named"`.
- *A second desk format for the automatic save* — refused: a desk cannot be legal in one file
  and illegal in the other; `setup_in` is one function.
- *Autosave, dirty tracking, a background writer, fsync* — none; crash durability is not
  claimed.
- *One boolean restore answer* — rejected: a startup has four different things to do, and a
  first launch reported as an error is the one way this becomes noise.
- *Restoring selection, focus, the document or the browser's location* — rejected: they are this
  run's; measured on a real screen, a restored layout paints identically except for which pane
  wears the focus ink.
- *Deleting or moving the legacy file after import* — rejected: never deleted, moved or
  rewritten; an existing user-root file always wins; pinned by case `"WUX-3: repeated launches
  converge -- the import can never fire twice"`.
- *Counting unresolved panes in the startup notice* — removed after use: at the instant a
  restored desk is applied no provider has had a turn.

**Consequences.** `--isolated` is the flag every witness harness and executor live run must
carry; an environment with no resolvable root is the same absence, said once. A declined
viewport is not a refusal, so that run keeps its session; a refused one stands as a condition
with a maker action. The document is still not read at launch.

**Laws supported.** [WL-SESSION-01](../workshop/session.md),
[WL-SESSION-02](../workshop/session.md), [WL-SESSION-03](../workshop/session.md),
[WL-SESSION-04](../workshop/session.md), [WL-SESSION-13](../workshop/session.md),
[WL-SESSION-14](../workshop/session.md), [WL-SESSION-15](../workshop/session.md),
[WL-SESSION-16](../workshop/session.md), [WL-SESSION-17](../workshop/session.md).
