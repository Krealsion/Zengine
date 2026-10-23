// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_VIEW_HOST_HPP
#define ZENGINE_VIEW_HOST_HPP

// The view host, the Flow host's sibling: over the host's existing bus it registers each view as
// its own participant from its description, granted only what the description declares, applies
// a change at the same shapes in place, and registers a change of shapes afresh. It owns no
// Kernel, rendering policy or file policy. Law: agents/view.md. Reference: view/docs/view.md;
// its office's manual is view/docs/zengine.view.host.md.

#include "activation/activation.hpp"
#include "manual/vocabulary.hpp"
#include "view/view.hpp"
#include "view/vocabulary.hpp"

#include "manuals/zengine.view.host.hpp" // this office's manual, compiled in from its page

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

/// The state of the host's office: none. Its sessions and views are the host's, never reload-kept
/// state.
struct ViewHostState {
    ZEN_SHAPE(ViewHostState, 1);
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

    /// PAIR AN OWNER'S PRESENTER with the view named `name`: the one running now, drawn again
    /// with what the presenter told, and every view that later runs under that name; a null
    /// presenter unpairs it. At most `kMaxViews` names are paired.
    void pair(const std::string& name, std::shared_ptr<Presenter> presenter) {
        if (!presenter) {
            presenters_.erase(name);
        } else if (!presenters_.count(name) && presenters_.size() >= kMaxViews) {
            throw std::invalid_argument("the view host pairs at most " + std::to_string(kMaxViews) + " views");
        } else {
            presenters_[name] = presenter;
        }
        for (const auto& [id, live] : views_)
            if (live->description().name == name && !retiring_.count(id)) {
                live->pair(presenter);
                bus_.send(loom::WeaveId{id}, loom::Message(loom::to_value(detail::ViewRedraw{})));
            }
    }

private:
    friend class Manager;
    loom::Switchboard& bus_;
    loom::WeaveId manager_{};
    std::map<std::uint64_t, View*> views_;
    std::map<std::uint64_t, bool> retiring_;
    std::map<std::string, std::shared_ptr<Presenter>> presenters_; ///< by the name of the view

    /// REGISTER ONE VIEW from an admitted description: its own participant, holding its name as
    /// its office, granted what the description implies. `succeeding` names the running view it
    /// replaces, which the bound does not count. Loom's registration is the step that can refuse,
    /// and it is taken while that view still runs, so a refusal changes nothing. At the same
    /// name the successor stands sealed to the host's office until the office moves to it, with
    /// no delivery between; the caller then retires the view it succeeds.
    std::uint64_t create(Description description, std::uint64_t succeeding = 0) {
        if (views_.size() - views_.count(succeeding) >= kMaxViews)
            throw std::invalid_argument("the view host already runs " + std::to_string(kMaxViews) + " views");
        const auto name = description.name;
        const View* incumbent = view(succeeding);
        const bool same_office = incumbent && incumbent->description().name == name;
        if (!same_office && bus_.role_holder(name).valid())
            throw std::invalid_argument("`" + name + "` is already held by a running participant");
        auto grant = view_grant(description);
        const auto paired = presenters_.find(name);
        auto view = std::make_unique<View>(std::move(description),
                                           paired == presenters_.end() ? nullptr : paired->second);
        View* raw = view.get();
        const auto id = same_office ? bus_.register_weave(std::move(view), std::move(grant))
                                    : bus_.register_weave(std::move(view), std::move(grant), name);
        if (same_office && !(bus_.seal_weave(id, manager_) &&
                             bus_.commit_candidate(id, loom::WeaveId{succeeding}, name))) {
            bus_.unregister_weave(id);
            throw std::invalid_argument("`" + name + "` could not pass to its successor");
        }
        raw->set_self(id);
        views_.emplace(id.value, raw);
        return id.value;
    }

    /// Tell a registered view to offer its pane, and whether to ask Workshop to show it.
    void start(std::uint64_t id, bool reveal = true) {
        bus_.send(loom::WeaveId{id}, loom::Message(loom::to_value(detail::ViewStart{reveal})));
    }

    /// A change at the same shapes, taken by the running participant.
    void apply(std::uint64_t id, Description description) {
        View* live = view(id);
        if (!live) throw std::invalid_argument("the view is no longer running");
        live->adopt(std::move(description));
        bus_.send(loom::WeaveId{id}, loom::Message(loom::to_value(detail::ViewRedraw{})));
    }

    /// STOP ONE VIEW: its last picture says why in its area's window, queued as its office while it
    /// still holds it, and the participant is unregistered once that picture has been delivered.
    void stop(std::uint64_t id, const std::string& why) {
        View* live = view(id);
        if (!live) return;
        if (auto last = live->window_farewell(why))
            (void)bus_.office_send_to_role_as(loom::WeaveId{id}, live->description().name, last->first,
                                              loom::Message(loom::to_value(last->second), loom::WeaveId{id}));
        retiring_[id] = true;
        (void)bus_.send_as_to_role(manager_, kViewHostRole,
                                   loom::Message(loom::to_value(detail::ViewRetire{static_cast<std::int64_t>(id)}), manager_));
    }

    /// Unregister at once: its successor already holds the office.
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

/// THE VIEW HOST'S OFFICE: run, resume, apply and stop, each for the asker's own session; and
/// what a shape it accepts does, from its page.
class Manager final
    : public zengine::With<manual::ManualFacet<manual::pages::zengine_view_host::kManual>>::Weave<
          Manager, detail::ViewHostState,
          loom::Accept<ViewRun, ViewResume, ViewApply, ViewStop, detail::ViewRetire>,
          loom::Emit<ViewAnswer>> {
public:
    explicit Manager(Host& host) : host_(host) {}

    void on(const ViewRun& run, loom::Mail& mail) {
        register_view(mail, "run", run.session, run.description, false);
    }
    void on(const ViewResume& resume, loom::Mail& mail) {
        register_view(mail, "resume", resume.session, resume.description, true);
    }
    void on(const ViewApply& apply, loom::Mail& mail) {
        serve(mail, "apply", apply.session, [&](ViewAnswer& answer, const Key&, Owned::iterator found) {
            View* live = running(answer, found);
            auto next = read(apply.description);
            if (same_shapes(live->description(), next)) {
                host_.apply(found->second, std::move(next));
                answer.reason = "applied in place: " + answer.office + " tells and says the same shapes";
                return;
            }
            // A NEW SURFACE IS A NEW PARTICIPANT: Loom fixes what a weave accepts and emits when it
            // registers, so a change of shapes registers afresh, and the running view retires only
            // once its successor stands.
            const bool renamed = next.name != live->description().name;
            const auto told = told_words(next);
            answer.office = next.name;
            const auto successor = host_.create(std::move(next), found->second);
            if (renamed) host_.stop(found->second, "was renamed " + answer.office);
            else host_.remove(found->second);
            found->second = successor;
            host_.start(successor);
            answer.fresh = true;
            answer.reason = "registered " + answer.office + " afresh: its shapes changed" + told;
        });
    }
    void on(const ViewStop& stop, loom::Mail& mail) {
        serve(mail, "stop", stop.session, [&](ViewAnswer& answer, const Key&, Owned::iterator found) {
            running(answer, found);
            host_.stop(found->second, "stopped; run it again from the View Builder");
            owned_.erase(found);
            answer.reason = "stopped " + answer.office;
        });
    }
    /// The host's note to itself; one from any other sender is ignored.
    void on(const detail::ViewRetire& retire, loom::Mail& mail) {
        if (mail.sender() == self_) host_.retire(static_cast<std::uint64_t>(retire.subject));
    }

    /// No crash revival and no fall back to a last good state: the office is the host's own,
    /// mounted once, and its views are the host's to keep.
    loom::LifecyclePolicy policy_config() const { return {0, false}; }
    void revive(const loom::Value&) override {}

private:
    using Key = std::pair<std::string, std::string>;
    using Owned = std::map<Key, std::uint64_t>;

    /// ONE ASK FOR THE ASKER'S OWN SESSION, answered once, to the asker alone. The asker and the
    /// session's name are judged here, in this order, and the rest by `act`, given the view the
    /// session runs if any; a refusal is the answer's reason.
    template <class Act>
    void serve(loom::Mail& mail, const char* action, const std::string& session, Act&& act) {
        ViewAnswer answer;
        answer.action = action;
        try {
            if (!mail.sender().valid()) throw std::invalid_argument("a live requesting participant is required");
            answer.session = session;
            if (answer.session.empty() || answer.session.size() > 128)
                throw std::invalid_argument("a session name is 1 to 128 bytes");
            const std::string office(mail.authored_role());
            if (!office.empty() && host_.bus_.role_holder(office) != mail.sender())
                throw std::invalid_argument("the request's authored office has moved to another participant");
            const Key key{office.empty() ? "id/" + std::to_string(mail.sender().value) : "office/" + office,
                          answer.session};
            act(answer, key, owned_.find(key));
            answer.ok = true;
        } catch (const std::exception& error) {
            answer.reason = error.what();
        }
        (void)mail.answer(answer);
    }

    /// RUN OR RESUME: a session that runs no view registers one from its description.
    void register_view(loom::Mail& mail, const char* action, const std::string& session,
                       const loom::Bytes& description, bool resume) {
        serve(mail, action, session, [&](ViewAnswer& answer, const Key& key, Owned::iterator found) {
            if (found != owned_.end())
                throw std::invalid_argument("this session already runs a view; apply a change or stop it first");
            auto admitted = read(description);
            const auto told = told_words(admitted);
            answer.office = admitted.name;
            const auto id = host_.create(std::move(admitted));
            owned_.emplace(key, id);
            // A RESUMED VIEW ASKS NOTHING OF THE DESK: the desk a weaver left says where its pane
            // stands, or that it is hidden.
            host_.start(id, !resume);
            answer.fresh = true;
            answer.reason = "registered " + answer.office + told;
        });
    }

    /// APPLY OR STOP: the view the session runs, still running, named in the answer.
    View* running(ViewAnswer& answer, Owned::iterator found) {
        if (found == owned_.end()) throw std::invalid_argument("no view runs in this session for this participant");
        View* live = host_.view(found->second);
        if (!live) throw std::invalid_argument("the view is no longer running");
        answer.office = live->description().name;
        return live;
    }

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
    Owned owned_;
};

inline loom::WeaveId Host::mount() {
    if (manager_.valid()) return manager_;
    if (bus_.role_holder(kViewHostRole).valid()) throw std::invalid_argument("the view host's office is already held");
    loom::Grant grant;
    grant.allow_to_any(ViewAnswer::zen_name, ViewAnswer::zen_version);
    grant.allow_to_any(manual::ShapeDocumentShown::zen_name, manual::ShapeDocumentShown::zen_version);
    grant.allow_to_role(detail::ViewRetire::zen_name, detail::ViewRetire::zen_version, kViewHostRole);
    // ...and Loom's own doors, which every woven weave answers: its accept set, and its state,
    // which holds nothing, so an asker is answered rather than left waiting.
    loom::allow_describe_answers(grant);
    loom::allow_poke_answers(grant);
    auto manager = std::make_unique<Manager>(*this);
    auto* raw = manager.get();
    manager_ = bus_.register_weave(std::move(manager), std::move(grant), kViewHostRole);
    raw->zen_set_self(manager_);
    return manager_;
}

/// Deliberate host policy: install on a participant allowed to run views. Knowing these shapes
/// confers no authority by itself.
inline void allow_view_requests(loom::Grant& grant) {
    for (const auto& schema : {loom::schema_of<ViewRun>(), loom::schema_of<ViewResume>(),
                               loom::schema_of<ViewApply>(), loom::schema_of<ViewStop>()})
        grant.allow_to_role(schema->name(), schema->version(), kViewHostRole);
}

} // namespace zengine::view
#endif
