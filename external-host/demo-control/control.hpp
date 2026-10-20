// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_DEMO_CONTROL_CONTROL_HPP
#define ZENGINE_DEMO_CONTROL_CONTROL_HPP
#include "vocabulary.hpp"
#include "graphics/canvas.hpp"
#include "graphics/vocabulary.hpp"
#include "input/vocabulary.hpp"
#include "workshop/pane_causes.hpp"
#include "workshop/pane_facets.hpp"
#include "workshop/pane_vocabulary.hpp"
#include "workshop/pane_text.hpp"
#include "manual/vocabulary.hpp"
#include "manuals/zengine.demo.hpp" // this weave's manual, compiled in from its page
#include <zen/weave.hpp>

namespace zengine::demo {
namespace ws = zengine::workshop;
/// The controls' room and their one picture, in Workshop and in a graphics window: the reset row
/// is the part `control:reset`, whose press asks `reset`, and the state beneath it the part
/// `status`, which asks nothing. A right press in Workshop is handed back, so its own pane menu
/// opens there.
using ControlDrawing =
    ws::PaneDrawingFacet<kControlsPane, kRole, manual::pages::zengine_demo::kManual>;
using Actions = ws::PaneActionFacet<kControlsPane, kRole>;
class Control final
    : public zengine::With<ControlDrawing, Actions,
                           manual::ManualFacet<manual::pages::zengine_demo::kManual>>::
    Weave<Control, DemoStatus,
    loom::Accept<DemoServiceOpened, DemoServiceClosed, DemoWorkRequested, DemoWorkFinished,
                 DemoResetRequested, DemoStatusRequested, DemoReadyRequested>,
    loom::Emit<DemoWork, DemoStatus, loom::Ack, loom::Refused>> {
public:
    void on_activated(loom::Mail& m) {
        unserved();
        drawing().announce(*this, m);
    }
    ws::v2::PaneOffered offering() const {
        return ws::v2::PaneOffered{kControlsPane, "Demo", "Repeat this demo through its attached ELH recipe", 4, 56};
    }
    /// ...and the reset its row performs, with no key: the row's part is `control:reset`. Its
    /// press answers the attached service's wait with the work.
    void offered(loom::Mail& m) {
        this->template facet<Actions>().declare(
            m, {{"reset", "reset the demo", input::scan::kUnknown, input::mod::kNone, {},
                 {ws::cause_of<DemoWork>()}}});
    }
    /// THE PANE DRAWS ITS PICTURE wherever a room was granted: Workshop's canvas or prose room, or
    /// a graphics window's. A refused picture leaves the last good one showing; a later paint
    /// draws again.
    void on_room(loom::Mail& m) { paint(m); }
    /// WHAT THE CONTROLS SHOW, a line of the room's text each: the setup's name, the reset button
    /// and where the demo stands, each part recorded with what a press on it asks. A line past the
    /// room is neither drawn nor recorded.
    void picture(graphics::Sketch& f) const {
        const ControlDrawing& room = drawing();
        const auto line = [&](const std::string& path, std::int64_t row, const std::string& text) {
            if (row >= room.rows()) return false;
            f.text(path, 0, row * room.text().line,
                   ws::pane_text::drawable(ws::pane_text::fit(text, room.columns())),
                   graphics::by_role(surface::role::kFill));
            return true;
        };
        (void)line("name", 0, state_.name);
        if (line("control:reset", 1, "[ Reset demo ]")) f.action("control:reset", "reset");
        if (line("status", 2, state_.state + ": " + state_.note)) f.action("status", "");
    }
    /// A revived state names no service of this image's: a kernel reload sends no activation.
    void revive(const loom::Value& v) override {
        state_ = loom::from_value<DemoStatus>(v);
        unserved();
    }
    /// ITS ONE ACTION, `reset`: what a press on the reset button asks, in Workshop or a window.
    void on_action(const std::string& id, loom::Mail& m) {
        if (id == "reset") reset(m, false);
    }
    void on(const DemoStatusRequested&, loom::Mail& m) { (void)m.answer(state_); }
    void on(const DemoReadyRequested& r, loom::Mail& m) {
        if (r.generation < 0 || waiters_.size() >= 8) {
            (void)m.answer(loom::Refused{"invalid ready generation or too many waiters"}); return;
        }
        if (r.generation <= state_.generation && (state_.state == "ready" || state_.state == "failed")) {
            (void)m.answer(state_); return;
        }
        auto reply = m.defer_answer();
        if (reply.valid()) waiters_.push_back({r.generation, std::move(reply)});
    }
    void on(const DemoServiceOpened& r, loom::Mail& m) {
        if (worker_.valid() || r.name.empty() || r.name.size() > 64) {
            (void)m.answer(loom::Refused{"a demo service is already attached, or its name is invalid"}); return;
        }
        worker_ = m.sender(); state_.name = r.name; ++state_.generation;
        state_.state = "working"; state_.note = "Preparing demo"; offered_ = true;
        (void)m.answer(loom::Ack{}); paint(m);
    }
    void on(const DemoWorkRequested&, loom::Mail& m) {
        if (m.sender() != worker_ || next_.valid() || doing_) {
            (void)m.answer(loom::Refused{"only the idle attached service may wait for demo work"}); return;
        }
        if (offered_) {
            offered_ = false; doing_ = true; (void)m.answer(DemoWork{state_.generation});
        } else next_ = m.defer_answer();
    }
    void on(const DemoResetRequested&, loom::Mail& m) { reset(m, true); }
    void on(const DemoWorkFinished& r, loom::Mail& m) {
        if (m.sender() != worker_ || !doing_ || r.generation != state_.generation) {
            (void)m.answer(loom::Refused{"completion does not name the current demo work"}); return;
        }
        doing_ = false; state_.state = r.passed ? "ready" : "failed";
        state_.note = r.note.substr(0, 256);
        if (request_.valid()) {
            if (r.passed) (void)loom::answer_deferred(request_, m, loom::Ack{});
            else (void)loom::answer_deferred(request_, m, loom::Refused{state_.note});
            request_ = {};
        }
        (void)m.answer(loom::Ack{}); paint(m);
    }
    void on(const DemoServiceClosed&, loom::Mail& m) {
        if (m.sender() != worker_) { (void)m.answer(loom::Refused{"not the attached demo service"}); return; }
        if (next_.valid()) (void)loom::answer_deferred(next_, m, loom::Refused{"demo service closed"});
        if (request_.valid()) (void)loom::answer_deferred(request_, m, loom::Refused{"demo service closed during reset"});
        next_ = {}; request_ = {}; worker_ = {}; doing_ = offered_ = false;
        state_.state = "unavailable"; state_.note = "Demo service stopped";
        for (auto& waiter : waiters_)
            (void)loom::answer_deferred(waiter.reply, m, loom::Refused{state_.note});
        waiters_.clear();
        (void)m.answer(loom::Ack{}); paint(m);
    }
private:
    /// WITH NO SERVICE ATTACHED TO THIS IMAGE, THE DEMO IS UNAVAILABLE, whatever a revived state
    /// said: a service attached to the image before a reload is not this one's, its waits and its
    /// work end with that image, and it must start again. The generation carries on.
    void unserved() {
        if (worker_.valid() || state_.state == "unavailable") return;
        state_.note = state_.state.empty()
            ? "Start the demo's ELH service"
            : "The demo's office was reloaded; start the demo's ELH service again";
        state_.state = "unavailable";
    }
    void reset(loom::Mail& m, bool answering) {
        if (!worker_.valid() || doing_ || offered_) {
            if (answering) (void)m.answer(loom::Refused{"demo reset service is busy or unavailable"});
            else { state_.note = "Reset service is busy or unavailable"; paint(m); }
            return;
        }
        if (answering) {
            request_ = m.defer_answer();
            if (!request_.valid()) return;
        }
        ++state_.generation; state_.state = "working"; state_.note = "Resetting demo";
        if (next_.valid()) {
            doing_ = true;
            (void)loom::answer_deferred(next_, m, DemoWork{state_.generation}); next_ = {};
        } else offered_ = true;
        paint(m);
    }
    ControlDrawing& drawing() { return this->template facet<ControlDrawing>(); }
    const ControlDrawing& drawing() const { return this->template facet<ControlDrawing>(); }
    void paint(loom::Mail& m) {
        if (state_.state == "ready" || state_.state == "failed") {
            for (auto it = waiters_.begin(); it != waiters_.end();) {
                if (it->generation <= state_.generation) {
                    (void)loom::answer_deferred(it->reply, m, state_);
                    it = waiters_.erase(it);
                } else ++it;
            }
        }
        drawing().show(*this, m); // nothing until a room is granted
    }
    loom::WeaveId worker_{};
    loom::DeferredAnswer next_, request_;
    struct Waiter { std::int64_t generation; loom::DeferredAnswer reply; };
    std::vector<Waiter> waiters_;
    bool offered_ = false, doing_ = false;
};
} // namespace zengine::demo
#endif
