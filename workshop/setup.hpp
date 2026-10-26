// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_SETUP_HPP
#define ZENGINE_WORKSHOP_SETUP_HPP

// What a weaver calls the arrangement they are working in, and what that may mean.
// Workshop law: agents/workshop/setup-file.md (+8 registers; agents/workshop.md routes)

#include "pane_settings.hpp"
#include "panes.hpp"
#include "property.hpp"
#include "settings_seam_vocabulary.hpp" // a layout's baseline, and how long a pin list is

#include "ui/vocabulary.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace zengine::workshop {

/// The name a fresh Workshop's setup carries: a setup always has a name, so "unnamed" is no state.
inline constexpr const char* kDefaultSetupName = "Default";

/// The setup file's suggested name.
inline constexpr const char* kDefaultSetupFileName = "workshop-setup.json";

// ---- The bounds -------------------------------------------------------------

/// How long a setup's human name may be.
// WL-SETUP-09 -- agents/workshop/setup-file.md
inline constexpr std::size_t kMaxSetupNameLen = 32;

/// How long either half of a `PaneRef` may be.
// WL-SETUP-01, WL-SETUP-10 -- agents/workshop/setup-file.md
inline constexpr std::size_t kMaxPaneKeyLen = 64;

/// How many pane references one setup may carry: a desk may seat every pane a catalog holds.
// WL-MIG-03 -- agents/workshop/migration.md; WL-SETUP-01 -- agents/workshop/setup-file.md
inline constexpr std::size_t kMaxSetupPanes = 96;

/// THE MOST LAYOUTS ONE RUN KEEPS.
// WL-LAYOUT-08 -- agents/workshop/layouts.md
inline constexpr std::size_t kMaxLayouts = 8;

/// THE LARGEST PIXEL AMOUNT A PANE MAY BE AUTHORED AT, a place or an extent.
// WL-SETUP-03 -- agents/workshop/setup-file.md
inline constexpr std::int64_t kMaxPanePixels = 65536;

/// HOW MANY UNITS ONE DESK KEEPS, over all its rows: each setting, each lock, each pin list and
/// each pinned key is one. The bound keeps one layout's line of a session, every other bound full,
/// inside Loom's decode budget (`kMaxDecodedCells`), at no more than five decoded cells a unit.
// WL-SETTING-02 -- agents/workshop/settings.md
inline constexpr std::size_t kMaxPaneSettingsPerDesk = 512;

// ---- The value ---------------------------------------------------------------

/// WHICH PANE A WEAVER MEANT -- durably, and without naming a catalog slot.
// WL-SETUP-01 -- agents/workshop/setup-file.md
struct PaneRef {
    std::string provider;
    std::string pane;

    friend bool operator==(const PaneRef&, const PaneRef&) = default;
};

// ---- THE AUTHORED WINDOW: the smallest difference from a default --------------

/// THE WORDS A PANE'S AUTHORED WINDOW IS SAID IN are `ui`'s: a place is one of `ui::kPlaceWords`
/// and each size one of `ui::kSizeWords`, numbered and spelled by that one table. The two this host
/// names most get names of its own here.
// WL-SETUP-03, WL-SETUP-04 -- agents/workshop/setup-file.md
namespace pane_unit {
inline constexpr std::int64_t kDefault = ui::kSizeDefault; ///< the developer's answer
/// AN ABSOLUTE COUNT OF CANVAS PIXELS: one window pixel each, twelve to a terminal's cell.
// WL-GEO-06 -- agents/workshop/geometry.md; WL-SETUP-04 -- agents/workshop/setup-file.md
inline constexpr std::int64_t kPixels = ui::kSizePixels;
static_assert(kDefault == ui::kPlaceDefault && kPixels == ui::kPlacePixels,
              "`default` and `pixels` are one number in both of ui's lists");
} // namespace pane_unit

/// A cell as a setup or a session file counts it, in pixels: the files' own number, which no
/// medium's cell changes.
inline constexpr std::int64_t kSavedCellPx = 12;

/// THE SMALLEST AUTHORED EXTENT, in canvas pixels: one cell, the terminal window's step, so every
/// window can show an extent a weaver authors.
// WL-GEO-06 -- agents/workshop/geometry.md
inline constexpr std::int64_t kPanePxMin = kSavedCellPx;

/// WHERE A WEAVER PUT A PANE -- one fact, both coordinates.
// WL-PANE-11 -- agents/workshop/panes-and-windows.md; WL-SETUP-03 -- agents/workshop/setup-file.md
struct PanePlace {
    /// A place word (`ui::kPlaceWords`); `x` and `y` are its offsets, inward from the edges it
    /// names -- `share`'s ten-thousandths of the room from its top-left -- and 0 on an axis the
    /// word reads none of.
    std::int64_t mode = pane_unit::kDefault;
    std::int64_t x = 0;
    std::int64_t y = 0;

    friend bool operator==(const PanePlace&, const PanePlace&) = default;
};

/// HOW BIG A WEAVER MADE ONE AXIS OF A PANE.
// WL-SETUP-01 -- agents/workshop/setup-file.md
struct PaneSize {
    std::int64_t mode = pane_unit::kDefault; ///< a size word (`ui::kSizeWords`)
    std::int64_t amount = 0;                 ///< `pixels`' count, `share`'s ten-thousandths, else 0

    friend bool operator==(const PaneSize&, const PaneSize&) = default;
};

/// ONE ROW OF A SETUP: which pane, the smallest thing a weaver said about its window, and the
/// settings this layout keeps for it.
// WL-SETUP-01, WL-SETUP-07 -- agents/workshop/setup-file.md
struct SetupPane {
    PaneRef ref;
    PanePlace place;
    PaneSize width;
    PaneSize height;
    std::int64_t front = 0;
    /// In key order, one each; none is every setting at the pane's default.
    std::vector<PaneSetting> settings;

    friend bool operator==(const SetupPane&, const SetupPane&) = default;
};

/// A LOCK ON ONE KEY OF ONE ROW OF THIS LAYOUT: what the row keeps for the key stands while the
/// lock does. It holds no value.
// WL-LOCK-01 -- agents/workshop/setting-locks.md
struct SetupLock {
    PaneRef ref;
    std::string key;

    friend bool operator==(const SetupLock&, const SetupLock&) = default;
};

/// ONE PANE'S PIN LIST IN THIS LAYOUT, in pin order: what a reading of the pane shows first while
/// pins are kept per layout.
// WL-SETTING-12 -- agents/workshop/settings.md
struct SetupPins {
    PaneRef ref;
    std::vector<std::string> keys;

    friend bool operator==(const SetupPins&, const SetupPins&) = default;
};

/// A setup: what a weaver calls this arrangement, which panes it has, in order, and what this
/// layout keeps for itself -- the theme it was last given, its locks and its pins.
// WL-LAYOUT-01, WL-LAYOUT-06 -- agents/workshop/layouts.md
// WL-PANE-07 -- agents/workshop/panes-and-windows.md
struct Setup {
    std::string name;
    std::vector<SetupPane> panes;
    LayoutTheme theme;
    std::vector<SetupLock> locks;
    std::vector<SetupPins> pins;

    friend bool operator==(const Setup&, const Setup&) = default;
};

/// What a layout's link to its setup file says: derived from the desk and what the file was last
/// known to hold, stored nowhere.
// WL-LAYOUT-02 -- agents/workshop/layouts.md
namespace setup_link {
inline constexpr std::int64_t kNone = 0;     ///< no artifact is associated
inline constexpr std::int64_t kCurrent = 1;  ///< the desk equals the last known value
inline constexpr std::int64_t kModified = 2; ///< it is associated and differs from it
} // namespace setup_link

// ---- The reference a built-in kind carries, and the kind a reference names ---

/// THE DURABLE REFERENCE FOR AN INTERNAL KIND.
// WL-SETUP-01 -- agents/workshop/setup-file.md
inline PaneRef pane_ref_of(std::int64_t kind) {
    const BuiltinPane& row = builtin_pane(kind);
    return PaneRef{row.provider, row.pane};
}

/// WHICH INTERNAL KIND THIS REFERENCE NAMES, OR NOTHING.
inline std::optional<std::int64_t> resolve_builtin_pane(const PaneRef& ref) {
    for (std::size_t i = 0; i < kBuiltinPaneCount; ++i) {
        if (ref.provider == kBuiltinPanes[i].provider && ref.pane == kBuiltinPanes[i].pane) {
            return kBuiltinPanes[i].kind;
        }
    }
    return std::nullopt;
}

/// WHICH KIND THIS REFERENCE NAMES ON THIS SCREEN, IN THIS RUN, OR NOTHING --
/// asked of the compile-time catalog and of what this session has been offered.
inline std::optional<std::int64_t> resolve_pane(const PaneRef& ref, const Panes& panes) {
    const std::optional<std::int64_t> built_in = resolve_builtin_pane(ref);
    if (built_in.has_value()) {
        return built_in;
    }
    if (const RuntimePane* row = panes.runtime.find(ref.provider, ref.pane)) {
        return row->kind;
    }
    return std::nullopt;
}

/// Whether this build can currently present the pane this reference names.
inline bool resolvable(const PaneRef& ref, const Panes& panes) {
    return resolve_pane(ref, panes).has_value();
}

/// A reference as a person reads it: `provider/pane`.
inline std::string ref_text(const PaneRef& ref) { return ref.provider + "/" + ref.pane; }

/// A SETUP'S NAME AS ONE QUOTED TOKEN OF WEAVER-FACING PROSE.
// WL-TAB-07 -- agents/workshop/tab-run.md
inline std::string quoted_setup_name(const std::string& name) {
    std::string quoted;
    quoted.reserve(name.size() + 2);
    quoted += '"';
    for (const char c : name) {
        if (c == '\\' || c == '"') {
            quoted += '\\';
        }
        quoted += c;
    }
    quoted += '"';
    return quoted;
}

// ---- The law: what this application will accept as a setup -------------------

/// What this application accepts as a setup's human name; a typed name and a loaded one meet
/// this one function.
// WL-SETUP-09 -- agents/workshop/setup-file.md
inline Written check_setup_name(const std::string& name) {
    if (name.empty()) {
        return Written::no("a setup name cannot be empty");
    }
    if (name.size() > kMaxSetupNameLen) {
        return Written::no("a setup name is at most " + std::to_string(kMaxSetupNameLen) +
                           " bytes");
    }
    bool anything = false;
    for (const char c : name) {
        const unsigned char byte = static_cast<unsigned char>(c);
        if (byte < 0x20u || byte == 0x7Fu) {
            return Written::no("a setup name cannot contain control characters");
        }
        if (byte != ' ') {
            anything = true;
        }
    }
    if (!anything) {
        return Written::no("a setup name needs more than spaces in it");
    }
    return Written::ok();
}

/// Either half of a `PaneRef`, judged by shape and never by meaning. A view, so the offer door
/// judges Loom's stamp before anything owns a copy of it.
// WL-SETUP-10 -- agents/workshop/setup-file.md
inline Written check_pane_key(std::string_view key, const char* which) {
    if (key.empty()) {
        return Written::no(std::string("a pane reference's ") + which + " cannot be empty");
    }
    if (key.size() > kMaxPaneKeyLen) {
        return Written::no(std::string("a pane reference's ") + which + " is at most " +
                           std::to_string(kMaxPaneKeyLen) + " bytes");
    }
    for (const char c : key) {
        const unsigned char byte = static_cast<unsigned char>(c);
        if (byte <= ' ' || byte == 0x7Fu) {
            return Written::no(std::string("a pane reference's ") + which +
                               " cannot contain spaces or control characters");
        }
    }
    return Written::ok();
}

// WL-SETUP-10 -- agents/workshop/setup-file.md
inline Written check_pane_ref(const PaneRef& ref) {
    const Written provider = check_pane_key(ref.provider, "provider");
    if (!provider.accepted) {
        return provider;
    }
    return check_pane_key(ref.pane, "pane key");
}

// ---- What this application accepts as authored window intent -----------------
// WL-SETUP-03 -- agents/workshop/setup-file.md

/// ONE COORDINATE of an authored place. NEGATIVE IS REFUSED AT THE ROOT.
// WL-ARR-06 -- agents/workshop/arrangement.md; WL-SETUP-03 -- agents/workshop/setup-file.md
inline Written check_pane_place_coord(std::int64_t v) {
    if (v < 0) {
        return Written::no("a pane place cannot be negative");
    }
    if (v > kMaxPanePixels) {
        return Written::no("a pane place is at most " + std::to_string(kMaxPanePixels) +
                           " pixels");
    }
    return Written::ok();
}

/// One of ui's word lists as a weaver reads it: `a, b or c`.
template <std::size_t N>
inline std::string word_list(const std::array<std::string_view, N>& words) {
    std::string out;
    for (std::size_t i = 0; i < N; ++i) {
        out += i == 0 ? "" : (i + 1 == N ? " or " : ", ");
        out += words[i];
    }
    return out;
}

/// Every word a pane's place may be said in, and every word a size may: two closed lists.
// WL-SETUP-04 -- agents/workshop/setup-file.md
inline std::string pane_place_words() { return word_list(ui::kPlaceWords); }
inline std::string pane_size_words() { return word_list(ui::kSizeWords); }

/// A PLACE: one of ui's place words, with an offset on each axis the word reads, inward from the
/// edge it names, and 0 on each it does not: `default` and a centred axis read none. `share`'s
/// two are 0 to `ui::kShareWhole` ten-thousandths of the room's width and height.
// WL-SETUP-03 -- agents/workshop/setup-file.md
inline Written check_pane_place(const PanePlace& p) {
    if (p.mode == pane_unit::kDefault) {
        if (p.x != 0 || p.y != 0) {
            return Written::no("a default pane place carries no coordinates");
        }
        return Written::ok();
    }
    const std::string_view word = ui::place_word(p.mode);
    if (word.empty()) {
        return Written::no("a pane place is " + pane_place_words());
    }
    for (const bool across : {true, false}) {
        const std::int64_t v = across ? p.x : p.y;
        const bool reads = across ? ui::place_reads_x(p.mode) : ui::place_reads_y(p.mode);
        if (!reads && v != 0) {
            return Written::no("a `" + std::string(word) + "` pane place is centred " +
                               (across ? "across, so it carries no x" : "down, so it carries no y"));
        }
        if (reads && ui::place_reads_share(p.mode)) {
            if (v < 0 || v > ui::kShareWhole) {
                return Written::no(std::string("a share pane place's ") + (across ? "x" : "y") +
                                   " is 0 to " + std::to_string(ui::kShareWhole) +
                                   " ten-thousandths of the room's " +
                                   (across ? "width" : "height") + " (3333 is a third) -- not " +
                                   std::to_string(v));
            }
        } else if (reads) {
            if (const Written legal = check_pane_place_coord(v); !legal.accepted) {
                return legal;
            }
        }
    }
    return Written::ok();
}

/// ONE AXIS OF A SIZE: one of ui's size words; `pixels` counts at least a cell and at most
/// `kMaxPanePixels`, `share` 1 to `ui::kShareWhole` ten-thousandths of the room, and every other
/// word carries no amount.
// WL-SETUP-03 -- agents/workshop/setup-file.md
inline Written check_pane_size(const PaneSize& s, const char* which) {
    const std::string_view word = ui::size_word(s.mode);
    if (word.empty()) {
        return Written::no(std::string("a pane ") + which + " is " + pane_size_words());
    }
    if (s.mode == pane_unit::kPixels) {
        if (s.amount < kPanePxMin) {
            return Written::no(std::string("a pane ") + which + " is at least " +
                               std::to_string(kPanePxMin) + " pixels");
        }
        if (s.amount > kMaxPanePixels) {
            return Written::no(std::string("a pane ") + which + " is at most " +
                               std::to_string(kMaxPanePixels) + " pixels");
        }
        return Written::ok();
    }
    if (s.mode == ui::kSizeShare) {
        if (s.amount < 1 || s.amount > ui::kShareWhole) {
            return Written::no(std::string("a pane ") + which + "'s share is 1 to " +
                               std::to_string(ui::kShareWhole) +
                               " ten-thousandths of the room (5000 is half) -- not " +
                               std::to_string(s.amount));
        }
        return Written::ok();
    }
    if (s.amount != 0) {
        return Written::no("a " + std::string(word) + " pane " + which + " carries no amount");
    }
    return Written::ok();
}

/// WHAT A ROW'S SETTINGS MAY BE, judged by form alone: each one a setting (`pane_setting_problem`),
/// in key order and once each, at most `kMaxPaneSettingsPerRow`, and none under `workshop.` but
/// `workshop.skin`, which takes what its setting takes: a newer Workshop's word this one does not
/// know would be dropped unread.
// WL-SETTING-02 -- agents/workshop/settings.md
inline Written check_pane_settings(const PaneRef& ref, const std::vector<PaneSetting>& settings) {
    const std::string whose = "`" + ref_text(ref) + "`";
    if (settings.size() > kMaxPaneSettingsPerRow) {
        return Written::no(whose + " keeps at most " + std::to_string(kMaxPaneSettingsPerRow) +
                           " settings in a layout -- this row has " +
                           std::to_string(settings.size()));
    }
    for (std::size_t i = 0; i < settings.size(); ++i) {
        const PaneSetting& s = settings[i];
        const std::string wrong = pane_setting_problem(s);
        if (!wrong.empty()) {
            return Written::no(whose + ": " + wrong);
        }
        if (is_desk_setting_key(s.key)) {
            if (s.key != kWorkshopSkinKey) {
                return Written::no(whose + ": `" + s.key +
                                   "` is a Workshop setting this Workshop does not know");
            }
            if (const std::string refused =
                    pane_setting_refused_by(narrow_setting_row(workshop_skin_setting_row()), s);
                !refused.empty()) {
                return Written::no(whose + ": " + refused);
            }
        }
        if (i > 0 && !(settings[i - 1].key < s.key)) {
            return Written::no(whose + ": a row keeps its settings in key order, once each -- `" +
                               s.key + "` follows `" + settings[i - 1].key + "`");
        }
    }
    return Written::ok();
}

/// EVERY LAW ONE AUTHORED ROW MEETS, minus the two that are about the WHOLE setup
/// (no duplicate reference, and the rank permutation). One function so a typed
/// gesture and a loaded file cannot come to disagree about a row.
inline Written check_setup_pane(const SetupPane& row) {
    const Written legal = check_pane_ref(row.ref);
    if (!legal.accepted) {
        return legal;
    }
    const Written placed = check_pane_place(row.place);
    if (!placed.accepted) {
        return placed;
    }
    const Written wide = check_pane_size(row.width, "width");
    if (!wide.accepted) {
        return wide;
    }
    const Written tall = check_pane_size(row.height, "height");
    if (!tall.accepted) {
        return tall;
    }
    return check_pane_settings(row.ref, row.settings);
}

/// How many settings a desk keeps, over all its rows.
inline std::size_t desk_setting_count(const std::vector<SetupPane>& rows) noexcept {
    std::size_t n = 0;
    for (const SetupPane& row : rows) {
        n += row.settings.size();
    }
    return n;
}

/// HOW MANY UNITS A DESK SPENDS: its settings, its locks, its pin lists and their keys.
// WL-SETTING-02 -- agents/workshop/settings.md
inline std::size_t desk_units(const Setup& s) noexcept {
    std::size_t n = desk_setting_count(s.panes) + s.locks.size() + s.pins.size();
    for (const SetupPins& list : s.pins) {
        n += list.keys.size();
    }
    return n;
}

/// WHAT IS WRONG WITH A BASELINE'S THEME NAME, or empty: 1 to `kMaxPaneSettingTextLen` bytes of
/// `a`-`z`, `0`-`9` and `-`, as a theme's file is named.
// WL-LOCK-03 -- agents/workshop/setting-locks.md
inline std::string layout_theme_name_problem(std::string_view name) {
    if (name.empty() || name.size() > kMaxPaneSettingTextLen) {
        return "a baseline's theme name is 1 to " + std::to_string(kMaxPaneSettingTextLen) +
               " bytes";
    }
    for (const char c : name) {
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')) {
            return "a baseline's theme name is `a`-`z`, `0`-`9` and `-` -- `" + std::string(name) +
                   "` is not";
        }
    }
    return std::string();
}

/// WHAT A LAYOUT KEEPS FOR ITSELF, judged: its baseline a theme's name or none, and each lock and
/// pin list on a row the desk seats, once each, with keys a setting may have.
// WL-LAYOUT-06 -- agents/workshop/layouts.md
inline Written check_layout_own(const Setup& s) {
    if (s.theme.name.empty() && s.theme.fingerprint != 0) {
        return Written::no("a baseline of no theme has no fingerprint");
    }
    if (!s.theme.name.empty()) {
        if (const std::string wrong = layout_theme_name_problem(s.theme.name); !wrong.empty()) {
            return Written::no(wrong);
        }
    }
    for (std::size_t i = 0; i < s.locks.size(); ++i) {
        const SetupLock& lock = s.locks[i];
        const std::string whose = "`" + lock.ref.provider + "/" + lock.ref.pane + "`";
        if (std::find_if(s.panes.begin(), s.panes.end(), [&](const SetupPane& row) {
                return row.ref == lock.ref;
            }) == s.panes.end()) {
            return Written::no("a lock on " + whose + " holds a row this layout does not seat");
        }
        if (const std::string wrong = pane_setting_key_problem(lock.key); !wrong.empty()) {
            return Written::no("a lock on " + whose + ": " + wrong);
        }
        for (std::size_t j = 0; j < i; ++j) {
            if (s.locks[j] == lock) {
                return Written::no("the lock on `" + lock.key + "` of " + whose +
                                   " is listed twice");
            }
        }
    }
    for (std::size_t i = 0; i < s.pins.size(); ++i) {
        const SetupPins& list = s.pins[i];
        const std::string whose = "`" + list.ref.provider + "/" + list.ref.pane + "`";
        if (std::find_if(s.panes.begin(), s.panes.end(), [&](const SetupPane& row) {
                return row.ref == list.ref;
            }) == s.panes.end()) {
            return Written::no("the pins of " + whose + " are for a row this layout does not seat");
        }
        if (list.keys.size() > kMaxPinsPerPane) {
            return Written::no("the pins of " + whose + " hold more than " +
                               std::to_string(kMaxPinsPerPane) + " keys");
        }
        for (std::size_t k = 0; k < list.keys.size(); ++k) {
            if (const std::string wrong = pane_setting_key_problem(list.keys[k]); !wrong.empty()) {
                return Written::no("the pins of " + whose + ": " + wrong);
            }
            for (std::size_t m = 0; m < k; ++m) {
                if (list.keys[m] == list.keys[k]) {
                    return Written::no("the pins of " + whose + " pin `" + list.keys[k] +
                                       "` twice");
                }
            }
        }
        for (std::size_t j = 0; j < i; ++j) {
            if (s.pins[j].ref == list.ref) {
                return Written::no("the pins of " + whose + " are listed twice");
            }
        }
    }
    return Written::ok();
}

/// A row's place and sizes as ui's authored values: each number present where its word reads it.
inline ui::Place ui_place_of(const PanePlace& p) {
    ui::Place out{p.mode, std::nullopt, std::nullopt};
    if (ui::place_reads_x(p.mode)) {
        out.x = p.x;
    }
    if (ui::place_reads_y(p.mode)) {
        out.y = p.y;
    }
    return out;
}

inline ui::Size ui_size_of(const PaneSize& s) {
    return ui::Size{s.mode, ui::size_reads_amount(s.mode) ? std::optional<std::int64_t>(s.amount)
                                                          : std::nullopt};
}

/// THE DESK'S INTENT AS ui's PARTS, with no room: the room a frame, and each row a part standing
/// in it by its words. `ui::problem` judges it; nothing solves it.
// WL-SETUP-07 -- agents/workshop/setup-file.md
inline std::vector<ui::Element> desk_intent(const Setup& s) {
    std::vector<ui::Element> parts;
    parts.reserve(s.panes.size() + 1);
    ui::Element room;
    room.id = 1;
    room.label = "the room";
    parts.push_back(room);
    for (std::size_t i = 0; i < s.panes.size(); ++i) {
        const SetupPane& row = s.panes[i];
        ui::Element e;
        e.id = static_cast<std::int64_t>(i) + 2;
        e.label = ref_text(row.ref);
        e.context = room.id;
        e.place = ui_place_of(row.place);
        e.width = ui_size_of(row.width);
        e.height = ui_size_of(row.height);
        parts.push_back(std::move(e));
    }
    return parts;
}

/// THE WHOLE-SETUP LAW, asked once on a complete candidate.
/// It judges the name, every row, how many there are, whether any two name the
/// same pane, whether the ranks are a permutation, how many settings the rows keep, and what ui
/// finds wrong with the desk's intent alone (`ui::problem`).
// WL-SETUP-07 -- agents/workshop/setup-file.md
inline Written check_setup(const Setup& s) {
    const Written named = check_setup_name(s.name);
    if (!named.accepted) {
        return named;
    }
    if (s.panes.size() > kMaxSetupPanes) {
        return Written::no("a setup holds at most " + std::to_string(kMaxSetupPanes) +
                           " panes -- this one names " + std::to_string(s.panes.size()));
    }
    const std::int64_t n = static_cast<std::int64_t>(s.panes.size());
    for (std::size_t i = 0; i < s.panes.size(); ++i) {
        const Written legal = check_setup_pane(s.panes[i]);
        if (!legal.accepted) {
            return legal;
        }
        if (s.panes[i].front < 0 || s.panes[i].front >= n) {
            return Written::no("a pane's front order is 0 to " + std::to_string(n - 1) +
                               " -- `" + ref_text(s.panes[i].ref) + "` says " +
                               std::to_string(s.panes[i].front));
        }
        for (std::size_t j = 0; j < i; ++j) {
            if (s.panes[j].ref == s.panes[i].ref) {
                return Written::no("`" + ref_text(s.panes[i].ref) + "` is named twice");
            }
            if (s.panes[j].front == s.panes[i].front) {
                return Written::no("two panes claim front order " +
                                   std::to_string(s.panes[i].front) + " -- `" +
                                   ref_text(s.panes[j].ref) + "` and `" +
                                   ref_text(s.panes[i].ref) + "`");
            }
        }
    }
    if (const Written own = check_layout_own(s); !own.accepted) {
        return own;
    }
    const std::size_t units = desk_units(s);
    if (units > kMaxPaneSettingsPerDesk) {
        return Written::no("a setup keeps at most " + std::to_string(kMaxPaneSettingsPerDesk) +
                           " settings, locks and pins over all its panes -- this one keeps " +
                           std::to_string(units));
    }
    if (const std::string wrong = ui::problem(desk_intent(s)); !wrong.empty()) {
        return Written::no(wrong);
    }
    return Written::ok();
}

// ---- Operations on the authored intent ---------------------------------------

/// The answer for a pane the setup does not name.
// WL-SETUP-01 -- agents/workshop/setup-file.md
inline constexpr std::size_t kNoPaneRow = static_cast<std::size_t>(-1);

/// WHICH ROW OF THE SETUP NAMES THIS PANE, or `kNoPaneRow`. The one lookup, so a selection,
/// a geometry edit and an ordering operation all find a pane the same way -- by the
/// `PaneRef` that IS its identity, never by a runtime kind.
// WL-SETUP-01 -- agents/workshop/setup-file.md
inline std::size_t pane_row(const Setup& s, const PaneRef& ref) {
    for (std::size_t i = 0; i < s.panes.size(); ++i) {
        if (s.panes[i].ref == ref) {
            return i;
        }
    }
    return kNoPaneRow;
}

inline bool has_pane(const Setup& s, const PaneRef& ref) {
    return pane_row(s, ref) != kNoPaneRow;
}

/// The authored row for this reference, or nothing. By handle, and nothing may
/// hold one across an edit that could grow `panes`.
inline const SetupPane* pane_of(const Setup& s, const PaneRef& ref) {
    const std::size_t at = pane_row(s, ref);
    return at == kNoPaneRow ? nullptr : &s.panes[at];
}

inline SetupPane* pane_of(Setup& s, const PaneRef& ref) {
    const std::size_t at = pane_row(s, ref);
    return at == kNoPaneRow ? nullptr : &s.panes[at];
}

/// Add a reference to the end of the setup's order, or say it was already there.
// WL-PANE-07 -- agents/workshop/panes-and-windows.md; WL-SETUP-07 -- agents/workshop/setup-file.md
inline bool add_pane(Setup& s, const PaneRef& ref) {
    if (has_pane(s, ref)) {
        return false;
    }
    SetupPane row;
    row.ref = ref;
    row.front = static_cast<std::int64_t>(s.panes.size()); // == n-1 after the push
    s.panes.push_back(std::move(row));
    return true;
}

/// Remove a reference, AND CLOSE THE RANKS OVER IT; its locks and pins in this layout go with it.
// WL-SETUP-07 -- agents/workshop/setup-file.md; WL-SETTING-06 -- agents/workshop/settings.md
inline bool remove_pane(Setup& s, const PaneRef& ref) {
    const std::size_t at = pane_row(s, ref);
    if (at == kNoPaneRow) {
        return false;
    }
    std::erase_if(s.locks, [&ref](const SetupLock& l) { return l.ref == ref; });
    std::erase_if(s.pins, [&ref](const SetupPins& l) { return l.ref == ref; });
    const std::int64_t gone = s.panes[at].front;
    s.panes.erase(s.panes.begin() + static_cast<std::ptrdiff_t>(at));
    for (SetupPane& row : s.panes) {
        if (row.front > gone) {
            --row.front;
        }
    }
    return true;
}

/// WRITE ONE SETTING OF A PANE'S ROW IN THIS LAYOUT, OR CLEAR IT: an absent `value` clears `key`,
/// and a value equal to `fallback`, the pane's default, is stored as its absence. Judged whole
/// first -- the row on this desk, the setting's form, the row's and the desk's bounds -- and
/// written whole or not at all. What the pane takes is its declaration's to judge, before this.
// WL-SETTING-05 -- agents/workshop/settings.md
inline Written author_pane_setting(Setup& s, const PaneRef& ref, const std::string& key,
                                   const std::optional<PaneSetting>& value,
                                   const std::optional<PaneSetting>& fallback) {
    SetupPane* row = pane_of(s, ref);
    if (row == nullptr) {
        return Written::no(ref_text(ref) + " is not in this layout -- open it first");
    }
    std::vector<PaneSetting> candidate;
    candidate.reserve(row->settings.size() + 1);
    bool had = false;
    for (const PaneSetting& kept : row->settings) {
        if (kept.key == key) {
            had = true;
        } else {
            candidate.push_back(kept);
        }
    }
    const bool stored = value.has_value() && !(fallback.has_value() && *value == *fallback);
    if (!value.has_value() && !had) {
        return Written::no(key + " already takes its default -- nothing is stored to clear");
    }
    if (stored) {
        const std::string wrong = pane_setting_problem(*value);
        if (!wrong.empty()) {
            return Written::no(wrong);
        }
        std::size_t at = 0;
        while (at < candidate.size() && candidate[at].key < key) {
            ++at;
        }
        candidate.insert(candidate.begin() + static_cast<std::ptrdiff_t>(at), *value);
    }
    const Written legal = check_pane_settings(ref, candidate);
    if (!legal.accepted) {
        return legal;
    }
    const std::size_t desk = desk_units(s) - row->settings.size() + candidate.size();
    if (desk > kMaxPaneSettingsPerDesk) {
        return Written::no("this layout keeps " + std::to_string(kMaxPaneSettingsPerDesk) +
                           " settings already, the most a desk keeps -- clear one first");
    }
    row->settings = std::move(candidate);
    return Written::ok();
}

// ---- THE CANONICAL FRONT ORDER: five operations, all exact permutations ------
// WL-SETUP-07 -- agents/workshop/setup-file.md

/// The pane that currently sits at this rank, or `kNoPaneRow`. Total, and used by
/// the two step operations to find the neighbour they swap with.
inline std::size_t pane_at_front(const Setup& s, std::int64_t rank) {
    for (std::size_t i = 0; i < s.panes.size(); ++i) {
        if (s.panes[i].front == rank) {
            return i;
        }
    }
    return kNoPaneRow;
}

/// THE AUTHORED ORDER BECOMES THE DEFAULT ORDER: `front[i] = i`, in list order.
// WL-SETUP-07 -- agents/workshop/setup-file.md
inline void reset_front(Setup& s) {
    for (std::size_t i = 0; i < s.panes.size(); ++i) {
        s.panes[i].front = static_cast<std::int64_t>(i);
    }
}

/// SEND TO FRONT, or say it is already there.
// WL-SETUP-07 -- agents/workshop/setup-file.md
inline bool send_to_front(Setup& s, const PaneRef& ref) {
    const std::size_t at = pane_row(s, ref);
    if (at == kNoPaneRow) {
        return false;
    }
    if (s.panes[at].front == static_cast<std::int64_t>(s.panes.size()) - 1) {
        return false;
    }
    const std::int64_t was = s.panes[at].front;
    for (SetupPane& row : s.panes) {
        if (row.front > was) {
            --row.front;
        }
    }
    s.panes[at].front = static_cast<std::int64_t>(s.panes.size()) - 1;
    return true;
}

/// Send to back, or say it is already there -- `send_to_front`'s mirror, no-op included.
inline bool send_to_back(Setup& s, const PaneRef& ref) {
    const std::size_t at = pane_row(s, ref);
    if (at == kNoPaneRow) {
        return false;
    }
    if (s.panes[at].front == 0) {
        return false;
    }
    const std::int64_t was = s.panes[at].front;
    for (SetupPane& row : s.panes) {
        if (row.front < was) {
            ++row.front;
        }
    }
    s.panes[at].front = 0;
    return true;
}

/// SWAP WITH THE PANE IMMEDIATELY IN FRONT, or say there is none.
// WL-SETUP-07 -- agents/workshop/setup-file.md
inline bool raise_one(Setup& s, const PaneRef& ref) {
    const std::size_t at = pane_row(s, ref);
    if (at == kNoPaneRow) {
        return false;
    }
    const std::size_t ahead = pane_at_front(s, s.panes[at].front + 1);
    if (ahead == kNoPaneRow) {
        return false;
    }
    const std::int64_t was = s.panes[at].front;
    s.panes[at].front = s.panes[ahead].front;
    s.panes[ahead].front = was;
    return true;
}

inline bool lower_one(Setup& s, const PaneRef& ref) {
    const std::size_t at = pane_row(s, ref);
    if (at == kNoPaneRow) {
        return false;
    }
    if (s.panes[at].front == 0) {
        return false;
    }
    const std::size_t behind = pane_at_front(s, s.panes[at].front - 1);
    if (behind == kNoPaneRow) {
        return false;
    }
    const std::int64_t was = s.panes[at].front;
    s.panes[at].front = s.panes[behind].front;
    s.panes[behind].front = was;
    return true;
}

// ---- THE GEOMETRY DOORS: what a hand and a key both end at ------------------------------
// WL-SETUP-08 -- agents/workshop/setup-file.md

/// AUTHOR A PLACE. Writes the place and nothing else. `x`/`y` are pixels from the room's
/// top-left, directly under the top band.
// WL-PANE-11 -- agents/workshop/panes-and-windows.md
inline Written author_pane_place(Setup& s, const PaneRef& ref, std::int64_t x,
                                 std::int64_t y) {
    SetupPane* row = pane_of(s, ref);
    if (row == nullptr) {
        return Written::no("`" + ref_text(ref) + "` is not in this setup");
    }
    const PanePlace proposed{pane_unit::kPixels, x, y};
    const Written legal = check_pane_place(proposed);
    if (!legal.accepted) {
        return legal;
    }
    row->place = proposed;
    return Written::ok();
}

/// AUTHOR BOTH SIZE AXES AT ONCE, each in its own unit.
// WL-PANE-11 -- agents/workshop/panes-and-windows.md; WL-SETUP-08 -- agents/workshop/setup-file.md
inline Written author_pane_size(Setup& s, const PaneRef& ref, const PaneSize& width,
                                const PaneSize& height) {
    SetupPane* row = pane_of(s, ref);
    if (row == nullptr) {
        return Written::no("`" + ref_text(ref) + "` is not in this setup");
    }
    const Written wide = check_pane_size(width, "width");
    if (!wide.accepted) {
        return wide;
    }
    const Written tall = check_pane_size(height, "height");
    if (!tall.accepted) {
        return tall;
    }
    row->width = width;
    row->height = height;
    return Written::ok();
}

/// ONE AXIS of what a single gesture proposes for a pane's window, in the room's pixels: the near
/// edge where the hand left it, the extent it pulled, and what the axis stands at now -- its near
/// edge, its extent, the room's span along it and the window's step -- which the door needs to say
/// the axis in its own word.
// WL-ARR-06 -- agents/workshop/arrangement.md
struct PaneAxisProposal {
    std::optional<std::int64_t> position;
    std::optional<PaneSize> extent;
    std::int64_t base = 0;
    std::int64_t length = 0;
    std::int64_t span = 0;
    std::int64_t step = 1;
};

/// What authoring a window proposal did: `written` answers for the gesture as a
/// whole, and `place_written` says whether the place moved — the caller reconciles
/// (`apply_setup`) exactly then, because an authored place takes the pane out of the
/// stack; with no room rationing the stack, that reconcile opens and closes nothing.
/// `said` names each axis the hand wrote in another word than it stood in, and the word it was.
struct WindowWritten {
    Written written;
    bool place_written = false;
    std::string said;
};

/// The share of `span` whose edge, laid from `anchor`, falls on `length` pixels: the least share
/// reaching it from the start, the most from the end, so the floored edge is the hand's pixel for
/// every span up to `ui::kShareWhole` pixels. Never below 1 or past the whole.
// WL-ARR-06 -- agents/workshop/arrangement.md
inline std::int64_t share_reaching(std::int64_t length, std::int64_t span, ui::Anchor anchor) {
    if (span <= 0 || length >= span) {
        return ui::kShareWhole;
    }
    if (length <= 0) {
        return 1;
    }
    // length < span <= int64's range, and length * whole fits: length is at most kMaxPanePixels.
    const std::int64_t whole = ui::kShareWhole;
    const std::int64_t s = anchor == ui::Anchor::End ? length * whole / span
                                                     : (length * whole + span - 1) / span;
    return s < 1 ? 1 : (s > whole ? whole : s);
}

/// The share of `span` at which a `share` place's floored edge is `at` pixels from the room's
/// start: the least that reaches it, so the edge is the hand's pixel for every span up to
/// `ui::kShareWhole` pixels. 0 at or before the start, the whole at or past the end.
// WL-ARR-06 -- agents/workshop/arrangement.md
inline std::int64_t share_placing(std::int64_t at, std::int64_t span) {
    if (at <= 0 || span <= 0) {
        return 0;
    }
    if (at >= span) {
        return ui::kShareWhole;
    }
    // at < span, and at * whole fits: at is at most kMaxPanePixels.
    return (at * ui::kShareWhole + span - 1) / span;
}

/// A share as a weaver reads it: hundredths of a percent, `33.33%`.
inline std::string share_text(std::int64_t share) {
    const std::int64_t hundredths = share % 100;
    return std::to_string(share / 100) + "." + (hundredths < 10 ? "0" : "") +
           std::to_string(hundredths) + "%";
}

/// AUTHOR WHAT ONE GESTURE PROPOSES FOR A PANE'S WINDOW — per axis, each in its own word: an
/// anchor's offset from its own edge; a share place's, a share's, or a fill's pulled at its far
/// edge, the share whose edge is the hand's pixel. A moved centred or `default` place becomes
/// `pixels` at the hand's corner; `fit`, `default` and a moved `fill` become the hand's extent in
/// pixels. Every word of the weaver's that the hand changed is said (WL-ARR-06).
// WL-ARR-05, WL-ARR-06, WL-ARR-10 -- agents/workshop/arrangement.md
// WL-PED-05 -- agents/workshop/pane-manager.md
inline WindowWritten author_pane_window(Setup& s, const PaneRef& ref,
                                        const PaneAxisProposal& horizontal,
                                        const PaneAxisProposal& vertical) {
    SetupPane* row = pane_of(s, ref);
    if (row == nullptr) {
        return WindowWritten{Written::no("`" + ref_text(ref) + "` is not in this setup"), false,
                             {}};
    }
    Written wide = Written::ok();
    if (horizontal.position.has_value()) {
        wide = check_pane_place_coord(*horizontal.position);
    }
    if (wide.accepted && horizontal.extent.has_value()) {
        wide = check_pane_size(*horizontal.extent, "width");
    }
    Written tall = Written::ok();
    if (vertical.position.has_value()) {
        tall = check_pane_place_coord(*vertical.position);
    }
    if (tall.accepted && vertical.extent.has_value()) {
        tall = check_pane_size(*vertical.extent, "height");
    }
    const bool h_asks = horizontal.position.has_value() || horizontal.extent.has_value();
    const bool v_asks = vertical.position.has_value() || vertical.extent.has_value();
    bool h_lands = h_asks && wide.accepted;
    bool v_lands = v_asks && tall.accepted;
    if (!h_lands && !v_lands) {
        if (!h_asks && !v_asks) {
            return WindowWritten{Written::ok(), false, {}};
        }
        return WindowWritten{!wide.accepted ? wide : tall, false, {}};
    }

    // Where each axis's edges stand after the hand, in the room.
    struct Moved {
        std::int64_t from = 0, to = 0, length = 0;
        bool near_moved = false, far_moved = false, lands = false;
    };
    const auto moved = [](const PaneAxisProposal& p, bool lands) {
        Moved m;
        m.lands = lands;
        m.from = lands && p.position.has_value() ? *p.position : p.base;
        m.length = lands && p.extent.has_value() ? p.extent->amount : p.length;
        m.to = m.from + m.length;
        m.near_moved = lands && m.from != p.base;
        m.far_moved = lands && m.to != p.base + p.length;
        return m;
    };
    Moved mx = moved(horizontal, h_lands);
    Moved my = moved(vertical, v_lands);

    PanePlace place = row->place;
    PaneSize width = row->width;
    PaneSize height = row->height;
    std::string said;
    const auto say = [&said](const std::string& what) {
        said += (said.empty() ? "" : "; ") + what;
    };

    // THE PLACE. A word with a centred axis the hand moved cannot say where the hand left it, and
    // neither can `default` once the near edge moved: both become `pixels` at the hand's corner.
    const ui::Anchor was_x = ui::place_anchor_x(place.mode);
    const ui::Anchor was_y = ui::place_anchor_y(place.mode);
    const bool centred_moved = (was_x == ui::Anchor::Center && (mx.near_moved || mx.far_moved)) ||
                               (was_y == ui::Anchor::Center && (my.near_moved || my.far_moved));
    const bool default_moved = place.mode == pane_unit::kDefault &&
                               ((h_lands && horizontal.position.has_value()) ||
                                (v_lands && vertical.position.has_value()));
    const auto in_room = [](std::int64_t v) { return v < 0 ? std::int64_t{0} : v; };
    if (centred_moved || default_moved) {
        // A PLACE IS ONE FIELD. Each axis the hand moved writes its edge; the other contributes
        // where it stands, in the room. A pane above the room (the Layouts pane in its band) has
        // no room place on that axis, so it comes down to the room's edge, the nearest one it has.
        if (place.mode != pane_unit::kDefault) {
            say("place now pixels -- was " + std::string(ui::place_word(place.mode)));
        }
        place = PanePlace{pane_unit::kPixels, in_room(mx.from), in_room(my.from)};
    } else if (place.mode == pane_unit::kPixels) {
        if (h_lands && horizontal.position.has_value()) {
            place.x = mx.from;
        }
        if (v_lands && vertical.position.has_value()) {
            place.y = my.from;
        }
    } else if (ui::place_reads_share(place.mode)) {
        // A SHARE PLACE keeps its word: the share whose edge is the hand's pixel.
        if (mx.near_moved) {
            place.x = share_placing(mx.from, horizontal.span);
        }
        if (my.near_moved) {
            place.y = share_placing(my.from, vertical.span);
        }
    } else if (place.mode != pane_unit::kDefault) {
        // AN ANCHORED WORD keeps its word: the offset is counted from its own edge again.
        const auto offset = [](ui::Anchor anchor, const Moved& m, const PaneAxisProposal& p,
                               std::int64_t& at, Written& verdict) {
            if (!m.lands || anchor == ui::Anchor::Center) {
                return;
            }
            const std::int64_t v = anchor == ui::Anchor::End ? p.span - m.to : m.from;
            if ((anchor == ui::Anchor::End ? m.far_moved : m.near_moved)) {
                verdict = check_pane_place_coord(v);
                if (verdict.accepted) {
                    at = v;
                }
            }
        };
        offset(was_x, mx, horizontal, place.x, wide);
        offset(was_y, my, vertical, place.y, tall);
        // AN AXIS REFUSED HERE WRITES NOTHING, its size included; the other still settles.
        h_lands = h_lands && wide.accepted;
        v_lands = v_lands && tall.accepted;
        if (!h_lands && !v_lands) {
            return WindowWritten{!wide.accepted ? wide : tall, false, {}};
        }
        mx.lands = h_lands;
        my.lands = v_lands;
    }
    const ui::Anchor now_x = ui::place_anchor_x(place.mode);
    const ui::Anchor now_y = ui::place_anchor_y(place.mode);

    // THE SIZES, each in its own word.
    const auto size = [&say](PaneSize& at, const Moved& m, const PaneAxisProposal& p,
                             ui::Anchor was, ui::Anchor now, const char* axis) {
        const std::string word(ui::size_word(at.mode));
        // `default` becomes pixels unsaid, as it always has: no word of the weaver's is lost.
        const auto to_pixels = [&] {
            at = PaneSize{pane_unit::kPixels, m.length};
            if (word != "default") {
                say(std::string(axis) + " now " + std::to_string(m.length) + " px -- was " + word);
            }
        };
        if (!m.lands) {
            // An axis the hand left alone keeps its word, unless the place it fills from moved.
            if (at.mode == ui::kSizeFill && was != now) {
                to_pixels();
            }
            return;
        }
        const bool pulled = p.extent.has_value();
        switch (at.mode) {
        case pane_unit::kPixels:
            if (pulled) {
                at.amount = m.length;
            }
            return;
        case ui::kSizeShare:
            if (pulled) {
                at.amount = share_reaching(m.length, p.span, now);
            }
            return;
        case ui::kSizeFill: {
            // A FILL'S FAR EDGE is the frame's; the hand pulling it there writes the share whose
            // edge is the hand's pixel. Its near edge pulled leaves the far edge where it was.
            const bool far_pulled = now == ui::Anchor::End ? m.near_moved : m.far_moved;
            if (pulled && far_pulled) {
                at = PaneSize{ui::kSizeShare, share_reaching(m.length, p.span, now)};
                say(std::string(axis) + " now " + share_text(at.amount) + " -- was fill");
            } else if (!pulled || was != now) {
                to_pixels();
            }
            return;
        }
        default:
            if (pulled) {
                to_pixels();
            }
            return;
        }
    };
    size(width, mx, horizontal, was_x, now_x, "width");
    size(height, my, vertical, was_y, now_y, "height");

    const bool place_written = !(place == row->place);
    if (const Written legal = check_pane_place(place); !legal.accepted) {
        return WindowWritten{legal, false, {}};
    }
    row->place = place;
    row->width = width;
    row->height = height;
    return WindowWritten{Written::ok(), place_written, said};
}

/// The resets: each removes one authored difference and leaves the others.
inline bool reset_pane_place(Setup& s, const PaneRef& ref) {
    SetupPane* row = pane_of(s, ref);
    if (row == nullptr || row->place.mode == pane_unit::kDefault) {
        return false;
    }
    row->place = PanePlace{};
    return true;
}

inline bool reset_pane_width(Setup& s, const PaneRef& ref) {
    SetupPane* row = pane_of(s, ref);
    if (row == nullptr || row->width.mode == pane_unit::kDefault) {
        return false;
    }
    row->width = PaneSize{};
    return true;
}

inline bool reset_pane_height(Setup& s, const PaneRef& ref) {
    SetupPane* row = pane_of(s, ref);
    if (row == nullptr || row->height.mode == pane_unit::kDefault) {
        return false;
    }
    row->height = PaneSize{};
    return true;
}

/// A fresh Workshop's setup: the default panes, then Info at the room's top-right.
// WL-LAYOUT-03 -- agents/workshop/layouts.md; WL-SETUP-07 -- agents/workshop/setup-file.md
inline Setup default_setup() {
    Setup s;
    s.name = kDefaultSetupName;
    s.panes.reserve(kDefaultPaneCount);
    for (const std::int64_t kind : kDefaultPanes) {
        (void)add_pane(s, pane_ref_of(kind));
    }
    // Info opens at the room's top-right and Notices at its bottom-left, each at the size it asks
    // for, because these desk rows say so: the two places this host names a weave's office, as rows
    // a weaver may move or delete like any other. An office that never arrives leaves an unresolved
    // row (`unresolved_panes`).
    const std::pair<PaneRef, std::int64_t> seated[] = {
        {PaneRef{kInfoPaneProvider, kInfoPaneKey}, ui::kPlaceTopRight},
        {PaneRef{kNoticesPaneProvider, kNoticesPaneKey}, ui::kPlaceBottomLeft},
    };
    for (const auto& [ref, place] : seated) {
        if (add_pane(s, ref)) {
            for (SetupPane& row : s.panes) {
                if (row.ref == ref) {
                    row.place = PanePlace{place, 0, 0};
                }
            }
        }
    }
    return s;
}

} // namespace zengine::workshop

// What the live panes make of a setup: a part of this header, which needs the whole setup value
// before it.
#include "setup_reconcile.hpp"

#endif // ZENGINE_WORKSHOP_SETUP_HPP
