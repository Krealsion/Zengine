// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_INPUT_INPUT_WEAVE_HPP
#define ZENGINE_INPUT_INPUT_WEAVE_HPP

// The Input weave, over an injected Reader (the zen-ui-pixel move: the brain
// is testable everywhere, the platform is a thin edge). A Reader is anything
// with `std::vector<V> poll()`, for some variant V of published shapes — the
// real ones (input.cpp, input_sdl.cpp) fetch and translate native events; the
// suite's fake feeds scripted batches, so the weave's whole message contract is
// pinned without a console in sight. The weave's Emit set is DERIVED from V
// (see EmitsOf below), so a reader that can hand over one more kind of fact
// says so once, in its own file.
//
// The weave is INDIFFERENT to what a poll contains: it publishes whatever the
// reader hands back, by shape. That is why the vocabulary can change without
// changing a line of the pumping below — the moment a backend preserves is the
// reader's business, and delivering it is this weave's.
//
// The weave is DEAF until driven and says nothing on its own: a weave runs
// only when a message arrives. It DECLARES the drive it wants — a repeating
// role-addressed beat on kPumpTimerId — and the timer binding (timer/
// binding.hpp) owns the protocol that keeps it established. PumpInput
// (vocabulary.hpp's named addition) stays as the same hands on direct request,
// for suites and timer-less hosts. Everything it hears from the platform it
// publishes — by shape, to whoever accepts; it neither knows nor chooses its
// consumers.
//
// ROLE-ADDRESSED ON PURPOSE: the beat is kInputRole's pulse, not this
// incarnation's, so a successor inherits it rather than standing a second one
// beside it. This weave is also the binding's proof that the convenience is not
// secretly requester-only.

#include "translate.hpp"
#include "vocabulary.hpp"

#include "timer/binding.hpp"

#include <zen/weave.hpp>
#include <zen/weave/standard_shapes.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace zengine::input {

/// Two honest counters, poke-inspectable like any state: how often the weave
/// was given hands, and how many events it has spoken.
struct InputState {
    std::int64_t pumped = 0;
    std::int64_t emitted = 0;
    ZEN_EXPOSE();
    ZEN_SHAPE(InputState, 1, ZEN_FIELD(pumped), ZEN_FIELD(emitted));
};

namespace detail {

/// The variant a Reader's poll() hands back.
template <class Reader>
using ReaderEvent = typename std::decay_t<decltype(std::declval<Reader&>().poll())>::value_type;

/// That variant's alternatives, as the weave's Emit set.
///
/// DERIVED AND NOT SPELLED, and the reason is written a few lines up in this
/// file's own header: "The weave is INDIFFERENT to what a poll contains: it
/// publishes whatever the reader hands back, by shape." A hard-coded Emit list
/// would be the one place that was not indifferent — true of the readers that
/// happened to exist when it was written, and silently wrong for the next one.
///
/// The SDL reader broke that. SDL carries window LIFECYCLE and input on one
/// process-global queue, so the weave that owns the queue is the only thing
/// that can see a close request, and that request is not an input moment and
/// must not be spelled as one (translate_sdl.hpp). Deriving the Emit set is
/// what lets the SDL reader declare the extra shape WITHOUT the Input package
/// itself gaining a surface dependency, and without the terminal and Win32
/// weaves advertising a fact they can never produce. What each weave says it
/// can say is now exactly what its reader can hand it.
///
/// ...PLUS THE FOUR ANSWERS A SESSION DOOR GIVES. They are spelled here rather than derived
/// because they are this weave's own and not a reader's: the session vocabulary is the same
/// for every backend, and an answer crosses the library seam as bytes the host resolves
/// against the registry, so a shape only this weave ever says must be declared by it.
template <class V>
struct EmitsOf;
template <class... Ts>
struct EmitsOf<std::variant<Ts...>> {
    using type = loom::Emit<Ts..., InputSessionOpened, InputInjected, loom::Ack, loom::Refused>;
};

} // namespace detail

template <class Reader>
class InputWeaveT;

/// The base, named once — three spellings of it is what the `using ...::on`
/// line below used to cost.
template <class Reader>
using InputWeaveBase =
    zengine::timer::TimedWeave<InputWeaveT<Reader>, InputState,
                               loom::Accept<PumpInput, InputSessionRequested, InputSessionClosed,
                                            InjectInput>,
                               typename detail::EmitsOf<detail::ReaderEvent<Reader>>::type>;

template <class Reader>
class InputWeaveT : public InputWeaveBase<Reader> {
public:
    InputWeaveT() { declare_pump(); }
    explicit InputWeaveT(Reader reader) : reader_(std::move(reader)) { declare_pump(); }

    /// The one line of ceremony the binding cannot remove: this weave has its
    /// own `on` handler, which would otherwise HIDE the binding layer's three.
    /// (WeaveBase dispatches via self->on(...) on the derived type.)
    using InputWeaveBase<Reader>::on;

    /// The direct door: the same hands, on request, for suites, diagnostics and
    /// timer-less hosts.
    void on(const PumpInput&, loom::Mail& mail) { pump(mail); }

    // ---- the session doors (vocabulary.hpp says what a session is and is not) ----------

    void on(const InputSessionRequested& asked, loom::Mail& mail) {
        if (session_.open) {
            (void)mail.answer(loom::Refused{
                "busy: input session " + std::to_string(session_.id) + " is held by " +
                (session_.holder == mail.sender() ? std::string("you") : "another participant") +
                "; it must be closed before another opens"});
            return;
        }
        session_ = Session{};
        session_.open = true;
        session_.id = ++last_session_;
        session_.holder = mail.sender();
        session_.purpose = asked.purpose;
        (void)mail.answer(InputSessionOpened{session_.id});
    }

    void on(const InputSessionClosed& closed, loom::Mail& mail) {
        // THE HOLDER, PERSONALLY; OR AN OFFICE, DELIBERATELY, ON THE HOLDER'S BEHALF. A
        // stranger's personal word naming the holder is refused: office speech is the one
        // fact Loom verified at authorship, and a guest's proxy holds no office. An office
        // may say session 0 -- "whatever this holder holds" -- because the door that closes
        // a dead guest's session knows the guest, not the number.
        const bool by_office = !mail.authored_role().empty() && session_.open &&
                               closed.holder == static_cast<std::int64_t>(session_.holder.value);
        const bool names_it = closed.session == session_.id || (by_office && closed.session == 0);
        if (!session_.open || !names_it) {
            (void)mail.answer(loom::Refused{"no open input session numbered " +
                                            std::to_string(closed.session)});
            return;
        }
        const bool by_holder = mail.sender() == session_.holder;
        if (!by_holder && !by_office) {
            (void)mail.answer(loom::Refused{
                "input session " + std::to_string(session_.id) +
                " is closed by its holder, or by an office closing it on the holder's behalf"});
            return;
        }
        release_held(mail);
        session_ = Session{};
        (void)mail.answer(loom::Ack{});
    }

    void on(const InjectInput& batch, loom::Mail& mail) {
        if (!session_.open || batch.session != session_.id) {
            (void)mail.answer(loom::Refused{"no open input session numbered " +
                                            std::to_string(batch.session)});
            return;
        }
        if (mail.sender() != session_.holder) {
            (void)mail.answer(loom::Refused{"input session " + std::to_string(session_.id) +
                                            " is another participant's"});
            return;
        }
        if (batch.events.size() > kMaxInjectedEvents) {
            (void)mail.answer(loom::Refused{"a batch carries at most " +
                                            std::to_string(kMaxInjectedEvents) + " moments; this one has " +
                                            std::to_string(batch.events.size())});
            return;
        }
        // JUDGED WHOLE BEFORE ANYTHING IS PUBLISHED: a batch with one bad moment publishes
        // nothing, so an agent never has to guess how far a refused batch got. Each moment is
        // judged on its own and against the keys the session WOULD hold down after the moments
        // before it -- so a batch that passes the held-key bound at any moment is refused whole,
        // before one press is published or one key is remembered.
        std::vector<std::int64_t> would_hold = session_.keys_down;
        for (std::size_t i = 0; i < batch.events.size(); ++i) {
            std::string why;
            if (!well_formed(batch.events[i], &why) || !holds(batch.events[i], would_hold, &why)) {
                (void)mail.answer(loom::Refused{"moment " + std::to_string(i) + ": " + why});
                return;
            }
        }
        InputInjected done;
        done.session = session_.id;
        done.first_seq = session_.seq + 1;
        for (const InjectedEvent& e : batch.events) {
            publish_one(e, mail);
        }
        done.last_seq = session_.seq;
        done.admitted = static_cast<std::int64_t>(batch.events.size());
        (void)mail.answer(done);
    }

    /// The open session, for a host presenting it. 0 when none is open.
    std::int64_t open_session() const noexcept { return session_.open ? session_.id : 0; }
    loom::WeaveId session_holder() const noexcept { return session_.holder; }

private:
    /// ONE SESSION, plain members and never state: a session belongs to an incarnation, and a
    /// successor that inherited a held key it never heard pressed would release nothing.
    struct Session {
        bool open = false;
        std::int64_t id = 0;
        loom::WeaveId holder{};
        std::string purpose;
        std::int64_t seq = 0;
        std::vector<std::int64_t> keys_down;    ///< scancodes pressed and not yet released
        std::vector<std::int64_t> buttons_down; ///< buttons pressed and not yet released
        std::int64_t last_x = 0;
        std::int64_t last_y = 0;
        std::int64_t last_space = space::kUnknown;
    };

    static bool well_formed(const InjectedEvent& e, std::string* why) {
        const bool pointer = e.kind == "PointerMoved" || e.kind == "PointerButton" ||
                             e.kind == "PointerWheel";
        if (e.kind != "KeyPressed" && e.kind != "KeyReleased" && e.kind != "TextEntered" &&
            !pointer) {
            *why = "unknown kind '" + e.kind + "'";
            return false;
        }
        if (pointer && e.space != space::kCells && e.space != space::kPixels) {
            *why = "a pointer moment needs a known space (cells or pixels)";
            return false;
        }
        if ((e.kind == "KeyPressed" || e.kind == "KeyReleased") &&
            (e.scancode < 1 || e.scancode > kMaxScancode)) {
            *why = "scancode " + std::to_string(e.scancode) +
                   " is outside the supported key domain 1.." + std::to_string(kMaxScancode);
            return false;
        }
        if (e.kind == "PointerButton" && (e.button < 1 || e.button > 3)) {
            *why = "a button is 1, 2 or 3";
            return false;
        }
        if (e.kind == "TextEntered" && e.text.empty()) {
            *why = "TextEntered carries text";
            return false;
        }
        return true;
    }

    /// WHAT THE SESSION WOULD HOLD after this moment, carried forward in `held`: a press of a
    /// key already down holds nothing new (an auto-repeat), a release lets one go, and a press
    /// past `kMaxHeldKeys` is refused -- the whole batch with it.
    static bool holds(const InjectedEvent& e, std::vector<std::int64_t>& held, std::string* why) {
        if (e.kind == "KeyPressed") {
            for (const std::int64_t k : held) {
                if (k == e.scancode) {
                    return true;
                }
            }
            if (held.size() >= kMaxHeldKeys) {
                *why = "pressing scancode " + std::to_string(e.scancode) + " would hold " +
                       std::to_string(held.size() + 1) + " keys down at once; a session holds at "
                       "most " + std::to_string(kMaxHeldKeys) + " -- release one first";
                return false;
            }
            held.push_back(e.scancode);
        } else if (e.kind == "KeyReleased") {
            forget(held, e.scancode);
        }
        return true;
    }

    template <class T>
    static void remember(std::vector<T>& v, T x) {
        for (const T& have : v) {
            if (have == x) {
                return;
            }
        }
        v.push_back(x);
    }
    template <class T>
    static void forget(std::vector<T>& v, T x) {
        for (auto it = v.begin(); it != v.end(); ++it) {
            if (*it == x) {
                v.erase(it);
                return;
            }
        }
    }

    void publish_one(const InjectedEvent& e, loom::Mail& mail) {
        ++session_.seq;
        if (e.kind == "KeyPressed") {
            remember(session_.keys_down, e.scancode);
            mail.publish(KeyPressed{e.scancode, e.name, e.modifiers});
        } else if (e.kind == "KeyReleased") {
            forget(session_.keys_down, e.scancode);
            mail.publish(KeyReleased{e.scancode, e.name, e.modifiers});
        } else if (e.kind == "TextEntered") {
            mail.publish(TextEntered{e.text});
        } else if (e.kind == "PointerMoved") {
            session_.last_x = e.x;
            session_.last_y = e.y;
            session_.last_space = e.space;
            mail.publish(PointerMoved{e.x, e.y, e.dx, e.dy, e.space, e.modifiers});
        } else if (e.kind == "PointerButton") {
            session_.last_x = e.x;
            session_.last_y = e.y;
            session_.last_space = e.space;
            if (e.pressed) {
                remember(session_.buttons_down, e.button);
            } else {
                forget(session_.buttons_down, e.button);
            }
            mail.publish(PointerButton{e.button, e.pressed, e.x, e.y, e.space, e.modifiers});
        } else {
            session_.last_x = e.x;
            session_.last_y = e.y;
            session_.last_space = e.space;
            mail.publish(PointerWheel{e.wheel_dx, e.wheel_dy, e.x, e.y, e.space, e.modifiers});
        }
    }

    /// EVERYTHING THE SESSION STILL HELD DOWN COMES UP, as ordinary published moments, before
    /// the session is gone -- so a consumer that saw the press sees the release from the same
    /// producer, and no held button quietly survives into unrelated work.
    void release_held(loom::Mail& mail) {
        for (const std::int64_t sc : session_.keys_down) {
            ++session_.seq;
            mail.publish(KeyReleased{sc, std::string(), mod::kNone});
        }
        for (const std::int64_t b : session_.buttons_down) {
            ++session_.seq;
            mail.publish(PointerButton{b, false, session_.last_x, session_.last_y,
                                       session_.last_space == space::kUnknown ? space::kCells
                                                                              : session_.last_space,
                                       mod::kNone});
        }
        session_.keys_down.clear();
        session_.buttons_down.clear();
    }

    void declare_pump() {
        pump_ = this->timers().repeat_to_role(kPumpTimerId,
                                              std::chrono::milliseconds(kPumpBeatMs), kInputRole,
                                              &InputWeaveT::on_pump_beat);
    }

    /// The beat: the same hands PumpInput opens, on the clock's schedule. No id
    /// filtering here — the binding routed this firing to this callback because
    /// this callback is the one that asked for that id.
    void on_pump_beat(const zengine::timer::TimerFired&, loom::Mail& mail) { pump(mail); }

    void pump(loom::Mail& mail) {
        ++this->state_.pumped;
        for (const detail::ReaderEvent<Reader>& ev : reader_.poll()) {
            ++this->state_.emitted;
            std::visit([&mail](const auto& e) { mail.publish(e); }, ev);
        }
    }

    typename InputWeaveBase<Reader>::Handle pump_;
    Reader reader_;
    Session session_;
    std::int64_t last_session_ = 0;
};

} // namespace zengine::input

#endif // ZENGINE_INPUT_INPUT_WEAVE_HPP
