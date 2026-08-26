# Agent law — Operators

Routed detail behind [`AGENTS.md`](../AGENTS.md), for tasks touching `operator/` — named
semantic rules, the catalog, the host/consumer seam, and providers. Public reference:
[`../docs/reference/operator-host.md`](../docs/reference/operator-host.md) and
[`../docs/reference/operator-providers.md`](../docs/reference/operator-providers.md). How a
host's load plan mounts providers is [`realization.md`](realization.md). Phase tags like
(SEM-0) are provenance markers into this repository's history; the law here is current.

## One semantic rule has an owner, and the Timer is not it (SEM-0)

What a Timer makes of an authored delay is `timer.normalize_delay` — a named operator, composed
from two published primitives, evaluated by one evaluator that every consumer comes through.

```text
timer.normalize_delay(delay_ms : Int, repeat : Bool) -> effective_delay : Int

    floor_zero = math.max(delay_ms, 0)
    floor_one  = math.max(floor_zero, 1)
    effective  = logic.select_int(repeat, floor_one, floor_zero)
```

- **An operator's SIGNATURE IS A PAIR OF `loom::Schema`s**, and that one decision is why
  `operator/` adds no type system. Every port's `TypeRef` comes from `loom::type_ref_for`, the
  table every `ZEN_SHAPE` field already goes through; the argument pack is admitted by
  `loom::admit`, so **no arity check is ever written** and a missing port is a `MissingField`;
  and a signature versions itself, because `Schema::content_id()` already is that number. A
  derived port schema is byte-for-byte the hand-built one, content id included, and the suite
  asserts it.
- **`make_operator<&fn>("id", {"lhs","rhs"}, "result")` is the whole registration.** Arity,
  every parameter type and the return type are the compiler's; the identity and the PORT NAMES
  are authored, because C++20 has no parameter source names at all — not in `decltype`, not in
  `__PRETTY_FUNCTION__`, not in `__FUNCSIG__`. A wrong NUMBER of port names does not compile
  (the parameter is a `std::array` sized by `arity_of<F>`), so that refusal cannot be a runtime
  case and is not written as one. ⚠ A block-scope lambda cannot be the `<&F>` argument — its
  `_FUN` has no linkage.
- **`timer.normalize_delay` carries no native body**, and `is_composite()` is a public question
  precisely so a suite can say so. A `normalize_delay(delay, repeat)` registered as a native
  operator would satisfy every other case in the operator suite and would prove only that
  registration works. Do not "simplify" it into one; and do not add `math.clamp`, which is the
  same mistake wearing a primitive's clothes.
- **Composition needs no generated C++ and no compiler.** A graph is data, evaluated directly.
  Compilation is what introducing a missing native PRIMITIVE costs, never what recomposing
  existing power costs.
- **Resolve at spend, never hold.** A node keeps an identity and the two `ContentId`s it was
  authored against, and nothing caches a resolved operator, an index or a callable. That is not
  a performance choice (the resolve measures ~7-10 ns against a ~590 ns evaluation) — it is
  what makes a disagreement between an executor and a previewer UNREPRESENTABLE. A recorded
  signature is also what lets a re-shaped step be reported as *found, and not what this was
  authored against* rather than as *missing*.
- **One store, read twice.** `Catalog::identities()` walks the same map `evaluate()` resolves
  through. Do not add a name list beside it.
- **Four failure modes, two owners.** Unresolved and signature-mismatch are the catalog's
  sentences; a bad argument pack and a bad answer are `loom::admit`'s, quoted verbatim. There
  is no operator error enum and none is wanted.
- **The substitution instrument is the canonicality proof.** The suite replaces `math.max`
  underneath and watches the running weave and an independent reader move together. Two
  implementations that merely agreed could not do that. The canary that reverts the Timer's
  delegation reddens exactly the path witnesses while the behaviour matrix stays green, which
  is the distinction: the witnesses are about the PATH, not the answer.
- **An operator's answer is RETURNED, not delivered.** It is not an `Emit<>`, no port schema is
  registered with a Switchboard, and a complete round trip runs with no bus in the process. Do
  not turn arithmetic into message traffic: a five-node DAG over messages is five sequential
  dispatch turns.
- **`op::invocations()` is `loom::gate_invocations()`'s sibling** and carries its caveats —
  process-wide, monotonic, read as a DELTA, decides nothing. It counts NATIVE bodies only, so
  the number does not move when a rule is refactored.
- **An ordinary helper stays an ordinary C++ function** until two or more surfaces need the
  same rule and would otherwise each hold a copy. This package is not a logic framework: the
  primitive vocabulary is deliberately minimal, and everything a future logic system will want
  — `min`, `clamp`, `and`, `or`, `greater_than`, a Float max — is deliberately absent.

## A loaded weave can spend the host's operators (OPH-0)

A dynamically loaded weave is built by `create(void)` with nothing and sees exactly one host
table for the rest of its life — Loom's `ZenHostApi` — and none of its doors is a callable.
`operator/host_abi.h` is the answer: a consumer image may OPTIONALLY export one symbol saying
*I can receive an operator host*, and a host about to load it offers a narrow C table with two
verbs, `describe` and `evaluate`.

- **`zengine-operator-consumer` is a package with no catalog in it**, and the split is the
  product claim rather than tidiness. A loaded consumer links it and NOT `zengine-operator`, so
  `op::Catalog`, `OperatorDef`, `make_operator` and the primitives are not merely unused there
  — they are not in that image at all. **Do not link `zengine-operator` into a loadable
  consumer**; that is the one edit that quietly gives it a private catalog to disagree with.
- **The offer BRACKETS the load and is then withdrawn**, and both halves matter. A weave's
  first legitimate need is inside `create()`, which the Kernel calls and no host can get
  between — and on the real path (`zen.LoadWeave` → Weave Manager → control door) the load is
  several deliveries deep. So `OperatorOffer` goes up before the command is sent and comes
  down after the answer. Its destructor offers `nullptr` unconditionally, which is what makes
  the module's slot empty outside one load and two instances of one image two separate
  handoffs rather than one durable module-wide binding. A canary that removes the withdrawal
  reddens exactly one case, and it is the one that asks.
- **The host opens the image itself, and that is the seam's one duplication.** Loom's
  `LoadedLibrary` is defined in its `.cpp` and `lib_symbol` has one caller for one name, so
  there is NO public door to a second exported symbol of a kernel-loaded image.
  `OperatorOffer` opens the same path with the same flags (`RTLD_NOW | RTLD_LOCAL` /
  `LoadLibraryA`), which refcounts to ONE image, and releases its share inside `load()` — so
  `kernel_lifetime_counts()` sees exactly what it would for a weave that never heard of
  operators. Do not turn that share into something the host keeps.
- **`detail::offered_host_slot()` is DECLARED in the header and DEFINED by the macro**, and it
  must stay that way. A `static` inside an inline function is vague-linkage: on ELF the host
  executable's copy interposes into an RTLD_LOCAL library and on PE it does not, so the same
  code would mean different things on the two platforms this project ships. The macro's
  non-inline definition emits it `STB_LOCAL` in exactly one image on both (`nm` says `b`, not
  `V`). A consumer that uses `OperatorHost` and forgets the macro gets a link error naming
  that function, which is the failure to want.
- **The version rides in a FIELD on both tables and is checked by both sides.** A suffixed
  symbol name would give a host one bit — the lookup failed — and an absent symbol already
  means *an ordinary weave, load it normally*. Two different facts must not arrive as the same
  silence. The host reads the consumer's number before it reads any other field, because every
  other field's meaning is what the version decides.
- **`Catalog::evaluate` has two entrances and one body.** The `loom::Unverified` overload
  exists so the seam does not admit for itself and then say, in its own words, what
  `catalog.hpp` already says about a refused pack. A case asserts the loaded consumer's refusal
  string is character-for-character an in-process caller's; two spellings of one refusal is how
  a caller and a callee stop meaning the same thing.
- **`describe` derives from the definition `evaluate` resolves.** There is no hand-written
  descriptor beside an operator and there must never be one — and the gate makes that
  self-enforcing: a canary that emits a plausible-but-different descriptor reddens the describe
  case AND every evaluation, because a pack built from a lying description is refused by the
  real input schema.
- **The suite does not depend on the artifacts it loads.** `$<TARGET_FILE:...>` in a
  `target_compile_definitions` is a path, not a build edge, so
  `cmake --build build --target zengine-operator-tests` will happily run last build's
  fixtures. Build the whole tree before believing a result — which the official lane does, and
  which a hand-run canary loop must be told to do.
- **The stranger fence is header-only honest.** `zengine-operator-stranger` is a static
  library rather than a source file in a suite, and the shape is the claim: the independent
  consumer links `zengine-operator` and nothing else. These are header-only packages, so no
  link line can stop a later edit from including sideways; what it says, checkably, is that
  the translation unit names no timer symbol and no timer string. OPH-0's stranger is the same
  idea in another IMAGE and its fence is stronger for it.

## One authoring is one live answer (CAT-0)

A process running the shipped Timer beside a loaded stranger must not hold two live catalogs
built from one authoring. A Zengine host owns ONE `op::Catalog`, and a Timer it boots inside an
`OperatorOffer` resolves `timer.normalize_delay` through that instance.

- **A Timer chooses ONE authority at construction and keeps it for life**
  (`timer::DelayAuthority`). HOST-BACKED holds a bound `op::OperatorHost` and the contract it
  described; LOCAL-FALLBACK holds an `op::Catalog`. **Exactly one of the two members is
  engaged**, and that is the enforcement rather than a comment asking for care: in host-backed
  mode there is no local catalog in the object for a later edit to reach. Do not add a setter,
  a rebind, or an opportunistic re-check per schedule — a Timer whose semantics depend on
  which load happened to be in flight is the one thing a scoped offer exists to prevent.
- **An offered host is CHECKED before it is accepted, and a failed check refuses the load.**
  The Timer describes `timer.normalize_delay` across the seam and compares both port schemas
  against the ones its own package authors (`loom::same_identity`; `Schema::content_id()`
  already versions a signature — do not invent a second hash). A host with no such rule, or
  with another signature, makes the constructor throw, `create()` return null, and
  `Kernel::load` refuse. **There is deliberately no path from a host that failed to a local
  evaluation**, and adding one is the single edit that reintroduces everything this law
  removed: a Timer that diverges from its host exactly when the host became inconsistent. The
  expectation is derived from `normalize_delay` inside the constructor — the same call this
  package's provider contribution is built from — and the scaffolding it needs dies at the
  closing brace; a host-backed Timer holds no catalog at all.
- **Fallback is a supported arrangement and is not warned at.** `zengine-snake` offers nothing
  and gets the Timer it always got; so does every host that predates the seam. Do not make the
  operator host a dependency of using a Timer. A Timer holds a catalog only in the no-host
  arrangement, and what it CONTRIBUTES is one composition rather than a vocabulary.
- **`ZEN_EXPORT_WEAVE` builds its weave with `new S()`, so an artifact that must take an offer
  needs a class of its own.** `timer/timer.cpp` and `tests/weavelib/timer_virtual.cpp` each
  derive a six-line `TimerService` whose only member is a constructor passing
  `DelayAuthority(OperatorHost::offered())` down. It adds no state and overrides nothing. The
  refusal sentence goes to **stderr** before the throw, because `create()`'s contract is a
  null pointer and a null pointer carries no reason — the same thing `zengine-skin-sdl` does
  for a failed init.
- **Do not write `ZENGINE_OPERATOR_CONSUMER()` in a test binary.**
  `detail::offered_host_slot()` has default visibility, so an executable that defined it would
  sit in the lookup scope of every `.so` that also defines it. A suite drives the consumer
  side with `op::OperatorHost::over(api)`, which is what that spelling is for. For the same
  reason `timer_weave.hpp` never touches the slot: only artifact sources do.
- **`zengine-timer` links BOTH operator targets and the pair is the point.** It SUPPLIES its
  delay composition (`zengine-operator`, the authoring surface and the contribution codec) and
  CONSUMES a host's runtime surface (`zengine-operator-consumer`). Provider and consumer are
  different roles and one artifact may be both; do not collapse them, and do not conclude a
  provider must be the thing that instantiates what it authors. Both targets are header-only
  INTERFACE targets: the Timer's `ldd` and PE import table are unchanged; its export table
  names THREE symbols (see providers, below).
- **The host's catalog is a local of `main`, and the DECLARATION ORDER is the lifetime claim**
  — `op::Catalog` (EMPTY; provider mounts fill it), then `OperatorHostSurface`, then
  `loom::Kernel`, so reverse-order destruction takes the Kernel and its artifacts down first.
  No static, no registry, no accessor. A case reads that ordering off the SOURCE FILE, because
  Workshop's `main()` claims a terminal and no case can run it — declared as a tripwire rather
  than a proof.
- **`Kernel::reload_from` is the OTHER `create()` site**, and an operator-aware host owes a
  reload the same bracket it owes a load. Both are pinned: bracketed keeps the binding,
  unbracketed comes back a fallback Timer. Nothing in this repository can take the unbracketed
  path — Workshop's boot weave may send `zen.LoadWeave` and nothing else — and closing it in
  general means a Kernel that can be TOLD an artifact must always be offered something, which
  is a loader question rather than an operator one.
- **A canonicality claim needs a substitution, not a matrix.** `sabotaged_operators()` is
  invisible to every structural check (same identity, ports, types, content ids), so a host
  built over it moves a host-backed Timer and a loaded stranger together and leaves a fallback
  Timer where it was. Observe the Timer through `TimerHandoffEntry.delay_ms` off a real
  `zen.Bequest` — **never** through `host_backed()`, which is a diagnostic and decides
  nothing. The same instrument exists as an ARTIFACT (`zengine-provider-min`), mounted over
  the basic provider at run time and unmounted again.

## Powers come from providers; the host owns resolution (PROV-0)

`operator/provider_abi.h` points the OPH-0 seam the other way: a loaded image may OPTIONALLY
export one symbol saying *I supply these operator definitions*, and a host mounts it.

```text
zengine-operators-basic    math.max, logic.select_int      NOT a weave
zengine-timer              timer.normalize_delay           weave + provider + consumer
host resolution            all three, layered, replaceable
```

- **A host authors NO operator and cannot.** Its catalog starts empty, it mounts artifacts,
  and it includes no semantic header. A case in `test_operator_provider.cpp` reads
  `workshop.cpp` for forbidden strings — declared as a tripwire rather than a proof, because
  Workshop's `main()` claims a terminal. **A host knows HOW to host operators; it does not
  know WHAT any of them means.** Do not reintroduce a semantic include there to save a mount.
  Which artifacts it mounts is the load plan's law ([`realization.md`](realization.md)).
- **A PROVIDER IS NOT A WEAVE.** `zengine-operators-basic` exports
  `zengine_operator_provider` and no `zen_weave_abi` at all: no Kernel loads it, it has no
  WeaveId, role, grant, manifest or bus, and the host opens it directly. Build one with
  **`zengine_provider()`**, not `zengine_weave()` — the difference on the link line is
  `loom::switchboard`, and its absence is the claim. Do not make `zen_weave_abi` semantically
  required just because a host wants operators.
- **A COMPOSITE CROSSES AS STRUCTURE, never as a callback.** `describe` emits the GRAPH; its
  nodes still say `math.max` on the far side and resolve against the host's current providers
  at every spend. `ProviderDefinitions::invoke` REFUSES a composite on purpose. If a
  composition were an opaque call back into its own image, a power replaced underneath could
  never propagate through it. Do not "optimise" that into a provider-side evaluation.
- **The provider-local INDEX is transient and is never an operator's meaning.** What is
  durable is the identity and the two port schemas; the index is how a host that is HOLDING
  the record reaches the code. `invoke_at(index)` costs what a raw function pointer costs
  (298.7 vs 300.2 ns, measured), so never export a callable accessor.
- **The hold is the whole `ProviderRecord`, not the image.** A native contribution's callable
  closes over the record, the record holds one `ImageShare`, and `Catalog::unmount` drops the
  CONTRIBUTIONS and only then the custody — so nothing that can call into an image outlives
  it, by refcount rather than by any statement ordering it. Mount and unmount only between
  evaluations; everything here is single-threaded.
- **`detail::provider_definitions()` is DECLARED in the header and DEFINED by the macro**, for
  `offered_host_slot`'s reason exactly (ELF interposition vs per-DLL PE statics). A provider
  that forgets `ZENGINE_OPERATOR_PROVIDER` gets a link error naming that function.
- **Shadowing is intentional; layering is not a set of duplicates.** An identity holds a STACK
  of contributions and `back()` is active. An ordinary second contribution to a taken identity
  REFUSES — load order, filesystem order and map iteration are not policy — and only
  `MountMode::Overlay` may cover one, only where both port schemas are `same_identity` with
  what is already there. Unmount REVEALS the same object underneath rather than rebuilding it;
  a case asserts the POINTER, because a rebuild would compare equal in every other way.
- **A mount is ALL OR NOTHING.** Every contribution in a batch is judged before any is
  installed, so a refusal leaves the catalog exactly as it was. A provider contributing zero
  is refused too: that is the only way a provider can say its own authoring threw.
- **`fallback_vocabulary()` is the no-host arrangement's LOCAL assembly**, and it is what
  keeps `zengine-snake` and every pre-seam host working without loading a second artifact. Do
  not call it from a host that has providers.
- **`op::image_counts()` is `kernel_lifetime_counts()`'s sibling** — process-wide, monotonic,
  read as a DELTA, decides nothing, and it counts THIS package's shares (an offer's, a
  mount's) and not the Kernel's. Like `op::invocations()` it is a vague-linkage static and is
  **not** a cross-image instrument; only a host opens an image for itself, and a host is an
  executable.
- **`Catalog::run` CONTAINS a native throw.** A native body may live in another image, so
  "the provider could not answer" is an evaluation's own refusal rather than an exception
  leaving a call whose contract is a value or a reason.

## Do not assume

- The delay a maker authors is the delay that is scheduled — it is normalized, and the rule is
  `timer.normalize_delay`. An `EnsureTimer` comparison runs the same rule, so `-500` repeating
  really is the standing 1 ms beat.
- `zengine-operator` is a place to put helpers — it is for rules TWO surfaces need. One
  consumer is a C++ function.
- A Timer evaluates the rule its own image carries — it does **only** where no host offered it
  one. In `zengine-workshop` the answer comes from the host's catalog, and substituting a
  primitive there changes what the Timer schedules.
- An offer covers an artifact — it covers **one load** of one image, and every `create()` the
  Kernel performs needs its own, `reload_from` included.
