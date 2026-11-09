// SPDX-License-Identifier: AGPL-3.0-only
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

// WL-ATTN-02 -- agents/workshop/attention.md; WL-NOTE-01 -- agents/workshop/notices.md
void WorkshopWeave::say(std::string text, bool bad) {
    ++notices_said_;
    session_.notice = text;
    session_.notice_is_bad = bad;
    NoticeSaid said{static_cast<std::int64_t>(notices_said_), std::move(text), bad};
    if (notices_owed_.size() >= kMaxNoticesOwed) {
        notices_owed_.erase(notices_owed_.begin());
    }
    notices_owed_.push_back(std::move(said));
}

// A SEATED PANE'S REFUSAL, SAID WHOLE. The pane's row paints the same sentence cut at its width;
// this is the notice the record keeps, and the one a reading of the refusal is.
void WorkshopWeave::on(const PaneRefusalRequested& asked, loom::Mail& mail) {
    const std::string role(mail.authored_role());
    if (asked.text.empty() || role.empty() || !host_->role_holder ||
        host_->role_holder(role) != mail.sender()) {
        return;
    }
    bool seated = false;
    for (const RuntimePane& row : session_.panes.runtime.entries) {
        if (row.provider == role && session_.panes.has(row.kind)) {
            seated = true;
            break;
        }
    }
    if (!seated) {
        return;
    }
    say(asked.text, true);
}

// WL-NOTE-01 -- agents/workshop/notices.md
void WorkshopWeave::publish_notices(loom::Mail& mail) {
    std::vector<NoticeSaid> said;
    said.swap(notices_owed_);
    for (const NoticeSaid& notice : said) {
        (void)mail.as_role(kWorkshopProvider).publish(notice);
    }
}

} // namespace zengine::workshop
