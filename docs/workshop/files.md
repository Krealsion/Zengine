# Files

**How-to.** Browsing your filesystem, marking the places you keep coming back to, and opening
a file without going through the Builder first.

## Two different facts: your project, and where you are looking

The project is **the directory you launched Workshop in** — nothing more configurable than
that, and it is printed on the startup banner:

```text
zengine-workshop - project: /home/you/my-thing
```

The same Workshop serves two projects by being launched in two places. There is no project
file, no project setting and no `--project` flag; if you want a different project, start
Workshop there.

**Files starts there and may go anywhere.** Where you are browsing and what your project
*means* are two separate facts, and only the first one moves:

| fact | what it decides | what changes it |
|---|---|---|
| the project | what a relative source path in a build recipe means | only relaunching somewhere else |
| the Files location | what you are looking at right now | browsing, and only for this session |
| what you can read at all | your operating system's answer, not Workshop's | your operating system |

So you can walk above your project, into a sibling checkout, onto another drive, and back —
and nothing about your project has moved. Looking at something is not choosing it.

If the system cannot report a working directory the banner says so, and Files has nowhere to
start rather than guessing somewhere for you. The same is true if it reports one Workshop
cannot write down — a directory named with characters this build cannot carry in a path. You
get the run without a project rather than a crash on the way in, and nothing nearby is
substituted for the directory you were actually standing in. If you have marked places
before, you can still jump to one of them (below) and carry on from there.

## Opening the pane

Files is an ordinary pane. Press **`p`**, choose **Files**, press Return — and the same
gesture removes it again. It moves, resizes, stacks and rides a saved setup exactly like every
other pane (see [Panes](panes.md)).

Where you had browsed to is **not** saved: a new Workshop starts where it was launched. What
is saved is that the pane is on your desk, where it sits, and the places you deliberately
marked. Half-finished browsing is not a preference; a place you asked to keep is.

## Moving around

Press into the pane to point your keys at it, then:

| key | does |
|---|---|
| `↑` `↓` | move the cursor |
| `Return` | **enter** a directory, or **open** a file in the editor |
| `Backspace` | up one directory |
| `r` | look again |
| `u` | **use this file as the current recipe catalog** — see [the Builder](builder.md#choosing-a-recipe-catalog-while-workshop-is-running) |
| `m` | **mark** this directory, or unmark it if it is already marked |
| `n` / `N` | go to the **next** / **previous** marked place |

The mouse works the way the keys do, with one deliberate extra step: **the first press on a
row selects it, and pressing the row that is already selected opens it.** So the press that
points your keys at the pane can never also open a file — which matters most when you have
unsaved edits, because opening a different file is exactly what would be refused.

The wheel moves the cursor through the listing.

Directories are listed first, then files, and each group is sorted by its exact filename
bytes — so the order is the same on every machine, in every language setting.

## Marking places worth returning to

Once Files can go anywhere, "take me back" becomes a thing you need. **`m` marks the directory
you are looking at**, and pressing it again unmarks it. `n` and `N` move forward and backward
through everywhere this Workshop knows about:

- **origin** — where this Workshop was launched. Regenerated every run, and never written down.
- **your marks** — the places you pressed `m` in. These survive restarts.
- **your filesystem roots** — `/` on Linux and macOS; on Windows, the drives this system
  reports right now. They are asked for at the moment you jump, never remembered, and they are
  the drives Windows lists rather than every path that could ever be reached (a network share
  you can type the name of will not be in the list).

The header tells you which of these you are standing in:

```text
Files 3/24  origin  /home/you/my-thing
Files 1/9   marked  /home/you/other-checkout
Files 1/23  root  /
```

**A mark is a place and nothing else.** It does not make a directory part of your project, does
not change what a relative recipe source means, does not give Workshop permission it did not
already have, and does not make anything buildable or trusted. Marking the directory you
launched in is perfectly reasonable — origin dies with the run, your mark does not.

Marks live in their own file (`--marks <path>`, printed on the banner). A mark to somewhere
that is not currently there — an unplugged drive, a checkout you have not made yet — is
**kept**, because "not there today" is not "you did not mean it".

## What it shows you, and what it does not

**Everything that is there.** Dot-files, `.git`, generated build directories: the pane does not
hide real contents to look tidy. What is in the directory is what you see.

**No `..` row, and no wall.** `Backspace` goes up one directory, and it keeps going: past your
project, and up to `/` or to a drive root, where there is nothing above to go to and Workshop
says so. A directory that is a **link** is shown, **marked `(link)`, and can be entered.**
Going back up from inside one returns you to where you walked in from rather than to wherever
the link led — Workshop never quietly rewrites your path into the link's target. On Windows the
mark is the system's own answer — Workshop asks whether the entry is a reparse point — so a
**junction** is marked the same way as a symbolic link, measured on both supported Windows
toolchains, MSVC and MinGW.

**What you can read is your operating system's business.** Workshop claims no sandbox and
pretends to no perimeter: a directory this process may not read refuses in the system's own
words, right where you are, and Workshop does not bounce you back somewhere else to hide it.

**A name it cannot open honestly is marked as such.** Workshop carries file paths as plain
bytes, so a filename outside printable ASCII cannot be opened truthfully on both supported
platforms. The row stays visible — with the bytes it cannot carry shown as `?` — and refuses to
be opened, rather than quietly opening something else.

This holds for names the system itself will not spell. Some filenames — a Windows filename
holding an invalid character sequence, for instance — cannot be turned into text at all, and
another program can leave one in any directory you walk into. **Such an entry is still a row.**
You are shown that something is there, marked the same way, and it cannot be opened; the rest of
the directory lists normally, and browsing is never interrupted by a name.

**It does not judge file contents.** Any file can be sent to the editor; the editor decides
whether it can edit it and says so in its own words (see [the editor's byte rules](editor.md)).
So a `.png` or a binary opens *at* the editor and is refused *by* it, which is where the real
answer lives.

## Refreshing

The listing is a **snapshot**, not a live view. It is taken again when:

- the pane opens;
- you enter a directory or go up;
- you press `r`;
- a build you started finishes — because a build can create or remove files.

Nothing polls and nothing watches the filesystem. **If another program changes the project
while you are looking at it, the pane will not notice until one of the moments above.** Press
`r` when you want to be certain.

## Using a file as the recipe catalog

`u` hands the file the cursor is on to the one owner of this session's build recipes. A
directory refuses (a catalog is one file), and a name the pane cannot carry refuses for the
same reason it cannot be opened. Nothing here looks at the file's **name** or **extension** to
decide whether it is a catalog: you said it is, and the recipe owner reads it and answers in
its own words. If it is not one, you are told so and the recipes you were already using are
still the recipes you are using.

The whole of what that gesture does — including what happens to a build already running, and
why nothing is remembered for next time — is on [the Builder's
page](builder.md#choosing-a-recipe-catalog-while-workshop-is-running).

## What it is not

Not a file manager: nothing here renames, deletes, copies or creates. Not a search: there is
no filter box and no recursive index. Not a second place where your project is described —
what a row knows is a name and whether it is a directory, and nothing else. In particular it
knows nothing about recipes: `u` above hands over a *path*, and every judgement about what is
in the file belongs to the owner that reads it.

Marks are not bookmarks with names, not a history of where you have been, not folders you can
group, and not a way to say "these directories are my project". They are places, and that is
all they are.
