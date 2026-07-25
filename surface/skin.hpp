#ifndef ZENGINE_SURFACE_SKIN_HPP
#define ZENGINE_SURFACE_SKIN_HPP

// The Skin weave shell, over an injected Medium (the Input package's Reader
// move, pointed at output). A Medium is anything with:
//
//   void frame(const zengine::snake::SnakeVisual&, bool first);
//   void note(std::string_view slot, std::string_view text);
//
// The real ones own an actual surface RAII-style — the terminal medium enters
// the alternate screen in its constructor and restores it in its destructor,
// the SDL medium opens and closes a window — so loading a Skin claims the
// surface and unloading releases it, with no cleanup protocol to forget. The
// suite's fake medium just records calls, so the weave's whole message
// contract is pinned without a terminal or a window in sight.
//
// The shell is deliberately dumb: count, delegate, and say hello exactly once.
// Everything visual lives in the Medium; everything wire-shaped lives here.
// A Medium also provides `void pump()` — service your OS surface, nothing
// else — driven by the host's PumpSurface lap message (see vocabulary.hpp);
// media with nothing to service keep it empty.

#include "vocabulary.hpp"

#include "snake/vocabulary.hpp" // the V1 canvas payload — see vocabulary.hpp on the coupling
#include "timer/vocabulary.hpp" // the skin asks for its own beat now

#include <zen/weave.hpp>

#include <cstdint>
#include <utility>

namespace zengine::surface {

/// Two honest counters, poke-inspectable like any state: frames painted and
/// text notes delivered (delivered, not necessarily rendered — a slot the
/// Medium doesn't know is dropped there, and the golden tests pin which).
struct SkinState {
    std::int64_t frames = 0;
    std::int64_t texts = 0;
    std::int64_t pumps = 0;
    ZEN_EXPOSE();
    ZEN_SHAPE(SkinState, 1, ZEN_FIELD(frames), ZEN_FIELD(texts), ZEN_FIELD(pumps));
};

template <class Medium>
class SkinT : public loom::WeaveBase<SkinT<Medium>, SkinState,
                                     loom::Accept<zengine::snake::SnakeVisual, SurfaceText,
                                                  PumpSurface, zengine::timer::TimerReady,
                                                  zengine::timer::TimerFired>,
                                     loom::Emit<SurfaceReady, zengine::timer::StartRoleTimer>> {
public:
    SkinT() = default;
    explicit SkinT(Medium medium) : medium_(std::move(medium)) {}

    void on(const zengine::snake::SnakeVisual& v, loom::Mail& mail) {
        hello_once(mail);
        medium_.frame(v, this->state_.frames == 0);
        ++this->state_.frames;
    }

    void on(const SurfaceText& t, loom::Mail& mail) {
        hello_once(mail);
        medium_.note(t.slot, t.text);
        ++this->state_.texts;
    }

    /// Execution time, not intent: service the medium's OS surface, on
    /// direct request (suites, diagnostics, timer-less hosts).
    void on(const PumpSurface&, loom::Mail& mail) {
        hello_once(mail);
        medium_.pump();
        ++this->state_.pumps;
    }

    /// The TimerService woke with this skin already on the surface: a first
    /// breath like any other (the boot path — a swapped-in successor instead
    /// inherits the standing role beat, or wakes on the operator's status
    /// republish, and its hello_once asks then).
    void on(const zengine::timer::TimerReady&, loom::Mail& mail) { hello_once(mail); }

    /// The beat: the same hands PumpSurface opens, on the clock's schedule.
    /// Counted on the same honest counter — it is the same act.
    void on(const zengine::timer::TimerFired& f, loom::Mail& mail) {
        hello_once(mail);
        if (f.id != kPumpTimerId) {
            return; // someone else's ask aimed at this role: data, not a drive
        }
        medium_.pump();
        ++this->state_.pumps;
    }

    Medium& medium() { return medium_; }

private:
    /// One hello per INCARNATION, not per identity: a deliberate plain member
    /// (the v2 world's `asked_` stance), never state — a successor or a
    /// reloaded instance re-claims its surface, so it must re-announce even
    /// where state rides across. The BEAT ask rides the same moment: claiming
    /// the surface means keeping its medium serviced, so the skin's first
    /// breath both says hello and asks the Timer package for kPumpTimerId
    /// (role-addressed — an upsert against whatever beat already stands, so
    /// successors replace the schedule, never double it; with no TimerService
    /// loaded the ask refuses cleanly and the PumpSurface door still works).
    void hello_once(loom::Mail& mail) {
        if (announced_) {
            return;
        }
        announced_ = true;
        mail.publish(SurfaceReady{});
        mail.send_to_role(zengine::timer::kTimerRole,
                          zengine::timer::StartRoleTimer{kPumpTimerId, kPumpBeatMs,
                                                         /*repeat=*/true, kSkinRole});
    }

    bool announced_ = false;
    Medium medium_;
};

} // namespace zengine::surface

#endif // ZENGINE_SURFACE_SKIN_HPP
