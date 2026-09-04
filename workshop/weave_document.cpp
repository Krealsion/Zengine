// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The bodies of `weave.hpp`'s section -- the document's gestures: the object operations (make,
// delete, move and size, and what each says), the inspector's cursor, draft and selection, the
// workspace, the notice and the status line -- compiled once into `zengine-workshop-logic` and
// linked by the host and every suite; the declarations, the constants and the constexpr functions
// stay in the header.
// Workshop law: agents/workshop/document.md (+4 registers; agents/workshop.md routes)

#include "weave.hpp"

namespace zengine::workshop {

// ---- THE DOCUMENT'S GESTURES: the objects, the inspector, and what the tool says ----

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

} // namespace zengine::workshop
