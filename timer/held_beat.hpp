// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_TIMER_HELD_BEAT_HPP
#define ZENGINE_TIMER_HELD_BEAT_HPP

// A SOURCE'S BEAT, LET GO WHILE A HOST'S WAIT HOLDS IT. A weave that reads the outside world claims
// what it waits on as its role (`loom::host::WaitSet`) after every delivery, and its beat stops
// once the host's one wait (Loom's `zen/host/reactor.hpp`) says it holds that claim; what it waits
// on then wakes it. The beat runs while no wait holds it, while it cannot be waited on, and while
// it has a reason of its own to look on time. A host without the wait never says so: every beat
// runs as ever. Reference: timer/docs/zengine.timer.md.

#include <zen/host/wait_set.hpp>
#include <zen/weave/weave.hpp>

#include <optional>
#include <string>
#include <string_view>

namespace zengine::timer {

class HeldBeat {
public:
    /// What the host's wait said. True for a wake: the source looks now, then `settle`s. A
    /// `Waited` from any participant is nothing, so no weave stops another's beat.
    bool hear(const loom::host::Waited& w, const loom::Mail& mail) {
        if (mail.sender().valid()) {
            return false;
        }
        held_ = w.held;
        why_ = w.why;
        return w.held && w.ready;
    }

    /// AFTER THE SOURCE LOOKED: claim what it waits on now as `role` -- `set`, or nothing when it
    /// cannot be waited on -- and say whether its beat runs: while no wait holds it, while it
    /// cannot be waited on, and while it must look `on_time`.
    bool settle(loom::Mail& mail, std::string_view role,
                const std::optional<loom::host::WaitSet>& set, bool on_time) {
        (void)mail.as_role(role).claim(set.value_or(loom::host::WaitSet{}));
        return !held_ || !set.has_value() || on_time;
    }

    /// The same, running or stopping a `TimerHandle` as that says.
    template <class Handle>
    void settle(loom::Mail& mail, std::string_view role,
                const std::optional<loom::host::WaitSet>& set, bool on_time, Handle& beat) {
        const bool beats = settle(mail, role, set, on_time);
        if (beats && beat.canceled()) {
            beat.restart(mail);
        } else if (!beats && !beat.canceled()) {
            beat.cancel(mail);
        }
    }

    /// Whether a host's wait holds this source's claim, as it last said, and why not.
    bool held() const noexcept { return held_; }
    const std::string& why() const noexcept { return why_; }

private:
    bool held_ = false;
    std::string why_;
};

} // namespace zengine::timer

#endif // ZENGINE_TIMER_HELD_BEAT_HPP
