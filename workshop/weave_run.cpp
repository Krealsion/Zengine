// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// `WorkshopWeave`'s run: the frontier and the clock read alive, the repaint that publishes the
// slots and then the picture, and the one quit that asks the room.
// Workshop law: agents/workshop/attention.md (+5 registers; agents/workshop.md routes)

#include "weave.hpp"

namespace zengine::workshop {

// ---- THE RUN: what is true now, the picture published, and the way out --------------

// WL-ATTN-04 -- agents/workshop/attention.md
ProjectFrontier WorkshopWeave::frontier_now() const {
    return host_->frontier ? host_->frontier() : ProjectFrontier{};
}

std::int64_t WorkshopWeave::interaction_now() const {
    return host_->interaction_now ? host_->interaction_now() : interaction_now_ms();
}

void WorkshopWeave::repaint(loom::Mail& mail) {
    // ⚠ WHAT THE HOST HAS LEARNED SINCE THE LAST PICTURE, FIRST. The boot's own discoveries --
    // an optional plan row that refused, and so a tool that is not in this Workshop -- settle
    // inside a delivery that can arrive after the first surface did (`HostContext`). One
    // integer compared; the list is re-taken only when it moved, and `establish` is keyed.
    if (conditions_taken_ != host_->conditions_generation) {
        take_host_conditions();
    }
    refresh_setup_name(); // a name editor's window is only true against the room it has now
    refresh_external_rooms(mail); // ...and an external pane's room, against the same one
    // THE FRONTIER IS DERIVED HERE, PER PAINT, AND STORED NOWHERE. `paint` stays a
    // pure projection of what it is handed, and what it is handed is this repaint's
    // reading of the living realization owner — never a member, never a field of the
    // session, never yesterday's answer.
    const ProjectFrontier frontier = frontier_now();
    // The slots first and the picture last: the SDL medium composes the attention slot into the
    // picture it draws, so a slot published after the canvas would show a frame late.
    mail.publish(
        zengine::surface::SurfaceText{zengine::surface::kSlotStatus, status_line()});
    // What is currently true and worth a glance, on the always-visible slot: derived at every
    // repaint and held nowhere, so a resolved condition is gone because it stopped being
    // returned. Empty is the retraction.
    mail.publish(zengine::surface::SurfaceText{
        zengine::surface::kSlotScore,
        attention_compact(attention_conditions(session_, frontier))});
    // ...AND THE SAME TRUTH IN FULL, TO WHOEVER IS PRESENTING IT. The chip is a glance and
    // this is the reading behind it: one sentence per condition, in the host's own order,
    // said only when it changed.
    say_conditions(frontier, mail);
    // ...AND THE ONE PANE INVENTORY, to whoever is listing it (WL-DESK-04). Same beat, same
    // rule, same silence when nothing changed: it is derived at every gesture, so the
    // comparison inside is what keeps a launcher from repainting for nothing.
    publish_inventory(mail);
    // ...AND THE EFFECTIVE KEYMAP, to whoever presents keys (WL-DESK-11), on the same terms.
    publish_keymap(mail);
    publish_pane_subject(mail);
    // ...and what the terminal participant's record holds, to whoever is presenting it.
    // Same beat, same rule, same silence when nothing changed.
    say_transcript(mail);
    auto canvas = paint(session_);
    if (carried_.drag && value_drag_.moved && !value_drag_.released && drag_pointer_.understood) {
        surface::SurfaceLayer overlay;
        const auto rect = wire_rect_of(FineRect{drag_pointer_.sub.x, drag_pointer_.sub.y,
            12 * surface::kCellSubs, surface::kCellSubs}, surface::role::kAccent);
        overlay.rects.push_back(rect);
        overlay.labels.push_back(surface::SurfaceLabel{rect.x, rect.y, "[value]", surface::role::kFill,
                                                       rect.sub_x, rect.sub_y});
        canvas.layers.push_back(std::move(overlay));
    }
    mail.publish(std::move(canvas));
    // ...and behind the canvas, the fence that makes a newly shown picture the one a press is
    // stamped with, queued after it so no delivery comes between them.
    fence_pictures(mail);
}

// WL-ATTN-12 -- agents/workshop/attention.md
void WorkshopWeave::say_conditions(const ProjectFrontier& frontier, loom::Mail& mail) {
    std::vector<StandingCondition> now = standing_conditions(session_, frontier);
    if (conditions_said_ && same_conditions(now, said_conditions_)) {
        // NO NEWS IS SILENCE, and silence is what makes this seam terminate. A pane that
        // hears a publication says its rows; content ending in a repaint would publish
        // again; the loop has no quiet state unless this arm exists.
        return;
    }
    said_conditions_ = now;
    conditions_said_ = true;
    // `to_any`, NOT ADDRESSED, AND SAID AS THE OFFICE. Which weave presents this is the load
    // plan's business, and a host that addressed one would be a host with a pane compiled
    // into it again -- but a reader still has to be able to tell this host's reading from
    // any weave's opinion, so the publication carries the office stamp and the pane refuses
    // an unstamped one. `as_role` adds provenance, never a capability (MSG-07).
    (void)mail.as_role(kWorkshopProvider).publish(StandingConditions{std::move(now)});
}

// WL-EDIT-03 -- agents/workshop/editor.md
// WL-MAKER-08 -- agents/workshop/maker-pane.md
// WL-SESSION-13 -- agents/workshop/session.md
void WorkshopWeave::quit(loom::Mail& mail) {
    if (quitting_) {
        say("quitting -- waiting for " + std::to_string(quit_outstanding_) +
                " pane(s) to answer",
            true);
        return;
    }
    // A maker-made pane holds the door synchronously: a definition that differs from its file is
    // authored truth this host holds, and it leaves only by the maker's save or discard.
    if (session_.panels.maker.dirty()) {
        say(maker_pane_dirty_sentence("Workshop stays open"), true);
        return;
    }
    // The unsaved-loss floor at the one exit, asked of the room: every pane holding a maker's
    // unsaved work accepts the question, and Loom's fan-out count is how many answers are owed
    // (zero: the exit proceeds now). Gestures are held until the last answer (`hold_input`). An
    // answer that can never come does not hold the hands: a refused delivery is written in the
    // host's book and refuses the quit (`quit_delivery.hpp`). The ask's number is minted by the
    // paste book, so an answer to it can never settle a paste.
    quit_ask_ = paste_asks_.mint_correlation();
    const loom::OfficePublication asked =
        mail.as_role(kWorkshopProvider).publish(PaneQuitRequested{}, quit_ask_);
    if (!asked.authored) {
        // THIS WEAVE DOES NOT HOLD ITS OWN OFFICE, so it cannot ask and cannot know. A host
        // composed that way is a defect worth reading, not a reason to lose a maker's work.
        say("Workshop could not ask its panes whether they hold unsaved work -- it does not "
            "hold its own office; Workshop stays open",
            true);
        return;
    }
    if (asked.recipients == 0) {
        finish_quit();
        return;
    }
    quitting_ = true;
    quit_outstanding_ = asked.recipients;
    quit_refusals_.clear();
    held_input_.clear();
    held_dropped_ = 0;
}

// WL-SESSION-13 -- agents/workshop/session.md
void WorkshopWeave::finish_quit() {
    quitting_ = false;
    held_input_.clear();
    save_last_session();
    host_->quit = true;
    if (host_->request_stop) {
        host_->request_stop();
    }
}

// WL-EDIT-03 -- agents/workshop/editor.md
void WorkshopWeave::on(const PaneQuitAnswered& said, loom::Mail& mail) {
    if (!mail.answers_ask() || !quitting_ || mail.correlation() != quit_ask_) {
        return; // not Loom's answer to the ask this host is waiting on
    }
    if (!said.permitted) {
        quit_refusals_.push_back(said.refusal.empty()
                                     ? said.pane + " refused the quit and gave no reason"
                                     : said.refusal);
    }
    if (quit_outstanding_ > 0) {
        --quit_outstanding_;
    }
    if (quit_outstanding_ > 0) {
        return; // more answers are owed, and the decision waits for the last of them
    }
    if (quit_refusals_.empty()) {
        quitting_ = false;
        finish_quit();
        return;
    }
    std::string why;
    for (const std::string& one : quit_refusals_) {
        if (!why.empty()) {
            why += "; ";
        }
        why += one;
    }
    refuse_quit(std::move(why), mail);
}

// WL-SESSION-19 -- agents/workshop/session.md
void WorkshopWeave::on(const QuitDeliveryRefusalNoted&, loom::Mail& mail) {
    // THE BOOK IS TAKEN WHOLE, EVERY TIME, so nothing in it outlives the wake-up that followed it:
    // an entry for an ask that already ended -- refused, or answered -- is discarded here, and
    // one written while no quit is in flight means nothing to this weave.
    const std::vector<UndeliveredQuit> noted = host_->undelivered_quits.take();
    if (!quitting_) {
        return;
    }
    std::string undelivered;
    for (const UndeliveredQuit& one : noted) {
        // THE ASK IS THIS WEAVE'S OWN NUMBER (`paste_asks_.mint_correlation`), and the watch took
        // the entry only from a refusal the bus stamped as this weave's -- so an equal number is
        // this quit's delivery and nothing else's. An older quit's late fact carries an older one.
        if (one.ask != quit_ask_) {
            continue;
        }
        undelivered += (undelivered.empty() ? "the quit could not ask " : "; nor ") +
                       undelivered_quit_words(one);
    }
    if (undelivered.empty()) {
        return;
    }
    // ONE KNOWN REFUSAL MAKES THIS QUIT IMPOSSIBLE, so it ends now: an answer still owed by
    // another pane cannot turn it back into an exit, and waiting for it would hold the maker's
    // hands for nothing. What panes already refused is said too, in their own words.
    std::string why = undelivered + "; Workshop stays open, and a quit after the repair asks again";
    for (const std::string& one : quit_refusals_) {
        why += "; " + one;
    }
    refuse_quit(std::move(why), mail);
}

// WL-SESSION-19 -- agents/workshop/session.md
void WorkshopWeave::refuse_quit(std::string why, loom::Mail& mail) {
    quitting_ = false;
    quit_ask_ = 0;
    quit_outstanding_ = 0;
    quit_refusals_.clear();
    if (held_dropped_ > 0) {
        why += " (" + std::to_string(held_dropped_) +
               " gesture(s) that arrived while the quit was pending were dropped)";
    }
    say(std::move(why), true);
    // AND THE MAKER'S HANDS GET BACK WHAT THEY DID MEANWHILE, in order, through the same
    // handlers -- a refused quit costs no keystroke.
    replay_held(mail);
    repaint(mail);
}

bool WorkshopWeave::hold_input(HeldInput held) {
    if (!quitting_) {
        return false;
    }
    if (held_input_.size() >= kMaxHeldInput) {
        ++held_dropped_;
        return true;
    }
    held.actor = input_actor_;
    held_input_.push_back(std::move(held));
    return true;
}

void WorkshopWeave::replay_held(loom::Mail& mail) {
    std::vector<HeldInput> replay;
    replay.swap(held_input_);
    held_dropped_ = 0;
    struct RestoreActor {
        InputActor& current;
        InputActor before;
        ~RestoreActor() { current = before; }
    } restore{input_actor_, input_actor_};
    for (const HeldInput& held : replay) {
        input_actor_ = held.actor;
        switch (held.kind) {
        case HeldInput::Kind::kKey: on(held.key, mail); break;
        case HeldInput::Kind::kText: on(held.text, mail); break;
        case HeldInput::Kind::kButton: on(held.button, mail); break;
        case HeldInput::Kind::kMoved: on(held.moved, mail); break;
        case HeldInput::Kind::kWheel: on(held.wheel, mail); break;
        }
    }
}

} // namespace zengine::workshop
