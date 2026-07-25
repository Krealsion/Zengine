#ifndef ZENGINE_INPUT_INPUT_WEAVE_HPP
#define ZENGINE_INPUT_INPUT_WEAVE_HPP

// The Input weave, over an injected Reader (the zen-ui-pixel move: the brain
// is testable everywhere, the platform is a thin edge). A Reader is anything
// with `std::vector<InputEvent> poll()` — the real ones (input.cpp) fetch and
// translate native events; the suite's fake feeds scripted batches, so the
// weave's whole message contract is pinned without a console in sight.
//
// The weave is DEAF until driven and says nothing on its own: a weave runs
// only when a message arrives. It arranges its own drive — on the Timer
// package's hello it asks for the kPumpTimerId role beat and polls on each
// firing — and PumpInput (vocabulary.hpp's named addition) stays as the same
// hands on direct request, for suites and timer-less hosts. Everything it
// hears from the platform it publishes — by shape, to whoever accepts; it
// neither knows nor chooses its consumers.

#include "translate.hpp"
#include "vocabulary.hpp"

#include "timer/vocabulary.hpp"

#include <zen/weave.hpp>

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
    : public loom::WeaveBase<InputWeaveT<Reader>, InputState,
                             loom::Accept<PumpInput, zengine::timer::TimerReady,
                                          zengine::timer::TimerFired>,
                             loom::Emit<KeyPressed, KeyReleased, MouseButton, MouseMoved,
                                        MouseWheel, zengine::timer::StartRoleTimer>> {
public:
    InputWeaveT() = default;
    explicit InputWeaveT(Reader reader) : reader_(std::move(reader)) {}

    void on(const PumpInput&, loom::Mail& mail) { pump(mail); }

    /// The TimerService woke: ask for our beat. Role-addressed — the beat is
    /// kInputRole's pulse, not this incarnation's — and an upsert on the
    /// service's side, so re-hearing the hello re-asks harmlessly.
    void on(const zengine::timer::TimerReady&, loom::Mail& mail) {
        mail.send_to_role(zengine::timer::kTimerRole,
                          zengine::timer::StartRoleTimer{kPumpTimerId, kPumpBeatMs,
                                                         /*repeat=*/true, kInputRole});
    }

    /// The beat: the same hands PumpInput opens, on the clock's schedule. Any
    /// other id aimed at this role is someone else's ask — data, not a drive.
    void on(const zengine::timer::TimerFired& f, loom::Mail& mail) {
        if (f.id != kPumpTimerId) {
            return;
        }
        pump(mail);
    }

private:
    void pump(loom::Mail& mail) {
        ++this->state_.pumped;
        for (const InputEvent& ev : reader_.poll()) {
            ++this->state_.emitted;
            std::visit([&mail](const auto& e) { mail.publish(e); }, ev);
        }
    }

    Reader reader_;
};

} // namespace zengine::input

#endif // ZENGINE_INPUT_INPUT_WEAVE_HPP
