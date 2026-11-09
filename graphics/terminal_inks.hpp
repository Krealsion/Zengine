// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_GRAPHICS_TERMINAL_INKS_HPP
#define ZENGINE_GRAPHICS_TERMINAL_INKS_HPP

// The sixteen inks a terminal shows, by the words a terminal look names them with, and the two
// ways between them and colours: a colour to its nearest ink, and an ink to the colour xterm shows
// it in. A terminal window writes them; a theme of one medium shown in the other is converted
// with them. Reference: graphics/docs/graphics.md, "The terminal's look".

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace zengine::graphics::window {

/// The sixteen inks a terminal shows, by name, in the order their codes count (30 to 37, then the
/// bright ones, 90 to 97), each with the colour a terminal shows it in by default (xterm's).
/// The ink word for the terminal's default colours: its default ink (SGR 39) and ground (49).
inline constexpr std::string_view kTerminalDefault = "terminal default";
inline constexpr std::array<const char*, 8> kTerminalInkNames = {
    "black", "red", "green", "yellow", "blue", "magenta", "cyan", "white"};
inline constexpr std::array<std::uint32_t, 16> kTerminalInkColors = {
    0x000000, 0xcd0000, 0x00cd00, 0xcdcd00, 0x0000ee, 0xcd00cd, 0x00cdcd, 0xe5e5e5,
    0x7f7f7f, 0xff0000, 0x00ff00, 0xffff00, 0x5c5cff, 0xff00ff, 0x00ffff, 0xffffff};

/// The ink a word names, as SGR's codes: `terminal default`, the terminal's default ink (39), or
/// one of its sixteen (`red` 31, `bright red` 91), each perhaps `bold` (`;1`); "" where the word names
/// none of them.
inline std::string sgr_codes_of_word(std::string_view word) {
    std::string codes;
    bool bold = false;
    constexpr std::string_view kBold = " bold";
    if (word.size() > kBold.size() && word.substr(word.size() - kBold.size()) == kBold) {
        bold = true;
        word.remove_suffix(kBold.size());
    }
    if (word == kTerminalDefault) {
        codes = "39";
    } else {
        int base = 30;
        constexpr std::string_view kBright = "bright ";
        if (word.substr(0, kBright.size()) == kBright) {
            base = 90;
            word.remove_prefix(kBright.size());
        }
        for (std::size_t i = 0; i < kTerminalInkNames.size(); ++i) {
            if (word == kTerminalInkNames[i]) {
                codes = std::to_string(base + static_cast<int>(i));
            }
        }
    }
    if (codes.empty()) {
        return {};
    }
    return bold ? codes + ";1" : codes;
}

/// Which of the sixteen inks is nearest a colour (0xRRGGBBAA, its alpha not counted), by the
/// colours a terminal shows them in by default: its SGR code, 30 to 37 or 90 to 97.
inline int nearest_terminal_ink(std::int64_t rgba) noexcept {
    const auto channel = [](std::uint32_t c, int shift) {
        return static_cast<std::int64_t>((c >> shift) & 0xffu);
    };
    const auto rgb = static_cast<std::uint32_t>((static_cast<std::uint64_t>(rgba) >> 8) & 0xffffffu);
    int best = 0;
    std::int64_t best_distance = -1;
    for (std::size_t i = 0; i < kTerminalInkColors.size(); ++i) {
        const std::uint32_t c = kTerminalInkColors[i];
        const std::int64_t dr = channel(rgb, 16) - channel(c, 16);
        const std::int64_t dg = channel(rgb, 8) - channel(c, 8);
        const std::int64_t db = channel(rgb, 0) - channel(c, 0);
        const std::int64_t d = dr * dr + dg * dg + db * db;
        if (best_distance < 0 || d < best_distance) {
            best_distance = d;
            best = static_cast<int>(i);
        }
    }
    return best < 8 ? 30 + best : 90 + (best - 8);
}

/// The word a terminal look names an ink by, from its SGR code (30 to 37, 90 to 97): `red`,
/// `bright red`; "" for any other code.
inline std::string ink_word_of_code(int code) {
    if (code >= 30 && code <= 37) {
        return kTerminalInkNames[static_cast<std::size_t>(code - 30)];
    }
    if (code >= 90 && code <= 97) {
        return std::string("bright ") + kTerminalInkNames[static_cast<std::size_t>(code - 90)];
    }
    return {};
}

/// THE COLOUR XTERM SHOWS AN INK IN (0xRRGGBBAA, opaque), from its word, `bold` and all; none for
/// `terminal default`, whose colour is the terminal's own, or a word naming no ink.
inline std::optional<std::int64_t> ink_rgba(std::string_view word) {
    std::string codes = sgr_codes_of_word(word);
    if (codes.size() > 2 && codes.compare(codes.size() - 2, 2, ";1") == 0) {
        codes.resize(codes.size() - 2);
    }
    if (codes.empty() || codes == "39") {
        return std::nullopt;
    }
    const int code = std::stoi(codes);
    const std::size_t at = static_cast<std::size_t>(code >= 90 ? code - 90 + 8 : code - 30);
    return static_cast<std::int64_t>(static_cast<std::uint64_t>(kTerminalInkColors[at]) << 8 |
                                     0xffu);
}

} // namespace zengine::graphics::window

#endif // ZENGINE_GRAPHICS_TERMINAL_INKS_HPP
