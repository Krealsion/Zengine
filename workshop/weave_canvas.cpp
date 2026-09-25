// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#include "weave.hpp"
#include "screen_canvas.hpp"
#include <cmath>
#include <limits>

namespace zengine::workshop {

bool WorkshopWeave::canvas_owner_current(std::int64_t kind) const {
    const auto* row = session_.panels.runtime.of_kind(kind);
    const auto* pane = session_.panels.external_pane(kind);
    return row && pane && pane->canvas.grant > 0 && pane->canvas.owner.valid() &&
        host_->role_holder && host_->role_holder(row->provider) == pane->canvas.owner;
}

void WorkshopWeave::lose_canvas_hold(std::size_t slot, loom::Mail& mail) {
    auto& held = canvas_holds_[slot];
    if (!held.active) return;
    auto event = held.event;
    event.phase = canvas_pointer::kLost;
    (void)mail.as_role(kWorkshopProvider).send(held.owner, event);
    if (slot > 0) secondary_cont_[slot - 1] = SecondaryContinuation{};
    held = CanvasHold{};
}

void WorkshopWeave::end_canvas_holds(loom::Mail& mail) {
    for (std::size_t i = 0; i < 3; ++i) {
        const auto& held = canvas_holds_[i];
        if (!held.active) continue;
        const auto* pane = session_.panels.external_pane(held.kind);
        if (!session_.panels.has(held.kind) || !canvas_owner_current(held.kind) ||
            !pane || pane->canvas.grant != held.event.grant || session_.arrange.open ||
            session_.context.open || session_.presented.open)
            lose_canvas_hold(i, mail);
    }
}

void WorkshopWeave::refresh_canvas_rooms(loom::Mail& mail) {
    const auto sc = screen_of(session_);
    for (auto& pane : session_.panels.external) {
        const auto* row = session_.panels.runtime.of_kind(pane.kind);
        if (!row) continue;
        const auto owner = host_->role_holder ? host_->role_holder(row->provider) : loom::WeaveId{};
        const bool capable = owner.valid() && host_->holder_accepts &&
            host_->holder_accepts(row->provider, *loom::schema_of<PaneCanvasRoom>()) &&
            host_->holder_accepts(row->provider, *loom::schema_of<PaneCanvasPointer>());
        const auto where = bounds_of(session_.panels, session_.setup.active, pane.kind, sc);
        const auto body = capable && where.open
            ? canvas_body_place(where.rect, sc,
                external_title_rows(session_.panels, pane.kind, session_.pane_titles)) : FineRect{};
        auto& c = pane.canvas;
        const auto grain = chrome_grain(sc);
        const bool graphical = sc.cell_px > 0;
        if (capable && c.owner == owner && c.grant != 0 && c.x == body.x && c.y == body.y &&
            c.width == body.w && c.height == body.h && c.grain == grain && c.graphical == graphical &&
            c.text_advance_px == sc.text_advance_px && c.text_line_px == sc.text_line_px)
            continue;
        if (!capable && c.grant == 0) continue;
        for (std::size_t i = 0; i < 3; ++i)
            if (canvas_holds_[i].active && canvas_holds_[i].kind == pane.kind) lose_canvas_hold(i, mail);
        for (auto& continuation : secondary_cont_)
            if (continuation.kind == pane.kind) continuation = SecondaryContinuation{};
        // Only geometry may carry an old picture forward as an explicitly stale preview.
        // Input and grant identity still start over, including throughout repeated resizes.
        const bool preview = capable && c.owner == owner && c.grant > 0 &&
            (c.heard || c.preview) && c.width > 0 && c.height > 0 && !body.empty() &&
            c.grain == grain && c.graphical == graphical &&
            c.text_advance_px == sc.text_advance_px && c.text_line_px == sc.text_line_px;
        PaneCanvasContent previous;
        if (preview) previous = std::move(c.content);
        c = ExternalPane::Canvas{};
        pane.forget_pictures();
        pane.heard = false;
        pane.awaiting = true;
        pane.shown.clear();
        if (!capable || canvas_grants_ == (std::numeric_limits<std::int64_t>::max)()) continue;
        c.owner = owner;
        c.grant = ++canvas_grants_;
        c.x = body.x; c.y = body.y; c.width = body.w; c.height = body.h;
        c.grain = grain; c.graphical = graphical;
        c.text_advance_px = sc.text_advance_px; c.text_line_px = sc.text_line_px;
        c.preview = preview;
        if (preview) c.content = std::move(previous);
        (void)mail.as_role(kWorkshopProvider).send(owner,
            PaneCanvasRoom{row->pane, c.grant, c.width, c.height, grain, graphical,
                           c.text_advance_px, c.text_line_px});
    }
    end_canvas_holds(mail);
}

void WorkshopWeave::on(const PaneCanvasContent& content, loom::Mail& mail) {
    const auto* row = session_.panels.runtime.find(mail.authored_role(), content.pane);
    if (!row || mail.authored_role().empty()) return;
    auto* pane = session_.panels.external_pane(row->kind);
    std::string_view reason;
    if (!pane || !canvas_owner_current(row->kind) || pane->canvas.owner != mail.sender())
        reason = "canvas provider no longer holds this pane";
    else if (content.grant != pane->canvas.grant || pane->canvas.width <= 0 || pane->canvas.height <= 0)
        reason = "canvas room grant is no longer current";
    else if (content.picture <= pane->picture)
        reason = "canvas picture number must increase within its grant";
    else reason = canvas_content_problem(content);
    if (!reason.empty()) {
        (void)mail.as_role(kWorkshopProvider).send(mail.sender(),
            PaneCanvasRejected{content.pane, content.grant, content.picture, std::string(reason)},
            mail.correlation());
        return;
    }
    pane->canvas.content = content;
    pane->canvas.heard = true;
    pane->canvas.preview = false;
    pane->picture = content.picture;
    pane->heard = true;
    pane->awaiting = false;
    pane->clear_refusal();
    pane->clear_caret();
    repaint(mail);
}

bool WorkshopWeave::canvas_press(std::int64_t kind, const input::PointerButton& b,
                                 bool keys_went_here, loom::Mail& mail) {
    const auto* row = session_.panels.runtime.of_kind(kind);
    const auto* pane = session_.panels.external_pane(kind);
    const auto at = canvas_point_of(b.space, b.x, b.y);
    if (!row || !pane || pane->canvas.grant == 0 || !at.understood || b.button < 1 || b.button > 3)
        return false;
    const auto& c = pane->canvas;
    const FineRect body{c.x, c.y, c.width, c.height};
    if (!body.contains_at(at.sub.x, at.sub.y, at.grain)) return false;
    // A waiting or retired picture owns its room, but cannot acquire a new gesture.
    if (!canvas_owner_current(kind) || !c.heard || pane->stamp.aimed <= 0) return true;
    const auto slot = static_cast<std::size_t>(b.button - 1);
    lose_canvas_hold(slot, mail);
    if (canvas_gestures_ == (std::numeric_limits<std::int64_t>::max)()) return true;
    PaneCanvasPointer event{row->pane, c.grant, pane->stamp.aimed, ++canvas_gestures_,
        canvas_pointer::kPress, b.button, surface::sub_px(at.sub.x, c.x),
        surface::sub_px(at.sub.y, c.y), b.modifiers, 0, 0, keys_went_here};
    const auto correlation = ++gesture_asks_;
    const auto sent = mail.as_role(kWorkshopProvider).send(c.owner, event, correlation);
    if (!sent.valid()) return true;
    canvas_holds_[slot] = CanvasHold{true, kind, c.owner, c.x, c.y, event, sent};
    if (slot > 0) {
        auto& continuation = secondary_cont_[slot - 1];
        continuation = SecondaryContinuation{};
        continuation.live = true;
        continuation.kind = kind;
        continuation.button = b.button;
        continuation.correlation = correlation;
        continuation.gesture_at_press = gestures_;
        continuation.cell = at;
    }
    note_routed(kind);
    return true;
}

bool WorkshopWeave::canvas_release(const input::PointerButton& b, loom::Mail& mail) {
    if (b.button < 1 || b.button > 3) return false;
    const auto slot = static_cast<std::size_t>(b.button - 1);
    const bool owed = canvas_holds_[slot].active;
    end_canvas_holds(mail);
    auto& held = canvas_holds_[slot];
    if (!held.active) return owed;
    auto event = held.event;
    const auto at = canvas_point_of(b.space, b.x, b.y);
    if (at.understood) {
        event.x = surface::sub_px(at.sub.x, held.origin_x);
        event.y = surface::sub_px(at.sub.y, held.origin_y);
    }
    event.phase = at.understood ? canvas_pointer::kRelease : canvas_pointer::kLost;
    event.modifiers = b.modifiers;
    (void)mail.as_role(kWorkshopProvider).send(held.owner, event);
    if (slot > 0) secondary_cont_[slot - 1].released = true;
    held = CanvasHold{};
    return true;
}

bool WorkshopWeave::canvas_motion(const input::PointerMoved& m, loom::Mail& mail) {
    end_canvas_holds(mail);
    bool sent = false;
    const auto at = canvas_point_of(m.space, m.x, m.y);
    for (std::size_t i = 0; i < 3; ++i) {
        auto& held = canvas_holds_[i];
        if (!held.active) continue;
        sent = true;
        if (!at.understood) { lose_canvas_hold(i, mail); continue; }
        held.event.phase = canvas_pointer::kMove;
        held.event.x = surface::sub_px(at.sub.x, held.origin_x);
        held.event.y = surface::sub_px(at.sub.y, held.origin_y);
        held.event.modifiers = m.modifiers;
        (void)mail.as_role(kWorkshopProvider).send(held.owner, held.event);
        note_routed(held.kind);
    }
    return sent;
}

bool WorkshopWeave::canvas_wheel(std::int64_t kind, const input::PointerWheel& w, loom::Mail& mail) {
    const auto* row = session_.panels.runtime.of_kind(kind);
    const auto* pane = session_.panels.external_pane(kind);
    if (!row || !pane || pane->canvas.grant == 0) return false;
    const auto at = canvas_point_of(w.space, w.x, w.y);
    const auto& c = pane->canvas;
    if (!canvas_owner_current(kind) || !c.heard || pane->stamp.aimed <= 0 || !at.understood ||
        !FineRect{c.x, c.y, c.width, c.height}.contains_at(at.sub.x, at.sub.y, at.grain) ||
        !std::isfinite(w.dx) || !std::isfinite(w.dy)) return true;
    if (canvas_gestures_ == (std::numeric_limits<std::int64_t>::max)()) return true;
    (void)mail.as_role(kWorkshopProvider).send(c.owner,
        PaneCanvasPointer{row->pane, c.grant, pane->stamp.aimed, ++canvas_gestures_,
            canvas_pointer::kWheel, 0, surface::sub_px(at.sub.x, c.x),
            surface::sub_px(at.sub.y, c.y), w.modifiers, w.dx, w.dy, typing_pane(session_) == kind});
    note_routed(kind);
    return true;
}
} // namespace zengine::workshop
