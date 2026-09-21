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

Files is an ordinary pane. Press **`Ctrl`+`p`**, move to **Files**, press Return — it opens
with your keys in it, and `x` on the same row of the Pane Manager closes it again. It moves, resizes, stacks and rides a saved setup exactly like every
other pane (see [Panes](panes.md)).

**It is not compiled into Workshop.** Files is a separate artifact, `zengine-files`, loaded at
startup because a row in the load plan says so — the same way the Timer, the Compose pane and
the introspection panes arrive (see [Load plans](load-plans.md)). Take the row out and you get
a Workshop with no Files pane and no error; put your own artifact in its place and you get a
different one. Nothing about that is visible while you are using it, which is the point.

One consequence you may see once: a desk you saved with an older Workshop names this pane the
way that Workshop offered it, and the reference is brought forward when the desk is read. You
are told, once, on the run that does it:

```text
reopened your last desk "Morning" -- 132x41 cells; the Files pane moved to its own office --
zengine.workshop/project-files is now zengine.files/project-files
```

Nothing is rewritten on disk until you next save that desk in the ordinary way.

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
| `a` | **pick buildable** — list what this directory can try to build and author one recipe row — see [the Builder](builder.md#authoring-a-recipe-from-files) |
| `m` | **mark** this directory, or unmark it if it is already marked |
| `n` / `N` | go to the **next** / **previous** marked place |

**Under the listing there is a row of labelled controls, and pressing one does what the key
beside it in the table above does.** `[menu]` `[enter]`/`[open]` `[up a directory]`
`[look again]` `[use as recipes]` `[pick buildable]` `[mark here]`/`[unmark here]`
`[previous mark]` `[next mark]` — each acts on what the pane is showing: the selected row for
the two that open a file, and where you are standing for the rest. A control the pane does not
believe applies right now is drawn in round brackets instead — `(use as recipes)` on a
directory — and pressing it still answers, with the same sentence the key would have written,
because a control you can aim at owes you a reason. In a short or narrow pane the strip keeps
what fits and says `+N in menu`; the rest are in `[menu]`, which is always the first control
and is never the one dropped.

**`[menu]`, `Shift+M`, or a right press on a row opens this pane's own menu**, presented
wherever your hand is. (`Shift+M` is the browser's and the chooser's; **while you are typing
into the authoring line there is no default key for the menu at all**, because a key that
opens it would eat the capital letter its keystroke produced. `[menu]` and the right press
still work there, and you can bind `files.menu` to a key of your own.) It carries every
operation the strip does, spelled with what it will
act on — `open \`alpha.cpp\``, `use \`recipes.json\` as this project's recipes`, `mark this
place` — plus `manage this pane...`, which hands the pane itself to Workshop's own pane menu
(arrange, order, edit its code, remove). A right press on a row that names nothing — the
header, a `... N more` marker, blank space — hands the press back to Workshop, whose own menu
opens instead. A menu stays open across whatever else happens; when its answer arrives, Files
checks that the place and the row it was opened about are still what is here, and refuses in
words rather than acting on something else.

**A right press does not move your keys — but a row you chose that starts you typing does.**
Right-pressing a candidate while your keys are somewhere else leaves them there, which is what
keeps pointing at a pane from being an act in it. Choosing `author a recipe for …`, or a
`type the …` row, opens a line you are meant to type into, so Files asks Workshop for the
keyboard as part of that choice — granted only while the choice is still the last thing you
did, so a press somewhere else in the meantime keeps your keys where you put them. A chosen
row that only walks the browser (`look at this directory again`, `up a directory`) takes
nothing.

The mouse works the way the keys do, with one deliberate extra step: **a press selects a row,
and pressing the row that is already selected opens it — once your keys are already in
Files.** A press that brings your keys here from somewhere else (the Editor after it opened a
file, or Workshop's own keys) only selects and points the keys at Files, even on the row that
was selected, so it can never also open a file — which matters most when you have unsaved
edits, because opening a different file is exactly what would be refused. While a layout's
name line has the keys, presses in Files only select, and the bottom band stops saying that
typing goes to Files; finish the name and the next press on the selected row opens it. With pane
titles hidden, Files' title comes back when your keys arrive, and the row you pressed is still
the row you selected.

While you are choosing what to build or typing a recipe field, only that step's keys are in force
([below](#authoring-a-recipe-from-here)). On a field only Return and Escape are claimed, and every
other key goes into the line you are editing, so Backspace deletes a character rather than
walking up a directory.

**If you move these keys in your own keymap file**, your spelling is in force, name by name. The
browsing names — `files.up`, `files.down`, `files.open`, `files.parent`, `files.refresh`,
`files.use-recipes`, `files.mark`, `files.next-mark`, `files.previous-mark` and
`files.pick-buildable` — have not changed. Return has a separate name in each step, and each one
moves alone:

| name | what Return does | where |
|---|---|---|
| `files.open` | enter or edit | browsing |
| `files.choose` | choose this | choosing what to build |
| `files.commit-field` | commit this field | typing a recipe field |

`files.up` and `files.down` also move through the list of candidates, and `files.cancel` (Escape)
backs out of choosing or typing; each means the same thing wherever it is in force, so each keeps
one name. Three more names have **no default key at all**, and are yours to bind:
`files.next-field` (what `[next field]` does — keep this field and step, and nothing else),
`files.write-recipe` (the write), and `files.menu` **while a field is open**, where a printable
key would eat the character it produced. **An override of `files.open` now moves the browsing Return only**; earlier Workshops
also applied it while choosing and while typing a field. To move those, write `files.choose` or
`files.commit-field` as well — no name is copied onto another. And if your file also put
`files.up`, `files.down` or `files.cancel` on `return`, that key now meets the chooser's or the
line's own Return: when that step opens, Workshop refuses its keys in words naming both, and
Escape and Return do nothing there until one of the two moves in your file and Workshop starts
again ([hotkeys](hotkeys.md#the-keymap-file)).

Workshop names each key when it reads it, by the keys in force at that moment, and Files acts on
that name. A Return read while a field's keys are in force is `files.commit-field`: if the field
has closed before Files receives it, it does nothing, and it never becomes the browser's open.
Once the browser's keys are in force, a Return is `files.open` and opens the file under the
cursor, as it should. That is a promise about the operation a key was read as — not about how
fast you type, what was on screen, or which row or field you meant.

The wheel moves the cursor through the listing, and through the list of candidates while you
are choosing what to build.

**The listing scrolls by the least it can.** Moving the cursor to a row you can already see
does not re-lay the list under your hand; it scrolls only when the cursor would otherwise
leave the window. That is what makes pressing a row twice reliable — and if the rows *do*
move between your aim and the press (a build finishing re-walks the directory), the press is
refused with `the rows moved -- press again` rather than spent on whatever slid into that
place.

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

This holds for names the system itself will not spell. Some filenames — on Windows, a filename
holding an invalid UTF-16 sequence, for instance — cannot be turned into text at all, and
another program can leave one in any directory you walk into. That is the answer of both
supported Windows standard libraries, measured: MSVC's STL reports no mapping in the target code
page, MinGW's libstdc++ an illegal byte sequence. **Such an entry is still a row.**
You are shown that something is there, marked the same way, and it cannot be opened; the rest of
the directory lists normally, and browsing is never interrupted by a name.

**It does not judge file contents.** Any file can be sent to the Editor pane; the Editor
decides whether it can edit it and says so in its own words, which the Files pane repeats in
its first row (see [the editor's byte rules](editor.md)). So a `.png` or a binary opens *at*
the Editor and is refused *by* it, which is where the real answer lives. If no Editor pane is
loaded, `Return` on a file is refused at once, in the pane's first row, saying so; if the
open cannot even be sent — no opening office is present in this Workshop — the row says that
instead, and a later `Return` is a fresh attempt.

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

## Authoring a recipe from here

**Every field is shown, and you can go back to any of them.** The chooser lists what this
directory can try to build (`[pick buildable]`, or `a`); taking a candidate — a second press
on it, `[author a recipe for this]`, or Return — opens four field rows with the line standing
on the first. `[next field]` and the arrows keep the field in hand and step to the next, and
**pressing a field row stands the line on that field again, keeping what it already holds**.
`[write the recipe]` writes the whole draft from whichever field you are on, and refuses by
name while a required one is empty. `[abandon]` (Escape) drops the draft whole; nothing is
written until the write.

**`[next field]` never writes.** On the last field it is drawn `(next field)`, and pressing it
says so rather than doing something else — writing is `[write the recipe]`, deliberately and
only. Return keeps the bargain it always had: it commits the field in hand, steps to the next,
and **from the last field it writes the draft**.

**A short pane keeps the field you are typing into on the screen.** If the room can only draw
one or two field rows, the one with the line in it is the one you get, the rows above and below
it are counted (`... 2 more fields — this pane's menu names every one`), and **the menu offers a
`type the …` row for every other field** — so a field the room cannot draw is still one press
away. That menu also carries `keep this field and type the …` (`this is the last field` on the
one after it), `write the recipe for …` and `abandon this recipe`, which is the whole strip,
including the field row you cannot step past.

**A narrow pane keeps the value you are typing visible, not just its label.** Some field
labels — `package prefix (comma-separated)`, a Builder role's `role for …` — are long enough on
their own to fill a body that is only thirty or so columns wide, and the label used to win that
room outright: what you typed was accepted and kept, but none of it was drawn. The label gives
way now, shortened to leave the value a useful minimum of room, never the other way around; a
wider room shows the label whole again, and nothing about the value itself ever changes because
of it.

What you type is a **draft**: Workshop composes the row, checks it by the recipe law, appends
it to the catalog in force and installs it. The pane writes no file.


`a` lists what the directory you are looking at can at least try to build — a `.cpp` file, or a
directory that holds a configured CMake tree — and, when you choose one, asks the few things
nothing can detect and appends one recipe row, as you typed it, to the catalog in force (or to a
`build-recipes.json` it creates in your project — the catalog in force from then on, the next
launch from there included).

**Both steps happen inside the pane.** The list of candidates replaces the listing in the pane's
own space, `↑`/`↓` move through it, Return chooses, Escape backs out; then each field is a line
in the same space, Return commits it and moves on, and Escape abandons the whole thing without
writing anything. There is no popup and nothing covers your desk.

Workshop itself composes no recipe: what you typed is handed over, and the same rule that reads
a catalog file checks it, appends it as written and installs it. The whole of it is on [the
Builder's page](builder.md#authoring-a-recipe-from-files).

## What it is not

Not a file manager: nothing here renames, deletes, copies or creates. Not a search: there is
no filter box and no recursive index. Not a second place where your project is described —
what a row knows is a name and whether it is a directory, and nothing else. In particular it
knows nothing about recipes: `u` above hands over a *path*, `a` hands over a *place* and its
names, and every judgement about what is in a file belongs to the owner that reads it.

Marks are not bookmarks with names, not a history of where you have been, not folders you can
group, and not a way to say "these directories are my project". They are places, and that is
all they are.
