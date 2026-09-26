// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_TESTS_LIFECYCLE_DOOR_HPP
#define ZENGINE_TESTS_LIFECYCLE_DOOR_HPP

// The suites' stand-in for Loom's control door. A Zengine package arranges its time on
// `zen.Activated`, believed only when LOOM ATTESTS IT (activation/activation.hpp), so a test
// cannot hand-post the public shape: the suites activate as the real door does, or they would
// prove what the running system does not do. The authority is `loom::host_lifecycle_authority`
// (`zen/host/lifecycle_wiring.hpp`, which no weave-authoring header includes), and it takes the
// `Switchboard` itself: a harness owns the bus, so it IS a host, by construction, not exemption.

#include <zen/host/lifecycle_wiring.hpp> // host wiring — the harness is the host
#include <zen/switchboard.hpp>
#include <zen/weave.hpp>
#include <zen/weave/lifecycle.hpp>

#include <cstdint>
#include <string>

namespace zengine::testing {

/// "Announce a commit for `target`." `announce` is what Loom is asked to attest; `claim` is what
/// the payload states. Honest callers pass the same value twice; they are separate so a test can
/// FORGE an attestation that disagrees with its payload, because an API that could not express
/// the attack would make the pin worthless.
struct ActivateOrder {
    std::int64_t target = 0;
    std::int64_t announce = 0;
    std::int64_t claim = 0;
    ZEN_SHAPE(ActivateOrder, 1, ZEN_FIELD(target), ZEN_FIELD(announce), ZEN_FIELD(claim));
};

struct DoorState {
    std::int64_t announced = 0;
    ZEN_SHAPE(DoorState, 1, ZEN_FIELD(announced));
};

/// A lifecycle operator: it holds a real authority because the test, standing in
/// for the host, gave it one.
class TestDoor : public loom::WeaveBase<TestDoor, DoorState, loom::Accept<ActivateOrder>,
                                        loom::Emit<loom::Activated>> {
public:
    explicit TestDoor(loom::LifecycleAuthority authority) : authority_(authority) {}

    void on(const ActivateOrder& o, loom::Mail& mail) {
        ++state_.announced;
        mail.announce_lifecycle(authority_, loom::WeaveId{static_cast<std::uint64_t>(o.target)},
                                loom::Activated{o.claim}, o.announce);
    }

private:
    loom::LifecycleAuthority authority_;
};

/// An ordinary weave with the ordinary grant for `zen.Activated` and nothing
/// else — no authority, no privilege, no special mount. What it sends is a
/// perfectly legal, perfectly shaped, perfectly stamped message that means
/// nothing.
class Impostor : public loom::WeaveBase<Impostor, DoorState, loom::Accept<ActivateOrder>,
                                        loom::Emit<loom::Activated>> {
public:
    void on(const ActivateOrder& o, loom::Mail& mail) {
        ++state_.announced;
        mail.send(loom::WeaveId{static_cast<std::uint64_t>(o.target)}, loom::Activated{o.claim});
    }
};

/// Mount a door and hand it the authority — the host's gesture, in one line.
inline loom::WeaveId mount_door(loom::Switchboard& bus) {
    return loom::mount<TestDoor>(bus, loom::host_lifecycle_authority(bus));
}

/// Ask `door` to announce, by an ordinary ROOT send: the trigger is the test's,
/// the attestation is Loom's, and the send that carries it is the door's own
/// ordinary gated send.
inline void order_activation(loom::Switchboard& bus, loom::WeaveId door, loom::WeaveId target,
                             std::int64_t announce, std::int64_t claim) {
    bus.send(door, loom::Message(loom::to_value(ActivateOrder{
                       static_cast<std::int64_t>(target.value), announce, claim})));
}

inline void order_activation(loom::Switchboard& bus, loom::WeaveId door, loom::WeaveId target,
                             std::int64_t sequence) {
    order_activation(bus, door, target, sequence, sequence);
}

} // namespace zengine::testing

#endif // ZENGINE_TESTS_LIFECYCLE_DOOR_HPP
