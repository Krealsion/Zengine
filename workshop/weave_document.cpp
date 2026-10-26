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

// WL-ATTN-02 -- agents/workshop/attention.md; WL-NOTE-01 -- agents/workshop/notices.md
void WorkshopWeave::say(std::string text, bool bad) {
    ++notices_said_;
    say_again(std::move(text), bad);
}

void WorkshopWeave::say_again(std::string text, bool bad) {
    session_.notice = text;
    session_.notice_is_bad = bad;
    NoticeSaid said{static_cast<std::int64_t>(notices_said_), std::move(text), bad};
    // SAID AGAIN BEFORE IT WAS PUBLISHED: the owed one is replaced, and goes once.
    if (!notices_owed_.empty() && notices_owed_.back().number == said.number) {
        notices_owed_.back() = std::move(said);
        return;
    }
    if (notices_owed_.size() >= kMaxNoticesOwed) {
        notices_owed_.erase(notices_owed_.begin());
    }
    notices_owed_.push_back(std::move(said));
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
