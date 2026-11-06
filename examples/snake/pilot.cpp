// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

// The snake's pilot, a participant that plays the game live: it hears every `SnakeVisual` the
// world publishes and answers each with one `SnakeTurn`, its choice pilot.hpp's. It never steps
// the world: the world's own beat is the game's time. When it is activated it takes the wheel,
// writing the world's `keys` false so a player's keys and the pilot never steer at once; Loom
// tells no weave it is unloaded, so whoever lands it gives the keys back (the host's operator,
// play.cpp). Reference: examples/snake/README.md.

#include "pilot.hpp"
#include "vocabulary.hpp"

#include "activation/activation.hpp"

#include <zen/kernel/export.hpp>
#include <zen/weave.hpp>
#include <zen/weave/lifecycle.hpp>
#include <zen/weave/poke.hpp>

#include <cstdint>

namespace {

using namespace zengine::snake;

/// What the pilot has done: the steps it heard, the turns it sent, and whether the world said
/// its keys are off.
struct SnakePilotState {
    std::int64_t heard = 0;
    std::int64_t turns = 0;
    bool wheel = false;
    ZEN_EXPOSE();
    ZEN_SHAPE(SnakePilotState, 1, ZEN_FIELD(heard), ZEN_FIELD(turns), ZEN_FIELD(wheel));
};

class SnakePilot
    : public loom::WeaveBase<SnakePilot, SnakePilotState,
                             loom::Accept<SnakeVisual, loom::Activated, loom::Ack, loom::Refused>,
                             loom::Emit<SnakeTurn, loom::PokeWrite>> {
public:
    /// Each step the world takes, steered: one turn for the next.
    void on(const SnakeVisual& v, loom::Mail& mail) {
        ++state_.heard;
        if (!v.alive || v.snake.empty()) {
            return;
        }
        mail.send_to_role(kWorldRole, SnakeTurn{pilot_choice(v)});
        ++state_.turns;
    }

    /// Live: the wheel taken, the world's own keys written off.
    void on(const loom::Activated& a, loom::Mail& mail) {
        if (!activation_.accept(mail, a)) {
            return;
        }
        state_.wheel = false;
        asked_ = static_cast<std::uint64_t>(activation_.sequence());
        mail.send_to_role(kWorldRole, loom::PokeWrite{"keys", "false"}, asked_);
    }

    /// The world's answer to that write, and only that one: Loom's word that it answers the ask.
    void on(const loom::Ack&, loom::Mail& mail) {
        if (answers(mail)) {
            state_.wheel = true;
        }
    }
    void on(const loom::Refused&, loom::Mail& mail) {
        if (answers(mail)) {
            state_.wheel = false;
        }
    }

private:
    bool answers(const loom::Mail& mail) const {
        return asked_ != 0 && mail.answers_ask() && mail.correlation() == asked_;
    }

    zengine::ActivationCursor activation_;
    std::uint64_t asked_ = 0;
};

} // namespace

ZEN_EXPORT_WEAVE(SnakePilot)
