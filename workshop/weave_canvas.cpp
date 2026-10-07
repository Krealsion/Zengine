// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#include "weave.hpp"
#include "screen_canvas.hpp"
#include <cmath>
#include <limits>
#include <utility>

namespace zengine::workshop {

bool WorkshopWeave::canvas_owner_current(std::int64_t kind) const {
    const auto* row = session_.panes.runtime.of_kind(kind);
    const auto* pane = session_.panes.external_pane(kind);
    return row && pane && pane->canvas.grant > 0 && pane->canvas.owner.valid() &&
        host_->role_holder && host_->role_holder(row->provider) == pane->canvas.owner;
}

loom::Ticket WorkshopWeave::send_canvas_pointer(loom::WeaveId owner, const PaneCanvasPointer& e,
                                               bool legacy, loom::Mail& mail,
                                               std::uint64_t correlation) {
    if (!legacy) return mail.as_role(kWorkshopProvider).send(owner, e, correlation);
    return mail.as_role(kWorkshopProvider).send(owner,
        v1::PaneCanvasPointer{e.pane, e.grant, e.picture, e.gesture, e.phase, e.button,
                              legacy_subs_of_px(e.x), legacy_subs_of_px(e.y), e.modifiers, e.dx,
                              e.dy, e.keys_went_here},
        correlation);
}

void WorkshopWeave::send_canvas_hover(loom::WeaveId owner, const PaneCanvasHover& h, bool legacy,
                                      loom::Mail& mail) {
    if (!legacy) {
        (void)mail.as_role(kWorkshopProvider).send(owner, h);
        return;
    }
    (void)mail.as_role(kWorkshopProvider).send(owner,
        v1::PaneCanvasHover{h.pane, h.grant, h.picture, legacy_subs_of_px(h.x),
                            legacy_subs_of_px(h.y), h.over, h.carrying});
}

void WorkshopWeave::lose_canvas_hold(std::size_t slot, loom::Mail& mail) {
    auto& held = canvas_holds_[slot];
    if (!held.active) return;
    auto event = held.event;
    event.phase = canvas_pointer::kLost;
    (void)send_canvas_pointer(held.owner, event, held.legacy, mail);
    if (slot > 0) secondary_cont_[slot - 1] = SecondaryContinuation{};
    held = CanvasHold{};
}

void WorkshopWeave::end_canvas_holds(loom::Mail& mail) {
    for (std::size_t i = 0; i < 3; ++i) {
        const auto& held = canvas_holds_[i];
        if (!held.active) continue;
        const auto* pane = session_.panes.external_pane(held.kind);
        if (!session_.panes.has(held.kind) || !canvas_owner_current(held.kind) ||
            !pane || pane->canvas.grant != held.event.grant || session_.arrange.open ||
            session_.context.open || session_.presented.open)
            lose_canvas_hold(i, mail);
    }
}

void WorkshopWeave::refresh_canvas_rooms(loom::Mail& mail) {
    const auto sc = screen_of(session_);
    for (auto& pane : session_.panes.external) {
        const auto* row = session_.panes.runtime.of_kind(pane.kind);
        if (!row) continue;
        const auto owner = host_->role_holder ? host_->role_holder(row->provider) : loom::WeaveId{};
        const auto accepts = [&](const auto& schema) {
            return host_->holder_accepts && host_->holder_accepts(row->provider, *schema);
        };
        // The pixel doors first; a holder that accepts only the earlier sub-unit doors is met in
        // them, with the same geometry translated at the door.
        const bool current = owner.valid() && accepts(loom::schema_of<PaneCanvasRoom>()) &&
            accepts(loom::schema_of<PaneCanvasPointer>());
        const bool legacy = !current && owner.valid() &&
            accepts(loom::schema_of<v2::PaneCanvasRoom>()) &&
            accepts(loom::schema_of<v1::PaneCanvasPointer>());
        const bool capable = current || legacy;
        const auto where = bounds_of(session_.panes, session_.setup.active, pane.kind, sc);
        const std::int64_t titles = external_title_rows(session_.panes, pane.kind, session_.pane_titles);
        const auto body = capable && where.open ? canvas_body_place(where.rect, sc, titles) : PixelRect{};
        auto& c = pane.canvas;
        const auto grain = chrome_grain(sc);
        const bool graphical = sc.cell_px > 0;
        const bool same_kind = capable && c.owner == owner && c.grant != 0 && c.grain == grain &&
            c.graphical == graphical && c.text_advance_px == sc.text_advance_px &&
            c.text_line_px == sc.text_line_px && c.legacy == legacy;
        if (same_kind && c.x == body.x && c.y == body.y && c.width == body.w && c.height == body.h) {
            c.title_waits = false;
            continue;
        }
        // THE ROOM A HELD PRESS'S TITLE ROW MAKES WAITS FOR THE PRESS (WL-FOCUS-11): the keys a
        // press moved bring or take the title their pane wears while it has them, and the room
        // that row alone changes -- the pane where it was, its room the body it had with the other
        // title -- is granted once the press ends, so the press goes on in the room it was aimed
        // at and its picture stays where it was drawn until then.
        bool held = false;
        for (const CanvasHold& hold : canvas_holds_) held = held || (hold.active && hold.kind == pane.kind);
        const auto retitled = canvas_body_place(where.rect, sc, titles > 0 ? 0 : kExternalHeaderRows);
        if (same_kind && held && !body.empty() && c.x == retitled.x && c.y == retitled.y &&
            c.width == retitled.w && c.height == retitled.h) {
            c.title_waits = true;
            continue;
        }
        if (!capable && c.grant == 0) continue;
        for (std::size_t i = 0; i < 3; ++i)
            if (canvas_holds_[i].active && canvas_holds_[i].kind == pane.kind) lose_canvas_hold(i, mail);
        for (auto& continuation : secondary_cont_)
            if (continuation.kind == pane.kind) continuation = SecondaryContinuation{};
        if (canvas_hover_.kind == pane.kind) canvas_hover_ = CanvasHover{};
        // Only geometry may carry an old picture forward as an explicitly stale preview.
        // Input and grant identity still start over, including throughout repeated resizes.
        const bool preview = capable && c.owner == owner && c.grant > 0 &&
            (c.heard || c.preview) && c.width > 0 && c.height > 0 && !body.empty() &&
            c.grain == grain && c.graphical == graphical &&
            c.text_advance_px == sc.text_advance_px && c.text_line_px == sc.text_line_px &&
            c.legacy == legacy;
        v5::PaneCanvasContent previous;
        if (preview) previous = std::move(c.content);
        c = ExternalPane::Canvas{};
        pane.forget_pictures();
        pane.heard = false;
        pane.awaiting = true;
        pane.shown.clear();
        pane.parts.clear();
        if (!capable || canvas_grants_ == (std::numeric_limits<std::int64_t>::max)()) continue;
        c.owner = owner;
        c.grant = ++canvas_grants_;
        c.x = body.x; c.y = body.y; c.width = body.w; c.height = body.h;
        c.grain = grain; c.graphical = graphical;
        c.text_advance_px = sc.text_advance_px; c.text_line_px = sc.text_line_px;
        c.preview = preview;
        c.legacy = legacy;
        if (preview) c.content = std::move(previous);
        if (legacy) {
            (void)mail.as_role(kWorkshopProvider).send(owner,
                v2::PaneCanvasRoom{row->pane, c.grant, legacy_subs_of_px(c.width),
                                   legacy_subs_of_px(c.height), legacy_subs_of_px(grain), graphical,
                                   c.text_advance_px, c.text_line_px});
        } else {
            (void)mail.as_role(kWorkshopProvider).send(owner,
                PaneCanvasRoom{row->pane, c.grant, c.width, c.height, grain, graphical,
                               c.text_advance_px, c.text_line_px});
        }
    }
    end_canvas_holds(mail);
}

void WorkshopWeave::on(const PaneCanvasContent& content, loom::Mail& mail) {
    admit_canvas_content(canvas_content_of(content), mail);
}

void WorkshopWeave::on(const v2::PaneCanvasContent& content, loom::Mail& mail) {
    // JUDGED BY ITS OWN RULES, in the sub-units it was drawn in: a sliver narrower than a pixel is
    // a rect those rules allow, and only the conversion makes it nothing.
    admit_canvas_content(canvas_content_of(canvas_content_of_legacy(content)), mail,
                         canvas_content_problem(canvas_content_as_said(content)));
}

// A picture naming its parts: its names judged beside it, after every rule a picture meets.
void WorkshopWeave::on(const v4::PaneCanvasContent& content, loom::Mail& mail) {
    admit_canvas_content(
        canvas_content_of(PaneCanvasContent{content.pane, content.grant, content.picture,
                                            content.rects, content.labels, content.texts},
                          content.parts),
        mail);
}

void WorkshopWeave::on(const v5::PaneCanvasContent& content, loom::Mail& mail) {
    admit_canvas_content(content, mail);
}

void WorkshopWeave::admit_canvas_content(const v5::PaneCanvasContent& content, loom::Mail& mail,
                                         std::string_view said) {
    const auto* row = session_.panes.runtime.find(mail.authored_role(), content.pane);
    if (!row || mail.authored_role().empty()) return;
    auto* pane = session_.panes.external_pane(row->kind);
    const std::string named = canvas_parts_problem(content.parts);
    std::string_view reason;
    if (!pane || !canvas_owner_current(row->kind) || pane->canvas.owner != mail.sender())
        reason = "canvas provider no longer holds this pane";
    else if (content.grant != pane->canvas.grant || pane->canvas.width <= 0 || pane->canvas.height <= 0)
        reason = "canvas room grant is no longer current";
    else if (content.picture <= pane->picture)
        reason = "canvas picture number must increase within its grant";
    else if (!said.empty())
        reason = said;
    else reason = canvas_content_problem(content);
    if (reason.empty() && !named.empty()) reason = named;
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
    const auto* row = session_.panes.runtime.of_kind(kind);
    const auto* pane = session_.panes.external_pane(kind);
    const auto at = canvas_point_of(b.space, b.x, b.y);
    if (!row || !pane || pane->canvas.grant == 0 || !at.understood || b.button < 1 || b.button > 3)
        return false;
    const auto& c = pane->canvas;
    const PixelRect body{c.x, c.y, c.width, c.height};
    if (!body.contains_at(at.px.x, at.px.y, at.grain)) return false;
    // A waiting or retired picture owns its room, but cannot acquire a new gesture.
    if (!canvas_owner_current(kind) || !c.heard || pane->stamp.aimed <= 0) return true;
    const auto slot = static_cast<std::size_t>(b.button - 1);
    lose_canvas_hold(slot, mail);
    if (canvas_gestures_ == (std::numeric_limits<std::int64_t>::max)()) return true;
    PaneCanvasPointer event{row->pane, c.grant, pane->stamp.aimed, ++canvas_gestures_,
        canvas_pointer::kPress, b.button, surface::sub_px(at.px.x, c.x),
        surface::sub_px(at.px.y, c.y), b.modifiers, 0, 0, keys_went_here};
    const auto correlation = ++gesture_asks_;
    const auto sent = send_canvas_pointer(c.owner, event, c.legacy, mail, correlation);
    if (!sent.valid()) return true;
    canvas_holds_[slot] = CanvasHold{true, kind, c.owner, c.x, c.y, event, sent, c.legacy};
    // A primary press is the pane's to continue, as a prose press is: under its number the
    // provider may ask to carry a value out, and the press's release is where it lands.
    if (slot == 0) press_sent_ = GestureSent{kind, gestures_, correlation};
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
        event.x = surface::sub_px(at.px.x, held.origin_x);
        event.y = surface::sub_px(at.px.y, held.origin_y);
    }
    event.phase = at.understood ? canvas_pointer::kRelease : canvas_pointer::kLost;
    event.modifiers = b.modifiers;
    (void)send_canvas_pointer(held.owner, event, held.legacy, mail);
    if (slot > 0) secondary_cont_[slot - 1].released = true;
    const std::int64_t kind = held.kind;
    held = CanvasHold{};
    // A room the press's title row waited for is granted now the press has ended (WL-FOCUS-11),
    // and painted at once, so the picture stands in the room the pointer answers in.
    if (const ExternalPane* pane = session_.panes.external_pane(kind);
        pane != nullptr && pane->canvas.title_waits)
        repaint(mail);
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
        held.event.x = surface::sub_px(at.px.x, held.origin_x);
        held.event.y = surface::sub_px(at.px.y, held.origin_y);
        held.event.modifiers = m.modifiers;
        (void)send_canvas_pointer(held.owner, held.event, held.legacy, mail);
        note_routed(held.kind);
    }
    return sent;
}

// THE POINTER RESTING OVER A CANVAS: the canvas body on top under it, read from the geometry this
// host holds, told to a holder that accepts the hover door, and once per place.
// The canvas it left is told so first. A mode, a menu or a room the pointer is not over is none.
void WorkshopWeave::canvas_hover(const input::PointerMoved& m, loom::Mail& mail) {
    std::int64_t kind = kNoPaneKind;
    PaneCanvasHover over;
    loom::WeaveId owner{};
    const auto at = canvas_point_of(m.space, m.x, m.y);
    if (at.understood && !session_.arrange.open && !session_.context.open && !session_.presented.open) {
        const Occupancy here = occupied_at(session_.panes, session_.setup.active, screen_of(session_), at);
        const auto* row = here.occupied ? session_.panes.runtime.of_kind(here.kind) : nullptr;
        const auto* pane = here.occupied ? session_.panes.external_pane(here.kind) : nullptr;
        const bool hears = host_->holder_accepts && row && pane &&
            host_->holder_accepts(row->provider, pane->canvas.legacy
                                                     ? *loom::schema_of<v1::PaneCanvasHover>()
                                                     : *loom::schema_of<PaneCanvasHover>());
        if (row && pane && canvas_owner_current(here.kind) && pane->canvas.heard &&
            pane->stamp.aimed > 0 && hears) {
            const auto& c = pane->canvas;
            if (PixelRect{c.x, c.y, c.width, c.height}.contains_at(at.px.x, at.px.y, at.grain)) {
                kind = here.kind;
                owner = c.owner;
                over = PaneCanvasHover{row->pane, c.grant, pane->stamp.aimed,
                                       surface::sub_px(at.px.x, c.x), surface::sub_px(at.px.y, c.y),
                                       true, !carried_.data.empty()};
            }
        }
    }
    if (canvas_hover_.kind != kNoPaneKind && (canvas_hover_.kind != kind || canvas_hover_.grant != over.grant))
        leave_canvas_hover(mail);
    if (kind == kNoPaneKind) return;
    if (canvas_hover_.kind == kind && canvas_hover_.x == over.x && canvas_hover_.y == over.y &&
        canvas_hover_.carrying == over.carrying)
        return;
    const bool legacy = session_.panes.external_pane(kind)->canvas.legacy;
    send_canvas_hover(owner, over, legacy, mail);
    canvas_hover_ =
        CanvasHover{kind, over.grant, over.x, over.y, owner, over.pane, over.carrying, legacy};
}

void WorkshopWeave::leave_canvas_hover(loom::Mail& mail) {
    const auto was = std::exchange(canvas_hover_, CanvasHover{});
    if (was.kind == kNoPaneKind) return;
    const auto* pane = session_.panes.external_pane(was.kind);
    // A provider whose room was granted afresh has already put its hover down with that room.
    if (!pane || !canvas_owner_current(was.kind) || pane->canvas.grant != was.grant) return;
    send_canvas_hover(was.owner,
        PaneCanvasHover{was.pane, was.grant, pane->stamp.aimed, was.x, was.y, false, false},
        was.legacy, mail);
}

bool WorkshopWeave::canvas_wheel(std::int64_t kind, const input::PointerWheel& w, loom::Mail& mail) {
    const auto* row = session_.panes.runtime.of_kind(kind);
    const auto* pane = session_.panes.external_pane(kind);
    if (!row || !pane || pane->canvas.grant == 0) return false;
    const auto at = canvas_point_of(w.space, w.x, w.y);
    const auto& c = pane->canvas;
    if (!canvas_owner_current(kind) || !c.heard || pane->stamp.aimed <= 0 || !at.understood ||
        !PixelRect{c.x, c.y, c.width, c.height}.contains_at(at.px.x, at.px.y, at.grain) ||
        !std::isfinite(w.dx) || !std::isfinite(w.dy)) return true;
    if (canvas_gestures_ == (std::numeric_limits<std::int64_t>::max)()) return true;
    (void)send_canvas_pointer(c.owner,
        PaneCanvasPointer{row->pane, c.grant, pane->stamp.aimed, ++canvas_gestures_,
            canvas_pointer::kWheel, 0, surface::sub_px(at.px.x, c.x),
            surface::sub_px(at.px.y, c.y), w.modifiers, w.dx, w.dy, typing_pane(session_) == kind},
        c.legacy, mail);
    note_routed(kind);
    return true;
}
} // namespace zengine::workshop
