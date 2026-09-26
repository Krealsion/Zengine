// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_SURFACE_SKIN_HPP
#define ZENGINE_SURFACE_SKIN_HPP

// The Skin weave shell over an injected Medium: it counts, delegates, says hello once per
// incarnation, reports the Medium's extent and placement, and keeps the one captured picture;
// everything visual is the Medium's. A Medium provides `frame`, `canvas`, `note`, `pump`,
// `extent`, `placement`, `place`, `capture`, `clipboard_copy` and `clipboard_text`, each
// required rather than detected: a Medium quietly lacking one would look exactly like an honest
// medium that cannot, on every lane. A real Medium claims its surface RAII-style.
// Surface law: agents/surface.md

#include "vocabulary.hpp"

#include "activation/activation.hpp" // the skin arranges its own time from its activation
#include "snake/vocabulary.hpp" // the snake frame the Skins still accept
#include "timer/vocabulary.hpp"      // the skin asks for its own beat now

#include <zen/weave.hpp>
#include <zen/weave/standard_shapes.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

namespace zengine::surface {

/// Honest counters, poke-inspectable: frames painted (a SnakeVisual or a canvas, the same act),
/// text notes delivered, pumps. `frames == 0` tells a Medium the frame it is handed is the
/// first, and its surface still has to be claimed.
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
                                                  SurfaceCaptureRequested,
                                                  SurfaceCaptureChunkRequested,
                                                  loom::Activated,
                                                  zengine::timer::TimerReady,
                                                  zengine::timer::TimerFired>,
                                     loom::Emit<SurfaceReady, SurfaceExtent, SurfacePlacement,
                                                ClipboardText, SurfaceCaptured,
                                                SurfaceCaptureChunk, loom::Refused,
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
        capture_if_due(mail);
    }

    /// The general canvas: the same act as a frame, handled identically.
    void on(const SurfaceCanvas& c, loom::Mail& mail) {
        announce_surface_once(mail);
        medium_.canvas(c, this->state_.frames == 0);
        ++this->state_.frames;
        report_placement(mail);
        report_extent(mail);
        capture_if_due(mail);
    }

    /// A PICTURE OF WHAT THIS MEDIUM PRESENTS (vocabulary.hpp says what it proves). Now, or on
    /// the paint that passes `after_frame`; one may wait at a time.
    void on(const SurfaceCaptureRequested& asked, loom::Mail& mail) {
        announce_surface_once(mail);
        if (asked.after_frame >= 0 && asked.after_frame >= this->state_.frames) {
            if (pending_capture_.valid()) {
                (void)mail.answer(refused_capture("a capture is already waiting for frame " +
                                                  std::to_string(pending_after_ + 1) +
                                                  "; one waits at a time"));
                return;
            }
            pending_capture_ = mail.defer_answer();
            if (!pending_capture_.valid()) {
                (void)mail.answer(refused_capture("this delivery cannot defer its answer"));
                return;
            }
            pending_after_ = asked.after_frame;
            return;
        }
        (void)mail.answer(take_capture());
    }

    /// ONE CHUNK OF THE RETAINED PICTURE, or a refusal naming which picture is retained now.
    void on(const SurfaceCaptureChunkRequested& asked, loom::Mail& mail) {
        if (retained_.number == 0 || asked.capture != retained_.number) {
            (void)mail.answer(loom::Refused{
                retained_.number == 0
                    ? std::string("no picture is retained; ask for a capture first")
                    : "capture " + std::to_string(asked.capture) + " is gone; the retained one is " +
                          std::to_string(retained_.number)});
            return;
        }
        const std::int64_t total = static_cast<std::int64_t>(retained_.picture.bytes.size());
        if (asked.offset < 0 || asked.offset > total) {
            (void)mail.answer(loom::Refused{"offset " + std::to_string(asked.offset) +
                                            " is outside a picture of " + std::to_string(total) +
                                            " bytes"});
            return;
        }
        SurfaceCaptureChunk chunk;
        chunk.capture = retained_.number;
        chunk.offset = asked.offset;
        chunk.total = total;
        const std::int64_t n = total - asked.offset < kCaptureChunkBytes ? total - asked.offset
                                                                          : kCaptureChunkBytes;
        const auto begin = retained_.picture.bytes.begin() + asked.offset;
        chunk.data.assign(begin, begin + n);
        (void)mail.answer(chunk);
    }

    void on(const SurfaceText& t, loom::Mail& mail) {
        announce_surface_once(mail);
        medium_.note(t.slot, t.text);
        ++this->state_.texts;
    }

    /// A maker copied text: hand it to the medium. Uncounted, because SkinState is a wire shape
    /// and a field there is a version.
    void on(const ClipboardCopy& c, loom::Mail& mail) {
        announce_surface_once(mail);
        medium_.clipboard_copy(c.text);
    }

    /// A maker asked to paste: read the medium's platform clipboard now, and answer the one
    /// participant that asked. `mail.answer` carries Loom's answer provenance, so the asker can
    /// require this delivery to be the answer to its ask.
    void on(const ClipboardTextRequested&, loom::Mail& mail) {
        announce_surface_once(mail);
        const std::optional<std::string> text = medium_.clipboard_text();
        (void)mail.answer(ClipboardText{text.has_value(), text.value_or(std::string())});
    }

    /// A remembered placement offered back: the medium judges it. What is reported after is
    /// where the window is, through the ordinary change-guarded report, and the extent is re-read
    /// because a remembered maximize is a resize.
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

    /// The Timer service became available: ask again, and only ask (see `ask_for_pump_timer`).
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
        // The beat notices a dragged window, its place and its edge. Placement is reported
        // first: a maximize changes both, and a consumer keeping the normal window's room must
        // hear the state before the size.
        report_placement(mail);
        report_extent(mail);
    }

    Medium& medium() { return medium_; }

    /// The frame count, for a suite that pins the ordering rule.
    std::int64_t frames() const noexcept { return this->state_.frames; }

private:
    /// THE ONE RETAINED PICTURE, numbered. Plain members: a picture belongs to the surface an
    /// incarnation held, and a successor has painted nothing yet.
    struct Retained {
        std::int64_t number = 0;
        std::int64_t frame = 0;
        CapturedPicture picture;
    };

    static SurfaceCaptured refused_capture(std::string why) {
        SurfaceCaptured c;
        c.ok = false;
        c.refusal = std::move(why);
        return c;
    }

    /// READ THE MEDIUM'S PICTURE and retain it, replacing the previous one. The medium may
    /// have nothing to show (no window yet, no terminal) and says so; a picture over the bound
    /// is refused whole rather than cut.
    SurfaceCaptured take_capture() {
        std::optional<CapturedPicture> picture = medium_.capture();
        if (!picture.has_value()) {
            return refused_capture("this medium has no picture to read back right now");
        }
        if (static_cast<std::int64_t>(picture->bytes.size()) > kMaxCaptureBytes) {
            return refused_capture("the picture is " + std::to_string(picture->bytes.size()) +
                                   " bytes, over the " + std::to_string(kMaxCaptureBytes) +
                                   " this Skin retains");
        }
        retained_.number = ++captures_;
        retained_.frame = this->state_.frames;
        retained_.picture = std::move(*picture);
        SurfaceCaptured c;
        c.ok = true;
        c.capture = retained_.number;
        c.frame = retained_.frame;
        c.width = retained_.picture.width;
        c.height = retained_.picture.height;
        c.cell_px = retained_.picture.cell_px;
        c.format = retained_.picture.format;
        c.bytes = static_cast<std::int64_t>(retained_.picture.bytes.size());
        return c;
    }

    /// The deferred capture, spent on the paint that passed its frame -- and only then.
    void capture_if_due(loom::Mail& mail) {
        if (!pending_capture_.valid() || this->state_.frames <= pending_after_) {
            return;
        }
        loom::DeferredAnswer due = std::move(pending_capture_);
        pending_capture_ = loom::DeferredAnswer{};
        (void)loom::answer_deferred(due, mail, take_capture());
    }

    /// One hello per incarnation, a plain member and never state: a successor re-claims its
    /// surface, so it re-announces even where state rides across.
    void announce_surface_once(loom::Mail& mail) {
        if (announced_) {
            return;
        }
        announced_ = true;
        mail.publish(SurfaceReady{});
    }

    /// Ask the Timer for the pump beat: role-addressed and an upsert, so a successor replaces the
    /// schedule rather than doubling it. With no Timer the send is rejected at the library seam,
    /// invisibly from here, which is why `TimerReady` asks again. Separate from the hello on
    /// purpose: sharing one "once" left a Skin activated before any Timer existed serviced by
    /// nothing, forever.
    void ask_for_pump_timer(loom::Mail& mail) {
        mail.send_to_role(zengine::timer::kTimerRole,
                          zengine::timer::StartRoleTimer{kPumpTimerId, kPumpBeatMs,
                                                         /*repeat=*/true, kSkinRole});
    }

    /// Say how much room there is, when it changes and only then: asked after every act that
    /// could change it (a frame can create a window; the beat notices a drag). The change guard
    /// is the policy, and compares the whole value -- cells, text metric and `cell_px` move
    /// independently. {0,0} is no opinion and never published, but it is remembered, so a medium
    /// that loses its surface and regains it at the same size says so again. A plain member: a
    /// successor reports its own room.
    void report_extent(loom::Mail& mail) {
        const SurfaceExtent now = medium_.extent();
        if (now.width == reported_.width && now.height == reported_.height &&
            now.text_advance_px == reported_.text_advance_px &&
            now.text_line_px == reported_.text_line_px && now.cell_px == reported_.cell_px) {
            return;
        }
        reported_ = now;
        if (now.width <= 0 || now.height <= 0) {
            return; // no opinion: say nothing rather than saying zero
        }
        mail.publish(now);
    }

    /// Say where the window sits, when it changes and only then: `report_extent`'s shape, but
    /// absent rather than zeroed, since `(0,0)` is a real place. A plain member, for the
    /// extent's reason.
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
    Retained retained_;
    std::int64_t captures_ = 0;
    loom::DeferredAnswer pending_capture_;
    std::int64_t pending_after_ = -1;
};

} // namespace zengine::surface

#endif // ZENGINE_SURFACE_SKIN_HPP
