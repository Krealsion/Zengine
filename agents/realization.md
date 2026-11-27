# Agent law — Load plans, realization and the Builder

Routed detail behind [the guide](../CONTRIBUTING.md), for tasks touching `realization/` (the owner,
the plan and its file, what the owner claims), `builder/`, or a host's boot path — the authored load plan, the
realization owner, the load conversation, and how building relates to participating. Public
reference: [`workshop/docs/load-plan.md`](../workshop/docs/load-plan.md) and
[`builder/docs/zengine.builder.md`](../builder/docs/zengine.builder.md).

## The running arrangement is an authored FILE

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
  ⚠ **A ROLE is deliberately NOT forbidden**: `timer::kTimerRole` is still in the host, inside
  GRANTS, and a grant naming a role is a statement about who may be spoken to. A role cannot
  become a load; only a stem can.
- **The intra-record order is SEMANTIC LAW and lives in `realization/owner.hpp`**, not in the plan
  and not in the host: mount, then offer, then load. A host-backed Timer validates the rule it
  is about to spend inside its own `create()`, so a file that could say *weave, then provider*
  would author a Timer whose semantics depend on which load was in flight.
- **The inter-artifact order is authored POLICY and there is no solver.**
  Where order genuinely bites is an
  OVERLAY — an overlay row before the row it means to cover installs over nothing, and the
  ordinary mount it was meant to cover then collides with it.
- **A failed handoff REFUSES the artifact.** `NotAConsumer` and `Offered` are both ordinary; a
  `VersionMismatch` is not "no host intended" and loading anyway would swap the process's
  semantic authority for the image's own copy. `NotOpened` is deliberately NOT refused there —
  the LOAD owns that sentence, which is how a missing artifact is reported in the loader's own
  words.
- **One artifact is the atomic unit.** A record that mounted and then failed to load unmounts
  ITS OWN contribution before reporting. Earlier artifacts stay; a whole-plan transaction was
  not built.
- **A row AUTHORED OPTIONAL that refuses is an unavailable tool, not a refused project.**
  The walk records the refusing layer's own sentence (`Executed::unavailable`,
  row state `unavailable`, reason `unavailable_why`), steps over the row, and performs every
  row behind it in authored order. A REQUIRED row's refusal still stops the walk (`Failed`)
  with the rows before it standing. Nothing is retried or granted.
  An on-demand realization is never stepped over: a weaver who
  asked for that row is owed its refusal.
- **An OPTIONAL surface is a LIST OF AT MOST ONE, and the split is deliberate.** Zen's wire
  grammar has seven kinds and none is `optional`, so presence is carried by the kind that
  already means "zero or more" and the record carries only its own fields — which is what lets
  the gate refuse *a weave declaration missing `role`* as a MISSING FIELD rather than as an
  empty string somebody has to remember to check. AT MOST ONE is the plan's law
  (`check_surface_counts`), not the wire's.
- **A hand-written plan needs no `content_id` and an Int is a QUOTED STRING.**
  `to_text` still emits the canonical one-line form, and a second write of a loaded plan
  is byte-identical to the first.
- **A stem carries no path separator and no `..`**, and that is an authority rule: a plan is
  an execution-authority document, and a stem that could climb out of the host's artifact
  directory would make *which files may run* a question about the plan's text. The host owns
  the one rule that spells a stem as a file (`HostContext::so`), which is why ONE plan is
  legal on Linux and Windows with no platform field.
- **`--load-plan` is the door, and there is no compiled-in fallback plan**: a missing file
  refuses by path and the host exits without mounting or loading anything.
- **Filesystem presence is not load authority.**
- **A role may have authored CHOICES** (format version 2): the artifacts a weaver may switch
  it between, one of which starts; the others load only when a switch asks, and realization
  records which one holds the role. The law is `WL-SWITCH-01` and `-02`
  (`agents/workshop/editor-switch.md`); participation is still conferred by the plan alone.

## Realization is a living owner, not a call

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
- **⚠ NOTHING IN THE LOAD PATH TAKES A TURN.** The executor commands the load and RETURNS; the
  host's own loop is what delivers.
  Ordinary traffic already queued when realization begins must NOT be delivered by the time
  `begin()` returns.
- **A PROVIDER-ONLY ROW SETTLES INSIDE `begin()`, AND SOMETHING DEPENDS ON THAT.** `perform_row`
  mounts, sees no weave intent, and settles with the host having turned nothing — so every row ABOVE
  the first weave row is live before one delivery has been made. Workshop reads its session file on
  the first window's offer, which cannot arrive until a window has loaded, so a conversion provider
  authored above the window's row is mounted in time by authored order alone
  (`workshop/layouts/session_history.hpp`; [workshop.md](workshop.md)). That is a REASON TO KEEP the barrier
  honest, not a new rule: reordering a plan's rows, or letting a waiting row be stepped over, would
  move a mount that a durable owner is now depending on.
- **AN UNANSWERED LOAD STAYS UNANSWERED.** No timeout, no deadline, no retry, no cancellation
  and no unanswerability vocabulary. The row is `loading`, the plan has not advanced, and
  nothing has been refused, for as many turns as a host cares to spend. Do not reintroduce a
  deadline, a retry, a turn budget or an empty-turn early-out under any spelling:
  `Switchboard::pending()` is `queue_.size()` at one instant, and a respondent that deferred
  its answer holds it off the queue entirely (measured: a zero-work turn with the answer owed,
  arriving afterwards). A timeout is not a settlement, and "the answer became impossible" is
  somebody else's state that nothing in this process knows.
- **ONE OWNER REALIZES ONE PLAN.** `begin` refuses a second rather than abandoning a cursor
  and a possibly-outstanding conversation. A host wanting a second arrangement against the
  same catalog and Kernel builds a SECOND owner — which is what makes a runtime provider
  collision reachable at all, since one plan cannot name an artifact twice.
- **THE OWNER IS ONE PARTICIPANT, VOICE AND STATE MACHINE.** `realization::Owner` is a weave in
  the role `zengine.realization`: it hears `zen.Result`/`Ack`/`Refused`, settles ONLY on
  correlation AND bus-stamped respondent (see the conversation law below), and moves its own
  walk on.
  The role lets its words be observed by name, and grants nobody anything.
  A host puts it on the bus with `realization::mount`, under the grant the host writes, and
  keeps the handle (`Mounted`) after the Kernel: the handle takes the owner off the bus before
  the artifacts go.
- **WHAT IT KNOWS, IT CLAIMS.** Where the plan is (`PlanStatus`: its state, an enumeration
  (`PlanState`), the row it waits on and the rows behind it, the optional rows stepped over, what
  rows gave way from, the roles still to come), every authored choice with the one that holds its role (`ChoicesHeld`)
  and each row that loaded a weave, with its artifact and its reload rule (`RealizedRows`), are
  the role's latest claims (`realization/vocabulary.hpp`), claimed as each entry point returns.
  The desk, the guest door, the project door, the layouts' owner and the editor switch read
  them by their grants' observe rules, in their own deliveries, never through a host closure
  over the owner.
- **THE HOST'S FAILURE POLICY IS THE HOST'S.**
  The owner has no
  opinion about process lifetime, and COMPLETION ends nothing.
- **THE CONTROL DOOR PATH IS LOAD-BEARING.** `Kernel::load` is reachable from the host and
  must not be shortcut to: only the control door can announce `zen.Activated`, from inside a
  delivery (`Switchboard::announce_as` is private), so a direct load produces a registered,
  routable, role-bound weave that NEVER BREATHES.
  The row's completion fact is the ANSWER, which arrives strictly after the Kernel has
  the artifact — a case walks to that instant and checks the owner has not believed it yet.

## The load conversation: the asker keeps the book

- **`load::BootAnswers` is a thin adapter over `loom::AskBook`** (`zen/weave/ask_book.hpp`),
  the asker-side conversation record — the sibling of `loom::relay`. Do not add a second
  correlation counter, a second expected-sender field, or a private `mine(mail)` here or in
  any Zengine host; `open` / `settle` / `awaiting` / `forget` are the whole surface, and the
  book has never heard of `zen.Result`. What stays local is **payload semantics** —
  `answered`, `refused`, `reason`, `weave` — because which shape means success is
  the owner's question and nobody else's.
- **Settlement is correlation AND bus-stamped respondent, both halves.** A correlation is a
  number the sender chooses (the first conversation of a fresh book is `1`), so it identifies
  a conversation and never authenticates a speaker; the stamped sender says WHO spoke, never
  WHAT ABOUT.
  Not `kernel.is_loaded(...)` (true while the `zen.Result` naming the new
  WeaveId is still queued) and not an empty dispatch turn.
- **⚠ The book holds ONE**, because that is what this adapter has: the executor asks for one
  artifact at a time and waits for it (the authored order is not a scheduling hint).
  If a second
  conversation is ever opened while one is outstanding the book refuses the NEW one and keeps
  what it is waiting on — never shedding the older, which is `loom::relay`'s policy and is
  wrong for the participant whose own question it is.
- **Local forgetting is not cancellation, and the difference is the whole point.**
  `AskBook::forget` changes this host's books and nothing else: Loom has no cancellation
  vocabulary, so nothing is sent, no `DeferredAnswer` is revoked, and the respondent still
  holds whatever answer right it held. A late answer to a forgotten load matches no record,
  settles nothing, and — because a forgotten conversation's correlation is never handed back —
  cannot settle the NEXT load either. Say `forget` only where the owner has genuinely stopped
  caring: **the one thing that forgets here is `~Owner`** — an owner taken off the bus with a
  row in flight, the only honest way left to stop caring. Its destructor drops the offer, then
  forgets the ask. It does NOT
  unwind the catalog: the host's own declaration order takes the Kernel down before the
  catalog, and an owner racing that order would help nobody.

## Build procedure is authored, and it is NOT participation

What a project can build is a fourth durable file,
[`../workshop/default-build-recipes.json.in`](../workshop/default-build-recipes.json.in)
(configured, then written beside the host), and it is deliberately not a section of the load
plan.

```text
build recipes   HOW an artifact can be PRODUCED      read by the Builder
load plan       HOW an artifact PARTICIPATES         read by the realization owner
                          \___ joined by ONE STRING: the artifact stem ___/
```

- **ONE COMPLETED CATALOG PER RUNNING HOST, AND BOTH WEAVES READ IT.**
  `workshop::CurrentRecipes` (`workshop/recipes.hpp`) is the session owner of the completed
  catalog, of the tool's reduced views, and of the authored FILE all three came from
  — derived from the same rows in one `hold()` so they cannot disagree;
  `BuildRunnerWeave::catalog_` and `BuilderWeave::recipes_` are `const&` into it and neither
  keeps a copy, so replacing what the owner holds replaces what the whole program builds and
  shows. ⚠ The SUBTRACTION is untouched — the tool still reads only `RecipeView`, never a
  build procedure.
- **A CATALOG IS REPLACED LIVE, AND IT IS ONE TRANSACTION.** `install_recipes` (read
  → parse → complete → hold) is the ONE seam, spent by the launch and by the weaver's
  `files.use-recipes` gesture alike, so `--recipes` is INITIAL STATE and not a second recipe
  policy. Every pre-install step works on a candidate in its own frame, so a refusal leaves the
  path, the rows and the views exactly as they were; a valid EMPTY catalog is a successful
  replacement, not a failure. Completion stays project-parametric — the catalog file's own
  directory is never a source base, and choosing one moves no project anchor. The Builder's
  standing choice follows recipe IDENTITY across the arrival and never a row index
  ([workshop.md](workshop.md)); a build already in flight keeps the artifact file it was
  ordered for (`BuilderWeave::path_`) and is neither cancelled nor re-aimed. Nothing is
  persisted, discovered or watched.
- **The host keeps `ZENGINE_BUILDER_CMAKE`** — its own CMake, by absolute path, or none in the
  package a stranger downloads (`agents/packaging.md`), whose Builder then refuses in its own
  words — **and no target and no build directory.** What a FILE may name is inputs to a mechanism this package
  already holds; a program is the one thing it may not, which is why there is no third recipe
  kind and must not be one. ⚠ A `command:` field anywhere — file, message or struct — would
  turn every recipe catalog into an arbitrary-execution document.
- **TWO KINDS, AND EXACTLY ONE PER RECIPE.** `CMakeTargetRecipe` names a CONFIGURED build tree
  and a target (never a source tree: re-configuring somebody else's project is deciding a
  policy that is not Builder's). `SingleSourceRecipe` names one `.cpp`, its package prefixes,
  its link targets, optionally a build tree to borrow a toolchain from, a workspace, and what it
  `makes`: native, boxed or both. Neither kind, or both, is refused rather than resolved by
  precedence.
- **A PRODUCT IS A STEM, AND A RECIPE MAKES ONE OR TWO** (`products_of`). A single source's
  native product is its stem, its boxed one the stem with `kModuleSuffix`; a CMake target's one
  product is its stem, boxed when that names a module. The tool's views, the catalog's rows,
  `BuildRequested`, `RunBuild` and staging each name a product by its stem, so a plan row loads
  either by the name it already joins on. A boxed product's project is its own (`boxed/` in the
  workspace) and calls the package's `zengine_wasm_weave()`; its compiler is the host's
  `ZENGINE_BUILDER_WASI_SDK`, kept as `ZENGINE_BUILDER_CMAKE` is and empty in the package, so no
  recipe names one. Its absence, or a package with no backend, is a refusal in words.
- **A SINGLE-SOURCE BUILD IS ONE PROCESS, AND THE DRIVER IS A GENERATED `cmake -P` SCRIPT.**
  Configure and build are two invocations; a two-step sequence in the runner would make the
  process custodian a workflow engine and a two-step state machine in the tool would give a
  semantic owner a cursor over somebody else's procedure. Both refused. No shell, no
  `/bin/sh`, no `.bat` on either platform — and ONE operation, ONE identity and ONE ending
  describe a whole build.
- **THE TOOLCHAIN IS BORROWED WITH `load_cache()` AND NEVER GUESSED.** Generator, platform,
  toolset, make program, CXX compiler, build type — read out of a configured tree the recipe
  names, by CMake, in a file a weaver can open.
  MSVC needs the
  Visual Studio environment and INHERITS it from the host process; Zengine does not set,
  invent or look for it.
- **`outcome::kSucceeded` IS `exit 0 AND THE FILE IS THERE`, IN THAT ORDER**, and the order is
  the whole stale-artifact guarantee: a failed build is FAILED whatever is sitting at the
  destination, so an artifact left by an earlier success can never satisfy the current
  operation. A green build with its artifact absent is `kNoArtifact` — its own value, because
  it is its own problem and folding it into either neighbour tells a weaver something false.
  ⚠ There is no scan, no newest-file rule and no "there is one DLL".
- **THE SEAM IS TWO SHAPES AND ONE NEW GRANT, AND THE RELOAD MAKES IT TWO GRANTS.** The tool may say `OfferArtifact` — ONLY when
  the weaver asked for realization, because the shape carries an INTENT and a standing offer
  nobody made is not one.
  It is a COMMAND in this vocabulary's own table — an OFFER,
  not an order, because every eligibility rule and every refusal is the realization owner's.
  ⚠ THE ANNOUNCED PATH IS NOT USED: the owner resolves a stem with the HOST's rule, so a
  message naming a path cannot redirect a load. The dangerous grant in this process is still
  exactly one and it is still the booter's.
- **WORKSHOP'S JOURNAL KEEPS A BUILD'S ENDINGS AND NOTHING IT SAYS AS IT RUNS.** One value,
  `durable_log_selection` in `workshop/log_selection.hpp`, is what the host's durable log keeps:
  Loom's `default_selection()` untouched, refusals only under `--log-refusals`, and
  `kDurableBuildShapes` -- `BuildFinished`, `BuildNotStarted`, `ArtifactRealized` and Loom's
  `zen.AskEnded`, so a build its runner never answered is kept too -- uncapped, with their whole
  payloads. Never `BuildStatus`, republished on every chunk, nor `BuildOutput`.
  So `ArtifactRealized` is where a realize refusal that a row and a notice both cut is read whole.

## A pending row is a BARRIER, not a hole

**Authored plan order IS realization order.** A walk that skips a row it cannot perform has
silently replaced that with ELIGIBILITY order — whatever happened to be on disk goes first.

```text
row N is waiting on the weaver
    -> the row is `pending`, the owner is `Waiting`, and it RETURNS TO THE HOST
    -> row N+1 is NOT reached, NOT mounted, NOT loaded, and the host is NOT asked about it
    -> realize(N) settles it  ->  the frontier moves ON BY ONE  ->  the walk resumes
```

- **A ROW MAY BE `pending`, AND IT IS THE HOST THAT SAYS SO.** The owner has ONE
  predicate (`AwaitingBuild`) and learns nothing else: the host answers from two facts that
  are its own — the artifact file is absent, and some authored recipe produces that stem.
  ⚠ THIS IS NOT "SKIP WHAT IS MISSING": an artifact nothing here can build still refuses the
  plan by name. It is not build-on-missing either; nothing starts, requests or remembers a
  build.
- **⚠ AND STILL NOTHING LOOKS AT A DISK.** A frontier that STOPS makes *is the file there
  yet?* very tempting; it is the HOST's question, asked once per row through `AwaitingBuild`
  and never polled.
  Plain Build and Build & Realize stay two
  gestures; a plain build leaves the row `pending` with the file on disk until somebody asks.
- **`Realization` HAS `Waiting`, AND `Complete` MEANS COMPLETE.** `Complete` is reachable
  from exactly one place — the walk running off the END of the plan — and the walk passes a
  row only by performing it or, authored optional, by recording it `unavailable`. So
  `outcome().ok` cannot read *the whole arrangement is live* while an authored artifact is
  untouched, and `unavailable` names every optional row that is not live. ⚠ TWO SUBJECTS, TWO WORDS, on
  purpose: the OWNER is `Waiting`, the ROW it is waiting on is `pending`. Collapsing them
  leaves no way to say *which* row.
- **THE HOST IS TOLD AT EVERY REST, NOT ONCE.** `Settled` fires at three resting points —
  every row settled (resolved, or optional and unavailable), a required row refused, and the
  walk stopped at a waiting row — because all three are moments realization will not move
  again on its own, and the third is the one a weaver has to act on. A host tells them apart
  from the value alone: `ok`, or `refusal` non-empty, or `waiting_on` non-empty, with
  `unavailable` beside any of them. ⚠ `!done.ok` IS NOT A REFUSAL; test `refusal` before
  calling anything failed, or a host prints *project refused:* with no reason and exits 4 on
  a healthy run.
- **`realize(stem)` PERFORMS ONE WAITING ROW, AND EVERY ELIGIBILITY RULE IS THE PLAN'S.**
  Busy, already resolved, not named by the plan, or **not the frontier row** — each refused in
  words. ⚠ THE ALREADY-RESOLVED ARM IS THE RELOAD IN PLACE: a live weave-only row
  is reloaded from its rebuilt product through `zen.ReloadWeave` — the host stages the image
  off the loaded path (`StageArtifact`), the offer brackets it, the booter settles it, the row
  is `reloading` meanwhile and `resolved` after, and a kernel refusal is said in a weaver's words
  with the incumbent untouched; a provider+weave row is refused in words. ⚠ AN ON-DEMAND
  REFUSAL MUST NOT SET `Realization::Failed`: that is what the
  host's settle notice reads to end the process with exit 4, and a weaver whose hand-asked
  realization was refused has not lost the Workshop they are working in. The row goes back to
  waiting — the frontier returns to exactly where the ask found it, which is what makes a
  corrected build a RETRY rather than a restart.
- **A LATER ARTIFACT MAY BE BUILT EARLY AND NOT REALIZED EARLY.** Builder owns building and a
  recipe it exposes is a recipe a weaver may run; the file appearing changes nothing about
  order. Asking to realize it is refused BY THE NAME OF THE ROW IT IS BEHIND — and an
  ineligible ask moves nothing at all, least of all to `Failed`. When the frontier eventually
  reaches that row, the ordinary path finds the file and proceeds: there is no `prebuilt`
  state and must not be.
- **Row states are eight tokens, each with its owner** — `authored`, `loading`, `pending`,
  `resolved`, `refused`, then `reloading` (a reload in place), `switched` (a role moved to
  another authored choice) and `unavailable` (an optional row stepped over) — replacing a bool
  under which a row nobody had reached, a row in flight and a row that REFUSED were
  indistinguishable. ⚠ The Arrangement and Project projections carry `unavailable` with its
  reason; reading such a row as `authored` would tell a weaver nothing had tried.
  ⚠ A `loading` row publishes NO resolved field, even though
  its provider may already be mounted: within one row the mount precedes the load, and what came
  of the ROW is undecided, because a refusal rolls that mount back. The discovery door
  (`FindPowers`) reads the live catalog and shows it immediately: two questions, two owners, two
  currencies.

## The frontier is visible and actionable, and gained no authority

The Builder pane shows the waiting frontier and `f` builds-and-realizes it, and the whole
feature is one read-only seam plus one gesture over the existing route.

⚠ THE PANE IS A LOADED WEAVE (`builder/builder-pane/`), so the seam below is a message
boundary: the pane calls no host function, it ASKS the read-only project role
(`ProjectFrontierRequested` -> `ProjectFrontierSaid`, `workshop/builder_seam_vocabulary.hpp`),
and `ProjectDoor` answers out of the owner's claim the bullets below describe.

- **THE OWNER PROJECTS ITS OWN FRONTIER.** `Owner::behind()` joined `waiting_on()` —
  how many authored rows are behind the waiting row, derived from the same cursor, 0 in every
  non-waiting state. Both are in the owner's claim of where the plan is (`PlanStatus`), which
  the desk and the project door read at every ask into a by-value
  `ProjectFrontier{waiting, artifact, blocked}` (panes.hpp), stored NOWHERE on this side. ⚠ No copy exists on the host's path — a
  `ProjectFrontier` member, session field, or answer cached between asks is the mirror this
  seam exists to refuse.
- **`f` SPENDS THE EXISTING ROUTE, WHOLE.**
  One send (`BuildRequested`), same role, same grant,
  after the one class `build` ask the gesture's first beat made (`agents/workshop/guests.md`,
  WL-GUEST-03), carried to it and never asked again; everything downstream is the tool's, the
  runner's, and the owner's, unchanged. There is no
  second build path and no direct load, and a source tripwire beside the introspection suite's
  pins it: no presentation source under `workshop/` — `weave.hpp`, `screen.hpp`, `panes.hpp`,
  the seam vocabularies and the subject `.cpp` files beside them, walked by `presentation_sources`
  rather than listed — spells `realization/owner.hpp`, `realization::Owner`, `OfferArtifact`, `RunBuild` or
  `kBuildRunnerRole`, and a second tripwire reads the pane's own image under the same names.
- **⚠ SEVERAL RECIPES MAY PRODUCE ONE ARTIFACT, AND THE GESTURE NEVER CHOOSES.** That
  cardinality is authored law (`builder::check_recipes` deduplicates IDENTITIES, deliberately
  not artifacts, and a case pins the acceptance). With several matches `f` refuses and names
  them; what it may spend is the weaver's own standing pick — `picked`, the recipe `c` named,
  written ONLY by `c`, standing only while that recipe is still the choice, and released when a
  catalog arrival no longer holds it — because the catalog's own first row is nobody's choice,
  and neither is a choice Edit Code or `f` itself moved.
- **NO AUTOMATIC ANYTHING, STILL.** The view is a reading, not a power: encountering a
  buildable missing frontier starts nothing, plain `b` still leaves the row `pending` with
  the file on disk, and the weaver gesture remains the only way a compiler starts.

## Do not assume

- A plan row is configuration — it is an **execution-authority decision**: a provider row
  lets a native artifact contribute executable semantic power to the host, and a weave row
  lets it participate under a role. Nothing signs or restricts one.
- The load plan solves replacement — it does **not**. It is initial and restart load intent;
  a reload in place keeps every row as authored and is same shapes only, the
  provider-vs-`reload_from` interaction is still open (a provider+weave row is refused), and
  the executor has no unload, replace or remount path.
- Restart persistence means a clean build recreates the artifacts — it means a fresh PROCESS
  reconstructs the same arrangement from the same file.
- A recipe and a plan row are two views of one thing — they are two truths with two owners,
  joined by the artifact stem and nothing else. A role, a mount mode or a load order in a
  recipe, or a source path, package prefix or build tree in a plan, is the duplication this
  split exists to refuse.
- Zengine compiles the single-source route — CMake does. Zengine writes two small files and
  starts one `cmake -P`; nothing in this repository names a compiler, a flag, a library or an
  output suffix for it.
- A `pending` row means the artifact is missing — it means REALIZATION DID NOTHING, at the
  host's word. Whether a file is absent is the host's fact and whether a build is running is
  the Builder's; the projection publishes what realization did.
- A successful build loads its artifact — it does **not**. It offers one fact, and only when
  a weaver asked; the realization owner decides, bounded by the authored plan, and an
  already-loaded weave-only artifact is reloaded in place, same shapes only. There is no
  replacement here.
