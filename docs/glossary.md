# Glossary

Zengine's own words, each said once. Loom's words are in
[Loom's terminology](https://github.com/Krealsion/Loom/blob/main/docs/terminology.md), which this
page links and does not repeat; a word Loom's page holds is the proper name here too.

A page explains such a word where a reader first meets it, or links here.

## Loom's words

Loom's glossary owns *weave*, *role*, *incarnation*, *grant*, *ask*, *answer*, *correlation*,
*`zen.Activated`*, *sealed candidate*, *coordinator*, *prepared replacement*, *claim* (a Sense's
latest claim, which a weave's `loom::host::WaitSet` is) and *the letter* (`PrepareShutdown`, then
`Bequest` and `ClaimBequest`). This page links them rather than say them again.

## Zengine's words

- **Host.** The program that runs Loom and the weaves in it: Workshop, the app, the snake example,
  `loom-host`.
- **Workshop.** Zengine's host for makers, where every feature is mounted.
- **Load plan.** The file that says which weaves a host loads, and as which roles.
- **Manual.** A weave's page for makers, compiled into the weave, which answers a section of it to
  anyone who asks.
- **Operator.** A named function in Zengine's catalog (`operator/`), native or composite, which Flow
  composes into weaves and a weave can run by name, such as `timer.normalize_delay`. It is where
  Zengine keeps a rule too small to be a weave of its own and shared by many. One weave supplies it
  and one evaluator runs it, so every user of the rule gets the same answer.
- **The host's one wait.** Loom's wait (`loom::host::Reactor`), where a host rests between turns of
  its bus. It wakes each weave that claimed something on it, when that thing is ready or its time
  comes, and sends it the time in a `loom::host::Waited` message.
- **Wake.** One `Waited` the host's wait sends a weave because what it claimed is ready or due.
- **Source.** A weave that reads the outside world, such as a window, a console or a child's pipes,
  and waits on it through the host's one wait (`timer/source_wait.hpp`).
- **The Timer.** The weave holding `zengine.timer`, which fires timers for any weave that asks.
- **Steward.** Loom's keeper of bequests, held by its manager role, which a new incarnation asks
  for the letter left for it.

## Words the code uses in a sense of its own

A word the code uses in a sense of its own has a proper name here, or gives way to plain words for
what happens. The code's identifiers keep their names until a change of their own renames them.

- **Input actor.** One source of presses and keys at a window: the weaver's own, which the
  platform gives, or a guest's, which the Input role names. The code calls it `InputActor`, and
  also `Hand` (`AreaHandRefused`, "the weaver's hand"). It is not the actor of the actor model,
  which every weave is.
- **"Hand", in its other senses**, takes plain words: the host's wait sends a weave the time, in a
  `Waited` message; an area is given to another owner; a function is passed a rectangle.
- **"Room"** has no proper name. Each use says what it means: the area a window gives a pane to
  draw in (`graphics::AreaRoom`, `pane::RowsRoom`, `pane::CanvasRoom`); the bounds a layout is
  solved in (`ui::Room`); the part of Workshop's surface no pane covers; or plain space.
- **"Says", of a weave**, is written "sends a message".
