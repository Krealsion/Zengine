// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_TIMER_BINDING_HPP
#define ZENGINE_TIMER_BINDING_HPP

// The timer binding. A weave declares the timers it wants, and the binding sends the Timer the
// messages that keep them true: it accepts `zen.Activated` and `TimerReady`, asks for every
// declared timer, and routes each `TimerFired` to its callback by id. The declared timers are a
// fixed table, asked for at those two moments (TIMER-05). The binding is not a scheduler and does
// not read time a second way: the plain messages stay public, and a firing runs on the ordinary
// thread, in the ordinary handler, with the ordinary `Mail`. The page for makers is
// `timer/docs/timed-weaves.md`.
//
// A weave built on this declares, in its manifest, every Timer message the binding sends and
// accepts. The weave's own `on` handlers hide the binding's, so it must write
// `using TimedWeave::on;`, and the compiler refuses it by name otherwise. A derived
// `on(const loom::Activated&, loom::Mail&)` would silently replace the binding's, because a member
// with the same signature wins over the using-declaration, and no timer would ever be asked for;
// so that is refused at compile time too. Work at activation goes in the optional
// `on_timed_activation` hook, which runs after the binding has asked for every timer.

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

/// Names the class that owns the activation handler dispatch would choose. It is declared and
/// never defined: `decltype` deduces `C` from `&Self::on`, picking the overload whose parameters
/// match, and it returns `C*` so that no weave has to be returnable by value. Only `C` is deduced
/// and the parameter types stay fixed: deducing `Activated` and `Mail` too would match every
/// handler, the deduction would be ambiguous, the `requires` that asks for it would be false, and
/// the check would let through exactly the classes it exists to refuse.
template <class C>
C* activation_owner(void (C::*)(const loom::Activated&, loom::Mail&));

} // namespace detail

/// Where one binding stands. It has three states because two questions differ: should the binding
/// ask for this timer again, and has it fired? A one-shot that has fired answers no to the first.
enum class BindingState {
    Waiting,  ///< wanted, and not yet finished with
    Spent,    ///< a one-shot that has fired; done unless restarted
    Canceled, ///< no longer wanted, and the Timer has been told
};

/// One declared binding, as the weave's author described it. It records only what this weave
/// wants: a timer it wants to exist, not a timer the Timer is known to hold.
template <class Self>
struct Binding {
    using Callback = void (Self::*)(const TimerFired&, loom::Mail&);

    std::string id;
    std::int64_t delay_ms = 0;
    bool repeat = false;
    std::string role; ///< empty when the beat comes back to this weave; else the role it goes to
    ContinuityOrder order{};
    BindingState state = BindingState::Waiting;
    Callback callback = nullptr;
    std::string resolved; ///< the last receipt's outcome, empty until one arrives
    std::string reason;   ///< the last receipt's reason, a sentence that stands alone
};

/// A small handle to one declared binding.
///
/// It holds the binding's index and owns nothing. Bindings are declared once, normally in the
/// constructor, and never removed, so the index stays valid for as long as this weave runs.
/// Copying a handle gives a second handle to the same binding.
template <class Self>
class TimerHandle {
public:
    TimerHandle() = default;

    /// Stops wanting this timer and tells the Timer. Marking it here keeps a later `TimerReady`
    /// from asking for it again, and telling the Timer stops the firing. A firing already on its
    /// way can still arrive after this, and it still reaches the callback.
    void cancel(loom::Mail& mail) {
        if (owner_ != nullptr) {
            owner_->cancel_at(index_, mail);
        }
    }

    /// Wants the timer again and asks for it now: a spent or cancelled binding becomes waiting, and
    /// one ordered ask goes out. This is the only way a spent one-shot is armed again; no other
    /// call, and no activation or replacement, brings one back.
    void restart(loom::Mail& mail) {
        if (owner_ != nullptr) {
            owner_->restart_at(index_, mail);
        }
    }

    /// Where this binding stands, as this weave knows it. The Timer is not asked, because a weave
    /// cannot ask the Timer what it holds.
    BindingState state() const {
        return owner_ != nullptr ? owner_->state_at(index_) : BindingState::Canceled;
    }
    bool waiting() const { return state() == BindingState::Waiting; }
    bool spent() const { return state() == BindingState::Spent; }
    bool canceled() const { return state() == BindingState::Canceled; }

    /// What the Timer's last receipt for this binding reported, as one of the kResolution* words,
    /// or empty before the first receipt.
    const std::string& resolution() const {
        require_owner();
        return owner_->resolution_at(index_);
    }
    const std::string& resolution_reason() const {
        require_owner();
        return owner_->reason_at(index_);
    }

    /// The binding's id. It throws on a handle that names no binding (check `valid()`), because
    /// asking such a handle is a mistake in the program, and it must not become a null
    /// dereference or a quiet empty id.
    const std::string& id() const {
        require_owner();
        return owner_->id_at(index_);
    }
    bool valid() const { return owner_ != nullptr; }

    // There is no destructor, on purpose. A weave being torn down has no `Mail`, so a destructor
    // that claimed to cancel the timer could not send the cancel. Dropping a handle changes nothing
    // at the Timer: the timer stands, and a repeating timer whose weave is gone fires into the
    // bus's refusals until it is cancelled.

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

/// The table of declared bindings, reached through `timers()`. Declaring a binding sends nothing:
/// the factories only record what the weave wants, because no `Mail` exists during construction.
/// The binding asks the Timer for the table's timers on an accepted activation and on `TimerReady`.
template <class Self>
class TimerBindings {
public:
    using Callback = typename Binding<Self>::Callback;
    using Handle = TimerHandle<Self>;

    /// Declares a repeating timer that fires back to this weave. `order` says what should happen
    /// to a timer the Timer already holds under this id when the binding asks again; the default
    /// prefers keeping the time left and accepts starting again.
    Handle repeat(std::string id, std::chrono::milliseconds delay, Callback cb,
                  ContinuityOrder order = {}) {
        return declare(std::move(id), delay, /*repeat=*/true, /*role=*/{}, cb, order,
                       /*role_form=*/false);
    }

    /// Declares a one-shot that fires back to this weave, once for as long as the weave runs unless
    /// it is restarted. After it fires it is spent, and no reload or replacement of the Timer, and
    /// no `TimerReady`, brings it back.
    Handle once(std::string id, std::chrono::milliseconds delay, Callback cb,
                ContinuityOrder order = {}) {
        return declare(std::move(id), delay, /*repeat=*/false, /*role=*/{}, cb, order,
                       /*role_form=*/false);
    }

    /// Declares a repeating timer that fires to whoever holds `role` at each beat. The beat belongs
    /// to the role, so a successor in the role inherits it. It has a name of its own because the
    /// two ways of addressing a beat are different promises about who hears it and who may cancel
    /// it.
    Handle repeat_to_role(std::string id, std::chrono::milliseconds delay, std::string role,
                          Callback cb, ContinuityOrder order = {}) {
        return declare(std::move(id), delay, /*repeat=*/true, std::move(role), cb, order,
                       /*role_form=*/true);
    }

    /// The one-shot form of `repeat_to_role`.
    Handle once_to_role(std::string id, std::chrono::milliseconds delay, std::string role,
                        Callback cb, ContinuityOrder order = {}) {
        return declare(std::move(id), delay, /*repeat=*/false, std::move(role), cb, order,
                       /*role_form=*/true);
    }

    std::size_t size() const { return bindings_.size(); }

private:
    template <class S, class State, class A, class E, class D, class C>
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
        // An empty role is refused here. The Timer treats a role ask with no role as no ask at
        // all, so the binding would silently fall back to firing at this weave, which its author
        // did not choose.
        if (role_form && role.empty()) {
            throw std::invalid_argument("zengine::timer: binding '" + id +
                                        "' asks for a role beat with no role — use repeat()/once() "
                                        "for the requester-addressed mode");
        }
        // A duplicate id is refused here, because a firing carries only its id and two bindings
        // that shared one could not be told apart. It throws, which is how this project reports a
        // mistake in a program: a load through the Kernel is then refused cleanly, and a weave
        // built into a program fails loudly.
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

    /// Marks the binding cancelled first and then tells the Timer; without the mark, the next
    /// `TimerReady` would ask again for what was just cancelled. `CancelTimer` removes only the
    /// timers the bus names this weave as having started, and this sends exactly what a weave
    /// written without the binding would send, so the binding gains no extra power.
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

    /// Asks again for every binding still waiting. Spent and cancelled bindings are skipped, or a
    /// spent one-shot would come back whenever the Timer did. Each ask prefers keeping the time
    /// left, so a timer the Timer kept costs nothing, and a timer that starts again says so in its
    /// receipt.
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

    /// Records what the Timer's receipt reports. The id is the key, exactly as it is for a firing, so a
    /// receipt for an id this weave never declared is ignored, as every weave must ignore an id it
    /// did not ask for.
    void record(const TimerResolution& r) {
        for (Binding<Self>& b : bindings_) {
            if (b.id == r.id) {
                b.resolved = r.resolved;
                b.reason = r.reason;
                return;
            }
        }
    }

    /// Sends one firing to the one binding that asked for it, by exact id; a firing under an id the
    /// weave never declared runs nothing. A one-shot is marked spent before its callback runs, so
    /// a `restart` inside the callback has the last word.
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

/// The base class a weave with declared timers derives from: `WeaveBase`, with the Timer's
/// messages already handled. The binding's messages are added in front of the author's own
/// `Accept` and `Emit` lists, and the author's `Claims` pass through as they are. The manifest
/// therefore carries the combined contract exactly, with no wildcard, no wider grant and no message
/// sent that is not declared.
template <class Self, class State, class AcceptList, class EmitList = loom::Emit<>,
          class Dispatch = Self, class ClaimList = loom::Claims<>>
class TimedWeave;

template <class Self, class State, class... A, class... E, class Dispatch, class... C>
class TimedWeave<Self, State, loom::Accept<A...>, loom::Emit<E...>, Dispatch, loom::Claims<C...>>
    : public loom::WeaveBase<
          Dispatch, State,
          typename zengine::detail::UnionLists<
              loom::Accept<loom::Activated, TimerReady, TimerFired, TimerResolution>,
              loom::Accept<A...>>::type,
          typename zengine::detail::UnionLists<
              loom::Emit<EnsureTimer, EnsureRoleTimer, CancelTimer>,
              loom::Emit<E...>>::type,
          loom::Claims<C...>> {
public:
    using Bindings = TimerBindings<Self>;
    using Handle = TimerHandle<Self>;

    /// The compile-time checks, made for every weave built on this class, in the constructor
    /// rather than in `timers()`. A weave may never call `timers()`, as when all its delays are
    /// data sent with the plain messages, and a check in a function that is never instantiated
    /// never runs: the mistake then surfaced as 72 lines of template errors that named no cause.
    /// `Self` is a complete type here, because this constructor is instantiated from its own.
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

    /// The table of declared bindings. Call its factories during construction; they record what
    /// the weave wants and send nothing.
    Bindings& timers() { return bindings_; }
    const Bindings& timers() const { return bindings_; }

    // ---- the handlers every timed weave needs, written once here -----------

    /// This weave is live, so the binding asks for every timer it declared. The check of who sent
    /// the activation, and of whether it was seen before, is made here once: the cursor accepts
    /// only an activation Loom attests, so a weave that merely knows the message cannot make this
    /// one ask for its timers again.
    void on(const loom::Activated& a, loom::Mail& mail) {
        if (!activation_.accept(mail, a)) {
            return; // Not attested, a duplicate or a replay: no timer is asked for again.
        }
        bindings_.reconcile(mail);
        // Only then does the author's hook run, so it may rely on its timers having been asked
        // for. An activation the cursor refused never reaches it.
        if constexpr (has_activation_hook()) {
            static_cast<Self*>(this)->on_timed_activation(a, mail);
        }
    }

    /// The Timer is serving, for the first time or again after a reload or a replacement, so the
    /// binding asks for its declared timers again. This is also what serves a weave that was
    /// loaded before the Timer.
    void on(const TimerReady&, loom::Mail& mail) { bindings_.reconcile(mail); }

    /// A firing runs exactly one binding's callback, or none.
    void on(const TimerFired& f, loom::Mail& mail) {
        bindings_.dispatch(static_cast<Self*>(this), f, mail);
    }

    /// A receipt is recorded, so that the author can read from the handle what the Timer did. It
    /// is not hidden: a tool watching the bus still sees it.
    void on(const TimerResolution& r, loom::Mail&) { bindings_.record(r); }

    // ---- the author's hook ---------------------------------------------------
    //
    // A weave may define `void on_timed_activation(const loom::Activated&, loom::Mail&);`, and the
    // call to it is compiled in only when it does. It runs after this weave accepted an activation
    // and asked for its timers, never for a refused activation or for `TimerReady`. It gets the
    // ordinary `Mail` and no extra power, and it must not accept the activation again. It must be
    // public, because a private hook cannot be told apart from a missing one and would be skipped
    // without a word.

protected:
    /// The activation's state, read-only, for a subclass that truly needs to read it. The binding
    /// already acts on it, and `on_timed_activation` already runs inside an accepted activation.
    const zengine::ActivationCursor& activation() const { return activation_; }

private:
    /// Whether an activation handler with the signature dispatch calls can be reached on `Self`.
    /// False means the derived class hid every `on` of the base, the mistake of a missing `using`.
    /// Activation is the message tested because it is the one a derived class may never handle
    /// itself. The test makes the call rather than taking the handler's address, because MSVC
    /// 19.50 cannot deduce an overload set that spans two classes, which is exactly the correct
    /// case of own handlers plus the `using`, and `WeaveBase` dispatches by calling anyway.
    static constexpr bool activation_addressable() {
        return requires(Self& s, const loom::Activated& a, loom::Mail& m) { s.on(a, m); };
    }

    /// The same question, under the name the visibility `static_assert` in the constructor uses.
    static constexpr bool handlers_are_visible() { return activation_addressable(); }

    /// Whether this compiler can name the class that owns the reachable activation handler. It is
    /// asked apart from whether the handler can be reached, because the two are answered in
    /// different ways, and only this one fails on MSVC 19.50 for an overload set that spans two
    /// classes.
    static constexpr bool activation_owner_is_deducible() {
        return requires { detail::activation_owner(&Self::on); };
    }

    /// Whether dispatch would choose the binding's own activation handler: which handler it is, not
    /// only whether one can be called. The middle branch skips the owner check only where the reachable
    /// handler is the base's, reached through the using-declaration, which is the correct case. A
    /// handler the derived class declares is in the set directly, and its owner deduces on both
    /// compilers the CI builds with, GCC and MSVC, so the case this check refuses is always judged.
    static constexpr bool activation_is_the_bindings() {
        if constexpr (!activation_addressable()) {
            return true; // Every base `on` is hidden; the visibility check reports that instead.
        } else if constexpr (!activation_owner_is_deducible()) {
            return true; // Only the correct case reaches here: the base's handler, via the `using`.
        } else {
            return std::is_same_v<decltype(detail::activation_owner(&Self::on)), TimedWeave*>;
        }
    }

    /// Whether `Self` defines the hook exactly as the binding will call it.
    static constexpr bool has_activation_hook() {
        return requires(Self& s, const loom::Activated& a, loom::Mail& m) {
            s.on_timed_activation(a, m);
        };
    }

    /// Whether `Self` has a member of that name at all, so that a hook with a nearly right
    /// signature gets a compile error rather than being skipped. Taking the address of an
    /// overloaded name fails, so a hook overloaded with a wrong signature makes this false and
    /// is not caught; no other check catches it either.
    static constexpr bool names_activation_hook() {
        return requires { &Self::on_timed_activation; };
    }

    zengine::ActivationCursor activation_; ///< this incarnation's own; never in the weave's state
    Bindings bindings_;
};

using zengine::With;

} // namespace zengine::timer

#endif // ZENGINE_TIMER_BINDING_HPP
