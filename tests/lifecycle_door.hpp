#ifndef ZENGINE_TESTS_LIFECYCLE_DOOR_HPP
#define ZENGINE_TESTS_LIFECYCLE_DOOR_HPP

// The suites' stand-in for the Loom's control door (R2B-1).
//
// Every Zengine package that arranges its own time does so on `zen.Activated`,
// and since R2B-1 that fact is only believed when LOOM ATTESTS IT. So a test
// that wants a weave to come alive can no longer hand-post the public shape —
// which is exactly the point, and exactly why this file exists rather than a
// root shortcut: the suites must activate the way the real door does, or they
// would be proving something the running system does not do.
//
// A HOST hands out the authority; a weave cannot mint one. Since R2B-1a the only
// expression that yields one is `loom::host_lifecycle_authority(bus)`, declared
// in `zen/host/lifecycle_wiring.hpp` — a host-wiring header no weave-authoring
// header includes — and it requires the `Switchboard` itself, which a weave
// never holds. This suite may call it because a test harness IS a host: it owns
// the bus. That is being inside the boundary by construction, not by exemption.
//
// It is also the FORGE. `announce`/`claim` are separate on purpose so a test can
// ask for an attestation that disagrees with its own payload, and `Impostor` is
// an ordinary weave holding nothing but the ordinary grant for the public shape
// — the whole threat model in one class. An honest API that could not express
// the attack would make the pin worthless.

#include <zen/host/lifecycle_wiring.hpp> // host wiring — the harness is the host
#include <zen/switchboard.hpp>
#include <zen/weave.hpp>
#include <zen/weave/lifecycle.hpp>

#include <cstdint>
#include <string>

namespace zengine::testing {

/// "Announce a commit for `target`." `announce` is what Loom is asked to attest;
/// `claim` is what the payload states. Honest callers pass the same value twice.
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
