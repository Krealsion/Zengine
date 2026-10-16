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
#include "pane_menu.hpp"      // the office Workshop answers to, the one asker of a scope here
#include "pane_operation.hpp" // the question about a sender's scope, and its answer

#include "input/vocabulary.hpp"
#include "manual/vocabulary.hpp"
#include "timer/binding.hpp"

#include "manuals/zengine.guests.hpp" // this office's manual, compiled in from its page

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
/// How many asks for the plan at rest the door holds at once.
inline constexpr std::size_t kGuestPlanHeld = 32;

/// Two honest counters, poke-inspectable like any state.
struct GuestDoorState {
    std::int64_t services = 0; ///< beats on which the crossing was serviced
    std::int64_t said = 0;     ///< inventories published
    ZEN_EXPOSE();
    ZEN_SHAPE(GuestDoorState, 1, ZEN_FIELD(services), ZEN_FIELD(said));
};

class GuestDoor;
using GuestDoorBase =
    zengine::With<manual::ManualFacet<manual::pages::zengine_guests::kManual>>::Weave<
    GuestDoor, GuestDoorState,
    loom::Accept<GuestConnectionsRequested, v2::GuestConnectionsRequested,
                 v3::GuestConnectionsRequested, GuestRowDescribedRequested,
                 v2::GuestRowDescribedRequested, GuestPlanRequested, ActorScopeRequested>,
    loom::Emit<GuestConnections, v2::GuestConnections, v3::GuestConnections, GuestRowDescribed,
               v2::GuestRowDescribed, GuestPlanSaid, ActorScopeJudged, loom::Refused,
               input::InputSessionClosed>,
    zengine::timer::TimedWeave>;

class GuestDoor final : public GuestDoorBase {
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

    using GuestDoorBase::on;

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

    /// ...AT VERSION 3, each row's role and scopes beside its powers and losses, by the same rule.
    // WL-GUIDE-01 -- agents/workshop/guide.md
    void on(const v3::GuestConnectionsRequested&, loom::Mail& mail) {
        (void)mail.answer(inventory_v3(guest_session(mail.sender()) ? mail.sender() : loom::WeaveId{}));
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

    /// ...AT VERSION 2, with the file's version and the row's role and doors.
    // WL-GUIDE-01 -- agents/workshop/guide.md
    void on(const v2::GuestRowDescribedRequested&, loom::Mail& mail) {
        const std::optional<std::size_t> row = row_index(mail.sender());
        if (!row || *row >= file_.rows.size()) {
            (void)mail.answer(
                loom::Refused{"only a session this door admitted has a row to describe"});
            return;
        }
        const guests::GuestRow& r = file_.rows[*row];
        const GuestRowDescribed first = described(r);
        v2::GuestRowDescribed said;
        said.name = first.name;
        said.may = first.may;
        said.observe = first.observe;
        said.host = first.host;
        said.version = file_.version;
        said.role = r.role;
        said.by_door = r.by_door;
        said.capture = r.capture;
        said.guide = r.guide;
        (void)mail.answer(said);
    }

    /// WHERE THE PLAN IS, from the reading the host wired: at once, or with `wait` held until the
    /// plan is at rest and answered on the door's beat. The rows the answer names reach a guest only
    /// when its row already shows rows' names. Held asks are bounded; past the bound, refused.
    // WL-GUEST-12 -- agents/workshop/guests.md
    void on(const GuestPlanRequested& asked, loom::Mail& mail) {
        if (!plan_) {
            (void)mail.answer(loom::Refused{"this host wired no reading of its load plan"});
            return;
        }
        const GuestPlanSaid now = plan_();
        if (!asked.wait || now.at_rest) {
            (void)mail.answer(plan_for(mail.sender(), now));
            return;
        }
        if (held_.size() >= kGuestPlanHeld) {
            (void)mail.answer(loom::Refused{
                "the guest door already holds " + std::to_string(kGuestPlanHeld) +
                " asks for the plan at rest; ask again without wait, or once one is answered"});
            return;
        }
        loom::DeferredAnswer reply = mail.defer_answer();
        if (!reply.valid()) {
            // Loom kept the answer that could not be held: say so with it.
            (void)mail.answer(loom::Refused{"the bus could not hold this ask for the plan at rest; "
                                            "ask again without wait"});
            return;
        }
        held_.push_back(HeldPlanAsk{mail.sender(), std::move(reply)});
    }

    /// WHETHER A SENDER'S HAND MAY ACT IN THE CLASSES `asked` NAMES, asked by Workshop's office:
    /// judged against the row this door admitted the session under and the file's host, as
    /// Workshop judges it. A participant this door never admitted, the weaver's own, is not
    /// narrowed; nothing is remembered.
    // WL-GUEST-04 -- agents/workshop/guests.md
    void on(const ActorScopeRequested& asked, loom::Mail& mail) {
        if (!mail.authored_from_role(pane_menu::kWorkshopRole)) {
            return;
        }
        const scope::GuestRowFacts row =
            asked.session > 0 ? facts(loom::WeaveId{static_cast<std::uint64_t>(asked.session)})
                              : scope::GuestRowFacts{};
        const std::string refusal =
            asked.classes.empty()
                ? std::string("the question names no action class")
                : scope::judge_all(row, guests::host_fact_of(file_), asked.classes);
        (void)mail.answer(ActorScopeJudged{refusal.empty(), refusal});
    }

    /// The host's reading of its realization owner, read at every ask and on every beat that
    /// holds an ask; it keeps no copy of the plan.
    using PlanReading = std::function<GuestPlanSaid()>;
    void reads_plan(PlanReading reading) { plan_ = std::move(reading); }

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

    /// The inventory at version 3: version 2's, each row's role and scopes beside.
    v3::GuestConnections inventory_v3(loom::WeaveId only = {}) const {
        const v2::GuestConnections second = inventory_v2(only);
        v3::GuestConnections said;
        said.listen = second.listen;
        said.refused = second.refused;
        said.shed = second.shed;
        std::size_t i = 0;
        const auto add = [&](const loom::Connection& c) {
            const v2::GuestConnection& was = second.rows[i++];
            v3::GuestConnection row{was.connection, was.state,  was.claimed, was.established,
                                    was.peer,       was.session, was.refusal, was.powers,
                                    was.host,       was.version, was.losses,  {},
                                    {}};
            const std::optional<std::size_t> index = admitted_->row_of(c.connection);
            if (index && *index < file_.rows.size() && (!only.valid() || c.session == only)) {
                row.role = file_.rows[*index].role;
                row.scopes = guests::scopes_of(file_.rows[*index]);
            }
            said.rows.push_back(std::move(row));
        };
        for (const loom::Connection& c : server_->connections()) add(c);
        for (const loom::Connection& c : ended_) add(c);
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
    /// One ask for the plan at rest, held until it is.
    struct HeldPlanAsk {
        loom::WeaveId asker;
        loom::DeferredAnswer reply;
    };

    /// The plan as `asker` is told it: the rows named only to a guest whose row already shows
    /// rows' names, and to this host's own participants.
    GuestPlanSaid plan_for(loom::WeaveId asker, GuestPlanSaid said) const {
        const std::optional<std::size_t> row = row_index(asker);
        if (row && (*row >= file_.rows.size() || !guests::reads_row_names(file_.rows[*row]))) {
            said.waiting_on.clear();
            said.unavailable.clear();
        }
        return said;
    }

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
            // ITS HELD ASK FOR THE PLAN IS LET GO: there is nobody left to answer.
            for (auto it = held_.begin(); it != held_.end();) {
                if (it->asker == session) {
                    loom::release_deferred(it->reply, mail);
                    it = held_.erase(it);
                } else {
                    ++it;
                }
            }
        }
        gone_.clear();
        // THE PLAN AT REST ANSWERS EVERY ASK HELD FOR IT, from this delivery: a deferred answer is
        // spent by the incarnation that deferred it.
        if (!held_.empty() && plan_) {
            const GuestPlanSaid now = plan_();
            if (now.at_rest) {
                for (const HeldPlanAsk& held : held_) {
                    (void)loom::answer_deferred(held.reply, mail, plan_for(held.asker, now));
                }
                held_.clear();
            }
        }
        if (dirty_) {
            dirty_ = false;
            ++this->state_.said;
            (void)mail.as_role(kGuestsRole).publish(inventory());
            (void)mail.as_role(kGuestsRole).publish(inventory_v2());
            (void)mail.as_role(kGuestsRole).publish(inventory_v3());
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
    GuestDoorBase::Handle beat_;
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
    PlanReading plan_;
    std::vector<HeldPlanAsk> held_; ///< asks for the plan at rest, at most `kGuestPlanHeld`
};

/// THE DOOR'S GRANT: its beat, its inventory, an asker's own row, where the plan is, a sender's
/// scope judged, a refusal, what a shape it accepts does, and one word to the Input office.
/// Nothing else.
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
    g.allow_to_any(v3::GuestConnections::zen_name, v3::GuestConnections::zen_version);
    g.allow_to_any(GuestRowDescribed::zen_name, GuestRowDescribed::zen_version);
    g.allow_to_any(v2::GuestRowDescribed::zen_name, v2::GuestRowDescribed::zen_version);
    g.allow_to_any(GuestPlanSaid::zen_name, GuestPlanSaid::zen_version);
    g.allow_to_any(ActorScopeJudged::zen_name, ActorScopeJudged::zen_version);
    g.allow_to_any(loom::Refused::zen_name, loom::Refused::zen_version);
    g.allow_to_any(manual::ShapeDocumentShown::zen_name, manual::ShapeDocumentShown::zen_version);
    g.allow_to_role(input::InputSessionClosed::zen_name, input::InputSessionClosed::zen_version,
                    input::kInputRole);
    loom::allow_poke_answers(g);
    return g;
}

/// THE OBSERVATION RELAY BESIDE THIS DOOR (loom's zen/observe/relay.hpp): a guest observes only
/// what its row's `observe` list names (`guests::observation_of`), `cause` is read from the fences
/// this door's server opened, and a gone session's subscriptions are forgotten. The pointer is for
/// the host's own calls (`revoke`); the door must outlive the relay's use of it
/// (external-host/docs/external-host.md).
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
