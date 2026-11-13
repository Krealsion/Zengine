// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

// The TimerService weave library. Scheduling lives in timer_weave.hpp, handed the time by the
// host's one wait (TIMER-02): nothing here reads a clock. This file decides which operator truth
// the instance spends: its constructor takes the host's offer inside `create()`, the only window a
// scoped offer leaves open (operator/docs/operator-host.md).
// Timer law: timer/docs/timer-laws.md

#include "normalize.hpp"
#include "timer_weave.hpp"
#include "vocabulary.hpp"

#include "operator/host.hpp"
#include "operator/provider.hpp"

#include <zen/kernel/export.hpp>

#include <cstdint>
#include <cstdio>
#include <exception>
#include <vector>

namespace {

/// The shipped Timer: the service, spending whatever authority this load was offered -- a
/// constructor and nothing else, since `ZEN_EXPORT_WEAVE` builds with `new S()`. It may refuse to
/// exist: an offer that cannot serve `timer.normalize_delay` at this Timer's
/// signature throws, the reason goes to stderr, and the Kernel refuses the load in the throw's
/// words.
class TimerService : public zengine::timer::TimerWeave {
public:
    TimerService() : zengine::timer::TimerWeave(offered_authority()) {}

private:
    static zengine::timer::DelayAuthority offered_authority() {
        try {
            return zengine::timer::DelayAuthority(zengine::op::OperatorHost::offered());
        } catch (const std::exception& e) {
            std::fprintf(stderr, "zengine-timer: %s\n", e.what());
            std::fflush(stderr);
            throw;
        }
    }
};

} // namespace

ZEN_EXPORT_WEAVE(TimerService)

/// This image can receive an operator host. Optional: a host that offers nothing loads this
/// library as it always did and gets the vocabulary this repository authors.
ZENGINE_OPERATOR_CONSUMER();

/// ...and this image supplies a power, `timer.normalize_delay`. Three independent relationships,
/// one artifact: `zen_weave_abi` (it can be the Timer), `zengine_operator_provider` (it supplies
/// the rule) and `zengine_operator_consumer` (it can spend a host's truth). A host mounts the
/// provider first and then offers its resolution to the instance it creates, so the contribution
/// exists before the instance that needs it. The rule's primitives come from whoever supplies
/// them, so another provider for either changes what this Timer schedules without a rebuild.
ZENGINE_OPERATOR_PROVIDER(zengine::timer::kTimerRole, zengine::timer::provider_contributions)
