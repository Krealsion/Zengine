// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The controls adapter: snake's own input binding, as a weave. WASD and the arrows become
// `SnakeTurn`, sent by role to whoever holds `snake.world` at delivery, so steering survives the
// world being swapped; everything else is not its business. Replaceable (remapped keys, an AI
// pilot, a replay feeder), and it accepts exactly the door it uses: `KeyPressed`.
// Reference: docs/reference/snake.md.

#include "vocabulary.hpp"

#include "input/vocabulary.hpp"

#include <zen/kernel/export.hpp>
#include <zen/weave.hpp>

#include <cstdint>

namespace {

using namespace zengine::snake;

/// One honest counter: how many turns this binding has spoken for its player.
struct ControlsState {
    std::int64_t turns = 0;
    ZEN_EXPOSE();
    ZEN_SHAPE(ControlsState, 1, ZEN_FIELD(turns));
};

class SnakeControls
    : public loom::WeaveBase<SnakeControls, ControlsState,
                             loom::Accept<zengine::input::KeyPressed>, loom::Emit<SnakeTurn>> {
public:
    void on(const zengine::input::KeyPressed& k, loom::Mail& mail) {
        namespace scan = zengine::input::scan;
        std::int64_t dir = 0;
        switch (k.scancode) {
        case scan::kW:
        case scan::kUp: dir = kUp; break;
        case scan::kA:
        case scan::kLeft: dir = kLeft; break;
        case scan::kS:
        case scan::kDown: dir = kDown; break;
        case scan::kD:
        case scan::kRight: dir = kRight; break;
        default: return; // not a steering key; not this weave's business
        }
        ++state_.turns;
        mail.send_to_role(kWorldRole, SnakeTurn{dir});
    }
};

} // namespace

ZEN_EXPORT_WEAVE(SnakeControls)
