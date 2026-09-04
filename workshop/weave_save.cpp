// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The bodies of `weave.hpp`'s sections -- save and open -- compiled once into
// `zengine-workshop-logic` and linked by the host and every suite; the declarations, the
// constants and the constexpr functions stay in the header.
// Workshop law: agents/workshop/contextual.md (+14 registers; agents/workshop.md routes)

#include "weave.hpp"

namespace zengine::workshop {

// ---- Save and open -------------------------------------------------------

// WL-DOC-16 -- agents/workshop/document-file.md
void WorkshopWeave::save_document() {
    if (host_->document_path.empty()) {
        say(kNoDocumentFile, true);
        return;
    }
    const Row* draft = editing_row();
    if (draft != nullptr) {
        say(draft->label() + " is still being edited -- " + hotkey(Act::kDraftCommit) +
                " commits, " + hotkey(Act::kDraftCancel) + " cancels; nothing was saved",
            true);
        return;
    }
    const Written written = persist::save_file(host_->document_path, state_);
    if (!written.accepted) {
        say(written.refusal, true);
        return;
    }
    // What is on disk is now what is in memory. Recorded as a COPY of the
    // document rather than as a flag, so "saved" cannot drift from the truth
    // it describes -- see WorkshopDoc's operator==.
    saved_ = state_;
    say("saved " + host_->document_path, false);
}

// WL-CTX-01 -- agents/workshop/contextual.md; WL-DOC-16 -- agents/workshop/document-file.md
void WorkshopWeave::load_document() {
    if (host_->document_path.empty()) {
        say(kNoDocumentFile, true);
        return;
    }
    const Written read = persist::load_file(host_->document_path, state_);
    if (!read.accepted) {
        // The document, the selection, the drag and any draft are all
        // exactly as they were. A failed load costs a maker nothing but the
        // notice.
        say(read.refusal, true);
        return;
    }
    end_drag(session_);
    open_on_first();
    // A DOCUMENT REPLACEMENT IS THE ONE PATH an old object identity can come to
    // alias a different object -- the file restores the mint -- so a captured
    // contextual subject from the old document is dropped at this door, exactly as
    // the selection is re-established rather than preserved. A room or pane
    // subject names nothing the replacement touched and stands.
    if (session_.context.subject == context_subject::kObject) {
        session_.context = ContextMenu{};
    }
    saved_ = state_;
    say("loaded " + host_->document_path + " -- " + std::to_string(state_.elements.size()) +
            " objects",
        false);
}

void WorkshopWeave::open_on_first() {
    session_.selected = state_.elements.empty() ? 0 : state_.elements.front().id;
    rebuild_rows();
}

void WorkshopWeave::create_object() {
    const std::int64_t id = create(state_, session_);
    if (id == 0) {
        // The mint is spent. Unreachable by pressing `n`; reachable in one
        // line of a loaded file, which is why this gesture has an answer
        // rather than an overflow.
        say("this document has no identity left to give -- nothing was created", true);
        return;
    }
    say("created #" + std::to_string(id) + " -- a new identity, not a new name", false);
}

// WL-DOC-10 -- agents/workshop/document.md
std::string WorkshopWeave::deleted_notice(std::int64_t was) const {
    if (session_.selected == 0) {
        return "deleted #" + std::to_string(was) + " -- the document is empty";
    }
    return "deleted #" + std::to_string(was) + " -- now on #" +
           std::to_string(session_.selected);
}

void WorkshopWeave::delete_object() {
    const std::int64_t was = session_.selected;
    const Written gone = delete_selected(state_, session_);
    if (!gone.accepted) {
        say(gone.refusal, true);
        return;
    }
    say(deleted_notice(was), false);
}

// WL-CTX-07 -- agents/workshop/contextual.md
Written WorkshopWeave::delete_object_at(std::int64_t id) {
    if (id == session_.selected) {
        return delete_selected(state_, session_);
    }
    const Written removed = doc::remove(state_, id);
    if (removed.accepted) {
        rebuild_rows();
    }
    return removed;
}

// WL-CTX-07 -- agents/workshop/contextual.md
void WorkshopWeave::context_delete_object(std::int64_t id) {
    if (draft_live(session_)) {
        say(finish_draft_first(), true);
        return;
    }
    const bool was_selected = id == session_.selected;
    const Written gone = delete_object_at(id);
    if (!gone.accepted) {
        say(gone.refusal, true);
        return;
    }
    // The selection moved only when the deleted one WAS it; a deletion that touched
    // no selection must not claim one moved.
    say(was_selected ? deleted_notice(id) : "deleted #" + std::to_string(id), false);
}

void WorkshopWeave::move_by(std::int64_t ddx, std::int64_t ddy) {
    const Handled moved = nudge(state_, session_, ddx, ddy);
    if (!moved.accepted()) {
        say(moved.written.refusal, true);
        return;
    }
    const ui::Element* e = doc::find(state_, session_.selected);
    if (e != nullptr) {
        say(move_notice(*e, moved), false);
    }
}

// WL-DOC-07 -- agents/workshop/document.md
void WorkshopWeave::size_by(std::int64_t dw, std::int64_t dh) {
    const Handled done = grow(state_, session_, dw, dh);
    if (!done.accepted()) {
        say(done.written.refusal, true);
        return;
    }
    const ui::Element* e = doc::find(state_, session_.selected);
    if (e != nullptr) {
        say(size_notice(*e, done), false);
    }
}

// WL-DOC-08 -- agents/workshop/document.md
std::string WorkshopWeave::edge_of(const Handled& done) {
    return done.clamped() ? " -- " + done.boundary : std::string();
}

// WL-DOC-06 -- agents/workshop/document.md
std::string WorkshopWeave::move_notice(const ui::Element& e, const Handled& done) {
    const std::string where = e.context == ui::kRootContext
                                  ? std::string()
                                  : " in #" + std::to_string(e.context);
    return "#" + std::to_string(e.id) + " is at " + std::to_string(e.x) + "," +
           std::to_string(e.y) + where + edge_of(done);
}

std::string WorkshopWeave::size_notice(const ui::Element& e, const Handled& done) {
    return "#" + std::to_string(e.id) + " is now " + TextForm<ui::Extent>::format(e.width) +
           " x " + TextForm<ui::Extent>::format(e.height) + edge_of(done);
}

bool WorkshopWeave::inspector_shown() const { return session_.panels.has(panel::kInfo); }

// WL-INFO-10 -- agents/workshop/info-body.md
bool WorkshopWeave::inspector_absent() {
    if (inspector_shown()) {
        return false;
    }
    say("the properties are not showing -- " + hotkey(Act::kPicker) +
        " opens the Info panel",
        true);
    return true;
}

void WorkshopWeave::move_cursor(std::int64_t delta) {
    if (inspector_absent()) {
        return;
    }
    if (delta < 0) {
        if (session_.cursor > 0) {
            --session_.cursor;
        }
    } else if (session_.cursor + 1 < session_.rows.size()) {
        ++session_.cursor;
    }
}

// WL-INFO-05 -- agents/workshop/info-body.md
void WorkshopWeave::begin_edit() {
    if (inspector_absent()) {
        return;
    }
    if (session_.cursor >= session_.rows.size()) {
        return;
    }
    Row& row = session_.rows[session_.cursor];
    if (!row.editable()) {
        say(row.label() + " is not authored -- it is what the workspace makes of the "
                          "authored value",
            true);
        return;
    }
    row.begin();
    say("editing " + row.label() + " -- " + hotkey(Act::kDraftCommit) + " commits, " +
            hotkey(Act::kDraftCancel) + " cancels",
        false);
}

void WorkshopWeave::resize_workspace(std::int64_t delta) {
    // The ceiling is THIS SCREEN'S room, not a constant: a surface can offer
    // more of it, and a `]` that stopped at 48 cells on a window with room for eighty
    // would be the tool refusing space it had already been given.
    const std::int64_t room = screen_of(session_).room_w;
    std::int64_t want = session_.workspace_w + delta;
    if (want < kWorkspaceMinW) {
        want = kWorkspaceMinW;
    }
    if (want > room) {
        want = room;
    }
    session_.workspace_w = want;
    rebuild_rows(); // the resolved row closes over the extent it resolves against
    say("workspace is now " + std::to_string(want) +
            " cells wide -- authored values unchanged",
        false);
}

void WorkshopWeave::select_next() {
    if (state_.elements.empty()) {
        return;
    }
    std::size_t at = 0;
    for (std::size_t i = 0; i < state_.elements.size(); ++i) {
        if (state_.elements[i].id == session_.selected) {
            at = i;
            break;
        }
    }
    select(state_.elements[(at + 1) % state_.elements.size()].id);
}

void WorkshopWeave::select(std::int64_t id) {
    if (id == session_.selected) {
        return;
    }
    session_.selected = id;
    rebuild_rows();
}

// WL-INFO-06 -- agents/workshop/info-body.md
void WorkshopWeave::rebuild_rows() { refocus(state_, session_); }

// WL-TEXT-03 -- agents/workshop/text-box.md
void WorkshopWeave::refresh_setup_name() {
    if (!session_.setup.naming.open) {
        return;
    }
    session_.setup.naming.line.keep_caret_visible(
        setup_name_columns(session_, screen_of(session_)));
}

void WorkshopWeave::say(std::string text, bool bad) {
    session_.notice = std::move(text);
    session_.notice_is_bad = bad;
}

// WL-DOC-19 -- agents/workshop/document-file.md
std::string WorkshopWeave::status_line() const {
    std::string line =
        "[workshop] " + std::to_string(state_.elements.size()) + " objects | " +
        (session_.selected == 0 ? std::string("nothing selected")
                                : "selected #" + std::to_string(session_.selected));
    if (!host_->document_path.empty()) {
        line += " | " + host_->document_path + (state_ == saved_ ? " saved" : " UNSAVED");
    }
    return line;
}

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
        pane->clear_refusal();
        pane->heard = false;
        pane->awaiting = true;
        // DELIBERATELY AUTHORED AS `zengine.workshop` AND ADDRESSED TO THE OFFICE THE
        // DESCRIPTOR CAME IN UNDER. The authorship is what lets the provider verify the
        // ask (its side refuses a room from anyone else); the destination is a ROLE
        // rather than a WeaveId, so a provider that was replaced still gets its room.
        (void)mail.as_role(kWorkshopProvider)
            .send_to_role(office, PaneRoom{key, body.rows, body.columns});
    }
}

// WL-PRESS-04 -- agents/workshop/press-chain.md
void WorkshopWeave::external_press(std::int64_t kind, const zengine::input::PointerButton& b,
                                   loom::Mail& mail) {
    const ExternalPressAt at =
        external_press_at(session_.panels, session_.setup.active, screen_of(session_), kind,
                          session_.pane_titles, b.space, b.x, b.y);
    if (!at.named) {
        return;
    }
    const RuntimePane* row = session_.panels.runtime.of_kind(kind);
    const ExternalPane* pane = session_.panels.external_pane(kind);
    // A ROOM THIS PANE HAS NOT BEEN GRANTED HAS NO LATTICE TO NAME A PLACE IN. `granted`
    // is false for exactly one beat -- between a panel opening and the repaint that
    // grants it -- and a press in that beat would be a position in a room the provider
    // has never been told about.
    if (row == nullptr || pane == nullptr || !pane->granted) {
        return;
    }
    (void)mail.as_role(kWorkshopProvider)
        .send_to_role(row->provider, PanePressed{row->pane, at.row, at.column});
}

// WL-FOCUS-01, WL-FOCUS-05 -- agents/workshop/focus.md
std::int64_t WorkshopWeave::keyboard_pane() const {
    return zengine::workshop::keyboard_pane(session_.panels);
}

void WorkshopWeave::external_key(std::int64_t kind, const zengine::input::KeyPressed& k,
                                 loom::Mail& mail) {
    const RuntimePane* row = session_.panels.runtime.of_kind(kind);
    if (row == nullptr) {
        return;
    }
    (void)mail.as_role(kWorkshopProvider)
        .send_to_role(row->provider, PaneKey{row->pane, k.scancode, k.modifiers});
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
}

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
