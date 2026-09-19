# The Input package

**Reference.** What the Input weave produces, what each shape preserves, and which backend
produces it. If you are writing a weave that reacts to a person, this is the vocabulary you
accept.

Source: [`input/vocabulary.hpp`](../../input/vocabulary.hpp) ·
[`input/translate.hpp`](../../input/translate.hpp) ·
[`input/input_weave.hpp`](../../input/input_weave.hpp). Where a reported position *lands* is
[pointer spaces](pointer-spaces.md).

The floor games sit on: exactly one Input weave (`zengine-input`, holding the `zengine.input`
role) is the sole producer of the input shapes and the only code that talks to the platform.
Consumers only accept; there is no polling API.

**The law: Input reports coherent MOMENTS; applications interpret GESTURES.** A moment
carries everything the backend already knew when one thing happened, so no consumer has to
reconstruct a fact the platform had already stated. A drag, a resize or a selection is
application meaning and is not spoken here at any version.

| shape | what it preserves |
|---|---|
| `KeyPressed` / `KeyReleased` v2 | which key changed state, plus the **modifiers held at that transition**. SDL scancodes are the wire identity; `name` is convenience, never authority. |
| `TextEntered` v1 | **what the user actually typed**, UTF-8, as the platform's own keyboard layout produced it. The only truthful route to a character — nobody computes `Shift+5 -> %`. |
| `PointerMoved` v1 | position, delta, coordinate **space**, modifiers. |
| `PointerButton` v1 | button, transition, **the position it happened at**, space, modifiers. |
| `PointerWheel` v1 | notches, position, space, modifiers. |

Positions are int64 and carry a `space` (`kCells` on the two terminal backends, `kPixels` on the
SDL one) so a terminal cell can never be mistaken for an SDL pixel. Editing controls are keys,
never text: Backspace, Enter and Escape arrive as transitions, and what they *mean* is the
application's.

Backends today are the ones snake and Workshop run on. The **POSIX terminal** parses raw-mode
bytes with a *stateful, incremental* parser — an OS read boundary is not an event boundary, so a
mouse report split across reads is rejoined rather than translated into the keystrokes its bytes
happen to spell; a lone `ESC` is held until an empty poll resolves it as the Escape key. Pointer
reports are SGR (`ESC [ < b ; x ; y M/m`), 1-based and translated to the 0-based contract, and —
since TEXT-0 — the CSI editing keys are named too: the arrows, Home, End and Delete in their
bare, tilde-numbered and `1;m`-modified spellings, with the modifier parameter *measured* (xterm's
1 + Shift/Alt/Ctrl bitmask; the Meta bit is deliberately not claimed). The **Win32 console**
reads `INPUT_RECORD`s: `uChar.UnicodeChar` is the text, `dwControlKeyState` the modifiers,
`dwMousePosition` the position — all present on the record and all preserved — and its VK table
names Home, End and Delete. The SDL **Reader** owns the window's one process-global event
queue: SDL scancodes pass through as the wire identity they already are, and one fact on that
queue is not an input moment and is routed in the Surface vocabulary instead — the close box
(`SurfaceCloseRequested`). The platform's clipboard events are in the reader's ignored set
deliberately, not from disinterest: clipboard read follows paste intent, so the clipboard is
read through the Skin when a paste asks with `ClipboardTextRequested` — see
[surface.md](surface.md) — and never watched; no shape leaving this reader carries clipboard
text. Each backend's exact honest reach — which modifiers it can vouch for on which keys —
stays documented in `input/translate.hpp`.

**Who turns the terminal's pointer on:** the **Skin**, because terminal modes are output and the
output stream is already claimed and released on the Skin's own lifetime (`surface/skin_tui.hpp`).
Input never writes a byte to the terminal; it parses SGR reports whenever they arrive, and they
only arrive because a Skin asked. The two packages need no coordination surface between them.

The weave arranges its own execution: on the TimerService's hello it asks for a repeating
role-addressed beat (`zengine.input.pump`, 10ms — the package owns its own pace) and polls on
each firing; `PumpInput` stays as the same hands on direct request, for suites and timer-less
hosts.

## An input session: a second source of moments for the one producer

The floor does not move — one Input weave produces the shapes above. A **session** is a second
*source* for that producer: a participant on the bus (an agent's proxy across the crossing, a
test weave, a script) hands the weave moments it composed, and the weave publishes them as the
same `KeyPressed`, `TextEntered`, `PointerButton` and the rest, in the order handed, from the
same identity, to the same consumers. Nothing downstream can tell an injected moment from a
platform one; everything from the bus onward is exercised for real. What is *not* exercised is
the platform edge — the console reader, the SDL queue, the OS translation — which only a hand
on a device reaches.

| shape | what it does |
|---|---|
| `InputSessionRequested{purpose}` → `InputSessionOpened{session}` / `zen.Refused` | opens the one session; a second opener is refused `busy` and told which session stands |
| `InjectInput{session, events}` → `InputInjected{admitted, first_seq, last_seq}` / `zen.Refused` | publishes the moments, in order; judged **whole** — one bad moment (an unknown kind, an unknown pointer space, a button outside 1..3, a key outside 1..511, a press that would hold more than 16 keys down, more than 64 moments) publishes nothing and names its index |
| `InputSessionClosed{session, holder}` → `zen.Ack` / `zen.Refused` | closes it; every key and button the session still held down is **released first**, as ordinary published moments. An office may say `session` 0 for "whatever that holder holds" |

A session belongs to the bus-stamped sender that opened it, never to a name in a payload. It is
closed by that holder personally, or **on its behalf by an office speaking deliberately** —
Workshop's guest door closes a guest's session when the guest's connection dies — and never by
a stranger's personal word. Admitted is not processed: `InputInjected` says the moments are on
the bus in order; what a consumer makes of them is that consumer's, later, on its own delivery.
A session is not focus, not a lease on the keyboard and not a claim against the platform: a
person at the keyboard is still heard while one is open, and the two sources interleave in
arrival order.

**What a session may hold down is bounded, and so is what closing it costs.** A key moment names
a scancode in SDL's space, `1..kMaxScancode` (511; 0 is "unknown key", which nobody presses on
purpose) — the values every backend already speaks, so an injected key means on every backend
what a platform one means. A session holds at most `kMaxHeldKeys` (16) keys down at once;
buttons are 1..3 already. Each batch is judged against the held state it *would* leave, moment
by moment, before anything is published: pressing a key that is already down is an auto-repeat
and holds nothing new, a release makes room, and a batch that would pass the bound at any
moment — even one that lets the key go again later in the same batch — is refused whole, naming
that moment. Closing therefore publishes at most sixteen key releases and three button releases.
The bound is on what a session holds, never on how long: nothing expires, and a key held
legitimately stays held until it is released or the session closes.
