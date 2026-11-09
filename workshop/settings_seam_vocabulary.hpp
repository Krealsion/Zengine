// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_SETTINGS_SEAM_VOCABULARY_HPP
#define ZENGINE_WORKSHOP_SETTINGS_SEAM_VOCABULARY_HPP

// The settings seam, the layouts' owner's doors: one pane's row of the live layout, read and
// written by message at its revision, `provider` and `pane` addressing it and never authority; a
// role's role-wide settings, kept in the settings file; a pane's pin list, never handed to the
// pane; a lock in one layout or everywhere; and every layout's rows read at once and written in one
// batch judged whole. A layout is named by its id, 0 the live one. What a pane asks of its own row
// and says it cannot use is the pane's side of the hand-off (workshop/pane_settings.hpp).
// Workshop law: agents/workshop/settings.md, agents/workshop/setting-locks.md

#include "pane_settings.hpp"

#include <zen/weave/shape.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace zengine::workshop {

/// Where a lock holds a setting: in one layout, or in every layout and every row made later.
inline constexpr const char* kLockScopeLayout = "layout";
inline constexpr const char* kLockScopeEverywhere = "everywhere";

/// Where a lock holds a setting, as a row says it, and the refusal of a write it holds.
inline constexpr const char* kLockedEverywhere = kLockScopeEverywhere;
inline constexpr const char* kLockedInLayout = "in this layout";
inline std::string locked_setting_refusal(const std::string& key) {
    return key + " is locked -- nothing writes it while the lock stands, and only the weaver "
                 "unlocks it";
}

/// ONE KEY A LOCK HOLDS, and where the lock holds it: `layout` or `everywhere`.
struct SettingLockShown {
    std::string key;
    std::string scope;
    ZEN_SHAPE(SettingLockShown, 1, ZEN_FIELD(key), ZEN_FIELD(scope));
};

/// A LOCK AS WORKSHOP'S SETTINGS FILE KEEPS IT. Everywhere: a pane target (`<role>/<pane>`) pins
/// its value, or `clear` pins its default; a role target freezes the role's one value and
/// carries none. In one layout: `<role>/<pane>@<layout>` freezes the value that layout's row
/// holds, and carries none.
struct SettingLock {
    std::string target;
    std::string key;
    std::optional<bool> flag = std::nullopt;
    std::optional<std::int64_t> number = std::nullopt;
    std::optional<std::string> text = std::nullopt;
    bool clear = false;
    ZEN_SHAPE(SettingLock, 1, ZEN_FIELD(target), ZEN_FIELD(key), ZEN_FIELD(flag),
              ZEN_FIELD(number), ZEN_FIELD(text), ZEN_FIELD(clear));
    friend bool operator==(const SettingLock&, const SettingLock&) = default;
};

/// Read one pane's row of the live layout, answered to the asker alone.
struct PaneSettingsRequested {
    std::string provider;
    std::string pane;
    ZEN_SHAPE(PaneSettingsRequested, 1, ZEN_FIELD(provider), ZEN_FIELD(pane));
};

/// THE LIVE LAYOUT'S ROW FOR THE PANE: the revision it stands at (0 when the live layout seats no
/// such pane), what its counted declaration takes (empty when none counts), what the row keeps,
/// the keys locked, the keys pinned in pin order, and the owner's sentence about the row: why it
/// was withheld or is not on the live layout, else how far its hand-off got and what the pane said
/// it cannot use.
struct PaneSettingsAnswered {
    std::string provider;
    std::string pane;
    std::int64_t revision = 0;
    std::vector<v2::PaneSettingRow> rows = {};
    std::vector<PaneSetting> settings = {};
    std::vector<SettingLockShown> locks = {};
    std::vector<std::string> pins = {};
    std::string said;
    ZEN_SHAPE(PaneSettingsAnswered, 1, ZEN_FIELD(provider), ZEN_FIELD(pane), ZEN_FIELD(revision),
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
struct PaneSettingCommitAnswered {
    bool accepted = false;
    std::string refusal;
    std::int64_t revision = 0;
    std::string handed;
    ZEN_SHAPE(PaneSettingCommitAnswered, 1, ZEN_FIELD(accepted), ZEN_FIELD(refusal),
              ZEN_FIELD(revision), ZEN_FIELD(handed));
};

// ---- Role-wide settings -----------------------------------------------------------------

/// The most roles that declare role-wide settings, the most settings one role declares, and
/// the most kept between them all.
inline constexpr std::size_t kMaxSettingRoles = 16;
inline constexpr std::size_t kMaxRoleSettings = kMaxPaneSettingsPerRow;
inline constexpr std::size_t kMaxRoleSettingsKept = 256;

/// A role -> Workshop, stamped: the role-wide settings it takes, judged whole by a pane
/// declaration's rules; a later declaration replaces it. It counts while the weave that sent it
/// holds the role.
struct RoleSettingsDeclared {
    std::vector<v2::PaneSettingRow> rows;
    ZEN_SHAPE(RoleSettingsDeclared, 1, ZEN_FIELD(rows));
};

/// Workshop -> the role's holder, when it accepts this: the settings Workshop keeps for the
/// role, in key order, a default kept as its absence, and the revision they stand at.
struct RoleSettings {
    std::int64_t revision = 0;
    std::vector<PaneSetting> settings;
    ZEN_SHAPE(RoleSettings, 1, ZEN_FIELD(revision), ZEN_FIELD(settings));
};

/// Read one role's settings, answered to the asker alone.
struct RoleSettingsRequested {
    std::string role;
    ZEN_SHAPE(RoleSettingsRequested, 1, ZEN_FIELD(role));
};

/// THE ROLE'S SETTINGS AS WORKSHOP KEEPS THEM: the revision they stand at (0 when nothing
/// declares for the role), what its declaration takes, what is kept, the keys locked, and
/// whether the settings file holds what is kept -- `written`, or why not.
struct RoleSettingsAnswered {
    std::string role;
    std::int64_t revision = 0;
    std::vector<v2::PaneSettingRow> rows = {};
    std::vector<PaneSetting> settings = {};
    std::vector<SettingLockShown> locks = {};
    std::string written;
    ZEN_SHAPE(RoleSettingsAnswered, 1, ZEN_FIELD(role), ZEN_FIELD(revision), ZEN_FIELD(rows),
              ZEN_FIELD(settings), ZEN_FIELD(locks), ZEN_FIELD(written));
};

/// WRITE ONE ROLE-WIDE SETTING at the revision it was read at: a value in its kind's one field,
/// or `clear` and no field for the default. Answered `RoleSettingCommitAnswered`.
struct RoleSettingCommitRequested {
    std::string role;
    std::int64_t revision = 0;
    std::string key;
    std::optional<bool> flag = std::nullopt;
    std::optional<std::int64_t> number = std::nullopt;
    std::optional<std::string> text = std::nullopt;
    bool clear = false;
    ZEN_SHAPE(RoleSettingCommitRequested, 1, ZEN_FIELD(role), ZEN_FIELD(revision),
              ZEN_FIELD(key), ZEN_FIELD(flag), ZEN_FIELD(number), ZEN_FIELD(text),
              ZEN_FIELD(clear));
};

/// WHAT A ROLE-WIDE COMMIT CAME TO: accepted, or refused in words; the role's revision after
/// it, or as it stood; and `handed`, saying applied or handed and written apart.
struct RoleSettingCommitAnswered {
    bool accepted = false;
    std::string refusal;
    std::int64_t revision = 0;
    std::string handed;
    ZEN_SHAPE(RoleSettingCommitAnswered, 1, ZEN_FIELD(accepted), ZEN_FIELD(refusal),
              ZEN_FIELD(revision), ZEN_FIELD(handed));
};

// ---- The role-wide settings a pane built on an earlier vocabulary says ----------------------
//
// The same declaration and asks under their earlier names, the role named `office`: the layouts'
// owner reads each as its current form and answers an asker in the name it asked by.

struct OfficeSettingsDeclared {
    std::vector<v2::PaneSettingRow> rows;
    ZEN_SHAPE(OfficeSettingsDeclared, 1, ZEN_FIELD(rows));
};
struct OfficeSettingsRequested {
    std::string office;
    ZEN_SHAPE(OfficeSettingsRequested, 1, ZEN_FIELD(office));
};
struct OfficeSettingsAnswered {
    std::string office;
    std::int64_t revision = 0;
    std::vector<v2::PaneSettingRow> rows = {};
    std::vector<PaneSetting> settings = {};
    std::vector<SettingLockShown> locks = {};
    std::string written;
    ZEN_SHAPE(OfficeSettingsAnswered, 1, ZEN_FIELD(office), ZEN_FIELD(revision), ZEN_FIELD(rows),
              ZEN_FIELD(settings), ZEN_FIELD(locks), ZEN_FIELD(written));
};
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
struct OfficeSettingCommitAnswered {
    bool accepted = false;
    std::string refusal;
    std::int64_t revision = 0;
    std::string handed;
    ZEN_SHAPE(OfficeSettingCommitAnswered, 1, ZEN_FIELD(accepted), ZEN_FIELD(refusal),
              ZEN_FIELD(revision), ZEN_FIELD(handed));
};

inline RoleSettingsDeclared current(const OfficeSettingsDeclared& a) {
    return RoleSettingsDeclared{a.rows};
}
inline RoleSettingsRequested current(const OfficeSettingsRequested& a) {
    return RoleSettingsRequested{a.office};
}
inline RoleSettingCommitRequested current(const OfficeSettingCommitRequested& a) {
    return RoleSettingCommitRequested{a.office, a.revision, a.key, a.flag, a.number, a.text, a.clear};
}
inline OfficeSettingsAnswered as_office(const RoleSettingsAnswered& a) {
    return OfficeSettingsAnswered{a.role, a.revision, a.rows, a.settings, a.locks, a.written};
}
inline OfficeSettingCommitAnswered as_office(const RoleSettingCommitAnswered& a) {
    return OfficeSettingCommitAnswered{a.accepted, a.refusal, a.revision, a.handed};
}

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
/// in the list the pin scope reads -- for a per-layout scope, the list of the layout whose id is
/// `layout`, 0 the live one.
struct PaneSettingPinRequested {
    std::string provider;
    std::string pane;
    std::string key;
    std::string act;
    std::int64_t layout = 0;
    /// Version 2: `layout` is a layout's id, 0 the live one; version 1 named its place.
    ZEN_SHAPE(PaneSettingPinRequested, 2, ZEN_FIELD(provider), ZEN_FIELD(pane), ZEN_FIELD(key),
              ZEN_FIELD(act), ZEN_FIELD(layout));
};

/// WHAT A PIN ACT CAME TO: accepted, or refused in words; the list after it, in pin order; and
/// which scope's list that is.
struct PaneSettingPinAnswered {
    bool accepted = false;
    std::string refusal;
    std::vector<std::string> pins = {};
    std::string scope;
    ZEN_SHAPE(PaneSettingPinAnswered, 1, ZEN_FIELD(accepted), ZEN_FIELD(refusal),
              ZEN_FIELD(pins), ZEN_FIELD(scope));
};

// ---- Locks --------------------------------------------------------------------------------

/// What a lock act does, by its word.
inline constexpr const char* kLockAct = "lock";
inline constexpr const char* kUnlockAct = "unlock";

/// LOCK OR UNLOCK ONE SETTING: `target` is `<role>/<pane>`, or `<role>` for a role-wide
/// setting; `act` is `kLockAct` or `kUnlockAct`; `scope` is `kLockScopeLayout` (a pane's row in the
/// layout whose id is `layout`, 0 the live one) or `kLockScopeEverywhere`, which pins the value
/// that layout holds into every layout. An unlock is the weaver's: from the Terminal, or naming in
/// `gesture` the correlation of the weaver's current act sent to the asking role.
struct PaneSettingLockRequested {
    std::string target;
    std::string key;
    std::string act;
    std::string scope;
    std::int64_t layout = 0;
    std::int64_t gesture = 0;
    /// Version 2: `layout` is a layout's id, 0 the live one; version 1 named its place.
    ZEN_SHAPE(PaneSettingLockRequested, 2, ZEN_FIELD(target), ZEN_FIELD(key), ZEN_FIELD(act),
              ZEN_FIELD(scope), ZEN_FIELD(layout), ZEN_FIELD(gesture));
};

/// WHAT A LOCK ACT CAME TO: accepted, or refused in words; for an everywhere lock on a pane, the
/// layouts whose rows it wrote, and for a layout's lock, that layout.
struct PaneSettingLockAnswered {
    bool accepted = false;
    std::string refusal;
    std::vector<std::string> written = {};
    ZEN_SHAPE(PaneSettingLockAnswered, 1, ZEN_FIELD(accepted), ZEN_FIELD(refusal),
              ZEN_FIELD(written));
};

// ---- Every layout at once -----------------------------------------------------------------

/// Read every layout's rows, answered to the asker alone.
struct LayoutSettingsRequested {
    ZEN_SHAPE(LayoutSettingsRequested, 1);
};

/// A LAYOUT'S BASELINE AS AN OLDER FILE KEPT IT: the theme it was last given and that theme's
/// fingerprint then. Read from such a file as the bottom of the layout's themes, and never written.
struct LayoutTheme {
    std::string name;
    std::int64_t fingerprint = 0;
    ZEN_SHAPE(LayoutTheme, 1, ZEN_FIELD(name), ZEN_FIELD(fingerprint));
    friend bool operator==(const LayoutTheme&, const LayoutTheme&) = default;
};

/// ONE ROW OF ONE LAYOUT: the pane it seats, the settings it keeps, the keys locked on it there,
/// and the theme laid over the layout's for that pane alone ("" none).
struct LayoutPaneSettings {
    std::string provider;
    std::string pane;
    std::vector<PaneSetting> settings = {};
    std::vector<std::string> locked = {};
    std::string theme = {};
    /// Version 2: a row says the theme kept beside it.
    ZEN_SHAPE(LayoutPaneSettings, 2, ZEN_FIELD(provider), ZEN_FIELD(pane), ZEN_FIELD(settings),
              ZEN_FIELD(locked), ZEN_FIELD(theme));
    friend bool operator==(const LayoutPaneSettings&, const LayoutPaneSettings&) = default;
};

/// ONE LAYOUT: the id a door names it by, its place on the shelf, its name, the themes it wears,
/// bottom first, and its rows.
struct LayoutSettings {
    std::int64_t id = 0;
    std::int64_t position = 0;
    std::string name;
    std::vector<std::string> themes = {};
    std::vector<LayoutPaneSettings> panes = {};
    /// Version 3: a layout says the themes it wears, where version 2 said its baseline.
    ZEN_SHAPE(LayoutSettings, 3, ZEN_FIELD(id), ZEN_FIELD(position), ZEN_FIELD(name),
              ZEN_FIELD(themes), ZEN_FIELD(panes));
    friend bool operator==(const LayoutSettings&, const LayoutSettings&) = default;
};

/// WHAT A PANE OR A ROLE TAKES: a pane's counted declaration, or, with `pane` empty, a role's
/// role-wide rows.
struct DeclaredSettings {
    std::string provider;
    std::string pane;
    std::vector<v2::PaneSettingRow> rows = {};
    ZEN_SHAPE(DeclaredSettings, 1, ZEN_FIELD(provider), ZEN_FIELD(pane), ZEN_FIELD(rows));
    friend bool operator==(const DeclaredSettings&, const DeclaredSettings&) = default;
};

/// EVERY LAYOUT'S ROWS, what each pane and role takes, the everywhere locks, which layout is
/// live (its place), and the revision all of it stands at (0 when withheld).
struct LayoutSettingsAnswered {
    std::int64_t revision = 0;
    std::int64_t active = 0;
    std::vector<LayoutSettings> layouts = {};
    std::vector<DeclaredSettings> declared = {};
    std::vector<SettingLock> everywhere = {};
    /// Version 2: each layout says the themes it wears and each row the theme beside it.
    ZEN_SHAPE(LayoutSettingsAnswered, 2, ZEN_FIELD(revision), ZEN_FIELD(active),
              ZEN_FIELD(layouts), ZEN_FIELD(declared), ZEN_FIELD(everywhere));
    friend bool operator==(const LayoutSettingsAnswered&, const LayoutSettingsAnswered&) = default;
};

/// ONE WRITE OF A BATCH: a value in its kind's one field, or `clear`, under `key` of `target` --
/// `<role>/<pane>` in the layout whose id is `layout` (0 the live one), or `<role>`, for which
/// `layout` is not read.
struct SettingWrite {
    std::int64_t layout = 0;
    std::string target;
    std::string key;
    std::optional<bool> flag = std::nullopt;
    std::optional<std::int64_t> number = std::nullopt;
    std::optional<std::string> text = std::nullopt;
    bool clear = false;
    /// Version 2: `layout` is a layout's id, 0 the live one; version 1 named its place.
    ZEN_SHAPE(SettingWrite, 2, ZEN_FIELD(layout), ZEN_FIELD(target), ZEN_FIELD(key),
              ZEN_FIELD(flag), ZEN_FIELD(number), ZEN_FIELD(text), ZEN_FIELD(clear));
};

/// THE THEMES ONE LAYOUT WEARS, bottom first, written whole to the layout whose id is `layout` (0
/// the live one): none wears the pool's own look.
struct LayoutThemesWrite {
    std::int64_t layout = 0;
    std::vector<std::string> themes = {};
    ZEN_SHAPE(LayoutThemesWrite, 1, ZEN_FIELD(layout), ZEN_FIELD(themes));
};

/// THE THEME ONE ROW KEEPS BESIDE IT, laid over its layout's for that pane alone, written to row
/// `<role>/<pane>` of the layout whose id is `layout` (0 the live one): "" removes it.
struct RowThemeWrite {
    std::int64_t layout = 0;
    std::string target;
    std::string theme;
    ZEN_SHAPE(RowThemeWrite, 1, ZEN_FIELD(layout), ZEN_FIELD(target), ZEN_FIELD(theme));
};

/// MANY WRITES, LAYOUTS' THEMES AND ROWS' THEMES AT THE REVISION THE LAYOUTS WERE READ AT, judged
/// whole before anything is written; `cause` is what the band says they were.
struct SettingsWriteRequested {
    std::int64_t revision = 0;
    std::vector<SettingWrite> writes = {};
    std::vector<LayoutThemesWrite> themes = {};
    std::vector<RowThemeWrite> rows = {};
    std::string cause;
    /// Version 3: a layout's themes and a row's theme, where version 2 wrote a baseline.
    ZEN_SHAPE(SettingsWriteRequested, 3, ZEN_FIELD(revision), ZEN_FIELD(writes),
              ZEN_FIELD(themes), ZEN_FIELD(rows), ZEN_FIELD(cause));
};

/// What one write came to, by its word.
inline constexpr const char* kWriteWritten = "written";
inline constexpr const char* kWriteUnchanged = "unchanged";
inline constexpr const char* kWriteLocked = "locked";
inline constexpr const char* kWriteNotTaken = "not-taken";
inline constexpr const char* kWriteNotInLayout = "not-in-layout";

/// WHAT WRITE `index` CAME TO, by its word (`kWriteWritten` .. `kWriteNotInLayout`), and why.
struct SettingWriteResult {
    std::int64_t index = 0;
    std::string outcome;
    std::string said;
    ZEN_SHAPE(SettingWriteResult, 1, ZEN_FIELD(index), ZEN_FIELD(outcome), ZEN_FIELD(said));
};

/// WHAT A BATCH CAME TO: accepted with each write's outcome, or refused whole with nothing written;
/// the revision the layouts stand at after it; and the panes and roles it handed or applied.
struct SettingsWriteAnswered {
    bool accepted = false;
    std::string refusal;
    std::int64_t revision = 0;
    std::vector<SettingWriteResult> results = {};
    std::vector<std::string> handed = {};
    ZEN_SHAPE(SettingsWriteAnswered, 1, ZEN_FIELD(accepted), ZEN_FIELD(refusal),
              ZEN_FIELD(revision), ZEN_FIELD(results), ZEN_FIELD(handed));
};

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_SETTINGS_SEAM_VOCABULARY_HPP
