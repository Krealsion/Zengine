// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// A command run by name: the palette's door (`command_vocabulary.hpp`). The palette finds; it
// grants nothing. Each command is performed by the path its own key or press takes, as the hand
// that pressed the palette's row, and judged where that path is judged.
// Workshop law: agents/workshop/desktop.md (+1 register; agents/workshop.md routes)

#include "weave.hpp"

#include <memory>

namespace zengine::workshop {

// WL-DESK-18 -- agents/workshop/desktop.md
void WorkshopWeave::on(const CommandRunRequested& asked, loom::Mail& mail) {
    // FROM THE PALETTE'S OFFICE, AS ITS HOLDER, and from nobody else.
    if (mail.authored_role() != kPaletteRole || !host_->role_holder ||
        host_->role_holder(kPaletteRole) != mail.sender()) {
        return;
    }
    // ANSWERED ONCE WHAT IT RAN HAS: a pane's seat is the layouts' store's to answer first.
    auto held = std::make_shared<loom::DeferredAnswer>(mail.defer_answer());
    const auto answer_in = [this, held](const std::string& refusal, loom::Mail& m) {
        if (!refusal.empty()) {
            say(refusal, true);
        }
        (void)loom::answer_deferred(*held, m, CommandRunAnswered{refusal.empty(), refusal});
        repaint(m);
    };
    const auto answer = [&](const std::string& refusal) { answer_in(refusal, mail); };
    const PaneRef palette{kPaletteRole, kPalettePane};
    const std::optional<std::int64_t> palette_kind = resolve_pane(palette, session_.panes);
    // UNDER THE PALETTE'S OWN ROW, the one this Workshop sent it for a hand's key, still that
    // hand's latest gesture and unspent: the run is that press's, and spends it.
    const std::uint64_t corr = mail.correlation();
    Hand* by = nullptr;
    each_hand([&](Hand& h) {
        if (corr != 0 && palette_kind.has_value() && h.action_sent.answering == corr &&
            h.action_sent.kind == *palette_kind && h.action_sent.gesture == h.latest) {
            by = &h;
            h.action_sent = GestureSent{};
        }
    });
    if (by == nullptr) {
        answer("a command runs only under the palette's own row, as its hand's latest key");
        return;
    }
    drop_kept(corr);
    // AS THAT HAND: whatever the path reads as the hand at work is this one, until the run ends.
    struct RestoreInput {
        InputActor& actor;
        InputActor before;
        ~RestoreInput() { actor = before; }
    } restore{input_actor_, input_actor_};
    input_actor_ = by->actor;
    // THE PALETTE IS PUT DOWN FIRST, the keys going back to the pane it opened over, so the
    // command acts where the hand was.
    if (has_pane(session_.setup.active, palette)) {
        close_pane(palette,
                   [](const PaneCloseAnswered&, const std::vector<PaneSetting>&, loom::Mail&) {},
                   mail);
    }
    if (asked.kind == kCommandClose) {
        answer(std::string());
        return;
    }
    if (asked.kind == kCommandPane) {
        const PaneRef ref{asked.office, asked.pane};
        const std::optional<std::int64_t> kind = resolve_pane(ref, session_.panes);
        if (!kind.has_value()) {
            answer(asked.pane + " @" + asked.office + " is not a pane this Workshop has");
            return;
        }
        if (const std::string why = refused_toward(by->actor, *kind); !why.empty()) {
            answer(why);
            return;
        }
        launch_pane(ref,
                    [this, ref, answer_in](const PaneLaunchAnswered& shown, loom::Mail& m) {
                        if (shown.refusal.empty() && shown.opened) {
                            say("showed " + inventory_name(ref), false);
                        }
                        answer_in(shown.refusal, m);
                    },
                    mail);
        return;
    }
    if (asked.kind != kCommandAction) {
        answer("`" + asked.kind + "` is not a kind of command -- an action, a pane or close");
        return;
    }
    // WORKSHOP'S OWN ROW: the keys are Workshop's, as `focus.leave` gives them, and the row is
    // dispatched by its key in force, as that key in command mode would be.
    if (asked.office == kWorkshopProvider) {
        const ActionRow* row = row_of_id(asked.id);
        if (row == nullptr) {
            answer("`" + asked.id + "` is not an action of Workshop's");
            return;
        }
        Gesture key;
        for (const Gesture& g : session_.keymap.row_gestures(*row)) {
            if (is_bound(g) && !is_bound(key)) {
                key = g;
            }
        }
        if (!is_bound(key) ||
            session_.keymap.action_for(KeyContext::kCommand, key.scancode, key.modifiers) !=
                row->act) {
            answer("`" + asked.id + "` answers to no key in command mode here");
            return;
        }
        // ...and stay Workshop's when the palette goes down after: it gives back no keys it no
        // longer holds the claim to.
        session_.panes.keyboard = kNoPaneKind;
        palette_over_ = kNoPaneKind;
        command(input::KeyPressed{key.scancode, std::string(), key.modifiers}, mail);
        answer(std::string());
        return;
    }
    // AN APPLICATION ROW, asked of the desktop as its key asks it, on the pane holding the keys.
    if (asked.pane.empty()) {
        const AppRow* row = session_.keymap.app_row_of_id(asked.id);
        if (row == nullptr || asked.office != kDesktopRole) {
            answer("`" + asked.id + "` is not one of the application's rows");
            return;
        }
        request_app_action(asked.id, mail, *by);
        answer(std::string());
        return;
    }
    // A PANE'S ROW: the pane seated and given the keys as a launch gives them, then the row sent
    // under a number recorded as this hand's act, so the pane's next ask is approved as for a key.
    const PaneRef ref{asked.office, asked.pane};
    const RuntimePane* pane = session_.panes.runtime.find(asked.office, asked.pane);
    const auto* rows = pane != nullptr ? session_.keymap.pane_rows(pane->kind) : nullptr;
    bool declared = false;
    if (rows != nullptr) {
        for (const auto& row : rows->rows) {
            declared = declared || row.id == asked.id;
        }
    }
    if (pane == nullptr || !declared) {
        answer("`" + asked.id + "` is not a row in force of " + inventory_name(ref));
        return;
    }
    const std::int64_t kind = pane->kind;
    const std::string provider = pane->provider;
    if (const std::string why = refused_toward(by->actor, kind); !why.empty()) {
        answer(why);
        return;
    }
    const InputActor actor = by->actor;
    const std::string pane_key = asked.pane;
    const std::string id = asked.id;
    launch_pane(
        ref,
        [this, ref, kind, provider, actor, pane_key, id, answer_in](const PaneLaunchAnswered& shown,
                                                                    loom::Mail& m) {
            if (!shown.refusal.empty()) {
                answer_in(shown.refusal, m);
                return;
            }
            // THE HAND THAT PRESSED THE PALETTE'S ROW, found again: the row's act is its.
            Hand* hand = nullptr;
            each_hand([&](Hand& h) {
                if (h.actor.known == actor.known && h.actor.local == actor.local &&
                    h.actor.participant == actor.participant) {
                    hand = &h;
                }
            });
            if (hand == nullptr) {
                answer_in("the hand that ran it is gone -- nothing was sent", m);
                return;
            }
            const std::uint64_t answering = ++gesture_asks_;
            const loom::Ticket sent = m.as_role(kWorkshopProvider)
                                          .send_to_role(provider, PaneActionRequested{pane_key, id},
                                                        answering);
            if (!sent.valid()) {
                answer_in("the row could not be sent to " + inventory_name(ref), m);
                return;
            }
            note_routed(kind);
            keep_act(*hand, hand->action_sent = GestureSent{kind, hand->latest, answering});
            answer_in(std::string(), m);
        },
        mail);
}

// ...AND PUT DOWN, the keys going back to the pane that held them when it took them, if that pane
// is still on the desk; otherwise they stay with Workshop.
void WorkshopWeave::palette_closed(const PaneRef& ref) {
    if (ref.provider != kPaletteRole || ref.pane != kPalettePane) {
        return;
    }
    const std::int64_t over = palette_over_;
    palette_over_ = kNoPaneKind;
    if (over == kNoPaneKind || !session_.panes.has(over)) {
        return;
    }
    session_.panes.selected = over;
    session_.panes.keyboard = kind_takes_keyboard(over) ? over : kNoPaneKind;
}

} // namespace zengine::workshop
