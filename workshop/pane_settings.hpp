// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_WORKSHOP_PANE_SETTINGS_HPP
#define ZENGINE_WORKSHOP_PANE_SETTINGS_HPP

// A pane's settings: the choices a pane makes about how it presents itself, which Workshop keeps
// per layout in the pane's setup row and hands back to it. A setting is a key and a value of one
// of three kinds -- a flag, a number or a text -- in that kind's own field, exactly one present.
// A pane declares the settings it takes; Workshop judges a weaver's edit against them, keeps what
// it stores, and hands a seated pane its row's settings. What a setting means is the pane's.
// Reference: docs/reference/workshop-panes.md#a-panes-settings.

#include <zen/weave/shape.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace zengine::workshop {

/// The longest key, the longest text, the most settings one pane's row keeps (and so the most a
/// pane declares), and the most choices a text setting names.
inline constexpr std::size_t kMaxPaneSettingKeyLen = 32;
inline constexpr std::size_t kMaxPaneSettingTextLen = 64;
inline constexpr std::size_t kMaxPaneSettingsPerRow = 24;
inline constexpr std::size_t kMaxPaneSettingChoices = 16;

/// Keys under this prefix are Workshop's own.
inline constexpr std::string_view kDeskSettingPrefix = "workshop.";

/// How a flag is spelled where a weaver reads or types one.
inline constexpr const char* kSettingOn = "on";
inline constexpr const char* kSettingOff = "off";

/// ONE SETTING: its key, and its value in the one field of its kind. A value with no field
/// present, or two, is no value, and every reader refuses it by its key.
struct PaneSetting {
    std::string key;
    std::optional<bool> flag;
    std::optional<std::int64_t> number;
    std::optional<std::string> text;
    ZEN_SHAPE(PaneSetting, 1, ZEN_FIELD(key), ZEN_FIELD(flag), ZEN_FIELD(number),
              ZEN_FIELD(text));
    friend bool operator==(const PaneSetting&, const PaneSetting&) = default;
};

/// ONE SETTING A PANE TAKES: its key, which is also what a weaver reads it by, and its default in
/// the field of its kind -- which field is present IS the setting's kind. A text takes one of
/// `choices` when it names any, and any text when it names none; a number takes `low` to `high`,
/// a side left absent being open; a flag takes either.
struct PaneSettingRow {
    std::string key;
    std::optional<bool> flag;
    std::optional<std::int64_t> number;
    std::optional<std::string> text;
    std::vector<std::string> choices;
    std::optional<std::int64_t> low;
    std::optional<std::int64_t> high;
    ZEN_SHAPE(PaneSettingRow, 1, ZEN_FIELD(key), ZEN_FIELD(flag), ZEN_FIELD(number),
              ZEN_FIELD(text), ZEN_FIELD(choices), ZEN_FIELD(low), ZEN_FIELD(high));
    friend bool operator==(const PaneSettingRow&, const PaneSettingRow&) = default;
};

/// Provider -> Workshop: the settings one pane takes, judged whole under the office stamp and
/// refused aloud; a later declaration replaces it. It counts while the weave that sent it holds
/// the office, and nothing else waits on it.
struct PaneSettingsDeclared {
    std::string pane;
    std::vector<PaneSettingRow> rows;
    ZEN_SHAPE(PaneSettingsDeclared, 1, ZEN_FIELD(pane), ZEN_FIELD(rows));
};

/// Workshop -> the office's holder, when it accepts this: the settings the live layout keeps for
/// this pane, in key order, sent before any room the pane is granted. None is every setting at
/// its default; a pane reads the ones it declared and passes over the rest.
struct PaneSettings {
    std::string pane;
    std::vector<PaneSetting> settings;
    ZEN_SHAPE(PaneSettings, 1, ZEN_FIELD(pane), ZEN_FIELD(settings));
};

// ---- Kinds -------------------------------------------------------------------------------

/// WHICH OF THE THREE FIELDS A VALUE FILLS: one kind, none, or more than one.
enum class SettingKind { kNone, kFlag, kNumber, kText, kSeveral };

/// The kind a setting holds, or a declared row's default.
template <class Valued> inline SettingKind setting_kind(const Valued& v) noexcept {
    const int present = (v.flag ? 1 : 0) + (v.number ? 1 : 0) + (v.text ? 1 : 0);
    if (present == 0) {
        return SettingKind::kNone;
    }
    if (present > 1) {
        return SettingKind::kSeveral;
    }
    return v.flag ? SettingKind::kFlag : v.number ? SettingKind::kNumber : SettingKind::kText;
}

/// A kind's name, as a sentence says it.
inline const char* setting_kind_word(SettingKind k) noexcept {
    switch (k) {
    case SettingKind::kFlag: return "a flag";
    case SettingKind::kNumber: return "a number";
    case SettingKind::kText: return "a text";
    case SettingKind::kSeveral: return "more than one value";
    case SettingKind::kNone: break;
    }
    return "no value";
}

/// A VALUE AS A WEAVER READS IT: `on` or `off`, a decimal, or the text itself; empty for no
/// value or for more than one.
template <class Valued> inline std::string setting_value_text(const Valued& v) {
    switch (setting_kind(v)) {
    case SettingKind::kFlag: return *v.flag ? kSettingOn : kSettingOff;
    case SettingKind::kNumber: return std::to_string(*v.number);
    case SettingKind::kText: return *v.text;
    default: return std::string();
    }
}

// ---- Form ---------------------------------------------------------------------------------

/// WHAT IS WRONG WITH A KEY, or empty: 1 to `kMaxPaneSettingKeyLen` bytes of `a`-`z`, `0`-`9`,
/// `.` and `-`.
inline std::string pane_setting_key_problem(std::string_view key) {
    if (key.empty()) {
        return "a setting's key cannot be empty";
    }
    if (key.size() > kMaxPaneSettingKeyLen) {
        return "a setting's key is at most " + std::to_string(kMaxPaneSettingKeyLen) +
               " bytes -- this one has " + std::to_string(key.size());
    }
    for (const char c : key) {
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '-')) {
            return "a setting's key is `a`-`z`, `0`-`9`, `.` and `-` -- `" + std::string(key) +
                   "` is not";
        }
    }
    return std::string();
}

/// Whether a key is one of Workshop's own.
inline bool is_desk_setting_key(std::string_view key) noexcept {
    return key.substr(0, kDeskSettingPrefix.size()) == kDeskSettingPrefix;
}

/// WHETHER A SETTING MAY HOLD THIS TEXT: at most `kMaxPaneSettingTextLen` bytes of printable
/// ASCII, which every pane can draw.
inline bool pane_setting_text_ok(std::string_view text) noexcept {
    if (text.size() > kMaxPaneSettingTextLen) {
        return false;
    }
    for (const char c : text) {
        if (static_cast<unsigned char>(c) < 0x20u || static_cast<unsigned char>(c) > 0x7Eu) {
            return false;
        }
    }
    return true;
}

/// WHAT IS WRONG WITH ONE SETTING, or empty: its key, exactly one value, and a text's form.
inline std::string pane_setting_problem(const PaneSetting& s) {
    const std::string key = pane_setting_key_problem(s.key);
    if (!key.empty()) {
        return key;
    }
    const SettingKind kind = setting_kind(s);
    if (kind == SettingKind::kNone || kind == SettingKind::kSeveral) {
        return "setting `" + s.key + "` holds " + setting_kind_word(kind) +
               " -- a setting holds exactly one of a flag, a number or a text";
    }
    if (kind == SettingKind::kText && !pane_setting_text_ok(*s.text)) {
        return "setting `" + s.key + "` holds a text past " +
               std::to_string(kMaxPaneSettingTextLen) + " bytes of printable ASCII";
    }
    return std::string();
}

// ---- What a declared setting takes --------------------------------------------------------

/// WHAT A DECLARED SETTING TAKES, in a weaver's words: "on or off", "one of shown, hidden", ...
inline std::string pane_setting_takes(const PaneSettingRow& row) {
    switch (setting_kind(row)) {
    case SettingKind::kFlag: return std::string(kSettingOn) + " or " + kSettingOff;
    case SettingKind::kNumber:
        if (row.low && row.high) {
            return "a whole number from " + std::to_string(*row.low) + " to " +
                   std::to_string(*row.high);
        }
        if (row.low) {
            return "a whole number from " + std::to_string(*row.low) + " up";
        }
        if (row.high) {
            return "a whole number up to " + std::to_string(*row.high);
        }
        return "a whole number";
    case SettingKind::kText: {
        if (row.choices.empty()) {
            return "a text of at most " + std::to_string(kMaxPaneSettingTextLen) +
                   " printable characters";
        }
        std::string out = "one of ";
        for (std::size_t i = 0; i < row.choices.size(); ++i) {
            out += (i == 0 ? "" : ", ") + row.choices[i];
        }
        return out;
    }
    default: return "nothing";
    }
}

/// A VALUE AS A SENTENCE NAMES IT: its kind, and the value itself where it has one.
template <class Valued> inline std::string setting_value_named(const Valued& v) {
    const SettingKind kind = setting_kind(v);
    if (kind == SettingKind::kNone || kind == SettingKind::kSeveral) {
        return setting_kind_word(kind);
    }
    return std::string(setting_kind_word(kind)) + " `" + setting_value_text(v) + "`";
}

/// WHY A DECLARED SETTING DOES NOT TAKE A VALUE, naming what it does take; empty when it takes it.
template <class Valued>
inline std::string pane_setting_refused_by(const PaneSettingRow& row, const Valued& v) {
    const SettingKind want = setting_kind(row);
    const SettingKind have = setting_kind(v);
    bool takes = have == want;
    if (takes && want == SettingKind::kNumber) {
        takes = !(row.low && *v.number < *row.low) && !(row.high && *v.number > *row.high);
    }
    if (takes && want == SettingKind::kText) {
        takes = pane_setting_text_ok(*v.text);
        if (takes && !row.choices.empty()) {
            takes = false;
            for (const std::string& c : row.choices) {
                takes = takes || c == *v.text;
            }
        }
    }
    if (takes) {
        return std::string();
    }
    return row.key + " takes " + pane_setting_takes(row) + ", not " + setting_value_named(v);
}

/// WHAT IS WRONG WITH ONE DECLARED SETTING, or empty: a pane's own key, a default of one kind, and
/// what it takes said in its kind's terms, its default among it.
inline std::string pane_setting_row_problem(const PaneSettingRow& row) {
    const std::string key = pane_setting_key_problem(row.key);
    if (!key.empty()) {
        return key;
    }
    const std::string at = "setting `" + row.key + "`";
    if (is_desk_setting_key(row.key)) {
        return at + " is Workshop's own -- a pane declares no `" +
               std::string(kDeskSettingPrefix) + "` key";
    }
    const SettingKind kind = setting_kind(row);
    if (kind == SettingKind::kNone || kind == SettingKind::kSeveral) {
        return at + "'s default holds " + setting_kind_word(kind) +
               " -- a default is exactly one of a flag, a number or a text, and that is its kind";
    }
    if (kind != SettingKind::kText && !row.choices.empty()) {
        return at + " is " + setting_kind_word(kind) + ", and only a text names choices";
    }
    if (kind != SettingKind::kNumber && (row.low || row.high)) {
        return at + " is " + setting_kind_word(kind) + ", and only a number has a low or a high";
    }
    if (row.low && row.high && *row.low > *row.high) {
        return at + "'s low is above its high";
    }
    if (row.choices.size() > kMaxPaneSettingChoices) {
        return at + " names more than " + std::to_string(kMaxPaneSettingChoices) + " choices";
    }
    for (std::size_t i = 0; i < row.choices.size(); ++i) {
        const std::string& c = row.choices[i];
        if (c.empty() || c == "-" || c.front() == ' ' || c.back() == ' ' ||
            !pane_setting_text_ok(c)) {
            return at + "'s choice `" + c + "` is not one a weaver can type -- a choice is " +
                   "printable text, not `-`, with no space at either edge";
        }
        for (std::size_t j = 0; j < i; ++j) {
            if (row.choices[j] == c) {
                return at + " names the choice `" + c + "` twice";
            }
        }
    }
    const std::string own = pane_setting_refused_by(row, row);
    if (!own.empty()) {
        return at + "'s default is not one it takes: " + own;
    }
    return std::string();
}

/// WHAT IS WRONG WITH A WHOLE DECLARATION, or empty: at most `kMaxPaneSettingsPerRow` settings,
/// each sound, no key twice.
inline std::string pane_settings_declared_problem(const PaneSettingsDeclared& d) {
    if (d.rows.size() > kMaxPaneSettingsPerRow) {
        return "a pane declares at most " + std::to_string(kMaxPaneSettingsPerRow) +
               " settings -- this declares " + std::to_string(d.rows.size());
    }
    for (std::size_t i = 0; i < d.rows.size(); ++i) {
        const std::string wrong = pane_setting_row_problem(d.rows[i]);
        if (!wrong.empty()) {
            return wrong;
        }
        for (std::size_t j = 0; j < i; ++j) {
            if (d.rows[j].key == d.rows[i].key) {
                return "setting `" + d.rows[i].key + "` is declared twice";
            }
        }
    }
    return std::string();
}

/// A DECLARED SETTING'S DEFAULT, as the setting it stands for.
inline PaneSetting pane_setting_default(const PaneSettingRow& row) {
    return PaneSetting{row.key, row.flag, row.number, row.text};
}

/// THE SETTING UNDER `key` in a list, or nullptr.
inline const PaneSetting* find_pane_setting(const std::vector<PaneSetting>& all,
                                            std::string_view key) noexcept {
    for (const PaneSetting& s : all) {
        if (s.key == key) {
            return &s;
        }
    }
    return nullptr;
}

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_PANE_SETTINGS_HPP
