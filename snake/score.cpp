// The score weave — the late arrival. It accepts FoodEaten (the locked
// contract) and maintains its own count, independent of the score field the
// world happens to put in SnakeVisual. That independence is what makes the
// late-addition moment REAL and visible: loaded after the game has eaten
// twice, its count reads 0 while the world's reads 2 — it counts what it has
// *witnessed*, which is exactly what "began participating" means.
//
// Since the Surface migration it draws nothing: the tally is PUBLISHED as
// SurfaceText on the "score" slot, and whichever skin holds the surface
// decides where (and whether) a score line lives. It also accepts the skins'
// SurfaceReady hello and answers by re-publishing its current tally, so the
// line survives the painter being replaced mid-game.

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
