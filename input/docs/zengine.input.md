# zengine.input -- Input

**Manual.** Input: the weave that turns the keyboard and the pointer into messages, answering to the
role `zengine.input` (a role is the name a weave answers to). `zengine-input` holds it: it
reads the terminal, or the console on Windows. Workshop's plan loads it beside either window
([the shipped plan](../../workshop/docs/load-plans.md#the-shipped-plan)); a program of your own
loads it in that role. Your weave hears a person by accepting what it publishes; an
agent or a test speaks for one through an input session, by the messages below.

## About

Input reports *moments*, and your weave interprets *gestures*. A moment carries what the platform
knew when one thing happened: a key went down or came up, with the modifiers held at that instant;
text arrived as the keyboard layout typed it; the pointer moved, a button changed or the wheel
turned, each with where the pointer was. A drag or a selection is yours to make of them. What each
shape carries, and what each platform can vouch for, is [the Input package](input.md).

The weave never writes to the screen. It says pointer positions in terminal cells. On a POSIX
terminal it reads its standard input in raw mode, and says a key's release right after its press,
since a terminal reports whole keystrokes; pointer reports arrive only once whatever shows the
terminal has turned them on, as the TUI window does. On Windows it reads the console's
records, releases included. With no terminal (a pipe, a test run) it reads nothing, and says
nothing. When a TUI window (`zengine.window.tui`) holds the process's terminal in the
same run, it leaves the terminal's input to the window and serves input sessions alone. Beside the
GUI window ([`zengine.window.gui`](../../graphics/docs/zengine.window.gui.md)), which reads its own
events, it serves input sessions alone and leaves the console the run was started from as it was:
not read, what is typed there echoed, and a Ctrl+C there ending the run as it ends any program.

The role owns the double-click time, a setting a weaver sets
([`RoleSettings`](#rolesettings)): a window counts each hand's clicks by it, a session's as
the weaver's.

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

The weave is a source of the host's one wait, and keeps no time of its own. After every delivery
it claims, as its role, what its reader waits on (`loom.host.WaitSet` v2): the terminal's input
while it reads it, nothing while a window holds it; and, while a timed motion plays or a lone
`ESC` waits to be released, a time to look again, `kLookAgainMs` after the time the wait last
handed it. A reader the wait cannot hold is looked at on that time alone. It reads when the wait
says what it waits on is ready or its time came ([`Waited`](#loomhostwaited)); a host that runs no
wait on its role wakes it never, and `PumpInput` is that host's door
([a source's own time](../../timer/docs/timers.md#the-hosts-wait)).


## Commands

### `PumpInput`

`PumpInput` v1 gives the weave its hands once, as a wake does: it moves a timed motion on, then
polls the platform once and publishes what came.

- **Takes:** nothing.
- **Answers:** nothing is answered; what the platform had is published ([Follow](#follow)).
- **Refuses:** nothing.
- **Example:** a suite, or a host that runs no wait, sends `PumpInput{}` to `zengine.input`.

### `InputSessionRequested`

`InputSessionRequested` v1 opens the one input session, held by you: the sender the bus names, never
a name in the message.

- **Takes:** `purpose`, one line for a person reading what is open.
- **Answers:** `InputSessionOpened` v1, to you alone: `session`, the number you name it by, counted
  from 1 by this weave.
- **Refuses:** in `zen.Refused`, while a session is open: `busy: input session 1 is held by this
  connection, opened for "workshop probe"; it must be closed before another opens` when the sender
  holds it -- every run on one link is that one sender -- or `busy: input session 1 is held by
  another participant, opened for "workshop probe"; it must be closed before another opens`. The
  purpose is the holder's own, `opened with no purpose given` when it gave none, and cut on a
  character boundary with `...` past <!-- value kPurposeQuoted -->160<!-- /value -->
  bytes.
- **Example:** `mail.send_to_role("zengine.input", zengine::input::InputSessionRequested{"workshop probe"});`,
  as the Workshop probe opens its session.

### `InputSessionClosed`

`InputSessionClosed` v1 closes the session: by its holder, or by a role speaking on the holder's
behalf.

- **Takes:** `session`; `holder`, the holder's weave id, read only when a role closes it. A role
  may say `session` 0 for whatever that holder holds.
- **Answers:** first every key and button the session still holds comes up, published as ordinary
  `KeyReleased` and `PointerButton` moments at its last pointer position, still the holder's; a
  timed motion still running is refused to its asker, `pointer motion cancelled: session closed`;
  then `zen.Ack`. The releases may finish a gesture half made; nothing is undone.
- **Refuses:** in `zen.Refused`: `no open input session numbered 3`; to anyone else, even one naming
  the holder, `input session 1 is closed by its holder, or by a role closing it on the holder's
  behalf`.
- **Example:** when a guest's connection is gone, Workshop's guest door sends
  `{session: 0, holder: <the guest's id>}` as `zengine.guests`.

### `InjectInput`

`InjectInput` v2 publishes moments you composed, in the order given, through your session.

- **Takes:** `session`; `events`, each a `Moment` v1, a union whose one case is the shape it
  becomes -- `KeyPressed`, `KeyReleased`, `TextEntered`, `PointerMoved`, `PointerButton` or
  `PointerWheel` -- holding that shape's fields; a case it does not declare the gate refuses. A key
  names a scancode in 1..`kMaxScancode`, a button 1, 2 or 3, text a character, a pointer moment
  its space, cells or pixels. A batch holds at most `kMaxInjectedEvents` moments, and a session at
  most `kMaxHeldKeys` keys down; a press of a key already down is a repeat and holds nothing new.
- **Answers:** `InputInjected` v1, to you alone: `admitted`, how many were published, and
  `first_seq` and `last_seq`, their numbers in the session's own count. Published is not
  processed: what a consumer makes of a moment is its own, later.
- **Refuses:** in `zen.Refused`, publishing nothing: `no open input session numbered 2`; `input
  session 1 is another participant's`; `pointer motion is in progress`; `a batch carries at most
  <kMaxInjectedEvents> moments; this one has <n>`. The batch is judged whole first, each moment
  against the keys the ones before it would leave held: `moment 0: a pointer moment needs a known
  space (cells or pixels)`; `moment 0: scancode <n> is outside the supported key domain
  1..<kMaxScancode>`; `moment 0: a button is 1, 2 or 3`; `moment 0: TextEntered carries text`;
  `moment 2: pressing scancode <n> would hold <k> keys down at once; a session holds at most
  <kMaxHeldKeys> -- release one first`.
- **Example:** `{KeyPressed: {scancode: scan::kA, name: "", modifiers: mod::kCtrl}}` and its
  `KeyReleased` type Ctrl+A. Within the double-click time, a `scan::kLeftShift` tap sent twice is
  a double Shift, and a press and release at one point sent twice a double click, for your hand.

### `PointerMotionRequested`

`PointerMotionRequested` v1 moves the pointer over time, from where your session last put it.

- **Takes:** `session`; `x` and `y`, the end, in the space of the session's last pointer moment;
  `duration_ms`; `bend`, 0 for a straight line, otherwise how far both control points of a cubic
  curve stand off the line, in that space. Seed the start with a pointer moment first.
- **Answers:** at each wake, at most one `PointerMoved`, the holder's, at the point the time the
  wait handed has reached since the first it handed after the ask: a late wake skips ahead, never
  bursts, and a point equal to the last is left out
  until the end. At the end, `InputInjected` v1 to you alone, counting the moves: they were
  published, not that anything came of them. While it runs, `InjectInput` is refused, and closing
  the session ends it.
- **Refuses:** in `zen.Refused`: `pointer motion requires the held input session`; `motion needs
  an idle session, known bounded pointer position, duration 1..60000 ms and finite bend`, which
  also refuses a second motion while one runs; `motion needs an answerable request`.
- **Example:** after a `PointerMoved` to cell (10, 5), `{session: 1, x: 40, y: 12, duration_ms:
  300, bend: 0}` glides to (40, 12) in about a third of a second.

### `PointerTimingAsked`

`PointerTimingAsked` v1 asks the double-click time: how close in time two presses of one button
are one double click, which a window counts the weaver's clicks by. It is the role's one
role-wide setting, `input.double-click-ms`, which a weaver sets in the Settings pane
([`RoleSettings`](#rolesettings)).

- **Takes:** nothing.
- **Answers:** `PointerTimingSaid` v1, to you alone, as `zengine.input`: `double_click_ms`, the time
  two presses of one button are one double click within. Believe it from the role alone
  (`mail.authored_from_role("zengine.input")`).
- **Refuses:** nothing.
- **Example:** a window sends `PointerTimingAsked{}` to `zengine.input` each time it is activated.

## Hears

### `RoleSettings`

`RoleSettings` v1, from `zengine.layouts` alone: `input.double-click-ms` as Workshop keeps it, a
default kept as its absence, which the weave declares to that role
([`RoleSettingsDeclared`](../../workshop/docs/zengine.layouts.md#rolesettingsdeclared)) each
time it is activated. A time other than the one it holds is held to 200..2000 ms, taken and
published ([Follow](#follow)); until one is handed it, it holds
<!-- value kDefaultDoubleClickMs -->400<!-- /value --> ms.

### `loom.host.Waited`

`loom.host.Waited` v2, from the host alone, with the time on the wait's clock: its one wait holds
the role's claim, or holds it no longer, so the weave looks on a time alone; or what it waits on
is ready, or the time it claimed came, and the weave looks once, as `PumpInput` does. From any
participant it is nothing.

## Follow

The weave publishes each moment as it happens, in the platform's order: `KeyPressed` v2,
`KeyReleased` v2, `TextEntered` v1, `PointerMoved` v1, `PointerButton` v1 and `PointerWheel` v1,
in its own name. Just before each, it publishes `AttributedInput` v2 as `zengine.input`, carrying the
same moment as its `event`, a `Moment`: `local` true and `actor` 0 for the platform's, `local` false and `actor` the holder's
weave id for a session's. Follow one of the two, not both, or you hear each moment twice. When the
double-click time it holds changes, it publishes `PointerTimingSaid` v1 as `zengine.input`. Nothing
else is said unasked, and there is no state to read but its two counters, by poke.

## See also

- [The Input package](input.md): every moment's fields, keys, modifiers and pointer spaces, and what
  each platform can vouch for.
- [Pointer spaces](../../workshop/docs/pointer-spaces.md): where a reported position lands.
- [The host's wait](../../timer/docs/timers.md#the-hosts-wait): how a source is woken.
- [A weave's manual](../../manual/docs/manual.md): how this page is carried and asked.
