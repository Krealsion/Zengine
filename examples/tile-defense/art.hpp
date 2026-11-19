// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_EXAMPLES_TILE_DEFENSE_ART_HPP
#define ZENGINE_EXAMPLES_TILE_DEFENSE_ART_HPP

// The tile defense's sheet, made in its own code when it starts: the map's tiles, the two towers
// and their animations, the walkers walking, the health bars, the arrow in its eight directions,
// the fireball in flight, its burst, and the cursor -- each frame a 16-pixel square of pixels for
// a window that shows pixels, and a character in its colours for a window of characters. Nothing
// is read from a file. Reference: examples/tile-defense/README.md.

#include "graphics/sheet.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <utility>

namespace zengine::tile_defense {

namespace gfx = zengine::graphics;

/// The frames the game draws with, by name.
struct Art {
    gfx::SheetArgs sheet;
    std::array<gfx::FrameId, 3> grass{};
    gfx::FrameId road, tree, rock, castle;
    std::array<gfx::FrameId, 4> arrow_tower{}, fire_tower{};
    std::array<gfx::FrameId, 4> runner{}, brute{};
    gfx::FrameId bar_back, bar_front;
    std::array<gfx::FrameId, 8> arrow{}; ///< east first, then clockwise: south-east, south, ...
    std::array<gfx::FrameId, 4> fireball{};
    std::array<gfx::FrameId, 3> burst{};
    gfx::FrameId cursor;
};

/// The side of a frame, in the sheet's pixels.
inline constexpr std::int64_t kFramePx = 16;

namespace art {

/// The colours a window of characters shows the frames in: the sixteen a terminal knows by heart.
inline gfx::Color term(std::uint32_t rgb) { return gfx::rgba((rgb << 8) | 0xffu); }
inline const gfx::Color kGreen = term(0x00cd00), kBrightGreen = term(0x00ff00);
inline const gfx::Color kYellow = term(0xcdcd00), kBrightYellow = term(0xffff00);
inline const gfx::Color kRed = term(0xcd0000), kBrightRed = term(0xff0000);
inline const gfx::Color kBlue = term(0x0000ee), kMagenta = term(0xff00ff);
inline const gfx::Color kWhite = term(0xe5e5e5), kBrightWhite = term(0xffffff);
inline const gfx::Color kGrey = term(0x7f7f7f), kBlack = term(0x000000);

using gfx::Rgba;
using gfx::rgba_of;

/// A disc of radius `r` about (at_x, at_y) of the box `b`, in pixels of the sheet.
inline void disc(gfx::Sheet& s, const gfx::Box& b, double at_x, double at_y, double r, Rgba c) {
    const double cx = static_cast<double>(b.x) + at_x, cy = static_cast<double>(b.y) + at_y;
    for (auto y = static_cast<std::int64_t>(cy - r - 1); y <= static_cast<std::int64_t>(cy + r + 1); ++y) {
        for (auto x = static_cast<std::int64_t>(cx - r - 1); x <= static_cast<std::int64_t>(cx + r + 1); ++x) {
            const double dx = static_cast<double>(x) + 0.5 - cx, dy = static_cast<double>(y) + 0.5 - cy;
            if (dx * dx + dy * dy <= r * r) {
                s.put(x, y, c);
            }
        }
    }
}

/// A line from (x0, y0) to (x1, y1), a pixel wide.
inline void line(gfx::Sheet& s, std::int64_t x0, std::int64_t y0, std::int64_t x1, std::int64_t y1,
                 Rgba c) {
    const std::int64_t dx = std::abs(x1 - x0), dy = -std::abs(y1 - y0);
    const std::int64_t sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
    std::int64_t err = dx + dy;
    for (int guard = 0; guard < 64; ++guard) {
        s.put(x0, y0, c);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        const std::int64_t e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

/// A pixel's shade moved by a number of the pixel's own (its place hashed), for texture.
inline Rgba speckle(Rgba c, std::int64_t x, std::int64_t y, std::int64_t seed, int amount) {
    const auto h = static_cast<std::uint32_t>((x * 73856093) ^ (y * 19349663) ^ (seed * 83492791));
    const int d = static_cast<int>(h % static_cast<std::uint32_t>(2 * amount + 1)) - amount;
    const auto move = [d](std::uint8_t v) {
        return static_cast<std::uint8_t>(std::clamp(static_cast<int>(v) + d, 0, 255));
    };
    return Rgba{move(c.r), move(c.g), move(c.b), c.a};
}

inline void grass(gfx::Sheet& s, const gfx::Box& b, std::int64_t seed) {
    const Rgba base = rgba_of(0x3f8f3aff);
    for (std::int64_t y = 0; y < b.h; ++y) {
        for (std::int64_t x = 0; x < b.w; ++x) {
            s.put(b.x + x, b.y + y, speckle(base, x, y, seed, 14));
        }
    }
    for (std::int64_t i = 0; i < 3 + seed; ++i) { // blades
        const std::int64_t x = (seed * 5 + i * 7) % 15, y = (seed * 3 + i * 11) % 13 + 2;
        s.put(b.x + x, b.y + y, rgba_of(0x7fd067ff));
        s.put(b.x + x, b.y + y - 1, rgba_of(0x6cc257ff));
    }
}

inline void road(gfx::Sheet& s, const gfx::Box& b) {
    const Rgba base = rgba_of(0xb99a63ff);
    for (std::int64_t y = 0; y < b.h; ++y) {
        for (std::int64_t x = 0; x < b.w; ++x) {
            s.put(b.x + x, b.y + y, speckle(base, x, y, 9, 12));
        }
    }
    s.put(b.x + 4, b.y + 5, rgba_of(0x8f7448ff));
    s.put(b.x + 11, b.y + 10, rgba_of(0x8f7448ff));
    s.put(b.x + 8, b.y + 13, rgba_of(0xd8be8aff));
}

inline void tree(gfx::Sheet& s, const gfx::Box& b) {
    grass(s, b, 2);
    s.fill(gfx::Box{b.x + 7, b.y + 10, 2, 5}, rgba_of(0x6b4423ff));
    disc(s, b, 8.0, 7.0, 5.5, rgba_of(0x1f5f2aff));
    disc(s, b, 7.0, 6.0, 3.5, rgba_of(0x2e7d35ff));
    s.put(b.x + 6, b.y + 4, rgba_of(0x58a84fff));
}

inline void rock(gfx::Sheet& s, const gfx::Box& b) {
    grass(s, b, 4);
    disc(s, b, 8.0, 9.5, 4.5, rgba_of(0x6f6f73ff));
    disc(s, b, 7.0, 8.5, 3.0, rgba_of(0x9a9aa0ff));
}

inline void castle(gfx::Sheet& s, const gfx::Box& b) {
    road(s, b);
    const Rgba stone = rgba_of(0x9aa0b4ff), dark = rgba_of(0x5b6072ff);
    s.fill(gfx::Box{b.x + 2, b.y + 4, 12, 11}, stone);
    for (std::int64_t i = 0; i < 4; ++i) {
        s.fill(gfx::Box{b.x + 2 + i * 3, b.y + 2, 2, 2}, stone);
    }
    s.fill(gfx::Box{b.x + 6, b.y + 9, 4, 6}, dark);
    s.fill(gfx::Box{b.x + 7, b.y, 1, 4}, dark);
    s.fill(gfx::Box{b.x + 8, b.y, 4, 2}, rgba_of(0x3b5bd9ff));
}

/// An arrow tower, its bow drawn back by `pull` (0 at rest to 2 drawn), or just loosed (3).
inline void arrow_tower(gfx::Sheet& s, const gfx::Box& b, int pull) {
    const Rgba wood = rgba_of(0x8a5a2bff), dark = rgba_of(0x5a3818ff);
    s.fill(gfx::Box{b.x + 3, b.y + 8, 10, 7}, wood);
    s.fill(gfx::Box{b.x + 3, b.y + 14, 10, 1}, dark);
    s.fill(gfx::Box{b.x + 5, b.y + 5, 6, 4}, dark);
    const Rgba bow = pull == 3 ? rgba_of(0xfff2a8ff) : rgba_of(0xe8c26aff);
    const std::int64_t back = pull == 3 ? 0 : pull;
    line(s, b.x + 4, b.y + 1, b.x + 4 + 2, b.y + 5, bow);
    line(s, b.x + 12, b.y + 1, b.x + 12 - 2, b.y + 5, bow);
    line(s, b.x + 4, b.y + 1, b.x + 8, b.y + 3 + back, rgba_of(0xddddddff));
    line(s, b.x + 12, b.y + 1, b.x + 8, b.y + 3 + back, rgba_of(0xddddddff));
    if (pull != 3) {
        line(s, b.x + 8, b.y + 3 + back, b.x + 8, b.y, rgba_of(0xf4f4f4ff));
    }
}

/// A fireball tower, its brazier burning at `flame` (0 to 2), or flaring as it throws (3).
inline void fire_tower(gfx::Sheet& s, const gfx::Box& b, int flame) {
    const Rgba stone = rgba_of(0x7a7068ff), dark = rgba_of(0x4b443eff);
    s.fill(gfx::Box{b.x + 3, b.y + 9, 10, 6}, stone);
    s.fill(gfx::Box{b.x + 2, b.y + 7, 12, 3}, dark);
    const double r = flame == 3 ? 5.0 : 2.8 + 0.6 * flame;
    disc(s, b, 8.0, 5.5, r, rgba_of(0xd8361aff));
    disc(s, b, 8.0, 6.0, r * 0.65, rgba_of(0xff8c1aff));
    disc(s, b, 8.0, 6.5, r * 0.35, rgba_of(0xffe14dff));
}

/// A walker in its `step` of four, a runner or a brute.
inline void walker(gfx::Sheet& s, const gfx::Box& b, bool brute, int step) {
    const Rgba body = brute ? rgba_of(0x8e3fb4ff) : rgba_of(0xd9473dff);
    const Rgba dark = brute ? rgba_of(0x5a2275ff) : rgba_of(0x8c241eff);
    const double r = brute ? 5.0 : 3.6;
    const std::int64_t bob = (step % 2 == 0) ? 0 : 1;
    // legs, apart and together by step
    const std::int64_t spread = (step == 1 || step == 3) ? 1 : 3;
    s.fill(gfx::Box{b.x + 8 - spread - 1, b.y + 12, 2, 3}, dark);
    s.fill(gfx::Box{b.x + 8 + spread - 1, b.y + 12, 2, 3}, dark);
    disc(s, b, 8.0, 8.0 - static_cast<double>(bob), r, body);
    disc(s, b, 8.0, 3.5 - static_cast<double>(bob), brute ? 3.0 : 2.4, dark);
    s.put(b.x + 9, b.y + 3 - bob, rgba_of(0xffffffff));
    if (brute) {
        s.fill(gfx::Box{b.x + 3, b.y + 7 - bob, 10, 2}, dark);
    }
}

/// An arrow flying `dir` of eight, east first and clockwise.
inline void arrow(gfx::Sheet& s, const gfx::Box& b, int dir) {
    const double a = dir * 0.7853981633974483;
    const double cx = static_cast<double>(b.x) + 8.0, cy = static_cast<double>(b.y) + 8.0;
    const double ux = std::cos(a), uy = std::sin(a);
    const auto at = [&](double t, double side) {
        return std::pair<std::int64_t, std::int64_t>{std::llround(cx + ux * t - uy * side),
                                                     std::llround(cy + uy * t + ux * side)};
    };
    const auto [tx, ty] = at(-6, 0);
    const auto [hx, hy] = at(6, 0);
    line(s, tx, ty, hx, hy, rgba_of(0xc9a46aff));
    const auto [l1x, l1y] = at(3, -2.5);
    const auto [l2x, l2y] = at(3, 2.5);
    line(s, hx, hy, l1x, l1y, rgba_of(0xf0f0f0ff));
    line(s, hx, hy, l2x, l2y, rgba_of(0xf0f0f0ff));
    const auto [f1x, f1y] = at(-6, -2);
    const auto [f2x, f2y] = at(-6, 2);
    line(s, tx, ty, f1x, f1y, rgba_of(0xffffffff));
    line(s, tx, ty, f2x, f2y, rgba_of(0xffffffff));
}

/// A fireball in its `flicker` of four.
inline void fireball(gfx::Sheet& s, const gfx::Box& b, int flicker) {
    const double r = 5.2 + (flicker % 2) * 0.8;
    disc(s, b, 8.0, 8.0, r, rgba_of(0xd8361aff));
    disc(s, b, flicker == 1 ? 7.0 : 8.0, flicker == 2 ? 7.0 : 8.0, r * 0.7, rgba_of(0xff8c1aff));
    disc(s, b, 7.5, 7.5, r * 0.38, rgba_of(0xffe14dff));
    s.put(b.x + 7, b.y + 7, rgba_of(0xffffffff));
}

/// A burst in its `age` of three: bright, wide, then smoke.
inline void burst(gfx::Sheet& s, const gfx::Box& b, int age) {
    const double r = 3.5 + age * 2.0;
    const Rgba outer = age == 2 ? rgba_of(0x6b6b6bcc) : rgba_of(0xd8361aff);
    disc(s, b, 8.0, 8.0, r, outer);
    if (age < 2) {
        disc(s, b, 8.0, 8.0, r * 0.65, rgba_of(0xff8c1aff));
        disc(s, b, 8.0, 8.0, r * 0.3, rgba_of(0xffffb0ff));
    }
}

inline void cursor(gfx::Sheet& s, const gfx::Box& b) {
    const Rgba c = rgba_of(0xfff176ff);
    for (std::int64_t i = 0; i < 16; ++i) {
        if (i < 5 || i > 10) {
            s.put(b.x + i, b.y, c);
            s.put(b.x + i, b.y + 15, c);
            s.put(b.x, b.y + i, c);
            s.put(b.x + 15, b.y + i, c);
        }
    }
}

} // namespace art

/// THE GAME'S SHEET, made once: eight frames across and seven down, each `kFramePx` square.
inline Art make_art() {
    gfx::Sheet s(kFramePx, kFramePx, 8, 7);
    Art a;
    for (std::size_t i = 0; i < a.grass.size(); ++i) {
        const char* glyph = i == 0 ? " " : (i == 1 ? "," : "'");
        a.grass[i] = s.frame([&](const gfx::Box& b) { art::grass(s, b, static_cast<std::int64_t>(i)); },
                             glyph, art::kBrightGreen, art::kGreen);
    }
    a.road = s.frame([&](const gfx::Box& b) { art::road(s, b); }, " ", art::kBlack, art::kYellow);
    a.tree = s.frame([&](const gfx::Box& b) { art::tree(s, b); }, "^", art::kBlack, art::kGreen);
    a.rock = s.frame([&](const gfx::Box& b) { art::rock(s, b); }, "o", art::kGrey, art::kGreen);
    a.castle = s.frame([&](const gfx::Box& b) { art::castle(s, b); }, "#", art::kBrightWhite,
                       art::kBlue);
    const std::array<gfx::Color, 4> arrow_inks = {art::kYellow, art::kBrightYellow,
                                                  art::kBrightYellow, art::kBrightWhite};
    for (int i = 0; i < 4; ++i) {
        a.arrow_tower[static_cast<std::size_t>(i)] =
            s.frame([&](const gfx::Box& b) { art::arrow_tower(s, b, i); }, "A",
                    arrow_inks[static_cast<std::size_t>(i)], art::kBlack);
    }
    const std::array<gfx::Color, 4> fire_inks = {art::kRed, art::kBrightRed, art::kRed,
                                                 art::kBrightYellow};
    for (int i = 0; i < 4; ++i) {
        a.fire_tower[static_cast<std::size_t>(i)] =
            s.frame([&](const gfx::Box& b) { art::fire_tower(s, b, i); }, "F",
                    fire_inks[static_cast<std::size_t>(i)], art::kBlack);
    }
    for (int i = 0; i < 4; ++i) {
        a.runner[static_cast<std::size_t>(i)] =
            s.frame([&](const gfx::Box& b) { art::walker(s, b, false, i); }, i % 2 == 0 ? "r" : "R",
                    art::kBrightRed);
    }
    for (int i = 0; i < 4; ++i) {
        a.brute[static_cast<std::size_t>(i)] =
            s.frame([&](const gfx::Box& b) { art::walker(s, b, true, i); }, "B", art::kMagenta);
    }
    // the health bars share one slot: its top two rows dark, the two beneath them bright
    const gfx::Box bars = s.take_slot();
    s.fill(gfx::Box{bars.x, bars.y, bars.w, 2}, art::rgba_of(0x3a0d0dff));
    s.fill(gfx::Box{bars.x, bars.y + 2, bars.w, 2}, art::rgba_of(0x5fe35aff));
    a.bar_back = s.cut(gfx::Box{bars.x, bars.y, bars.w, 2}, "", gfx::no_color());
    a.bar_front = s.cut(gfx::Box{bars.x, bars.y + 2, bars.w, 2}, "", gfx::no_color());
    const std::array<const char*, 8> arrow_glyphs = {"-", "\\", "|", "/", "-", "\\", "|", "/"};
    for (int i = 0; i < 8; ++i) {
        a.arrow[static_cast<std::size_t>(i)] =
            s.frame([&](const gfx::Box& b) { art::arrow(s, b, i); },
                    arrow_glyphs[static_cast<std::size_t>(i)], art::kBrightWhite);
    }
    const std::array<const char*, 4> ball_glyphs = {"*", "o", "*", "O"};
    for (int i = 0; i < 4; ++i) {
        a.fireball[static_cast<std::size_t>(i)] =
            s.frame([&](const gfx::Box& b) { art::fireball(s, b, i); },
                    ball_glyphs[static_cast<std::size_t>(i)],
                    i % 2 == 0 ? art::kBrightYellow : art::kYellow, art::kRed);
    }
    const std::array<const char*, 3> burst_glyphs = {"@", "*", "."};
    for (int i = 0; i < 3; ++i) {
        a.burst[static_cast<std::size_t>(i)] =
            s.frame([&](const gfx::Box& b) { art::burst(s, b, i); },
                    burst_glyphs[static_cast<std::size_t>(i)],
                    i == 2 ? art::kGrey : art::kBrightYellow, i == 0 ? art::kRed : gfx::no_color());
    }
    a.cursor = s.frame([&](const gfx::Box& b) { art::cursor(s, b); }, "", gfx::no_color(),
                       art::kBrightWhite);
    a.sheet = s.args();
    return a;
}

} // namespace zengine::tile_defense

#endif // ZENGINE_EXAMPLES_TILE_DEFENSE_ART_HPP
