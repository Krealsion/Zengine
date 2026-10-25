// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_TESTS_LAYOUTS_STAND_IN_HPP
#define ZENGINE_TESTS_LAYOUTS_STAND_IN_HPP

// THE LAYOUTS' OFFICE BESIDE A STAND-IN WORKSHOP, which seats no pane: a pane that asks for its row
// is handed every setting at its default, as the layouts' owner hands a pane the live layout seats
// nowhere; its declarations and its words about its row are heard and kept nowhere. For the rigs
// that load a pane image with no Workshop, so the pane holds a row and draws.

#include "workshop/pane_settings.hpp"

#include <zen/switchboard.hpp>
#include <zen/weave.hpp>

#include <memory>
#include <string>

namespace zengine::testing {

struct LayoutsStandInState {
    ZEN_SHAPE(LayoutsStandInState, 1);
};

class LayoutsStandIn
    : public loom::WeaveBase<
          LayoutsStandIn, LayoutsStandInState,
          loom::Accept<workshop::PaneSettingsAsked, workshop::PaneSettingsDeclared,
                       workshop::v2::PaneSettingsDeclared, workshop::PaneSettingUnused>,
          loom::Emit<workshop::v2::PaneSettings>> {
public:
    void on(const workshop::PaneSettingsAsked& asked, loom::Mail& mail) {
        const std::string office(mail.authored_role());
        if (!office.empty()) {
            ++asks;
            (void)mail.as_role(workshop::kLayoutsRole)
                .send_to_role(office, workshop::v2::PaneSettings{asked.pane, 0, {}});
        }
    }
    void on(const workshop::PaneSettingsDeclared&, loom::Mail&) {}
    void on(const workshop::v2::PaneSettingsDeclared&, loom::Mail&) {}
    void on(const workshop::PaneSettingUnused&, loom::Mail&) {}
    int asks = 0;
};

/// MOUNT ONE IN `zengine.layouts`, when nothing holds it there.
inline LayoutsStandIn* mount_layouts_stand_in(loom::Switchboard& bus) {
    if (bus.role_holder(workshop::kLayoutsRole).valid()) {
        return nullptr;
    }
    auto stand = std::make_unique<LayoutsStandIn>();
    LayoutsStandIn* raw = stand.get();
    loom::Grant hands;
    hands.allow_to_any(workshop::v2::PaneSettings::zen_name,
                       workshop::v2::PaneSettings::zen_version);
    raw->zen_set_self(
        bus.register_weave(std::move(stand), std::move(hands), std::string(workshop::kLayoutsRole)));
    return raw;
}

} // namespace zengine::testing

#endif // ZENGINE_TESTS_LAYOUTS_STAND_IN_HPP
