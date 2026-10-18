// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_PANE_SETTINGS_TAKEN_HPP
#define ZENGINE_WORKSHOP_PANE_SETTINGS_TAKEN_HPP

// What a pane takes from its row's settings, the pane's side of the hand-off: every setting it
// declares, in force at the value handed when its setting takes it and at its default otherwise,
// and a word for each value handed that its setting does not take, said back against the revision
// it came at. A hand-off is taken whole, so a list that keeps none is every setting at its default;
// a key the pane does not declare is passed over.
// Workshop law: agents/workshop/settings.md

#include "workshop/pane_settings.hpp"
#include "workshop/settings_seam_vocabulary.hpp"

#include <zen/weave.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace zengine::workshop {

/// WHO A PANE SAYS IT TO: Workshop's office, spelled as a pane spells it.
inline constexpr const char* kSettingsHolderRole = "zengine.workshop";

/// WHAT ONE HAND-OFF CAME TO FOR A PANE: each declared setting in force, in declaration order, and
/// each value it was handed that its setting does not take.
struct SettingsTaken {
    std::vector<PaneSetting> in_force;
    std::vector<PaneSettingUnused> unused;

    const PaneSetting* find(std::string_view key) const { return find_pane_setting(in_force, key); }
    bool flag(std::string_view key) const {
        const PaneSetting* s = find(key);
        return s != nullptr && s->flag.value_or(false);
    }
    std::int64_t number(std::string_view key) const {
        const PaneSetting* s = find(key);
        return s != nullptr ? s->number.value_or(0) : 0;
    }
    std::string text(std::string_view key) const {
        const PaneSetting* s = find(key);
        return s != nullptr ? s->text.value_or(std::string()) : std::string();
    }
};

/// TAKE ONE HAND-OFF against the settings the pane declares for the pane it names.
inline SettingsTaken take_pane_settings(const std::vector<v2::PaneSettingRow>& declared,
                                        const v2::PaneSettings& handed) {
    SettingsTaken out;
    const auto unused = [&out, &handed](const std::string& key, const std::string& why) {
        out.unused.push_back(PaneSettingUnused{handed.pane, handed.revision, key,
                                               why.substr(0, kMaxPaneSettingTextLen)});
    };
    for (const v2::PaneSettingRow& row : declared) {
        PaneSetting in_force = pane_setting_default(narrow_setting_row(row));
        if (const PaneSetting* held = find_pane_setting(handed.settings, row.key)) {
            if (v2::pane_setting_refused_by(row, *held).empty()) {
                in_force = *held;
            } else {
                // WHY, IN WHAT THE SETTING TAKES: a line Workshop can keep whatever the value was.
                unused(row.key, "it takes " + pane_setting_takes(narrow_setting_row(row)));
            }
        }
        out.in_force.push_back(std::move(in_force));
    }
    return out;
}

/// ...AND SAY WHAT IT CANNOT USE to Workshop, as the office that holds the pane.
inline void say_settings_unused(loom::Mail& mail, std::string_view office,
                                const SettingsTaken& taken) {
    for (const PaneSettingUnused& u : taken.unused) {
        (void)mail.as_role(office).send_to_role(kSettingsHolderRole, u);
    }
}

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_PANE_SETTINGS_TAKEN_HPP
