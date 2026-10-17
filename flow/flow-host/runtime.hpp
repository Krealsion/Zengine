// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_FLOW_HOST_RUNTIME_HPP
#define ZENGINE_FLOW_HOST_RUNTIME_HPP

// The Flow host: interpreted Flow projects run as ordinary maker participants on the host's own
// bus and catalog, each in a session of its asker's. Reference: flow/docs/flow-runtime.md; its
// office's manual is flow/docs/zengine.flow.host.md.

#include "activation/activation.hpp"
#include "flow/flow-host/vocabulary.hpp"
#include "flow/project.hpp"
#include "maker/weave.hpp"
#include "manual/vocabulary.hpp"
#include "operator/host.hpp"
#include "timer/binding.hpp"

#include "manuals/zengine.flow.host.hpp" // this office's manual, compiled in from its page

#include <algorithm>
#include <deque>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace zengine::flow_host {
inline constexpr std::size_t kMaxSessions = 8;
inline constexpr std::size_t kMaxPendingInputs = 16;
inline constexpr std::size_t kMaxEvents = 128;
inline constexpr std::size_t kMaxEventBytes = 64u << 10;
inline constexpr std::size_t kMaxJournalBytes = 256u << 10;

inline loom::Bytes bytes(std::string_view text) { return {text.begin(), text.end()}; }
inline std::string_view view(const loom::Bytes& data) {
    return {reinterpret_cast<const char*>(data.data()), data.size()};
}

namespace detail {
struct FlowHostPulse {
    std::string subject;
    ZEN_SHAPE(FlowHostPulse, 1, ZEN_FIELD(subject));
};
/// The state of the host's own weaves, its office and its clock: none. Their sessions and beat
/// are the host's, never reload-kept state.
struct FlowHostClockState { ZEN_SHAPE(FlowHostClockState, 1); };
struct Observation {
    std::deque<FlowEvent> events;
    std::int64_t next = 1;
    std::int64_t dropped = 0;
    std::size_t kept_bytes = 0;
    bool changed = false;

    void record(std::string kind, std::uint64_t correlation, loom::Bytes payload = {},
                std::string text = {}) {
        if (payload.size() > kMaxEventBytes) {
            payload.clear();
            text += " [payload omitted: observation exceeds 64 KiB]";
        }
        if (text.size() > kMaxEventBytes) text.resize(kMaxEventBytes);
        FlowEvent event{next++, std::move(kind), std::to_string(correlation),
                        std::move(payload), std::move(text)};
        const auto size = event.payload.size() + event.detail.size();
        while (!events.empty() && (events.size() >= kMaxEvents || kept_bytes + size > kMaxJournalBytes)) {
            kept_bytes -= events.front().payload.size() + events.front().detail.size();
            events.pop_front();
            ++dropped;
        }
        kept_bytes += size;
        events.push_back(std::move(event));
        changed = true;
    }
};

// This ordinary participant is the current sender of injected examples. Its grant
// names only this subject and this definition's accepted shapes. It also receives
// outputs without granting itself authority to anybody else's messages.
class Observer final : public loom::Weave {
public:
    Observer(loom::WeaveId subject, std::vector<std::shared_ptr<const loom::Schema>> shapes,
             std::shared_ptr<Observation> observation)
        : subject_(subject), shapes_(std::move(shapes)), observation_(std::move(observation)) {
        shapes_.push_back(loom::schema_of<loom::Refused>());
    }
    std::vector<std::shared_ptr<const loom::Schema>> accepted_schemas() const override { return shapes_; }
    void handle(const loom::Message& message, loom::Bus&) override {
        if (message.sender != subject_) return;
        const bool refused = loom::same_identity(message.payload.schema(), *loom::schema_of<loom::Refused>());
        observation_->record(refused ? "refused" : "output", message.correlation,
            bytes(loom::serialize(message.payload)),
            refused ? loom::from_value<loom::Refused>(message.payload).reason : message.payload.schema().name());
    }
    loom::Value snapshot() const override {
        return loom::Value(loom::make_schema("zengine.flow.Observer", 1, {}));
    }
    loom::Value policy() const override { return maker::default_value(loom::lifecycle_policy_schema()); }
    void revive(const loom::Value&) override {}
private:
    loom::WeaveId subject_;
    std::vector<std::shared_ptr<const loom::Schema>> shapes_;
    std::shared_ptr<Observation> observation_;
};
} // namespace detail

class Manager;

// Host-owned adapter: references the host's existing bus and catalog, never owns a
// second host. Declare after both, so destruction unregisters its weaves first.
// The ordinary Manager owns session custody and user command policy. These methods
// are host authority and are not offered to a loaded pane as pointers.
class RuntimeHost {
public:
    RuntimeHost(loom::Switchboard& bus, op::Catalog& catalog) : bus_(bus), catalog_(catalog) {
        observer_ = bus_.add_observer([this](const loom::BusEvent& event) { observed(event); });
    }
    RuntimeHost(const RuntimeHost&) = delete;
    RuntimeHost& operator=(const RuntimeHost&) = delete;
    ~RuntimeHost() {
        bus_.remove_observer(observer_);
        if (clock_.valid()) bus_.unregister_weave(clock_);
        if (manager_.valid()) bus_.unregister_weave(manager_);
        while (!sessions_.empty()) remove(sessions_.begin()->first);
    }
    loom::WeaveId mount();
    loom::WeaveId id() const noexcept { return manager_; }

    // Call once per ordinary host turn. This never pumps, waits or dispatches.
    // Notifications are queued outside input fences so a pane's inspection cannot
    // become part of the very fence whose completion it is trying to display.
    void poll() {
        for (auto& [subject, session] : sessions_) {
            for (auto it = session.fences.begin(); it != session.fences.end();) {
                if (bus_.fence_state(it->fence) != loom::FenceState::Open) {
                    bus_.release_fence(it->fence);
                    it = session.fences.erase(it);
                    session.observation->changed = true;
                } else ++it;
            }
            if (session.observation->changed && manager_.valid()) {
                const auto ticket = bus_.send_as_to_role(manager_, kFlowHostRole, loom::Message(
                    loom::to_value(detail::FlowHostPulse{std::to_string(subject)}), manager_));
                if (ticket.valid()) session.observation->changed = false;
            }
        }
    }

private:
    friend class Manager;
    struct Pending {
        loom::Fence fence;
        loom::Ticket ticket;
        std::uint64_t correlation = 0;
    };
    struct Session {
        maker::Definition definition;
        loom::WeaveId subject{}, sender{};
        std::shared_ptr<detail::Observation> observation;
        std::vector<Pending> fences;
    };
    loom::Switchboard& bus_;
    op::Catalog& catalog_;
    loom::WeaveId manager_{}, clock_{};
    loom::ObserverId observer_{};
    std::map<std::uint64_t, Session> sessions_;

    std::uint64_t create(flow::Project project) {
        if (bus_.role_holder(project.definition.name).valid())
            throw std::invalid_argument("the definition's role is already held: " + project.definition.name);
        auto registered = maker::register_definition(bus_, catalog_, project.definition);
        if (!registered) throw std::runtime_error(registered.reason);
        loom::WeaveId sender;
        try {
            const auto restored = bus_.swap_state(registered.id, loom::serialize(project.state));
            if (!restored.revived) throw std::runtime_error(restored.refusal.message());
            auto observation = std::make_shared<detail::Observation>();
            loom::Grant grant;
            for (const auto& shape : project.definition.accepts)
                grant.allow(shape->name(), shape->version(), registered.id);
            sender = bus_.register_weave(std::make_unique<detail::Observer>(registered.id,
                project.definition.emits, observation), std::move(grant));
            sessions_.emplace(registered.id.value, Session{std::move(project.definition),
                registered.id, sender, std::move(observation), {}});
        } catch (...) {
            if (sender.valid()) bus_.unregister_weave(sender);
            bus_.unregister_weave(registered.id);
            throw;
        }
        return registered.id.value;
    }
    void remove(std::uint64_t subject) {
        auto found = sessions_.find(subject);
        if (found == sessions_.end()) return;
        for (const auto& pending : found->second.fences) bus_.release_fence(pending.fence);
        bus_.unregister_weave(found->second.sender);
        bus_.unregister_weave(found->second.subject);
        sessions_.erase(found);
    }
    Session& session(std::uint64_t id) {
        auto found = sessions_.find(id);
        if (found == sessions_.end()) throw std::invalid_argument("managed subject no longer exists");
        return found->second;
    }
    std::size_t pending(Session& s) const {
        return static_cast<std::size_t>(std::count_if(s.fences.begin(), s.fences.end(),
            [this](const Pending& input) { return bus_.fence_state(input.fence) == loom::FenceState::Open; }));
    }
    void apply(std::uint64_t id, maker::Definition definition) {
        auto& s = session(id);
        if (pending(s) != 0) throw std::invalid_argument("inputs still have pending dispatch; try the edit after they settle");
        auto result = maker::apply_behaviour_edit(bus_, catalog_, s.subject, definition);
        if (!result) throw std::runtime_error(result.reason);
        s.definition = std::move(definition);
        s.observation->changed = true;
    }
    void send(std::uint64_t id, const loom::Bytes& payload, std::uint64_t correlation) {
        auto& s = session(id);
        if (s.fences.size() >= kMaxPendingInputs)
            throw std::invalid_argument("this session already retains 16 inputs; let the host advance before sending more");
        if (payload.size() > maker::kMaxFileBytes) throw std::invalid_argument("input exceeds 1 MiB");
        const auto parsed = loom::parse(view(payload));
        std::optional<loom::Value> value;
        std::string reason = "input does not match a declared accepted schema";
        for (const auto& shape : s.definition.accepts) {
            auto admitted = loom::admit(parsed, shape);
            if (admitted) { value = std::move(admitted).value(); break; }
            reason = admitted.first_error().message();
        }
        if (!value) throw std::invalid_argument(reason);
        loom::Fence fence;
        // Host injection deliberately retains a concrete, grant-limited participant.
        // Payload files can never select the sender, target or reply authority.
        const auto ticket = bus_.send_as_fenced(s.sender, s.subject,
            loom::Message(std::move(*value), s.sender, {}, correlation), &fence);
        if (!ticket.valid()) throw std::runtime_error("input was not queued (host dispatch fence unavailable)");
        s.fences.push_back({fence, ticket, correlation});
        s.observation->record("queued", correlation, payload, "input queued; delivery and behavior are separate facts");
    }
    void observed(const loom::BusEvent& event) {
        // Preserve these direct-attempt facts at delivery, before the global outcome
        // journal can roll. Scope is our own inputs, never unrelated host traffic.
        const auto found = sessions_.find(event.target.value);
        if (found == sessions_.end() || event.sender != found->second.sender) return;
        auto& s = found->second;
        const auto attempt = std::find_if(s.fences.begin(), s.fences.end(),
            [&event](const Pending& p) { return p.ticket.seq == event.seq; });
        if (attempt == s.fences.end()) return;
        if (event.kind == loom::EventKind::Delivered)
            s.observation->record("dispatched", event.correlation, {}, "input handler returned; inspect outputs and state for its result");
        else if (event.kind == loom::EventKind::Refused)
            s.observation->record("dispatch-refused", event.correlation, {}, event.refusal.message());
        else if (event.kind == loom::EventKind::HandlerFailed)
            s.observation->record("handler-failed", event.correlation, {}, "input handler did not complete normally");
    }
    void fill(FlowAnswer& answer, std::uint64_t id, std::int64_t after = 0) {
        auto& s = session(id);
        auto* live = dynamic_cast<maker::Weave*>(bus_.weave(s.subject));
        if (!live) throw std::runtime_error("the managed maker weave is no longer live");
        answer.subject = std::to_string(id);
        const auto state = loom::admit(loom::parse(bus_.snapshot_bytes(s.subject)), s.definition.state);
        if (!state) throw std::runtime_error(state.first_error().message());
        answer.project = bytes(flow::project_bytes({s.definition, state.value()}));
        answer.pending = static_cast<std::int64_t>(pending(s));
        answer.first_sequence = s.observation->events.empty() ? s.observation->next : s.observation->events.front().sequence;
        answer.last_sequence = s.observation->next - 1;
        answer.dropped = s.observation->dropped;
        for (const auto& event : s.observation->events)
            if (event.sequence > after) answer.events.push_back(event);
    }
    FlowCatalogAnswer catalog() const {
        FlowCatalogAnswer answer;
        std::size_t total = 0;
        for (const auto& identity : catalog_.identities()) {
            const auto* definition = catalog_.find(identity);
            if (!definition) continue;
            const auto descriptor = bytes(loom::serialize(op::encode_operator_desc(identity,
                *definition->inputs(), *definition->outputs())));
            total += identity.size() + descriptor.size();
            if (total > maker::kMaxFileBytes)
                throw std::runtime_error("operator catalog exceeds this query's 1 MiB limit");
            answer.operators.push_back({identity, descriptor});
        }
        answer.ok = true;
        return answer;
    }
};

// The office `zengine.flow.host`: each request for the asker's own session, the catalog, and the
// host's own pulse; what a shape it accepts does is answered from its page.
class Manager final
    : public zengine::With<manual::ManualFacet<manual::pages::zengine_flow_host::kManual>>::Weave<
          Manager, detail::FlowHostClockState,
          loom::Accept<FlowRun, FlowApply, FlowSend, FlowInspect, FlowStop, FlowCatalog,
                       detail::FlowHostPulse>,
          loom::Emit<FlowAnswer, FlowCatalogAnswer, FlowChanged>> {
public:
    explicit Manager(RuntimeHost& host) : host_(host) {}

    void on(const FlowRun& run, loom::Mail& mail) {
        serve(mail, run.session, "run", [&](FlowAnswer& answer, const Key& key, const std::string& office) {
            if (owned_.find(key) != owned_.end()) throw std::invalid_argument("session already runs; apply behavior or explicitly stop it before running a fresh project");
            if (owned_.size() >= kMaxSessions) throw std::invalid_argument("Flow already owns eight running sessions");
            if (run.project.size() > maker::kMaxFileBytes) throw std::invalid_argument("project exceeds 1 MiB");
            const auto id = host_.create(flow::read_project(view(run.project)));
            owned_.emplace(key, Owned{id, mail.sender(), office});
            host_.fill(answer, id);
        });
    }
    void on(const FlowApply& apply, loom::Mail& mail) {
        serve(mail, apply.session, "apply", [&](FlowAnswer& answer, const Key& key, const std::string&) {
            const auto id = owned(key)->second.subject;
            if (apply.definition.size() > maker::kMaxFileBytes) throw std::invalid_argument("definition exceeds 1 MiB");
            auto definition = maker::read_definition(view(apply.definition));
            if (!definition) throw std::invalid_argument(definition.reason);
            host_.apply(id, std::move(definition.definition));
            host_.fill(answer, id);
        });
    }
    void on(const FlowSend& send, loom::Mail& mail) {
        serve(mail, send.session, "send", [&](FlowAnswer& answer, const Key& key, const std::string&) {
            const auto id = owned(key)->second.subject;
            host_.send(id, send.payload, mail.correlation());
            host_.fill(answer, id);
            answer.reason = "queued; inspect subsequent dispatch, outputs and state";
        });
    }
    void on(const FlowInspect& inspect, loom::Mail& mail) {
        serve(mail, inspect.session, "inspect", [&](FlowAnswer& answer, const Key& key, const std::string&) {
            const auto id = owned(key)->second.subject;
            if (inspect.after < 0) throw std::invalid_argument("event cursor cannot be negative");
            host_.fill(answer, id, inspect.after);
        });
    }
    void on(const FlowStop& stop, loom::Mail& mail) {
        serve(mail, stop.session, "stop", [&](FlowAnswer& answer, const Key& key, const std::string&) {
            const auto found = owned(key);
            const auto id = found->second.subject;
            if (host_.pending(host_.session(id)) != 0) throw std::invalid_argument("inputs still have pending dispatch; stop after they settle");
            host_.fill(answer, id);
            host_.remove(id);
            owned_.erase(found);
            answer.reason = "stopped; answer contains the final project snapshot";
        });
    }
    void on(const FlowCatalog&, loom::Mail& mail) {
        FlowCatalogAnswer answer;
        try { answer = host_.catalog(); }
        catch (const std::exception& error) { answer.reason = error.what(); }
        mail.answer(answer);
    }
    // The host's own beat: each session that moved tells its owner, as this office. A pulse from
    // any other sender is ignored.
    void on(const detail::FlowHostPulse& pulse, loom::Mail& mail) {
        if (mail.sender() != self_) return;
        for (const auto& [key, record] : owned_) {
            if (std::to_string(record.subject) != pulse.subject) continue;
            const FlowChanged changed{key.second};
            if (record.office.empty()) mail.as_role(kFlowHostRole).send(record.owner, changed);
            else mail.as_role(kFlowHostRole).send_to_role(record.office, changed);
        }
    }

    // No crash revival and no fall back to a last good state: the office is the host's own,
    // mounted once, and its sessions are the host's to keep.
    loom::LifecyclePolicy policy_config() const { return {0, false}; }
    void revive(const loom::Value&) override {}

private:
    using Key = std::pair<std::string, std::string>;
    struct Owned {
        std::uint64_t subject = 0;
        loom::WeaveId owner{};
        std::string office;
    };

    // ONE REQUEST FOR THE ASKER'S OWN SESSION, answered once, to the asker alone. The asker and
    // the session's name are judged here, in this order, and the rest by `act`; a refusal is the
    // answer's reason. A session is keyed by the office the ask was authored as, while its holder
    // asks, or else by the asker's own identity.
    template <class Act>
    void serve(loom::Mail& mail, const std::string& session, const char* action, Act&& act) {
        FlowAnswer answer;
        try {
            if (!mail.sender().valid()) throw std::invalid_argument("a live requesting participant is required");
            answer.session = session;
            if (answer.session.empty() || answer.session.size() > 128)
                throw std::invalid_argument("session name must be between 1 and 128 bytes");
            const std::string office(mail.authored_role());
            if (!office.empty() && host_.bus_.role_holder(office) != mail.sender())
                throw std::invalid_argument("the request's authored office has moved to another participant");
            const Key key{office.empty() ? "id/" + std::to_string(mail.sender().value) : "office/" + office,
                          answer.session};
            answer.action = action;
            act(answer, key, office);
            answer.ok = true;
        } catch (const std::exception& error) { answer.reason = error.what(); }
        mail.answer(answer);
    }
    std::map<Key, Owned>::iterator owned(const Key& key) {
        const auto found = owned_.find(key);
        if (found == owned_.end()) throw std::invalid_argument("no session by this name belongs to this requesting participant");
        return found;
    }

    RuntimeHost& host_;
    std::map<Key, Owned> owned_;
};

namespace detail {
struct FlowHostClockStart { ZEN_SHAPE(FlowHostClockStart, 1); };
class Clock final : public timer::TimedWeave<Clock, FlowHostClockState, loom::Accept<FlowHostClockStart>> {
public:
    explicit Clock(RuntimeHost& host) : host_(host) {
        beat_ = timers().repeat("zengine.flow.observe", std::chrono::milliseconds(16), &Clock::beat);
    }
    using TimedWeave::on;
    void on(const FlowHostClockStart&, loom::Mail& mail) {
        if (!mail.sender().valid()) beat_.restart(mail);
    }
    void beat(const timer::TimerFired&, loom::Mail&) { host_.poll(); }
private:
    RuntimeHost& host_;
    timer::TimerHandle<Clock> beat_;
};
} // namespace detail

inline loom::WeaveId RuntimeHost::mount() {
    if (manager_.valid()) return manager_;
    if (bus_.role_holder(kFlowHostRole).valid()) throw std::invalid_argument("Flow host role is already held");
    loom::Grant grant;
    grant.allow_to_any(FlowAnswer::zen_name, FlowAnswer::zen_version);
    grant.allow_to_any(FlowCatalogAnswer::zen_name, FlowCatalogAnswer::zen_version);
    grant.allow_to_any(FlowChanged::zen_name, FlowChanged::zen_version);
    grant.allow_to_any(manual::ShapeDocumentShown::zen_name, manual::ShapeDocumentShown::zen_version);
    grant.allow_to_role(detail::FlowHostPulse::zen_name, detail::FlowHostPulse::zen_version, kFlowHostRole);
    // ...and Loom's own doors, which every woven weave answers: its accept set, and its state,
    // which holds nothing, so an asker is answered rather than left waiting.
    loom::allow_describe_answers(grant);
    loom::allow_poke_answers(grant);
    auto manager = std::make_unique<Manager>(*this);
    auto* raw = manager.get();
    manager_ = bus_.register_weave(std::move(manager), std::move(grant), kFlowHostRole);
    raw->zen_set_self(manager_);
    loom::Grant tick;
    tick.allow_to_role(timer::EnsureTimer::zen_name, timer::EnsureTimer::zen_version, timer::kTimerRole);
    tick.allow_to_role(timer::CancelTimer::zen_name, timer::CancelTimer::zen_version, timer::kTimerRole);
    auto clock = std::make_unique<detail::Clock>(*this);
    auto* raw_clock = clock.get();
    clock_ = bus_.register_weave(std::move(clock), std::move(tick));
    raw_clock->zen_set_self(clock_);
    bus_.send(clock_, loom::Message(loom::to_value(detail::FlowHostClockStart{})));
    return manager_;
}

// Deliberate host policy helper: install only on participants allowed to create
// ordinary data-authored weaves. Knowing these shapes alone confers no authority.
inline void allow_flow_requests(loom::Grant& grant) {
    for (const auto& schema : {loom::schema_of<FlowRun>(), loom::schema_of<FlowApply>(),
            loom::schema_of<FlowSend>(), loom::schema_of<FlowInspect>(),
            loom::schema_of<FlowStop>(), loom::schema_of<FlowCatalog>()})
        grant.allow_to_role(schema->name(), schema->version(), kFlowHostRole);
}
} // namespace zengine::flow_host
#endif
