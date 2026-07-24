#ifndef ZENGINE_INPUT_INPUT_WEAVE_HPP
#define ZENGINE_INPUT_INPUT_WEAVE_HPP

// The Input weave, over an injected Reader (the zen-ui-pixel move: the brain
// is testable everywhere, the platform is a thin edge). A Reader is anything
// with `std::vector<InputEvent> poll()` — the real ones (input.cpp) fetch and
// translate native events; the suite's fake feeds scripted batches, so the
// weave's whole message contract is pinned without a console in sight.
//
// The weave is DEAF until pumped and says nothing on its own: a weave runs
// only when a message arrives, so PumpInput (vocabulary.hpp's named addition)
// is what opens its hands. Everything it hears from the platform it publishes
// — by shape, to whoever accepts; it neither knows nor chooses its consumers.

#include "translate.hpp"
#include "vocabulary.hpp"

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
    : public loom::WeaveBase<InputWeaveT<Reader>, InputState, loom::Accept<PumpInput>,
                             loom::Emit<KeyPressed, KeyReleased, MouseButton, MouseMoved,
                                        MouseWheel>> {
public:
    InputWeaveT() = default;
    explicit InputWeaveT(Reader reader) : reader_(std::move(reader)) {}

    void on(const PumpInput&, loom::Mail& mail) {
        ++this->state_.pumped;
        for (const InputEvent& ev : reader_.poll()) {
            ++this->state_.emitted;
            std::visit([&mail](const auto& e) { mail.publish(e); }, ev);
        }
    }

private:
    Reader reader_;
};

} // namespace zengine::input

#endif // ZENGINE_INPUT_INPUT_WEAVE_HPP
