# Drive Workshop from another host

For a ready-to-use isolated desk with a reusable host and a Reset button, start with
[demo setups](demo-setups.md).

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
inject moments or timed pointer motion, close it), `capture` (surface pictures, visible pane rows
and the screen point of one painted cell), `inspect` (ask any participant what it accepts or to
describe its exposed structure again, `zen.PokeDescribe`, and the guest door for the connection inventory) and
`inventory` (list, add, capture, rename and remove entries, read them, and save against their
revisions, plus acquiring retained Terminal values and picking up typed fields through Info; the legacy capture slot remains available — [the inventory reference](../reference/inventory.md)).
The optional `demo` power reaches the demo service, Workshop setup application and the Info/Compose/Inventory view reset doors; see [demo setups](demo-setups.md). It grants no input, capture or inventory access by itself.
The separate `open` power lets a guest's own gesture reopen a saved file location dropped on the
Editor: the Editor asks Workshop to approve opening a source through the managed opening
(`zengine.opening`) for that gesture, and Workshop asks the guest's grant, as it does for a carry.
It reaches the managed opening and nothing beside it — no save, no build, no Editor door — and the
Editor's unsaved-work floor still stands; the gesture itself still needs `input`. Carrying text or
a location out of an Editor needs `inventory` (the carry), as any carry does
([the source editor](editor.md#carrying-text-commands-and-file-places)).
The separate `toolbox` power permits saving/restoring Inventory toolbox files under the Workshop
process's filesystem access. It grants no execution or input authority. The one-request
[`workshop/toolbox` tool](toolboxes.md#restore-an-executors-test-fixture) uses this power; injected
input also needs it to operate those file controls. Ordinary `inventory` permission is insufficient.
The Inventory → Info interaction requires both `input` and `inventory`: Workshop checks the
initiating input actor for each acquisition, read, and save. A reference grants no authority. A row
may also say `"admit": "ask"`: such a guest waits for Workshop to decide, able to act on
nothing until it does — that is the seam a per-connection prompt will attach to; today the
decision is a suite's or a host's.

Launch with it:

```sh
zengine-workshop --load-plan <workshop dir>/graphical-load-plan.json --guests guests.json
```

```text
zengine-workshop - guests: listening on 127.0.0.1:7654 for 1 guest(s) named in guests.json (door: weave #12; the Connections pane lists them)
```

Open the **Connections** pane from the Pane Manager (`Ctrl+P`). It says `nobody is connected`,
and will say who is, as they come and go.

Both paths below need this step and nothing more from each other — pick one:

## 2. Which path: a session's own tools, or a compiled probe

| A reader wants to... | Read |
|---|---|
| **Start or return to work** | § 1 above (admission, done once), then [§ 3](#3-from-a-loom-session-journeys-as-python-tools) for the session path's own setup, or [§ 4](#4-link-a-host-to-it) for the probe's |
| **Find an existing capability** | [§ 3](#3-from-a-loom-session-journeys-as-python-tools) — `loom-session tools`/`describe` reads the `workshop` package's own tools from its manifest, never by running one |
| **Act on Workshop** | Either path uses the owner doors (`zengine.input`, `zengine.skin`, `zengine.guests`, `zengine.inventory`) admitted by § 1's file; [§ 3](#3-from-a-loom-session-journeys-as-python-tools) speaks them as a tool's `ctx.ask`, [§ 5](#5-run-the-journey) as console `send` |
| **Understand the result** | [§ 3](#3-from-a-loom-session-journeys-as-python-tools)'s own run/crossings account (a session's general answer, [Loom's session guide](https://github.com/Krealsion/Loom/blob/main/docs/guides/sessions.md)), or [§ 6](#6-read-what-happened-from-the-agents-side) for the probe's `history`/`log` console commands |
| **Recover or finish** | [§ 3](#3-from-a-loom-session-journeys-as-python-tools)'s "What a person meets on this route" and "Giving Workshop's input session back", or [§ 7](#7-refusals-and-the-connections-end) for the probe's own refusal words |
| **Extend the harness** | [§ 3](#3-from-a-loom-session-journeys-as-python-tools) — a small editable Python tool, its own `loom-tool.json` manifest, and [Loom's own generic map of this](https://github.com/Krealsion/Loom/blob/main/docs/guides/session-tools-map.md) for the mechanics a new tool shares with every other session's |

**Recommended: the session path, § 3.** No compiler, no restart between an edit and the next
run, and a maintained package (`external-host/tools/workshop/`) already speaks Workshop's
input, capture, connection, and inventory doors. Read § 1 above, then jump straight to [§ 3](#3-from-a-loom-session-journeys-as-python-tools).

For the inventory collection, `workshop/inventory-collect` captures a new named entry and
returns its reference in `entry.json` plus the self-describing `pair.bin`. `workshop/drag`
takes `start` and `end` points (`x,y` pixels or `x,yc` cells), injects one complete primary drag,
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
  chunks, the connection inventory — so a tool's asks are encoded and Workshop's answers
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

**A capability change updates its own manifest help in the same change.** `loom-session
tools`/`describe` reads a tool's accepted inputs, outputs and refusals from `loom-tool.json` and
the tool's own docstring, never by running it — so a maker who only ever reads `describe` sees
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
your profile — never the one you are working in, which an injected chord would type into.

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
into the pane a maker is still looking at: stop that Workshop first, or keep a second, separate
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

Its `loom-tools.json` approves the package:

```json
{ "packages": [ { "path": "<zengine prefix>/share/zengine/loom-tools/workshop",
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
- **The Pane Manager keeps the keyboard.** Once Ctrl+P has opened it, a second Ctrl+P changes
  nothing and the run fails saying so (`what Workshop presents did not change after ctrl+p`);
  press what the Pane Manager answers to instead (`--input chord=down`).
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

`tests/session/workshop_journey.py` is this whole route as a test, behind the `session` gate
([build and test](../contributing/build-and-test.md)): a run left pending at Workshop's Skin while
no client is attached, found finished by a new one; an edit rerun; two runs against one input
holder, on one link and on two; a tool bug and its cleanup; a run CANCELLED while it owns the
input session, whose cleanup closes it and whose successor on the same link opens one again;
Workshop killed while a capture is open; a new session lifetime refusing the old handles.

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
  maker the waiting connection and let them decide is later work. The decision seam is real
  and enforced now.
- **No transport security.** A credential crosses the loopback socket in the clear, which is
  why the guests file refuses any listener but loopback.
- **No arbitration between a hand and an agent** beyond one session at a time: a person at the
  keyboard is still heard while a session is open, and the two sources interleave in arrival
  order.
- **One picture retained** at a time, fetched by chunk; a picture over 32 MiB is refused, never
  cut.

Everything a stranger needs is published: `zengine::input` and `zengine::surface` for the
shapes, `workshop/guest_seam_vocabulary.hpp` for the inventory, and Loom's
`zen/bridge/link.hpp` for the envelope. Nothing in `workshop/` beyond those is reached.

## Visible rows and timed drag stories

The `capture` guest power also permits `PaneViewRequested{provider,pane}` at
`zengine.workshop`. Its `PaneView` answer contains the current picture number and visible text
rows with addressable pointer points and coordinate space. Geometry comes from the painter's
resolved pane body. Closed, unsettled, canvas-backed, overlapping, modal-covered or off-workspace panes refuse.
This reads presentation; it does not select, activate, grant authority or expose arbitrary state.
A later gesture can still encounter a changed picture. Legacy unnumbered panes report picture
zero; the query is not an interaction lease. Receivers must fence their own drops.
`PanePointRequested{provider,pane,picture,row,column}` answers where one painted cell is now, for
the picture the caller read, so a tool presses a control such as `[Save copy]` without knowing a
font; `hand.control(provider, pane, label)` and `hand.field(...)` in `hand.py` use it.

`workshop/drag` uses the session's timed Input motion; `duration_ms` controls time and `bend`
selects a linear path (zero) or cubic Bezier. The [Inventory-to-Compose demo](inventory-compose.md)
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
