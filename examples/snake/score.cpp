// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The score weave, the late-load demonstration and nothing more: it counts the `FoodEaten` it
// witnesses, independent of the world's own score, so loaded after two meals it reads 0 while the
// world reads 2 -- which is what "began participating" means. It draws its tally in an area of its
// own, as the office `snake.score`, and draws it whole again in a window started afresh.
// Reference: examples/snake/README.md.

#include "vocabulary.hpp"

#include "graphics/drawing.hpp"
#include "graphics/vocabulary.hpp"

#include <zen/kernel/export.hpp>
#include <zen/weave.hpp>

#include <string>

namespace {

using namespace zengine::snake;
namespace gfx = zengine::graphics;

struct ScoreState {
    std::int64_t eaten = 0;
    ZEN_EXPOSE();
    ZEN_SHAPE(ScoreState, 1, ZEN_FIELD(eaten));
};

class ScoreWeave
    : public gfx::DrawingWeave<ScoreWeave, ScoreState, loom::Accept<FoodEaten>> {
public:
    using DrawingWeave::on;

    ScoreWeave() : DrawingWeave(kScoreRole) {}

    void on(const FoodEaten&, loom::Mail&) { ++state_.eaten; }

    void picture(gfx::Sketch& f) const {
        f.text("eaten", 0, 0,
               "[score weave] eaten since I joined: " + std::to_string(state_.eaten),
               gfx::by_role(gfx::role::kFill));
    }
};

} // namespace

ZEN_EXPORT_WEAVE(ScoreWeave)
