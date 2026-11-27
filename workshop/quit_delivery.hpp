// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_QUIT_DELIVERY_HPP
#define ZENGINE_WORKSHOP_QUIT_DELIVERY_HPP

// What a quit question that ended with no answer comes to, in a weaver's words, and the host's
// session door, which ends the host's lifetime once the weaver's quit is settled. Workshop asks
// each participant that takes the question through Loom's ask door, so an ask that cannot be
// answered ends as Loom's `zen.AskEnded`, told to Workshop and to nobody else.

#include <zen/session/vocabulary.hpp>
#include <zen/weave.hpp>
#include <zen/weave/ask_ended.hpp>

#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace zengine::workshop {

/// WHO COULD NOT BE ASKED, OR NEVER ANSWERED, AND WHY, in a weaver's words and Loom's: the roles it
/// held when it was asked and its weave, what the ending means for that participant, and the
/// reason's own name.
std::string unanswered_quit_words(const std::vector<std::string>& roles, loom::WeaveId weave,
                                  const loom::AskEnded& ended);

// THE HOST'S SESSION DOOR: the participant holding Loom's session role (`loom.session`) in
// Workshop's host, which ends this host's lifetime when it is asked to by Loom's own word for it,
// `loom.session.Shutdown`. Workshop asks it once the weaver's quit is settled; nothing reaches the
// host but that message. Host wiring: it is made with what ending does here, which no message can
// hand a weave, and it reads and keeps nothing of the bus.

/// Counters, and nothing else.
struct HostSessionState {
    std::int64_t shutdowns = 0; ///< `Shutdown`s heard
    ZEN_SHAPE(HostSessionState, 1, ZEN_FIELD(shutdowns));
};

/// The door. Mount it in `loom::session::kSessionRole` with `host_session_grant()`.
class HostSessionDoor final
    : public loom::WeaveBase<HostSessionDoor, HostSessionState,
                             loom::Accept<loom::session::Shutdown>, loom::Emit<loom::Ack>> {
public:
    /// `stop` is what ending this host's lifetime does: the host's bus told to stop, so the turn
    /// in flight ends and the host's loop finishes. Empty, the door only says it is ending.
    explicit HostSessionDoor(std::function<void()> stop = {}) : stop_(std::move(stop)) {}

    /// A door holding host wiring does not reload: a revived one would hold no host.
    loom::LifecyclePolicy policy_config() const { return loom::LifecyclePolicy{0, true}; }

    /// END THIS HOST LIFETIME. Who may say it is the sender's grant to send `Shutdown` to this
    /// role, which only Workshop's holds. An ask is answered `zen.Ack`; Workshop's word, sent
    /// without one, is answered by the stop itself.
    // WL-SESSION-13 -- agents/workshop/session.md
    void on(const loom::session::Shutdown& said, loom::Mail& mail) {
        ++state_.shutdowns;
        ending_ = true;
        reason_ = said.reason;
        if (mail.correlation() != 0) {
            (void)mail.answer(loom::Ack{});
        }
        if (stop_) {
            stop_();
        }
    }

    /// Whether a `Shutdown` was heard, and the reason it gave.
    bool ending() const noexcept { return ending_; }
    const std::string& reason() const noexcept { return reason_; }

private:
    std::function<void()> stop_;
    bool ending_ = false;
    std::string reason_;
};

/// The grant the door needs: its answer to whoever asked, and the poke answers.
inline loom::Grant host_session_grant() {
    loom::Grant g;
    g.allow_to_any(loom::Ack::zen_name, loom::Ack::zen_version);
    loom::allow_poke_answers(g);
    return g;
}

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_QUIT_DELIVERY_HPP
