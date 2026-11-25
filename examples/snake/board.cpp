// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

// `snake-board`, the board's weave, held as `snake.board`: it provides what the board draws for a
// `SnakeBoardData`, of that data alone (Loom's calls, MSG-14), and the world draws the answer in
// its own picture. It draws an outline one cell outside the board and a node a filled cell
// (`c.<cell>`): the head in the accent, the body in the fill, the food in the alert, so any palette
// colours it as it colours any rectangle. It takes no press and keeps nothing: a press on the board
// reaches the world that drew it.
// Reference: examples/snake/README.md.

#include "board.hpp"

#include "graphics/canvas.hpp"

#include <zen/kernel/export.hpp>
#include <zen/weave.hpp>

#include <string>

namespace {

namespace gfx = zengine::graphics;
using namespace zengine::snake;

class SnakeBoard final
    : public loom::WeaveBase<SnakeBoard, SnakeBoardState, loom::Accept<>, loom::Emit<>,
                             loom::Claims<>, loom::Provides<SnakeBoardData>> {
public:
    /// The board drawn whole from its data, each node named beneath the place the world puts it.
    static gfx::ElementDrawn provide(const SnakeBoardData& b) {
        gfx::Sketch f;
        const std::int64_t c = b.cell;
        f.outline_rect("border", 0, 0, (b.cols + 2) * c, (b.rows + 2) * c,
                       b.dead ? gfx::colors::alert : gfx::colors::quiet);
        const auto cell = [&](std::int64_t at, gfx::Color ink) {
            if (at < 0 || at >= b.cols * b.rows) {
                return;
            }
            f.fill_rect("c." + std::to_string(at), (at % b.cols + 1) * c, (at / b.cols + 1) * c, c,
                        c, ink);
        };
        cell(b.food, gfx::colors::alert);
        for (std::size_t i = 1; i < b.body.size(); ++i) {
            cell(b.body[i], gfx::colors::text);
        }
        if (!b.body.empty()) {
            cell(b.body.front(), gfx::colors::accent);
        }
        return f.as_element();
    }
};

} // namespace

ZEN_EXPORT_WEAVE(SnakeBoard)
