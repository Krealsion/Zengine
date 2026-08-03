// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

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

#include "activation/activation.hpp" // the skin arranges its own time from its activation
#include "snake/vocabulary.hpp"      // the V1 canvas payload — see vocabulary.hpp on the coupling
#include "timer/vocabulary.hpp"      // the skin asks for its own beat now

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
                                                  PumpSurface, loom::Activated,
                                                  zengine::timer::TimerReady,
                                                  zengine::timer::TimerFired>,
                                     loom::Emit<SurfaceReady, zengine::timer::StartRoleTimer>> {
public:
    SkinT() = default;
    explicit SkinT(Medium medium) : medium_(std::move(medium)) {}

    void on(const zengine::snake::SnakeVisual& v, loom::Mail& mail) {
        announce_surface_once(mail);
        medium_.frame(v, this->state_.frames == 0);
        ++this->state_.frames;
    }

    void on(const SurfaceText& t, loom::Mail& mail) {
        announce_surface_once(mail);
        medium_.note(t.slot, t.text);
        ++this->state_.texts;
    }

    /// Execution time, not intent: service the medium's OS surface, on
    /// direct request (suites, diagnostics, timer-less hosts).
    void on(const PumpSurface&, loom::Mail& mail) {
        announce_surface_once(mail);
        medium_.pump();
        ++this->state_.pumps;
    }

    /// This incarnation is live and holds the surface: say so, and arrange the
    /// servicing it needs.
    void on(const loom::Activated& a, loom::Mail& mail) {
        if (!activation_.accept(mail, a)) {
            return; // unattested or already acted on
        }
        announce_surface_once(mail);
        ask_for_pump_timer(mail);
    }

    /// The Timer service became available. ASK ONLY — deliberately not routed
    /// through the announce path, which is the whole point of the split (see
    /// ask_for_pump_timer).
    void on(const zengine::timer::TimerReady&, loom::Mail& mail) { ask_for_pump_timer(mail); }

    /// The beat: the same hands PumpSurface opens, on the clock's schedule.
    /// Counted on the same honest counter — it is the same act.
    void on(const zengine::timer::TimerFired& f, loom::Mail& mail) {
        announce_surface_once(mail);
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
    /// where state rides across.
    void announce_surface_once(loom::Mail& mail) {
        if (announced_) {
            return;
        }
        announced_ = true;
        mail.publish(SurfaceReady{});
    }

    /// Claiming the surface means keeping its medium serviced, so the skin asks
    /// the Timer package for kPumpTimerId — role-addressed, an upsert against
    /// whatever beat already stands, so successors replace the schedule rather
    /// than doubling it. With no TimerService loaded the ask goes nowhere and
    /// the PumpSurface door still works — and where it dies is worth knowing
    /// (measured in the timer suite's load-order case): the send crosses the
    /// library seam as bytes and the host resolves the claimed schema against
    /// the bus registry first, so with nobody accepting StartRoleTimer the
    /// shape is unregistered and the send is rejected AT THE SEAM. Not "refused
    /// by an unheld role" — earlier than that, and invisible from in here.
    /// Which is exactly why TimerReady is still load-bearing: this weave cannot
    /// tell that its ask evaporated, so something has to tell it to try again.
    ///
    /// SEPARATE FROM THE HELLO ON PURPOSE (R2A-2), and the bug that forced it is
    /// worth keeping named: these two used to share `hello_once`, so a skin that
    /// activated BEFORE any Timer existed said hello, sent an ask that went
    /// nowhere, and then — when the Timer finally appeared and published
    /// TimerReady — returned early from the already-spent hello and never
    /// retried. It would have been serviced by nothing, forever, on a bus that
    /// was working fine. Announcing is ONCE; asking must stay REPEATABLE (and
    /// a re-ask replaces and re-anchors the schedule rather than costing
    /// nothing — see TimerReady in timer/vocabulary.hpp).
    void ask_for_pump_timer(loom::Mail& mail) {
        mail.send_to_role(zengine::timer::kTimerRole,
                          zengine::timer::StartRoleTimer{kPumpTimerId, kPumpBeatMs,
                                                         /*repeat=*/true, kSkinRole});
    }

    bool announced_ = false;
    zengine::ActivationCursor activation_; ///< per-incarnation, never state
    Medium medium_;
};

} // namespace zengine::surface

#endif // ZENGINE_SURFACE_SKIN_HPP
