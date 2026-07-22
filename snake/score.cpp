// The score weave — the late arrival. It accepts ONLY FoodEaten (the locked
// contract) and maintains its own count, independent of the score field the
// world happens to put in SnakeVisual. That independence is what makes the
// late-addition moment REAL and visible: loaded after the game has eaten
// twice, its count reads 0 while the world's reads 2 — it counts what it has
// *witnessed*, which is exactly what "began participating" means.
//
// It owns terminal row 2 (by the shared convention) and repaints it on every
// event it hears.

#include "vocabulary.hpp"

#include <zen/kernel/export.hpp>
#include <zen/weave.hpp>

#include <cstdio>
#include <string>

namespace {

using namespace zengine::snake;

struct ScoreState {
    std::int64_t eaten = 0;
    ZEN_EXPOSE();
    ZEN_SHAPE(ScoreState, 1, ZEN_FIELD(eaten));
};

class ScoreWeave
    : public loom::WeaveBase<ScoreWeave, ScoreState, loom::Accept<FoodEaten>, loom::Emit<>> {
public:
    void on(const FoodEaten&, loom::Mail&) {
        ++state_.eaten;
        std::string out = "\x1b[2;1H\x1b[2K  \x1b[33m[score weave]\x1b[0m eaten since I joined: ";
        out += std::to_string(state_.eaten);
        std::fwrite(out.data(), 1, out.size(), stdout);
        std::fflush(stdout);
    }
};

} // namespace

ZEN_EXPORT_WEAVE(ScoreWeave)
