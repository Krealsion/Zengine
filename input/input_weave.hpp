// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_INPUT_INPUT_WEAVE_HPP
#define ZENGINE_INPUT_INPUT_WEAVE_HPP

// The Input weave, over an injected Reader: anything with `std::vector<V> poll()` for a variant
// V of published shapes (input.cpp and input_sdl.cpp translate native events; the suite's fake
// feeds scripted batches). It publishes whatever a poll hands back, by shape, and its Emit set
// is derived from V. It declares a repeating beat on `kPumpTimerId`, addressed to `kInputRole`
// so a successor inherits it, and the timer binding keeps it established; `PumpInput` is the
// same hands on direct request, for suites and timer-less hosts.
// Reference: docs/reference/input.md.

#include "component/motion.hpp"
#include "translate.hpp"
#include "vocabulary.hpp"

#include "timer/binding.hpp"

#include <zen/weave.hpp>
#include <zen/weave/standard_shapes.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <cmath>
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

/// That variant's alternatives, as the weave's Emit set: derived, not spelled, so each weave
/// declares exactly what its reader can hand it -- the SDL reader adds the surface close request
/// without the Input package depending on Surface -- plus the answers the session doors give,
/// which are this weave's own for every backend.
template <class V>
struct EmitsOf;
template <class... Ts>
struct EmitsOf<std::variant<Ts...>> {
    using type = loom::Emit<Ts..., InputSessionOpened, InputInjected, AttributedInput, loom::Ack, loom::Refused>;
};

} // namespace detail

template <class Reader>
class InputWeaveT;

/// The base, named once.
template <class Reader>
using InputWeaveBase =
    zengine::timer::TimedWeave<InputWeaveT<Reader>, InputState,
                               loom::Accept<PumpInput, InputSessionRequested, InputSessionClosed,
                                            InjectInput, PointerMotionRequested>,
                               typename detail::EmitsOf<detail::ReaderEvent<Reader>>::type>;

template <class Reader>
class InputWeaveT : public InputWeaveBase<Reader> {
public:
    InputWeaveT() { declare_pump(); }
    using Clock = std::chrono::steady_clock;
    using Now = std::function<Clock::time_point()>;
    explicit InputWeaveT(Reader reader, Now now = [] { return Clock::now(); })
        : reader_(std::move(reader)), now_(std::move(now)) { declare_pump(); }

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
        if (motion_.answer.valid()) {
            (void)loom::answer_deferred(motion_.answer, mail, loom::Refused{"pointer motion cancelled: session closed"});
            motion_ = {};
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
        if (motion_.answer.valid()) {
            (void)mail.answer(loom::Refused{"pointer motion is in progress"});
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

    void on(const PointerMotionRequested& request, loom::Mail& mail) {
        constexpr std::int64_t bound = 1000000;
        if (!session_.open || request.session != session_.id || mail.sender() != session_.holder) {
            (void)mail.answer(loom::Refused{"pointer motion requires the held input session"}); return;
        }
        if (motion_.answer.valid() || session_.last_space == space::kUnknown ||
            request.duration_ms < 1 || request.duration_ms > 60000 ||
            request.x < -bound || request.x > bound || request.y < -bound || request.y > bound ||
            session_.last_x < -bound || session_.last_x > bound ||
            session_.last_y < -bound || session_.last_y > bound ||
            !std::isfinite(request.bend) || std::abs(request.bend) > bound) {
            (void)mail.answer(loom::Refused{"motion needs an idle session, known bounded pointer position, duration 1..60000 ms and finite bend"}); return;
        }
        auto answer = mail.defer_answer();
        if (!answer.valid()) { (void)mail.answer(loom::Refused{"motion needs an answerable request"}); return; }
        motion_.answer = std::move(answer);
        motion_.path = component::motion::Path::curved(
            {static_cast<double>(session_.last_x), static_cast<double>(session_.last_y)},
            {static_cast<double>(request.x), static_cast<double>(request.y)}, request.bend);
        motion_.started = now_();
        motion_.duration = request.duration_ms;
        motion_.first = session_.seq + 1;
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
            emit_event(KeyPressed{e.scancode, e.name, e.modifiers}, mail, false);
        } else if (e.kind == "KeyReleased") {
            forget(session_.keys_down, e.scancode);
            emit_event(KeyReleased{e.scancode, e.name, e.modifiers}, mail, false);
        } else if (e.kind == "TextEntered") {
            emit_event(TextEntered{e.text}, mail, false);
        } else if (e.kind == "PointerMoved") {
            session_.last_x = e.x;
            session_.last_y = e.y;
            session_.last_space = e.space;
            emit_event(PointerMoved{e.x, e.y, e.dx, e.dy, e.space, e.modifiers}, mail, false);
        } else if (e.kind == "PointerButton") {
            session_.last_x = e.x;
            session_.last_y = e.y;
            session_.last_space = e.space;
            if (e.pressed) {
                remember(session_.buttons_down, e.button);
            } else {
                forget(session_.buttons_down, e.button);
            }
            emit_event(PointerButton{e.button, e.pressed, e.x, e.y, e.space, e.modifiers}, mail, false);
        } else {
            session_.last_x = e.x;
            session_.last_y = e.y;
            session_.last_space = e.space;
            emit_event(PointerWheel{e.wheel_dx, e.wheel_dy, e.x, e.y, e.space, e.modifiers}, mail, false);
        }
    }

    /// EVERYTHING THE SESSION STILL HELD DOWN COMES UP, as ordinary published moments, before
    /// the session is gone -- so a consumer that saw the press sees the release from the same
    /// producer, and no held button quietly survives into unrelated work.
    void release_held(loom::Mail& mail) {
        for (const std::int64_t sc : session_.keys_down) {
            ++session_.seq;
            emit_event(KeyReleased{sc, std::string(), mod::kNone}, mail, false);
        }
        for (const std::int64_t b : session_.buttons_down) {
            ++session_.seq;
            emit_event(PointerButton{b, false, session_.last_x, session_.last_y,
                                       session_.last_space == space::kUnknown ? space::kCells
                                                                              : session_.last_space,
                                       mod::kNone}, mail, false);
        }
        session_.keys_down.clear();
        session_.buttons_down.clear();
    }

    template <class Event>
    void emit_event(const Event& event, loom::Mail& mail, bool local) {
        InjectedEvent e;
        if constexpr (std::is_same_v<Event, KeyPressed> || std::is_same_v<Event, KeyReleased>) {
            e.kind = std::is_same_v<Event, KeyPressed> ? "KeyPressed" : "KeyReleased";
            e.scancode = event.scancode; e.name = event.name; e.modifiers = event.modifiers;
        } else if constexpr (std::is_same_v<Event, TextEntered>) {
            e.kind = "TextEntered"; e.text = event.text;
        } else if constexpr (std::is_same_v<Event, PointerMoved> ||
                             std::is_same_v<Event, PointerButton> ||
                             std::is_same_v<Event, PointerWheel>) {
            e.x = event.x; e.y = event.y; e.space = event.space; e.modifiers = event.modifiers;
            if constexpr (std::is_same_v<Event, PointerMoved>) {
                e.kind = "PointerMoved"; e.dx = event.dx; e.dy = event.dy;
            } else if constexpr (std::is_same_v<Event, PointerButton>) {
                e.kind = "PointerButton"; e.button = event.button; e.pressed = event.pressed;
            } else {
                e.kind = "PointerWheel"; e.wheel_dx = event.dx; e.wheel_dy = event.dy;
            }
        }
        if (!e.kind.empty()) {
            (void)mail.as_role(kInputRole).publish(AttributedInput{
                local, local ? 0 : static_cast<std::int64_t>(session_.holder.value), std::move(e)});
        }
        mail.publish(event);
    }

    struct Motion {
        loom::DeferredAnswer answer;
        component::motion::Path path;
        Clock::time_point started;
        std::int64_t duration = 0, first = 0;
    };
    void advance_motion(loom::Mail& mail) {
        if (!motion_.answer.valid()) return;
        const double elapsed = std::chrono::duration<double, std::milli>(now_()-motion_.started).count();
        const double t = component::motion::progress(elapsed, static_cast<double>(motion_.duration));
        const auto p = motion_.path.at(t);
        const auto x = static_cast<std::int64_t>(std::llround(p.x));
        const auto y = static_cast<std::int64_t>(std::llround(p.y));
        if (x != session_.last_x || y != session_.last_y || t == 1) {
            InjectedEvent e;
            e.kind = "PointerMoved"; e.x = x; e.y = y; e.space = session_.last_space;
            e.dx = x-session_.last_x; e.dy = y-session_.last_y;
            publish_one(e, mail);
        }
        if (t == 1) {
            const InputInjected done{session_.id, session_.seq-motion_.first+1,
                                     motion_.first, session_.seq};
            (void)loom::answer_deferred(motion_.answer, mail, done);
            motion_ = {};
        }
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
        advance_motion(mail);
        for (const detail::ReaderEvent<Reader>& ev : reader_.poll()) {
            ++this->state_.emitted;
            std::visit([&](const auto& e) { emit_event(e, mail, true); }, ev);
        }
    }

    typename InputWeaveBase<Reader>::Handle pump_;
    Reader reader_;
    Session session_;
    Now now_ = [] { return Clock::now(); };
    Motion motion_;
    std::int64_t last_session_ = 0;
};

} // namespace zengine::input

#endif // ZENGINE_INPUT_INPUT_WEAVE_HPP
