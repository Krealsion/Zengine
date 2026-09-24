# Current limitations

**Reference.** What does not work yet, in one place, so you can plan around it instead of
discovering it. Everything here was read out of source, not inferred from architecture.

This page describes version **0.1.0**. It is a pre-release; interfaces change without
deprecation cycles.

## The library itself

### The installable package does not cover everything Zengine builds

`find_package(zengine)` exports nine capability targets and installs the loadable artifacts
they need — see [using Zengine from another
project](../getting-started.md#using-zengine-from-another-project). Three things are
deliberately outside it, and each is a real limit rather than an oversight:

- **The SDL-backed skin and input reader are not installed.** When no SDL3 is present the
  build fetches one and links it out of its own build tree, so installing those two artifacts
  without it would ship images that cannot load. A consumer who wants a window today builds
  Zengine from source.
- **Workshop is not installed.** Its executable compiles the path of the build tree that
  produced it into itself, for the Builder tool it mounts, so an installed copy would carry a
  stranger's directory layout. Workshop is launched from a build tree.
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

### A sender sees its own send's fate only if it asks

A send that goes nowhere — no holder for the role, a grant that does not permit it, a shape
this process has never declared — produces no error your handler can read unless your weave
asks for one. A weave that explicitly accepts Loom's `zen.DispatchRefused` is told when one of
its directed or role-addressed sends was refused before its target ran, and a send that could
not be queued at all returns a ticket that is not valid; the Files, Builder, Editor and Info
panes use both. A publication is never reported that way, and a delivered message that is never
answered tells its sender nothing. The refusal is always visible to a **host-installed
observer**. This is a recorded Loom seam, not a Zengine choice; the practical consequence is
that "it loads and nothing happens" is still the failure a newcomer hits first, and diagnosing
it needs a tap. See [getting started](../getting-started.md#when-it-does-not-work).

### Loading the Timer service makes the process permanently non-quiescent

The service seeds its one successor beat inside every beat's own handler, so from the moment it
is loaded the bus always has something queued — whether or not any schedule is outstanding. The
consequence for a host is that **"wait until the bus goes quiet" is never a termination
condition once the Timer is loaded**: `loom::Switchboard::drain_until_idle()` on such a process
does not return, exactly as its name and contract say. An ordinary host loop asks for
`pump_pending()` and gets control back every turn; snake and Workshop want the drain, because
the bus *is* their whole program and `stop()` is their exit.

## Workshop

### The source you were editing is not reopened at launch

The desk is, and the window size is — both automatically, from the last session. The Editor's
**source** is not: the Editor starts empty, and you open the file again from Files or the
Builder's `e`. Detail in [workspace continuity](setups.md#workspace-continuity). (This section
was about the prototype object document, which every session began by reopening with
`Ctrl`+`o`; it retired with the object canvas.)

### One run holds at most eight layouts

[Layouts](setups.md#several-layouts-in-one-workshop) all come back after a restart now — the set,
your order, their names, the one you were standing on and which Setup file each is related to —
so what is left here is the bound: a ninth layout is refused rather than quietly replacing one,
and so is a duplicate that would make a ninth.

What a layout still does **not** carry is anything Workshop holds once for the whole run: the
Editor's source, the project, the file browser's location, your marks, your recipes, the keymap and the
window are one each, and switching layouts changes none of them.

### One Setup file per run, chosen on the command line

A layout keeps whichever Setup file it was saved to or restored from, and different layouts may
be related to different files — but there is **no way to choose a new file from inside
Workshop**. The one a layout can acquire is the one `--setup` names, so within a single run every
association a maker makes fresh points at that same path. Keeping several arrangements on disk
still means several runs:

```sh
zengine-workshop --setup layouts/wide.json
zengine-workshop --setup layouts/inspect.json
```

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

The outermost part of that rectangle is the pane's visible edge. On a terminal that is one
cell on each side, so the default pane shows 46 columns by 7 rows; in a window it is one
device pixel, so the same pane keeps nearly all of its interior. The tightest case is
therefore the terminal's, and two panes feel it most there: the Builder drops its
lowest-priority row at the default height, and the Compose pane's **form** does not fit one —
it needs eight rows of body and the default grants six, so a form opened in a default-sized
Compose pane shows its Submit control and none of its fields. **One `=` fixes it** and the
whole form comes back; the default itself is unchanged.

Panes are also drawn **over** the material you are building. There is no docking, no tiling and
no reflow.

### Pointing without pressing reaches nobody in a terminal

Nothing in Workshop uses an idle pointer today — the one gesture that did, reading past a cut
row, is [retired](panes.md#reading-a-value-the-pane-had-to-cut--retired) — but the difference
between the two media is real and worth knowing before anything asks for one again. **In the
graphical window a hover is reported.** In a terminal Workshop asks for button-event mouse
reporting only — presses, releases and drags — so an idle pointer is reported to nobody.
Asking terminals for every idle motion would pay for that gesture on every keystroke of every
session, which is not a trade this application has chosen to make.

Two smaller edges, in both media:

| | today |
|---|---|
| Which surfaces can be read past? | **none, any more.** It worked only where Workshop itself cut a value it still held: the Info panel's object and property rows, and before them the Files pane's location and names. Both are loaded panes now, and a pane sends text it has already cut — so Workshop never receives the longer value and has nothing to reveal |
| The notice row, and the Builder's realize row | **not readable past** — a long refusal is cut in both, and a load the Loom refuses is longer than either. Launch with `--log <path>` and the whole sentence is kept in that journal ([Builder](builder.md#using-it)) |
| Could the pane protocol be widened to ask for the longer text? | **it will not be.** A pane is a participant, not a store the host reaches into; asking one for a value it chose not to send is the shape this project refuses. Widen the pane or the window instead |

### Builder builds what an authored file says, and no more

What can be built is a **recipe catalog a maker wrote**: edited in a text editor, or grown one
row at a time from the Files pane (`a`), which asks for what nothing can detect and appends
exactly what was typed. There is no recipe editor in Workshop, and no row is changed or removed
except in a text editor. Two recipe kinds exist
— an existing CMake target, and one `.cpp` that Zengine wraps in a generated CMake project —
and there is deliberately no third: no arbitrary shell recipe, no multi-source recipe, no
globbed source list, no dependency solver.

**Which catalog file** is in force can be changed while Workshop runs: point at it in the
[Files](files.md) pane and press `u`. That is one explicit choice of one file a maker selected,
and it is the whole of the automation: nothing searches for catalogs, adopts a conventional
filename, reads a `CMakeLists.txt` or detects a build system; a recipe is written only when a
maker presses `a` and answers. The `u` choice is not remembered — the next launch starts from
`--recipes`, else the project's own catalog, else the shipped default — and a catalog `a`
created lives in the project as `build-recipes.json`, beside the `workshop-plan.json` a row `o`
authored goes into: both *are* in force next launch from that directory with no flags. Both
are one line of JSON, as every file Workshop writes is.

A successful build **can** enter the running project, where the project already authored
participation for that artifact: a waiting row is realized, and a row that is already live is
**reloaded in place** — same `WeaveId`, state kept, for a weave whose shapes did not change. A
changed shape is refused before the running weave is touched, and the refusal names the change;
replacing it is the Loom's prepared replacement with an authored migration, which Workshop does
not host yet. No automatic build-on-missing and no reload on a file appearing: a maker presses a
key. Detail in [Builder](builder.md).

**A build's own words are read in the Builder, from memory.** `l` shows what a build said, bound
to that build. The build tool keeps only its last few operations, and of a long build the
beginning and the end with the middle counted; nothing is written to a file, searched, or kept
across a restart, and a row shows what a canvas can draw of a compiler's characters, counted. On
Windows, Ninja has already re-encoded a compiler's non-ASCII characters by the time Workshop reads
them, so its quotes read as `?`.
[Reading what a build said](builder.md#reading-what-a-build-said).

**Workshop's own panes are changed from inside Workshop only through a development setup** — a
development catalog and a runtime copied from one configured Zengine build tree
([develop Workshop](develop-workshop.md)). That covers the pane weaves Zengine ships. The host
executable, the skins, the input readers, the Timer and the operator providers are not rebuilt or
reloaded from inside Workshop, and a development runtime is not an installed Workshop that updates
itself.

**Artifacts are still named by stem beside the host.** A project's built pane is kept beside the
Workshop that loaded it (`tally.so`, `tally.reloads/`), not in the project, so two projects run
from one Workshop build directory stage into — and a promotion in either writes — the same files;
a reload's copy takes a name no file already has, so the two never load each other's reload copies,
but the promoted file is shared. A development setup gives each build tree a runtime of its own
and so does not share them.

### The source editor holds one file at a time, in plain ASCII

**Workshop can open, edit and save any file this process can read** — from the
[Files](files.md) pane, or from the Builder for a recipe's own source; see [the source
editor](editor.md), a loaded pane that holds the document itself — and
the loop `edit → save → build → realize → inspect` closes without leaving the application. The
honest bounds on that capability today:

| question | answer |
|---|---|
| Can it open a file outside the project? | **yes** — [Files](files.md) can browse anywhere your operating system lets this process read, and the file it opens keeps its own absolute path. Opening a file somewhere else does not move your project |
| More than one file at a time? | **no** — one document; opening another (with the first saved or deliberately discarded) replaces it |
| Non-ASCII source? | **no** — the shipped media place columns by byte and glyphs by sequence, so a caret over multi-byte text would lie; such a file is refused whole and never rewritten |
| Mixed line endings? | **no** — one file, one convention (LF or CRLF, preserved exactly); a mixed file is refused rather than normalized |
| Search, syntax highlighting, line numbers, splits? | **no** — it is a competent plain editor, not an IDE |
| Does unsaved source survive a crash? | **no** — like every draft here it dies with the process; an *orderly* quit asks the Editor and refuses while source is unsaved |
| Does it survive a reload of the Editor's own image? | **yes** — the document, its unsaved edits, caret, selection and scroll position ride a same-shape reload; the undo history and a paste still on its way do not |
| Does it survive the Editor pane being closed? | **yes** — the pane is a presentation; bring it back from the Pane Manager and the document is where it was |

What text editing also exists is one single-line editor in this host — the layout-name line —
and more in loaded panes over the same component: Info's property draft, the Terminal's command
line, the Pane Manager's name line and Files' authoring line.

**A pane's line now shows you a caret, and still cannot be swept.** A pane may publish where
its caret is and what it has selected beside the rows it sends, so the Terminal's command line
has an insertion point again — a bar between glyphs in a window, an inserted `_` in a cell.
Info's property draft does not yet say one; it can, unchanged, whenever that image is next
touched. What no pane's LINE has is a pointer sweep: the two editors accept one while the
button is held (`PaneDragged`) and no single-line pane does, so in a line a range is selected by
`Shift` and the arrow keys.

### Neovim in the Editor pane draws plain text, one range, and no colours

The Editor's office can be held by Neovim instead ([Neovim in Workshop](neovim.md)), and a switch
carries your document both ways. The honest bounds today:

| question | answer |
|---|---|
| Syntax highlighting, search matches, colours? | **no** — the pane protocol carries rows, one caret and one selection range; what Neovim colours is not drawn |
| Non-ASCII on screen? | **drawn as `?`** — each cell is one plain character (box drawing becomes `-`, `|`, `+`); the document itself is untouched |
| A blockwise selection? | **its cursor's row only** on screen; a switch carries its cursor only, and says so |
| Several windows, tabs or buffers? | **inside Neovim, yes** — a switch to the standard Editor carries the current buffer, and asks your consent before other unsaved buffers are lost |
| Your own Neovim configuration? | **opt-in** — Neovim starts clean unless `ZENGINE_NEOVIM_PROFILE=user`; the pane's top row says which profile is running, and a name that is neither `clean` nor `user` and names no init file refuses the start ([which configuration is running](neovim.md#which-configuration-is-running)) |
| A Neovim session across a rebuild of the Neovim editor itself? | **no** — that reload is refused while Neovim runs; quit Neovim or switch editors first |
| `Ctrl`+`k` inside Neovim? | **no** — it stays Workshop's hotkey view, the keyboard road back to Workshop's controls |
| A terminal's own paste into Neovim? | **as typing** — Workshop does not yet tell a terminal paste from keys, so paste in Insert mode; in Normal mode the text runs as commands |
| A copy made before a switch, pasted after it, in a terminal? | **no** — a terminal medium cannot read the system clipboard, and the editor that starts at a switch has seen no earlier copy; the graphical window reads the real clipboard |
| A switch control in a pane? | **not yet** — the switch is asked from the Terminal pane; a swapper pane and a confirmation dialog are later presentations of the same office |

### The Terminal's history and its record are this run's, and bounded by the participant

The [Terminal](terminal.md) recalls commands with `↑` and `↓` and reads back through its record;
both live in the participant, and the participant lives as long as the run.

| question | answer |
|---|---|
| Is command history kept across launches? | **no** — there is no history file; a new launch starts with none |
| How far back does it go? | as far as the participant's record keeps, which is the newest **256 entries of every kind** — commands, notices, answers together |
| Can I scroll to an entry the record dropped? | **no** — the row above the view counts those separately as `dropped for good`, and the view moves to the oldest kept if the entry you were reading is evicted |
| Does `↑` mean history while I am composing? | **no** — while a command is being composed, or a list was asked for, `↑` `↓` move the list; history is for a line with nothing on it |
| Can I search the record? | **not yet** — you can walk it by page, wheel or end; there is no find |
| Can I resize a command's own wrapping? | it wraps to the pane's width; following the newest output shows the **end** of a long entry, and its start is above |
| Are the reading keys remappable? | **yes** — they are the pane's declared rows (`terminal.scroll-up`, `terminal.scroll-down`, `terminal.oldest`, `terminal.newest`), so the [keymap file](hotkeys.md) moves them |

**Where a line can go is read when you ask, and promises nothing.** At the address, `Tab` lists the
offices held and the weaves registered **at that moment**, each with what it is. It is not
permission — authority is per shape and per target — and not a promise the destination is still
there when you send: a weave that is gone refuses the delivery, on your own record, and is never
quietly replaced by another. An `@office` reaches whoever holds that office at delivery, which the
list says beside it.

**`Esc` from a pane that types is the pane's until it has nothing left.** The Terminal sheds its
list, then its line, then itself. Both editors keep every `Esc` — the standard Editor's does
nothing by design, Neovim's is Neovim's — so the way out of those is a press elsewhere, which the
keyboard band names while they hold the keys. A pane that takes no keys at all is put down by `Esc`
directly.

### A pane asks for some answers, and an answer can arrive after you have moved on

A pane that is its own loaded program cannot call into Workshop; it **asks**, and the answer
comes back a moment later. Three things the Terminal does work that way — running a line, being
told what could come next, and pasting — and in each case you may have typed something else in
between. The rule is the same for all three: **an answer that is no longer about what you are
typing is discarded, silently.**

| what you did | what you see |
|---|---|
| Typed a word, then pressed `Esc` before the suggestions arrived | no list. `Tab` asks again, for the line you have now — it never accepts the suggestion for the word you cancelled |
| Recalled a command, or kept one with `Enter`, while suggestions were in flight | no list, even when the recalled line reads exactly like the one you asked about: an answer belongs to the intent that asked for it |
| Moved the caret back into the line while suggestions were in flight | `completion follows the END of the line`, and it stays that way. `End` brings the list back |
| Pressed `Ctrl`+`V`, then cleared the line and typed a different command | **nothing is pasted**, and nothing says so. Paste again |
| Pressed `Ctrl`+`V` and kept typing on the same line | the text arrives where your caret is. Typing is an edit, not a new command |

The last two are the same distinction everywhere in Workshop: **a paste belongs to the draft
that asked for it.** Clearing a line with `Esc` ends that draft, and so does submitting the
Terminal's line with `Enter`; typing into it does not. An Info property draft is the exception
to the second: `Enter` sends its value, and the draft closes only when the host writes exactly
what was sent.

**Info's commit is an ask too, and says how it ended.** Info sends one commit at a time, to
the host, which writes it through the door the desk already has; the answer can arrive after
you typed more, pressed `Esc`, or inspected something else:

| what happened | what the pane says |
|---|---|
| the host wrote the value, and you had typed more since | `earlier commit written -- later edits not sent`; the draft stays open with your newer text |
| you pressed `Esc` before the answer | `commit already sent -- the draft is closed, and it may still be written`, replaced by how it ended while that sentence is still showing |
| the inspected pane, its desk or its rows changed before the commit arrived | the draft is abandoned (`commit already sent -- the draft ended: …`), and the host refuses the commit — `commit refused -- the inspected pane, its desk or its rows changed before it arrived, so nothing was written` |
| the commit could not be sent, or Loom refused it on its way | `commit not submitted` or `commit not delivered`; nothing is written, and the draft stays for `Enter` |
| the host received it and has not answered | nothing new: it stays outstanding, and `Enter` again says `commit not sent -- an earlier commit is still unanswered` |

The same sentences, with what each one leaves behind, are in step 3 of
[getting started with Workshop](getting-started.md).

**What a paste puts on a single-line field.** The Terminal's line is one line, so a clipboard
holding more than one becomes one: a tab, a line feed, a carriage return and a CRLF pair each
arrive as a single space, and two copied lines land joined by one. A paste is also **one
gesture for undo** — `Ctrl`+`Z` after pasting takes back the paste and leaves what you had
typed before it.

**Text outside plain ASCII is not pasted, and the pane says so.** A line whose row draws those
bytes as spaces would show you something other than what you would submit, so the whole paste
is refused with a sentence above the line rather than a half of it landing. Typing such a
character is refused the same way — that door is the pane's own, and it arrived with the pane;
the modal terminal this replaced let those bytes into the line.

**A refusal never takes the line's row.** If a door refuses what the Terminal asked it to run,
the sentence appears *above* your command line — the line stays where it was, with your caret
on it, so you can edit and try again. In a pane exactly one row tall there is no row for the
sentence that is not your own line, so the line keeps it and the refusal is not shown.

**You are unlikely to ever see that refusal in Workshop**, and the reason is worth saying: the
only one this door gives is *no terminal participant is mounted on this bus*, and Workshop
always mounts one. The Terminal is a loadable pane, so a different host could load it without a
participant — that host would see the refusal, and this is what it would look like.

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
like a symbolic link: on Windows, Workshop asks the system itself whether an entry is a reparse
point — which a junction and a symbolic link both are — measured on both supported Windows
toolchains, MSVC and MinGW. (The C++ standard library is not asked there, because the two
libraries answer differently: one reports a junction as an ordinary directory, and asking
either whether it is a *symbolic link* misses a junction.)

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
into that text at all — on Windows, a filename holding an invalid UTF-16 sequence, which both
supported Windows standard libraries refuse to convert (measured: MSVC's STL reports no mapping
in the target code page, MinGW's libstdc++ an illegal byte sequence), and, under MSVC's STL
only, a directory named outside the system's active code page, which libstdc++ carries as UTF-8
instead. These are not names you would type; they are names another program can leave where
you are about to look.

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
| reload a weave in place — same shapes, same `WeaveId`, state kept | **yes** — from the Builder, for a weave-only row this project built, Workshop's own pane weaves included in a [development setup](develop-workshop.md); the rebuilt image lands off the loaded file, and promote / revert say which image a restart loads |
| replace a weave whose shape changed, or migrate its state | **no** — a prepared replacement with an authored migration is the maker's, and Workshop does not host one yet |
| reload an artifact that also supplies operators | **no** — refused in words until unmount-and-remount exists |

**Loading is initial and restart intent, and a reload in place changes no row.** The plan
executor has no unload or remount path — that is written down in its own source, not inferred —
and its one reload path keeps the plan's row exactly as authored: what changes is which image the
row runs, never how it participates.

**What realizes a project is now a live thing rather than a call**, and that changes what is
*missing* rather than what is possible. Workshop begins the project and returns to its ordinary
loop; each row settles when its own load answer arrives, and the owner of that work is still
there afterwards, holding the authored plan, a cursor and what each row produced. What that made
reachable is the reload in place above — the object that performs it is still there when the
maker rebuilds — and what is still not offered is an unload, a replacement or a migration.

**Reversible provider overlay is not artifact hot reload**, and the two must not be read as one.
Overlay and unmount are reversible *within the host's operator catalog*: unmounting an overlay
reveals what was underneath, unchanged and unrebuilt. That is a real, proven capability about
**contributions**. It says nothing about replacing a loaded **artifact** while the system runs.
The Loom's `Kernel::reload_from` is what a reload in place spends, through the Manager's
`zen.ReloadWeave`, and its provider-custody caveat is exactly why a provider+weave artifact is
refused: reloading the weave half alone would leave the catalog on the old image. One platform
fact shapes all of this: a loaded artifact's file is **mapped** by the process — Windows refuses
a writer on it and Linux lets a writer change code under the running program — so a rebuilt
product never lands on it, and the image a reload opens is a per-operation copy.

### Info edits the pane grammar that exists, and no more

[Info](panes.md#a-pane-as-a-subject--info) edits exactly the geometry a pane's setup row can say
today — an absolute place (`X`, `Y`), a width and a height — and shows the rest beside it: the
pane's rank in the front order and whether it is on the layout, which are arranging's and the
Pane Manager's to change, and the resolved rectangle and state. It does not author anchors,
fill, docking, snapping, locks, sibling-relative or parent-relative placement, because no pane
can hold those yet; it does not edit what a pane *does*, wire panes together, or reach a loaded
weave's own state; the "inspect" route from outside it is the Pane Manager's row menu
(`inspect in Info`), which asks Info's own door — the right-click menu on a pane itself has no
such row. Its default height is the stack's nine rows, which shows only a
few rows of it at a time; `=` in the desk arrangement makes it usable, and that default is [the same open question](#panes-are-9-rows-tall-by-default-and-a-bigger-terminal-does-not-change-that)
every stacked pane has.

### Info views are four, and their drafts live in the running image

[Info views](info-views.md) are the default Info pane and three more; there is no fifth, and a
closed view's slot is reused rather than a new pane minted. Only the default view inspects pane
properties. Drafts, titles, selections, samples and watches are not saved: a same-shape Info reload
or a Workshop restart starts every view empty. Watching follows one Inventory entry through
Inventory's change notices; Sample source re-asks only a captured structure description and never
watches its source automatically. A sample reports Loom's process-local id of whoever answered, so
an equal id across a restart cannot prove the same incarnation answered.

### Inventory folders organize, and no more

[Named folders](inventory-folders.md) put each entry in exactly one folder. There is no search,
tag, smart folder, Back/Forward history, multi-select or recursive delete: a folder that holds
anything cannot be removed. A folder is not a hotkey context and grants nothing. Info and Compose
do not show where an entry is filed. Folders last as long as the collection, which survives a
restart only through a [toolbox](toolboxes.md).

### The Pane Creator makes one kind of pane, and it is text

[The Pane Creator](panes.md#the-pane-creator--a-pane-made-of-data) is the first pane whose
inside is authored data, and it is exactly that small: one region kind (`text`), one line of
plain ASCII per region, one region seeded per pane, and one open pane definition at a time.
There is no control, no button, no input region, no wiring, no state, no anchors, fill or
nesting, no second region kind, and no way to rename a pane once it is made — its name is its
durable identity. The region is placed by typing numbers into Info's rows for that pane (the
Pane Manager's `n` makes the pane; Info edits it); there is no dragging or resizing of a region
on the pane itself. A code-backed pane's `INTERIOR` is
a read-only capture of its body, never a decomposition of its painter, and a loaded pane's is
its provider's own: the Pane Creator's representation is one way a pane can be built, not a
form every pane must be converted into. And a definition file holds presentation only —
nothing in it can be made to send, sample, bind or load.

### Cross-pane interaction is typed values and references, and no more

Panes that implement Workshop's carry doors exchange owned typed data: an Inventory entry dragged
into Info or Compose, a live reference placed in Info, a field dragged from one Info view into
another or into Compose ([the pane reference](../reference/workshop-panes.md#authorizing-an-input-operation-and-carrying-data)).
Each acquisition needs the actor's permission, and the receiver decides what a drop means. There
is no shared selection across panes and no general protocol for handing one pane an arbitrary
object; a pane that implements none of those doors neither sends nor receives one.

The ownership map a future cross-pane gesture would have to cross is recorded in
[the architecture notes](../architecture/README.md#cross-pane-interaction).

### Panes have no installation story

A pane arrives because an artifact was in the load plan and the weave offered one. There is no
discovery, no plugin directory, no versioning of pane offers, and no way for a maker to install
somebody else's pane other than by editing a plan file and having the artifact on disk.

**Nothing negotiates a pane built for another Workshop.** A shape that changed gains a second
version beside the first, and each side speaks the versions it was built with. For Info that
means: an Info image built before a pane became its subject still asks for the object
document's picture, which this Workshop no longer publishes, so it waits, showing
`OBJECTS (waiting)`, and nothing it sends is written; build Info from this source.

## Another host driving Workshop

- **No prompt per connection.** A guests-file row with `"admit": "ask"` waits for a decision
  and can act on nothing meanwhile; the surface that shows a maker the waiting connection and
  takes the decision is later work ([external host](external-host.md)).
- **A credential crosses the loopback socket in the clear.** The guests file refuses any
  listener but loopback; there is no transport security in the crossing.
- **Injection enters at the Input weave**, below the platform edge: the console reader, the
  SDL queue and the OS are not exercised by it.
- **One session at a time, and no arbitration** between a hand and an agent beyond arrival
  order. A session holds at most 16 keys down at once, from SDL's scancodes 1..511.
- **One picture retained at a time**, fetched in 32 KiB chunks; over 32 MiB is refused.
- **A picture after input waits for what the injection set in motion on Workshop's bus, and no
  more.** An injection asked with `settle` is answered once every delivery it caused has been
  dispatched -- the desk's handling and its repaint among them. Work a participant defers to a
  timer or a later turn is not waited for, and the picture shows the frame it was taken at.

## Platforms

| | state |
|---|---|
| Linux / WSL, GCC 11+ | fully supported; the canonical lane |
| Linux, Clang | builds; not a routine lane |
| Windows, MinGW-w64 GCC 13.1+ (libstdc++) x64 | supported; the required Windows lane, and the build this project is developed on day to day. Runtime DLLs must be on `PATH` or beside the binaries. Binutils 2.40 assembles the Workshop application's translation units since Workshop's bodies were split out of its headers and compiled once (measured 2026-09-03 on the host translation unit); before that it refused the application itself with `file too big`, 2.45 assembled it, and the test suites built either way |
| Windows, MSVC 19.50 (VS 2026) x64 | supported; the advisory Windows lane, and the toolchain released Windows users are expected to build with. clang-cl and ARM64 are unverified |
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
