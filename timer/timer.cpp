// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

// The loadable Timer, `zengine-timer`. Its scheduling is in timer_weave.hpp, and the host's one
// wait sends it the time (TIMER-02), so nothing here reads a clock. This file decides whose copy of
// the delay rule the Timer uses. Its constructor takes the host's offer inside `create()`, because
// that is the only moment a host's offer is open to it (`operator/docs/operator-host.md`).

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

/// The Timer this library ships, using whichever copy of the delay rule this load was offered.
/// It adds a constructor and nothing else, because `ZEN_EXPORT_WEAVE` builds it with `new S()`. It
/// may refuse to exist: an offer that cannot serve `timer.normalize_delay` with this Timer's
/// signature throws, the reason is printed on stderr, and the Kernel refuses the load in the
/// throw's words.
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

/// This library can be offered an operator host. The offer is optional: a host that offers
/// nothing loads the library all the same, and the Timer uses the catalog this repository builds.
ZENGINE_OPERATOR_CONSUMER();

/// This library also supplies an operator, `timer.normalize_delay`. One library therefore stands in
/// three separate relationships: `zen_weave_abi` (it can be the Timer), `zengine_operator_provider`
/// (it supplies the rule) and `zengine_operator_consumer` (it can use a host's rules). A host
/// installs what the library provides first and then offers its catalog to the Timer it creates,
/// so the rule exists before the Timer that needs it. The rule's primitives come from whichever
/// weave supplies them, so another provider of either changes what this Timer schedules without a
/// rebuild.
ZENGINE_OPERATOR_PROVIDER(zengine::timer::kTimerRole, zengine::timer::provider_contributions)
