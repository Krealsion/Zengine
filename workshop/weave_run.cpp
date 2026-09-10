// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The bodies of `weave.hpp`'s section -- the run: the frontier and the clock read alive, the
// repaint that publishes the slots and then the picture, the one quit that asks the room and
// writes the desk on the way out, and the draft-first sentence -- compiled once into
// `zengine-workshop-logic` and linked by the host and every suite; the declarations, the
// constants and the constexpr functions stay in the header.
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
    refresh_inspector(); // and a draft's window is only true against the room it has now
    refresh_setup_name(); // ...and so is the name editor's, against the same room
    refresh_pane_name();  // ...and the Pane Creator's name prompt, against its heading
    refresh_external_rooms(mail); // ...and an external pane's room, against the same one
    // THE FRONTIER IS DERIVED HERE, PER PAINT, AND STORED NOWHERE. `paint` stays a
    // pure projection of what it is handed, and what it is handed is this repaint's
    // reading of the living realization owner — never a member, never a field of the
    // session, never yesterday's answer.
    const ProjectFrontier frontier = frontier_now();
    // THE SLOTS GO FIRST, AND THE PICTURE LAST. A slot is a line of text a
    // publisher hands the MEDIUM, and the medium owns what it makes of it — the SDL
    // medium composes the attention slot INTO the picture it draws, so a slot published
    // after the canvas would show one frame late. Ordering them ahead costs nothing
    // anywhere else (a title is set, a terminal row is written) and it is what makes
    // "the compact indicator is current" a fact rather than a race.
    mail.publish(
        zengine::surface::SurfaceText{zengine::surface::kSlotStatus, status_line()});
    // WHAT IS CURRENTLY TRUE AND WORTH A GLANCE, ON THE ONE ALWAYS-VISIBLE SLOT
    // WORKSHOP HAD NEVER SPENT. It is derived at every repaint from live owners and
    // held nowhere, exactly as the canvas is — so a condition that resolved is gone
    // from it because it stopped being returned, and NOBODY had to un-say anything.
    // EMPTY IS THE RETRACTION: a medium clears its presentation of a slot published
    // empty, which is why the disappearance needs no path of its own.
    mail.publish(zengine::surface::SurfaceText{
        zengine::surface::kSlotScore,
        attention_compact(attention_conditions(session_, frontier))});
    // ...AND THE SAME TRUTH IN FULL, TO WHOEVER IS PRESENTING IT. The chip is a glance and
    // this is the reading behind it: one sentence per condition, in the host's own order,
    // said only when it changed.
    say_conditions(frontier, mail);
    // ...AND WHAT THE OBJECT DOCUMENT LOOKS LIKE, to whoever is listing it. The workspace
    // plane below draws the same document; this is the same truth in the form a pane can
    // read, said on the same beat and by the same rule.
    say_document(mail);
    // ...and what the terminal participant's record holds, to whoever is presenting it.
    // Same beat, same rule, same silence when nothing changed.
    say_transcript(mail);
    mail.publish(paint(state_, session_));
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

// WL-DOC-20 -- agents/workshop/document.md
void WorkshopWeave::say_document(loom::Mail& mail) {
    DocumentShown now = document_shown(state_, session_);
    if (document_said_ && same_document(now, said_document_)) {
        return; // no news is silence, and silence is what makes this seam terminate
    }
    said_document_ = now;
    document_said_ = true;
    (void)mail.as_role(kWorkshopProvider).publish(std::move(now));
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
    // A MAKER-MADE PANE HOLDS THE DOOR SYNCHRONOUSLY, AS IT ALWAYS DID: a definition that
    // differs from its file is a maker's authored truth this host still holds, and it may
    // leave this process only by their own save or their own discard.
    if (session_.panels.maker.dirty()) {
        say(maker_pane_dirty_sentence("Workshop stays open"), true);
        return;
    }
    // THE UNSAVED-LOSS FLOOR AT THE ONE EXIT, ASKED OF THE ROOM. The source document used
    // to be session state this line could read; it is the Editor weave's, and any pane
    // that holds a maker's unsaved work accepts the question. All three arrival doors --
    // `q`, the ctrl chord, a native close box -- meet the same ask, the fan-out count Loom
    // hands back is exactly how many answers are owed, and zero owed is the authoritative
    // "nobody holds anything": the exit proceeds now. Otherwise the room is being asked,
    // every gesture is held until the last answer (`hold_input`), and the answer handler
    // finishes or refuses. There is no confirmation surface and no armed second press: a
    // maker who has saved or discarded simply quits.
    // ONE SEQUENCE PER ASKER (`loom::AskBook::mint_correlation`): a number the paste book is
    // not already waiting on, so an answer to this ask can never settle a paste.
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
    quitting_ = false;
    if (quit_refusals_.empty()) {
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
    if (held_dropped_ > 0) {
        why += " (" + std::to_string(held_dropped_) +
               " gesture(s) that arrived while the quit was pending were dropped)";
    }
    say(why, true);
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
    held_input_.push_back(std::move(held));
    return true;
}

void WorkshopWeave::replay_held(loom::Mail& mail) {
    std::vector<HeldInput> replay;
    replay.swap(held_input_);
    held_dropped_ = 0;
    for (const HeldInput& held : replay) {
        switch (held.kind) {
        case HeldInput::Kind::kKey: on(held.key, mail); break;
        case HeldInput::Kind::kText: on(held.text, mail); break;
        case HeldInput::Kind::kButton: on(held.button, mail); break;
        case HeldInput::Kind::kMoved: on(held.moved, mail); break;
        case HeldInput::Kind::kWheel: on(held.wheel, mail); break;
        }
    }
}

// WL-CTX-07 -- agents/workshop/contextual.md
std::string WorkshopWeave::finish_draft_first() const {
    return "finish the draft first -- " + hotkey(Act::kDraftCommit) + " commits it, " +
           hotkey(Act::kDraftCancel) + " cancels";
}

} // namespace zengine::workshop
