// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_INPUT_DOUBLE_CLICK_SETTING_HPP
#define ZENGINE_INPUT_DOUBLE_CLICK_SETTING_HPP

// The `zengine.input` role's one role-wide setting, `kSettingDoubleClickMs`, as the loadable
// weave carries it: an `InputWeaveT` composed with `DoubleClickSetting` declares the setting to
// Workshop's layouts' owner at every activation and sets the time it is handed
// (`InputWeaveT::set_double_click_ms`). Not installed: it speaks Workshop's settings seam
// (WL-SETTING-10). Reference: input/docs/zengine.input.md.

#include "vocabulary.hpp"

#include "workshop/pane_settings.hpp"
#include "workshop/settings_seam_vocabulary.hpp"

#include <zen/weave.hpp>
#include <zen/weave/lifecycle.hpp>

namespace zengine::input {

struct DoubleClickSetting {
    using Accept = loom::Accept<loom::Activated, workshop::RoleSettings>;
    using Emit = loom::Emit<workshop::RoleSettingsDeclared>;

    /// EVERY ACTIVATION: the setting declared to the role that keeps it, which hands it back.
    template <class Self>
    void on(Self&, const loom::Activated&, loom::Mail& mail) {
        workshop::v2::PaneSettingRow row;
        row.key = kSettingDoubleClickMs;
        row.number = kDefaultDoubleClickMs;
        row.low = kLeastDoubleClickMs;
        row.high = kMostDoubleClickMs;
        row.tags = {"pointer"};
        row.summary = "how close in time two presses are one double click, in ms";
        (void)mail.as_role(kInputRole)
            .send_to_role(workshop::kLayoutsRole, workshop::RoleSettingsDeclared{{row}});
    }

    /// THE SETTING AS THE LAYOUTS' OWNER KEEPS IT, from that role alone: a default is kept as
    /// its absence, so none is `kDefaultDoubleClickMs`.
    template <class Self>
    void on(Self& self, const workshop::RoleSettings& kept, loom::Mail& mail) {
        if (!mail.authored_from_role(workshop::kLayoutsRole)) {
            return;
        }
        std::int64_t ms = kDefaultDoubleClickMs;
        for (const workshop::PaneSetting& s : kept.settings) {
            if (s.key == kSettingDoubleClickMs && s.number) {
                ms = *s.number;
            }
        }
        self.set_double_click_ms(ms, mail);
    }
};

} // namespace zengine::input

#endif // ZENGINE_INPUT_DOUBLE_CLICK_SETTING_HPP
