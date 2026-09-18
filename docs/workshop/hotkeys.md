# Hotkeys and the keymap

**Workshop as a product.** What a key means, how to find out without leaving the screen, and
how to change a binding durably. The default bindings are listed in
[cheat_sheet.md](../../cheat_sheet.md#keys); this page is about the machinery that keeps every
listed key true.

## One binding truth

Workshop keeps one table of **actions** — a stable identity like `workshop.manage` or
`layout.new`, a label, the context it is available in, and a default gesture. Panes add their
own rows beside it — the Builder's `builder.build`, the Pane Manager's `launcher.close` — and
the desktop adds the keys that work anywhere. Dispatch resolves your keystroke against that one
map, and every place a gesture is written on screen — the bottom band, each mode's heading, the
notices, the Hotkeys pane — is a projection of it. There is no second list to drift: remap a
binding and the screen spells the new one everywhere, because nothing else has a spelling of
its own.

A binding matches its modifiers **exactly**. `w` arranges the desk; `Ctrl`+`w` is a different
gesture, and removes the layout you are on. Where one family is deliberately spelled two ways —
move and resize on the arrows and `Shift`+arrows while arranging, build and load-after-build on
`b` and `Shift`+`b` in the Builder — those are separate actions with separate bindings, each
remappable on its own.

A remap changes how an action is **requested**, never what it may do or who performs it. The
keymap holds names and gestures only; the operations stay with their owners.

## Keys the application supplies — and how to take them away

Some keys are not Workshop's at all. **Which tool a chord opens, and what a key means where
nothing more specific claimed it, belong to the desktop** — a tool that arrives by a row in your
load plan like every other, and that you can edit, rebuild or replace. Out of the box it supplies
four:

| key | what it does |
|---|---|
| `Ctrl`+`p` | open the **Pane Manager**, or put you in it if it is already open |
| `Ctrl`+`t` | open the **Terminal**, or put you in it |
| `Ctrl`+`k` | open the **Hotkeys** pane, or put you in it |
| `Esc` | put down the pane you have selected, where nothing more specific wanted the key |

The first three are answered **above every mode**: they work while your hands are in a pane,
which is the whole point of a key that opens a tool. A pane that must own one of them says so by
name, and then the chord is the pane's for as long as that pane has the keyboard — that is a
declaration, not a guess, so it survives you moving either key.

`Esc` is answered **last**, exactly where it always was: every mode, draft and focused pane
answers its own `Esc` first, and only a press nothing claimed puts the selected pane down.
Neovim's `Esc` is still Neovim's.

**Remap them like any other key**, by id — `desktop.panes`, `desktop.terminal`,
`desktop.hotkeys`, `desktop.deselect` — and **switch one off** by binding it to `none`:

```json
{ "action": "desktop.deselect", "gesture": "none" }
```

The row stays in the Hotkeys pane with no key beside it, and nothing takes over for it. There is
no second copy of the behaviour inside Workshop waiting to answer — which is what makes these
defaults genuinely yours. If you ever want a different set, change the desktop's own source
(`desktop-pane/pane.cpp`) and rebuild it: everything on this list is declared there, and so are
the Pane Manager's and the Hotkeys pane's own keys.

**The old names still work.** A keymap file written when these were Workshop's own —
`workshop.terminal`, `workshop.hotkeys` — is read as `desktop.terminal` and `desktop.hotkeys`,
and the load says so once, naming the rename to make. The file is not rewritten.

**A Workshop with no desktop still runs.** If that row of your load plan cannot be loaded, the
launch says so, the condition stands in Attention for the whole run, and the empty room is
blank — you have no application defaults and no Pane Manager until you build it and launch
again. Workshop's own keys (`w`, `a`, the layout keys, `q`) still work.

## The Hotkeys pane — `Ctrl`+`k`

`Ctrl`+`k` opens the **Hotkeys** pane, or puts you in it: the desktop's list of every key as it
is **in force right now** — a heading for each place a key is answered (command mode, the
arranging scopes, the contextual menu, the name line, each pane that declared rows, the keys
above every mode), and under it each row: the key as your keymap file would spell it, what it
does, and the id a file names it by. A `*` marks a key your own file moved or switched off.
`↑` `↓` `Home` `End` scroll it.

It is an ordinary pane: arrange it, cover it, close it, and `Ctrl`+`k` brings it back. It is not
a mode — reading it takes no keyboard away from anything, and every key it lists works while it
is open. What it shows is what Workshop tells it: the effective keymap, re-told whenever your
file, a pane's declaration or the desktop's own rows change. Inside any text field, the text
box's own editing keys — copy, cut, paste, select, word movement, undo — are shown for discovery
but are **not remappable**: they belong to the editing component, in every box at once, and the
keymap does not reach into it. A loaded pane's vocabulary beyond its declared rows is the
pane's own, and the list says so rather than guessing.

## The band legend

The bottom band carries the same projection on its legend rows — three rows in a terminal, one
row of real type in a graphical window, however many the band's room genuinely holds of the
medium's own text after the notice has taken its row — in three persisted modes, the `legend`
word in the keymap file:

| word | the legend rows show |
|---|---|
| `full` (and `default`) | the current context's bindings, packed as room permits, cut with a mark |
| `compact` | only how to open the Hotkeys pane, e.g. `^k hotkeys` |
| `hidden` | nothing |

`hidden` blanks the legend rows and does nothing else. Every other row — the notice beside
them, and the Layouts row a whole band away — never moves with the preference, the screen keeps
its shape, and no binding is unbound by choosing not to look at the legend. Where the packed
rows cannot carry every pair, the cut is marked and the Hotkeys pane remains the complete list,
one keystroke away in every mode.

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
    {"action":"desktop.terminal","gesture":"ctrl+g"},
    {"action":"layout.new","gesture":"n"}
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
how `Ctrl`+`c` quits from command mode and copies inside a field. (The object document's
`Ctrl`+`s` and `Ctrl`+`o` and the old attention view's `Ctrl`+`a` were the other members of this
class, and retired with what they opened. The Editor's `Ctrl`+`s` is its own `save source` row,
and the Neovim-backed Editor's `Ctrl`+`o` is Neovim's own jump back —
[Neovim in Workshop](neovim.md).)

**A pane's own rows are judged later, too.** A pane declares its rows when it arrives and may
declare them again while it runs — Files does whenever its chooser or recipe line opens, and the
Pane Manager does while its name line is open. Each declaration meets your file under the same
collision law, and one that collides is refused: Workshop names the pane and both actions on its
band and keeps the rows it last accepted from that pane, so it goes on naming keys by those rows.
Nothing is sent to the pane, which carries on in its new step, where those names may mean
nothing. [Files](files.md#moving-around) says what that means for its chooser and recipe line
today, and how to recover.

**What is kept:** an override whose action id this build does not know is preserved exactly
as you wrote it — byte for byte, in place — not deleted and not an error. It is your intent,
addressed to whichever build understands it. An id for an action that **retired** — the
`p` picker's `workshop.picker` and `picker.*`, the old Pane Manager's `pane-editor.*` and
`draft.*`, the object canvas's `object.*` — is kept the same way, answered by nothing, and the
load says what it retired with and where the act went now (`desktop.panes` opens and closes
panes; arranging a pane orders it; Info's own rows commit and cancel its edits).

**What is said:** a gesture with a known backend gap is accepted and the gap is named once at
load. The honest example: a plain POSIX terminal reports `shift` only on letters, never on
`space` or the digits; it cannot produce `ctrl+h/i/j/m` distinctly from backspace, tab,
newline and return; `alt` arrives only on the editing keys and `super` never. A graphical
window has none of these limits. Workshop cannot see which backend feeds it, so it applies
what you authored and tells you which terminals cannot say it.

**Resetting** is deleting the file, or the rows you regret. An absent file *is* the defaults;
nothing is stored anywhere else.

## What this is not

Provider panes keep their own keyboards for everything they did not declare: a focused pane
receives every ordinary key and character uninterpreted, and what those mean is the provider's
to define and to document. What a pane *declares* — its own action ids, such as `powers.view`
— joins this keymap under the same collision law, appears in the legend and the Hotkeys pane,
and moves in the same file, by id, exactly as Workshop's own rows do. The editing component's
keys are shared by every text field and are not per-application. And
there is no command palette, no macro recorder and no key-sequence grammar — a binding is one
named key with the modifiers that were held.
