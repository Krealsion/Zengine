# Getting started with Workshop

Workshop is an interactive environment for making things, built with Zengine. It is optional:
the [C++ library](../getting-started.md) is usable without it, and nothing here is required to
write a weave.

**What Workshop is today:** a working surface where you create authored rectangles, select
them, move and resize them, inspect and edit their properties, watch invalid edits be refused,
save the result, and reopen it. Around that sit panes that show what the running system is
made of, a terminal overlay that talks to the live process, and a Builder that starts a real
build.

**What it is not yet:** a source editor, a project builder, or something that reopens where
you left off. Those are written down in [limitations](limitations.md) rather than left to be
discovered.

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
| `--document <path>` | `workshop.json`, beside the binary | the authored objects you are working on |
| `--setup <path>` | `workshop-setup.json`, beside the binary | the pane arrangement |
| `--load-plan <path>` | `default-load-plan.json`, beside the binary | which artifacts this run is made of |
| `--log <path>` | none | a durable journal of selected facts, appended as they happen; outlives the process |
| `--dump <path>` | none | what the volatile recorder still held when Workshop quit |

An empty path is refused by name. `--log` and `--dump` are two different questions: the
journal is what you keep on purpose, the dump is most of a session's story recovered after the
fact. Without either, nothing is written and `q` always leaves a live process.

### What it prints before it draws

Workshop prints a few lines of plain scrollback on the way up, and each is a fact you are
entitled to before you press anything:

- **what this host isolates** — `Kernel::containment_note()`, verbatim. An in-process Zengine
  host isolates nothing; read the sentence literally.
- **the executed load plan** — which artifacts were mounted and loaded, into which roles.
- **what the Builder will run** — the actual program and arguments, because a panel cannot show
  you a command before the runner has started it.

## The screen

```text
 +--------------------------------------------+------------------------+
 | title                                      |                        |
 +--------------------------------------------+   the object list      |
 |                                            |   and the inspector    |
 |   the workspace: your authored objects      |                        |
 |                                            |                        |
 |   panes stack here, over the material       |                        |
 |                                            |                        |
 +--------------------------------------------+------------------------+
 | status: what just happened, or a refusal                            |
 +---------------------------------------------------------------------+
```

The side panel is a fixed 28 columns. Everything left of it is the workspace, and panes are
drawn **over** it — they cover the material you are building. That is uncomfortable on purpose:
inventing a docking system before anybody had felt the discomfort would be answering a demand
nobody had made. A wider terminal splits the surplus evenly between a pane and the material
underneath it, so the maker always keeps half.

The status line is where every refusal appears. Workshop refuses rather than clamps: an
illegal extent leaves both stored coordinates untouched and says why.

## Your first five minutes

1. **`n`** — create an object. It gets a fresh identity that is neither its label nor its
   index, and is never handed out twice.
2. **`Tab`** — select the next object. `h` `j` `k` `l` move the selected one by a cell;
   `Shift` with them resizes it.
3. **`↑` `↓`** — move the inspector cursor down the selected object's properties.
   **`Enter`** — edit the one under it. Type, then `Enter` to commit or `Esc` to abandon.
   - `Width` and `Height` are **one** property each — a mode plus an amount, `12` cells or
     `70%` — presented as one row even though two fields are stored.
   - `Resolved` is a separate, read-only row. Narrowing the workspace with `[` moves
     `Resolved` and never touches `Width`. That distinction is the whole point of the
     [ui package](../reference/ui.md).
   - Try an illegal value. The refusal names what is wrong and the property does not move.
4. **`p`** — open the pane picker and look at what this build has. `↑` `↓` choose, `Enter`
   opens or removes, `Esc` cancels. See [panes](panes.md).
5. **`Ctrl`+`s`** — save the document. **`q`** or **`Ctrl`+`c`** — quit. **`Ctrl`+`o`** —
   open it again.

> **You must press `Ctrl`+`o` to get your document back, and `r` to get your panes back.**
> Neither is restored at launch. See [workspace continuity](setups.md#workspace-continuity).

## The full key map

[cheat_sheet.md](../../cheat_sheet.md#keys) has it in one table. The short version:

| | |
|---|---|
| objects | `n` new · `d` delete · `Tab` select · `h j k l` move · `Shift`+`h j k l` resize |
| inspector | `↑` `↓` cursor · `Enter` edit |
| workspace | `[` `]` narrow / widen by 4 cells |
| panes | `p` picker · `w` management ([panes](panes.md)) |
| setups | `s` name and save · `r` restore ([setups](setups.md)) |
| document | `Ctrl`+`s` save · `Ctrl`+`o` open |
| other | `b` build ([builder](builder.md)) · `Shift`+`Space` terminal overlay · `q` / `Ctrl`+`c` quit |

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

Neither is a degraded version of the other, and the same published intent produces both.

## Next

- [Panes](panes.md) — the picker, management, and how to get a bigger one.
- [Setups](setups.md) — saving an arrangement, and what does not come back.
- [Load plans](load-plans.md) — choosing what a run is made of.
- [Builder](builder.md) — what it builds today.
- [Introspection](../reference/introspection.md) — `Loaded`, `Project`, `Powers`.
- [Limitations](limitations.md) — read this before planning around Workshop.
- [Making a Workshop tool](../guides/make-a-workshop-tool.md) — if you want to add a pane.
