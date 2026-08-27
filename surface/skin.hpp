// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_SURFACE_SKIN_HPP
#define ZENGINE_SURFACE_SKIN_HPP

// The Skin weave shell, over an injected Medium (the Input package's Reader
// move, pointed at output). A Medium is anything with:
//
//   void frame(const zengine::snake::SnakeVisual&, bool first);
//   void canvas(const SurfaceCanvas&, bool first);
//   void note(std::string_view slot, std::string_view text);
//   void clipboard_copy(const std::string& text);   // a maker copied this: offer it to the
//                                                   // medium's clipboard, with the medium's
//                                                   // own honesty (TEXT-0)
//   std::optional<std::string> clipboard_text();    // a maker asked to paste: the platform
//                                                   // clipboard's CURRENT text, or nullopt
//                                                   // where no truthful read exists (QR-11)
//   SurfaceExtent extent() const;   // how much room I have, in cells; {0,0} = no opinion
//                                   // and, since HD-1, how big one character of
//                                   // mine is; zeroes there mean "text is a cell"
//   std::optional<SurfacePlacement> placement();     // where my window sits on its desktop
//                                                    // (WUX-3); nullopt = I have no desktop
//                                                    // placement fact (a terminal, or no
//                                                    // window yet) — the honest silence
//   void place(const SurfacePlacementRemembered&);   // a remembered placement offered back:
//                                                    // validate it against the desktop that
//                                                    // exists NOW and apply what is safe; a
//                                                    // medium with no desktop does nothing
//
// `clipboard_copy` is REQUIRED of a Medium rather than detected on one, the Sink's own rule
// (skin_tui.hpp) for the Sink's own reason: a Medium that quietly lacked it would be a
// medium on which copy silently reaches nothing, which is an ordinary honest state a fake in
// a suite reaches every day — so the mistake would look exactly like the truth, forever.
// Requiring it makes a forgetful Medium a compile error instead of a silent dead chord.
// `clipboard_text` is required for the same reason, and nullopt is not a failure: it is a
// terminal medium's standing truth, and the asker's fallback (what this process itself last
// copied) is the strongest paste that medium honestly has. `placement` and `place` are
// required for the same reason again — a Medium that quietly lacked either would be one on
// which the desktop conversation silently reaches nothing — and their honest do-nothing
// answers (nullopt; an empty body) are one line each on a medium with no desktop.
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
//
// `extent()` is the one thing a Medium is ASKED rather than told, and the shell
// turns the answer into the one message that travels medium -> publisher
// (SurfaceExtent). A Medium that cannot answer returns {0,0} and the shell says
// nothing at all — a terminal Skin has no drawable it owns the size of, and a
// window Skin has no answer until its window exists.

#include "vocabulary.hpp"

#include "activation/activation.hpp" // the skin arranges its own time from its activation
#include "snake/vocabulary.hpp"      // the V1 canvas payload — see vocabulary.hpp on the coupling
#include "timer/vocabulary.hpp"      // the skin asks for its own beat now

#include <zen/weave.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

namespace zengine::surface {

/// Two honest counters, poke-inspectable like any state: frames painted and
/// text notes delivered (delivered, not necessarily rendered — a slot the
/// Medium doesn't know is dropped there, and the golden tests pin which).
///
/// `frames` counts EVERY painted frame, whether the intent behind it was a
/// SnakeVisual or a SurfaceCanvas — it is the same act, so it gets the same
/// counter, the argument `pumps` already makes for PumpSurface and TimerFired.
/// It is also load-bearing rather than decorative: `frames == 0` is what tells
/// a Medium that the frame it is being handed is the FIRST one and the surface
/// still has to be claimed. No new field, so no shape version bump and no
/// change to what a poke of a Skin returns.
struct SkinState {
    std::int64_t frames = 0;
    std::int64_t texts = 0;
    std::int64_t pumps = 0;
    ZEN_EXPOSE();
    ZEN_SHAPE(SkinState, 1, ZEN_FIELD(frames), ZEN_FIELD(texts), ZEN_FIELD(pumps));
};

template <class Medium>
class SkinT : public loom::WeaveBase<SkinT<Medium>, SkinState,
                                     loom::Accept<zengine::snake::SnakeVisual, SurfaceCanvas,
                                                  SurfaceText, ClipboardCopy,
                                                  ClipboardTextRequested,
                                                  SurfacePlacementRemembered, PumpSurface,
                                                  loom::Activated,
                                                  zengine::timer::TimerReady,
                                                  zengine::timer::TimerFired>,
                                     loom::Emit<SurfaceReady, SurfaceExtent, SurfacePlacement,
                                                ClipboardText,
                                                zengine::timer::StartRoleTimer>> {
public:
    SkinT() = default;
    explicit SkinT(Medium medium) : medium_(std::move(medium)) {}

    void on(const zengine::snake::SnakeVisual& v, loom::Mail& mail) {
        announce_surface_once(mail);
        medium_.frame(v, this->state_.frames == 0);
        ++this->state_.frames;
        report_placement(mail);
        report_extent(mail); // the first frame is what brings a window into existence
    }

    /// The general canvas — the same act as a frame, from general intent
    /// instead of snake's. Deliberately the identical three lines: announce,
    /// delegate, count. Nothing about a canvas is more privileged than a
    /// SnakeVisual, and the shell would be lying about that if it treated one
    /// specially.
    void on(const SurfaceCanvas& c, loom::Mail& mail) {
        announce_surface_once(mail);
        medium_.canvas(c, this->state_.frames == 0);
        ++this->state_.frames;
        report_placement(mail);
        report_extent(mail);
    }

    void on(const SurfaceText& t, loom::Mail& mail) {
        announce_surface_once(mail);
        medium_.note(t.slot, t.text);
        ++this->state_.texts;
    }

    /// A maker copied text somewhere in this process: hand it to the medium, which offers
    /// it to whatever clipboard the medium honestly has (see ClipboardCopy's contract in
    /// vocabulary.hpp). Deliberately uncounted — SkinState is a wire shape, and a field
    /// there is a version, not a convenience.
    void on(const ClipboardCopy& c, loom::Mail& mail) {
        announce_surface_once(mail);
        medium_.clipboard_copy(c.text);
    }

    /// A maker asked to PASTE somewhere in this process: read the medium's platform
    /// clipboard NOW — because this paste was requested, and for no other reason — and
    /// answer the one participant that asked (QR-11; the custody law is
    /// ClipboardTextRequested's, vocabulary.hpp). `mail.answer` carries Loom's own answer
    /// provenance, so the asker can require this delivery to be THE answer to its ask
    /// rather than anybody's helpful payload. Uncounted, for ClipboardCopy's reason.
    void on(const ClipboardTextRequested&, loom::Mail& mail) {
        announce_surface_once(mail);
        const std::optional<std::string> text = medium_.clipboard_text();
        (void)mail.answer(ClipboardText{text.has_value(), text.value_or(std::string())});
    }

    /// A REMEMBERED PLACEMENT, OFFERED BACK (WUX-3): hand it to the medium, whose judgment
    /// it is (the vocabulary's contract — validate against the desktop that exists now,
    /// apply what is safe, do nothing on a medium with no desktop). What is then REPORTED
    /// is the truth, not an echo: the same change-guarded placement report every drag goes
    /// through, so a want the medium adapted comes back as where the window actually is.
    /// The extent is re-read too, because a remembered maximize is a resize.
    void on(const SurfacePlacementRemembered& p, loom::Mail& mail) {
        announce_surface_once(mail);
        medium_.place(p);
        report_placement(mail);
        report_extent(mail);
    }

    /// Execution time, not intent: service the medium's OS surface, on
    /// direct request (suites, diagnostics, timer-less hosts).
    void on(const PumpSurface&, loom::Mail& mail) {
        announce_surface_once(mail);
        medium_.pump();
        ++this->state_.pumps;
        report_placement(mail);
        report_extent(mail);
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
        // THE BEAT IS WHAT NOTICES A PERSON DRAGGING A WINDOW — its place and its edge
        // both. Placement is reported FIRST, deliberately: a maximize changes both facts
        // in one gesture, and a consumer keeping "the normal window's room" must hear
        // that the window is maximized before it hears the maximized room, or it files
        // the new size under the wrong state (WUX-3; the same argument at every report
        // site, made once here).
        report_placement(mail);
        report_extent(mail);
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
    /// SEPARATE FROM THE HELLO ON PURPOSE, and the bug that forces it is worth
    /// keeping named: sharing one `hello_once` between them means a skin that
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

    /// SAY HOW MUCH ROOM THERE IS, WHEN IT CHANGES AND ONLY THEN.
    ///
    /// Asked of the Medium after every act that could have changed its answer —
    /// a frame or a canvas (either can be what creates a window in the first
    /// place) and every pump (the 10ms beat is what notices a person dragging a
    /// window edge, and a resize arrives through no other door this weave has).
    ///
    /// THE CHANGE GUARD IS THE WHOLE OF THE POLICY. Without it this would publish
    /// a hundred times a second at a publisher that would repaint on every one of
    /// them, which is a busy loop wearing a message's clothes. With it, a still
    /// window is silent and a dragged one speaks once per size it passes through.
    ///
    /// {0,0} IS "NO OPINION" AND IS NEVER PUBLISHED. A terminal Medium always
    /// answers that way, and a window Medium answers that way until its window
    /// exists. Publishing zeroes would tell a publisher there is no room, which
    /// is a different sentence and a false one. The value is still REMEMBERED, so
    /// a medium that loses its surface and gets it back at the same size says so
    /// again.
    ///
    /// A PLAIN MEMBER, never state: the reported extent belongs to an
    /// incarnation's surface, exactly as `announced_` belongs to its hello. A
    /// successor claims its own surface and must report its own room, even where
    /// state rides across.
    /// THE TEXT METRIC RIDES THE SAME GUARD, and it has to. Since HD-1 a
    /// graphical medium answers with two facts — how many cells, and how big one
    /// character is — and they change independently: a person dragging a window
    /// edge moves the first and not the second, and a font that opens late (or
    /// fails and leaves the bitmap face drawing) moves the second and not the
    /// first. Comparing only the extent would have published the first kind of
    /// change and swallowed the second, so a Workshop that started before the
    /// font was ready would keep wrapping against cells until somebody happened
    /// to resize the window.
    void report_extent(loom::Mail& mail) {
        const SurfaceExtent now = medium_.extent();
        if (now.width == reported_.width && now.height == reported_.height &&
            now.text_advance_px == reported_.text_advance_px &&
            now.text_line_px == reported_.text_line_px) {
            return;
        }
        reported_ = now;
        if (now.width <= 0 || now.height <= 0) {
            return; // no opinion: say nothing rather than saying zero
        }
        mail.publish(now);
    }

    /// SAY WHERE THE WINDOW SITS, WHEN IT CHANGES AND ONLY THEN (WUX-3).
    ///
    /// `report_extent`'s own shape, one fact over: asked of the Medium beside every extent
    /// read, change-guarded so a still window is silent and a dragged one speaks once per
    /// place it passes through, and ABSENT rather than zeroed — a Medium with no desktop
    /// placement answers nullopt and nothing is published, because unlike the extent there
    /// is no in-band absent value ((0,0) is a real place on every desktop). The last
    /// reported value is a plain member for the extent's reason: a successor claims its
    /// own surface and must report its own placement.
    void report_placement(loom::Mail& mail) {
        const std::optional<SurfacePlacement> now = medium_.placement();
        if (!now.has_value()) {
            return; // no desktop placement fact: silence, never zeroes
        }
        if (placement_said_ && now->x == said_placement_.x && now->y == said_placement_.y &&
            now->maximized == said_placement_.maximized) {
            return;
        }
        placement_said_ = true;
        said_placement_ = *now;
        mail.publish(*now);
    }

    bool announced_ = false;
    SurfaceExtent reported_{}; ///< the last extent said out loud, per incarnation
    bool placement_said_ = false;    ///< whether any placement has been said, per incarnation
    SurfacePlacement said_placement_{}; ///< ...and the last one that was
    zengine::ActivationCursor activation_; ///< per-incarnation, never state
    Medium medium_;
};

} // namespace zengine::surface

#endif // ZENGINE_SURFACE_SKIN_HPP
