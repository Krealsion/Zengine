# Setups and workspace continuity

**How-to, plus an explicit current-state verdict.** How Workshop persists an arrangement, how
you get it back, and what does not come back on its own.

## The three files

Workshop keeps its persisted things apart, on purpose, in separate files; the ones about
what you are looking at are these:

| file | argument | holds |
|---|---|---|
| the **document** | `--document`, default `workshop.json` | the authored objects: identities, labels, placements, extents |
| the **setup** | `--setup`, default `workshop-setup.json` | one desk you deliberately kept: which panes, where, how big, in what order |
| the **last session** | `--session`, default `workshop-session.json` | the desk you were actually using when you left, plus how much room the surface had |
| a **pane you made** | `--pane`, default `workshop-pane.json` | what is *inside* a pane the [Pane Creator](panes.md#the-pane-creator--a-pane-made-of-data) made: its name and its regions — never where it sits, which is the desk's |

They are separate because they answer different questions. The document is the thing you are
making. The setup is a room you chose to keep and gave a name. The last session is the room you
happened to be in — written when Workshop leaves, read when it arrives, by nobody's gesture. A
pane you made is a thing you built; a desk only says where it participates.

Sharing a document should not import somebody else's pane layout, and closing a window should
not rewrite a desk you saved under a name.

## Saving a setup

Press **`s`**. It writes the layout you are on to its Setup file. That is all it does — it does
not ask you for a name, and it does not touch any of your other layouts.

**Which file?** The one this layout is associated with, if it has one. If it does not, the file
named by `--setup` — and a successful write makes that its association from then on. (Renaming
a layout is a separate gesture; see *Renaming a layout* below.)

A failed write leaves the last good file intact, the live arrangement untouched, and the layout
related to exactly what it was related to before. Nothing about the relationship moves until the
file has actually been replaced.

On success the status line says what was saved, where, and — if any pane in it is a reference
this build cannot present — how many are unresolved and names the first one.

## Restoring a setup

Press **`r`**. It reads the layout's Setup file — its own, if it has one, otherwise the file
named by `--setup` — and applies it to **the layout you are on**. A successful read makes that
file this layout's association, exactly as a successful write does.

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

**The layout selector is a pane called Layouts**, and by default it is the first row of
Workshop — a run of tabs on the left, with the one you are on between `>` and `<`, a `+` at the
end of the run, and the active layout's Setup status at the right-hand edge:

```text
 Code >Build< Inspect +          setup: workshop-setup.json | modified | s save  r restore
```

Names are shown exactly as you typed them. Every tab keeps one cell on each side of its name,
so the marker is the same width as the blank it replaces — switching layouts never shifts the
tabs beside it or the status to their right — and a name with a space in it is still one tab,
because the gap *between* two tabs is two cells and a space *inside* a name is one:

```text
 Home >My Layout< Art +
```

It is an ordinary pane. It is in the picker (`p`) under **Layouts**, you can move and resize it
in the desk arrangement (`w`) like any other, a pane you put in front of it covers it and takes
your clicks there, and each layout keeps its own answer about where it is and whether it is on
the desk at all. Take it off a layout and that layout simply has no tab run in it; the keys
below still reach every layout, and the picker puts it back where it started.

**Removing it does not give the space to your document.** The two rows at the top of Workshop
are reserved whether or not anything stands on them — exactly as the column beside your
workspace stays reserved when you remove Info — so a `%`-sized object is the size it was
whatever you do with the selector. Moving it away leaves those rows empty on purpose.

| gesture | does |
|---|---|
| `.` | the next layout |
| `,` | the previous one |
| `=`, or press `+` | a new, **empty** layout, added at the end |
| `Ctrl`+`w` | remove the layout you are on |
| press a tab | go to that layout |
| **double-click a tab** | rename that layout |
| **drag a tab** | move it along the run |
| **right-click a tab** | rename, duplicate, move left or right, close |

The `+` is a button, not a layout: it is not in the run, `.` and `,` never land on it, and a
row too narrow to hold it simply does not paint one — the key still works.

Switching changes **which arrangement you are looking at** and nothing underneath it. The same
panes, the same tools, the same open source file, the same place in the file browser, the same
marks, the same recipes, the same window. A pane that is in two layouts is **one pane** — one
provider, one tool, one lot of state — shown in both.

Each layout keeps what you authored in it: which panes participate, where each one is, how big,
and which is in front. Switch away and back and it is exactly as you left it.

Three plain bounds:

- **A layout is always there.** Removing the last one is refused; closing the one you are on
  puts you on its neighbour and keeps the order of the rest, and closing one you are *not* on
  leaves the desk you are looking at exactly where it was.
- **One run holds at most eight.** Adding a ninth — or duplicating into a ninth — is refused
  rather than dropping one.
- **Names may repeat**, and a name is not a selector: position is what a tab is.

When there are more layouts than the row can show, the run shows a contiguous window around the
one you are on and counts the rest — `<2` on the left, `3>` on the right. `.` and `,` walk
**every** layout, including the ones the row could not paint, and the painted run follows you.

### Renaming a layout

**Double-click its tab**, or choose *rename layout* from the tab's right-click menu. Type,
`Enter` commits, `Esc` cancels. The name meets the same validator a file's name meets, and a
refusal leaves the editor open with your text still in it so you fix what you typed rather than
retyping it.

**Renaming writes no file.** It is a change to the layout, and the session remembers it like
every other layout fact. If the layout is associated with a Setup file, renaming it makes it
`modified` against that file — because the name is part of what a desk *is* — and `s` is what
writes the new name out.

### Duplicating a layout

Right-click a tab and choose *duplicate layout*. The copy lands directly after its source, gets
the same name (names may repeat; position is what a tab is), and becomes the one you are on.

**A duplicate is never associated with anything.** The desk is copied; the relationship to a
Setup file is not — otherwise the copy would claim a file it had never been written to, and the
first `s` would overwrite the very file you duplicated in order to leave alone.

### Reordering layouts

**Drag a tab** along the run, or use *move layout left* / *move layout right* from its
right-click menu. Only the order changes: the same desk stays live, each layout keeps whatever
Setup file it was related to, and the new order comes back after a restart.

### What `setup:` on the first row means

The right-hand end of the first row is about **the layout you are standing on**, and it says one
of three things:

| it says | it means |
|---|---|
| `setup: none` | this layout is not associated with any Setup file |
| `setup: <file> \| current` | this desk **is** the last value Workshop knew that file to hold |
| `setup: <file> \| modified` | it is associated with that file, and has since diverged from it |

**`none` does not mean unsaved.** Workshop remembers every layout for you, automatically, in the
session — `none` only says that you have not related this desk to a standalone Setup file.
Plenty of layouts never need one.

**`current` is about what Workshop knows, not about the disk right now.** It means *this desk
equals the last value this Workshop successfully wrote to, or read from, that file*. Workshop
does not watch the file, does not re-read it, and does not poll it — so if another program edits
it behind Workshop's back, you find out the next time you press `s` or `r`, and not before.

A layout becomes associated with a file exactly when a `s` or an `r` **succeeds**. A refused
write or an unreadable file changes nothing at all.

**Two layouts may share one file**, and that stays honest: when one of them writes it, the other
stops saying `current` and starts saying `modified`, because what the file holds is no longer
what that layout has.

### ...and the setup file still holds exactly one

The two file gestures act on the layout you are on and on nothing else:

- **`s`** writes **the current layout's desk** to its Setup file.
- **`r`** reads that file back into **the current layout**.

Neither one touches your other layouts, and neither one is a rename.

**All of your layouts come back.** Closing Workshop writes the whole run — every layout, in
your order, under its own name, with the one you were standing on marked as the live one — and
the next launch gives them all back. That is the *session's* job, not the setup file's; the two
promises are still separate, and the table below says which is which.

The setup file is still exactly one desk. To keep an arrangement under a name of its own, stand
on it and press `s`. To have several on disk, keep several files and pass the one you want:

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
| your layouts | **yes** — all of them, in your order, under their own names |
| which layout you were on | **yes** — the same one is live again |
| which Setup file each is associated with | **yes** — and whether it still matches it |
| which panes were open | yes — per layout |
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

...and when more than one layout came back, it says which one of how many you are standing on:

```text
reopened your last desk "Code" (2 of 3 layouts) -- 120x44 cells
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
| the file exists and cannot be read or understood | says why, names the reason, opens with the default setup, and **leaves your file exactly as it is — including on the way out** |
| the file was written by an **older** Workshop | reads it through a conversion, if this arrangement has one — see below |
| the file claims **this** version but says something impossible — no layouts, more layouts than a run holds, an active position that is not one of them, a desk that is not a legal setup | refuses it as the current-version file it claims to be, names the fact that is wrong, and **does not go looking for a conversion** |
| the file is fine but the saved size is not one this Workshop opens at | restores the **desk**, opens at the default size, and names the value it declined |
| a pane in it is a reference this build cannot present | restores everything else, keeps the reference, and names the first unresolved one |

A saved size is honoured only if it is inside the band Workshop is honest at — 78x22 to
640x400 cells. Outside it the size is **declined, not clamped**: clamping a nonsense width into
the band would still open a window nobody chose, on a display Workshop cannot see.

**Crash recovery is not claimed.** The session is written on an orderly close. A Workshop that
is killed loses the session it was in, and the previous file is still there.

### A session written by an older Workshop

Workshop's session reader understands exactly one shape: the one this build writes. It does not
carry a copy of every shape it has ever written, and that is deliberate — a reader that grows a
road per vintage grows forever.

What reads an older file is a **conversion**, supplied by an ordinary artifact your arrangement
loads (`zengine-workshop-session-history`, named in the shipped load plans beside every other
artifact). Workshop's session reader looks for one, spends it, and then puts the result through
its own ordinary checks — the same ones a file written five minutes ago goes through. So an old
file is never admitted on easier terms than a new one.

A session from before layouts came back opens as **exactly one layout** holding exactly the desk
it always held. A session from before Setup associations opens with every one of its layouts
related to **nothing**. Both are the honest reading: those files could not say how many layouts
you had, or which file a desk came from, so Workshop does not invent an answer -- and in
particular it does not quietly decide that your desks came from whatever `--setup` names.

Three consequences worth knowing:

- **An old file cannot make anything load.** Its version is a *lookup key*: it can pick among the
  conversions your arrangement already has, and it can do nothing else. If the conversion is not
  there, Workshop says so — it does not go looking for one, and nothing on your disk is opened
  because a file asked for it.
- **Reading never rewrites.** A converted session opens in memory and your file is untouched.
  The next ordinary close writes the current shape, on the same rule that has always written it —
  and from then on that session needs no conversion at all.
- **Removing the conversion artifact is a real decision.** Delete its row from your load plan and
  a current session still opens exactly as before; a session from an older Workshop is refused,
  by number, naming the conversion that is missing:

```text
session version 3 cannot be read: no live conversion from `WorkshopSession` v3 to v5
(`zengine.migrate.WorkshopSession.v3-to-v5`) -- opening with the default setup
```

  **and that run keeps no session at all** — closing it writes nothing, so your file is still
  there, unchanged, when you put the row back. Workshop says so while it is standing:
  *session refused — this run keeps no session*.

## Last session versus named setup

They are deliberately different promises, and keeping them apart is why there are two files:

| | named setup | last session |
|---|---|---|
| written by | you, with `s` | Workshop, on close |
| read by | you, with `r` | Workshop, on start |
| has a name you chose | yes | it carries whatever name the desk had |
| holds the window size | no | yes |
| how many desks | exactly one | all the layouts you were using, in order, with the live one marked |
| holds which file a desk is related to | no -- a Setup file never names another file | yes, per layout |
| how many files | one per run, chosen by `--setup`; keep as many files as you like | one |

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
| saving a layout / setup | **yes** | `s`, writes the layout you are on to its Setup file |
| renaming a layout without writing a file | **yes** | double-click its tab, or its right-click menu |
| duplicating a layout | **yes** | its right-click menu; the copy is related to no file |
| reordering layouts | **yes** | drag a tab, or *move layout left* / *right* |
| keeping all your layouts across a restart | **yes** | automatic, in the session file |
| loading / restoring it | **yes** | `r`, reads that Setup file into the layout you are on |
| keeping several layouts at once | **yes**, within the run | tabs on Workshop's first row; `.` `,` `=` `Ctrl`+`w`, and the tab gestures |
| knowing whether a layout matches its Setup file | **yes** | `setup:` on the first row: none, current or modified |
| selecting among setup **files** | **no in-application selection** | one file per run, chosen by `--setup`; a layout keeps whichever it was related to |
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
