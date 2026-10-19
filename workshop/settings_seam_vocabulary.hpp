// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_SETTINGS_SEAM_VOCABULARY_HPP
#define ZENGINE_WORKSHOP_SETTINGS_SEAM_VOCABULARY_HPP

// The settings seam: one pane's settings on the live layout, read and written by message at the
// revision the row stands at, judged by the one setter Info's rows use; `provider` and `pane` address
// a row and are never authority. A pane says, stamped, a value it cannot use. An office declares,
// stamped, its office-wide settings -- one value for the whole Workshop, kept in Workshop's settings
// file -- read and written by the same askers. A pane's pin list, which an editor lists first, is
// edited by message and never handed to the pane.
// Workshop law: agents/workshop/settings.md

#include "pane_settings.hpp"

#include <zen/weave/shape.hpp>

#include <cstddef>
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

// ---- Office-wide settings -----------------------------------------------------------------

/// The most offices that declare office-wide settings, the most settings one office declares, and
/// the most kept between them all.
inline constexpr std::size_t kMaxSettingOffices = 16;
inline constexpr std::size_t kMaxOfficeSettings = kMaxPaneSettingsPerRow;
inline constexpr std::size_t kMaxOfficeSettingsKept = 256;

/// An office -> Workshop, stamped: the office-wide settings it takes, judged whole by a pane
/// declaration's rules; a later declaration replaces it. It counts while the weave that sent it
/// holds the office.
struct OfficeSettingsDeclared {
    std::vector<v2::PaneSettingRow> rows;
    ZEN_SHAPE(OfficeSettingsDeclared, 1, ZEN_FIELD(rows));
};

/// Workshop -> the office's holder, when it accepts this: the settings Workshop keeps for the
/// office, in key order, a default kept as its absence, and the revision they stand at.
struct OfficeSettings {
    std::int64_t revision = 0;
    std::vector<PaneSetting> settings;
    ZEN_SHAPE(OfficeSettings, 1, ZEN_FIELD(revision), ZEN_FIELD(settings));
};

/// Read one office's settings, answered to the asker alone.
struct OfficeSettingsRequested {
    std::string office;
    ZEN_SHAPE(OfficeSettingsRequested, 1, ZEN_FIELD(office));
};

/// THE OFFICE'S SETTINGS AS WORKSHOP KEEPS THEM: the revision they stand at (0 when nothing
/// declares for the office), what its declaration takes, what is kept, the keys locked, and
/// whether the settings file holds what is kept -- `written`, or why not.
struct OfficeSettingsShown {
    std::string office;
    std::int64_t revision = 0;
    std::vector<v2::PaneSettingRow> rows = {};
    std::vector<PaneSetting> settings = {};
    std::vector<SettingLockShown> locks = {};
    std::string written;
    ZEN_SHAPE(OfficeSettingsShown, 1, ZEN_FIELD(office), ZEN_FIELD(revision), ZEN_FIELD(rows),
              ZEN_FIELD(settings), ZEN_FIELD(locks), ZEN_FIELD(written));
};

/// WRITE ONE OFFICE-WIDE SETTING at the revision it was read at: a value in its kind's one field,
/// or `clear` and no field for the default. Answered `PaneSettingActed`, its `handed` saying
/// applied and written apart.
struct OfficeSettingCommitRequested {
    std::string office;
    std::int64_t revision = 0;
    std::string key;
    std::optional<bool> flag = std::nullopt;
    std::optional<std::int64_t> number = std::nullopt;
    std::optional<std::string> text = std::nullopt;
    bool clear = false;
    ZEN_SHAPE(OfficeSettingCommitRequested, 1, ZEN_FIELD(office), ZEN_FIELD(revision),
              ZEN_FIELD(key), ZEN_FIELD(flag), ZEN_FIELD(number), ZEN_FIELD(text),
              ZEN_FIELD(clear));
};

// ---- Pins ---------------------------------------------------------------------------------

/// The most settings one pane's pin list holds.
inline constexpr std::size_t kMaxPinsPerPane = 8;

/// What a pin act does, by its word: pin a key at the list's end, unpin it, move it one place up
/// or down, or put the list back to the pane's declared pins.
inline constexpr const char* kPinAct = "pin";
inline constexpr const char* kUnpinAct = "unpin";
inline constexpr const char* kPinUpAct = "up";
inline constexpr const char* kPinDownAct = "down";
inline constexpr const char* kPinResetAct = "reset";

/// Which list a pin act edits, as `workshop.pinned-scope` says it.
inline constexpr const char* kPinsWorkshopWide = "workshop-wide";
inline constexpr const char* kPinsPerLayout = "per-layout";

/// EDIT ONE PANE'S PIN LIST: `key` under `act` (`kPinAct` .. `kPinResetAct`; `reset` names no key),
/// in the list the pin scope reads -- for a per-layout scope, the list of layout `layout` (its
/// place on the shelf), -1 the live one.
struct PaneSettingPinRequested {
    std::string provider;
    std::string pane;
    std::string key;
    std::string act;
    std::int64_t layout = -1;
    ZEN_SHAPE(PaneSettingPinRequested, 1, ZEN_FIELD(provider), ZEN_FIELD(pane), ZEN_FIELD(key),
              ZEN_FIELD(act), ZEN_FIELD(layout));
};

/// WHAT A PIN ACT CAME TO: accepted, or refused in words; the list after it, in pin order; and
/// which scope's list that is.
struct PaneSettingPinActed {
    bool accepted = false;
    std::string refusal;
    std::vector<std::string> pins = {};
    std::string scope;
    ZEN_SHAPE(PaneSettingPinActed, 1, ZEN_FIELD(accepted), ZEN_FIELD(refusal), ZEN_FIELD(pins),
              ZEN_FIELD(scope));
};

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_SETTINGS_SEAM_VOCABULARY_HPP
