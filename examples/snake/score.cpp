// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The score weave, the late-load demonstration and nothing more: it counts the `FoodEaten` it
// witnesses, independent of the world's own score, so loaded after two meals it reads 0 while the
// world reads 2 -- which is what "began participating" means. It draws its tally in an area of its
// own, as the office `snake.score`, and draws it whole again in a window started afresh.
// Reference: examples/snake/README.md.

#include "vocabulary.hpp"

#include "graphics/canvas.hpp"
#include "graphics/vocabulary.hpp"

#include "activation/activation.hpp"

#include <zen/kernel/export.hpp>
#include <zen/weave.hpp>
#include <zen/weave/standard_shapes.hpp>

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
    : public loom::WeaveBase<ScoreWeave, ScoreState,
                             loom::Accept<FoodEaten, gfx::AreaRoom, gfx::DrawnRefused,
                                          loom::Activated>,
                             loom::Emit<gfx::Drawn, gfx::RoomAsked>> {
public:
    void on(const FoodEaten&, loom::Mail& mail) {
        ++state_.eaten;
        speak(mail);
    }

    void on(const gfx::AreaRoom& r, loom::Mail& mail) {
        if (mail.authored_from_role(gfx::kGraphicsRole) && r.area == kScoreRole &&
            canvas_.set_window(r.window, r.epoch)) {
            speak(mail);
        }
    }

    void on(const gfx::DrawnRefused&, loom::Mail&) {}

    void on(const loom::Activated& a, loom::Mail& mail) {
        if (activation_.accept(mail, a)) {
            (void)mail.as_role(kScoreRole)
                .send_to_role(gfx::kGraphicsRole, gfx::RoomAsked{kScoreRole});
        }
    }

private:
    void speak(loom::Mail& mail) {
        gfx::Sketch f;
        f.text("eaten", 0, 0,
               "[score weave] eaten since I joined: " + std::to_string(state_.eaten),
               gfx::by_role(gfx::role::kFill));
        canvas_.draw(f);
        canvas_.show(mail);
    }

    gfx::Canvas canvas_{kScoreRole};
    zengine::ActivationCursor activation_;
};

} // namespace

ZEN_EXPORT_WEAVE(ScoreWeave)
