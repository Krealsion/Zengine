# Agent law — The pane seam and the tools that arrive through it

Routed detail behind [`AGENTS.md`](../AGENTS.md), for tasks touching the external pane
protocol, `introspection/` or `composer/` — what crosses the Workshop↔provider seam, and the
shipped tools that live entirely on the far side of it. Workshop's own screen and routing law
is [`workshop.md`](workshop.md); public reference:
[`../docs/reference/workshop-panes.md`](../docs/reference/workshop-panes.md) and
[`../docs/reference/introspection.md`](../docs/reference/introspection.md). Phase tags like
(SEL-0) are provenance markers into this repository's history; the law here is current.

## A press crosses the seam as a place, never as a meaning (SEL-0)

`PanePressed{pane, row, column}` is the `PaneRoom` budget read backwards — a place in the
lattice Workshop already granted, and nothing that would let a provider locate itself on a
screen.

```text
occupied_at   -> Occupancy{occupied, what, kind}     ONE geometry walk, topmost first
                 is_runtime_kind(kind)?              -> external_press, and Workshop says NOTHING
external_press_at(panels, setup, screen, kind, space, x, y) -> ExternalPressAt{named, row, column}
                 bounds_of -> external_body_place -> prose_at -> minus kExternalHeaderRows
                 named == false  =>  no sentence. The press was still the pane's.
```

- **`Occupancy` carries the KIND it met, and that is the same answer rather than a second
  one.** The one caller asks a further question of the walk that already decided what is on
  top; resolving the pane again to locate the press would be two geometries for one press,
  which is what the `bounds_of`-for-both rule exists to refuse. `kNoKind = -1` for the picker,
  negative for `role::kNone`'s reason. Nothing switches on a built-in kind — the question asked
  is `is_runtime_kind`, which is about which SEAM owns the press.
- **Consumed by occupancy, before anything is sent.** A pane that owns visible room owns
  pointer refusal for that room, and nothing waits for the provider: there is no reply shape,
  `consumed` never crosses the wire (WP-R0), and a press that named no row is consumed
  identically and simply travels no further. The Terminal overlay and arrangement still
  take every press whole, one layer up, and the picker still answers first inside
  `occupied_at`.
- **Workshop says NOTHING on the notice line for an external pane**, and that inverts the rule
  for built-ins. `<name> is here -- nothing under it can be taken hold of` is TRUE of a
  built-in and would be a claim about an OUTCOME here, made before the outcome exists. What a
  press on a provider's row means is that provider's vocabulary; the answer arrives later as
  ordinary `PaneContent`. INT-R0's rule decides it: a refusal belongs to the deepest layer
  whose vocabulary contains the reason, and this layer's does not.
- **The header row is subtracted in BOTH directions or in neither.** `external_body_place`
  reserves the resolved header rows out of the fit before a provider is told its budget, so
  the row a provider means by 0 is the region's prose row under the header. Forgetting the
  subtraction on the way back is the off-by-one that would be invisible until a pane had more
  than one selectable row. Since WUX-1 the count is `external_title_rows`'s answer — the
  pane-title preference, with the keyboard-holding pane always keeping its title — resolved
  once and carried on `ExternalBodyPlace::header_rows`; the painter, the press path and the
  room grant spend that one answer, and a hidden title RETURNS its row to the provider's
  budget through the ordinary grant-on-change door.
- **A row that fits no prose is not a row.** Anything outside `[0, rows) × [0, columns)` — the
  header, the pixel remainder under the last prose line of a graphical medium, an unrecognised
  `space` — is refused rather than clamped. Rounding to a nearest row hands a provider a press
  at a place it never wrote to.
- **Workshop holds no selection, no focus and no memory of the press.** No
  `Workshop::selected_*`, no pane focus, no capture, no record of which pane a maker touched
  last, and no repaint on the forwarding path: Workshop's picture did not change, and if the
  provider answers, its own handler repaints. Nothing here reads `ExternalPane::shown` and
  nothing may — the moment Workshop looks at a provider's rows to decide what a press means,
  the seam has stopped being one.
- **A provider interprets the press against what it is CURRENTLY SHOWING.** `project_loaded`
  returns the row-to-entry map beside the rows it built (the one-measurer rule reaching
  interaction), the provider retains that value and drops it on every room grant, and a press
  costs one lookup and no observation. **A provider that re-queried its source to interpret a
  press would let a maker select something they were never shown** — silently, and only
  sometimes.
- **The fact a pane publishes carries DATA and no authority.**
  `LoadedSelected{pane, library, role}` is an occurrence, not a transition, and a listener that
  hears it has acquired nothing: a grant is per `(shape, version, target)` and a value in a
  message is not one. Values may flow; authority must not flow implicitly with them.

## The keyboard crosses as two shapes (MSG-0)

`PaneKey v1` `{pane, scancode, modifiers}` (`input::scan` / `input::mod`, forwarded) and
`PaneTextInput v1` `{pane, text}` (what the platform committed, forwarded). They ADDED to the
protocol and revised nothing — the older pane shapes are byte-identical, so a provider that
knows only the earlier protocol is unchanged and valid. Which pane has the keys, and how the
screen says so, is Workshop routing law
([`workshop.md`](workshop.md#the-keyboard-goes-where-the-maker-last-pressed-msg-0)).

- **Workshop does not ask a provider whether it wants keys, and there is no shape for saying
  so.** A read-only pane that is pressed does take the keyboard, and Loom's gate refuses the
  deliveries — the substrate's own correct answer to being sent a shape a weave never declared,
  visible on the tap. Adding a declaration would be a private per-seam copy of
  `zen.DescribeAccepted`, which is the door that already answers exactly that question.
- **No key release, no focus-changed shape, no capture, no hotkey registration, no IME.** The
  shape's ARRIVAL is the gesture, SEL-0's rule one gesture on.

## The Loaded pane: the first stranger tool (INTR-0)

`introspection/` builds `zengine-introspection`, an ordinary loadable weave, and it is the
first thing in this repository whose pane arrives entirely through the external protocol.
**Workshop compiled nothing for it**: `weave.hpp`, `panel.hpp` and `screen.hpp` do not name it,
no `panel::k*` was minted, and the picker learned its row from a live offer.

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
  from the declaration, because `Emit<...>` is informational in this Loom and `Kernel::load`
  binds `allow_any()` to every library it opens — so a declaration proves nothing on its own
  and is not quoted as though it did.
- **THE PANE'S ONE BEAT IS THE ROOM GRANT.** Loom gives a participant no arrival or departure
  event, so there is nothing to subscribe to and nothing here polls or times out. It re-reads
  when the pane opens, when a valid re-offer refreshes it, and when the resolved prose capacity
  moves — and the last row of the pane says `snapshot`, because between two grants that is
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
- **The picker MARKS a name it cut.** `picker_entry_text` runs the name through `detail::fit`
  before `detail::pad`: fit for the truth, pad for the alignment. `kPickerNameCols` is 10 and
  admission allows 32, so the cut fires the moment a name belongs to a party this build never
  compiled. `pad` still truncates in silence and that is still right for a column whose longest
  word is a constant somebody checked; the STATE column is padded and not fitted for exactly
  that reason.

## The Composer is a tool that WRITES a message (MSG-0)

`composer/` builds `zengine-composer`, an ordinary loadable weave, and it is the first thing in
this repository a maker can TYPE into across the pane seam. Workshop compiled nothing for it.

```text
PaneRef   zengine.composer / compose      the durable pair a saved setup names
office    zengine.composer                the only address anything reaches it by
stem      zengine-composer                a line in the HOST'S boot list
```

- **IT PUBLISHES NO VOCABULARY OF ITS OWN, and that absence is the headline.** Introspection
  needed one shape because it learned a fact nobody else could say for it. This one learns
  nothing: it hears `LoadedSelected`, asks `zen.DescribeAccepted`, reads `zen.AcceptedShapes`,
  speaks Workshop's pane sentences, and sends a shape belonging to whoever it is addressed to.
  Every sentence in its life was already in somebody's vocabulary — which is the strongest
  available evidence that the describe door left nothing missing for a composer to invent.
- **IT IS A RAW `loom::Weave`, AND IT HAD TO BE.** `zen.AcceptedShapes` is not a ZEN_SHAPE
  (its fields are lists of `zen.SchemaDesc`), `Accept<...>` takes types, and
  `WeaveBase::accepted_schemas()` is `final` — so a weave that wants to READ the answer cannot
  be woven. The cost is stated rather than hidden: this weave advertises no `zen.Poke*` doors
  and no `zen.DescribeAccepted` door of its own, so **a maker cannot ask the Composer what the
  Composer accepts.** That is a real asymmetry in a tool whose subject is that question.
- **THE ONE DECISION `draft.hpp` MAKES** is `lex_value(raw, /*quoted=*/kind == Text)`. The
  command-line lexer infers a type FROM THE TOKEN because a command line has nothing else; a
  form knows it from the schema, and those are opposite directions — `1000` typed into a Text
  field lexes to Int and would be refused for a field it is perfectly good for. Quoting is the
  command grammar's own way of saying "these bytes are text" and a Text field says the same
  thing with a schema. **Everything else is Loom's**: `compose_message` places and type-checks,
  `assemble` builds, and every refusal a maker reads is the ladder's own sentence naming the
  field and its declared kind. `1O00` in an Int field is caught without this repository knowing
  what an Int is.
- **EVERY ARGUMENT IS NAMED**, so the ladder only ever climbs rung 1 (all-or-error, never a
  guess) and the guessing rungs are unreachable BY CONSTRUCTION — there is no way to build an
  unnamed `Arg` in the file.
- **PRESENCE AND VALUE ARE TWO MEMBERS AND NEVER ONE.** `FieldDraft{present, TextBox}`. An
  absent field contributes no `Arg`, so `assemble` leaves it out of the Value — which is what
  makes `Text present with ""` and `Text absent` two different messages on the wire, and
  `Bool present with false` different from `Bool unset`. `cycle` is the ONE presence gesture
  and the three-state Bool is what that one rule PRODUCES over a kind with two values, not a
  rule of its own. The brackets are what say present: `[hello]`, `[]`, against
  `(required)`/`(absent)`.
- **A SNAPSHOT IS NOT THE REGISTRY.** `Snapshot{unique_ptr<Registry> deps, roots}` is replaced
  WHOLE per discovery. `register_schema` takes a claim nobody ever releases, so one long-lived
  Registry would accumulate every vocabulary a maker ever looked at and become the schema
  catalog this Loom deliberately does not have. Roots are resolved BEFORE deps, which keeps the
  describe door's distinction alive to the send: `deps` is what a root NEEDS, `roots` is what
  may be SENT.
- **`RenderedRow{SurfaceTextRow, RowMeaning}` is ONE value.** This pane has four interactive
  row kinds across two layouts; one `push_back` carries both halves, so a row without a meaning
  is unsayable. Provider-local, deliberately not a Surface shape and not a component.
- **`value_capacity` is asked by the painter AND by the caret-window reconciliation** (the
  one-geometry rule); and **`MessageDraft::desc` holds the schema's type spellings, derived
  once by `begin_draft`** — deriving them at the point of use made a form projection quadratic
  in the field count (measured: 4 µs at three fields, 473 µs at forty, on every keystroke;
  1.8 µs after). Nothing polled and nothing was wrong; it was the same answer computed
  `rows × fields` times.
- **It says `SUBMITTED` and nothing stronger.** Composed, assembled, handed to the bus — not
  delivered, not accepted, not acted on. The Ticket is deliberately not checked: an office send
  answers whether the AUTHORSHIP was permitted, which is one of five things that must go right
  and the most misleading of them to report as success.
- **The `LoadedSelected` edge is a LOCAL V0 POLICY and is written down as a limitation.** Every
  Composer in this build follows every Loaded pane, always, because `composer.cpp` says so: not
  authored by a maker, not switchable, not aimable at a second Loaded pane. What should replace
  it is maker-authored logic, NOT a binding engine extracted from one edge. It verifies
  `authored_from_role(zengine.introspection)` — a fact about a maker's gesture is worth exactly
  as much as the office it came from.
- **One question outstanding, matched by CORRELATION**, which is Loom's own. The bound is
  stated rather than oversold: the answer is sent PERSONALLY by the construction layer, so
  there is no authored office to verify, and `send_to_role` never told the asker which
  incarnation it resolved to, so there is no expected sender either.
- **`ZEN_FIELD` DERIVES EVERY FIELD REQUIRED**, unconditionally (`build_schema`), and only a
  woven weave answers the describe door — so **no accept-set the construction layer answers can
  contain an optional field**, in either repository. An optional field reaches a maker only
  from a weave that implements `loom::Weave` directly and answers `zen.DescribeAccepted`
  itself. The Composer's optional machinery is correct and is exercised by exactly such a
  target (`Optionals` in tests/test_workshop_panes.cpp).
- **The suite does not load the shipped Timer, and that is measured rather than chosen.** The
  Timer service re-arms its own beat inside its own handler and
  `Switchboard::drain_until_idle()` does exactly what it says, so loading it into a rig whose
  every gesture drains never returns — it hung, at the load. `TimerSeat` holds the Timer's own
  real SHAPES in the Timer's own office; the real SERVICE is exercised in the live run.

## The system can show what it is (INTR-1)

`zengine.introspection` offers Workshop THREE panes:

```text
loaded        the Kernel's loaded() map                which WEAVES are loaded
arrangement   the realization owner: its authored     which AUTHORED PARTICIPATIONS
              plan and its resolved rows               resolved, and where each one
              (picker name `Project`)                  has got to
powers        the host's op::Catalog                   which POWERS resolve, and whose
                                                       contribution satisfies each
```

- **THE PANES DISAGREE ON PURPOSE AND THAT IS THE HONESTY CLAIM.** A provider-only artifact is
  a row of `arrangement` and is ABSENT from `loaded`, because no Kernel loads a provider. Do
  not "fix" that. Three questions, three owners, three currencies; one merged table would need
  a row kind that is none of them.
- **THE SEAM IS AN OFFICE, NOT AN INJECTION.** `workshop/arrangement.hpp` mounts one read-only
  participant (`ArrangementDoor`, office `zengine.arrangement`) holding `const` references into
  `main`; the loaded tool ASKS it and gets a value back. No `Catalog*`, `PlanExecutor*` or
  container crosses into a dynamic artifact, `ZenHostApi` did not widen, and there is no second
  injected capability. It is the same seam `zen.ListLoaded` already spends, pointed at more
  facts.
- **THE ANSWER IS LOOM'S OWN, so the asker checks `mail.answers_ask()`** rather than a
  correlation alone — the door ANSWERS (attested provenance no payload can write) where the
  Manager RELAYS. Where the stronger bound exists it is taken; the correlation is still
  compared, because it says WHICH room is being answered.
- **⚠ IT DERIVES AT EVERY ASK AND KEEPS NOTHING.** No mirror, no cache, no registry, no
  snapshot between asks — which is what makes an overlay mounted since the last reading appear
  in the next one with nobody notified. A copied provider map answered from the door's
  constructor turns the overlay witness and the keeps-nothing witness RED and leaves every
  derivation-tier case green.
- **AN OFFICE MAY ASK; ANONYMOUS SPEECH MAY NOT.** The rule names nobody — no allow-list — so
  a tool added tomorrow asks with no edit here. It is NOT containment and is not reported as
  one: the loader binds `allow_any()` to every library. What keeps these facts from becoming
  ambient is that the door PUBLISHES NOTHING; every answer goes to the one weave that asked.
- **The projection pairs two owners.** A resolved row does not know whether its mount was an
  overlay — the MODE is only in the plan — so `describe_arrangement` walks the AUTHORED list
  and asks the resolved list about each stem. Walking the resolved rows instead loses the
  authored mode. There is deliberately NO resolved role: `ResolvedArtifact::role` is the
  authored role copied forward, and the office the Kernel bound is the Loaded pane's fact.
  **⚠ A provider-only row reports no offer outcome** — its `offer` field is the FIELD'S
  DEFAULT and no offer was made; copying the enum straight through publishes a default as an
  observation. The row-state law itself is realization's:
  [`realization.md`](realization.md).
- **NEITHER PROJECTION NAMES A POWER, A PROVIDER OR AN ARTIFACT**, and a source tripwire reads
  both files for quoted literals and identifiers (never bare words — these files EXPLAIN what
  they refuse to branch on). A hard-coded vocabulary turns the genericity witnesses AND the
  tripwire red.
- **NO ROW CARRIES A CONTROL.** No unmount, replace, reload, disable or activate anywhere, and
  no pane message mutates load or provider state. The maker gets the knowledge; the power is a
  later phase's to grant.
- **⚠ THE DEFAULT PANE IS EIGHT PROSE ROWS AND `kStackRows` IS FIXED**, so a bigger TERMINAL
  buys columns and no rows. A block-per-entry projection and an eight-row default are in
  tension: the shipped six-artifact `Project` pane shows ONE artifact and `... 5 more` until a
  maker authors a taller window. That is counted rather than hidden, and a second denser
  layout was deliberately not invented.
- **The pane KEY is `arrangement` and the picker NAME is `Project`**, because
  `kPickerNameCols` is ten cells and `Arrangement` is eleven. The key is the durable half a
  saved setup names; do not rename it to match the name.
- **Each pane keeps its OWN room and its OWN outstanding question.** All three can be open at
  once, and one shared `rows_`/`columns_` would have made the last grant decide how the other
  two were drawn.
