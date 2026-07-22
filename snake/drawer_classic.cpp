// The classic drawer — the first skin: one character per cell, an ASCII border,
// monochrome, banner underneath. It accepts ONLY SnakeVisual (the locked drawer
// contract) and holds nothing but its frame count.
//
// Terminal convention (shared, by convention not contract, with the host and
// the score weave): row 1 is the host's status line, row 2 is the score line,
// row 3 downward belongs to whoever draws. A drawer's first frame clears its
// region; after that it repaints in place. stdout is the canvas — the host put
// the terminal in raw mode and stays out of rows ≥ 3.

#include "vocabulary.hpp"

#include <zen/kernel/export.hpp>
#include <zen/weave.hpp>

#include <cstdio>
#include <string>

namespace {

using namespace zengine::snake;

struct ClassicDrawerState {
    std::int64_t frames = 0;
    ZEN_EXPOSE();
    ZEN_SHAPE(ClassicDrawerState, 1, ZEN_FIELD(frames));
};

class ClassicDrawer : public loom::WeaveBase<ClassicDrawer, ClassicDrawerState,
                                             loom::Accept<SnakeVisual>, loom::Emit<>> {
public:
    void on(const SnakeVisual& v, loom::Mail&) {
        std::string out;
        out.reserve(static_cast<std::size_t>((v.width + 4) * (v.height + 6)));
        out += "\x1b[3;1H";
        if (state_.frames == 0) {
            out += "\x1b[0J"; // first frame: claim the canvas below the status rows
        }
        out += "+";
        out.append(static_cast<std::size_t>(v.width), '-');
        out += "+\r\n";
        for (std::int64_t y = 0; y < v.height; ++y) {
            out += '|';
            for (std::int64_t x = 0; x < v.width; ++x) {
                out += glyph(v, x, y);
            }
            out += "|\r\n";
        }
        out += "+";
        out.append(static_cast<std::size_t>(v.width), '-');
        out += "+\r\n";
        out += "\x1b[2K  classic drawer · score " + std::to_string(v.score);
        out += v.alive ? " · alive" : " · DEAD (n = new game)";
        out += "\r\n";
        std::fwrite(out.data(), 1, out.size(), stdout);
        std::fflush(stdout);
        ++state_.frames;
    }

private:
    static char glyph(const SnakeVisual& v, std::int64_t x, std::int64_t y) {
        if (!v.snake.empty() && v.snake.front().x == x && v.snake.front().y == y) {
            return 'O';
        }
        for (std::size_t i = 1; i < v.snake.size(); ++i) {
            if (v.snake[i].x == x && v.snake[i].y == y) {
                return 'o';
            }
        }
        if (v.food.x == x && v.food.y == y) {
            return '*';
        }
        return ' ';
    }
};

} // namespace

ZEN_EXPORT_WEAVE(ClassicDrawer)
