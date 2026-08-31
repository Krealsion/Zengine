# Setups and workspace continuity

**How-to, plus an explicit current-state verdict.** How Workshop persists an arrangement, how
you get it back, and what does not come back on its own.

## The three files

Workshop keeps three separate persisted things, on purpose, in three files:

| file | argument | holds |
|---|---|---|
| the **document** | `--document`, default `workshop.json` | the authored objects: identities, labels, placements, extents |
| the **setup** | `--setup`, default `workshop-setup.json` | a desk you deliberately **named**: which panes, where, how big, in what order |
| the **last session** | `--session`, default `workshop-session.json` | the desk you were actually using when you left, plus how much room the surface had |

They are separate because they answer different questions. The document is the thing you are
making. The setup is a room you chose to keep and gave a name. The last session is the room you
happened to be in — written when Workshop leaves, read when it arrives, by nobody's gesture.

Sharing a document should not import somebody else's pane layout, and closing a window should
not rewrite a desk you saved under a name.

## Saving a setup

Press **`s`**. A one-line editor opens over the current setup's name.

| key | does |
|---|---|
| type | ordinary single-line editing, with a caret and a scrolling window |
| `Enter` | commit: validate, then write the file |
| `Esc` | cancel; the name is unchanged and nothing is written |

The name meets the same validator a file's name meets. A refusal leaves the editor open with
your text still in it, so you fix what you typed rather than retyping it. Nothing is written
and the active setup's name does not move until the whole thing is legal **and** the file has
been replaced — so a failed write leaves the last good file intact and the live arrangement
untouched.

On success the status line says what was saved, where, and — if any pane in it is a reference
this build cannot present — how many are unresolved and names the first one.

## Restoring a setup

Press **`r`**. It reads the file named by `--setup` and applies it.

This is a transaction, structurally: the loader *returns* a candidate rather than writing into
anything, so there is no path by which a pane closes before a bad field near the end of the
file has been met. A refusal costs you the notice and nothing else — the active setup, the open
panes, the Builder pane's status and the document are all exactly as they were.

**An unresolved reference is not a failure.** A pane this build has never heard of loads, stays
in the setup, is counted, is named, and is saved again unchanged. Workshop knows one thing
there: it has no catalog row for that reference. It does not know whether the presenter exists,
is loading, was unloaded, or was never installed — and silence is not evidence of absence. So
the word is *unresolved*, never *unavailable*.

## Several layouts in one Workshop

**A layout is a desk arrangement — the same thing a setup file holds.** One running Workshop
keeps several of them and you move between them from inside it, without opening a second
Workshop, a second window, a second project or a second copy of any pane.

**The first row of Workshop is the layout selector** — a run of tabs on the left, with the one
you are on between `>` and `<`, and the setup's own status to their right:

```text
 Code >Build< Inspect | UNSAVED | workshop-setup.json | s name/save  r restore
```

Names are shown exactly as you typed them. Every tab keeps one cell on each side of its name,
so the marker is the same width as the blank it replaces — switching layouts never shifts the
tabs beside it or the status to their right — and a name with a space in it is still one tab,
because the gap *between* two tabs is two cells and a space *inside* a name is one:

```text
 Home >My Layout< Art
```

| key | does |
|---|---|
| `.` | the next layout |
| `,` | the previous one |
| `=` | a new layout — a **copy** of the one you are on, added at the end |
| `Ctrl`+`w` | remove the layout you are on |
| press a tab | go to that layout |

Switching changes **which arrangement you are looking at** and nothing underneath it. The same
panes, the same tools, the same open source file, the same place in the file browser, the same
marks, the same recipes, the same window. A pane that is in two layouts is **one pane** — one
provider, one tool, one lot of state — shown in both.

Each layout keeps what you authored in it: which panes participate, where each one is, how big,
and which is in front. Switch away and back and it is exactly as you left it.

Three plain bounds:

- **A layout is always there.** Removing the last one is refused; removing the one you are on
  puts you on its neighbour and keeps the order of the rest.
- **One run holds at most eight.** Adding a ninth is refused rather than dropping one.
- **Names may repeat**, and a name is not a selector: position is what a tab is.

When there are more layouts than the row can show, the run shows a contiguous window around the
one you are on and counts the rest — `<2` on the left, `3>` on the right. `.` and `,` walk
**every** layout, including the ones the row could not paint, and the painted run follows you.

### ...and the setup file still holds exactly one

The two gestures act on the layout you are on and on nothing else:

- **`s`** names and writes **the current layout** to the `--setup` file.
- **`r`** reads that file back into **the current layout**.

Neither one touches your other layouts. `UNSAVED` means what it always meant: the layout you are
on differs from what is in the setup file.

**Additional layouts are available within the current run; only the active desk is restored
after a restart today.** Saving and restoring the whole set of layouts is planned work, not an
experiment — see [what does not come back yet](limitations.md#only-the-layout-you-are-on-comes-back-after-a-restart).

To keep an arrangement past a restart today, stand on it and press `s`. To have several on disk,
keep several files and pass the one you want:

```sh
zengine-workshop --setup layouts/wide.json
zengine-workshop --setup layouts/inspect.json
```

## The last session

**You do not save this and you do not load it.** When Workshop closes normally — `q`, `Ctrl`+`c`
or the window's close box, all one door — it writes the desk you were on and the room you were
in to the `--session` file. When it starts, it reads that file back.

```text
launch  ->  the surface says hello
        ->  Workshop paints once, at its floor size
        ->  the last session is read
        ->  the room is restored
        ->  the desk is rebuilt INTO that room
        ->  work
```

What comes back:

| | |
|---|---|
| which panes were open | yes |
| where each was placed, and how big | yes — the authored values, exactly as saved |
| which pane was in front | yes |
| the size of the Workshop window | yes, **to the nearest whole cell** — see below |
| the window's screen position | **yes**, on a graphical run — validated against the monitors that exist now; see [limitations](limitations.md#the-window-comes-back-where-you-left-it-into-the-desktop-that-exists-now) |
| whether it was maximized | **yes**, beside the *normal* size and place unmaximizing returns to |
| your document | **no** — `Ctrl`+`o` still opens it |

The status line says what happened, in the notice row:

```text
reopened your last desk "Debugging" -- 120x44 cells
```

### Why the window size is in cells

Workshop does not own its window. Whichever Skin holds `zengine.skin` does, and the only thing
that Skin ever tells Workshop about the room is how many **canvas cells** it has. So that is
what is written down, and a graphical medium turns cells back into pixels on the way out. The
honest cost is a bound rather than a hope: **you get back the size you chose, floored to whole
cells** — at most eleven pixels short on each axis.

### When it cannot be honoured

Four different things can happen, and they are four different sentences:

| situation | what Workshop does |
|---|---|
| there is no session file yet | opens with the defaults, and says **nothing** — a first launch is not an error |
| the file exists and cannot be read or understood | says why, names the reason, opens with the default setup, and **leaves your file exactly as it is** |
| the file is fine but the saved size is not one this Workshop opens at | restores the **desk**, opens at the default size, and names the value it declined |
| a pane in it is a reference this build cannot present | restores everything else, keeps the reference, and names the first unresolved one |

A saved size is honoured only if it is inside the band Workshop is honest at — 78x22 to
640x400 cells. Outside it the size is **declined, not clamped**: clamping a nonsense width into
the band would still open a window nobody chose, on a display Workshop cannot see.

**Crash recovery is not claimed.** The session is written on an orderly close. A Workshop that
is killed loses the session it was in, and the previous file is still there.

## Last session versus named setup

They are deliberately different promises, and keeping them apart is why there are two files:

| | named setup | last session |
|---|---|---|
| written by | you, with `s` | Workshop, on close |
| read by | you, with `r` | Workshop, on start |
| has a name you chose | yes | it carries whatever name the desk had |
| holds the window size | no | yes |
| how many | one file per run, chosen by `--setup`; keep as many files as you like | one |

An automatic save never touches the file you named, in either direction. Closing Workshop
writes a session and leaves `workshop-setup.json` byte-for-byte alone; taking a session back
reads no setup file at all.

## Workspace continuity

> **Can a maker reopen Workshop and return to useful work without reconstructing their panes
> manually?**
>
> **Yes, and without pressing anything.** The panes, their geometry, their order and the window
> size come back on their own. **The document does not** — `Ctrl`+`o` is still a gesture.

Source-traced, precisely:

| | exists? | how |
|---|---|---|
| saving a layout / setup | **yes** | `s`, writes the `--setup` file — the layout you are on |
| loading / restoring it | **yes** | `r`, reads the `--setup` file into the layout you are on |
| keeping several layouts at once | **yes**, within the run | tabs on Workshop's first row; `.` `,` `=` `Ctrl`+`w` |
| selecting among setup **files** | **no in-application selection** | one file per run, chosen by `--setup` |
| persisting pane position | **yes** | authored place, in the setup and in the session |
| persisting pane size | **yes** | authored size in cells or pixels, in both |
| persisting which panes are open | **yes** | the pane list, in both |
| persisting pane order (depth) | **yes** | the rank permutation, in both |
| persisting the window's size | **yes** | the session's viewport, in cells — the *normal* window's room |
| persisting the window's position and maximized state | **yes** | remembered opaquely from the medium's own reports; the medium validates them against live displays at restore |
| **restoring the desk and the room at launch** | **yes** | automatic, from the `--session` file |
| **restoring the document at launch** | **no** | the document loader is reached only from `Ctrl`+`o` |

So the actual workflow every session is now:

```text
launch  ->  Ctrl+o   (get the document back)
        ->  work
        ->  Ctrl+s   (document)
```

**Why the document is still a gesture.** It is a different kind of fact: a setup is the room and
a document is the work, and opening the last document a maker touched is a stronger claim than
opening the last room they were in — it decides what they are editing, and a wrong guess
overwrites nothing but looks exactly like their file. What makes forgetting `Ctrl`+`o` quiet is
unchanged and worth knowing: a fresh Workshop seeds two example objects, so you get a
plausible-looking document that is not yours rather than an obviously empty one.

## The document

`Ctrl`+`s` saves, `Ctrl`+`o` opens. The document loader is a transaction for the same reason
the setup's is: a bad field near the end of the file must not leave you with half a document.
Loading a document brings back **its** identities — the objects you saved are the objects you
get, with the same identities the inspector and the canvas were using.

The title row shows the document path and whether it is `saved` or `UNSAVED`, compared rather
than flagged — a copy of the last-saved state, so the indicator cannot drift.
