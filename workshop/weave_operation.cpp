// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#include "weave.hpp"
#include <algorithm>
#include <limits>

namespace zengine::workshop {
// ONE HAND PER ACTOR: the weaver's for platform input and input no actor was attributed to, and
// one per guest participant, the oldest forgotten past the bound.
WorkshopWeave::Hand& WorkshopWeave::hand_of(const InputActor& actor) {
    if (!actor.known || actor.local) return weaver_hand_;
    for (Hand& h : guest_hands_)
        if (h.actor.participant == actor.participant) return h;
    if (guest_hands_.size() >= kMaxGuestHands) {
        const auto oldest = std::min_element(guest_hands_.begin(), guest_hands_.end(),
            [](const Hand& a, const Hand& b) { return a.latest < b.latest; });
        guest_hands_.erase(oldest);
    }
    Hand fresh;
    fresh.actor = actor;
    guest_hands_.push_back(fresh);
    return guest_hands_.back();
}

const WorkshopWeave::Hand* WorkshopWeave::find_hand(const InputActor& actor) const {
    if (!actor.known || actor.local) return &weaver_hand_;
    for (const Hand& h : guest_hands_)
        if (h.actor.participant == actor.participant) return &h;
    return nullptr;
}

std::uint64_t WorkshopWeave::latest_of(const InputActor& actor) const {
    const Hand* h = find_hand(actor);
    return h != nullptr ? h->latest : 0;
}

void WorkshopWeave::keep_act(Hand& h, const GestureSent& sent) {
    if (sent.answering == 0) return;
    if (h.kept.size() >= kKeptActs) h.kept.erase(h.kept.begin());
    h.kept.push_back(sent);
}

void WorkshopWeave::drop_kept(std::uint64_t correlation) {
    each_hand([correlation](Hand& h) {
        h.kept.erase(std::remove_if(h.kept.begin(), h.kept.end(),
                                    [correlation](const GestureSent& k) { return k.answering == correlation; }),
                     h.kept.end());
    });
}

void WorkshopWeave::count_gesture() {
    ++gestures_;
    Hand& h = hand();
    h.actor = input_actor_;
    h.latest = gestures_;
    if (recent_acts_.size() >= kMaxRecentActs) recent_acts_.erase(recent_acts_.begin());
    recent_acts_.emplace_back(gestures_, input_actor_);
}

WorkshopWeave::Hand* WorkshopWeave::hand_of_act(std::uint64_t act) {
    for (const auto& [number, actor] : recent_acts_)
        if (number == act) return const_cast<Hand*>(find_hand(actor));
    return nullptr;
}

bool WorkshopWeave::duplicate_input(const loom::Mail& mail) const {
    return !applying_attributed_ && attributed_producer_.valid() &&
           mail.sender() == attributed_producer_;
}

void WorkshopWeave::on(const input::AttributedInput& event, loom::Mail& mail) {
    if (!mail.authored_from_role(input::kInputRole) ||
        (event.local ? event.actor != 0 : event.actor <= 0)) return;
    if (!carried_.data.empty() && !carried_.actor.local) {
        const auto authority = host_->input_authority
            ? host_->input_authority(carried_.actor.participant) : loom::GrantAuthority{};
        if (!mail.describe_authority(authority).available) {
            carried_ = {};
            say("Carried reference released: its input actor has left", false);
            repaint(mail);
        }
    }
    attributed_producer_ = mail.sender();
    struct RestoreInput {
        InputActor& actor;
        bool& applying;
        InputActor before;
        bool was_applying;
        ~RestoreInput() { actor = before; applying = was_applying; }
    } restore{input_actor_, applying_attributed_, input_actor_, applying_attributed_};
    input_actor_ = {true, event.local, loom::WeaveId{static_cast<std::uint64_t>(event.actor)}};
    applying_attributed_ = true;
    const auto& e = event.event;
    // A DRAG CARRY ENDS AT ITS CARRIER'S NEXT GESTURE, OR THE WEAVER'S: another guest's never.
    if (!quitting_ && carried_.drag &&
        (input_actor_.local || (input_actor_.local == carried_.actor.local &&
                                input_actor_.participant == carried_.actor.participant)) &&
        (e.kind == "KeyPressed" || e.kind == "TextEntered" ||
        e.kind == "PointerWheel" || (e.kind == "PointerButton" && e.pressed))) {
        carried_ = {}; value_drag_ = {};
        say("Value drag cancelled by a new gesture", false);
    }
    if (e.kind == "KeyPressed") on(input::KeyPressed{e.scancode, e.name, e.modifiers}, mail);
    else if (e.kind == "TextEntered") on(input::TextEntered{e.text}, mail);
    else if (e.kind == "PointerButton")
        on(input::PointerButton{e.button, e.pressed, e.x, e.y, e.space, e.modifiers}, mail);
    else if (e.kind == "PointerMoved")
        on(input::PointerMoved{e.x, e.y, e.dx, e.dy, e.space, e.modifiers}, mail);
    else if (e.kind == "PointerWheel")
        on(input::PointerWheel{e.wheel_dx, e.wheel_dy, e.x, e.y, e.space, e.modifiers}, mail);

}

void WorkshopWeave::on(const PaneShortcutInvoked& asked, loom::Mail& mail) {
    if (!mail.authored_from_role(kDesktopRole) || !mail.correlation()) return;
    Hand* by = nullptr;
    each_hand([&](Hand& h) {
        if (h.app_asked.answering == mail.correlation() && h.app_asked.gesture == h.latest) by = &h;
    });
    if (by == nullptr) return;
    by->app_asked = {};
    const auto* pane = session_.panes.runtime.find(asked.office, asked.pane);
    const auto* rows = pane ? session_.keymap.pane_rows(pane->kind) : nullptr;
    bool declared = false;
    if (rows) for (const auto& row : rows->rows) if (row.id == asked.action) declared = true;
    if (!pane || !declared || asked.holder <= 0 || !host_->role_holder ||
        host_->role_holder(asked.office).value != static_cast<std::uint64_t>(asked.holder)) {
        say("Shortcut unavailable: its pane, action or provider has changed", true); repaint(mail); return;
    }
    if (const std::string why = refused_toward(by->actor, pane->kind); !why.empty()) {
        refuse_input(why, mail);
        return;
    }
    const auto correlation = ++gesture_asks_;
    const auto sent = mail.as_role(kWorkshopProvider).send_to_role(asked.office,
        PaneActionRequested{asked.pane, asked.action}, correlation);
    if (sent.valid()) keep_act(*by, by->shortcut_sent = {pane->kind, by->latest, correlation});
    else { say("Shortcut invocation could not be queued", true); repaint(mail); }
}

// ONE CURRENT GESTURE APPROVES ONE OPERATION FOR THE PANE IT WAS DELIVERED TO, ONCE: the requester
// holds the office that offered the pane, the pane is on the desk (or a shortcut sent this action),
// the correlation is that pane's unspent press, action, menu choice or shortcut still its hand's
// latest, a guest's live Loom authority permits the send it names, and its admitted row holds every
// action class the act is. The gesture is spent, and `approved` says whose it was.
// WL-GUEST-03, WL-GUEST-05 -- agents/workshop/guests.md
std::string WorkshopWeave::approve_gesture(const std::string& pane_key, std::int64_t gesture,
                                           const std::string& role, const std::string& shape,
                                           std::int64_t version, loom::Mail& mail,
                                           const std::vector<std::string>& classes,
                                           const std::string& subject, InputActor* approved) {
    const auto* pane = session_.panes.runtime.find(mail.authored_role(), pane_key);
    const auto corr = gesture > 0 ? static_cast<std::uint64_t>(gesture) : 0;
    bool shortcut = false;
    if (pane && corr)
        each_hand([&](Hand& h) {
            shortcut = shortcut || (h.shortcut_sent.kind == pane->kind &&
                                    h.shortcut_sent.answering == corr && h.shortcut_sent.gesture == h.latest);
        });
    if (mail.authored_role().empty() || !pane || (!session_.panes.has(pane->kind) && !shortcut) ||
        !host_->role_holder || host_->role_holder(mail.authored_role()) != mail.sender()) {
        return "the requesting pane is no longer on this desk";
    }
    const bool names_send = !role.empty() || !shape.empty() || version != 0;
    // THE RECORD THIS CORRELATION NAMES, in whichever hand sent it, current while nothing of that
    // hand's own came since: another hand's act neither spends nor stales it.
    Hand* by = nullptr;
    each_hand([&](Hand& h) {
        if (!corr) return;
        for (auto* sent : {&h.action_sent, &h.press_sent, &h.shortcut_sent}) {
            if (sent->answering == corr && sent->kind == pane->kind && sent->gesture == h.latest) {
                by = &h;
                *sent = GestureSent{};
            }
        }
        for (auto& sent : h.secondary_cont) {
            if (sent.live && !sent.spent && !sent.interrupted && sent.correlation == corr &&
                sent.kind == pane->kind && sent.gesture_at_press == h.latest) {
                by = &h;
                sent.spent = true;
            }
        }
        if (!h.choice_answered.spent && h.choice_answered.correlation == corr &&
            h.choice_answered.kind == pane->kind && h.choice_answered.gesture == h.latest) {
            by = &h;
            h.choice_answered.spent = true;
        }
    });
    // A CLASSED ASK NAMING NO SEND spends its act's record while it is unspent, whatever its hand
    // did since: no send rides on it, so nothing is laundered through a stale gesture, and its
    // classes are judged for the hand whose act it was.
    if (by == nullptr && corr && !names_send && !classes.empty()) {
        each_hand([&](Hand& h) {
            if (by != nullptr) return;
            for (const GestureSent& k : h.kept) {
                if (k.answering != corr || k.kind != pane->kind) continue;
                by = &h;
                for (auto* sent : {&h.action_sent, &h.press_sent, &h.shortcut_sent})
                    if (sent->answering == corr) *sent = GestureSent{};
                if (h.choice_answered.correlation == corr) h.choice_answered.spent = true;
                return;
            }
        });
    }
    if (by != nullptr) drop_kept(corr);
    // A SEND ON AN ACTOR'S BEHALF NEEDS THAT ACTOR NAMED; an act the pane does itself, judged by its
    // classes alone, takes unattributed input as the weaver's hand -- no guest's moment arrives
    // unattributed, since no guest may say a raw input moment.
    if (by == nullptr || (!by->actor.known && (names_send || classes.empty()))) {
        return "this operation needs a current attributed input gesture";
    }
    const InputActor actor = by->actor;
    if ((names_send || classes.empty()) &&
        (role.empty() || shape.empty() || version <= 0 ||
         version > std::numeric_limits<std::uint32_t>::max())) {
        return "the operation must name its destination and versioned shape";
    }
    if (names_send && !actor.local) {
        const auto authority = host_->input_authority
            ? host_->input_authority(actor.participant) : loom::GrantAuthority{};
        const auto live = mail.describe_authority(authority);
        if (!live.available || !live.permits_role(shape, static_cast<std::uint32_t>(version), role)) {
            return "the input actor has no authority for this operation";
        }
    }
    // A SEND ON A GUEST'S BEHALF IS THE CLASSES ITS SHAPE IS: the owner hears it from the pane
    // that relays it, never from the guest, so it is judged here, for the hand that asked.
    if (names_send) {
        if (std::string refused = scope::judge_send(guest_of(actor), host_->host_fact, shape); !refused.empty())
            return refused;
    }
    if (std::string refused = judge_classes(actor, classes, subject); !refused.empty()) return refused;
    if (approved != nullptr) *approved = actor;
    return {};
}

// The classes an act is, judged for one actor: the weaver's own hand and every participant no guest
// door admitted are not narrowed; a guest's are judged against its admitted row and this host.
// WL-GUEST-03 -- agents/workshop/guests.md
std::string WorkshopWeave::judge_classes(const InputActor& actor, const std::vector<std::string>& classes,
                                         const std::string& subject) const {
    if (classes.empty()) return {};
    return scope::judge_all(guest_of(actor), host_->host_fact, classes, subject);
}

scope::GuestRowFacts WorkshopWeave::guest_of(const InputActor& actor) const {
    if (!actor.known || actor.local || !host_->guest_row) return {};
    return host_->guest_row(actor.participant);
}

// ON A WEAVER'S HOST, THE PLACES ONLY THE WEAVER'S HAND REACHES: the editor office, whichever
// implementation holds it, the Terminal, and the Hotkeys pane, whose every act edits the keymap
// file. A guest's keys, text and presses bound there are refused in words at the dispatch.
// WL-GUEST-06 -- agents/workshop/guests.md
std::string WorkshopWeave::refused_toward(const InputActor& actor, std::int64_t kind) const {
    const RuntimePane* row = session_.panes.runtime.of_kind(kind);
    if (row == nullptr) return {};
    const scope::GuestRowFacts guest = guest_of(actor);
    if (!guest.admitted || host_->host_fact.development) return {};
    if (row->provider == kEditorRole) return scope::refuse_toward(guest, host_->host_fact, "the editor");
    if (row->provider == kTerminalRole) return scope::refuse_toward(guest, host_->host_fact, "the Terminal");
    if (row->provider == kDesktopRole && row->pane == kHotkeysPane)
        return scope::refuse_toward(guest, host_->host_fact, "the Hotkeys pane, which edits the keymap file,");
    return {};
}

void WorkshopWeave::refuse_input(const std::string& why, loom::Mail& mail) {
    say(why, true);
    repaint(mail);
}

std::string WorkshopWeave::authorize_pane_operation(const PaneOperationRequested& asked, loom::Mail& mail) {
    InputActor actor;
    auto reason = approve_gesture(asked.pane, asked.gesture, asked.role, asked.shape, asked.version, mail,
                                  {}, {}, &actor);
    if (reason.empty())
        approved_operation_ = {mail.sender(), asked.pane,
                               asked.gesture > 0 ? static_cast<std::uint64_t>(asked.gesture) : 0,
                               latest_of(actor), actor};
    return reason;
}
void WorkshopWeave::on(const PaneOperationRequested& asked, loom::Mail& mail) {
    const auto reason = authorize_pane_operation(asked, mail);
    (void)mail.answer(PaneOperationAnswered{reason.empty(), reason});
}
// WL-GUEST-03 -- agents/workshop/guests.md
// A classed act's approval is the pane's to carry to its later beats; this host keeps nothing of it,
// so it never stands in for the one carry `approved_operation_` waits on.
void WorkshopWeave::on(const v2::PaneOperationRequested& asked, loom::Mail& mail) {
    const auto reason = approve_gesture(asked.pane, asked.gesture, asked.role, asked.shape, asked.version,
                                        mail, asked.classes, asked.subject);
    (void)mail.answer(PaneOperationAnswered{reason.empty(), reason});
}

// A DOOR NO GESTURE CROSSED: an owner asks about the sender of a message it received directly. The
// session is judged as a gesture's actor is, from its admitted row; any other participant is not a
// guest and is not narrowed. Only an office asks, and nothing is remembered.
// WL-GUEST-04 -- agents/workshop/guests.md
void WorkshopWeave::on(const ActorScopeRequested& asked, loom::Mail& mail) {
    if (mail.authored_role().empty()) return;
    InputActor actor;
    actor.known = asked.session > 0;
    actor.local = false;
    actor.participant = loom::WeaveId{static_cast<std::uint64_t>(asked.session > 0 ? asked.session : 0)};
    const std::string refusal = asked.classes.empty() ? std::string("the question names no action class")
                                                      : judge_classes(actor, asked.classes, {});
    (void)mail.answer(ActorScopeJudged{refusal.empty(), refusal});
}

// AN OBSERVATION LEASE: the same one-gesture approval, kept for repeated reads of one shape at one
// role about one subject. One lease per pane (a new request replaces it), a bounded book, and no
// authority of its own: every continuation is judged again below.
void WorkshopWeave::on(const PaneObservationRequested& asked, loom::Mail& mail) {
    InputActor actor;
    auto reason = approve_gesture(asked.pane, asked.gesture, asked.role, asked.shape, asked.version, mail,
                                  {}, {}, &actor);
    if (reason.empty() && (asked.subject.empty() || asked.subject.size() > 256))
        reason = "the observation must name its subject";
    const std::string office(mail.authored_role());
    if (reason.empty()) {
        // A LEASE WHOSE HOLDER NO LONGER HOLDS ITS OFFICE can never be continued: forget it
        // before counting, so a departed provider's approvals cannot fill the book.
        std::erase_if(leases_, [&](const ObservationLease& l) {
            return (l.office == office && l.pane == asked.pane) ||
                   !host_->role_holder || host_->role_holder(l.office) != l.holder;
        });
        if (leases_.size() >= kMaxObservationLeases)
            reason = "too many observations are active; pause one first";
    }
    if (!reason.empty()) { (void)mail.answer(PaneObservationAnswered{false, reason, 0}); return; }
    leases_.push_back({++next_lease_, mail.sender(), office, asked.pane, asked.role, asked.shape,
                       asked.subject, asked.version, actor});
    (void)mail.answer(PaneObservationAnswered{true, {}, leases_.back().id});
}

// ONE MORE OBSERVATION UNDER A LEASE: its holder still holds the office, the pane is still on the
// desk, the subject is the one approved, and a guest actor is still present with the authority.
// Any lapse refuses AND forgets the lease; a request from anyone but its holder forgets nothing.
void WorkshopWeave::on(const PaneObservationContinued& asked, loom::Mail& mail) {
    const std::string office(mail.authored_role());
    const auto it = std::find_if(leases_.begin(), leases_.end(), [&](const ObservationLease& l) {
        return l.id == asked.lease && l.office == office && l.pane == asked.pane && l.holder == mail.sender();
    });
    if (office.empty() || it == leases_.end()) {
        (void)mail.answer(PaneObservationAnswered{false, "this observation is not current; start it again with a gesture", 0});
        return;
    }
    std::string reason;
    const auto* pane = session_.panes.runtime.find(office, asked.pane);
    if (!host_->role_holder || host_->role_holder(office) != mail.sender()) {
        reason = "the observing pane's office changed hands";
    } else if (asked.subject != it->subject) {
        reason = "the observation's subject changed; start it again";
    } else if (!pane || !session_.panes.has(pane->kind)) {
        reason = "the observing pane is no longer on the desk";
    } else if (!it->actor.local) {
        const auto authority = host_->input_authority
            ? host_->input_authority(it->actor.participant) : loom::GrantAuthority{};
        const auto live = mail.describe_authority(authority);
        if (!live.available || !live.permits_role(it->shape, static_cast<std::uint32_t>(it->version), it->role))
            reason = "the actor who started this observation has left or lost its authority";
    }
    if (!reason.empty()) {
        leases_.erase(it);
        (void)mail.answer(PaneObservationAnswered{false, reason, 0});
        return;
    }
    (void)mail.answer(PaneObservationAnswered{true, {}, it->id});
}

// ONLY THE HOLDER ENDS ITS OWN LEASE -- one by id, or with lease 0 every one it holds on that pane,
// which is what an arriving image says: a reload keeps the WeaveId, so this book cannot tell the
// predecessor's leases from the successor's. Ending only removes approvals; it grants nothing.
void WorkshopWeave::on(const PaneObservationEnded& asked, loom::Mail& mail) {
    const std::string office(mail.authored_role());
    std::erase_if(leases_, [&](const ObservationLease& l) {
        return (asked.lease == 0 || l.id == asked.lease) && l.office == office && l.pane == asked.pane &&
               l.holder == mail.sender();
    });
}

void WorkshopWeave::on(const PaneCarryRequested& asked, loom::Mail& mail) {
    accept_carry(asked, false, false, mail);
}
void WorkshopWeave::on(const PaneValueCarryRequested& asked, loom::Mail& mail) {
    accept_carry({asked.pane, asked.label, asked.data}, true, asked.drag, mail);
}
void WorkshopWeave::on(const v2::PaneValueCarryRequested& asked, loom::Mail& mail) {
    if (asked.token.empty() || asked.token.size() > 128) {
        (void)mail.answer(PaneCarryAnswered{false, "the transfer token needs 1..128 bytes"}); return;
    }
    accept_carry({asked.pane, asked.label, asked.data}, true, asked.drag, mail, asked.token);
}
void WorkshopWeave::accept_carry(const PaneCarryRequested& asked, bool value, bool drag, loom::Mail& mail,
                                 std::string token) {
    const auto* pane = session_.panes.runtime.find(mail.authored_role(), asked.pane);
    if (!pane || !session_.panes.has(pane->kind) || !approved_operation_.actor.known ||
        approved_operation_.pane_owner != mail.sender() ||
        approved_operation_.pane != asked.pane ||
        approved_operation_.correlation != mail.correlation() ||
        approved_operation_.gesture != latest_of(approved_operation_.actor)) {
        (void)mail.answer(PaneCarryAnswered{false, "the approved acquisition is no longer current"});
        return;
    }
    const InputActor carrier = approved_operation_.actor;
    const std::uint64_t approved = approved_operation_.gesture;
    approved_operation_ = {};
    // A DRAG IS THE CARRIER'S OWN APPROVED PRESS, still its latest: another hand's press, made
    // since, holds no carry of this one's.
    if (drag && (value_drag_.gesture == 0 || value_drag_.gesture != approved ||
                 value_drag_.actor.local != carrier.local ||
                 value_drag_.actor.participant != carrier.participant)) {
        (void)mail.answer(PaneCarryAnswered{false, "this value drag no longer has its primary press"});
        return;
    }
    if (asked.data.empty() || asked.data.size() > kMaxCarryBytes || asked.label.size() > kMaxCarryLabelBytes) {
        (void)mail.answer(PaneCarryAnswered{false, "the carried reference exceeds its limits"});
        return;
    }
    if (!carried_.data.empty()) {
        (void)mail.answer(PaneCarryAnswered{false, "another item is already being carried"}); return;
    }
    carried_ = {asked.data, asked.label, carrier, value, drag,
                std::string(mail.authored_role()), asked.pane, std::move(token)};
    if (!drag) say("Carrying " + asked.label + " — click a receiving pane; Escape cancels", false);
    // A DRAG BEGUN ON A CANVAS IS NOW A CARRY: the press's hold ends there as lost, so its later
    // motion and its release are the carry's, never the canvas's.
    if (drag && canvas_holds_[0].active && canvas_holds_[0].kind == pane->kind) lose_canvas_hold(0, mail);
    (void)mail.answer(PaneCarryAnswered{true, {}});
    if (drag && value_drag_.released) finish_value_drag(mail);
    repaint(mail);
}

// WL-GUEST-06 -- agents/workshop/guests.md
bool WorkshopWeave::drop_carry(std::int64_t kind, const ExternalPressAt& at, loom::Mail& mail,
                               std::int64_t picture, const PointedAt& point,
                               const std::optional<CanvasRelease>& released) {
    if (carried_.data.empty()) return false;
    if (!input_actor_.known || input_actor_.local != carried_.actor.local ||
        input_actor_.participant != carried_.actor.participant) {
        say("Only the actor carrying this reference may place it", true);
        return true;
    }
    // A GUEST'S DROP, by click or by a drag's release, hands its bytes to the pane it lands on, and
    // on a weaver's host the places only the weaver's hand reaches take none.
    if (const std::string why = refused_toward(input_actor_, kind); !why.empty()) {
        say(why, true);
        return true;
    }
    const auto* pane = session_.panes.runtime.of_kind(kind);
    const auto* presentation = session_.panes.external_pane(kind);
    if (pane && presentation && presentation->canvas.grant != 0)
        return drop_on_canvas(*pane, *presentation, point, released, mail);
    const bool origin_drop = pane && carried_.value && host_->holder_accepts &&
        host_->holder_accepts(pane->provider, *loom::schema_of<v2::PaneValueDrop>());
    if (!at.named || !pane || !presentation || presentation->canvas.grant != 0 || !host_->holder_accepts ||
        (!origin_drop && !host_->holder_accepts(pane->provider, carried_.value ? *loom::schema_of<PaneValueDrop>()
                                                                             : *loom::schema_of<PaneDrop>()))) {
        say("This place does not accept the carried item; Escape cancels", true);
        return true;
    }
    const auto correlation = ++gesture_asks_;
    const auto aimed = picture < 0 ? presentation->stamp.aimed : picture;
    const auto sent = origin_drop
        ? mail.as_role(kWorkshopProvider).send_to_role(pane->provider,
            v2::PaneValueDrop{pane->pane, carried_.data, at.row, at.column, aimed,
                             carried_.source_office, carried_.source_pane, carried_.token}, correlation)
        : carried_.value
        ? mail.as_role(kWorkshopProvider).send_to_role(pane->provider,
            PaneValueDrop{pane->pane, carried_.data, at.row, at.column, aimed}, correlation)
        : mail.as_role(kWorkshopProvider).send_to_role(pane->provider,
            PaneDrop{pane->pane, carried_.data, at.row, at.column, aimed}, correlation);
    if (!sent.valid()) {
        say("Item placement could not be queued; it is still held", true);
        return true;
    }
    const bool value = carried_.value;
    carried_ = {};
    keep_act(hand(), hand().press_sent = GestureSent{kind, hand().latest, correlation});
    session_.panes.selected = kind;
    session_.panes.keyboard = kind;
    note_routed(kind);
    say(std::string(value ? "Value" : "Reference") + " sent to " + pane->name, false);
    return true;
}

// A VALUE OR A REFERENCE PLACED ON A CANVAS: the place in the canvas's local pixels and the
// picture it was aimed at, for the provider to hit-test in what it drew; a provider without the
// canvas door for what is carried is told nothing and the item stays held, as on any place that
// does not accept it. A drag's drop names the canvas its release met, however long the carry
// took to be answered.
bool WorkshopWeave::drop_on_canvas(const RuntimePane& pane, const ExternalPane& presentation,
                                   const PointedAt& point,
                                   const std::optional<CanvasRelease>& released,
                                   loom::Mail& mail) {
    const auto& c = presentation.canvas;
    const CanvasRelease at = released ? *released
        : CanvasRelease{presentation.stamp.aimed, c.grant, PixelRect{c.x, c.y, c.width, c.height}};
    if (at.grant != c.grant) {
        say("The canvas the item was released on is gone; Escape cancels", true);
        return true;
    }
    const bool value = carried_.value;
    // Workshop's mark over a refused picture is Workshop's own chrome: nothing is placed under it,
    // judged where a drag's release met it.
    const bool marked = released ? released->marked : on_refused_mark(pane.kind, point);
    if (!point.understood || !host_->holder_accepts ||
        !host_->holder_accepts(pane.provider, !value   ? *loom::schema_of<PaneCanvasDrop>()
                                              : c.legacy ? *loom::schema_of<v1::PaneCanvasValueDrop>()
                                                         : *loom::schema_of<PaneCanvasValueDrop>()) ||
        !at.body.contains_at(point.px.x, point.px.y, point.grain) || marked) {
        say("This place does not accept the carried item; Escape cancels", true);
        return true;
    }
    const auto correlation = ++gesture_asks_;
    const std::int64_t x = surface::sub_px(point.px.x, at.body.x);
    const std::int64_t y = surface::sub_px(point.px.y, at.body.y);
    const auto sent = !value
        ? mail.as_role(kWorkshopProvider).send_to_role(pane.provider,
              PaneCanvasDrop{pane.pane, at.grant, at.picture, x, y, carried_.data}, correlation)
        : c.legacy
        ? mail.as_role(kWorkshopProvider).send_to_role(pane.provider,
              v1::PaneCanvasValueDrop{pane.pane, at.grant, at.picture, legacy_subs_of_px(x),
                                      legacy_subs_of_px(y), carried_.data,
                                      carried_.source_office, carried_.source_pane,
                                      carried_.token},
              correlation)
        : mail.as_role(kWorkshopProvider).send_to_role(pane.provider,
              PaneCanvasValueDrop{pane.pane, at.grant, at.picture, x, y, carried_.data,
                                  carried_.source_office, carried_.source_pane, carried_.token},
              correlation);
    if (!sent.valid()) {
        say("Item placement could not be queued; it is still held", true);
        return true;
    }
    carried_ = {};
    const auto kind = pane.kind;
    keep_act(hand(), hand().press_sent = GestureSent{kind, hand().latest, correlation});
    session_.panes.selected = kind;
    session_.panes.keyboard = kind;
    note_routed(kind);
    say(std::string(value ? "Value" : "Reference") + " sent to " + pane.name, false);
    return true;
}

void WorkshopWeave::begin_value_drag(const input::PointerButton& b) {
    value_drag_ = {};
    drag_pointer_ = canvas_point_of(b.space, b.x, b.y);
    value_drag_.gesture = gestures_;
    value_drag_.actor = input_actor_;
    value_drag_.x = b.x; value_drag_.y = b.y; value_drag_.space = b.space;
}
bool WorkshopWeave::move_value_drag(const input::PointerMoved& motion, loom::Mail& mail) {
    auto& drag = value_drag_;
    if (!drag.gesture || drag.gesture != latest_of(drag.actor) || drag.released || !input_actor_.known ||
        input_actor_.local != drag.actor.local || input_actor_.participant != drag.actor.participant)
        return false;
    if (motion.space != drag.space) return false;
    drag_pointer_ = canvas_point_of(motion.space, motion.x, motion.y);
    const std::uint64_t threshold = motion.space == input::space::kPixels ? 4 : 1;
    const auto distance = [](std::int64_t a, std::int64_t b) -> std::uint64_t {
        return a >= b ? std::uint64_t(a) - std::uint64_t(b) : std::uint64_t(b) - std::uint64_t(a);
    };
    if (distance(motion.x, drag.x) >= threshold || distance(motion.y, drag.y) >= threshold)
        drag.moved = true;
    if (!carried_.drag) return false;
    if (drag.moved) { say("Dragging " + carried_.label + " — release over a receiving pane", false); repaint(mail); }
    return true;
}
bool WorkshopWeave::release_value_drag(const input::PointerButton& button, loom::Mail& mail) {
    auto& drag = value_drag_;
    if (button.button != 1 || !drag.gesture || drag.gesture != latest_of(drag.actor) || drag.released ||
        !input_actor_.known || input_actor_.local != drag.actor.local ||
        input_actor_.participant != drag.actor.participant) return false;
    // The release is retained even when it precedes the asynchronous acquisition answer.
    drag.released = true;
    if (button.space == drag.space && !session_.arrange.open && !session_.context.open && !session_.presented.open) {
        const auto point = canvas_point_of(button.space, button.x, button.y);
        const auto owner = occupied_at(session_.panes, session_.setup.active, screen_of(session_), point);
        if (point.understood && owner.occupied && is_runtime_kind(owner.kind)) {
            drag.target = owner.kind;
            drag.at = external_press_at(session_.panes, session_.setup.active, screen_of(session_),
                owner.kind, session_.pane_titles, button.space, button.x, button.y);
            drag.point = point;
            const auto* pane = session_.panes.runtime.of_kind(owner.kind);
            const auto* presentation = session_.panes.external_pane(owner.kind);
            if (pane && presentation && host_->role_holder) {
                drag.receiver = host_->role_holder(pane->provider);
                drag.picture = presentation->stamp.aimed;
                const auto& c = presentation->canvas;
                if (c.grant != 0)
                    drag.canvas = CanvasRelease{presentation->stamp.aimed, c.grant,
                                                PixelRect{c.x, c.y, c.width, c.height},
                                                on_refused_mark(owner.kind, point)};
            }
        }
    }
    if (!carried_.drag) return false;
    finish_value_drag(mail); return true;
}
void WorkshopWeave::finish_value_drag(loom::Mail& mail) {
    const auto drag = value_drag_;
    value_drag_ = {};
    if (!carried_.drag) return;
    if (!drag.moved) { carried_ = {}; return; }
    const auto* pane = session_.panes.runtime.of_kind(drag.target);
    if (drag.gesture != latest_of(drag.actor) || !pane || !drag.receiver.valid() || !host_->role_holder ||
        host_->role_holder(pane->provider) != drag.receiver) {
        carried_ = {}; say("Value drag cancelled: no current receiver at the release", true); return;
    }
    const auto previous = input_actor_;
    input_actor_ = drag.actor;
    (void)drop_carry(drag.target, drag.at, mail, drag.picture, drag.point, drag.canvas);
    input_actor_ = previous;
    if (!carried_.data.empty()) {
        carried_ = {};
        say("Value was not placed: " + session_.notice, true);
    }
}
} // namespace zengine::workshop
