// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_QUIT_DELIVERY_HPP
#define ZENGINE_WORKSHOP_QUIT_DELIVERY_HPP

// The host's witness of a quit question Loom could not deliver. The question is a publication,
// which `zen.DispatchRefused` does not cover, so the host observes the bus tap's `Refused` event
// (sender, target, correlation), writes it in a book in `HostContext`, and wakes Workshop with an
// empty delivery. The wake-up is not authority: Workshop acts only on an entry for its own quit.

#include "pane/vocabulary.hpp"

#include <zen/session/vocabulary.hpp>
#include <zen/switchboard/switchboard.hpp>
#include <zen/weave.hpp>

#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace zengine::workshop {

/// ONE DELIVERY OF A QUIT QUESTION THAT LOOM REFUSED, copied off the tap while the event was valid.
struct UndeliveredQuit {
    std::uint64_t ask = 0;         ///< the correlation the refused envelope carried: the asker's own
    loom::WeaveId participant{};   ///< the recipient fanout resolved, as the bus stamped it
    std::vector<std::string> roles; ///< the roles that recipient held at the refusal, if any
    loom::RefusalReason reason = loom::RefusalReason::None;
};

/// THE HOST'S BOOK OF REFUSED QUIT DELIVERIES. Written by `QuitDeliveryWatch` and emptied by the
/// Workshop weave at each wake-up, nothing else. Unbounded by type and bounded by what feeds it:
/// one entry per quit envelope Loom refused, every one of which queued a wake-up that empties it.
class UndeliveredQuits {
public:
    void note(UndeliveredQuit one) { noted_.push_back(std::move(one)); }
    /// Everything written since the last take, in the order the refusals happened.
    std::vector<UndeliveredQuit> take() { return std::exchange(noted_, {}); }
    bool empty() const noexcept { return noted_.empty(); }

private:
    std::vector<UndeliveredQuit> noted_;
};

/// THE WAKE-UP: "a quit delivery was refused; read the book". It carries nothing, deliberately, so
/// nothing on the wire can be mistaken for the fact.
struct QuitDeliveryRefusalNoted {
    ZEN_SHAPE(QuitDeliveryRefusalNoted, 1);
};

/// THE OBSERVER, for as long as this object lives. Registered on construction and removed on
/// destruction, so a host declares it after the bus and after the `HostContext` whose book it
/// writes, and it is gone before either. It holds no participant and no weave, only ids.
class QuitDeliveryWatch {
public:
    QuitDeliveryWatch(loom::Switchboard& bus, loom::WeaveId workshop, UndeliveredQuits& book);
    ~QuitDeliveryWatch();
    QuitDeliveryWatch(const QuitDeliveryWatch&) = delete;
    QuitDeliveryWatch& operator=(const QuitDeliveryWatch&) = delete;

private:
    loom::Switchboard& bus_;
    loom::ObserverId tap_ = 0;
};

/// WHO COULD NOT BE ASKED, AND WHY, in a weaver's words and Loom's: the role and weave, what the
/// refusal means for that participant, and the reason's own name.
std::string undelivered_quit_words(const UndeliveredQuit& one);

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
