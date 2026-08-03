// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The controls adapter — snake's own input binding, as a weave.
//
// The Input package speaks keyboards (KeyPressed, SDL scancodes); the world
// speaks intent (SnakeTurn) and deliberately never learns what a keyboard is.
// This weave is the whole distance between them: WASD and the arrows become
// turns, everything else is not its business. Because the binding is a weave
// and not a line in the world, it is REPLACEABLE like everything else here —
// a remapped-keys adapter, an AI pilot, or a replay feeder can take its place
// (or stand beside it) without the world or the Input package changing a line.
//
// It addresses the world BY ROLE, exactly as the host's own loop always did:
// the turn goes to whoever holds snake.world at delivery, so steering survives
// the world being swapped mid-game (moment 3 does exactly that).
//
// It accepts exactly the door it uses: KeyPressed. Releases and the mouse are
// simply not in its accept-set — pub-sub means they cost it nothing, and its
// silhouette stays an honest statement of what it consumes.

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
