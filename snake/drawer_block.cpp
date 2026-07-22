// The block drawer — the SECOND skin, and deliberately different code, not a
// re-parameterized classic: double-width cells, no border (a colored rule
// above and below instead), SGR color and inverse-video, banner ABOVE the
// board. When the swap happens mid-game, the change must be unmistakable.
//
// Same locked contract (accepts only SnakeVisual), same terminal convention
// (rows 1–2 belong to host and score; the canvas starts at row 3).

#include "vocabulary.hpp"

#include <zen/kernel/export.hpp>
#include <zen/weave.hpp>

#include <cstdio>
#include <string>

namespace {

using namespace zengine::snake;

struct BlockDrawerState {
    std::int64_t frames = 0;
    ZEN_EXPOSE();
    ZEN_SHAPE(BlockDrawerState, 1, ZEN_FIELD(frames));
};

class BlockDrawer : public loom::WeaveBase<BlockDrawer, BlockDrawerState,
                                           loom::Accept<SnakeVisual>, loom::Emit<>> {
public:
    void on(const SnakeVisual& v, loom::Mail&) {
        const std::size_t cols = static_cast<std::size_t>(v.width) * 2;
        std::string out;
        out.reserve(cols * static_cast<std::size_t>(v.height + 6) * 2);
        out += "\x1b[3;1H";
        if (state_.frames == 0) {
            out += "\x1b[0J";
        }
        out += "\x1b[2K\x1b[7m BLOCK DRAWER \x1b[0m  score ";
        out += std::to_string(v.score);
        if (!v.alive) {
            out += "  \x1b[31;7m DEAD — n starts over \x1b[0m";
        }
        out += "\r\n\x1b[36m";
        out.append(cols, '=');
        out += "\x1b[0m\r\n";
        for (std::int64_t y = 0; y < v.height; ++y) {
            out += "\x1b[2K";
            for (std::int64_t x = 0; x < v.width; ++x) {
                out += cell(v, x, y);
            }
            out += "\r\n";
        }
        out += "\x1b[36m";
        out.append(cols, '=');
        out += "\x1b[0m\r\n";
        std::fwrite(out.data(), 1, out.size(), stdout);
        std::fflush(stdout);
        ++state_.frames;
    }

private:
    static std::string cell(const SnakeVisual& v, std::int64_t x, std::int64_t y) {
        if (!v.snake.empty() && v.snake.front().x == x && v.snake.front().y == y) {
            return "\x1b[32;7m@@\x1b[0m";
        }
        for (std::size_t i = 1; i < v.snake.size(); ++i) {
            if (v.snake[i].x == x && v.snake[i].y == y) {
                return "\x1b[32m##\x1b[0m";
            }
        }
        if (v.food.x == x && v.food.y == y) {
            return "\x1b[33;7m()\x1b[0m";
        }
        return "  ";
    }
};

} // namespace

ZEN_EXPORT_WEAVE(BlockDrawer)
