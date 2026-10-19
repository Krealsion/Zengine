# zengine.input -- Input

**Manual.** Input: the weave that turns the keyboard and the pointer into messages, answering to the
office `zengine.input` (an office is the name a weave answers to). Two weaves hold it, one in any
run: `zengine-input` reads the terminal, or the console on Windows, and `zengine-input-sdl` reads an
SDL window. Workshop's terminal plan loads the first and its graphical plan the second
([the shipped plans](../../workshop/docs/load-plans.md#the-three-shipped-plans)); a program of
your own loads one in that office. Your weave hears a person by accepting what it publishes; an
agent or a test speaks for one through an input session, by the messages below.

## About

Input reports *moments*, and your weave interprets *gestures*. A moment carries what the platform
knew when one thing happened: a key went down or came up, with the modifiers held at that instant;
text arrived as the keyboard layout typed it; the pointer moved, a button changed or the wheel
turned, each with where the pointer was. A drag or a selection is yours to make of them. What each
shape carries, and what each platform can vouch for, is [the Input package](input.md).

The weave is the only code reading the platform's input, and it never writes to the screen. Where
the two weaves differ:

- **`zengine-input`** says pointer positions in terminal cells. On a POSIX terminal it reads its
  standard input in raw mode, and says a key's release right after its press, since a terminal
  reports whole keystrokes; pointer reports arrive only once the terminal Skin has turned them on.
  On Windows it reads the console's records, releases included. With no terminal (a pipe, a test
  run) it reads nothing, and says nothing. When a terminal window
  (`zengine.window.terminal`) holds the process's terminal in the same run, it leaves the
  terminal's input to the window and serves input sessions alone.
- **`zengine-input-sdl`** says pointer positions in window pixels, and a pointer moment carries no
  modifiers. It reads SDL's event queue, beside the window Skin, and the window's close box reaches
  you as `SurfaceCloseRequested`, never as a key. It reads none of the queue an OS window holds.
  With no display it reads nothing, and says so once on standard error:
  `zengine-input-sdl: SDL_Init(SDL_INIT_VIDEO) failed: <SDL's reason>`.

A reload in place keeps the open session, which its holder goes on using and whose held keys are
released when it closes; the last session number, so none is given twice; and the counters a poke
reads, `pumped` (polls) and `emitted` (moments the platform gave). A timed pointer move in flight is
not kept: its asker is never answered. An unload ends the session without releasing what it held.

## Use

To hear a person, accept the shapes under [Follow](#follow): `#include "input/vocabulary.hpp"` and
link `zengine::input`. There is nothing to ask for and nothing to poll.

To speak for a person -- an agent, a test, a probe -- open a session with
[`InputSessionRequested`](#inputsessionrequested), hand it moments with
[`InjectInput`](#injectinput) or a timed pointer move with
[`PointerMotionRequested`](#pointermotionrequested), and close it with
[`InputSessionClosed`](#inputsessionclosed). Its moments are published as the same shapes, in
order, from the same weave. A session is not focus and not a lease: a person at the keyboard is
still heard, the two interleaved as they arrive, and nothing expires. To tell the sources apart,
accept `AttributedInput` and believe it only when Loom says `zengine.input` authored it
(`mail.authored_from_role("zengine.input")`).

Who may send: any weave its host grants the shapes. In Workshop, a guest whose row has the `input`
power may send the four session shapes, and Workshop closes a gone guest's session on its behalf.

The weave keeps its own beat. When it is activated, and whenever the Timer says it is ready, it asks
the Timer for a repeating beat, `zengine.input.pump`, every `kPumpBeatMs` milliseconds
(`EnsureRoleTimer`, addressed to its office, so a successor inherits it), and each beat polls the
platform once ([the Timer binding](../../timer/docs/timer-binding.md)).

The OS window ([`zengine.window.sdl`](../../graphics/docs/zengine.window.sdl.md)) reads SDL's queue
itself, and while it is loaded it claims the queue (`ZENGINE_WINDOW_SDL_INPUT_PID` in the process's
environment, `input/window_claim.hpp`): `zengine-input-sdl` beside it reads none of the queue and
serves input sessions alone, as `zengine-input` does beside a terminal window.

## Commands

### `PumpInput`

`PumpInput` v1 gives the weave its hands once, as a beat does: it moves a timed motion on, then
polls the platform once and publishes what came.

- **Takes:** nothing.
- **Answers:** nothing is answered; what the platform had is published ([Follow](#follow)).
- **Refuses:** nothing.
- **Example:** a suite, or a host with no Timer, sends `PumpInput{}` to `zengine.input`.

### `InputSessionRequested`

`InputSessionRequested` v1 opens the one input session, held by you: the sender the bus names, never
a name in the message.

- **Takes:** `purpose`, one line for a person reading what is open.
- **Answers:** `InputSessionOpened` v1, to you alone: `session`, the number you name it by, counted
  from 1 by this weave.
- **Refuses:** in `zen.Refused`, while a session is open: `busy: input session 1 is held by you; it
  must be closed before another opens`, or `busy: input session 1 is held by another participant;
  it must be closed before another opens`.
- **Example:** `mail.send_to_role("zengine.input", zengine::input::InputSessionRequested{"workshop probe"});`,
  as the Workshop probe opens its session.

### `InputSessionClosed`

`InputSessionClosed` v1 closes the session: by its holder, or by an office speaking on the holder's
behalf.

- **Takes:** `session`; `holder`, the holder's weave id, read only when an office closes it. An office
  may say `session` 0 for whatever that holder holds.
- **Answers:** first every key and button the session still holds comes up, published as ordinary
  `KeyReleased` and `PointerButton` moments at its last pointer position, still the holder's; a
  timed motion still running is refused to its asker, `pointer motion cancelled: session closed`;
  then `zen.Ack`. The releases may finish a gesture half made; nothing is undone.
- **Refuses:** in `zen.Refused`: `no open input session numbered 3`; to anyone else, even one naming
  the holder, `input session 1 is closed by its holder, or by an office closing it on the holder's
  behalf`.
- **Example:** when a guest's connection is gone, Workshop's guest door sends
  `{session: 0, holder: <the guest's id>}` as `zengine.guests`.

### `InjectInput`

`InjectInput` v1 publishes moments you composed, in the order given, through your session.

- **Takes:** `session`; `events`, each an `InjectedEvent` v1 whose `kind` names the shape it
  becomes -- `KeyPressed`, `KeyReleased`, `TextEntered`, `PointerMoved`, `PointerButton` or
  `PointerWheel` -- and whose fields for that kind are read, the rest ignored. A key names a
  scancode in 1..`kMaxScancode`, a button 1, 2 or 3, a pointer moment its space, cells or pixels.
  A batch holds at most `kMaxInjectedEvents` moments, and a session at most `kMaxHeldKeys` keys
  down; a press of a key already down is a repeat and holds nothing new.
- **Answers:** `InputInjected` v1, to you alone: `admitted`, how many were published, and
  `first_seq` and `last_seq`, their numbers in the session's own count. Published is not
  processed: what a consumer makes of a moment is its own, later.
- **Refuses:** in `zen.Refused`, publishing nothing: `no open input session numbered 2`; `input
  session 1 is another participant's`; `pointer motion is in progress`; `a batch carries at most
  <kMaxInjectedEvents> moments; this one has <n>`. The batch is judged whole first, each moment
  against the keys the ones before it would leave held: `moment 1: unknown kind 'Hover'`; `moment 0:
  a pointer moment needs a known space (cells or pixels)`; `moment 0: scancode <n> is outside the
  supported key domain 1..<kMaxScancode>`; `moment 0: a button is 1, 2 or 3`; `moment 0:
  TextEntered carries text`; `moment 2: pressing scancode <n> would hold <k> keys down at once; a
  session holds at most <kMaxHeldKeys> -- release one first`.
- **Example:** two moments, `{kind: "KeyPressed", scancode: scan::kA, modifiers: mod::kCtrl}` and
  its `KeyReleased`, type Ctrl+A.

### `PointerMotionRequested`

`PointerMotionRequested` v1 moves the pointer over time, from where your session last put it.

- **Takes:** `session`; `x` and `y`, the end, in the space of the session's last pointer moment;
  `duration_ms`; `bend`, 0 for a straight line, otherwise how far both control points of a cubic
  curve stand off the line, in that space. Seed the start with a pointer moment first.
- **Answers:** on each beat, at most one `PointerMoved`, the holder's, at the point the elapsed time
  has reached: a late beat skips ahead, never bursts, and a point equal to the last is left out
  until the end. At the end, `InputInjected` v1 to you alone, counting the moves: they were
  published, not that anything came of them. While it runs, `InjectInput` is refused, and closing
  the session ends it.
- **Refuses:** in `zen.Refused`: `pointer motion requires the held input session`; `motion needs
  an idle session, known bounded pointer position, duration 1..60000 ms and finite bend`, which
  also refuses a second motion while one runs; `motion needs an answerable request`.
- **Example:** after a `PointerMoved` to cell (10, 5), `{session: 1, x: 40, y: 12, duration_ms:
  300, bend: 0}` glides to (40, 12) in about a third of a second.

## Hears

### `TimerReady`

`TimerReady` v1, from the Timer: it is there, first or again after a reload. The weave asks for its
beat again, which keeps the one beat it has rather than adding a second.

### `TimerFired`

`TimerFired` v1, from the Timer: a beat. Its own, `zengine.input.pump`, polls the platform once, as
`PumpInput` does; another timer's firing aimed at the office polls nothing.

### `TimerResolution`

`TimerResolution` v1, from the Timer: what it made of the weave's order for its beat, kept by the
binding and answered with nothing.

## Follow

The weave publishes each moment as it happens, in the platform's order: `KeyPressed` v2,
`KeyReleased` v2, `TextEntered` v1, `PointerMoved` v1, `PointerButton` v1 and `PointerWheel` v1,
in its own name. Just before each, it publishes `AttributedInput` v1 as `zengine.input`, carrying the
same moment: `local` true and `actor` 0 for the platform's, `local` false and `actor` the holder's
weave id for a session's. Follow one of the two, not both, or you hear each moment twice.
`zengine-input-sdl` also publishes `SurfaceCloseRequested` v1 when the window is asked to close,
with no `AttributedInput`. Nothing else is said unasked, and there is no state to read but its two
counters, by poke.

## See also

- [The Input package](input.md): every moment's fields, keys, modifiers and pointer spaces, and what
  each platform can vouch for.
- [Pointer spaces](../../workshop/docs/pointer-spaces.md): where a reported position lands.
- [The Timer binding](../../timer/docs/timer-binding.md): how the beat is kept.
- [A weave's manual](../../manual/docs/manual.md): how this page is carried and asked.
