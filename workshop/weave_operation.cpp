// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#include "weave.hpp"
#include <limits>

namespace zengine::workshop {
bool WorkshopWeave::duplicate_input(const loom::Mail& mail) const {
    return !applying_attributed_ && attributed_producer_.valid() &&
           mail.sender() == attributed_producer_;
}

void WorkshopWeave::on(const input::AttributedInput& event, loom::Mail& mail) {
    if (!mail.authored_from_role(input::kInputRole) ||
        (event.local ? event.actor != 0 : event.actor <= 0)) return;
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
    if (e.kind == "KeyPressed") on(input::KeyPressed{e.scancode, e.name, e.modifiers}, mail);
    else if (e.kind == "TextEntered") on(input::TextEntered{e.text}, mail);
    else if (e.kind == "PointerButton")
        on(input::PointerButton{e.button, e.pressed, e.x, e.y, e.space, e.modifiers}, mail);
    else if (e.kind == "PointerMoved")
        on(input::PointerMoved{e.x, e.y, e.dx, e.dy, e.space, e.modifiers}, mail);
    else if (e.kind == "PointerWheel")
        on(input::PointerWheel{e.wheel_dx, e.wheel_dy, e.x, e.y, e.space, e.modifiers}, mail);

}

void WorkshopWeave::on(const PaneOperationRequested& asked, loom::Mail& mail) {
    const auto* pane = session_.panels.runtime.find(mail.authored_role(), asked.pane);
    const auto refuse = [&](const char* reason) {
        (void)mail.answer(PaneOperationAnswered{false, reason});
    };
    if (mail.authored_role().empty() || !pane || !session_.panels.has(pane->kind) ||
        !host_->role_holder || host_->role_holder(mail.authored_role()) != mail.sender()) {
        refuse("the requesting pane is no longer on this desk");
        return;
    }
    const auto corr = asked.gesture > 0 ? static_cast<std::uint64_t>(asked.gesture) : 0;
    bool current = false;
    for (auto* sent : {&action_sent_, &press_sent_}) {
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
        refuse("this operation needs a current attributed input gesture");
        return;
    }
    if (asked.role.empty() || asked.shape.empty() || asked.version <= 0 ||
        asked.version > std::numeric_limits<std::uint32_t>::max()) {
        refuse("the operation must name its destination and versioned shape");
        return;
    }
    if (!gesture_actor_.local) {
        const auto authority = host_->input_authority
            ? host_->input_authority(gesture_actor_.participant) : loom::GrantAuthority{};
        const auto live = mail.describe_authority(authority);
        if (!live.available || !live.permits_role(asked.shape,
                static_cast<std::uint32_t>(asked.version), asked.role)) {
            refuse("the input actor has no authority for this operation");
            return;
        }
    }
    approved_operation_ = {mail.sender(), asked.pane, corr, gestures_};
    (void)mail.answer(PaneOperationAnswered{true, {}});
}
void WorkshopWeave::on(const PaneCarryRequested& asked, loom::Mail& mail) {
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
    if (asked.data.empty() || asked.data.size() > 65536 || asked.label.size() > 128) {
        (void)mail.answer(PaneCarryAnswered{false, "the carried reference exceeds its limits"});
        return;
    }
    carried_ = {asked.data, asked.label, gesture_actor_};
    say("Carrying " + asked.label + " — click a receiving pane; Escape cancels", false);
    (void)mail.answer(PaneCarryAnswered{true, {}});
    repaint(mail);
}

bool WorkshopWeave::drop_carry(std::int64_t kind, const ExternalPressAt& at, loom::Mail& mail) {
    if (carried_.data.empty()) return false;
    if (!input_actor_.known || input_actor_.local != carried_.actor.local ||
        input_actor_.participant != carried_.actor.participant) {
        say("Only the actor carrying this reference may place it", true);
        return true;
    }
    const auto* pane = session_.panels.runtime.of_kind(kind);
    const auto* presentation = session_.panels.external_pane(kind);
    if (!at.named || !pane || !presentation || presentation->canvas.grant != 0 || !host_->holder_accepts ||
        !host_->holder_accepts(pane->provider, *loom::schema_of<PaneDrop>())) {
        say("This place does not accept a carried reference; Escape cancels", true);
        return true;
    }
    const auto correlation = ++escape_asks_;
    (void)mail.as_role(kWorkshopProvider).send_to_role(
        pane->provider, PaneDrop{pane->pane, std::move(carried_.data), at.row, at.column,
                                 presentation ? presentation->stamp.aimed : 0}, correlation);
    carried_ = {};
    press_sent_ = EscapeSent{kind, gestures_, correlation};
    session_.panels.selected = kind;
    session_.panels.keyboard = kind;
    note_routed(kind);
    say("Reference sent to " + pane->name, false);
    return true;
}
} // namespace zengine::workshop
