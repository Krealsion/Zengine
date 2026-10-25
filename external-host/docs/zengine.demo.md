# zengine.demo -- the Demo pane

**Manual.** The Demo pane a ready-to-use setup shows, with its Reset demo button, and the office the
setup's demo service attaches to. It is a *weave*: a piece of Workshop that is loaded, replaced and
spoken to by messages, built as `zengine-demo-control`. Other weaves reach it by its *office*, the
name it answers to, `zengine.demo`, and it offers one pane, `controls`. `demo.py` puts it in a
setup's plan; [ready-to-use setups](../../workshop/docs/demo-setups.md) says how to start one.

## About

A setup's *demo service* is a run on the other Loom host that knows how to prepare the setup: the
`workshop` package's `workshop/demo-serve`. It attaches here, and waits for work. Each piece of work
is a *generation*, numbered from 1: the preparation the service does when it attaches, then one per
reset. When the work ends, the service says whether it passed, with a note, and the pane shows it.

The pane shows the setup's name, the `[ Reset demo ]` button and where the demo stands: `unavailable`
(no service is attached), `working`, `ready` or `failed`, with its note. It keeps nothing of the
setup itself: what a reset does is the service's.

A reload in place keeps the name, where the demo stood and its generation, but no service: the one
attached before belongs to the image that was replaced, and its waits and its work end with it.
The demo stands `unavailable: The demo's office was reloaded; start the demo's ELH service again`,
the old service's next word is refused, and a service started again attaches, its work numbered
after the last generation. A program waiting on the replaced image is never answered.

## Use

Press `[ Reset demo ]` to prepare the setup again, and wait for `ready`. Its line in the pane's menu
is `reset the demo`. A program resets with [`DemoResetRequested`](#demoresetrequested), and waits
for a generation with [`DemoReadyRequested`](#demoreadyrequested); a guest needs its row's `demo`
power for either.

## Commands

### `reset`

Resets the demo: the next generation's work goes to the service, which prepares the setup again.

- **Takes:** the attached service, idle. No default key; the `[ Reset demo ]` row, the part
  `control:reset`; the menu line `reset the demo`.
- **Answers:** answers the attached service's wait with [`DemoWork`](#demoworkrequested); the status
  row reads `working: Resetting demo`, then the service's outcome, `ready: <its note>` or `failed:
  <its note>`.
- **Refuses:** with no service attached, or while one is working, the status row's note reads `Reset
  service is busy or unavailable`, and nothing else changes.
- **Example:** press `[ Reset demo ]`, then wait for the status row to read `ready`.

## Hears

### `DemoServiceOpened`

`DemoServiceOpened` v1 `{name}`, from a demo service: it attaches, names the setup, and its first
work starts, `working: Preparing demo`; answered `zen.Ack`. Refused `a demo service is already
attached, or its name is invalid`: a second service, or a name empty or too long.

### `DemoWorkRequested`

`DemoWorkRequested` v1, from the attached service: wait for work. Answered `DemoWork` `{generation}`
when there is work, at once or when a reset asks for it. Refused `only the idle attached service may
wait for demo work`.

### `DemoWorkFinished`

`DemoWorkFinished` v1 `{generation, passed, note}`, from the attached service: that generation's work
ended. The demo stands `ready` or `failed` with the note, a reset waiting on it is answered, and the
service is answered `zen.Ack`. Refused `completion does not name the current demo work`.

### `DemoResetRequested`

`DemoResetRequested` v1, from anyone granted it: reset, as [`reset`](#reset) does, answered when the
work ends, `zen.Ack` when it passed and `zen.Refused` with its note when it failed, or `demo service
closed during reset`. Refused at once `demo reset service is busy or unavailable`.

### `DemoStatusRequested`

`DemoStatusRequested` v1: answered `DemoStatus` `{name, state, note, generation}` at once.

### `DemoReadyRequested`

`DemoReadyRequested` v1 `{generation}`: answered `DemoStatus` once the demo is `ready` or `failed` at
that generation or a later one, at once when it already is. Refused `invalid ready generation or too
many waiters`; a waiting ask is refused `Demo service stopped` when the service detaches.

### `DemoServiceClosed`

`DemoServiceClosed` v1, from the attached service: it detaches, `unavailable: Demo service stopped`;
its waiting ask is refused `demo service closed`, and a reset waiting on it `demo service closed
during reset`. Answered `zen.Ack`; refused `not the attached demo service`.

### `PaneCatalogRequested`

`PaneCatalogRequested` v1, from Workshop: the pane is offered again, with its one action.
Shape: [`workshop/pane_vocabulary.hpp`](../../workshop/pane_vocabulary.hpp).

### `PaneActionRequested`

`PaneActionRequested` v1, from Workshop: the action [`reset`](#reset), as the menu line asks it.
From anyone else it does nothing. Shape: [`workshop/pane_vocabulary.hpp`](../../workshop/pane_vocabulary.hpp).

### `PaneDocumentRequested`

`PaneDocumentRequested` v1: answered `PaneDocumentShown` from this page, to the asker alone.
Shape: [`workshop/pane_document.hpp`](../../workshop/pane_document.hpp).

### `AreaRoom`

`AreaRoom` v4, from the graphics window that shows the area `zengine.demo`, as that window's office:
the pane's room there and the window's text. In a new window, incarnation, size or measure, or an
empty area, the pane draws the lines the room holds there whole (`Drawn`), a line of that text
each, the reset button the part `control:reset`, then only what changed. Shape:
[`graphics/vocabulary.hpp`](../../graphics/vocabulary.hpp).

### `DrawnRefused`

`DrawnRefused` v1, from that window: a drawing it refused, and why. What it shows stays as it was.
A refused change is followed at once by the drawing last sent, whole; after a refused whole drawing,
nothing is sent until the pane's lines change, and that drawing goes whole.
Shape: [`graphics/vocabulary.hpp`](../../graphics/vocabulary.hpp).

### `ActionRequested`

`ActionRequested` v1, from that window, where it shows the pane's area deaf to the pointer: a press
on the reset button there asks `reset`, which is [`reset`](#reset). An action the pane's last
drawing there does not name does nothing.
Shape: [`graphics/vocabulary.hpp`](../../graphics/vocabulary.hpp).

### `AreaButtonChanged`

`AreaButtonChanged` v3, from that window: a button pressed in the pane's area, at its point, with
what the window drew there. The first button on the reset button asks `reset`, which is
[`reset`](#reset); the third is handed back, so Workshop's own pane menu opens there. One the window
landed on another control than the drawing shows there now is refused in words. A release, or a
press on nothing the drawing names, does nothing.
Shape: [`graphics/vocabulary.hpp`](../../graphics/vocabulary.hpp).

### `AreaPointerMoved`

`AreaPointerMoved` v2, from that window: the pointer moved in the pane's area. It does nothing.
Shape: [`graphics/vocabulary.hpp`](../../graphics/vocabulary.hpp).

### `AreaWheelTurned`

`AreaWheelTurned` v2, from that window: the wheel over the pane's area. It does nothing.
Shape: [`graphics/vocabulary.hpp`](../../graphics/vocabulary.hpp).

## Follow

The weave publishes nothing: the pane's rows are its one account. A program follows the demo by
asking: `DemoStatusRequested` for where it stands, or `DemoReadyRequested` held for the generation it
waits on, the next one after a reset.

## Shows

### `name`

The setup's name, as its service attached with it.

### `status`

Where the demo stands and its note, `ready: <the service's note>`; `unavailable: Start the demo's ELH
service` before a service attaches.

## See also

- [Ready-to-use setups](../../workshop/docs/demo-setups.md), where the Demo pane is met.
- [Drive Workshop from another host](external-host.md): the guests file, whose `demo` power reaches
  this office.
- The shapes: [`external-host/demo-control/vocabulary.hpp`](../demo-control/vocabulary.hpp).
