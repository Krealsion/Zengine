// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// `WorkshopWeave`'s external panes: the room grant, the forwarded press, key, wheel and text, the
// second button, and a pane's own menu.
// Workshop law: agents/workshop/focus.md (+4 registers; agents/workshop.md routes)

#include "weave.hpp"

namespace zengine::workshop {

// ---- THE EXTERNAL PANE'S ROOM AND GESTURES: the grant, a press, a key, the wheel, text ----

// WL-PANE-06 -- agents/workshop/panes-and-windows.md
void WorkshopWeave::refresh_external_rooms(loom::Mail& mail) {
    refresh_canvas_rooms(mail);
    const Screen sc = screen_of(session_);
    for (const Panel& p : session_.panels.open) {
        if (!is_runtime_kind(p.kind)) {
            continue;
        }
        const PanelBounds where = bounds_of(session_.panels, session_.setup.active, p.kind, sc);
        const ExternalBodyPlace body = external_body_place(
            where.rect, sc,
            external_title_rows(session_.panels, p.kind, session_.pane_titles));
        if (!body.present) {
            continue;
        }
        const RuntimePane* row = session_.panels.runtime.of_kind(p.kind);
        ExternalPane* pane = session_.panels.external_pane(p.kind);
        if (row == nullptr || pane == nullptr) {
            continue;
        }
        if (pane->granted && pane->rows == body.rows && pane->columns == body.columns) {
            continue; // the same room: saying so again would be noise a provider must parse
        }
        const std::string office = row->provider;
        const std::string key = row->pane;
        pane->rows = body.rows;
        pane->columns = body.columns;
        pane->granted = true;
        pane->shown.clear();
        pane->heard = false;
        pane->awaiting = true;
        // The refusal is not cleared: a room goes out whenever the surface resizes, and only
        // content this host accepted may take back a sentence about content (WL-ATTN-04).
        // Authored as `zengine.workshop`, so the provider can verify the ask, and sent to the
        // role, so a replaced provider still gets its room.
        (void)mail.as_role(kWorkshopProvider)
            .send_to_role(office, PaneRoom{key, body.rows, body.columns});
    }
}

// WL-FOCUS-04 -- agents/workshop/focus.md
bool holder_accepts_on(const loom::Switchboard& bus, std::string_view role,
                       const loom::Schema& shape) {
    const loom::WeaveId holder = bus.role_holder(role);
    if (!holder.valid()) {
        return false;
    }
    for (const std::shared_ptr<const loom::Schema>& door : bus.accepted_schemas(holder)) {
        if (door != nullptr && loom::same_identity(*door, shape)) {
            return true;
        }
    }
    return false;
}

// WL-PRESS-04 -- agents/workshop/press-chain.md; WL-FOCUS-04 -- agents/workshop/focus.md
bool WorkshopWeave::external_press(std::int64_t kind, const ExternalPressAt& at,
                                   bool keys_went_here, loom::Mail& mail) {
    if (!at.named) {
        return false;
    }
    const RuntimePane* row = session_.panels.runtime.of_kind(kind);
    const ExternalPane* pane = session_.panels.external_pane(kind);
    // A pane not yet granted a room has no lattice to name a place in: a press between its opening
    // and the granting repaint names nothing.
    if (row == nullptr || pane == nullptr || !pane->granted || pane->canvas.grant != 0) {
        return false;
    }
    // One press, one sentence, in the version the office's holder accepts now. A holder replaced
    // before delivery may refuse it; Loom records that, and nothing resends in another version.
    // Under a number, so a pane answering with its own rows continues this gesture
    // (`on(PaneMenuRequested)`).
    const std::uint64_t answering = ++escape_asks_;
    if (host_->holder_accepts &&
        host_->holder_accepts(row->provider, *loom::schema_of<v3::PanePressed>())) {
        // Version three names the picture the press was aimed at: the newest the medium had been
        // handed when the press was read (`PictureFence`), not merely the newest admitted.
        (void)mail.as_role(kWorkshopProvider)
            .send_to_role(row->provider,
                          v3::PanePressed{row->pane, at.row, at.column, keys_went_here,
                                          pane->stamp.aimed},
                          answering);
    } else if (host_->holder_accepts &&
               host_->holder_accepts(row->provider, *loom::schema_of<v2::PanePressed>())) {
        (void)mail.as_role(kWorkshopProvider)
            .send_to_role(row->provider,
                          v2::PanePressed{row->pane, at.row, at.column, keys_went_here},
                          answering);
    } else {
        (void)mail.as_role(kWorkshopProvider)
            .send_to_role(row->provider, PanePressed{row->pane, at.row, at.column}, answering);
    }
    press_sent_ = EscapeSent{kind, gestures_, answering};
    note_routed(kind); // admitted work, not yet delivered (WL-OPEN-03)
    return true;
}

// WL-TEXT-14 -- agents/workshop/text-box.md
void WorkshopWeave::external_drag(std::int64_t kind, const zengine::input::PointerMoved& m,
                                  loom::Mail& mail) {
    const RuntimePane* row = session_.panels.runtime.of_kind(kind);
    const ExternalPane* pane = session_.panels.external_pane(kind);
    if (row == nullptr || pane == nullptr || !pane->granted || !session_.panels.has(kind)) {
        session_.text_drag = TextDrag{}; // the pane the press began in is gone: the sweep ends
        return;
    }
    // THE SAME MEASURER THE PRESS SPENT, resolved at THIS motion against the body the pane
    // has now -- and then NOT clamped: a row above the body is negative, a row below it is
    // past `rows`, and what either means is the pane's (`PaneDragged`).
    const Screen sc = screen_of(session_);
    const PanelBounds where = bounds_of(session_.panels, session_.setup.active, kind, sc);
    if (!where.open) {
        session_.text_drag = TextDrag{};
        return;
    }
    const ExternalBodyPlace body = external_body_place(
        where.rect, sc, external_title_rows(session_.panels, kind, session_.pane_titles));
    if (!body.present) {
        return; // a room too small for a row: the sweep waits for one, and sends nothing
    }
    const ProseAt at = prose_at(m.space, m.x, m.y, body.region_x, body.region_y, body.fit);
    if (!at.understood) {
        return;
    }
    (void)mail.as_role(kWorkshopProvider)
        .send_to_role(row->provider, PaneDragged{row->pane, at.row - body.header_rows, at.column});
    note_routed(kind);
}

// WL-FOCUS-01, WL-FOCUS-05 -- agents/workshop/focus.md
std::int64_t WorkshopWeave::keyboard_pane() const {
    return zengine::workshop::keyboard_pane(session_.panels);
}

// WL-KEY-15 -- agents/workshop/keyboard.md; WL-ARR-16 -- agents/workshop/arrangement.md
bool WorkshopWeave::external_key(std::int64_t kind, const zengine::input::KeyPressed& k,
                                 loom::Mail& mail) {
    const RuntimePane* row = session_.panels.runtime.of_kind(kind);
    if (row == nullptr) {
        return false;
    }
    const bool escape = k.scancode == input::scan::kEscape && k.modifiers == input::mod::kNone;
    // Only an Escape gets its own number, the identity an answer must echo; no published shape
    // gains a field. The pane's own rows come first, against the effective map, and a match
    // crosses as the id, not the key. Every declared action goes out under its own number, so a
    // menu asked for by key is told apart from one about an earlier keystroke (`action_sent_`).
    if (const PaneRow* action =
            session_.keymap.pane_action_for(kind, k.scancode, k.modifiers)) {
        const std::uint64_t answering = ++escape_asks_;
        (void)mail.as_role(kWorkshopProvider)
            .send_to_role(row->provider, PaneActionRequested{row->pane, action->id}, answering);
        note_routed(kind);
        action_sent_ = EscapeSent{kind, gestures_, answering};
        if (escape) {
            escape_sent_ = EscapeSent{kind, gestures_, answering};
        }
        return true;
    }
    const std::uint64_t answering = escape ? ++escape_asks_ : 0;
    // An Escape nothing on the far side could spend: the holder accepts no key and the pane
    // declared no row for it, so the key is this host's. A host that cannot ask sends it.
    if (escape && host_->holder_accepts &&
        !host_->holder_accepts(row->provider, *loom::schema_of<PaneKey>())) {
        return false;
    }
    (void)mail.as_role(kWorkshopProvider)
        .send_to_role(row->provider, PaneKey{row->pane, k.scancode, k.modifiers}, answering);
    note_routed(kind);
    if (escape) {
        escape_sent_ = EscapeSent{kind, gestures_, answering};
    }
    return true;
}

void WorkshopWeave::external_wheel(std::int64_t kind, const zengine::input::PointerWheel& w,
                                   loom::Mail& mail) {
    const ExternalPressAt at =
        external_press_at(session_.panels, session_.setup.active, screen_of(session_), kind,
                          session_.pane_titles, w.space, w.x, w.y);
    if (!at.named) {
        return;
    }
    const RuntimePane* row = session_.panels.runtime.of_kind(kind);
    const ExternalPane* pane = session_.panels.external_pane(kind);
    if (row == nullptr || pane == nullptr || !pane->granted) {
        return;
    }
    (void)mail.as_role(kWorkshopProvider)
        .send_to_role(row->provider, PaneWheel{row->pane, w.dx, w.dy});
    note_routed(kind);
}

// ---- The second button: custody, continuation, and a pane's own menu -------------------------

namespace {
std::size_t secondary_slot(std::int64_t button) noexcept { return button == 2 ? 0 : 1; }
} // namespace

// WL-PRESS-06 -- agents/workshop/press-chain.md
bool WorkshopWeave::external_button(std::int64_t kind, std::int64_t button,
                                    const ExternalPressAt& at, const PointedAt& cell,
                                    loom::Mail& mail) {
    if (!at.named || (button != 2 && button != 3)) {
        return false;
    }
    const RuntimePane* row = session_.panels.runtime.of_kind(kind);
    const ExternalPane* pane = session_.panels.external_pane(kind);
    if (row == nullptr || pane == nullptr || !pane->granted) {
        return false;
    }
    // THE DOOR, READ OFF THE BUS NOW: a holder without it is sent nothing, and the host's own
    // chrome answers as it always did. (A host that cannot ask sends nothing.)
    if (!host_->holder_accepts ||
        !host_->holder_accepts(row->provider, *loom::schema_of<PaneButton>())) {
        return false;
    }
    const std::size_t s = secondary_slot(button);
    // ARBITRATION, NEVER SILENCE: a press of a button believed to be down means its release was
    // never reported (a lost focus, a window that swallowed it); the old hold ends aloud, with a
    // `lost` release owed to its pane, before the new press is recorded.
    if (secondary_hold_[s].active) {
        if (const RuntimePane* old = session_.panels.runtime.of_kind(secondary_hold_[s].kind)) {
            (void)mail.as_role(kWorkshopProvider)
                .send_to_role(old->provider, PaneButton{old->pane, button, false, 0, 0, true, 0});
        }
        secondary_hold_[s] = SecondaryHold{};
        secondary_cont_[s] = SecondaryContinuation{};
    }
    // THIS PRESS IS A GESTURE FOR THE OTHER BUTTON'S CONTINUATION.
    for (SecondaryContinuation& other : secondary_cont_) {
        if (other.live && other.button != button) {
            other.interrupted = true;
        }
    }
    const std::uint64_t answering = ++escape_asks_;
    const loom::Ticket sent =
        mail.as_role(kWorkshopProvider)
            .send_to_role(row->provider,
                          PaneButton{row->pane, button, true, at.row, at.column, false,
                                     pane->stamp.aimed},
                          answering);
    if (!sent.valid()) {
        return false; // nothing queued: known non-delivery, and the host's surface answers
    }
    secondary_hold_[s] = SecondaryHold{true, kind, button, answering, sent};
    SecondaryContinuation& c = secondary_cont_[s];
    c = SecondaryContinuation{};
    c.live = true;
    c.kind = kind;
    c.button = button;
    c.correlation = answering;
    c.gesture_at_press = gestures_;
    c.cell = cell;
    note_routed(kind);
    return true;
}

// WL-PRESS-06 -- agents/workshop/press-chain.md
bool WorkshopWeave::external_release(std::int64_t button, const zengine::input::PointerButton& b,
                                     loom::Mail& mail) {
    if (button != 2 && button != 3) {
        return false;
    }
    const std::size_t s = secondary_slot(button);
    SecondaryHold& h = secondary_hold_[s];
    if (!h.active) {
        return false;
    }
    const std::int64_t kind = h.kind;
    h = SecondaryHold{};
    // THE RELEASE ENDS CUSTODY AND NEVER RESTORES ELIGIBILITY: a continuation stays eligible only
    // if nothing but this release happened since its press -- and the release itself is no
    // gesture (`on(PointerButton)`), so "nothing since" is the count still standing at the press.
    SecondaryContinuation& c = secondary_cont_[s];
    if (c.live && !c.released) {
        c.released = true;
    }
    const RuntimePane* row = session_.panels.runtime.of_kind(kind);
    if (row == nullptr) {
        return false;
    }
    // THE PLACE, resolved against the pane's body NOW and not clamped (`PaneDragged`'s rule); a
    // body that cannot be resolved sends the release at (0, 0), which a pane must not read as a
    // row it was aimed at -- `lost` is false: a hand did let go.
    std::int64_t prow = 0;
    std::int64_t pcol = 0;
    if (session_.panels.has(kind)) {
        const Screen sc = screen_of(session_);
        const PanelBounds where = bounds_of(session_.panels, session_.setup.active, kind, sc);
        if (where.open) {
            const ExternalBodyPlace body = external_body_place(
                where.rect, sc, external_title_rows(session_.panels, kind, session_.pane_titles));
            if (body.present) {
                const ProseAt at =
                    prose_at(b.space, b.x, b.y, body.region_x, body.region_y, body.fit);
                if (at.understood) {
                    prow = at.row - body.header_rows;
                    pcol = at.column;
                }
            }
        }
    }
    (void)mail.as_role(kWorkshopProvider)
        .send_to_role(row->provider, PaneButton{row->pane, button, false, prow, pcol, false, 0});
    note_routed(kind);
    return true;
}

// WL-PRESS-06 -- agents/workshop/press-chain.md
void WorkshopWeave::end_lost_holds(loom::Mail& mail) {
    end_canvas_holds(mail);
    for (std::size_t s = 0; s < 2; ++s) {
        SecondaryHold& h = secondary_hold_[s];
        if (h.active && !session_.panels.has(h.kind)) {
            if (const RuntimePane* row = session_.panels.runtime.of_kind(h.kind)) {
                (void)mail.as_role(kWorkshopProvider)
                    .send_to_role(row->provider,
                                  PaneButton{row->pane, h.button, false, 0, 0, true, 0});
            }
            h = SecondaryHold{};
        }
        // The continuation is invalidated on its own terms: a pane that left the desk after the
        // release cannot have its press handed back or a menu opened for it.
        SecondaryContinuation& c = secondary_cont_[s];
        if (c.live && !session_.panels.has(c.kind)) {
            c = SecondaryContinuation{};
        }
    }
    // ...AND A MENU PRESENTED FOR A PANE THAT LEFT is withdrawn: its subject has no room on the
    // desk any more, and a choice about it would reach a pane the maker cannot see. The presenter
    // answers the requester unchosen, in these words.
    if (session_.presented.open) {
        const RuntimePane* row =
            session_.panels.runtime.find(session_.presented.office, session_.presented.pane);
        if (row == nullptr || !session_.panels.has(row->kind)) {
            withdraw_menu("the pane left the desk", mail);
        }
    }
    if (choice_answered_.kind != kNoPaneKind && !session_.panels.has(choice_answered_.kind)) {
        choice_answered_ = ChoiceAnswered{};
    }
}

// WL-PRESS-06 -- agents/workshop/press-chain.md
bool WorkshopWeave::end_refused_button(const loom::Ticket& refused_attempt, loom::Mail& mail) {
    if (refused_attempt.valid()) {
        for (std::size_t i = 0; i < 3; ++i) {
            auto& held = canvas_holds_[i];
            if (held.active && held.attempt.valid() && held.attempt.seq == refused_attempt.seq) {
                held = CanvasHold{};
                if (i > 0) secondary_cont_[i - 1] = SecondaryContinuation{};
                return true;
            }
        }
    }
    if (!refused_attempt.valid()) {
        return false;
    }
    for (std::size_t s = 0; s < 2; ++s) {
        SecondaryHold& h = secondary_hold_[s];
        if (!h.active || !h.attempt.valid() || h.attempt.seq != refused_attempt.seq) {
            continue; // not this slot's attempt (or an old one): it cancels nothing newer
        }
        // The press never reached its recipient: the hold and its continuation are dropped, so the
        // release sends nothing and no pass-back or menu finds a record. The pane, never told,
        // holds nothing; Loom's tap already attributes the refusal.
        h = SecondaryHold{};
        secondary_cont_[s] = SecondaryContinuation{};
        (void)mail;
        return true;
    }
    return false;
}

// WL-CTX-08 -- agents/workshop/contextual.md
void WorkshopWeave::on(const PanePassRequested& said, loom::Mail& mail) {
    const std::string_view office = mail.authored_role();
    if (office.empty()) {
        return;
    }
    const RuntimePane* row = session_.panels.runtime.find(office, said.pane);
    if (row == nullptr) {
        return; // a pane this office never offered is no pane of the desk's
    }
    if (const auto* pane = session_.panels.external_pane(row->kind);
        pane && pane->canvas.grant != 0 &&
        (!canvas_owner_current(row->kind) || pane->canvas.owner != mail.sender())) return;
    if (mail.correlation() == 0) {
        return; // an answer echoing nothing answers nothing
    }
    SecondaryContinuation* c = nullptr;
    for (SecondaryContinuation& each : secondary_cont_) {
        if (each.live && each.correlation == mail.correlation()) {
            c = &each;
        }
    }
    if (c == nullptr || c->kind != row->kind || c->spent) {
        return; // stale, another pane's, or already handed back
    }
    if (c->interrupted || gestures_ != c->gesture_at_press) {
        return; // the maker did something since: the press is not their latest act
    }
    c->spent = true;
    // THE HOST'S CONFIGURED FALLBACK FOR A BODY PRESS: its own pane menu, at the press's cell,
    // on the subject the record names rather than whatever is under the hand now. A pane's menu
    // still presented is withdrawn first -- one surface, and the newer act wins.
    withdraw_menu("a newer press", mail);
    ContextMenu next;
    next.open = true;
    next.anchored = true;
    next.anchor_x = c->cell.cell.x;
    next.anchor_y = c->cell.cell.y;
    next.subject = context_subject::kPane;
    next.pane = PaneRef{row->provider, row->pane};
    session_.context = next;
    repaint(mail);
}

// WL-CTX-09 -- agents/workshop/pane-menu.md
PointedAt WorkshopWeave::cell_of_body_place(std::int64_t kind, std::int64_t row,
                                            std::int64_t column) const {
    PointedAt out;
    if (!session_.panels.has(kind)) {
        return out;
    }
    const Screen sc = screen_of(session_);
    const PanelBounds where = bounds_of(session_.panels, session_.setup.active, kind, sc);
    if (!where.open) {
        return out;
    }
    const ExternalBodyPlace body = external_body_place(
        where.rect, sc, external_title_rows(session_.panels, kind, session_.pane_titles));
    if (!body.present) {
        return out;
    }
    // A PLACE OUTSIDE THE GRANTED LATTICE ANCHORS AT THE BODY'S ORIGIN: the pane named a row it
    // no longer has (the room shrank behind its request), and the menu still opens beside it.
    const std::int64_t r = row >= 0 && row < body.fit.rows ? row : 0;
    const std::int64_t c = column >= 0 && column < body.fit.columns ? column : 0;
    out.understood = true;
    out.cell.x = body.region_x + c;
    out.cell.y = body.region_y + body.header_rows + r;
    out.sub.x = out.cell.x * surface::kCellGrainSubs;
    out.sub.y = out.cell.y * surface::kCellGrainSubs;
    out.grain = surface::kCellGrainSubs;
    return out;
}

// WL-CTX-09 -- agents/workshop/pane-menu.md
void WorkshopWeave::on(const PaneMenuRequested& asked, loom::Mail& mail) {
    const std::string_view office = mail.authored_role();
    if (office.empty()) {
        return; // an office asks; personal speech is answered by nobody
    }
    const RuntimePane* row = session_.panels.runtime.find(office, asked.pane);
    if (row == nullptr) {
        return; // a pane this office never offered
    }
    if (const auto* pane = session_.panels.external_pane(row->kind);
        pane && pane->canvas.grant != 0 &&
        (!canvas_owner_current(row->kind) || pane->canvas.owner != mail.sender())) return;
    // THE HOST ANSWERS ONLY WHAT IT REFUSES HERE: an ask it never grants reaches no presenter, so
    // nobody else could answer it. What the rows say -- how many, how long, whether any -- is the
    // presenter's to judge and refuse; this handler judges custody and nothing about content.
    const auto refuse = [&](const std::string& why) {
        (void)mail.as_role(kWorkshopProvider)
            .send_to_role(row->provider,
                          PaneMenuAnswered{asked.pane, asked.subject, false, std::string(), why},
                          mail.correlation());
    };
    if (mail.correlation() == 0) {
        refuse("a menu continues a gesture, and this request echoes none");
        return;
    }
    if (!session_.panels.has(row->kind)) {
        refuse("the pane is not on the desk");
        return;
    }
    // Eligibility, judged where the menu opens: a request continues a secondary press on a
    // pass-back's terms, or a keyed action while that keystroke is the maker's latest act. A late
    // menu is refused; nothing here moves the keys or the selection.
    PointedAt at;
    bool eligible = false;
    for (SecondaryContinuation& c : secondary_cont_) {
        if (!c.live || c.correlation != mail.correlation()) {
            continue;
        }
        if (c.kind == row->kind && !c.spent && !c.interrupted && gestures_ == c.gesture_at_press) {
            c.spent = true;
            at = c.cell;
            eligible = true;
        }
        break;
    }
    if (!eligible && action_sent_.answering == mail.correlation() &&
        action_sent_.kind == row->kind && action_sent_.gesture == gestures_) {
        action_sent_ = EscapeSent{};
        at = cell_of_body_place(row->kind, asked.row, asked.column);
        eligible = true;
    }
    // ...OR A PRIMARY PRESS, on the same three terms: this pane, this number, and still the
    // maker's latest act. A pane that draws a `[menu]` control answers the click that hit it.
    if (!eligible && press_sent_.answering == mail.correlation() &&
        press_sent_.kind == row->kind && press_sent_.gesture == gestures_) {
        press_sent_ = EscapeSent{};
        at = cell_of_body_place(row->kind, asked.row, asked.column);
        eligible = true;
    }
    if (!eligible) {
        refuse("late -- the maker acted since that gesture, or it was already spent");
        return;
    }
    // ...AND SOMEBODY TO PRESENT IT. The office's holder is read off the bus now; the role is
    // resolved again at delivery, and a presenter that left in between is Loom's refusal to
    // settle (`on(DispatchRefused)`), answered then.
    if (!host_->holder_accepts ||
        !host_->holder_accepts(kPresenterRole, *loom::schema_of<MenuGranted>())) {
        const std::string why = std::string("no presenter holds `") + kPresenterRole +
                                "` -- nothing can present this menu";
        refuse(why);
        // ...AND THE MAKER IS TOLD, because a missing participant is a fact about this room, and
        // a right-click that opened nothing with nothing said would read as a dead mouse.
        say(row->name + "'s menu did not open -- " + why, true);
        repaint(mail);
        return;
    }
    grant_menu(*row, asked, mail.correlation(), at, mail);
    repaint(mail);
}

// WL-CTX-09 -- agents/workshop/pane-menu.md
void WorkshopWeave::grant_menu(const RuntimePane& row, const PaneMenuRequested& asked,
                               std::uint64_t correlation, const PointedAt& at, loom::Mail& mail) {
    // ONE SURFACE AT A TIME: an older menu is withdrawn (its presenter answers it) and the host's
    // own menu closed -- the newer, deliberate gesture wins.
    withdraw_menu("replaced by a newer menu", mail);
    close_context();
    const PanelProsePlace room =
        presented_room(at.understood, at.cell.x, at.cell.y, screen_of(session_));
    PresentedMenu next;
    next.open = true;
    next.menu = ++menus_;
    next.office = row.provider;
    next.pane = row.pane;
    next.subject = asked.subject;
    next.correlation = correlation;
    next.anchored = at.understood;
    next.anchor_x = at.cell.x;
    next.anchor_y = at.cell.y;
    next.room_rows = room.present ? room.rows : 0;
    next.room_columns = room.present ? room.columns : 0;
    next.first_input = gestures_;
    next.last_input = gestures_;
    session_.presented = next;
    // TO THE ROLE, AS THIS OFFICE, UNDER THE REQUEST'S NUMBER: the presenter answers the requester
    // under the same number, and authenticates this grant the way a pane authenticates a room.
    const loom::Ticket sent =
        mail.as_role(kWorkshopProvider)
            .send_to_role(kPresenterRole,
                          MenuGranted{next.menu, next.office, next.pane, next.subject, asked.rows,
                                      next.room_rows, next.room_columns},
                          correlation);
    if (!sent.valid()) {
        end_menu_unanswered("the menu could not be handed to a presenter", mail);
        return;
    }
    // ...AND FROM THIS ATTEMPT ON, what this host says to the presenter's office is about this
    // menu until it ends: a refusal Loom names by an attempt this late is this menu's own.
    session_.presented.first_attempt = sent.seq;
}

// WL-CTX-09 -- agents/workshop/pane-menu.md
void WorkshopWeave::withdraw_menu(const std::string& why, loom::Mail& mail) {
    if (!session_.presented.open) {
        return;
    }
    const PresentedMenu ended = session_.presented;
    session_.presented = PresentedMenu{};
    // The presenter answers its requester; this host only says the menu is over. Until Loom has had
    // its say about this sentence, who asked is kept, so a withdrawal the presenter cannot receive
    // is answered here.
    const loom::Ticket sent =
        mail.as_role(kWorkshopProvider).send_to_role(kPresenterRole, MenuWithdrawn{ended.menu, why});
    WithdrawnMenu withdrawn;
    withdrawn.menu = ended.menu;
    withdrawn.office = ended.office;
    withdrawn.pane = ended.pane;
    withdrawn.subject = ended.subject;
    withdrawn.correlation = ended.correlation;
    withdrawn.why = why;
    withdrawn.first_attempt = ended.first_attempt;
    withdrawn.last_attempt = sent.seq;
    if (!sent.valid()) {
        // NOTHING QUEUED: no presenter will hear it, so none will answer. Answered now, and
        // nothing is kept -- there is no attempt for Loom to say anything about.
        answer_withdrawn(withdrawn, why + " -- the presenter could not be told (nothing was queued)",
                         mail);
        return;
    }
    withdrawn_.push_back(std::move(withdrawn));
    // ...UNTIL THIS HAS COME ROUND TWICE: every refusal of the menu's sentences is queued ahead of
    // the second hop (`WithdrawalFence`), so the record is forgotten with nothing left to hear.
    (void)mail.as_role(kWorkshopProvider)
        .send_to_role(kWorkshopProvider, WithdrawalFence{ended.menu, 1});
}

// WL-CTX-10 -- agents/workshop/pane-menu.md
void WorkshopWeave::answer_withdrawn(const WithdrawnMenu& menu, const std::string& why,
                                     loom::Mail& mail) {
    // THE HOST ANSWERS WHAT NO PRESENTER CAN -- unchosen, as its office, about the pane and subject
    // asked, under the ask's own number, so the requester's record settles and nothing is chosen.
    (void)mail.as_role(kWorkshopProvider)
        .send_to_role(menu.office,
                      PaneMenuAnswered{menu.pane, menu.subject, false, std::string(), why},
                      menu.correlation);
}

// WL-CTX-10 -- agents/workshop/pane-menu.md
bool WorkshopWeave::end_refused_menu(const loom::Ticket& refused_attempt,
                                     const std::string& reason, loom::Mail& mail) {
    if (!refused_attempt.valid()) {
        return false;
    }
    const std::uint64_t attempt = refused_attempt.seq;
    // THE MENU ON THE SCREEN, when the refused sentence was its own -- its grant or an act
    // forwarded to it: no presenter can answer it, so this host ends it and answers.
    const PresentedMenu& open = session_.presented;
    if (open.open && open.first_attempt != 0 && attempt >= open.first_attempt) {
        end_menu_unanswered("the presenter left -- " + reason, mail);
        return true;
    }
    // ...OR ONE ALREADY WITHDRAWN, whose requester the presenter would have answered had it been
    // told: answered here, once, and forgotten.
    for (std::size_t i = 0; i < withdrawn_.size(); ++i) {
        if (attempt < withdrawn_[i].first_attempt || attempt > withdrawn_[i].last_attempt) {
            continue;
        }
        const WithdrawnMenu refused = withdrawn_[i];
        withdrawn_.erase(withdrawn_.begin() + static_cast<std::ptrdiff_t>(i));
        answer_withdrawn(refused, refused.why + " -- the presenter could not be told (" + reason + ")",
                         mail);
        return true;
    }
    // AN OLDER MENU'S SENTENCE, whose menu ended answered: it ends, alters and answers nothing
    // newer -- the newer menu's own sentences all come after its grant.
    return false;
}

// WL-CTX-10 -- agents/workshop/pane-menu.md
void WorkshopWeave::forget_withdrawn(std::int64_t menu) {
    for (std::size_t i = 0; i < withdrawn_.size(); ++i) {
        if (withdrawn_[i].menu == menu) {
            withdrawn_.erase(withdrawn_.begin() + static_cast<std::ptrdiff_t>(i));
            return;
        }
    }
}

// WL-CTX-10 -- agents/workshop/pane-menu.md
void WorkshopWeave::on(const WithdrawalFence& fence, loom::Mail& mail) {
    if (!mail.authored_from_role(kWorkshopProvider) || fence.menu <= 0 || fence.menu > menus_) {
        return; // only this office's own fence, about a menu it granted, forgets anything
    }
    if (fence.hop == 1) {
        (void)mail.as_role(kWorkshopProvider)
            .send_to_role(kWorkshopProvider, WithdrawalFence{fence.menu, 2});
        return;
    }
    if (fence.hop != 2) {
        return;
    }
    // Every refusal of the menu's sentences has been heard, and so has the presenter's own word
    // (`MenuClosed`, `MenuReturned`): both were queued ahead of this second hop.
    forget_withdrawn(fence.menu);
}

const std::vector<WithdrawnMenu>& WorkshopWeave::withdrawn_menus() const { return withdrawn_; }

// WL-CTX-09 -- agents/workshop/pane-menu.md
void WorkshopWeave::end_menu_unanswered(const std::string& why, loom::Mail& mail) {
    if (!session_.presented.open) {
        return;
    }
    const PresentedMenu ended = session_.presented;
    session_.presented = PresentedMenu{};
    // NO PRESENTER CAN ANSWER THIS ONE, SO THIS HOST DOES -- unchosen, as its office, under the
    // request's number: the requester settles its ask and nothing more happens. A choice never
    // comes from here. The maker is told the menu closed and why, on the host's own line.
    (void)mail.as_role(kWorkshopProvider)
        .send_to_role(ended.office,
                      PaneMenuAnswered{ended.pane, ended.subject, false, std::string(), why},
                      ended.correlation);
    say("the menu closed -- " + why, true);
    repaint(mail);
}

// WL-CTX-09 -- agents/workshop/pane-menu.md
bool WorkshopWeave::about_open_menu(std::int64_t menu, const loom::Mail& mail) const {
    return mail.authored_from_role(kPresenterRole) && session_.presented.open && menu > 0 &&
           menu == session_.presented.menu;
}

// WL-CTX-09 -- agents/workshop/pane-menu.md
void WorkshopWeave::forward_menu_input(std::int64_t kind, std::int64_t verb,
                                       std::int64_t scancode, std::int64_t modifiers,
                                       std::int64_t button, std::int64_t line, loom::Mail& mail) {
    PresentedMenu& menu = session_.presented;
    if (!menu.open) {
        return;
    }
    // NUMBERED AS THE ACT IT IS. A release carries its press's number because it is not counted
    // (`on(PointerButton)`), so a presenter that chooses on the release names the click's act.
    menu.last_input = gestures_;
    MenuInput in;
    in.menu = menu.menu;
    in.input = static_cast<std::int64_t>(gestures_);
    in.kind = kind;
    in.verb = verb;
    in.scancode = scancode;
    in.modifiers = modifiers;
    in.button = button;
    in.line = line;
    in.picture = menu.stamp.aimed;
    const loom::Ticket sent = mail.as_role(kWorkshopProvider).send_to_role(kPresenterRole, in);
    if (!sent.valid()) {
        // NOTHING QUEUED: no presenter can hear the maker's act, so none can answer the menu.
        end_menu_unanswered("the presenter could not be reached", mail);
    }
}

// WL-CTX-09 -- agents/workshop/pane-menu.md
void WorkshopWeave::menu_key(const zengine::input::KeyPressed& k, loom::Mail& mail) {
    // THE MAKER'S OWN CONTEXTUAL ROWS NAME THE KEY -- wherever they moved `context.up`, the
    // presenter hears "up" -- and the key that opens a menu closes it, the shared rule. What each
    // verb MEANS on this menu is the presenter's; a key no row names still crosses, with its
    // scancode, for a presenter that reads keys of its own.
    std::int64_t verb = menu_verb::kNone;
    switch (session_.keymap.action_for(KeyContext::kContext, k.scancode, k.modifiers)) {
    case Act::kContextUp: verb = menu_verb::kUp; break;
    case Act::kContextDown: verb = menu_verb::kDown; break;
    case Act::kContextChoose: verb = menu_verb::kChoose; break;
    case Act::kContextBack: verb = menu_verb::kBack; break;
    default:
        if (session_.keymap.matches(Act::kContextOpen, k.scancode, k.modifiers)) {
            verb = menu_verb::kBack;
        }
        break;
    }
    forward_menu_input(menu_input::kKey, verb, k.scancode, k.modifiers, 0, -1, mail);
}

// WL-CTX-09 -- agents/workshop/pane-menu.md
bool WorkshopWeave::menu_button(const zengine::input::PointerButton& b, loom::Mail& mail) {
    // A SECONDARY RELEASE ENDS A HOLD BEGUN BEFORE THE MENU OPENED (WL-PRESS-06): custody first.
    if (!b.pressed && (b.button == 2 || b.button == 3)) {
        (void)external_release(b.button, b, mail);
        return true;
    }
    // A RIGHT PRESS RE-ASKS THE QUESTION ABOUT WHATEVER IS UNDER IT NOW: the menu is withdrawn
    // and the press routed as it would have been without it -- a pane with the door is offered it
    // first, else the host's own menu opens. Opening is re-targeting, not a toggle.
    if (b.pressed && b.button == 3) {
        withdraw_menu("a newer press", mail);
        return false;
    }
    const PointedAt where = canvas_point_of(b.space, b.x, b.y);
    if (!where.understood) {
        return true;
    }
    const PresentedPressAt hit =
        presented_press_at(session_, screen_of(session_), b.space, b.x, b.y, where);
    if (!b.pressed) {
        // A PRIMARY RELEASE: a drag that began before the menu opened ends here (the Terminal's
        // and management's own repair), and the presenter hears where the hand came up.
        (void)end_held_gestures();
        forward_menu_input(menu_input::kRelease, menu_verb::kNone, 0, 0, b.button,
                           hit.inside ? hit.line : -1, mail);
        return true;
    }
    // A PRESS INSIDE IS THE PRESENTER'S TO READ; ONE OUTSIDE IS SPENT ON THE MENU -- nothing
    // beneath it is selected, focused or sent it -- and the presenter says what it means.
    if (hit.inside) {
        forward_menu_input(menu_input::kPress, menu_verb::kNone, 0, 0, b.button, hit.line, mail);
    } else {
        forward_menu_input(menu_input::kOutside, menu_verb::kNone, 0, 0, b.button, -1, mail);
    }
    return true;
}

// WL-CTX-09 -- agents/workshop/pane-menu.md
void WorkshopWeave::on(const MenuShown& shown, loom::Mail& mail) {
    if (!about_open_menu(shown.menu, mail)) {
        return; // not the presenter, or not the menu that is open: nothing moves
    }
    PresentedMenu& menu = session_.presented;
    // THE LINES ARE JUDGED AS A PANE'S ROWS ARE: within the room granted, drawable, bounded. A
    // presenter that cannot be drawn loses the menu, in words, rather than painting past its room.
    std::string refused;
    if (shown.lines.size() > kMaxMenuLines ||
        static_cast<std::int64_t>(shown.lines.size()) > menu.room_rows) {
        refused = "the presenter showed " + std::to_string(shown.lines.size()) +
                  " lines in a room of " + std::to_string(menu.room_rows);
    }
    for (const surface::SurfaceTextRow& line : shown.lines) {
        if (!refused.empty()) {
            break;
        }
        if (line.text.size() > kMaxMenuLineLen ||
            static_cast<std::int64_t>(line.text.size()) > menu.room_columns) {
            refused = "the presenter showed a line wider than its room";
            break;
        }
        for (const char c : line.text) {
            const unsigned char byte = static_cast<unsigned char>(c);
            if (byte < 0x20u || byte >= 0x7Fu) {
                refused = "the presenter showed a line carrying a byte a canvas cannot draw";
                break;
            }
        }
    }
    if (!refused.empty()) {
        withdraw_menu(refused, mail);
        repaint(mail);
        return;
    }
    menu.lines = shown.lines;
    menu.picture = shown.picture;
    repaint(mail);
}

// WL-CTX-09 -- agents/workshop/pane-menu.md
void WorkshopWeave::on(const MenuClosed& closed, loom::Mail& mail) {
    if (!about_open_menu(closed.menu, mail)) {
        // A menu this host already took off the screen, ended by the image that held it: that word
        // comes no later than its answer to the requester, so the record goes and a later give-back
        // cannot answer twice.
        if (mail.authored_from_role(kPresenterRole) && closed.menu > 0) {
            forget_withdrawn(closed.menu);
        }
        return;
    }
    const PresentedMenu ended = session_.presented;
    session_.presented = PresentedMenu{};
    // A choice is recorded as the continuation of the act that made it, one this host forwarded to
    // that menu; a request continuing it is honored while that act is the maker's latest (the
    // choosing click's release is no new act).
    const std::uint64_t act = closed.input > 0 ? static_cast<std::uint64_t>(closed.input) : 0;
    if (closed.chosen && act >= ended.first_input && act <= ended.last_input) {
        if (const RuntimePane* row = session_.panels.runtime.find(ended.office, ended.pane)) {
            ChoiceAnswered c;
            c.kind = row->kind;
            c.gesture = act;
            c.correlation = ended.correlation;
            c.cell.understood = ended.anchored;
            c.cell.cell.x = ended.anchor_x;
            c.cell.cell.y = ended.anchor_y;
            c.spent = false;
            choice_answered_ = c;
        }
    }
    repaint(mail);
}

// WL-CTX-10 -- agents/workshop/pane-menu.md
void WorkshopWeave::on(const MenuReturned& returned, loom::Mail& mail) {
    if (!mail.authored_from_role(kPresenterRole) || returned.menu <= 0 ||
        returned.menu > menus_) {
        return; // the office that presents, about a menu this host granted, and nobody else
    }
    // AN INTERACTION HANDED BACK UNANSWERED. The office that presents says it cannot answer this
    // requester, and no other party can: so this host does -- for the menu still on the screen...
    if (session_.presented.open && session_.presented.menu == returned.menu) {
        end_menu_unanswered("the presenter could not answer it -- " + returned.why, mail);
        return;
    }
    // ...or for one already withdrawn while who asked is still kept: answered once, and forgotten.
    for (std::size_t i = 0; i < withdrawn_.size(); ++i) {
        if (withdrawn_[i].menu != returned.menu) {
            continue;
        }
        const WithdrawnMenu given = withdrawn_[i];
        withdrawn_.erase(withdrawn_.begin() + static_cast<std::ptrdiff_t>(i));
        answer_withdrawn(given,
                         given.why + " -- the presenter could not answer it (" + returned.why + ")",
                         mail);
        return;
    }
    // A MENU THAT IS OVER AND SETTLED: its requester was answered where it ended, so this takes
    // nothing -- least of all from a newer menu, which is another grant with another number.
}

// WL-CTX-09 -- agents/workshop/pane-menu.md
void WorkshopWeave::on(const PresenterReady& ready, loom::Mail& mail) {
    if (!mail.authored_from_role(kPresenterRole) || !session_.presented.open) {
        return;
    }
    if (ready.menu == session_.presented.menu) {
        // A HANDOFF: the holder that arrived carries the open menu (a reload that kept the
        // presenter's state). Its pictures start over -- a new image numbers afresh -- and its
        // next `MenuShown` draws the menu in its own way; the maker's interaction continues.
        session_.presented.stamp.forget();
        session_.presented.picture = 0;
        return;
    }
    // A HOLDER THAT DOES NOT CARRY IT cannot answer it: the menu ends here, answered by this host.
    end_menu_unanswered("the presenter was replaced", mail);
}

// WL-CTX-09 -- agents/workshop/pane-menu.md
void WorkshopWeave::on(const PaneManageRequested& asked, loom::Mail& mail) {
    const std::string_view office = mail.authored_role();
    if (office.empty()) {
        return;
    }
    const RuntimePane* row = session_.panels.runtime.find(office, asked.pane);
    if (row == nullptr || mail.correlation() == 0) {
        return;
    }
    // A CONTINUATION OF THE CHOICE A PRESENTER LAST REPORTED FOR THAT PANE, and only while that
    // choice is still the maker's latest act; once, and never for a pane the inventory does not
    // name.
    if (choice_answered_.spent || choice_answered_.kind != row->kind ||
        choice_answered_.correlation != mail.correlation() ||
        choice_answered_.gesture != gestures_) {
        return;
    }
    choice_answered_.spent = true;
    const PaneRef subject{asked.office, asked.target};
    bool named = false;
    for (const CatalogRow& r : inventory_rows(session_.setup.active, session_.panels)) {
        if (r.ref == subject) {
            named = true;
            break;
        }
    }
    if (!named) {
        say("no pane `" + ref_text(subject) + "` in this Workshop -- nothing to manage", true);
        repaint(mail);
        return;
    }
    withdraw_menu("the host's own menu opened", mail);
    ContextMenu next;
    next.open = true;
    next.anchored = choice_answered_.cell.understood;
    next.anchor_x = choice_answered_.cell.cell.x;
    next.anchor_y = choice_answered_.cell.cell.y;
    next.subject = context_subject::kPane;
    next.pane = subject;
    session_.context = next;
    repaint(mail);
}

// WL-CTX-09 -- agents/workshop/pane-menu.md
void WorkshopWeave::on(const PaneKeyboardRequested& asked, loom::Mail& mail) {
    const std::string_view office = mail.authored_role();
    if (office.empty()) {
        return;
    }
    const RuntimePane* row = session_.panels.runtime.find(office, asked.pane);
    if (row == nullptr || mail.correlation() == 0) {
        return;
    }
    // A continuation of the choice a presenter last reported for that pane, judged as a manage
    // request is (`choice_answered_`). The menu left the keys where they were; a pane whose chosen
    // row begins an edit asks for them here.
    if (choice_answered_.spent || choice_answered_.kind != row->kind ||
        choice_answered_.correlation != mail.correlation() ||
        choice_answered_.gesture != gestures_) {
        return;
    }
    choice_answered_.spent = true;
    if (!session_.panels.has(row->kind) || !kind_takes_keyboard(row->kind)) {
        return; // a pane not on the desk, or one that takes no keys, gets none
    }
    // THE GUARDED TRANSITION: the pane becomes the selected, keyboard-holding pane, so the next
    // key -- the one being captured, or the spelling being typed -- reaches it. Its own rows say
    // what it is now showing; nothing here types anything.
    session_.panels.selected = row->kind;
    session_.panels.keyboard = row->kind;
    repaint(mail);
}

// WL-ARR-15 -- agents/workshop/arrangement.md
void WorkshopWeave::on(const PaneEscapeUnspent& said, loom::Mail& mail) {
    const std::string_view office = mail.authored_role();
    if (office.empty()) {
        return;
    }
    const RuntimePane* row = session_.panels.runtime.find(office, said.pane);
    if (row == nullptr) {
        return; // a pane this office never offered is no pane of the desk's
    }
    const std::int64_t kind = row->kind;
    // The particular Escape this answers, first: current state cannot identify it, since a second
    // Escape recreates that state. The number was minted for one keystroke, and zero (an answer
    // echoing nothing) is never an Escape.
    if (mail.correlation() == 0 || mail.correlation() != escape_sent_.answering) {
        return;
    }
    // STILL THE MAKER'S LATEST GESTURE, INTO THIS PANE, WHICH STILL HAS THE DESK AND THE KEYS.
    // A key, text, press or wheel since leaves this about an Escape that is no longer what the
    // maker did last, and putting a pane down under a later gesture would act on a stale word.
    if (escape_sent_.kind != kind || escape_sent_.gesture != gestures_ ||
        session_.panels.selected != kind || typing_pane(session_) != kind) {
        return;
    }
    escape_sent_ = EscapeSent{};
    // What an unspent Escape means is the desktop's (WL-DESK-02); the judgement above stays the
    // host's, as facts about the room. The chain has two links, both checked: the pane echoed this
    // host's number, and `request_app_action` mints a second for the desktop. A desktop declaring
    // no default row leaves the gesture meaning nothing; nothing compiled in answers behind it.
    if (const AppRow* app_row = session_.keymap.app_action_for(
            app_precedence::kDefault, KeyContext::kPane, input::scan::kEscape,
            input::mod::kNone, kind)) {
        request_app_action(app_row->id, mail);
    }
    repaint(mail);
}

// WL-ARR-13, WL-ARR-14 -- agents/workshop/arrangement.md
// WL-FOCUS-05 -- agents/workshop/focus.md
// WL-FRONT-04 -- agents/workshop/planes.md
void WorkshopWeave::unselect_pane() {
    const std::string name = kind_name(session_.panels, session_.panels.selected);
    session_.panels.selected = kNoPaneKind;
    session_.panels.keyboard = kNoPaneKind;
    say("unselected " + name, false);
}

void WorkshopWeave::external_text(std::int64_t kind, const zengine::input::TextEntered& t,
                                  loom::Mail& mail) {
    const RuntimePane* row = session_.panels.runtime.of_kind(kind);
    if (row == nullptr) {
        return;
    }
    (void)mail.as_role(kWorkshopProvider)
        .send_to_role(row->provider, PaneTextInput{row->pane, t.text});
    note_routed(kind);
}

} // namespace zengine::workshop
