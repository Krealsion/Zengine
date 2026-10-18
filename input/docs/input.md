# The Input package

**Reference.** The vocabulary a weave includes to hear a person, and the pieces the Input weaves
are made of: what each shape preserves, the key, modifier and space numbers, how each platform's
events become those shapes, and the weave a reader is wrapped in. The office that publishes them,
its input sessions and its timed pointer motion are [its manual, `zengine.input`](zengine.input.md).

Source: [`input/vocabulary.hpp`](../vocabulary.hpp) ·
[`input/translate.hpp`](../translate.hpp) ·
[`input/input_weave.hpp`](../input_weave.hpp). Where a reported position *lands* is
[pointer spaces](../../workshop/docs/pointer-spaces.md).

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

The backends are the ones snake and Workshop run on. The **POSIX terminal** parses raw-mode
bytes with a *stateful, incremental* parser — an OS read boundary is not an event boundary, so a
mouse report split across reads is rejoined rather than translated into the keystrokes its bytes
happen to spell; a lone `ESC` is held until an empty poll resolves it as the Escape key. Pointer
reports are SGR (`ESC [ < b ; x ; y M/m`), 1-based and translated to the 0-based contract, and
the CSI editing keys are named too: the arrows, Home, End and Delete in their
bare, tilde-numbered and `1;m`-modified spellings, with the modifier parameter *measured* (xterm's
1 + Shift/Alt/Ctrl bitmask; the Meta bit is deliberately not claimed). The **Win32 console**
reads `INPUT_RECORD`s: `uChar.UnicodeChar` is the text, `dwControlKeyState` the modifiers,
`dwMousePosition` the position — all present on the record and all preserved — and its VK table
names Home, End and Delete. The SDL **Reader** drains the window's one process-global event
queue, unless an OS window in the process claims it (`input/window_claim.hpp`) and reads it itself
with the same translation (`input/translate_sdl_core.hpp`): SDL scancodes pass through as the wire
identity they already are, and one fact on that queue is not an input moment and is routed in the
Surface vocabulary instead — the close box (`SurfaceCloseRequested`). The platform's clipboard
events are in the reader's ignored set deliberately, not from disinterest: clipboard read follows
paste intent, so the clipboard is
read through the Skin when a paste asks with `ClipboardTextRequested` — see
[surface/docs/surface.md](../../surface/docs/surface.md) — and never watched; no shape leaving this reader carries clipboard
text. Each backend's exact honest reach — which modifiers it can vouch for on which keys —
stays documented in `input/translate.hpp`. The terminal readers (`input/terminal_reader.hpp`)
serve any program reading a terminal, a terminal window among them, not only the Input weave.

**Who turns the terminal's pointer on:** the **Skin**, because terminal modes are output and the
output stream is already claimed and released on the Skin's own lifetime (`surface/skin_tui.hpp`).
Input never writes a byte to the terminal; it parses SGR reports whenever they arrive, and they
only arrive because a Skin asked. The two packages need no coordination surface between them.

## The weave over a reader

`InputWeaveT<Reader, Facets...>` (`input/input_weave.hpp`) is the whole Input weave around a
reader: anything with `poll()` returning a vector of a variant of the shapes it publishes. Its Emit
set is derived from that variant, so a reader that can hand it a further shape (the SDL reader's
`SurfaceCloseRequested`) declares it and no other weave does. It keeps the office's beat, its
sessions and its timed motion, as [the manual](zengine.input.md) says. `Facets` compose onto it:
the two shipped weaves name their office's page, `OfficePage`, a `manual::ManualFacet` that adds
the door answering what each accepted shape does; with none there is no such door, as a suite's
weave over a scripted reader is built.
