# Workshop panes and setups — reference

**Reference.** The exact contracts behind Workshop's panes: the pane system, the authored pane
window, the external pane protocol, and the persisted setup. This is the page a *tool author*
needs; a weaver wants [panes](panes.md) and [setups](setups.md), and the
task-shaped walkthrough is [making a Workshop tool](make-a-workshop-tool.md). What Workshop does
with each shape it accepts -- its answer, to whom, and each refusal in its words -- is
[Workshop's manual](zengine.workshop.md); this page holds what spans them.

Source: [`workshop/pane_vocabulary.hpp`](../pane_vocabulary.hpp) ·
[`workshop/pane_carry.hpp`](../pane_carry.hpp) ·
[`workshop/pane_picture.hpp`](../pane_picture.hpp) ·
[`workshop/pane_settings.hpp`](../pane_settings.hpp) ·
[`workshop/setup.hpp`](../setup.hpp) ·
[`workshop/panes.hpp`](../panes.hpp) ·
[`workshop/arrangement.hpp`](../arrangement.hpp) ·
[`workshop/screen.hpp`](../screen.hpp).

> ⚠ **Three things these contracts were written around have retired, and the rationale below
> still names them where it was argued with them.** The prototype **object canvas** and its
> document — a room of authored rectangles, saved and opened with `Ctrl`+`S`/`Ctrl`+`O` — is
> gone: the room is empty, and an old `workshop.json` is named once at startup and left alone.
> The **terminal overlay** is gone: the Terminal is a pane. And the **`p` picker** is gone with
> the host's own **Pane Manager**: presence is two doors on the desktop seam
> ([below](#the-pane-system)), the shipped desktop's Pane Manager spends them, and a pane as a
> subject is Info's (`InspectPaneRequested`, `PaneCommitRequested`,
> `workshop/inspection_seam_vocabulary.hpp`). Where a paragraph below says *picker*, read the
> Pane Manager over those two doors; where it says *document* or *object*, it describes a room
> that is empty now.

## The pane system

> A weave may provide a tool; a **pane** is its presentation.

The **Pane Manager** — the desktop's pane, `Ctrl`+`p` — lists the one inventory Workshop says
out loud and opens a pane through Workshop's launch door, or closes one through the layouts'
close door. A pane that is not in the
catalog cannot be opened by any gesture at all, and the catalog has **two halves**: a
compile-time constant array of Workshop's own, and a bounded **session-local runtime catalog**
of panes some office actually offered this run (see
*[A weave may offer a pane](#a-weave-may-offer-a-pane)* below). The first two built-ins were
chosen to be unalike:

| kind | presents | behind it |
|---|---|---|
| `Builder` | one known build target, and how its build is going | a weave holding `zengine.builder` |
| `Info` | the `OBJECTS` list and the `PROPERTIES` inspector | nothing — the document and the session |

⚠ **`Builder` is a LOADED pane now**, and the row above is history rather than this build's
catalog: it arrives by a plan row naming `zengine-builder-pane`, exactly as `Files` does, and
Workshop compiles nothing for it. It is left in the table because the pair is what the two
paragraphs below argue with, and because a reader who finds the pane on their screen should be
able to find out where it went. The built-in half of the catalog is `Layouts` alone. `Info`
and `Editor`, which were built-ins when this table was written, are loaded panes now
(`zengine-info-pane`, `zengine-editor-pane`), and so is the host's old `Pane Manager`, as the
desktop's `zengine.desktop/launcher`; a setup naming any of them under `zengine.workshop` is
converted at read.

⚠ **`Attention` is a loaded pane that was never a built-in one.** What is currently true used
to be an OVERLAY: a global chord opened it, it owned the keyboard while it was up, it was drawn
into a popup Workshop resolved for itself, and no saved setup could name it. It arrives by a
plan row naming `zengine-attention-pane` now, and it is in the Pane Manager, on the desk and in
the setup like anything else. What it shows it does not derive: Workshop publishes every current
condition as a value and the pane presents them — the one host-to-pane sentence this protocol
has gained since panes began declaring their actions.

⚠ **`Connections` is a loaded pane over a fact the host's guest door owns.** It arrives by a
plan row naming `zengine-connections-pane` and shows the other hosts connected to this Workshop
— each row what the peer *claimed*, what this host *established*, whether it is admitted,
waiting on a decision, refused or gone — as `zengine.guests` publishes the inventory and
answers a presenter that asks. It reads `GuestConnections` v2: beneath each connection a
guests-file row admitted, it says that row's version and what its powers do not reach here
(`version N -- not here: <losses>`, or `version N -- all its powers reach here`), and its heading
ends `-- a development host` when an admitted connection's row stands on one: the inventory says
the file's host with each row it describes. It holds no copy, declares no actions and makes no decision;
the admission seam a per-connection prompt will attach to is the door's
([external host](../../external-host/docs/external-host.md)).

`pane == weave` is deliberately **not** an architectural rule, and `Info` is what pays for
that sentence rather than asserting it: opening it sends no message, asks no office and needs
no weave mounted anywhere, and it has no per-pane state for a close to destroy. A Workshop
hosting no tools at all opens it and it works.

**Presence is the desk's, and two doors change it** (`workshop/desktop_seam_vocabulary.hpp`):

```text
PaneLaunchRequested{office, pane}  ->  Workshop: open it, or focus it if it is open   (never toggles)
PaneCloseRequested{office, pane}   ->  zengine.layouts: take its row off the live layout (unloads nothing)
```

Only an office may ask either, and each is answered to the asker; a close's answer names the
settings and locks its row took. The shipped desktop's Pane
Manager spends them on `Enter` and `x`, over the one inventory Workshop publishes
(`PaneInventory`), with each row's state beside its name — `[open]`, closed, `[load]`
(still to come: the run has not settled the plan row that loads it), `[gone]` —
because a door whose current state is invisible is a gesture a weaver has to guess at. (The `p`
picker owned presence with ONE door in both directions — select a closed kind to open it, an open
one to remove it — until it retired. Launching was built never to toggle for exactly that reason,
and closing became a door of its own. `x` is the Pane Manager's own key; command mode still binds
nothing to it.)

**`Info` is open at boot**, and it was not always a pane at all: originally `paint`
drew them unconditionally, and the only way to not have them was to edit `paint`. What the
migration moved is where they are painted from; what a weaver sees at boot is byte-identical.

- **The pane is not the tool.** The Builder pane holds a *copy* of the last `BuildStatus` the
  Builder tool published, and removing the pane destroys the copy and nothing else. Reopening
  it sends `builder::StatusRequested` and shows the tool's own answer — including `asks N
  ever`, the tool's running count, which comes back as 3 rather than as 1 and is the number a
  pane that owned the state could not produce.
- **Workshop gained two sentences and later gave them back.** Its grant used to add
  `StatusRequested` and `BuildRequested`, both scoped *to the Builder office*; the pane is a
  loaded weave now and says them in its own image, so this host holds neither. The only build
  anything here can ask for is still the one the tool has already named — a pane that has not
  heard from its tool cannot ask for anything, and says so.
- **There are two places, they are named, and there is no layout policy**. A kind
  DECLARES its place in the catalog — `placement::kOverlayStack` or `placement::kTopBand` —
  and one function turns a place plus a screen into the rectangle that pane occupies:

  ```text
  pane kind  ->  placement intent (panes.hpp)  ->  placement_bounds()  ->  the painter is
                                                                           handed that rect
  ```

  The **overlay stack** is anchored to the canvas's top-left, stacked downwards — the terminal
  overlay's mechanism pointed at the other corner — and it covers the top of the material a
  weaver is building. It is **48 cells plus half the room's surplus over that, floored**, and the
  room is the whole surface — 63 cells at the 78×22 minimum, 74 at 100 columns of surface, 124
  at 200, 344 at 640. *A wider room is shared
  by the pane and the weaver* — the same half-share the terminal overlay takes at the other
  corner — so the columns the pane does not take stay reachable at every extent, and the ones
  it does take are its own for paint **and** for the pointer. Its height, its column, its row
  and the blank row between slots do not move with the surface. A desk row's **place word**
  stands its pane at a point of the room instead -- `top-right`, `center`, `bottom-left` and the
  rest, or `share`, a share of the room's width and height from its top-left -- reserved out of
  nothing, with the workspace running underneath -- and two panes at one
  point is a desk saying so rather than an accident.
  A slot is earned by being *placed in the stack*, so an `Info` ahead of a `Builder` in the open
  list never pushes it down a slot it does not occupy. `bounds_of(panes, kind, screen)` is the
  one path to an open pane's bounds — a closed one answers with an empty rectangle rather than
  with the place it would have had. When each painter carried its own column instead, the
  two places existed only as agreement between them; what a third kind costs now is a catalog
  row and a painter, neither of which is geometry. Docking and tabs are still absent (a pane is
  dragged and sized by arranging, below), and what using two unalike panes felt like is the
  evidence for whichever of them gets built.
- **A visible pane occupies pointer space, not only pixels**. Bounds resolved in one
  path made the question sayable and the measured answer was that nobody asked it: a press on a
  cell the Builder was visibly covering took hold of the object underneath, selected it and
  began a drag a weaver could not see. The routing rule, in order:

  ```text
  the terminal overlay, while it is open   -- it has the pointer entirely
  a visible pane, by its resolved bounds  -- it occupies what it covers
  the workspace and the document underneath
  ```

  The first is a **mode** and the second is a **place**, which is the whole design: the overlay
  takes every pointer event anywhere, because a weaver typing into it is not also authoring in
  the workspace; a pane takes only the presses that land on it, because a weaver with a pane
  open *is*. `occupied_at(panes, screen, cx, cy)` is the one question — it names no kind, and
  it asks the same `bounds_of` the painter was handed, so occupancy cannot drift from painting.
  (The picker answered too, as the mode that padded itself to a whole slot precisely so it could
  not be read through, until it retired.) **Only a press is occluded**, and the two asymmetries
  are why no capture, focus or z-order state exists: a press on a pane begins nothing, so a pointer that later leaves it
  drags nothing (the absence of a drag is the memory); a gesture that began on the workspace
  owns the pointer until its release, so the release ends it wherever the hand is — occluding
  that would strand a drag with the button up. **Motion is never occluded**, because stopping a
  drag at a pane's edge would clamp the document: an object would be unable to reach a cell a
  weaver is entitled to put it at merely because something is drawn over that cell.
- **A pane is as visible as it is occupied**. Every open pane paints a backdrop
  across the whole of its resolved bounds — the same rectangle `bounds_of` hands its painter
  and `occupied_at` answers about, so there is one geometry rather than two that agree. Until
  Before it had a ground, `Info` painted bare labels: it refused a press across 28×17 cells while an object
  dragged under the column showed its body and its selection ring straight *through* the pane,
  with the pane's own words on top. That was a real defect and it was one rectangle telling a
  weaver two different things. What it is **not** is an argument for a painted-cell mask: what a
  hand meets is still bounds, because a mask would make occlusion depend on the length of a
  label. Whitespace inside a pane is the pane's.
- **Removing `Info` gives its 28 columns back, and always could have.** The workspace runs the
  full width of the surface underneath every pane, `Info` included, so taking it off the desk
  reveals room rather than creating it. What is still refused is a room that CHANGES with which
  panes are open: the workspace's extent is what a share resolves against, and a `%`-wide object
  that resized because a weaver hid a list of names would make a pane's presence visible in the
  picture of the document. That rule settles the drag question above too: a pane may cover what
  a weaver authored, and may not change what they are able to author.
- **No focus framework.** Twenty contexts for the keyboard (`KeyContext`, `workshop/keymap.hpp`)
  plus one per external pane holding the keys, keyed by its runtime handle, resolved fresh at
  every keystroke by one routing chain: the terminal overlay, the arrangement scopes, the
  contextual surface, the modes, a focused pane, then command mode. `p` was an unbound key and
  `b` was one until the Builder pane took it, and it is an unbound key here again — because
  the Builder is a pane weave and `b` is one of ITS rows, active only while a weaver's typing is
  pointed at it. The inspector's own keys (`up`, `down`,
  Return) belong to `Info`: with it removed they say so instead of driving rows nobody can see,
  which would otherwise open a draft that no screen shows and that `^s` would then refuse to
  save over. The pointer's rule is the three lines above it and is still one `if` per line —
  there is no focused pane, no z-order, no capture and no widget tree, and no pane affordance
  is clickable.
- **A build now has a middle, and the pane shows it**. Pressing `b` paints `asked --
  waiting for it to start`, and a beat later `running -- op #1, 4 out` with the command that is
  running and the newest lines it has said. The two numbers are there because they are what make
  a running build *visible* rather than asserted: a weaver who watches `out` climb while moving a
  rectangle has watched Workshop stay alive while a real child process ran, which a build that
  held the pump could not have produced. They stay on the row after it ends, so the evidence
  does not vanish at the moment it becomes a result. While the pane froze instead, "what is
  happening right now" had no answer for the whole time it mattered.
- **Announcing and learning are different.** A status that arrives for a build this pane
  asked for is announced on the notice line; one that merely arrives — the answer to a reopen —
  is shown in the pane's rows and never announced. The first live run got that wrong out loud,
  saying `built zengine-snake -- exit 0` about a build that had finished minutes earlier.
  Non-blocking custody made that distinction worth more, not less: a pane opened *while* a build is running
  is told `running` and must announce nothing, so the fact is held across every intermediate
  condition and released only at one the build will not leave.

- **Two outcomes, two rows, two notices.** A build outcome and a **realization** outcome are
  different truths with different owners, so the pane shows both and derives neither from the
  other: a build that worked whose realization was refused is a completely different situation
  from a build that failed. `c` moves the weaver's choice through the recipes the tool published,
  `b` builds the chosen one, and with *load after build* armed (`Shift+b`) it offers the result
  to the running project too; `o` asks a role and puts the chosen artifact into the project's plan
  ([the weaver's page](../../builder/docs/zengine.builder-pane.md#builderload)). The choice
  is genuinely the pane's — what the tool holds is what it *built*.

What can be built is an **authored file** now, not a target compiled into the executable
([Builder](../../builder/docs/zengine.builder-pane.md)); what this Workshop ships is a recipe for
`snake-world-v2`, in Zengine's own build tree. That target is deliberately not one of
the artifacts this running Workshop has loaded — building one of those would overwrite a shared
library the process has mapped — and `zengine-workshop` rebuilding itself is the same hazard
aimed at the host binary, which stays out of scope. A live weave is reloaded in place from a copy
staged beside Workshop, never over the file it loaded, and an artifact that also supplies
operators is refused in words
([when a built artifact is loaded again](load-plans.md#when-a-built-artifact-is-loaded-again)).

Workshop's weave is declared in `workshop/weave.hpp` and its bodies compile once from
`workshop/weave_<subject>.cpp`, not in the host's translation unit — so the suite mounts it on a
real bus and walks `input message -> gesture -> semantic operation` end to end. It is mounted **in-process**: nothing asks to unload it, so the
reloadable-weave machinery would be ceremony bought with nothing. The weaves it *loads* are
other packages'. The host gates on `if(TARGET loom::kernel)` like snake's.

## A setup has a name

A weaver can **name the arrangement they are working in, save it, close Workshop, start a fresh
one, and get the same panes back**. That arrangement is a **setup**, and it is deliberately two
things and nothing else:

```text
Setup
    a human name
    an ordered list of PaneRefs        <- each row carries an authored WINDOW and the
                                          pane's SETTINGS; see below
```

- **A `PaneRef` is a provider/service key plus a pane key**, both text
  (`workshop/setup.hpp`). The one built-in is `zengine.workshop/layouts` (`pane_key::kLayouts`),
  and a saved file spells it that way — never `pane_kind::kLayouts`, never a catalog ordinal,
  never a `WeaveId`. Two reasons, and the second is the load-bearing one: an
  ordinal is not durable (renumber the constants and every saved setup opens the other pane),
  and **an ordinal cannot be absent** — there is no integer meaning *a pane this build has never
  heard of*, so a setup built on one would have to drop such an entry on load, which is a saved
  file quietly editing itself.
- **The durable reference lives on the catalog row** (`workshop/panes.hpp`), beside the internal
  kind rather than in a table next to it, so there is nothing for a second table to disagree
  with. Two `static_assert`s over the catalog say every row has a reference and no two rows share
  one — both failures are otherwise silent.
- **Resolution is fallible, and internal lookup stayed total.** `builtin_pane(unknown)` still
  answers with the catalog's FIRST ROW, which is correct for its callers (they derive a kind
  from an open pane or a desk row) and is an accident of order rather than a choice — it
  was the Builder until that pane became a weave. `resolve_pane(ref, runtime)` is a **second,
  narrower door** that answers with *nothing*: an unknown provider or an unknown pane key
  resolves to no kind, and **an unknown reference never becomes a built-in**. Nothing that meets a file goes through the total
  one. It consults the built-in catalog **and** this session's runtime catalog, and the
  runtime half is a **required argument** rather than a default or a second overload —
  `resolve_builtin_pane` is the narrow question under its own name, so neither can be reached by
  accident. (The parameter earns itself on one line: the setup status text asks this function, and
  a spelling a caller could forget would count a pane a weaver can *see* as `1 unresolved` on the
  row directly beneath it.)
- **An unresolved reference is kept, said, and saved again unchanged.** A setup naming
  `third.party.tools/history` loads, stays exactly as authored, produces no pane and no
  placeholder, is counted on the setup line (`1 unresolved`) and named in the notice. The word is
  **unresolved**, never *unavailable*: Workshop knows it has no catalog row for the reference and
  knows nothing whatever about whoever could present it. A setup can be **saved and have an
  unresolved pane at the same time** — `setup "Future" saved | 1 unresolved` is a coherent line.
- **The provider key is a route, not a credential.** It says which namespace to read a pane key
  in. It does not say which package author created the pane, which binary is running, that the
  same author returned after a restart, or that anything claiming the string is authentic. The
  setup by itself adds no role, office, discovery message or registry; the external pane
  protocol adds a *live* office and a discovery message and **changes none of those non-claims** — a Loom role is a replacement-stable service
  route on this bus in this process, and never an author identity across a restart.
- **Authored intent and resolved presentation have one path between them.** `setup.active.panes`
  is which panes a weaver *meant*; `panes.open` is which presentations this build could make of
  that intent on this screen. `reconcile` (`workshop/setup.hpp`) is the only thing that opens or
  closes a pane on a setup's behalf, and the two doors edit the **setup** rather than the pane
  list — so neither door can leave the two describing different arrangements. Three cases are
  distinguished on purpose: a pane open on both sides is *left alone* (no lost view, no duplicate
  refresh), one that closes goes through `close_kind` (so a removed Builder's copied status is
  forgotten by the same act), and one that opens performs whatever asking that kind does — which
  is nothing, for every built-in now: the `StatusRequested` the picker once sent for the Builder
  is the Builder pane's own, asked on its room grant.
- **The setup is a separate value and a separate file from the document.** The same document is
  worth opening in two arrangements and the same arrangement is worth using over two documents, so
  a single project container would make both unsayable. `--setup <path>` (default
  `workshop-setup.json`) is the setup's; `--document <path>` was the document's, and
  `Ctrl+S`/`Ctrl+O` were **document** commands that touched no setup byte. Each reader refuses the
  other's file by name rather than half-reading it — and since the document retired with the
  canvas, `--document` names an old file to be said once and left exactly as it is.
- **`s` writes the layout you are on; `r` reads a Setup file into it.** Naming is a separate
  gesture: a one-line editor opened by double-clicking a layout's tab (or from that tab's
  contextual menu), where `enter` commits the rename and `esc` cancels, and which **writes no
  file at all**. The editor opens on the name the layout already has, reuses
  `component::TextBox` for the text, the caret and the window, and **swallows the character its
  own keystroke produced** — the key transition and the character are two facts that both
  arrive. It is a mode, reachable only from command mode, so it cannot coexist with the
  contextual menu or with the arrangement.
- **The first row of Workshop is the layout selector**: the tabs, a `+`, and — at the row's
  right-hand edge — the active layout's Setup status, `setup: none` or
  `setup: <path> | current|modified [| N unresolved] | s save  r restore`, fitted with
  `detail::fit` so a cut is *marked*. The **verdict** is what may not elide; which artifact is
  what does. `current` is **computed by comparing** the layout's desk with the value last
  written to or read from that file — never a dirty flag, which would need a hand at every place
  a pane is added or removed, and never a filesystem read.
- **The file has its own format identity and its own bounds**
  (`workshop/setup_persist.hpp`): `"format":"zengine-workshop-setup"`, one version written, the Loom's own
  compat codec, deterministic output so `save -> load -> save` is byte-identical, unknown fields
  rejected, and a name/key/count/byte ceiling refused *before* anything is copied into the live
  setup. Loading **returns** a candidate rather than writing into anything, so "a malformed file
  never leaves Workshop halfway restored" is structural: a refusal changes no pane, no setup, no
  Builder view and no other file's byte. Saving goes through the one safe write every durable file
  here uses, so a detected failure leaves the last good setup file byte-identical.
- **No resolved rectangle, no metric, and no session interaction state is persisted** in a setup
  file. A list's cursor, the Terminal's draft, the Builder's copied status and the selection
  are all session; so is the workspace extent, which no setup file carries — the same setup
  restored under a different window's room yields the same references and different bounds,
  which is the setup's authored/resolved proof. (A row's authored *place* and *size* are intent
  rather than a rectangle; the extent is durable one level **above** a setup, in the window
  file beside the last session — see the final section.)

Deliberately absent, so the absences are decisions: no opaque provider configuration (a pane's
[settings](#a-panes-settings) are typed values the layouts' owner judges by form); no multiple
pane instances; no setup catalog, recent list, autosave or import/export; no tabs, docking or
layout weave. Workshop manages **one** active setup path. (The external provider, office and
discovery protocol this list once excluded now exists, bounded — the section below says exactly
how far. So do pane drag/resize, authored pane geometry and an arrange mode, equally bounded
— the section after that says how far.)

## The code authors a default; the weaver authors an override; the host resolves the room

A weaver can **select a pane, move it, resize it by an edge or corner, change what is in front of
what, reset any of that, and save the arrangement by name** — with the keyboard alone, or with a
pointer, reaching the same doors. Panes may overlap, and **every pane the setup names is reachable
whether or not it can currently be seen.** Workshop's unit is the **whole canvas pixel**,
twelve to a cell: a pane a weaver dragged by a single window pixel differs from its neighbour
by exactly one, while a pane on a cell boundary is an exact multiple and the character medium's
picture of it has not moved by a byte. Setup format is **version <!-- value setup_persist::kFormatVersion -->6<!-- /value -->**, its amounts in pixels
under the word `pixels` and each row's [settings](#a-panes-settings) beside them. A **version-4**
file reads with no settings, a **version-3** file (`subcells`, four to a pixel) is read back to the
pixel each edge was painted at, and a **version-2** whole-cell file still loads, its cells
mapped exactly (x 12); the next explicit save writes version <!-- value setup_persist::kFormatVersion -->6<!-- /value -->. A version-1 file is refused by
its number.

```text
authored setup                 resolved presentation          session interaction
    PaneRef                        current seat                   selected PaneRef
    place  {mode, x, y}            current rectangle              management step
    width  {mode, amount}          current clipping               chosen edge/corner
    height {mode, amount}          projection / refusal           pointer gesture custody
    front  (a canonical rank)      visibility and hit order
    settings (key, one value)      the area's room
```

**Only the first column persists.**

- **Sparse, so a default stays a default.** Each geometry field carries a **mode**, and
  `default` means *no override — the developer's answer, whatever it becomes in a later build*.
  A full snapshot would convert every developer default into a weaver decision at the moment of
  first save, which is the defect a compared copy removes. The unused numbers
  of a `default` must be zero, so absent intent has exactly one canonical spelling.
- **Each axis is independent.** Moving a pane freezes neither size axis; resizing one axis freezes
  neither the place nor the other axis. A default-width pane goes on taking its half-share of
  the room after a place edit.
- **An authored place is measured from the room's top-left, not an offset** from where the
  developer put it. An offset is authored against a default a later build may change, so the
  same saved bytes would silently mean somewhere else. **Resetting** is what gives back
  "wherever the default puts it". A place at y 0 stands directly under the top band, whatever
  height that band was fitted to on this face, so one desk is arranged alike in a window and a
  terminal; a place is never negative, so no pane stands over the band. The unit is the canvas's
  own pixel — the window's, drawn 1:1, and a twelfth of a terminal's cell.
- **An axis authored in `pixels` is presented at exactly its pixels, on every medium.** The
  window draws it there; a terminal shows the cells it covers. An older build wrote such an
  axis without presenting it; one under a cell is read back as one cell.
- **`front` is a canonical rank, not an accumulating counter.** Over `n` rows the set of ranks is
  exactly `{0 … n-1}` — 0 back-most, `n-1` front-most, no tie and therefore no secondary key.
  Paint walks it ascending, the pointer descending. A permutation of `0..n-1` is *unique* for a
  given order, so reset writes bytes identical to a setup that was never reordered, and ten
  thousand alternating "send to front" operations leave every rank inside the bound.
- **Reordering moves nothing else.** `seat_panes`, `reconcile` and `bounds_of` all read the
  setup's LIST, and no ordering operation writes anything any of them reads — so "raising a pane
  cannot move, resize, mount, unmount, reseat or regrant it" is the *absence of a write*.
- **An authored place spends no reactive slot.** A pane the weaver put somewhere is not in the
  tiling, so it neither consumes a tile nor can be made to *wait* for one. Resetting its place
  puts it back.
- **The host clips; it never rewrites.** A rectangle running past the canvas is legal authored
  intent, drawn and met and granted room for the part this screen has, and saved exactly as the
  weaver said it.
- **Info is an ordinary arranged pane, and so is the Terminal** (its own page is
  [terminal/docs/zengine.terminal.md](../../terminal/docs/zengine.terminal.md)). `screen_of` reserves
  nothing across the width: `room_w` is the surface, it is what every share of the workspace
  resolves against, and every place stands on it. Management authors Info's geometry like any
  other pane's. The shipped default setup is what opens it at the right edge, by naming that
  place (`"mode": "top-right"`) rather than by coordinates no desk could know.
- **`w` opens pane management**, from command mode. Inside it: `tab`/`up` select, `m` move,
  `s` size (`tab` cycles the eight edges and corners, arrows resize), `f`/`b` front/back,
  `r`/`l` raise/lower one, `0` reset (`p` place, `w` width, `h` height, `o` order), `esc` back one
  level. **Every resize edge preserves its opposite anchor**: the edge a hand pulls is
  the edge that moves, and the one opposite holds still — pulling the top edge changes `y` and
  the height *together* so the bottom edge stays put, and a corner holds the corner across from
  it. Right and bottom pulls anchor the place by not writing it, so a reactive pane stays
  reactive; a left or top pull authors the place with the size as one transaction *on its own
  axis* — a refused height can never leave a moved top edge behind. **Independent axes settle
  independently**: a move or corner gesture blocked on one axis — dragged past the
  left wall, or pulled under the one-cell minimum — still applies the other axis's legal
  proposal, and the blocked coordinate keeps its own value rather than clamping to the wall.
  Only a gesture refused on every axis it moved writes nothing. **A hand's edge snaps**: one
  within `workshop.snap-reach` of the room's edge or another pane's on the screen comes to it, the line
  met marked across the room while held; Alt held sets it aside, keys and typed values never
  snap, and nothing about a snap is saved. Edits commit immediately;
  `esc` is *back*, not *cancel*, and there is no undo.
- **Graphical interaction is pixel-responsive; the TUI stays honestly cell-grained.** A window
  pointer's press and motion are spent at their own resolution — one pixel of hand is one
  pixel of pane, with no whole-cell threshold anywhere on the path — and what a medium
  paints is the *one quantization law* at its own grain: a span `[L, R)` lands on the
  device units `[floor(L/g), floor(R/g))`, one window pixel or one terminal cell per unit. Hit
  testing floors by the same grain, so the first painted cell of an edge inside a cell answers
  the hand and the cell before it does not. A terminal therefore shows a pane placed inside a
  cell on the cells its floored edges cover — snapped, truthfully — and projecting it rewrites
  nothing: whole-cell values stay exact, other values resolve deterministically, and the
  underlying arrangement is untouched by any number of frames.
- **A pointer press claims one gesture until release.** Crossing another pane, crossing the
  Terminal's rectangle, and reordering mid-drag all change nothing about who is being moved.
  Outside management mode nothing about the pointer changed: a selected pane behind another one
  claims no press, so a selection never becomes a click-through, and no selection auto-raises.
- **The Pane Manager and arrangement share one list and not one purpose.** The inventory is the
  union of the combined catalog and every `PaneRef` the setup names, so an unresolved pane finally
  has a row. The Pane Manager keeps *presence*, through the two doors; arrangement owns
  *arrangement* and binds no toggle. Seven states, one classifier: `closed`, `unresolved`,
  `refused`, `waiting`, `off-room`, `covered`, `open` — `covered` means every visible cell is
  behind the **union** of what is in front, and one visible cell is enough to be `open`.

## A pane's settings

A pane can keep **choices about how it presents itself in each layout** — a row shown or hidden,
a step, a mode — in the layout's own setup row, beside the pane's place and size
(`workshop/pane_settings.hpp`).

- **A setting is a key and exactly one value, in the field of its kind**: `flag` (on or off),
  `number` (a whole number) or `text` (printable ASCII). A row's settings are written in key
  order, once each, as `"settings":[{"key":"legend","flag":false}]`, and a row that keeps none
  writes `"settings":[]`. A value holding no kind, or two, is refused by every reader — the
  setup file, a session's desk and a desk an agent hands across — naming its key.
- **The layouts' owner judges a setting's form and nothing else**, so a row whose pane this build cannot
  present keeps its settings byte for byte, and no setting is converted, defaulted or dropped. A
  row keeps at most `kMaxPaneSettingsPerRow` settings and a desk `kMaxPaneSettingsPerDesk`; a key
  is at most `kMaxPaneSettingKeyLen` bytes and a text `kMaxPaneSettingTextLen`.
- **Keys under `workshop.` are Workshop's own.** A row keeps one, `workshop.skin`, the skin its
  pane's area is painted in, which the pane is never handed; a file holding any other is refused
  by name rather than half read, and a pane declares none.
- **Three kinds and no more**: a switch, a count or step, a word. A choice among words is a text
  the pane restricts, a fraction a whole number of a finer unit, a set one flag per member, and a
  colour a role's name. A further kind would move every file format that carries a setting.
- **A pane declares the settings it takes** (`PaneSettingsDeclared`, beside its offer, to
  [`zengine.layouts`](zengine.layouts.md), the office that keeps the layouts): each a
  key, its default in the field of its kind, and what it takes — a text one of its `choices` (or
  any text when it names none), a number from `low` to `high`. The declaration is judged whole
  under the office stamp, a refusal is said as Workshop's notice and leaves the settings in force, a later
  one replaces it, and it counts only while the weave that sent it holds the office, at the
  incarnation that sent it: a reloaded image declares again, or is shown none of its
  predecessor's settings. Version 2 of the declaration says how a weaver finds each setting -- up
  to `kMaxSettingTags` tags of its own (words like `chrome` or `scroll`, never `pinned`, `all` or
  `kept`, which an editor's tab strip spells for itself), whether it is among the pane's default
  pins (`kMaxDefaultPins` at most), and a one-line `summary` -- and at most `kMaxDeclaredTags`
  distinct tags between them. The layouts' owner judges these by form and never acts on them.
- **The layouts' owner hands a seated pane its row's settings** (`PaneSettings`) when the office's
  holder accepts the shape, declared or not, without the `workshop.` keys: when the pane asks for
  them (`PaneSettingsAsked`, which a pane sends once in each life of the weave holding its office,
  after the first message that life delivers), and again whenever they differ from what that holder
  was last handed. A pane the live layout seats nowhere -- one a graphics window shows while
  Workshop seats it nowhere -- is handed every setting at its default. **A pane draws no room
  before it holds a row**: its room comes from the window that shows its area and its row from the
  layouts' owner, in no order Loom promises, so it holds what the room gave and draws its first
  picture once the row has come. A pane passes over keys it did not declare, and says so where a
  setting shows when it cannot use the value it was handed. A pane a managed opening seats shows
  the picture it prepared before the seat; its next picture has its row. A holder that accepts
  version 2 (`v2::PaneSettings`) is handed the row's **revision** with them: a number minted
  afresh whenever the desk put live, the row's settings, the counted declaration or the office's
  holder moved, so a holder is handed again at each new one. A pane that cannot use a value says
  so back to the layouts' owner, `PaneSettingUnused` naming the revision it was handed, and the
  owner keeps it with the row until a later revision is handed or the pane asks again; a reading
  of the row says it, and so does Attention. A pane takes its row through the settings facet,
  `PaneSettingsFacet` (`workshop/pane_settings_taken.hpp`, installed): it asks, takes a row from
  the layouts' owner alone and whole against the pane's `setting_rows()`, says each value it cannot
  use, and calls the pane's `on_settings(taken, mail)`, `taken.revision` the revision it came at;
  its `held()` says whether the pane may draw.
- **A pane may commit its own word** -- a setting a weaver changes by acting in the pane, as Files'
  places open and shut -- by the commit below, as its office, naming itself and the revision it was
  last handed. The owner writes the row, hands it back at its new revision and then answers, so
  the pane shows what the row keeps, never what it asked for; a stale revision, a lock or a value
  the declaration does not take is refused in words, and the pane says the refusal where the
  setting shows. A pane holds one such commit at a time and refuses another while it is unanswered.
- **A row is read and written by message** (`workshop/settings_seam_vocabulary.hpp`):
  `PaneSettingsRequested` answers the live layout's row for one pane -- its revision, the counted
  declaration, what it keeps, its pins and the band's sentence -- and `PaneSettingCommitRequested`
  writes one setting at the revision read, or refuses in words with nothing written; both are
  `zengine.layouts`'s, and Info's rows read and write through them. An office asks, and so may this
  Workshop's Terminal and a guest whose row has `settings`; the Terminal's line spells every field,
  `clear=false` included:

  ```text
  ask @zengine.layouts PaneSettingsRequested 1 provider=zengine.terminal pane=terminal
  ask @zengine.layouts PaneSettingCommitRequested 1 provider=zengine.terminal pane=terminal revision=$r1.revision clear=false key=wheel-rows number=5
  ```

## A weave may offer a pane

> **The office authors the pane; Workshop places its area; the window gives it its room.**

A weave that is not Workshop can offer Workshop a **pane**: a row in the Pane Manager, and a
rectangle a weaver can open, which Workshop places as the pane's own area in the window that shows
the desk. The pane draws there and hears the pointer there, from the window; Workshop draws only
its frame. The protocol's core (`workshop/pane_vocabulary.hpp`, which declares every pane shape
that crosses today) is the offer, the keys and the window's own shapes
(`graphics/vocabulary.hpp`):

```text
PaneCatalogRequested   Workshop  ->  everyone   "who has panes?"
PaneOffered            provider  ->  Workshop   "I have this one."
AreaRoom               window    ->  provider   "here is your area's room, and how I set text."
Drawn                  provider  ->  window     "here is what the area shows."
AreaButtonChanged      window    ->  provider   "a weaver pressed here, in your area."
PaneKeys               Workshop  ->  provider   "the keys are, or are no longer, here."
PaneEscapeUnspent      provider  ->  Workshop   "the Escape you sent me was unspent here."
                                                   (under that Escape's own correlation)
```

- **`PaneOffered` carries no provider field, and the absence is the enforcement.**
  The provider half of a `PaneRef` is `mail.authored_role()` — the office Loom *verified at the
  moment the sentence was authored*, carried as delivery provenance that no payload can write and
  no sender can choose. There is nothing to compare against the stamp because there is
  nothing to compare. **Holding an office is not speaking as one**: a provider that reaches for
  `mail.send_to_role` instead of `mail.as_role(R).send_to_role` registers nothing, *even though it
  currently holds the office* — which is the sharpest negative case in the suite.
- **What a Loom role proves, exactly.** That the sender held this office at this moment, on this
  bus, in this process. It is a live, replacement-stable **service route**. It is not a package
  author, a signature, a publisher, or evidence that the same author returned after a restart.
  This protocol makes none of those claims and adds no mechanism that could grow into one.
- **Discovery converges in either load order**, with no polling and no timer. A provider loaded
  *first* announces on its attested `zen.Activated` to an office nobody holds yet, and that
  sentence is simply gone; Workshop then office-publishes `PaneCatalogRequested` on the first
  window's offer (`WindowOffered`) — its ordinary startup hook, because Loom deliberately sends no
  `zen.Activated` to a *native* mount and inventing one would be a fake lifecycle event — and
  every provider that verifies the
  authorship re-offers. Repetition is harmless: identity de-duplicates, so a re-offer refreshes a
  descriptor in place and grows the catalog by nothing.
- **Everything a live message can make Workshop retain is bounded before a byte is kept.** A
  provider key and a pane key by the setup file's own `check_pane_key`; a name at 32 bytes and a
  summary at 64, neither empty, neither all spaces, neither carrying a control byte; the combined
  catalog at **`kMaxPaneCatalogEntries` total entries** (`workshop/panes.hpp`), built-ins
  included. Admission is atomic in both directions — an invalid first offer adds nothing, an
  invalid *refresh* leaves the last accepted descriptor whole, and a refresh is still allowed while
  full because the bound is on how many distinct panes are held rather than on how often a provider
  may correct itself.
- **A runtime offer cannot shadow a built-in**, and two offices offering one pane key stay two
  panes: the `PaneRef` is the *pair*, so neither office can refresh or overwrite the other's row.
- **The setup file keeps no runtime fact.** No descriptor, drawing, room, handle or liveness fact
  is saved. A setup naming `third.party/hello` loads, stays exactly as authored, resolves the
  moment that office offers the pane — *without the file being touched* — and is unresolved again
  in a fresh process where the provider is absent.
- **Workshop chooses the placement, and no room rations it.** Every external pane with no place
  of its own goes in the overlay stack, down one column from the room's top; a pane that would pass
  the room's floor, the bottom band's top, begins the column again at its top. At the 78×22
  minimum one pane of the fallback height fills the column, so a second stands over the first,
  in front by its rank. Every resolved reference is seated: a launch, a reveal and an open are
  never refused for room, growth and a shrink lay the same panes out again, and none closes.
- **A list windows the combined population rather than truncating it** — the picker did, through
  `list_window` and its own `omitted_text` wording, and the desktop's Pane Manager does in its own
  image (keeping its cursor's row in view, and counting what is above and below it). The markers
  come *out* of the list's row budget; the pane does not get taller.
- **The area is the pane's body, and the window gives its room.** Workshop owns the pane's frame
  -- its edge and its title rows -- and places the body beneath them as the pane's area, sized by
  `fit_region`'s answer; the window that shows it gives the area's owner its room in pixels and the
  window's text measure (`AreaRoom`), when the area is placed, when it moves or resizes, and when a
  holder arriving with nothing asks it (`RoomAsked`). Two things move the body: a **wider
  surface**, because a stack slot takes half the room's surplus, and a hidden or shown **title**.
  A *taller* surface moves neither: a slot's height is fixed.
- **A drawing past what a window keeps is the window's to refuse, and said to its drawer.** A
  window keeps at most `kMaxNodesPerArea` nodes and `kMaxElementsPerArea` parts of one area; a
  drawing past them is refused to its drawer alone (`DrawnRefused`), and the area shows what it
  showed. The shipped helpers draw such a picture as its bound in words instead.
- **Silence is `waiting`, and never `unavailable`.** Loom gives Workshop no participant-visible
  provider-unload notification and a sender's silence does not prove a delivery's fate — so nothing
  times out, nothing polls, no catalog row is withdrawn and no setup reference is deleted. Until a
  pane draws, its frame says `waiting for <name>` behind its area, and `<name> draws in no window`
  for an office that takes no room in one. That is a stated limit, not liveness.
- **Closing ends only the presentation.** The provider's weave, its office, its semantic state
  and its catalog row all outlive it: the area is withdrawn, no unload is sent, and the close door
  — a close, never a toggle — is the one way off the desk.
- **Workshop gained grant rules and no powers.** `PaneCatalogRequested` and the pane
  conversation's own shapes (`PaneKeys`, `PaneKey`, `PaneTextInput`, `PaneActionRequested`, the
  drops) go `allow_to_any` — the first because the ask *is* the discovery and there is no role to
  scope it to yet, the rest because Workshop sends to one resolved role that is runtime data. The Builder
  sentences stay role-scoped; Workshop still commands no lifecycle, loads no weave, reaches no
  Manager, and holds no observation, filesystem, process or network authority. Workshop is now
  mounted **in** the `zengine.workshop` office so a provider can verify its ask — and holding an
  office is not a super-grant: every rule is still checked at every send.
- **The pane protocol grants no application or host authority** — no whole-screen drawing,
  document, filesystem, process, network or lifecycle access. A pane draws only in its own area;
  input follows the pane seam below. That is
  a fact about the *protocol*. It is **not** a containment claim: a trusted in-process dynamic library already shares
  this process's memory, and Loom's current default grant for a normally loaded in-process weave is
  `allow_any`. Visibility did not create those facts and this protocol does not solve them.
- **The witness is a real shared library.** `tests/weavelib/workshop_hello.cpp` is loaded through
  the real Kernel and Manager under a real attested activation, and it is a **fixture, not a
  product**: no host boots it. A registration hook would have proved nothing about the ABI it
  exists to exercise.

### Escape in a pane that holds the keys

Workshop's last meaning for `Esc` is to put the selected pane down, and every pane that holds the
keys answers a bare `Esc` the same way unless it means more there: it **drops what it has
selected**, and with nothing selected it says so — `PaneEscapeUnspent{pane}`, sent as the office
that offered the pane and **under the correlation the Escape arrived on** — and Workshop puts the
pane down. That default is `workshop/pane_escape.hpp`, installed beside the protocol:

```cpp
void on(const PaneKey& key, loom::Mail& mail) {
    if (pane_escape::answer(key, mail, kOffice, [&] { return drop_selection(mail); }))
        return;   // a bare Escape: the selection dropped, or the Escape handed back unspent
    ...           // the pane's own keys
}
```

A selection is a row the weaver chose and the pane can let go of (Loaded's weave, a power in
Powers); a marker that always stands on some row is where a list rests, not a selection, and
Escape passes it by. A pane that lists `PaneEscapeUnspent` in its `Emit<...>` declares that it
judges its own Escape and is sent it; the editors declare it and keep every Escape, and a pane
whose Escape means more (leaving a form, shedding a line) spends it. **A pane that never mentions
Escape is put down by it without being sent it**: its holder declares no way to hand an Escape
back, so Workshop answers at once. So is one whose holder accepts no `PaneKey` and whose pane
declared no row for it. Both are the holder's own declarations read off the bus at the keystroke,
never an inference from a pane's silence. The word is honoured only while the pane is still
selected, still where the keys go, and that Escape is still the last gesture of the hand that
pressed it — a key, some text, a press or the wheel of that hand since makes it stale, and another
hand's does not — and there is no answer and no retry.

Deliberately absent: no focus-changed notification from Workshop beyond `PaneKeys`, key release
or double-press notification. The pointer reaches a pane in its area from the window -- presses,
moves, the wheel and the window's word that a hold ended ([A pane's picture](#a-panes-picture)).
Keys and text cross as `PaneKey`/`PaneTextInput` to the pane a weaver last pressed into, and an
action a pane declared beside its offer as `PaneActionRequested`, the resolved id in place of the
key. There is no reply,
disposition or acknowledgement to any of them except the one above, which answers nothing and is
about `Esc` alone; no
multiple instances of one `PaneRef`; no provider-owned screen placement, docking, tabs or
resize handles; no compositor or second canvas publisher; no unload notification, timeout,
heartbeat, liveness query, `unavailable` state or catalog retraction; **no observation surface of
any kind inside the protocol** — a provider that wants to know something asks its owner with its
own grant, exactly as any weave would, and the shapes carry no `QueryRole`, no `ListLoaded`,
no Senses and no service registry; no package identity, signature, marketplace or cross-restart
author claim; no out-of-process provider support; no provider scan directory, autoload list or
plugin SDK. **No Loom change of any kind.**

### A caret and a selection

A pane with an insertion point draws it, and what it has selected, in its own picture: the shipped
helpers draw both as marks in the run they stand in (`RowsCaret`, in `workshop/pane_rows.hpp`, for
`rows_drawing`; a run's `caret_col` and selection columns for `canvas_sketch`). Workshop judges
neither and draws neither.

- **Each window draws it its own way, and neither moves a character.** A window that sets type
  draws a bar at the left of the character the caret stands before; a window of cells draws that
  character again in the accent role, and a bar where it stands before none. A selection is its
  columns drawn again in the accent role. A press on that row reaches the pane as the column of
  the character the pressed cell shows, the caret's own cell included.
- **It asks for nothing.** No blink, shape, colour, scroll or claim on the keyboard: a pane draws
  where, in rows it drew, the insertion point of text it wrote is.

## The pane facets

A pane office here writes what its pane shows and what its acts mean; the conversations every
pane holds with Workshop are facets it composes with `zengine::With`
([`workshop/pane_facets.hpp`](../pane_facets.hpp)), and the shapes they carry are the facets'
`Accept` and `Emit` lists, never the weave's. What each facet calls on the weave is a public member
of the weave; a member the facet calls only when present is optional.

- **`PaneFacet<Pane, Office, Page, Offer>`: the offer, the room and the manual.** It holds the
  activation the office lives under; offers `Pane` from `Office` on each accepted `zen.Activated`
  and on each `PaneCatalogRequested` Workshop's office sends; answers the manual from `Page`, as
  `PaneManualFacet` does; and keeps the room the window that shows the pane's area gave it
  (`graphics::AreaRoom`, taken only from that window's office), with the rows and columns the pane
  composes for (`rows()`, `columns()`). The weave supplies `offering()`, its `Offer`
  (`v2::PaneOffered` for a pane of rows, `v3::PaneOffered` for a picture of a given size), and
  `on_room(mail)`, where it draws. Optional: `offered(mail)`, after each offer (its rows, its
  settings); `on_activated(mail)`, which then offers by `announce` where it chooses.
- **`PaneRowsFacet<Pane, Office, Page, Meaning, Offer>`: a pane that says rows.** `PaneFacet`, the
  rows being composed (`begin`, `push`), what each row and run of columns means
  (`component::RowMap<Meaning>`, `map()`), the caret (`caret()`), and `publish`, which sends the
  rows to the graphics window that shows the area named `Office` (`graphics::AreaRoom`, from that
  window's office, runs `on_room`) as a drawing
  ([`workshop/pane_drawing.hpp`](../pane_drawing.hpp), `rows_drawing`): the room's ground, the
  whole of it, then each row's text a line of the window's text down and a character an advance of
  it across (`AreaRoom::measure`), in its role over its ground, cut into runs where a part begins
  or ends, each run a named part holds
  recorded as asking that part's name -- where parts overlap, the one a press on Workshop's rows
  reaches -- and in front of them the caret the pane says, a bar at the left of its cell, and its
  selection, the selected columns again in the accent role, each asking what the part beneath it
  asks. Only what changed goes, and the whole after the window's room changed. A refused change
  (`graphics::DrawnRefused`) is followed at once by the rows last sent, whole; a refused whole
  drawing is not sent again, nor are those rows unchanged, and the next rows go whole
  (`WindowDrawing`, which an office of several panes keeps for each pane too, and both Editors,
  Flow and the View Builder keep for their areas). A row's fold mark (`TextRow::mark`, the column
  of a section's `+` or `-`) is drawn as that character in a room of cells and, where the window
  sets type, as the icon table's `right` or `down` outline in its place. The pane sends Workshop no
  rows. The window's pointer in the area
  (`graphics::AreaButtonChanged`, `AreaPointerMoved`, `AreaPointerLeft`, `AreaWheelTurned`, from
  that window's office) is read on the rows it was last sent: the first button pressed on a shown
  cell is `press(RowPress, mail)` there, `keys_went_here` as Workshop's last `PaneKeys` said; the
  third is `right_press` on what the cell means, else passed back under the window's number for the
  press; the wheel is `wheel(dy, mail)`; a release means nothing. A pane that hears the pointer
  resting on it says so (`PaneRowsFacet<..., Offer, kHearsRest>`, which then accepts
  `AreaPointerLeft` too): a move with no button held -- one no press's number carries, which only a
  medium that reports a resting pointer gives -- is its `rest(RowCell, mail)` once each time the
  cell under the pointer changes, and the leave its `pointer_left(mail)` once a rest was handed;
  `resting()` is the cell last handed. A move a held press carries is a drag and means nothing to
  rows, nor does any move to a pane that does not hear rests. A press there is a Workshop
  gesture -- Workshop heard where it landed -- so an act it must approve may continue it under that
  number. `spend_notice(notice, act, say)` spends a weaver's act: it clears the notice, runs the
  act, and says the rows once without the notice when one stood and the act published none of its
  own. Where a window shows the area deaf to the pointer, `graphics::ActionRequested` naming a
  part of the rows is `press(RowPress, mail)` at the part's place -- a cell only it holds, else the
  first a press reaches it at -- with `window` set and `keys_went_here` not: no Workshop gesture
  stands behind it, and an act Workshop must approve is refused. An id `answers(id)` admits is
  `on_action(id, mail)`; any other is no act. The room is asked once each life while no window has
  said it (`graphics::RoomAsked`).
- **`PaneDrawingFacet<Pane, Office, Page, Offer>`: a pane that draws one picture.** `PaneFacet`,
  and the weave's `picture(graphics::Sketch&)` -- named nodes and element records, as any drawer
  draws ([A drawing weave](../../graphics/docs/graphics.md#a-drawing-weave)) -- shown by
  `show(self, mail)` wherever the pane stands. The graphics window that shows the area named
  `Office` is sent what changed since it last held the area's drawing (`graphics::Drawn`): nothing
  when nothing did, the whole picture after its room changed, and after a refused change the
  picture last sent, whole, at once (a refused whole picture is not sent again until it changes).
  The weave calls `show` where its state changed and in `on_room`, which the window's room
  (`graphics::AreaRoom`, from that window's office) runs, so a message the pane ignores draws
  nothing; the picture is set in the window's measure (`text()`, which the weave's `picture`
  stands its rows by). A press on a part is the weave's `on_action(id, mail)` with the action the
  part's record names: in the window that shows the area, the first button pressed on a part of
  the picture last shown there (`graphics::AreaButtonChanged`), or, where the window shows the
  area deaf to the pointer, `graphics::ActionRequested` for an action that picture names or one
  `answers(id)` admits. The third button is passed back, and the wheel is `wheel(dy, mail)`, as
  `PaneRowsFacet`'s. A pane that hears rests (`kHearsRest`, after `Offer`) is handed a move with no
  button held as `rest(part, mail)`, the part the picture last shown there names under the pointer
  ("" for none), once each time that part changes, and the leave (`graphics::AreaPointerLeft`) as
  `pointer_left(mail)` once a rest was handed. The room is asked once each life while no
  window has said it (`graphics::RoomAsked`), so a pane reloaded into a window draws there again.
  Operators other than text are the window's alone.
- **`PaneActionFacet<Pane, Office>`: the actions a pane declares.** `declare(mail, rows, mode,
  elsewhere)` sends `v3::PaneActions` as `Office`, replacing the rows last declared. A
  `PaneActionRequested` counts only as Workshop's own word about `Pane`, and only for an id the
  weave's `answers(id)` admits where the weave has one, so an id the pane does not answer to in the
  mode it is in spends nothing; it is then the weave's `on_action(id, mail)`, the same member a
  press on a part a `PaneDrawingFacet` pane drew reaches, so a pane writes one. `answers` must be
  public, as every member a facet calls is.
- **`PaneMenuFacet<Pane, Office>`: the pane's own menu.** It holds the one menu the image asked
  the presenter for and has not heard answered (`pane_menu::Asked`, never reload-kept state).
  `offer(subject)` begins a menu; `send(mail, menu)` sends it as `Office`, and
  `send_continuing(mail, menu, correlation)` continues the gesture it answers, each replacing the
  ask held. A `PaneMenuAnswered` settles only that ask, from the presenter's office, about the pane
  and subject it asked about, once; its chosen row is the weave's `on_menu(id, subject, mail)`.
- **`AskFacet<Pane, Office, Act, Approvals, Capacity>`: the asks a pane still waits on.** One
  bounded table (`loom::AskBook`) numbered from the pane's one correlation sequence, each entry the
  pane's own record of the act, `Act`, captured when it was asked. `approve` asks Workshop whether
  a gesture's actor may do an act (`v2::PaneOperationRequested`); `ask` and `ask_with` send a
  request to an office; `answered(mail, which)` takes the ask an answer settles, by Loom's answer
  provenance and its correlation. Workshop's `PaneOperationAnswered` is the weave's
  `on_approval(act, answer, mail)`, and Loom's `zen.DispatchRefused` for exactly one held send --
  its attempt, office, shape and version -- is `on_refused(act, refused, mail)`; a forged, stale
  or unmatched one settles nothing. A full table refuses the new ask (`Asked::kFull`), a send that
  queued nothing is never held (`Asked::kUnqueued`), and the weave says either in its own words.
  The table is the image's alone: a reloaded pane asked nothing, so no earlier answer settles it.
- **`PaneSettingsFacet<Pane, Office>`: the pane's row.**
  ([`workshop/pane_settings_taken.hpp`](../pane_settings_taken.hpp)) Once in each life, after the
  first message the life delivers, it asks the layouts' owner for `Pane`'s row as `Office`
  (`PaneSettingsAsked`), and again after a delivery whose ask queued nothing. A `v2::PaneSettings`
  authored by `zengine.layouts` for `Pane` is taken whole against the weave's static
  `setting_rows()`, each value it cannot use said back (`PaneSettingUnused`), and handed to the
  weave's `on_settings(taken, mail)`; any other office's is passed over. `held()` says whether the
  life holds a row, and the weave draws no room until it does; the first row the life holds is the
  weave's `on_settings_held(mail)` too, where it has one, to draw the room it drew nothing in.

## A pane's picture

A pane draws in one of three ways ([`workshop/pane_facets.hpp`](../pane_facets.hpp)):

- **Rows.** A pane on `PaneRowsFacet` pushes rows of text, each with what it means, and the facet
  draws them for its whole area through one `RowsBlock`, below: Files, the Terminal, the Builder,
  Compose and Notices.
- **A sketch, drawn straight.** A pane on `PaneDrawingFacet` has its weave's `picture(gfx::Sketch&)`
  drawn in the window's measure over its room's ground, whatever it places there, rows among it
  where it wants them (`place_rows`, below): Attention, Connections and the demo controls.
- **A canvas picture.** A pane offered a body of canvas pixels composes a picture as a value and
  draws it with `canvas_sketch`, as this section says: the views the View Builder makes, and Flow.

A canvas picture is a value a pane composes and draws in its own area
([`workshop/pane_picture.hpp`](../pane_picture.hpp)): rectangles (`CanvasRect`), labels
(`CanvasLabel`, one canvas cell per printable ASCII byte), runs of text in the window's measure
(`CanvasText`) and the parts it names (`CanvasPart`), for the room it is composed in
(`CanvasRoom`: the area's size, the window's grain -- one pixel where it shows every pixel, a cell
where its unit is the cell -- and how it sets text). None of it crosses as a message: the pane draws
it with `canvas_sketch` ([`workshop/pane_canvas_drawing.hpp`](../pane_canvas_drawing.hpp)), and
the window shows what it draws.

A canvas pane asks for its body with `v3::PaneOffered{pane, name, summary, width, height,
text_rows}`: a body of whole canvas pixels and rows of the medium's text beneath it. Workshop
places exactly that body in a window, and the cells that hold it in a terminal, so a 680 by 360
view runs in a 680 by 360 room.

- **What `canvas_sketch` draws.** The room's ground, the whole of it, so nothing behind the area
  shows through; then the picture's rectangles, labels and runs, in that order, under a `picture`
  group; each named part's place painted again, clipped there, under a group whose record asks the
  part's name (`PartFace` where the part says it is unavailable), a later part over an earlier
  one; and each run's caret and selection in front of both, as marks asking what the part beneath
  them asks. Every node is named by its place in its group, so a node a change adds goes after its
  siblings. A window lands a press where what it painted names an action, so a part is pressed
  wherever the picture shows it and an unnamed place nowhere.
- **Bounds.** At most `kPaneCanvasMaxRects` rectangles, `kPaneCanvasMaxLabels` labels and
  `kPaneCanvasMaxTexts` runs, `kPaneCanvasMaxLabelBytes` bytes a label, `kPaneCanvasMaxTextRunBytes`
  a run and `kPaneCanvasMaxTextBytes` in all, and no more than a window keeps of one area: a picture
  past them is drawn as its ground and that sentence (`CanvasSketch::over`), never sent to be
  refused.
- **Measured text.** A `CanvasText`'s `x,y` name its padded region's origin, or with `padded`
  false its first character's cell, so unpadded runs one line apart stack as rows do; its caret and
  selection are byte columns of its text; `background` is a ground under its characters.
  `workshop/pane_canvas_text.hpp`'s `canvas_text_metrics(room)` and `clip_canvas_text` give the
  same sizing and clip, for hit tests; `fit_region` (`workshop/pane_text_fit.hpp`) is the one
  answer to how much text a region holds.
- **Rows.** A pane whose picture is rows of text draws them with `rows_drawing`
  ([`workshop/pane_drawing.hpp`](../pane_drawing.hpp)) for the room a window gives (`RowsRoom`): a
  row a line of the window's text and a character an advance of it, on the medium's own ground,
  each run a named part holds asking that part's name, the caret and the selection as marks
  (`RowsCaret`); `window_cell` reads a point of the area back to a row and a column.
- **The pointer is the window's.** The window tells the area's owner of a press, its moves and
  its release under its number for the press, the wheel, and that a hold ended
  (`AreaButtonChanged`, `AreaPointerMoved`, `AreaWheelTurned`, `AreaHoldEnded`);
  `WindowDrawing::pointer` reads each against the drawing last sent. Where the medium reports a
  resting pointer, it tells the owner too of each move with no button held, under no number, and
  of the pointer leaving the area (`AreaPointerLeft`): an office of several panes whose panes hear
  it (`MultiPaneFacet<..., kHearsRest>`) hands each pane's first move onto another cell to the
  weave's `on_window_rest(pane, cell, mail)` and the leave to its `on_window_left(pane, mail)`,
  beside the moves `on_window_pointer` is handed as before. A secondary press may
  continue into the menu presenter, citing the window's number; a primary press may carry a value
  out, as `PaneValueCarryRequested{drag=true}` under that number, and once Workshop accepts the
  carry its own area stands in front, so later motion and the release are the carry's.
- **Where a carried item would land, and where it landed.** A pane that accepts `PaneCanvasHover`
  is told the point of its area a carried item rests over, and `over=false` once when the hand
  leaves; a drop is `PaneCanvasValueDrop` or `PaneCanvasDrop`, the area it landed in and a point
  of it. It is presentation only: no selection, focus, keyboard or gesture moves.

Keep measured sizes out of saved authoring data: they describe the current room, not a document or
graph's durable coordinates.

### Text inside a picture

A pane's picture is free: whatever shapes and colours it draws, where it puts them. Rows of text are
a helper inside it, never the picture's limit ([`workshop/pane_rows_block.hpp`](../pane_rows_block.hpp)):

- **`RowsBlock<Meaning>`** holds the row tools for a room of rows and columns: rows pushed with what
  each row and run of columns means (`component::RowMap`), a control strip (`push_controls`), a
  list through its window (`push_window`), the caret and the selection. `draw` puts them into the
  pane's picture as a group at a place of its choosing, a row a line of the window's text, on their
  own ground or on what the picture drew beneath them; `take` gives the rows and their parts for a
  picture of rows alone.
- **A press read back to a row.** Each meaning's part is named, and the window lands a press on the
  element it drew over that element's whole face (WL-PRESS-07); `named(part)` gives back what the
  part the press reached means, so a pane never reads a cell to find it.
- **`place_rows`** puts any `rows_drawing` into a picture the same way.

[The tally pane](../../examples/tally-pane/tally.cpp) draws a card, a badge and a button where it
puts it, and says its words as rows standing on the card. A pane on `PaneRowsFacet` holds one
`RowsBlock` for its whole area.

## A pane may be pressed

> **Selection is a fact, not a command.**

A weaver can press a row of the `Loaded` pane. The row is marked, and the pane publishes an ordinary
Loom message saying which entry that was. **The pane does not know who listens**: Compose is one
listener, and takes that entry as the target it composes for
([Inventory to Compose](../../inventory/docs/inventory-compose.md)). `Loaded` draws its rows in its
own area, so its press arrives from the window as a point it reads back to a row and a column
(`window_cell`):

```text
weaver presses a visible row
    -> the window lands the press in the area it shows there, and tells, with what it drew
       under the hand (the area, the part named there, the drawing's number),
       AreaButtonChanged { area, button, pressed, x, y, under, part, drawing } to the area's office
       AreaPressed { window, area, button, pressed, x, y, ... }          to Workshop, which placed it
    -> the provider acts on the row the window drew there, refused where another row stands now
    -> LoadedSelected { pane, library, role }   published; nobody answers
```

- **Workshop learns nothing about what a pane's rows mean.** It hears where a press landed and
  holds no row identities, no selectable flags, no weave metadata and no list-item semantics; it
  moves the desk's selection and the keys, and sends the pane nothing about the press.
- **Whether the keys were already there is Workshop's word, and the pane's own press.**
  `PaneKeys{pane, here}` is said to the pane's office whenever the keys come or go, and after each
  primary press in its area, so a pane reads it as it was before the press moved them: a press
  that brings the keys back finds `here` false. A primary press of its own brings them, so the next
  press is made with them here even before Workshop's word on the first has come; that word, when
  it comes, stands. It is a fact, not an instruction: a pane may open a row only on a press made
  while the keys were already its own and that row was already selected, as Files does, and a pane
  with no such rule ignores it.
- **A press names what it landed on.** The window says, with each press, what it drew under the
  hand: the area, the part named there, and that area's drawing number (`graphics::Drawn::number`).
  The helpers judge it (`WindowDrawing::moved`): a named part is the same element wherever it is
  redrawn, over its whole face (`graphics::part_at`); an unnamed row, while the drawing's number --
  the pane's `component::RowMap` number, which moves exactly when the row-to-meaning map does --
  stands. A press is refused, in the pane's own words, only where another element or none stands
  there now: `RowPress::moved`, `WindowPointer::moved`, a drawing facet's `fenced`. A drop carries
  the window's word from its release and is judged alike.
- **A press is a point of the pane's own area.** The area stands under Workshop's title rows,
  which the pane never draws and is never told about, so its first row is the pane's own row 0.
  No window origin and no medium identity crosses: the point is in the area's pixels, and the
  room the window gave says how it sets text. **A place that names no row is the pane's to
  refuse**: a strip too short to fit a row is not one, and rounding it would invent one.
- **What is in front owns the press, as the window judges it.** The window lands a press in the
  front-most area that hears the pointer there; Workshop's own area stands in front while a weaver
  arranges, a menu is open or something is carried, and takes the press whole.
- **The press is read against the snapshot the weaver actually saw.** Interpreting one asks the
  Weave Manager nothing — the row-to-entry map is returned by the same function that *built* the
  rows, so there is no second calculation to drift. Unload a library under an open pane and press
  the row that still names it: the fact names what was on screen. That is the load-bearing case.
- **The identity is what the pane observed**: the loaded library's name, and the role bound at
  load, with an empty role meaning the kernel bound none. Never a `WeaveId`, never promoted into a
  participant identity, and never a claim that anything is alive now.
- **Selecting is an occurrence, not a state transition.** The same row pressed twice publishes
  twice — a future trigger reading *whenever the weaver selects this one* is owed both — while the
  picture does not change, because the mark is already there. Two questions, two answers.
- **Only entry rows select.** The heading, the caveat, the source line, the blank separator and the
  omission marker publish nothing: `... 17 more` is a *population fact*, not a stand-in for one
  hidden weave, and "select the first hidden one" is a gesture this pane does not offer.
- **The selection belongs to the pane.** There is no `Workshop::selected_weave`, no setup-wide
  current selection and no ambient singleton. It is transient runtime UI state, held as a *name* so
  it survives a resize that windows the entry out of sight, cleared only when the absence is
  actually observed — and clearing publishes nothing, because a library going away is not a weaver's
  gesture.
- **Authority does not travel with the value.** A listener that hears a library name and a role has
  learned two strings. It cannot thereby message, interrogate, load, unload or impersonate the
  thing named: a grant is per `(shape, version, target)` and is written by whoever mounts a weave.
  **Values may flow; authority must not flow implicitly with them.**
- **Through the ordinary Loom route, never a callback.** The fact is *published*, and an
  independent test listener — one that compiles the vocabulary header and nothing else of the tool,
  registered with nobody — hears it. There is no `std::function`, no Workshop listener pointer, no
  observer singleton and no direct call.

Deliberately absent: no callback, trigger, condition, binding graph, reactive variable or action
pipeline; what may be sent to the selected thing is Compose's to ask, not the pane's; no selection
history; no `Selection<T>`, `SelectionBus` or global selection vocabulary — one list is not
evidence for a reusable one. No pane-to-pane dependency: `Loaded` knows nothing of Info, the
Terminal, the Builder or any future tool, and opens, closes and targets nothing. **No Loom change
of any kind**, and no setup format movement.

## The second button, and a menu a pane asks for

> **A right-click is the pane's first, delivery is the disposition, and a pane keeps no menu
> state.**

A secondary press reaches the pane from the window as any press does, under the window's number
for it. What a pane may do with it is an ordinary optional door in `workshop/pane_vocabulary.hpp`,
none of which changes a pane that does not take it:

```text
PanePassRequested   provider -> Workshop   "that press was not mine": the host's own pane menu
                                           opens once, while the press is the latest act
                                           of the hand that pressed
PaneMenuRequested   provider -> Workshop   present these rows (id, label) beside (row, column),
                                           about `subject`, continuing the gesture by its number
PaneMenuAnswered    presenter -> provider  chosen + id, or not chosen + why; once per request
                                           (Workshop answers only an ask it refused, or one
                                           whose presenter left, was replaced, could not be told
                                           the menu was withdrawn, or gave it back unanswered)
PaneManageRequested provider -> Workshop   open the host's pane menu on (office, target);
                                           continues a menu answer, once
```

- **Delivery is consumption.** A secondary press in a pane's area is the pane's, heard from the
  window; nothing opens, and neither selection nor keys move. The release goes to the area that
  was pressed wherever the pointer is, and a hold the window ends is the press lost
  (`AreaHoldEnded`). The title row, the border, a tab and the room still get the host's own menu,
  and so does a right press on a pane whose office does not hear the window: no keys move, and the
  host's pane menu opens at the press. A right press is never lost to a shipped pane either —
  each acts, offers its rows, or hands the press back where it has nothing to offer.
- **Every continuation echoes a number.** A press carries the window's number for it, and the
  host mints one per `PaneActionRequested`; a pass-back or a menu request echoes it in Loom's envelope (the
  `PaneEscapeUnspent` discipline) and is judged where the host acts: newest press of its
  button, unspent, its pane on the desk, and no newer act of the same hand since — a release completes its press
  and is no act of its own, so a click's release never makes its choice late. Closing the pane
  invalidates the continuation whether or not the button is up. A stale, zero, spent or foreign
  number moves nothing.
- **A menu is presented by a participant, not performed.** Workshop judges the ask and grants
  it to whoever holds `zengine.presenter` — the shipped `zengine-menu-presenter`, loaded by a
  plan row like any weave, or any replacement (next section). The presenter shows the rows,
  reads the weaver's keys and presses, ends the menu and ANSWERS it, as its office, under the
  request's number, subject-bound — `answer.subject` is the pane's own word, echoed unread.
  Beneath the pane's rows, in the same menu, the grant carries the host's own pane menu for that
  pane — the standard rows (`arrange`, `Order >`, `Reset >`, `edit code`, `hide pane`); one chosen
  is the host's to spend on that pane, and the pane is answered unchosen
  (`a standard row was chosen`). No pane performs a standard row, and the host performs none of
  a pane's. The menu takes no keys and no selection, and restores nothing after: a press
  elsewhere is the way
  on. With the shipped presenter, Escape and an outside press (spent on dismissing) answer it
  unchosen; the host withdraws it — and the presenter answers it unchosen — on a newer menu, a
  right press elsewhere, the host's own menu, or the pane leaving the desk or being offered
  again. A withdrawal the presenter cannot receive — it was unloaded while the menu was open,
  and Loom refuses the message — is answered by Workshop instead, unchosen, under the request's
  number, saying why the menu ended. So is one a presenter received but cannot carry: an image
  that arrived after the menu opened holds no such menu, and hands the interaction back
  (`MenuReturned`) instead of leaving its requester waiting. A withdrawal that was delivered to
  an image that DOES hold the menu stays that presenter's to answer. At most `kMaxPaneMenuRows` rows of ids up to `kMaxPaneMenuIdLen`; a menu taller than
  the room is windowed and says so. With no presenter loaded the ask is refused in words. A
  chosen row is a fact about the weaver's gesture, never an authority grant.
- **An answer is safe to act on only through the ask's own record.** `pane_menu::Asked` —
  what `Offer::send` returns — is the one read: `take(mail, answer)` returns the chosen id only
  for an answer from the presenter's office, under this image's pending number, about the pane
  and subject asked, once; Workshop's refusal settles an ask but never chooses; anything else
  settles nothing. Keep the record in the image, not in reload-kept state: a reloaded pane then
  cancels its predecessor's menus instead of acting on rows its predecessor was showing. Whether
  the subject still applies is still the pane's to judge.
- **A gesture a menu may continue.** A menu request must echo the correlation of the gesture it
  continues, and three gestures carry one: a secondary press and a **primary press**, each under
  the window's number, and a declared action sent by key (`PaneActionRequested`). All three are judged the same way where the menu would open — this pane, this
  number, and still the latest act of the hand that made it — and each is spent once, so a late or replayed
  request is refused in words rather than opened over whatever the weaver did next. The primary
  press is what lets a pane that draws its own controls answer a click on a `[menu]` of its own;
  it moves no keys and no selection, exactly as the other two do not.
- **The helpers are optional and installed beside the protocol.** `workshop/pane_escape.hpp` is
  Escape's default ([above](#escape-in-a-pane-that-holds-the-keys)). `workshop/pane_menu.hpp`:
  `Offer(pane, subject).at(row, col).row(id, label).send(mail, office)` builds and sends the
  request continuing the delivery's gesture and returns its `Asked`; `.row(id, label, key)` is a
  row beside the key that does what it does now, carried in its label after `kMenuKeyMark`, which
  the shipped presenter draws in a column of its own; `.submenu(id, label)` is a
  row whose choice opens another menu, its label ending in the one mark the host's own groups
  wear (`kSubmenuMark`, " >") -- a row that asks for more input ends in "..." instead; `pass_back`, `manage`
  and `take_keyboard` are the other lines a consumer would otherwise write.
  `take_keyboard_continuing` is `take_keyboard` under a number the pane names rather than the
  delivery's own, for a chosen row whose edit opens only after an office has answered: the pane
  keeps the choice's number across that round trip and spends it where the line appears, and
  the host judges it exactly as it judges the same-delivery form — once, and only while that
  choice is still its hand's latest act. A pane
  may write the raw shapes instead, and then owes the four checks `Asked::take` makes. The
  shipped `examples/guard-pane` consumes the button and asks for nothing; the Pane Manager and
  the Hotkeys pane offer menus; the Neovim editor, on the window's pointer, passes a right press
  and its release to Neovim, offers its own menu on the Visual highlight and on its status row, and
  hands back a right press beside its rows.

### The menu presenter, and replacing it

> **Presenting a pane's menu is an office, held by an ordinary weave.**

`workshop/presenter_vocabulary.hpp`, installed beside the protocol (`zengine::pane`), is the whole
seam between Workshop and the participant in `zengine.presenter`:

```text
MenuGranted     Workshop -> presenter   present this offer as menu n, for office/pane, in a room
                                        of rows x columns, with the host's standard rows to show
                                        beneath it and the pane's name, in area a of the window
                                        that shows the desk; under the request's correlation
MenuShown       presenter -> Workshop   menu n is drawn in area a, r lines of at most c
                                        characters, numbered as picture p; no lines
MenuInput       Workshop -> presenter   the weaver did this to menu n: a key (with the verb the
                                        weaver's contextual rows name), or a press outside; act
                                        number g
MenuClosed      presenter -> Workshop   menu n is over and its requester is answered, chosen
                                        at act g, or under the window's number for the press in
                                        area a that chose, or not -- or the standard row s was
                                        chosen so, which Workshop spends
MenuReturned    presenter -> Workshop   menu n is not mine to carry: I hold no such menu, so
                                        take the interaction back, and here is why
MenuWithdrawn   Workshop -> presenter   menu n is over because the host ended it, and why
PresenterReady  presenter -> Workshop   I hold the office now, carrying menu n (or 0)
HeldMenu        state                   the open menu, as the shipped presenters keep it across
                                        a reload
```

Workshop keeps what is fixed: which ask is eligible, one menu at a time, where the popup opens
and its frame, the presenter's area inside it -- named for that menu alone, placed in front of
the desk and sized to what the presenter says the menu takes, for the weaver's hand alone where
the pane is -- the weaver's keys, which it forwards, when custody moves, and the standard rows:
which it grants, and spending the one chosen, while the act that chose it is its hand's latest.
A press in the presenter's area is the presenter's, from the window, and the act it is
Workshop's. With no window showing the desk, a pane's menu is refused in words. The presenter
decides everything about the menu itself — whether an offer can be shown, how its lines read and
are drawn (`MenuDrawing`, an optional helper beside the shapes), what a key or press means, when
it ends — and answers the requester, which is why a pane authenticates a choice from
`zengine.presenter`.

Replacing it is ordinary: name another artifact in the load plan's `zengine.presenter` row, or
reload another image in its place. `examples/numbered-presenter` is one — numbered rows, the
standard rows numbered on after the pane's, a digit chooses, up and down wrap, a click chooses on
its release; the shipped presenter sets the standard rows off with a rule — and because it
keeps the same
`HeldMenu` state, a reload between it and the shipped presenter HANDS OVER a menu that is open:
the new image says `PresenterReady` naming it, shows it its own way, and answers under the same
number. A holder that does not carry the menu (a presenter keeping other state, or none) ends it,
and Workshop answers the requester itself; so does a presenter that leaves, which Workshop learns
from Loom refusing one of that menu's own messages — never from a refusal about an older menu,
which ends and answers nothing newer. The third way is the image's own word: an act or a
withdrawal that overtakes an arrival reaches an image holding no such menu, which GIVES IT BACK
(`MenuReturned`) rather than saying it closed a menu it never answered, and Workshop settles
that requester — whether the menu is still on the screen or already withdrawn, and never a newer
menu or one already answered. `MenuClosed` is the opposite word, and it is what tells Workshop
to stop keeping who asked.

**What a give-back says, and when it is guaranteed to settle anything.** It says that image
cannot carry the interaction NOW, and returns responsibility for any answer still outstanding.
It is **not** a statement that nobody ever answered that requester: an image holding no such
menu does not know what an earlier image, or itself before a reload, already said. Workshop
settles only a record it still retains, so a menu already answered and any newer one are
unaffected. For a WITHDRAWAL the supported timing is synchronous: Workshop retires a withdrawn
menu's record once its fence comes round, so a give-back sent while handling that withdrawal is
guaranteed to find the record, and one deferred past it is not — it may settle nothing, and a
presenter that defers owes its requester an answer of its own. Both shipped presenters give
back from inside the handler for exactly that reason. None of this changes a requester: the Pane Manager and the
Hotkeys pane perform the same operations whichever presenter presents their menus.

**Not here yet: actions offered over a hovered item.** Hover reaches a canvas pane as a fact
with its own leave (`PaneCanvasHover`), and a text pane not at all; a pane's actions follow
keyboard focus, and the only grant a presenter receives is a modal menu. An action that becomes
available while the pointer rests on an item — a hint, a key an unfocused pane could receive —
would still need a non-modal grant kind that forwards no input, and an explicit decision on keys
reaching an unfocused pane; neither exists, and hover never moves focus. The menu's grant, withdrawal and picture fence are
where such an offer would connect.

## The desk comes back on its own

A weaver can **close Workshop after arranging it and reopen it into the same desk, at the same
size, in the same place on the desktop, with no gesture.** That is a third persisted thing and
a third file — and the session's default home is the per-user **state** folder
(machine-local: a viewport and a desktop position describe *this* machine), while the project
files keep following the project:

```text
--document   workshop.json           an old object document, from     (launch directory)
                                     before the canvas retired: named
                                     once, never read
--setup      workshop-setup.json     a desk you NAMED, with `s`,      (launch directory)
                                     and read back with `r`
--session    workshop-session.json   the layouts you were USING,      (per-user state root)
                                     each with its id
(beside it)  workshop-session.window.json  the room the desk was in, and   (per-user state root)
                                     where the window sat
--marks      workshop-marks.json     the filesystem PLACES you asked  (per-user state root)
                                     to be able to come back to
```

The marks file rides the machine-local root beside the session, and for the same reason the
viewport does: a mark is an absolute path, so it describes *this* machine's disks. It is not
a desk and holds nothing about one — the places a weaver kept are not an arrangement, and the
directory they happened to be browsing when they quit is deliberately not remembered at all.

- **One representation of a desk, two files.** `session_persist::WorkshopSession` nests
  `setup_persist::WorkshopSetup` as a field rather than paraphrasing it, so the four layers that
  judge a setup file judge the desk inside a session file (`setup_persist::setup_in`, factored out
  of `from_text` for exactly this). A desk cannot be legal in one file and illegal in the other.
  The session format is **version <!-- value session_persist::kFormatVersion -->12<!-- /value -->**: the weaver's whole ordered run of layouts — each an
  ordinary saved setup, its panes' settings, locks, pins and baseline included, with the id the
  layouts' owner minted for it and the Setup file it is related to, if any — which position was
  live, the id the next layout takes, and which authored choice held each office the plan lets you
  switch (the Editor's `standard` or `neovim`). The file is a header line holding everything but
  the layouts, with their count, then one line for each layout; each line is a value read on its
  own, so no one read holds the whole run. [`zengine.layouts`](zengine.layouts.md) alone writes
  it. The room, the desktop placement and the OS window's own room are Workshop's **window
  file** beside it, which Workshop alone writes; while none stands, a session before version 12
  is read for them once. Older versions do **not** load through
  roads this reader carries — it admits one shape and nothing else. What reads them is a
  *conversion*, contributed by an ordinary operator provider the arrangement mounts
  (`zengine-workshop-session-history`): one direct edge from each older version, which keeps the
  meaning the file was written with. A version-1, -2 or -3 session opens as exactly one layout
  holding the desk it always held; a session before version 5 relates every layout to no Setup
  file; a desk before version 6 gains the Layouts pane its layout surface always was; and a room
  and desks before version 7, kept in cells and sub-units, land on the pixels the window painted
  them at; a desk before version 8 holds no settings; a session before version 9 remembers no choice;
  and one before version 10 keeps no OS window's room; a session before version 11 was one value,
  and comes back whole; and one before version 12 numbers its layouts in order. Without the provider such a file is refused
  by its number, naming the conversion that is missing. The next close writes version <!-- value session_persist::kFormatVersion -->12<!-- /value -->.
- **The viewport is the window file's, not the session's**, and that is the whole reason it is
  not kept with the desks: the same desk is worth having in a big window and in a small one, so how
  much room the surface had describes the *application* rather than the arrangement. It is
  `{width, height}` in canvas pixels, its own shape rather than the window's `WindowOffered` — that
  is a message free to grow a field whenever a window has something new to say, and the text metric
  in particular would be a stale claim about a font the moment it was written down.
- **Pixels, because the room crosses the seam in pixels.** The window is its own weave; the only
  thing it says about its room is its offer (`WindowOffered`), in canvas pixels, and Workshop lays
  its desk out in an area of that room. So the durable number is the one that crosses that seam:
  the OS window grows to the room the desk is laid out in, so a restored window is the weaver's
  chosen size, to the pixel, where the display holds it; it never shrinks to a smaller one, and a
  terminal keeps its own. The restore's notice says the room in the unit the window names —
  pixels in the OS window, cells in a terminal — and since a window names its unit when it
  offers itself a room, the notice waits for the first window that does, and is said once.
- **Position and maximized state ARE persisted, opaquely, and the medium is the
  judge.** The graphics vocabulary's placement pair closed the old deliberate omission: the
  medium reports where its *normal* window sits (its own desktop units, maximized state
  beside it), Workshop remembers the last report in its window file — coordinates it cannot
  interpret and does not try to — and offers it back once at restore. The medium then
  validates against the displays that exist *now*: a position wholly on a display restores
  verbatim, any other moves in only until the whole window is on one, and with no display
  truth nothing moves (the arithmetic is `placement_within`). A terminal run reports no
  placement, applies none, and *retains* the remembered value
  rather than erasing it. The saved viewport is the **normal** window's room, so a
  maximized close restores as a maximized window that unmaximizes to the size you chose.
- **The room, and then the desk into it.** The viewport is adopted before the desk is applied.
  The order changes no seating: every resolved pane is seated, whatever the room.
- **Written on an orderly close, by the one door.** `q`, `Ctrl`+`c` and the close the window that
  shows the desk says (`SurfaceCloseRequested`, as that window's office) all reach `quit()`,
  which writes the window file and asks `zengine.layouts` to write the session before it stops
  the bus. No autosave, no dirty
  tracking, no background writer — and no crash durability, which is not claimed here or in
  `persist::write_file`.
- **Distinct answers, not one boolean.** No previous session (silent — a first launch is
  never reported as an error); a session that cannot be read (named, defaults used, and the
  weaver's file left exactly as it is); a window file that cannot be read (the default size,
  said, and the desk restored); a window file whose viewport is outside the band this
  Workshop is honest at (`78x22`..`640x400` cells — the desk is restored, the size is
  **declined rather than clamped**, and the declined value is named); and everything restored.
- **Neither direction opens a setup file.** Closing writes a session and leaves the standalone
  artifact byte-identical; restoring a session reads no setup file at all. What a restored
  session *does* bring back is each layout's **Setup association** — which file it is related to
  and the last value this Workshop knew that file to hold — because that is a fact about
  Workshop's own knowledge rather than about the disk, and remembering it is not the same as
  going to look.


## What Workshop answers, and a pane's words

What Workshop does with each shape a pane or another weave sends it -- the approval an act asks
([`PaneOperationRequested`](zengine.workshop.md#paneoperationrequested)), a lease for repeated reads
([`PaneObservationRequested`](zengine.workshop.md#paneobservationrequested)), an item carried
([`PaneCarryRequested`](zengine.workshop.md#panecarryrequested),
[`PaneValueCarryRequested`](zengine.workshop.md#panevaluecarryrequested)), where a pane's words
and cells are ([`PaneViewRequested`](zengine.workshop.md#paneviewrequested),
[`PanePointRequested`](zengine.workshop.md#panepointrequested)) and the desk
([`DeskReadRequested`](zengine.workshop.md#deskreadrequested)) -- and each answer and refusal, is
[Workshop's manual](zengine.workshop.md). The carry's shapes are `workshop/pane_carry.hpp`'s, the
approval's and the lease's `workshop/pane_operation.hpp`'s, and the readings'
`workshop/pane_view.hpp`'s. The first consumers of the carry are
[Inventory](../../inventory/docs/zengine.inventory-pane.md) and [Info](../../info/docs/info-views.md): the
reference route carries an `InventoryReference`, the value route the item itself and its
metadata, and the two doors keep reference-shaped data from reading as a request to follow it.
Neither carrying a value nor showing a snapshot grants permission to change its source.

What follows is what spans those answers: the parts a pane names, what a press may cause, and the
desk read whole.

### A pane names its parts

A pane names the parts a weaver acts on -- a row, a control inside one, an element of its picture
-- from what each means, keeping each name across its redraws. The names ride inside the drawing
they name, each the action a record asks of the place it paints: a run of a row's columns
(`RowPart{name, row, column, columns, available}`, drawn by `rows_drawing`) or a rectangle of the
picture (`CanvasPart{name, x, y, w, h, available}`, drawn by `canvas_sketch`), plain values of
`workshop/pane_parts.hpp` and `workshop/pane_picture.hpp` that never cross as messages. A presenter names its menu's lines in its own drawing, each by the id of the row it shows (a
record's action, `MenuDrawing`), and the desk read says them as the window shows them.

- **A name is the pane's.** Workshop names nothing inside a pane, and the window says each name as
  the drawing asked it. A name is 1 to `kMaxPanePartNameLen` (256) bytes of printable ASCII, not
  all spaces, said once in its picture, and a picture lists at most `kMaxPaneParts` (2048) parts. A
  row part is a run of at least one column of a row it draws, inside the room's columns; a canvas
  part has a positive extent and may lie partly outside the room.
- **A picture lists its parts in the order its pane reads a press.** Where parts hold one place, a
  press there reaches the later: a control listed after the row it stands in, a handle after its
  element, as each shipped canvas pane reads a press on the hits it drew. A place a press reaches
  that the pane names nothing is listed with an empty name wherever it lies over a part the pane
  names; it shares its name with nothing, and Workshop says no such place.
- **The helpers are installed beside the protocol** (`workshop/pane_parts.hpp`).
  `row_parts(map, columns, name_of)` names what a `component::RowMap` recorded -- a whole row
  across the room's columns, a run as itself -- from what each means, and lists the spans in
  `RowMap::press_order`: on a row the widest first, so the span `at` answers is the last that holds
  a place. A span whose meaning is named "", a name the judge would refuse or a name already taken
  stays as a place unnamed. `PartNames` gathers a pane's own parts the same way, keeping every
  place, and past `kMaxPaneParts` drops the earliest, which take no press from a part after them.
  `pane_part_name_problem`, `row_parts_problem` and `canvas_parts_problem` say what is wrong with
  a pane's parts, for a pane to ask before it draws them.
- **A pane moving its rows into a picture names the same parts in it.** A name says what a part
  means, not how it is drawn, so a walk written against the rows still holds.
- **A part says whether its operation applies now** (`available`): true where the pane draws its
  face `[label]`, false where it draws `(label)` (`component/control_strip.hpp`). A part that
  performs nothing, a row, says true, and so does every part of an earlier version. It is a hint,
  as the face is: the pane judges the press again when it arrives. `row_parts(map, columns,
  name_of, available_of)` names a row map's parts with it.

Workshop answers each part beside the pane's words in its reading (`PaneView`,
[below](#the-desk-said-whole)), and a menu's named lines beside its lines in the desk's `DeskMenu`:

| Message | Meaning |
|---|---|
| `PanePart{name, text, place, x, y, space, available}` | One part: its name as the pane said it; the characters it covers -- for a canvas part, the characters drawn wholly inside it, each word's joined to the next word's by a space, so a part a word crosses says the characters on its side; the place covering it where the medium draws it; its point, or none; and whether its operation applies now, as its pane said it |

- **A part's point is a place of its own**, where a press reaches it: one no part listed after it
  holds. A pane's is where its window lands a press (below); on Workshop's own surfaces a row
  part's point is its middle character of its own -- a row `> [open] Files` whose `[open]` mark is
  a part of its own lands on the name, beside the mark -- else its middle blank cell of its own. A
  part with no place of its own is said with its words and place and no point: `x` and `y` are 0
  and `space` is 0, `input::space::kUnknown`, which no consumer reads and `InjectInput` refuses;
  `workshop/act` never presses one. Every part of a pane whose office nobody holds is said so too
  ([`PaneViewRequested`](zengine.workshop.md#paneviewrequested)). A point is in the medium's own units: a window's pixel, or a terminal's cell. A
  terminal shows a span `[begin, end)` on the cells from `floor(begin / kCellPx)` up to
  `floor(end / kCellPx)`, that one left out, so a canvas part it paints on no cell is not
  said there.
- **A pane drawing in an area of its own is read from the window** that shows it: each record its
  drawing names an action by is a part, its place the box its drawing shows there, its point where
  the window's own judge lands a press on it, and its availability what the record says
  (`graphics::PartFace`, `part_available` in `workshop/pane_drawing.hpp`).
- **A part the body does not show is not said**, and one it cuts is said as far as it shows.
- **Workshop names its own surfaces' parts**: its menu's lines by the action or the group each
  shows, and Layouts' tabs ([below](#the-desk-said-whole)); a pane's menu is named by its
  presenter, and the shipped presenter names each line by its row's id.
- **A pane naming nothing still reads as words.** A press on a part is ordinary input at its point:
  no message presses one. The guest `capture` power grants these queries too.

### What a press may cause

A pane says, on each action it declares, what a press of it may send, so a reader finds the
control that sends a message -- or the one in another mode that leads to it -- without reading the
pane's code or its manual's words:

```text
v3::PaneActions     provider -> Workshop   {pane, mode, rows, elsewhere}
v3::PaneActionRow                          v2's {id, label, scancode, modifiers, supersedes}
                                           + causes: [ActionCause{shape, version, subject}]
                                           + enters: the mode a press leaves the pane in, or ""
ModeCauses                                 {mode, causes}: what another mode's rows may cause
v2::AppActions      desktop  -> Workshop   v2::AppActionRow: AppActionRow's fields + causes
```

- **A cause is a shape a press may send, to anyone, beside the pane protocol** -- the pane's offer,
  rows, content, caret, menus, its answer to a manual's ask and the approval it asks before an act.
  A send that waits on an answer is the press's: `builder.load` asks whether the plan names the
  artifact. One that waits on another gesture, a menu's choice, is not. "May": a press in another
  state sends less, or nothing.
- **A subject says what the send is about**: empty for the pane's own state (the chosen recipe);
  `on <prefix>` for the part a press lands on, named `<prefix>` and its subject (the Pane
  Manager's `launcher.open`, `on pane:`); `to <office>/<pane>` for one pane always (`desktop.panes`).
  No field of the message is declared.
- **The modes.** `mode` names the mode the rows are in force for, a row's `enters` the mode its
  press leaves the pane in, and `elsewhere` each other mode with what its rows may cause -- never
  those rows, which only the mode in force declares and Workshop dispatches. A pane with one mode
  may name none.
- **Judged with the keys, and whole**: at most `kMaxActionCauses` causes a row and
  `kMaxPaneModes` other modes of `kMaxModeCauses` causes each; a mode named in lowercase letters,
  digits and `-`, at most `kMaxPaneModeLen` bytes. A cause twice, a version below 1, a subject in
  another form, an `enters` naming no mode `elsewhere` names, or another mode named twice or as the
  one in force refuses the declaration, and the rows in force stand. An earlier version is read
  with no causes and no modes.
- **The helpers are installed beside the protocol** (`workshop/pane_causes.hpp`):
  `cause_of<Shape>(subject)` names a cause by its shape's own name and version, `on_part` and
  `to_pane` spell a subject, `causes_elsewhere(modes, mode, rows_of)` says `elsewhere` from the
  function that declares each mode's rows, and `pane_declaration_problem` is the judge's words.
- **Workshop says them with the keys.** `KeymapShown` gives each binding its address -- `office`,
  `pane`, `id` -- its `causes` and `enters`, and `modes` each pane's mode and `elsewhere`. Workshop
  resolves nothing: a reader joins them with the desk and the parts' availability.

### The desk, said whole

The same header answers the whole desk in one turn, and one pane's reading page by page under a
stamp naming what it stands on, and publishes a notice whenever the desk moves. A drawing's number
alone names nothing: it starts again when a pane's office is held again. So a `PaneStamp` names
what a pane shows, and where, by a fingerprint, never a number. One
version of each of these shapes stands.

| Message | Meaning |
|---|---|
| `DeskReadRequested{}` | Anyone granted it, to Workshop: the desk in one turn |
| `DeskRead{desk, stamps, panes}` | The desk as `DeskView`; every presented pane's `PaneStamp`, from the front back; and as many of those panes' readings, in that order, as one decoded value and one reply's bytes hold |
| `PaneStamp{provider, pane, holder, incarnation, epoch, fingerprint}` | What one pane's reading stands on: the `WeaveId` holding its office, that holder's incarnation, the incarnation of the window showing the desk, and a fingerprint of what the pane shows and where |
| `PaneViewRequested{provider, pane, from, stamp}` | A page of one pane's reading from item `from`, continuing the reading `stamp` names; a stamp naming no holder (holder 0, as an empty stamp does) at `from` 0 reads the pane as it stands |
| `PaneView{stamp, area, advance, line, words, parts, covered, in_flight, from, total}` | The reading's stamp; the place its text stands on and its text measure, so any cell of it is `area` plus a column's `advance` and a row's `line`; its words and then its parts from item `from`, of `total`, as many as one decoded value and one reply's bytes hold; what covers the pane; and whether what it shows is still in flight |
| `PaneCover{by, words, rect}` | What covers part of a pane -- `by` names each: a pane in front by its name, `menu`, `arranging`, `band` -- how many of its words and parts are not said for it, and the `DeskRect` bounding what they cover of it; empty for a pane nothing covers |
| `PanePointRequested{provider, pane, fingerprint, word, column}`, `PanePoint{provider, pane, fingerprint, word, column, x, y, space}` | Where one character of a word of the reading at `fingerprint` is now; refused, in words, when what the pane shows moved |
| `DeskStamps{desk, panes}` | Published by Workshop's office to whoever follows it: the desk number and every presented pane's `PaneStamp`, from the front back, as a `DeskRead` would say them now |
| `DeskView{desk, width, height, cell_px, space, room, panes, arranging, menu, words}` | The desk by Workshop's own numbers: each pane's state, rank, place and size, selection and keys; the room; arranging; the menu, a `DeskMenu{open, office, pane, picture, place, lines, parts}`; the band's legend as `PaneWord`s with their places and no point, since the band takes no press; and `desk`, a number that moves when anything it says moves |

- **Where a pane is read from.** A pane drawing in a window area Workshop placed is read as that
  window says the area shows: Workshop asks it (`graphics::AreaShowAsked`) and answers the read
  once it has, at most `kMaxDeskWaits` (<!-- value kMaxDeskWaits -->4<!-- /value -->) reads
  waiting at once and `kDeskWaitMs` (<!-- value kDeskWaitMs grouped -->2,000<!-- /value --> ms)
  each, after which, or when the window starts again, the read is refused in words. What another
  office draws in front of the area is left out of the window's answer; what Workshop draws over
  it is Workshop's own cover. A pane no window shows is refused in its frame's words
  (`<name> draws in no window`, or that it waits for its provider); Layouts is read as Workshop
  draws it.
- **One value, then a page at a time.** A `DeskRead` stays inside one decoded value: Loom's
  [decode budget](https://github.com/Krealsion/Loom/blob/main/docs/reference/bounds.md#the-decode-materialization-bound),
  which Workshop restates as `kDecodedCellBudget`
  (<!-- value kDecodedCellBudget grouped -->65,536<!-- /value --> cells) and counts by Loom's rule:
  a cell for each declared field of every message the decoder enters, present or not, and one for
  each element of every list. It stays inside one reply's bytes too, `kReplyByteBudget`
  (<!-- value kReplyByteBudget MiB -->16<!-- /value --> MiB, a quarter of Loom's frame), counted
  in the larger of Loom's native and JSON serializations, the two an answer to an agent's run
  crosses in; a reading is carried whole or only named by its stamp. A stamp with no reading in
  the answer -- past the last one, or of a pane whose reading Workshop refused -- names a pane to
  ask alone, from item 0 under an empty stamp: it names the pane, not a reading to continue. A
  `PaneView` page holds as many items as the same two bounds leave; the next is asked from where it
  ended, under the stamp the first page answered, until `total`. Workshop holds nothing between
  asks.
- **A stamp that moved is stale.** A page asked under a stamp the pane no longer stands on -- its
  holder, incarnation, window or what it shows moved -- is refused as stale, in words, and the
  reader reads the pane again from item 0. What the pane shows moving moves the fingerprint, a
  caret or a colour too; so does the pane's place moving (a resize, a move, a title row shown or
  hidden, a metric renewed); a reload in place moves the incarnation, and a window started again
  the epoch. A cover that moves between pages shifts the items `from` counts and leaves the stamp
  alone, so a reader compares each page's `total` and `covered` with the first page's and reads
  the pane again from item 0 when they differ; a `from` past `total` is refused, in words saying
  to read it again from the start.
- **A covered word is not said.** A word or part whose place meets anything drawn over the pane --
  a pane in front of it, an open menu, Workshop's or a pane's, arranging, over all of it, the
  band where the pane stands outside the room -- is left out,
  counted in `covered.words`, and never given a point. While a menu is open a primary press
  outside it reaches nothing beside it -- it closes the menu, or is refused -- so every word and
  part said beside it has its place and no point.
- **A drawing in flight says so.** While the window has not yet said what a pane's area shows,
  or waits for the pane's whole drawing, its reading says `in_flight` and no word, at any page: its
  settling moves its stamp, and the notice. A closed pane, one not presented, one no window shows,
  and one whose reading the window refused are refused in words; a pane whose provider left with
  its last drawing standing is read as the window shows it, with no point.
- **A few desk reads a second, each asker.** Every `DeskRead` composes the whole desk, so Workshop
  answers at most `kDeskReadsPerSecond` (<!-- value kDeskReadsPerSecond -->4<!-- /value -->) from
  one asker in any second, refuses the next in words naming the milliseconds until it may read
  again, and counts no read older than a second. Pages are not counted.
- **The desk number.** `desk` moves whenever the `DeskView` Workshop says differs from the last
  one it said, and holds while nothing does, so a reader knows whether the geometry it read still
  stands. Consistency is per pane: each reading stands on its own stamp.
- **A notice when the desk moved.** Workshop publishes `DeskStamps` as its office at the end of
  each delivery that moved the desk number or a presented pane's stamp -- one notice for every
  repaint that delivery made -- and a `DeskRead` that
  finds the number moved without one publishes it. A delivery that may have moved the desk asks
  the window what each presented area shows since it last said, so a pane drawing in its own area
  moves the notice too. A repaint that moved nothing publishes nothing. The notice is the whole
  state, so a relay may keep only the newest (`latest`): the newest stands for every one before
  it. Its `cause` is best effort.
- **Layouts reads as Workshop paints it.** Asked as `zengine.workshop` `layouts`, or in a
  `DeskRead`, its rows are words and its tabs are parts Workshop names, as it names its menu's
  lines: `layout:<name>` for a layout's tab -- each byte of the name outside printable ASCII, and
  `%`, `#` and `+`, spelled `%XX` in upper-case hex, so no tab is named `layout:+`, and a name two
  layouts share followed by `#` and its place in the layout order, counted from 1 -- and `layout:+`
  for the tab that makes one. While a layout's name is being typed no tab is named. Its
  fingerprint is that of what it says, with its naming line's caret.
- **The inventory and the keymap answer a `capture` guest.** `PaneInventoryRequested` and
  `KeymapRequested`, answered to an office, are answered to a guest session whose admitted row
  holds `capture` too, to it alone; other personal speech is still answered by nobody.
- **The guest door grants these.** The guest `capture` power grants `DeskReadRequested`,
  `PaneViewRequested` and `PanePointRequested` to `zengine.workshop`, and a row naming
  `DeskStamps` (version 3) in its `observe` follows the notice only with `capture`.

## Delegated shortcuts

`workshop/pane_shortcuts.hpp` declares `PaneShortcuts`: a provider's active global bindings,
proposed to `zengine.desktop`. Each row names a local id, label, pane/action and gesture.
Desktop composes these with its own defaults and returns `PaneShortcutsAnswered` only after
Workshop judges the whole application declaration. Refusal retains the prior mapping. A desktop
activation publishes `PaneShortcutsRequested`; displaced declarations notify registered providers
with `PaneShortcutsWithdrawn`. Providers own their recovery policy. An invocation reaches the pane
through Workshop ([`PaneShortcutInvoked`](zengine.workshop.md#paneshortcutinvoked)); a value's
attributed origin is [`PaneValueCarryRequested`](zengine.workshop.md#panevaluecarryrequested)'s
second version. Inventory's image-local tokens bind the exact reference, revision and arrangement
generation; expired or forged tokens do not move anything.

A WeaveId is not a code-incarnation token. Generic providers choose their same-id reload policy.
The shipped Inventory pane keeps its registration acknowledgement flag in the image, not saved
state: a replacement refuses invocation until Desktop acknowledges its fresh declaration.
