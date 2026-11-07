// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_SETTINGS_VOCABULARY_HPP
#define ZENGINE_SETTINGS_VOCABULARY_HPP

// THE SETTINGS OFFICE'S DOORS, `zengine.settings`, held by the `zengine-settings` weave: apply a
// theme to layouts, save one, snapshot a layout or promote its entries as one, list the themes,
// resolve one, say where each of a layout's settings came from, and hear the slots an office
// exposes. The theme file and its resolution are `theme_persist.hpp`.

#include "settings/slots.hpp"
#include "settings/theme_persist.hpp"

#include <zen/weave/shape.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace zengine::settings {

/// The words `ThemeApplyRequested::mode` takes: `merge` applies what the theme names and leaves
/// the rest; `reset` returns the rest to its default first, so the theme applies whole.
inline constexpr const char* kApplyMerge = "merge";
inline constexpr const char* kApplyReset = "reset";

/// APPLY THEME `theme` to the layouts whose ids are `layouts` (none named: the live one; or every
/// layout), only the settings carrying one of `only_tags` when any is named, as `mode` says
/// (`merge` when empty); `baseline` makes it the layouts' baseline too, which chooses the palette
/// their areas wear.
struct ThemeApplyRequested {
    std::string theme;
    std::vector<std::int64_t> layouts;
    bool all_layouts = false;
    std::vector<std::string> only_tags;
    std::string mode;
    bool baseline = false;

    ZEN_SHAPE(ThemeApplyRequested, 2, ZEN_FIELD(theme), ZEN_FIELD(layouts),
              ZEN_FIELD(all_layouts), ZEN_FIELD(only_tags), ZEN_FIELD(mode),
              ZEN_FIELD(baseline));
    friend bool operator==(const ThemeApplyRequested&, const ThemeApplyRequested&) = default;
};

/// One entry an apply left alone in one layout (0 for an entry of no layout: a colour, a key),
/// and why.
struct ThemeSkip {
    std::int64_t layout = -1;
    std::string target;
    std::string key;
    std::string why;

    ZEN_SHAPE(ThemeSkip, 1, ZEN_FIELD(layout), ZEN_FIELD(target), ZEN_FIELD(key), ZEN_FIELD(why));
    friend bool operator==(const ThemeSkip&, const ThemeSkip&) = default;
};

/// THE ANSWER TO `ThemeApplyRequested`: settings written and found already so; the entries nothing
/// took, skipped and said; the settings locks kept, the final layer; the keys set; the palette
/// offered and its revision; each office whose slot entries wait for it to declare; and whether the
/// baseline moved -- or the refusal, naming the entry that refused the theme.
struct ThemeActed {
    bool accepted = false;
    std::string refusal;
    std::string theme;
    std::int64_t written = 0;
    std::int64_t unchanged = 0;
    std::vector<ThemeSkip> skipped;
    std::vector<ThemeSkip> locked;
    std::int64_t keys = 0;
    std::string palette;
    std::int64_t revision = 0;
    std::vector<std::string> waiting;
    bool baseline = false;

    ZEN_SHAPE(ThemeActed, 2, ZEN_FIELD(accepted), ZEN_FIELD(refusal), ZEN_FIELD(theme),
              ZEN_FIELD(written), ZEN_FIELD(unchanged), ZEN_FIELD(skipped), ZEN_FIELD(locked),
              ZEN_FIELD(keys), ZEN_FIELD(palette), ZEN_FIELD(revision), ZEN_FIELD(waiting),
              ZEN_FIELD(baseline));
    friend bool operator==(const ThemeActed&, const ThemeActed&) = default;
};

/// SAVE `theme` as `themes/<name>.json`, over a theme of that name only with `replace`.
struct ThemeSaveRequested {
    ThemeFile theme;
    bool replace = false;

    ZEN_SHAPE(ThemeSaveRequested, 2, ZEN_FIELD(theme), ZEN_FIELD(replace));
    friend bool operator==(const ThemeSaveRequested&, const ThemeSaveRequested&) = default;
};

/// SAVE LAYOUT `layout`'s settings as theme `name`: flattened, or `composed` as its baseline
/// included and its own entries over it.
struct ThemeSnapshotRequested {
    std::int64_t layout = -1;
    std::string name;
    bool composed = false;

    ZEN_SHAPE(ThemeSnapshotRequested, 1, ZEN_FIELD(layout), ZEN_FIELD(name), ZEN_FIELD(composed));
    friend bool operator==(const ThemeSnapshotRequested&, const ThemeSnapshotRequested&) = default;
};

/// SAVE CHOSEN ENTRIES OF LAYOUT `layout` as theme `name`, and with `include_in_baseline` make the
/// layout's baseline include it.
struct ThemePromoteRequested {
    std::int64_t layout = -1;
    std::string name;
    std::vector<ThemeSetting> entries;
    bool include_in_baseline = false;

    ZEN_SHAPE(ThemePromoteRequested, 1, ZEN_FIELD(layout), ZEN_FIELD(name), ZEN_FIELD(entries),
              ZEN_FIELD(include_in_baseline));
    friend bool operator==(const ThemePromoteRequested&, const ThemePromoteRequested&) = default;
};

/// The answer to a save, a snapshot or a promotion: the theme's name and the file written, or the
/// refusal.
struct ThemeSaved {
    bool accepted = false;
    std::string refusal;
    std::string name;
    std::string path;

    ZEN_SHAPE(ThemeSaved, 1, ZEN_FIELD(accepted), ZEN_FIELD(refusal), ZEN_FIELD(name),
              ZEN_FIELD(path));
    friend bool operator==(const ThemeSaved&, const ThemeSaved&) = default;
};

/// Ask for every theme in the themes folder.
struct ThemesRequested {
    ZEN_SHAPE(ThemesRequested, 1);
    friend bool operator==(const ThemesRequested&, const ThemesRequested&) = default;
};

/// The answer to `ThemesRequested`: each file, by name, a broken one with its refusal.
struct ThemesShown {
    std::vector<ThemeSummary> themes;

    ZEN_SHAPE(ThemesShown, 2, ZEN_FIELD(themes));
    friend bool operator==(const ThemesShown&, const ThemesShown&) = default;
};

/// Resolve theme `theme` with its includes, applying nothing; answered by a `ThemeResolved`.
struct ThemeResolveRequested {
    std::string theme;

    ZEN_SHAPE(ThemeResolveRequested, 1, ZEN_FIELD(theme));
    friend bool operator==(const ThemeResolveRequested&, const ThemeResolveRequested&) = default;
};

/// Ask where each setting layout `layout` declares came from.
struct ThemeProvenanceRequested {
    std::int64_t layout = -1;

    ZEN_SHAPE(ThemeProvenanceRequested, 1, ZEN_FIELD(layout));
    friend bool operator==(const ThemeProvenanceRequested&,
                           const ThemeProvenanceRequested&) = default;
};

/// One setting of a layout: its value as a weaver reads it, the theme it came from or `loose`,
/// and whether it is locked.
struct ThemeProvenanceEntry {
    std::string target;
    std::string key;
    std::string value;
    std::string source;
    bool locked = false;

    ZEN_SHAPE(ThemeProvenanceEntry, 1, ZEN_FIELD(target), ZEN_FIELD(key), ZEN_FIELD(value),
              ZEN_FIELD(source), ZEN_FIELD(locked));
    friend bool operator==(const ThemeProvenanceEntry&, const ThemeProvenanceEntry&) = default;
};

/// THE ANSWER TO `ThemeProvenanceRequested`: the layout's baseline and whether it is current,
/// and every setting with its source -- or the refusal.
struct ThemeProvenanceShown {
    bool accepted = false;
    std::string refusal;
    std::int64_t layout = -1;
    std::string baseline;
    std::string status;
    std::vector<ThemeProvenanceEntry> entries;

    ZEN_SHAPE(ThemeProvenanceShown, 1, ZEN_FIELD(accepted), ZEN_FIELD(refusal),
              ZEN_FIELD(layout), ZEN_FIELD(baseline), ZEN_FIELD(status), ZEN_FIELD(entries));
    friend bool operator==(const ThemeProvenanceShown&, const ThemeProvenanceShown&) = default;
};

// ---- What the host tells the office, and what the office says later -------------------------

/// THE HOST -> the settings office, each time any of it changes and whenever another weave comes
/// to hold the office: the folder of the weaver's own themes ("" for none, as an isolated launch
/// keeps none), the folder of the themes shipped beside the host, the offices its load plan names,
/// and the theme the host's own areas wear where a layout names no baseline. A theme of the
/// weaver's wins over a shipped one of its name.
struct ThemesPlaced {
    std::string themes;
    std::string shipped;
    std::vector<std::string> planned;
    std::string worn;

    ZEN_SHAPE(ThemesPlaced, 1, ZEN_FIELD(themes), ZEN_FIELD(shipped), ZEN_FIELD(planned),
              ZEN_FIELD(worn));
    friend bool operator==(const ThemesPlaced&, const ThemesPlaced&) = default;
};

/// THE SETTINGS OFFICE -> the host, on each of its activations: say where the themes are, so an
/// office held after the host last said it hears it.
struct ThemesPlacedRequested {
    ZEN_SHAPE(ThemesPlacedRequested, 1);
    friend bool operator==(const ThemesPlacedRequested&, const ThemesPlacedRequested&) = default;
};

/// THE SETTINGS OFFICE -> any, once an office the plan named has declared its slots: the entries of
/// theme `theme` that waited for it and that its declaration does not hold, skipped, each with why.
struct ThemeSkipsSaid {
    std::string theme;
    std::string palette;
    std::vector<ThemeSkip> skipped;

    ZEN_SHAPE(ThemeSkipsSaid, 1, ZEN_FIELD(theme), ZEN_FIELD(palette), ZEN_FIELD(skipped));
    friend bool operator==(const ThemeSkipsSaid&, const ThemeSkipsSaid&) = default;
};

/// How many applies the office holds at once, waiting on the layouts and the keymap.
inline constexpr std::size_t kMaxAppliesInFlight = 4;

} // namespace zengine::settings

#endif // ZENGINE_SETTINGS_VOCABULARY_HPP
