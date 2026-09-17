// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_NEOVIM_KEYS_HPP
#define ZENGINE_NEOVIM_KEYS_HPP

// WHAT A MAKER'S HAND DID, SAID IN NEOVIM'S KEY NOTATION.
//
// Workshop hands a pane two facts for one keystroke (`workshop/pane_vocabulary.hpp`): the KEY
// (`PaneKey`: a scancode and the modifiers the backend observed) and, when the platform's
// layout made one, the TEXT (`PaneTextInput`). A printable keystroke arrives as both, in that
// order; a Ctrl chord arrives as the key alone. So the rule here is:
//
//   TEXT is sent as text -- the only truthful route to a character, because a scancode knows no
//        layout -- with `<` spelled `<lt>`, which Neovim's input parser otherwise reads as the
//        start of a key name and swallows (measured: an unescaped `<x>` inserted nothing).
//   A KEY is sent only when no text will follow it: a named key (`<CR>`, `<Esc>`, `<Tab>`,
//        `<BS>`, the arrows, ...) or a chord with Ctrl, Alt or Super. A plain or shifted
//        printable key is left to the text that carries it. Ctrl+Alt together with a printable
//        key is AltGr on Windows layouts, and its text is what the maker meant.
//
// THE KEYS BEYOND `input::scan`'s NAMED SET arrive only from the SDL backend, which passes
// SDL's own scancodes through (`input/translate_sdl.hpp`): Insert, PageUp, PageDown and F1..F12
// are spelled here by SDL's numbers, and no terminal backend produces them.

#include "input/vocabulary.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace zengine::neovim {

namespace sdl_scan {
inline constexpr std::int64_t kF1 = 58;
inline constexpr std::int64_t kF12 = 69;
inline constexpr std::int64_t kInsert = 73;
inline constexpr std::int64_t kPageUp = 75;
inline constexpr std::int64_t kPageDown = 78;
} // namespace sdl_scan

/// TEXT AS NEOVIM INPUT: `<` is the one byte Neovim's parser would read as something else.
inline std::string text_input(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    for (const char c : text) {
        if (c == '<') {
            out += "<lt>";
        } else {
            out.push_back(c);
        }
    }
    return out;
}

namespace detail {

/// The character a printable scancode names on a US layout, used ONLY inside a chord
/// (`<C-]>`), where Neovim wants the key's own character and no text arrives. 0 when the
/// scancode is not a printable key.
inline char chord_char(std::int64_t scancode) noexcept {
    namespace scan = zengine::input::scan;
    if (scancode >= scan::kA && scancode <= scan::kZ) {
        return static_cast<char>('a' + (scancode - scan::kA));
    }
    if (scancode >= scan::k1 && scancode <= scan::k9) {
        return static_cast<char>('1' + (scancode - scan::k1));
    }
    switch (scancode) {
    case scan::k0: return '0';
    case scan::kMinus: return '-';
    case scan::kEquals: return '=';
    case scan::kLeftBracket: return '[';
    case scan::kRightBracket: return ']';
    case scan::kBackslash: return '\\';
    case scan::kSemicolon: return ';';
    case scan::kApostrophe: return '\'';
    case scan::kGrave: return '`';
    case scan::kComma: return ',';
    case scan::kPeriod: return '.';
    case scan::kSlash: return '/';
    default: return 0;
    }
}

/// The name of a key that produces no text, or empty.
inline std::string named(std::int64_t scancode) {
    namespace scan = zengine::input::scan;
    switch (scancode) {
    case scan::kReturn: return "CR";
    case scan::kEscape: return "Esc";
    case scan::kBackspace: return "BS";
    case scan::kTab: return "Tab";
    case scan::kSpace: return "Space";
    case scan::kHome: return "Home";
    case scan::kEnd: return "End";
    case scan::kDelete: return "Del";
    case scan::kLeft: return "Left";
    case scan::kRight: return "Right";
    case scan::kUp: return "Up";
    case scan::kDown: return "Down";
    case sdl_scan::kInsert: return "Insert";
    case sdl_scan::kPageUp: return "PageUp";
    case sdl_scan::kPageDown: return "PageDown";
    default: break;
    }
    if (scancode >= sdl_scan::kF1 && scancode <= sdl_scan::kF12) {
        return "F" + std::to_string(scancode - sdl_scan::kF1 + 1);
    }
    return std::string();
}

} // namespace detail

/// THE KEY AS NEOVIM INPUT, or empty when this keystroke is left to its text (or names nothing
/// Neovim has a spelling for).
inline std::string key_input(std::int64_t scancode, std::int64_t modifiers) {
    namespace mod = zengine::input::mod;
    namespace scan = zengine::input::scan;
    const bool ctrl = (modifiers & mod::kCtrl) != 0;
    const bool alt = (modifiers & mod::kAlt) != 0;
    const bool shift = (modifiers & mod::kShift) != 0;
    const bool super = (modifiers & mod::kSuper) != 0;
    std::string name = detail::named(scancode);
    if (name.empty()) {
        const char c = detail::chord_char(scancode);
        if (c == 0) {
            return std::string();
        }
        if ((!ctrl && !alt && !super) || (ctrl && alt)) {
            return std::string(); // its text carries it (AltGr included)
        }
        name.assign(1, c);
    } else if (scancode == scan::kSpace && !ctrl && !alt && !super) {
        return std::string(); // a plain or shifted space arrives as text
    }
    std::string prefix;
    if (ctrl) {
        prefix += "C-";
    }
    if (shift) {
        prefix += "S-";
    }
    if (alt) {
        prefix += "M-";
    }
    if (super) {
        prefix += "D-";
    }
    if (prefix.empty() && name.size() > 1) {
        return "<" + name + ">";
    }
    return "<" + prefix + name + ">";
}

/// THE MODIFIERS A MOUSE EVENT CARRIES, as `nvim_input_mouse` spells them.
inline std::string mouse_modifiers(std::int64_t modifiers) {
    namespace mod = zengine::input::mod;
    std::string out;
    if ((modifiers & mod::kCtrl) != 0) {
        out += 'C';
    }
    if ((modifiers & mod::kShift) != 0) {
        out += 'S';
    }
    if ((modifiers & mod::kAlt) != 0) {
        out += 'A';
    }
    return out;
}

} // namespace zengine::neovim

#endif // ZENGINE_NEOVIM_KEYS_HPP
