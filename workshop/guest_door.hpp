// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_GUEST_DOOR_HPP
#define ZENGINE_WORKSHOP_GUEST_DOOR_HPP

// The guest door: the one participant holding this Workshop's listener, its admission policy, its
// connection inventory and each admitted session's row. A weave, not a loop: the host loop never
// returns while a Timer beats, so the door services the bridge on a repeating beat, from inside
// the bus. Not authority: every guest's send is checked at the bus under the guest's own grant.

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
#include <map>
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
                                        loom::Accept<GuestConnectionsRequested,
                                                     v2::GuestConnectionsRequested,
                                                     GuestRowDescribedRequested>,
                                        loom::Emit<GuestConnections, v2::GuestConnections,
                                                   GuestRowDescribed, loom::Refused,
                                                   input::InputSessionClosed>> {
public:
    /// `listener` is a listening socket the host opened (bridge_listen_tcp); `listen` is how
    /// the host spells where it listens, for the inventory; `file` is the guests file whose rows
    /// are the policy, each session's row recorded as it is admitted. The bus must outlive the door.
    GuestDoor(loom::Switchboard& bus, loom::socket_t listener, std::string listen,
              guests::GuestsFile file)
        : listen_(std::move(listen)), file_(std::move(file)),
          admitted_(std::make_shared<guests::AdmittedRows>()),
          server_(std::make_unique<loom::BridgeServer>(bus, listener,
                                                       guests::admission_of(file_, admitted_))) {
        server_->on_connection([this](const loom::Connection& c) { on_connection(c); });
        beat_ = this->timers().repeat_to_role(kGuestBeatId, std::chrono::milliseconds(kGuestBeatMs),
                                              kGuestsRole, &GuestDoor::on_beat);
    }

    using zengine::timer::TimedWeave<GuestDoor, GuestDoorState,
                                     loom::Accept<GuestConnectionsRequested,
                                                  v2::GuestConnectionsRequested,
                                                  GuestRowDescribedRequested>,
                                     loom::Emit<GuestConnections, v2::GuestConnections,
                                                GuestRowDescribed, loom::Refused,
                                                input::InputSessionClosed>>::on;

    /// A presenter that just arrived asks; it is answered with the inventory as it stands.
    void on(const GuestConnectionsRequested&, loom::Mail& mail) {
        (void)mail.answer(inventory());
    }

    /// ...AT VERSION 2: every row's powers and losses to this host's own participants, and to a
    /// guest only its own -- one guest never learns another's grants.
    // WL-GUEST-07 -- agents/workshop/guests.md
    void on(const v2::GuestConnectionsRequested&, loom::Mail& mail) {
        (void)mail.answer(inventory_v2(guest_session(mail.sender()) ? mail.sender() : loom::WeaveId{}));
    }

    /// THE ASKER'S OWN ROW, read from the record this door kept as it admitted the session -- never
    /// a row found by name -- and nothing of another. A sender it never admitted is refused.
    // WL-GUEST-10 -- agents/workshop/guests.md
    void on(const GuestRowDescribedRequested&, loom::Mail& mail) {
        const std::optional<std::size_t> row = row_index(mail.sender());
        if (!row || *row >= file_.rows.size()) {
            (void)mail.answer(
                loom::Refused{"only a session this door admitted has a row to describe"});
            return;
        }
        (void)mail.answer(described(file_.rows[*row]));
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

    /// WHICH ROW ADMITTED `session`, from the record the policy kept as it admitted the connection:
    /// never a row found by name. A session whose connection has gone is still its row's (an ask
    /// about it may yet be on its way); empty only for a session this door never admitted.
    std::optional<std::size_t> row_index(loom::WeaveId session) const {
        for (const loom::Connection& c : server_->connections()) {
            if (c.session == session && c.state == loom::ConnectionState::Admitted) {
                return admitted_->row_of(c.connection);
            }
        }
        if (const auto it = sessions_.find(session.value); it != sessions_.end()) return it->second;
        return std::nullopt;
    }

    /// The facts the action classes read for `session` (`actor_scope.hpp`); not admitted only for a
    /// session this door never admitted.
    scope::GuestRowFacts facts(loom::WeaveId session) const {
        const std::optional<std::size_t> row = row_index(session);
        if (!row || *row >= file_.rows.size()) return {};
        return guests::facts_of(file_.rows[*row], file_);
    }

    /// The guests file this door admits by.
    const guests::GuestsFile& file() const { return file_; }

    /// Service the crossing once, outside the beat -- for a host or a suite that turns the
    /// bus itself. Inside a beat the door does this on its own.
    void service() { server_->service(); }

    loom::BridgeServer& server() { return *server_; }

    /// Whether `who` is a session this door admitted: a guest, not one of this host's own.
    bool guest_session(loom::WeaveId who) const {
        for (const loom::Connection& c : server_->connections()) {
            if (c.session == who && c.state == loom::ConnectionState::Admitted) return true;
        }
        return false;
    }

    /// The inventory at version 2. `only` names the one session whose row's powers and losses are
    /// said; an invalid id says every row's.
    v2::GuestConnections inventory_v2(loom::WeaveId only = {}) const {
        v2::GuestConnections said;
        said.listen = listen_;
        said.refused = static_cast<std::int64_t>(server_->refused_count());
        said.shed = static_cast<std::int64_t>(server_->declined_count());
        for (const loom::Connection& c : server_->connections()) {
            said.rows.push_back(row_v2_of(c, only));
        }
        for (const loom::Connection& c : ended_) {
            said.rows.push_back(row_v2_of(c, only));
        }
        return said;
    }

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
    /// One row as its session is told it: its name, powers and observe list, and the file's
    /// `host`; never its credential.
    GuestRowDescribed described(const guests::GuestRow& row) const {
        GuestRowDescribed said;
        said.name = row.name;
        said.may = row.may;
        for (const guests::ObserveScope& s : row.observe) {
            GuestObserve seen;
            seen.producer = s.producer;
            seen.shape = s.shape;
            seen.version = s.version;
            said.observe.push_back(std::move(seen));
        }
        said.host = file_.host;
        return said;
    }

    v2::GuestConnection row_v2_of(const loom::Connection& c, loom::WeaveId only) const {
        const GuestConnection was = row_of(c);
        v2::GuestConnection row;
        row.connection = was.connection;
        row.state = was.state;
        row.claimed = was.claimed;
        row.established = was.established;
        row.peer = was.peer;
        row.session = was.session;
        row.refusal = was.refusal;
        const std::optional<std::size_t> index = admitted_->row_of(c.connection);
        if (index && *index < file_.rows.size() && (!only.valid() || c.session == only)) {
            const guests::GuestRow& admitted = file_.rows[*index];
            row.powers = admitted.may;
            row.host = file_.host;
            row.version = file_.version;
            row.losses = guests::losses_of(admitted, file_);
        }
        return row;
    }

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
        if (c.state == loom::ConnectionState::Admitted && c.session.valid()) {
            if (const std::optional<std::size_t> row = admitted_->row_of(c.connection)) {
                remember(c.session, *row);
            }
        }
        if (c.state == loom::ConnectionState::Closed || c.state == loom::ConnectionState::Refused) {
            let_go_.push_back(c.connection);
        }
        if (c.state == loom::ConnectionState::Closed) {
            // The server drops a closed connection from its list in the same service; the
            // inventory says it `closed` ONCE, from this word, and then drops it too.
            ended_.push_back(c);
            if (c.session.valid()) {
                gone_.push_back(c.session);
            }
        }
    }

    void remember(loom::WeaveId session, std::size_t row) { sessions_[session.value] = row; }

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
            (void)mail.as_role(kGuestsRole).publish(inventory_v2());
            ended_.clear(); // said once
        }
        // A CONNECTION THE SERVER LET GO HOLDS NO ROW: the record keeps only the live ones.
        for (const std::uint64_t connection : let_go_) admitted_->forget(connection);
        let_go_.clear();
    }

    std::string listen_;
    guests::GuestsFile file_;
    std::shared_ptr<guests::AdmittedRows> admitted_;
    std::unique_ptr<loom::BridgeServer> server_;
    typename zengine::timer::TimedWeave<GuestDoor, GuestDoorState,
                                        loom::Accept<GuestConnectionsRequested,
                                                     v2::GuestConnectionsRequested,
                                                     GuestRowDescribedRequested>,
                                        loom::Emit<GuestConnections, v2::GuestConnections,
                                                   GuestRowDescribed, loom::Refused,
                                                   input::InputSessionClosed>>::Handle
        beat_;
    bool dirty_ = true; ///< the first beat says the inventory once, empty or not
    std::vector<loom::WeaveId> gone_;
    std::function<void(loom::WeaveId)> when_gone_;
    std::vector<loom::Connection> ended_; ///< closed since the last inventory, said once
    std::vector<std::uint64_t> let_go_;   ///< closed or refused since the last beat: their rows forgotten
    /// EVERY SESSION THIS DOOR ADMITTED, AND ITS ROW, kept past its connection for the process's
    /// life: an ask already on its way about a guest that has since gone is judged as that guest's,
    /// never as no guest's, and forgetting one would make it no guest's. One entry per session a
    /// credential admitted; no session's id is ever another's.
    std::map<std::uint64_t, std::size_t> sessions_;
};

/// THE DOOR'S GRANT: its beat, its inventory, an asker's own row or its refusal, and one word to
/// the Input office. Nothing else.
inline loom::Grant guest_door_grant() {
    loom::Grant g;
    g.allow_to_role(zengine::timer::EnsureRoleTimer::zen_name,
                    zengine::timer::EnsureRoleTimer::zen_version, zengine::timer::kTimerRole);
    g.allow_to_role(zengine::timer::EnsureTimer::zen_name, zengine::timer::EnsureTimer::zen_version,
                    zengine::timer::kTimerRole);
    g.allow_to_role(zengine::timer::CancelTimer::zen_name, zengine::timer::CancelTimer::zen_version,
                    zengine::timer::kTimerRole);
    g.allow_to_any(GuestConnections::zen_name, GuestConnections::zen_version);
    g.allow_to_any(v2::GuestConnections::zen_name, v2::GuestConnections::zen_version);
    g.allow_to_any(GuestRowDescribed::zen_name, GuestRowDescribed::zen_version);
    g.allow_to_any(loom::Refused::zen_name, loom::Refused::zen_version);
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
        bus, guests::observation_of(file, [d](loom::WeaveId s) { return d->row_index(s); }),
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
