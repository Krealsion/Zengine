#ifndef ZENGINE_TIMER_BINDING_HPP
#define ZENGINE_TIMER_BINDING_HPP

// The timer binding — one Zengine word for a sentence everybody was writing.
//
//   A weave DECLARES the time behaviour it wants; the binding owns the Timer
//   protocol and the lifecycle reconciliation required to maintain it.
//
// R2A-2 made the raw Timer conversation correct, and in doing so made its
// ceremony visible. Every consumer that wanted a heartbeat had to write the
// same seven steps: accept `zen.Activated`, deduplicate it, accept
// `TimerReady`, send a StartTimer/StartRoleTimer, accept `TimerFired`, filter
// by id, and re-ask whenever the service appeared or came back. That machinery
// is real and none of it is a domain decision — so it belongs in package
// vocabulary, written once, not in every author's file.
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
// `TimerFired` accepted; `StartTimer`, `StartRoleTimer`, `CancelTimer` emitted
// — even if it only uses one addressing mode. That is deliberate: the
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

#include "vocabulary.hpp"

#include "activation/activation.hpp"

#include <zen/weave.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace zengine::timer {

template <class Self>
class TimerBindings;

/// One declared binding, as the author described it. This is DESIRED LOCAL
/// STATE and nothing else: it names a schedule this incarnation wants to exist,
/// not a schedule that does.
template <class Self>
struct Binding {
    using Callback = void (Self::*)(const TimerFired&, loom::Mail&);

    std::string id;
    std::int64_t delay_ms = 0;
    bool repeat = false;
    std::string role;      ///< empty = requester-addressed; else role-addressed
    bool desired = true;   ///< reconciliation re-establishes this one
    Callback callback = nullptr;
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

    /// Want it again, and ask now. Reconciles exactly once, like an activation.
    void restart(loom::Mail& mail) {
        if (owner_ != nullptr) {
            owner_->restart_at(index_, mail);
        }
    }

    /// Is this binding currently wanted? (Local truth, not a service query —
    /// a weave cannot ask the Timer what it holds.)
    bool desired() const { return owner_ != nullptr && owner_->desired_at(index_); }

    const std::string& id() const { return owner_->id_at(index_); }
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
    /// (`StartTimer` — the requester-addressed mode.)
    Handle repeat(std::string id, std::chrono::milliseconds delay, Callback cb) {
        return declare(std::move(id), delay, /*repeat=*/true, /*role=*/{}, cb);
    }

    /// Declare a one-shot delivered back to THIS weave's identity.
    Handle once(std::string id, std::chrono::milliseconds delay, Callback cb) {
        return declare(std::move(id), delay, /*repeat=*/false, /*role=*/{}, cb);
    }

    /// Declare a repeating timer delivered to whoever HOLDS `role` at each
    /// firing. (`StartRoleTimer` — the beat belongs to the slot, not to this
    /// incarnation, so a successor inherits it rather than doubling it.)
    ///
    /// Deliberately a DIFFERENT NAME rather than an overload that infers the
    /// mode from an extra string: the two addressing modes are different
    /// promises about who hears the beat and who may cancel it, and a caller
    /// should have to say which one they mean.
    Handle repeat_to_role(std::string id, std::chrono::milliseconds delay, std::string role,
                          Callback cb) {
        return declare(std::move(id), delay, /*repeat=*/true, std::move(role), cb);
    }

    /// The one-shot twin of repeat_to_role.
    Handle once_to_role(std::string id, std::chrono::milliseconds delay, std::string role,
                        Callback cb) {
        return declare(std::move(id), delay, /*repeat=*/false, std::move(role), cb);
    }

    std::size_t size() const { return bindings_.size(); }

private:
    template <class S, class State, class A, class E>
    friend class TimedWeave;
    friend class TimerHandle<Self>;

    Handle declare(std::string id, std::chrono::milliseconds delay, bool repeat_,
                   std::string role, Callback cb) {
        if (id.empty()) {
            throw std::invalid_argument("zengine::timer: a binding needs an id");
        }
        if (cb == nullptr) {
            throw std::invalid_argument("zengine::timer: binding '" + id + "' has no callback");
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
        b.desired = true;
        b.callback = cb;
        bindings_.push_back(std::move(b));
        return Handle{this, bindings_.size() - 1};
    }

    bool desired_at(std::size_t i) const { return i < bindings_.size() && bindings_[i].desired; }
    const std::string& id_at(std::size_t i) const { return bindings_[i].id; }

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
        bindings_[i].desired = false;
        mail.send_to_role(kTimerRole, CancelTimer{bindings_[i].id});
    }

    void restart_at(std::size_t i, loom::Mail& mail) {
        if (i >= bindings_.size()) {
            return;
        }
        bindings_[i].desired = true;
        ask(bindings_[i], mail);
    }

    /// Re-establish every wanted binding.
    ///
    /// CARDINALITY-IDEMPOTENT, NOT TIMING-NEUTRAL — and the difference is the
    /// whole reason this is spelled out rather than called "harmless". The
    /// Timer upserts on its existing keys, so re-asking never produces a second
    /// entry or a doubled beat. It DOES replace the schedule and re-anchor it:
    /// the next firing is a full delay from now, not from whenever the original
    /// ask landed. A binding reconciled mid-cycle therefore loses the remainder
    /// of that cycle. That is the correct trade for a service that may just have
    /// come back with an empty table, and it is not free.
    void reconcile(loom::Mail& mail) {
        for (const Binding<Self>& b : bindings_) {
            if (b.desired) {
                ask(b, mail);
            }
        }
    }

    void ask(const Binding<Self>& b, loom::Mail& mail) {
        if (b.role.empty()) {
            mail.send_to_role(kTimerRole, StartTimer{b.id, b.delay_ms, b.repeat});
        } else {
            mail.send_to_role(kTimerRole, StartRoleTimer{b.id, b.delay_ms, b.repeat, b.role});
        }
    }

    /// Route one firing to the ONE binding that asked for it.
    ///
    /// Exact id match, first and only. An id nobody declared is data, not a
    /// drive (a role beat can be aimed at by anyone), and two bindings can never
    /// both answer because two bindings can never share an id.
    void dispatch(Self* self, const TimerFired& f, loom::Mail& mail) {
        for (const Binding<Self>& b : bindings_) {
            if (b.id == f.id) {
                (self->*(b.callback))(f, mail);
                return;
            }
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
    : public loom::WeaveBase<Self, State,
                             loom::Accept<loom::Activated, TimerReady, TimerFired, A...>,
                             loom::Emit<StartTimer, StartRoleTimer, CancelTimer, E...>> {
public:
    using Bindings = TimerBindings<Self>;
    using Handle = TimerHandle<Self>;

    /// The declared-bindings table. Call the factories on it during
    /// construction; they record desire and send nothing.
    Bindings& timers() {
        // The one line of ceremony, checked here rather than left to template
        // soup: this is instantiated when an author calls timers() from their
        // constructor, which is exactly the moment Self is complete enough to
        // ask whether its handlers are visible.
        static_assert(
            requires(Self& s, const TimerFired& f, loom::Mail& m) { s.on(f, m); },
            "zengine::timer::TimedWeave: this weave's own on() handlers HIDE the binding "
            "layer's. Add `using TimedWeave::on;` to the class. (WeaveBase dispatches via "
            "self->on(...) on the derived type, and a derived on() hides every base one.)");
        return bindings_;
    }
    const Bindings& timers() const { return bindings_; }

    // ---- the ceremony, owned here so no author writes it again -------------

    /// This incarnation is live: establish everything it declared.
    ///
    /// Activation deduplication lives HERE, once, so consumers do not each
    /// carry a cursor. The cursor's caveat travels with it: sender plus
    /// sequence gives lineage and deduplication, NOT authentication.
    void on(const loom::Activated& a, loom::Mail& mail) {
        if (!activation_.accept(mail.sender(), a.sequence)) {
            return; // duplicate or replayed: nothing is re-established
        }
        bindings_.reconcile(mail);
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

protected:
    /// Visible to the author only so a subclass can read its own activation
    /// state if it genuinely needs to; the binding layer already acts on it.
    const zengine::ActivationCursor& activation() const { return activation_; }

private:
    zengine::ActivationCursor activation_; ///< per-incarnation, never state
    Bindings bindings_;
};

} // namespace zengine::timer

#endif // ZENGINE_TIMER_BINDING_HPP
