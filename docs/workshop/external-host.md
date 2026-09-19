# Drive Workshop from another host

**Walkthrough.** Let an agent's own Loom host connect to a running Workshop, be admitted as a
guest you named, open an input session, press keys, take a picture of what Workshop presents,
and keep the whole exchange in its own history — with Workshop showing the connection while it
lasts. Two processes on one machine; nothing is discovered automatically and nothing crosses a
network.

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
  journey, and its `CMakeLists.txt` reaches both packages by `find_package` alone.

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
whole of what its session may then say, as three powers: `input` (open an input session,
inject moments, close it), `capture` (a picture of the surface, by chunk) and `inspect` (ask
any participant what it accepts, and the guest door for the inventory). A row may also say
`"admit": "ask"`: such a guest waits for Workshop to decide, able to act on nothing until it
does — that is the seam a per-connection prompt will attach to; today the decision is a
suite's or a host's.

Launch with it:

```sh
zengine-workshop --load-plan <workshop dir>/graphical-load-plan.json --guests guests.json
```

```text
zengine-workshop - guests: listening on 127.0.0.1:7654 for 1 guest(s) named in guests.json (door: weave #12; the Connections pane lists them)
```

Open the **Connections** pane from the Pane Manager (`Ctrl+P`). It says `nobody is connected`,
and will say who is, as they come and go.

## 2. Link a host to it

In an empty directory of the agent's own, `loom-boot.json`:

```json
{
  "boot":  [ { "name": "probe", "path": "/abs/path/to/build/probe/zengine-workshop-probe.so" } ],
  "links": [ { "name": "workshop", "connect": "127.0.0.1:7654",
               "identity": "agent-from-elsewhere", "credential": "open-sesame" } ],
  "history": {
    "log": "probe-run.log",
    "retain": [ { "shape": "SurfaceCaptureChunk", "last_n": "1", "in_recent": false,
                  "retain_payload": false } ],
    "keep":   [ { "shape": "InputInjected" }, { "shape": "SurfaceCaptured" },
                { "shape": "GuestConnections" }, { "shape": "loom.link.Outcome" } ]
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
loom> start probe /abs/path/to/build/probe/zengine-workshop-probe.so
```

The second line is the whole of what the probe may say: one envelope, to one office. What the
link's *session* may say to Workshop is Workshop's guests file, and nothing here widens it.

## 3. Run the journey

```text
loom> weaves
  weave 5  accepts: loom.link.Ask v1 loom.link.StatusRequested v1 ...
  weave 6  accepts: RunProbe v1 InputSessionOpened v1 InputInjected v1 GuestConnections v1 SurfaceCaptured v1 ...
loom> send 6 RunProbe 1 link=workshop text= scancode=19 modifiers=2 inspect=zengine.guests picture=workshop-probe.bmp
  sent.  no answer yet (ask 3; 'asks' to see it)
loom>
  [ask 3 settled] zen.Result v1  from weave 6  PASS: session 1 opened by zengine.input; injected 2 moment(s),
  session seq 1..2; zengine.guests lists 1 connection(s); this session is 'agent' (claimed 'agent-from-elsewhere',
  weave 20); capture 1 at frame 100: 1920x1009 image/bmp, 5811894 bytes; picture written to workshop-probe.bmp
  (5811894 of 5811894 bytes, image/bmp); session 1 closed
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
spelled in). The probe asks for it *after the frame it last saw*, and the bus's order puts the
chord's consumer's repaint before the request that followed its answer — so the picture shows
what that input did to the screen. It says nothing about asynchronous work still pending, and
it is not a second renderer: a terminal Skin hands back its cells (`text/cells`), a window its
pixels (`image/bmp`), and either is what the medium itself drew.

**What injection tests, and what it does not.** An injected moment enters at the Input weave
and is published as the same `KeyPressed`, `TextEntered` and `PointerButton` every consumer
already accepts, in order, from the same producer — so everything from the bus onward is the
real thing. The platform edge — the console reader, the SDL queue, the OS — is not exercised;
only a hand on a device is.

## 4. Read what happened, from the agent's side

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

The 137 picture chunks were observed and, by the plan's `retain` row, kept one deep with no
payload and out of recent context -- `history recent` shows the asks around them and not the
flood -- which is what "bounded" means here: the recorder's recent window forgot 86 records
and the log took 8.

The recorder is the host's bounded working memory; the log is what it chose not to forget,
selected by shape in the boot plan. Both are Loom's own
([history](https://github.com/Krealsion/Loom/blob/main/docs/reference/history.md)); what the
agent's host records are **its own deliveries** — the link's speech to the probe, carrying the
far owners' answers — with the far session and correlation intact. Receiving `SurfaceCaptured`
here is evidence the Skin answered; it is not an observation of the Skin's execution, and the
host does not relabel it as one.

The five outcomes a crossing can have are kept apart in that record: an owner's answer (its
own shape), the link's `loom.link.Outcome` — `refused` (dropped before Workshop's bus),
`dispatch-refused` (Workshop's bus said no: a power the row does not grant), `unlinked` (no
session), `lost` (the link went down after the send: unknown, and nothing is resent) — and
silence, which has no shape and leaves the probe's own book open.

## 5. Refusals, and the connection's end

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
