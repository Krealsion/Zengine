// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_SETTINGS_SEAM_VOCABULARY_HPP
#define ZENGINE_WORKSHOP_SETTINGS_SEAM_VOCABULARY_HPP

// The settings seam, the layouts' owner's doors: one pane's row of the live layout, read and
// written by message at its revision, `provider` and `pane` addressing it and never authority; an
// office's office-wide settings, kept in the settings file; a pane's pin list, never handed to the
// pane; a lock in one layout or everywhere; and every layout's rows read at once and written in one
// batch judged whole. A layout is named by its id, 0 the live one. Beside them, Workshop's: a value
// a pane it handed a row cannot use.
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

/// A LOCK AS WORKSHOP'S SETTINGS FILE KEEPS IT. Everywhere: a pane target (`<office>/<pane>`) pins
/// its value, or `clear` pins its default; an office target freezes the office's one value and
/// carries none. In one layout: `<office>/<pane>@<layout>` freezes the value that layout's row
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
/// the keys locked, the keys pinned in pin order, and the desk's sentence about the row.
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

/// WRITE ONE OFFICE-WIDE SETTING at the revision it was read at: a value in its kind's one field,
/// or `clear` and no field for the default. Answered `OfficeSettingCommitAnswered`.
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

/// WHAT AN OFFICE-WIDE COMMIT CAME TO: accepted, or refused in words; the office's revision after
/// it, or as it stood; and `handed`, saying applied or handed and written apart.
struct OfficeSettingCommitAnswered {
    bool accepted = false;
    std::string refusal;
    std::int64_t revision = 0;
    std::string handed;
    ZEN_SHAPE(OfficeSettingCommitAnswered, 1, ZEN_FIELD(accepted), ZEN_FIELD(refusal),
              ZEN_FIELD(revision), ZEN_FIELD(handed));
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

/// LOCK OR UNLOCK ONE SETTING: `target` is `<office>/<pane>`, or `<office>` for an office-wide
/// setting; `act` is `kLockAct` or `kUnlockAct`; `scope` is `kLockScopeLayout` (a pane's row in the
/// layout whose id is `layout`, 0 the live one) or `kLockScopeEverywhere`, which pins the value
/// that layout holds into every layout. An unlock is the weaver's: from the Terminal, or naming in
/// `gesture` the correlation of the weaver's current act sent to the asking office.
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

/// A LAYOUT'S BASELINE: the theme it was last given and that theme's fingerprint then; a name of
/// "" for none. The layouts' owner keeps it and judges its form; what the theme holds is the
/// theme owner's.
struct LayoutTheme {
    std::string name;
    std::int64_t fingerprint = 0;
    ZEN_SHAPE(LayoutTheme, 1, ZEN_FIELD(name), ZEN_FIELD(fingerprint));
    friend bool operator==(const LayoutTheme&, const LayoutTheme&) = default;
};

/// ONE ROW OF ONE LAYOUT: the pane it seats, the settings it keeps, and the keys locked on it there.
struct LayoutPaneSettings {
    std::string provider;
    std::string pane;
    std::vector<PaneSetting> settings = {};
    std::vector<std::string> locked = {};
    ZEN_SHAPE(LayoutPaneSettings, 1, ZEN_FIELD(provider), ZEN_FIELD(pane), ZEN_FIELD(settings),
              ZEN_FIELD(locked));
    friend bool operator==(const LayoutPaneSettings&, const LayoutPaneSettings&) = default;
};

/// ONE LAYOUT: the id a door names it by, its place on the shelf, its name, its baseline and its
/// rows.
struct LayoutSettings {
    std::int64_t id = 0;
    std::int64_t position = 0;
    std::string name;
    LayoutTheme theme;
    std::vector<LayoutPaneSettings> panes = {};
    /// Version 2: each layout says its id beside its place.
    ZEN_SHAPE(LayoutSettings, 2, ZEN_FIELD(id), ZEN_FIELD(position), ZEN_FIELD(name),
              ZEN_FIELD(theme), ZEN_FIELD(panes));
    friend bool operator==(const LayoutSettings&, const LayoutSettings&) = default;
};

/// WHAT A PANE OR AN OFFICE TAKES: a pane's counted declaration with `workshop.skin` first, or, with
/// `pane` empty, an office's office-wide rows.
struct DeclaredSettings {
    std::string provider;
    std::string pane;
    std::vector<v2::PaneSettingRow> rows = {};
    ZEN_SHAPE(DeclaredSettings, 1, ZEN_FIELD(provider), ZEN_FIELD(pane), ZEN_FIELD(rows));
    friend bool operator==(const DeclaredSettings&, const DeclaredSettings&) = default;
};

/// EVERY LAYOUT'S ROWS, what each pane and office takes, the everywhere locks, which layout is
/// live (its place), and the revision all of it stands at (0 when withheld).
struct LayoutSettingsAnswered {
    std::int64_t revision = 0;
    std::int64_t active = 0;
    std::vector<LayoutSettings> layouts = {};
    std::vector<DeclaredSettings> declared = {};
    std::vector<SettingLock> everywhere = {};
    ZEN_SHAPE(LayoutSettingsAnswered, 1, ZEN_FIELD(revision), ZEN_FIELD(active),
              ZEN_FIELD(layouts), ZEN_FIELD(declared), ZEN_FIELD(everywhere));
    friend bool operator==(const LayoutSettingsAnswered&, const LayoutSettingsAnswered&) = default;
};

/// ONE WRITE OF A BATCH: a value in its kind's one field, or `clear`, under `key` of `target` --
/// `<office>/<pane>` in the layout whose id is `layout` (0 the live one), or `<office>`, for which
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

/// ONE LAYOUT'S BASELINE, written to the layout whose id is `layout` (0 the live one): a name of ""
/// removes it.
struct LayoutThemeWrite {
    std::int64_t layout = 0;
    LayoutTheme theme;
    /// Version 2: `layout` is a layout's id, 0 the live one; version 1 named its place.
    ZEN_SHAPE(LayoutThemeWrite, 2, ZEN_FIELD(layout), ZEN_FIELD(theme));
};

/// MANY WRITES AND BASELINES AT THE REVISION THE LAYOUTS WERE READ AT, judged whole before
/// anything is written; `cause` is what the band says they were.
struct SettingsWriteRequested {
    std::int64_t revision = 0;
    std::vector<SettingWrite> writes = {};
    std::vector<LayoutThemeWrite> baselines = {};
    std::string cause;
    /// Version 2: its writes and baselines name layouts by id.
    ZEN_SHAPE(SettingsWriteRequested, 2, ZEN_FIELD(revision), ZEN_FIELD(writes),
              ZEN_FIELD(baselines), ZEN_FIELD(cause));
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
/// the revision the layouts stand at after it; and the panes and offices it handed or applied.
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
