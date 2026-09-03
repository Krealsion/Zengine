# A presentation owns no facts

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the laws it
supports are in [project](../workshop/project.md) and
[panes-and-windows](../workshop/panes-and-windows.md).

**Context.** The Builder panel presents a tool this application does not own, and the first live
run produced a lie without a rule about that: reopening the panel asked the tool, the tool
answered with its last outcome, and the screen announced `built zengine-snake -- exit 0` about a
build that had finished a minute earlier, because the arrival of a success is not the same event
as a build succeeding. Async builds then gave a build a middle, and a panel opened while a child
was alive learned `running` without having watched it begin. The same shape waited at the
external panes, one provider further out: a room granted and answered by nothing valid, where
saying `unavailable` would assert a fact about the provider that a sender's silence does not
prove.

**Decision.** A panel may report what it watched happen; what it merely learned belongs in its
rows. `awaiting` records "I asked and have not been answered" and is released only at an outcome
the build will not leave; `awaiting_realization` is its twin, held longer, so a build's ending and
the realization of what it produced are two announcements. The tool's status is kept only while a
panel presents it: closing forgets the copy and reaches no tool, and reopening asks again. The
only build Workshop can name is one the tool told it about, by the tool's name, and the realize
intention travels in the same sentence; the frontier gesture performs the one string comparison
the maker used to make across two panes and refuses to choose among several producers. A pane
with a room and no answer says waiting, a fact about this panel, and never unavailable.

**Alternatives considered.**
- *Tried: announcing every arriving status* — the first live run's lie, corrected; pinned by case
  `"a panel opened mid-build is TOLD it is running, and announces nothing"`.
- *Tried: reading `chosen`'s default of 0 as a choice, or spending the first producing row* — the
  sharp case plants the matching row first and goes red on either spelling; case `"BLD-2: several
  recipes produce the frontier -- `f` never chooses for the maker"`.
- *Argued: keeping the tool's status against a panel opened later* — refused: that is how a
  presentation quietly becomes a second owner of somebody else's facts; pinned by case
  `"closing forgets the panel's copy; the TOOL keeps its own count"`.
- *Argued: saying `unavailable` for a silent provider* — refused: Loom gives Workshop no
  participant-visible unload notification, so silence proves no fate; pinned by case `"silence is
  waiting, and Workshop never says unavailable"`.

**Consequences.** Which recipe catalog the session uses lives on the `Session` beside the source
document and not on the panel, because the panel is destroyed and remade by `close_panel`.
Everything after a build request belongs to owners that are not here: the tool refuses or
orders, the runner runs, the tool offers, and the realization owner decides in its own words;
there is no second build path, no direct load and no new sentence on the bus. A build's ending
and a realization's answer are two rows and two notices.

**Laws supported.** [WL-PANE-16](../workshop/panes-and-windows.md),
[WL-PROJ-11](../workshop/project.md), [WL-PROJ-12](../workshop/project.md),
[WL-PROJ-13](../workshop/project.md), [WL-PROJ-14](../workshop/project.md).
