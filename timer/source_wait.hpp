// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_TIMER_SOURCE_WAIT_HPP
#define ZENGINE_TIMER_SOURCE_WAIT_HPP

// What a source waits on, and the time by which it must look again, as it tells the host's one
// wait. A source is a weave that reads the outside world: a window, a console, a child's pipes, a
// door. After every delivery it claims, as its role, what it waits on (`loom::host::WaitSet`), and
// the host's wait (Loom's `zen/host/reactor.hpp`) wakes it when that is ready. A source that must
// also look again on time puts that time in the same claim (`until_ms`), on the wait's clock and
// counted from the time the wait last sent it (`Waited::now_ms`), so it reads no clock of its own.
// A source whose handles the wait cannot hold claims a time alone and looks then, which is how it
// polls. A host that runs no wait wakes no source. The pages that use this are "The host's wait" in
// `timer/docs/timers.md` and the Input role's manual.

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

/// A time already past on every clock a host's wait keeps. A source that must look but has not
/// yet been sent the time claims it, so it is due at the next wait, which sends it the time.
inline constexpr std::int64_t kTimePast = 0;

class SourceWait {
public:
    /// Reads what the host's wait sent, and returns true when the source should look now: what it
    /// waits on is ready, or the time it claimed has come. Only the host's own `Waited`, which has
    /// no sender, counts; one from any weave changes nothing, so no weave can wake another's
    /// source. Every `Waited` the host sends carries the wait's clock, which is kept as `now_ms`.
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
            // The wait will not hold what this source waits on until that changes. Meanwhile the
            // source polls on a time alone, rather than claiming the same refused set after
            // every look.
            refused_ = last_;
        }
        return w.held && (w.ready || w.due);
    }

    /// Claims, as `role`, what the source waits on now, after it has looked: `set`, or nothing
    /// when it cannot be waited on. The claim names a time to look again, `every_ms` after the
    /// time the wait last sent, when the source must look `on_time`, and also when the wait cannot
    /// hold `set`, so that the source polls on that time instead.
    void settle(loom::Mail& mail, std::string_view role,
                const std::optional<loom::host::WaitSet>& set, bool on_time,
                std::int64_t every_ms) {
        if (refused_ && (!set || !same(*set, *refused_))) {
            refused_.reset(); // What the source waits on changed, so the wait may hold it now.
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

    /// The latest time the host's wait sent this source, or none before the first.
    std::optional<std::int64_t> now_ms() const noexcept { return now_; }

    /// Whether the host's wait holds this source's claim, as its last `Waited` reported, and if not, why.
    bool held() const noexcept { return held_; }
    const std::string& why() const noexcept { return why_; }

private:
    /// `ms` after the time the wait last sent, saturating, or a time already past when no time has
    /// been sent yet.
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
