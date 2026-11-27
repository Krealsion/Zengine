# Agent law — The pane seam and the tools that arrive through it

Routed detail behind [the guide](../CONTRIBUTING.md), for tasks touching the external pane
protocol, `introspection/` or `composer/` — what crosses the seam between Workshop and a pane, told
from both sides, and the shipped tools that live entirely on the far side of it. The contract's own
law, what a pane and any host owe each other, is the register
[`pane-contract.md`](pane-contract.md) routes (`PANE-SEAM`); Workshop's own screen and routing
law is [`workshop.md`](workshop.md); public reference:
[`workshop/docs/workshop-panes.md`](../workshop/docs/workshop-panes.md) and
[`introspection/docs/zengine.introspection.md`](../introspection/docs/zengine.introspection.md).

## A press reaches a pane from the window, as a place in its area

The window that shows a pane's area tells the area's owner role of a press there --
`AreaButtonChanged`: the button, whether it went down, the point in the area's pixels, under the
window's number for the press -- and tells Workshop, which placed the area, where it landed
(`AreaPressed`, in the window's pixels, under the same number). The pane reads the point against
what it drew there (`WindowDrawing::pointer`, `window_cell`, `pane/drawing.hpp`); nothing
lets a provider locate itself on a screen.

- **The title rows are Workshop's frame, outside the area.** The area stands under the resolved
  header rows (`ExternalBodyPlace::header_rows`), so the row a pane means by 0 is its own first
  row. A hidden title returns its row by placing the area again; a press the window holds keeps
  the area where the press was aimed until the window says it ended. A provider
  must not let the new room undo what the press selected (Files keeps its selection by name
  across a same-place re-listing).
- **A place outside the rows names nothing.** The window cuts what a pane draws at its area's
  edge; a press past the last row a pane drew is the pane's to refuse, never to round to a
  nearest row.
- **Workshop holds no selection INSIDE a pane and interprets no row meaning.** What it holds is
  which PANE: the desk's selection and the keyboard's candidate (`Panes::selected`,
  `Panes::keyboard`, [`workshop/focus.md`](workshop/focus.md)). The moment Workshop
  looked at a provider's rows to decide what a press means, the seam would stop being one.
- **A provider interprets the press against what it is CURRENTLY SHOWING.**
  **A provider that re-queried its source to interpret a
  press would let a weaver select something they were never shown** — silently, and only
  sometimes.
- **The fact a pane publishes carries DATA and no authority.**
  `LoadedSelected{pane, library, roles}` is an occurrence, not a transition, and a listener that
  hears it has acquired nothing: a grant is per `(shape, version, target)` and a value in a
  message is not one. Values may flow; authority must not flow implicitly with them.

## Input authority and carried data

Inventory acquisitions and edits use `pane/operation.hpp`: a pane echoes the current
input/menu-choice correlation and names the exact owner operation. Workshop verifies that the
gesture belongs to that pane, is current and unspent **for its own hand** -- each hand, the weaver's
and each guest's, keeps its own gestures and records, so one hand's act never makes another's
stale (WL-GUEST-05) -- and has authenticated Input attribution. A physical hand is the host's
weaver; an injected hand must hold the named operation in its live Loom authority, or, for a
classed operation, the class in its admitted row. An allowed answer approves this intent once and
grants no new bus authority. The consumer still sends through its own ordinary grant.

`pane/carry.hpp` transports one owned reference envelope from an approved acquisition.
The same actor picks it up and places it; Escape cancels. Workshop interprets no payload fields.
A destination owns its decoding and must authorize a subsequent read/write.
The inventory receivers are described in [inventory](inventory.md). The carrier is bounded
to <!-- value kMaxCarryBytes KiB -->64<!-- /value --> KiB and is image-local, never a reload-kept pointer or an implicit grant.

`PaneValueCarryRequested` carries a value copy. Its `drag` flag selects primary release or
keyboard pick-and-place; receivers accept `PaneCanvasValueDrop`, a distinct door from live
references (`PaneCanvasDrop`), each a point of the area it landed in.
The release's receiver is retained; a departed receiver cannot be silently replaced.
Acquiring bytes grants no authority to an operation requested by the receiver.

`PaneObservationRequested` retains that one approval, narrowly, for repeated reads: the same
current-gesture and actor check (`approve_gesture`) spends the gesture and records a lease of
holder, the holder's role, pane, the observed role, shape, version, subject and actor, one per pane and
sixteen in all.
Each `PaneObservationContinued` is judged again -- role holder, desk presence, subject, and an
injected actor's live authority -- and any lapse refuses and forgets it; only the holder's own
continuation or `PaneObservationEnded` touches a lease, and a request first forgets leases whose
holder no longer holds its role. It grants nothing on the bus, is never created by a timer,
invalidation or old correlation, and lives only in the running Workshop. Ending is the pane's on
every exit: a request stopped before its answer ends the lease that answer grants, and because a
reload keeps the WeaveId, an arriving image sends `PaneObservationEnded{pane, 0}` -- every lease
it holds on that pane -- for each pane it may observe from.

`PanePointRequested` answers where one character of a word of a reading is now, refusing it
when what the pane shows moved. It reads presentation; pressing the point is ordinary input.
Workshop names
nothing inside a pane: a word's number is its place in one answer.

## A pane draws in its own area of the window

The
complete reference is [`workshop/docs/workshop-panes.md`](../workshop/docs/workshop-panes.md); the
host's laws are [`workshop/canvas.md`](workshop/canvas.md). The provider owns its picture, hit
testing and every semantic action. No node, wire or pan behavior belongs in the host.

- **A room is the window's to give.** A holder arriving with nothing asks it (`RoomAsked`); a new
  room starts the drawing afresh; a reloaded provider keeps no room, picture map or gesture.
- **The pointer is the window's word.** A pane hears `AreaButtonChanged`, `AreaPointerMoved`,
  `AreaWheelTurned` and `AreaHoldEnded` in its area where its role takes them, as every shipped
  pane's does: a press's moves and release held to it under the window's number for the press,
  and a hold the window ends the press lost ([the Graphics package](../graphics/docs/graphics.md#who-says-what)).
  Workshop relays it none of them. A continuation it sends about the press -- a carry, a
  pass-back, a menu -- cites the window's number, which Workshop kept from `AreaPressed`.

## The keyboard crosses as two shapes

Which pane has the keys, and how the
screen says so, is Workshop routing law
([`workshop/focus.md`](workshop/focus.md)).

- **Workshop does not ask a provider whether it wants keys, and there is no shape for saying
  so.** A read-only pane that is pressed does take the keyboard, and Loom's gate refuses the
  deliveries — the substrate's own correct answer to being sent a shape a weave never declared,
  visible on the tap. Adding a declaration would be a private per-seam copy of
  `zen.DescribeAccepted`, which is the door that already answers exactly that question.
- **No key release, no focus-changed shape and no IME.** The
  shape's ARRIVAL is the gesture, the press's rule one gesture on.
- **Escape deselects by default (WL-ARR-15).** A bare Escape no row of the pane claims reaches
  it only when its role's holder lists `PaneEscapeUnspent` in what it emits: the declaration
  that it judges its own Escape. It drops what it has selected, or, with nothing selected, says
  `PaneEscapeUnspent{pane}` as the role that offered the pane, **echoing the correlation that
  Escape arrived under**, and Workshop puts the pane down while the pane still has the desk and
  the keys and that Escape is still the last gesture the host handled.
  What counts as a selection is the pane's: a chosen row
  it can let go (Loaded, Powers), never a marker that always rests on a row (Files, Inventory and
  its views, the Desktop's lists, a field cursor).
  A holder that does not declare the word is never sent a bare Escape: it could not hand
  one back, so Escape's last meaning answers at once (`holder_emits`). There is no answer and no
  retry; a later key, text, press or wheel of the same hand makes the word stale.
  Zero, an older Escape's number and an already-spent one all
  move nothing.
  **And a bare Escape is not sent at all** when the role's holder accepts no `PaneKey` and the
  pane declared no row for it (`holder_accepts`, the same reading the press's version comes from):
  nothing could have spent it, so Escape's last meaning answers at once (WL-ARR-16). That is a
  declaration read off the bus, never an inference from silence.

## The wheel reaches a pane from the window

- **No rows-per-notch and no accumulator on Workshop's side.** How many rows a notch is worth is
  the provider's grammar -- the shipped Powers and Composer spend one row per notch and carry
  fractions until they are worth one.

## The reveal and the quit cross as four more shapes

- **The reveal is one ask and one answer**: **`PaneRevealRequested v1` `{pane}`**
  (provider → Workshop, as the role that offered the pane: seat me now, my act needs nothing
  more) and **`PaneRevealAnswered v1` `{pane, seated, refusal}`** (Workshop's answer, on the
  delivery that asked, about what that delivery DID — the pane is seated, selected and has the
  keys, or nothing moved and here is the launch door's sentence). Judged first, through the
  launch door's own trial seat on a copy of the setup; written only if the seat is real. A
  refusal moved nothing; a screen that shrank before the ask arrived refuses it in the launch
  door's words; a
  shrink after the answer is an ordinary presentation change to a pane on the desk.
  A reveal naming a pane the role never offered is refused; one from
  nobody is dropped unanswered.
  A pane whose act needs a
  seat and nothing else asks for one. An act that changes a DOCUMENT and its presentation
  together — opening a source — is the managed opening below, because the two facts live in
  two weaves and FIFO dispatch lets either change between two deliveries: the reveal-as-
  commitment shape held the asker's gestures to a bound and dropped the rest, and no bound
  repairs that.
- **`PaneQuitRequested v1` `{}`**, Workshop → each participant that takes it, an ASK as the role,
  and **`PaneQuitAnswered v1` `{pane, permitted, refusal}`**, its answer: may this Workshop end? A
  pane asked MUST answer it, about the instant it answers; the host asks each participant Loom's
  participants claim lists as taking it, ends the process at once when nobody does, and otherwise holds
  every gesture until the last answer, refuses on any refusal (saying it, and replaying the held
  gestures), and proceeds on all permissions. A pane with an answer still owed to it — a paste in
  flight — refuses rather than waits, so one clipboard read cannot hold every other pane's exit.
  An ask Loom ends with no answer -- refused, returned from unanswered, its kept answer let go --
  refuses the quit. What is NOT solved: a participant that keeps its answer and never spends it
  holds the quit open, and that is named rather than timed out.

## The second button reaches a pane from the window, and a menu is presented by a participant

Between Workshop
and the participant holding `zengine.presenter`, `pane/presenter_vocabulary.hpp`:
`MenuGranted`, `MenuInput` and `MenuWithdrawn` Workshop → presenter, `MenuShown`, `MenuClosed`
and `PresenterReady` presenter → Workshop, and `HeldMenu`, the reload state the shipped
presenters share.
A holder that accepts only an older
`MenuGranted` does not hold the role for this host, so a menu is refused where it would open,
in words.
The host's
side is law in `workshop/press-chain.md` (WL-PRESS-06), `workshop/contextual.md`,
`workshop/pane-menu.md` (WL-CTX-09) and `workshop/desktop-presenting.md`; the
presenter's is WL-CTX-10, in `workshop/pane-menu.md` beside it; the helpers a pane may use are
`pane/menu.hpp`, installed beside the protocol.

- **The pane is first, and delivery is the disposition.** A secondary press in a pane's area is
  the pane's, heard from the window where its role takes `AreaButtonChanged`: no menu, no
  selection, no keys. A pane may act (the guard example blocks while the button is held), hand
  the press back (`PanePassRequested`, citing the window's number: the host's own pane menu opens
  once), or ask for a menu of its own rows. The shipped panes hand back every right press they
  have nothing for, so none is lost; a holder's silence is still its own disposition, which the
  host cannot see. **A right press on a pane whose role does not hear the window is not
  lost:** no keys move, and the host's own pane menu opens at the press — the same menu the
  chrome (the title row) and the Pane Manager reach.
- **The host learns a presenter left from Loom, by the exact attempt, and answers THAT menu.**
  **Taking a
  menu off the screen is not answering it:** the withdrawal is an ask of the presenter's role,
  under the menu's number, so the host keeps who asked (`WithdrawnMenu`: role, pane, subject,
  number, why, the attempt span) until the ask is answered or ends. The presenter answers its
  requester, then the withdrawal: `MenuClosed`, or `MenuReturned` for a menu it does not hold, on
  which the host answers the requester.
  A
  withdrawal Loom delivered and
  the presenter neither answered nor gave back stays that presenter's silence: no timeout, retry
  or departure notice is implied.
- **Not in this contract:** a secondary drag spent by a shipped pane (neither Editor spends a
  secondary motion), modifier state on a window's button events (`mod::kNone` always), a host-performed pane operation from a menu row, the
  host's OWN menus (chrome, room, tab, pass-back, a doorless body, a `manage` row) presented by
  the presenter — they stay the host's, the management route that must work with no presenter at
  all; the presenter shows the standard rows only beneath a pane's own — and a
  pane's actions offered on HOVER.

## Opening a source is a managed opening, jointly published (WL-OPEN)

The Editor's document and Workshop's presentation of its pane change TOGETHER, at one
published boundary, coordinated by the native opening manager the host mounts in
`zengine.opening`. The law is [`workshop/opening.md`](workshop/opening.md) (`WL-OPEN`); the
Editor's half is [`workshop/editor.md`](workshop/editor.md) (WL-EDIT-05); the
substrate — joint publication of latest claims, the showing, the record's lifetime — is Loom's
joint-publication reference page, never restated here.

- **The two claims are identities, never documents.** `EditorDocument` carries the path, the
  epoch, the convention, the content revision and the dirty flag; `PanePresentation` the seat,
  selection, keys, room, admitted generation, the count of inputs routed to the pane, the stack
  capacity and the setup digest — the desk's panes, places, sizes and ranks, and not a pane's
  settings. Each owner derives its claim from its own state at the end of every delivery and
  claims only when it moved (`after_delivery`), so an edit to A, a routed input, a resize or an
  authored change to the desk's panes, places, sizes or ranks moves a claim and aborts a
  preparation bound to the previous revision. Nothing is held. A rename or a setting written
  meanwhile survives the publication, which keeps the live desk's name and settings over the
  trial's copy.
- **Every managed sentence is judged under the role stamp.** The trial, the admission and
  the settlement are taken only from `zengine.opening`; a preparation only from the manager;
  a forged settlement, preparation or admission reaches its party and is dropped by it. The
  manager's own answers are matched by Loom's `answers_ask()` plus its correlation and stage.

## A pane declares its actions, and the host dispatches the resolved id

The host's side — the join, the collision law, the legend, the dispatch — is
Workshop's law, [`workshop/keyboard.md`](workshop/keyboard.md) (WL-KEY-15).

**And there is a second published version, beside v1 and not instead of it**:
`v2::PaneActions v2` of `v2::PaneActionRow v2`, which is v1's four fields plus `supersedes`.
The field was first added to v1 in place, and that was wrong for a reason the substrate states:
a published `(name, version)` is frozen and identity across a `.so` seam is the content-id
derived from the shape, so adding a field changed the identity of `PaneActionRow` v1 AND of the
`PaneActions` v1 that encloses it — a provider built against the old header and a host built
against the new one could no longer both register
([Loom GATE-04](https://github.com/Krealsion/Loom/blob/main/docs/laws/admission-laws.md)).
Workshop accepts both doors and joins
them into one admitted row set; a v1 declaration means what it always meant — this pane owns no
host action — and nothing reinterprets old bytes.

- **A row is the host's own catalog row minus `Act` and minus `KeyContext`.** The id is in the
  pane's namespace and is what a weaver's keymap file names, so it is durable the way a pane key
  is; the label is what the band prints; the gesture is the SAME two numbers `PaneKey` carries,
  `input::scan` and `input::mod`, and never a key name — a name is a spelling the host's grammar
  owns. `scan::kUnknown` declares a row with no default, one a weaver may bind.
- **Judged whole under the role stamp, exactly as the offer is.** An empty role retains
  nothing; a pane this role never offered is refused by name; the rows meet a bound
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
- **A PANE'S NOTICE STANDS UNTIL THE WEAVER'S NEXT ACT, never until it has been said once.**
  A built-in wrote its sentence on the band; a pane has only its own room, so the sentence is a
  row it publishes — and ONE gesture produces SEVERAL publications in one drain (write the
  notice, say the rows, ask a door whose answer arrives on the same turn and says them again),
  of which Workshop keeps the LAST. So a notice cleared inside `say` is a notice no weaver ever
  reads. Clear it where the pane ACTS on a gesture, and subtract its row from the composition's
  budget, or the room's last row is cut after the fact.
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
  one nobody declared.
  Each pane
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
  exactly what was sent, because a write does not cover typing done after it left.
  **A request Loom attests ended is not outstanding**: an ask whose ticket is not
  valid, or one `zen.AskEnded` names by its exact attempt -- refused before it arrived, or its
  owner gone or silent -- is released and said as unanswered, and nothing waits for ever
  (WL-INFO-13).

## A pane declares its settings, and is handed its layout's

The owner's side is
Workshop's law, [`workshop/settings.md`](workshop/settings.md); a pane's side is
`PaneSettingsFacet` (`pane/settings_taken.hpp`).

- **The pane means; the layouts' owner judges and keeps.** A pane checks the stamp, which is
  `zengine.layouts`'s, and its pane key, as it does for a room; takes the list whole, each setting
  it declared from it or its default; passes over a key it did not declare; and, when it cannot use
  a value — no kind, two, or one its setting does not take — keeps its default and says so where
  the setting shows, and a v2 pane may say it to the owner too, against the revision it was
  handed. Held settings are plain members, never Loom state, so a reload is refused for nothing a
  setting changed, and the reloaded image asks for its settings again. Gaining the `PaneSettings` door changes an
  image's accepted schemas: a pane that gains it arrives with a restarted Workshop.

## The Loaded pane: the first stranger tool

- **The fact it shows is the KERNEL's, and there is no second copy of it.**
  The provider holds no `known_weaves_`, keeps
  no diff, derives no arrival or departure, and stores no timestamp.
- **The wire form has no escaping**, and it is the one thing to know before reading it: the
  door joins on `,` and `@` and emits no delimiter of its own, so a library name containing
  either is unrecoverable in principle. `parse_loaded` splits on the LAST `@` — the ambiguity's
  better half, not a solution — and says so where the reading happens.
- **`zen.ListLoaded` is not the load capability.** A Loom grant is per
  `(shape, version, target)`, so asking what is loaded is exactly that one question;
  `LoadWeave`/`SwapWeave`/`ReloadWeave`/`UnloadLibrary`/`UnloadRole` are absent from this
  weave's Emit set and from every send it makes. The suite pins that **from a bus tap**, not
  from the declaration, because `Emit<...>` declares vocabulary and gates no send in this Loom
  (an undeclared shape is still sendable under a grant).
  A Kernel mints no grant of its own, and one with no policy admits nothing. A declaration
  proves nothing on its own and is not quoted as though it did.
- **An entry and its omission marker are ONE demand on the budget.**
  Showing PART of a list obliges saying
  how much was hidden.
- **A list MARKS a name it cut.**

## The Composer is a schema-directed message form

`composer/` builds `zengine-composer`, holding `zengine.composer` and offering pane `compose`.
The public weaver route is [Inventory to Compose](../inventory/docs/inventory-compose.md).

- LoadedSelected is accepted only from the introspection role. The target supplies its
  accepted roots and referenced closure through authenticated DescribeAccepted answers.
  The whole snapshot is replaced per selection; dependencies are not sendable roots.
- The rows are the pane's own drawing in its area
  (`rows_drawing`), numbered by what they mean (`RowMeaning`, which names the target and form a
  row was drawn under): a press or a drop reads its place back to a row of the rows it drew and
  acts only while that row's meaning stands, so an incompatible drop leaves the draft intact.
  Existing included fields require explicit exclusion before replacement. Whole forms require an
  empty draft and one of the selected target's accepted root schemas.
- The value and reference transfer doors remain distinct (`PaneCanvasValueDrop`,
  `PaneCanvasDrop`). A reference is copied as data into a compatible field; its contents are not
  automatically read and it conveys no authority.
- Ctrl+U discards a field; excluded local
  values never enter a preset. StoredDraft opens against the entire accepted closure and
  FieldValue checks the destination's declared type, including empty containers.
  A drop never submits. Submit separately authorizes the exact destination and versioned
  shape through PaneOperationRequested. Permission and storage requests freeze the form.
- An output ticket proves queuing only. Authenticated dispatch refusals and Refused answers
  are reported; arbitrary application success is not inferred. Pending questions may remain
  pending. New requests do not acquire authority from stored data or metadata.
- Loaded-to-Compose selection remains a local policy, not a general binding engine.

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
- **THE SEAM IS A ROLE, NOT AN INJECTION.** The host mounts two read-only participants holding
  `const` references into `main`: `ArrangementDoor` (`workshop/arrangement.hpp`, role
  `zengine.arrangement`) over the realization owner, and `PowersDoor`
  (`workshop/powers_door.hpp`, role `zengine.powers`) over the catalog; the loaded tool ASKS
  and gets a value back. No `Catalog*`, `Owner*` or container crosses into a dynamic
  artifact, `ZenHostApi` did not widen, and there is no second injected capability.
- **THE ANSWER IS LOOM'S OWN, so the asker checks `mail.answers_ask()`** rather than a
  correlation alone — the door ANSWERS (attested provenance no payload can write) where the
  Manager RELAYS. Where the stronger bound exists it is taken; the correlation is still
  compared, because it says WHICH room is being answered.
- **⚠ IT DERIVES AT EVERY ASK AND KEEPS NOTHING.** No mirror, no cache, no registry, no
  snapshot between asks — which is what makes an overlay mounted since the last reading appear
  in the next one with nobody notified.
- **AT THE ARRANGEMENT DOOR A ROLE MAY ASK; ANONYMOUS SPEECH MAY NOT.** The rule names
  nobody — no allow-list — so a tool added tomorrow asks with no edit here. It is NOT
  containment and is not reported as one: the loader binds `allow_any()` to every library. What
  keeps these facts from becoming ambient is that the door PUBLISHES NOTHING; every answer goes
  to the one weave that asked.
- **THE DISCOVERY DOOR ANSWERS WHOEVER ASKED**, a role or a participant speaking for itself,
  because the Workshop Terminal asks it in its own name and holds no role; only a root's send,
  which has nobody to answer, is counted instead. It too publishes nothing, and asking it grants
  nothing to send, mount or open: the Terminal's widening is its two asks, to `zengine.powers`
  alone (WL-TERM-18).
- **The projection pairs two owners.**
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
- **The pane KEY is `arrangement` and the pane NAME is `Project`**, because the retired
  picker's name column (`kPickerNameCols`) was ten cells and `Arrangement` is eleven. The key is
  the durable half a saved setup names; do not rename it to match the name.

## The Powers pane became a browser, and the seam did not move

- **THE TWO VIEWS ARE DERIVED AND THE CLASSIFICATION IS NEVER AUTHORED TWICE.**
  `PowerRow::kind` is the discovery door's reading of `op::is_source` (and
  `op::declares_migration`) off the definition the host resolves through, so the pane asks the
  door rather than the identity's spelling — `source.anything` taking an argument is an Operator
  here, and a case arranges exactly that. There is no registration flag and nowhere for a second
  answer to live.
- **THE SEARCH IS THE DOOR'S.** The view, the query and the composite filter are one
  `FindPowers` (`powers_question`), asked again on every change and on every grant; the pane
  never re-matches text, so it shows the rows Flow and the Terminal are answered for the same
  question.
- **⚠ BROWSING CANNOT EVALUATE, STRUCTURALLY.** `zengine-introspection` links no operator
  target: there is no catalog, definition, callable or `evaluate` in that image, so the property
  is a fact about the build graph rather than a discipline. A tripwire reads the two sources for
  `#include "operator/` and the CMake for an operator link edge — never for bare identifiers,
  because both files EXPLAIN at length what they refuse to reach.
- **A KEY IS GUARDED BY PANE BEFORE ANY STATE MOVES.** This role offers three panes and
  Workshop points the keyboard at the last one pressed, so every arm is behind one
  `key.pane != kPowersPane` test.
- **THE SAMPLE LEAVES AS A MESSAGE AND COMES BACK AS PROSE.**
  No `loom::Value`, `OperatorDef`, callable or provider image crosses — and could not,
  because a woven weave's accept-set is closed at compile time and cannot name the answer's
  shape. One outstanding sample, matched by `answers_ask()` plus the correlation; the identity a
  retained answer wears is the one THIS PANE ASKED FOR, never the one the payload carries.

## Preferred space and demo owner operations

`workshop::v2::PaneOffered` is version 2 with `rows` and
`columns`: requested body text units, both 1..`pane::kMaxPaneComfort` or zero/zero for the fallback. The first
accepted offer fixes the preference for that runtime pane identity. A later offer refreshes
the label/summary but keeps the preference; authored geometry wins per axis. Workshop owns
metric conversion, title/chrome allowance, fitting and stack seating. This is a preference,
not a minimum size or a rectangle the provider may enforce. Simple v1 examples remain supported.

A setup naming a pane this Workshop cannot present is refused with the first such pane's
provider and key, which is how setup preparation learns which provider to build, or which pane to
leave off a desk it does not prepare. `PaneResetRequested` explicitly discards transient view/draft state
in the addressed Info, Compose or Inventory pane, refusing while its owner operation is pending.
It neither removes inventory entries nor reverses earlier external effects.
[Ready-to-use setups](../workshop/docs/demo-setups.md) owns the public scope and
recovery path.
