# Current limitations

**Reference.** What does not work yet, in one place, so you can plan around it instead of
discovering it. Everything here was read out of source, not inferred from architecture.

This page describes version **0.1.0**. It is a pre-release; interfaces change without
deprecation cycles.

## The library itself

### The installable package does not cover everything Zengine builds

`find_package(zengine)` exports eight capability targets and installs the loadable artifacts
they need — see [using Zengine from another
project](../getting-started.md#using-zengine-from-another-project). Four things are
deliberately outside it, and each is a real limit rather than an oversight:

- **The SDL-backed skin and input reader are not installed.** When no SDL3 is present the
  build fetches one and links it out of its own build tree, so installing those two artifacts
  without it would ship images that cannot load. A consumer who wants a window today builds
  Zengine from source.
- **Workshop is not installed.** Its executable compiles the path of the build tree that
  produced it into itself for the Builder panel, so an installed copy would carry a stranger's
  directory layout. Workshop is launched from a build tree.
- **The external-pane vocabularies are not exported**, for the reason below: there is no way
  for an externally-built pane to arrive in a Workshop run.
- **The Timer's own service headers are not installed** (`timer/normalize.hpp`,
  `timer/timer_weave.hpp`), so writing a *replacement* Timer service outside this repository
  is not a supported path. Using the Timer is; being one is not.

### A declared Timer binding's delay is fixed at declaration

`TimedWeave`'s declared bindings cover *authored rhythms*. There is no way to change a
binding's delay, and a binding created at run time waits for the next `TimerReady` rather than
reconciling immediately. A delay that is **data** — from a message, a person, a job — must use
the raw Timer protocol, which means writing the accept and emit sets, the id filter and the
receipt handling by hand.

This is the designed split ([TIMER-05](../laws/timer-laws.md)) and it is stated in the guides.
It is listed here because it is the first thing most Timer authors meet, and the convenient
layer does not cover it.

### A sender cannot observe its own send's fate

A send that goes nowhere — no holder for the role, a grant that does not permit it, a shape
this process has never declared — produces no error your handler can read. The refusal is real
and is visible to a **host-installed observer**. This is a recorded Loom seam, not a Zengine
choice; the practical consequence is that "it loads and nothing happens" is the failure a
newcomer hits first, and diagnosing it needs a tap. See
[getting started](../getting-started.md#when-it-does-not-work).

### Loading the Timer service makes the process permanently non-quiescent

The service seeds its one successor beat inside every beat's own handler, so from the moment it
is loaded the bus always has something queued — whether or not any schedule is outstanding. The
consequence for a host is that **"wait until the bus goes quiet" is never a termination
condition once the Timer is loaded**: `loom::Switchboard::drain_until_idle()` on such a process
does not return, exactly as its name and contract say. An ordinary host loop asks for
`pump_pending()` and gets control back every turn; snake and Workshop want the drain, because
the bus *is* their whole program and `stop()` is their exit.

## Workshop

### The document is not restored at launch

The desk is, and the window size is — both automatically, from the last session. The
**document** is not: every session still begins with `Ctrl`+`o`.

What makes that cost more than one keypress: a fresh Workshop seeds two example objects, so
forgetting `Ctrl`+`o` gives you a plausible document that is not yours rather than an obviously
empty one. Detail in [workspace continuity](setups.md#workspace-continuity).

### A pane's geometry is stored once and read in whatever unit your face uses

Your pane geometry lives on one medium-independent grid. A graphical Workshop reads it back
to you in **pixels** and a terminal reads it in **cells**, and where a face cannot say your
number exactly it shows its nearest answer marked `~` and says `(~ projected)`.

**Looking never authors.** Opening the same desk on the other medium, reading its geometry
there, and saving writes back exactly the numbers you authored. Only a real gesture changes
them. What is *not* here: you cannot author a different size for each medium, there is no
per-medium override, and neither unit is Zen's "true" one. Detail in [reading a pane's
geometry](panes.md#reading-a-panes-geometry--and-whose-number-it-is).

### The window comes back where you left it, into the desktop that exists now

The last session restores the **size** of the Workshop window (in canvas cells, to the nearest
whole cell), its **desktop position**, and whether it was **maximized** — a maximized close
comes back maximized, and unmaximizing lands on the size and place you had before maximizing.

Two honest bounds. First, the remembered position is validated against the monitors that exist
*at restore time*, by the graphical medium (the only party that can see them): a reachable
position restores exactly, partial off-screen overhangs included, and a position that would
leave the window's grab strip unreachable — an unplugged monitor, a moved dock — is brought
back inside the nearest display's usable area instead of being replayed blindly. Second, a
**terminal** run has no window of its own to place — the emulator owns it — so it neither
restores nor claims a position; it simply carries your remembered one forward untouched for
the next graphical run.

### The session is written on an orderly close, and only then

`q`, `Ctrl`+`c` and the window's close box all reach the same door and all write it. A Workshop
that is **killed** loses the session it was in; the previous session file is untouched and is
what the next launch reads. There is no autosave, no background writer and no crash recovery,
and none of those is claimed anywhere else either.

### Panes are 9 rows tall by default and a bigger terminal does not change that

Width scales with the surface; height does not. A larger pane is authored by arranging it —
drag a handle (right-click the pane → `arrange`, or `w` for the whole desk), press **`=`** for
a coarse four-cell step on both axes, or `Shift`+arrows for one cell at a time. It persists
correctly once authored. There is still no "fill the room" and no auto-fit. Detail in [pane
geometry](panes.md#pane-geometry).

One cell on each side of that rectangle is the pane's visible edge, so the default pane
shows 46 columns by 7 rows. Two panes feel this most: the Builder drops its lowest-priority
row at the default height, and the Compose pane's **form** does not fit one — it needs eight
rows of body and the default grants six, so a form opened in a default-sized Compose pane
shows its Submit control and none of its fields. **One `=` fixes it** and the whole form
comes back; the default itself is unchanged.

Panes are also drawn **over** the material you are building. There is no docking, no tiling and
no reflow.

### Pointing without pressing is a window-only gesture

Hovering a clipped line to [read what it cut](panes.md#reading-a-value-the-pane-had-to-cut)
needs your pointer's position while no button is down, and that is a real difference between
the two media rather than an omission in one of them. **In the graphical window it works.** In
a terminal Workshop asks for button-event mouse reporting only — presses, releases and drags —
so an idle pointer is reported to nobody and there is nothing to hover with. Asking terminals
for every idle motion would pay for that gesture on every keystroke of every session, which is
not a trade this application has chosen to make.

Two smaller edges, in both media:

| | today |
|---|---|
| Which surfaces can be read past? | the Info panel's object and property rows, and the Files pane's location and its listed names — the places Workshop itself cut a value it still holds |
| A pane that shortened its own text before sending it | **not recoverable** — Workshop can only show what it was given, and it does not ask a provider for a longer version |
| Does the reveal follow the pointer out of the window? | the reveal follows the last position the pointer reported; nothing tells Workshop the pointer left the window, so a reveal can stay on screen until the pointer comes back or something else repaints |
| Is there a timed marquee? | **no** — the reveal is driven by where your pointer is along the row, because Workshop draws when something happens and nothing here happens on its own |

### Builder builds what an authored file says, and no more

What can be built is a **recipe catalog a maker wrote**, edited in a text editor: there is no
recipe editor in Workshop and no way to add a single recipe at run time. Two recipe kinds exist
— an existing CMake target, and one `.cpp` that Zengine wraps in a generated CMake project —
and there is deliberately no third: no arbitrary shell recipe, no multi-source recipe, no
globbed source list, no dependency solver.

**Which catalog file** is in force can be changed while Workshop runs: point at it in the
[Files](files.md) pane and press `u`. That is one explicit choice of one file a maker selected,
and it is the whole of the automation: nothing searches for catalogs, adopts a conventional
filename, reads a `CMakeLists.txt`, detects a build system or writes a recipe. The choice is
not remembered either — the next launch starts from `--recipes` or the shipped default, exactly
as before.

A successful build **can** enter the running project, but only where the project already
authored participation for that artifact and this run left the row waiting. There is no hot
reload — an artifact that is already live is refused in words, and a rebuilt file has not
changed the image that is running — and no automatic build-on-missing: a maker presses a key.
Detail in [Builder](builder.md).

### The source editor holds one file at a time, in plain ASCII

**Workshop can open, edit and save any file this process can read** — from the
[Files](files.md) pane, or from the Builder for a recipe's own source; see [the source
editor](editor.md) — and
the loop `edit → save → build → realize → inspect` closes without leaving the application. The
honest bounds on that capability today:

| question | answer |
|---|---|
| Can it open a file outside the project? | **yes** — [Files](files.md) can browse anywhere your operating system lets this process read, and the file it opens keeps its own absolute path. Opening a file somewhere else does not move your project |
| More than one file at a time? | **no** — one document; opening another (with the first saved or deliberately discarded) replaces it |
| Non-ASCII source? | **no** — the shipped media place columns by byte and glyphs by sequence, so a caret over multi-byte text would lie; such a file is refused whole and never rewritten |
| Mixed line endings? | **no** — one file, one convention (LF or CRLF, preserved exactly); a mixed file is refused rather than normalized |
| Search, syntax highlighting, line numbers, splits? | **no** — it is a competent plain editor, not an IDE |
| Does unsaved source survive a crash? | **no** — like every draft here it dies with the process; an *orderly* quit refuses while source is unsaved |

What text editing also exists is three single-line editors, each over the same component: an
inspector property draft, the setup-name line, and the terminal overlay's command line.

### The Files pane is a snapshot, and two identity questions are unanswered

The [Files](files.md) listing is taken when the pane opens, when you navigate, when you press
`r`, and when a build you started finishes. **Nothing watches the filesystem**, so a file
another program creates or deletes in between is not on screen until one of those moments.
Press `r` when it matters.

Two things this release deliberately does not decide:

| question | today |
|---|---|
| Are `Src.cpp` and `src.cpp` the same file on Windows? | **Workshop treats them as two documents** — paths are compared as text, and asking the filesystem about identity on every open is a cost nothing yet needs |
| Are two hard links to one file the same document? | **no** — same answer, same reason |

**Links are traversed as text, not resolved.** A linked directory is marked `(link)` and can
be entered, and going back up returns you to where you walked in from. Workshop never rewrites
the path into the link's target — that would move you somewhere you never navigated to — which
means the same directory reached through a link and reached directly is two spellings, the
same identity limit as the two rows above. A **Windows directory junction** is marked exactly
like a symbolic link: measured, not assumed — Workshop asks whether an entry is a directory
when followed but not when unfollowed, and a junction answers exactly that way. (Asking
whether it is a *symbolic link* would have missed it, which is why Workshop does not ask
that.)

### Where you may browse is your system's answer, and marks are places only

Workshop models no filesystem boundary of its own. It has exactly the access the process it
runs as has; a directory you may not read refuses in the system's own words, where you are
standing, and nothing is substituted for it.

| question | today |
|---|---|
| Can Files reach outside the project? | **yes** — up to `/` or a drive root, into links, and onto any drive the system reports |
| Does browsing somewhere change what my project means? | **no** — a relative source in a build recipe is always resolved against the directory Workshop was launched in, wherever you happen to be looking |
| Are the Windows drives listed every path I can reach? | **no** — they are the *logical drives this system reports*. A network share you can spell the path of is reachable and is not in that list |
| Can I mark a file? | **no** — directories only |
| Can I name a mark? | **no** — a mark is its path. Names can be earned when something needs a stable human name for one |
| Does the browsing location come back next launch? | **no**, deliberately — marks come back, half-finished browsing does not |

One narrow case is worth knowing: if a run has **no origin** (the system could not report a
working directory Workshop can write down) **and no marks yet**, the Files pane will not take
the keyboard, because there is nowhere it could offer to take you without asking the system on
every keystroke. Mark a place once from an ordinary launch and that run can jump to it.

### Some filesystem names cannot be written down, and that is said rather than survived

Workshop holds every path as plain text. A filesystem will accept names that cannot be turned
into that text at all — on Windows, a filename holding an invalid character sequence, or a
directory named outside the system's active code page. These are not names you would type; they
are names another program can leave where you are about to look.

| where you meet one | today |
|---|---|
| A file or directory with such a name, in a listing | **shown, marked, and not openable** — the row says something is there, the rest of the directory lists normally, and browsing carries on |
| The directory you launched Workshop in | **the run simply has no project** — the banner says so, exactly as when the system cannot report a directory at all |

**Neither case ends the program, and neither substitutes a different path.** A name Workshop
cannot write down is refused where it is met; nothing nearby is opened, browsed, or reported in
its place. What Workshop *can* carry is unchanged: a path is still plain bytes, and a filename
outside printable ASCII is still visible and still not openable.

### Lifecycle

| operation | today |
|---|---|
| load weaves and mount providers at startup, from an authored plan | **yes** |
| overlay a provider contribution over an existing one, reversibly | **yes** — `"mode": "overlay"` in the plan |
| unmount a provider | **only** as a failed record's own rollback, and at teardown |
| roll back one artifact's partial record | **yes** — one artifact is the atomic unit |
| roll back a whole plan | **no** — earlier artifacts stay; you are told which artifact stopped it and what still stands |
| unload a weave at run time | **no** from Workshop |
| reload or replace a weave in place | **no** from Workshop |
| any run-time change to what is loaded | **no** |

**Loading is initial and restart intent.** The plan executor has no unload, reload or remount
path — that is written down in its own source, not inferred.

**What realizes a project is now a live thing rather than a call**, and that changes what is
*missing* rather than what is possible. Workshop begins the project and returns to its ordinary
loop; each row settles when its own load answer arrives, and the owner of that work is still
there afterwards, holding the authored plan, a cursor and what each row produced. Nothing new is
offered to a maker by that on its own — there is still no unload, no reload, and no run-time
change to what is loaded. What it removes is the reason those were impossible to *reach*: the
object that would have to perform them used to have returned before the host loop started.

**Reversible provider overlay is not artifact hot reload**, and the two must not be read as one.
Overlay and unmount are reversible *within the host's operator catalog*: unmounting an overlay
reveals what was underneath, unchanged and unrebuilt. That is a real, proven capability about
**contributions**. It says nothing about replacing a loaded **artifact** while the system runs.
The Loom's `Kernel::reload_from` exists and has open provider-custody caveats; nothing in
Workshop reaches it.

### No cross-pane interaction

You cannot drag a semantic object from one pane into another. There is no drag-and-drop between
panes, no shared selection across panes, and no protocol by which one pane could hand an object
to another. An external pane is read-only prose and receives no input at all.

The ownership map a future cross-pane gesture would have to cross is recorded in
[the architecture notes](../architecture/README.md#cross-pane-interaction).

### Panes have no installation story

A pane arrives because an artifact was in the load plan and the weave offered one. There is no
discovery, no plugin directory, no versioning of pane offers, and no way for a maker to install
somebody else's pane other than by editing a plan file and having the artifact on disk.

## Platforms

| | state |
|---|---|
| Linux / WSL, GCC 11+ | fully supported; the canonical lane |
| Linux, Clang | builds; not a routine lane |
| Windows, MSVC 19.50 (VS 2026) x64 | the portable subset builds and the suites run. clang-cl and ARM64 are unverified |
| Windows, MinGW-w64 | builds; runtime DLLs must be on `PATH` or beside the binaries |
| macOS | never built. Unclassified, not "unsupported" |
| the Loom's OS sandbox | **Linux only.** On Windows the kernel exists as an explicit development/demo backend with **no isolation**, and says so at every surface |

Detail in [supported toolchains](../contributing/supported-toolchains.md).

## What is deliberately not here

These are absences by decision, not gaps, and they are not going to arrive because a toolkit is
expected to have them:

- **No widget set.** The component package has one component, earned by working consumers
  (four of them, since TEXT-0) needing the same machinery. There is no Button, List, Dropdown,
  ScrollView, focus tree, tab order or theme.
- **No notifications.** [Attention](attention.md) shows what is **true right now**, and that
  is the whole of it: nothing accumulates, nothing is unread, nothing pops up, nothing expires
  and nothing animates. There is no history of things that stopped being true — a record of
  what happened is `--log` and `--dump`, which are a different question.
- **No layout system.** The drawing vocabulary has no parent/child, anchors or percentages —
  whoever publishes has already decided where things go. The `ui` package resolves an authored
  extent against a frame and states no containment.
- **No shell.** The Builder's process runner takes a program and an argument vector.
  `popen()` would have been shorter and would also have been a general shell capability.
- **No dependency solver.** A load plan's order is authored policy; a person wrote the rows in
  the order they must happen.
- **Containment is not claimed.** An in-process weave shares the host's address space. A grant
  bounds what a weave may *say*, never what it may *touch*. `Kernel::containment_note()` says
  so at run time; read it literally.
