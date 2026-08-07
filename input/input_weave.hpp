// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_INPUT_INPUT_WEAVE_HPP
#define ZENGINE_INPUT_INPUT_WEAVE_HPP

// The Input weave, over an injected Reader (the zen-ui-pixel move: the brain
// is testable everywhere, the platform is a thin edge). A Reader is anything
// with `std::vector<InputEvent> poll()` — the real ones (input.cpp) fetch and
// translate native events; the suite's fake feeds scripted batches, so the
// weave's whole message contract is pinned without a console in sight.
//
// The weave is INDIFFERENT to what a poll contains: it publishes whatever the
// reader hands back, by shape. That is why W-4 changed the vocabulary without
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
#include <utility>
#include <variant>

namespace zengine::input {

/// Two honest counters, poke-inspectable like any state: how often the weave
/// was given hands, and how many events it has spoken.
struct InputState {
    std::int64_t pumped = 0;
    std::int64_t emitted = 0;
    ZEN_EXPOSE();
    ZEN_SHAPE(InputState, 1, ZEN_FIELD(pumped), ZEN_FIELD(emitted));
};

template <class Reader>
class InputWeaveT
    : public zengine::timer::TimedWeave<InputWeaveT<Reader>, InputState,
                                        loom::Accept<PumpInput>,
                                        loom::Emit<KeyPressed, KeyReleased, TextEntered,
                                                   PointerMoved, PointerButton, PointerWheel>> {
public:
    InputWeaveT() { declare_pump(); }
    explicit InputWeaveT(Reader reader) : reader_(std::move(reader)) { declare_pump(); }

    /// The one line of ceremony the binding cannot remove: this weave has its
    /// own `on` handler, which would otherwise HIDE the binding layer's three.
    /// (WeaveBase dispatches via self->on(...) on the derived type.)
    using zengine::timer::TimedWeave<InputWeaveT<Reader>, InputState, loom::Accept<PumpInput>,
                                     loom::Emit<KeyPressed, KeyReleased, TextEntered, PointerMoved,
                                                PointerButton, PointerWheel>>::on;

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
        for (const InputEvent& ev : reader_.poll()) {
            ++this->state_.emitted;
            std::visit([&mail](const auto& e) { mail.publish(e); }, ev);
        }
    }

    typename InputWeaveT::Handle pump_;
    Reader reader_;
};

} // namespace zengine::input

#endif // ZENGINE_INPUT_INPUT_WEAVE_HPP
