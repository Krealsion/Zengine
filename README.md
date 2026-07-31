# Zengine

Zen's **default set** — the engine-shaped weaves most projects will want, built on the Loom.

## The three tiers

**The Loom is everyone's, Zengine is the default set, your weaves are yours.**

The Loom is the substrate: values, schemas, the gate, the switchboard. Zengine is one
opinionated set of weaves built on it — a set you can take, replace piece by piece, or ignore
entirely. Your own weaves sit alongside Zengine's as peers, not as plugins into it.

This repository is the middle tier, and it is **separate from the Loom on purpose**. Zengine
consumes the Loom exactly as a stranger would, so every rough edge in the Loom's public surface
hits the house before it hits a guest. The dependency arrow is structurally un-invertible: the
Loom's build cannot see Zengine.

## How this consumes the Loom

**Default — the stranger's path** (`ZEN_LOOM_DEV=OFF`): `find_package(loom)` against an
installed, exported Loom, consumed exactly as a third party would. Two steps, because a
stranger cannot skip the install:

```sh
# in Zen/Loom — build and install the Loom
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j"$(nproc)"
cmake --install build --prefix "$PWD/build/_install"

# in Zen/Zengine — consume it
cmake -S . -B build -DCMAKE_PREFIX_PATH="$PWD/../Loom/build/_install"
cmake --build build -j"$(nproc)"
ctest --test-dir build
```

**The dev override — the sibling path** (`-DZEN_LOOM_DEV=ON`): `add_subdirectory(../Loom)`,
for editing the two trees together without an install round-trip:

```sh
# Linux / WSL
cmake -S . -B build-dev -DZEN_LOOM_DEV=ON
cmake --build build-dev -j"$(nproc)"
ctest --test-dir build-dev
```

On **Windows** (MinGW), the dev override is also the turnkey path, because it is the one that
brings a kernel: dev mode defaults `LOOM_ENABLE_WINDOWS_KERNEL=ON` — the Loom's explicit
development/demo backend (**no isolation**; the Loom prints its banner and
`Kernel::containment_note()` says so) — so the snake package and its suite build and run
natively. Pass `-DLOOM_ENABLE_WINDOWS_KERNEL=OFF` to decline. The MinGW runtime DLLs must be
on `PATH` (or beside the binaries) to run.

```powershell
# Windows / MinGW — one flag more than it used to be, on purpose
cmake -S . -B build-win -G Ninja -DZEN_LOOM_DEV=ON
cmake --build build-win
ctest --test-dir build-win
```

Both paths expose the **same target names** (`loom::core`, `loom::switchboard`,
`loom::kernel`) — the Loom's export sets `EXPORT_NAME` to match its in-tree aliases — so the
override is a genuine drop-in and the two paths cannot silently come to mean different things.

**Why the stranger's path is the default** (a decision, re-affirmed 2026-07-27): it is what
makes a mistaken dependency on an unexported target (the UI trio, the console, the TUI, the
bridge, the SDL skin — all Zengine-destined, each moving in its own port phase) fail on
*every* developer's machine, not only in the verification lanes. Dev mode reaches the whole
Loom build tree and is silent about that entire class of mistake. The default was ON for one
experiment's length; the trust gate found it had drifted there without a decision, so the
discipline is back where it cannot be forgotten — and dev mode stays exactly one flag away.

## `reference/` — the read-only quarry

`reference/` holds the **V1 Zen engine**: a quarry to read, not a codebase to port. Nothing in
it is built by this repo. Ports are read-and-rewrite, phase by phase, landing in their proper
home from birth — never a lift-and-shift.

*Provenance:* a plain file import of the V1 repo's **working tree** (taken 2026-07-18, including
changes uncommitted there at the time), not a history-carrying subtree split. Its git history
stays in that original working copy (`G:\dev\BloodRush\Zen`) and is deliberately not carried
here — note its `origin` points at the *same* GitHub project as the Loom's, which is a further
reason the import drops `.git` rather than nesting a second repo under `reference/`. Dropped at
import: the 206 MB Python virtualenv, the derived `all_code.txt` concatenation, and
editor/agent cruft. No CMake `CompilerId` junk existed to drop.

## Test discipline

**Per-repo green.** Zengine's lane runs Zengine's tests against its pinned/installed Loom and
**does not re-run the Loom's suite** — a dependency's proof rides its version. Every report-back
states *which repo's green was proven*; "green" must never silently mean "green in one of two."

*Honest today-note:* the Loom is still under active development, so during Loom phases its
delegated-scope suite still runs there, per phase. The don't-re-prove economy arrives as the
Loom stabilizes; the structure is ready for it now.

Zengine's green is six tests: the **smoke** (link the Loom's exported surface, drive a value
through the real gate, confirm the gate **refuses** a malformed candidate — the refusal is what
makes it a proof instead of a greeting), the **snake suite** (`tests/test_snake.cpp`) — the
Stage 2 vertical slice proven headless: the locked contract pinned by content-id, the simulation
and the v1→v2 migration as pure math, the three live-evolution moments driven end to end
through real `.so` weaves, the real kernel, and the real Weave Manager, and the phase's negative
space (a skinless game writes **zero bytes** to stdout, with a painted-bytes negative control) —
the **timer suite** (`tests/test_timer.cpp`): the Timer package's contract by content-id, every
schedule (one-shot, repeat, upsert, clamps, cancels, role succession, honest vacancy, the
dead-requester floor) over a fake clock through a real bus, the activation law (a chain authored
from an activation; premature, duplicate, foreign, stale and replayed beats establishing
nothing), the real `.so` authoring and re-seeding its chain on the real clock, the load-order
matrix, the migration chains — the world ticking, the input weave polling, and the skin
servicing its medium with nobody pumping them — and (R2B-0) the **continuity lane**: the letter
and the order model pinned as units, then proven end to end through real libraries, the real
kernel, the real steward and a real graceful replacement, over a *virtual* clock so that "a
five-second one-shot had two seconds remaining, and the successor fired it two seconds later"
is an exact integer nobody had to sleep for — the **input suite**
(`tests/test_input.cpp`): the Input package's locked contract and SDL-scancode identity pinned
by content-id and literal value, both backends' translations as pure math on every lane, the
weave's publish path and its self-arranged beat through a real bus, and the keys-become-turns
chain through the real libraries — and the **surface suite** (`tests/test_surface.cpp`): the
Surface package's contract by content-id, the terminal skins as golden bytes, the SDL skin's
frame plan as pure math on every lane, the hello handshake (which now also asks for the skin's
beat) and the one-owner rule through the real kernel, the granted-operator speaking recipe, and
(where built) the SDL skin driven by the same intent under SDL's dummy video driver — and the
**trust-gate probes** (`tests/test_audit_probes.cpp`): a different KIND of suite, kept
deliberately. It pins what the substrate measurably does to a live beat chain when the timer
service itself is swapped, reloaded, double-wound, or joined late — *including where that was
unwanted*. Read its header before changing it. All four probes were flipped by R2A-2 into
witnesses of the earned promise while keeping every measured half: probe A still asserts that a
swap kills the incumbent's parked beat (`CapabilityDenied`, sender-death), and now also that the
activated successor authors a new chain. It deliberately measures the **hard** path; since
R2B-0 the service converses about its own succession, so the graceful path — and the continuity
it buys — is the timer suite's.

## `timer/` — the Timer package

Time, message-shaped. Games and packages do not read the OS clock and do not sleep: a weave
that wants time ASKS — `StartTimer{id, delay_ms, repeat}` (fired back to the asker) or
`StartRoleTimer{…, role}` (fired to whoever holds the role at each firing — the beat that
survives its starter being swapped) — and time ARRIVES as `TimerFired{id}`, delivered by the
**TimerService** (`zengine-timer`, holding `zengine.timer`). `CancelTimer`/`CancelAllMyTimers`
cancel; re-asking with the same id **replaces** the schedule (an upsert — also how a cadence
changes, and how a successor takes over a role beat instead of doubling it).

The service is the one place in the running system that owns a monotonic clock and the one
nap. It runs on its own **beat chain**, and since R2A-2 that chain is **authored, not
inherited**:

> Every successfully activated Timer incarnation establishes exactly **one** beat chain. A new
> activation owns a new chain; stale, duplicate, replayed, inherited or foreign `Drive`s cannot
> establish another.

**The host does not wind the clock** — it contributes nothing to time, not even a first breath.
Loading the service is what starts it: the Loom's control door sends the freshly committed
incarnation one `zen.Activated{sequence}`, and the service answers by publishing `TimerReady`
and seeding `Drive` serial 0. Each valid beat naps to the soonest deadline (capped at 10ms),
fires what came due, and seeds exactly its one successor. Pumping the bus IS running the world,
paced by the one legal sleep.

### Activation is now ATTESTED, not merely stamped (R2B-1)

> Activation identity is no longer inferred from an arbitrary stamped sender. The activation
> cursor accepts only **Loom-attested** activation, and then applies sequence replay protection.

Every Zengine package that arranges its own time does so on `zen.Activated`. R2A-1 gave that
fact a narrow meaning and a stamped sender; it could not answer the next question — *was that
sender authorized to announce a lifecycle commit at all?* Any weave granted the public shape
could manufacture a first breath for someone else's incarnation, and a consumer had no way to
tell. `zengine::ActivationCursor::accept(mail, activated)` now owns both halves, once, so no
author rediscovers the rule:

1. **Provenance** — `mail.lifecycle_attested()` is a *delivery fact* the bus sets and no payload
   can carry, bound by Loom to the incarnation being delivered to; and the sequence Loom
   attested must equal the one the payload states, so a proof minted for one activation cannot
   authenticate another.
2. **Lineage** — then the old rules, unchanged: positive, and newer-per-sender, so a duplicate
   or replay makes nothing happen twice.

An unattested `zen.Activated` is not a weaker lineage — it is **not a lineage at all**, and is
ignored entirely. All four consumers (Timer, Input, Skin, SnakeClock, plus the suites' probes)
go through this one call; replacing the cursor's signature rather than adding an overload is
what made the compiler enumerate the audit instead of a human trying to remember it.

Honest limits, kept out loud: this proves *Loom* authorized the commit, not that a particular
host wiring is the right one — the host decides who holds the lifecycle authority, and a host
that hands it to two operators has two lineages by its own choice. And it does not cross a
process boundary: an out-of-process weave receives no attestation and therefore accepts no
activation, failing closed at the seam.

`Drive` is **v2** and carries its own ownership — `{activation_sender, activation_sequence,
serial}` — so a beat is acted on only when the service is activated, the key names the
activation it is living under, the serial is the one expected, and the stamped sender is the
chain's own. The sender travels as canonical decimal **Text**: a `WeaveId` is unsigned 64-bit
and the wire's `Int` is signed, so an Int field would silently narrow it (the same spelling the
kernel's control door already uses to answer a load).

`TimerReady` remains, with a changed job: *"the Timer service has accepted an activation and is
available; re-establish the timers you require."* It is published **once per accepted
activation**, and it is no longer anyone's only first breath — every package arranges its own
time on its own `zen.Activated`. What it still covers is the opposite load order (a consumer
loaded *before* the service, whose ask went nowhere — rejected at the library/schema seam,
since with no service present nobody accepts the shape and it is never registered, so it does
not even reach role resolution) and the service's own
succession (a new incarnation's private schedule table is empty, and this is what refills it).

Honest V1 edges, pinned in the suite: the service cannot SEE a requester die (a weave gets no
delivery outcomes and the bus broadcasts no unloads), so a dead requester's directed timer
fires into clean `NoSuchTarget` refusals until cancelled or the service is replaced — weave
ids are never reused, so it can never hit a stranger; the standing heartbeats that must
survive replacement are role-addressed instead, where requester death is a non-event.

**The service's own succession** (`tests/test_audit_probes.cpp`, the four Trust-Gate probes —
written in the 2026-07-26 audit as measurements of unwanted behaviour, and flipped by R2A-2 into
witnesses of the earned promise). The substrate behaviour they measured has **not changed**:
`zen.SwapWeave` still kills the incumbent's parked beat, refused `CapabilityDenied` once the
incumbent is unregistered (sender-death, not role vacancy), and the probe still asserts exactly
that. What changed is that it no longer matters — the successor is *activated* on the way in and
authors a chain of its own, so the old chain dies honestly and time continues. `zen.ReloadWeave`
likewise no longer *inherits*: the predecessor's parked beat reaches the new instance and is
inert (a fresh incarnation begins unactivated, and the serial is not one it expects), and the
reload's own activation publishes the next `TimerReady` and seeds a fresh chain. A replayed
`Drive` — even from the real stamped Timer sender, with the live key — and a replayed activation
both establish nothing. A consumer loaded long after time started is no longer deaf: its own
activation makes it ask.

### The timer binding — one word for a sentence everybody was writing

R2A-2 made the raw Timer conversation correct and, in doing so, made its ceremony visible.
Every consumer that wanted a heartbeat wrote the same seven steps: accept `zen.Activated`,
deduplicate it, accept `TimerReady`, send the ask, accept `TimerFired`, filter by id, and re-ask
whenever the service appeared or came back. None of that is a domain decision, so it is package
vocabulary now (`timer/binding.hpp`):

```cpp
class SnakeClock : public timer::TimedWeave<SnakeClock, ClockState,
                                            loom::Accept<>, loom::Emit<SnakeTick>> {
public:
    SnakeClock() : tick_(timers().repeat("snake.tick", 120ms, &SnakeClock::on_tick)) {}
private:
    void on_tick(const timer::TimerFired&, loom::Mail& mail) {
        ++state_.ticks;
        mail.send_to_role(kWorldRole, SnakeTick{});
    }
    Handle tick_;
};
```

**A binding is desired local state.** `timers().repeat(...)` records what this incarnation
wants; it sends nothing. There is no `Mail` during construction and there may be no Timer in
the process at all. The binding is *reconciled* later, while handling an ordinary message — on
an accepted activation, and again on every `TimerReady`. Reconciliation was
**cardinality-idempotent but never timing-neutral**: the upsert keys mean a re-ask never doubles
a beat, but the raw asks replaced and *re-anchored* the schedule, so a binding reconciled
mid-cycle silently lost the remainder of that cycle. R2B-0 is what lets a binding say it would
rather not — it re-asks with an **order** (see *Continuity* below), so where there is a matching
schedule to preserve a re-ask now costs nothing at all, and where there is not it restarts and
*says so* in a receipt.

**Both addressing modes stay visible**, as different names rather than an inferred overload:
`repeat`/`once` are requester-addressed (`StartTimer`), `repeat_to_role`/`once_to_role` are
role-addressed (`StartRoleTimer`) — different promises about who hears the beat and who may
cancel it. Dispatch is by exact id to exactly one callback; an id nobody declared is data, not
a drive. Duplicate local ids are **refused at declaration** (a firing carries only an id, so
two bindings sharing one could not be told apart — a programmer error, taking the project's
established path for one).

**The convenience hides ceremony from the author, never the conversation from Loom.** A bound
weave's manifest carries the whole Timer protocol it speaks — `zen.Activated`/`TimerReady`/
`TimerFired`/`TimerResolution` accepted, `EnsureTimer`/`EnsureRoleTimer`/`CancelTimer` emitted —
and its grant is derived from exactly that. No wildcard acceptance, no `allow_any`, no
undeclared emission, no host-root send, no Switchboard reach. Since R2B-0 the binding speaks the
**ordered** forms, so its manifest says so and stops claiming the raw ones; the raw protocol
stays public and unchanged for a weave that wants restart/upsert with no negotiation.

**Cancellation is both halves** — the binding stops being wanted locally *and* `CancelTimer` is
sent — so a later `TimerReady` does not resurrect it; `restart(mail)` wants it again. A handle's
**destructor claims nothing remote**: during teardown there may be no valid `Mail`, and
dead-requester cleanup is still open. It clears local bookkeeping only, and says so.

Adapter weaves remain the right answer where time-to-domain translation is replaceable policy:
`snake-clock` is still a weave (a pause driver, slow-motion clock or replay feeder can take its
slot) — only its ceremony left.

**The one line of ceremony, and the hole it did not cover.** `WeaveBase` dispatches by calling
`self->on(shape, mail)` on the *derived* type, and a derived `on` hides every base one — so an
author with handlers of their own writes `using TimedWeave::on;`. Forgetting it is a hard
compile error, and that much was always true. What it did **not** cover is a derived handler
with the *same signature*:

```cpp
using TimedWeave::on;
void on(const loom::Activated&, loom::Mail&) { /* domain work */ }   // NO
```

The language excludes a base declaration from a using-declaration's set when the derived class
declares the same parameter list, so this did not even ambiguate — it silently became the
dispatch target. The bindings were never reconciled, no Timer order was ever sent, and nothing
complained at compile time or run time. The weave activated, the author's code ran, and time
never started.

> **A derived weave may extend Timer activation. It may never accidentally replace it.**

So the raw `on(zen.Activated)` handler is the binding's alone, and redefining it is now a
**compile-time refusal that names the alternative** — checked by asking which class owns the
activation handler dispatch would select, not whether one is callable (a derived one is
perfectly callable, which is exactly the danger). The alternative is one optional hook:

```cpp
class ActivationAware : public timer::TimedWeave<ActivationAware, State,
                                                 loom::Accept<>, loom::Emit<Ready>> {
public:
    ActivationAware() : tick_(timers().repeat("aware.tick", 10ms, &ActivationAware::on_tick)) {}

    /// Runs AFTER the bindings reconciled, and only for an accepted activation.
    void on_timed_activation(const loom::Activated&, loom::Mail& mail) {
        mail.publish(Ready{});          // ordinary domain work, ordinary Mail
    }
private:
    void on_tick(const timer::TimerFired&, loom::Mail&) { ++state_.ticks; }
    Handle tick_;
};
```

The rules, all pinned: the hook runs **after** every waiting binding was reconciled, so it may
assume its timers are ordered; it does **not** run for an unattested, duplicate, replayed, stale
or foreign activation, because the cursor decided that once, above it; `TimerReady` reconciles
the bindings and **never** invokes the hook, because the service becoming available is not this
weave's activation; and it adds **nothing** to the manifest — every shape it sends must already
be in the weave's own `Emit<...>`. A weave that defines no hook is untouched: the call is
`if constexpr`-guarded, so it is not compiled at all, and there is no virtual, no stored
callback and no cost. The hook must be **public** — a private one cannot be told apart from an
absent one, so it would be silently skipped; a wrong *signature* is caught and named.

### Continuity — what survives when the Timer dies (R2B-0)

> **Death is universal. Inheritance is authored.**

The Loom supplies the replacement moment and carries the envelope. What crosses is this
package's own decision, and making it required naming the three different kinds of state a
Timer holds:

| | what it is | who owns it | does it cross? |
|---|---|---|---|
| **Intent** | which timer a consumer wants (id, delay, repeat, addressing) | the consumer | no — it is re-declared on the consumer's own activation |
| **Progress** | how far a schedule has advanced: the remaining duration | the service | **yes** — this is what the letter is for |
| **Binding lifecycle** | waiting / spent / canceled | the consumer incarnation | no — see the boundary below |

Progress is the only one a re-ask cannot reconstruct, so it is the only one that travels.

**The first package-local order model.** A consumer now says what it would *prefer* and what it
will *settle for*, and the Timer answers with what it actually did:

```text
request  ->  available menu  ->  resolved choice  ->  receipt
```

`EnsureTimer{id, delay_ms, repeat, preferred, fallback}` and `EnsureRoleTimer{…, role, …}` are
**new** shapes rather than fields on `StartTimer` — a frozen `(name, version)` keeps meaning
exactly what it meant, and the raw fire-and-forget vocabulary keeps its explicit restart/upsert
promise, unreinterpreted. The menu is three words: `preserve_remaining`, `restart_delay`,
`drop`. **Refusal is an outcome, not a menu choice** — an empty `fallback` means the preference
is required, and if it is unavailable the order is refused and *nothing* is created or changed.

`TimerResolution{id, resolved, reason}` goes to the stamped requester and states the RESULT, not
merely success: `preserved_remaining` / `restarted_delay` / `dropped` / `refused`, with a
self-contained reason a stranger or a console can read. It is an ordinary declared message, so a
tap sees both halves of every order; the binding consumes it and exposes it on the handle
(`resolution()`, `resolution_reason()`).

**Matching is defined and pinned.** A standing entry matches an order when it has the same
upsert key *and* the same schedule meaning — same repeat mode, same clamped delay. A changed
addressing mode has a different key by construction; a changed delay or repeat mode finds the
entry but not a match. Both resolve as *unavailable* and go to the fallback, because calling
either "preserved" would describe a schedule nobody asked for.

**The letter.** `TimerHandoff{entries}` — one bequest item, one shape, at most
`kMaxHandoffEntries` entries, through the ordinary `bequeath_item` / `claim_item` gate. Each
entry carries the requester as lossless decimal Text, the id, the role (empty = requester-
addressed), the declared delay, the repeat flag, and **`remaining_ms` rather than a due time**:
the successor's clock is a different monotonic epoch, so an absolute deadline would be a number
with no meaning there. The consequence, said plainly: **replacement downtime is paused.** Two
seconds left before the swap is two seconds left after it, whatever happened in between. That
is continuity of a *delay* and must never be described as preservation of a deadline.

A letter is **adopted whole or not at all**: over the bound, or one entry whose requester is not
canonical decimal, and nothing is taken. An honest predecessor cannot produce either, so such a
letter is untrusted input rather than a large truth. A handoff written to a *different version*
of the shape is answered by the gate itself, never by a label the reader trusted.

**"Whole" means the whole letter, and the letter is a bounded subset of the table.** A service
standing more than `kMaxHandoffEntries` timers offers continuity for the first
`kMaxHandoffEntries` in table order; **the rest are not offered continuity at all** — not
preserved, not restored, and not reported as missing. Their consumers meet an unavailable
preservation on their next ordered re-ask and fall back exactly as they would after a hard
replacement. Nothing here claims an arbitrarily large active table crosses completely.

**The claim is authenticated (R2B-1).** The heir reaches the steward BY ROLE — precisely because
it cannot know the steward's `WeaveId` — so it cannot pre-bind the answer's sender, and a shape
plus the (published) `kClaimCorrelation` is exactly what any weave holding the same grant can
also produce. For *this* letter that gap was load-bearing: a forged handoff names the identities
future firings are addressed to. So the Timer now requires three things of an answer: Loom's
word that this is the one authorized answer to the request it actually sent
(`Mail::answers_ask()`), its own correlation, and an open claim. The Manager answers through
`mail.answer(...)`, which only the incarnation the claim was delivered to can do, once. An
ordinary weave that knows the shape, the correlation, the role name and the handoff byte format
is pinned in the suite doing its best and inheriting nothing — before the genuine answer, after
it, and with the legitimate steward's own bytes replayed the ordinary way.

**Successor bootstrap — the ordering is the whole thing.**

```text
zen.Activated -> claim, by role, from the steward
              -> seed Drive 0                        [bootstrap]
   answer (Bequest | Refused) -> restore, or start fresh
              -> replay whatever arrived while deciding
              -> publish TimerReady
   two bootstrap beats with no answer -> there was no steward: start fresh, the same way
```

`TimerReady` **may not be published before that decision**, and the reason is mechanical: it is
what makes every standing consumer re-ask, and a consumer that re-asks before restoration finds
nothing to preserve and re-anchors — silently converting "two seconds left" into "five seconds
from now". The bootstrap is exactly **two queue turns**, derived from the bus's own FIFO
ordering rather than tuned, and it is a count of turns and not milliseconds: no wall-clock
timeout, no spin, and **no permanent dependency on a steward existing at all** (a direct
control-door load with no Manager reaches beat two unanswered and starts fresh — pinned).
Operations arriving during the window are held in a bounded list and replayed in arrival order
*after* the inheritance, so a fresh request always beats inherited state for the same key;
overflow is counted (`deferred_dropped`) *and* answered with a `refused` receipt.

**Binding lifecycle, honestly.** The old single `desired` bool answered two questions at once:
"should this be re-established?" and "has this already happened?". It is now `Waiting` / `Spent`
/ `Canceled`. A one-shot means **once per binding incarnation, unless explicitly restarted** —
and it is marked `Spent` *before* its callback runs, so a callback may deliberately
`restart(mail)` and have the last word. A spent or canceled binding does not reconcile, and no
Timer reload, replacement or availability notice resurrects it.

**The binding-incarnation boundary, stated directly:** the lifecycle state is local to the
CONSUMER incarnation. A Timer replacement does not reset a spent binding; a consumer reload or
replacement constructs a *new* binding incarnation which may declare the timer again.
Preserving binding lifecycle across consumer death is a future generated/package-authored
handoff seam, not something silently delivered here.

**What the three replacement paths actually do**, with nothing rounded up:

- **graceful swap** — the letter crosses; a matching re-ask resolves `preserved_remaining` and
  is not re-anchored; `TimerReady` comes only after restoration.
- **hard swap** — no letter exists. `preserve_remaining` is unavailable, the default order falls
  back to `restarted_delay` and says so; a required-preservation order is refused and creates
  nothing.
- **reload** — reload does **not** run the graceful ceremony, and the schedule table is
  deliberately not part of `TimerState`. For continuity purposes reload is a *fresh service*:
  the default order falls back to restart, required preservation refuses, and nothing here
  claims remaining-duration preservation. (Moving schedule progress into reload-transplanted
  state is a possible future design, recorded as one — never an accidental promise.)
- **initial load** — nothing to preserve; the default order resolves `restarted_delay`. Not an
  error: the first preparation of the timer.

The general pattern, worth naming and **not** worth promoting to Loom law:

> Every package meets the same common lifecycle moments, but authors its own menu of survivable
> state and acceptable degradation.

Still open, and named rather than implied: the Timer's private schedule table is not persisted
across a process restart, there is no dead-requester cleanup, the Skin is not migrated to the
binding (its `SurfaceReady` concerns are separate and it was left as the next consumer), and
activation carries **no trust anchor** — sender plus sequence gives lineage and deduplication,
not proof that the sender is the one true operator. **An absolute alarm is a distinct future
timer kind** whose intent is a *deadline*, not a delay; every shape here is relative, and an
alarm must not be approximated with relative-one-shot semantics (which pause across downtime by
construction — exactly what a deadline must not do).

Banked, not built: absolute deadline timers; generated binding-state handoff; a reusable
cross-package order vocabulary (the trigger is a *third* package wanting this same menu);
fork-time continuity using the same intent/progress distinction.

**Banked, not built — a future concurrency direction:** *many threads may think; one weave
speaks.* A future thought worker may read immutable snapshots, compute proposals, and queue
private message intents. Only the normal weave execution thread may stamp the sender, use
`Mail`, enter Loom, or mutate ordinary weave state. Nothing in this phase implements or
depends on that; it is recorded so the boundary is chosen deliberately when it arrives.

## `input/` — the Input package

The floor games sit on: exactly one Input weave (`zengine-input`, holding the `zengine.input`
role) is the sole producer of the five locked shapes — `KeyPressed`/`KeyReleased` (SDL scancodes
as the wire identity of a key; `name` is convenience, never authority), `MouseButton`,
`MouseMoved`, `MouseWheel` — and the only code that talks to the platform. Consumers only
accept; there is no polling API. Backends today are the ones snake runs on: the POSIX terminal
(raw mode; strokes synthesize press+release) and the Win32 console (real key transitions, mouse
records); an SDL **Reader** (the window's own input, including its close box) is the named
follow-on now that the Surface package gives it a window to read. The weave arranges its own
execution: on the TimerService's hello it asks for a repeating role-addressed beat
(`zengine.input.pump`, 10ms — the package owns its own pace now) and polls on each firing;
`PumpInput` stays as the same hands on direct request, for suites and timer-less hosts.

## `surface/` — the Surface package

Visual intent in, output out. No game, world, or panel weave talks to the terminal, a window,
or a renderer: they **publish** intent, and a **Skin** — a replaceable loadable weave holding
the singleton `zengine.skin` role — claims the actual surface and paints. Claiming is RAII
(the constructor takes the medium, the destructor gives it back; a swap is release-then-claim
because the Manager delivers the unload first), and ownership is enforced ground: loading a
second skin into the held role is a clean refusal.

The vocabulary is deliberately tiny: `SurfaceText{slot, text}` (a line of **plain** text for a
named slot — "status", "score"; styling is the skin's business) and `SurfaceReady` (the active
skin's hello, published once per incarnation on its first message; text publishers re-publish
their current line on hearing it, so a fresh painter starts complete — the tally line survives
the painter being replaced mid-game). `SnakeVisual` is the V1 canvas payload, accepted by the
skins directly — a named coupling; the general canvas vocabulary is a later phase.

Three skins ship: **`zengine-skin-tui-classic`** and **`zengine-skin-tui-block`** (the old
snake drawers' looks, now living where drawing lives — the terminal medium is one header,
golden-byte tested), and **`zengine-skin-sdl`** — a real window, same intent, zero
medium-specific fields added anywhere (the agnosticism proof). The SDL skin is the only
target that sees SDL: it fetches a **pinned static SDL3** where none is installed
(checksum in the build; `-DZENGINE_SDL_SKIN=OFF` declines), plans every frame as pure math
(`skin_sdl_plan.hpp`, pinned on every lane), and degrades gracefully with no display — the
suite drives it under SDL's dummy driver, and the window title carries the text slots.

The SDL window is **output-only in V1, structurally**: it is created not-focusable (a window
that cannot hear must not take the keys — the terminal stays the game's one ear until the SDL
Reader phase makes the window an ear too), and it keeps itself answering its OS: a skin's
own activation asks the Timer package for the `zengine.skin.pump` role beat (10ms), and the
beat services the window's event queue even when the world publishes nothing (a dead world
starves a frame-driven pump; the OS calls the result "not responding"). Role-addressed is the
load-bearing half: the beat belongs to the SLOT, so a swapped-in skin inherits it without
asking. Terminal media no-op the beat, exactly as they no-op'd the old host-sent pump;
`PumpSurface` stays as the same hands on direct request, for suites and timer-less hosts.

## `snake/` — the first game panel

The Stage 2 vertical slice: a playable snake whose parts are genuinely separate weaves.
Since the Surface migration, **snake contains no drawing code at all** — the suite pins a
skinless game at zero stdout bytes.

- **World** (`snake-world-v1`/`-v2`, one source) owns the simulation and holds
  `SnakeWorldState`; it publishes `SnakeVisual`/`FoodEaten`/`SnakeDied` and never knows its
  consumers. Both versions converse (`zen.PrepareShutdown` → a letter); v2 is additionally an
  heir — it claims by role on first wake and folds a v1 inheritance through `migrate()`.
  Untouched by the Surface migration — the whole ceremony reduction was consumer-side.
- **Score** (`snake-score`) accepts only `FoodEaten` and counts what it *witnesses* — loaded
  late into a live game, its count honestly differs from the world's. It publishes its tally
  as `SurfaceText` (and re-publishes on a skin's hello) instead of painting a row.
- **Controls** (`snake-controls`) is snake's input binding: it accepts the Input package's
  `KeyPressed` and turns WASD/arrows into `SnakeTurn`, sent to the world **by role** so
  steering survives the world being swapped mid-game. The binding is a weave, so it is
  replaceable like everything else (a remap, an AI pilot, a replay feeder).
- **Clock** (`snake-clock`) is snake's time binding — the controls move, pointed at time: it
  asks the Timer package for the 120ms repeating beat (`snake.tick`, the cadence the host
  used to hard-code, now living where the pace belongs) and turns each `TimerFired` into
  `SnakeTick`, sent to the world **by role** so time survives the world being swapped. The
  world never learns where ticks come from; only the source of time moved.
- **Host** (`zengine-snake`) owns the boot list — nothing else. It reads no keys (the Input
  weave produces them), owns no screen (the skin claims it at load), and since the Timer
  package keeps no clock, never sleeps, and pumps nobody. It contributes nothing to time
  either: it queues the boot list and `pump()` IS the game. Loading the timer service is what
  starts the clock — the control door activates it and it authors its own chain — so there is
  no wind, and no boot-pump-then-wind ceremony to order correctly. The loop ends when the
  operator's quit key stops the bus (or, honestly, when the bus goes quiet because no clock is
  deployed, or because activation could not establish time).
  Its status line is published intent like everything else, spoken by the granted operator
  weave that also sends every lifecycle command through the Weave Manager. Run it under WSL
  from the build tree; keys: wasd steer, `1` swap the TUI skin, `2` load score, `3` grow the
  world (graceful v1→v2 migration), `4` swap to the SDL skin (where deployed), `r` reload in
  place, `n` new game, `l` list, `q` quit.

This package is the **hosting consumer** that pulled `loom::kernel` onto the Loom's export
surface, and whose nested shapes surfaced (and pulled the completion of) the manifest's
documented-but-unbuilt `referenced` section (`zen.Manifest` v3). The snake targets gate on
`if(TARGET loom::kernel)`, so a Windows Loom install still configures — the package simply
skips.

## Working in this tree

Zengine and the Loom live side by side under a shared `Zen/` root:

```
Zen/
  Loom/        the substrate — everyone's
  Zengine/     this repo — the default set
  playground/  Josh's own weaves
```

Assistant sessions are launched from the **`Zen/` root**, never from inside a sub-repo: the
memory graph is keyed to that path, so launching from `Zen/Zengine/` silently cold-starts
without it. Run git per-repo (`git -C Loom …`, `git -C Zengine …`).
