// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_SURFACE_SKIN_SDL_GLYPHS_HPP
#define ZENGINE_SURFACE_SKIN_SDL_GLYPHS_HPP

// The SDL Skin's letters — a bitmap the size of a canvas cell, and nothing more.
//
// WHY THIS EXISTS AT ALL. `SurfaceLabel` has always carried text over a canvas;
// the terminal skins draw it because a terminal already owns a font, and the SDL
// skin dropped every one of them because a window does not. That hole (P8) is
// what made the graphical Workshop unreadable: object names, the inspector, the
// notice line, the help and the resize handle are ALL labels, so the medium was
// rendering the furniture and none of the meaning.
//
// WHY IT IS DATA IN A HEADER AND NOT A FONT DEPENDENCY. Three properties decided
// it, and none of them is aesthetic:
//
//   - it is PURE, so the whole label path can be pinned as arithmetic on every
//     lane — including the Windows stranger lane, which builds no SDL at all.
//     A font library's raster can only be witnessed through the library.
//   - it needs no asset, no runtime discovery, no install path and no third-party
//     licence. There is nothing to find at startup and nothing to ship beside the
//     binary, so the package cannot half-arrive on someone else's machine.
//   - it is deliberately too small to grow into a typography system. There is no
//     size, no family, no fallback and no layout here: one bitmap, one cell.
//
// It is a debug-grade face and it is meant to be. G-0's success condition is that
// a person can READ the graphical Workshop, not that the type is good.
//
// THE METRIC. A glyph is six columns by six rows, drawn at `kGlyphScale` device
// pixels per glyph pixel so that one glyph fills exactly one canvas cell:
//
//     rows 0-4    capitals, digits and ascenders
//     rows 1-4    x-height
//     row  5      descenders (g j p q y) and the comma's tail
//     column 5    tracking — the gap to the next cell, so words do not touch
//
// THE TABLE covers printable ASCII 0x20-0x7E and nothing else. That is a real
// limitation, stated here rather than discovered later: `SurfaceLabel::text` is a
// `std::string` and may hold any byte, so anything outside that range — a control
// character, or any byte of a multi-byte UTF-8 sequence — renders as the visible
// `kUnknownGlyph` box. It is never dropped. A character that silently disappears
// would be P8 again at character granularity, which is the one outcome this file
// exists to prevent.

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

/// What a byte outside the table draws: a filled-outline box. Visible on purpose
/// — see the header note on why silence is not an option here.
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

/// The bitmap for one BYTE of a label, never a codepoint.
///
/// Byte, deliberately: the terminal skins advance one canvas cell per byte too
/// (`canvas_body` indexes `l.text[i]`), so both media place the same character in
/// the same cell and a canvas cannot mean two different pictures. The cost is
/// stated where it is paid — a multi-byte sequence occupies one cell per byte
/// here and draws `kUnknownGlyph` in each of them.
constexpr const Glyph& glyph_of(unsigned char byte) noexcept {
    if (byte < kFirstGlyph || byte > kLastGlyph) {
        return kUnknownGlyph;
    }
    return kGlyphs[byte - kFirstGlyph];
}

} // namespace zengine::surface

#endif // ZENGINE_SURFACE_SKIN_SDL_GLYPHS_HPP
