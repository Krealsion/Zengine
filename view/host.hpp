// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_VIEW_HOST_HPP
#define ZENGINE_VIEW_HOST_HPP

// The view host, the Flow host's sibling: over the host's existing bus it registers each view as
// its own participant from its description, granted only what the description declares, applies
// a change at the same shapes in place, and registers a change of shapes afresh. It owns no
// Kernel, rendering policy or file policy. Law: agents/view.md. Reference: docs/reference/view.md.

#include "view/view.hpp"
#include "view/vocabulary.hpp"

#include <zen/switchboard/switchboard.hpp>

#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace zengine::view {

/// The most views one host keeps running at once.
inline constexpr std::size_t kMaxViews = 8;

namespace detail {
/// The host's note to itself that a stopped view's last picture is queued ahead of it.
struct ViewRetire {
    std::int64_t subject = 0;
    ZEN_SHAPE(ViewRetire, 1, ZEN_FIELD(subject));
};
} // namespace detail

class Manager;

/// Host-owned adapter over the host's bus. Declare it after the bus, so its destruction
/// unregisters its participants first.
class Host {
public:
    explicit Host(loom::Switchboard& bus) : bus_(bus) {}
    Host(const Host&) = delete;
    Host& operator=(const Host&) = delete;
    ~Host() {
        for (const auto& [id, view] : views_) {
            (void)view;
            bus_.unregister_weave(loom::WeaveId{id});
        }
        if (manager_.valid()) bus_.unregister_weave(manager_);
    }
    loom::WeaveId mount();
    loom::WeaveId id() const noexcept { return manager_; }

    /// The live view registered under `id`, or null.
    View* view(std::uint64_t id) const {
        const auto found = views_.find(id);
        return found == views_.end() ? nullptr : found->second;
    }

private:
    friend class Manager;
    loom::Switchboard& bus_;
    loom::WeaveId manager_{};
    std::map<std::uint64_t, View*> views_;
    std::map<std::uint64_t, bool> retiring_;

    /// REGISTER ONE VIEW from an admitted description: its own participant, holding its name as
    /// its office, granted what the description implies; then told to offer its pane.
    std::uint64_t create(Description description) {
        if (views_.size() >= kMaxViews)
            throw std::invalid_argument("the view host already runs " + std::to_string(kMaxViews) + " views");
        if (bus_.role_holder(description.name).valid())
            throw std::invalid_argument("`" + description.name + "` is already held by a running participant");
        auto grant = view_grant(description);
        const auto name = description.name;
        auto view = std::make_unique<View>(std::move(description));
        View* raw = view.get();
        const auto id = bus_.register_weave(std::move(view), std::move(grant), name);
        raw->set_self(id);
        views_.emplace(id.value, raw);
        bus_.send(id, loom::Message(loom::to_value(detail::ViewStart{})));
        return id.value;
    }

    /// A change at the same shapes, taken by the running participant.
    void apply(std::uint64_t id, Description description) {
        View* live = view(id);
        if (!live) throw std::invalid_argument("the view is no longer running");
        live->adopt(std::move(description));
        bus_.send(loom::WeaveId{id}, loom::Message(loom::to_value(detail::ViewRedraw{})));
    }

    /// STOP ONE VIEW: its last picture says why, queued as its office while it still holds it,
    /// and the participant is unregistered once that picture has been delivered.
    void stop(std::uint64_t id, const std::string& why) {
        View* live = view(id);
        if (!live) return;
        if (auto last = live->farewell(why))
            (void)bus_.office_send_to_role_as(loom::WeaveId{id}, live->description().name,
                                              kWorkshopRole, loom::Message(loom::to_value(*last), loom::WeaveId{id}));
        retiring_[id] = true;
        (void)bus_.send_as_to_role(manager_, kViewHostRole,
                                   loom::Message(loom::to_value(detail::ViewRetire{static_cast<std::int64_t>(id)}), manager_));
    }

    /// Unregister at once: the successor takes the same office in the same turn.
    void remove(std::uint64_t id) {
        if (!views_.count(id)) return;
        views_.erase(id);
        retiring_.erase(id);
        bus_.unregister_weave(loom::WeaveId{id});
    }

    void retire(std::uint64_t id) {
        if (retiring_.count(id)) remove(id);
    }
};

/// THE VIEW HOST'S OFFICE: run, apply and stop, each for the asker's own session.
class Manager final : public loom::Weave {
public:
    explicit Manager(Host& host) : host_(host) {}
    void set_self(loom::WeaveId id) { self_ = id; }

    std::vector<std::shared_ptr<const loom::Schema>> accepted_schemas() const override {
        return {loom::schema_of<ViewRun>(), loom::schema_of<ViewApply>(), loom::schema_of<ViewStop>(),
                loom::schema_of<detail::ViewRetire>()};
    }
    std::vector<std::shared_ptr<const loom::Schema>> emitted_schemas() const override {
        return {loom::schema_of<ViewAnswer>()};
    }

    void handle(const loom::Message& request, loom::Bus& bus) override {
        const auto& schema = request.payload.schema();
        const auto is = [&schema](const auto& shape) { return loom::same_identity(schema, *shape); };
        if (is(loom::schema_of<detail::ViewRetire>())) {
            if (request.sender == self_)
                host_.retire(static_cast<std::uint64_t>(loom::from_value<detail::ViewRetire>(request.payload).subject));
            return;
        }
        ViewAnswer answer;
        answer.action = is(loom::schema_of<ViewRun>()) ? "run" : is(loom::schema_of<ViewApply>()) ? "apply" : "stop";
        try {
            if (!request.sender.valid()) throw std::invalid_argument("a live requesting participant is required");
            answer.session = request.payload.get("session")->as_text();
            if (answer.session.empty() || answer.session.size() > 128)
                throw std::invalid_argument("a session name is 1 to 128 bytes");
            const std::string office(request.provenance.authored_role());
            if (!office.empty() && host_.bus_.role_holder(office) != request.sender)
                throw std::invalid_argument("the request's authored office has moved to another participant");
            const auto key = std::make_pair(office.empty() ? "id/" + std::to_string(request.sender.value)
                                                           : "office/" + office,
                                            answer.session);
            const auto found = owned_.find(key);
            if (is(loom::schema_of<ViewRun>())) {
                if (found != owned_.end())
                    throw std::invalid_argument("this session already runs a view; apply a change or stop it first");
                auto admitted = read(loom::from_value<ViewRun>(request.payload).description);
                const auto told = told_words(admitted);
                answer.office = admitted.name;
                owned_.emplace(key, host_.create(std::move(admitted)));
                answer.fresh = true;
                answer.reason = "registered " + answer.office + told;
            } else {
                if (found == owned_.end())
                    throw std::invalid_argument("no view runs in this session for this participant");
                View* live = host_.view(found->second);
                if (!live) throw std::invalid_argument("the view is no longer running");
                answer.office = live->description().name;
                if (is(loom::schema_of<ViewStop>())) {
                    host_.stop(found->second, "stopped; run it again from the View Builder");
                    owned_.erase(found);
                    answer.reason = "stopped " + answer.office;
                } else if (is(loom::schema_of<ViewApply>())) {
                    auto next = read(loom::from_value<ViewApply>(request.payload).description);
                    if (same_shapes(live->description(), next)) {
                        host_.apply(found->second, std::move(next));
                        answer.reason = "applied in place: " + answer.office + " tells and says the same shapes";
                    } else {
                        // A NEW SURFACE IS A NEW PARTICIPANT: Loom fixes what a weave accepts and
                        // emits when it registers, so a change of shapes registers afresh.
                        const auto before = live->description().name;
                        const auto told = told_words(next);
                        if (next.name != before) {
                            if (host_.bus_.role_holder(next.name).valid())
                                throw std::invalid_argument("`" + next.name + "` is already held by a running participant");
                            host_.stop(found->second, "was renamed " + next.name);
                        } else {
                            host_.remove(found->second);
                        }
                        answer.office = next.name;
                        found->second = host_.create(std::move(next));
                        answer.fresh = true;
                        answer.reason = "registered " + answer.office + " afresh: its shapes changed" + told;
                    }
                } else {
                    throw std::invalid_argument("unknown view request");
                }
            }
            answer.ok = true;
        } catch (const std::exception& error) {
            answer.reason = error.what();
        }
        (void)bus.answer(loom::Message(loom::to_value(answer), self_));
    }

    loom::Value snapshot() const override {
        return loom::Value(loom::make_schema("zengine.view.Manager", 1, {}));
    }
    loom::Value policy() const override {
        loom::Value v(loom::lifecycle_policy_schema());
        v.set("max_reloads", loom::Cell::integer(0));
        v.set("revive_from_last_good", loom::Cell::boolean(false));
        return v;
    }
    void revive(const loom::Value&) override {}

private:
    static Description read(const loom::Bytes& bytes) {
        if (bytes.size() > maker::kMaxFileBytes) throw std::invalid_argument("a description exceeds the file limit");
        auto admitted = read_description(std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size()));
        if (!admitted) throw std::invalid_argument(admitted.reason);
        return std::move(admitted.description);
    }
    static std::string told_words(const Description& d) {
        std::string out;
        for (const auto& shape : d.told()) out += (out.empty() ? "; it waits until told " : ", ") + shape->name();
        return out;
    }

    Host& host_;
    loom::WeaveId self_{};
    std::map<std::pair<std::string, std::string>, std::uint64_t> owned_;
};

inline loom::WeaveId Host::mount() {
    if (manager_.valid()) return manager_;
    if (bus_.role_holder(kViewHostRole).valid()) throw std::invalid_argument("the view host's office is already held");
    loom::Grant grant;
    grant.allow_to_any(ViewAnswer::zen_name, ViewAnswer::zen_version);
    grant.allow_to_role(detail::ViewRetire::zen_name, detail::ViewRetire::zen_version, kViewHostRole);
    auto manager = std::make_unique<Manager>(*this);
    auto* raw = manager.get();
    manager_ = bus_.register_weave(std::move(manager), std::move(grant), kViewHostRole);
    raw->set_self(manager_);
    return manager_;
}

/// Deliberate host policy: install on a participant allowed to run views. Knowing these shapes
/// confers no authority by itself.
inline void allow_view_requests(loom::Grant& grant) {
    for (const auto& schema : {loom::schema_of<ViewRun>(), loom::schema_of<ViewApply>(),
                               loom::schema_of<ViewStop>()})
        grant.allow_to_role(schema->name(), schema->version(), kViewHostRole);
}

} // namespace zengine::view
#endif
