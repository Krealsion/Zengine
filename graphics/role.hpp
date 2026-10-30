// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_GRAPHICS_ROLE_HPP
#define ZENGINE_GRAPHICS_ROLE_HPP

// The theme's roles: what a colour means, never the colour. Each medium picks its own ink for a
// role, so one drawing reads in a monochrome terminal and in a window of light alike. A header of
// its own, with nothing but the numbers, so a medium's lowest layer (a terminal's ink table,
// graphics/window/terminal_io.hpp) names a role without the vocabulary's shapes.
// Reference: graphics/docs/graphics.md.

#include <cstdint>

namespace zengine::graphics {

/// A role a medium does not know paints as `kFill` rather than vanishing.
namespace role {
inline constexpr std::int64_t kFill = 0;   ///< ordinary authored material
inline constexpr std::int64_t kAccent = 1; ///< the one thing being pointed at
inline constexpr std::int64_t kMuted = 2;  ///< present, deliberately quiet
inline constexpr std::int64_t kAlert = 3;  ///< something the weaver must see
inline constexpr std::int64_t kGround = 4; ///< opaque, empty material beneath content
/// The medium's own ground: opaque, empty, and what the medium shows where nothing is drawn -- a
/// window's own background, a terminal's default one, light or dark. A ground and never an ink:
/// a rectangle or a row's background names it, and text drawn in it paints as `kFill`.
inline constexpr std::int64_t kMediumGround = 5;
/// Drawn at a guest's ask, never Workshop's own voice: a guide mark a guest set on a control for
/// the weaver to see. Every guest-caused mark shares it, so the weaver tells it from the desk's
/// own emphasis by role, in every medium.
inline constexpr std::int64_t kGuide = 6;
/// A box's outline: a quiet line that shows where a box a weaver types in stands, light grey in a
/// look of light. Drawn only by a medium that sets type; a medium of characters spells the box.
inline constexpr std::int64_t kEdge = 7;
/// No role at all: the absence of a background, not an ink role. Negative, so it cannot collide
/// with a role a later vocabulary adds (which an older medium would silently paint as `kFill`).
/// Never pass it to a medium's role-to-ink table; test for it first.
inline constexpr std::int64_t kNone = -1;
} // namespace role

} // namespace zengine::graphics

#endif // ZENGINE_GRAPHICS_ROLE_HPP
