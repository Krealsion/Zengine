# Drive Workshop from another host

For a ready-to-use isolated desk with a reusable host, its material, hotkeys and a Reset button,
start with [ready-to-use setups](demo-setups.md).

**Walkthrough.** Let an agent's own Loom host connect to a running Workshop, be admitted as a
guest you named, open an input session, press keys, take a picture of what Workshop presents,
and keep the whole exchange in its own history — with Workshop showing the connection while it
lasts. Two processes on one machine, connected by a loopback socket; no automatic discovery
or connections from other machines.

```
Workshop (--guests file)  <--- loopback socket --->  loom-host (a `links` row + the probe weave)
    zengine.input  an input session, injected moments
    zengine.skin   a picture of the surface, fetched by chunk
    zengine.guests the connection inventory (and the Connections pane)
```

## What you need

- **A built Workshop** — the graphical one for a real picture, or the terminal one for a
  headless run — and its shipped load plan, which now names the Connections pane.
- **An installed Loom** with `loom-host` (its `bin/`), and installed Loom and Zengine packages
  to build the probe against ([the Loom guide](https://github.com/Krealsion/Loom/blob/main/docs/guides/running-loom.md),
  and Zengine's own install).
- **The probe**, built as a stranger: `examples/workshop-probe/` is one weave that runs the
  journey, and its `CMakeLists.txt` reaches both packages by `find_package` alone. Needed only
  for [the compiled-probe path](#4-link-a-host-to-it) below — the session path in § 3 needs no
  compiler at all.

```sh
cmake -S examples/workshop-probe -B build/probe -DCMAKE_PREFIX_PATH="<loom prefix>;<zengine prefix>"
cmake --build build/probe
```

## 1. Say who may connect

Workshop listens for no other host unless you name one. Write a guests file:

```json
{
  "listen": "127.0.0.1:7654",
  "guests": [
    { "name": "agent", "credential": "open-sesame", "may": ["input", "capture", "inspect"] }
  ]
}
```

Each row is one guest *this Workshop knows*: `name` is the name Workshop will establish for
it — whatever the peer claims — `credential` is what the peer must present, and `may` is the
whole of what its session may then say, as explicit powers: `input` (open an input session,
inject moments or timed pointer motion, close it), `capture` (surface pictures, and the desk's
words: the desk in one turn, each pane's visible words and named parts and where one character of
them is, the pane inventory and the keymap --
[the desk, read by message](#the-desk-a-panes-words-and-timed-drag-stories)), `inspect` (ask any participant what it accepts or to
describe its exposed structure again, `zen.PokeDescribe`, and the guest door for the connection inventory) and
`inventory` (list, add, capture, rename and remove entries, read them, and save against their
revisions, plus acquiring retained Terminal values and picking up typed fields through Info; the legacy capture slot remains available — [the inventory reference](../reference/inventory.md)).
The optional `demo` power reaches the demo service, Workshop setup application and the Info/Compose/Inventory view reset doors; see [demo setups](demo-setups.md). It grants no input, capture or inventory access by itself.
The separate `open` power lets a guest's own gesture reopen a saved file location dropped on the
Editor: the Editor asks Workshop to approve opening a source through the managed opening
(`zengine.opening`) for that gesture, and Workshop asks the guest's grant, as it does for a carry.
It reaches the managed opening and nothing beside it — no save, no build, no Editor door — and the
Editor's unsaved-work floor still stands; the gesture itself still needs `input`. That drop
reaches the Editor on a development host alone: on a weaver's host the editor answers only the
weaver's hand ([below](#whose-host-this-is-and-what-each-power-reaches-there)). Carrying text or
a location out of an Editor needs `inventory` (the carry), as any carry does
([the source editor](editor.md#carrying-text-commands-and-file-places)).
The separate `toolbox` power permits restoring Inventory toolbox files, and saving them under the
Workshop process's filesystem access on a development host alone: a save is a file write, which a
weaver's host refuses a guest. It grants no execution or input authority. The one-request
[`workshop/toolbox` tool](toolboxes.md#restore-an-executors-test-fixture) uses this power; injected
input also needs it to operate those file controls. Ordinary `inventory` permission is insufficient.
The Inventory → Info interaction requires both `input` and `inventory`: Workshop checks the
initiating input actor for each acquisition, read, and save. A reference grants no authority. A row
may also say `"admit": "ask"`: such a guest waits for Workshop to decide, able to act on
nothing until it does — that is the seam a per-connection prompt will attach to; today the
decision is a suite's or a host's.

### Whose host this is, and what each power reaches there

A guests file names its own **version**, at the top: `"version": "2"`, written as the file writes
every number, a base-10 string. A file naming no version is version 1, and a version this Workshop
does not read refuses the file in words. Version 2 adds two words:

- **`host`**, a word of the whole file: `"weaver"`, the default, or `"development"`. A **weaver's
  host** is the machine a person works on; a **development host** is an agent's own Workshop,
  started for its work, where its hand may do what the weaver's does.
- **`build`**, a power: the Builder's builds, realizations and loads -- `b`, `B`'s load, `f`, the
  load-built control, adding to the load plan, promote and revert. A guest with it builds only the
  catalog in force, which a guest on a weaver's host cannot change. It grants no message: no guest
  is ever granted `BuildRequested` or a realization ask. A row without it is refused the Builder's
  builds on either host, in words, before anything runs.

`input` reaches every control a key or a press reaches, as the weaver's hand does, but on a
**weaver's host** Workshop refuses a guest's hand, in words, wherever the act would write the
weaver's files, put its words where the weaver commits them, or reach a shell:

- **every file write**: the Editor's save, Files' recipe switch (`u`), recipe authoring and marks,
  the Builder's line that adds to the load plan, Flow's save, generate, export and library save,
  the View Builder's save, Workshop's own `s` and `t`, the toolbox save (by key, by message, or
  relayed through the Composer), and quitting, which writes the last session -- all but the View
  Builder's run record, the project's `view-builder.json`, which it keeps for any hand's Run,
  Stop, New or Open;
- **typed text**, toward any pane or line: what a guest types rests nowhere for the weaver to
  commit as the weaver's own;
- **its keys, text, presses, wheels and drops toward the editor** (the standard Editor or
  Neovim's, whichever holds the office), **the Terminal** and **the Hotkeys pane**, which edits the
  keymap file;
- **opening the guests file**, through Files, the Builder's edit-source or Edit Code, and any
  open a pane relays on its behalf -- the Composer's Submit, a stored command -- whose path
  Workshop does not see.

![A 1440 by 720 Workshop on a weaver's host: the Connections pane says the admitted guest's row is version 2 and names what it does not reach here, file writes first; the Attention pane stands a condition that the guest does not reach everything its powers name; and the band says, in red, that a guest's typed text rests in no pane](images/guest-on-a-weavers-host.png)

On a **development host** none of these is refused: a guest with `input` writes, types and reaches
the editor and the Terminal there as the weaver would, and one with `build` builds. A guest's
Terminal line is then sent by the Terminal as itself, as the weaver's is.

**A version-1 file still mounts**, as a weaver's host, each row keeping its reach less those
refusals; an old row never has `build`. What each row's powers do not reach is said beside the row:
at launch, on the Attention pane, and in the Connections pane, which reads the door's inventory at
version 2 (`GuestConnections` v2: each row's powers, the file's host and version, and its losses).
A guest that asks for that inventory hears every connection and its own row's powers and losses
alone, and none may observe it through the relay. What a row's powers reach past what they once
did is said the same way, at launch and on Attention: a row holding `capture` reads the desk said
whole, the pane inventory and the keymap, which a row written for pictures and pane rows did not,
so the widening stands beside it as a condition (`guests.gain/<n>`, `n` its place in the file,
counted from 0) and is never silent.

Any admitted session may also ask the door for its own row, whatever its powers:
`GuestRowDescribedRequested` (`{}`) is answered by `GuestRowDescribed`, the row the door admitted
that session under -- its `name`, its powers (`may`), its `observe` list, each entry a
`GuestObserve{producer, shape, version}`, and the file's `host` -- never its credential and nothing
of another row. Two rows of one name stay apart: the answer is the row that session came in under.
A session the door never admitted is refused in words. The file is read once, at launch: a changed
row applies at the next launch.

```json
{
  "version": "2",
  "host": "development",
  "listen": "127.0.0.1:7654",
  "guests": [
    { "name": "agent", "credential": "open-sesame", "may": ["input", "capture", "inspect", "build"] }
  ]
}
```

Launch with it:

```sh
zengine-workshop --load-plan <workshop dir>/graphical-load-plan.json --guests guests.json
```

```text
zengine-workshop - guests: listening on 127.0.0.1:7654 for 1 guest(s) named in guests.json (door: weave #12; the Connections pane lists them)
zengine-workshop - guests: the file is version 2, and this is a development host: guests may write and build here
zengine-workshop - guests: 'agent' (version 2) also reaches here: the desk read whole: `capture` also reads the desk's words, the pane inventory and the keymap
```

Its row reaches every power it names there, so no loss is said; its `capture` reaches further than
the power's name once did, and that is said. The version-1 file of
[§ 1](#1-say-who-may-connect), a weaver's host, says what its row does not reach too:

```text
zengine-workshop - guests: the file is version 1, and this is a weaver's host: a guest writes no file, types into no pane and reaches none of the editor, the Terminal and the Hotkeys pane
zengine-workshop - guests: 'agent' (version 1) does not reach here: the Builder's builds and loads: `build` is a version-2 power; file writes: this is a weaver's host; the editor, the Terminal and the Hotkeys pane: they answer only the weaver's hand here; typed text: a guest's text rests in no pane here; opening the guests file
zengine-workshop - guests: 'agent' (version 1) also reaches here: the desk read whole: `capture` also reads the desk's words, the pane inventory and the keymap
```

Open the **Connections** pane from the Pane Manager (`Ctrl+P`). It says `nobody is connected`,
and will say who is, as they come and go.

Both paths below need this step and nothing more from each other — pick one:

## 2. Which path: a session's own tools, or a compiled probe

| A reader wants to... | Read |
|---|---|
| **Start or return to work** | § 1 above (admission, done once), then [§ 3](#3-from-a-loom-session-journeys-as-python-tools) for the session path's own setup, or [§ 4](#4-link-a-host-to-it) for the probe's |
| **Find an existing capability** | [§ 3](#3-from-a-loom-session-journeys-as-python-tools) — `loom-session tools`/`describe` reads the `workshop` and `desk` packages' own tools from their manifests, never by running one |
| **Act on Workshop** | Either path uses the owner doors (`zengine.input`, `zengine.skin`, `zengine.guests`, `zengine.inventory`) admitted by § 1's file; [§ 3](#3-from-a-loom-session-journeys-as-python-tools) speaks them as a tool's `ctx.ask`, [§ 5](#5-run-the-journey) as console `send` |
| **Understand the result** | [§ 3](#3-from-a-loom-session-journeys-as-python-tools)'s own run/crossings account (a session's general answer, [Loom's session guide](https://github.com/Krealsion/Loom/blob/main/docs/guides/sessions.md)), or [§ 6](#6-read-what-happened-from-the-agents-side) for the probe's `history`/`log` console commands |
| **Recover or finish** | [§ 3](#3-from-a-loom-session-journeys-as-python-tools)'s "What a person meets on this route" and "Giving Workshop's input session back", or [§ 7](#7-refusals-and-the-connections-end) for the probe's own refusal words |
| **Extend the harness** | [§ 3](#3-from-a-loom-session-journeys-as-python-tools) — a small editable Python tool, its own `loom-tool.json` manifest, and [Loom's own generic map of this](https://github.com/Krealsion/Loom/blob/main/docs/guides/session-tools-map.md) for the mechanics a new tool shares with every other session's |

**Recommended: the session path, § 3.** No compiler, no restart between an edit and the next
run, and a maintained package (`external-host/tools/workshop/`) already speaks Workshop's
input, capture, connection, and inventory doors, beside a reader (`external-host/tools/desk/`)
that writes the whole desk as text for an agent. Read § 1 above, then jump straight to [§ 3](#3-from-a-loom-session-journeys-as-python-tools).

For the inventory collection, `workshop/inventory-collect` captures a new named entry and
returns its reference in `entry.json` plus the self-describing `pair.bin`. `workshop/drag`
takes `start` and `end` points (`x,y` window pixels, or a terminal cell that says which cells it
means: `x,y@console`, the console's own, or `x,y@canvas`, a canvas cell), injects one complete
primary drag,
and saves before/after pictures. These are searchable tools in the same package. Their input
path retains the guest's authority; dispatch settlement alone does not prove destination success.

**The compiled-probe path, § 4–7**, is the lower-level alternative: one weave, one journey, no
session host to keep running, useful where a single proof run without Python is what a case
needs. It shares § 1's admission and nothing else the session path depends on.

## 3. From a Loom session: journeys as Python tools

The probe (§ 4 below) is one compiled weave running one journey. A **Loom session** —
`loom-host --serve`,
[Loom's sessions guide](https://github.com/Krealsion/Loom/blob/main/docs/guides/sessions.md) —
keeps a host running for clients that come and go, and runs editable Python **tools** as named
runs: edit a tool and the next run uses it, with no compiler and no restart. This repository
ships what such a session needs to speak to Workshop, and nothing of the session itself:

- **`zengine-guest-vocabulary`**, installed in `lib/zengine/`: a weave the session host boots.
  It declares Workshop's guest shapes there — input sessions and injection, captures and their
  chunks, the desk and each pane's words, the pane inventory and the keymap, the connection
  inventory and the asker's own row — so a tool's asks are encoded and Workshop's answers
  re-admitted by identity. It accepts nothing and says nothing.
- **The `workshop` tool package**, installed at `share/zengine/loom-tools/workshop`:
  `workshop/connections` (the connection inventory, and this session's own row),
  `workshop/inspect-capture`
  (the journey below as a tool: inspect, open an input session, click and/or press a chord with
  settlement — optionally clearing a field first and typing into it, or repeating a chord
  several presses in one batch — picture, close, and every failure after the session opened
  closes it first and says both), `workshop/verify-recipe` (reads one recipe back from the
  project's own `build-recipes.json` on the local filesystem and checks named fields against
  it — a LOCAL OBSERVATION of what a recipe-authoring run left behind, never a claim about who
  wrote it or when; pair it with `inspect-capture`'s own settlement for a claim about causation)
  and `workshop/inventory-capture` (asks Workshop's *storage* inventory — a different inventory
  than the connections one above — to capture a named participant's own `zen.PokeStructure`,
  then verifies Get against that capture's own snapshot; [the inventory
  reference](../reference/inventory.md) owns the pair's own contract and encoding).
- **Tools that work the way a weaver's hands do**, in the same package, judged by what Workshop
  says it holds -- each pane's words and the desk's own numbers, which the `capture` power reads
  -- rather than by comparing pictures: `workshop/act` (steps through one input session: press,
  type, open a pane by its Pane Manager row's name, walk a list's cursor to a named row, press a
  part a pane names by its name, press into a pane or on one of its words, in a text or a canvas
  pane, or on a line of the menu on the screen, turn the wheel over a part by its name or over a
  pane's first word, check a pane's place, size, state or keys on the desk by number, expect or
  rule out text, keep a pane's words and named parts or a picture -- a cropped PNG if asked),
  `workshop/nvim-edit` (edits to one file typed through Workshop's [Neovim pane](neovim.md) and
  saved by Neovim; before typing, Neovim itself is asked whether its buffer is exactly that file,
  unmodified and equal to the disk -- unsaved work, a draft never saved included, is refused
  untouched -- and after `:w` whether it saved the planned text, which the file must equal too),
  `workshop/place` (a pane's place and size written through Info, one field at a time, each
  row chosen by the name Info gives it),
  `workshop/builder` (the [Builder](builder.md)'s keys, each followed to its owner's answer: a
  build to its operation's ending on `last` and, when it asked for one, that operation's
  realization ending on `realize` -- two answers kept apart; the ask and operation are kept to
  the end, so another build's ending is SUPERSEDED rather than taken for this one; a wait that
  runs out first is UNRESOLVED and names the operation), and `workshop/inventory-organize` and
  `workshop/inventory-controls` (names, folders and portable controls, through Inventory's owner
  operations). Two more are local
  observations like `workshop/verify-recipe`: `workshop/source` (list, read, search or compare
  files) and `workshop/lane` (this repository's build, `tests/verify.cmake` or one test case,
  with the logs kept). `workshop/repo` does a checkout's Git and pull-request chores as the
  session's user: status, diff, staging named paths, a commit as a named author with no trailer,
  pushing a branch, and opening or reading a pull request and its checks; it never merges and
  never force-pushes. [The tower defense example](../../examples/tower-defense/README.md) was
  made with them, and replays how.
- **The `desk` tool package**, installed at `share/zengine/loom-tools/desk` beside `workshop`:
  `desk/read`, which reads the whole desk under the guest's own row and writes it as text files
  for a model, `AGENTS.md` first ([the desk written as text](#the-desk-written-as-text-for-an-agent)).
  It is a package of its own because `workshop` already holds as many tools as Loom lets one
  package hold.

**Steps kept with a setup.** A [ready-to-use setup](demo-setups.md) may carry named **walks** --
lists of `workshop/act` steps, the same steps its `starting` list uses -- and
`python external-host/demo.py walk <name> --root <root> --pictures <folder>` replays one against
that setup's running root, in a window or in a terminal, and puts the pictures it takes in the
folder named, beside `steps.json` ([replaying a setup's walks](demo-setups.md#replay-a-setups-walks)).
A walk worth keeping goes in its setup rather than in a scratch file, so the next reader replays
it instead of pressing it again.

**A capability change updates its own manifest help in the same change.** `loom-session
tools`/`describe` reads a tool's accepted inputs, outputs and refusals from `loom-tool.json` and
the tool's own docstring, never by running it — so a weaver who only ever reads `describe` sees
exactly what shipped when the two are edited together, and a stale description when they are not.
Widening or narrowing what a chord, a field or an input accepts is the same kind of edit as
adding one: the manifest and the tool's own help text are part of the capability, not paperwork
after it.

**Try a complete story:** [author and check a recipe through an ELH](elh-recipe-journey.md)
provides a small reproducible multi-config fixture, the Files/menu/field sequence, refusal and
saved-field checks, and a returning-client route using these tools.

**Which Workshop.** Any Workshop that named this host in a guests file ([§ 1](#1-say-who-may-connect)):
one you already have open, if you started it with `--guests`, or one started for the purpose. A
Workshop launched without a guests file listens for no host at all, so attaching to a running
application stays that application's own decision, written in its file before it started. For
proof runs, start a Workshop of the task's own with `--isolated`, which reads and writes none of
your profile — never the one you are working in, which an injected chord would type into — and
name it a development host in its guests file: the tools that type, save, edit through Neovim,
place a pane through Info or load what they build (`workshop/act`'s typing, `workshop/nvim-edit`,
`workshop/place`, `workshop/builder`'s load-it) are refused a guest's hand on a weaver's host.
`workshop/builder`'s builds need the row's `build` on either host.

**Building while a proof Workshop is running.** A running `zengine-workshop` keeps every pane DLL
its load plan named memory-mapped for as long as it runs — `zengine-files.dll` and the rest stay
locked open by the OS, not merely "in use" by Workshop's own bookkeeping. A rebuild's own staging
step (copying a freshly linked DLL beside the host) fails outright against a locked file, on
Windows with an explicit "Error copying file"; the fix is never to work around the lock, only to
stop what is holding it — **by identity, not by every process sharing a name.** A machine this
task does not own exclusively can have another Workshop already running (the founder's own, a
parallel task's) that a name-wide stop would kill without warning. Identify the specific process
this task itself launched — its pid, recorded when you started it, or looked up from the port your
own `guests.json` names (on Windows, `Get-NetTCPConnection -LocalPort <port> | Select
OwningProcess`) — and stop only that:

```powershell
# Windows, PowerShell -- stop the one process THIS task's proof launched, by its own pid
Stop-Process -Id <pid this task recorded> -Force
```

then rebuild, then relaunch. This also means a source edit made *because* a live proof surfaced
something — the very case this walkthrough exists for — cannot be tested by editing and rebuilding
into the pane a weaver is still looking at: stop that Workshop first, or keep a second, separate
checkout building while the first stays untouched for the picture already taken. A practical split
that avoids most of this friction: keep the source checkout's own build tree for compiling, and
run proof instances from a build you already staged and are done editing — a rebuild in progress
in one tree never locks a DLL a different, already-built tree loaded. A Workshop relaunched after
such a rebuild is a **new process**; whether it gets a **new loopback port** is `guests.json`'s own
`listen` value, not the `--isolated` flag by itself (below, "a clean restart") — and, if the load
plan's own weaves changed shape, a new runtime that the previous one's guests file and session
state do not carry forward. Record which source revision and which running process a proof
actually used (a build's own commit/diff state, the launched executable's path, and the port or
lifetime a `status` answered) rather than assuming a later reader can reconstruct it from the
walkthrough alone; this repository's own reportbacks are where that record belongs, not this
guide.

The session directory's `loom-boot.json` boots the run manager and the vocabulary, and links that
Workshop (`.dll` for `.so` on Windows):

```json
{
  "boot":  [ { "name": "runs",  "path": "<loom prefix>/lib/loom/loom-runs.so", "role": "loom.runs" },
             { "name": "vocab", "path": "<zengine prefix>/lib/zengine/zengine-guest-vocabulary.so" } ],
  "links": [ { "name": "workshop", "connect": "127.0.0.1:7654",
               "identity": "agent-session", "credential": "open-sesame" } ],
  "history": {
    "log": "session.log",
    "payload_budget": "67108864",
    "retain": [ { "shape": "SurfaceCaptureChunk", "last_n": "1", "in_recent": false,
                  "retain_payload": false },
                { "shape": "loom.link.Crossed", "last_n": "256", "retain_payload": true } ]
  }
}
```

Its `loom-tools.json` approves the packages, `desk` beside `workshop` (leave it out where no
agent reads the desk as text):

```json
{ "packages": [ { "path": "<zengine prefix>/share/zengine/loom-tools/workshop",
                  "approve": "any-revision" },
                { "path": "<zengine prefix>/share/zengine/loom-tools/desk",
                  "approve": "any-revision" } ] }
```

Start it with `loom-host --serve work`, and complete the run-manager trust, answer and worker
permissions in [Loom's first-start guide](https://github.com/Krealsion/Loom/blob/main/docs/guides/sessions.md#2-start-a-session-and-decide-what-it-may-do).
That includes explicitly starting `runs` after approving it. Detached startup does not grant
those permissions automatically. Add these Workshop-specific decisions at the same console:

```text
loom> authority trust vocab
loom> authority allow runs loom.link.Ask v1 -> role loom.link.workshop
loom> authority allow runs loom.link.StatusRequested v1 -> role loom.link.workshop
```

`trust vocab` lets the vocabulary run in the host. The two `allow` lines are the ceiling of what
the run manager may pass on to a run, and the package asks for exactly them; what the link's
session may say to Workshop is still Workshop's guests file. These three lines are recorded to
disk (`loom-authority.json`, beside `loom-boot.json`) the moment you type them, so they outlive
this one console.

**Finishing the first start: two ways, pick one.** The interactive console above is one running
host; a later `loom-session start` is a *different* host process over the *same* session
directory, and two hosts cannot serve one session directory at once (`loom-session.lock`
refuses the second). So either:

- type `start vocab <zengine prefix>/lib/zengine/zengine-guest-vocabulary.so` right here at this
  same console, and this session is now live, still attached to this console; or
- `quit` this console to end the bootstrap host cleanly, then start the session detached
  elsewhere (`loom-session start work`) — its own boot walk reads the now-recorded trust and
  boots `vocab` on its own, with no console and no re-typing the grant.

Either way, the trust granted above is what makes it possible; typing it is not itself a start.

**What actually starts vocab, and confirming it did.** The authority lines above are a grant,
not a start: `vocab` boots because `loom-boot.json`'s own `boot` array names it, and that boot
runs every time a host starts serving this session (the interactive console's own start, or a
later detached `loom-session start`) — the trust grant is what a boot needs to succeed, done
once, not the boot itself. **An admitted link is not proof the vocabulary booted or can encode a
tool's asks, only that the loopback connection itself succeeded.** Loom's own host mounts links
*before* it runs the boot walk (`host_main.cpp`, "LINKS BEFORE THE BOOT WALK, so a boot row's
weave finds its link's office already held when it is told it is live") — the two happen in that
order regardless of whether `vocab` goes on to boot cleanly, so `loom-session status work`'s
`link workshop -> ...: admitted as '<name>' (far session N)` line, by itself, says only that
Workshop accepted the connection. The confirmation that `vocab` is actually live and can encode a
tool's own asks is a **run that reaches Workshop and answers**, e.g.
`loom-session run work workshop/connections --name first-connections --wait 30` passing live; a `link` line reading `lost`
or missing outright is the one thing that unambiguously means the connection itself failed, and
the fix there is almost always in `guests.json` (the name and credential Workshop admits) or in
whether Workshop is listening at the address `loom-boot.json`'s `links` names.

**A clean restart, for a `loom-boot.json` edit or a Workshop that moved.** A session already
running does not reread `loom-boot.json` on its own — editing the `connect` address (Workshop
restarted with `--isolated` picks a fresh loopback port unless one is fixed in its own
`guests.json`, so a later launch's port is not the earlier one) changes nothing about a session
already serving. `loom-session status work` still shows the link's *old* address, `lost`, until
the session is stopped and started again:

```text
$ loom-session stop work
the session was asked to end; it ends after this turn
$ loom-session start work
serving work at 127.0.0.1:<new port> -- lifetime <new lifetime> (pid <new pid>)
```

This is also the moment the session's own **lifetime** changes — expected, not a fault, and
exactly what [Loom's own session guide, § 8](https://github.com/Krealsion/Loom/blob/main/docs/guides/sessions.md#8-when-something-goes-wrong)
says of "after the host ends": a restarted host is a new lifetime, nothing resumes, and what a
clean `stop` leaves behind on disk is what the next session inherits. Worth noting in any record
of a run that crosses a restart, so a later reader is not left assuming one continuous session
where there were two.

```text
$ loom-session run work workshop/inspect-capture --name look --input chord=ctrl+p --wait 90
run 00cc5c97/look -- passed (live)
  tool workshop/inspect-capture, revision b28f848fd620
  summary: far session 30 ('agent'); 1 connection(s); input session 1; 2 moment(s) settled; frame 64 -> 70 (image/bmp, 936x264)
  artifact connections.json: verified, 255 bytes, sha256 03fcd04af8bf1640
  artifact before.bmp: verified, 741366 bytes, sha256 932df57fe40fc7cb
  artifact after.bmp: verified, 741366 bytes, sha256 a4b2369f7d645bdf
  ask 4 loom.link.StatusRequested v1 -> loom.link.workshop: answer loom.link.Status
  ask 7 GuestConnectionsRequested v1 -> zengine.guests via workshop: answer GuestConnections
  ask 11 InputSessionRequested v1 -> zengine.input via workshop: answer InputSessionOpened
  ask 14 SurfaceCaptureRequested v1 -> zengine.skin via workshop: answer SurfaceCaptured
  ask 16..60 SurfaceCaptureChunkRequested v1 -> zengine.skin via workshop: answer SurfaceCaptureChunk (23 asks)
  ask 64 InjectInput v1 -> zengine.input via workshop: answer InputInjected
  ask 67 SurfaceCaptureRequested v1 -> zengine.skin via workshop: answer SurfaceCaptured
  ask 69..113 SurfaceCaptureChunkRequested v1 -> zengine.skin via workshop: answer SurfaceCaptureChunk (23 asks)
  ask 117 InputSessionClosed v1 -> zengine.input via workshop: answer zen.Ack
  note: cleanup close input session 1: done
$ loom-session crossings work look
run look: worker session 15; 75 remembered deliveries to it -- 8 answered its asks or crossed a link
  seq 231 SurfaceCaptured v1 corr 67 from #7 <- crossing: link workshop epoch 1, far session 30 as 'agent', attempt 28, far sender #16, answer [the tool asked SurfaceCaptureRequested via workshop: answer]
  seq 222 InputInjected v1 corr 64 from #7 <- crossing: link workshop epoch 1, far session 30 as 'agent', attempt 27, far sender #0, settled [the tool asked InjectInput via workshop: answer]
  ...
  and 67 other deliveries to it: 1 loom.runs.Directive v1 from #8, 66 zen.Ack v1 from #8 (--json lists each)
```

(Trimmed: the directory, the inputs and the worker's notes.) The pictures are in the run's
`out/` directory, checked whole by the tool and verified on disk by the run manager. `crossings`
reads the session host's own history: each answer that came through the link names the crossing
it descends from — which far session, established as whom, which attempt, which far author — as
the link recorded what arrived. The retention above keeps one picture chunk and the crossings'
bytes, and the payload budget is what decides how far back those can be read: chunks are most
of a run's traffic.

What a person meets on this route:

- **Every run on one link is one participant to Workshop.** Workshop admits one input-session
  holder, and the holder it sees is the link's far session, not the run — so a second run on the
  same link is refused `busy: ... held by you`. A second participant is a second guests row and a
  second link.
- **Ctrl+P toggles the Pane Manager.** A press opens it with the keys; a second press closes it
  again, wherever the keys are ([panes](panes.md#showing-going-to-and-hiding--the-pane-manager)).
  Press Ctrl+P only when the Pane Manager is not already on the desk (`workshop/act`'s `open` step
  checks first, then presses the row by its name), then press what it answers to
  (`--input chord=down`).
- **A lost link is an unknown outcome.** When Workshop goes away after a tool's request was
  submitted, the run fails saying the outcome is UNKNOWN and nothing was resent; Workshop's guest
  door closes a lost guest's input session, and the run never claims it closed it.

### Giving Workshop's input session back

Workshop holds one input session at a time and it is the LINK's far session that holds it, so a
run that ends without closing one leaves every later run on that link refused `busy: ... held by
you` — for as long as the link lives, which is as long as the session host does. What gives it
back is the tool's cleanup (`ctx.on_cleanup`, registered the moment the session is opened), and
the three ways a run can end are not the same here:

| how the run ended | what happens to Workshop's input session |
|---|---|
| it finished, failed, or the tool raised | the cleanup closes it, and the run records the Input owner's own answer |
| `loom-session cancel` | the cleanup still runs — a cancellation ends the tool's work, not its giving back — and the next run on the same link opens a session normally |
| `loom-session cancel --force` | nothing of the tool runs: the session stays held, and the run says so rather than claiming a close |
| the worker itself died (the run is `crashed`) | nothing of the tool runs either -- there is nobody left to ask -- so the session stays held exactly as after `--force`. `cancel` on such a run stops what the worker left running; it does not give anything back |
| the link is lost | nothing can be closed from here; Workshop's guest door closes a lost guest's session, which is the only thing that does |

A cleanup line reading `cleanup close input session <n>: done` is the Input owner's answer, not
an attempt; anything else names what actually happened. If a `--force` left one held, ending the
link — stop the session host, or let Workshop drop the guest — is what releases it, because the
holder Workshop knows is the far session and not the run.

### Following what an owner says, instead of reading its pane

A pane's rows say what was painted; they cannot say *which* build a key started, a covered pane
says only what shows of it, and a closed one nothing at all. Workshop lets a guest **observe** its participants' own
publications through Loom's observation relay ([Loom's observation
reference](https://github.com/Krealsion/Loom/blob/main/docs/reference/observation.md)), which it
mounts at `loom.observe` beside the guest door. A row may observe exactly what its `observe` list
names — one entry per office, shape and version, the version written as Zen's JSON writes an Int:

```json
{ "name": "agent", "credential": "...", "may": ["input", "capture", "inspect"],
  "observe": [ { "producer": "zengine.builder", "shape": "BuildAsked",  "version": "1" },
               { "producer": "zengine.builder", "shape": "BuildStatus", "version": "4" },
               { "producer": "zengine.realization", "shape": "RealizationAsked", "version": "1" },
               { "producer": "zengine.realization", "shape": "ArtifactRealized", "version": "3" },
               { "producer": "zengine.realization", "shape": "ArtifactPromoted", "version": "2" } ] }
```

- **Nothing else implies it.** `input`, `capture` and `inspect` are not observation, and a row
  without `observe` may not even ask the relay. **Observing grants nothing to act with**: a guest
  that sees the Builder's words still builds only by pressing its keys, with its `input` and
  `build` powers.
  The one thing it may say is a read: a row that observes the Builder's `BuildStatus` at the
  version the Builder publishes (v4) may ask the Builder for the current one
  (`BuildStatusRequested`), answered to it alone — the baseline a returning observer joins. A row
  naming another version is shown nothing of that picture, and may not ask for it either.
- **What carries the desk's words needs `capture` too.** A publication carrying a pane's or
  Workshop's words or the picture -- `SurfaceCanvas`, `SurfaceText`, `TranscriptShown`,
  `PaneSubjectShown`, `PaneInventory`, `KeymapShown` and `StandingConditions`, from whichever
  office publishes it -- is refused, in words, to a row without `capture`, whatever its `observe`
  list names; a row with `capture` still observes only what that list names. `ClipboardCopy`, the
  weaver's copied text, is refused to every row: it needs a power of its own, `clipboard`, which
  no guests file grants yet.
- **Subscribe before you press, and press with settlement.** A publication the press set in
  motion carries the run's own correlation for that press (`cause`); `workshop/builder` finds the
  `BuildAsked` its own press caused, keeps that ask's number and follows `BuildStatus` for it to
  the build's ending and, separately, the realization's. A status about a later ask before its
  ending is SUPERSEDED, never taken for it. From the press on, the pane may be covered, closed or
  resized. The Builder's words: [builder](builder.md#what-an-observer-is-told).
- **Promote and revert are the realization owner's to answer** (`zengine.realization`). Their
  press causes `RealizationAsked` — the owner's word that it took the ask as number N, or refused
  it and why — and the answer names N: `ArtifactPromoted` in the same delivery, `ArtifactRealized`
  for a revert only when its reload settles, which is why the number and not the press's `cause`
  joins them. A status that merely reads `promoted:`, or another ask's answer about the same
  artifact, never completes this press.
- **Coming back to an operation.** A wait that ran out names the operation and the Workshop run
  that numbered it: `act=look op=N relay=<lifetime>` subscribes, asks the Builder where it stands,
  and follows op N's build and realization separately to their endings — completed before it
  asked, or later. It presses nothing, needs no `input` power and no visible Builder pane, and
  never issues another build. It decides nothing before joining the Builder's answer with every
  word that arrived with it: the answer may already speak of the next ask, and op N's ending may
  be among those words. An answer about an ask the Builder has not numbered yet (`op` 0) decides
  nothing about op N until that ask's runner answers, so the look waits for the Builder's next
  word; running out of time there is UNRESOLVED, naming op N and the relay, like any other wait.
  If the Builder has moved to another operation or ask without op N's ending in any picture, or
  the relay is not the one that numbered op N (Workshop restarted and counts afresh), it says it
  cannot establish op N here instead of adopting another operation.
- **What it costs.** Every run on one link is one far subscriber: at most 8 subscriptions at
  once for it, a window of words standing unacknowledged per subscription, and every loss said as
  a `Gap`. A count that loses words is unknown, not smaller.
- **How it ends.** The run's cleanup releases its subscriptions; a lost link ends them `lost`;
  a guest that disconnects has them forgotten. The host can withdraw a guest's subscriptions
  (`revoke`, told as `Ended` `revoked`) — a host seam today, like `decide` for an `ask` row: no
  weaver control calls it yet.

**A monitor** is a run that plays an application and watches its owner's words under a policy.
The reusable part is the `workshop` package's `monitor.py`; a policy is its own small package
beside the application it knows, whose manifest says `"uses": ["workshop"]` ([Loom's sessions
guide](https://github.com/Krealsion/Loom/blob/main/docs/guides/sessions.md#a-package-that-builds-on-another)),
approved in `loom-tools.json` beside the `workshop` package. [The tower defense
example](../../examples/tower-defense/README.md#watch-it-play-under-a-policy) ships one:
`tower-defense/monitor` plays the game (or watches it), counts enemies crossing a checkpoint cell
from the game's own `TdOccurred` words, and ends FINISHED, NEEDS ATTENTION (pausing the game
through the run's own input session, confirmed by the game's word) or INCONCLUSIVE, with its
evidence in `monitor.json`. Start it, keep its handle (the session lifetime and the run name),
and come back: a client that stops waiting leaves the run to its manager, and `wait`, `show`
and `cancel` reach it by name. Nothing here wakes an agent; the run's ending is what a returning
client reads.

`tests/session/workshop_journey.py` is this whole route as a test, behind the `session` gate
([build and test](../contributing/build-and-test.md)): a run left pending at Workshop's Skin while
no client is attached, found finished by a new one; an edit rerun; two runs against one input
holder, on one link and on two; a tool bug and its cleanup; a run CANCELLED while it owns the
input session, whose cleanup closes it and whose successor on the same link opens one again;
Workshop killed while a capture is open; a new session lifetime refusing the old handles. Before
its processes start it runs the tools' own checks against scripted panes, `builder`'s completion
and `nvim-edit`'s confirmation among them, and `desk/read`'s against a scripted Workshop
(`tests/session/desk_read_checks.py`): `AGENTS.md` written first, each word and part where its
reading says, and an `out` inside a checkout refused with nothing written. Beside it, `tests/session/story_journey.py` holds [the
tower defense story](../../examples/tower-defense/README.md)'s own custody to the same processes
-- runs its wait gave up on, a Workshop and session host it must see end, and a watcher's session
it reuses while it runs and replaces only once it is seen to end -- and
`tests/session/neovim_journey.py`, behind `session-neovim` (the session tooling and a named
Neovim at once), drives `workshop/nvim-edit` against a real Neovim.

## 4. Link a host to it

**The compiled-probe path, continued from § 1.** In an empty directory of the agent's own,
`loom-boot.json`:

```json
{
  "boot":  [ { "name": "probe", "path": "/abs/path/to/build/probe/zengine-workshop-probe.so" } ],
  "links": [ { "name": "workshop", "connect": "127.0.0.1:7654",
               "identity": "agent-from-elsewhere", "credential": "open-sesame" } ],
  "history": {
    "log": "probe-run.log",
    "retain": [ { "shape": "SurfaceCaptureChunk", "last_n": "1", "in_recent": false,
                  "retain_payload": false },
                { "shape": "loom.link.Crossed", "last_n": "8", "retain_payload": false } ],
    "keep":   [ { "shape": "InputInjected" }, { "shape": "SurfaceCaptured" },
                { "shape": "GuestConnections" }, { "shape": "loom.link.Outcome" },
                { "shape": "loom.link.Crossed", "cap": "256" } ]
  }
}
```

```text
$ loom-host
loom-host 0.1.0   containment: ...
  history: durable stream probe-run.log
  link workshop -> 127.0.0.1:7654: admitted as 'agent' (session 20; office loom.link.workshop, weave 5)
  refused  probe
              admission refused at open: no standing decision for 'probe' (...); approve it at the console with:  authority trust probe
  0 started, 1 refused by policy  -- boot INCOMPLETE
```

Two things happened and one did not. The link connected and Workshop **admitted it as
`agent`** — the name in *Workshop's* file, not the `identity` the host claimed; the
Connections pane now shows `#1 agent (claims 'agent-from-elsewhere')  admitted  weave 20`.
The probe did not run: being named in a boot plan is not permission to execute native code.

```text
loom> authority trust probe
loom> authority allow probe loom.link.Ask v1 -> role loom.link.workshop
loom> authority allow probe loom.link.StatusRequested v1 -> role loom.link.workshop
loom> start probe /abs/path/to/build/probe/zengine-workshop-probe.so
```

The two allow lines let the probe query its link and send requests through that one office.
The authenticated Status answer establishes which link answers it and which far session belongs
to it. What that *session* may say to Workshop is Workshop's guests file; neither grant widens it.

## 5. Run the journey

```text
loom> weaves
  weave 5  accepts: loom.link.Ask v1 loom.link.StatusRequested v1 ...
  weave 6  accepts: RunProbe v1 InputSessionOpened v1 InputInjected v1 GuestConnections v1 SurfaceCaptured v1 ...
loom> send 6 RunProbe 1 link=workshop text= scancode=19 modifiers=2 inspect=zengine.guests picture=workshop-probe.bmp
  sent.  no answer yet (ask 3; 'asks' to see it)
loom>
  [ask 3 settled] zen.Result v1 from weave 6
  PASS: link 'workshop' admitted as 'agent' (far session 20); session 1 opened by zengine.input;
  injected 2 moment(s), session seq 1..2, settled on Workshop's bus;
  zengine.guests lists 1 connection(s); this session is 'agent' (claimed 'agent-from-elsewhere',
  weave 20); capture 1 at frame 100: 1920x1009 image/bmp, 5811894 bytes;
  picture written to workshop-probe.bmp (5811894 of 5811894 bytes, image/bmp, read back whole);
  session 1 closed
```

The console names every field (`send` composes from the schema, not from the struct's
defaults): `scancode=19 modifiers=2` is Ctrl+P, `scancode=81 modifiers=0` a bare Down arrow,
`text=hello` types after the chord, `picture=<file>` says where the picture lands. The answer
arrives when the journey settles, on a later console turn. Every step in that line is what
the far **owner** answered — the Input weave's `InputInjected`, the guest door's inventory, the
Skin's `SurfaceCaptured` — never something the probe inferred from a picture or from being
admitted. Open `workshop-probe.bmp`: it is what the graphical Skin presented, read back from
its own renderer after the frame that followed the chord — the Pane Manager the chord opened is
in it.

**What a picture proves, and what it does not.** It is evidence of *presentation* at one frame,
in the medium's units (`cell_px` maps its pixels to the canvas lattice a pointer moment is
spelled in). The probe sends injection with `loom.link.Ask.settle=true`: the link waits for
both the Input owner's answer and Workshop's dispatch fence before handing that answer back.
The fence follows the injection and messages synchronously queued by its dispatch descendants,
including the consumer's repaint. Only then does the probe request the current presentation.
This is a causal ordering guarantee for that bus work, not a delay or an assumption about FIFO.
It does not wait for timers, asynchronous child-process replies, another host, or all background
work, and settlement alone does not say whether the intended application operation succeeded.
Those owners still need their own completion answers. A terminal Skin hands back its cells
(`text/cells`), a window its pixels (`image/bmp`); either is what that medium itself drew.

**What injection tests, and what it does not.** An injected moment enters at the Input weave
and is published as the same `KeyPressed`, `TextEntered` and `PointerButton` every consumer
already accepts, in order, from the same producer — so everything from the bus onward is the
real thing. The platform edge — the console reader, the SDL queue, the OS — is not exercised;
only a hand on a device is. Each injected batch has at most 64 events, with scancodes 1..511
and at most 16 keys held across batches. Validation checks the whole prospective held state
before publishing anything; an invalid batch is refused whole.

## 6. Read what happened, from the agent's side

```text
loom> history
  retained 107 (105 recent, 1 protected, 11 last-call over 11 shapes), forgotten 86, observed 192, ...
loom> history last InputInjected
  Retained: #14 [LastCall|Recent] seq=13 #5 -> #6 InputInjected v1 Delivered corr=2 ... payload=Retained/35B
loom> history last SurfaceCaptured
  Retained: #18 [LastCall|Recent] seq=17 #5 -> #6 SurfaceCaptured v1 Delivered corr=4 ... payload=Retained/58B
loom> history tallies
  SurfaceCaptureChunk  observed 137  recorded 137  declined 0  last-call held 1
  loom.link.Ask  observed 142  recorded 142  declined 0  last-call held 1
  ...
loom> history payload 14
loom> log
  probe-run.log: selected 7 of 312 observed, appended 8, 1921 bytes, ...
loom> log read
  ... BusObservation InputInjected v1 ...
  ... BusObservation SurfaceCaptured v1 ...
```

The picture chunks are observed but kept one deep without payload and outside recent context.
Crossing records retain their metadata without payload in recent context and eight deep in the
last-call window. The durable log keeps the first 256 crossing **records**, including their
payloads, then records that its per-shape cap was reached. It does not mean 256 bytes per record
or a rolling log. Other selected shapes continue. These are host policy choices, not promises
that every crossing or a whole picture will remain available.

For a missing answer inside Workshop, its own log matters too. The demo launcher enables
`--log-refusals`; after stopping that run, use `zengine-workshop --read-log <run>/workshop.log`.
The [pane authoring guide](../guides/make-a-workshop-tool.md#find-a-missing-reply-grant) explains
how to connect a denied reply to retained request history, and the limits of that evidence.

For an investigation that needs recent remote provenance decoded in the console, set that
Crossed row's `retain_payload` to `true` and choose a suitable `last_n`; `history.payload_budget`
sets the recorder's total byte budget (for example, `"8388608"` for 8 MiB). Picture chunks occur
inside Crossed payloads too, so more retention costs memory. Forgotten or unretained bytes remain
reported as such. The durable log's independent selection can preserve them beyond memory.

The recorder is the host's bounded working memory; the log is what it chose not to forget
([Loom history](https://github.com/Krealsion/Loom/blob/main/docs/reference/history.md)). These
are observations of **local deliveries**, not observations of execution on the remote bus.
The link first says `loom.link.Crossed` to itself with the far session, attempt, remote author,
answer kind and payload. Its resulting typed answer names that crossing as its dispatch parent.
`history last loom.link.Crossed`, `history payload <retained-id>` and `log read` let the operator
follow this evidence within the selected retention. The typed answer has the local ask's
correlation and Loom answer authority; an ordinary participant saying the same words cannot
settle the ask. The link authenticates its own crossing record before using it.

The five outcomes a crossing can have are kept apart in that record: an owner's answer (its
own shape), the link's `loom.link.Outcome` — `refused` (dropped before Workshop's bus),
`dispatch-refused` (Workshop's bus said no: a power the row does not grant), `unlinked` (no
session), `lost` (the link went down after the send: unknown, and nothing is resent) — and
silence, which has no shape and leaves the probe's own book open.

## 7. Refusals, and the connection's end

A run reports PASS only after the matching connection row, attributable owner answers, complete
capture transfer, successful file write/flush/close and input-session closure. A reachable failure
after opening input first attempts closure; STOPPED reports both the original reason and the
cleanup outcome. An unwritable `picture` path therefore stops rather than passing with no file.
A refused or silent cleanup remains refused or pending. A lost link is unknown; the probe does
not replay the action or claim it observed closure. The guest door owns disconnect cleanup.
After observed closure, another `RunProbe` can open a fresh session.

Connect with a wrong credential and the link is told in Workshop's words:

```text
  link workshop -> 127.0.0.1:7654: denied -- no guest of this Workshop presents that credential ('links connect workshop' tries again)
```

Nothing of it remains on Workshop's bus. Quit `loom-host` while a session is open and the
Connections pane shows the row `closed` once, then drops it; the guest door closes the input
session on the guest's behalf, and a key the session still held down comes up from the Input
weave itself. A second host connecting afterwards is a new session under a new weave id — a
late answer to the old one reaches nobody.

## What this does not do yet

- **No prompt per connection.** An `"admit": "ask"` row waits; the surface that will show a
  weaver the waiting connection and let them decide is later work. The decision seam is real
  and enforced now.
- **No transport security.** A credential crosses the loopback socket in the clear, which is
  why the guests file refuses any listener but loopback.
- **No arbitration between a hand and an agent** beyond one session at a time: a person at the
  keyboard is still heard while a session is open, and the two sources interleave in arrival
  order. Each hand's gestures are its own -- one's key never makes the other's pending act stale
  -- but a guest's key still lands where the weaver's keys are, and every control a key or a press
  reaches beyond the refusals above (a layout's removal, the desktop's launch and close, the
  clipboard) answers a guest on a weaver's host too.
- **Admission does not ask where bytes came from.** On a weaver's host a guest builds only the
  catalog in force, which the weaver's hand sets, but a load is still admitted by trusting every
  artifact, and an agent with a shell as the weaver's own user still writes the weaver's files
  directly, outside the desk.
- **One picture retained** at a time, fetched by chunk; a picture over 32 MiB is refused, never
  cut.

Everything a stranger needs is published: `zengine::input` and `zengine::surface` for the
shapes, `workshop/guest_seam_vocabulary.hpp` for the inventory, and Loom's
`zen/bridge/link.hpp` for the envelope. Nothing in `workshop/` beyond those is reached.

## The desk, a pane's words, and timed drag stories

The `capture` guest power also permits the readings below at `zengine.workshop`, each answered
from what Workshop owns and none read off a picture. It reaches further than rows written for
pictures and pane rows once did -- the desk in one turn, a pane's paged reading, the pane
inventory and the keymap -- and Workshop says so beside each such row, at launch and on Attention
([whose host this is](#whose-host-this-is-and-what-each-power-reaches-there)).

- **The desk in one turn.** `DeskReadRequested`, `{}`, is answered by `DeskRead`: the desk as
  `DeskView` version 3, every presented pane's stamp from the front back, and as many of those
  panes' readings (`PaneView` version 4), in that order, as one decoded value holds -- Loom's
  decode budget, <!-- value kDecodedCellBudget grouped -->65,536<!-- /value --> cells. A stamp with
  no reading beside it -- past the last one, or of a pane whose reading Workshop refused -- names a
  pane to ask alone. `DeskView`
  version 3 adds Workshop's own words to version 2: the band's notice and legend, each a word with
  its place and no point, since the band takes no press; the status slot, a word with no place,
  as the medium is handed it; and `desk`, a number that moves whenever what the desk says moves
  and holds while nothing does. Workshop answers at most
  <!-- value kDeskReadsPerSecond -->4<!-- /value --> desk reads a second to one asker -- and every
  run on one link is one asker -- and refuses the next in words naming how many milliseconds to
  wait.
- **A pane's reading, paged under its stamp.** `PaneViewRequested` version 4,
  `{provider,pane,from,stamp}`, is answered by `PaneView` version 4: version 3's words and then its
  parts from item `from`, as many as one decoded value holds, `total` counting both, and the stamp
  the reading stands on -- the holder of the pane's office, that holder's incarnation, the canvas
  room `grant` its picture was drawn for (0 for a text pane) and the `picture`. Ask from 0 with an
  empty stamp, then each next page from where the last ended, under the stamp the first page
  answered: a page asked under a stamp the pane no longer stands on is refused as stale, even when
  the new picture's number equals the old, and the pane is read again from 0. A repaint moves the
  picture; a resize, a move, a title row shown or hidden or a metric renewed moves the room grant;
  a reload in place moves the incarnation. A cover that moves between pages shifts the items
  `from` counts and leaves the stamp alone: compare each page's `total` and `covered` with the
  first page's, and read the pane again from 0 when they differ; a `from` past a reading a cover
  shrank is refused in words saying so. A partly covered pane says its visible words and parts
  only, and `covered` names what covers it (a pane in front by its name, `menu`, `arranging`,
  `refused mark`, `band`), how many words and parts it did not say, and the rectangle they lie in;
  a covered word is never given a point. While a menu is open a primary press outside it reaches
  nothing beside it -- it closes the menu, or is refused -- so nothing said beside it has a point.
  A pane whose newest picture is still in flight -- a room just granted that no picture has
  answered included -- says `in_flight` and no word, at any page: ask it again a moment later. A
  closed pane, one not presented, one whose last update Workshop refused -- nothing of it
  standing, or the picture a new room asked for -- and one not yet drawn for its room and waiting
  for its provider are refused in words. Layouts reads here too, as `zengine.workshop` `layouts`,
  its tabs parts Workshop names (the table below).
- **The pane inventory and the keymap.** `PaneInventoryRequested` and `KeymapRequested`, answered
  to an office, are answered to a guest session whose row holds `capture` too, to it alone:
  `PaneInventory`, the panes the Pane Manager lists with each one's state, and `KeymapShown`, the
  bindings in force.
- **The desk.** `DeskViewRequested` version 2, `{}`, is answered by `DeskView` version 2: every
  pane on the desk, in the desk's order, with its state (`open`, `covered`, `off-room`,
  `unresolved`), its rank from the front (0 in front), the place its authored intent resolves to
  and the part of it the canvas has, and whether it is selected or holds the keys; the room;
  whether arranging is open; and the menu on the screen, if one is -- Workshop's own or a pane's
  -- with each line where it is drawn, and the lines it names. Version 1 says the same without the
  names.
- **A pane's words and parts.** `PaneViewRequested` version 3, `{provider,pane}`, is answered by
  `PaneView` version 3: a text pane's rows, or a canvas pane's labels and runs of measured text as
  it drew them last, each a word with its place and the point a press names it by; and beside them
  every part the pane names -- a row, a control, an element -- under the pane's own name, with the
  characters it covers, its place and its point: a place of its own, where a press reaches it as
  the pane reads a press, or none for a part every place of which another part takes. A canvas
  picture that takes no press -- the one a managed opening shows, until its pane draws its own, or
  one its office's holder no longer holds -- gives no word or part a point. A pane keeps
  a part's name across its redraws, so a walk finds it wherever the last redraw put it ([the parts
  a pane names](../reference/workshop-panes.md#a-pane-names-its-parts)). Version 2 answers the words
  alone, and version 1 a text pane's rows, as text fitted to the body rather than as drawn; a pane
  drawing its own picture refuses version 1 in words. Versions 1 to 3 refuse a covered pane whole.
- **Where one character is.** `PanePointRequested` version 2,
  `{provider,pane,picture,word,column}`, answers where one character of one word is now, for the
  picture the caller read; version 1, `{provider,pane,picture,row,column}`, one painted cell of a
  text pane, and a pane drawing its own picture refuses it; version 3, the same fields, one cell of
  a pane's text lattice -- a text pane's painted cell, as version 1 answers it, or the cell of the
  lattice a canvas pane's room sets its text on, a blank one or the one after a row's last
  character too. So a tool presses a control such as `[Save copy]`, or the View Builder's `[Label]`,
  without knowing a font.

![A 1440 by 900 window after a walk read by messages: the View Builder, at 96,102 and 840 by 240 as the desk says, made label1 when its [Label] was pressed by what it says; Info, holding the keys, has Height chosen by select and shows the 240 px written through it](images/desk-read-by-message.png)

![A 1440 by 900 window after a walk by names: the View Builder, opened by its Pane Manager row's name, made button1 when kind:button was pressed, and selected it when element:button1 was pressed where the redraw had moved it; control:run ran the view my.view, in front, and element:button1 pressed in it says button1 says nothing yet](images/parts-pressed-by-name.png)

![Two overlapping buttons pressed by name in a 1440 by 900 window, each where its pane gives it the press: above, button2 lies over button1's centre and the View Builder selects button1 when element:button1 is pressed, on the part of it button2 leaves; below, the running view my.view says button1 says nothing yet when its element:button1 is pressed](images/overlapping-parts-pressed-by-name.png)

Every place is in canvas pixels: a window's pixel is one, a terminal's cell is `kCanvasCellPx`
(<!-- value kCanvasCellPx -->12<!-- /value -->), and a terminal's console counts `kTuiCanvasTopRow`
(<!-- value kTuiCanvasTopRow -->2<!-- /value -->) rows above the canvas. A point to press is in the space the medium
reads input in, which the answer names. Versions 1 to 3 refuse the words of a closed, unsettled,
overlapping, modal-covered or off-workspace pane, and a pane's while Workshop's mark over its
refused picture covers part of it; version 4 says what shows of a covered pane and names what
covers it; the desk still answers. This reads presentation; it
does not select, activate, grant authority or expose arbitrary state. A later gesture can still
encounter a changed picture. Legacy unnumbered panes report picture zero; the query is not an
interaction lease. Receivers must fence their own drops. `hand.words`, `hand.word_point` and
`hand.desk` in `hand.py` ask them; `hand.row` finds a row by what it says and `hand.part` a part by
its name through them, wheeling the pane toward each edge when asked to scroll, `hand.last_part`
the lowest part of a kind saying a text (a Terminal's newest value), `hand.first` a pane's first
word, `hand.control` a `[label]` by its word, `hand.spot` a text by the word holding it (an
Inventory crumb, or `[Up]`), `hand.row_starting` the topmost row starting with a text, pressed at
its third character as `hand.row` presses (inside an Inventory view's first box), and `hand.field`
an Info view's field by its part `field:<path>`, wheeling the view's selection until it shows, so
a tool reads a pane that draws a picture as it reads a text pane; `act.rows_by_place` numbers a
pane's lines by where they stand, a blank row a canvas draws as no word among them. `hand.point`
asks `PanePointRequested` version 3 for one cell of a pane's text lattice by its row and column --
a text pane's painted cell, or the cell of a canvas pane's lattice, a blank row's and the one after
a line's last character too -- so a tool presses a canvas pane's cell as it presses a text pane's;
`hand.lattice_point` chooses the cell from the pane's words, reading the pane again where it
redrew between the reading and the point. A canvas picture that takes no press -- the one a managed
opening shows, until its pane draws its own, or one its office's holder no longer holds -- says its
words and parts with no point, and every helper that presses reads the pane again until one does
(`hand.pressing`), each reading for at most ten seconds.
`workshop/act` steps on them: `at` presses one cell of a pane's text lattice the same way
(`{"at": ["td.game", "td", 5, 17]}`); `desk` checks a
pane's place, size, state or keys by number (`{"desk": [provider, pane], "is": {"visible": {"w":
480}}}`); `part` presses a part by its pane and its name (`{"part": ["zengine.view.builder",
"view-builder", "kind:label"]}`), and fails rather than press one with no point -- `part`,
`into`, `click`, `at`, `wheel` and `control` wait out a picture that takes no press, for their
`seconds`; `open` presses the Pane Manager's row named for a pane
(`{"open": "pane:zengine.files/project-files"}`),
choosing it first where it is not chosen; `menu` presses the line of the menu on the screen named
so, or holding some text; `wheel` turns the wheel once, by `dy` and `dx` notches (`dy` 1 away from
the weaver, -1 toward), over a part by its name or over a pane's first word (`{"wheel":
["zengine.introspection", "loaded"], "dy": -1}`), which a canvas pane hears as its own wheel; and
`click`, `control`, `expect` and `select` read a canvas pane's words as a text pane's, and `open`
and `select` read a row it draws as several runs as one line. `select` walks a list to the row its
pane names so (Info's `property:Height`), or, in a pane naming none, the row whose text after a
one- or two-column marker is the name, a value the list sets beside it after two blanks. Info's
sizes follow the medium: a window's are pixels (`240 px`), a terminal's cells (`20 cells`), and
Info refuses an amount typed in the other unit.

**The names each shipped pane gives its parts.** A walk names a part as its pane does: a name says
what the part means, never where it is drawn, and a pane keeps it wherever a redraw puts it, in a
window and in a terminal alike. A place a pane names nothing is still pressed as the pane reads a
press there, and is never said.

| Pane | Office and pane | Its parts | It names nothing on |
|---|---|---|---|
| Pane Manager | `zengine.desktop` `launcher` | `pane:<office>/<pane>`, a pane's row, and `mark:<office>/<pane>`, the mark inside it (`[open]`, `[    ]`, `[load]`, `[gone]`) | the heading, the more-above and more-below markers, the notes |
| Hotkeys | `zengine.desktop` `hotkeys` | `binding:<group>/<id>/<key>`, a binding's row (`binding:<group>/<id>` where it has no key), and `line`, the spelling line while one is open | the heading, a group's heading, the markers, the footer, a binding's key cell |
| Loaded | `zengine.introspection` `loaded` | `weave:<library>`, a loaded weave's row | the heading, the omission marker, the caveat, the source line |
| Project | `zengine.introspection` `arrangement` | none: nothing in it is acted on | everything |
| Powers | `zengine.introspection` `powers` | `control:sources`, `control:operators`, `control:composite`, `control:sample`, and `power:<identity>`, a power's row | the `find:` field, the markers, the detail, a retained sample, the census |
| Attention | `zengine.attention` `attention` | `condition:<key>`, a condition's row | the glance, the notice, a condition's explanation |
| Connections | `zengine.connections` `connections` | none: nothing in it is acted on | everything |
| Demo | `zengine.demo` `controls` | `control:reset`, the reset row, and `status`, the state beneath it | the setup's name |
| Info | `zengine.info` `info` | `pane:<office>/<pane>`, a pane's row, and `property:<label>`, a property's row; while it shows a value, an Info view's `control:<id>` and `field:<path>` | the rows naming neither; while it shows a value, what an Info view names nothing on |
| An Info view | `zengine.info` `info.2` to `info.4` | `control:<id>`, a control, and `field:<path>`, a field (`field:meta[<n>].<path>` for a capture's metadata) | the title, the state and detail rows, a notice, the markers, a hint, the line being typed and its label |
| Files | `zengine.files` `project-files` | `entry:<name>`, `candidate:<name>`, `field:<name>`, `control:<id>` | the headers, a notice, the markers and the `more fields` row, an empty listing or its refusal, a strip's gaps and `+N in menu` |
| Builder | `zengine.builder-pane` `builder` | `recipe:<name>`, `control:<id>`, `line` | the header, a notice, the facts it reports but the recipe row, the list's heading and markers, the `loads` row, the reader's header and every output line, a strip's gaps and `+N in menu` |
| Inventory | `zengine.inventory-pane` `inventory` | `entry:<owner>:<entry>`, `folder:<owner>:<id>`, `crumb:<owner>:<id>`, `control:up`, `control:here` | the heading; on the location row `(Up)` at the root, the separators, an elided crumb and the count in views; the markers, a notice or hint, the line being typed and its label |
| An Inventory view | `zengine.inventory-pane` `inventory.<n>` | `entry:<owner>:<entry>`, a box holding an entry | the heading, the boxes' borders and an empty box, the selected label, the counts, a notice or hint, the line being typed and its label, `Resize` where no box fits |
| Terminal | `zengine.terminal` `terminal` | `entry:<observation>`, `line`, `newest`, `candidate:<what it says>`, and `legend`, the legend row where its layout shows it | a notice, the heading, the marker above the view, an entry carrying no value, the list's heading, the history row |
| Composer | `zengine.composer` `compose` | `message:<name> v<version>`, `field:<name>`, `control:submit`, `control:back` | the rows naming nothing |
| Editor | `zengine.editor` `editor` | `status`, and `line:<n>`, a document line | |
| Neovim's Editor | `zengine.editor` `editor` | `status` | Neovim's own screen |
| A running view | its own office, `view` | `element:<id>` | |
| View Builder | `zengine.view.builder` `view-builder` | `control:<action>`, `kind:<kind>`, `box:<field>`, `box:<id>.<field>`, `list:<id>`, `canvas`, `element:<id>`, `size:<sx>,<sy>`, `handle:<id>.<sx>,<sy>`, `show:<id>.<field>`, `<action>:<id>` | a place whose name repeats or is refused |
| Flow | `zengine.flow` `flow` | `node:<id>`, `port:<id>.<n>`, `control:<action>`, and every other place by its action and arguments, `<action>:<argument>:...` (`found:<kind>:<identity>`, `source-field:<name>`, `emitted-row:<n>`) | a place whose name repeats or is refused |
| A pane's menu | its presenter's | each line by its row's id | |
| Workshop's own menu | `zengine.workshop` | each line by the action or the group it shows (`workshop.manage`, `setup.restore`, `Order`) | |
| Layouts | `zengine.workshop` `layouts` | Workshop names them, in `PaneView` version 4 and a desk read alone: `layout:<name>`, a layout's tab -- a byte of the name outside printable ASCII, and `%`, `#` and `+`, spelled `%XX`, and a name two layouts share followed by `#` and its place in the layout order, counted from 1 -- and `layout:+`, the tab that makes one | what stands beside the tabs (the layout's file and status), the room's size, the name being typed (while one is, no tab is named) |

`workshop/drag` uses the session's timed Input motion; `duration_ms` controls time, `bend`
selects a linear path (zero) or cubic Bezier, and `button` is the one held through it (left,
middle or right; left unless named); `hold` names modifiers held through it (`alt`,
`ctrl+shift`), and then the drag moves in sixteen straight steps the tool injects, since Input's
timed motion carries no modifier; `held.bmp` is Workshop at the end of the motion with the
button still down, between `before.bmp` and `after.bmp`. `workshop/act`'s `rest` step moves the pointer to a
window pixel or a terminal cell (`x,y@console` or `x,y@canvas`) with no button held, so a canvas pane that asks for its hover is told where
it rests. The [Inventory-to-Compose demo](inventory-compose.md)
finds visible rows and verifies owner state without screen-coordinate constants. Its helpers
live in `external-host/tools/workshop/hand.py`; interpolation and schema/authority decisions
remain in Zengine. Existing input-only guests acquire no inventory operation authority.

Portable Inventory boxes and row/column views use `InventoryViewsRequested` and
`InventoryViewEdit` at `zengine.inventory-pane`, included in the guest `inventory` power, as are
the [named folder](inventory-folders.md) doors at `zengine.inventory` (`InventoryFolderCreate`,
`InventoryFolderRename`, `InventoryFolderMove`, `InventoryFolderRemove`, `InventoryFile`) and
versions 2 of `InventoryAdd` and `InventoryList`.
This authorizes configuration, not execution of arbitrary stored messages. Each invoked command
still needs the input actor's actual target/shape permission. The
[portable-slot demo](inventory-slots.md) is available as `workshop/inventory-slots-demo` in the
same maintained tool package.

The [Terminal capture story](terminal.md#dragging-a-command-or-reply-into-inventory) is available
as `workshop/terminal-inventory-demo`. It captures actual submitted and received data, creates a
preset, fills it from the saved reply through Info, and checks execution authority separately.
Use the `presets` demo setup, Reset before a repeat, and a fresh short ASCII word for `label`.

The [independent Info views](info-views.md#the-inspection-workbench) story is `workshop/workbench`:
`phase=restore` restores the packaged inspection-workbench toolbox, `phase=story` runs the story
through visible controls and `phase=prepare` regenerates the toolbox. Use the `workbench` demo
setup; its guest needs `input`, `capture`, `inspect`, `inventory`, `toolbox` and `demo`. Its
[organized variant](inventory-folders.md#the-organized-workbench) adds `variant=organized`,
`phase=folders`, `phase=retrieve`, `phase=menus` and `phase=organize`, with the `folders` setup. The
phases depend on each other, so run each with `--wait` (the recipe there says why).

## The desk written as text for an agent

`desk/read`, the one tool of the `desk` package, reads the whole desk under your guest row and
writes it for a model to read, as plain text files in one directory of its own. Approve the
package in `loom-tools.json` beside `workshop` ([§ 3](#3-from-a-loom-session-journeys-as-python-tools));
a [ready-to-use setup](demo-setups.md) approves both. Name each read anew, since a run name already
held answers with that earlier run:

```sh
loom-session run work desk/read --name desk-1 --input out=/home/me/agent-desk --wait 60
```

- **What it needs.** The row's `capture`, under which the desk read, each pane's pages, the pane
  inventory and the keymap are answered; its own row is answered whatever its powers. And a
  Workshop that speaks `DeskRead`, `PaneView` version 4 and `GuestRowDescribed`. `link` names the
  session's link to that Workshop, `workshop` unless you say otherwise.
- **`out` is judged before anything is asked.** It must be an absolute directory outside every
  source checkout: one that is, or lies under, a directory holding `.git` is refused, since a
  coding agent loads an `AGENTS.md` it finds in a checkout as standing instructions. A file, a
  directory holding an `AGENTS.md` this reader did not write, or one whose `panes` is a file or a
  link, is refused too. A refused `out` asks nothing and writes nothing, and is the one way the
  run fails.
- **`AGENTS.md` first.** Before any ask, it writes the reader's own fixed words -- the files, the
  coordinates, the stamps, how to press a part, the tools, best practice, and a link to
  `guide.md` -- and nothing any pane, office or guest said. The run's result names its path first,
  so it is the agent's first read.
- **Then it asks**, through the link: the guest door for your own row, Workshop for the desk in
  one turn, each pane past it alone and page by page under its stamp, then the pane inventory and
  the keymap.
- **Then the readings, beside `AGENTS.md`.** `guide.md`, the guide in sections by purpose, every
  text the desk said quoted in a block after a line naming who said it, as data; `desk.txt`, the
  sparse desk -- its first lines, Workshop's own words, the status slot, the menu while one is
  open, then each pane from the front back: a header with its stamp, each word `x,y text` at its
  middle row and each part `name@x,y` at its press point, `name@-` where it has none, what covers
  the pane, or why it was not read; `glance.txt`, the desk drawn at the medium's own text cell, for
  where things are, paged by row band past
  <!-- value desk_render.DENSE_CELLS grouped -->262,144<!-- /value --> cells; and
  `panes/<office>.<pane>.txt`, each pane's own lines beneath the desk's first line -- `~2` and on
  before `.txt` where two references would name one file, and every page opening with its pane's
  header line.
- **Each read replaces the last.** Every reading file is replaced whole, so a reader of it sees the
  last reading or this one, never half of either, and a reading file this read did not write again
  -- a pane gone from the desk -- is removed. A file past
  <!-- value desk_render.SECTION_BYTES KiB -->64<!-- /value --> KiB continues in `<name>-2` and on,
  each page saying where it continues and where it came from, and each page file one pane's; a
  block of what the desk said that a page ends inside is closed there and opened again on the
  next, after the line naming who said it.
- **A refusal is written, never raised.** A pane Workshop refused is written `not read:` in
  Workshop's words. A page refused as stale or past a reading that shrank, or answered under
  another cover, starts its pane again from the first item, and one answered in flight a tenth of
  a second later; a pane whose picture is in flight is asked again a tenth of a second later; each
  a few times in a row before the pane is written not read. When the desk number moved before the
  last page, the desk is read again, and a reading that still moved says the span of desk numbers
  it stands on; a pane read on a later picture than the desk read's says so. A desk read refused for the rate is asked once more after the wait
  Workshop names. The tool's `describe` says the rest, its recovery words included.
- **It presses nothing and changes nothing in Workshop.** To act on a reading, press a part at the
  point it lists (`workshop/inspect-capture`'s `click`), or by name with `workshop/act`'s `part`
  step, which reads the pane again by version 3 as it presses: a part only version 4 says -- a
  Layouts tab, or one of a partly covered pane -- is pressed at its listed point.
