// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_GUEST_DOOR_HPP
#define ZENGINE_WORKSHOP_GUEST_DOOR_HPP

// The guest door: the one participant holding this Workshop's listener, its admission policy and
// its connection inventory. A weave, not a loop: the host loop never returns while a Timer beats,
// so the door services the bridge on a repeating beat, from inside the bus. Not authority: every
// guest's send is checked at the bus under the guest's own grant.

#include "guest_seam_vocabulary.hpp"
#include "guests.hpp"

#include "input/vocabulary.hpp"
#include "timer/binding.hpp"

#include <zen/bridge/server.hpp>
#include <zen/weave.hpp>
#include <zen/weave/standard_shapes.hpp>

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace zengine::workshop {

/// The beat the door services the crossing on: `kGuestsRole`'s own, so a successor inherits it.
inline constexpr const char* kGuestBeatId = "zengine.guests.service";
inline constexpr std::int64_t kGuestBeatMs = 10;

/// Two honest counters, poke-inspectable like any state.
struct GuestDoorState {
    std::int64_t services = 0; ///< beats on which the crossing was serviced
    std::int64_t said = 0;     ///< inventories published
    ZEN_EXPOSE();
    ZEN_SHAPE(GuestDoorState, 1, ZEN_FIELD(services), ZEN_FIELD(said));
};

class GuestDoor final
    : public zengine::timer::TimedWeave<GuestDoor, GuestDoorState,
                                        loom::Accept<GuestConnectionsRequested>,
                                        loom::Emit<GuestConnections, input::InputSessionClosed>> {
public:
    /// `listener` is a listening socket the host opened (bridge_listen_tcp); `listen` is how
    /// the host spells where it listens, for the inventory. The bus must outlive the door.
    GuestDoor(loom::Switchboard& bus, loom::socket_t listener, std::string listen,
              loom::BridgeAdmission admission)
        : listen_(std::move(listen)),
          server_(std::make_unique<loom::BridgeServer>(bus, listener, std::move(admission))) {
        server_->on_connection([this](const loom::Connection& c) { on_connection(c); });
        beat_ = this->timers().repeat_to_role(kGuestBeatId, std::chrono::milliseconds(kGuestBeatMs),
                                              kGuestsRole, &GuestDoor::on_beat);
    }

    using zengine::timer::TimedWeave<GuestDoor, GuestDoorState,
                                     loom::Accept<GuestConnectionsRequested>,
                                     loom::Emit<GuestConnections, input::InputSessionClosed>>::on;

    /// A presenter that just arrived asks; it is answered with the inventory as it stands.
    void on(const GuestConnectionsRequested&, loom::Mail& mail) {
        (void)mail.answer(inventory());
    }

    /// THE DECISION SEAM, after the fact: a connection the policy deferred is admitted or
    /// refused here. Called by the host (a console command, a suite, tomorrow a popup).
    bool decide(std::uint64_t connection, const loom::ConnectionVerdict& verdict) {
        return server_->decide(connection, verdict);
    }
    bool disconnect(std::uint64_t connection) { return server_->disconnect(connection); }

    /// Told, on the door's beat, of every guest session that has gone -- host wiring for what else
    /// held something on that guest's behalf (the observation relay forgets its subscriptions).
    void when_gone(std::function<void(loom::WeaveId)> tell) { when_gone_ = std::move(tell); }

    /// The name this door established for `session`, or empty when no live connection is it.
    std::string established(loom::WeaveId session) const {
        for (const loom::Connection& c : server_->connections()) {
            if (c.session == session && c.state == loom::ConnectionState::Admitted) {
                return c.established_name;
            }
        }
        return {};
    }

    /// Service the crossing once, outside the beat -- for a host or a suite that turns the
    /// bus itself. Inside a beat the door does this on its own.
    void service() { server_->service(); }

    loom::BridgeServer& server() { return *server_; }
    GuestConnections inventory() const {
        GuestConnections said;
        said.listen = listen_;
        said.refused = static_cast<std::int64_t>(server_->refused_count());
        said.shed = static_cast<std::int64_t>(server_->declined_count());
        for (const loom::Connection& c : server_->connections()) {
            said.rows.push_back(row_of(c));
        }
        for (const loom::Connection& c : ended_) {
            said.rows.push_back(row_of(c));
        }
        return said;
    }

private:
    static GuestConnection row_of(const loom::Connection& c) {
        GuestConnection row;
        row.connection = static_cast<std::int64_t>(c.connection);
        row.state = loom::name_of(c.state);
        row.claimed = c.claimed_name;
        row.established = c.established_name;
        row.peer = c.peer;
        row.session = static_cast<std::int64_t>(c.session.value);
        row.refusal = c.refusal;
        return row;
    }

    /// The server's word about a connection, from inside `service()`: remembered, and acted on
    /// at the end of the beat where there is a Mail to act with.
    void on_connection(const loom::Connection& c) {
        dirty_ = true;
        if (c.state == loom::ConnectionState::Closed) {
            // The server drops a closed connection from its list in the same service; the
            // inventory says it `closed` ONCE, from this word, and then drops it too.
            ended_.push_back(c);
            if (c.session.valid()) {
                gone_.push_back(c.session);
            }
        }
    }

    void on_beat(const zengine::timer::TimerFired&, loom::Mail& mail) {
        ++this->state_.services;
        server_->service();
        for (const loom::WeaveId& session : gone_) {
            // THE GUEST IS GONE; WHATEVER INPUT SESSION IT HELD ENDS WITH IT, closed on its behalf
            // by this office -- the one closer the Input weave accepts beside the holder itself.
            // A guest that held none is told nothing: the refusal comes back to this door and
            // is data, not a fault.
            input::InputSessionClosed close;
            close.session = 0;
            close.holder = static_cast<std::int64_t>(session.value);
            (void)mail.as_role(kGuestsRole).send_to_role(input::kInputRole, close);
            if (when_gone_) {
                when_gone_(session);
            }
        }
        gone_.clear();
        if (dirty_) {
            dirty_ = false;
            ++this->state_.said;
            (void)mail.as_role(kGuestsRole).publish(inventory());
            ended_.clear(); // said once
        }
    }

    std::string listen_;
    std::unique_ptr<loom::BridgeServer> server_;
    typename zengine::timer::TimedWeave<GuestDoor, GuestDoorState,
                                        loom::Accept<GuestConnectionsRequested>,
                                        loom::Emit<GuestConnections, input::InputSessionClosed>>::Handle
        beat_;
    bool dirty_ = true; ///< the first beat says the inventory once, empty or not
    std::vector<loom::WeaveId> gone_;
    std::function<void(loom::WeaveId)> when_gone_;
    std::vector<loom::Connection> ended_; ///< closed since the last inventory, said once
};

/// THE DOOR'S GRANT: its beat, its inventory, and one word to the Input office. Nothing else.
inline loom::Grant guest_door_grant() {
    loom::Grant g;
    g.allow_to_role(zengine::timer::EnsureRoleTimer::zen_name,
                    zengine::timer::EnsureRoleTimer::zen_version, zengine::timer::kTimerRole);
    g.allow_to_role(zengine::timer::EnsureTimer::zen_name, zengine::timer::EnsureTimer::zen_version,
                    zengine::timer::kTimerRole);
    g.allow_to_role(zengine::timer::CancelTimer::zen_name, zengine::timer::CancelTimer::zen_version,
                    zengine::timer::kTimerRole);
    g.allow_to_any(GuestConnections::zen_name, GuestConnections::zen_version);
    g.allow_to_role(input::InputSessionClosed::zen_name, input::InputSessionClosed::zen_version,
                    input::kInputRole);
    loom::allow_poke_answers(g);
    return g;
}

/// THE OBSERVATION RELAY BESIDE THIS DOOR (loom's zen/observe/relay.hpp): a guest observes only
/// what its row's `observe` list names (`guests::observation_of`), `cause` is read from the fences
/// this door's server opened, and a gone session's subscriptions are forgotten. The pointer is for
/// the host's own calls (`revoke`); the door must outlive the relay's use of it
/// (docs/workshop/external-host.md).
inline loom::observe::Relay* mount_observation(loom::Switchboard& bus, GuestDoor& door,
                                                const guests::GuestsFile& file) {
    GuestDoor* d = &door;
    loom::observe::Relay* relay = loom::observe::mount_relay(
        bus, guests::observation_of(file, [d](loom::WeaveId s) { return d->established(s); }),
        [d](loom::Fence f) -> std::optional<loom::observe::FenceOrigin> {
            const auto from = d->server().settle_origin(f);
            if (!from) {
                return std::nullopt;
            }
            return loom::observe::FenceOrigin{from->session, from->correlation};
        });
    door.when_gone([relay](loom::WeaveId session) { (void)relay->forget(session); });
    return relay;
}

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_GUEST_DOOR_HPP
