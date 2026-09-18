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
    const std::uint64_t answering = escape ? ++escape_asks_ : 0;
    // THE PANE'S OWN ROWS FIRST, against the EFFECTIVE map -- the maker's override where
    // one is authored, the pane's default otherwise. A match crosses as the id and NOT as
    // the key: one keystroke, one sentence, and the pane acts on a name.
    if (const PaneRow* action =
            session_.keymap.pane_action_for(kind, k.scancode, k.modifiers)) {
        (void)mail.as_role(kWorkshopProvider)
            .send_to_role(row->provider, PaneActionRequested{row->pane, action->id}, answering);
        note_routed(kind);
        if (escape) {
            escape_sent_ = EscapeSent{kind, gestures_, answering};
        }
        return true;
    }
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
