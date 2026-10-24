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
    ++notices_said_;
    stamps_owed_ = desk_owed_ = true; // the band's words are the desk's, said with or without a repaint
}

} // namespace zengine::workshop
