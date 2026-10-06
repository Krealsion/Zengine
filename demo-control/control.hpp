// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_DEMO_CONTROL_CONTROL_HPP
#define ZENGINE_DEMO_CONTROL_CONTROL_HPP
#include "vocabulary.hpp"
#include "activation/activation.hpp"
#include "workshop/pane_canvas_rows.hpp"
#include "workshop/pane_menu.hpp"
#include "workshop/pane_vocabulary.hpp"
#include "workshop/pane_text.hpp"
#include <zen/weave.hpp>

namespace zengine::demo {
namespace ws = zengine::workshop;
class Control final : public loom::WeaveBase<Control, DemoStatus,
    loom::Accept<loom::Activated, ws::PaneCatalogRequested, ws::PaneRoom, ws::PaneCanvasRoom,
                 ws::PaneCanvasPointer, ws::PaneCanvasRejected,
                 DemoServiceOpened, DemoServiceClosed, DemoWorkRequested, DemoWorkFinished,
                 DemoResetRequested, DemoStatusRequested, DemoReadyRequested>,
    loom::Emit<ws::v2::PaneOffered, ws::v4::PaneContent, ws::v5::PaneCanvasContent,
               ws::PanePassRequested, DemoWork, DemoStatus, loom::Ack, loom::Refused>> {
public:
    void on(const loom::Activated& a, loom::Mail& m) {
        if (!activation_.accept(m, a)) return;
        state_.state = "unavailable"; state_.note = "Start the demo's ELH service";
        offer(m);
    }
    void on(const ws::PaneCatalogRequested&, loom::Mail& m) {
        if (m.authored_from_role("zengine.workshop")) offer(m);
    }
    void on(const ws::PaneRoom& r, loom::Mail& m) {
        if (!m.authored_from_role("zengine.workshop") || r.pane != "controls") return;
        rows_ = r.rows; columns_ = r.columns; paint(m);
    }
    /// THE PANE DRAWS ITS ROWS ON ITS CANVAS while it holds a room there, and says them as prose
    /// only to a host that grants none.
    void on(const ws::PaneCanvasRoom& r, loom::Mail& m) {
        if (!m.authored_from_role("zengine.workshop") || r.pane != "controls") return;
        room_ = r; paint(m);
    }
    /// A primary press anywhere on the reset row resets; a right press is handed back, so
    /// Workshop's own pane menu opens there.
    void on(const ws::PaneCanvasPointer& p, loom::Mail& m) {
        if (!m.authored_from_role("zengine.workshop") || p.pane != "controls" ||
            p.grant != room_.grant || p.phase != ws::canvas_pointer::kPress) return;
        if (p.button == 3) {
            (void)ws::pane_menu::pass_back(m, kRole, "controls");
            return;
        }
        const ws::RowCell at = ws::row_cell_at(ws::canvas_rows(room_), p.x, p.y);
        if (p.button == 1 && at.shown && at.row == 1) reset(m, false);
    }
    /// A refused picture leaves the last good one showing; a later paint draws again.
    void on(const ws::PaneCanvasRejected&, loom::Mail&) {}
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
    void offer(loom::Mail& m) {
        m.as_role(kRole).send_to_role("zengine.workshop",
            ws::v2::PaneOffered{"controls", "Demo", "Repeat this demo through its attached ELH recipe", 4, 56});
    }
    void paint(loom::Mail& m) {
        if (state_.state == "ready" || state_.state == "failed") {
            for (auto it = waiters_.begin(); it != waiters_.end();) {
                if (it->generation <= state_.generation) {
                    (void)loom::answer_deferred(it->reply, m, state_);
                    it = waiters_.erase(it);
                } else ++it;
            }
        }
        const bool canvas = room_.grant > 0 && room_.width > 0 && room_.height > 0;
        const ws::CanvasRows lattice = ws::canvas_rows(room_);
        const std::int64_t budget = canvas ? lattice.rows : rows_;
        const std::int64_t columns = canvas ? lattice.columns : columns_;
        if (budget <= 0 || columns <= 0) return;
        std::vector<surface::SurfaceTextRow> rows;
        for (const auto& text : {state_.name, std::string("[ Reset demo ]"),
                                 state_.state + ": " + state_.note}) {
            if (static_cast<std::int64_t>(rows.size()) == budget) break;
            rows.push_back({ws::pane_text::drawable(ws::pane_text::fit(text, columns)), surface::role::kFill});
        }
        // THE RESET ROW IS NAMED `control:reset`, and the state beneath it `status`.
        std::vector<ws::PaneRowPart> parts;
        if (rows.size() > 1) parts.push_back(ws::PaneRowPart{"control:reset", 1, 0, columns});
        if (rows.size() > 2) parts.push_back(ws::PaneRowPart{"status", 2, 0, columns});
        if (canvas) {
            m.as_role(kRole).send_to_role("zengine.workshop",
                ws::rows_picture(room_, pictures_.next(room_, 0), rows, parts));
            return;
        }
        m.as_role(kRole).send_to_role("zengine.workshop",
                                      ws::v4::PaneContent{"controls", std::move(rows), 0, 0, std::move(parts)});
    }
    zengine::ActivationCursor activation_;
    loom::WeaveId worker_{};
    loom::DeferredAnswer next_, request_;
    struct Waiter { std::int64_t generation; loom::DeferredAnswer reply; };
    std::vector<Waiter> waiters_;
    bool offered_ = false, doing_ = false;
    std::int64_t rows_ = 0, columns_ = 0;
    ws::PaneCanvasRoom room_;
    ws::CanvasPictures pictures_;
};
} // namespace zengine::demo
#endif
