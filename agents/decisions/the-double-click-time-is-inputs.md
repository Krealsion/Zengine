# The double-click time is input's

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the law it
supports is in [pointer](../workshop/pointer.md).

**Context.** Two readers timed a double click themselves: Workshop's tab run and Double Shift
against a constant of Workshop's, and Flow's pane against a steady clock it read as each press
arrived, on an interval a case kept equal to Workshop's. A press carried neither a time nor a
count, so a reader that was slow to hear the second press -- a sanitizer's run, a busy machine --
saw two presses where the weaver made one double click.

**Decision.** The time is a role-wide setting of `zengine.input`, `input.double-click-ms`
(200 to 2000, 400 by default), which the input role says to whoever asks and publishes when it
changes (`PointerTimingSaid`). The window counts each hand's presses by it on the medium's own
clock, as it reads them, and says the count with each press (`AreaButtonChanged::clicks`,
`AreaPressed::clicks`): one more than the press of that button before it when within the time,
else 1, so 2 for a double click's second press. A reader takes a count of 2 or more with its own
place rule -- the tab the last press armed, Flow's target and slack -- and never times a press. An
agent's input is real input from its own hand: a tool sends the actual clicks through its input
session, and the window counts them by the same rule, for that hand, so an agent's click and the
weaver's never make one double click. Double Shift is keys, which the window does not count; Workshop times Shift's
taps by the time the input role says.

**Alternatives considered.**
- *Tried: a reader's own constant, kept equal by a case* — Flow's case went red when its two
  presses landed more than the interval apart under the sanitizer, and the equality case pinned
  two copies of one number rather than one owner.
- *Argued: a timestamp on the press, each reader judging* — every reader would carry the
  interval and the same arithmetic.
- *Argued: an agent's press saying its own count* — it would reach a pane by a road the weaver's
  input never takes, where an agent's input goes through what the weaver's does.
- *Argued: the platform's own count* — SDL counts by the desktop's time and a console reports a
  double click of its own, so the two media would disagree, and a terminal reports none.
- *Argued: the time read as a sense* — reading another role's claim takes an observe rule each
  host grants, where a published word reaches every window the host already loads.

**Consequences.** One gesture means one thing in every medium and at any runner's speed: a case
states its windows' clock (`InteractionClock`, the suites' devices). The
count is the window's, so a press the window gave nobody still counts toward the next.

**Laws supported.** [WL-PTR-01](../workshop/pointer.md).
