// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

// The bodies of tests/pane_stand_in.hpp, compiled once into zengine-pane-stand-in.

#include "pane_stand_in.hpp"

#include "graphics/vocabulary.hpp"
#include "pane/menu.hpp"

#include <zen/kernel/control.hpp>
#include <zen/kernel/manager.hpp>

#include <algorithm>
#include <system_error>
#include <utility>

#if defined(_WIN32)
#include <process.h>
#else
#include <unistd.h>
#endif

namespace zengine::testing {
namespace {

namespace pn = zengine::pane;
namespace gfx = zengine::graphics;

struct StandInState {
    std::int64_t n = 0;
    ZEN_SHAPE(StandInState, 1, ZEN_FIELD(n));
};

/// WORKSHOP'S ROLE, HELD BY NOBODY'S CODE: it accepts the offer and the rows a pane says to
/// Workshop, in every version a pane may speak, so they are delivered rather than refused at the
/// role's door, and does nothing with them; the rig's tap reads them. What it sends a pane, a key
/// or an action, the rig sends in its name, as Workshop's role.
class WorkshopStandIn
    : public loom::WeaveBase<WorkshopStandIn, StandInState,
                             loom::Accept<pn::PaneOffered, pn::v2::PaneOffered, pn::v3::PaneOffered,
                                          pn::PaneActions, pn::v2::PaneActions, pn::v3::PaneActions,
                                          pn::v4::PaneActions, loom::DispatchRefused>,
                             loom::Emit<pn::PaneActionRequested, pn::PaneKey>> {
public:
    template <class Said> void on(const Said&, loom::Mail&) {}
};

/// THE TUI WINDOW'S ROLE, HELD BY NOBODY'S CODE: it holds each drawing sent it as a window holds
/// it, an area's nodes in their order. The room it gives an area the rig sends in its name.
class WindowStandIn : public loom::WeaveBase<WindowStandIn, StandInState, loom::Accept<gfx::Drawn>,
                                             loom::Emit<gfx::AreaRoom>> {
public:
    explicit WindowStandIn(std::map<std::string, std::vector<gfx::Node>>& drawn) : drawn_(&drawn) {}

    /// `whole` first lets go of everything in the area; a removed node takes its subtree with it; a
    /// node put again changes in its place, and a new one goes after its siblings.
    void on(const gfx::Drawn& d, loom::Mail&) {
        std::vector<gfx::Node>& nodes = (*drawn_)[d.area];
        if (d.whole) {
            nodes.clear();
        }
        for (const std::string& gone : d.removed) {
            nodes.erase(std::remove_if(nodes.begin(), nodes.end(),
                                       [&gone](const gfx::Node& n) { return gfx::under(n.path, gone); }),
                        nodes.end());
        }
        for (const gfx::Node& put : d.put) {
            const auto at = std::find_if(nodes.begin(), nodes.end(),
                                         [&put](const gfx::Node& n) { return n.path == put.path; });
            if (at != nodes.end()) {
                *at = put;
            } else {
                nodes.push_back(put);
            }
        }
    }

private:
    std::map<std::string, std::vector<gfx::Node>>* drawn_;
};

/// THE GRAPHICS DIRECTORY'S ROLE, HELD BY NOBODY'S CODE: it hears an owner that starts again ask
/// for its room, which the rig answers as the window that shows the area.
class DirectoryStandIn : public loom::WeaveBase<DirectoryStandIn, StandInState,
                                                loom::Accept<gfx::RoomAsked>, loom::Emit<>> {
public:
    explicit DirectoryStandIn(std::vector<std::string>& asked) : asked_(&asked) {}
    void on(const gfx::RoomAsked& asked, loom::Mail&) { asked_->push_back(asked.area); }

private:
    std::vector<std::string>* asked_;
};

/// The pane and the rows of one declaration, in whichever version it was spoken.
template <class Actions> std::pair<std::string, std::vector<DeclaredRow>> declared(const loom::Value& payload) {
    const Actions said = loom::from_value<Actions>(payload);
    std::vector<DeclaredRow> rows;
    for (const auto& row : said.rows) {
        rows.push_back(DeclaredRow{row.id, row.scancode, row.modifiers});
    }
    return {said.pane, std::move(rows)};
}

std::string this_process() {
#if defined(_WIN32)
    return std::to_string(_getpid());
#else
    return std::to_string(getpid());
#endif
}

struct BootState {
    std::int64_t n = 0;
    ZEN_SHAPE(BootState, 1, ZEN_FIELD(n));
};

/// Commands the Weave Manager and hears its answers, as a host's boot does: a load's weave, a
/// reload's acknowledgement, or a refusal.
class Booter : public loom::WeaveBase<Booter, BootState,
                                      loom::Accept<loom::Result, loom::Ack, loom::Refused>,
                                      loom::Emit<loom::LoadWeave, loom::ReloadWeave>> {
public:
    Booter(std::vector<std::string>& ok, std::vector<std::string>& no, std::size_t& acked)
        : ok_(&ok), no_(&no), acked_(&acked) {}
    void on(const loom::Result& r, loom::Mail&) { ok_->push_back(r.value); }
    void on(const loom::Ack&, loom::Mail&) { ++*acked_; }
    void on(const loom::Refused& r, loom::Mail&) { no_->push_back(r.reason); }

private:
    std::vector<std::string>* ok_;
    std::vector<std::string>* no_;
    std::size_t* acked_;
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
    loom::Grant workshop;
    workshop.allow_to_any(pn::PaneActionRequested::zen_name, pn::PaneActionRequested::zen_version);
    workshop.allow_to_any(pn::PaneKey::zen_name, pn::PaneKey::zen_version);
    holder = bus.register_weave(std::make_unique<WorkshopStandIn>(), std::move(workshop),
                                pn::pane_menu::kWorkshopRole);
    REQUIRE(bus.role_holder(pn::pane_menu::kWorkshopRole) == holder);
    loom::Grant window;
    window.allow_to_any(gfx::AreaRoom::zen_name, gfx::AreaRoom::zen_version);
    window_ = bus.register_weave(std::make_unique<WindowStandIn>(drawn), std::move(window),
                                 std::string(gfx::kWindowRolePrefix) + gfx::kTuiWindow);
    directory_ = bus.register_weave(std::make_unique<DirectoryStandIn>(asked), loom::Grant{},
                                    gfx::kGraphicsRole);
    tap_ = bus.add_observer([this](const loom::BusEvent& ev) {
        if (ev.kind != loom::EventKind::Delivered && ev.kind != loom::EventKind::Refused) {
            return;
        }
        const std::string said = ev.schema_name + "/" + std::to_string(ev.schema_version) +
                                 (ev.kind == loom::EventKind::Refused ? " refused" : "");
        if (ev.sender == holder) {
            from_workshop.push_back(said);
            return;
        }
        if (ev.addressed_role != pn::pane_menu::kWorkshopRole) {
            return;
        }
        to_workshop.push_back(said);
        if (ev.kind != loom::EventKind::Delivered || ev.payload == nullptr ||
            ev.schema_name != pn::v4::PaneActions::zen_name) {
            return;
        }
        // Every version a pane may speak carries rows with an id and a gesture: the one it spoke
        // is read.
        std::pair<std::string, std::vector<DeclaredRow>> rows;
        if (ev.schema_version == pn::v4::PaneActions::zen_version) {
            rows = declared<pn::v4::PaneActions>(*ev.payload);
        } else if (ev.schema_version == pn::v3::PaneActions::zen_version) {
            rows = declared<pn::v3::PaneActions>(*ev.payload);
        } else if (ev.schema_version == pn::v2::PaneActions::zen_version) {
            rows = declared<pn::v2::PaneActions>(*ev.payload);
        } else if (ev.schema_version == pn::PaneActions::zen_version) {
            rows = declared<pn::PaneActions>(*ev.payload);
        }
        actions.clear();
        for (const DeclaredRow& row : rows.second) actions.push_back(row.id);
        declared_[{ev.authored_role, rows.first}] = std::move(rows.second);
    });
}

PaneStandInRig::~PaneStandInRig() { bus.remove_observer(tap_); }

PaneStandInRig::OwnedDir::~OwnedDir() {
    if (!path.empty()) {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }
}

loom::WeaveId PaneStandInRig::load(const std::string& name, const std::string& path, const std::string& role) {
    loom::Grant reach;
    reach.allow(loom::LoadWeave::zen_name, loom::LoadWeave::zen_version, manager_);
    const std::size_t before = loaded_.size();
    const loom::WeaveId booter = loom::mount_granted<Booter>(bus, std::move(reach), loaded_, refusals, acked_);
    bus.send_as(booter, manager_,
                loom::Message(loom::to_value(loom::LoadWeave{name, path, {role}}), booter, booter, 0));
    settle();
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

void PaneStandInRig::show(const std::string& owner, const std::string& area, std::int64_t columns,
                          std::int64_t rows) {
    shown_[area] = Shown{owner, columns, rows};
    ++epoch_;
    tell_room(area);
    settle();
}

std::string PaneStandInRig::picture(const std::string& area) const {
    std::string text;
    const auto at = drawn.find(area);
    if (at == drawn.end()) {
        return text;
    }
    for (const gfx::Node& n : at->second) {
        if (const gfx::TextArgs* t = gfx::operands<gfx::TextArgs>(n)) {
            text += t->text + "\n";
        }
    }
    return text;
}

void PaneStandInRig::key(const std::string& role, const std::string& pane, std::int64_t scancode,
                         std::int64_t modifiers) {
    std::string action;
    const auto rows = declared_.find({role, pane});
    if (rows != declared_.end() && scancode != 0) { // 0 is `zengine::input::scan::kUnknown`: no gesture
        for (const DeclaredRow& row : rows->second) {
            if (row.scancode == scancode && row.modifiers == modifiers) {
                action = row.id;
                break;
            }
        }
    }
    const loom::Value said = action.empty() ? loom::to_value(pn::PaneKey{pane, scancode, modifiers})
                                            : loom::to_value(pn::PaneActionRequested{pane, action});
    (void)bus.role_send_to_role_as(holder, pn::pane_menu::kWorkshopRole, role,
                                   loom::Message(said, holder, loom::WeaveId{}, 0));
    settle();
}

std::string PaneStandInRig::reload(const std::string& name, const std::string& path) {
    loom::Grant reach;
    reach.allow(loom::ReloadWeave::zen_name, loom::ReloadWeave::zen_version, manager_);
    const std::size_t refused = refusals.size(), acked = acked_;
    const loom::WeaveId booter = loom::mount_granted<Booter>(bus, std::move(reach), loaded_, refusals, acked_);
    bus.send_as(booter, manager_,
                loom::Message(loom::to_value(loom::ReloadWeave{name, path}), booter, booter, 0));
    settle();
    if (refusals.size() > refused) {
        return refusals.back();
    }
    return acked_ > acked ? std::string() : std::string("the Weave Manager answered nothing");
}

loom::WeaveId PaneStandInRig::weave_id(const std::string& name) const { return kernel_.weave_id(name); }

std::string PaneStandInRig::copy_of(const std::string& image, const std::string& prefix) {
    if (dir_.path.empty()) {
        static int counter = 0;
        dir_.path = std::filesystem::temp_directory_path() /
                    ("zengine-panes-apart-" + this_process() + "-" + std::to_string(++counter));
        std::error_code ec;
        std::filesystem::remove_all(dir_.path, ec);
        std::filesystem::create_directories(dir_.path);
    }
    const std::filesystem::path to = dir_.path / (prefix + std::filesystem::path(image).filename().string());
    std::filesystem::copy_file(image, to, std::filesystem::copy_options::overwrite_existing);
    return to.generic_string();
}

void PaneStandInRig::tell_room(const std::string& area) {
    const Shown& at = shown_.at(area);
    gfx::AreaRoom room;
    room.area = area;
    room.window = gfx::kTuiWindow;
    room.epoch = epoch_;
    room.w = at.columns * gfx::kCellPx;
    room.h = at.rows * gfx::kCellPx;
    room.empty = drawn[area].empty();
    room.cell_px = 0; // a window of characters: its unit is the cell
    (void)bus.role_send_to_role_as(window_, std::string(gfx::kWindowRolePrefix) + gfx::kTuiWindow, at.owner,
                                   loom::Message(loom::to_value(room), window_, loom::WeaveId{}, 0));
}

void PaneStandInRig::settle() {
    bus.drain_until_idle();
    // Each round answers what was asked before it. An owner asks once in each life, so a round or
    // two settles a case, and the bound keeps an owner that asks on every room from spinning.
    for (int round = 0; round < 8; ++round) {
        std::vector<std::string> waiting;
        bool answered = false;
        for (const std::string& area : std::exchange(asked, {})) {
            if (shown_.count(area) != 0) {
                tell_room(area);
                answered = true;
            } else {
                waiting.push_back(area);
            }
        }
        asked = std::move(waiting);
        if (!answered) {
            return;
        }
        bus.drain_until_idle();
    }
}

} // namespace zengine::testing
