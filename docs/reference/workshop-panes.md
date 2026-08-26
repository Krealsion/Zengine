# Workshop panes and setups — reference

**Reference.** The exact contracts behind Workshop's panes: the panel system, the authored pane
window, the external pane protocol, and the persisted setup. This is the page a *tool author*
needs; a maker wants [panes](../workshop/panes.md) and [setups](../workshop/setups.md), and the
task-shaped walkthrough is [making a Workshop tool](../guides/make-a-workshop-tool.md).

Source: [`workshop/pane_vocabulary.hpp`](../../workshop/pane_vocabulary.hpp) ·
[`workshop/setup.hpp`](../../workshop/setup.hpp) ·
[`workshop/panel.hpp`](../../workshop/panel.hpp) ·
[`workshop/arrangement.hpp`](../../workshop/arrangement.hpp) ·
[`workshop/screen.hpp`](../../workshop/screen.hpp).

## The panel system

> A weave may provide a tool; a **panel** is its presentation.

`[+ panel]` on the screen's title row (`p`) opens a small picker over the catalog of panel
kinds Workshop knows how to present (`panel.hpp`). The picker is still the only door — a pane
that is not in the catalog cannot be opened by any gesture at all — and the catalog
has **two halves**: a compile-time constant array of Workshop's own, and a bounded
**session-local runtime catalog** of panes some office actually offered this run (see
*[A weave may offer a pane](#a-weave-may-offer-a-pane-wp-0)* below). The two built-ins are
chosen to be unalike:

| kind | presents | behind it |
|---|---|---|
| `Builder` | one known build target, and how its build is going | a weave holding `zengine.builder` |
| `Info` | the `OBJECTS` list and the `PROPERTIES` inspector | nothing — the document and the session |

`panel == weave` is deliberately **not** an architectural rule, and `Info` is what pays for
that sentence rather than asserting it: opening it sends no message, asks no office and needs
no weave mounted anywhere, and it has no per-panel state for a close to destroy. A Workshop
hosting no tools at all opens it and it works.

**The picker owns panel presence**. One door, both directions:

```text
closed panel  ->  select  ->  open
open panel    ->  select  ->  remove
```

so the picker lists each kind as `open` or `closed` beside its name — a toggle whose current
state is invisible is a gesture a maker has to guess at. An earlier design spelled removal `x`, which was
unambiguous while one kind existed; a second kind would have made that key choose a panel, and
choosing means either a per-panel binding or a focused panel. Both are frameworks this Workshop
has declined, so presence moved wholly to the picker and `x` is an unbound key again.

**`Info` is open at boot**, and it was not always a panel at all: originally `paint`
drew them unconditionally, and the only way to not have them was to edit `paint`. What the
migration moved is where they are painted from; what a maker sees at boot is byte-identical.

- **The panel is not the tool.** The Builder panel holds a *copy* of the last `BuildStatus` the
  Builder tool published, and closing the panel destroys the copy and nothing else. Reopening
  it sends `builder::StatusRequested` and shows the tool's own answer — including `asks N
  ever`, the tool's running count, which comes back as 3 rather than as 1 and is the number a
  panel that owned the state could not produce.
- **Workshop gained two sentences and no powers.** Its grant adds `StatusRequested` and
  `BuildRequested`, both scoped *to the Builder office*. It cannot reach the runner, and the
  only build it can ask for is the one the tool has already named — a panel that has not heard
  from its tool cannot ask for anything, and says so.
- **There are two places, they are named, and there is no layout policy**. A kind
  DECLARES its place in the catalog — `placement::kOverlayStack` or `placement::kSideRegion` —
  and one function turns a place plus a screen into the rectangle that panel occupies:

  ```text
  panel kind  ->  placement intent (panel.hpp)  ->  placement_bounds()  ->  the painter is
                                                                           handed that rect
  ```

  The **overlay stack** is anchored to the canvas's top-left, stacked downwards — the terminal
  overlay's mechanism pointed at the other corner — and it covers the top of the material a
  maker is building. It is **48 cells wide at the 78×22 minimum, where that is exactly the
  workspace's width, and 48 plus half the room's surplus over that anywhere wider** (
  floored: 59 cells at 100 columns of surface, 109 at 200, 329 at 640). *A wider room is shared
  by the pane and the maker* — the same half-share the terminal overlay takes at the other
  corner — so the columns the panel does not take stay reachable at every extent, and the ones
  it does take are its own for paint **and** for the pointer. Its height, its column, its row
  and the blank row between slots do not move with the surface. The **side region** is
  the fixed right-hand column `Info` has always been; it holds exactly one panel, and a second
  kind declaring it is a compile-time refusal rather than two panels painting over each other.
  A slot is earned by being *placed in the stack*, so an `Info` ahead of a `Builder` in the open
  list never pushes it down a slot it does not occupy. `bounds_of(panels, kind, screen)` is the
  one path to an open panel's bounds — a closed one answers with an empty rectangle rather than
  with the place it would have had. When each painter carried its own column instead, the
  two places existed only as agreement between them; what a third kind costs now is a catalog
  row and a painter, neither of which is geometry. Docking, tabs, saved layouts, dragging,
  resizing and focus are all still absent, and what using two unalike panels felt like is the
  evidence for whichever of them gets built.
- **A visible panel occupies pointer space, not only pixels**. Bounds resolved in one
  path made the question sayable and the measured answer was that nobody asked it: a press on a
  cell the Builder was visibly covering took hold of the object underneath, selected it and
  began a drag a maker could not see. The routing rule, in order:

  ```text
  the terminal overlay, while it is open   -- it has the pointer entirely
  a visible panel, by its resolved bounds  -- it occupies what it covers
  the workspace and the document underneath
  ```

  The first is a **mode** and the second is a **place**, which is the whole design: the overlay
  takes every pointer event anywhere, because a maker typing into it is not also authoring in
  the workspace; a panel takes only the presses that land on it, because a maker with a panel
  open *is*. `occupied_at(panels, screen, cx, cy)` is the one question — it names no kind, and
  it asks the same `bounds_of` the painter was handed, so occupancy cannot drift from painting.
  The picker answers too, as the mode that pads itself to a whole slot precisely so it cannot be
  read through. **Only a press is occluded**, and the two asymmetries are why no capture, focus
  or z-order state exists: a press on a panel begins nothing, so a pointer that later leaves it
  drags nothing (the absence of a drag is the memory); a gesture that began on the workspace
  owns the pointer until its release, so the release ends it wherever the hand is — occluding
  that would strand a drag with the button up. **Motion is never occluded**, because stopping a
  drag at a panel's edge would clamp the document: an object would be unable to reach a cell a
  maker is entitled to put it at merely because something is drawn over that cell.
- **A panel is as visible as it is occupied**. Every open panel paints a backdrop
  across the whole of its resolved bounds — the same rectangle `bounds_of` hands its painter
  and `occupied_at` answers about, so there is one geometry rather than two that agree. Until
  Before it had a ground, `Info` painted bare labels: it refused a press across 28×17 cells while an object
  dragged under the column showed its body and its selection ring straight *through* the panel,
  with the panel's own words on top. That was a real defect and it was one rectangle telling a
  maker two different things. What it is **not** is an argument for a painted-cell mask: what a
  hand meets is still bounds, because a mask would make occlusion depend on the length of a
  label. Whitespace inside a panel is the panel's.
- **Removing `Info` leaves its 28 columns empty, deliberately.** Giving them to the workspace
  would not be a tidier layout — the workspace's extent is what a share resolves against, so
  every `%`-wide object on screen would change size because a maker hid a list of names. A
  panel's presence must not be visible in the picture of the document. That rule is what settles
  the drag question above too: a panel may cover what a maker authored, and may not change what
  they are able to author.
- **No focus framework.** Four modes for the keyboard, in priority: the terminal overlay, the
  panel picker, an open inspector draft, then command mode. `p` and `b` were unbound keys and
  `b` still does nothing with no Builder panel open. The inspector's own keys (`up`, `down`,
  Return) belong to `Info`: with it removed they say so instead of driving rows nobody can see,
  which would otherwise open a draft that no screen shows and that `^s` would then refuse to
  save over. The pointer's rule is the three lines above it and is still one `if` per line —
  there is no focused panel, no z-order, no capture and no widget tree, and no panel affordance
  is clickable.
- **A build now has a middle, and the panel shows it**. Pressing `b` paints `asked --
  waiting for it to start`, and a beat later `running -- op #1, 4 out` with the command that is
  running and the newest lines it has said. The two numbers are there because they are what make
  a running build *visible* rather than asserted: a maker who watches `out` climb while moving a
  rectangle has watched Workshop stay alive while a real child process ran, which a build that
  held the pump could not have produced. They stay on the row after it ends, so the evidence
  does not vanish at the moment it becomes a result. While the panel froze instead, "what is
  happening right now" had no answer for the whole time it mattered.
- **Announcing and learning are different.** A status that arrives for a build this panel
  asked for is announced on the notice line; one that merely arrives — the answer to a reopen —
  is shown in the panel's rows and never announced. The first live run got that wrong out loud,
  saying `built zengine-snake -- exit 0` about a build that had finished minutes earlier.
  Non-blocking custody made that distinction worth more, not less: a panel opened *while* a build is running
  is told `running` and must announce nothing, so the fact is held across every intermediate
  condition and released only at one the build will not leave.

The target is `zengine-snake`, and the recipe is the target's own: `${CMAKE_COMMAND} --build
<this build tree> --target zengine-snake`, both baked at configure time. It is deliberately not
one of the weaves this running Workshop has loaded — building one of those would overwrite a
shared library the process has mapped — and `zengine-workshop` rebuilding itself is the same
hazard aimed at the host binary, which is Build+Load's problem and not this phase's.

Workshop's weave lives in `workshop/weave.hpp`, not in the host's translation unit — so the
suite mounts it on a real bus and walks `input message -> gesture -> semantic operation` end to
end. It is mounted **in-process**: nothing asks to unload it, so the
reloadable-weave machinery would be ceremony bought with nothing. The weaves it *loads* are
other packages'. The host gates on `if(TARGET loom::kernel)` like snake's.

## A setup has a name

A maker can **name the arrangement they are working in, save it, close Workshop, start a fresh
one, and get the same panes back**. That arrangement is a **setup**, and in this phase it is
deliberately two things and nothing else:

```text
Setup
    a human name
    an ordered list of PaneRefs        <- each row carries an authored WINDOW; see below
```

- **A `PaneRef` is a provider/service key plus a pane key**, both text
  (`workshop/setup.hpp`). The built-ins are `zengine.workshop/info` and
  `zengine.workshop/builder`, and a saved file spells them that way — never `panel::kInfo`, never
  a catalog ordinal, never a `WeaveId`. Two reasons, and the second is the load-bearing one: an
  ordinal is not durable (renumber the constants and every saved setup opens the other panel),
  and **an ordinal cannot be absent** — there is no integer meaning *a pane this build has never
  heard of*, so a setup built on one would have to drop such an entry on load, which is a saved
  file quietly editing itself.
- **The durable reference lives on the catalog row** (`workshop/panel.hpp`), beside the internal
  kind rather than in a table next to it, so there is nothing for a second table to disagree
  with. Two `static_assert`s over the catalog say every row has a reference and no two rows share
  one — both failures are otherwise silent.
- **Resolution is fallible, and internal lookup stayed total.** `panel_kind(unknown)` still
  answers with the Builder, which is correct for its callers (they derive a kind from a picker
  cursor or an open panel). `resolve_pane(ref, runtime)` is a **second, narrower door** that
  answers with *nothing*: an unknown provider or an unknown pane key resolves to no kind, and **an
  unknown reference never becomes the Builder**. Nothing that meets a file goes through the total
  one. It consults the built-in catalog **and** this session's runtime catalog, and the
  runtime half is a **required argument** rather than a default or a second overload —
  `resolve_builtin_pane` is the narrow question under its own name, so neither can be reached by
  accident. (The parameter earns itself on one line: the setup status text asks this function, and
  a spelling a caller could forget would count a pane a maker can *see* as `1 unresolved` on the
  row directly beneath it.)
- **An unresolved reference is kept, said, and saved again unchanged.** A setup naming
  `third.party.tools/history` loads, stays exactly as authored, produces no panel and no
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
  is which panes a maker *meant*; `panels.open` is which presentations this build could make of
  that intent on this screen. `reconcile` (`workshop/setup.hpp`) is the only thing that opens or
  closes a panel on a setup's behalf, and the picker now edits the **setup** rather than the panel
  list — so a `p` gesture cannot leave the two describing different arrangements. Three cases are
  distinguished on purpose: a panel open on both sides is *left alone* (no lost view, no duplicate
  refresh), one that closes goes through `close_panel` (so a removed Builder's copied status is
  forgotten by the same act), and one that opens performs whatever asking that kind does — which
  for the Builder is the `StatusRequested` the picker has always sent, and for Info is nothing.
- **The setup is a separate value and a separate file from the document.** The same document is
  worth opening in two arrangements and the same arrangement is worth using over two documents, so
  a single project container would make both unsayable. `--setup <path>` (default
  `workshop-setup.json`) is the setup's; `--document <path>` is the document's; `Ctrl+S`/`Ctrl+O`
  remain **document** commands and touch no setup byte. Each reader refuses the other's file by
  name rather than half-reading it.
- **`s` opens a one-line name editor; `enter` validates the name and saves; `esc` cancels. `r`
  restores.** Both were unbound before this phase. The editor opens on the name the setup already
  has (the common gesture is *save this again*), reuses `component::TextBox` for the text, the
  caret and the window, and **swallows the `s` its own keystroke produced** — the key transition
  and the character are two facts that both arrive. It is a fifth mode, reachable only from
  command mode, so it cannot coexist with the picker or with a live inspector draft.
- **The setup line is the band's first row**, which was blank: `setup "Name" saved|UNSAVED [| N
  unresolved] | <path> | s name/save  r restore`, fitted with `detail::fit` so a cut is *marked*.
  The name is first because it is the identity and must never be the thing that elides. `saved` is
  **computed by comparing** the active setup with the one last written or read — never a dirty
  flag, which would need a hand at every place a pane is added or removed.
- **The file has its own format identity and its own bounds**
  (`workshop/setup_persist.hpp`): `"format":"zengine-workshop-setup"`, one version, the Loom's own
  compat codec, deterministic output so `save -> load -> save` is byte-identical, unknown fields
  rejected, and a name/key/count/byte ceiling refused *before* anything is copied into the live
  setup. Loading **returns** a candidate rather than writing into anything, so "a malformed file
  never leaves Workshop halfway restored" is structural: a refusal changes no panel, no setup, no
  Builder view and no document byte. Saving goes through the document's own safe write, so a
  detected failure leaves the last good setup file byte-identical.
- **No resolved rectangle, no metric, and no session interaction state is persisted** in a setup
  file. The picker's cursor, the Terminal's draft, the Builder's copied status and the selection
  are all session; so is the workspace extent, which no setup file carries — the same setup
  restored under a different `SurfaceExtent` yields the same references and different bounds,
  which is the setup's authored/resolved proof. (WIND-2 added authored *place* and *size* to a
  row, which is intent rather than a rectangle; WUX-0 made the extent durable one level **above**
  a setup, in the last-session file — see the final section.)

Deliberately absent, so the absences are decisions: no opaque provider configuration; no multiple
pane instances; no setup catalog, recent list, autosave or import/export; no tabs, docking or
layout weave. Workshop manages **one** active setup path. (The external provider, office and
discovery protocol this list once excluded now exists, bounded — the section below says exactly
how far. So do panel drag/resize, authored panel geometry and an arrange mode, equally bounded
— the section after that says how far.)

## The code authors a default; the maker authors an override; the host resolves the room

A maker can **select a pane, move it, resize it by an edge or corner, change what is in front of
what, reset any of that, and save the arrangement by name** — with the keyboard alone, or with a
pointer, reaching the same doors. Panes may overlap, and **every pane the setup names is reachable
whether or not it can currently be seen.** Setup format is **version 2**, a clean break: a
version-1 file is refused by its number.

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
  A full snapshot would convert every developer default into a maker decision at the moment of
  first save, which is the defect a compared copy removes. The unused numbers
  of a `default` must be zero, so absent intent has exactly one canonical spelling.
- **Each axis is independent.** Moving a pane freezes neither size axis; resizing one axis freezes
  neither the place nor the other axis. A default-width pane goes on taking its half-share of
  the room after a place edit.
- **`cells` on a place is absolute, not an offset** from where the developer put it. An offset is
  authored against a default a later build may change, so the same saved bytes would silently
  mean somewhere else. **Resetting** is what gives back "wherever the default puts it".
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
- **An authored place spends no reactive slot.** A pane the maker put somewhere is not in the
  tiling, so it neither consumes a tile nor can be made to *wait* for one. Resetting its place
  puts it back.
- **The host clips; it never rewrites.** A rectangle running past the canvas is legal authored
  intent, drawn and met and granted room for the part this screen has, and saved exactly as the
  maker said it.
- **Info stays in its reserved column and the Terminal stays a mode.** `screen_of` reserves the
  side column whether or not Info is open, and `room_w` is what every share of the workspace
  resolves against — so a movable Info would change the resolved size of objects in a maker's
  document. Management refuses to author its geometry and says why.
- **`w` opens pane management**, from command mode. Inside it: `tab`/`up` select, `m` move,
  `s` size (`tab` cycles the eight edges and corners, arrows resize), `f`/`b` front/back,
  `r`/`l` raise/lower one, `0` reset (`p` place, `w` width, `h` height, `o` order), `esc` back one
  level. **An edge names an axis and a direction, not an anchor** — a resize writes size and never
  place, so the pane grows from its own corner whichever edge is pulled. Edits commit
  immediately; `esc` is *back*, not *cancel*, and there is no undo.
- **A pointer press claims one gesture until release.** Crossing another pane, crossing the
  Terminal's rectangle, and reordering mid-drag all change nothing about who is being moved.
  Outside management mode nothing about the pointer changed: a selected pane behind another one
  claims no press, so a selection never becomes a click-through, and no selection auto-raises.
- **The picker and management share one list and not one purpose.** The inventory is the union of
  the combined catalog and every `PaneRef` the setup names, so an unresolved pane finally has a
  row. The picker keeps *presence* (selecting an open row removes it); management owns
  *arrangement* and binds no toggle. Seven states, one classifier: `closed`, `unresolved`,
  `refused`, `waiting`, `off-room`, `covered`, `open` — `covered` means every visible cell is
  behind the **union** of what is in front, and one visible cell is enough to be `open`.

## A weave may offer a pane

> **The office authors the pane; Workshop grants the room.**

A weave that is not Workshop can offer Workshop a **pane**: a row in the picker, a panel a maker
can open, and a bounded budget of prose to fill it with. Five shapes are the entire protocol
(`workshop/pane_vocabulary.hpp`) — four for the room and its rows, and [one bounded
press](#a-pane-may-be-pressed):

```text
PaneCatalogRequested   Workshop  ->  everyone   "who has panes?"
PaneOffered            provider  ->  Workshop   "I have this one."
PaneRoom               Workshop  ->  provider   "here is how much prose it gets."
PaneContent            provider  ->  Workshop   "here is what it says."
PanePressed            Workshop  ->  provider   "a maker pressed here, in that room."
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
  overlay stack, and a presentation may only enter `Panels::open` if its rectangle ends at or above
  `kWorkspaceY + room_h`, which *is* `notice_y - 1` — the row the setup line occupies. At the
  78×22 minimum only one overlay slot fits. A resolved reference that does not fit is **waiting**,
  a third picker state that is neither `open` nor `closed`: the authored intent is retained and
  named, growth opens it with no gesture, and a shrink closes the presentation through the ordinary
  close door and destroys its cache.
- **The picker windows the combined population** through `list_window` — the OBJECTS list's own
  function, its own three rules and its own `omitted_text` wording. It did not get taller: the
  markers come *out* of the eight-row budget.
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
  and its catalog row all outlive the presentation; no unload is sent and the picker remains the one
  owner of presence in both directions.
- **Workshop gained two grant rules and no powers.** `PaneCatalogRequested` and `PaneRoom`, both
  `allow_to_any` — the first because the ask *is* the discovery and there is no role to scope it to
  yet, the second because Workshop sends to one resolved role that is runtime data. The Builder
  sentences stay role-scoped; Workshop still commands no lifecycle, loads no weave, reaches no
  Manager, and holds no observation, filesystem, process or network authority. Workshop is now
  mounted **in** the `zengine.workshop` office so a provider can verify its ask — and holding an
  office is not a super-grant: every rule is still checked at every send.
- **The pane protocol grants a provider nothing either** — no canvas speech, no document, no
  filesystem, no process, no network, no screen, no lifecycle, and exactly one
  inbound gesture and nothing that could put text into a pane. That is a fact about the
  *protocol*. It is **not** a containment claim: a trusted in-process dynamic library already shares
  this process's memory, and Loom's current default grant for a normally loaded in-process weave is
  `allow_any`. Visibility did not create those facts and this protocol does not solve them.
- **The witness is a real shared library.** `tests/weavelib/workshop_hello.cpp` is loaded through
  the real Kernel and Manager under a real attested activation, and it is a **fixture, not a
  product**: no host boots it. A registration hook would have proved nothing about the ABI it
  exists to exercise.

Deliberately absent, and each one is a decision: no keyboard, focus, capture, hover, release,
wheel, double-press or drag forwarding, and no reply, disposition or acknowledgement to a press; no
multiple instances of one `PaneRef`; no provider-owned placement, coordinates, docking, tabs or
resize handles; no compositor or second canvas publisher; no unload notification, timeout,
heartbeat, liveness query, `unavailable` state or catalog retraction; **no observation surface of
any kind inside the protocol** — a provider that wants to know something asks its owner with its
own grant, exactly as any weave would, and the five shapes carry no `QueryRole`, no `ListLoaded`,
no Senses and no service registry; no package identity, signature, marketplace or cross-restart
author claim; no out-of-process provider support; no provider scan directory, autoload list or
plugin SDK. **No Loom change of any kind.**

## A pane may be pressed

> **Selection is a fact, not a command.**

A maker can press a row of the `Loaded` pane. The row is marked, and the pane publishes an ordinary
Loom message saying which entry that was. **Nothing in this build listens** — and that is the
phase, not an unfinished half of it.

```text
maker presses a visible row
    -> Workshop resolves WHICH pane by geometry it already holds, and WHERE
       in the room it granted that pane
    -> PanePressed { pane, row, column }        the fifth and last shape
    -> the provider maps the row against the projection it is CURRENTLY showing
    -> LoadedSelected { pane, library, role }   published; nobody answers
```

- **Workshop learns nothing about what a pane's rows mean.** It sends a row and a column of the
  budget it granted, and holds no row identities, no selectable flags, no weave metadata and no
  list-item semantics. Three presses on three different rows produce three messages differing only
  in where the hand was — pinned from a bus tap, which also shows Workshop's whole outbound
  vocabulary is five shapes and that `PaneContent` still travels one way only.
- **The coordinate is the `PaneRoom` lattice and nothing else.** Row 0 is the first row of the
  provider's body, under Workshop's header row, which the provider was never granted and is never
  told about. Every forwarded press is inside `[0, rows) × [0, columns)` — swept over the whole
  rectangle in both media. No pixel, no cell, no canvas coordinate, no window origin and no medium
  identity crosses the seam, so the same gesture in a terminal and in a window arrives as the same
  two numbers. **A press that names no row is not sent**: the header row and the pixel remainder
  under the last prose line of a graphical medium are consumed by the pane and travel no further,
  because a strip too short to fit prose is not a row and rounding it would invent one.
- **A pane that owns visible room owns pointer refusal for that room**, and Workshop decides that
  by occupancy before it sends anything — WP-R0's split, unchanged: which pane owns a press is
  geometry Workshop already holds, so `consumed` never crosses the wire and nothing waits for a
  provider. Management chrome still gets first refusal: the picker, the pane-management mode and
  the Terminal overlay each take the press whole.
- **The press is read against the snapshot the maker actually saw.** Interpreting one asks the
  Weave Manager nothing — the row-to-entry map is returned by the same function that *built* the
  rows, so there is no second calculation to drift. Unload a library under an open pane and press
  the row that still names it: the fact names what was on screen. That is the load-bearing case.
- **The identity is what the pane observed**: the loaded library's name, and the role bound at
  load, with an empty role meaning the kernel bound none. Never a `WeaveId`, never promoted into a
  participant identity, and never a claim that anything is alive now.
- **Selecting is an occurrence, not a state transition.** The same row pressed twice publishes
  twice — a future trigger reading *whenever the maker selects this one* is owed both — while the
  picture does not change, because the mark is already there. Two questions, two answers.
- **Only entry rows select.** The heading, the caveat, the source line, the blank separator and the
  omission marker publish nothing: `... 17 more` is a *population fact*, not a stand-in for one
  hidden weave, and "select the first hidden one" is a gesture this pane does not offer.
- **The selection belongs to the pane.** There is no `Workshop::selected_weave`, no setup-wide
  current selection and no ambient singleton. It is transient runtime UI state, held as a *name* so
  it survives a resize that windows the entry out of sight, cleared only when the absence is
  actually observed — and clearing publishes nothing, because a library going away is not a maker's
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
pipeline; no Message Composer and no query of what may be sent to the selected thing; no selection
history; no `Selection<T>`, `SelectionBus` or global selection vocabulary — one list is not
evidence for a reusable one. No pane-to-pane dependency: `Loaded` knows nothing of Info, the
Terminal, the Builder or any future tool, and opens, closes and targets nothing. **No Loom change
of any kind**, and no setup format movement.

## The desk comes back on its own (WUX-0)

A maker can **close Workshop after arranging it and reopen it into the same desk, at the same
size, with no gesture.** That is a third persisted thing and a third file:

```text
--document   workshop.json           what you MADE
--setup      workshop-setup.json     a desk you NAMED, with `s`, and read back with `r`
--session    workshop-session.json   the desk you were USING, and the room it was in
```

- **One representation of a desk, two files.** `session_persist::WorkshopSession` nests
  `setup_persist::WorkshopSetup` as a field rather than paraphrasing it, so the four layers that
  judge a setup file judge the desk inside a session file (`setup_persist::setup_in`, factored out
  of `from_text` for exactly this). A desk cannot be legal in one file and illegal in the other.
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
  than a hope: a restored window is the maker's chosen size **floored to whole cells**, at most
  `kCanvasCellPx - 1` pixels short on each axis.
- **Position and maximized state are NOT persisted**, and the reason is the same seam read from
  the other side: no message in the Surface vocabulary carries either, in either direction, so
  persisting them would be a new publisher-to-medium protocol rather than a new field.
- **The first picture of a run is Workshop's floor, and the restored room is the second.** A
  medium that has been told nothing has only a run's first picture to size itself from, and a
  graphical one makes that size the smallest the window may ever be dragged to. So
  `on(SurfaceReady)` paints once at the minimum extent and *then* takes the session back — asking
  for the remembered room first would leave a maker unable to shrink their own window.
- **The room, and then the desk into it.** `apply_setup` seats panes against
  `stack_capacity(screen_of(...))`, so how much of a desk can be presented is a fact about the
  screen. The viewport is adopted before the desk is applied; reversing the two leaves a pane
  waiting for room it already had, and that is one of the phase's mutations.
- **Written on an orderly close, by the one door.** `q`, `Ctrl`+`c` and `SurfaceCloseRequested`
  all reach `quit()`, which writes the session before it stops the bus. No autosave, no dirty
  tracking, no background writer — and no crash durability, which is not claimed here or in
  `persist::write_file`.
- **Four distinct answers, not one boolean.** No previous session (silent — a first launch is
  never reported as an error); a session that cannot be read (named, defaults used, and the
  maker's file left exactly as it is); a session read whose viewport is outside the band this
  Workshop is honest at (`78x22`..`640x400` cells — the desk is restored, the size is
  **declined rather than clamped**, and the declined value is named); and everything restored.
- **Neither direction touches the named setup file.** Closing writes a session and leaves
  `workshop-setup.json` byte-identical; restoring a session reads no setup file at all. `s` and
  `r` mean exactly what they meant, and `setup.on_file` — this run's copy of what is in the
  *setup* file — is deliberately not written by a session restore, so a restored session still
  reads `UNSAVED` and that word still means "not written to the setup file in this run".
