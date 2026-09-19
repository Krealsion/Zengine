// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The bodies of `weave.hpp`'s section -- the external pane's room and gestures: the room granted
// once per repaint and only when it changed, a press, a key, the wheel and text forwarded to the
// provider, and the selected pane put down -- compiled once into `zengine-workshop-logic` and
// linked by the host and every suite; the declarations, the constants and the constexpr functions
// stay in the header.
// Workshop law: agents/workshop/focus.md (+4 registers; agents/workshop.md routes)

#include "weave.hpp"

namespace zengine::workshop {

// ---- THE EXTERNAL PANE'S ROOM AND GESTURES: the grant, a press, a key, the wheel, text ----

// WL-PANE-06 -- agents/workshop/panes-and-windows.md
void WorkshopWeave::refresh_external_rooms(loom::Mail& mail) {
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
        // ⚠ AND THE REFUSAL IS NOT CLEARED HERE, THOUGH IT USED TO BE. A room goes out
        // whenever the surface resizes or a maker drags this pane's edge, so clearing it here
        // let a maker un-say the sentence explaining an empty pane by widening their window --
        // with nothing valid having arrived and the pane still showing nothing. What replaced
        // it was `waiting`, which is true and says less. The rows, `heard` and `awaiting` DO
        // turn over above, because those are facts about the ROOM; a refusal is a fact about
        // the CONTENT, and only content this host accepted may take it back (WL-ATTN-04).
        // DELIBERATELY AUTHORED AS `zengine.workshop` AND ADDRESSED TO THE OFFICE THE
        // DESCRIPTOR CAME IN UNDER. The authorship is what lets the provider verify the
        // ask (its side refuses a room from anyone else); the destination is a ROLE
        // rather than a WeaveId, so a provider that was replaced still gets its room.
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
    // A ROOM THIS PANE HAS NOT BEEN GRANTED HAS NO LATTICE TO NAME A PLACE IN. `granted`
    // is false for exactly one beat -- between a panel opening and the repaint that
    // grants it -- and a press in that beat would be a position in a room the provider
    // has never been told about.
    if (row == nullptr || pane == nullptr || !pane->granted) {
        return false;
    }
    // ONE PRESS, ONE SENTENCE, IN THE VERSION THE OFFICE'S HOLDER ACCEPTS NOW. The host reads
    // that off the bus at this instant; the role is resolved again at delivery, so a holder
    // replaced in between may refuse the version chosen here -- Loom records that refusal
    // against this send, and nothing here sends the press again in the other version, which
    // would be a second gesture delivered after whatever the maker did next.
    if (host_->holder_accepts &&
        host_->holder_accepts(row->provider, *loom::schema_of<v3::PanePressed>())) {
        // ...THE THIRD VERSION NAMES THE PICTURE THE PRESS WAS AIMED AT: the newest one the
        // medium had been handed when the press was read (`PictureFence`), not merely the newest
        // admitted -- a picture queued ahead of this press but not yet shown is not one the hand
        // could have aimed at. Echoed here, judged by the pane.
        (void)mail.as_role(kWorkshopProvider)
            .send_to_role(row->provider, v3::PanePressed{row->pane, at.row, at.column,
                                                         keys_went_here, pane->stamp.aimed});
    } else if (host_->holder_accepts &&
               host_->holder_accepts(row->provider, *loom::schema_of<v2::PanePressed>())) {
        (void)mail.as_role(kWorkshopProvider)
            .send_to_role(row->provider,
                          v2::PanePressed{row->pane, at.row, at.column, keys_went_here});
    } else {
        (void)mail.as_role(kWorkshopProvider)
            .send_to_role(row->provider, PanePressed{row->pane, at.row, at.column});
    }
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
    // THIS ESCAPE'S OWN NUMBER, and only an Escape gets one: it is the identity an answer has
    // to echo, carried in Loom's envelope the way a relay carries a forwarded ask's (the
    // settlement pair -- WHICH conversation, and WHO is speaking). Nothing else about the
    // keystroke changes, and no published shape gains a field.
    // THE PANE'S OWN ROWS FIRST, against the EFFECTIVE map -- the maker's override where
    // one is authored, the pane's default otherwise. A match crosses as the id and NOT as
    // the key: one keystroke, one sentence, and the pane acts on a name.
    //
    // ...AND EVERY DECLARED ACTION GOES OUT UNDER A NUMBER OF ITS OWN, not only an Escape: a
    // menu a pane asks for by key echoes it, so the host can tell a request about THIS
    // keystroke from one about an earlier one (`action_sent_`). A pane that never echoes it
    // is unchanged.
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
    // AN ESCAPE NOTHING ON THE FAR SIDE COULD SPEND: the office's holder accepts no key and the
    // pane declared no row for it, so a send would only be refused at Loom's gate. That is the
    // holder's own declaration read off the bus -- not a guess from silence -- and the key is
    // this host's to answer. A host that cannot say sends it as before.
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
        // THE CONTINUATION IS INVALIDATED ON ITS OWN TERMS: a pane that left the desk after the
        // release cannot have its press handed back or a menu opened for it, whatever its
        // button is doing. (The review's first integration finding.)
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
    if (!refused_attempt.valid()) {
        return false;
    }
    for (std::size_t s = 0; s < 2; ++s) {
        SecondaryHold& h = secondary_hold_[s];
        if (!h.active || !h.attempt.valid() || h.attempt.seq != refused_attempt.seq) {
            continue; // not this slot's attempt (or an old one): it cancels nothing newer
        }
        // THE PRESS NEVER REACHED ITS RECIPIENT: there is no custody to keep and none to hand
        // back. The hold and its continuation are dropped, so the physical release below sends
        // nothing, and a pass-back or menu that tried to continue this press finds no record.
        // Nothing is said to the pane -- it was never told of a press, so it holds nothing -- and
        // no menu opens: a refused body press is not a request for the host's chrome. Loom's tap
        // already attributes the refusal; that is the settlement's whole visible half.
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

// WL-CTX-09 -- agents/workshop/contextual.md
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

// WL-CTX-09 -- agents/workshop/contextual.md
void WorkshopWeave::on(const PaneMenuRequested& asked, loom::Mail& mail) {
    const std::string_view office = mail.authored_role();
    if (office.empty()) {
        return; // an office asks; personal speech is answered by nobody
    }
    const RuntimePane* row = session_.panels.runtime.find(office, asked.pane);
    if (row == nullptr) {
        return; // a pane this office never offered
    }
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
    // ELIGIBILITY, JUDGED HERE WHERE THE MENU OPENS. The request continues a secondary press
    // -- eligible on a pass-back's exact terms -- or a declared action sent by key, eligible
    // while that keystroke is the maker's latest act. Anything newer (a click elsewhere, a key)
    // makes it late, and a late menu is refused rather than opened over what the maker did next.
    // Nothing here moves the keys or the selection.
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

// WL-CTX-09 -- agents/workshop/contextual.md
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
    }
}

// WL-CTX-09 -- agents/workshop/contextual.md
void WorkshopWeave::withdraw_menu(const std::string& why, loom::Mail& mail) {
    if (!session_.presented.open) {
        return;
    }
    const std::int64_t menu = session_.presented.menu;
    session_.presented = PresentedMenu{};
    // THE PRESENTER ANSWERS ITS REQUESTER; this host only says the menu is over and why. If the
    // presenter has left, Loom refuses this at delivery -- and there is no menu left to end.
    (void)mail.as_role(kWorkshopProvider).send_to_role(kPresenterRole, MenuWithdrawn{menu, why});
}

// WL-CTX-09 -- agents/workshop/contextual.md
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

// WL-CTX-09 -- agents/workshop/contextual.md
bool WorkshopWeave::about_open_menu(std::int64_t menu, const loom::Mail& mail) const {
    return mail.authored_from_role(kPresenterRole) && session_.presented.open && menu > 0 &&
           menu == session_.presented.menu;
}

// WL-CTX-09 -- agents/workshop/contextual.md
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

// WL-CTX-09 -- agents/workshop/contextual.md
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

// WL-CTX-09 -- agents/workshop/contextual.md
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

// WL-CTX-09 -- agents/workshop/contextual.md
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

// WL-CTX-09 -- agents/workshop/contextual.md
void WorkshopWeave::on(const MenuClosed& closed, loom::Mail& mail) {
    if (!about_open_menu(closed.menu, mail)) {
        return;
    }
    const PresentedMenu ended = session_.presented;
    session_.presented = PresentedMenu{};
    // A CHOICE IS RECORDED AS THE CONTINUATION OF THE ACT THAT MADE IT -- the act the presenter
    // names, and only one this host forwarded to that menu (or the ask that opened it). A
    // requester's keyboard or management request continuing the choice is then honored exactly
    // while that act is still the maker's latest: a newer key or press defeats it, the release of
    // the choosing click does not (it is not a new act).
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

// WL-CTX-09 -- agents/workshop/contextual.md
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

// WL-CTX-09 -- agents/workshop/contextual.md
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

// WL-CTX-09 -- agents/workshop/contextual.md
void WorkshopWeave::on(const PaneKeyboardRequested& asked, loom::Mail& mail) {
    const std::string_view office = mail.authored_role();
    if (office.empty()) {
        return;
    }
    const RuntimePane* row = session_.panels.runtime.find(office, asked.pane);
    if (row == nullptr || mail.correlation() == 0) {
        return;
    }
    // A CONTINUATION OF THE CHOICE A PRESENTER LAST REPORTED FOR THAT PANE, judged exactly as a
    // manage request is (`choice_answered_`): once, and only while that choice is still the
    // maker's latest act, so a newer press or key defeats a late grab. The menu deliberately left
    // the keys where they were; a pane whose chosen row begins an edit asks for them here, and the
    // transition is guarded -- not the unconditional delayed reveal the research once used.
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
    // THE PARTICULAR ESCAPE THIS ANSWERS, FIRST. Matching current state cannot identify the
    // event being answered: a second Escape leaves this pane selected, typed into and the
    // maker's latest gesture all over again, and the first Escape's answer would be spent on
    // the second's record. The number is the one this host minted for that one keystroke, so
    // an answer to an Escape that is over matches nothing -- and zero, which is what an
    // answer echoing nothing carries, is never an Escape.
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
    // ⭐ AND WHAT AN UNSPENT ESCAPE MEANS IS THE DESKTOP'S (WL-DESK-02). This host still owns
    // the JUDGEMENT above -- which Escape this answers, whether it is still the maker's latest
    // gesture, whether this pane still has the desk and the keys -- because all four are facts
    // about the room, and the room is the host's. What it no longer owns is the CONSEQUENCE.
    //
    // ⚠ SO THE CORRELATION CHAIN HAS TWO LINKS NOW, AND BOTH ARE CHECKED. The pane echoed the
    // number this host minted for the keystroke it was sent; `request_app_action` mints a
    // second for the desktop, and the desktop's answer is judged against THAT. A reply that
    // crosses a later gesture at either link acts on nothing.
    //
    // ⚠ AND A DESKTOP THAT DECLARES NO DEFAULT ROW FOR THIS GESTURE LEAVES IT MEANING NOTHING,
    // which is what disabling a default has to mean. `app_action_for` says so by returning
    // nullptr, and nothing here supplies a compiled-in answer behind it.
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
