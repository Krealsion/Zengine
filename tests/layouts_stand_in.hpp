// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_TESTS_LAYOUTS_STAND_IN_HPP
#define ZENGINE_TESTS_LAYOUTS_STAND_IN_HPP

// THE LAYOUTS' ROLE BESIDE A STAND-IN WORKSHOP, which seats no pane: a pane that asks for its row
// is handed every setting at its default, as the layouts' owner hands a pane the live layout seats
// nowhere -- at once, or, while it holds, when a case releases it; its declarations and its words
// about its row are heard and kept nowhere. For the rigs that load a pane image with no Workshop, so
// the pane holds a row and draws.

#include "pane/settings.hpp"

#include <zen/switchboard.hpp>
#include <zen/weave.hpp>

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace zengine::testing {

struct LayoutsStandInState {
    ZEN_SHAPE(LayoutsStandInState, 1);
};

class LayoutsStandIn
    : public loom::WeaveBase<
          LayoutsStandIn, LayoutsStandInState,
          loom::Accept<pane::PaneSettingsAsked, pane::PaneSettingsDeclared,
                       pane::v2::PaneSettingsDeclared, pane::PaneSettingUnused>,
          loom::Emit<pane::v2::PaneSettings>> {
public:
    void on(const pane::PaneSettingsAsked& asked, loom::Mail& mail) {
        const std::string role(mail.authored_role());
        if (role.empty()) {
            return;
        }
        ++asks;
        if (hold) {
            held.emplace_back(role, asked.pane);
            return;
        }
        (void)mail.as_role(pane::kLayoutsRole)
            .send_to_role(role, pane::v2::PaneSettings{asked.pane, 0, {}});
    }
    /// THE ROWS HELD, HANDED NOW: every default, to each role that asked.
    void release(loom::Switchboard& bus) {
        hold = false;
        for (const auto& [role, pane] : held) {
            (void)bus.role_send_to_role_as(
                self, pane::kLayoutsRole, role,
                loom::Message(loom::to_value(pane::v2::PaneSettings{pane, 0, {}}), self, {}, 0));
        }
        held.clear();
    }
    void on(const pane::PaneSettingsDeclared&, loom::Mail&) {}
    void on(const pane::v2::PaneSettingsDeclared&, loom::Mail&) {}
    void on(const pane::PaneSettingUnused&, loom::Mail&) {}
    int asks = 0;
    bool hold = false; ///< while true, an ask is kept unanswered until `release`
    std::vector<std::pair<std::string, std::string>> held;
    loom::WeaveId self{};
};

/// MOUNT ONE IN `zengine.layouts`, when nothing holds it there; `hold` keeps each ask unanswered.
inline LayoutsStandIn* mount_layouts_stand_in(loom::Switchboard& bus, bool hold = false) {
    if (bus.role_holder(pane::kLayoutsRole).valid()) {
        return nullptr;
    }
    auto stand = std::make_unique<LayoutsStandIn>();
    LayoutsStandIn* raw = stand.get();
    raw->hold = hold;
    loom::Grant hands;
    hands.allow_to_any(pane::v2::PaneSettings::zen_name,
                       pane::v2::PaneSettings::zen_version);
    raw->self =
        bus.register_weave(std::move(stand), std::move(hands), std::string(pane::kLayoutsRole));
    raw->zen_set_self(raw->self);
    return raw;
}

} // namespace zengine::testing

#endif // ZENGINE_TESTS_LAYOUTS_STAND_IN_HPP
