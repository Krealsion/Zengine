// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
//
// A loadable weave written entirely against the INSTALLED Zengine package. It takes an order
// with a delay that is runtime data, so it speaks the raw Timer protocol rather than declaring
// an authored rhythm -- the split documented in the Timer's own reference.
//
// The only Zengine headers here are the two the package publishes for the Timer, spelled the
// way the documentation spells them. Nothing in this file knows where Zengine's source or
// build tree is, and it must stay that way: that is what this fixture exists to prove.

#include "kitchen.hpp"

#include "timer/binding.hpp"
#include "timer/vocabulary.hpp"

#include <zen/kernel/export.hpp>
#include <zen/weave.hpp>

#include <cstdint>
#include <string>

namespace {

namespace timer = zengine::timer;
using kitchen::BakeDone;
using kitchen::BakeOrder;

struct OvenState {
    std::string baking;      ///< the dish currently in the oven, or empty
    std::int64_t served = 0; ///< how many have come out
    ZEN_EXPOSE();
    ZEN_SHAPE(OvenState, 1, ZEN_FIELD(baking), ZEN_FIELD(served));
};

constexpr const char* kBakeTimer = "kitchen.oven.bake";

class Oven : public timer::TimedWeave<Oven, OvenState, loom::Accept<BakeOrder>,
                                      loom::Emit<BakeDone>> {
public:
    using TimedWeave::on; // required: keeps the layer's own handlers reachable

    /// An order arrives. Place a one-shot for the ordered delay.
    void on(const BakeOrder& order, loom::Mail& mail) {
        state_.baking = order.dish;
        mail.send_to_role(timer::kTimerRole,
                          timer::StartTimer{kBakeTimer, order.minutes, /*repeat=*/false});
    }

    /// The oven's own timer fired. Serve it.
    void on(const timer::TimerFired& fired, loom::Mail& mail) {
        if (fired.id != kBakeTimer || state_.baking.empty()) {
            return;
        }
        ++state_.served;
        mail.publish(BakeDone{state_.baking});
        state_.baking.clear();
    }
};

} // namespace

ZEN_EXPORT_WEAVE(Oven)
