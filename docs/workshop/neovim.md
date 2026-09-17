# Neovim in Workshop

**Walkthrough and reference, current state.** Edit in Neovim inside a running Workshop, switch the
Editor between Workshop's standard Editor and Neovim while you work, and take your unsaved edits,
your caret and your selection with you each way. The same Neovim editor also runs from a Loom with
no Workshop at all, with Neovim's own interface in a second terminal. The exact contract is
[editor-switch.md](../reference/editor-switch.md).

## Before you start

- **A Neovim, 0.11 or newer.** Tested on 0.11.6 and 0.12.5. Workshop looks for `nvim` on your
  search path; set `ZENGINE_NEOVIM` to the full path of another one.
- **Your configuration stays yours.** By default Neovim starts **clean**: no `init.lua`, no
  plugins, and its state kept apart from yours under the application name `zengine-neovim-clean`.
  Set `ZENGINE_NEOVIM_PROFILE=user` to start it with your own configuration, or to the path of an
  init file to use that one.
- **The load plan authors the choice.** Both shipped plans name two editors for the Editor's office,
  `standard` (the one that starts) and `neovim`. A plan of your own names them under `choices`
  ([load plans](load-plans.md)).

## Switch while you work

Open the **Terminal** pane from the picker (`p`), press into its line, and type:

```text
ask @zengine.editor-switch EditorSwitchRequested 1 destination=neovim
```

Workshop starts Neovim in the Editor pane's room, hands it the document -- the file, your unsaved
edits, the caret, the selection, the scroll position -- and the Editor pane becomes Neovim, in the
same place on your desk. What came of it is said on Workshop's notice line, in the answer's own
words -- ``editor switch 1: switched -- switched zengine.editor from `standard` to `neovim` `` --
and the Terminal's record shows that the answer arrived, with the switch office's weave number:
`v EditorSwitchAnswered v1 from #14  [Loom: answers ask 1]`.

Switch back the same way:

```text
ask @zengine.editor-switch EditorSwitchRequested 1 destination=standard
```

Asking for the editor you already have is harmless: the notice says `already-active` and nothing
is loaded. To see where you are -- which editor holds the office, which are authored, and whether
a switch is under way, said on the notice line:

```text
ask @zengine.editor-switch EditorSwitchStatusRequested 1
```

### When a switch asks first

A switch away from Neovim can lose something the standard Editor cannot hold -- **another buffer
with unsaved changes**, or a **terminal job** running in Neovim. Then nothing is loaded yet, the
answer is `needs-confirmation`, and the Attention pane keeps the question standing with the exact
line that confirms it -- its `op` and its `consent` filled in:

```text
ask @zengine.editor-switch EditorSwitchConfirmed 1 op=3 consent=c4f2a91c
```

Use the numbers your answer gave. If what would be lost changes before the switch takes -- you
modify another buffer meanwhile -- you are asked again with a new consent, and nothing crosses on
the old one. To stop instead:

```text
ask @zengine.editor-switch EditorSwitchCancelled 1 op=3
```

Nothing is saved for you, ever: write the other buffer (`:w`) and ask again if you want to keep it.

### When a switch says no

Every refusal says why, in the words of whatever refused, and **nothing moves**: your editor, your
document and your desk are as they were, and the editor you were in tells you on its pane.

| you see | do |
|---|---|
| `Neovim is not available: ...` | install Neovim, or set `ZENGINE_NEOVIM` to it |
| `... older than this Workshop supports` | use Neovim 0.11 or newer |
| `Neovim stopped at a prompt while starting` | your configuration printed an error; fix it, or use the clean profile |
| `Neovim is waiting at a prompt` | answer the prompt in Neovim (usually Enter), then ask again |
| `Neovim's current buffer is not a file` | go to a file's buffer (a help or terminal buffer cannot be carried) |
| `... has no file name` | write it somewhere first (`:w name`) |
| `the standard Editor cannot carry ...` | the document holds bytes the standard Editor does not edit (outside printable ASCII) |
| `... is under way (warming)` | a switch is still in progress; wait for its answer, or cancel it |

A pending **count** in Neovim (you typed `3` and stopped) is not a refusal: the switch cancels it
the way Escape would, and says so among the resets. While a switch is under way, the Attention pane
shows it, with the line that cancels it.

## Editing in the Neovim pane

Press into the pane and your keys are Neovim's: modes, motions, `:` commands, search, Visual
selection, undo, macros. The top row is Workshop's -- `saved` or `UNSAVED`, Neovim's mode and the
file -- and it shows a notice when there is one; everything under it is Neovim's own screen.

| key | in the Neovim pane |
|---|---|
| `Ctrl`+`s` | writes the buffer (`:write`), without leaving the mode you are in |
| `Ctrl`+`o` | Neovim's own jump back (and one Normal command from Insert) -- not Workshop's open |
| `Esc` | Neovim's; it never takes you out of the pane |
| `Ctrl`+`k` | still Workshop's hotkey view |
| a press elsewhere | gives your keys back to Workshop |

A press in Neovim's screen places Neovim's cursor, a drag selects, and the wheel scrolls. Resizing
the pane resizes Neovim.

**Opening files reaches Neovim.** The Files pane, the Builder's `e` and **edit code** on a pane all
open into whichever editor holds the office, so while Neovim does, they open in Neovim -- in a new
buffer, beside any unsaved ones.

**Copy and paste cross with everything else.** A yank to `+` or `*` reaches your clipboard, and a
paste from `+` or `*` asks for it -- unless your own configuration chose a clipboard provider, which
is then left alone.

**What the pane cannot show.** Every cell is drawn as one plain character, so box drawing becomes
`-`, `|` and `+` and anything else becomes `?` -- only on screen; the document is untouched. The pane
shows one selection range and no colours: syntax highlighting and search matches are not drawn, and
a blockwise selection shows only its cursor's row.

## Ending

- **Hiding or covering the pane** ends nothing; Neovim keeps running.
- **Switching away** ends Neovim once the other editor has the document.
- **`:qa` inside Neovim** ends it; the Editor pane stays, says Neovim exited, and holds no document.
  Opening a file starts a new Neovim.
- **Quitting Workshop** asks Neovim first: it is refused while any buffer has unsaved changes or a
  terminal job is running, naming them.
- **Rebuilding and reloading the Neovim editor itself** is refused while Neovim runs, because a
  reload would end it; quit Neovim or switch editors first.

## From a Loom with no Workshop

The same `zengine-neovim-editor` artifact runs under `loom-host`, installed from a Zengine prefix
(`lib/zengine/`). Neovim runs **headless and listening**, and you attach Neovim's own interface
from a **second terminal**, so `loom-host` keeps its console.

1. Name the Timer and the Neovim editor in your boot file, the Neovim editor in the Editor's office:

   ```json
   {
     "boot": [
       { "name": "zengine-timer", "path": "<prefix>/lib/zengine/zengine-timer.so", "role": "zengine.timer" },
       { "name": "zengine-neovim-editor", "path": "<prefix>/lib/zengine/zengine-neovim-editor.so", "role": "zengine.editor" }
     ]
   }
   ```

   On Windows the files end in `.dll`.
2. Start `loom-host`, approve both with `authority trust zengine-timer` and
   `authority trust zengine-neovim-editor`, and start it again.
3. Let the Neovim editor ask the Timer for its beat:
   `authority allow zengine-neovim-editor EnsureTimer v1 -> role zengine.timer`.
4. Find its weave number with `weaves`, and ask it to start Neovim on a file:

   ```text
   loom> send <weave> NeovimStartRequested 1 path="notes.txt"
   ```

   The answer names the address Neovim listens at and the exact line to attach with.
5. In a second terminal, run that line -- `nvim --server <address> --remote-ui` -- and edit.
   `:w` writes. Closing that interface (`:q` in it, or closing the terminal) leaves Neovim running.
6. Back at `loom>`, `send <weave> NeovimStatusRequested 1` says what Neovim holds, and
   `send <weave> NeovimStopRequested 1` ends it -- refused while a buffer has unsaved changes, unless
   you add `discard=true`.
