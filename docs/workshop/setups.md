# Setups and workspace continuity

**How-to, plus an explicit current-state verdict.** How Workshop persists an arrangement, how
you get it back, and what does not come back on its own.

## The two files

Workshop keeps two separate persisted things, on purpose, in two files:

| file | argument | holds |
|---|---|---|
| the **document** | `--document`, default `workshop.json` | the authored objects: identities, labels, placements, extents |
| the **setup** | `--setup`, default `workshop-setup.json` | the arrangement: which panes are open, where, how big, in what order — and a **name** |

They are separate because they answer different questions. The document is the thing you are
making. The setup is the room you are making it in. Sharing a document should not import
somebody else's pane layout.

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

## Selecting among setups

**There is one setup file per run, chosen by `--setup` on the command line.** There is no
in-application list of setups, no recent-setups menu, and no profile manager. To keep more than
one arrangement, keep more than one file and pass the one you want:

```sh
zengine-workshop --setup layouts/wide.json
zengine-workshop --setup layouts/inspect.json
```

The `name` inside a setup is a label a maker reads, not a selector.

## Workspace continuity

> **Can a maker reopen Workshop and return to useful work without reconstructing their panes
> manually?**
>
> **Almost — but not without knowing two keys, and not automatically.** Persistence is real and
> complete in both directions. **Restoration is manual.** Nothing is read at launch.

Source-traced, precisely:

| | exists? | how |
|---|---|---|
| saving a layout / setup | **yes** | `s`, writes the `--setup` file |
| loading / restoring it | **yes** | `r`, reads the `--setup` file |
| selecting among setups | **no in-application selection** | one file per run, chosen by `--setup` |
| persisting pane position | **yes** | authored place, in the setup |
| persisting pane size | **yes** | authored size in cells or pixels, in the setup |
| persisting which panes are open | **yes** | the setup's pane list |
| persisting pane order (depth) | **yes** | the setup's rank permutation |
| **restoring any of it at launch** | **no** | the setup loader is reached only from the `r` key |
| **restoring the document at launch** | **no** | the document loader is reached only from `Ctrl`+`o` |

So the actual workflow every session is:

```text
launch  ->  Ctrl+o   (get the document back)
        ->  r        (get the panes back)
        ->  work
        ->  Ctrl+s   (document)
        ->  s Enter  (setup)
```

Two keypresses is not much. **The cost is not the keypresses — it is that neither is
discoverable and neither happens by default.** A maker who does not know `r` exists concludes
that Workshop does not persist layouts at all, having saved one successfully. That is the gap:
not a missing mechanism, a missing default.

### The smallest missing seam

Everything needed already exists and is already a transaction. The gap is one decision nobody
has made: **should a host read its authored document and setup at startup, and what does it do
when either file is absent or refused?**

Concretely, that is (a) calling the same two loaders once during startup, (b) a rule for
"absent file" that is distinct from "malformed file" — an empty path is already refused by
name, and a first run has no file at all — and (c) whether a `--no-restore` escape is needed
for a maker whose setup names a pane that crashes.

None of it is designed here, and none of it is built. It is recorded as the highest-ranked
Workshop usability seam.

## The document

`Ctrl`+`s` saves, `Ctrl`+`o` opens. The document loader is a transaction for the same reason
the setup's is: a bad field near the end of the file must not leave you with half a document.
Loading a document brings back **its** identities — the objects you saved are the objects you
get, with the same identities the inspector and the canvas were using.

The title row shows the document path and whether it is `saved` or `UNSAVED`, compared rather
than flagged — a copy of the last-saved state, so the indicator cannot drift.
