// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_TIMER_SOURCE_WAIT_HPP
#define ZENGINE_TIMER_SOURCE_WAIT_HPP

// WHAT A SOURCE WAITS ON, AND THE TIME IT MUST LOOK BY, said to the host's one wait. A weave that
// reads the outside world -- a window, a console, a child's pipes, a door -- claims what it waits
// on as its role (`loom::host::WaitSet`) after every delivery, and the host's wait (Loom's
// `zen/host/reactor.hpp`) wakes it when that is ready. A source that must look again on time says
// when in the same claim (`until_ms`), on the wait's clock, from the time the wait last handed it
// (`Waited::now_ms`), so it reads no clock of its own; one the wait cannot hold claims a time alone
// and looks then, which is how it polls. A host that runs no wait wakes no source. Reference:
// timer/docs/zengine.timer.md, "A source's own time".

#include <zen/host/wait_set.hpp>
#include <zen/weave/weave.hpp>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace zengine::timer {

/// A time already past on every clock a host's wait keeps: claimed by a source that must look but
/// has not been handed the time yet, it is due at the next wait, which hands it the time.
inline constexpr std::int64_t kTimePast = 0;

class SourceWait {
public:
    /// What the host's wait said. True when the source looks now: what it waits on is ready, or
    /// the time it claimed came. Only the host's own word counts (it has no sender): a `Waited`
    /// from any participant is nothing, so no weave wakes another's source. Every one the host
    /// sends hands over the wait's clock, kept as `now_ms`.
    bool hear(const loom::host::Waited& w, const loom::Mail& mail) {
        if (mail.sender().valid()) {
            return false;
        }
        if (!now_ || w.now_ms > *now_) {
            now_ = w.now_ms;
        }
        held_ = w.held;
        why_ = w.why;
        if (!w.held && claimed_handles_) {
            // The wait will not hold what this source waits on until that changes: it polls on a
            // time alone meanwhile, rather than claiming the same refusal after every look.
            refused_ = last_;
        }
        return w.held && (w.ready || w.due);
    }

    /// AFTER THE SOURCE LOOKED: claim what it waits on now as `role` -- `set`, or nothing when it
    /// cannot be waited on -- with the time it must look by: `every_ms` after the time last handed
    /// when it must look `on_time`, and when the wait cannot hold `set`, so it polls on that time.
    void settle(loom::Mail& mail, std::string_view role,
                const std::optional<loom::host::WaitSet>& set, bool on_time,
                std::int64_t every_ms) {
        if (refused_ && (!set || !same(*set, *refused_))) {
            refused_.reset(); // what it waits on changed: the wait may hold it now
        }
        loom::host::WaitSet claim;
        const bool waitable = set.has_value() && !refused_;
        if (waitable) {
            claim = *set;
        }
        if (on_time || !waitable) {
            claim.until_ms = at(every_ms);
        }
        claimed_handles_ = waitable && (!claim.handles.empty() || !claim.writable.empty() ||
                                        claim.messages);
        last_ = set;
        (void)mail.as_role(role).claim(claim);
    }

    /// The latest time the host's wait handed this source, none before it handed one.
    std::optional<std::int64_t> now_ms() const noexcept { return now_; }

    /// Whether the host's wait holds this source's claim, as it last said, and why not.
    bool held() const noexcept { return held_; }
    const std::string& why() const noexcept { return why_; }

private:
    /// `ms` after the time last handed, or a time already past when none was handed yet.
    std::int64_t at(std::int64_t ms) const {
        if (!now_) {
            return kTimePast;
        }
        constexpr std::int64_t top = std::numeric_limits<std::int64_t>::max();
        return ms > 0 && *now_ > top - ms ? top : *now_ + std::max<std::int64_t>(ms, 0);
    }

    static bool same(const loom::host::WaitSet& a, const loom::host::WaitSet& b) {
        return a.handles == b.handles && a.writable == b.writable && a.messages == b.messages;
    }

    std::optional<std::int64_t> now_;
    bool held_ = false;
    std::string why_;
    bool claimed_handles_ = false;
    std::optional<loom::host::WaitSet> last_;
    std::optional<loom::host::WaitSet> refused_;
};

} // namespace zengine::timer

#endif // ZENGINE_TIMER_SOURCE_WAIT_HPP
