// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_INFO_SETTINGS_ROWS_HPP
#define ZENGINE_INFO_SETTINGS_ROWS_HPP

// The inspected pane's SETTINGS rows, as the layouts' owner reads its row of the live layout
// (`PaneSettingsAnswered`): a row per setting the declaration gives, and a kept row per key the row
// stores that none of them takes, each said with its lock; and a row's typed text as the one
// commit that writes it (`PaneSettingCommitRequested`), or why that text is no value it takes.
// Reference: info/docs/zengine.info.md. Workshop law: agents/workshop/settings.md

#include "workshop/inspection_seam_vocabulary.hpp"
#include "pane/settings.hpp"
#include "workshop/settings_seam_vocabulary.hpp"

#include "property/property.hpp" // a number typed, read by the inspector's one conversion

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace zengine::info_pane {

/// ONE SETTINGS ROW AS INFO SHOWS IT: the key, the value said with its lock, whether it is a
/// declared setting or a kept key no declaration takes, and the lock's own refusal when one holds.
struct SettingsRow {
    std::string key;
    std::string value;
    bool kept = false;
    std::string locked; ///< the refusal of a write while the lock stands, or empty
    pane::PaneSettingRow declared; ///< the declared row, for a declared setting
};

namespace settings_rows {

inline std::string_view without_edge_spaces(std::string_view text) {
    while (!text.empty() && text.front() == ' ') {
        text.remove_prefix(1);
    }
    while (!text.empty() && text.back() == ' ') {
        text.remove_suffix(1);
    }
    return text;
}

/// WHERE A LOCK ON `key` HOLDS, as a row says it, or empty for none.
inline std::string lock_of(const workshop::PaneSettingsAnswered& read, const std::string& key) {
    for (const workshop::SettingLockShown& l : read.locks) {
        if (l.key == key) {
            return l.scope == workshop::kLockScopeLayout ? workshop::kLockedInLayout
                                                         : workshop::kLockedEverywhere;
        }
    }
    return std::string();
}

/// A DECLARED SETTING'S VALUE: what the row stores, its default when it stores none; a stored value
/// the setting does not take is shown as stored, never repaired, beside what it takes.
// WL-SETTING-05 -- agents/workshop/settings.md
inline std::string value_text(const workshop::PaneSettingsAnswered& read,
                              const pane::PaneSettingRow& row) {
    const pane::PaneSetting fallback = pane::pane_setting_default(row);
    const pane::PaneSetting* stored = pane::find_pane_setting(read.settings, row.key);
    if (stored == nullptr) {
        return pane::setting_value_text(fallback);
    }
    if (pane::pane_setting_refused_by(row, *stored).empty()) {
        return pane::setting_value_text(*stored);
    }
    return pane::setting_value_text(*stored) + " -- takes " + pane::pane_setting_takes(row) +
           "; " + pane::setting_value_text(fallback) + " in effect";
}

/// THE ROWS, in the declaration's order and then the kept keys in the row's: every key the row
/// stores that no declared setting takes is a kept row.
// WL-SETTING-05 -- agents/workshop/settings.md
inline std::vector<SettingsRow> rows_of(const workshop::PaneSettingsAnswered& read) {
    std::vector<SettingsRow> out;
    std::vector<pane::PaneSettingRow> declared;
    for (const pane::v2::PaneSettingRow& row : read.rows) {
        declared.push_back(pane::narrow_setting_row(row));
    }
    std::vector<std::string> kept;
    for (const pane::PaneSetting& stored : read.settings) {
        bool takes = false;
        for (const pane::PaneSettingRow& row : declared) {
            takes = takes || row.key == stored.key;
        }
        if (!takes) {
            kept.push_back(stored.key);
        }
    }
    for (const pane::PaneSettingRow& row : declared) {
        SettingsRow r;
        r.key = row.key;
        r.declared = row;
        r.value = value_text(read, row);
        if (const std::string where = lock_of(read, row.key); !where.empty()) {
            // A LOCKED SETTING SAYS ITS LOCK IN ITS VALUE, and its write is refused, naming it.
            r.value += " -- locked " + where;
            r.locked = workshop::locked_setting_refusal(row.key);
        }
        out.push_back(std::move(r));
    }
    for (const std::string& key : kept) {
        SettingsRow r;
        r.key = key;
        r.kept = true;
        const pane::PaneSetting* stored = pane::find_pane_setting(read.settings, key);
        r.value = pane::setting_value_text(*stored) +
                  " -- kept; no setting here takes it, and `-` clears it";
        if (!lock_of(read, key).empty()) {
            r.locked = workshop::locked_setting_refusal(key);
        }
        out.push_back(std::move(r));
    }
    return out;
}

/// WHAT A ROW'S TYPED TEXT WRITES: the commit at the revision the row was read at, or why the text
/// is no value it takes. `-` clears, as it resets every row Info edits; a flag or a number is read
/// from what stands between the spaces, and a text is the text as typed, its spaces part of it.
// WL-SETTING-05 -- agents/workshop/settings.md
struct Typed {
    std::optional<workshop::PaneSettingCommitRequested> commit;
    std::string refusal;
};

inline Typed commit_of(const workshop::PaneSettingsAnswered& read, const SettingsRow& row,
                       const std::string& text) {
    Typed out;
    if (!row.locked.empty()) {
        out.refusal = row.locked;
        return out;
    }
    const std::string_view body = without_edge_spaces(text);
    workshop::PaneSettingCommitRequested commit;
    commit.provider = read.provider;
    commit.pane = read.pane;
    commit.revision = read.revision;
    commit.key = row.key;
    if (body == "-") {
        if (pane::find_pane_setting(read.settings, row.key) == nullptr) {
            out.refusal = row.key + " already takes its default -- nothing is stored to clear";
            return out;
        }
        commit.clear = true;
        out.commit = std::move(commit);
        return out;
    }
    if (row.kept) {
        out.refusal = "kept for a pane that declares it -- it takes only `-`, which clears it";
        return out;
    }
    const bool verbatim = pane::setting_kind(row.declared) == pane::SettingKind::kText;
    pane::PaneSetting typed;
    typed.key = row.key;
    switch (pane::setting_kind(row.declared)) {
    case pane::SettingKind::kFlag:
        if (body == pane::kSettingOn || body == pane::kSettingOff) {
            typed.flag = body == pane::kSettingOn;
        }
        break;
    case pane::SettingKind::kNumber:
        // A NUMBER'S ONE CONVERSION IS THE INSPECTOR'S OWN (WL-DOC-02).
        typed.number = zengine::property::TextForm<std::int64_t>::parse(body);
        break;
    case pane::SettingKind::kText: typed.text = text; break;
    default: break;
    }
    if (pane::setting_kind(typed) == pane::SettingKind::kNone ||
        !pane::pane_setting_refused_by(row.declared, typed).empty()) {
        out.refusal = "takes " + pane::pane_setting_takes(row.declared) + ", not `" +
                      (verbatim ? text : std::string(body)) + "`";
        return out;
    }
    commit.flag = typed.flag;
    commit.number = typed.number;
    commit.text = typed.text;
    out.commit = std::move(commit);
    return out;
}

} // namespace settings_rows
} // namespace zengine::info_pane

#endif // ZENGINE_INFO_SETTINGS_ROWS_HPP
