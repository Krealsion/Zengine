# Workshop panes and setups — reference

**Reference.** The exact contracts behind Workshop's panes: the pane system, the authored pane
window, the external pane protocol, and the persisted setup. This is the page a *tool author*
needs; a weaver wants [panes](../workshop/panes.md) and [setups](../workshop/setups.md), and the
task-shaped walkthrough is [making a Workshop tool](../guides/make-a-workshop-tool.md).

Source: [`workshop/pane_vocabulary.hpp`](../../workshop/pane_vocabulary.hpp) ·
[`workshop/pane_canvas_vocabulary.hpp`](../../workshop/pane_canvas_vocabulary.hpp) ·
[`workshop/setup.hpp`](../../workshop/setup.hpp) ·
[`workshop/panes.hpp`](../../workshop/panes.hpp) ·
[`workshop/arrangement.hpp`](../../workshop/arrangement.hpp) ·
[`workshop/screen.hpp`](../../workshop/screen.hpp).

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
out loud and opens or closes a pane through the host's two doors. A pane that is not in the
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
answers a presenter that asks. It holds no copy, declares no actions and makes no decision;
the admission seam a per-connection prompt will attach to is the door's
([external host](../workshop/external-host.md)).

`pane == weave` is deliberately **not** an architectural rule, and `Info` is what pays for
that sentence rather than asserting it: opening it sends no message, asks no office and needs
no weave mounted anywhere, and it has no per-pane state for a close to destroy. A Workshop
hosting no tools at all opens it and it works.

**Presence is the desk's, and two doors change it** (`workshop/desktop_seam_vocabulary.hpp`):

```text
PaneLaunchRequested{office, pane}  ->  open it, or focus it if it is open   (never toggles)
PaneCloseRequested{office, pane}   ->  take its row off the live desk       (unloads nothing)
```

Only an office may ask either, and each is answered to the asker. The shipped desktop's Pane
Manager spends them on `Enter` and `x`, over the one inventory Workshop publishes
(`PaneInventory`), with each row's state beside its name — `[open]`, closed, `[room]`, `[load]`
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
  DECLARES its place in the catalog — `placement::kOverlayStack` or `placement::kSideRegion` —
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
  and the blank row between slots do not move with the surface. The **side region** is
  the fixed right-hand column `Info` has always been — a place, reserved out of nothing, with
  the workspace running underneath it. Exactly one built-in kind DEFAULTS there, and a second
  declaring it is a compile-time refusal; a setup file may put any pane there by name, and two
  panes in one place is a desk saying so rather than an accident.
  A slot is earned by being *placed in the stack*, so an `Info` ahead of a `Builder` in the open
  list never pushes it down a slot it does not occupy. `bounds_of(panes, kind, screen)` is the
  one path to an open pane's bounds — a closed one answers with an empty rectangle rather than
  with the place it would have had. When each painter carried its own column instead, the
  two places existed only as agreement between them; what a third kind costs now is a catalog
  row and a painter, neither of which is geometry. Docking, tabs, saved layouts, dragging,
  resizing and focus are all still absent, and what using two unalike panes felt like is the
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
  ([the weaver's page](../workshop/builder.md#loading-a-built-artifact-into-the-plan)). The choice
  is genuinely the pane's — what the tool holds is what it *built*.

What can be built is an **authored file** now, not a target compiled into the executable
([Builder](../workshop/builder.md)); what this Workshop ships is a recipe for
`zengine-skin-tui-block`, in Zengine's own build tree. That target is deliberately not one of
the artifacts this running Workshop has loaded — building one of those would overwrite a shared
library the process has mapped — and `zengine-workshop` rebuilding itself is the same hazard
aimed at the host binary, which stays out of scope: nothing here reloads a live artifact, and
an already-loaded artifact is refused in words rather than replaced.

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
    an ordered list of PaneRefs        <- each row carries an authored WINDOW; see below
```

- **A `PaneRef` is a provider/service key plus a pane key**, both text
  (`workshop/setup.hpp`). The built-ins are `zengine.workshop/info` and
  `zengine.workshop/builder`, and a saved file spells them that way — never `pane_kind::kInfo`, never
  a catalog ordinal, never a `WeaveId`. Two reasons, and the second is the load-bearing one: an
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
  (`workshop/setup_persist.hpp`): `"format":"zengine-workshop-setup"`, one version, the Loom's own
  compat codec, deterministic output so `save -> load -> save` is byte-identical, unknown fields
  rejected, and a name/key/count/byte ceiling refused *before* anything is copied into the live
  setup. Loading **returns** a candidate rather than writing into anything, so "a malformed file
  never leaves Workshop halfway restored" is structural: a refusal changes no pane, no setup, no
  Builder view and no other file's byte. Saving goes through the one safe write every durable file
  here uses, so a detected failure leaves the last good setup file byte-identical.
- **No resolved rectangle, no metric, and no session interaction state is persisted** in a setup
  file. A list's cursor, the Terminal's draft, the Builder's copied status and the selection
  are all session; so is the workspace extent, which no setup file carries — the same setup
  restored under a different `SurfaceExtent` yields the same references and different bounds,
  which is the setup's authored/resolved proof. (A row's authored *place* and *size* are intent
  rather than a rectangle; the extent is durable one level **above** a setup, in the
  last-session file — see the final section.)

Deliberately absent, so the absences are decisions: no opaque provider configuration; no multiple
pane instances; no setup catalog, recent list, autosave or import/export; no tabs, docking or
layout weave. Workshop manages **one** active setup path. (The external provider, office and
discovery protocol this list once excluded now exists, bounded — the section below says exactly
how far. So do pane drag/resize, authored pane geometry and an arrange mode, equally bounded
— the section after that says how far.)

## The code authors a default; the weaver authors an override; the host resolves the room

A weaver can **select a pane, move it, resize it by an edge or corner, change what is in front of
what, reset any of that, and save the arrangement by name** — with the keyboard alone, or with a
pointer, reaching the same doors. Panes may overlap, and **every pane the setup names is reachable
whether or not it can currently be seen.** The arrangement lattice is **fine**:
authored pane geometry is held in *sub-cell units* — 1/48 of a canvas cell, `subcells` in the
file — so a pane a weaver dragged by a single window pixel differs from its neighbour by a few
of them, while a pane on a cell boundary is an exact multiple and the character medium's
picture of it has not moved by a byte. Setup format is **version 3**; a **version-2**
whole-cell file still loads, its cells mapped exactly onto the finer lattice (x 48), and the
next explicit save writes version 3. A version-1 file is refused by its number, as ever.

```text
authored setup                 resolved presentation          session interaction
    PaneRef                        current seat                   selected PaneRef
    place  {mode, x, y}            current rectangle              management step
    width  {mode, amount}          current clipping               chosen edge/corner
    height {mode, amount}          projection / refusal           pointer gesture custody
    front  (a canonical rank)      visibility and hit order
                                   external PaneRoom
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
- **`subcells` on a place is absolute, not an offset** from where the developer put it. An
  offset is authored against a default a later build may change, so the same saved bytes would
  silently mean somewhere else. **Resetting** is what gives back "wherever the default puts
  it". The unit is medium-independent on purpose: a sub-cell is a fraction of the canvas's own
  cell, never a monitor pixel, so a desk keeps its meaning when the hardware changes.
- **`pixels` is declared, valid everywhere, and currently unprojectable.** No medium in this build
  publishes a trustworthy per-axis device-pixel scale for a canvas cell — the text metric
  identifies a medium that sets real *type*, which is a different fact, and `kCanvasCellPx` is one
  Skin's layout number that `surface/pointing.hpp` forbids Workshop to hold as a standing fact. So
  a pixel axis **saves, loads and round-trips exactly**, and is **refused at projection**, whole:
  the pane is not presented, rather than presented at the default width with an honoured height.
  There is no fallback. The future rule, once a real scale exists, is
  `cells = max(1, pixels / scale)`, floored, per axis.
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
  [workshop/terminal.md](../workshop/terminal.md)). `screen_of` reserves
  nothing across the width: `room_w` is the surface, it is what every share of the workspace
  resolves against, and the right column stands on it. Management authors Info's geometry like
  any other pane's. The shipped default setup is what opens it at the right edge, by naming
  that place (`"mode": "right-column"`) rather than by coordinates no desk could know.
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
  Only a gesture refused on every axis it moved writes nothing. Edits commit immediately;
  `esc` is *back*, not *cancel*, and there is no undo.
- **Graphical interaction is pixel-responsive; the TUI stays honestly cell-grained.** A window
  pointer's press and motion are spent at their own resolution — one pixel of hand is four
  sub-units of lattice, with no whole-cell threshold anywhere on the path — and what a medium
  paints is the *one quantization law* at its own grain: a fine span `[L, R)` lands on the
  device units `[floor(L/g), floor(R/g))`, one window pixel or one terminal cell per unit. Hit
  testing floors by the same grain, so the first painted pixel of a fractional edge answers
  the hand and the pixel before it does not. A terminal therefore shows a finely-placed pane
  on the cells its floored edges cover — snapped, truthfully — and projecting it rewrites
  nothing: exact-cell values stay exact, sub-cell values resolve deterministically, and the
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

## A weave may offer a pane

> **The office authors the pane; Workshop grants the room.**

A weave that is not Workshop can offer Workshop a **pane**: a row in the Pane Manager, a rectangle
a weaver can open, and a bounded budget of prose to fill it with. Five shapes are the protocol's
core (`workshop/pane_vocabulary.hpp`, which declares every shape that crosses today) — four for
the room and its rows, and [one bounded press](#a-pane-may-be-pressed):

```text
PaneCatalogRequested   Workshop  ->  everyone   "who has panes?"
PaneOffered            provider  ->  Workshop   "I have this one."
PaneRoom               Workshop  ->  provider   "here is how much prose it gets."
PaneContent            provider  ->  Workshop   "here is what it says."
PanePressed            Workshop  ->  provider   "a weaver pressed here, in that room."
PaneEscapeUnspent      provider  ->  Workshop   "the Escape you sent me was unspent here."
                                                   (under that Escape's own correlation)
```

- **`PaneOffered` and `PaneContent` carry no provider field, and the absence is the enforcement.**
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
  sentence is simply gone; Workshop then office-publishes `PaneCatalogRequested` on `SurfaceReady`
  — its ordinary startup hook, because Loom deliberately sends no `zen.Activated` to a *native*
  mount and inventing one would be a fake lifecycle event — and every provider that verifies the
  authorship re-offers. Repetition is harmless: identity de-duplicates, so a re-offer refreshes a
  descriptor in place and grows the catalog by nothing.
- **Everything a live message can make Workshop retain is bounded before a byte is kept.** A
  provider key and a pane key by the setup file's own `check_pane_key`; a name at 32 bytes and a
  summary at 64, neither empty, neither all spaces, neither carrying a control byte; the combined
  catalog at **32 total entries**, built-ins included, so at most thirty distinct runtime
  `PaneRef`s. Admission is atomic in both directions — an invalid first offer adds nothing, an
  invalid *refresh* leaves the last accepted descriptor whole, and a refresh is still allowed while
  full because the bound is on how many distinct panes are held rather than on how often a provider
  may correct itself.
- **A runtime offer cannot shadow a built-in**, and two offices offering one pane key stay two
  panes: the `PaneRef` is the *pair*, so neither office can refresh or overwrite the other's row.
- **The setup file did not move.** `setup_persist.hpp` is untouched, the schema is the same version
  1, and no descriptor, content, room, handle or liveness fact is saved. A setup naming
  `third.party/hello` loads, stays exactly as authored, resolves the moment that office offers the
  pane — *without the file being touched* — and is unresolved again in a fresh process where the
  provider is absent.
- **Workshop chooses the placement and refuses what will not fit.** Every external pane goes in the
  overlay stack, and a presentation may only enter `Panes::open` if its rectangle ends at or above
  `kWorkspaceY + room_h`, which *is* `notice_y - 1` — the row the setup line occupies. At the
  78×22 minimum only one overlay slot fits. A resolved reference that does not fit is **waiting**,
  a third state that is neither `open` nor `closed`: the authored intent is retained and
  named, growth opens it with no gesture, and a shrink closes the presentation through the ordinary
  close door and destroys its cache.
- **A list windows the combined population rather than truncating it** — the picker did, through
  `list_window` and its own `omitted_text` wording, and the desktop's Pane Manager does in its own
  image (keeping its cursor's row in view, and counting what is above and below it). The markers
  come *out* of the list's row budget; the pane does not get taller.
- **The room is `fit_region`'s answer and nothing else.** Workshop owns one header row naming the
  pane and its office, and grants the body beneath it as *prose rows and columns* — never a
  rectangle, a cell, a pixel, a font or the identity of the medium that answered. It is sent when
  the pane opens, when a valid re-offer refreshes it, and when the resolved capacity changes, and
  at no other time. Two things move that capacity: a **wider surface**, because a stack slot
  takes half the room's surplus — at 200×60 the grant is `8×109` where the minimum
  composition's is `8×48` — and a real **text metric**, because a face that is not a cell fits a
  different amount of prose in the same rectangle. A *taller* surface moves neither: a slot's
  height is fixed. A grant clears the cached rows *before* it is sent, so the cache can never
  hold rows admitted under a wider room.
- **Over-budget content is refused whole, never truncated.** Too many rows, one row too wide, or a
  byte outside `SurfaceTextRow`'s plain-ASCII contract, and *not one row* is kept: the pane clears
  what it was showing, leaves one bounded Workshop-owned refusal, names only the already-admitted
  `PaneRef` in the notice, and stays open so a later valid update recovers it. A pane showing eight
  rows of a twelve-row answer, unmarked, would present a partial sentence as the provider's whole
  one.
- **Silence is `waiting`, and never `unavailable`.** Loom gives Workshop no participant-visible
  provider-unload notification and a sender's silence does not prove a delivery's fate — so nothing
  times out, nothing polls, no catalog row is withdrawn and no setup reference is deleted. If a
  provider disappears after sending valid content, Workshop **cannot know that happened** and goes
  on showing the last rows that office reported. That is a stated limit, not liveness.
- **Closing destroys only Workshop's copy.** The provider's weave, its office, its semantic state
  and its catalog row all outlive the presentation; no unload is sent, and the close door —
  a close, never a toggle — is the one way off the desk.
- **Workshop gained two grant rules and no powers.** `PaneCatalogRequested` and `PaneRoom`, both
  `allow_to_any` — the first because the ask *is* the discovery and there is no role to scope it to
  yet, the second because Workshop sends to one resolved role that is runtime data. The Builder
  sentences stay role-scoped; Workshop still commands no lifecycle, loads no weave, reaches no
  Manager, and holds no observation, filesystem, process or network authority. Workshop is now
  mounted **in** the `zengine.workshop` office so a provider can verify its ask — and holding an
  office is not a super-grant: every rule is still checked at every send.
- **The pane protocol grants no application or host authority** — no whole-screen canvas
  publication, document, filesystem, process, network or lifecycle access. Its optional local
  canvas speaks only inside room Workshop grants; input follows the pane seam below. That is
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
selected, still where the keys go, and that Escape is still the last gesture Workshop handled — a
key, some text, a press or the wheel since makes it stale — and there is no answer and no retry.

Deliberately absent: no focus-changed notification, idle hover over a text pane (a canvas pane
may ask for one, [below](#optional-pane-local-canvas)), key release or double-press
notification. Secondary buttons and the optional local canvas have explicit release custody;
the older prose sweep still ends silently. Keys and text cross as `PaneKey`/`PaneTextInput` to the pane
a weaver last pressed into, the wheel as `PaneWheel` — the notches, forwarded,
following the pointer as a press does — a sweep as `PaneDragged`, and an action a pane declared
beside its offer as `PaneActionRequested`, the resolved id in place of the key. There is no reply,
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

## Optional pane-local canvas

`workshop/pane_canvas_vocabulary.hpp` defines a bounded drawing capability beside prose.
A holder accepting both the current `PaneCanvasRoom` and
`PaneCanvasPointer` receives a room when Workshop can resolve its current identity; a host
without this capability continues to grant `PaneRoom`, so a provider can keep a text fallback.
Once a canvas grant exists, Workshop ignores that pane's prose content until the canvas
capability leaves. Keyboard, text input, actions, pane placement and menus keep their owners.

`PaneCanvasRoom` v2 carries `pane, grant, width, height, grain, graphical,
text_advance_px, text_line_px`. It grants local coordinates in
1/48 canvas-cell units, below the title and inside the chrome. `grain` states the medium's
device resolution in those units. `graphical` describes its reported device scale, not the
presence of a prose font. The text metric is the active medium's measured advance and line
height; zero means the cell projection, including a graphical medium whose font is unavailable.
Zero width or height revokes usable room. The host mints a new
positive grant when room geometry, text metric or provider changes and on re-offer; never persist grants
or held gestures in a provider's reload state. A fresh image waits for a fresh room.

`PaneCanvasContent` v2 carries `pane, grant, picture, rects, labels, texts` and replaces one
whole picture. Rectangles
carry local `x,y,w,h,role`; labels carry `x,y,text,role`. Rectangles are painted in vector order,
then labels and measured text above them, on the pane's own plane. Workshop clips before translating, so no
primitive can escape its body. Offscreen positions are legal, allowing a provider to own pan
and zoom. Labels are fixed-size canvas lettering: one 48-by-48 cell per printable ASCII byte,
with partially visible edge glyphs omitted whole; they are not prose-font text or scaled type.
Lines may be made from thin rectangles; there are no paths, textures, transforms or scenegraph.

`PaneCanvasText{x,y,text,role,caret_col,sel_begin_col,sel_end_col}` is one line of measured
prose. Its `x,y` name the local region origin **before** the text inset, not a baseline.
Caret and selection use ASCII source-byte columns; negative means absent and the selected
range is `[begin,end)`. The existing Surface text renderer supplies the type, caret and
highlight. Its ground stays beneath the text, so a graph or button background shows through.

Use the installed `workshop/pane_canvas_text.hpp` for the same sizing the host uses.
`canvas_text_metrics(room)` returns advance, line height, inset and device grain in local
subunits; the complete one-row region is `line + 2*inset` tall. `clip_canvas_text(run, clip,
room)` returns the visible adjusted run and its exact padded `bounds`, suitable for hit tests.
The clip may be a sidebar or graph viewport inside the room. For example:

```cpp
PaneCanvasText run{0, 0, "Configure", surface::role::kFill};
auto placed = clip_canvas_text(run, {0, 0, room.width, room.height}, room);
if (placed.visible()) {
    content.texts.push_back(placed.text);
    // Retain placed.bounds beside this picture's action for hit testing.
}
```

Clipping removes whole leading/trailing glyphs and whole rows; it does not reflow or shift
surviving glyphs. The entire generated region, including its insets, remains inside the clip.
Caret and selection columns follow the crop, including the cell projection's inserted caret.
An empty line with a caret reserves one column. Keep measured sizes out of saved authoring data:
they describe the current room, not a document or graph's durable coordinates.

The v2 room and content identities must be used together. Fixed labels and the pointer and
rejection schemas retain their versions and meaning; no Surface schema changed. Participants
whose declarations change must be rebuilt and restarted before using the new conversation.

Admission is whole: positive extents, one of the five Surface roles (including the opaque
`kGround` background), at most 4096 rectangles, 2048 labels, 2048 text runs, 4096 bytes per label
or run, and 131072 combined text bytes.
Both text forms require printable ASCII; a nonnegative caret and each selection range must
lie within its run. A positive picture number
must strictly increase within its grant. Authenticated stale or malformed updates receive
`PaneCanvasRejected{pane,grant,picture,reason}` and leave the last good picture unchanged.
Unauthenticated content changes nothing. A new room clears the old picture's admission and
fence. When only the same provider's geometry changes, Workshop may keep the old image clipped
to the new body with an **updating** marker. This preview cannot receive input and does not
claim the provider has answered; valid new content replaces it. It never survives a close,
zero room, re-offer, owner/capability change, or changed text metric.

`PaneCanvasHover{pane,grant,picture,x,y,over,carrying}` is the one idle-pointer fact, for a
provider whose holder accepts it: Workshop tells the canvas on top under a pointer that holds no
button where it rests, in local subunits on the picture handed to the medium, once per place, and
tells it `over=false` once when the pointer leaves its body, a mode or menu opens, a sweep
takes the motion, or any press begins: the gesture a press begins owns the pointer.
`carrying` is true while a carried value is over it, so a receiver may mark where it would land;
a drag's carry is told the hover too. A new room puts the hover down with no leave. It is
presentation only: no selection, focus, keyboard or gesture moves, and nothing reads what the
canvas drew. A terminal reports no idle pointer, so there only a carried drag is told.

`PaneCanvasPointer` carries `pane,grant,picture,gesture,phase,button,x,y,modifiers,dx,dy,
keys_went_here`. Phases are `canvas_pointer::kPress`, `kMove`, `kRelease`, `kLost`, and
`kWheel`; buttons are 1/2/3 and a wheel uses 0. Press and wheel name the fenced picture actually
handed to the medium, using the same `PictureStamp` as prose. Providers judge that identity
before hit testing. A held gesture keeps the press's grant, picture and gesture number through
motion and release even when its own drag causes repaint; only its provider interprets it.
Coordinates may leave the room while held. A release arrives under a mode or outside the pane;
closing, re-offering, changing its room or holder, losing understood coordinates, or opening a
modal surface ends custody with `kLost`. A duplicate press ends the prior hold first. The host
sends to the granted provider identity, so a successor cannot inherit a predecessor's drag.
Providers must also reject unknown grants, including queued input received after in-place
reload. A press refused by Loom ends host custody without inventing a release to the pane.

Secondary canvas presses establish the existing menu continuation, with the correlation of
that pointer message. `PaneMenuRequested` and `PanePassRequested` echo it normally; host and
presenter retain menu custody. Primary presses retain Workshop's ordinary selection/focus
behavior, and a primary press may carry a value out as a prose press does: echoing the press's
correlation, the provider asks `PaneOperationRequested` and then
`PaneValueCarryRequested{drag=true}`. Once Workshop accepts the carry it ends the press's hold
with `kLost`, so later motion and the release are the carry's, and the release places the value
where the hand lets go; a release that arrived before the carry is retained, and a click that
never moved places nothing. No key release, font scaling, or physical-display timing
guarantee is added beyond the hover door above. The host has no node, wire, port, selection,
pan, or zoom semantics.

## A pane may be pressed

> **Selection is a fact, not a command.**

A weaver can press a row of the `Loaded` pane. The row is marked, and the pane publishes an ordinary
Loom message saying which entry that was. **The pane does not know who listens**: Compose is one
listener, and takes that entry as the target it composes for
([Inventory to Compose](../workshop/inventory-compose.md)).

```text
weaver presses a visible row
    -> Workshop resolves WHICH pane by geometry it already holds, and WHERE
       in the room it granted that pane
    -> PanePressed { pane, row, column }        the fifth shape
       (or v2::PanePressed { pane, row, column, keys_went_here }, below)
    -> the provider maps the row against the projection it is CURRENTLY showing
    -> LoadedSelected { pane, library, role }   published; nobody answers
```

- **Workshop learns nothing about what a pane's rows mean.** It sends a row and a column of the
  budget it granted, and holds no row identities, no selectable flags, no weave metadata and no
  list-item semantics. Three presses on three different rows produce three messages differing only
  in where the hand was — pinned from a bus tap, which also names every shape Workshop says across
  that life and shows that `PaneContent` still travels one way only.
- **A second version says whether the keys were already there, and only that.**
  `v2::PanePressed` is the same place plus `keys_went_here`: true exactly when ordinary keys were
  reaching this pane at the instant of the press — no mode, naming line or menu had
  them, and the keyboard was pointed at this pane. Workshop reads it, and the row, *before* the
  press moves the keyboard, so a press that brings the keys back says `false`, and a press on a
  pane whose titles are hidden names the row painted where it landed (that pane's title returns
  with the keys, and the smaller room it leaves is granted right after the press). It is a fact,
  not an instruction: the Files pane opens a row only on a press that says the keys were already
  its own and that row was already selected, and a pane with no such rule ignores it. Nothing is
  said when the keys leave a pane.
- **One press crosses once, in the version its pane can read.** Workshop sends the second
  version only when the host answers that the office's current holder accepts it — read from the
  bus's own role table and accept-sets — and the first version otherwise, unchanged, so a pane
  built before the second existed hears exactly what it always heard. A first-version press states
  no fact: a pane that accepts both must treat it as "not known" (Files selects on it, and Return
  still opens). The answer is an inspection, not a promise: the office is resolved again at
  delivery, and if another holder without the second door has taken it by then, Loom refuses that
  one press, records the refusal against Workshop's send, and nothing is sent again. A pane that
  adds the second door changes what it accepts, which Loom will not reload in place: restart
  Workshop to load it.
- **The coordinate is the `PaneRoom` lattice and nothing else.** Row 0 is the first row of the
  provider's body, under Workshop's header row, which the provider was never granted and is never
  told about. Every forwarded press is inside `[0, rows) × [0, columns)` — swept over the whole
  rectangle in both media. No pixel, no cell, no canvas coordinate, no window origin and no medium
  identity crosses the seam, so the same gesture in a terminal and in a window arrives as the same
  two numbers. **A press that names no row is not sent**: the header row and the pixel remainder
  under the last prose line of a graphical medium are consumed by the pane and travel no further,
  because a strip too short to fit prose is not a row and rounding it would invent one.
- **A pane that owns visible room owns pointer refusal for that room**, and Workshop decides that
  by occupancy before it sends anything: which pane owns a press is
  geometry Workshop already holds, so `consumed` never crosses the wire and nothing waits for a
  provider. Management chrome still gets first refusal: the contextual menu and the arrangement
  each take the press whole (the Terminal was a third until it became a pane, and a pane's
  boundary makes a press its own by geometry).
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

## The second button, a menu a pane asks for, and a picture's number

> **A right-click is the pane's first, delivery is the disposition, and a pane keeps no menu
> state.**

Three more things a pane may do, each an ordinary optional door in
`workshop/pane_vocabulary.hpp`, and none of which changes a pane that does not take it:

```text
PaneButton          Workshop -> provider   button 2 or 3 down / up at (row, column), or a `lost`
                                           release the host sent because no hand could;
                                           `picture` is the number of the picture shown
PanePassRequested   provider -> Workshop   "that press was not mine": the host's own pane menu
                                           opens once, while the press is the latest act
PaneMenuRequested   provider -> Workshop   present these rows (id, label) beside (row, column),
                                           about `subject`, continuing the gesture by its number
PaneMenuAnswered    presenter -> provider  chosen + id, or not chosen + why; once per request
                                           (Workshop answers only an ask it refused, or one
                                           whose presenter left, was replaced, could not be told
                                           the menu was withdrawn, or gave it back unanswered)
PaneManageRequested provider -> Workshop   open the host's pane menu on (office, target);
                                           continues a menu answer, once
v3::PaneContent     provider -> Workshop   rows + a `picture` number for this row-to-meaning map
v3::PanePressed     Workshop -> provider   v2's press + the picture the press was aimed at
```

- **Delivery is consumption.** A secondary press over a pane's body is sent only to a holder
  whose accept set has `PaneButton`, read off the bus at the send; nothing opens, and neither
  selection nor keys move. The release goes to the pressing pane wherever the pointer is,
  unclamped, and under an open mode too. A pane closed while a button is down hears one `lost`
  release; a press of a button the host believes down ends the old hold aloud, never silently.
  The title row, the border, a tab and the room still get the host's own menu, and so does a
  right press in a body whose holder lacks the door: nothing is sent and no keys move, and the
  host's pane menu opens at the press. A right press is never lost to a shipped pane either —
  each acts, offers its rows, or hands the press back where it has nothing to offer.
- **Every continuation echoes a number.** The host mints a correlation per secondary press and
  per `PaneActionRequested`; a pass-back or a menu request echoes it in Loom's envelope (the
  `PaneEscapeUnspent` discipline) and is judged where the host acts: newest press of its
  button, unspent, its pane on the desk, and no newer act since — a release completes its press
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
- **A press names its picture.** A pane that composes with `v3::PaneContent` numbers each
  composition — the shipped panes use `component::RowMap`, whose number moves exactly when the
  row-to-meaning map does, so a repaint that moves no row keeps it — and the host echoes on
  `v3::PanePressed` and `PaneButton` the number of the picture the medium held when the press
  was read: a newer picture counts only once the medium has handled the canvas that showed it,
  so a press queued ahead of new content, or read before the medium handled it, names the older
  picture. A pane acts only when that is its current map's, else refuses in words. What this
  orders is Workshop's handoff of the canvas against the input queued behind it; it is no
  evidence of when a display physically showed the picture, nor of where input the platform
  buffered came from. What it does not close: the medium's own drawing latency after it handled
  the canvas, and a press the platform buffered before the input beat read it; that residue is
  named, and no frame history is kept.
- **A gesture a menu may continue.** A menu request must echo the correlation of the gesture it
  continues, and three gestures carry one: a secondary press (`PaneButton`), a declared action
  sent by key (`PaneActionRequested`), and a **primary press** (`PanePressed` and its later
  versions). All three are judged the same way where the menu would open — this pane, this
  number, and still the weaver's latest act — and each is spent once, so a late or replayed
  request is refused in words rather than opened over whatever the weaver did next. The primary
  press is what lets a pane that draws its own controls answer a click on a `[menu]` of its own;
  it moves no keys and no selection, exactly as the other two do not.
- **The helpers are optional and installed beside the protocol.** `workshop/pane_escape.hpp` is
  Escape's default ([above](#escape-in-a-pane-that-holds-the-keys)). `workshop/pane_menu.hpp`:
  `Offer(pane, subject).at(row, col).row(id, label).send(mail, office)` builds and sends the
  request continuing the delivery's gesture and returns its `Asked`; `.submenu(id, label)` is a
  row whose choice opens another menu, its label ending in the one mark the host's own groups
  wear (`kSubmenuMark`, " >") -- a row that asks for more input ends in "..." instead; `pass_back`, `manage`,
  `take_keyboard` and `HeldButton` are the other lines a consumer would otherwise write.
  `take_keyboard_continuing` is `take_keyboard` under a number the pane names rather than the
  delivery's own, for a chosen row whose edit opens only after an office has answered: the pane
  keeps the choice's number across that round trip and spends it where the line appears, and
  the host judges it exactly as it judges the same-delivery form — once, and only while that
  choice is still the weaver's latest act. A pane
  may write the raw shapes instead, and then owes the four checks `Asked::take` makes. The
  shipped `examples/guard-pane` consumes the button and asks for nothing; the Pane Manager and
  the Hotkeys pane offer menus; the Neovim editor passes a right press and release to Neovim and
  nothing more (no secondary drag crosses the seam).

### The menu presenter, and replacing it

> **Presenting a pane's menu is an office, held by an ordinary weave.**

`workshop/presenter_vocabulary.hpp`, installed beside the protocol (`zengine::pane`), is the whole
seam between Workshop and the participant in `zengine.presenter`:

```text
MenuGranted     Workshop -> presenter   present this offer as menu n, for office/pane, in a room
                                        of rows x columns, with the host's standard rows to show
                                        beneath it and the pane's name; under the request's
                                        correlation
MenuShown       presenter -> Workshop   menu n shows these lines now, numbered as picture p
MenuInput       Workshop -> presenter   the weaver did this to menu n: a key (with the verb the
                                        weaver's contextual rows name), or a press / release on a
                                        line, or a press outside; act number g, picture p
MenuClosed      presenter -> Workshop   menu n is over and its requester is answered, chosen
                                        at act g or not -- or the standard row s was chosen at
                                        act g, which Workshop spends
MenuReturned    presenter -> Workshop   menu n is not mine to carry: I hold no such menu, so
                                        take the interaction back, and here is why
MenuWithdrawn   Workshop -> presenter   menu n is over because the host ended it, and why
PresenterReady  presenter -> Workshop   I hold the office now, carrying menu n (or 0)
HeldMenu        state                   the open menu, as the shipped presenters keep it across
                                        a reload
```

Workshop keeps what is fixed: which ask is eligible, one menu at a time, where the popup opens,
drawing the presenter's lines inside the granted room, which of the weaver's keys and presses
reach it, when custody moves, and the standard rows: which it grants, and spending the one
chosen, while the act that chose it is the weaver's latest. The presenter decides everything
about the menu itself —
whether an offer can be shown, how its lines read, what a key or press means, when it ends — and
answers the requester, which is why a pane authenticates a choice from `zengine.presenter`.

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
(machine-local: a viewport and a desktop position describe *this* machine), while the two
project files keep following the project:

```text
--document   workshop.json           an old object document, from     (launch directory)
                                     before the canvas retired: named
                                     once, never read
--setup      workshop-setup.json     a desk you NAMED, with `s`,      (launch directory)
                                     and read back with `r`
--pane       workshop-pane.json      a PANE you made: its name and    (project directory)
                                     its regions, never where it sits
--session    workshop-session.json   the desk you were USING, the     (per-user state root)
                                     room it was in, and where the
                                     window sat
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
  The session format is **version 4**: the weaver's whole ordered run of layouts, each one an
  ordinary saved setup, plus which position was live, the viewport and the desktop placement.
  Older versions do **not** load through roads this reader carries — it admits one shape and
  nothing else. What reads them is a *conversion*, contributed by an ordinary operator provider
  the arrangement mounts (`zengine-workshop-session-history`); with it, a version-1, version-2
  or version-3 session opens as exactly one layout holding exactly the desk it always held, and
  without it that file is refused by its number, naming the conversion that is missing. The next
  close writes version 4.
- **The viewport is one level above the desk**, and that is the whole reason the session is not
  simply a second setup: the same desk is worth having in a big window and in a small one, so how
  much room the surface had describes the *application* rather than the arrangement. It is
  `{width, height}` in canvas cells, its own shape rather than `surface::SurfaceExtent` — that is
  a message free to grow a field whenever a medium has something new to say, and the text metric
  in particular would be a stale claim about a font the moment it was written down.
- **Cells, because cells are what Workshop knows.** The window belongs to whichever Skin holds
  `zengine.skin`, behind a C ABI; the only thing it publishes about its room is `SurfaceExtent`,
  and the only thing Workshop says back is how large a picture it would like to paint. So the
  durable number is the one that crosses that seam, and the fidelity is a stated bound rather
  than a hope: a restored window is the weaver's chosen size **floored to whole cells**, at most
  `kCanvasCellPx - 1` pixels short on each axis.
- **Position and maximized state ARE persisted, opaquely, and the medium is the
  judge.** The Surface vocabulary's placement pair closed the old deliberate omission: the
  medium reports where its *normal* window sits (its own desktop units, maximized state
  beside it), Workshop remembers the last report in the session — coordinates it cannot
  interpret and does not try to — and offers it back once at restore. The medium then
  validates against the displays that exist *now*: a position wholly on a display restores
  verbatim, any other moves in only until the whole window is on one, and with no display
  truth nothing moves (`surface/agents` law; the arithmetic is `placement_within`). A
  terminal run reports no placement, applies none, and *retains* the remembered value
  rather than erasing it. The saved viewport is the **normal** window's room, so a
  maximized close restores as a maximized window that unmaximizes to the size you chose.
- **The first picture of a run is Workshop's floor, and the restored room is the second.** A
  medium that has been told nothing has only a run's first picture to size itself from, and a
  graphical one makes that size the smallest the window may ever be dragged to. So
  `on(SurfaceReady)` paints once at the minimum extent and *then* takes the session back — asking
  for the remembered room first would leave a weaver unable to shrink their own window.
- **The room, and then the desk into it.** `apply_setup` seats panes against
  `stack_capacity(screen_of(...))`, so how much of a desk can be presented is a fact about the
  screen. The viewport is adopted before the desk is applied; reversing the two leaves a pane
  waiting for room it already had.
- **Written on an orderly close, by the one door.** `q`, `Ctrl`+`c` and `SurfaceCloseRequested`
  all reach `quit()`, which writes the session before it stops the bus. No autosave, no dirty
  tracking, no background writer — and no crash durability, which is not claimed here or in
  `persist::write_file`.
- **Four distinct answers, not one boolean.** No previous session (silent — a first launch is
  never reported as an error); a session that cannot be read (named, defaults used, and the
  weaver's file left exactly as it is); a session read whose viewport is outside the band this
  Workshop is honest at (`78x22`..`640x400` cells — the desk is restored, the size is
  **declined rather than clamped**, and the declined value is named); and everything restored.
- **Neither direction opens a setup file.** Closing writes a session and leaves the standalone
  artifact byte-identical; restoring a session reads no setup file at all. What a restored
  session *does* bring back is each layout's **Setup association** — which file it is related to
  and the last value this Workshop knew that file to hold — because that is a fact about
  Workshop's own knowledge rather than about the disk, and remembering it is not the same as
  going to look.


## Authorizing an input operation and carrying data

`workshop/pane_operation.hpp` exposes `PaneOperationRequested{pane, role, shape, version, gesture}` and
`PaneOperationAnswered{allowed, reason}`. A pane sends the request as its office, naming a
current input gesture or menu choice’s correlation in `gesture`. The request’s own envelope
uses the pane’s normal conversation counter; that counter is shared with its other requests,
so permission and owner replies cannot collide with unrelated conversations. Workshop checks that this pane
still owns that gesture and consumes it once. Authenticated physical input identifies the
weaver; injected input identifies the actual session holder. For an injected actor, Workshop
reads that participant's current Loom authority for the named shape and role. Missing
attribution or permission is a refusal. The answer authorizes this intent at that check; it is
not a new grant, a reusable approval, or an assertion that the owner operation succeeded.
The pane must still send the operation under its own ordinary bus grant and handle its result.

`workshop/pane_carry.hpp` carries an owned reference envelope after such an acquisition:

| Message | Direction and meaning |
|---|---|
| `PaneCarryRequested{pane, label, data}` | Provider to Workshop, continuing its approved acquisition; at most <!-- value kMaxCarryBytes KiB -->64<!-- /value --> KiB and a <!-- value kMaxCarryLabelBytes -->128<!-- /value -->-byte label |
| `PaneValueCarryRequested{pane, label, data, drag}` | Copy acquisition; `drag=true` places on primary release, false uses click-to-place |
| `PaneValueDrop{pane, data, row, column, picture}` | A value copy, separate from the reference door |
| `PaneCarryAnswered{carried, reason}` | Authenticated answer to that request |
| `PaneDrop{pane, data, row, column, picture}` | Workshop to the selected receiver, under a new input correlation; `picture` is the aimed prose picture as for `PanePressed v3` |
| `PaneCanvasValueDrop{pane, grant, picture, x, y, data, source_office, source_pane, token}` | A value copy placed on a canvas pane: the place in the canvas's local subunits, its room grant and the aimed picture, with v2's attribution |

The actor picks up the reference and clicks a receiving pane to place it. Escape cancels;
another actor cannot place or cancel the held reference. If the initiating guest participant
has left, the next attributed input releases its reference. Closing an input session alone
does not end a still-connected participant. An unsupported destination leaves the reference held. Workshop interprets no payload fields and keeps no pointer into its provider. The receiver
owns decoding and the meaning of the drop; subsequent reads or writes need their own authority.
A request that cannot be queued leaves the reference held. A later Loom dispatch refusal
is reported with its destination and attempt; it is never retried automatically.
A successful send is not a completed receiver operation. The reference door serves prose panes
only. A value released or clicked onto a **canvas** pane reaches a provider that accepts
`PaneCanvasValueDrop` as that place in its local subunits, within the room it was granted and
against the picture the medium showed, so the provider hit-tests what it drew; a canvas
provider without that door is sent nothing and the value stays held, as anywhere it is not
accepted. A drag's drop names the picture and place its release met, even when the carry is
answered after the canvas repainted, and a canvas granted afresh in between refuses it in words.
A drop on a canvas is no canvas gesture: it begins no pointer custody.

A value drag starts with the source's primary press, becomes a drag after four pixels or one
cell of motion, and ends at that same actor's release. A simple click selects without transfer.
A release can arrive before the source's acquisition reply: Workshop retains its receiver,
position and aimed picture under the same gesture. A later key, press, text or wheel makes the
old continuation stale. An absent or replaced receiver cancels; nothing is automatically retried.
Escape cancels. Source bytes remain owned copies throughout; a receiver still authorizes its
own writes. A value release to an unsupported place cancels the drag, rather than leaving an
invisible item held after the button is up.

The first consumers are [Inventory and Info](inventory.md#inspect-and-edit-through-workshop).
The reference route carries an `InventoryReference` in the pair codec; the value route carries
the actual item and its separate metadata. The distinct doors prevent confusing reference-shaped
user data with a request to follow it.
Neither carrying that value nor displaying a snapshot grants permission to mutate its source.

### A bounded observation a gesture approves

The same header carries a narrow, retained form of that approval for repeated reads:

| Message | Direction and meaning |
|---|---|
| `PaneObservationRequested{pane, role, shape, version, gesture, subject}` | Pane to Workshop: the current gesture's actor approves repeated reads of one shape at one role, about one `subject` string the pane names |
| `PaneObservationContinued{pane, lease, subject}` | Pane to Workshop before every observation under that lease |
| `PaneObservationAnswered{allowed, reason, lease}` | Workshop's answer to either; `lease` is zero when refused |
| `PaneObservationEnded{pane, lease}` | Pane to Workshop when it stops (pause, close, hide, a new subject); unanswered. Lease 0 ends every lease the sender holds on that pane |

A lease is created only through the same current-gesture and actor check as
`PaneOperationRequested`, and spends that gesture. Workshop keeps at most one lease per pane and
sixteen in all, and records its holder, office, pane, role, shape, version, subject and actor.
Every continuation is judged again: the requester must still hold the office that offered the
pane, the pane must be on the desk, the subject must be the approved one, and an injected actor
must still be present with Loom authority for that shape and role. Any lapse refuses and forgets
the lease; only its holder's continuation or ending can end it, so another participant's request
changes nothing. A new request first forgets every lease whose holder no longer holds its office.
A lease is not a grant: the pane still sends each read under its own ordinary
grant. Timers, invalidations and old correlations never create a lease. Leases live in the
running Workshop only. [Info views](../workshop/info-views.md#watch-a-linked-entry) use one to
watch an Inventory entry.

Ending is the pane's duty on every exit, including two a pane can miss. A pane that stops before
the answer arrives ends whatever lease that answer grants, and does not start watching again. A
reloaded provider keeps its WeaveId, so Workshop cannot tell its predecessor's leases from its
own; an arriving image sends `PaneObservationEnded{pane, 0}` for each pane it may observe from.

### Where a painted cell is

`workshop/pane_view.hpp` also answers `PanePointRequested{provider, pane, picture, row, column}`
with `PanePoint{provider, pane, picture, row, column, x, y, space}`: the center of that prose cell
in the input space the medium reads, measured and then resolved by the same press measurer. It is
refused when the pane's handed-out picture is not `picture`, the cell is outside the visible text,
or the pane is closed or covered, exactly as `PaneViewRequested` is. A point is not a gesture;
pressing it is ordinary input. The guest `capture` power grants both queries.

## Attributed value origins and delegated shortcuts

`v2::PaneValueCarryRequested` adds an opaque source-owned token to the pure copy payload.
Workshop stamps the actual source office/pane into `v2::PaneValueDrop`; receivers supporting only
v1 still receive the unchanged copy payload. An empty token remains a copy. The host must explicitly grant the v2 drop alongside v1; adding
an Emit declaration does not widen a host-authored grant. A receiver must not
infer a move from user data or metadata. Inventory's image-local tokens bind the exact reference,
revision and arrangement generation; expired or forged tokens do not move anything.

`workshop/pane_shortcuts.hpp` declares `PaneShortcuts`: a provider's active global bindings,
proposed to `zengine.desktop`. Each row names a local id, label, pane/action and gesture.
Desktop composes these with its own defaults and returns `PaneShortcutsAnswered` only after
Workshop judges the whole application declaration. Refusal retains the prior mapping. A desktop
activation publishes `PaneShortcutsRequested`; displaced declarations notify registered providers
with `PaneShortcutsWithdrawn`. Providers own their recovery policy.

On invocation, Desktop continues the current AppAction through `PaneShortcutInvoked`, naming
the registered holder WeaveId. Workshop checks that holder and the pane's declared
action, then forwards `PaneActionRequested` under the current attributed gesture. This can reach
a hidden pane; a subsequent PaneOperationRequested still needs that current actor's exact
operation authority. Neither shortcut registration nor an old binding transfers authority.

A WeaveId is not a code-incarnation token. Generic providers choose their same-id reload policy.
The shipped Inventory pane keeps its registration acknowledgement flag in the image, not saved
state: a replacement refuses invocation until Desktop acknowledges its fresh declaration.
