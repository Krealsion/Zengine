// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_SURFACE_SKIN_SDL_GLYPHS_HPP
#define ZENGINE_SURFACE_SKIN_SDL_GLYPHS_HPP

// The SDL Skin's letters: a bitmap the size of a canvas cell, so a window with no font can
// still read its labels. Data in a header, not a font dependency: pure, so every lane pins the
// label path, SDL built or not; no asset to find or ship; too small to become a typography
// system. Debug-grade on purpose. A glyph is 6x6 (rows 0-4 capitals and digits, row 5
// descenders, column 5 tracking). It covers printable ASCII 0x20-0x7E; any other byte draws the
// visible `kUnknownGlyph`, never nothing.

#include <cstdint>

namespace zengine::surface {

inline constexpr int kGlyphCols = 6;
inline constexpr int kGlyphRows = 6;

/// One glyph: six rows, bit `c` of a row set means column `c` is ink.
struct Glyph {
    std::uint8_t row[kGlyphRows] = {};

    friend bool operator==(const Glyph&, const Glyph&) = default;
};

namespace detail {

/// Six rows of six characters, top to bottom: `#` is ink, anything else is not.
/// The parameter is a reference to an exactly-sized array, so a row typed one
/// character short is a compile error rather than a glyph that quietly shifts.
constexpr Glyph art(const char (&s)[kGlyphCols * kGlyphRows + 1]) {
    Glyph g{};
    for (int r = 0; r < kGlyphRows; ++r) {
        std::uint8_t bits = 0;
        for (int c = 0; c < kGlyphCols; ++c) {
            if (s[r * kGlyphCols + c] == '#') {
                bits = static_cast<std::uint8_t>(bits | (1u << c));
            }
        }
        g.row[r] = bits;
    }
    return g;
}

} // namespace detail

inline constexpr unsigned char kFirstGlyph = 0x20; ///< space
inline constexpr unsigned char kLastGlyph = 0x7e;  ///< tilde

/// What a byte outside the table draws: a filled-outline box, visible so no byte vanishes.
inline constexpr Glyph kUnknownGlyph =
    detail::art("####.."
                "#..#.."
                "#..#.."
                "#..#.."
                "####.."
                "......");

/// Printable ASCII, in order, `kFirstGlyph` first. Each entry is six six-character
/// rows; read a glyph by reading its six groups left to right.
inline constexpr Glyph kGlyphs[kLastGlyph - kFirstGlyph + 1] = {
    detail::art("......" "......" "......" "......" "......" "......"), // 0x20
    detail::art("..#..." "..#..." "..#..." "......" "..#..." "......"), // !
    detail::art(".#.#.." ".#.#.." "......" "......" "......" "......"), // "
    detail::art(".#.#.." "#####." ".#.#.." "#####." ".#.#.." "......"), // #
    detail::art("..#..." ".####." "#....." "....#." "####.." "..#..."), // $
    detail::art("##..#." "##.#.." "..#..." ".#.##." "#..##." "......"), // %
    detail::art(".##..." "#..#.." ".##..." "#.#.#." ".##.#." "......"), // &
    detail::art("..#..." "..#..." "......" "......" "......" "......"), // '
    detail::art("...#.." "..#..." "..#..." "..#..." "...#.." "......"), // (
    detail::art(".#...." "..#..." "..#..." "..#..." ".#...." "......"), // )
    detail::art("......" "#.#.#." ".###.." "#.#.#." "......" "......"), // *
    detail::art("......" "..#..." "#####." "..#..." "......" "......"), // +
    detail::art("......" "......" "......" "......" "..#..." ".#...."), // ,
    detail::art("......" "......" ".###.." "......" "......" "......"), // -
    detail::art("......" "......" "......" "......" "..#..." "......"), // .
    detail::art("....#." "...#.." "..#..." ".#...." "#....." "......"), // /
    detail::art(".###.." "#..##." "#.#.#." "##..#." ".###.." "......"), // 0
    detail::art("..#..." ".##..." "..#..." "..#..." ".###.." "......"), // 1
    detail::art(".###.." "#...#." "..##.." ".#...." "#####." "......"), // 2
    detail::art("####.." "....#." ".###.." "....#." "####.." "......"), // 3
    detail::art("#..#.." "#..#.." "#####." "...#.." "...#.." "......"), // 4
    detail::art("#####." "#....." "####.." "....#." "####.." "......"), // 5
    detail::art(".###.." "#....." "####.." "#...#." ".###.." "......"), // 6
    detail::art("#####." "....#." "...#.." "..#..." "..#..." "......"), // 7
    detail::art(".###.." "#...#." ".###.." "#...#." ".###.." "......"), // 8
    detail::art(".###.." "#...#." ".####." "....#." ".###.." "......"), // 9
    detail::art("......" "......" "..#..." "......" "..#..." "......"), // :
    detail::art("......" "......" "..#..." "......" "..#..." ".#...."), // ;
    detail::art("...#.." "..#..." ".#...." "..#..." "...#.." "......"), // <
    detail::art("......" ".####." "......" ".####." "......" "......"), // =
    detail::art(".#...." "..#..." "...#.." "..#..." ".#...." "......"), // >
    detail::art(".###.." "#...#." "..##.." "......" "..#..." "......"), // ?
    detail::art(".###.." "#...#." "#.###." "#....." ".###.." "......"), // @
    detail::art(".###.." "#...#." "#####." "#...#." "#...#." "......"), // A
    detail::art("####.." "#...#." "####.." "#...#." "####.." "......"), // B
    detail::art(".###.." "#...#." "#....." "#...#." ".###.." "......"), // C
    detail::art("####.." "#...#." "#...#." "#...#." "####.." "......"), // D
    detail::art("#####." "#....." "####.." "#....." "#####." "......"), // E
    detail::art("#####." "#....." "####.." "#....." "#....." "......"), // F
    detail::art(".###.." "#....." "#..##." "#...#." ".###.." "......"), // G
    detail::art("#...#." "#...#." "#####." "#...#." "#...#." "......"), // H
    detail::art(".###.." "..#..." "..#..." "..#..." ".###.." "......"), // I
    detail::art("....#." "....#." "....#." "#...#." ".###.." "......"), // J
    detail::art("#...#." "#..#.." "###..." "#..#.." "#...#." "......"), // K
    detail::art("#....." "#....." "#....." "#....." "#####." "......"), // L
    detail::art("#...#." "##.##." "#.#.#." "#...#." "#...#." "......"), // M
    detail::art("#...#." "##..#." "#.#.#." "#..##." "#...#." "......"), // N
    detail::art(".###.." "#...#." "#...#." "#...#." ".###.." "......"), // O
    detail::art("####.." "#...#." "####.." "#....." "#....." "......"), // P
    detail::art(".###.." "#...#." "#...#." "#..#.." ".##.#." "......"), // Q
    detail::art("####.." "#...#." "####.." "#..#.." "#...#." "......"), // R
    detail::art(".####." "#....." ".###.." "....#." "####.." "......"), // S
    detail::art("#####." "..#..." "..#..." "..#..." "..#..." "......"), // T
    detail::art("#...#." "#...#." "#...#." "#...#." ".###.." "......"), // U
    detail::art("#...#." "#...#." "#...#." ".#.#.." "..#..." "......"), // V
    detail::art("#...#." "#...#." "#.#.#." "##.##." "#...#." "......"), // W
    detail::art("#...#." ".#.#.." "..#..." ".#.#.." "#...#." "......"), // X
    detail::art("#...#." ".#.#.." "..#..." "..#..." "..#..." "......"), // Y
    detail::art("#####." "...#.." "..#..." ".#...." "#####." "......"), // Z
    detail::art("..##.." "..#..." "..#..." "..#..." "..##.." "......"), // [
    detail::art("#....." ".#...." "..#..." "...#.." "....#." "......"), // backslash
    detail::art(".##..." "..#..." "..#..." "..#..." ".##..." "......"), // ]
    detail::art("..#..." ".#.#.." "......" "......" "......" "......"), // ^
    detail::art("......" "......" "......" "......" "......" "#####."), // _
    detail::art(".#...." "..#..." "......" "......" "......" "......"), // `
    detail::art("......" "###..." "..##.." "#..#.." ".###.." "......"), // a
    detail::art("#....." "###..." "#..#.." "#..#.." "###..." "......"), // b
    detail::art("......" ".###.." "#....." "#....." ".###.." "......"), // c
    detail::art("...#.." ".###.." "#..#.." "#..#.." ".###.." "......"), // d
    detail::art("......" ".##..." "#..#.." "####.." ".###.." "......"), // e
    detail::art("..##.." ".#...." "####.." ".#...." ".#...." "......"), // f
    detail::art("......" ".###.." "#..#.." ".###.." "...#.." "###..."), // g
    detail::art("#....." "###..." "#..#.." "#..#.." "#..#.." "......"), // h
    detail::art("..#..." "......" "..#..." "..#..." "..#..." "......"), // i
    detail::art("...#.." "......" "...#.." "...#.." "...#.." "###..."), // j
    detail::art("#....." "#..#.." "#.#..." "##...." "#..#.." "......"), // k
    detail::art(".##..." "..#..." "..#..." "..#..." ".###.." "......"), // l
    detail::art("......" "##.##." "#.#.#." "#.#.#." "#.#.#." "......"), // m
    detail::art("......" "###..." "#..#.." "#..#.." "#..#.." "......"), // n
    detail::art("......" ".##..." "#..#.." "#..#.." ".##..." "......"), // o
    detail::art("......" "###..." "#..#.." "###..." "#....." "#....."), // p
    detail::art("......" ".###.." "#..#.." ".###.." "...#.." "...#.."), // q
    detail::art("......" "#.##.." "##...." "#....." "#....." "......"), // r
    detail::art("......" ".###.." "##...." "..##.." "###..." "......"), // s
    detail::art(".#...." "####.." ".#...." ".#...." "..##.." "......"), // t
    detail::art("......" "#..#.." "#..#.." "#..#.." ".###.." "......"), // u
    detail::art("......" "#...#." "#...#." ".#.#.." "..#..." "......"), // v
    detail::art("......" "#...#." "#.#.#." "#.#.#." ".#.#.." "......"), // w
    detail::art("......" "#..#.." ".##..." ".##..." "#..#.." "......"), // x
    detail::art("......" "#..#.." "#..#.." ".###.." "...#.." "###..."), // y
    detail::art("......" "####.." "..#..." ".#...." "####.." "......"), // z
    detail::art("...#.." "..#..." ".##..." "..#..." "...#.." "......"), // {
    detail::art("..#..." "..#..." "..#..." "..#..." "..#..." "......"), // |
    detail::art(".#...." "..#..." "..##.." "..#..." ".#...." "......"), // }
    detail::art("......" "......" ".##.#." "#..##." "......" "......"), // ~
};

/// The bitmap for one byte of a label, never a codepoint: the terminal Skins advance a cell
/// per byte too, so a multi-byte sequence takes one cell per byte and draws `kUnknownGlyph`.
constexpr const Glyph& glyph_of(unsigned char byte) noexcept {
    if (byte < kFirstGlyph || byte > kLastGlyph) {
        return kUnknownGlyph;
    }
    return kGlyphs[byte - kFirstGlyph];
}

} // namespace zengine::surface

#endif // ZENGINE_SURFACE_SKIN_SDL_GLYPHS_HPP
