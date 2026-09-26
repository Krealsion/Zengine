// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_TIMER_BINDING_HPP
#define ZENGINE_TIMER_BINDING_HPP

// The timer binding: a weave declares the time behaviour it wants, and the binding owns the
// Timer protocol and the lifecycle reconciliation that keeps it true (accepting `zen.Activated`
// and `TimerReady`, placing orders, routing `TimerFired` by id). It is not a scheduler or a
// second reading of time: the raw vocabulary stays public, and a firing runs on the ordinary
// thread, in the ordinary handler, with the ordinary `Mail`. Law: TIMER-05,
// docs/laws/timer-laws.md; the model and its boundary: docs/reference/timer-binding.md.
//
// A weave that mixes this in declares, in its manifest, the whole Timer protocol the binding
// speaks. Its own `on` handlers hide the binding's, so it writes `using TimedWeave::on;` (a
// named compile error otherwise). A derived `on(const loom::Activated&, loom::Mail&)` would
// silently replace the binding's -- a same-signature member excludes the using-declaration's --
// and time would never start, so it is refused at compile time; activation work goes in the
// optional `on_timed_activation` hook, which runs after the bindings reconciled.

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

/// Which class owns the activation handler dispatch would select: declared, never defined, and
/// deduced from `&Self::on` -- the overload whose pattern matches -- inside `decltype`; `C*` so no
/// weave must be returnable by value. The parameter types stay concrete, only `C` deduced: a
/// deduced `Activated`/`Mail` would match every handler, deduction would go ambiguous, and the
/// check would abstain on exactly the classes it exists to refuse.
template <class C>
C* activation_owner(void (C::*)(const loom::Activated&, loom::Mail&));

} // namespace detail

/// Where one binding stands: three states, because "should reconciliation re-establish this?"
/// and "has it happened?" are different questions -- a fired one-shot answers no to the first.
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

    /// Stop wanting this timer and tell the service: the local half stops a later `TimerReady`
    /// re-establishing it, the remote half stops the firing. A firing already in flight can still
    /// arrive after this, and still reaches the callback.
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

    /// The last thing the Timer said it did about this binding -- a kResolution* spelling -- or
    /// empty before any receipt.
    const std::string& resolution() const {
        require_owner();
        return owner_->resolution_at(index_);
    }
    const std::string& resolution_reason() const {
        require_owner();
        return owner_->reason_at(index_);
    }

    /// Throws on an invalid handle (check `valid()`): asking one for its id is a programmer error,
    /// never a null dereference or a quiet empty id.
    const std::string& id() const {
        require_owner();
        return owner_->id_at(index_);
    }
    bool valid() const { return owner_ != nullptr; }

    // There is deliberately no destructor: teardown has no `Mail`, so a cancelling destructor
    // could only lie. A dropped handle is a local event; the service keeps the schedule, and a
    // repeating timer whose requester is gone fires into clean refusals until cancelled.

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

/// The declared-bindings table, reached through `timers()`. Declaration is not execution: the
/// factories record desire and send nothing (no `Mail` exists during construction); the table
/// is reconciled from an accepted activation or a `TimerReady`.
template <class Self>
class TimerBindings {
public:
    using Callback = typename Binding<Self>::Callback;
    using Handle = TimerHandle<Self>;

    /// Declare a repeating timer delivered back to this weave. `order` says what happens to a
    /// schedule that already exists at reconciliation; the default prefers keeping the remaining
    /// time and accepts restarting.
    Handle repeat(std::string id, std::chrono::milliseconds delay, Callback cb,
                  ContinuityOrder order = {}) {
        return declare(std::move(id), delay, /*repeat=*/true, /*role=*/{}, cb, order,
                       /*role_form=*/false);
    }

    /// Declare a one-shot delivered back to this weave: once per binding incarnation, unless
    /// restarted. Fired, it is Spent, and no Timer reload, replacement or notice brings it back.
    Handle once(std::string id, std::chrono::milliseconds delay, Callback cb,
                ContinuityOrder order = {}) {
        return declare(std::move(id), delay, /*repeat=*/false, /*role=*/{}, cb, order,
                       /*role_form=*/false);
    }

    /// Declare a repeating timer delivered to whoever holds `role` at each firing; the beat is
    /// the slot's, so a successor inherits it. A separate name, since the two addressing modes are
    /// different promises about who hears the beat and who may cancel it.
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
        // An empty role is refused here: the service treats a role ask with no role as no ask,
        // so the binding would silently become the requester mode the author did not choose.
        if (role_form && role.empty()) {
            throw std::invalid_argument("zengine::timer: binding '" + id +
                                        "' asks for a role beat with no role — use repeat()/once() "
                                        "for the requester-addressed mode");
        }
        // A duplicate id is refused here: a firing carries only its id, so two bindings sharing
        // one could not be told apart. Thrown, the project's path for a programmer error (a
        // kernel load then refuses cleanly; a native mount fails loudly).
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

    /// The local half first, then the service: without it the next `TimerReady` would
    /// re-establish what was just cancelled. `CancelTimer` removes only what the stamped sender
    /// started; this sends what a hand-written consumer would and grants nothing extra.
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

    /// Re-establish every binding still waiting; spent and cancelled ones are skipped, or a spent
    /// one-shot would come back whenever the Timer did. Ordered re-asks prefer keeping the
    /// remaining time, so a preserved schedule costs nothing and a restart is said in a receipt.
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

    /// Route one firing to the one binding that asked for it, by exact id; an undeclared id is
    /// data, not a drive. A one-shot is marked Spent before its callback runs, so a `restart`
    /// inside the callback has the last word.
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

/// The authoring base: `WeaveBase` plus the Timer protocol, already handled. The author's own
/// Accept/Emit get the binding's prepended; the manifest carries the composed contract and no
/// wildcard, widened grant or undeclared emission.
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

    /// Both walls, checked for every bound weave, in the constructor rather than in `timers()`: a
    /// weave may never call `timers()` (runtime delays use the raw protocol), and a check it never
    /// instantiates is not there -- measured as 72 lines of template soup naming no cause. `Self`
    /// is complete here, since this is instantiated from its constructor.
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

    /// This incarnation is live: establish everything it declared. Trust and deduplication live
    /// here once: the cursor requires Loom's attestation, so no weave that merely knows the shape
    /// can make this one re-establish its timers.
    void on(const loom::Activated& a, loom::Mail& mail) {
        if (!activation_.accept(mail, a)) {
            return; // unattested, duplicate or replayed: nothing is re-established
        }
        bindings_.reconcile(mail);
        // ...and only then the author's hook, which may assume its timers are ordered; a refused
        // activation never reaches it.
        if constexpr (has_activation_hook()) {
            static_cast<Self*>(this)->on_timed_activation(a, mail);
        }
    }

    /// The Timer service is available, first or again after a reload or swap: re-establish the
    /// declared bindings. This is what covers a consumer loaded before the service.
    void on(const TimerReady&, loom::Mail& mail) { bindings_.reconcile(mail); }

    /// A firing: exactly one binding's callback, or none.
    void on(const TimerFired& f, loom::Mail& mail) {
        bindings_.dispatch(static_cast<Self*>(this), f, mail);
    }

    /// A receipt: consumed, never hidden (a tap still sees it), so the author can read what the
    /// Timer did from the handle.
    void on(const TimerResolution& r, loom::Mail&) { bindings_.record(r); }

    // ---- the author's extension point --------------------------------------
    //
    // Optional: `void on_timed_activation(const loom::Activated&, loom::Mail&);`, compiled in
    // only when defined. It runs after this weave accepted an activation and reconciled its
    // bindings -- never for a refused activation or `TimerReady` -- with the ordinary `Mail` and
    // no extra authority, and it must not accept the activation again. It must be public: a
    // private hook is indistinguishable from an absent one, and would be silently skipped.

protected:
    /// Visible to the author only so a subclass can READ its own activation
    /// state if it genuinely needs to; the binding layer already acts on it, and
    /// `on_timed_activation` already runs inside an accepted activation.
    const zengine::ActivationCursor& activation() const { return activation_; }

private:
    /// Is an activation handler of the dispatch signature reachable on `Self`? False means the
    /// derived class hid every base `on`, the missing-`using` defect; activation is the probe
    /// because it is the one shape a derived class may never claim. It asks the call, not the
    /// address: MSVC 19.50 will not deduce an overload set spanning two classes (the correct
    /// shape, own handlers plus the `using`), and `WeaveBase` dispatches by calling anyway.
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

    /// Would dispatch select the binding's own activation handler? Identity, not callability. The
    /// middle branch abstains only where the reachable handler is the base's, via the
    /// using-declaration; a derived handler is in the set directly and deduces on both compilers
    /// (measured), so the shape this wall refuses never abstains.
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

    /// Does `Self` have a member of that name at all? Turns a near-miss signature into a
    /// diagnostic; it cannot see through an overload set, so a wrongly overloaded hook is missed.
    static constexpr bool names_activation_hook() {
        return requires { &Self::on_timed_activation; };
    }

    zengine::ActivationCursor activation_; ///< per-incarnation, never state
    Bindings bindings_;
};

} // namespace zengine::timer

#endif // ZENGINE_TIMER_BINDING_HPP
