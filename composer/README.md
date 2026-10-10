# Compose

**Compose is the Workshop pane in which you write a message and send it to a running weave.
Select a weave in the Loaded pane, and Compose asks it which messages it accepts; pick one,
fill in its fields and submit it.**

The form is built from the message shapes the weave itself reports, so nobody has to write a
form for a weave before it can be spoken to, and a value that does not fit its field is refused
with a sentence naming the field and the kind of value it takes. A finished command, or an
unfinished preset, can be kept in Inventory and dropped back onto Compose later; a drop fills
the form, and only Submit sends it.

## Pages

- [Reuse a stored command through Compose](../inventory/docs/inventory-compose.md): choose a
  message, fill it, store it, bring it back and send it, step by step
- [Panes](../workshop/docs/panes.md): how Compose and every other pane is shown, moved and sized
- [The Loaded pane](../introspection/docs/introspection.md): where the weave Compose writes to
  is chosen
- [Reusable message and value drafts](../message-draft/docs/message-drafts.md): the typed
  values and presets Compose's fields are made of

## What is in this folder

| | |
|---|---|
| `composer.cpp` | the Compose pane itself, a weave Workshop loads |
| `draft.hpp` | a message being written: its fields and what they add up to |
| `view.hpp` | what the pane shows, row by row, and what each row stands for |
| `vocabulary.hpp` | the pane's lasting names: its office `zengine.composer` and pane `compose` |
| `command_context.hpp` | the note kept beside a stored command: which weave it was written for |
| `CMakeLists.txt` | how it is built |

Compose is part of Workshop, not of the installed library, so its headers are not installed.

Working on it: [AGENTS.md](AGENTS.md).
