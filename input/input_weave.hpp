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

#include <chrono>
#include <cstdint>
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
template <class V>
struct EmitsOf;
template <class... Ts>
struct EmitsOf<std::variant<Ts...>> {
    using type = loom::Emit<Ts...>;
};

} // namespace detail

template <class Reader>
class InputWeaveT;

/// The base, named once — three spellings of it is what the `using ...::on`
/// line below used to cost.
template <class Reader>
using InputWeaveBase =
    zengine::timer::TimedWeave<InputWeaveT<Reader>, InputState, loom::Accept<PumpInput>,
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

private:
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
};

} // namespace zengine::input

#endif // ZENGINE_INPUT_INPUT_WEAVE_HPP
