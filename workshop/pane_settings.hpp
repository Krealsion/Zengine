// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_WORKSHOP_PANE_SETTINGS_HPP
#define ZENGINE_WORKSHOP_PANE_SETTINGS_HPP

// A pane's settings: the choices a pane makes about how it presents itself, which Workshop keeps
// per layout in the pane's setup row. A setting is a key and a value of one of three kinds -- a
// flag, a number or a text -- in that kind's own field, exactly one present. What a setting
// means is the pane's alone; Workshop judges a setting's form and keeps it as it was given.
// Reference: docs/reference/workshop-panes.md#a-panes-settings.

#include <zen/weave/shape.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace zengine::workshop {

/// The longest key, the longest text, and the most settings one pane's row keeps.
inline constexpr std::size_t kMaxPaneSettingKeyLen = 32;
inline constexpr std::size_t kMaxPaneSettingTextLen = 64;
inline constexpr std::size_t kMaxPaneSettingsPerRow = 24;

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
