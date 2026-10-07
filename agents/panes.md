# Agent law — The pane seam and the tools that arrive through it

Routed detail behind [`AGENTS.md`](../AGENTS.md), for tasks touching the external pane
protocol, `introspection/` or `composer/` — what crosses the Workshop↔provider seam, and the
shipped tools that live entirely on the far side of it. Workshop's own screen and routing law
is [`workshop.md`](workshop.md); public reference:
[`../docs/reference/workshop-panes.md`](../docs/reference/workshop-panes.md) and
[`../docs/reference/introspection.md`](../docs/reference/introspection.md).

## A press crosses the seam as a place, never as a meaning

`PanePressed{pane, row, column}` is the `PaneRoom` budget read backwards — a place in the
lattice Workshop already granted, and nothing that would let a provider locate itself on a
screen.

```text
occupied_at   -> Occupancy{occupied, what, kind}     ONE geometry walk, topmost first
typing_pane(session) == kind -> keys_went_here     } both read BEFORE the press writes
external_press_at(panes, setup, screen, kind,     } Panes::selected and Panes::keyboard
                  titles, space, x, y) -> ExternalPressAt{named, row, column}
                 bounds_of -> external_body_place -> prose_at -> minus the resolved title rows
                 is_runtime_kind(kind)?              -> external_press, and Workshop says NOTHING
                 named == false  =>  no sentence. The press was still the pane's.
                 holder_accepts(office, v2)?  =>  v2::PanePressed, else PanePressed v1 -- once
```

- **`Occupancy` carries the KIND it met, and that is the same answer rather than a second
  one.** The one caller asks a further question of the walk that already decided what is on
  top; resolving the pane again to locate the press would be two geometries for one press,
  which is what the `bounds_of`-for-both rule exists to refuse. `kNoKind = -1` for the bare
  room (and for the picker, while it was), negative for `role::kNone`'s reason. Nothing
  switches on a built-in kind — the question asked is `is_runtime_kind`, which is about which
  SEAM owns the press.
- **Consumed by occupancy, before anything is sent.** A pane that owns visible room owns
  pointer refusal for that room, and nothing waits for the provider: there is no reply shape,
  `consumed` never crosses the wire, and a press that named no row is consumed
  identically and simply travels no further. Arrangement and the contextual surface take
  every press whole, one layer up (the Terminal overlay did too, and the picker answered first
  inside `occupied_at`, until each retired).
- **Workshop says NOTHING on the notice line for an external pane**, and that inverts the rule
  for built-ins. `<name> is here -- nothing under it can be taken hold of` is TRUE of a
  built-in and would be a claim about an OUTCOME here, made before the outcome exists. What a
  press on a provider's row means is that provider's vocabulary; the answer arrives later as
  ordinary `PaneContent`. The layering rule decides it: a refusal belongs to the deepest layer
  whose vocabulary contains the reason, and this layer's does not.
- **The header row is subtracted in BOTH directions or in neither.** `external_body_place`
  reserves the resolved header rows out of the fit before a provider is told its budget, so
  the row a provider means by 0 is the region's prose row under the header. Forgetting the
  subtraction on the way back is the off-by-one that would be invisible until a pane had more
  than one selectable row. The count is `external_title_rows`'s answer — the
  pane-title preference, with the keyboard-holding pane always keeping its title — resolved
  once and carried on `ExternalBodyPlace::header_rows`; the painter, the press path and the
  room grant spend that one answer, and a hidden title RETURNS its row to the provider's
  budget through the ordinary grant-on-change door. **The press path spends the answer the
  PRESSED picture had**, read before the press moves the keyboard: a hidden-titles pane wears
  its title exactly while it has the keys, so the press that brings them names the row painted
  where it landed, and the smaller room the returning title takes is granted right behind it.
  A provider must not let that grant undo what the press selected (Files keeps its selection
  by name across a same-place re-listing).
- **A row that fits no prose is not a row.** Anything outside `[0, rows) × [0, columns)` — the
  header, the pixel remainder under the last prose line of a graphical medium, an unrecognised
  `space` — is refused rather than clamped. Rounding to a nearest row hands a provider a press
  at a place it never wrote to.
- **Workshop holds no selection INSIDE a pane and interprets no row meaning.** What it holds is
  which PANE: the desk's selection and the keyboard's candidate (`Panes::selected`,
  `Panes::keyboard`, [`workshop/focus.md`](workshop/focus.md) WL-FOCUS-01). No row identity,
  no row meaning. Prose sweeps and secondary buttons keep their stated custody; the optional
  canvas below also keeps a grant-bound gesture. Forwarding interprets none of the
  provider's rows, but the press is still a press on Workshop's desk: it can move the selection
  and the keys, give a hidden title back, grant a different room and repaint Workshop's own
  presentation, and what the provider makes of it arrives as that provider's content.
  Nothing here reads `ExternalPane::shown` and nothing may — the moment Workshop looks at a
  provider's rows to decide what a press means, the seam has stopped being one.
- **A press crosses ONCE, in the version the office's holder accepts.** `PanePressed v1`
  `{pane, row, column}` is frozen; `v2::PanePressed` adds `keys_went_here` — ordinary keys
  reached this pane just before the press (`typing_pane`: the resolved context is a pane's,
  so a pane that is only the candidate under an open mode — a layout's name line — is "not
  here"). Workshop sends v2
  exactly when `HostContext::holder_accepts` answers that the office's current holder has that
  door — the host's `holder_accepts_on`, over the bus's role table and accept-sets, native and
  loaded holders alike — and v1 otherwise, so every older pane is unchanged. It is a fact, not
  an instruction: what it means for a row is the pane's, and a v1 press states NO fact, so a
  pane must not read it as "the keys were here" (Files selects on it; Return still opens).
- **Choosing the version is an inspection, not a delivery.** The office is resolved again at
  dispatch; a holder that changed in between and has no v2 door refuses the press
  `NotAccepted`, and that refusal is the press's outcome. Nothing is retried and no v1 follows
  — a second attempt would be a gesture delivered after whatever the weaver did next — and
  Workshop accepts no `zen.DispatchRefused` for it: the attribution is Loom's tap, whose
  refused event names the attempt, `PanePressed` v2, Workshop as sender and `zengine.workshop`
  as author, the addressed office, the holder that refused, and the pointer delivery it was
  authored from (`dispatch_parent`). A reload that ADDS the v2 door changes the image's
  accepted schemas, which Loom refuses to reload in place: a pane that gains it arrives with a
  restarted Workshop.
- **A provider interprets the press against what it is CURRENTLY SHOWING.** `project_loaded`
  returns the row-to-entry map beside the rows it built (the one-measurer rule reaching
  interaction), the provider retains that value and drops it on every room grant, and a press --
  on Loaded's own canvas, read back to its row (`row_cell_at`) -- costs one lookup and no
  observation. **A provider that re-queried its source to interpret a
  press would let a weaver select something they were never shown** — silently, and only
  sometimes.
- **The fact a pane publishes carries DATA and no authority.**
  `LoadedSelected{pane, library, role}` is an occurrence, not a transition, and a listener that
  hears it has acquired nothing: a grant is per `(shape, version, target)` and a value in a
  message is not one. Values may flow; authority must not flow implicitly with them.

## Input authority and carried data

Inventory acquisitions and edits use `workshop/pane_operation.hpp`: a pane echoes the current
input/menu-choice correlation and names the exact owner operation. Workshop verifies that the
gesture belongs to that pane, is current and unspent, and has authenticated Input attribution.
A physical hand is the host's weaver; an injected hand must hold the named operation in its live
Loom authority. An allowed answer approves this intent once and grants no new bus authority.
The consumer still sends through its own ordinary grant. This protocol does not retrofit all
older pane actions with actor authorization.

`workshop/pane_carry.hpp` transports one owned reference envelope from an approved acquisition.
The same actor picks it up and places it; Escape cancels. Workshop interprets no payload fields.
The destination receives a pane-local row, column and aimed picture, plus a fresh gesture
correlation. A destination owns its decoding and must authorize a subsequent read/write.
The inventory receivers are described in [inventory](inventory.md). The carrier is bounded
to <!-- value kMaxCarryBytes KiB -->64<!-- /value --> KiB and is image-local, never a reload-kept pointer or an implicit grant.

`PaneValueCarryRequested` carries a value copy. Its `drag` flag selects primary release or
keyboard pick-and-place; receivers accept `PaneValueDrop`, a distinct door from live references.
Workshop retains the primary gesture's release place while acquisition is pending. A newer
intent defeats that continuation; a click without sufficient movement transfers nothing.
Release outside a receiver cancels a drag, while an unsupported keyboard placement stays held.
The release's receiver and picture are retained; a departed receiver cannot be silently replaced.
Acquiring bytes grants no authority to an operation requested by the receiver. On a canvas pane
the place is the canvas's own (`PaneCanvasValueDrop`, and `PaneCanvasDrop` for a reference,
WL-CANVAS-04): local pixels, room grant and aimed picture, for a provider that accepts it. A canvas pane carries out
as a prose pane does: its primary press's number approves the acquisition, a drag begins with the
press, and the carry ends the press's canvas hold as lost (WL-CANVAS-05).

`PaneObservationRequested` retains that one approval, narrowly, for repeated reads: the same
current-gesture and actor check (`approve_gesture`) spends the gesture and records a lease of
holder, office, pane, role, shape, version, subject and actor, one per pane and sixteen in all.
Each `PaneObservationContinued` is judged again -- office holder, desk presence, subject, and an
injected actor's live authority -- and any lapse refuses and forgets it; only the holder's own
continuation or `PaneObservationEnded` touches a lease, and a request first forgets leases whose
holder no longer holds its office. It grants nothing on the bus, is never created by a timer,
invalidation or old correlation, and lives only in the running Workshop. Ending is the pane's on
every exit: a request stopped before its answer ends the lease that answer grants, and because a
reload keeps the WeaveId, an arriving image sends `PaneObservationEnded{pane, 0}` -- every lease
it holds on that pane -- for each pane it may observe from.

`PanePointRequested` answers one painted cell's input-space point through `visible_text_body`
and the press measurer (`cell_center`), refusing a moved picture exactly as `PaneViewRequested`
refuses a covered pane. It reads presentation; pressing the point is ordinary input. Version 2
of both answers a canvas pane too: its words are its labels and runs as the painter clips them,
through `visible_body` and `visible_words`, each with its place in canvas pixels and the point a
press names it by; a canvas word's point is checked against the body its press lands in.
`DeskViewRequested` answers the desk from Workshop's own numbers (WL-GEO-13). Workshop names
nothing inside a pane: a word's number is its place in one answer. A pane names its parts
(WL-HAND-06): `v4::PaneContent` and `v4::PaneCanvasContent` (and `v5`) carry `parts` -- a run of a row's
columns, a rectangle of the picture -- and `v2::MenuShown` a presenter's lines by the rows' ids,
each judged with the picture it names and refused whole with it (`row_parts_problem`,
`canvas_parts_problem`), listed in the order the pane reads a press, a place it names nothing
unnamed. `PaneView` version 3 says each beside the words with its place and its point, a place of
its own a press reaches -- none for a part with no such place -- and `DeskView` version 2 a
menu's named lines; a name is carried as the pane said it, and a press on a part is ordinary
input at that point.

## A pane may draw locally, with an explicit room and gesture identity

The optional `workshop/pane_canvas_vocabulary.hpp` shapes are a second presentation contract,
not a reinterpretation of prose rows. The complete wire reference, budgets and limits are in
[`../docs/reference/workshop-panes.md#optional-pane-local-canvas`](../docs/reference/workshop-panes.md#optional-pane-local-canvas).
Workshop owns local-to-screen translation, clipping, picture fencing and pointer custody;
[`workshop/canvas.md`](workshop/canvas.md) records those host laws. The provider owns its
picture, hit testing and every semantic action. No node, wire or pan behavior belongs in the
host. The one current-holder callback reads Loom's role table, never a second provider registry.

A provider accepts both room and pointer doors to opt in. Its picture echoes a fresh room's
grant and numbers compositions increasingly within it. A new room clears its prior picture.
Invalid or stale authenticated content is answered as `PaneCanvasRejected`, preserving the
last good picture. A same-provider geometry change may retain a display-only preview marked
as updating; it carries no current picture fence or input authority. Providers keep grants,
picture maps and gestures outside reload state;
they accept input only for a grant they currently hold, and begin a gesture only for a picture
they can interpret. A gesture's later moves and end keep its initiating picture while a drag
repaints newer ones. Lost is an end, never a successful drop.

`PaneCanvasText` uses the medium's reported metric and the shared
`workshop/pane_canvas_text.hpp` helpers for a one-row region and its hit bounds. The host
projects it through existing `SurfaceTextRegion` type, caret and selection, with the ground
beneath. Its complete padded region stays inside the clip; only whole glyphs and whole rows
are omitted. `v2::PaneCanvasText` (in `v5::PaneCanvasContent`) may say `padded` false: its x/y
then name its first character's cell, so runs one line apart stack as prose rows do and a body
holds as many of them as it holds rows; it is the padded run whose glyphs land there, its
padding allowed past the clip above, below and to its left, never to its right, where a caret
after its last character stands, nor to its left when it names a ground (`clip_canvas_run`); and a
`background`, a ground under its characters as a prose row's is. A clip adds no cell for a
caret: a run gives a caret after its last character a blank to stand on. Fixed cell labels keep their existing
meaning. A metric change grants fresh room.

The local canvas does not choose fonts, arbitrary scene nodes, or screen authority. A provider
that accepts `PaneCanvasHover` is told where an idle pointer rests and when it leaves
(WL-CANVAS-06): presentation alone, read from geometry Workshop holds, moving no focus or keys.
It coexists with prose as a fallback for an older host, not as two pictures fighting for the
same body: a canvas-capable room suppresses prose content. Keys, actions and menus use their
existing protocol. A secondary canvas press may continue into the existing menu presenter,
echoing its correlation; no new context-menu owner is introduced.

## The keyboard crosses as two shapes

`PaneKey v1` `{pane, scancode, modifiers}` (`input::scan` / `input::mod`, forwarded) and
`PaneTextInput v1` `{pane, text}` (what the platform committed, forwarded). They ADDED to the
protocol and revised nothing — the older pane shapes are byte-identical, so a provider that
knows only the earlier protocol is unchanged and valid. Which pane has the keys, and how the
screen says so, is Workshop routing law
([`workshop/focus.md`](workshop/focus.md) (WL-FOCUS-01)).

- **Workshop does not ask a provider whether it wants keys, and there is no shape for saying
  so.** A read-only pane that is pressed does take the keyboard, and Loom's gate refuses the
  deliveries — the substrate's own correct answer to being sent a shape a weave never declared,
  visible on the tap. Adding a declaration would be a private per-seam copy of
  `zen.DescribeAccepted`, which is the door that already answers exactly that question.
- **No key release, no focus-changed shape and no IME.** The
  shape's ARRIVAL is the gesture, the press's rule one gesture on.
- **Escape deselects by default (WL-ARR-15).** A bare Escape no row of the pane claims reaches
  it only when its office's holder lists `PaneEscapeUnspent` in what it emits: the declaration
  that it judges its own Escape. It drops what it has selected, or, with nothing selected, says
  `PaneEscapeUnspent{pane}` as the office that offered the pane, **echoing the correlation that
  Escape arrived under**, and Workshop puts the pane down while the pane still has the desk and
  the keys and that Escape is still the last gesture the host handled. `workshop/pane_escape.hpp`
  is that default (`pane_escape::answer`). What counts as a selection is the pane's: a chosen row
  it can let go (Loaded, Powers), never a marker that always rests on a row (Files, Inventory and
  its views, the Desktop's lists, a field cursor). A pane whose Escape means more spends it (the
  Composer leaves a form; the Terminal sheds its list, then its line) and the editors keep every
  Escape. A holder that does not declare the word is never sent a bare Escape: it could not hand
  one back, so Escape's last meaning answers at once (`holder_emits`). There is no answer and no
  retry; a later key, text, press or wheel makes the word stale. **The correlation is what names the Escape**: Workshop
  mints one per bare Escape and carries it on whichever message delivers it, because a second
  Escape restores every other check the first one met, and an answer about the first would
  otherwise be spent on the second. Zero, an older Escape's number and an already-spent one all
  move nothing.
  **And a bare Escape is not sent at all** when the office's holder accepts no `PaneKey` and the
  pane declared no row for it (`holder_accepts`, the same reading the press's version comes from):
  nothing could have spent it, so Escape's last meaning answers at once (WL-ARR-16). That is a
  declaration read off the bus, never an inference from silence. The weaver leaves a pane that keeps
  Escape by pressing a pane that takes no text, then Escape.

## The wheel crosses as one shape

`PaneWheel v1` `{pane, dx, dy}` — `input::PointerWheel`'s notches, forwarded unchanged, +1.0
per notch away from the weaver. It ADDED to the protocol and revised nothing, and every older
shape is byte-identical.

- **It follows the POINTER, not the keyboard.** Which pane receives it is `occupied_at`'s
  topmost answer — the same walk a press spends, the effective order with the selection lift
  — so a pane a weaver never pressed into is scrolled by pointing at it, and a pane in front
  keeps the gesture for its own cells (`external_wheel`, declared in weave.hpp, its body in
  weave_external.cpp). It is sent only while the
  pointer names a prose row of the granted body (`external_press_at`: the header and the
  remainder under the last row send nothing) and only to a pane holding a room. A pane holding
  a canvas room is sent the same notches as its canvas pointer's `kWheel` instead, at the
  pointer's local place (`canvas_wheel`), as every pane drawing its rows on its canvas is.
- **No place, no rows-per-notch, no accumulator on Workshop's side.** A wheel means "advance
  through what you are showing"; a row on it would be Workshop prescribing one list under the
  pointer. How many rows a notch is worth is the provider's grammar — the shipped Powers and
  Composer spend one row per notch (their compact rooms may show only a few rows; a notch that skipped
  a row the pane never showed would be worse than a slow wheel) and carry fractions until they
  are worth one. Workshop's own lists spent three (`kListWheelRows`) until the last of them —
  the picker and the host's Pane Manager — retired; every list is a pane's now.
- **Workshop asks nothing back**, `PaneKey`'s rule. A pane that does not accept the shape has
  the delivery refused at Loom's gate; a pane that accepts it and has nothing to move is
  unchanged. Grant: `to_any`, for `PanePressed`'s reason (workshop.cpp).
- **Loaded scrolls its viewport without changing selection.** Its origin is separate from
  the selected library identity. A whole wheel step obtains a new owner snapshot; it never
  publishes LoadedSelected. Arrangement remains static and ignores the wheel.

## The sweep, the reveal and the quit cross as five more shapes

The Editor's extraction added five shapes and no powers, all in `workshop/pane_vocabulary.hpp`
beside the shapes before them, and each is an ordinary optional capability any pane may spend:

- **`PaneDragged v1` `{pane, row, column}`**, Workshop → provider: the hand moved with the button
  down, in the pane a press named a row of. The position is the granted lattice's, resolved
  against the pane's body AT THIS MOTION through the same measurer the press spent, and
  deliberately NOT clamped — a row above the body is negative, one below it is past the granted
  count, and what either means is the pane's. There is no release shape: the host ends its own
  record on release, or when the pane loses its seat or its room, and sends nothing; a pane
  resolves a sweep from the positions it was given. The Terminal ignores it; the Editor steps its
  window on it.
  **Workshop arms its record from geometry alone** — a press that named ANY body row takes hold
  of the pane, because the host does not read a provider's rows to learn what they mean. So a
  drag EXTENDS a gesture the pane's own `PanePressed` began, and a pane that consumed the press
  as focus alone ignores the motions behind it. That is the pane's half of one gesture, and the
  Editor pins it.
- **The reveal is one ask and one answer**: **`PaneRevealRequested v1` `{pane}`**
  (provider → Workshop, as the office that offered the pane: seat me now, my act needs nothing
  more) and **`PaneRevealAnswered v1` `{pane, seated, refusal}`** (Workshop's answer, on the
  delivery that asked, about what that delivery DID — the pane is seated, selected and has the
  keys, or nothing moved and here is the launch door's sentence). Judged first, through the
  launch door's own trial seat on a copy of the setup; written only if the seat is real. A
  refusal moved nothing; a screen that shrank before the ask arrived refuses it in the launch
  door's words; a
  shrink after the answer is an ordinary presentation change to a pane on the desk. Workshop
  holds nothing between deliveries — no record, no reservation — so two offices' asks are two
  seats judged in order. A reveal naming a pane the office never offered is refused; one from
  nobody is dropped unanswered.
  **It is an ordinary pane's door, and no longer the Editor's open.** A pane whose act needs a
  seat and nothing else asks for one. An act that changes a DOCUMENT and its presentation
  together — opening a source — is the managed opening below, because the two facts live in
  two weaves and FIFO dispatch lets either change between two deliveries: the reveal-as-
  commitment shape held the asker's gestures to a bound and dropped the rest, and no bound
  repairs that.
- **`PaneQuitRequested v1` `{}`**, Workshop → everyone, a PUBLICATION as the office, and
  **`PaneQuitAnswered v1` `{pane, permitted, refusal}`**, its answer: may this Workshop end? A pane
  that accepts the ask MUST answer it, about the instant it answers; the host counts Loom's
  accepters at the publication, ends the process at once when nobody accepted, and otherwise holds
  every gesture until the last answer, refuses on any refusal (saying it, and replaying the held
  gestures), and proceeds on all permissions. A pane with an answer still owed to it — a paste in
  flight — refuses rather than waits, so one clipboard read cannot hold every other pane's exit.
  **A delivery Loom refuses ends the quit as a refusal** (WL-SESSION-19): the participant is held,
  dead, gone or no longer takes the question, so no answer can come for it. Loom's
  `zen.DispatchRefused` never reaches a PUBLICATION's author — `fanout` captures no refusal
  recipient — so the host reads the refusal off its own tap (`QuitDeliveryWatch`,
  `workshop/quit_delivery.hpp`): a `Refused` event under Workshop's sender stamp for this shape is
  written into a book Workshop reads, and Workshop is woken by a delivery that carries nothing.
  Workshop refuses the quit in flight on an entry for its own ask, naming the office, the weave and
  Loom's reason, even with another answer still owed; the next quit asks afresh. A pane author
  registers nothing, and a participant that shows no pane is counted like any other. What is NOT
  solved: an accepter that IS delivered the question and never answers holds the quit open, and
  that is named rather than timed out.

## The second button crosses as one shape, a menu is presented by a participant, and a press names its picture

`PaneButton v1` `{pane, button, pressed, row, column, lost, picture}`, Workshop → provider;
`PanePassRequested v1` `{pane}`, `PaneMenuRequested v1` `{pane, subject, row, column, rows}`,
`PaneKeyboardRequested v1` `{pane}` and `PaneManageRequested v1` `{pane, office, target}`,
provider → Workshop as the office that offered the pane; `PaneMenuAnswered v1`
`{pane, subject, chosen, id, refusal}`, the presenter (or, for an ask it refused or one no
presenter can answer, Workshop) → provider; `v3::PaneContent` `{pane, rows, generation,
picture}`, `v4::PaneContent` `{…, parts}` and `v3::PanePressed` `{…, picture}`. Between Workshop
and the participant holding `zengine.presenter`, `workshop/presenter_vocabulary.hpp`:
`MenuGranted`, `MenuInput` and `MenuWithdrawn` Workshop → presenter, `MenuShown`, `MenuClosed`
and `PresenterReady` presenter → Workshop, and `HeldMenu`, the reload state the shipped
presenters share. The pane shapes ADDED
to the protocol and revised nothing. `MenuGranted v2`, `MenuClosed v2` and `HeldMenu v2` carry
the host's standard rows (`standard`, below) and the name of the pane they act on
(`pane_name`); a holder that accepts only `MenuGranted v1` does
not hold the office for this host, so a menu is refused where it would open, in words. Several
NEST — `PaneMenuRequested` and `MenuGranted` carry
`vector<PaneMenuRow>`, `v3::PaneContent` and `MenuShown` surface rows, `v4::PaneContent` and
`v2::MenuShown` `PaneRowPart`s too — and since Loom ABI v9 a
nested component is agreed at admission like any other declared shape; these admit because
every party declares them from the one installed header, not because they are flat. The host's
side is law in `workshop/press-chain.md` (WL-PRESS-06), `workshop/contextual.md` (WL-CTX-08),
`workshop/pane-menu.md` (WL-CTX-09) and `workshop/desktop-presenting.md` (WL-DESK-14); the
presenter's is WL-CTX-10, in `workshop/pane-menu.md` beside it; the helpers a pane may use are
`workshop/pane_menu.hpp`, installed beside the protocol.

- **The pane is first, and delivery is the disposition.** A secondary press over a pane's body
  is sent to a holder whose accept set has the door (`holder_accepts`, the same reading the
  press's version comes from) and is consumed by delivery: no menu, no selection, no keys. A
  pane may act (the guard example blocks while the button is held), hand the press back
  (`PanePassRequested`, echoing its correlation: the host's own pane menu opens once), or ask
  for a menu of its own rows. The shipped panes hand back every right press they have nothing
  for, so none is lost; a holder's silence is still its own disposition, which the host cannot
  see. **A right press in a body without the door is not lost:** the holder is sent nothing,
  no keys move, and the host's own pane menu opens at the press — the same menu the chrome (the
  title row) and the Pane Manager reach. A send Loom refuses is attributed on the tap, and the
  host drops the custody it recorded so the physical release sends nothing — the failure stands,
  never manufactured into completion (`end_refused_button`, the review's fourth finding); a
  press whose send queued nothing at all is known undelivered, and the host's menu opens.
- **Three gestures may continue into a menu, and they are judged alike.** A secondary press
  (`PaneButton`), a declared action sent by key (`PaneActionRequested`) and a PRIMARY press
  (`PanePressed` and its versions) each go out under a correlation a pane may echo on a
  `PaneMenuRequested`; eligibility is the same three facts where the menu opens -- this pane,
  this number, and still the weaver's latest act -- and each is spent once
  (`secondary_cont_`, `action_sent_`, `press_sent_`). The primary press is what lets a pane
  that draws its own controls answer a click on a `[menu]` of its own; like the other two it
  moves no keys and no selection, and a late or replayed request is refused in words.
- **Two records per button, and closing invalidates on its own.** A hold is release custody
  and ends only on the release, owner loss or arbitration (a press of a button believed down:
  the old hold ends with a `lost` release before the new is recorded). A continuation is
  eligibility to be handed back or to open a menu: newest press of its button, unspent, its pane
  on the desk, no newer act since. A release is no act of its own — it completes the one its
  press began (`on(PointerButton)` counts presses, keys, text and the wheel, never a release) —
  so a click's own release never makes its choice late. A release never restores it; a pane that
  leaves the desk after the release cannot be handed back or given a menu — the review's first
  finding, repaired at `end_lost_holds`. The release goes to the ROLE, so a holder replaced
  mid-hold is not promised it; the shipped helper (`HeldButton`) ignores a release of a button
  the image never held, and a holder that gives up its office ends its own hold.
- **A menu is requested, judged where it opens, granted to the presenter, answered once.**
  Every declared action goes to a pane under a number of its own (`action_sent_`), so a request
  opened by key continues that keystroke and one opened by the second button continues that
  press; the host judges eligibility in its own handler, and a queued primary press elsewhere is
  a newer act, so the late request is refused and the keys stay where the newer press put them.
  An eligible ask is GRANTED to whoever holds `zengine.presenter` (`grant_menu`): the host keeps
  custody and place — `PresentedMenu`: whose menu, what about, where, the lines last shown, which
  acts it may name — and none of the pane's rows. Beneath them it grants its own pane menu for
  that pane, the STANDARD ROWS (`MenuGranted::standard`: the catalog's pane rows, ids and labels
  as its own menu shows them), so one menu holds both; a standard row chosen comes back named on
  `MenuClosed::standard`, the presenter answers the requester unchosen, and the host spends the
  row on that pane (`spend_standard_row`: an action through its own seam, a group by opening its
  own menu there) as the choosing act's continuation, while that act is the weaver's latest.
  No pane performs a standard row and the host performs no pane's row. While it is open the
  weaver's keys are named by the contextual
  rows and forwarded (`menu_key`, `MenuInput::verb`), presses inside or outside are forwarded
  with the line and the picture the medium held (`menu_button`), the character a forwarded key
  made is part of that key's act, and the popup draws the lines the presenter shows within the
  room the grant named (`paint_presented`; lines that overflow it lose the menu, in words). The
  host withdraws the menu when custody moves — a newer menu, a right press, its own menu, the
  pane leaving the desk or being offered again — and the presenter answers it unchosen in the
  host's words. The PRESENTER owns the rest (WL-CTX-10): what can be presented (the row bounds),
  how it reads, what each key and press means, when it ends, and the answer, as its office,
  under the request's number. It closes the menu to the host (`MenuClosed`) naming the act that
  chose -- the word that its requester is answered and nothing more is owed -- and the host
  records that act as the choice's continuation: a choice may continue,
  once, into the host's own pane menu (`PaneManageRequested`) or the keyboard
  (`PaneKeyboardRequested`), honored while that act is still the weaver's latest — a newer key or
  press defeats it, the choosing click's release does not. The reveal door is not the menu's route.
- **The requester reads an answer through its own record of the ask.** `pane_menu::Asked` is
  what `Offer::send` returns and what `take` settles: a choice counts only from the presenter's
  office (a refusal may also be Workshop's), under this image's pending number, once, about the
  pane and subject asked; the subject is then the pane's to judge against what it holds. Kept in
  the image and never in reload-kept state, so a reloaded requester CANCELS its predecessor's
  menus — the shipped desktop's policy; the host also withdraws the menu when the successor
  offers its pane again. Presenter lifetime: an image keeping `HeldMenu` reloaded in the
  presenter's place says `PresenterReady` naming the open menu and shows it again (a HANDOFF);
  a presenter that leaves or a holder that does not carry the menu ends it, and the host answers
  the requester unchosen itself. An act or a withdrawal that overtakes an arrival -- reaching an
  image for a menu it does not hold -- is GIVEN BACK (`MenuReturned`, with the image's words)
  rather than dropped, and the host settles that requester: the open menu ends answered, a
  withdrawn one's kept record is answered and forgotten, and a menu already over takes nothing.
  With no presenter a menu is refused where it would open. These are these consumers' policies,
  not a rule every participant must follow.
- **The host learns a presenter left from Loom, by the exact attempt, and answers THAT menu.**
  Every sentence the host queues to the presenter's office from a grant until that menu ends is
  about that menu, and Loom numbers attempts in queue order, so a `zen.DispatchRefused` names
  its menu (`end_refused_menu`): an attempt at or after the open menu's grant
  (`PresentedMenu::first_attempt`) ends the open menu, answered; one inside a withdrawn menu's
  span settles that one; anything older settles nothing -- an older menu's refusal never ends,
  alters or answers a newer one. The office's name alone could not tell them apart. **Taking a
  menu off the screen is not answering it:** the withdrawal is still the presenter's to answer,
  so the host keeps who asked (`WithdrawnMenu`: office, pane, subject, number, why, the attempt
  span) until Loom has had its say. A withdrawal that queues nothing, or one Loom refuses (the
  presenter unloaded after the menu opened), is answered by the host, unchosen, under the ask's
  number, saying why the menu ended and what Loom said. The record is forgotten when the host's
  own `WithdrawalFence` has come round twice behind the withdrawal -- each refusal is appended
  when its sentence is dispatched, ahead of the second hop -- or sooner, on the presenter's own
  `MenuClosed` or `MenuReturned` about that menu. Those two hops are also what makes the
  give-back reach a record still kept: the withdrawal is dispatched before the first hop, and
  what the image says back is queued while that hop is handled, ahead of the second. An image
  that answered nothing THAT TURN is outside what this orders. A withdrawal Loom delivered and
  the presenter neither answered nor gave back stays that presenter's silence: no timeout, retry
  or departure notice is implied. Establishing an older menu's refusal while a
  newer menu is open took a request made under the number its press will carry, before the pane
  heard the press, and a presenter killed and revived (Loom's crash-revival door) around the
  older withdrawal: a requester that asks when its press arrives asks behind every such refusal.
- **A press names its picture, or is refused.** `v3::PaneContent.picture` is the pane's own
  number for its row-to-meaning map (a `RowMap` moves it exactly when the map moves);
  the host records it at admission (`ExternalPane::picture`) but stamps a press with the picture
  the MEDIUM held when the press was read (`ExternalPane::stamp`), echoed on
  `v3::PanePressed` and `PaneButton`, and on `PaneCanvasPointer` to a pane drawing its own
  picture. The consumer acts only on its current map's number, else says
  `the list moved -- press again`; the desktop, whose rows are its canvas picture, numbers every
  picture afresh and acts only on a press whose picture was drawn under its map's current number
  (`CanvasPictures`). `v2::PaneContent.generation` keeps its meaning. Three
  pictures are distinguished: ADMITTED (the host accepted the content), HANDED OUT (a canvas
  showing it was published) and HANDLED (the medium's delivery of that canvas has run) -- the
  last this host can know; what a display showed, and when, it never sees. The host sends its
  own `PictureFence` behind the canvas that first hands a picture out and lets it come round
  twice: delivery is single-threaded FIFO and input is read by a delivery (the input weave's
  beat), so the first hop is handled right after the medium handled the canvas and the second is
  queued behind every press read before that. A raw press queued ahead of a newer picture's
  admission, or read after its admission but before the medium was handed it, is therefore
  stamped with the older picture and refused as moved -- the queued-before-admission case
  is closed through the real input and content owners, with two numbers per pane
  and no frame history. What remains, precisely: the medium's own latency AFTER it handled a
  canvas (a terminal's or compositor's paint, a vsync), and a press the platform buffered before
  the beat read it. The fence orders two things on the bus -- the canvas's handoff and the input
  queued behind it -- and is no evidence of physical display time, nor of where input the
  platform buffered came from. Closing those needs the medium to say what it showed, or input to
  carry its own time against the frame's -- a medium acknowledgement, not this host's. A
  re-offered pane starts over (`forget_pictures`): a reloaded image numbers from one again. The
  subject side is closed as well: a slot's meaning carries its subject (`LauncherMeaning::ref`,
  `KeysMeaning::ref`), so a same-length swap moves the map's number and a stale press is refused,
  not resolved against the row that moved in.
- **Not in this contract:** a secondary drag (`PaneDragged` carries no button, so the Editor's
  middle-button scroll and Neovim's right drag do not cross), modifier state on a window's
  button events (`mod::kNone` always), a host-performed pane operation from a menu row, the
  host's OWN menus (chrome, room, tab, pass-back, a doorless body, a `manage` row) presented by
  the presenter — they stay the host's, the management route that must work with no presenter at
  all; the presenter shows the standard rows only beneath a pane's own — and a
  pane's actions offered on HOVER. Hover reaches a canvas only as presentation, and pane actions
  follow keyboard focus; an item a pointer rests on, an offer that lives while it does, and a key that
  reaches an unfocused hovered pane are later, explicit policy (the grant, the withdrawal and
  the picture fence above are where such an offer would connect).

## Opening a source is a managed opening, jointly published (WL-OPEN)

The Editor's document and Workshop's presentation of its pane change TOGETHER, at one
published boundary, coordinated by the native opening manager the host mounts in
`zengine.opening`. The law is [`workshop/opening.md`](workshop/opening.md) (`WL-OPEN`); the
Editor's half is [`workshop/editor.md`](workshop/editor.md) (WL-EDIT-05, WL-EDIT-13); the
substrate — joint publication of latest claims, the showing, the record's lifetime — is Loom's
joint-publication reference page, never restated here. What crosses, all in
`workshop/open_seam_vocabulary.hpp` unless named otherwise:

```text
OpenSourceRequested v1 {path}   requester -> zengine.opening   (pane_seam_vocabulary.hpp; also
                                                                 zengine.editor, which RELAYS)
SourceOpened v1 {accepted, refusal}   the answer: published AND applied by both owners, or why not
PresentationTrialRequested v1 / PresentationTrial v1      manager -> desk: would it seat, what room
PrepareSourceRequested v1 / SourcePrepared v1             manager -> Editor: prepare B for that room
PresentationAdmitRequested v1 / PresentationAdmitted v1   manager -> desk: admit B's rows, offer
ManagedOpenProgress v1                                    manager -> desk (and both owners at
                                                           `apply`): what is awaited, or retracted
ManagedOpenSettled v1                                     manager -> both owners, afterwards
EditorDocument v1, PanePresentation v1                    the two latest claims, published jointly
v2::PaneContent v2, v2::PaneCaret v2                      (pane_vocabulary.hpp) rows and caret
                                                           naming the document's generation
```

- **The two claims are identities, never documents.** `EditorDocument` carries the path, the
  epoch, the convention, the content revision and the dirty flag; `PanePresentation` the seat,
  selection, keys, room, admitted generation, the count of inputs routed to the pane, the stack
  capacity and the setup digest. Each owner derives its claim from its own state at the end of
  every delivery and claims only when it moved (`after_delivery`), so an edit to A, a routed
  input, a resize or an authored change moves a claim and aborts a preparation bound to the
  previous revision. Nothing is held.
- **`v2::PaneContent` and `v2::PaneCaret` ADD a `generation` field beside the untouched v1
  doors** (GATE-04's reason: a published `(name, version)` is frozen). Workshop admits a v2
  projection unless it names a generation older than the one the pane holds — a picture of a
  document that has since been replaced, dropped rather than painted over the admitted rows. A
  v1 content carries none and is admitted as it always was; every pane that never commits
  jointly is unchanged and unrebuilt, and the separately built legacy provider still speaks v1.
- **Every managed sentence is judged under the office stamp.** The trial, the admission and
  the settlement are taken only from `zengine.opening`; a preparation only from the manager;
  a forged settlement, preparation or admission reaches its party and is dropped by it. The
  manager's own answers are matched by Loom's `answers_ask()` plus its correlation and stage.
- **The requesters keep their tickets.** Files' open, the Builder's recipe-source lookup and
  its open, and the Editor's relay each keep the send ticket and clear only the ask whose
  exact attempt Loom's `zen.DispatchRefused` names; an enqueue that queued nothing is refused
  at once, in words (WL-OPEN-07).
- **Not in this contract:** a document-only open, a queue of intents, a timeout, a retry, a
  rollback after publication, or a loaded manager. A host that replaces its manager mints
  its authority again (WL-OPEN-08).

## A pane declares its actions, and the host dispatches the resolved id

`PaneActions v1` `{pane, rows}` of `PaneActionRow v1` `{id, label, scancode, modifiers}`,
provider → Workshop, sent beside the offer; `PaneActionRequested v1` `{pane, id}`, Workshop →
provider. They ADDED to the protocol and revised nothing, and every older shape is
byte-identical. The host's side — the join, the collision law, the legend, the dispatch — is
Workshop's law, [`workshop/keyboard.md`](workshop/keyboard.md) (WL-KEY-15).

**And there is a second published version, beside v1 and not instead of it**:
`v2::PaneActions v2` of `v2::PaneActionRow v2`, which is v1's four fields plus `supersedes`.
The field was first added to v1 in place, and that was wrong for a reason the substrate states:
a published `(name, version)` is frozen and identity across a `.so` seam is the content-id
derived from the shape, so adding a field changed the identity of `PaneActionRow` v1 AND of the
`PaneActions` v1 that encloses it — a provider built against the old header and a host built
against the new one could no longer both register
([Loom GATE-04](https://github.com/Krealsion/Loom/blob/main/docs/laws/admission-laws.md)).
Measured, with an ordinary Registry, as `SchemaConflict`. Workshop accepts both doors and joins
them into one admitted row set; a v1 declaration means what it always meant — this pane owns no
host action — and nothing reinterprets old bytes.

- **A v2 row may say it STANDS IN FOR one of Workshop's own actions** (`supersedes`),
  and only for the ones Workshop declares ownable — today `document.save`. While that pane OWNS
  input the host's row is not requestable and no legend spells it, and the pane's rows may take
  its gesture without colliding: the two are one meaning in two scopes, and the exemption is the
  pane's, not one row's. Supersession is by ID, so a weaver who rebinds either row moves
  neither row's meaning. It is how a pane that holds a document of its own makes `^s` mean ITS
  save without the host naming that pane anywhere. **Owning input is not being remembered:** the
  pane's handle counts only while the resolved context is that pane's, so a contextual menu
  opened over a pane is the menu's, and the host's row is the weaver's key there.
- **A row is the host's own catalog row minus `Act` and minus `KeyContext`.** The id is in the
  pane's namespace and is what a weaver's keymap file names, so it is durable the way a pane key
  is; the label is what the band prints; the gesture is the SAME two numbers `PaneKey` carries,
  `input::scan` and `input::mod`, and never a key name — a name is a spelling the host's grammar
  owns. `scan::kUnknown` declares a row with no default, one a weaver may bind.
- **Judged whole under the office stamp, exactly as the offer is.** An empty office retains
  nothing; a pane this office never offered is refused by name; the rows meet a bound
  (`kMaxPaneActionRows`), an id law (present, printable, no space, unique, never one of
  Workshop's own) and the collision law over the effective map — the globals, which are what is
  active while a text-taking pane holds the keys, and the pane's own rows against each other. When adding a chord, check the desktop application rows too and exercise both declaration orders with the desktop loaded; a pane-only fixture cannot witness that join. A refused shape leaves the pane's previous rows standing; an accepted
  one replaces them. Two panes declaring one bare key are two contexts, keyed by handle, and
  never meet.
- **The weaver's file reaches the pane.** An override for an id nobody has declared is preserved
  as unknown when the file loads and applied the moment a pane declares it; a pane that declared
  before the file loaded is re-joined under the file at the load. The file always wins: in
  either order a pane whose rows the file's bindings collide with is the party refused, in the
  file's own words.
- **The resolved id crosses INSTEAD of the key, for one keystroke.** While the keyboard pane
  holds the keys, a gesture matching one of its effective bindings crosses as
  `PaneActionRequested` and the character it produced is swallowed; anything else crosses as
  `PaneKey`/`PaneTextInput` unchanged, so a `p` typed into a field is still a `p`. A provider
  acts on the name and never re-derives a binding it cannot see; a provider that keeps matching
  raw scancodes has made its declaration decorative, and its weaver's override reaches nothing.
- **Declaring is not wanting.** Rows point no keyboard at a pane and hold none: a press into its
  room is still the only way it gets the keys, and a pane that declared nothing is unchanged.
  The Powers pane declares four (`introspection/vocabulary.hpp`) and the Composer five
  (`composer/composer.cpp`); `loaded` and `arrangement` declare none.
- **A PANE'S NOTICE STANDS UNTIL THE WEAVER'S NEXT ACT, never until it has been said once.**
  A built-in wrote its sentence on the band; a pane has only its own room, so the sentence is a
  row it publishes — and ONE gesture produces SEVERAL publications in one drain (write the
  notice, say the rows, ask a door whose answer arrives on the same turn and says them again),
  of which Workshop keeps the LAST. So a notice cleared inside `say` is a notice no weaver ever
  reads. Clear it where the pane ACTS on a gesture, and subtract its row from the composition's
  budget, or the room's last row is cut after the fact. Measured twice before it was written
  down: `u` on a catalog produced no visible row (the project browser's whole-loop witness),
  and the Builder's seam suite caught the same class one pane over. `files/files.cpp` and
  `builder-pane/pane.cpp` both spell it.
  **AND SPENT MEANS GONE FROM THE PUBLISHED ROWS.** A private clear is complete only when the
  picture Workshop holds says it: an act whose answer is still on its way — the Builder's `e`
  lookup, Files' open, Info's select — says nothing of its own, and neither does an accepted act
  that changes no picture (a select of what is already selected), so a spent refusal stood
  painted beside what the act did (measured in the Builder, Files and Info). The handler that
  clears a standing notice therefore says the rows itself when the act it ran published none
  (`published_`). Other panes that clear a notice where the weaver acts carry the same obligation.
  **What is not an act spends nothing**, and is decided before the notice is touched: a key a
  line does not take, and an id the pane does not declare in the mode it is in — one that raced
  a re-declaration (Escape and Return in one poll: a cancel, then a commit to a closed draft) or
  one nobody declared. Cleared in private, the notice stood painted until an unrelated room grant
  said the rows (measured in Info, the Terminal, Attention, the Builder and Files). Each pane
  asks `answers`, which reads the rows its `declare` sends, so what it acts on and what it
  declared are one list per mode. That refuses a raced id only while **an id names one operation
  in every mode that declares it**. An operation only one mode has gets an id only that mode
  declares — Files' Return is `files.open`, `files.choose` or `files.commit-field`
  ([`workshop/files.md`](workshop/files.md) WL-FILES-16) — or the next mode would take the raced
  id as its own. **A notice's row moves every row beneath it**, so a pane reads
  a press against the rows it published with the notice in them.
  **AND AN ANSWER'S SENTENCE BELONGS TO THE ACT THAT ASKED.** Ending a draft ends the draft and
  not a write it already sent, so the sentence said then promises neither outcome, and no end
  says nothing was written: an empty record of outstanding requests is no proof that none was
  taken. A sentence about a pending request is that request's, and its answer retires or
  replaces that sentence, never a later act's. A draft incarnation is not its contents: an
  answer closes, alters and marks only the draft that sent it, and closes it only while it holds
  exactly what was sent, because a write does not cover typing done after it left. Info sends one
  commit at a time and declines another aloud ([`workshop/info-body.md`](workshop/info-body.md)
  WL-INFO-12). **A request Loom attests never arrived is not outstanding**: a send whose ticket is
  not valid, or one `zen.DispatchRefused` names by its exact attempt, is released and said as
  undelivered, and delivered silence still waits (WL-INFO-13). Info's commit also names the
  subject its draft was typed for, and the host writes it only while that subject holds
  ([`workshop/info-body.md`](workshop/info-body.md) WL-INFO-15).
- **Not in this contract:** the contextual surface, which declares over `kActionCatalog` ids at
  compile time and would need a runtime join on the pane subject; a `posix_gap` note for a pane
  row's authored gesture (said for the file's rows at load, not yet for a pane's at admission).

## The Loaded pane: the first stranger tool

`introspection/` builds `zengine-introspection`, an ordinary loadable weave, and it is the
first thing in this repository whose pane arrives entirely through the external protocol.
**Workshop compiled nothing for it**: no presentation source under `workshop/` names it —
`weave.hpp`, `screen.hpp`, `panes.hpp` and the subject `.cpp` files beside them, walked by
`presentation_sources` (tests/workshop_support.hpp) rather than listed — no `pane_kind::k*` was
minted, and the inventory learned its row from a live offer.

```text
PaneRef      zengine.introspection / loaded         the durable pair a saved setup names
office       zengine.introspection                  the only address anything reaches it by
stem         zengine-introspection                  a line in the HOST'S boot list
```

- **The fact it shows is the KERNEL's, and there is no second copy of it.** `zen.ListLoaded`
  to the Weave Manager, relayed to `zen.ListLibraries` at the control door, answered from
  `Kernel::loaded()` — a live map, never a cache. The provider holds no `known_weaves_`, keeps
  no diff, derives no arrival or departure, and stores no timestamp. `introspection/loaded.hpp`
  is the pure half (parse the answer, spend the budget) and links nothing, so what a reading
  MEANS is provable over a value.
- **The order is the kernel's.** `Kernel::loaded()` walks a `std::map` keyed by library name,
  so the answer is name-ordered, stable across runs and independent of boot order. Nothing
  sorts it here; a view that reordered a list its owner already ordered would then window it by
  a rule the owner never applied.
- **The wire form has no escaping**, and it is the one thing to know before reading it: the
  door joins on `,` and `@` and emits no delimiter of its own, so a library name containing
  either is unrecoverable in principle. `parse_loaded` splits on the LAST `@` — the ambiguity's
  better half, not a solution — and says so where the reading happens.
- **`zen.ListLoaded` is not the load capability.** A Loom grant is per
  `(shape, version, target)`, so asking what is loaded is exactly that one question;
  `LoadWeave`/`SwapWeave`/`ReloadWeave`/`UnloadLibrary`/`UnloadRole` are absent from this
  weave's Emit set and from every send it makes. The suite pins that **from a bus tap**, not
  from the declaration, because `Emit<...>` declares vocabulary and gates no send in this Loom
  (an undeclared shape is still sendable under a grant). This pane retains Workshop's
  permissive artifact policy; `workshop/admission.hpp` separately bounds the inventory office.
  A Kernel mints no grant of its own, and one with no policy admits nothing. A declaration
  proves nothing on its own and is not quoted as though it did.
- **Room grants and wheel gestures refresh the snapshot.** Loom gives a participant no arrival or departure
  event, so there is nothing to subscribe to and nothing here polls or times out. It re-reads
  when the pane opens, when a valid re-offer refreshes it, at every room granted it again -- on
  its canvas whenever its body moves or changes size, as prose when the resolved prose capacity
  moves -- and when the weaver scrolls. A separate list origin moves the viewport without
  changing selection or publishing LoadedSelected. The last row of the pane says `snapshot`, because between readings that is
  what it is.
- **A COUNT WITH AN UNSTATED POPULATION IS THE DEFECT THIS VIEW IS SHAPED AROUND.**
  `ListLoaded` enumerates kernel-loaded libraries, so every in-process weave — Workshop itself,
  the Builder, the runner, the terminal participant, the Manager, the control door — is
  outside it and cannot be spoken about. So `in-process weaves are not in the kernel's map` is
  reserved out of the row budget BEFORE the list is offered anything but its first row (the
  footer-reservation argument, in a second place). Do not make that line conditional on spare
  room.
- **An entry and its omission marker are ONE demand on the budget.** Reserving a single row for
  "the list" buys a row the marker then takes, so a four-row body spends two rows on notes and
  names no weave at all — the suite caught exactly that. Showing PART of a list obliges saying
  how much was hidden.
- **A list MARKS a name it cut.** The retired picker taught it: `picker_entry_text` ran the
  name through `detail::fit` before `detail::pad` — fit for the truth, pad for the alignment —
  because `kPickerNameCols` was 10 and admission allows 32, so the cut fired the moment a name
  belonged to a party this build never compiled. The desktop's Pane Manager keeps the rule in
  its own image: the name is written last on its row, and a cut is marked.

## The Composer is a schema-directed message form

`composer/` builds `zengine-composer`, holding `zengine.composer` and offering pane `compose`.
The public weaver route is [Inventory to Compose](../docs/workshop/inventory-compose.md).

- LoadedSelected is accepted only from the introspection office. The target supplies its
  accepted roots and referenced closure through authenticated DescribeAccepted answers.
  The whole snapshot is replaced per selection; dependencies are not sendable roots.
- This is a raw Loom Weave because AcceptedShapes is a runtime schema. It does not advertise
  the construction layer's automatic Poke/Describe doors. Its state retains counters only;
  live forms, clipboard asks and permission requests do not survive image replacement.
- Scalar text uses the shared message-draft helper and Loom lexer/composer. Typed fields use
  that helper's transactional gate and deep copy. Presence is distinct from empty/false.
- RenderedRow pairs prose and row meaning. The rows are the pane's own canvas picture
  (`rows_picture`), numbered by what they mean (`meaning_of`): a press or a drop reads its place
  back to a row of the picture it was aimed at and acts only while that picture's meaning stands,
  so an incompatible or stale drop leaves the draft intact. Existing included fields require
  explicit exclusion before replacement. Whole forms require an empty draft and one of the
  selected target's accepted root schemas. To a host granting no canvas it says its rows and
  caret as prose and takes no press or drop.
- The edited value's caret is the medium's (`ComposerView::caret_row`): its row keeps a blank
  after the value for a caret at its end, so no character moves as the caret does.
- The value and reference transfer doors remain distinct (`PaneCanvasValueDrop`,
  `PaneCanvasDrop`). A reference is copied as data into a compatible field; its contents are not
  automatically read and it conveys no authority.
- Ctrl+S stores a complete admitted command; Ctrl+B stores an original-schema partial preset
  through InventoryAdd and its actor authorization. Ctrl+U discards a field; excluded local
  values never enter a preset. StoredDraft opens against the entire accepted closure and
  FieldValue checks the destination's declared type, including empty containers.
  A drop never submits. Submit separately authorizes the exact destination and versioned
  shape through PaneOperationRequested. Permission and storage requests freeze the form.
- An output ticket proves queuing only. Authenticated dispatch refusals and Refused answers
  are reported; arbitrary application success is not inferred. Pending questions may remain
  pending. New requests do not acquire authority from stored data or metadata.
- Loaded-to-Compose selection remains a local policy, not a general binding engine.
- Pure forms are exercised in tests/test_composer.cpp; loaded interaction and actor authority
  are exercised by tests/test_workshop_inventory_info.cpp and the existing pane suites.

## The system can show what it is

`zengine.introspection` offers Workshop THREE panes:

```text
loaded        the Kernel's loaded() map                which WEAVES are loaded
arrangement   the realization owner: its authored     which AUTHORED PARTICIPATIONS
              plan and its resolved rows               resolved, and where each one
              (pane name `Project`)                    has got to
powers        the host's op::Catalog                   which POWERS resolve, and whose
                                                       contribution satisfies each
```

- **THE PANES DISAGREE ON PURPOSE AND THAT IS THE HONESTY CLAIM.** A provider-only artifact is
  a row of `arrangement` and is ABSENT from `loaded`, because no Kernel loads a provider. Do
  not "fix" that. Three questions, three owners, three currencies; one merged table would need
  a row kind that is none of them.
- **THE SEAM IS AN OFFICE, NOT AN INJECTION.** The host mounts two read-only participants holding
  `const` references into `main`: `ArrangementDoor` (`workshop/arrangement.hpp`, office
  `zengine.arrangement`) over the realization owner, and `PowersDoor`
  (`workshop/powers_door.hpp`, office `zengine.powers`) over the catalog; the loaded tool ASKS
  and gets a value back. No `Catalog*`, `PlanExecutor*` or container crosses into a dynamic
  artifact, `ZenHostApi` did not widen, and there is no second injected capability. It is the
  same seam `zen.ListLoaded` already spends, pointed at more facts.
- **THE ANSWER IS LOOM'S OWN, so the asker checks `mail.answers_ask()`** rather than a
  correlation alone — the door ANSWERS (attested provenance no payload can write) where the
  Manager RELAYS. Where the stronger bound exists it is taken; the correlation is still
  compared, because it says WHICH room is being answered.
- **⚠ IT DERIVES AT EVERY ASK AND KEEPS NOTHING.** No mirror, no cache, no registry, no
  snapshot between asks — which is what makes an overlay mounted since the last reading appear
  in the next one with nobody notified. A copied provider map answered from the door's
  constructor turns the overlay witness and the keeps-nothing witness RED and leaves every
  derivation-tier case green.
- **AT THE ARRANGEMENT DOOR AN OFFICE MAY ASK; ANONYMOUS SPEECH MAY NOT.** The rule names
  nobody — no allow-list — so a tool added tomorrow asks with no edit here. It is NOT
  containment and is not reported as one: the loader binds `allow_any()` to every library. What
  keeps these facts from becoming ambient is that the door PUBLISHES NOTHING; every answer goes
  to the one weave that asked.
- **THE DISCOVERY DOOR ANSWERS WHOEVER ASKED**, an office or a participant speaking for itself,
  because the Workshop Terminal asks it in its own name and holds no office; only a root's send,
  which has nobody to answer, is counted instead. It too publishes nothing, and asking it grants
  nothing to send, mount or open: the Terminal's widening is its two asks, to `zengine.powers`
  alone (WL-TERM-18).
- **The projection pairs two owners.** A resolved row does not know whether its mount was an
  overlay — the MODE is only in the plan — so `describe_arrangement` walks the AUTHORED list
  and asks the resolved list about each stem. Walking the resolved rows instead loses the
  authored mode. There is deliberately NO resolved role: `ResolvedArtifact::role` is the
  authored role copied forward, and the office the Kernel bound is the Loaded pane's fact.
  **⚠ A provider-only row reports no offer outcome** — its `offer` field is the FIELD'S
  DEFAULT and no offer was made; copying the enum straight through publishes a default as an
  observation. The row-state law itself is realization's:
  [`realization.md`](realization.md).
- **NO PROJECTION OR DOOR NAMES A POWER, A PROVIDER OR AN ARTIFACT**, and a source tripwire
  reads the two projections and the two doors for quoted literals and identifiers (never bare
  words — these files EXPLAIN what they refuse to branch on). A hard-coded vocabulary turns the
  genericity witnesses AND the tripwire red.
- **NO ROW CARRIES A CONTROL OVER THE SYSTEM.** No unmount, replace, reload, disable or
  activate anywhere, and no pane message mutates load or provider state. `powers` has controls,
  and every one of them is a decision about PRESENTATION except `[ Sample ]`,
  which runs one Source and changes nothing. Knowledge of a power is still not authority to
  replace it.
- **⚠ THE DEFAULT PANE IS EIGHT ROWS ON THE TERMINAL AND FOUR ON THE SHIPPED
  GRAPHICAL FACE, AND `kStackRows` IS FIXED**, so a bigger TERMINAL
  buys columns and no rows. A block-per-entry projection and an eight-row default are in
  tension: the shipped six-artifact `Project` pane shows ONE artifact and `... 5 more` until a
  weaver authors a taller window. That is counted rather than hidden, and a second denser
  layout was deliberately not invented.
- **The pane KEY is `arrangement` and the pane NAME is `Project`**, because the retired
  picker's name column (`kPickerNameCols`) was ten cells and `Arrangement` is eleven. The key is
  the durable half a saved setup names; do not rename it to match the name.
- **Each pane keeps its OWN room and its OWN outstanding question.** All three can be open at
  once, and one shared `rows_`/`columns_` would have made the last grant decide how the other
  two were drawn.

## The Powers pane became a browser, and the seam did not move

`powers` was a projection a weaver could only read. It is now the first pane in this repository a
weaver BROWSES: two derived views over one catalog, a `component::TextBox` query, a composite
filter, an identity-held cursor per view, and one explicit sample. **The pane protocol did not
widen** — `PaneKey`, `PaneTextInput` and the two clipboard sentences were all already there, and
the weave simply began accepting them. `introspection/powers.hpp` is the pure half.

- **THE TWO VIEWS ARE DERIVED AND THE CLASSIFICATION IS NEVER AUTHORED TWICE.**
  `PowerRow::kind` is the discovery door's reading of `op::is_source` (and
  `op::declares_migration`) off the definition the host resolves through, so the pane asks the
  door rather than the identity's spelling — `source.anything` taking an argument is an Operator
  here, and a case arranges exactly that. There is no registration flag and nowhere for a second
  answer to live. A conversion is an operator whose signature is its edge, so it is listed under
  `Operators`. **Source/Operator and composite are independent**: all four cells are legal, the
  badge reads the ACTIVE contribution (an overlay changes construction and not contract), and it
  promises *this implementation has known compositional structure* and nothing about opening it.
- **THE SEARCH IS THE DOOR'S.** The view, the query and the composite filter are one
  `FindPowers` (`powers_question`), asked again on every change and on every grant; the pane
  never re-matches text, so it shows the rows Flow and the Terminal are answered for the same
  question. Its list is one page of at most `kMaxPowerRows`; more is counted, never silent.
  A power its contributor does not offer for reuse is listed and says so, `(not offered)`.
- **A ROW IS A CARRY SOURCE.** A press on a power, held and moved, acquires its reference under
  that press -- `zengine.OperatorRef`, the identity and two content ids the door's row carried --
  as an ordinary value carry (`PaneOperationRequested`, then `PaneValueCarryRequested`), so
  Inventory keeps it like any value and Flow adds it as a node. The reference header names no
  catalog, so the image still links no operator target; a form's row carries nothing.
- **⚠ BROWSING CANNOT EVALUATE, STRUCTURALLY.** `zengine-introspection` links no operator
  target: there is no catalog, definition, callable or `evaluate` in that image, so the property
  is a fact about the build graph rather than a discipline. A tripwire reads the two sources for
  `#include "operator/` and the CMake for an operator link edge — never for bare identifiers,
  because both files EXPLAIN at length what they refuse to reach.
- **THE WHEEL WALKS THE CURSOR** — the canvas pointer's `kWheel` spends `intro::move_cursor`,
  the step Up and Down take, one row per notch with fractions carried, and re-says the pane only
  when the selection actually moved. The window follows because it is derived from the cursor
  (`powers_window`); no second scroll position was added. A wheel over Loaded scrolls its
  viewport (see the wheel section above); over the arrangement pane it spends nothing.
- **ONE PLACE MEANS ONE THING, and the map is COLUMNS as well as rows.** The chrome row carries
  three controls side by side, so `project_powers_ui` returns spans (`row`, `first..last`,
  meaning) beside the rows it built — the one-geometry rule again. A row SELECTS and
  `[ Sample ]` SAMPLES, never both: the first press into a cold pane is also the press that
  points the keyboard at it, so nothing here may mean two things. **A control the width CUT is
  not a target** — spans inside `fit`'s `...` are not recorded, or a press on an ellipsis would
  operate a control the weaver cannot see.
- **THE SELECTION IS AN IDENTITY PER VIEW, AND PRESENTATION MAY ONLY HIDE IT.** A query, the
  composite filter, the other view and a short window all hide the mark and hold the fact; only
  the door's `DescribePower` answer saying the identity is gone, or now belongs to the other
  view, clears it (`take_described`). No index, scroll offset or window start is stored — the
  window is derived from the list and the cursor every projection. The detail and the
  contribution stack are that same answer's, shown only while it is about the selection.
- **THE PANE RETAINS ITS LAST ANSWER, and that member is the one to read carefully.** It is
  what the cursor operates over BETWEEN asks; it is replaced whole, never diffed, and dropped at
  every grant, so between a grant and its answer there are no rows and no map and Workshop's own
  `(waiting for the provider)` says so -- or, where the room only moved or changed size, the last
  picture marked `(updating)`, which takes no press. What survives an answer is everything the
  WEAVER authored — view, query, filter, both selections, the retained sample — because none of
  those is a fact about the host.
- **TYPED AND PASTED TEXT IS GATED TO PRINTABLE ASCII AT THIS PANE'S DOOR, AND REFUSED WHOLE.**
  `TextBox::type` admits any UTF-8, and `canvas_content_problem` (a picture) and `judge_content`
  (prose) refuse a whole update for one byte a canvas cannot draw, so a chunk with any
  inadmissible byte is declined entirely — the Editor pane's own paste posture. Every road into
  the query passes that one door: typing, a mirrored `ClipboardCopy`, and the answer to a paste
  ask. ⚠ **The shipped Composer has the same latent
  exposure** and was deliberately not repaired here; it is a different owner and a bounded QR
  candidate.
- **THE QUERY'S SELECTION IS FUNCTIONAL AND INVISIBLE.** The pane says where the query's caret
  stands and draws no selection for it, so cut and copy work and no highlight shows. Named
  residual.
- **A KEY IS GUARDED BY PANE BEFORE ANY STATE MOVES.** This office offers three panes and
  Workshop points the keyboard at the last one pressed, so every arm is behind one
  `key.pane != kPowersPane` test. A case drives it through the real seam with two panes from
  this same office.
- **THE SAMPLE LEAVES AS A MESSAGE AND COMES BACK AS PROSE.** `SampleRequested{identity}` to
  `zengine.sources`; `op::sample` at the spend; the value rendered HOST-side; `SourceSampled`
  answered. No `loom::Value`, `OperatorDef`, callable or provider image crosses — and could not,
  because a woven weave's accept-set is closed at compile time and cannot name the answer's
  shape. One outstanding sample, matched by `answers_ask()` plus the correlation; the identity a
  retained answer wears is the one THIS PANE ASKED FOR, never the one the payload carries.
- **A RETAINED SAMPLE IS HISTORY AND THE ROW LEADS WITH THE TENSE.** `sampled when asked` comes
  FIRST because `fit` cuts the tail: a narrow pane loses the identity, which the list can
  recover, and never the claim. It survives view switches, filters, cursor moves and the
  provider unloading; nothing refreshes it, and a later sample replaces it with whatever is true
  then.
- **⚠ A HOST WITH NO SAMPLE DOOR MAKES THE GESTURE SILENT.** The ask reaches nobody, the
  question is retired and NOTHING is said — the same rule the two waiting panes keep, because
  rendering "the ask went nowhere" would put this tool's plumbing where a weaver reads facts
  about their system. Recorded as a limitation, not designed as a feature.
- **THE COMPOSITION DROPS WHOLE, BY PRIORITY, against TWO measured defaults** — 8x48 on the
  terminal and 4x71 on the shipped graphical face. Chrome, then the list (which keeps up to
  three rows before anything else takes any), then the selected detail, the retained sample, the
  bounding sentence, the catalog census and the provenance line. At four rows with a long list
  the pane is chrome and a list, and that is the honest answer for four rows.

## Preferred space and demo owner operations

`PaneOffered v1` is unchanged. `workshop::v2::PaneOffered` is version 2 with `rows` and
`columns`: requested body text units, both 1..`workshop::kMaxPaneComfort` or zero/zero for the fallback. The first
accepted offer fixes the preference for that runtime pane identity. A later offer refreshes
the label/summary but keeps the preference; authored geometry wins per axis. Workshop owns
metric conversion, title/chrome allowance, fitting and stack seating. This is a preference,
not a minimum size or a rectangle the provider may enforce. Simple v1 examples remain supported.
`workshop::v3::PaneOffered` is version 3 with `width`, `height` and `text_rows`: a canvas body in
canvas pixels, each 1..`workshop::kMaxPaneBodyPx`, and rows of the medium's own text beneath it,
or zero/zero. Workshop grants that body exactly, rounded up to the cells that hold it in a
terminal; the view host, Flow and the View Builder offer it.

`workshop/setup_control.hpp` carries `SetupApplyRequested` (serialized setup),
`PaneResetRequested` (pane name) and `WorkshopQuitRequested` (the one quit, answered with its
outcome). A setup naming a pane this Workshop cannot present is refused with the first such pane's
provider and key, which is how setup preparation learns which provider to build, or which pane to
leave off a desk it does not prepare. `PaneResetRequested` explicitly discards transient view/draft state
in the addressed Info, Compose or Inventory pane, refusing while its owner operation is pending.
It neither removes inventory entries nor reverses earlier external effects. Setup preparation
uses these owner doors; what a setup prepares is its description's (`setup.json`), read by the
Python service. [Ready-to-use setups](../docs/workshop/demo-setups.md) owns the public scope and
recovery path.
