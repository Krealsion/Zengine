# Files

**How-to.** Browsing the project you launched Workshop in, and opening a file from it without
going through the Builder first.

## What the project is

The project is **the directory you launched Workshop in** — nothing more configurable than
that, and it is printed on the startup banner:

```text
zengine-workshop - project: /home/you/my-thing
```

The same Workshop serves two projects by being launched in two places. There is no project
file, no project setting and no `--project` flag; if you want a different project, start
Workshop there.

If the system cannot report a working directory the banner says so, and the Files pane refuses
to browse rather than guessing somewhere for you.

## Opening the pane

Files is an ordinary pane. Press **`p`**, choose **Files**, press Return — and the same
gesture removes it again. It moves, resizes, stacks and rides a saved setup exactly like every
other pane (see [Panes](panes.md)).

Where you had browsed to is **not** saved: a new Workshop starts at the project root. What is
saved is that the pane is on your desk, and where it sits.

## Moving around

Press into the pane to point your keys at it, then:

| key | does |
|---|---|
| `↑` `↓` | move the cursor |
| `Return` | **enter** a directory, or **open** a file in the editor |
| `Backspace` | up one directory |
| `r` | look again |

The mouse works the way the keys do, with one deliberate extra step: **the first press on a
row selects it, and pressing the row that is already selected opens it.** So the press that
points your keys at the pane can never also open a file — which matters most when you have
unsaved edits, because opening a different file is exactly what would be refused.

The wheel moves the cursor through the listing.

Directories are listed first, then files, and each group is sorted by its exact filename
bytes — so the order is the same on every machine, in every language setting.

## What it shows you, and what it does not

**Everything that is there.** Dot-files, `.git`, generated build directories: the pane does not
hide real contents to look tidy. What is in the directory is what you see.

**The project, and only the project.** There is no `..` row. `Backspace` goes back up through
the directories you walked into, and at the root there is nothing to go up to. A directory that
is a **link** is shown but cannot be entered — following it would take you out of the project
while the pane still claimed you were inside it. On Windows that includes a **junction**, not
only a symbolic link.

**A name it cannot open honestly is marked as such.** Workshop carries file paths as plain
bytes, so a filename outside printable ASCII cannot be opened truthfully on both supported
platforms. The row stays visible — with the bytes it cannot carry shown as `?` — and refuses to
be opened, rather than quietly opening something else.

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

## What it is not

Not a file manager: nothing here renames, deletes, copies or creates. Not a search: there is
no filter box and no recursive index. Not a second place where your project is described —
what a row knows is a name and whether it is a directory, and nothing else.
