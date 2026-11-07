// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_GRAPHICS_TEXT_STYLE_HPP
#define ZENGINE_GRAPHICS_TEXT_STYLE_HPP

// What text a draw carries is set in beside its colour: a size, a font and a weight. A size and a
// font are names from the shared pool, or a size the drawing's pane exposes as a slot; the window
// turns a name into points and a face when it paints, from the palette the area wears, and offers
// each size's measure in the room it tells an owner. The names are C++ names generated from JSON
// with the pool's colours (cmake/ZengineNames.cmake): `zengine::graphics::sizes` and
// `zengine::graphics::fonts`. Reference: graphics/docs/graphics.md, "Text: sizes, fonts, weight".

#include <zen/weave/shape.hpp>

#include <cstdint>
#include <string>
#include <string_view>

namespace zengine::graphics {

/// How heavy a text's strokes are.
namespace text_weight {
inline constexpr std::int64_t kRegular = 0;
inline constexpr std::int64_t kBold = 1;
} // namespace text_weight

/// Where a text size comes from.
namespace size_from {
inline constexpr std::int64_t kPool = 2; ///< the shared pool's `name`, as the area's palette says
/// The slot `name` the area's owner exposes, as the area's palette says for that owner; `points`
/// where the palette sets none.
inline constexpr std::int64_t kSlot = 3;
} // namespace size_from

/// The pool's size a text carries where it names none.
inline constexpr const char* kBodySize = "body";
/// The pool's font a text carries where it names none.
inline constexpr const char* kTextFont = "text";

/// A TEXT SIZE A DRAW CARRIES: a pool size or a slot, by name ("" the pool's `body`), with a slot's
/// points where no palette sets it.
struct TextSize {
    std::int64_t from = size_from::kPool;
    std::string name;
    std::int64_t points = 0;
    ZEN_SHAPE(TextSize, 1, ZEN_FIELD(from), ZEN_FIELD(name), ZEN_FIELD(points));

    /// The pool size's name this one reads as: its own, or `body` where it names none.
    std::string pool_name() const { return name.empty() ? std::string(kBodySize) : name; }
    friend bool operator==(const TextSize&, const TextSize&) = default;
};

/// A SIZE FROM THE SHARED POOL, as the pool's generated header declares it
/// (`zengine::graphics::sizes::heading`): the name and nothing else.
struct PoolSize {
    const char* name;
    operator TextSize() const { return TextSize{size_from::kPool, name, 0}; }
};

/// A SIZE A PANE EXPOSES, as its own generated header declares it: its name and the points it
/// sets text at where no theme sets it.
struct SlotSize {
    const char* name;
    std::int64_t points;
    operator TextSize() const { return TextSize{size_from::kSlot, name, points}; }
};

/// A FONT FROM THE SHARED POOL, as the pool's generated header declares it
/// (`zengine::graphics::fonts::text`): the name and nothing else.
struct PoolFont {
    const char* name;
    operator std::string() const { return name; }
};

} // namespace zengine::graphics

#endif // ZENGINE_GRAPHICS_TEXT_STYLE_HPP
