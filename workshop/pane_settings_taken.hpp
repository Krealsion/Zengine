// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_PANE_SETTINGS_TAKEN_HPP
#define ZENGINE_WORKSHOP_PANE_SETTINGS_TAKEN_HPP

// What a pane takes from its row's settings, the pane's side of the hand-off: it asks the layouts'
// owner for its row when it arrives, takes a row from that owner alone, and draws no room before
// it holds one. Every setting it declares is in force at the value handed when its setting takes
// it and at its default otherwise, and each value handed that its setting does not take is said
// back against the revision it came at. A hand-off is taken whole, so a list that keeps none is
// every setting at its default; a key the pane does not declare is passed over.
// Reference: workshop/docs/workshop-panes.md#a-panes-settings.

#include "workshop/pane_settings.hpp"

#include <zen/weave.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace zengine::workshop {

/// WHAT ONE HAND-OFF CAME TO FOR A PANE: each declared setting in force, in declaration order, and
/// each value it was handed that its setting does not take, with why in the setting's own words;
/// and the revision of the row it was handed at, which a commit of the pane's own names
/// (`PaneSettingCommitRequested`).
struct SettingsTaken {
    std::vector<PaneSetting> in_force;
    std::vector<PaneSettingUnused> unused;
    std::vector<std::string> refused; ///< beside `unused`, one each: "<key> takes ..., not ..."
    std::int64_t revision = 0;

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
    /// Why the value handed under `key` was not taken, in its setting's words; empty when it was.
    std::string refusal(std::string_view key) const {
        for (std::size_t i = 0; i < unused.size() && i < refused.size(); ++i) {
            if (unused[i].key == key) {
                return refused[i];
            }
        }
        return std::string();
    }
};

/// TAKE ONE HAND-OFF against the settings the pane declares for the pane it names.
inline SettingsTaken take_pane_settings(const std::vector<v2::PaneSettingRow>& declared,
                                        const v2::PaneSettings& handed) {
    SettingsTaken out;
    out.revision = handed.revision;
    const auto unused = [&out, &handed](const std::string& key, const std::string& why,
                                        std::string refused) {
        out.unused.push_back(PaneSettingUnused{handed.pane, handed.revision, key,
                                               why.substr(0, kMaxPaneSettingTextLen)});
        out.refused.push_back(std::move(refused));
    };
    for (const v2::PaneSettingRow& row : declared) {
        PaneSetting in_force = pane_setting_default(narrow_setting_row(row));
        if (const PaneSetting* held = find_pane_setting(handed.settings, row.key)) {
            if (std::string refused = v2::pane_setting_refused_by(row, *held); refused.empty()) {
                in_force = *held;
            } else {
                // WHY, IN WHAT THE SETTING TAKES: a line the owner can keep whatever the value was.
                unused(row.key, "it takes " + pane_setting_takes(narrow_setting_row(row)),
                       std::move(refused));
            }
        }
        out.in_force.push_back(std::move(in_force));
    }
    return out;
}

/// ...AND SAY WHAT IT CANNOT USE to the layouts' owner, as the office that holds the pane.
inline void say_settings_unused(loom::Mail& mail, std::string_view office,
                                const SettingsTaken& taken) {
    for (const PaneSettingUnused& u : taken.unused) {
        (void)mail.as_role(office).send_to_role(kLayoutsRole, u);
    }
}

/// THE PANE'S SIDE OF ITS ROW, as a facet composed with `zengine::With`: once in each life, after
/// the first message the life delivers, the pane named `Pane` asks the layouts' owner for its row
/// as `Office`; a row handed by that owner alone, for that pane alone, is taken whole against the
/// weave's `setting_rows()`, what it cannot use is said back, and the weave's
/// `on_settings(taken, mail)` applies it. The first row the life holds is the weave's
/// `on_settings_held(mail)` too, where it has one: the room it drew nothing in is drawn now.
/// Until then `held()` is false, and the pane draws no room. Image-local: a successor holds
/// nothing and asks again.
template <const char* const& Pane, const char* const& Office>
class PaneSettingsFacet {
public:
    using Accept = loom::Accept<v2::PaneSettings>;
    using Emit = loom::Emit<PaneSettingsAsked, PaneSettingUnused>;

    template <class Self>
    void on(Self& self, const v2::PaneSettings& handed, loom::Mail& mail) {
        if (!from_owner(mail) || handed.pane != Pane) {
            return; // another office's word on this pane's row, or a word on another pane's
        }
        const SettingsTaken taken = take_pane_settings(Self::setting_rows(), handed);
        say_settings_unused(mail, Office, taken);
        const bool first = !held_;
        held_ = true;
        self.on_settings(taken, mail);
        if (first) {
            if constexpr (requires { self.on_settings_held(mail); }) {
                self.on_settings_held(mail);
            }
        }
    }

    /// THE ASK, once in each life: a send that queued nothing is asked again after the next.
    template <class Self>
    void after_delivery(Self&, loom::Mail& mail) {
        if (!asked_) {
            asked_ = mail.as_role(Office)
                         .send_to_role(kLayoutsRole, PaneSettingsAsked{Pane})
                         .valid();
        }
    }

    /// Whether this life holds its row: until it does, the pane draws no room.
    bool held() const noexcept { return held_; }

    /// Whether this delivery is the layouts' owner's word.
    static bool from_owner(const loom::Mail& mail) { return mail.authored_from_role(kLayoutsRole); }

private:
    bool held_ = false;
    bool asked_ = false;
};

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_PANE_SETTINGS_TAKEN_HPP
