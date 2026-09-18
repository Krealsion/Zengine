# Getting started with Workshop

Workshop is an interactive environment for making things, built with Zengine. It is optional:
the [C++ library](../getting-started.md) is usable without it, and nothing here is required to
write a weave.

**What Workshop is today:** a desk of panes over one room. A Terminal pane talks to the live
process, a Builder starts a real build and offers the result to the running project, an
[Editor pane](editor.md) opens the file a build recipe names and holds it while you edit, and
Info describes any pane and edits where it sits and how big it is — so `edit → save → build →
realize → inspect` closes without leaving the application. The desk itself — the Pane Manager
that opens, closes and makes panes, the Hotkeys pane, and the keys that work anywhere — is a
tool too, and you can edit, rebuild and replace it while Workshop runs. Your desk comes back on
its own when you relaunch.

**What it is not yet** is written down in [limitations](limitations.md) rather than left to
be discovered — the editor holds one plain-ASCII file at a time, a pane you make yourself holds
one line of text, and a rebuilt weave whose shape changed needs a prepared replacement rather
than a reload.

## Launch it

```sh
build/workshop/zengine-workshop
```

Workshop needs a terminal at least **78 columns by 22 rows**. That is not a preference: at
78x22 every piece of its furniture is exactly on screen with nothing overlapping, and it is
the composition every other size is measured against.

For a real window instead of a terminal, pass the graphical plan:

```sh
build/workshop/zengine-workshop --load-plan workshop/graphical-load-plan.json
```

Both are shipped files. They differ in exactly two rows — which skin claims the medium, and
which input backend produces the moments — and Workshop's own code is identical under both.
See [load plans](load-plans.md).

### Arguments

| argument | default | is |
|---|---|---|
| `--setup <path>` | `workshop-setup.json`, in the directory you launched from | a pane arrangement you named and saved |
| `--pane <path>` | `workshop-pane.json`, in the directory you launched from | a pane you made with the [Pane Creator](panes.md#the-pane-creator--a-pane-made-of-data): its name and its regions |
| `--session <path>` | `workshop-session.json`, in your **per-user state folder** | the desk, window size and window position you last used — written on close, read on start |
| `--keymap <path>` | `workshop-keymap.json`, in your **per-user config folder** | your hand-edited binding overrides ([hotkeys](hotkeys.md)) |
| `--prefs <path>` | `workshop-prefs.json`, in your **per-user config folder** | presentation preferences Workshop writes when you state one (pane titles, `t`) |
| `--isolated` | off | this run reads and writes **none** of your per-user config or session state — for tests, scratch experiments and clean-start diagnosis |
| `--load-plan <path>` | `workshop-plan.json` in the directory you launched from when there is one, else `default-load-plan.json` beside the binary | which artifacts this run is made of |
| `--recipes <path>` | `build-recipes.json` in the directory you launched from when there is one, else `default-build-recipes.json` beside the binary | what this run can build ([Builder](builder.md)) |
| `--log <path>` | none | a durable journal of selected facts, appended as they happen; outlives the process |
| `--dump <path>` | none | what the volatile recorder still held when Workshop quit |
| `--document <path>` | `workshop.json` in the project, when there is one | an object document from before the object canvas retired: named once at startup and left exactly as it is |

An empty path is refused by name. `--log` and `--dump` are two different questions: the
journal is what you keep on purpose, the dump is most of a session's story recovered after the
fact. Without either, nothing is written and `q` always leaves a live process.

The **project files** (setup, pane and — once you have written them — plan and recipes) follow
the project: launch from two directories and you have two projects. The three **maker files**
follow *you*: on Windows the config folder is `%APPDATA%\zengine-workshop` and the state folder
is `%LOCALAPPDATA%\zengine-workshop`; elsewhere they are `$XDG_CONFIG_HOME/zengine-workshop`
(falling back to `~/.config/...`) and `$XDG_STATE_HOME/zengine-workshop` (falling back to
`~/.local/state/...`). An explicit path always wins, `--isolated` wins over the defaults, and
a first launch after upgrading imports any old launch-directory `workshop-keymap.json` /
`workshop-session.json` into those folders **once** — saying so plainly, never deleting the
original, and never overwriting a file already there.

**An old object document is left alone.** Workshop used to open onto a canvas of authored
rectangles kept in `workshop.json`. That canvas retired; your desks, layouts and saved
arrangements did not. If the project still holds a `workshop.json` (or you name one with
`--document`), Workshop says so once at startup and leaves the file exactly as it is — it is
never read as anything else, moved or deleted.

### What it prints before it draws

Workshop prints a few lines of plain scrollback on the way up, and each is a fact you are
entitled to before you press anything:

```text
zengine-workshop - containment: in-process; trusted; no OS sandbox (out-of-process isolation is the isolation host's job)
zengine-workshop - document: retired with the object canvas
zengine-workshop - setup: workshop-setup.json
zengine-workshop - pane: /home/you/my-thing/workshop-pane.json
zengine-workshop - project: /home/you/my-thing
zengine-workshop - last session: C:/Users/you/AppData/Local/zengine-workshop/workshop-session.json (restored at startup, written on quit)
zengine-workshop - keymap: C:/Users/you/AppData/Roaming/zengine-workshop/workshop-keymap.json
zengine-workshop - prefs: C:/Users/you/AppData/Roaming/zengine-workshop/workshop-prefs.json
zengine-workshop - load plan: .../default-load-plan.json
zengine-workshop - load plan: 14 artifact(s) declared
zengine-workshop - log: nothing durable (--log <path> to keep one)
zengine-workshop - terminal: weave #3 (presented by the Terminal pane)
zengine-workshop - build recipes: .../default-build-recipes.json (1)
zengine-workshop - builder: weave #5 holds 1 recipe(s) (open its pane from the Pane Manager)
zengine-workshop - build runner: weave #4 builds with `/usr/bin/cmake`
zengine-workshop - recipe: skin-tui-block -> .../zengine-skin-tui-block.so
```

- **what this host isolates** — `Kernel::containment_note()`, verbatim. An in-process Zengine
  host isolates nothing; read the sentence literally.
- **which files this run is using**, and how many artifacts the plan declared.
- **which project this is** — the directory you launched in. It is what the Files pane
  browses and what a relative source path in a build recipe is relative to.
- **whether anything durable is being kept**, so a session that mattered is not discovered to
  have been unrecorded afterwards.
- **what the Builder will actually run** — the program and its arguments, because a panel cannot
  show you a command before the runner has started it, and what a key in this program will run
  is a fact you are entitled to before you press it.
- **a tool that is not here** — `zengine-workshop - unavailable: ...`, with the refusing layer's
  own sentence, for a pane the plan could not load. Workshop runs without it: see below.

## The screen

```text
 +---------------------------------------------------------------------+
 | Layouts: >Default< +                                    setup: none |
 +--------------------------------------------+------------------------+
 |                                            |                        |
 |   the room: the desktop's floor, and the   |   Info: the panes,     |
 |   panes you open, stacked here             |   and the one you      |
 |                                            |   inspect              |
 |                                            |                        |
 +--------------------------------------------+------------------------+
 | notice: what just happened, or a refusal                            |
 | legend: what your keys mean here                                    |
 +---------------------------------------------------------------------+
```

The **room** is the whole screen. Panes are drawn **over** it — none of them sits beside it —
and what shows through where no pane is, is the **desktop's floor**: a few lines saying where
things are, including any tool that is not in this Workshop. Panes cover the room you are
working in. That is uncomfortable on purpose: inventing a docking system before anybody had
felt the discomfort would be answering a demand nobody had made. A wider terminal splits the
surplus evenly between a pane and the room underneath it, so the room always keeps half.

**Info** is a loadable weave that arrives with its own artifact, not something Workshop
compiles in. Your desk names it, so it is there on a first run, in a fixed place at the right
edge. Every pane in the shipped plan is **optional**: if your build tree is missing
`zengine-info-pane.so`, Workshop still starts, prints which artifact it could not load, lists
Info as `[gone]` in the Pane Manager, names it on Workshop's own condition row and keeps the fact
in Attention for the whole run. (While the run is still loading a tool, its row reads `[load]`:
on its way, not missing.) Build
it and launch again. (The host's own infrastructure — the operators, the session history, the
skin, the input weave and the timer — is required, and a plan missing one of those does not
start.)

The **notice** line is where a refusal to what you just did appears. Workshop refuses rather
than clamps: a place or size Info cannot accept leaves the stored one untouched and says why.
It is a report about the gesture you made, and the next thing Workshop says replaces it.

Something that is **still true** when you read it — a settings file that could not be read, a
pane of yours that is off the screen, a tool that could not load — does not go there. It goes
to attention: one compact line where the medium can always show it (a box in the corner of the
window, the second reserved row of a terminal), and the **Attention** pane lists them all —
open it from the Pane Manager. Those disappear when they stop being true and at no other
moment. See [what needs your attention](attention.md).

The **first row** is a pane — [the layout selector](setups.md#several-layouts-in-one-workshop),
called **Layouts**: your layouts as tabs on the left with the one you are on between `>` and
`<`, a `+` that makes another, and — at the row's right-hand edge — what the layout you are on
is related to: `setup: none`, or the Setup file it was saved to or restored from and whether it
still matches it. Being a pane means you can move it, resize it, cover it or close it, like
Files or the Editor. The **bottom band** is what Workshop just said and what your keys mean
right now: the notice, then the legend rows advertising the current context's gestures. Every
one of those hints is a projection of the effective keymap — remap a binding and the screen
spells the new one — and `Ctrl`+`k` opens the **Hotkeys** pane, the complete list of every key
as it is in force. So the keys are on screen; this page is the fuller version, not the only
source. See [hotkeys and the keymap](hotkeys.md).

## Your first five minutes

**A fresh Workshop opens onto your desk**: the Layouts row, Info at the right, and the room.
The Layouts row says `>Default< +` beside `setup: none` — the one layout you have, marked as the
one you are on, related to no Setup file yet. `none` is not a warning: Workshop remembers that
layout for you either way.

1. **`Ctrl`+`p`** — the **Pane Manager**: every pane there is, open or not. `↑` `↓` choose,
   `Enter` opens one and puts you in it — a pane that is already open is simply where you land.
   **`x`** closes one: it leaves the layout, and its tool keeps running and keeps what it holds,
   so `Enter` brings it back as it was. `Ctrl`+`p` works while you are typing in another pane.
   See [panes](panes.md#opening-going-to-and-closing--the-pane-manager).
2. **`Ctrl`+`t`** — open the Terminal, or go to it if it is already up. `Ctrl`+`p`, `Ctrl`+`t`
   and `Ctrl`+`k` come from the **desktop**, a tool you can edit and rebuild like any other, so
   you can move them, switch them off, or change what they do — see
   [hotkeys](hotkeys.md#keys-the-application-supplies--and-how-to-take-them-away).
3. **Press into Info**, then **`↑` `↓`** to choose a pane and **`Enter`** to inspect it. Its
   rows say what it is, what you authored for it (`X` `Y` `Width` `Height`, each `-` until you
   say something) and what this screen makes of that. **`Tab`** moves you to those rows;
   **`Enter`** on `X` `Y` `Width` or `Height` opens a draft. Type a whole number in the face's
   unit (cells in a terminal, pixels in a window), then `Enter` to write it or `Esc` to
   abandon; `-` gives the axis back to Workshop's default. A pane's keys reach it only while it
   holds the keyboard — press somewhere else and `↑` `↓` are Workshop's again.
   - Try an illegal value. The refusal names what is wrong, your text stays in the draft, and
     the pane does not move.
   - **The subject stays until you choose another.** Pressing another pane, `Esc`, a press on
     the room — none of them moves what Info is inspecting. Info may inspect itself.
   - **You will not see a caret while you type a value.** A pane sends finished rows and a
     caret is not a row, so the value scrolls to where you are typing and the insertion point
     itself does not cross. It is a real cost of Info being a loaded pane, and it is the
     contract the Editor's own migration fixed: the Editor pane publishes its caret and its
     selection beside its rows, and Workshop draws them into the pane.
   - **One write at a time.** `Enter` sends the value; `Enter` again before Workshop answers
     sends nothing (`commit not sent`), and your text waits for the next `Enter`. If the layout
     underneath changed before the write arrived, it is refused rather than landing on the
     wrong pane, and nothing is written. A write that never reached Workshop says so
     (`commit not submitted`, `commit not delivered`); your draft and its text stay, and
     `Enter` sends them again.
4. **`w`** — arrange the desk: every pane wears handles. Drag a body to move it, an edge or
   corner to resize it; `Tab` steps between panes, the arrows move one cell, `Shift`+arrows
   resize, and **`=`** grows the pane four cells at once. `Esc` leaves. See
   [arranging](panes.md#moving-resizing-and-ordering--arrange).
5. **Right-click** a pane, a layout tab or the empty room — Workshop lists what can be done
   with the thing you pointed at, without selecting it. `Enter` chooses, `Esc` closes, and
   **`a`** opens the room's menu from the keyboard. See
   [the context menu](panes.md#the-context-menu--what-can-i-do-with-this).
6. **`s`** — save the layout you are on to the setup file; **`r`** restores it. **`=`** makes
   a new layout, **`.`** and **`,`** step between them. See [setups](setups.md).
7. **`q`** or **`Ctrl`+`c`** — quit.

> **Your desk comes back.** The panes you had open, where you put them, how big you made them
> and how big the window was are all restored automatically from the last session — no
> keypress. See [workspace continuity](setups.md#workspace-continuity).

## The full key map

[cheat_sheet.md](../../cheat_sheet.md#keys) has it in one place. The short version:

| | |
|---|---|
| anywhere | `Ctrl`+`p` the Pane Manager · `Ctrl`+`t` the Terminal · `Ctrl`+`k` the Hotkeys pane — even inside a pane ([hotkeys](hotkeys.md#keys-the-application-supplies--and-how-to-take-them-away)) |
| the Pane Manager | `↑` `↓` choose · `Enter` open or go to · `x` close · `n` make a pane of your own, `s` save it, `Ctrl`+`d` discard its edits ([Pane Creator](panes.md#the-pane-creator--a-pane-made-of-data)) |
| panes | `w` arrange desk · `t` pane titles · `Esc` put the selected pane down |
| anything | right-click, or `a` — what can I do with this ([context menu](panes.md#the-context-menu--what-can-i-do-with-this)) |
| layouts | `s` save · `r` restore · `=` new · `.` `,` next / previous · `Ctrl`+`w` remove ([setups](setups.md)) — the **last** session needs neither |
| quit | `q` · `Ctrl`+`c` where nothing takes text |
| in a pane | its own rows, while it holds the keyboard — press into it first: **Info**'s `↑` `↓` `Enter` `Tab`, the [Builder](builder.md)'s `b` `c` `f` `o`, the [browser](files.md)'s `u` `m` `a`, [Attention](attention.md)'s `↑` `↓` `d`, the [Terminal](terminal.md)'s `Enter` `Tab` `↑` `↓` `Esc` and `Ctrl`+`↑` `Ctrl`+`↓` `Ctrl`+`Home` `Ctrl`+`End` |
| leaving a pane | `Esc` where the pane has nothing of its own left to do with it — the [Terminal](terminal.md) after its list and its line — or a press elsewhere. Both editors keep every `Esc`; a press elsewhere is the way out of those |

These are the defaults; every binding can be remapped through the keymap file, and the
on-screen hints and the Hotkeys pane always spell the effective one ([hotkeys](hotkeys.md)).
Keys are read from the Input weave like any other Zengine application's — Workshop holds no
privilege the [snake example](../reference/snake.md) does not.

## Terminal and window differences

| | terminal | SDL window |
|---|---|---|
| how much room | the real terminal size, minus 3 reserved rows; a redirected or captured run gets the 78x22 minimum | the window's own size, and the person may resize it |
| text in a bounded region | one row per cell row, cut at the region's width | set in a real typeface at its own advance and line height |
| labels | the terminal's own font | an embedded 6x6 bitmap face, printable ASCII only; any other byte draws a visible unknown box |
| colour | an SGR *and a glyph* per role, so a monochrome terminal is not lied to | RGB per role |
| the title | two reserved slot lines | the window title carries the slot lines |
| what needs attention | the second reserved line | a compact box in the picture's top-right corner, and the title |

Neither is a degraded version of the other, and the same published intent produces both.

## Next

- [Attention](attention.md) — what is true right now, the pane that lists it, and why hiding
  one is not fixing it.
- [Panes](panes.md) — the Pane Manager, arranging, how to get a bigger one, Info as a pane's
  inspector, and the Pane Creator that makes one of your own out of data.
- [Setups](setups.md) — saving an arrangement, and what does not come back.
- [Load plans](load-plans.md) — choosing what a run is made of.
- [Builder](builder.md) — what it builds today.
- [The source editor](editor.md) — edit the file a recipe names, without leaving.
- [Edit a running pane](edit-a-running-pane.md), and [develop Workshop](develop-workshop.md) —
  change a pane while it runs: one of your own, or one Workshop ships, the desktop included.
- [Introspection](../reference/introspection.md) — `Loaded`, `Project`, `Powers`.
- [Limitations](limitations.md) — read this before planning around Workshop.
- [Making a Workshop tool](../guides/make-a-workshop-tool.md) — if you want to add a pane.
