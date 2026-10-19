# A lock holds a setting in a layout or everywhere

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the laws it
supports are in [setting-locks](../workshop/setting-locks.md).

**Context.** A weaver keeps some settings fixed while themes, scripts, agents and their own slips
write the rest, and a settings editor or a theme writes many rows across every layout at once. Both
need one owner that judges every write against what it would change.

**Decision.** Workshop holds the locks with the rows: a layout's lock freezes that layout's row; an
everywhere lock on a pane pins the value its layout holds into every row that seats the pane, and
into each row seated later; an office-wide setting's lock holds everywhere. Every write door refuses
a locked key, a batch passes it over and says so, and only the weaver's hand unlocks: the Terminal,
or a pane naming the weaver's own current act. The locks, each layout's pin lists and its baseline
theme stay in Workshop's settings file, keyed by the layout's name, so neither the setup file nor
the session changes version. Every layout is read at one revision and written in one batch judged
whole against it.

**Alternatives considered.**
- *Argued: per-layout locks and baselines in the setup file, a new version of it and the session*
  -- deferred: every setup and session reader moves for fields the settings file already holds.
- *Argued: a lock as a guest power* -- refused: unlocking is the weaver's, and no grant says it.
- *Argued: a batch written as far as it gets* -- refused: half a theme is a desk no one asked for.

**Consequences.** Layouts sharing a name share what is kept under it; a rename carries it, and a
removal takes it unless another layout still holds the name. A loaded weave holds `allow_any`, so a
lock restrains guests, the Terminal and honest writers, not code in Workshop's process.

**Laws supported.** [WL-LOCK-01](../workshop/setting-locks.md),
[WL-LOCK-02](../workshop/setting-locks.md), [WL-LOCK-03](../workshop/setting-locks.md),
[WL-LOCK-04](../workshop/setting-locks.md).
