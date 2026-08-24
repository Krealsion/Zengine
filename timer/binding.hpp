// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_TIMER_BINDING_HPP
#define ZENGINE_TIMER_BINDING_HPP

// The timer binding — one Zengine word for a sentence everybody was writing.
//
//   A weave DECLARES the time behaviour it wants; the binding owns the Timer
//   protocol and the lifecycle reconciliation required to maintain it.
//
// Without it, every consumer that wants a heartbeat writes the same seven
// steps: accept `zen.Activated`, deduplicate it, accept `TimerReady`, send a
// StartTimer/StartRoleTimer, accept `TimerFired`, filter by id, and re-ask
// whenever the service appears or comes back. That machinery is real and none
// of it is a domain decision — so it belongs in package vocabulary, written
// once, not in every author's file. The layer's contract is TIMER-05,
// docs/laws/timer-laws.md; the model and its boundary are
// docs/reference/timer-binding.md.
//
// WHAT THIS IS NOT. It is not a scheduler, not an event framework, and not a
// second interpretation of time. The raw Timer vocabulary stays public and
// unchanged; the Timer stays a separate weave; adapter weaves stay the right
// answer wherever time-to-domain translation is independently replaceable
// policy (snake-clock is exactly that, and survives this phase as a weave —
// only its ceremony left). Nothing here relays, buffers, or reinterprets a
// firing: the callback runs on the ordinary Loom execution thread, inside the
// ordinary handler, with the ordinary `Mail`.
//
// WHAT IT COSTS, said up front. A weave that mixes this in declares the WHOLE
// Timer protocol the binding can speak — `zen.Activated`, `TimerReady`,
// `TimerFired`, `TimerResolution` accepted; `EnsureTimer`, `EnsureRoleTimer`,
// `CancelTimer` emitted — even if it only uses one addressing mode. That is
// deliberate: the
// declaration says what this weave's code MAY say, the binding layer's code can
// say all three, and the manifest is the honest answer to "what conversation is
// this weave in?". The convenience hides ceremony from the author; it does not
// hide the conversation from Loom. Inspect any migrated weave and the Timer
// protocol is right there in its accept and emit sets.
//
// THE ONE LINE OF CEREMONY THAT REMAINS, and why it cannot go. `WeaveBase`
// dispatches by calling `self->on(shape, mail)` on the DERIVED type, and a
// derived class that declares any `on` overload HIDES every base-class `on`.
// So an author with their own handlers must write `using TimedWeave::on;`.
// Forgetting it is a hard compile error (never a silent miss), and there is a
// static_assert below that says so in words instead of template soup. Removing
// even that line would need a Loom change to how handlers are discovered, which
// this phase deliberately did not take.
//
// ...AND THE HOLE THAT LEFT, closed here. `using TimedWeave::on;` protects
// against ordinary name hiding — a derived handler for a DIFFERENT shape. It
// does nothing about a derived handler with the SAME signature:
//
//     using TimedWeave::on;
//     void on(const loom::Activated&, loom::Mail&) { /* domain work */ }
//
// [namespace.udecl] says a derived member with the same name and parameter list
// as one introduced by a using-declaration EXCLUDES the base declaration from
// the set. So this does not even ambiguate — it silently becomes the dispatch
// target, the bindings are never reconciled, no Timer order is ever sent, and
// nothing complains at compile time or at run time. The weave activates, the
// author's code runs, and time never starts.
//
//     A derived weave may EXTEND Timer activation. It may never accidentally
//     REPLACE it.
//
// Two things enforce that. The raw `on(zen.Activated)` handler stays the
// binding's alone and a derived redefinition is a compile-time refusal that
// names the alternative (see `activation_is_the_bindings` below). The
// alternative is one optional hook, `on_timed_activation`, which runs AFTER the
// bindings reconciled and only for an activation the cursor accepted.

#include "vocabulary.hpp"

#include "activation/activation.hpp"

#include <zen/weave.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace zengine::timer {

template <class Self>
class TimerBindings;

namespace detail {

/// WHICH CLASS OWNS THE ACTIVATION HANDLER DISPATCH WOULD SELECT.
///
/// Declared, never defined: it exists only to be deduced from inside `decltype`.
/// Passing `&Self::on` — an overload SET — is legal here precisely because the
/// parameter pattern matches exactly one of its members, so the compiler picks
/// that one and hands back the class that declared it. That is the whole trick:
/// "is this handler callable?" is the wrong question (a derived one is perfectly
/// callable), and "who does it belong to?" is the right one.
///
/// Returns `C*` rather than `C` so no weave type ever has to be returnable by
/// value for this to compile.
///
/// ⚠ THE PARAMETER TYPES MUST BE CONCRETE, AND ONLY `C` DEDUCED. Writing this
/// with a deduced `Activated`/`Mail` makes the pattern match EVERY
/// `on(const X&, loom::Mail&)` in the weave's overload set — the domain
/// handlers, `TimerReady`, `TimerFired`, all of them — so deduction goes
/// ambiguous, the requires-expression reports "not addressable", and the whole
/// check quietly abstains on exactly the classes it exists to refuse. The first
/// cut of this file did that and the collision fixture compiled clean; the
/// canary is what found it.
template <class C>
C* activation_owner(void (C::*)(const loom::Activated&, loom::Mail&));

} // namespace detail

/// Where one binding stands in this incarnation's life.
///
/// Three states and not a `desired` bool, because that bool answers two
/// different questions at once and gets one of them wrong. "Should
/// reconciliation re-establish this?" and "has this already happened?" are not
/// the same question, and a one-shot that fired must answer NO to the first for
/// a reason only the second explains.
enum class BindingState {
    Waiting,  ///< wanted, and not yet finished with
    Spent,    ///< a one-shot that has fired; done unless explicitly restarted
    Canceled, ///< no longer wanted; the service has been told
};

/// One declared binding, as the author described it. This is DESIRED LOCAL
/// STATE and nothing else: it names a schedule this incarnation wants to exist,
/// not a schedule that does.
template <class Self>
struct Binding {
    using Callback = void (Self::*)(const TimerFired&, loom::Mail&);

    std::string id;
    std::int64_t delay_ms = 0;
    bool repeat = false;
    std::string role; ///< empty = requester-addressed; else role-addressed
    ContinuityOrder order{};
    BindingState state = BindingState::Waiting;
    Callback callback = nullptr;
    std::string resolved; ///< the last receipt's outcome ("" until one arrives)
    std::string reason;   ///< and its self-contained why
};

/// A small local handle to one declared binding.
///
/// It is an INDEX, not an owner: bindings are declared once (normally in the
/// constructor) and never removed, so the index is stable for the incarnation's
/// life. Copying a handle copies a reference to the same binding.
template <class Self>
class TimerHandle {
public:
    TimerHandle() = default;

    /// Stop wanting this timer, and tell the service so. Both halves matter:
    /// the local half is what stops a later `TimerReady` from re-establishing
    /// it, and the remote half is what stops the service firing it.
    ///
    /// HONEST EDGE, inherited from the raw contract and deliberately not
    /// smoothed over: an already-in-flight firing can still arrive after this,
    /// and it still reaches the callback. The binding layer does not silently
    /// change Timer semantics to look tidier.
    void cancel(loom::Mail& mail) {
        if (owner_ != nullptr) {
            owner_->cancel_at(index_, mail);
        }
    }

    /// Want it again, and ask now: Spent or Canceled becomes Waiting and ONE
    /// ordered request goes out. This is the only way a spent one-shot arms
    /// again — nothing else, and no lifecycle event, resurrects one.
    void restart(loom::Mail& mail) {
        if (owner_ != nullptr) {
            owner_->restart_at(index_, mail);
        }
    }

    /// Where this binding stands. (Local truth, not a service query — a weave
    /// cannot ask the Timer what it holds.)
    BindingState state() const {
        return owner_ != nullptr ? owner_->state_at(index_) : BindingState::Canceled;
    }
    bool waiting() const { return state() == BindingState::Waiting; }
    bool spent() const { return state() == BindingState::Spent; }
    bool canceled() const { return state() == BindingState::Canceled; }

    /// The last thing the Timer said it did about this binding — one of the
    /// kResolution* spellings, or empty if no receipt has arrived yet. This is
    /// what makes "what actually happened to my timer?" an answerable question
    /// for the author rather than something only a tap can see.
    const std::string& resolution() const {
        require_owner();
        return owner_->resolution_at(index_);
    }
    const std::string& resolution_reason() const {
        require_owner();
        return owner_->reason_at(index_);
    }

    /// LOUD ON AN INVALID HANDLE, deliberately. `valid()` exists, so a default-
    /// constructed handle is part of this type's public surface and asking one
    /// for its id is a programmer error, not a runtime condition — it takes the
    /// project's established path for one (throw), never a null dereference and
    /// never a quiet empty string that would flow on as a real timer id.
    const std::string& id() const {
        require_owner();
        return owner_->id_at(index_);
    }
    bool valid() const { return owner_ != nullptr; }

    // THERE IS DELIBERATELY NO DESTRUCTOR, and its absence is a promise kept
    // rather than a detail overlooked. A destructor that "cancelled" would be
    // claiming something it cannot do: during teardown there is no valid `Mail`
    // — no bus, no stamped sender, no authorized send — so it could only either
    // lie or reach around the one door a weave speaks through. Letting a handle
    // die is therefore a purely local event; the service still holds the
    // schedule, and a repeating timer whose requester is gone fires into clean
    // refusals until something cancels it or the service is replaced. That is
    // the raw contract's dead-requester floor, unchanged and still open, and
    // this layer does not pretend to have closed it.

private:
    friend class TimerBindings<Self>;
    TimerHandle(TimerBindings<Self>* owner, std::size_t index) : owner_(owner), index_(index) {}

    void require_owner() const {
        if (owner_ == nullptr) {
            throw std::logic_error(
                "zengine::timer::TimerHandle: this handle names no binding (default-constructed "
                "or never assigned) — check valid() before asking it about one");
        }
    }

    TimerBindings<Self>* owner_ = nullptr;
    std::size_t index_ = 0;
};

/// The declared-bindings table for one weave. Reached through `timers()`.
///
/// DECLARATION IS NOT EXECUTION. Every factory here records desire and sends
/// nothing: there is no `Mail` during construction, there may be no Timer
/// service in the process at all, and a weave that contacted anything from its
/// constructor would be reaching outside the one place a weave is allowed to
/// speak. The bindings are reconciled later, while handling an ordinary
/// message, from an accepted activation or a `TimerReady`.
template <class Self>
class TimerBindings {
public:
    using Callback = typename Binding<Self>::Callback;
    using Handle = TimerHandle<Self>;

    /// Declare a repeating timer delivered back to THIS weave's identity.
    /// (The requester-addressed mode.)
    ///
    /// `order` says what should happen to a schedule that already exists when
    /// this binding is reconciled. The default — prefer preserving the
    /// remaining time, accept restarting the delay — is what makes a graceful
    /// Timer replacement continuous while letting an initial load, a hard
    /// replacement and a reload all start cleanly.
    Handle repeat(std::string id, std::chrono::milliseconds delay, Callback cb,
                  ContinuityOrder order = {}) {
        return declare(std::move(id), delay, /*repeat=*/true, /*role=*/{}, cb, order,
                       /*role_form=*/false);
    }

    /// Declare a one-shot delivered back to THIS weave's identity.
    ///
    /// ONCE MEANS ONCE PER BINDING INCARNATION, unless explicitly restarted —
    /// and that conditional clause is the thing the word `once` used to lack.
    /// When it fires the binding becomes Spent: it stops reconciling, and no
    /// Timer reload, replacement or availability notice brings it back.
    Handle once(std::string id, std::chrono::milliseconds delay, Callback cb,
                ContinuityOrder order = {}) {
        return declare(std::move(id), delay, /*repeat=*/false, /*role=*/{}, cb, order,
                       /*role_form=*/false);
    }

    /// Declare a repeating timer delivered to whoever HOLDS `role` at each
    /// firing. (The beat belongs to the slot, not to this incarnation, so a
    /// successor inherits it rather than doubling it.)
    ///
    /// Deliberately a DIFFERENT NAME rather than an overload that infers the
    /// mode from an extra string: the two addressing modes are different
    /// promises about who hears the beat and who may cancel it, and a caller
    /// should have to say which one they mean.
    Handle repeat_to_role(std::string id, std::chrono::milliseconds delay, std::string role,
                          Callback cb, ContinuityOrder order = {}) {
        return declare(std::move(id), delay, /*repeat=*/true, std::move(role), cb, order,
                       /*role_form=*/true);
    }

    /// The one-shot twin of repeat_to_role.
    Handle once_to_role(std::string id, std::chrono::milliseconds delay, std::string role,
                        Callback cb, ContinuityOrder order = {}) {
        return declare(std::move(id), delay, /*repeat=*/false, std::move(role), cb, order,
                       /*role_form=*/true);
    }

    std::size_t size() const { return bindings_.size(); }

private:
    template <class S, class State, class A, class E>
    friend class TimedWeave;
    friend class TimerHandle<Self>;

    Handle declare(std::string id, std::chrono::milliseconds delay, bool repeat_, std::string role,
                   Callback cb, ContinuityOrder order, bool role_form) {
        if (id.empty()) {
            throw std::invalid_argument("zengine::timer: a binding needs an id");
        }
        if (cb == nullptr) {
            throw std::invalid_argument("zengine::timer: binding '" + id + "' has no callback");
        }
        // AN EMPTY ROLE IS REFUSED, LOUDLY, AT DECLARATION. `repeat_to_role` and
        // `once_to_role` are the names an author reaches for when the beat must
        // belong to a SLOT rather than to this incarnation. An empty role cannot
        // mean that, and the service treats a role-addressed ask with no role as
        // no ask at all — so accepting one here would leave the author with a
        // binding that quietly behaves like the requester-addressed mode they
        // deliberately did not choose. Different promises, different names, and
        // no silent degradation between them.
        if (role_form && role.empty()) {
            throw std::invalid_argument("zengine::timer: binding '" + id +
                                        "' asks for a role beat with no role — use repeat()/once() "
                                        "for the requester-addressed mode");
        }
        // DUPLICATE IDS ARE REFUSED, LOUDLY, AT DECLARATION. A timer id is the
        // only thing a firing carries, so two bindings sharing one could not be
        // told apart — dispatch would have to pick, and picking silently is how
        // a weave ends up running the wrong behaviour forever. This is a
        // programmer error, so it takes the project's established path for one:
        // throw. A weave loaded through the kernel turns that into a clean
        // "library create() returned null" load refusal (the ABI's create thunk
        // catches everything), and a natively mounted one fails loudly at mount.
        for (const Binding<Self>& b : bindings_) {
            if (b.id == id) {
                throw std::invalid_argument("zengine::timer: duplicate binding id '" + id +
                                            "' — one id, one callback");
            }
        }
        Binding<Self> b;
        b.id = std::move(id);
        b.delay_ms = static_cast<std::int64_t>(delay.count());
        b.repeat = repeat_;
        b.role = std::move(role);
        b.order = order;
        b.state = BindingState::Waiting;
        b.callback = cb;
        bindings_.push_back(std::move(b));
        return Handle{this, bindings_.size() - 1};
    }

    BindingState state_at(std::size_t i) const {
        return i < bindings_.size() ? bindings_[i].state : BindingState::Canceled;
    }
    const std::string& id_at(std::size_t i) const { return bindings_[i].id; }
    const std::string& resolution_at(std::size_t i) const { return bindings_[i].resolved; }
    const std::string& reason_at(std::size_t i) const { return bindings_[i].reason; }

    /// Both halves, and the local one first because it is the half a naive
    /// implementation forgets: stop wanting it, THEN tell the service. Without
    /// the local half the next `TimerReady` would faithfully re-establish the
    /// thing that was just cancelled.
    ///
    /// ROLE-TIMER AUTHORITY is the service's, unchanged: `CancelTimer` removes
    /// only what the STAMPED SENDER started, so a weave can cancel a role timer
    /// only while it is the requester currently associated with it — and a
    /// successor takes that position by re-asking first. This layer sends the
    /// same message any hand-written consumer would; it grants nothing extra.
    void cancel_at(std::size_t i, loom::Mail& mail) {
        if (i >= bindings_.size()) {
            return;
        }
        bindings_[i].state = BindingState::Canceled;
        mail.send_to_role(kTimerRole, CancelTimer{bindings_[i].id});
    }

    void restart_at(std::size_t i, loom::Mail& mail) {
        if (i >= bindings_.size()) {
            return;
        }
        bindings_[i].state = BindingState::Waiting;
        ask(bindings_[i], mail);
    }

    /// Re-establish every binding still WAITING.
    ///
    /// Spent and canceled bindings are skipped, and that skip is the whole
    /// difference the lifecycle state buys: reconciling a spent one-shot would
    /// resurrect something the author already finished with, every time the
    /// Timer service so much as came back.
    ///
    /// WHAT AN ORDERED RE-ASK MEANS. The raw asks were cardinality-idempotent
    /// but never timing-neutral: they replaced and RE-ANCHORED, so a binding
    /// reconciled mid-cycle silently lost the rest of that cycle. The ordered
    /// form is how a binding says it would rather not: prefer keeping the
    /// remaining time, accept restarting if there is nothing to keep. Where
    /// there is a matching schedule and a graceful succession preserved it, a
    /// re-ask now costs nothing at all; where there is not, it restarts and
    /// SAYS SO in a receipt rather than leaving the author to guess.
    void reconcile(loom::Mail& mail) {
        for (const Binding<Self>& b : bindings_) {
            if (b.state == BindingState::Waiting) {
                ask(b, mail);
            }
        }
    }

    void ask(const Binding<Self>& b, loom::Mail& mail) {
        const std::string preferred = spelling_of(b.order.preferred);
        const std::string fallback = fallback_spelling(b.order);
        if (b.role.empty()) {
            mail.send_to_role(kTimerRole,
                              EnsureTimer{b.id, b.delay_ms, b.repeat, preferred, fallback});
        } else {
            mail.send_to_role(kTimerRole, EnsureRoleTimer{b.id, b.delay_ms, b.repeat, b.role,
                                                          preferred, fallback});
        }
    }

    /// Record what the Timer said it did. The id is the key, exactly as a firing
    /// is: a receipt for an id this weave never declared is data, not news, and
    /// is ignored — the ordinary consumer obligation.
    void record(const TimerResolution& r) {
        for (Binding<Self>& b : bindings_) {
            if (b.id == r.id) {
                b.resolved = r.resolved;
                b.reason = r.reason;
                return;
            }
        }
    }

    /// Route one firing to the ONE binding that asked for it.
    ///
    /// Exact id match, first and only. An id nobody declared is data, not a
    /// drive (a role beat can be aimed at by anyone), and two bindings can never
    /// both answer because two bindings can never share an id.
    ///
    /// A ONE-SHOT IS MARKED SPENT BEFORE ITS CALLBACK RUNS, and the order is the
    /// point: the callback is exactly where an author might deliberately
    /// `restart(mail)`, and marking afterwards would overwrite that decision
    /// with a stale one. Mark first, then hand over — so the callback's word is
    /// the last one.
    void dispatch(Self* self, const TimerFired& f, loom::Mail& mail) {
        for (Binding<Self>& b : bindings_) {
            if (b.id != f.id) {
                continue;
            }
            const Callback cb = b.callback;
            if (!b.repeat && b.state == BindingState::Waiting) {
                b.state = BindingState::Spent;
            }
            (self->*cb)(f, mail);
            return;
        }
    }

    std::vector<Binding<Self>> bindings_;
};

/// The authoring base: `WeaveBase` plus the Timer protocol, already handled.
///
/// An author writes their own Accept/Emit as usual and this layer prepends what
/// the binding needs. The composed contract is what the manifest carries — no
/// wildcard acceptance, no widened grant, no undeclared emission, no host-root
/// send, no Switchboard reach.
template <class Self, class State, class AcceptList, class EmitList = loom::Emit<>>
class TimedWeave;

template <class Self, class State, class... A, class... E>
class TimedWeave<Self, State, loom::Accept<A...>, loom::Emit<E...>>
    : public loom::WeaveBase<
          Self, State,
          loom::Accept<loom::Activated, TimerReady, TimerFired, TimerResolution, A...>,
          loom::Emit<EnsureTimer, EnsureRoleTimer, CancelTimer, E...>> {
public:
    using Bindings = TimerBindings<Self>;
    using Handle = TimerHandle<Self>;

    /// BOTH WALLS, checked for EVERY bound weave.
    ///
    /// Anchored in the constructor rather than in `timers()`, deliberately, and
    /// for both of them by the same argument: a weave can define its own
    /// handlers and never call `timers()` — it may want this layer for the
    /// activation half and place its schedules with the raw protocol, because
    /// the delay is runtime data — and a check the author may never instantiate
    /// is a check that is not there. That was measured: a weave with a domain
    /// handler, no `using`, and no declared binding produced 72 lines of
    /// template soup naming neither the cause nor the fix, while `timers()`
    /// held the sentence that would have said both. Every `TimedWeave` runs its
    /// own constructor, and `Self` is complete by the time this body is
    /// instantiated (it is instantiated from `Self`'s constructor, after
    /// `Self`'s definition closed).
    TimedWeave() {
        static_assert(
            activation_is_the_bindings(),
            "zengine::timer::TimedWeave: this weave defines its own "
            "on(const loom::Activated&, loom::Mail&), which REPLACES the binding layer's "
            "instead of extending it — the Timer bindings would never be reconciled and no "
            "timer would ever be ordered. That handler belongs to TimedWeave. To do domain "
            "work on activation, implement `void on_timed_activation(const loom::Activated&, "
            "loom::Mail&)` instead; it runs after the bindings reconciled, and only for an "
            "activation this weave accepted.");
        static_assert(
            !names_activation_hook() || has_activation_hook(),
            "zengine::timer::TimedWeave: this weave declares `on_timed_activation` with a "
            "signature the binding layer cannot call, so it would be silently ignored. The "
            "hook is exactly: void on_timed_activation(const loom::Activated&, loom::Mail&) "
            "— and it must be reachable from the binding layer (public).");
        static_assert(
            handlers_are_visible(),
            "zengine::timer::TimedWeave: this weave's own on() handlers HIDE the binding "
            "layer's. Add `using TimedWeave::on;` to the class. (WeaveBase dispatches via "
            "self->on(...) on the derived type, and a derived on() hides every base one.)");
    }

    /// The declared-bindings table. Call the factories on it during
    /// construction; they record desire and send nothing.
    Bindings& timers() { return bindings_; }
    const Bindings& timers() const { return bindings_; }

    // ---- the ceremony, owned here so no author writes it again -------------

    /// This incarnation is live: establish everything it declared.
    ///
    /// Activation trust AND deduplication live HERE, once, so no author has to
    /// rediscover the rule. The cursor requires Loom's attestation before it
    /// considers lineage at all — so a bound weave cannot be made to
    /// re-establish its timers by any weave that merely knows the public shape.
    void on(const loom::Activated& a, loom::Mail& mail) {
        if (!activation_.accept(mail, a)) {
            return; // unattested, duplicate or replayed: nothing is re-established
        }
        bindings_.reconcile(mail);
        // ...AND ONLY THEN THE AUTHOR'S CLAUSE. The order is the contract: an
        // author's activation work may assume its timers are already ordered,
        // which is the whole reason the hook exists rather than a raw handler.
        // It is not reached at all by an activation the cursor refused, so
        // "unattested, duplicate or replayed" means nothing happened, still.
        if constexpr (has_activation_hook()) {
            static_cast<Self*>(this)->on_timed_activation(a, mail);
        }
    }

    /// The Timer service is available — possibly for the first time, possibly
    /// again after a reload or swap with an empty table. Either way, the
    /// declared bindings are what this weave wants to exist, so re-establish
    /// them. This is the path that covers a consumer loaded BEFORE the service,
    /// whose activation-time asks could not reach it.
    void on(const TimerReady&, loom::Mail& mail) { bindings_.reconcile(mail); }

    /// A firing: exactly one binding's callback, or none.
    void on(const TimerFired& f, loom::Mail& mail) {
        bindings_.dispatch(static_cast<Self*>(this), f, mail);
    }

    /// A receipt: what the Timer actually did about one order.
    ///
    /// The binding CONSUMES it without hiding it — this is an ordinary declared
    /// message on an ordinary bus, so a tap and a console see the order and its
    /// answer whether or not any author ever reads one. What the binding adds is
    /// that the author CAN read it, from the handle, without writing protocol.
    void on(const TimerResolution& r, loom::Mail&) { bindings_.record(r); }

    // ---- THE AUTHOR'S EXTENSION POINT --------------------------------------
    //
    // Optional. A weave that defines nothing behaves exactly as it always did —
    // `if constexpr` means the call is not merely skipped at run time, it is not
    // compiled, so there is no virtual, no std::function, no stored callback and
    // no cost of any kind for the weaves that do not want it.
    //
    //     void on_timed_activation(const loom::Activated&, loom::Mail&);
    //
    // WHEN IT RUNS: after this weave accepted an activation AND after every
    // waiting binding was reconciled. Never for an unattested, duplicate,
    // replayed, stale or foreign activation — the cursor decided that already,
    // once, above. Never for `TimerReady`, which is the Timer service becoming
    // available and is not this weave's activation. Never for an ordinary
    // message.
    //
    // WHAT IT MAY DO: ordinary domain work through the ordinary live `Mail` —
    // and nothing more. It receives no additional authority, and every shape it
    // sends must already be in this weave's own `Emit<...>`, because the
    // manifest is composed from that list and the hook adds nothing to it.
    //
    // IT IS ALREADY INSIDE AN ACCEPTED ACTIVATION. Do not call
    // `activation().accept(...)` again — there is one cursor, it belongs to the
    // binding layer, and it has already spoken.
    //
    // IT MUST BE REACHABLE FROM HERE (public). A private hook cannot be
    // distinguished from an absent one — an access failure inside a
    // requires-expression is simply an unsatisfied requirement — so a private
    // one would be silently skipped. That is the one hazard here the compiler
    // cannot name for us, which is why it is named here.

protected:
    /// Visible to the author only so a subclass can READ its own activation
    /// state if it genuinely needs to; the binding layer already acts on it, and
    /// `on_timed_activation` already runs inside an accepted activation.
    const zengine::ActivationCursor& activation() const { return activation_; }

private:
    /// Is an activation handler of the dispatch signature reachable on `Self`?
    ///
    /// FALSE MEANS THE DERIVED CLASS HID EVERY BASE `on`, which is exactly the
    /// missing-`using` defect — so this doubles as the visibility probe, and it
    /// is the RIGHT probe for it. A `TimerFired` probe (what this check used to
    /// be, over in `timers()`) asks about a shape an author may legitimately
    /// handle themselves whenever the delay is runtime data, and a weave that
    /// does is a weave whose missing `using` the probe cannot see. Activation is
    /// the one shape a derived class may never claim — `activation_is_the_bindings`
    /// below refuses it by name — so a derived handler can never mask this.
    ///
    /// IT ASKS THE CALL, NOT THE ADDRESS, and that is a portability requirement
    /// rather than a preference. Naming the overload SET and letting a helper
    /// deduce its owning class asks the compiler to find the one candidate whose
    /// signature matches; MSVC 19.50 declines to do that whenever the set spans
    /// two classes — which is the CORRECT author's shape, own handlers plus
    /// `using TimedWeave::on;` — and the wall then fired on code that was right.
    /// Measured over the five shapes that matter (no handler of one's own; own
    /// handlers with the `using`; own handlers without it; a derived activation
    /// handler; and both at once), the call form agrees with itself on GCC and
    /// MSVC in every one, and the deduced form disagrees in the second. It is
    /// also the truer question: `WeaveBase` dispatches by CALLING `self->on(a, m)`,
    /// so whether that call compiles is precisely what the wall is about.
    static constexpr bool activation_addressable() {
        return requires(Self& s, const loom::Activated& a, loom::Mail& m) { s.on(a, m); };
    }

    /// The same question, under the name the diagnostic is about.
    static constexpr bool handlers_are_visible() { return activation_addressable(); }

    /// Can the OWNER of the reachable activation handler be named on this
    /// compiler? Separate from reachability because the two are answered by
    /// different mechanisms, and only this one is affected by the deduction gap
    /// described above.
    static constexpr bool activation_owner_is_deducible() {
        return requires { detail::activation_owner(&Self::on); };
    }

    /// Would `WeaveBase`'s `self->on(...)` select the binding's own activation
    /// handler? Identity, not callability: a derived one is perfectly callable,
    /// which is exactly why it is dangerous.
    ///
    /// THE MIDDLE BRANCH ABSTAINS, and what makes that safe is which shapes can
    /// reach it. Deduction only fails where the reachable handler is the BASE's
    /// and arrived through a using-declaration; a derived class that declares its
    /// own activation handler puts it in the set directly, and deduction picks it
    /// on both compilers — measured, for that handler alone and for that handler
    /// alongside a hidden base. So the shape this wall exists to refuse is never
    /// the shape that abstains, and the answer it declines to give would have been
    /// "the base owns it" in every case observed.
    static constexpr bool activation_is_the_bindings() {
        if constexpr (!activation_addressable()) {
            return true; // hidden entirely; the visibility assert owns that diagnostic
        } else if constexpr (!activation_owner_is_deducible()) {
            return true; // this compiler cannot name the owner; see above for why that is safe
        } else {
            return std::is_same_v<decltype(detail::activation_owner(&Self::on)), TimedWeave*>;
        }
    }

    /// Does `Self` provide the hook, exactly as the binding layer will call it?
    static constexpr bool has_activation_hook() {
        return requires(Self& s, const loom::Activated& a, loom::Mail& m) {
            s.on_timed_activation(a, m);
        };
    }

    /// Does `Self` have a member of that name at all? Used only to turn a
    /// near-miss signature into a diagnostic instead of a silent no-op. It
    /// cannot see through an overload SET (taking the address of one is
    /// ill-formed), so an author who overloads the hook wrongly is not caught —
    /// named here rather than implied to be covered.
    static constexpr bool names_activation_hook() {
        return requires { &Self::on_timed_activation; };
    }

    zengine::ActivationCursor activation_; ///< per-incarnation, never state
    Bindings bindings_;
};

} // namespace zengine::timer

#endif // ZENGINE_TIMER_BINDING_HPP
