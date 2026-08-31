# Hotkeys and the keymap

**Workshop as a product.** What a key means, how to find out without leaving the screen, and
how to change a binding durably. The default bindings are listed in
[cheat_sheet.md](../../cheat_sheet.md#keys); this page is about the machinery that keeps every
listed key true.

## One binding truth

Workshop keeps one table of **actions** — a stable identity like `object.new` or
`workshop.terminal`, a label, the context it is available in, and a default gesture. Dispatch
resolves your keystroke against that table, and every place a gesture is written on screen —
the bottom band, each mode's heading, the notices, the full hotkey view — is a projection of
the same table. There is no second list to drift: remap a binding and the screen spells the
new one everywhere, because nothing else has a spelling of its own.

A binding matches its modifiers **exactly**. `n` creates an object; `Ctrl`+`n` is a different
gesture and does nothing unless something is bound to it. Where one family is deliberately
spelled two ways — move and resize on `h j k l` and `Shift`+`h j k l`, build and
build-and-realize on `b` and `Shift`+`b` — those are separate actions with separate bindings,
each remappable on its own.

A remap changes how an action is **requested**, never what it may do or who performs it. The
keymap holds names and gestures only; the operations stay with their owners.

## The hotkey view — `Ctrl`+`k`

`Ctrl`+`k` opens a full list of what the keyboard means **right now**, **beside the pane you
are working with** — it appears at the top-left of the selected pane and follows it if you
move that pane or select another. With no pane selected it opens at the panel column, which
is where it has always opened. Nothing about its position is remembered: it is worked out
fresh each time from the pane you last pressed into, and near the bottom of the room it
shifts up so the whole view stays on screen.

The list is grouped by the layer that owns each row:

- the current context's own keys — command mode, the terminal line, the picker, the
  current-condition view, the two arranging scopes and their reset prompt, a property
  being edited, the source editor, the Files pane, or a focused pane;
- the keys answered above every mode (open, the terminal toggle, this view; where nothing
  is taking text, quit and the current-condition view; and everywhere but the source
  editor, the document's save — inside the editor the same chord is the editor's own
  `save source`);
- inside any text field, the text box's own editing keys — copy, cut, paste, select, word
  movement, undo — shown for discovery but **not remappable**: they belong to the editing
  component, in every box at once, and the keymap does not reach into it. The source
  editor's own editing keys are listed the same way, from its own declared vocabulary.

The view describes the context **beneath** it — open it over the terminal line and you read
the terminal line's keys — and it is modal while open: `Esc` or `Ctrl`+`k` puts it away, and
any other key is swallowed rather than executed, so reading a binding never performs one. For
a focused external pane it tells the honest whole of the story: every ordinary key goes to
the pane, and what each one means there is the provider's own. Workshop is deliberately never
told a provider's bindings and will not guess them.

## The band legend

The bottom band carries the same projection on its legend rows — three rows in a terminal, one
row of real type in a graphical window, however many the band's room genuinely holds of the
medium's own text after the notice has taken its row — in three persisted modes, the `legend`
word in the keymap file:

| word | the legend rows show |
|---|---|
| `full` (and `default`) | the current context's bindings, packed as room permits, cut with a mark |
| `compact` | only how to open the hotkey view, e.g. `^k hotkeys` |
| `hidden` | nothing |

`hidden` blanks the legend rows and does nothing else. Every other row — the notice beside
them, and the layout selector and workspace size on Workshop's own first row, a whole band
away — never moves with the preference, the screen keeps its shape, and no binding — the hotkey view's included — is unbound by choosing not to look at
the legend. Where the packed rows cannot carry every pair, the cut is marked and the full
hotkey view remains the complete list, one keystroke away in every mode.

## The keymap file

Overrides live in one hand-edited JSON file, `workshop-keymap.json` in your per-user config
folder by default — `%APPDATA%\zengine-workshop` on Windows, `$XDG_CONFIG_HOME/zengine-workshop`
(else `~/.config/zengine-workshop`) elsewhere — so your bindings follow *you* rather than the
directory you launched from (`--keymap <path>` chooses another file; `--isolated` reads none).
The defaults live in the program; the file carries only your differences:

```json
{"zen":"1","schema":"WorkshopKeymap","version":"1","value":{
  "format":"zengine-workshop-keymap","format_version":"1",
  "legend":"default",
  "overrides":[
    {"action":"workshop.terminal","gesture":"ctrl+g"},
    {"action":"object.new","gesture":"e"}
  ]}}
```

A gesture is modifier words joined to one key name with `+`: `ctrl+k`, `shift+h`, `[`, `]`.
Modifiers are `ctrl`, `shift`, `alt`, `super`; key names are the letters and digits, the
punctuation keys by their own character, and `return`, `escape`, `backspace`, `tab`, `space`,
`home`, `end`, `delete`, `left`, `right`, `up`, `down`. Nothing else is a binding: there are
no sequences, no leader keys and no macros.

The file is read once at startup and answered in words. A file that **applied** says so on the
notice line, with the override count — that happened, once, at this launch. A file that was
**refused** is a standing condition instead: the defaults stand for the whole run and for
every run until you change the file, so it goes to attention with the loader's own reason and
stays there until it is no longer true ([what needs your attention](attention.md)). Either
way the file is left exactly as you wrote it: Workshop never rewrites, trims or "fixes" it.

**What is refused, by name:**

- a gesture outside the grammar, naming what was found and what would have worked;
- an action authored twice;
- two actions holding one gesture in contexts that can be active together — the refusal names
  both actions and the contested gesture, because a lockout must not be savable. Reusing a
  gesture across contexts that cannot coexist is fine, and the defaults already do it (`w`
  opens the desk arrangement in command mode and resets a width inside the reset prompt);
- a **bare printable** on an always-available action: once anything on screen can take text,
  a bare letter cannot be global — it would be stolen from every field you type into;
- an editing chord (`ctrl+c`, `ctrl+v`, …) on an always-available action: every text field
  would consume it first, so the binding could not mean what it says where it matters most.

An action that is available only **where nothing is taking text** is a different class and is
allowed an editing chord, because in that class no text field is listening: that is exactly
how `Ctrl`+`c` quits from command mode and copies inside a field, and how `Ctrl`+`a` opens the
current-condition view from command mode and selects all inside one. A third class is
available **everywhere but the source editor** — the document's save, whose `Ctrl`+`s` is the
editor's own `save source` while your keys are in the source; because that class *is* active
inside the other text fields, it meets the same two guards a fully global action does.

**What is kept:** an override whose action id this build does not know is preserved exactly
as you wrote it — byte for byte, in place — not deleted and not an error. It is your intent,
addressed to whichever build understands it.

**What is said:** a gesture with a known backend gap is accepted and the gap is named once at
load. The honest example: a plain POSIX terminal reports `shift` only on letters, never on
`space` or the digits; it cannot produce `ctrl+h/i/j/m` distinctly from backspace, tab,
newline and return; `alt` arrives only on the editing keys and `super` never. A graphical
window has none of these limits. Workshop cannot see which backend feeds it, so it applies
what you authored and tells you which terminals cannot say it.

**Resetting** is deleting the file, or the rows you regret. An absent file *is* the defaults;
nothing is stored anywhere else.

## What this is not

Provider panes keep their own keyboards: a focused pane receives every ordinary key and
character uninterpreted, and its bindings are the provider's to define and to document. The
editing component's keys are shared by every text field and are not per-application. And
there is no command palette, no macro recorder and no key-sequence grammar — a binding is one
named key with the modifiers that were held.
