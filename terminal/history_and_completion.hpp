// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_TERMINAL_HISTORY_AND_COMPLETION_HPP
#define ZENGINE_TERMINAL_HISTORY_AND_COMPLETION_HPP

// The Terminal pane's two helps to the line: the participant's commands walked as history, and a
// recalled line locked in (WL-TERM-12, WL-TERM-13); and the one read, what could be said next,
// taken only by the intent that asked, with the list's own cursor (WL-TERM-11), whose candidate
// Enter takes once the weaver moved it (WL-TERM-21). The members are
// `TerminalPaneWeave`'s, declared in `pane.cpp`, which includes this file after the class; it
// compiles in no other place.
// Workshop law: agents/workshop/terminal-pane.md, agents/workshop/terminal.md

#include "terminal/vocabulary.hpp"

#include "workshop/terminal_seam_vocabulary.hpp"

#include "component/text_box.hpp"

#include <zen/weave.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace {

// ---- Command history: the participant's own record, walked ------------------------------

/// THE COMMANDS THE RECORD HOLDS, NEWEST FIRST. Only `command` entries -- what a
/// presentation asked this participant to run, recorded before it was parsed -- and never
/// an answer, a notice or a refusal. A run of the same command typed again and again is
/// walked as one, as its newest. Bounded by the record's own owner, so a command the
/// participant evicted is no longer here, and nothing in this pane keeps a copy of one.
inline std::vector<TerminalPaneWeave::Recallable> TerminalPaneWeave::recallable() const {
    std::vector<Recallable> out;
    for (std::size_t i = known_.entries.size(); i > 0; --i) {
        const ShownEntry& e = known_.entries[i - 1];
        if (e.kind != ws::kEntryCommand || e.text.empty()) {
            continue;
        }
        if (!out.empty() && out.back().text == e.text) {
            continue;
        }
        out.push_back(Recallable{known_.dropped + static_cast<std::int64_t>(i - 1), e.text});
    }
    return out;
}

/// ONE STEP THROUGH HISTORY: -1 older, +1 newer.
inline void TerminalPaneWeave::recall(int by, loom::Mail& mail) {
    const std::vector<Recallable> commands = recallable();
    if (!recall_.active) {
        if (commands.empty()) {
            history_note_ = "no command to recall yet";
        } else if (by > 0) {
            history_note_ = "nothing newer -- Up recalls the last command";
        } else {
            recall_.before = line_;
            show_recalled(commands.front());
        }
        say(mail);
        return;
    }
    const std::string now = line_.text();
    if (by < 0) {
        for (const Recallable& c : commands) {
            if (c.seq < recall_.seq && c.text != now) {
                show_recalled(c);
                say(mail);
                return;
            }
        }
        history_note_ = known_.dropped > 0 ? "the oldest kept; " +
                                                 std::to_string(known_.dropped) +
                                                 " older entries dropped for good"
                                           : "the oldest command";
        say(mail);
        return;
    }
    for (std::size_t i = commands.size(); i > 0; --i) {
        const Recallable& c = commands[i - 1];
        if (c.seq > recall_.seq && c.text != now) {
            show_recalled(c);
            say(mail);
            return;
        }
    }
    // PAST THE NEWEST IS THE LINE BEFORE THE FIRST RECALL, exactly as it was left.
    cancel_recall();
    say(mail);
}

inline void TerminalPaneWeave::show_recalled(const Recallable& c) {
    line_.set(c.text, c.text.size());
    recall_.active = true;
    recall_.seq = c.seq;
    dismissed_ = false;
    asked_for_list_ = false;
    unmove_cursor();
    moved();
    offered_ = TerminalCompletionOffered{};
    offered_about_ = here();
    offered_intent_ = intent_;
}

/// THE RECALLED LINE BECOMES THE DRAFT, where it stands, and the line before the recall is
/// let go. Answers whether there was a recall to end.
inline bool TerminalPaneWeave::settle_recall() {
    if (!recall_.active) {
        return false;
    }
    recall_ = Recall{};
    moved();
    return true;
}

/// BACK TO THE DRAFT BEFORE THE FIRST RECALL -- the whole box, so its undo, its caret and
/// the draft a paste in flight belongs to come back with its text.
inline void TerminalPaneWeave::cancel_recall() {
    line_ = recall_.before;
    recall_ = Recall{};
    moved();
    remember_line();
}

/// WHAT THE ROW ABOVE THE LINE SAYS ABOUT HISTORY, or nothing.
inline std::string TerminalPaneWeave::history_heading() const {
    if (!recall_.active) {
        return history_note_;
    }
    const std::vector<Recallable> commands = recallable();
    std::size_t newer = 0;
    for (const Recallable& c : commands) {
        newer += c.seq > recall_.seq ? 1 : 0;
    }
    std::string text = "history " + std::to_string(newer + 1) + " of " +
                       std::to_string(commands.size() > newer ? commands.size() : newer + 1);
    text += history_note_.empty() ? " -- Enter/Tab: edit, Esc: back" : " -- " + history_note_;
    return text;
}

// ---- The one read: what could be said next, and what an answer is about -----------------

/// Ask what could be said next: at most one question outstanding, never from a place the
/// answer could not be about. Every exit leaves the offer applying to the line that is there:
/// the three silences below re-stamp `offered_about_`, and the ask stamps `asked_about_`, so
/// the answer can be measured against the line it comes back to.
inline void TerminalPaneWeave::ask_completion(loom::Mail& mail) {
    wanted_ = true;
    wanted_intent_ = intent_;
    if (recall_.active) {
        // A RECALLED LINE IS BROWSED, NOT COMPOSED: no list is asked for it until the weaver
        // locks it in or edits it.
        offered_ = TerminalCompletionOffered{};
        offered_about_ = here();
        offered_intent_ = intent_;
        return;
    }
    if (!known_.attached) {
        offered_ = TerminalCompletionOffered{};
        offered_about_ = here();
        offered_intent_ = intent_;
        return; // nothing to ask, and a door that would answer "nothing" anyway
    }
    // Asked about the end of the line, where the caret must be: accepting a candidate drops
    // the typed part of the last token and appends the rest, which with a mid-line caret
    // would delete everything after it. So the pane says so aloud rather than going quiet,
    // since three different silences would look identical.
    if (!line_.at_end()) {
        offered_ = TerminalCompletionOffered{};
        offered_.open = true;
        offered_.heading = "completion follows the END of the line -- this caret is inside it";
        offered_about_ = here();
        offered_intent_ = intent_;
        unmove_cursor();
        return;
    }
    // AN UNTOUCHED LINE ASKS NOTHING. The line is empty immediately after a submit, and a
    // list there covers the answer the pane just gave. Typing is the gesture; the
    // completion key is the way to ask anyway.
    if (line_.empty() && !asked_for_list_) {
        offered_ = TerminalCompletionOffered{};
        offered_about_ = here();
        offered_intent_ = intent_;
        unmove_cursor();
        return;
    }
    if (completing_) {
        return; // one question at a time; its answer is dropped and asked again (`wanted_`)
    }
    asked_about_ = here();
    asked_intent_ = intent_;
    completion_pending_ = ++asked_;
    completing_ = true;
    (void)mail.as_role(pane::kTerminalPaneRole)
        .send_to_role(kWorkshopRole, TerminalCompletionRequested{line_.text()},
                      completion_pending_);
}

/// What a completion is about: the line, and where in it the weaver stands. The text, because
/// accepting strips `partial` off the end and appends `insert`, meaningless against a line
/// the partial is not a token of; the caret, because completion follows the end of the line,
/// and a mid-line caret would lose everything after it.
inline TerminalPaneWeave::Asking TerminalPaneWeave::here() const { return Asking{line_.text(), line_.caret()}; }

/// Is what this pane holds about the line in front of the weaver? Asked before the list is
/// drawn as well as before a candidate is taken. The cost is a publication with no list while
/// a fresh answer is in flight (the completion is a round trip); whether a medium draws both
/// canvases of that turn is the medium's business, unmeasured. What it buys: no candidate is
/// ever offered against a line that is not on the screen.
inline bool TerminalPaneWeave::offer_applies() const { return offered_intent_ == intent_ && offered_about_ == here(); }

/// THE WEAVER'S ACT CHANGED WHAT A COMPLETION WOULD BE ABOUT: a new intent, which no answer
/// already in flight belongs to, and no wish for a list until an act asks for one.
inline void TerminalPaneWeave::moved() {
    ++intent_;
    wanted_ = false;
}

// ---- The completion list's own cursor -------------------------------------------------

/// WHETHER THERE IS A CANDIDATE TO ACT ON -- and the first half of the question is
/// whether what this pane is holding is about the line in front of the weaver at all
/// (`offer_applies`). A list that outlived its line is not a list.
inline bool TerminalPaneWeave::selectable() const {
    return offer_applies() && offered_.open && !dismissed_ && !offered_.candidates.empty();
}

/// THE CURSOR MOVED BY THE WEAVER'S UP OR DOWN -- moved even where an end holds it in place, for
/// the weaver is choosing in the list, and Enter then takes what it stands on.
inline void TerminalPaneWeave::move_selection(int by) {
    if (!selectable()) {
        return;
    }
    cursor_moved_ = true;
    const std::size_t last = offered_.candidates.size() - 1;
    if (by < 0) {
        selected_ = selected_ == 0 ? 0 : selected_ - 1;
    } else {
        selected_ = selected_ >= last ? last : selected_ + 1;
    }
}

inline void TerminalPaneWeave::accept_candidate() {
    if (!selectable()) {
        return;
    }
    const ShownCandidate& c = offered_.candidates[selected_];
    // `partial` IS A TOKEN OF THIS VERY LINE, so it can never be longer than the line.
    // The `min` is written anyway rather than as an `if`, because the alternative to
    // clamping is appending without stripping, which is a doubled word on a line nobody
    // could explain.
    const std::size_t typed =
        offered_.partial.size() < line_.size() ? offered_.partial.size() : line_.size();
    std::string text = line_.text();
    text.resize(text.size() - typed);
    text += c.insert;
    const std::size_t at = text.size();
    line_.set(std::move(text), at);
}

/// THE CANDIDATE THE CURSOR IS ON, TAKEN -- by Tab, or by Enter once the weaver moved the cursor
/// -- and what could follow the line it leaves asked again. The cursor is the list's once more,
/// so the next Enter runs the line unless the weaver moves it.
inline void TerminalPaneWeave::take_candidate(loom::Mail& mail) {
    accept_candidate();
    cursor_moved_ = false;
    moved();
    remember_line();
    ask_completion(mail);
}

/// THE LIST'S CURSOR ON ITS FIRST ROW, where the list puts it and no weaver moved it.
inline void TerminalPaneWeave::unmove_cursor() {
    selected_ = 0;
    cursor_moved_ = false;
}

} // namespace

#endif // ZENGINE_TERMINAL_HISTORY_AND_COMPLETION_HPP
