// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_SETTINGS_SEAM_VOCABULARY_HPP
#define ZENGINE_WORKSHOP_SETTINGS_SEAM_VOCABULARY_HPP

// The settings seam: one pane's settings on the live layout, read and written by message. A reading
// says what the pane's counted declaration takes, what the layout's row keeps, and the revision the
// row stands at; a write names that revision and is judged against the declaration by the one
// setter Info's rows use. `provider` and `pane` address a row and are never authority: who may ask
// is the asker's own grant. A pane says, stamped, a value it was handed and cannot use.
// Workshop law: agents/workshop/settings.md

#include "pane_settings.hpp"

#include <zen/weave/shape.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace zengine::workshop {

/// ONE KEY A LOCK HOLDS, and where the lock holds it: `layout` or `everywhere`.
struct SettingLockShown {
    std::string key;
    std::string scope;
    ZEN_SHAPE(SettingLockShown, 1, ZEN_FIELD(key), ZEN_FIELD(scope));
};

/// Read one pane's row of the live layout, answered to the asker alone.
struct PaneSettingsRequested {
    std::string provider;
    std::string pane;
    ZEN_SHAPE(PaneSettingsRequested, 1, ZEN_FIELD(provider), ZEN_FIELD(pane));
};

/// THE LIVE LAYOUT'S ROW FOR THE PANE: the revision it stands at (0 when the live layout seats no
/// such pane), what its counted declaration takes (empty when none counts), what the row keeps,
/// the keys locked, the keys pinned in pin order, and the desk's sentence about the row.
struct PaneSettingsShown {
    std::string provider;
    std::string pane;
    std::int64_t revision = 0;
    std::vector<v2::PaneSettingRow> rows = {};
    std::vector<PaneSetting> settings = {};
    std::vector<SettingLockShown> locks = {};
    std::vector<std::string> pins = {};
    std::string said;
    ZEN_SHAPE(PaneSettingsShown, 1, ZEN_FIELD(provider), ZEN_FIELD(pane), ZEN_FIELD(revision),
              ZEN_FIELD(rows), ZEN_FIELD(settings), ZEN_FIELD(locks), ZEN_FIELD(pins),
              ZEN_FIELD(said));
};

/// WRITE ONE SETTING of the live layout's row at the revision it was read at: a value in its
/// kind's one field, or `clear` and no field for the setting's default.
struct PaneSettingCommitRequested {
    std::string provider;
    std::string pane;
    std::int64_t revision = 0;
    std::string key;
    std::optional<bool> flag = std::nullopt;
    std::optional<std::int64_t> number = std::nullopt;
    std::optional<std::string> text = std::nullopt;
    bool clear = false;
    ZEN_SHAPE(PaneSettingCommitRequested, 1, ZEN_FIELD(provider), ZEN_FIELD(pane),
              ZEN_FIELD(revision), ZEN_FIELD(key), ZEN_FIELD(flag), ZEN_FIELD(number),
              ZEN_FIELD(text), ZEN_FIELD(clear));
};

/// WHAT A COMMIT CAME TO: accepted, or refused in words naming what would have worked; the row's
/// revision after it, or as it stood; and how far the write got, in the band's words.
struct PaneSettingActed {
    bool accepted = false;
    std::string refusal;
    std::int64_t revision = 0;
    std::string handed;
    ZEN_SHAPE(PaneSettingActed, 1, ZEN_FIELD(accepted), ZEN_FIELD(refusal), ZEN_FIELD(revision),
              ZEN_FIELD(handed));
};

/// Provider -> Workshop, stamped: a setting it was handed at `revision` and cannot use, and why.
struct PaneSettingUnused {
    std::string pane;
    std::int64_t revision = 0;
    std::string key;
    std::string why;
    ZEN_SHAPE(PaneSettingUnused, 1, ZEN_FIELD(pane), ZEN_FIELD(revision), ZEN_FIELD(key),
              ZEN_FIELD(why));
};

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_SETTINGS_SEAM_VOCABULARY_HPP
