// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The score weave, the late arrival: it counts the `FoodEaten` it witnesses, independent of the
// world's own score, so loaded after two meals it reads 0 while the world reads 2 -- which is
// what "began participating" means. It publishes its tally as `SurfaceText` on the "score" slot
// and re-publishes it on a Skin's `SurfaceReady`, so the line survives the painter's replacement.
// Reference: docs/reference/snake.md.

#include "vocabulary.hpp"

#include "surface/vocabulary.hpp"

#include <zen/kernel/export.hpp>
#include <zen/weave.hpp>

#include <string>

namespace {

using namespace zengine::snake;
namespace surface = zengine::surface;

struct ScoreState {
    std::int64_t eaten = 0;
    ZEN_EXPOSE();
    ZEN_SHAPE(ScoreState, 1, ZEN_FIELD(eaten));
};

class ScoreWeave
    : public loom::WeaveBase<ScoreWeave, ScoreState,
                             loom::Accept<FoodEaten, surface::SurfaceReady>,
                             loom::Emit<surface::SurfaceText>> {
public:
    void on(const FoodEaten&, loom::Mail& mail) {
        ++state_.eaten;
        speak(mail);
    }

    void on(const surface::SurfaceReady&, loom::Mail& mail) { speak(mail); }

private:
    void speak(loom::Mail& mail) {
        mail.publish(surface::SurfaceText{
            surface::kSlotScore,
            "[score weave] eaten since I joined: " + std::to_string(state_.eaten)});
    }
};

} // namespace

ZEN_EXPORT_WEAVE(ScoreWeave)
