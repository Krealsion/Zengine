// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// `snake-board`: the board's code, opened by a window that presents a `SnakeBoardData`. It draws
// an outline one cell outside the board and a node a filled cell (`c.<cell>`): the head in the
// accent, the body in the fill, the food in the alert, so any skin paints it as it paints any
// rectangle. It takes no press: a press on the board reaches the world that drew it.
// Reference: examples/snake/README.md.

#include "board.hpp"

#include "graphics/element_export.hpp"

#include <algorithm>
#include <string>

namespace {

namespace gfx = zengine::graphics;
using namespace zengine::snake;

struct SnakeBoardCode {
    using Data = SnakeBoardData;
    using State = SnakeBoardState;
    using Changes = SnakeBoardChanges;

    static State start(const Data& d) { return State{d, false}; }

    static void change(State& s, const SnakeStep& step) {
        auto& body = s.board.body;
        if (step.freed >= 0) {
            const auto it = std::find(body.rbegin(), body.rend(), step.freed);
            if (it != body.rend()) {
                body.erase(std::next(it).base());
            }
        }
        if (step.head >= 0) {
            body.insert(body.begin(), step.head);
        }
        if (step.food >= 0) {
            s.board.food = step.food;
        }
        s.dead = s.dead || step.dead;
    }

    static void change(State& s, const SnakeNewGame& game) {
        s.board.body = game.body;
        s.board.food = game.food;
        s.dead = false;
    }

    static bool press(State&, const std::string&, const std::string&) { return false; }

    static void draw(const State& s, gfx::Sketch& f, const std::string& path) {
        const SnakeBoardData& b = s.board;
        const std::int64_t c = b.cell;
        f.group(path, 0, 0);
        f.outline_rect(path + "/border", 0, 0, (b.cols + 2) * c, (b.rows + 2) * c,
                       gfx::by_role(s.dead ? gfx::role::kAlert : gfx::role::kMuted));
        const auto cell = [&](std::int64_t at, std::int64_t role) {
            if (at < 0 || at >= b.cols * b.rows) {
                return;
            }
            f.fill_rect(path + "/c." + std::to_string(at), (at % b.cols + 1) * c,
                        (at / b.cols + 1) * c, c, c, gfx::by_role(role));
        };
        cell(b.food, gfx::role::kAlert);
        for (std::size_t i = 1; i < b.body.size(); ++i) {
            cell(b.body[i], gfx::role::kFill);
        }
        if (!b.body.empty()) {
            cell(b.body.front(), gfx::role::kAccent);
        }
    }
};

} // namespace

ZENGINE_EXPORT_ELEMENT(SnakeBoardCode)
