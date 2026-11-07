// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_SETTINGS_APPLY_HPP
#define ZENGINE_SETTINGS_APPLY_HPP

// HOW A THEME APPLIES, as values: every entry of a resolved theme judged against what takes it --
// a setting against the declarations the layouts' owner shows, a key against the actions the
// keymap holds, a colour, font or size against the pool, a slot against its office's declaration
// -- and the batch, the keymap's edits and the palette that judging gives. The settings weave
// (`weave.cpp`) asks the layouts' owner and the keymap for what it judges against and sends what
// this plans; nothing here sends a message.

#include "settings/theme_persist.hpp"
#include "settings/vocabulary.hpp"

#include "graphics/themes.hpp"
#include "graphics/vocabulary.hpp"
#include "workshop/desktop_seam_vocabulary.hpp"
#include "workshop/settings_seam_vocabulary.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace zengine::settings {

/// Why an entry was skipped, as `ThemeSkip::why` says it.
inline constexpr const char* kNothingTakes = "nothing takes it";
inline constexpr const char* kNotInLayout = "not in this layout";
inline constexpr const char* kNoSuchSlot = "its office declares no such slot";

/// WHAT A THEME IS JUDGED AGAINST: the layouts' rows and every declaration the layouts' owner
/// shows, the keymap in force, the pool's names, each office's declared slots and the offices the
/// load plan names.
struct ApplyWorld {
    workshop::LayoutSettingsAnswered layouts;
    workshop::KeymapShown keymap;
    std::vector<std::string> pool_colors;
    std::vector<std::string> pool_fonts;
    std::vector<std::string> pool_sizes;
    std::map<std::string, SlotsDeclared> declared;
    std::vector<std::string> planned;
};

/// ONE EDIT OF THE KEYMAP a theme asks: the action, the edit (`workshop::keymap_edit`) and the
/// spelled gesture it carries.
struct KeyEdit {
    std::string action;
    std::int64_t op = 0;
    std::string text;
    friend bool operator==(const KeyEdit&, const KeyEdit&) = default;
};

/// WHAT APPLYING A THEME COMES TO, judged before anything is written: the refusal, naming the entry
/// that refused it; or the layouts it applies to, the batch of settings and baselines, the keymap's
/// edits, the entries skipped, and its look -- the colours, fonts, sizes and outline for the pool,
/// each declared office's slots, and each planned office's slot entries, waiting.
struct ApplyPlan {
    bool accepted = false;
    std::string refusal;
    std::vector<std::int64_t> layouts;
    std::vector<workshop::SettingWrite> writes;
    std::vector<workshop::LayoutThemeWrite> baselines;
    std::vector<KeyEdit> keys;
    std::vector<ThemeSkip> skipped;
    std::string outline;
    std::vector<ThemeColor> colors;
    std::vector<ThemeFont> fonts;
    std::vector<ThemeSize> sizes;
    std::vector<ThemeSlots> slots;
    std::vector<ThemeSlots> waiting;
};

namespace detail {

inline bool holds(const std::vector<std::string>& names, std::string_view name) {
    return std::find(names.begin(), names.end(), name) != names.end();
}

/// The declaration of `office`'s pane `pane` ("" an office-wide one), or none.
inline const workshop::DeclaredSettings* declaration(const workshop::LayoutSettingsAnswered& l,
                                                     std::string_view office,
                                                     std::string_view pane) {
    for (const workshop::DeclaredSettings& d : l.declared) {
        if (d.provider == office && d.pane == pane) {
            return &d;
        }
    }
    return nullptr;
}

inline const workshop::v2::PaneSettingRow* row_of(const workshop::DeclaredSettings* d,
                                                  std::string_view key) {
    if (d == nullptr) {
        return nullptr;
    }
    for (const workshop::v2::PaneSettingRow& r : d->rows) {
        if (r.key == key) {
            return &r;
        }
    }
    return nullptr;
}

inline bool tagged(const workshop::v2::PaneSettingRow& row, const std::vector<std::string>& only) {
    if (only.empty()) {
        return true;
    }
    for (const std::string& t : row.tags) {
        if (holds(only, t)) {
            return true;
        }
    }
    return false;
}

inline workshop::SettingWrite write_of(std::int64_t layout, std::string target,
                                       const ThemeSetting& s) {
    workshop::SettingWrite w;
    w.layout = layout;
    w.target = std::move(target);
    w.key = s.key;
    w.flag = s.flag;
    w.number = s.number;
    w.text = s.text;
    w.clear = s.clear;
    return w;
}

/// A setting a declared row is handed: refused for the wrong kind always, and for a value the row
/// does not take as nothing taking it. Empty when the row takes it.
struct Taken {
    bool wrong_kind = false;
    std::string why;
};

inline Taken taken_by(const workshop::v2::PaneSettingRow& row, const ThemeSetting& s) {
    if (s.clear) {
        return {};
    }
    const workshop::SettingKind want = workshop::setting_kind(row);
    const workshop::SettingKind have = workshop::setting_kind(s);
    if (want != have) {
        return Taken{true, "`" + s.key + "` takes " + workshop::setting_kind_word(want) +
                               ", not " + workshop::setting_kind_word(have)};
    }
    std::string why = workshop::v2::pane_setting_refused_by(row, s);
    return Taken{false, std::move(why)};
}

/// The ids of every action the keymap holds in force or the weaver's file moves.
inline std::set<std::string> action_ids(const workshop::KeymapShown& k) {
    std::set<std::string> ids;
    for (const workshop::ShownBinding& b : k.rows) {
        if (!b.id.empty()) ids.insert(b.id);
    }
    for (const workshop::ShownBinding& b : k.moved) {
        if (!b.id.empty()) ids.insert(b.id);
    }
    return ids;
}

} // namespace detail

/// JUDGE THEME `theme`, applied as `asked` asks, against `world` (WL-THEME-01, WL-THEME-02): an entry nothing
/// takes is skipped and said where `on_failure` is `skip`, and refuses the theme where it is
/// `refuse`; a value of the wrong kind refuses it always; a planned office's slots wait. `merge`
/// writes what it names; `reset` also clears every other key of each pane it names, and resets
/// every key the keymap's file moves that it does not name.
inline ApplyPlan plan_apply(const ThemeResolved& theme, const ThemeApplyRequested& asked,
                            const ApplyWorld& world) {
    ApplyPlan plan;
    const bool skip = theme.on_failure == kOnFailureSkip;
    const bool reset = asked.mode == kApplyReset;
    const std::string named = "theme `" + theme.theme + "`";
    std::optional<std::string> refused;
    const auto miss = [&](std::int64_t layout, std::string target, std::string key,
                          std::string why) {
        if (!skip && !refused) {
            refused = named + ": `" + key + "` of `" + target + "` -- " + why +
                      ", and the theme refuses on a failure";
        }
        plan.skipped.push_back(ThemeSkip{layout, std::move(target), std::move(key), std::move(why)});
    };

    // THE LAYOUTS IT APPLIES TO, by id: those named, every one, or the live one.
    const workshop::LayoutSettingsAnswered& ls = world.layouts;
    std::vector<const workshop::LayoutSettings*> targets;
    for (const workshop::LayoutSettings& l : ls.layouts) {
        const bool named_one = std::find(asked.layouts.begin(), asked.layouts.end(), l.id) !=
                               asked.layouts.end();
        if (asked.all_layouts || named_one || (asked.layouts.empty() && l.position == ls.active)) {
            targets.push_back(&l);
        }
    }
    for (const std::int64_t id : asked.layouts) {
        const bool found = std::any_of(ls.layouts.begin(), ls.layouts.end(),
                                       [&](const workshop::LayoutSettings& l) { return l.id == id; });
        if (!found) {
            plan.refusal = named + ": no layout has the id " + std::to_string(id);
            return plan;
        }
    }
    for (const workshop::LayoutSettings* l : targets) {
        plan.layouts.push_back(l->id);
    }

    // THE SETTINGS, each through the declaration that takes it.
    std::set<std::pair<std::int64_t, std::string>> touched; // (layout, office/pane) the theme names
    for (const ResolvedThemeSetting& r : theme.settings) {
        const ThemeSetting& s = r.setting;
        const std::size_t slash = s.target.find('/');
        if (s.target != "*" && slash == std::string::npos) {
            // An office-wide setting: one write, of no layout.
            const workshop::v2::PaneSettingRow* row =
                detail::row_of(detail::declaration(ls, s.target, ""), s.key);
            if (row == nullptr) {
                miss(0, s.target, s.key, kNothingTakes);
                continue;
            }
            if (!detail::tagged(*row, asked.only_tags)) {
                continue;
            }
            const detail::Taken t = detail::taken_by(*row, s);
            if (t.wrong_kind) {
                plan.refusal = named + ": `" + s.key + "` of `" + s.target + "` -- " + t.why;
                return plan;
            }
            if (!t.why.empty()) {
                miss(0, s.target, s.key, t.why);
                continue;
            }
            plan.writes.push_back(detail::write_of(0, s.target, s));
            continue;
        }
        bool any = false;
        for (const workshop::LayoutSettings* l : targets) {
            bool in_layout = false;
            for (const workshop::LayoutPaneSettings& p : l->panes) {
                const std::string where = p.provider + "/" + p.pane;
                if (s.target != "*" && s.target != where) {
                    continue;
                }
                in_layout = true;
                const workshop::v2::PaneSettingRow* row =
                    detail::row_of(detail::declaration(ls, p.provider, p.pane), s.key);
                if (row == nullptr) {
                    if (s.target != "*") {
                        miss(l->id, where, s.key, kNothingTakes);
                    }
                    continue;
                }
                if (!detail::tagged(*row, asked.only_tags)) {
                    continue;
                }
                const detail::Taken t = detail::taken_by(*row, s);
                if (t.wrong_kind) {
                    plan.refusal = named + ": `" + s.key + "` of `" + where + "` -- " + t.why;
                    return plan;
                }
                if (!t.why.empty()) {
                    miss(l->id, where, s.key, t.why);
                    continue;
                }
                any = true;
                touched.insert({l->id, where});
                plan.writes.push_back(detail::write_of(l->id, where, s));
            }
            if (!in_layout && s.target != "*") {
                plan.skipped.push_back(ThemeSkip{l->id, s.target, s.key, kNotInLayout});
            }
        }
        if (s.target == "*" && !any) {
            miss(0, s.target, s.key, kNothingTakes);
        }
    }
    if (reset) {
        // EVERY OTHER KEY OF EACH PANE THE THEME NAMES, returned to its default.
        for (const workshop::LayoutSettings* l : targets) {
            for (const workshop::LayoutPaneSettings& p : l->panes) {
                const std::string where = p.provider + "/" + p.pane;
                if (touched.count({l->id, where}) == 0) {
                    continue;
                }
                for (const workshop::PaneSetting& kept : p.settings) {
                    const bool named_key = std::any_of(
                        plan.writes.begin(), plan.writes.end(), [&](const workshop::SettingWrite& w) {
                            return w.layout == l->id && w.target == where && w.key == kept.key;
                        });
                    if (!named_key) {
                        ThemeSetting clear;
                        clear.key = kept.key;
                        clear.clear = true;
                        plan.writes.push_back(detail::write_of(l->id, where, clear));
                    }
                }
            }
        }
    }
    if (asked.baseline) {
        for (const workshop::LayoutSettings* l : targets) {
            plan.baselines.push_back(
                workshop::LayoutThemeWrite{l->id, workshop::LayoutTheme{theme.theme,
                                                                        theme.fingerprint}});
        }
    }

    // THE KEYS, each an action the keymap holds.
    const std::set<std::string> ids = detail::action_ids(world.keymap);
    std::set<std::string> keyed;
    for (const ResolvedThemeKey& rk : theme.keys) {
        const ThemeKey& k = rk.key;
        if (ids.count(k.action) == 0) {
            miss(0, "keys", k.action, kNothingTakes);
            continue;
        }
        keyed.insert(k.action);
        const input::ParsedGesture first = input::parse_gesture(k.gestures.front());
        if (k.gestures.size() == 1 && !input::is_bound(first.gesture)) {
            plan.keys.push_back(KeyEdit{k.action, workshop::keymap_edit::kDisable, {}});
            continue;
        }
        for (std::size_t i = 0; i < k.gestures.size(); ++i) {
            plan.keys.push_back(KeyEdit{k.action,
                                        i == 0 ? workshop::keymap_edit::kSetSpelled
                                               : workshop::keymap_edit::kAddSpelled,
                                        k.gestures[i]});
        }
    }
    if (reset) {
        std::set<std::string> moved;
        for (const workshop::ShownBinding& b : world.keymap.rows) {
            if (b.authored && !b.id.empty()) moved.insert(b.id);
        }
        for (const workshop::ShownBinding& b : world.keymap.moved) {
            if (!b.id.empty()) moved.insert(b.id);
        }
        for (const std::string& id : moved) {
            if (keyed.count(id) == 0) {
                plan.keys.push_back(KeyEdit{id, workshop::keymap_edit::kReset, {}});
            }
        }
    }

    // ITS LOOK: the pool's names, and each office's slots.
    plan.outline = theme.outline;
    for (const ThemeColor& c : theme.colors) {
        if (detail::holds(world.pool_colors, c.name)) {
            plan.colors.push_back(c);
        } else {
            miss(0, "colors", c.name, kNothingTakes);
        }
    }
    for (const ThemeFont& f : theme.fonts) {
        if (detail::holds(world.pool_fonts, f.name)) {
            plan.fonts.push_back(f);
        } else {
            miss(0, "fonts", f.name, kNothingTakes);
        }
    }
    for (const ThemeSize& z : theme.sizes) {
        if (detail::holds(world.pool_sizes, z.name)) {
            plan.sizes.push_back(z);
        } else {
            miss(0, "sizes", z.name, kNothingTakes);
        }
    }
    for (const ThemeSlots& s : theme.slots) {
        const auto declared = world.declared.find(s.office);
        if (declared == world.declared.end()) {
            if (detail::holds(world.planned, s.office)) {
                plan.waiting.push_back(s);
            } else {
                for (const ThemeColor& c : s.colors) miss(0, s.office, c.name, kNothingTakes);
                for (const ThemeSize& z : s.sizes) miss(0, s.office, z.name, kNothingTakes);
            }
            continue;
        }
        ThemeSlots ready{s.office, {}, {}};
        for (const ThemeColor& c : s.colors) {
            const bool exposed =
                std::any_of(declared->second.colors.begin(), declared->second.colors.end(),
                            [&](const SlotColorDeclared& d) { return d.name == c.name; });
            if (exposed) {
                ready.colors.push_back(c);
            } else {
                miss(0, s.office, c.name, kNoSuchSlot);
            }
        }
        for (const ThemeSize& z : s.sizes) {
            const bool exposed =
                std::any_of(declared->second.sizes.begin(), declared->second.sizes.end(),
                            [&](const SlotSizeDeclared& d) { return d.name == z.name; });
            if (exposed) {
                ready.sizes.push_back(z);
            } else {
                miss(0, s.office, z.name, kNoSuchSlot);
            }
        }
        plan.slots.push_back(std::move(ready));
    }
    if (refused) {
        plan.refusal = *refused;
        return plan;
    }
    plan.accepted = true;
    return plan;
}

/// THE ENTRIES OF `waiting` THAT `declared` HOLDS, and the rest skipped, each with why: what a
/// planned office's slots come to once it declares.
inline std::pair<ThemeSlots, std::vector<ThemeSkip>> judge_waiting(const ThemeSlots& waiting,
                                                                  const SlotsDeclared& declared) {
    ThemeSlots ready{waiting.office, {}, {}};
    std::vector<ThemeSkip> skipped;
    for (const ThemeColor& c : waiting.colors) {
        if (std::any_of(declared.colors.begin(), declared.colors.end(),
                        [&](const SlotColorDeclared& d) { return d.name == c.name; })) {
            ready.colors.push_back(c);
        } else {
            skipped.push_back(ThemeSkip{0, waiting.office, c.name, kNoSuchSlot});
        }
    }
    for (const ThemeSize& z : waiting.sizes) {
        if (std::any_of(declared.sizes.begin(), declared.sizes.end(),
                        [&](const SlotSizeDeclared& d) { return d.name == z.name; })) {
            ready.sizes.push_back(z);
        } else {
            skipped.push_back(ThemeSkip{0, waiting.office, z.name, kNoSuchSlot});
        }
    }
    return {std::move(ready), std::move(skipped)};
}

// ---- The palette --------------------------------------------------------------------------

/// The pool's names, as the theme reader judges a theme's colours, fonts and sizes against them.
inline std::vector<std::string> pool_color_names() {
    std::vector<std::string> out;
    for (const graphics::PaletteColor& c : graphics::themes::built_in().colors) {
        out.push_back(c.name);
    }
    return out;
}
inline std::vector<std::string> pool_font_names() {
    std::vector<std::string> out;
    for (const graphics::PaletteFont& f : graphics::themes::built_in().fonts) {
        out.push_back(f.name);
    }
    return out;
}
inline std::vector<std::string> pool_size_names() {
    std::vector<std::string> out;
    for (const graphics::PaletteSize& s : graphics::themes::built_in().sizes) {
        out.push_back(s.name);
    }
    return out;
}

namespace detail {

inline graphics::PaletteColor* palette_color(std::vector<graphics::PaletteColor>& colors,
                                             std::string_view name) {
    for (graphics::PaletteColor& c : colors) {
        if (c.name == name) {
            return &c;
        }
    }
    return nullptr;
}

/// `c` over `into`: each part it names replaces that part.
inline void color_over(graphics::PaletteColor& into, const ThemeColor& c) {
    if (const std::optional<std::int64_t> rgba = rgba_of(c.color)) into.rgba = *rgba;
    if (!c.cells.empty()) into.cells = c.cells;
    if (!c.fill.empty()) into.fill = c.fill;
}

} // namespace detail

/// THE POOL'S OWN PALETTE under `name`: every colour at its default, the default outline.
inline graphics::PaletteOffered default_palette(std::string name) {
    graphics::PaletteOffered p = graphics::themes::built_in();
    p.palette = std::move(name);
    return p;
}

/// THE PALETTE `base` WITH A THEME'S LOOK OVER IT: each colour, font and size it gives over the
/// pool's, its outline where it gives one, and each office's slots, a slot colour it sets in part
/// painting the rest as its office declared it -- its default, and what a terminal shows of the
/// pool colour it is `like`.
inline graphics::PaletteOffered look_over(graphics::PaletteOffered base, const std::string& outline,
                                          const std::vector<ThemeColor>& colors,
                                          const std::vector<ThemeSlots>& slots,
                                          const std::map<std::string, SlotsDeclared>& declared,
                                          const std::vector<ThemeFont>& fonts = {},
                                          const std::vector<ThemeSize>& sizes = {}) {
    if (!outline.empty()) {
        base.outline = outline;
    }
    for (const ThemeFont& f : fonts) {
        for (graphics::PaletteFont& into : base.fonts) {
            if (into.name == f.name) into.face = f.face;
        }
    }
    for (const ThemeSize& z : sizes) {
        for (graphics::PaletteSize& into : base.sizes) {
            if (into.name == z.name) into.points = z.points;
        }
    }
    for (const ThemeColor& c : colors) {
        if (graphics::PaletteColor* into = detail::palette_color(base.colors, c.name)) {
            detail::color_over(*into, c);
        }
    }
    for (const ThemeSlots& s : slots) {
        auto office = std::find_if(base.slots.begin(), base.slots.end(),
                                   [&](const graphics::PaletteSlots& k) { return k.office == s.office; });
        if (office == base.slots.end()) {
            base.slots.push_back(graphics::PaletteSlots{s.office, {}, {}});
            office = base.slots.end() - 1;
        }
        const auto d = declared.find(s.office);
        for (const ThemeColor& c : s.colors) {
            graphics::PaletteColor* into = detail::palette_color(office->colors, c.name);
            if (into == nullptr) {
                graphics::PaletteColor fresh{c.name, 0, {}, {}};
                if (d != declared.end()) {
                    for (const SlotColorDeclared& sd : d->second.colors) {
                        if (sd.name != c.name) continue;
                        fresh.rgba = sd.rgba;
                        if (const graphics::PaletteColor* like =
                                detail::palette_color(base.colors, sd.like)) {
                            fresh.cells = like->cells;
                            fresh.fill = like->fill;
                        }
                    }
                }
                office->colors.push_back(std::move(fresh));
                into = &office->colors.back();
            }
            detail::color_over(*into, c);
        }
        for (const ThemeSize& z : s.sizes) {
            const auto at = std::find_if(office->sizes.begin(), office->sizes.end(),
                                         [&](const graphics::PaletteSize& k) { return k.name == z.name; });
            if (at == office->sizes.end()) {
                office->sizes.push_back(graphics::PaletteSize{z.name, z.points});
            } else {
                at->points = z.points;
            }
        }
    }
    return base;
}

} // namespace zengine::settings

#endif // ZENGINE_SETTINGS_APPLY_HPP
