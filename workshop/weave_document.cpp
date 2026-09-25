// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// `WorkshopWeave`'s own voice: the notice, the setup name's window, and the status line.
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
    // Which desk is live and how many panes it names: what a medium's own status line can show
    // beside the room. A desk's own saved-ness is the layout band's to say.
    const Setup& desk = session_.setup.active;
    return "[workshop] layout " + quoted_setup_name(desk.name) + " | " +
           std::to_string(desk.panes.size()) + (desk.panes.size() == 1 ? " pane" : " panes");
}

} // namespace zengine::workshop
