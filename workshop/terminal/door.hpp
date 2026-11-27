// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_TERMINAL_DOOR_HPP
#define ZENGINE_WORKSHOP_TERMINAL_DOOR_HPP

// The Terminal pane's door on Workshop's weave: the pane's act, its completion and a value it
// captures, each forwarded to the host's Terminal door (`kTerminalDoorRole`) and answered with
// that door's answer, held meanwhile; and the picture of the participant's record, read from that
// door's claim and said to the pane when it changed. Workshop holds no participant: the host's
// door does (`host_door.hpp`). The pane's image includes none of it (WL-TERM-08).
// Workshop law: agents/workshop/terminal-door.md

#include "workshop/panes.hpp"
#include "workshop/terminal_seam_vocabulary.hpp"

#include "pane/operation.hpp"

#include <zen/weave.hpp>
#include <zen/weave/ask_book.hpp>
#include <zen/weave/ask_ended.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace zengine::workshop {

/// THE TERMINAL'S DOOR ON WORKSHOP. What it keeps is the asks it forwarded, each with the pane's
/// answer right, and the last picture it said.
class TerminalDoor {
public:
    using Accept = loom::Accept<TerminalActRequested, TerminalCompletionRequested, TerminalActed,
                                TerminalCompletionOffered, TerminalValueAnswered, loom::AskEnded>;
    using Emit = loom::Emit<TranscriptShown, TerminalActed, TerminalCompletionOffered,
                            TerminalValueAnswered, TerminalLineAsked, TerminalCompletionAsked,
                            TerminalValueAsked>;

    /// How many forwarded asks wait on the host's door at once; past it the pane is refused.
    static constexpr std::size_t kMaxForwarded = 8;

    /// THE ONE ACT THAT AUTHORS, `submit`: forwarded, and its answer the door's; then `self`, the
    /// weave composing this door, repaints its desk so the picture follows the answer.
    template <class Self>
    void on(Self& self, const TerminalActRequested& asked, loom::Mail& mail) {
        (void)self;
        // A ROLE MAY ASK; ANONYMOUS SPEECH MAY NOT -- `on(DocumentActRequested)`'s rule, and the
        // reason it names nobody: a second presentation of this participant asks with no edit
        // here.
        if (mail.authored_role().empty()) {
            return;
        }
        if (asked.act != kTerminalSubmitAct) {
            (void)mail.answer(TerminalActed{false, "this door knows one act, and it is `submit`"});
            return;
        }
        forward(Asked::kLine, TerminalLineAsked{asked.line}, {}, mail);
    }

    /// What could be said next on a half-typed line, the fields in the order `self`'s session
    /// chose (WL-TERM-05): forwarded, and read there, never authored.
    template <class Self>
    void on(Self& self, const TerminalCompletionRequested& asked, loom::Mail& mail) {
        if (mail.authored_role().empty()) {
            return;
        }
        forward(Asked::kCompletion,
                TerminalCompletionAsked{asked.line, self.session().completion_required_first},
                {}, mail);
    }

    /// A VALUE THE PANE CAPTURES, once `self` has authorized the gesture asking for it
    /// (`approved`): forwarded, and the door's answer the pane's.
    // WL-TERM-17 -- agents/workshop/terminal-door.md
    template <class Self, class Approved>
    void capture(Self& self, const TerminalValueRequested& asked, const Approved& approved,
                 loom::Mail& mail) {
        (void)self;
        forward(Asked::kValue, TerminalValueAsked{asked.participant, asked.observation},
                Approval{approved.pane_owner, approved.pane, approved.correlation,
                         approved.gesture},
                mail);
    }

    /// THE DOOR'S ANSWERS, each taken only as Loom's answer to a forwarded ask of that kind from
    /// whoever holds the door's role now, and spent as the pane's.
    template <class Self>
    void on(Self& self, const TerminalActed& said, loom::Mail& mail) {
        if (!take(self, Asked::kLine, said, mail)) {
            return;
        }
        // THE ANSWER, THEN THE PICTURE. The door claimed the record its act left before this
        // answer was delivered, so the repaint this ends in says it -- which is how the pane
        // learns what its own line came to without this door telling it twice.
        self.repaint(mail);
    }
    template <class Self>
    void on(Self& self, const TerminalCompletionOffered& said, loom::Mail& mail) {
        // NO REPAINT. Nothing about this host changed: the participant was read and not
        // written, and what the answer becomes on a screen is the pane's own drawing.
        (void)take(self, Asked::kCompletion, said, mail);
    }
    template <class Self>
    void on(Self& self, const TerminalValueAnswered& said, loom::Mail& mail) {
        (void)take(self, Asked::kValue, said, mail);
    }

    /// LOOM'S WORD THAT A FORWARDED ASK ENDED WITH NO ANSWER: the pane is answered in words, and
    /// nothing waits. An ask that reached no door had nowhere to author.
    template <class Self>
    void on(Self& self, const loom::AskEnded& ended, loom::Mail& mail) {
        if (!mail.ask_ended()) {
            return; // the shape alone is anybody's speech
        }
        const std::optional<loom::PendingAsk> mine = asks_.end(ended, mail);
        if (!mine) {
            return;
        }
        refuse(self, mine->id, ended.never_delivered() ? kNoDoor
                                                      : "the Terminal's door never answered -- " +
                                                            pane::no_answer(ended),
               mail);
    }

    /// WHAT THE PARTICIPANT'S RECORD HOLDS, TO WHOEVER IS PRESENTING IT -- the host's door's
    /// claim, read on the repaint, compared against the last utterance, and said only when it
    /// changed. No claim is a reading too: no participant is mounted, and the pane says so.
    void say_transcript(loom::Mail& mail) {
        TranscriptShown now = picture(mail);
        if (transcript_said_ && same_transcript(now, said_transcript_)) {
            return; // no news is silence, and silence is what makes this seam terminate
        }
        said_transcript_ = now;
        transcript_said_ = true;
        // `to_any`, NOT ADDRESSED, AND SAID AS THE ROLE -- `say_conditions`' own three
        // reasons, unchanged one shape over.
        (void)mail.as_role(kWorkshopProvider).publish(std::move(now));
    }

    /// A party has just arrived and has heard nothing: the next `say_transcript` says the record.
    void say_again() { transcript_said_ = false; }

    /// The record as the host's door claims it now, or no participant when nothing does.
    // WL-TERM-02 -- agents/workshop/terminal-door.md
    static TranscriptShown picture(loom::Mail& mail) {
        const loom::SenseReading read = mail.latest_from_role<TranscriptShown>(kTerminalDoorRole);
        if (!read || !read.value.has_value() || !read.by.role_holder_is_current) {
            return TranscriptShown{};
        }
        return loom::from_value<TranscriptShown>(*read.value);
    }

private:
    enum class Asked : std::uint8_t { kLine, kCompletion, kValue };

    /// Which gesture approved a capture, so its refusal forgets that approval and no other.
    struct Approval {
        loom::WeaveId pane_owner{};
        std::string pane;
        std::uint64_t correlation = 0;
        std::uint64_t gesture = 0;
    };

    /// One forwarded ask: its record in the book, what it asked, and the pane's answer right.
    struct Held {
        std::uint64_t id = 0;
        Asked asked = Asked::kLine;
        loom::DeferredAnswer due;
        Approval approval;
    };

    /// What a pane hears when no door could be asked: nothing was authored.
    static constexpr const char* kNoDoor =
        "no terminal participant is mounted on this bus -- nothing was authored";

    /// Hold the pane's answer right and ask the host's door `msg`, as Workshop's role.
    template <class Msg>
    void forward(Asked asked, const Msg& msg, Approval approval, loom::Mail& mail) {
        if (held_.size() >= kMaxForwarded) {
            answer_now(asked, "the Terminal is still answering " +
                                  std::to_string(held_.size()) + " asks -- try again",
                       mail);
            return;
        }
        loom::DeferredAnswer due = mail.defer_answer();
        if (!due.valid()) {
            return; // nothing can carry an answer back, and nothing is asked
        }
        const loom::AskOpened opened =
            asks_.open_to_role(kTerminalDoorRole, Msg::zen_name, Msg::zen_version);
        held_.push_back(Held{opened.id, asked, std::move(due), std::move(approval)});
        if (!opened) {
            spend(held_.size() - 1, refusal_of(asked, "the Terminal cannot hold another ask"),
                  mail);
            return;
        }
        const loom::Ticket sent = mail.as_role(kWorkshopProvider)
                                      .ask_role(kTerminalDoorRole, msg, opened.correlation);
        if (!sent.valid() || !asks_.bind_attempt(opened.id, sent.seq)) {
            (void)asks_.forget(opened.id);
            spend(index_of(opened.id), refusal_of(asked, kNoDoor), mail);
        }
    }

    /// The door's answer of kind `asked`: spent as the pane's when it is Loom's answer to a held
    /// ask of that kind, from the weave holding the door's role now. True when it was.
    template <class Self, class Answer>
    bool take(Self& self, Asked asked, const Answer& said, loom::Mail& mail) {
        if (!mail.answers_ask() ||
            self.bus_now_.holder_of(kTerminalDoorRole, mail) != mail.sender()) {
            return false; // not the door's answer to an ask of this host's
        }
        const loom::PendingAsk* open = asks_.match(mail.correlation(), mail.sender());
        const std::size_t at = open != nullptr ? index_of(open->id) : held_.size();
        if (at == held_.size() || held_[at].asked != asked) {
            return false;
        }
        (void)asks_.settle(mail.correlation(), mail.sender());
        if constexpr (std::is_same_v<Answer, TerminalValueAnswered>) {
            if (!said.available) {
                forget_approval(self, held_[at].approval);
            }
        }
        spend(at, loom::to_value(said), mail);
        return true;
    }

    /// The pane's answer to held ask `id`, in words: its kind's refusal.
    template <class Self>
    void refuse(Self& self, std::uint64_t id, const std::string& why, loom::Mail& mail) {
        const std::size_t at = index_of(id);
        if (at == held_.size()) {
            return;
        }
        if (held_[at].asked == Asked::kValue) {
            forget_approval(self, held_[at].approval);
        }
        spend(at, refusal_of(held_[at].asked, held_[at].asked == Asked::kValue
                                                  ? "The source terminal is no longer available"
                                                  : why),
              mail);
    }

    /// A capture refused forgets the approval its gesture gave, while that approval stands.
    template <class Self>
    static void forget_approval(Self& self, const Approval& approval) {
        auto& standing = self.approved_operation_;
        if (standing.pane_owner == approval.pane_owner && standing.pane == approval.pane &&
            standing.correlation == approval.correlation && standing.gesture == approval.gesture) {
            standing = {};
        }
    }

    /// What a pane is told for an ask of kind `asked` that has no answer from the door.
    static loom::Value refusal_of(Asked asked, const std::string& why) {
        switch (asked) {
        case Asked::kLine: return loom::to_value(TerminalActed{false, why});
        case Asked::kCompletion: return loom::to_value(TerminalCompletionOffered{});
        case Asked::kValue: return loom::to_value(TerminalValueAnswered{false, why, {}, {}});
        }
        return loom::to_value(TerminalActed{false, why});
    }

    /// Answer the pane's ask now, from inside its own delivery.
    static void answer_now(Asked asked, const std::string& why, loom::Mail& mail) {
        switch (asked) {
        case Asked::kLine: (void)mail.answer(TerminalActed{false, why}); return;
        case Asked::kCompletion: (void)mail.answer(TerminalCompletionOffered{}); return;
        case Asked::kValue:
            (void)mail.answer(TerminalValueAnswered{false, why, {}, {}});
            return;
        }
    }

    /// Spend held ask `at`'s answer right with `answer`, and let it go.
    void spend(std::size_t at, const loom::Value& answer, loom::Mail& mail) {
        if (at >= held_.size()) {
            return;
        }
        Held done = std::move(held_[at]);
        held_.erase(held_.begin() + static_cast<std::ptrdiff_t>(at));
        (void)mail.bus().spend_deferred(done.due, loom::Message(answer));
    }

    std::size_t index_of(std::uint64_t id) const {
        const auto it = std::find_if(held_.begin(), held_.end(),
                                     [id](const Held& h) { return h.id == id; });
        return static_cast<std::size_t>(it - held_.begin());
    }

    loom::AskBook asks_{kMaxForwarded};
    std::vector<Held> held_;
    /// The last picture said, so the publication is silent when nothing changed.
    TranscriptShown said_transcript_;
    bool transcript_said_ = false;
};

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_TERMINAL_DOOR_HPP
