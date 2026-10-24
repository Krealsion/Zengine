// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_PREFS_PERSIST_HPP
#define ZENGINE_WORKSHOP_PREFS_PERSIST_HPP

// WORKSHOP'S SETTINGS FILE -- a seventh durable artifact, and the second file of the
// weaver-configuration kind: what holds in every layout -- every office's office-wide settings
// (Workshop's own among them), the Workshop-wide pin lists and the everywhere locks. What one
// layout keeps for itself is its desk's. A version-3 file also kept layouts' entries by name, read
// and carried until a session holds them; a version-1 or version-2 file, which held three
// preferences as words, is read as Workshop's own three settings.
// Workshop law: agents/workshop/settings.md, agents/workshop/focus.md, agents/workshop/key-spelling.md

#include "keymap.hpp"
#include "pane_settings.hpp"
#include "persist.hpp"
#include "screen.hpp" // the arrangement's measures: each setting's default and bounds
#include "settings_seam_vocabulary.hpp"

#include <zen/admission.hpp>
#include <zen/schema.hpp>
#include <zen/serialize.hpp>
#include <zen/value.hpp>
#include <zen/weave/shape.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace zengine::workshop::prefs_persist {

/// What a Workshop settings file says it is -- its own word, beside the family's others, so
/// handing Workshop the wrong one of its own files is named rather than half-read.
inline constexpr const char* kFormat = "zengine-workshop-prefs";

/// The format version this build writes. It reads this one and the three before it: version 3
/// kept layouts' entries beside the rest, version 2 held three preferences as words, version 1 the
/// titles alone.
// WL-SETTING-11 -- agents/workshop/settings.md
inline constexpr std::int64_t kFormatVersion = 4;
inline constexpr std::int64_t kLayoutEntriesVersion = 3;
inline constexpr std::int64_t kWordsVersion = 2;
inline constexpr std::int64_t kTitlesOnlyVersion = 1;

/// The ceiling is the read side of the decoder's own materialisation law -- a hostile file does
/// not get to choose the cost of refusing it.
inline constexpr std::uintmax_t kMaxPrefsBytes = 1u << 16;

/// The most pin lists, locks and (in a version-3 file) layout baselines the file keeps, and the
/// longest target a pin list or a lock names before any `@<layout>`.
inline constexpr std::size_t kMaxPinLists = 32;
inline constexpr std::size_t kMaxLocks = 128;
inline constexpr std::size_t kMaxLayoutThemes = 32;
inline constexpr std::size_t kMaxSettingTargetLen = 128;

/// The file's suggested name, beside the family's other defaults.
inline constexpr const char* kDefaultPrefsFileName = "workshop-prefs.json";

// ---- Workshop's own settings ----------------------------------------------------------

/// The office Workshop's own settings are kept under.
inline constexpr const char* kWorkshopOffice = "zengine.workshop";

/// Workshop's own keys: whether the arrangeable panes wear their titles, how the band and the
/// Hotkeys view spell a key, which pin list an editor edits, how the keyboard's selection is
/// ringed, the skin graphics areas wear, the order the Terminal offers a shape's fields in, and
/// the arrangement's measures.
inline constexpr const char* kPaneTitlesKey = "workshop.pane-titles";
inline constexpr const char* kBandKeysKey = "workshop.band-keys";
inline constexpr const char* kHotkeysKeysKey = "workshop.hotkeys-keys";
inline constexpr const char* kPinnedScopeKey = "workshop.pinned-scope";
inline constexpr const char* kFocusRingKey = "workshop.focus.ring";
inline constexpr const char* kSkinKey = kWorkshopSkinKey;
inline constexpr const char* kCompletionOrderKey = "workshop.completion-order";
inline constexpr const char* kArrangeStepKey = "workshop.arrange-step";
inline constexpr const char* kSnapReachKey = "workshop.snap-reach";
inline constexpr const char* kSideColumnsKey = "workshop.side-columns";
inline constexpr const char* kStackRowsKey = "workshop.stack-rows";

/// The completion's two orders: the shape's own, or its required fields before its optional ones.
inline constexpr const char* kCompletionDeclared = "declared";
inline constexpr const char* kCompletionRequiredFirst = "required-first";

/// The selection ring's two styles: the accent's two rings, or one ring in the chrome's ink.
inline constexpr const char* kRingAccent = "accent";
inline constexpr const char* kRingPlain = "plain";

// WL-FOCUS-11 -- agents/workshop/focus.md
inline constexpr const char* kTitlesDefault = "default";
inline constexpr const char* kTitlesShown = "shown";
inline constexpr const char* kTitlesHidden = "hidden";

inline constexpr const char* kTitlesWords = "default, shown or hidden";

/// What the code answers when no preference is authored: titles are shown.
inline constexpr bool kTitlesDefaultValue = true;

// WL-KEY-16 -- agents/workshop/key-spelling.md
inline constexpr const char* kKeysDefault = "default";
inline constexpr const char* kKeysShort = kSpelledShort;
inline constexpr const char* kKeysFull = kSpelledFull;

inline constexpr const char* kKeysWords = "default, short or full";

/// What the code answers when no spelling is authored: both full, as a keymap file writes a key.
inline constexpr KeySpelling kBandKeysDefaultValue = KeySpelling::kFull;
inline constexpr KeySpelling kHotkeysKeysDefaultValue = KeySpelling::kFull;

/// WORKSHOP'S OWN OFFICE-WIDE SETTINGS, declared in code: what each takes, its default, the tags
/// it is listed under, whether it is pinned by default, and what it does.
inline std::vector<workshop::v2::PaneSettingRow> workshop_office_rows() {
    const auto text = [](const char* key, const char* fallback, std::vector<std::string> choices,
                         std::vector<std::string> tags, bool pinned, const char* summary) {
        workshop::v2::PaneSettingRow row;
        row.key = key;
        row.text = fallback;
        row.choices = std::move(choices);
        row.tags = std::move(tags);
        row.pinned = pinned;
        row.summary = summary;
        return row;
    };
    // A NUMBER'S SUMMARY ENDS WITH ITS BOUNDS, read from them.
    const auto number = [](const char* key, std::int64_t fallback, std::int64_t low,
                           std::int64_t high, std::vector<std::string> tags, const char* summary) {
        workshop::v2::PaneSettingRow row;
        row.key = key;
        row.number = fallback;
        row.low = low;
        row.high = high;
        row.tags = std::move(tags);
        row.summary = std::string(summary) + " (" + std::to_string(low) + " to " +
                      std::to_string(high) + ")";
        return row;
    };
    workshop::v2::PaneSettingRow titles;
    titles.key = kPaneTitlesKey;
    titles.flag = kTitlesDefaultValue;
    titles.tags = {"chrome"};
    titles.pinned = true;
    titles.summary = "draw title text on pane borders";
    return {titles,
            text(kBandKeysKey, kKeysFull, {kKeysShort, kKeysFull}, {"chrome", "input"}, false,
                 "show short or full hotkey hints on the bottom band"),
            text(kHotkeysKeysKey, kKeysFull, {kKeysShort, kKeysFull}, {"chrome", "input"}, false,
                 "show short or full key chords in the Hotkeys pane"),
            text(kPinnedScopeKey, kPinsWorkshopWide, {kPinsWorkshopWide, kPinsPerLayout},
                 {"chrome", "settings"}, true, "pin order scope: workshop-wide or per-layout"),
            text(kFocusRingKey, kRingAccent, {kRingAccent, kRingPlain}, {"chrome", "focus"}, false,
                 "command-mode selection ring style: accent or plain"),
            workshop_skin_setting_row(),
            text(kCompletionOrderKey, kCompletionDeclared,
                 {kCompletionDeclared, kCompletionRequiredFirst}, {"completion", "input"}, false,
                 "argument completion field order: declared or required-first"),
            number(kArrangeStepKey, kCoarseStepCells, kMinArrangeStepCells, kMaxArrangeStepCells,
                   {"input", "layout"}, "coarse pane resize step in cells"),
            number(kSnapReachKey, kPaneSnapReachPx, kMinSnapReachPx, kMaxSnapReachPx,
                   {"input", "layout"}, "edge snap reach in canvas pixels"),
            number(kSideColumnsKey, kSideCols, kMinSideCols, kMaxSideCols, {"layout"},
                   "right-column width in cells"),
            number(kStackRowsKey, kStackRows, kMinStackRows, kMaxStackRows, {"layout"},
                   "overlay stack fallback height in rows")};
}

/// WORKSHOP'S SETTINGS AS THE SESSION SPENDS THEM: whether the arrangeable panes wear their
/// titles, how the band and the Hotkeys view each spell a key, whether pins are kept per layout,
/// whether the selection's ring is plain, the skin graphics areas wear, whether the completion
/// offers required fields first, and the arrangement's measures.
struct Presentation {
    bool titles_shown = kTitlesDefaultValue;
    KeySpelling band_keys = kBandKeysDefaultValue;
    KeySpelling hotkeys_keys = kHotkeysKeysDefaultValue;
    bool pins_per_layout = false;
    bool ring_plain = false;
    std::string skin = kSkinClassic;
    bool completion_required_first = false;
    std::int64_t arrange_step_cells = kCoarseStepCells;
    std::int64_t snap_reach_px = kPaneSnapReachPx;
    std::int64_t side_cols = kSideCols;
    std::int64_t stack_rows = kStackRows;
};

// ---- The file's own shape --------------------------------------------------------------

/// ONE OFFICE'S KEPT SETTINGS, in key order, a default kept as its absence.
struct OfficeSettingsKept {
    std::string office;
    std::vector<PaneSetting> settings;
    ZEN_SHAPE(OfficeSettingsKept, 1, ZEN_FIELD(office), ZEN_FIELD(settings));
    friend bool operator==(const OfficeSettingsKept&, const OfficeSettingsKept&) = default;
};

/// ONE PANE'S PIN LIST, in pin order: `<office>/<pane>` for the Workshop-wide list; a version-3
/// file also kept `<office>/<pane>@<layout>` for one layout's.
struct PinList {
    std::string target;
    std::vector<std::string> keys;
    ZEN_SHAPE(PinList, 1, ZEN_FIELD(target), ZEN_FIELD(keys));
    friend bool operator==(const PinList&, const PinList&) = default;
};

/// A lock as the file keeps it (`settings_seam_vocabulary.hpp`), the shape the doors carry.
using workshop::SettingLock;

/// A VERSION-3 FILE KEPT A LAYOUT'S BASELINE AS AN OFFICE OF ITS OWN, `@<layout>`, holding the
/// theme's name and its fingerprint under these two keys.
inline constexpr char kLayoutThemeOfficeMark = '@';
inline constexpr const char* kLayoutThemeNameKey = "theme";
inline constexpr const char* kLayoutThemeFingerprintKey = "fingerprint";

/// A whole saved settings file: what it is, which version, and what holds in every layout.
// WL-SETTING-11 -- agents/workshop/settings.md
struct WorkshopPrefs {
    std::string format;
    std::int64_t format_version = 0;
    std::vector<OfficeSettingsKept> offices;
    std::vector<PinList> pins;
    std::vector<SettingLock> locks;

    ZEN_SHAPE(WorkshopPrefs, 4, ZEN_FIELD(format), ZEN_FIELD(format_version),
              ZEN_FIELD(offices), ZEN_FIELD(pins), ZEN_FIELD(locks));
};

/// THE FILE AS VERSION 3 WROTE IT: version 4's fields, a layout's entries among them by its name.
/// Read, and written while what it keeps for layouts is not yet held by a session.
namespace v3 {
struct WorkshopPrefs {
    std::string format;
    std::int64_t format_version = 0;
    std::vector<OfficeSettingsKept> offices;
    std::vector<PinList> pins;
    std::vector<SettingLock> locks;

    ZEN_SHAPE(WorkshopPrefs, 3, ZEN_FIELD(format), ZEN_FIELD(format_version),
              ZEN_FIELD(offices), ZEN_FIELD(pins), ZEN_FIELD(locks));
};
} // namespace v3

/// THE FILE AS VERSION 2 WROTE IT, three preferences as words, copied verbatim: read as Workshop's
/// own three settings, and never written.
namespace v2 {
struct WorkshopPrefs {
    std::string format;
    std::int64_t format_version = 0;
    std::string titles;
    std::string band_keys;
    std::string hotkeys_keys;

    ZEN_SHAPE(WorkshopPrefs, 2, ZEN_FIELD(format), ZEN_FIELD(format_version),
              ZEN_FIELD(titles), ZEN_FIELD(band_keys), ZEN_FIELD(hotkeys_keys));
};
} // namespace v2

/// ...AND AS VERSION 1 WROTE IT, the titles alone.
namespace v1 {
struct WorkshopPrefs {
    std::string format;
    std::int64_t format_version = 0;
    std::string titles;

    ZEN_SHAPE(WorkshopPrefs, 1, ZEN_FIELD(format), ZEN_FIELD(format_version),
              ZEN_FIELD(titles));
};
} // namespace v1

/// The envelope's shape version and the format version are ONE NUMBER -- the family's coupling,
/// for the family's reason: a foreign version is refused by ITS number, before its fields are
/// judged against this shape.
static_assert(WorkshopPrefs::zen_version == static_cast<std::uint32_t>(kFormatVersion),
              "the prefs file's format version and its envelope's shape version are one "
              "number: a foreign version must be refused by ITS number, before its fields "
              "are judged against this shape");
static_assert(v3::WorkshopPrefs::zen_version ==
                  static_cast<std::uint32_t>(kLayoutEntriesVersion),
              "the file of layouts' entries is read by its own number");
static_assert(v2::WorkshopPrefs::zen_version == static_cast<std::uint32_t>(kWordsVersion),
              "the words file is read by its own number");
static_assert(v1::WorkshopPrefs::zen_version == static_cast<std::uint32_t>(kTitlesOnlyVersion),
              "the titles-only file is read by its own number");

// ---- What the file keeps, as values ----------------------------------------------------

/// THE SETTINGS FILE'S CONTENT: offices by name, each office's settings by key; pin lists by
/// target; locks by target and key. A default is kept as its absence, and an office keeping
/// nothing is not listed. Read from a version-3 file, it may also hold layouts' entries, by name.
struct Prefs {
    std::vector<OfficeSettingsKept> offices;
    std::vector<PinList> pins;
    std::vector<SettingLock> locks;
    friend bool operator==(const Prefs&, const Prefs&) = default;
};

/// The settings an office keeps, or none.
inline const OfficeSettingsKept* office_kept(const Prefs& p, std::string_view office) noexcept {
    for (const OfficeSettingsKept& o : p.offices) {
        if (o.office == office) {
            return &o;
        }
    }
    return nullptr;
}

/// THE VALUE AN OFFICE KEEPS UNDER `key`, or nullptr for its default.
inline const PaneSetting* office_value(const Prefs& p, std::string_view office,
                                       std::string_view key) noexcept {
    const OfficeSettingsKept* o = office_kept(p, office);
    return o == nullptr ? nullptr : find_pane_setting(o->settings, key);
}

/// KEEP `value` UNDER `key` FOR THE OFFICE, or clear it with none: in key order, the office
/// listed only while it keeps something. Says whether anything changed.
inline bool put_office_value(Prefs& p, const std::string& office, const std::string& key,
                             const std::optional<PaneSetting>& value) {
    auto at = std::find_if(p.offices.begin(), p.offices.end(),
                           [&](const OfficeSettingsKept& o) { return o.office == office; });
    if (!value) {
        if (at == p.offices.end()) {
            return false;
        }
        const std::size_t was = at->settings.size();
        std::erase_if(at->settings, [&](const PaneSetting& s) { return s.key == key; });
        const bool moved = at->settings.size() != was;
        if (at->settings.empty()) {
            p.offices.erase(at);
        }
        return moved;
    }
    if (at == p.offices.end()) {
        at = p.offices.insert(
            std::find_if(p.offices.begin(), p.offices.end(),
                         [&](const OfficeSettingsKept& o) { return o.office > office; }),
            OfficeSettingsKept{office, {}});
    }
    PaneSetting kept = *value;
    kept.key = key;
    for (PaneSetting& s : at->settings) {
        if (s.key == key) {
            const bool moved = !(s == kept);
            s = std::move(kept);
            return moved;
        }
    }
    at->settings.insert(std::find_if(at->settings.begin(), at->settings.end(),
                                     [&](const PaneSetting& s) { return s.key > key; }),
                        std::move(kept));
    return true;
}

/// Whether an office entry is a layout's baseline (`@<layout>`) rather than an office.
inline bool is_layout_theme_entry(std::string_view office) noexcept {
    return !office.empty() && office.front() == kLayoutThemeOfficeMark;
}

/// How many offices the file keeps settings for, and how many settings, every office's together;
/// a layout's baseline is neither.
inline std::size_t office_count(const Prefs& p) noexcept {
    return static_cast<std::size_t>(std::count_if(
        p.offices.begin(), p.offices.end(),
        [](const OfficeSettingsKept& o) { return !is_layout_theme_entry(o.office); }));
}
inline std::size_t kept_count(const Prefs& p) noexcept {
    std::size_t n = 0;
    for (const OfficeSettingsKept& o : p.offices) {
        n += is_layout_theme_entry(o.office) ? 0 : o.settings.size();
    }
    return n;
}

/// The pin list under `target`, or none.
inline const PinList* pin_list(const Prefs& p, std::string_view target) noexcept {
    for (const PinList& l : p.pins) {
        if (l.target == target) {
            return &l;
        }
    }
    return nullptr;
}

/// PUT A PIN LIST under its target, in target order.
inline void put_pin_list(Prefs& p, PinList list) {
    for (PinList& l : p.pins) {
        if (l.target == list.target) {
            l = std::move(list);
            return;
        }
    }
    const auto at = std::find_if(p.pins.begin(), p.pins.end(),
                                 [&](const PinList& l) { return l.target > list.target; });
    p.pins.insert(at, std::move(list));
}

/// THE LOCK ON `key` UNDER `target`, or none.
inline const SettingLock* lock_on(const Prefs& p, std::string_view target,
                                  std::string_view key) noexcept {
    for (const SettingLock& l : p.locks) {
        if (l.target == target && l.key == key) {
            return &l;
        }
    }
    return nullptr;
}

/// A pane target's name for one layout's list or lock: `<office>/<pane>@<layout>`.
inline std::string layout_target(std::string_view target, std::string_view layout) {
    return std::string(target) + "@" + std::string(layout);
}

/// PUT A LOCK under its target and key, in that order; one already there is replaced.
inline void put_lock(Prefs& p, SettingLock lock) {
    for (SettingLock& l : p.locks) {
        if (l.target == lock.target && l.key == lock.key) {
            l = std::move(lock);
            return;
        }
    }
    const auto at = std::find_if(p.locks.begin(), p.locks.end(), [&](const SettingLock& l) {
        return std::tie(l.target, l.key) > std::tie(lock.target, lock.key);
    });
    p.locks.insert(at, std::move(lock));
}

/// TAKE THE LOCK ON `key` UNDER `target` AWAY. Says whether there was one.
inline bool drop_lock(Prefs& p, std::string_view target, std::string_view key) {
    return std::erase_if(p.locks, [&](const SettingLock& l) {
               return l.target == target && l.key == key;
           }) != 0;
}

/// A LAYOUT'S BASELINE, or a name of "" when the file keeps none for it.
inline LayoutTheme layout_theme_of(const Prefs& p, std::string_view layout) {
    LayoutTheme out;
    const std::string office = std::string(1, kLayoutThemeOfficeMark) + std::string(layout);
    if (const PaneSetting* name = office_value(p, office, kLayoutThemeNameKey)) {
        out.name = name->text.value_or(std::string());
    }
    if (const PaneSetting* fingerprint = office_value(p, office, kLayoutThemeFingerprintKey)) {
        out.fingerprint = fingerprint->number.value_or(0);
    }
    return out;
}

/// WHETHER THE FILE KEEPS ANY LAYOUT'S ENTRY -- a pin list, a lock or a baseline under a layout's
/// name -- as only a version-3 file did.
// WL-SETTING-11 -- agents/workshop/settings.md
inline bool has_layout_entries(const Prefs& p) noexcept {
    const auto named = [](const std::string& target) {
        return target.find('@') != std::string::npos;
    };
    return std::any_of(p.offices.begin(), p.offices.end(),
                       [](const OfficeSettingsKept& o) {
                           return is_layout_theme_entry(o.office);
                       }) ||
           std::any_of(p.pins.begin(), p.pins.end(),
                       [&](const PinList& l) { return named(l.target); }) ||
           std::any_of(p.locks.begin(), p.locks.end(),
                       [&](const SettingLock& l) { return named(l.target); });
}

/// ...AND THE FILE WITHOUT THEM: what holds in every layout, alone.
inline Prefs without_layout_entries(Prefs p) {
    std::erase_if(p.offices,
                  [](const OfficeSettingsKept& o) { return is_layout_theme_entry(o.office); });
    std::erase_if(p.pins,
                  [](const PinList& l) { return l.target.find('@') != std::string::npos; });
    std::erase_if(p.locks,
                  [](const SettingLock& l) { return l.target.find('@') != std::string::npos; });
    return p;
}

// ---- Workshop's own settings, read and written as values --------------------------------

/// The value Workshop's office keeps under `key`, or the declared default.
inline PaneSetting workshop_value(const Prefs& p, std::string_view key) {
    if (const PaneSetting* kept = office_value(p, kWorkshopOffice, key)) {
        return *kept;
    }
    for (const workshop::v2::PaneSettingRow& row : workshop_office_rows()) {
        if (row.key == key) {
            return PaneSetting{row.key, row.flag, row.number, row.text};
        }
    }
    return PaneSetting{std::string(key), std::nullopt, std::nullopt, std::nullopt};
}

/// WORKSHOP'S SETTINGS AS THE SESSION SPENDS THEM, read from what the file keeps.
inline Presentation presentation_of(const Prefs& p) {
    const auto spelled = [&p](const char* key, KeySpelling fallback) {
        const PaneSetting v = workshop_value(p, key);
        if (v.text && *v.text == kKeysFull) {
            return KeySpelling::kFull;
        }
        if (v.text && *v.text == kKeysShort) {
            return KeySpelling::kShort;
        }
        return fallback;
    };
    Presentation out;
    const PaneSetting titles = workshop_value(p, kPaneTitlesKey);
    out.titles_shown = titles.flag.value_or(kTitlesDefaultValue);
    out.band_keys = spelled(kBandKeysKey, kBandKeysDefaultValue);
    out.hotkeys_keys = spelled(kHotkeysKeysKey, kHotkeysKeysDefaultValue);
    const PaneSetting scope = workshop_value(p, kPinnedScopeKey);
    out.pins_per_layout = scope.text && *scope.text == kPinsPerLayout;
    const PaneSetting ring = workshop_value(p, kFocusRingKey);
    out.ring_plain = ring.text && *ring.text == kRingPlain;
    const PaneSetting skin = workshop_value(p, kSkinKey);
    out.skin = skin.text && is_workshop_skin_choice(*skin.text) ? *skin.text
                                                                : std::string(kSkinClassic);
    const PaneSetting order = workshop_value(p, kCompletionOrderKey);
    out.completion_required_first = order.text && *order.text == kCompletionRequiredFirst;
    // A NUMBER OUTSIDE ITS BOUNDS NEVER REACHES HERE from a file or a commit, both judged against
    // the rows; one that did is held to them.
    const auto measure = [&p](const char* key, std::int64_t fallback, std::int64_t low,
                              std::int64_t high) {
        const PaneSetting v = workshop_value(p, key);
        return v.number ? std::clamp(*v.number, low, high) : fallback;
    };
    out.arrange_step_cells =
        measure(kArrangeStepKey, kCoarseStepCells, kMinArrangeStepCells, kMaxArrangeStepCells);
    out.snap_reach_px = measure(kSnapReachKey, kPaneSnapReachPx, kMinSnapReachPx, kMaxSnapReachPx);
    out.side_cols = measure(kSideColumnsKey, kSideCols, kMinSideCols, kMaxSideCols);
    out.stack_rows = measure(kStackRowsKey, kStackRows, kMinStackRows, kMaxStackRows);
    return out;
}

/// KEEP WORKSHOP'S SETTING `key` AT `value`: as its absence when it is the declared default.
inline bool put_workshop_value(Prefs& p, const std::string& key, PaneSetting value) {
    value.key = key;
    if (value == workshop_value(Prefs{}, key)) {
        return put_office_value(p, kWorkshopOffice, key, std::nullopt);
    }
    return put_office_value(p, kWorkshopOffice, key, value);
}

/// ...AND THE THREE A TOGGLE STATES, as the session holds them.
inline void put_presentation(Prefs& p, const Presentation& said) {
    PaneSetting titles;
    titles.flag = said.titles_shown;
    (void)put_workshop_value(p, kPaneTitlesKey, titles);
    PaneSetting band;
    band.text = std::string(spelling_word(said.band_keys));
    (void)put_workshop_value(p, kBandKeysKey, band);
    PaneSetting hotkeys;
    hotkeys.text = std::string(spelling_word(said.hotkeys_keys));
    (void)put_workshop_value(p, kHotkeysKeysKey, hotkeys);
}

/// A file keeping nothing but these three.
inline Prefs prefs_of(const Presentation& said) {
    Prefs out;
    put_presentation(out, said);
    return out;
}

// ---- Writing ---------------------------------------------------------------------------

inline WorkshopPrefs to_prefs(const Prefs& kept) {
    WorkshopPrefs out;
    out.format = kFormat;
    out.format_version = kFormatVersion;
    out.offices = kept.offices;
    out.pins = kept.pins;
    out.locks = kept.locks;
    return out;
}

/// THE FILE'S TEXT: version 4, or -- while it still carries layouts' entries a session does not
/// yet hold -- version 3, those entries unchanged.
// WL-SETTING-11 -- agents/workshop/settings.md
inline std::string to_text(const Prefs& kept) {
    if (has_layout_entries(kept)) {
        return loom::compat::serialize(loom::to_value(v3::WorkshopPrefs{
            kFormat, kLayoutEntriesVersion, kept.offices, kept.pins, kept.locks}));
    }
    return loom::compat::serialize(loom::to_value(to_prefs(kept)));
}

inline std::string to_text(const Presentation& said) { return to_text(prefs_of(said)); }

// ---- Reading ---------------------------------------------------------------------------

/// What reading produced: whether it worked, Workshop's settings as the session spends them, and
/// the whole of what the file keeps.
struct LoadedPrefs {
    Written outcome;
    Presentation prefs;
    Prefs kept;
    std::int64_t read_version = 0; ///< the version the file was written at

    static LoadedPrefs no(std::string why) {
        return LoadedPrefs{Written::no(std::move(why)), Presentation{}, Prefs{}, 0};
    }
};

/// One sentence for a version this build does not read, shared by the envelope preflight and the
/// field check so the two doors cannot word it differently.
inline std::string wrong_version(std::int64_t found) {
    return "prefs version " + std::to_string(found) + " -- this Workshop reads versions " +
           std::to_string(kTitlesOnlyVersion) + " to " + std::to_string(kFormatVersion);
}

/// The titles value a written word means. False for a word outside the closed set.
inline bool titles_in(const std::string& word, bool& out) {
    if (word == kTitlesDefault) {
        out = kTitlesDefaultValue;
        return true;
    }
    if (word == kTitlesShown) {
        out = true;
        return true;
    }
    if (word == kTitlesHidden) {
        out = false;
        return true;
    }
    return false;
}

/// The spelling a written word means, `fallback` for `default`. False for a word outside the
/// closed set.
inline bool keys_in(const std::string& word, KeySpelling fallback, KeySpelling& out) {
    if (word == kKeysDefault) {
        out = fallback;
        return true;
    }
    if (word == kKeysShort) {
        out = KeySpelling::kShort;
        return true;
    }
    if (word == kKeysFull) {
        out = KeySpelling::kFull;
        return true;
    }
    return false;
}

/// WHAT IS WRONG WITH A TARGET OR AN OFFICE NAME, or empty: 1 to `kMaxSettingTargetLen` bytes of
/// printable ASCII with no space.
inline std::string target_problem(std::string_view target, const char* what) {
    if (target.empty() || target.size() > kMaxSettingTargetLen) {
        return std::string(what) + " is 1 to " + std::to_string(kMaxSettingTargetLen) + " bytes";
    }
    for (const char c : target) {
        if (static_cast<unsigned char>(c) <= 0x20u || static_cast<unsigned char>(c) > 0x7Eu) {
            return std::string(what) + " `" + std::string(target) +
                   "` is printable ASCII with no space";
        }
    }
    return std::string();
}

/// WHAT IS WRONG WITH A PIN LIST'S OR A LOCK'S TARGET, or empty: a target, and after its first `@`
/// a layout's name as a layout is named (`check_setup_name`), spaces and all.
inline std::string layout_target_problem(std::string_view target, const char* what) {
    const std::size_t at = target.find('@');
    if (std::string wrong = target_problem(target.substr(0, at), what); !wrong.empty()) {
        return wrong;
    }
    if (at == std::string_view::npos) {
        return std::string();
    }
    if (const Written named = check_setup_name(std::string(target.substr(at + 1)));
        !named.accepted) {
        return std::string(what) + " `" + std::string(target) + "` names its layout wrongly: " +
               named.refusal;
    }
    return std::string();
}

/// WHAT IS WRONG WITH WHAT THE FILE KEEPS, or empty: each list within its bound and in
/// its order, nothing twice, every setting sound, and Workshop's own values ones its settings
/// take. A key of Workshop's office this build does not declare is kept as it is. A layout's entry
/// is refused unless `layout_entries`: a version-3 file's.
// WL-SETTING-11 -- agents/workshop/settings.md
inline std::string kept_problem(const Prefs& p, bool layout_entries = false) {
    if (!layout_entries && has_layout_entries(p)) {
        return "a version-" + std::to_string(kFormatVersion) +
               " settings file keeps what holds in every layout -- a layout's pins, locks and "
               "baseline are its own, in the session";
    }
    if (office_count(p) > kMaxSettingOffices) {
        return "the file keeps settings for at most " + std::to_string(kMaxSettingOffices) +
               " offices -- it lists " + std::to_string(office_count(p));
    }
    if (p.offices.size() - office_count(p) > kMaxLayoutThemes) {
        return "the file keeps at most " + std::to_string(kMaxLayoutThemes) +
               " layout baselines -- it keeps " +
               std::to_string(p.offices.size() - office_count(p));
    }
    if (kept_count(p) > kMaxOfficeSettingsKept) {
        return "the file keeps at most " + std::to_string(kMaxOfficeSettingsKept) +
               " office-wide settings -- it keeps " + std::to_string(kept_count(p));
    }
    const std::vector<workshop::v2::PaneSettingRow> own = workshop_office_rows();
    for (std::size_t i = 0; i < p.offices.size(); ++i) {
        const OfficeSettingsKept& o = p.offices[i];
        if (i > 0 && !(p.offices[i - 1].office < o.office)) {
            return "office `" + o.office + "` is listed twice or out of order";
        }
        if (is_layout_theme_entry(o.office)) {
            // A LAYOUT'S BASELINE: the layout's name, then its theme's name and fingerprint alone.
            const std::string layout = o.office.substr(1);
            if (const Written named = check_setup_name(layout); !named.accepted) {
                return "the baseline of `" + o.office + "` names its layout wrongly: " +
                       named.refusal;
            }
            const PaneSetting* name = find_pane_setting(o.settings, kLayoutThemeNameKey);
            const PaneSetting* fingerprint =
                find_pane_setting(o.settings, kLayoutThemeFingerprintKey);
            if (o.settings.size() != 2 || name == nullptr || fingerprint == nullptr ||
                setting_kind(*name) != SettingKind::kText ||
                setting_kind(*fingerprint) != SettingKind::kNumber ||
                !(o.settings[0].key < o.settings[1].key)) {
                return "the baseline of layout `" + layout + "` keeps `" +
                       std::string(kLayoutThemeFingerprintKey) + "` (a number) and `" +
                       kLayoutThemeNameKey + "` (a text), in that order, and nothing else";
            }
            if (std::string wrong = layout_theme_name_problem(*name->text); !wrong.empty()) {
                return "the baseline of layout `" + layout + "`: " + wrong;
            }
            continue;
        }
        if (std::string wrong = target_problem(o.office, "an office"); !wrong.empty()) {
            return wrong;
        }
        if (o.settings.size() > kMaxOfficeSettings) {
            return "office `" + o.office + "` keeps more than " +
                   std::to_string(kMaxOfficeSettings) + " settings";
        }
        for (std::size_t j = 0; j < o.settings.size(); ++j) {
            const PaneSetting& s = o.settings[j];
            if (std::string wrong = pane_setting_problem(s); !wrong.empty()) {
                return "office `" + o.office + "`: " + wrong;
            }
            if (j > 0 && !(o.settings[j - 1].key < s.key)) {
                return "office `" + o.office + "` keeps `" + s.key + "` twice or out of order";
            }
            if (o.office != kWorkshopOffice) {
                continue;
            }
            for (const workshop::v2::PaneSettingRow& row : own) {
                if (row.key != s.key) {
                    continue;
                }
                if (std::string wrong = workshop::v2::pane_setting_refused_by(row, s);
                    !wrong.empty()) {
                    return wrong;
                }
            }
        }
    }
    if (p.pins.size() > kMaxPinLists) {
        return "the file keeps at most " + std::to_string(kMaxPinLists) + " pin lists -- it keeps " +
               std::to_string(p.pins.size());
    }
    for (std::size_t i = 0; i < p.pins.size(); ++i) {
        const PinList& l = p.pins[i];
        if (std::string wrong = layout_target_problem(l.target, "a pin list's target");
            !wrong.empty()) {
            return wrong;
        }
        if (i > 0 && !(p.pins[i - 1].target < l.target)) {
            return "pin list `" + l.target + "` is listed twice or out of order";
        }
        if (l.keys.size() > kMaxPinsPerPane) {
            return "pin list `" + l.target + "` holds more than " +
                   std::to_string(kMaxPinsPerPane) + " keys";
        }
        for (std::size_t j = 0; j < l.keys.size(); ++j) {
            if (std::string wrong = pane_setting_key_problem(l.keys[j]); !wrong.empty()) {
                return "pin list `" + l.target + "`: " + wrong;
            }
            for (std::size_t k = 0; k < j; ++k) {
                if (l.keys[k] == l.keys[j]) {
                    return "pin list `" + l.target + "` pins `" + l.keys[j] + "` twice";
                }
            }
        }
    }
    if (p.locks.size() > kMaxLocks) {
        return "the file keeps at most " + std::to_string(kMaxLocks) + " locks -- it keeps " +
               std::to_string(p.locks.size());
    }
    for (std::size_t i = 0; i < p.locks.size(); ++i) {
        const SettingLock& l = p.locks[i];
        if (std::string wrong = layout_target_problem(l.target, "a lock's target");
            !wrong.empty()) {
            return wrong;
        }
        if (std::string wrong = pane_setting_key_problem(l.key); !wrong.empty()) {
            return "a lock on `" + l.target + "`: " + wrong;
        }
        if (i > 0 && !(std::tie(p.locks[i - 1].target, p.locks[i - 1].key) <
                       std::tie(l.target, l.key))) {
            return "the lock on `" + l.key + "` of `" + l.target +
                   "` is listed twice or out of order";
        }
        const SettingKind kind = setting_kind(l);
        if (kind == SettingKind::kSeveral || (l.clear && kind != SettingKind::kNone)) {
            return "the lock on `" + l.key + "` of `" + l.target +
                   "` holds more than one value -- a lock holds one, `clear`, or none";
        }
        if (kind == SettingKind::kText && !pane_setting_text_ok(*l.text)) {
            return "the lock on `" + l.key + "` of `" + l.target + "` holds a text past " +
                   std::to_string(kMaxPaneSettingTextLen) + " bytes of printable ASCII";
        }
        // ONE LAYOUT'S LOCK NAMES A PANE, AND FREEZES WHAT THAT LAYOUT'S ROW HOLDS: it pins nothing.
        const std::size_t at = l.target.find('@');
        if (at != std::string::npos) {
            const std::size_t slash = l.target.find('/');
            if (slash == std::string::npos || slash > at) {
                return "the lock on `" + l.key + "` of `" + l.target +
                       "` names a layout but no pane -- a layout's lock is on " +
                       "`<office>/<pane>@<layout>`";
            }
            if (kind != SettingKind::kNone || l.clear) {
                return "the lock on `" + l.key + "` of `" + l.target +
                       "` holds a value -- a layout's lock freezes its row and holds none";
            }
        }
    }
    return std::string();
}

/// Text to settings. Total: every input is either a settings file or a refusal with a reason, and
/// nothing here throws. The version preflight reads the envelope's CLAIM first, so a future file
/// is refused by its number rather than by the first field it renamed.
inline LoadedPrefs from_text(std::string_view bytes) {
    const loom::Unverified claim = loom::compat::parse(bytes);
    if (!claim.well_formed()) {
        const loom::Admission refused =
            loom::admit(claim, loom::schema_of<WorkshopPrefs>(), loom::Report::FirstError);
        return LoadedPrefs::no("not a Workshop prefs file: " +
                               refused.first_error().message());
    }
    const bool ours = claim.claimed_name() == std::string(WorkshopPrefs::zen_name);
    const std::uint32_t version = claim.claimed_version();
    if (ours && version != WorkshopPrefs::zen_version &&
        version != v3::WorkshopPrefs::zen_version && version != v2::WorkshopPrefs::zen_version &&
        version != v1::WorkshopPrefs::zen_version) {
        return LoadedPrefs::no(wrong_version(static_cast<std::int64_t>(version)));
    }
    std::string format;
    std::int64_t format_version = 0;
    std::int64_t expected = kFormatVersion;
    LoadedPrefs loaded;
    if (ours && version == v3::WorkshopPrefs::zen_version) {
        // VERSION 3: version 4's fields, read with what it kept for layouts by name.
        const loom::Admission admitted =
            loom::admit(claim, loom::schema_of<v3::WorkshopPrefs>(), loom::Report::FirstError);
        if (!admitted.ok()) {
            return LoadedPrefs::no(admitted.first_error().message());
        }
        const v3::WorkshopPrefs file = loom::from_value<v3::WorkshopPrefs>(admitted.value());
        format = file.format;
        format_version = file.format_version;
        expected = kLayoutEntriesVersion;
        loaded.kept = Prefs{file.offices, file.pins, file.locks};
    } else if (ours && version != WorkshopPrefs::zen_version) {
        // AN EARLIER FILE HELD THREE WORDS, OR THE TITLES ALONE: admitted against its own shape,
        // and read as Workshop's own settings, a word left out or `default` as the default.
        v2::WorkshopPrefs old;
        if (version == v1::WorkshopPrefs::zen_version) {
            const loom::Admission admitted = loom::admit(
                claim, loom::schema_of<v1::WorkshopPrefs>(), loom::Report::FirstError);
            if (!admitted.ok()) {
                return LoadedPrefs::no(admitted.first_error().message());
            }
            const v1::WorkshopPrefs one = loom::from_value<v1::WorkshopPrefs>(admitted.value());
            old = v2::WorkshopPrefs{one.format, one.format_version, one.titles, kKeysDefault,
                                    kKeysDefault};
            expected = kTitlesOnlyVersion;
        } else {
            const loom::Admission admitted = loom::admit(
                claim, loom::schema_of<v2::WorkshopPrefs>(), loom::Report::FirstError);
            if (!admitted.ok()) {
                return LoadedPrefs::no(admitted.first_error().message());
            }
            old = loom::from_value<v2::WorkshopPrefs>(admitted.value());
            expected = kWordsVersion;
        }
        format = old.format;
        format_version = old.format_version;
        Presentation words;
        if (!titles_in(old.titles, words.titles_shown)) {
            return LoadedPrefs::no("`" + old.titles + "` is not a pane-titles preference (" +
                                   kTitlesWords + ")");
        }
        if (!keys_in(old.band_keys, kBandKeysDefaultValue, words.band_keys)) {
            return LoadedPrefs::no("`" + old.band_keys +
                                   "` is not a spelling for the band's keys (" + kKeysWords + ")");
        }
        if (!keys_in(old.hotkeys_keys, kHotkeysKeysDefaultValue, words.hotkeys_keys)) {
            return LoadedPrefs::no("`" + old.hotkeys_keys +
                                   "` is not a spelling for the Hotkeys view's keys (" +
                                   kKeysWords + ")");
        }
        loaded.kept = prefs_of(words);
    } else {
        const loom::Admission admitted =
            loom::admit(claim, loom::schema_of<WorkshopPrefs>(), loom::Report::FirstError);
        if (!admitted.ok()) {
            return LoadedPrefs::no(admitted.first_error().message());
        }
        const WorkshopPrefs file = loom::from_value<WorkshopPrefs>(admitted.value());
        format = file.format;
        format_version = file.format_version;
        loaded.kept = Prefs{file.offices, file.pins, file.locks};
    }
    if (format != kFormat) {
        return LoadedPrefs::no("not a Workshop prefs file: it says it is `" + format + "`");
    }
    if (format_version != expected) {
        return LoadedPrefs::no(wrong_version(format_version));
    }
    if (std::string wrong = kept_problem(loaded.kept, expected == kLayoutEntriesVersion);
        !wrong.empty()) {
        return LoadedPrefs::no(wrong);
    }
    loaded.read_version = expected;
    loaded.prefs = presentation_of(loaded.kept);
    loaded.outcome = Written::ok();
    return loaded;
}

// ---- The file itself -------------------------------------------------------------------

/// Save the settings, through the family's one safe write: a complete candidate to a sibling,
/// then a rename over the destination. Through `write_file_making_room`, because this file's
/// ordinary home is the per-user configuration root, which is created on the first write and
/// never on a read.
inline Written save_file(const std::string& path, const Prefs& kept) {
    return persist::write_file_making_room(path, to_text(kept));
}

inline Written save_file(const std::string& path, const Presentation& said) {
    return save_file(path, prefs_of(said));
}

/// Read settings from a file. A missing file is not a refusal -- it is the defaults, silently --
/// so the caller that wants to distinguish "no file" from "a file this build refused" asks
/// `std::filesystem::exists` first, exactly as the keymap load does.
inline LoadedPrefs load_file(const std::string& path) {
    const persist::FileText read =
        persist::read_file(path, kMaxPrefsBytes, "a Workshop prefs file");
    if (!read.outcome.accepted) {
        return LoadedPrefs{read.outcome, Presentation{}, Prefs{}, 0};
    }
    return from_text(read.text);
}

} // namespace zengine::workshop::prefs_persist

#endif // ZENGINE_WORKSHOP_PREFS_PERSIST_HPP
