// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#include "weave.hpp"
#include <algorithm>
#include <limits>

namespace zengine::workshop {
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
    if (!quitting_ && carried_.drag && (e.kind == "KeyPressed" || e.kind == "TextEntered" ||
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
    if (!mail.authored_from_role(kDesktopRole) || !mail.correlation() ||
        mail.correlation() != app_asked_.answering || app_asked_.gesture != gestures_) return;
    app_asked_ = {};
    const auto* pane = session_.panels.runtime.find(asked.office, asked.pane);
    const auto* rows = pane ? session_.keymap.pane_rows(pane->kind) : nullptr;
    bool declared = false;
    if (rows) for (const auto& row : rows->rows) if (row.id == asked.action) declared = true;
    if (!pane || !declared || asked.holder <= 0 || !host_->role_holder ||
        host_->role_holder(asked.office).value != static_cast<std::uint64_t>(asked.holder)) {
        say("Shortcut unavailable: its pane, action or provider has changed", true); repaint(mail); return;
    }
    const auto correlation = ++escape_asks_;
    const auto sent = mail.as_role(kWorkshopProvider).send_to_role(asked.office,
        PaneActionRequested{asked.pane, asked.action}, correlation);
    if (sent.valid()) shortcut_sent_ = {pane->kind, gestures_, correlation};
    else { say("Shortcut invocation could not be queued", true); repaint(mail); }
}

// ONE CURRENT GESTURE APPROVES ONE (SHAPE, VERSION, ROLE) FOR THE PANE IT WAS DELIVERED TO, ONCE:
// the requester holds the office that offered the pane, the pane is on the desk (or a shortcut sent
// this action), the correlation is that pane's unspent current press, action, menu choice or
// shortcut, and a guest's live Loom authority permits the operation. The gesture is spent.
std::string WorkshopWeave::approve_gesture(const std::string& pane_key, std::int64_t gesture,
                                           const std::string& role, const std::string& shape,
                                           std::int64_t version, loom::Mail& mail) {
    const auto* pane = session_.panels.runtime.find(mail.authored_role(), pane_key);
    const bool shortcut = pane && gesture > 0 && shortcut_sent_.kind == pane->kind &&
        shortcut_sent_.answering == static_cast<std::uint64_t>(gesture) && shortcut_sent_.gesture == gestures_;
    if (mail.authored_role().empty() || !pane || (!session_.panels.has(pane->kind) && !shortcut) ||
        !host_->role_holder || host_->role_holder(mail.authored_role()) != mail.sender()) {
        return "the requesting pane is no longer on this desk";
    }
    const auto corr = gesture > 0 ? static_cast<std::uint64_t>(gesture) : 0;
    bool current = false;
    for (auto* sent : {&action_sent_, &press_sent_, &shortcut_sent_}) {
        if (corr && sent->answering == corr && sent->kind == pane->kind &&
            sent->gesture == gestures_) {
            current = true;
            *sent = EscapeSent{};
        }
    }
    for (auto& sent : secondary_cont_) {
        if (corr && sent.live && !sent.spent && !sent.interrupted &&
            sent.correlation == corr && sent.kind == pane->kind &&
            sent.gesture_at_press == gestures_) {
            current = true;
            sent.spent = true;
        }
    }
    if (corr && !choice_answered_.spent && choice_answered_.correlation == corr &&
        choice_answered_.kind == pane->kind && choice_answered_.gesture == gestures_) {
        current = true;
        choice_answered_.spent = true;
    }
    if (!current || !gesture_actor_.known) {
        return "this operation needs a current attributed input gesture";
    }
    if (role.empty() || shape.empty() || version <= 0 ||
        version > std::numeric_limits<std::uint32_t>::max()) {
        return "the operation must name its destination and versioned shape";
    }
    if (!gesture_actor_.local) {
        const auto authority = host_->input_authority
            ? host_->input_authority(gesture_actor_.participant) : loom::GrantAuthority{};
        const auto live = mail.describe_authority(authority);
        if (!live.available || !live.permits_role(shape, static_cast<std::uint32_t>(version), role)) {
            return "the input actor has no authority for this operation";
        }
    }
    return {};
}

std::string WorkshopWeave::authorize_pane_operation(const PaneOperationRequested& asked, loom::Mail& mail) {
    auto reason = approve_gesture(asked.pane, asked.gesture, asked.role, asked.shape, asked.version, mail);
    if (reason.empty())
        approved_operation_ = {mail.sender(), asked.pane,
                               asked.gesture > 0 ? static_cast<std::uint64_t>(asked.gesture) : 0, gestures_};
    return reason;
}
void WorkshopWeave::on(const PaneOperationRequested& asked, loom::Mail& mail) {
    const auto reason = authorize_pane_operation(asked, mail);
    (void)mail.answer(PaneOperationAnswered{reason.empty(), reason});
}

// AN OBSERVATION LEASE: the same one-gesture approval, kept for repeated reads of one shape at one
// role about one subject. One lease per pane (a new request replaces it), a bounded book, and no
// authority of its own: every continuation is judged again below.
void WorkshopWeave::on(const PaneObservationRequested& asked, loom::Mail& mail) {
    auto reason = approve_gesture(asked.pane, asked.gesture, asked.role, asked.shape, asked.version, mail);
    if (reason.empty() && (asked.subject.empty() || asked.subject.size() > 256))
        reason = "the observation must name its subject";
    const std::string office(mail.authored_role());
    if (reason.empty()) {
        std::erase_if(leases_, [&](const ObservationLease& l) { return l.office == office && l.pane == asked.pane; });
        if (leases_.size() >= kMaxObservationLeases)
            reason = "too many observations are active; pause one first";
    }
    if (!reason.empty()) { (void)mail.answer(PaneObservationAnswered{false, reason, 0}); return; }
    leases_.push_back({++next_lease_, mail.sender(), office, asked.pane, asked.role, asked.shape,
                       asked.subject, asked.version, gesture_actor_});
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
    const auto* pane = session_.panels.runtime.find(office, asked.pane);
    if (!host_->role_holder || host_->role_holder(office) != mail.sender()) {
        reason = "the observing pane's office changed hands";
    } else if (asked.subject != it->subject) {
        reason = "the observation's subject changed; start it again";
    } else if (!pane || !session_.panels.has(pane->kind)) {
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

void WorkshopWeave::on(const PaneObservationEnded& asked, loom::Mail& mail) {
    const std::string office(mail.authored_role());
    std::erase_if(leases_, [&](const ObservationLease& l) {
        return l.id == asked.lease && l.office == office && l.pane == asked.pane && l.holder == mail.sender();
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
    const auto* pane = session_.panels.runtime.find(mail.authored_role(), asked.pane);
    if (!pane || !session_.panels.has(pane->kind) || !gesture_actor_.known ||
        approved_operation_.pane_owner != mail.sender() ||
        approved_operation_.pane != asked.pane ||
        approved_operation_.correlation != mail.correlation() ||
        approved_operation_.gesture != gestures_) {
        (void)mail.answer(PaneCarryAnswered{false, "the approved acquisition is no longer current"});
        return;
    }
    approved_operation_ = {};
    if (drag && value_drag_.gesture != gestures_) {
        (void)mail.answer(PaneCarryAnswered{false, "this value drag no longer has its primary press"});
        return;
    }
    if (asked.data.empty() || asked.data.size() > 65536 || asked.label.size() > 128) {
        (void)mail.answer(PaneCarryAnswered{false, "the carried reference exceeds its limits"});
        return;
    }
    if (!carried_.data.empty()) {
        (void)mail.answer(PaneCarryAnswered{false, "another item is already being carried"}); return;
    }
    carried_ = {asked.data, asked.label, gesture_actor_, value, drag,
                std::string(mail.authored_role()), asked.pane, std::move(token)};
    if (!drag) say("Carrying " + asked.label + " — click a receiving pane; Escape cancels", false);
    (void)mail.answer(PaneCarryAnswered{true, {}});
    if (drag && value_drag_.released) finish_value_drag(mail);
    repaint(mail);
}

bool WorkshopWeave::drop_carry(std::int64_t kind, const ExternalPressAt& at, loom::Mail& mail,
                               std::int64_t picture) {
    if (carried_.data.empty()) return false;
    if (!input_actor_.known || input_actor_.local != carried_.actor.local ||
        input_actor_.participant != carried_.actor.participant) {
        say("Only the actor carrying this reference may place it", true);
        return true;
    }
    const auto* pane = session_.panels.runtime.of_kind(kind);
    const auto* presentation = session_.panels.external_pane(kind);
    const bool origin_drop = pane && carried_.value && host_->holder_accepts &&
        host_->holder_accepts(pane->provider, *loom::schema_of<v2::PaneValueDrop>());
    if (!at.named || !pane || !presentation || presentation->canvas.grant != 0 || !host_->holder_accepts ||
        (!origin_drop && !host_->holder_accepts(pane->provider, carried_.value ? *loom::schema_of<PaneValueDrop>()
                                                                             : *loom::schema_of<PaneDrop>()))) {
        say("This place does not accept the carried item; Escape cancels", true);
        return true;
    }
    const auto correlation = ++escape_asks_;
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
    press_sent_ = EscapeSent{kind, gestures_, correlation};
    session_.panels.selected = kind;
    session_.panels.keyboard = kind;
    note_routed(kind);
    say(std::string(value ? "Value" : "Reference") + " sent to " + pane->name, false);
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
    if (!drag.gesture || drag.gesture != gestures_ || drag.released || !input_actor_.known ||
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
    if (button.button != 1 || !drag.gesture || drag.gesture != gestures_ || drag.released ||
        !input_actor_.known || input_actor_.local != drag.actor.local ||
        input_actor_.participant != drag.actor.participant) return false;
    // The release is retained even when it precedes the asynchronous acquisition answer.
    drag.released = true;
    if (button.space == drag.space && !session_.arrange.open && !session_.context.open && !session_.presented.open) {
        const auto point = canvas_point_of(button.space, button.x, button.y);
        const auto owner = occupied_at(session_.panels, session_.setup.active, screen_of(session_), point);
        if (point.understood && owner.occupied && is_runtime_kind(owner.kind)) {
            drag.target = owner.kind;
            drag.at = external_press_at(session_.panels, session_.setup.active, screen_of(session_),
                owner.kind, session_.pane_titles, button.space, button.x, button.y);
            const auto* pane = session_.panels.runtime.of_kind(owner.kind);
            const auto* presentation = session_.panels.external_pane(owner.kind);
            if (pane && presentation && host_->role_holder) {
                drag.receiver = host_->role_holder(pane->provider);
                drag.picture = presentation->stamp.aimed;
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
    const auto* pane = session_.panels.runtime.of_kind(drag.target);
    if (drag.gesture != gestures_ || !pane || !drag.receiver.valid() || !host_->role_holder ||
        host_->role_holder(pane->provider) != drag.receiver) {
        carried_ = {}; say("Value drag cancelled: no current receiver at the release", true); return;
    }
    const auto previous = input_actor_;
    input_actor_ = drag.actor;
    (void)drop_carry(drag.target, drag.at, mail, drag.picture);
    input_actor_ = previous;
    if (!carried_.data.empty()) {
        carried_ = {};
        say("Value was not placed: " + session_.notice, true);
    }
}
} // namespace zengine::workshop
