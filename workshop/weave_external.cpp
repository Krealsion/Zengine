// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// `WorkshopWeave`'s external panes: the forwarded key and text, the second button's
// continuations, a pane's own menu, and the `?` it wears.
// Workshop law: agents/workshop/focus.md (+5 registers; agents/workshop.md routes)

#include "pane_document.hpp"
#include "screen_canvas.hpp"
#include "weave.hpp"

namespace zengine::workshop {

// ---- THE EXTERNAL PANE'S CHROME AND KEYS --------------------------------------------------

// WL-CHROME-09 -- agents/workshop/chrome.md; WL-PANE-16 -- agents/workshop/panes-and-windows.md
void WorkshopWeave::note_manuals() {
    // THE APPLICATION SHOWS A MANUAL while it declares the row that does; a pane's holder answers
    // one while it takes the door. Both asked now, of the live keymap and the live holder -- and
    // whether that holder draws in a window, which the frame's body says.
    bool shown = false;
    for (const AppRow& app : session_.keymap.app) {
        shown = shown || app.id == kManualAction;
    }
    for (const OpenPane& p : session_.panes.open) {
        if (!is_runtime_kind(p.kind)) {
            continue;
        }
        ExternalPane* pane = session_.panes.external_pane(p.kind);
        const RuntimePane* row = session_.panes.runtime.of_kind(p.kind);
        if (pane == nullptr) {
            continue;
        }
        pane->manual = shown && row != nullptr && host_->holder_accepts &&
                       host_->holder_accepts(row->provider,
                                             *loom::schema_of<PaneDocumentRequested>());
        pane->held = row != nullptr && host_->role_holder &&
                     host_->role_holder(row->provider).valid();
        pane->in_window = row != nullptr && host_->holder_accepts &&
                          host_->holder_accepts(row->provider,
                                                *loom::schema_of<graphics::AreaRoom>());
    }
}

// WL-CHROME-09 -- agents/workshop/chrome.md
void WorkshopWeave::press_manual(std::int64_t kind, loom::Mail& mail) {
    const RuntimePane* row = session_.panes.runtime.of_kind(kind);
    if (row == nullptr) {
        return;
    }
    request_app_action(kManualAction, mail, hand(), row);
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

// WL-ARR-16 -- agents/workshop/arrangement.md
bool holder_emits_on(const loom::Switchboard& bus, std::string_view role,
                     const loom::Schema& shape) {
    const loom::WeaveId holder = bus.role_holder(role);
    if (!holder.valid()) {
        return false;
    }
    for (const std::shared_ptr<const loom::Schema>& said : bus.emitted_schemas(holder)) {
        if (said != nullptr && loom::same_identity(*said, shape)) {
            return true;
        }
    }
    return false;
}

// WL-FOCUS-01, WL-FOCUS-05 -- agents/workshop/focus.md
std::int64_t WorkshopWeave::keyboard_pane() const {
    return zengine::workshop::keyboard_pane(session_.panes);
}

// WL-KEY-15 -- agents/workshop/keyboard.md; WL-ARR-16 -- agents/workshop/arrangement.md
// WL-GUEST-06 -- agents/workshop/guests.md
bool WorkshopWeave::external_key(std::int64_t kind, const zengine::input::KeyPressed& k,
                                 loom::Mail& mail) {
    const RuntimePane* row = session_.panes.runtime.of_kind(kind);
    if (row == nullptr) {
        return false;
    }
    // A GUEST'S KEY TOWARD A PLACE ONLY THE WEAVER'S HAND REACHES is refused here, and consumed:
    // no row of the pane's, no raw key, and no Escape's default meaning behind it.
    if (const std::string why = refused_toward(input_actor_, kind); !why.empty()) {
        say(why, true);
        return true;
    }
    const bool escape = k.scancode == input::scan::kEscape && k.modifiers == input::mod::kNone;
    // Only an Escape gets its own number, the identity an answer must echo; no published shape
    // gains a field. The pane's own rows come first, against the effective map, and a match
    // crosses as the id, not the key. Every declared action goes out under its own number, so a
    // menu asked for by key is told apart from one about an earlier keystroke (`Hand::action_sent`).
    if (const PaneRow* action =
            session_.keymap.pane_action_for(kind, k.scancode, k.modifiers)) {
        const std::uint64_t answering = ++gesture_asks_;
        (void)mail.as_role(kWorkshopProvider)
            .send_to_role(row->provider, PaneActionRequested{row->pane, action->id}, answering);
        note_routed(kind);
        keep_act(hand(), hand().action_sent = GestureSent{kind, hand().latest, answering});
        if (escape) {
            hand().escape_sent = GestureSent{kind, hand().latest, answering};
        }
        return true;
    }
    const std::uint64_t answering = escape ? ++gesture_asks_ : 0;
    // An Escape nothing on the far side could spend: the holder accepts no key and the pane
    // declared no row for it, so the key is this host's. A host that cannot ask sends it.
    if (escape && host_->holder_accepts &&
        !host_->holder_accepts(row->provider, *loom::schema_of<PaneKey>())) {
        return false;
    }
    // ...and one it could never hand back: a holder that does not declare `PaneEscapeUnspent`
    // among what it emits has no way to say the Escape meant nothing there, so Escape keeps its
    // default meaning and the key is this host's too (WL-ARR-16).
    if (escape && host_->holder_emits &&
        !host_->holder_emits(row->provider, *loom::schema_of<PaneEscapeUnspent>())) {
        return false;
    }
    (void)mail.as_role(kWorkshopProvider)
        .send_to_role(row->provider, PaneKey{row->pane, k.scancode, k.modifiers}, answering);
    note_routed(kind);
    if (escape) {
        hand().escape_sent = GestureSent{kind, hand().latest, answering};
    }
    return true;
}

// ---- The second button: continuation, and a pane's own menu ----------------------------------

// WL-PRESS-06 -- agents/workshop/press-chain.md
void WorkshopWeave::end_lost_holds(loom::Mail& mail) {
    // The continuation is invalidated on its own terms: a pane that left the desk after the
    // release cannot have its press handed back or a menu opened for it.
    each_hand([&](Hand& hd) {
        for (SecondaryContinuation& c : hd.secondary_cont) {
            if (c.live && !session_.panes.has(c.kind)) c = SecondaryContinuation{};
        }
    });
    // ...AND A MENU PRESENTED FOR A PANE THAT LEFT is withdrawn: its subject has no room on the
    // desk any more, and a choice about it would reach a pane the weaver cannot see. The presenter
    // answers the requester unchosen, in these words.
    if (session_.presented.open) {
        const RuntimePane* row =
            session_.panes.runtime.find(session_.presented.office, session_.presented.pane);
        if (row == nullptr || !session_.panes.has(row->kind)) {
            withdraw_menu("the pane left the desk", mail);
        }
    }
    each_hand([&](Hand& hd) {
        if (hd.choice_answered.kind != kNoPaneKind && !session_.panes.has(hd.choice_answered.kind))
            hd.choice_answered = ChoiceAnswered{};
    });
}

// WL-CTX-08 -- agents/workshop/contextual.md
void WorkshopWeave::on(const PanePassRequested& said, loom::Mail& mail) {
    const std::string_view office = mail.authored_role();
    if (office.empty()) {
        return;
    }
    const RuntimePane* row = session_.panes.runtime.find(office, said.pane);
    if (row == nullptr) {
        return; // a pane this office never offered is no pane of the desk's
    }
    if (mail.correlation() == 0) {
        return; // an answer echoing nothing answers nothing
    }
    SecondaryContinuation* c = nullptr;
    Hand* by = nullptr;
    each_hand([&](Hand& h) {
        for (SecondaryContinuation& each : h.secondary_cont) {
            if (each.live && each.correlation == mail.correlation()) {
                c = &each;
                by = &h;
            }
        }
    });
    if (c == nullptr || c->kind != row->kind || c->spent) {
        return; // stale, another pane's, or already handed back
    }
    if (c->interrupted || by->latest != c->gesture_at_press) {
        return; // that hand did something since: the press is not its latest act
    }
    c->spent = true;
    // THE HOST'S CONFIGURED FALLBACK FOR A BODY PRESS: its own pane menu, at the press's cell,
    // on the subject the record names rather than whatever is under the hand now. A pane's menu
    // still presented is withdrawn first -- one surface, and the newer act wins.
    withdraw_menu("a newer press", mail);
    ContextMenu next;
    next.open = true;
    next.anchored = true;
    next.anchor_x = c->cell.px.x;
    next.anchor_y = c->cell.px.y;
    next.subject = context_subject::kPane;
    next.pane = PaneRef{row->provider, row->pane};
    session_.context = next;
    repaint(mail);
}

// WL-CTX-09 -- agents/workshop/pane-menu.md
PointedAt WorkshopWeave::cell_of_body_place(std::int64_t kind, std::int64_t row,
                                            std::int64_t column) const {
    PointedAt out;
    if (!session_.panes.has(kind)) {
        return out;
    }
    const Screen sc = screen_of(session_);
    const PaneBounds where = bounds_of(session_.panes, session_.setup.active, kind, sc);
    if (!where.open) {
        return out;
    }
    // THE PANE'S AREA, where Workshop placed it, else where it would stand under its title rows.
    const ExternalPane* pane = session_.panes.external_pane(kind);
    const PixelRect area =
        pane != nullptr && pane->placed_w > 0 && pane->placed_h > 0
            ? PixelRect{pane->placed_x, pane->placed_y, pane->placed_w, pane->placed_h}
            : canvas_body_place(where.rect, sc,
                                external_title_rows(session_.panes, kind, session_.pane_titles));
    if (area.empty()) {
        return out;
    }
    // A row and a column of the window's text there; a place outside the area anchors at its
    // corner -- the pane named a row it no longer has, and the menu still opens beside it.
    const bool graphical = sc.text_advance_px > 0 && sc.text_line_px > 0;
    const std::int64_t advance = graphical ? sc.text_advance_px : graphics::kCellPx;
    const std::int64_t line = graphical ? sc.text_line_px : graphics::kCellPx;
    const bool inside = row >= 0 && column >= 0 && graphics::mul_px(row, line) < area.h &&
                        graphics::mul_px(column, advance) < area.w;
    out.understood = true;
    out.px.x = graphics::add_cells(area.x, inside ? graphics::mul_px(column, advance) : 0);
    out.px.y = graphics::add_cells(area.y, inside ? graphics::mul_px(row, line) : 0);
    out.cell.x = graphics::cell_of_pixel(out.px.x);
    out.cell.y = graphics::cell_of_pixel(out.px.y);
    out.grain = graphical ? graphics::kPixelGrainPx : graphics::kCellGrainPx;
    return out;
}

// WL-CTX-09 -- agents/workshop/pane-menu.md
void WorkshopWeave::on(const PaneMenuRequested& asked, loom::Mail& mail) {
    const std::string_view office = mail.authored_role();
    if (office.empty()) {
        return; // an office asks; personal speech is answered by nobody
    }
    const RuntimePane* row = session_.panes.runtime.find(office, asked.pane);
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
    if (!session_.panes.has(row->kind)) {
        refuse("the pane is not on the desk");
        return;
    }
    // Eligibility, judged where the menu opens: a request continues a secondary press on a
    // pass-back's terms, or a keyed action while that keystroke is the weaver's latest act. A late
    // menu is refused; nothing here moves the keys or the selection.
    PointedAt at;
    bool eligible = false;
    each_hand([&](Hand& h) {
        for (SecondaryContinuation& c : h.secondary_cont) {
            if (eligible || !c.live || c.correlation != mail.correlation()) {
                continue;
            }
            if (c.kind == row->kind && !c.spent && !c.interrupted && h.latest == c.gesture_at_press) {
                c.spent = true;
                at = c.cell;
                eligible = true;
            }
        }
    });
    each_hand([&](Hand& h) {
        if (!eligible && h.action_sent.answering == mail.correlation() &&
            h.action_sent.kind == row->kind && h.action_sent.gesture == h.latest) {
            h.action_sent = GestureSent{};
            at = cell_of_body_place(row->kind, asked.row, asked.column);
            eligible = true;
        }
        // ...OR A PRIMARY PRESS, on the same three terms: this pane, this number, and still its
        // hand's latest act. A pane that draws a `[menu]` control answers the click that hit it,
        // and the menu opens where the click was made.
        if (!eligible && h.press_sent.answering == mail.correlation() &&
            h.press_sent.kind == row->kind && h.press_sent.gesture == h.latest) {
            h.press_sent = GestureSent{};
            at = h.press_cell.understood ? h.press_cell
                                         : cell_of_body_place(row->kind, asked.row, asked.column);
            eligible = true;
        }
    });
    if (!eligible) {
        refuse("late -- the weaver acted since that gesture, or it was already spent");
        return;
    }
    // THE OPENING ACT IS SPENT ON THE MENU: its choice, made under the same number, is the
    // chooser's own record (`on(MenuClosed)`).
    drop_kept(mail.correlation());
    // ...AND SOMEBODY TO PRESENT IT. The office's holder is read off the bus now; the role is
    // resolved again at delivery, and a presenter that left in between is Loom's refusal to
    // settle (`on(DispatchRefused)`), answered then.
    if (!host_->holder_accepts ||
        !host_->holder_accepts(kPresenterRole, *loom::schema_of<MenuGranted>())) {
        const std::string why = std::string("no presenter holds `") + kPresenterRole +
                                "` -- nothing can present this menu";
        refuse(why);
        // ...AND THE WEAVER IS TOLD, because a missing participant is a fact about this room, and
        // a right-click that opened nothing with nothing said would read as a dead mouse.
        say(row->name + "'s menu did not open -- " + why, true);
        repaint(mail);
        return;
    }
    // ...IN A WINDOW: the presenter draws the menu in an area of its own of the window that shows
    // the desk, and with none there is nowhere to draw it.
    if (!window_.shown()) {
        const std::string why = "no window shows the desk -- nothing can present this menu";
        refuse(why);
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
    const ProsePlace room =
        presented_room(at.understood, at.px.x, at.px.y, screen_of(session_));
    PresentedMenu next;
    next.open = true;
    next.menu = ++menus_;
    next.office = row.provider;
    next.pane = row.pane;
    next.subject = asked.subject;
    next.correlation = correlation;
    next.anchored = at.understood;
    next.anchor_x = at.px.x;
    next.anchor_y = at.px.y;
    next.room_rows = room.present ? room.rows : 0;
    next.room_columns = room.present ? room.columns : 0;
    next.area = presented_menu_area(next.menu);
    next.first_input = gestures_;
    next.last_input = gestures_;
    // ...AND THE HOST'S OWN PANE MENU BENEATH THE PANE'S ROWS, in the same menu: the rows its
    // chrome offers, each the host's to spend when chosen (`spend_standard_row`), never the pane's.
    std::vector<PaneMenuRow> standard;
    for (const ContextEntry& entry : context_population(context_subject::kPane, "")) {
        if (!entry.is_group && entry.row == nullptr) {
            continue;
        }
        standard.push_back(
            PaneMenuRow{entry.is_group ? entry.group : entry.row->id, context_entry_text(entry)});
    }
    session_.presented = next;
    // TO THE ROLE, AS THIS OFFICE, UNDER THE REQUEST'S NUMBER: the presenter answers the requester
    // under the same number, and authenticates this grant the way a pane authenticates a room.
    const loom::Ticket sent =
        mail.as_role(kWorkshopProvider)
            .send_to_role(kPresenterRole,
                          MenuGranted{next.menu, next.office, next.pane, next.subject, asked.rows,
                                      next.room_rows, next.room_columns, std::move(standard),
                                      inventory_name(PaneRef{row.provider, row.pane}), next.area},
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
    // comes from here. The weaver is told the menu closed and why, on the host's own line.
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
    // NUMBERED AS THE ACT IT IS, its own hand's latest: another hand's act since renumbers it not.
    menu.last_input = gestures_;
    MenuInput in;
    in.menu = menu.menu;
    in.input = static_cast<std::int64_t>(hand().latest);
    in.kind = kind;
    in.verb = verb;
    in.scancode = scancode;
    in.modifiers = modifiers;
    in.button = button;
    in.line = line;
    in.picture = menu.picture;
    const loom::Ticket sent = mail.as_role(kWorkshopProvider).send_to_role(kPresenterRole, in);
    if (!sent.valid()) {
        // NOTHING QUEUED: no presenter can hear the weaver's act, so none can answer the menu.
        end_menu_unanswered("the presenter could not be reached", mail);
    }
}

// WL-CTX-09 -- agents/workshop/pane-menu.md
void WorkshopWeave::menu_key(const zengine::input::KeyPressed& k, loom::Mail& mail) {
    if (const RuntimePane* row = session_.panes.runtime.find(session_.presented.office, session_.presented.pane)) {
        if (const std::string why = refused_toward(input_actor_, row->kind); !why.empty()) {
            say(why, true);
            return;
        }
    }
    // THE WEAVER'S OWN CONTEXTUAL ROWS NAME THE KEY -- wherever they moved `context.up`, the
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
    // A MENU ABOUT A PLACE ONLY THE WEAVER'S HAND REACHES takes no press of a guest's; its
    // releases still end the custody they end, and reach no presenter.
    const RuntimePane* about = session_.panes.runtime.find(session_.presented.office, session_.presented.pane);
    const std::string refused = about != nullptr ? refused_toward(input_actor_, about->kind) : std::string();
    if (!b.pressed && (b.button == 2 || b.button == 3)) {
        return true; // a secondary release on Workshop's own area ends nothing of a pane's
    }
    if (b.pressed && !refused.empty()) {
        say(refused, true);
        return true;
    }
    // A RIGHT PRESS RE-ASKS THE QUESTION ABOUT WHATEVER IS UNDER IT NOW: the menu is withdrawn
    // and the press routed as it would have been without it -- a pane with the door is offered it
    // first, else the host's own menu opens. Opening is re-targeting, not a toggle.
    if (b.pressed && b.button == 3) {
        withdraw_menu("a newer press", mail);
        return false;
    }
    if (!b.pressed) {
        // A PRIMARY RELEASE: a drag that began before the menu opened ends here (the Terminal's
        // and management's own repair). A release on the menu is its presenter's, in its area.
        (void)end_held_gestures(mail);
        return true;
    }
    const PointedAt where = canvas_point_of(b.space, b.x, b.y);
    if (!where.understood) {
        return true;
    }
    // A PRESS ON THE MENU IS ITS PRESENTER'S, heard in its own area; one this host hears on the
    // menu's frame is spent there. ONE OUTSIDE IS SPENT ON THE MENU -- nothing beneath it is
    // selected, focused or sent it -- and the presenter says what it means.
    if (!presented_holds(session_, screen_of(session_), where)) {
        forward_menu_input(menu_input::kOutside, menu_verb::kNone, 0, 0, b.button, -1, mail);
    }
    return true;
}

// WL-CTX-09 -- agents/workshop/pane-menu.md
void WorkshopWeave::menu_pressed(const graphics::AreaPressed& p, std::uint64_t press,
                                 loom::Mail& mail) {
    PresentedMenu& menu = session_.presented;
    if (!p.pressed) {
        return; // a release is no act, and the presenter hears it in its area
    }
    // THE PRESENTER READS THE PRESS; THIS HOST COUNTS IT, as the act it is, so a choice the
    // presenter makes of it -- said under the press's number -- continues that act, its hand's.
    count_gesture();
    leave_carry_hover(mail);
    if (input_actor_.local && !guide_marks_.empty()) {
        guide_pressed(mail, canvas_point_of(input::space::kPixels, p.x, p.y));
    }
    if (p.button == 3) {
        // A RIGHT PRESS ON THE MENU ENDS IT, and asks nothing of what stands beneath.
        withdraw_menu("a newer press", mail);
        repaint(mail);
        return;
    }
    menu.last_input = gestures_;
    menu.presses.push_back(MenuPress{press, hand().latest});
    if (menu.presses.size() > kMaxMenuPresses) {
        menu.presses.erase(menu.presses.begin());
    }
}

// WL-CTX-09 -- agents/workshop/pane-menu.md
std::uint64_t WorkshopWeave::act_of_menu_press(const PresentedMenu& menu,
                                               std::uint64_t press) const {
    if (press == 0) {
        return 0;
    }
    for (const MenuPress& kept : menu.presses) {
        if (kept.press == press) {
            return kept.act;
        }
    }
    return 0;
}

// WL-CTX-09 -- agents/workshop/pane-menu.md
void WorkshopWeave::on(const MenuShown& shown, loom::Mail& mail) {
    if (!about_open_menu(shown.menu, mail)) {
        return; // not the presenter, or not the menu that is open: nothing moves
    }
    PresentedMenu& menu = session_.presented;
    // WHAT THE MENU TAKES IS JUDGED AS A PANE'S ROWS ARE: in the area granted, within the room
    // granted, bounded. A presenter whose menu cannot stand there loses it, in words, rather than
    // reaching past its room.
    std::string refused;
    if (shown.area != menu.area) {
        refused = "the presenter showed the menu in an area it was not granted";
    } else if (shown.rows <= 0 || shown.columns <= 0) {
        refused = "the presenter showed an empty menu";
    } else if (shown.rows > static_cast<std::int64_t>(kMaxMenuLines) ||
               shown.rows > menu.room_rows) {
        refused = "the presenter showed " + std::to_string(shown.rows) + " lines in a room of " +
                  std::to_string(menu.room_rows);
    } else if (shown.columns > static_cast<std::int64_t>(kMaxMenuLineLen) ||
               shown.columns > menu.room_columns) {
        refused = "the presenter showed a line wider than its room";
    }
    if (!refused.empty()) {
        withdraw_menu(refused, mail);
        repaint(mail);
        return;
    }
    menu.rows = shown.rows;
    menu.columns = shown.columns;
    menu.picture = shown.picture;
    stamps_owed_ = desk_owed_ = true; // the menu the desk says may have moved
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
    // A choice is recorded as the continuation of the act that made it: one this host forwarded to
    // that menu, or a press the window landed in its area, which the presenter's word cites by the
    // window's number. A request continuing it is honored while that act is the weaver's latest
    // (the choosing click's release is no new act).
    const std::uint64_t act = closed.input > 0 ? static_cast<std::uint64_t>(closed.input)
                                               : act_of_menu_press(ended, mail.correlation());
    // THE HAND THAT MADE THE CHOOSING ACT: its choice is that hand's to continue, while that act is
    // still its latest. An act the book no longer holds is no hand's latest.
    Hand* chooser = act != 0 ? hand_of_act(act) : nullptr;
    if (chooser != nullptr && closed.chosen && act >= ended.first_input && act <= ended.last_input) {
        if (const RuntimePane* row = session_.panes.runtime.find(ended.office, ended.pane)) {
            ChoiceAnswered c;
            c.kind = row->kind;
            c.gesture = act;
            c.correlation = ended.correlation;
            c.cell.understood = ended.anchored;
            c.cell.px.x = ended.anchor_x;
            c.cell.px.y = ended.anchor_y;
            c.cell.cell.x = graphics::cell_of_pixel(ended.anchor_x);
            c.cell.cell.y = graphics::cell_of_pixel(ended.anchor_y);
            c.spent = false;
            chooser->choice_answered = c;
            keep_act(*chooser, GestureSent{c.kind, c.gesture, c.correlation});
        }
    }
    // A STANDARD ROW IS THE HOST'S OWN: spent here on the pane the menu was about, as the act that
    // chose it, while that act is still the weaver's latest -- the presenter answered the
    // requester unchosen, so no pane performs it.
    if (chooser != nullptr && !closed.chosen && !closed.standard.empty() && act >= ended.first_input &&
        act <= ended.last_input && act == chooser->latest) {
        // The row is spent as the chooser's own act, so a class it is is judged for that hand.
        const InputActor before = input_actor_;
        input_actor_ = chooser->actor;
        spend_standard_row(ended, closed.standard, mail);
        input_actor_ = before;
    }
    repaint(mail);
}

// WL-CTX-09 -- agents/workshop/pane-menu.md
void WorkshopWeave::spend_standard_row(const PresentedMenu& ended, const std::string& id,
                                       loom::Mail& mail) {
    // THE CATALOG'S OWN PANE ROWS, the ones every grant carries: an id naming none spends nothing.
    const PaneRef pane{ended.office, ended.pane};
    for (const ContextEntry& entry : context_population(context_subject::kPane, "")) {
        if (entry.is_group && id == entry.group) {
            // A GROUP OPENS THE HOST'S OWN MENU AT THAT GROUP, beside the menu it was chosen on.
            ContextMenu next;
            next.open = true;
            next.anchored = ended.anchored;
            next.anchor_x = ended.anchor_x;
            next.anchor_y = ended.anchor_y;
            next.subject = context_subject::kPane;
            next.pane = pane;
            next.group = entry.group;
            session_.context = next;
            return;
        }
        if (!entry.is_group && entry.row != nullptr && id == entry.row->id) {
            ContextMenu spent;
            spent.subject = context_subject::kPane;
            spent.pane = pane;
            spend_context_choice(entry.row->act, spent, mail);
            return;
        }
    }
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
        // presenter's state). Its pictures start over -- a new image numbers afresh -- and it
        // draws the menu in its own way in the same area, its next `MenuShown` saying how much of
        // it the menu takes; the weaver's interaction continues.
        session_.presented.picture = 0;
        stamps_owed_ = desk_owed_ = true; // the menu's number is the desk's, and nothing repaints here
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
    const RuntimePane* row = session_.panes.runtime.find(office, asked.pane);
    if (row == nullptr || mail.correlation() == 0) {
        return;
    }
    // A CONTINUATION OF THE CHOICE A PRESENTER LAST REPORTED FOR THAT PANE, and only while that
    // choice is still its hand's latest act; once, and never for a pane the inventory does not
    // name.
    ChoiceAnswered* choice = nullptr;
    each_hand([&](Hand& h) {
        if (!h.choice_answered.spent && h.choice_answered.kind == row->kind &&
            h.choice_answered.correlation == mail.correlation() && h.choice_answered.gesture == h.latest)
            choice = &h.choice_answered;
    });
    if (choice == nullptr) {
        return;
    }
    choice->spent = true;
    const ChoiceAnswered& chosen = *choice;
    const PaneRef subject{asked.office, asked.target};
    bool named = false;
    for (const CatalogRow& r : inventory_rows(session_.setup.active, session_.panes)) {
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
    next.anchored = chosen.cell.understood;
    next.anchor_x = chosen.cell.px.x;
    next.anchor_y = chosen.cell.px.y;
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
    const RuntimePane* row = session_.panes.runtime.find(office, asked.pane);
    if (row == nullptr || mail.correlation() == 0) {
        return;
    }
    // A continuation of the choice a presenter last reported for that pane, judged as a manage
    // request is (`Hand::choice_answered`). The menu left the keys where they were; a pane whose
    // chosen row begins an edit asks for them here.
    ChoiceAnswered* choice = nullptr;
    each_hand([&](Hand& h) {
        if (!h.choice_answered.spent && h.choice_answered.kind == row->kind &&
            h.choice_answered.correlation == mail.correlation() && h.choice_answered.gesture == h.latest)
            choice = &h.choice_answered;
    });
    if (choice == nullptr) {
        return;
    }
    choice->spent = true;
    if (!session_.panes.has(row->kind) || !kind_takes_keyboard(row->kind)) {
        return; // a pane not on the desk, or one that takes no keys, gets none
    }
    // THE GUARDED TRANSITION: the pane becomes the selected, keyboard-holding pane, so the next
    // key -- the one being captured, or the spelling being typed -- reaches it. Its own rows say
    // what it is now showing; nothing here types anything.
    session_.panes.selected = row->kind;
    session_.panes.keyboard = row->kind;
    repaint(mail);
}

// WL-ARR-15 -- agents/workshop/arrangement.md
void WorkshopWeave::on(const PaneEscapeUnspent& said, loom::Mail& mail) {
    const std::string_view office = mail.authored_role();
    if (office.empty()) {
        return;
    }
    const RuntimePane* row = session_.panes.runtime.find(office, said.pane);
    if (row == nullptr) {
        return; // a pane this office never offered is no pane of the desk's
    }
    const std::int64_t kind = row->kind;
    // The particular Escape this answers, first: current state cannot identify it, since a second
    // Escape recreates that state. The number was minted for one keystroke, and zero (an answer
    // echoing nothing) is never an Escape.
    Hand* by = nullptr;
    each_hand([&](Hand& h) {
        if (mail.correlation() != 0 && mail.correlation() == h.escape_sent.answering) by = &h;
    });
    if (by == nullptr) {
        return;
    }
    // STILL ITS HAND'S LATEST GESTURE, INTO THIS PANE, WHICH STILL HAS THE DESK AND THE KEYS.
    // A key, text, press or wheel of that hand's since leaves this about an Escape that is no longer
    // what it did last, and putting a pane down under a later gesture would act on a stale word.
    if (by->escape_sent.kind != kind || by->escape_sent.gesture != by->latest ||
        session_.panes.selected != kind || typing_pane(session_) != kind) {
        return;
    }
    by->escape_sent = GestureSent{};
    // What an unspent Escape means is the desktop's (WL-DESK-02); the judgement above stays the
    // host's, as facts about the room. The chain has two links, both checked: the pane echoed this
    // host's number, and `request_app_action` mints a second for the desktop. A desktop declaring
    // no default row leaves the gesture meaning nothing; nothing compiled in answers behind it.
    if (const AppRow* app_row = session_.keymap.app_action_for(
            app_precedence::kDefault, KeyContext::kPane, input::scan::kEscape,
            input::mod::kNone, kind)) {
        request_app_action(app_row->id, mail, *by);
    }
    repaint(mail);
}

// WL-ARR-13, WL-ARR-14 -- agents/workshop/arrangement.md
// WL-FOCUS-05 -- agents/workshop/focus.md
// WL-FRONT-04 -- agents/workshop/planes.md
void WorkshopWeave::unselect_pane() {
    const std::string name = kind_name(session_.panes, session_.panes.selected);
    session_.panes.selected = kNoPaneKind;
    session_.panes.keyboard = kNoPaneKind;
    say("unselected " + name, false);
}

void WorkshopWeave::external_text(std::int64_t kind, const zengine::input::TextEntered& t,
                                  loom::Mail& mail) {
    const RuntimePane* row = session_.panes.runtime.of_kind(kind);
    if (row == nullptr) {
        return;
    }
    (void)mail.as_role(kWorkshopProvider)
        .send_to_role(row->provider, PaneTextInput{row->pane, t.text});
    note_routed(kind);
}

} // namespace zengine::workshop
