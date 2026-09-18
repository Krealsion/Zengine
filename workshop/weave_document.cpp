// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The bodies of `weave.hpp`'s section -- what the tool says: the notice, the setup name's window
// and the status line -- compiled once into `zengine-workshop-logic` and linked by the host and
// every suite; the declarations, the constants and the constexpr functions stay in the header.
//
// ⭐ THIS FILE HELD THE OBJECT DOCUMENT'S GESTURES -- make, delete, move and size an object, what
// each said, select, the inspector rows' rebuild and the workspace refit -- and they retired with
// the prototype canvas. What remains is the host's own voice.
// Workshop law: agents/workshop/attention.md (+2 registers; agents/workshop.md routes)

#include "weave.hpp"

namespace zengine::workshop {

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

// WL-DOC-23 -- agents/workshop/document-file.md
std::string WorkshopWeave::status_line() const {
    // WHICH DESK IS LIVE AND HOW MANY PANES IT NAMES -- what a medium's own status line can show
    // beside the room. It counted objects and compared the object document with its file until
    // that document retired; a desk's own saved-ness is the layout band's to say.
    const Setup& desk = session_.setup.active;
    return "[workshop] layout " + quoted_setup_name(desk.name) + " | " +
           std::to_string(desk.panes.size()) + (desk.panes.size() == 1 ? " pane" : " panes");
}

} // namespace zengine::workshop
