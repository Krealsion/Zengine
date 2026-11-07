// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_SETTINGS_SLOTS_HPP
#define ZENGINE_SETTINGS_SLOTS_HPP

// THE SLOTS AN OFFICE EXPOSES TO THEMES, as it says them to the settings office: the words a pane
// with slots of its own speaks, apart from the rest of the office's vocabulary.

#include <zen/weave/shape.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace zengine::settings {

/// The office every door here is addressed to.
inline constexpr const char* kSettingsRole = "zengine.settings";

// ---- The slots an office exposes -----------------------------------------------------------

/// ONE COLOUR AN OFFICE EXPOSES: its name, the colour it paints in where no theme sets it, and the
/// pool colour a terminal shows it as.
struct SlotColorDeclared {
    std::string name;
    std::int64_t rgba = 0;
    std::string like;

    ZEN_SHAPE(SlotColorDeclared, 1, ZEN_FIELD(name), ZEN_FIELD(rgba), ZEN_FIELD(like));
    friend bool operator==(const SlotColorDeclared&, const SlotColorDeclared&) = default;
};

/// ONE TEXT SIZE AN OFFICE EXPOSES: its name and the points it stands at where no theme sets it.
struct SlotSizeDeclared {
    std::string name;
    std::int64_t points = 0;

    ZEN_SHAPE(SlotSizeDeclared, 1, ZEN_FIELD(name), ZEN_FIELD(points));
    friend bool operator==(const SlotSizeDeclared&, const SlotSizeDeclared&) = default;
};

/// An office -> the settings office, as that office, on each activation: the slots it exposes to
/// themes, whole. Counts while the incarnation that said it holds the office; a theme's entries for
/// an office the load plan names wait until it says this, and then apply.
struct SlotsDeclared {
    std::vector<SlotColorDeclared> colors;
    std::vector<SlotSizeDeclared> sizes;

    ZEN_SHAPE(SlotsDeclared, 1, ZEN_FIELD(colors), ZEN_FIELD(sizes));
    friend bool operator==(const SlotsDeclared&, const SlotsDeclared&) = default;
};

/// What one office may expose, and how many offices' slots the settings office keeps.
inline constexpr std::size_t kMaxDeclaredSlots = 64;
inline constexpr std::size_t kMaxSlotOffices = 64;

/// THE SETTINGS OFFICE -> any, on each of its activations: say your slots again, so an office that
/// declared before this office was held is heard.
struct SlotsAsked {
    ZEN_SHAPE(SlotsAsked, 1);
    friend bool operator==(const SlotsAsked&, const SlotsAsked&) = default;
};

} // namespace zengine::settings

#endif // ZENGINE_SETTINGS_SLOTS_HPP
