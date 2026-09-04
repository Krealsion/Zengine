// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The bodies of `weave.hpp`'s section -- the run: the frontier and the clock read alive, the
// repaint that publishes the slots and then the picture, the one quit that writes the desk on the
// way out, and the draft-first sentence -- compiled once into `zengine-workshop-logic` and linked
// by the host and every suite; the declarations, the constants and the constexpr functions stay
// in the header.
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
    refresh_terminal();  // the pane is a snapshot, and a snapshot is only true when taken
    refresh_inspector(); // and a draft's window is only true against the room it has now
    refresh_setup_name(); // ...and so is the name editor's, against the same room
    refresh_pane_name();  // ...and the Pane Creator's name prompt, against its heading
    refresh_editor();     // ...and the source viewport, against the body it has now
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
        zengine::surface::kSlotScore, attention_compact(attention_shown(session_, frontier))});
    mail.publish(paint(state_, session_, frontier));
}

// WL-EDIT-03 -- agents/workshop/editor.md
// WL-MAKER-08 -- agents/workshop/maker-pane.md
// WL-SESSION-13 -- agents/workshop/session.md
void WorkshopWeave::quit() {
    // THE UNSAVED-LOSS FLOOR AT THE ONE EXIT: dirty source may leave this process
    // only by the maker's own deliberate act. All three arrival doors -- `q`, the
    // ctrl chord, a native close box -- meet the same refusal, which names the two
    // real ways out; there is no confirmation surface and no armed second press,
    // because a maker who has saved or discarded simply quits. The OBJECT document
    // keeps its recorded policy (its UNSAVED marker is its statement); the source
    // buffer is the one draft in this application whose loss is a file's worth of
    // work, which is why it alone holds the door.
    if (session_.editor.dirty()) {
        say("the source editor has unsaved changes -- " + hotkey(Act::kEditorSave) +
                " in the editor saves them, " + hotkey(Act::kEditorDiscard) +
                " discards them; Workshop stays open",
            true);
        return;
    }
    // AND A MAKER-MADE PANE HOLDS THE DOOR THE SAME WAY: a definition that
    // differs from its file is a maker's authored truth, and it may leave this process
    // only by their own save or their own discard.
    if (session_.panels.maker.dirty()) {
        say(maker_pane_dirty_sentence("Workshop stays open"), true);
        return;
    }
    save_last_session();
    host_->quit = true;
    if (host_->request_stop) {
        host_->request_stop();
    }
}

// WL-CTX-07 -- agents/workshop/contextual.md; WL-CTRL-03 -- agents/workshop/info-controls.md
std::string WorkshopWeave::finish_draft_first() const {
    return "finish the draft first -- " + hotkey(Act::kDraftCommit) + " commits it, " +
           hotkey(Act::kDraftCancel) + " cancels";
}

} // namespace zengine::workshop
