// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

// What a quit question that ended with no answer comes to (`quit_delivery.hpp`): one wording for
// the host and the suites.

#include "quit_delivery.hpp"

#include <variant>

namespace zengine::workshop {

namespace {

/// WHAT AN ENDING MEANS FOR THE PARTICIPANT IT NAMES -- the weaver's half; Loom's reason is next.
struct Meaning {
    const char* operator()(loom::AskNotDelivered reason) const {
        switch (reason) {
        case loom::AskNotDelivered::ApplicationFailed:
            return "it is held until it is reloaded or removed";
        case loom::AskNotDelivered::TargetUnavailable: return "it is not running";
        case loom::AskNotDelivered::NoSuchTarget: return "it is no longer mounted";
        case loom::AskNotDelivered::NotAccepted: return "it no longer takes the question";
        case loom::AskNotDelivered::CapabilityDenied:
            return "this Workshop is not granted the question";
        case loom::AskNotDelivered::GateRefused: return "the question did not admit at its door";
        case loom::AskNotDelivered::AskerSealed:
            return "the Workshop that asked was retired before the question arrived";
        }
        return "Loom refused the delivery";
    }
    const char* operator()(loom::AskNotAnswered reason) const {
        return reason == loom::AskNotAnswered::HandlerFailed
                   ? "it was asked, and its handler did not finish"
                   : "it was asked, and returned without answering";
    }
    const char* operator()(loom::AskNotKept reason) const {
        switch (reason) {
        case loom::AskNotKept::Released: return "it kept its answer, then let it go";
        case loom::AskNotKept::RespondentReplaced:
            return "its code was replaced before it answered";
        case loom::AskNotKept::RespondentRemoved: return "it was removed before it answered";
        case loom::AskNotKept::RespondentDied: return "it stopped running before it answered";
        case loom::AskNotKept::RespondentRetired: return "it was retired before it answered";
        }
        return "its kept answer ended unspent";
    }
    const char* operator()(loom::AnswerNotDelivered) const {
        return "it answered, and Loom could not deliver the answer";
    }
};

} // namespace

// WL-SESSION-19 -- agents/workshop/session.md
std::string unanswered_quit_words(const std::vector<std::string>& roles, loom::WeaveId weave,
                                  const loom::AskEnded& ended) {
    const std::string id = "weave " + std::to_string(weave.value);
    std::string held;
    for (const std::string& role : roles) {
        held += (held.empty() ? "" : "+") + role;
    }
    return (held.empty() ? id : held + " (" + id + ")") + " -- " +
           std::visit(Meaning{}, ended.stage.is) + " (" +
           std::string(loom::reason_name(ended.stage)) + ")";
}

} // namespace zengine::workshop
