# Agent law — Load plans, realization and the Builder

Routed detail behind [`AGENTS.md`](../AGENTS.md), for tasks touching `workshop/load_execute.hpp`,
`workshop/load_plan.hpp`, `builder/`, or a host's boot path — the authored load plan, the
realization owner, the load conversation, and how building relates to participating. Public
reference: [`../docs/reference/load-plan.md`](../docs/reference/load-plan.md) and
[`../docs/reference/builder.md`](../docs/reference/builder.md). Phase tags like (BLD-1a) are
provenance markers into this repository's history; the law here is current.

## The running arrangement is an authored FILE (LOAD-0)

What a host mounts and boots is [`../workshop/default-load-plan.json`](../workshop/default-load-plan.json).

```text
one artifact = one record, with ZERO OR MORE optional surfaces

    zengine-operators-basic   provider normal              (not a weave at all)
    zengine-timer             provider normal + weave      (ONE row, two surfaces)
    zengine-composer          weave                        (no provider mounted)
```

- **The host NAMES NO ARTIFACT STEM, and a tripwire refuses one.** `test_operator_provider.cpp`
  reads the host as a source file and checks forbidden strings — every shipped stem,
  `kComposerStem`, `kIntrospectionStem`, `mount_provider`, `OperatorOffer`, `MountMode`.
  ⚠ **A ROLE is deliberately NOT forbidden**: `surface::kSkinRole` and `timer::kTimerRole` are
  still in the host, inside GRANTS, and a grant naming a role is a statement about who may be
  spoken to. A role cannot become a load; only a stem can.
- **The intra-record order is SEMANTIC LAW and lives in `load_execute.hpp`**, not in the plan
  and not in the host: mount, then offer, then load. A host-backed Timer validates the rule it
  is about to spend inside its own `create()`, so a file that could say *weave, then provider*
  would author a Timer whose semantics depend on which load was in flight.
- **The inter-artifact order is authored POLICY and there is no solver.** ⚠ The obvious order
  witness is FALSE against this source and is pinned as false: putting the Timer's row before
  the basic provider's WORKS, because a composition crosses as STRUCTURE and resolves at spend,
  and by the time anything spends the plan has finished. Where order genuinely bites is an
  OVERLAY — an overlay row before the row it means to cover installs over nothing, and the
  ordinary mount it was meant to cover then collides with it. (That same fixture is the
  frontier falsifier — see the barrier law below.)
- **A failed handoff REFUSES the artifact.** `NotAConsumer` and `Offered` are both ordinary; a
  `VersionMismatch` is not "no host intended" and loading anyway would swap the process's
  semantic authority for the image's own copy. `NotOpened` is deliberately NOT refused there —
  the LOAD owns that sentence, which is how a missing artifact is reported in the loader's own
  words.
- **One artifact is the atomic unit.** A record that mounted and then failed to load unmounts
  ITS OWN contribution before reporting. Earlier artifacts stay, and the host says how many
  participated; a whole-plan transaction was not built.
- **An OPTIONAL surface is a LIST OF AT MOST ONE, and the split is deliberate.** Zen's wire
  grammar has seven kinds and none is `optional`, so presence is carried by the kind that
  already means "zero or more" and the record carries only its own fields — which is what lets
  the gate refuse *a weave declaration missing `role`* as a MISSING FIELD rather than as an
  empty string somebody has to remember to check. AT MOST ONE is the plan's law
  (`check_load_file`), not the wire's.
- **A hand-written plan needs no `content_id` and an Int is a QUOTED STRING.** Both measured
  against the compat codec; the shipped plans are indented for reading and carry no content
  id. `to_text` still emits the canonical one-line form, and a second write of a loaded plan
  is byte-identical to the first.
- **A stem carries no path separator and no `..`**, and that is an authority rule: a plan is
  an execution-authority document, and a stem that could climb out of the host's artifact
  directory would make *which files may run* a question about the plan's text. The host owns
  the one rule that spells a stem as a file (`HostContext::so`), which is why ONE plan is
  legal on Linux and Windows with no platform field.
- **`--load-plan` is the door, and there is no compiled-in fallback plan**: a missing file
  refuses by path and the host exits without mounting or loading anything. The graphical
  arrangement is a second SHIPPED PLAN staged only where both SDL artifacts exist.
- **Filesystem presence is not load authority.** The suite stages a real directory holding
  MORE artifacts than any plan names and proves an unlisted valid provider is neither opened
  nor mounted. A lookup table could not have asked the question.

## Realization is a living owner, not a call (BOOT-0)

`load::PlanExecutor` is persistent. It performs what it can, commands one weave load, and
RETURNS; the host's own loop delivers, and the answer advances it.

```text
begin(plan)                    the ordinary host loop        answered()
    mount what can be mounted      drain / pump                  withdraw the offer
    command one weave load         ...                           record the row
    RETURN                         ...                           advance(); RETURN
```

- **THE OWNER IS A LOCAL OF `main`, AND THAT IS A LIFETIME CLAIM BEFORE IT IS ANYTHING
  ELSE.** The obvious way to make an object event-driven here is to make it a weave. It must
  not be one: a registered weave is owned by the `Switchboard`, which a Zengine host declares
  BEFORE its catalog and Kernel so that reverse-order destruction takes the Kernel and its
  artifacts down first. This object holds an `op::OperatorOffer` — a share of an artifact
  image — so moving it into the bus would move its destruction to AFTER the catalog it
  unmounts from. And `op::mount_provider` needs a `Catalog&` while an offer IS A LIFETIME
  rather than an operation, so a weave owner would need a provider-mount message vocabulary —
  the generic host-action service that stays the host's own decision. ⚠ **Zero new
  authority**: the same one dangerous grant (`zen.LoadWeave -> manager`), still written by the
  host.
- **`op::OperatorOffer` IS A `std::optional` MEMBER, AND ITS BRACKET SPANS HOST TURNS.** It is
  neither copyable nor movable and was NOT made either; `emplace` constructs it in place and
  `reset()` runs the same destructor a closing brace would. It goes up before the command is
  sent (a consumer's first need is inside `create()`) and comes down in `answered()`, before
  the row is judged and before the next row begins. ⚠ A canary that resets it immediately
  after the send turns SIX cases red, including `WITHIN one record the provider is mounted
  BEFORE the weave is created`.
- **⚠ THE WITNESS FOR "THE OFFER REACHED `create()`" IS A REFUSAL, NOT A NUMBER.** With the
  Timer's own provider mounted, this host's `timer.normalize_delay` and the artifact's local
  fallback are THE SAME RULE and answer identically — so a `scheduled_delay` check cannot tell
  a live offer from a withdrawn one. What can: a Timer that IS offered a host must spend it
  and REFUSES a host publishing no such rule, while a Timer that met no offer loads happily.
- **⚠ NOTHING IN THE LOAD PATH TAKES A TURN.** The executor commands the load and RETURNS; the
  host's own loop is what delivers. The production fuse (`kLoadDrainTurns`, a 64-turn bounded
  drain per load) is GONE, not renamed, and nothing may replace it: an owner that returns to a
  host has nothing to count. `test_operator_provider.cpp` reads `load_execute.hpp` with the
  prose stripped and refuses ten scheduler verbs and nine async nouns, and reads the host
  source for plan-specific control flow in a host loop. The BEHAVIOURAL half is separate:
  ordinary traffic already queued when realization begins must NOT be delivered by the time
  `begin()` returns.
- **A PROVIDER-ONLY ROW SETTLES INSIDE `begin()`, AND SOMETHING NOW DEPENDS ON THAT (MIG-0).**
  `perform_row` mounts, sees no weave intent, and settles with the host having turned nothing
  — so every row ABOVE the first weave row is live before one delivery has been made. Workshop
  reads its session file from `on(SurfaceReady)`, which cannot arrive until a Skin has loaded,
  so a conversion provider authored above the Skin row is mounted in time by authored order
  alone (`workshop/session_history.hpp`; [workshop.md](workshop.md)). That is a REASON TO KEEP
  the barrier honest, not a new rule: reordering a plan's rows, or letting a waiting row be
  stepped over, would move a mount that a durable owner is now depending on.
- **AN UNANSWERED LOAD STAYS UNANSWERED.** No timeout, no deadline, no retry, no cancellation
  and no unanswerability vocabulary. The row is `loading`, the plan has not advanced, and
  nothing has been refused, for as many turns as a host cares to spend. Do not reintroduce a
  deadline, a retry, a turn budget or an empty-turn early-out under any spelling:
  `Switchboard::pending()` is `queue_.size()` at one instant, and a respondent that deferred
  its answer holds it off the queue entirely (measured: a zero-work turn with the answer owed,
  arriving afterwards). A timeout is not a settlement, and "the answer became impossible" is
  somebody else's state that nothing in this process knows.
- **A drain to idle never returns on a process with the Timer service loaded** — a Timer that
  has gone live re-arms its own beat inside its own handler. Workshop's loop drains and leaves
  by `Switchboard::stop()`; a host that wants control between turns wants `pump_pending()`.
- **ONE OWNER REALIZES ONE PLAN.** `begin` refuses a second rather than abandoning a cursor
  and a possibly-outstanding conversation. A host wanting a second arrangement against the
  same catalog and Kernel builds a SECOND owner — which is what makes a runtime provider
  collision reachable at all, since one plan cannot name an artifact twice. `PlanRig::realize_again`
  is that, in the suite.
- **THE PARTICIPANT IS A BRIDGE AND NOT A STATE MACHINE.** `load::PlanBooter` hears
  `zen.Result`/`Ack`/`Refused`, settles ONLY on correlation AND bus-stamped respondent (see
  the conversation law below), and its whole added responsibility is one line: hand the
  settled fact to the owner. It owns no catalog, no offer, no cursor and no order. ⚠ It is
  mounted BY HAND in the host rather than through `loom::mount_granted`, because the owner
  needs the PARTICIPANT and not just its id — the pointer is wired in the owner's constructor
  and unwired in its destructor, and ONE booter serves ONE owner.
- **⚠ `answers.answered` IS PAYLOAD, AND IT IS CLEARED BY THE NEXT `ask()`.** The owner opens
  row N+1's conversation inside the very handler that settled row N's, so by the time anything
  can look, that field is already about the next load. The CURSOR is the fact; do not assert
  on `answered` after a multi-row plan advances.
- **THE HOST'S FAILURE POLICY IS THE HOST'S.** A refused startup project still ends this
  Workshop and still exits 4 — expressed as an explicit settle-notice lambda that prints, sets
  `host.quit` and spends `host.request_stop`, because realization settles inside a delivery
  now and there is no call to return a code from. The owner has no opinion about process
  lifetime, and COMPLETION ends nothing.
- **THE CONTROL DOOR PATH IS LOAD-BEARING.** `Kernel::load` is reachable from the host and
  must not be shortcut to: only the control door can announce `zen.Activated`, from inside a
  delivery (`Switchboard::announce_as` is private), so a direct load produces a registered,
  routable, role-bound weave that NEVER BREATHES. A case loads the Timer both ways side by
  side. The row's completion fact is the ANSWER, which arrives strictly after the Kernel has
  the artifact — a case walks to that instant and checks the owner has not believed it yet.

## The load conversation: the asker keeps the book (QR-9, FRIC-2)

- **`load::BootAnswers` is a thin adapter over `loom::AskBook`** (`zen/weave/ask_book.hpp`),
  the asker-side conversation record — the sibling of `loom::relay`. Do not add a second
  correlation counter, a second expected-sender field, or a private `mine(mail)` here or in
  any Zengine host; `open` / `settle` / `awaiting` / `forget` are the whole surface, and the
  book has never heard of `zen.Result`. What stays local is **payload semantics** —
  `answered`, `refused`, `reason`, `weave` — because which shape means success is
  `PlanBooter`'s question and nobody else's.
- **Settlement is correlation AND bus-stamped respondent, both halves.** A correlation is a
  number the sender chooses (the first conversation of a fresh book is `1`), so it identifies
  a conversation and never authenticates a speaker; the stamped sender says WHO spoke, never
  WHAT ABOUT. Without the pair, an unrelated admitted `zen.Result` settles a load with a
  WeaveId no Kernel minted — measured, including a stray success CANCELLING a missing
  artifact's refusal. Not `kernel.is_loaded(...)` (true while the `zen.Result` naming the new
  WeaveId is still queued) and not an empty dispatch turn.
- **⚠ The book holds ONE**, because that is what this adapter has: the executor asks for one
  artifact at a time and waits for it (the authored order is not a scheduling hint). A
  capacity that exists to absorb an unwanted state is evidence about the state — a four-slot
  book paying for records nothing could read was deleted with its cause. If a second
  conversation is ever opened while one is outstanding the book refuses the NEW one and keeps
  what it is waiting on — never shedding the older, which is `loom::relay`'s policy and is
  wrong for the participant whose own question it is.
- **Local forgetting is not cancellation, and the difference is the whole point.**
  `AskBook::forget` changes this host's books and nothing else: Loom has no cancellation
  vocabulary, so nothing is sent, no `DeferredAnswer` is revoked, and the respondent still
  holds whatever answer right it held. A late answer to a forgotten load matches no record,
  settles nothing, and — because a forgotten conversation's correlation is never handed back —
  cannot settle the NEXT load either. Say `forget` only where the owner has genuinely stopped
  caring: **the one thing that forgets here is `~PlanExecutor`** — an owner ceasing to exist
  with a row in flight, the only honest way left to stop caring. Its destructor unwires the
  booter FIRST (the bus outlives it), then drops the offer, then forgets the ask. It does NOT
  unwind the catalog: the host's own declaration order takes the Kernel down before the
  catalog, and an owner racing that order would help nobody. Loom's own `TerminalSession` is
  the counter-example that keeps the rule honest: `await [turns]` merely PAUSES a person's
  patience, the ask stays visible to `pending`, and only `cancel` forgets it.

## Build procedure is authored, and it is NOT participation (BLD-1)

What a project can build is a fourth durable file,
[`../workshop/default-build-recipes.json.in`](../workshop/default-build-recipes.json.in)
(configured, then written beside the host), and it is deliberately not a section of the load
plan.

```text
build recipes   HOW an artifact can be PRODUCED      read by the Builder
load plan       HOW an artifact PARTICIPATES         read by the realization owner
                          \___ joined by ONE STRING: the artifact stem ___/
```

- **ONE COMPLETED CATALOG PER RUNNING HOST, AND BOTH WEAVES READ IT (PROJ-0).**
  `workshop::CurrentRecipes` (`workshop/recipes.hpp`) is the session owner of the completed
  catalog, of the tool's reduced views, and (PROJ-1) of the authored FILE all three came from
  — derived from the same rows in one `hold()` so they cannot disagree;
  `BuildRunnerWeave::catalog_` and `BuilderWeave::recipes_` are `const&` into it and neither
  keeps a copy, so replacing what the owner holds replaces what the whole program builds and
  shows. ⚠ The SUBTRACTION is untouched — the tool still reads only `RecipeView`, never a
  build procedure. Lifetime is the host's declaration order, and a temporary catalog is
  refused at compile time.
- **A CATALOG IS REPLACED LIVE, AND IT IS ONE TRANSACTION (PROJ-1).** `install_recipes` (read
  → parse → complete → hold) is the ONE seam, spent by the launch and by the maker's
  `files.use-recipes` gesture alike, so `--recipes` is INITIAL STATE and not a second recipe
  policy. Every pre-install step works on a candidate in its own frame, so a refusal leaves the
  path, the rows and the views exactly as they were; a valid EMPTY catalog is a successful
  replacement, not a failure. Completion stays project-parametric — the catalog file's own
  directory is never a source base, and choosing one moves no project anchor. The Builder's
  standing choice follows recipe IDENTITY across the arrival and never a row index
  ([workshop.md](workshop.md)); a build already in flight keeps the artifact file it was
  ordered for (`BuilderWeave::path_`) and is neither cancelled nor re-aimed. Nothing is
  persisted, discovered or watched.
- **The host keeps `ZENGINE_BUILDER_CMAKE`** — its own CMake, by absolute path — **and no
  target and no build directory.** What a FILE may name is inputs to a mechanism this package
  already holds; a program is the one thing it may not, which is why there is no third recipe
  kind and must not be one. ⚠ A `command:` field anywhere — file, message or struct — would
  turn every recipe catalog into an arbitrary-execution document.
- **TWO KINDS, AND EXACTLY ONE PER RECIPE.** `CMakeTargetRecipe` names a CONFIGURED build tree
  and a target (never a source tree: re-configuring somebody else's project is deciding a
  policy that is not Builder's). `SingleSourceRecipe` names one `.cpp`, its package prefixes,
  its link targets, optionally a build tree to borrow a toolchain from, and a workspace.
  Neither, or both, is refused rather than resolved by precedence.
- **A SINGLE-SOURCE BUILD IS ONE PROCESS, AND THE DRIVER IS A GENERATED `cmake -P` SCRIPT.**
  Configure and build are two invocations; a two-step sequence in the runner would make the
  process custodian a workflow engine and a two-step state machine in the tool would give a
  semantic owner a cursor over somebody else's procedure. Both refused. No shell, no
  `/bin/sh`, no `.bat` on either platform — and ONE operation, ONE identity and ONE ending
  describe a whole build. The script also tells `CMake configure FAILED` from
  `compile or link FAILED`, because it is the only party that sees both exit codes.
- **THE TOOLCHAIN IS BORROWED WITH `load_cache()` AND NEVER GUESSED.** Generator, platform,
  toolset, make program, CXX compiler, build type — read out of a configured tree the recipe
  names, by CMake, in a file a maker can open. ⚠ `CMAKE_C_COMPILER` is deliberately NOT
  borrowed: the generated project is `LANGUAGES CXX`, so passing it produces a
  manually-specified-variable warning in the middle of a maker's build output. MSVC needs the
  Visual Studio environment and INHERITS it from the host process; Zengine does not set,
  invent or look for it.
- **`outcome::kSucceeded` IS `exit 0 AND THE FILE IS THERE`, IN THAT ORDER**, and the order is
  the whole stale-artifact guarantee: a failed build is FAILED whatever is sitting at the
  destination, so an artifact left by an earlier success can never satisfy the current
  operation. A green build with its artifact absent is `kNoArtifact` — its own value, because
  it is its own problem and folding it into either neighbour tells a maker something false.
  The stamp taken when a build starts is NOT the test; it is how `built X` is told from
  `already up to date: X`. ⚠ There is no scan, no newest-file rule and no "there is one DLL".
- **⚠ A SINGLE-SOURCE RECIPE CANNOT REACH `kNoArtifact`,** because Zengine generates the
  project and therefore knows the target's output name IS the artifact stem. The outcome
  exists for an EXISTING CMake target, whose product is somebody else's decision and whose
  recipe's claim about it can simply be wrong. Both are witnessed; do not "simplify" the check
  away.
- **THE SEAM IS TWO SHAPES AND ONE NEW GRANT.** The tool may say `OfferArtifact` — ONLY when
  the maker asked for realization, because the shape carries an INTENT and a standing offer
  nobody made is not one. `PlanBooter` hears it, asks its owner, and publishes the owner's
  answer as `ArtifactRealized`. It is a COMMAND in this vocabulary's own table — an OFFER,
  not an order, because every eligibility rule and every refusal is the realization owner's.
  (Its original name claimed a fact — "the artifact is there" — that is equally true after a
  plain build, which publishes nothing; a message's name must state its truth condition.)
  ⚠ THE ANNOUNCED PATH IS NOT USED: the owner resolves a stem with the HOST's rule, so a
  message naming a path cannot redirect a load. The dangerous grant in this process is still
  exactly one and it is still the booter's.
- **`take_realization()` IS TAKEN AND NOT READ,** so a realization cannot be announced twice —
  which means a case cannot read it after driving the bus, because the booter already has.
  Read the published `ArtifactRealized` instead.
- **THE SUITE DRIVES A REAL CONFIGURED CMAKE TREE (`tests/buildfixture/`, `LANGUAGES NONE`),**
  configured at BUILD time, and only `cmake --build` runs inside a case. Neither production
  nor the suite can invent a build command; a fixture is a project. Every parameterisation is
  its own target, because `cmake --build --target` carries no arguments.
- **THE REAL COMPILE IS A LANE AND NOT A CTEST ENTRY.**
  `cmake -DZEN_BUILD_DIR=build -DZEN_WORK=<outside both repos> -P tests/build/run.cmake`
  installs a prefix, writes a one-file weave under a directory WITH A SPACE IN ITS NAME,
  drives `zengine-build-witness` (the Builder wiring with the picture removed, same weaves,
  same grants, same host loop), and fires two canaries — the package config removed, and one
  installed HEADER removed — each with a FRESH workspace, because `find_package` caches
  `zengine_DIR` and a reused workspace would report a resolution that never happened.

## A pending row is a BARRIER, not a hole (BLD-1a)

**Authored plan order IS realization order.** A walk that skips a row it cannot perform has
silently replaced that with ELIGIBILITY order — whatever happened to be on disk goes first.

```text
row N is waiting on the maker
    -> the row is `pending`, the owner is `Waiting`, and it RETURNS TO THE HOST
    -> row N+1 is NOT reached, NOT mounted, NOT loaded, and the host is NOT asked about it
    -> realize(N) settles it  ->  the frontier moves ON BY ONE  ->  the walk resumes
```

- **⭐ THE FALSIFIER IS AN OVERLAY, AND IT IS THE ONLY FIXTURE THAT CAN SAY THIS.** A two-row
  plan cannot tell "later rows ran early" from "later rows ran": both orders end with the same
  arrangement. An **overlay row authored BEFORE the ordinary provider it covers** does not —
  it is a BAD plan and the catalog refuses it (*needs an explicit overlay*), while the same
  two rows the other way round are accepted. So skipping the overlay row because its artifact
  is not built yet converts the refused plan into the accepted one, and the maker's file still
  says the wrong thing. ⚠ The witness is `an absent artifact cannot REORDER an overlay past
  what it covers`; it goes red under a canary that restores mark-pending-and-continue, along
  with eleven of the other thirteen.
- **A ROW MAY BE `pending`, AND IT IS THE HOST THAT SAYS SO.** `PlanExecutor` has ONE
  predicate (`AwaitingBuild`) and learns nothing else: the host answers from two facts that
  are its own — the artifact file is absent, and some authored recipe produces that stem.
  ⚠ THIS IS NOT "SKIP WHAT IS MISSING": an artifact nothing here can build still refuses the
  plan by name. It is not build-on-missing either; nothing starts, requests or remembers a
  build. With no predicate passed, every caller gets the pre-Builder behaviour unchanged, and
  a case pins that.
- **⚠ AND STILL NOTHING LOOKS AT A DISK.** A frontier that STOPS makes *is the file there
  yet?* very tempting; it is the HOST's question, asked once per row through `AwaitingBuild`
  and never polled. The `load_execute.hpp` tripwire refuses seven filesystem spellings beside
  its ten scheduler verbs and nine async nouns. Plain Build and Build & Realize stay two
  gestures; a plain build leaves the row `pending` with the file on disk until somebody asks.
- **`Realization` HAS `Waiting`, AND `Complete` MEANS COMPLETE.** `Complete` is reachable
  from exactly one place — the walk running off the END of the plan — and the walk cannot
  reach the end past a row it did not perform, so `outcome().ok` cannot read *the whole
  arrangement is live* while an authored artifact is untouched. ⚠ TWO SUBJECTS, TWO WORDS, on
  purpose: the OWNER is `Waiting`, the ROW it is waiting on is `pending`. Collapsing them
  leaves no way to say *which* row.
- **`Executed::waiting_on` IS ONE STRING, AND THE SHAPE IS THE LAW.** At most one row can be
  waiting, because the walk stops at the first one it cannot perform; a second slot could only
  ever hold a claim that later rows had leapfrogged an earlier one. It is DERIVED, not stored
  — `waiting_on()` is `cursor_` plus `state_`, the same argument `state_of` already makes for
  every other row state.
- **THE HOST IS TOLD AT EVERY REST, NOT ONCE.** `Settled` fires at three resting points —
  every row resolved, a row refused, and the walk stopped at a waiting row — because all
  three are moments realization will not move again on its own, and the third is the one a
  maker has to act on. A host tells them apart from the value alone: `ok`, or `refusal`
  non-empty, or `waiting_on` non-empty. ⚠ `!done.ok` IS NOT A REFUSAL; test `refusal` before
  calling anything failed, or a host prints *project refused:* with no reason and exits 4 on
  a healthy run.
- **`realize(stem)` PERFORMS ONE WAITING ROW, AND EVERY ELIGIBILITY RULE IS THE PLAN'S.**
  Busy, already resolved, not named by the plan, or **not the frontier row** — each refused in
  words. ⚠ THE ALREADY-RESOLVED ARM IS WHERE HOT RELOAD IS REFUSED, and the refusal says
  *restart*. ⚠ AN ON-DEMAND REFUSAL MUST NOT SET `Realization::Failed`: that is what the
  host's settle notice reads to end the process with exit 4, and a maker whose hand-asked
  realization was refused has not lost the Workshop they are working in. The row goes back to
  waiting — the frontier returns to exactly where the ask found it, which is what makes a
  corrected build a RETRY rather than a restart.
- **A LATER ARTIFACT MAY BE BUILT EARLY AND NOT REALIZED EARLY.** Builder owns building and a
  recipe it exposes is a recipe a maker may run; the file appearing changes nothing about
  order. Asking to realize it is refused BY THE NAME OF THE ROW IT IS BEHIND — and an
  ineligible ask moves nothing at all, least of all to `Failed`. When the frontier eventually
  reaches that row, the ordinary path finds the file and proceeds: there is no `prebuilt`
  state and must not be.
- **Row states are five tokens with five owners** — `authored`, `loading`, `pending`,
  `resolved`, `refused` — replacing a bool under which a row nobody had reached, a row in
  flight and a row that REFUSED were indistinguishable. ⚠ `building`, `available` and
  `mounting` were asked for and refused, each for a stated reason; a token with no owner goes
  stale in its first week. ⚠ A `loading` row publishes NO resolved field, even though its
  provider may already be mounted: within one row the mount precedes the load, and what came
  of the ROW is undecided, because a refusal rolls that mount back. `ResolvedPowers` reads
  the live catalog and shows it immediately: two questions, two owners, two currencies.
- **`describe_arrangement` TAKES THE OWNER, not a plan and a vector.** The owner holds the
  authored plan it is realizing, so a caller can no longer hand the projection one plan while
  the executor realizes another. The projection itself is the introspection tool's law:
  [`panes.md`](panes.md#the-system-can-show-what-it-is-intr-1).

## The frontier is visible and actionable, and gained no authority (BLD-2)

The Builder panel shows the waiting frontier and `f` builds-and-realizes it, and the whole
feature is one read-only seam plus one gesture over the existing route.

- **THE OWNER PROJECTS ITS OWN FRONTIER.** `PlanExecutor::behind()` joined `waiting_on()` —
  how many authored rows are behind the waiting row, derived from the same cursor, 0 in every
  non-waiting state. The host wires both into `HostContext::frontier`, a function returning a
  by-value `ProjectFrontier{waiting, artifact, blocked}` (panel.hpp); the weave derives it
  fresh at every repaint and every gesture and stores it NOWHERE. ⚠ No copy exists on the
  path — a `ProjectFrontier` member, session field, or answer cached between paints is the
  mirror this seam exists to refuse, and the live-mutation witness in
  `tests/test_workshop_panels.cpp` reddens one.
- **THE JOIN IS THE STEM, PERFORMED AT PRESENTATION.** Which recipes produce the frontier is
  answered by comparing the frontier artifact against the `RecipeCatalog` the tool itself
  published — the panel's existing copy. No plan→recipe edge was added anywhere, and the
  frontier view carries no recipe.
- **`f` SPENDS THE EXISTING ROUTE, WHOLE.** It sets the panel's `chosen` to the producing
  recipe — visibly, so the recipe row and the ask agree — and calls the same `build_now`
  path `Shift+b` calls, with `realize=true`. One send (`BuildRequested`), same office, same
  grant; everything downstream is the tool's, the runner's, and the owner's, unchanged. There
  is no second build path and no direct load, and a source tripwire beside INTR-1's pins it:
  no presentation source under `workshop/` — `weave.hpp`, `screen.hpp`, `panel.hpp` and the
  subject `.cpp` files beside them, walked by `presentation_sources` rather than listed —
  spells `PlanExecutor`, `load_execute`, `OfferArtifact`, `RunBuild` or `kBuildRunnerRole`.
- **⚠ SEVERAL RECIPES MAY PRODUCE ONE ARTIFACT, AND THE GESTURE NEVER CHOOSES.** That
  cardinality is authored law (`builder::check_recipes` deduplicates IDENTITIES, deliberately
  not artifacts, and a case pins the acceptance). With several matches `f` refuses and names
  them; what it may spend is the maker's own standing pick — `BuilderPane::picked`, written
  ONLY by `c`, reset when a catalog arrival clamps `chosen` — because `chosen == 0` is an
  index and not a choice. The falsifier stages the FIRST catalog row as a match: "use entry
  zero" and "read the default as a pick" both send an ask the case forbids.
- **THE `project` ROW EXISTS EXACTLY WHILE THE FRONTIER DOES.** `waiting <stem> (<recipe |
  N recipes | no recipe>, blocks <n>)`, taking the third `said` row only while waiting; with
  no frontier the panel is byte-for-byte BLD-1a's, because absence of a pending frontier is
  the whole answer and no `project ready` is manufactured. `blocks <n>` is `behind()` — the
  authored rows after the pending one, pinned against the owner where it is derived.
- **NO AUTOMATIC ANYTHING, STILL.** The view is a reading, not a power: encountering a
  buildable missing frontier starts nothing, plain `b` still leaves the row `pending` with
  the file on disk, and the maker gesture remains the only way a compiler starts.

## Do not assume

- Workshop knows which artifacts to mount and boot — it knows **neither**. It reads one
  authored plan and executes it, and names no stem at all.
- A plan row is configuration — it is an **execution-authority decision**: a provider row
  lets a native artifact contribute executable semantic power to the host, and a weave row
  lets it participate under a role. Nothing signs or restricts one.
- The load plan solves hot reload — it does **not**. It is initial and restart load intent;
  the provider-vs-`reload_from` interaction is still open, and the executor has no unload,
  reload or remount path.
- Restart persistence means a clean build recreates the artifacts — it means a fresh PROCESS
  reconstructs the same arrangement from the same file.
- Builder builds one hard-coded target — it builds an **authored recipe catalog**, and the
  host names no target and no build directory. What it still names is the CMake, because a
  file may not.
- The Builder or its runner owns the recipe catalog — **neither does**. The host that composed
  the process owns one completed catalog for the session, and both of them read it.
- A recipe and a plan row are two views of one thing — they are two truths with two owners,
  joined by the artifact stem and nothing else. A role, a mount mode or a load order in a
  recipe, or a source path, package prefix or build tree in a plan, is the duplication this
  split exists to refuse.
- A build that exits zero produced its artifact — that is TWO questions, and the second is
  `outcome::kNoArtifact` when the answer is no. The exit status is checked FIRST, which is
  the only reason a stale artifact cannot make a failed build look successful.
- Zengine compiles the single-source route — CMake does. Zengine writes two small files and
  starts one `cmake -P`; nothing in this repository names a compiler, a flag, a library or an
  output suffix for it.
- A `pending` row means the artifact is missing — it means REALIZATION DID NOTHING, at the
  host's word. Whether a file is absent is the host's fact and whether a build is running is
  the Builder's; the projection publishes what realization did.
- A successful build loads its artifact — it does **not**. It offers one fact, and only when
  a maker asked; the realization owner decides, bounded by the authored plan, and an
  already-loaded artifact is refused in words. There is no reload here.
