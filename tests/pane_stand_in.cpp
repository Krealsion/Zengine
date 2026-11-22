// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

// The bodies of tests/pane_stand_in.hpp, compiled once into zengine-pane-stand-in.

#include "pane_stand_in.hpp"

#include "pane/menu.hpp"

#include <zen/kernel/control.hpp>
#include <zen/kernel/manager.hpp>

#include <utility>

namespace zengine::testing {
namespace {

namespace pn = zengine::pane;

struct StandInState {
    std::int64_t n = 0;
    ZEN_SHAPE(StandInState, 1, ZEN_FIELD(n));
};

/// WORKSHOP'S ROLE, HELD BY NOBODY'S CODE: it accepts the offer and the rows a pane says to
/// Workshop, in every version a pane may speak, so they are delivered rather than refused at the
/// role's door, and does nothing with them; the rig's tap reads them.
class WorkshopStandIn
    : public loom::WeaveBase<WorkshopStandIn, StandInState,
                             loom::Accept<pn::PaneOffered, pn::v2::PaneOffered, pn::v3::PaneOffered,
                                          pn::PaneActions, pn::v2::PaneActions, pn::v3::PaneActions,
                                          pn::v4::PaneActions>,
                             loom::Emit<>> {
public:
    template <class Said> void on(const Said&, loom::Mail&) {}
};

struct BootState {
    std::int64_t n = 0;
    ZEN_SHAPE(BootState, 1, ZEN_FIELD(n));
};

/// Commands the Weave Manager and hears its answers, as a host's boot does.
class Booter : public loom::WeaveBase<Booter, BootState,
                                      loom::Accept<loom::Result, loom::Ack, loom::Refused>,
                                      loom::Emit<loom::LoadWeave>> {
public:
    Booter(std::vector<std::string>& ok, std::vector<std::string>& no) : ok_(&ok), no_(&no) {}
    void on(const loom::Result& r, loom::Mail&) { ok_->push_back(r.value); }
    void on(const loom::Ack&, loom::Mail&) {}
    void on(const loom::Refused& r, loom::Mail&) { no_->push_back(r.reason); }

private:
    std::vector<std::string>* ok_;
    std::vector<std::string>* no_;
};

struct AskerState {
    std::int64_t n = 0;
    ZEN_SHAPE(AskerState, 1, ZEN_FIELD(n));
};

/// Asks one role for a pane's manual, as any weave would, and keeps each answer and whether it
/// answered this asker's own ask.
class ManualAsker : public loom::WeaveBase<ManualAsker, AskerState, loom::Accept<pn::PaneDocumentShown>,
                                           loom::Emit<pn::PaneDocumentRequested>> {
public:
    void on(const pn::PaneDocumentShown& s, loom::Mail& mail) {
        heard.push_back(s);
        answers.push_back(mail.answers_ask());
    }
    std::vector<pn::PaneDocumentShown> heard;
    std::vector<bool> answers;
};

} // namespace

PaneStandInRig::PaneStandInRig()
    : kernel_(bus, loom::trust_every_artifact("a pane's suite loads only this build's images")),
      control_(loom::mount_control(kernel_, bus)), manager_(loom::mount_manager(control_, bus)) {
    holder = bus.register_weave(std::make_unique<WorkshopStandIn>(), loom::Grant{},
                                pn::pane_menu::kWorkshopRole);
    REQUIRE(bus.role_holder(pn::pane_menu::kWorkshopRole) == holder);
    tap_ = bus.add_observer([this](const loom::BusEvent& ev) {
        if (ev.kind != loom::EventKind::Delivered && ev.kind != loom::EventKind::Refused) {
            return;
        }
        if (ev.addressed_role != pn::pane_menu::kWorkshopRole) {
            return;
        }
        to_workshop.push_back(ev.schema_name + "/" + std::to_string(ev.schema_version) +
                              (ev.kind == loom::EventKind::Refused ? " refused" : ""));
        if (ev.kind != loom::EventKind::Delivered || ev.payload == nullptr ||
            ev.schema_name != pn::v4::PaneActions::zen_name) {
            return;
        }
        // Every version a pane may speak carries rows with an id: the one it spoke is read.
        actions.clear();
        if (ev.schema_version == pn::v4::PaneActions::zen_version) {
            for (const auto& row : loom::from_value<pn::v4::PaneActions>(*ev.payload).rows) actions.push_back(row.id);
        } else if (ev.schema_version == pn::v3::PaneActions::zen_version) {
            for (const auto& row : loom::from_value<pn::v3::PaneActions>(*ev.payload).rows) actions.push_back(row.id);
        } else if (ev.schema_version == pn::v2::PaneActions::zen_version) {
            for (const auto& row : loom::from_value<pn::v2::PaneActions>(*ev.payload).rows) actions.push_back(row.id);
        }
    });
}

PaneStandInRig::~PaneStandInRig() { bus.remove_observer(tap_); }

loom::WeaveId PaneStandInRig::load(const std::string& name, const std::string& path, const std::string& role) {
    loom::Grant reach;
    reach.allow(loom::LoadWeave::zen_name, loom::LoadWeave::zen_version, manager_);
    const std::size_t before = loaded_.size();
    const loom::WeaveId booter = loom::mount_granted<Booter>(bus, std::move(reach), loaded_, refusals);
    bus.send_as(booter, manager_,
                loom::Message(loom::to_value(loom::LoadWeave{name, path, {role}}), booter, booter, 0));
    bus.drain_until_idle();
    if (loaded_.size() <= before) {
        return loom::WeaveId{};
    }
    return loom::WeaveId{static_cast<std::uint64_t>(std::stoll(loaded_.back()))};
}

pn::PaneDocumentShown PaneStandInRig::ask_manual(const std::string& role, const std::string& pane,
                                                 const std::string& section) {
    auto held = std::make_unique<ManualAsker>();
    ManualAsker* asker = held.get();
    loom::Grant grant;
    grant.allow_to_role(pn::PaneDocumentRequested::zen_name, pn::PaneDocumentRequested::zen_version, role);
    const loom::WeaveId id = bus.register_weave(std::move(held), std::move(grant));
    asker->zen_set_self(id);
    (void)bus.send_as_to_role(
        id, role, loom::Message(loom::to_value(pn::PaneDocumentRequested{pane, section}), id, loom::WeaveId{}, 9));
    bus.drain_until_idle();
    REQUIRE(asker->heard.size() == 1);
    CHECK(asker->answers[0]); // the answer to this asker's own ask, and to it alone
    return asker->heard[0];
}

} // namespace zengine::testing
