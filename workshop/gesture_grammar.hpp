// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_GESTURE_GRAMMAR_HPP
#define ZENGINE_WORKSHOP_GESTURE_GRAMMAR_HPP

// How a keymap writes a key and reads one back -- a file's spelling, the band's and a
// legend's -- and what a backend cannot produce. A part of `workshop/keymap.hpp`, which
// includes it after the action catalog; nothing else includes it.
// Workshop law: agents/workshop/key-spelling.md (+2 registers; agents/workshop.md routes)

#include "input/vocabulary.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace zengine::workshop {

// ---- The written gesture grammar ---------------------------------------------------------
//
// A binding is modifier words joined to one key name with `+` (`ctrl+k`, `shift+h`, `[`);
// punctuation keys are named by their own character. Written out in the order ctrl, shift, alt,
// super; parsed in any order, with duplicates refused.

/// DOUBLE SHIFT: a gesture of Workshop's own, outside every scancode -- Shift tapped twice, alone
/// and quickly, which Workshop recognizes from Shift's presses and its release in between. It
/// takes no modifier and types nothing.
inline constexpr std::int64_t kDoubleShift = 0x10000;

/// Is this scancode one of the two Shift keys, pressed or let go alone?
inline constexpr bool is_shift_scan(std::int64_t sc) noexcept {
    return sc == scan::kLeftShift || sc == scan::kRightShift;
}

/// The written name of a named scancode, or nullptr for a value this grammar cannot say.
// WL-KEY-14 -- agents/workshop/key-spelling.md
inline constexpr const char* key_name_of(std::int64_t scancode) noexcept {
    switch (scancode) {
    case kDoubleShift: return "double-shift";
    case scan::kA: return "a";
    case scan::kB: return "b";
    case scan::kC: return "c";
    case scan::kD: return "d";
    case scan::kE: return "e";
    case scan::kF: return "f";
    case scan::kG: return "g";
    case scan::kH: return "h";
    case scan::kI: return "i";
    case scan::kJ: return "j";
    case scan::kK: return "k";
    case scan::kL: return "l";
    case scan::kM: return "m";
    case scan::kN: return "n";
    case scan::kO: return "o";
    case scan::kP: return "p";
    case scan::kQ: return "q";
    case scan::kR: return "r";
    case scan::kS: return "s";
    case scan::kT: return "t";
    case scan::kU: return "u";
    case scan::kV: return "v";
    case scan::kW: return "w";
    case scan::kX: return "x";
    case scan::kY: return "y";
    case scan::kZ: return "z";
    case scan::k1: return "1";
    case scan::k2: return "2";
    case scan::k3: return "3";
    case scan::k4: return "4";
    case scan::k5: return "5";
    case scan::k6: return "6";
    case scan::k7: return "7";
    case scan::k8: return "8";
    case scan::k9: return "9";
    case scan::k0: return "0";
    case scan::kReturn: return "return";
    case scan::kEscape: return "escape";
    case scan::kBackspace: return "backspace";
    case scan::kTab: return "tab";
    case scan::kSpace: return "space";
    case scan::kMinus: return "-";
    case scan::kEquals: return "=";
    case scan::kLeftBracket: return "[";
    case scan::kRightBracket: return "]";
    case scan::kBackslash: return "\\";
    case scan::kSemicolon: return ";";
    case scan::kApostrophe: return "'";
    case scan::kGrave: return "`";
    case scan::kComma: return ",";
    case scan::kPeriod: return ".";
    case scan::kSlash: return "/";
    case scan::kHome: return "home";
    case scan::kDelete: return "delete";
    case scan::kEnd: return "end";
    case scan::kLeft: return "left";
    case scan::kRight: return "right";
    case scan::kDown: return "down";
    case scan::kUp: return "up";
    default: return nullptr;
    }
}

/// The scancode a written key name means, or 0 for a name outside the grammar.
// WL-KEY-14 -- agents/workshop/key-spelling.md
inline std::int64_t scancode_of_name(std::string_view name) noexcept {
    if (name == key_name_of(kDoubleShift)) {
        return kDoubleShift;
    }
    for (std::int64_t sc = 1; sc < 128; ++sc) {
        const char* word = key_name_of(sc);
        if (word != nullptr && name == word) {
            return sc;
        }
    }
    return 0;
}

/// A gesture as the file spells it (`ctrl+shift+z`); total over every nameable scancode, and
/// admission refuses the rest on the way in.
// WL-KEY-14 -- agents/workshop/key-spelling.md
inline std::string gesture_word(const Gesture& g) {
    std::string out;
    if ((g.modifiers & mod::kCtrl) != 0) {
        out += "ctrl+";
    }
    if ((g.modifiers & mod::kShift) != 0) {
        out += "shift+";
    }
    if ((g.modifiers & mod::kAlt) != 0) {
        out += "alt+";
    }
    if ((g.modifiers & mod::kSuper) != 0) {
        out += "super+";
    }
    const char* name = key_name_of(g.scancode);
    out += name != nullptr ? name : "?";
    return out;
}

/// Is this scancode a letter key? The one set the POSIX terminal can infer Shift on.
inline constexpr bool is_letter_scan(std::int64_t sc) noexcept {
    return sc >= scan::kA && sc <= scan::kZ;
}

/// A gesture as the SCREEN spells it -- the band's own compact voice.
// WL-KEY-02 -- agents/workshop/keyboard.md; WL-KEY-13 -- agents/workshop/key-spelling.md
inline std::string gesture_text(const Gesture& g) {
    // An action that answers to no key says so: `?` would read as a key a weaver cannot find, and
    // `-` is a real binding.
    if (!is_bound(g)) {
        return "unbound";
    }
    const bool shift = (g.modifiers & mod::kShift) != 0;
    const bool capital = shift && is_letter_scan(g.scancode);
    std::string out;
    if ((g.modifiers & mod::kCtrl) != 0) {
        out += "^";
    }
    if (shift && !capital) {
        out += "shift+";
    }
    if ((g.modifiers & mod::kAlt) != 0) {
        out += "alt+";
    }
    if ((g.modifiers & mod::kSuper) != 0) {
        out += "super+";
    }
    switch (g.scancode) {
    case scan::kReturn: out += "enter"; break;
    case scan::kEscape: out += "esc"; break;
    case kDoubleShift: out += "shift shift"; break;
    default: {
        const char* name = key_name_of(g.scancode);
        if (name == nullptr) {
            out += "?";
        } else if (capital) {
            out += static_cast<char>(name[0] - 'a' + 'A');
        } else {
            out += name;
        }
        break;
    }
    }
    return out;
}

/// HOW A SURFACE SPELLS A KEY, the weaver's to choose for each surface apart: short, the band's
/// compact voice (`B`, `^p`, `enter`), or full, as a keymap file writes it (`shift+b`, `ctrl+p`,
/// `return`).
// WL-KEY-16 -- agents/workshop/key-spelling.md
enum class KeySpelling : std::uint8_t { kShort, kFull };

/// The two spellings' words, as a preferences file and `KeymapShown` write them.
inline constexpr const char* kSpelledShort = "short";
inline constexpr const char* kSpelledFull = "full";

inline const char* spelling_word(KeySpelling spelling) {
    return spelling == KeySpelling::kFull ? kSpelledFull : kSpelledShort;
}

/// ...and the spelling a word names, `kFull` for any word but `short`.
inline KeySpelling spelling_of(std::string_view word) {
    return word == kSpelledShort ? KeySpelling::kShort : KeySpelling::kFull;
}

/// A gesture in the spelling chosen: `gesture_text` or `gesture_word`, and `unbound` for no key.
// WL-KEY-16 -- agents/workshop/key-spelling.md
inline std::string spell_gesture(const Gesture& g, KeySpelling spelling) {
    if (spelling == KeySpelling::kShort || !is_bound(g)) {
        return gesture_text(g);
    }
    return gesture_word(g);
}

/// What parsing a written gesture produced: the gesture, or a refusal naming what was found and
/// what would have worked.
struct ParsedGesture {
    bool accepted = false;
    Gesture gesture;
    std::string refusal;
};

// WL-KEY-14 -- agents/workshop/key-spelling.md
// WL-DESK-08 -- agents/workshop/desktop.md
inline ParsedGesture parse_gesture(std::string_view text) {
    ParsedGesture out;
    if (text.empty()) {
        out.refusal = "a gesture cannot be empty";
        return out;
    }
    // `none` is a gesture a weaver may author -- the one that answers to no key. A disable, not a
    // delete: the row stays declared, listed and nameable, and nothing hard-wired stands behind it.
    if (text == "none") {
        out.accepted = true;
        out.gesture = kNoGesture;
        return out;
    }
    std::int64_t mods = 0;
    std::string_view rest = text;
    while (true) {
        const std::size_t plus = rest.find('+');
        // A trailing token with no `+` after it is the key name -- including `+` itself
        // being unsayable, which is fine: no scancode names it.
        if (plus == std::string_view::npos || plus + 1 >= rest.size()) {
            break;
        }
        const std::string_view word = rest.substr(0, plus);
        std::int64_t bit = 0;
        if (word == "ctrl") {
            bit = mod::kCtrl;
        } else if (word == "shift") {
            bit = mod::kShift;
        } else if (word == "alt") {
            bit = mod::kAlt;
        } else if (word == "super") {
            bit = mod::kSuper;
        } else {
            out.refusal = "`" + std::string(word) +
                          "` is not a modifier (ctrl, shift, alt or super)";
            return out;
        }
        if ((mods & bit) != 0) {
            out.refusal = "`" + std::string(word) + "` appears twice in `" +
                          std::string(text) + "`";
            return out;
        }
        mods |= bit;
        rest = rest.substr(plus + 1);
    }
    const std::int64_t sc = scancode_of_name(rest);
    if (sc == 0) {
        out.refusal = "`" + std::string(rest) + "` is not a key this keymap can name";
        return out;
    }
    if (sc == kDoubleShift && mods != 0) {
        out.refusal = "`double-shift` is Shift itself, so no modifier may be written on it";
        return out;
    }
    out.accepted = true;
    out.gesture = Gesture{sc, mods};
    return out;
}

// ---- Honesty about what a backend can produce --------------------------------------------

/// Is this one of the CSI editing keys the POSIX parser reads measured modifiers on?
inline constexpr bool is_posix_editing_scan(std::int64_t sc) noexcept {
    return sc == scan::kHome || sc == scan::kEnd || sc == scan::kDelete ||
           sc == scan::kLeft || sc == scan::kRight || sc == scan::kUp || sc == scan::kDown;
}

/// The measured reason a gesture cannot arrive from the POSIX terminal backend, or
/// nullptr when no such gap is known.
// WL-KEY-08 -- agents/workshop/keyboard.md
inline const char* posix_gap(const Gesture& g) noexcept {
    if (g.scancode == kDoubleShift) {
        return "a terminal reports no Shift alone; double-shift is the OS window's";
    }
    if ((g.modifiers & mod::kSuper) != 0) {
        return "super never arrives from a POSIX terminal";
    }
    if ((g.modifiers & mod::kAlt) != 0 && !is_posix_editing_scan(g.scancode)) {
        return "alt arrives only on the editing keys from a POSIX terminal";
    }
    // The POSIX wire carries ctrl+letter as one control byte with no mark for Shift, so
    // ctrl+shift+letter arrives as plain ctrl+letter.
    if ((g.modifiers & mod::kCtrl) != 0 && (g.modifiers & mod::kShift) != 0 &&
        is_letter_scan(g.scancode)) {
        return "ctrl+shift+letter collapses to plain ctrl+letter on a POSIX terminal";
    }
    if ((g.modifiers & mod::kCtrl) != 0 &&
        (g.scancode == scan::kH || g.scancode == scan::kI || g.scancode == scan::kJ ||
         g.scancode == scan::kM)) {
        return "ctrl+h/i/j/m are byte-identical to backspace/tab/newline/return on a POSIX "
               "terminal";
    }
    if ((g.modifiers & mod::kShift) != 0 && !is_letter_scan(g.scancode) &&
        !is_posix_editing_scan(g.scancode) && g.scancode != scan::kTab) {
        // Tab is the one non-letter whose shifted form a terminal spells on its own:
        // `ESC [ Z` is back-tab, and the translator reads it.
        return "shift is not observable on that key from a POSIX terminal";
    }
    return nullptr;
}

/// The sentence a known gap is said in: the gesture as spelled, the action it moves, and the gap.
/// The file's overrides at load and a pane's rows at admission say it alike.
// WL-KEY-08 -- agents/workshop/keyboard.md
inline std::string gap_sentence(std::string_view spelled, std::string_view id, const char* gap) {
    return "`" + std::string(spelled) + "` (" + std::string(id) + "): " + gap;
}

/// THE TEXT A CONSUMED PRINTABLE GESTURE'S OWN KEYSTROKE PRODUCES, or "" when none is
/// expected.
// WL-KEY-12 -- agents/workshop/keyboard.md
inline std::string expected_text_of(std::int64_t scancode, std::int64_t modifiers) {
    // Ctrl+Space is the one chord a window may also type, as a space: the Windows console does.
    if (scancode == scan::kSpace && modifiers == mod::kCtrl) {
        return " ";
    }
    if ((modifiers & (mod::kCtrl | mod::kAlt | mod::kSuper)) != 0) {
        return std::string();
    }
    if (is_letter_scan(scancode)) {
        const char c = static_cast<char>('a' + (scancode - scan::kA));
        return std::string(1, c);
    }
    if (scancode == scan::kSpace) {
        return " ";
    }
    if ((modifiers & mod::kShift) != 0) {
        return std::string(); // a shifted digit's or punctuation's face is the layout's
    }
    switch (scancode) {
    case scan::k1: return "1";
    case scan::k2: return "2";
    case scan::k3: return "3";
    case scan::k4: return "4";
    case scan::k5: return "5";
    case scan::k6: return "6";
    case scan::k7: return "7";
    case scan::k8: return "8";
    case scan::k9: return "9";
    case scan::k0: return "0";
    case scan::kMinus: return "-";
    case scan::kEquals: return "=";
    case scan::kLeftBracket: return "[";
    case scan::kRightBracket: return "]";
    case scan::kBackslash: return "\\";
    case scan::kSemicolon: return ";";
    case scan::kApostrophe: return "'";
    case scan::kGrave: return "`";
    case scan::kComma: return ",";
    case scan::kPeriod: return ".";
    case scan::kSlash: return "/";
    default: return std::string();
    }
}

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_GESTURE_GRAMMAR_HPP
