// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_INPUT_TRANSLATE_HPP
#define ZENGINE_INPUT_TRANSLATE_HPP

// Native events -> the locked shapes, as pure functions. No platform headers:
// the Win32 paths take the record fields as plain integers (spelled as local
// constants), so every lane — including the WSL suite that will never run a
// Windows console — pins the Windows translation, and vice versa. The thin
// platform readers in input.cpp only *fetch* native events; everything that
// decides what they *mean* lives here, under test.
//
// Untranslatable events are DROPPED, on both backends, by design: V1 has no
// modifier vocabulary and no use for "a key you don't know happened", so
// nothing emits scancode 0 today. The kUnknown door stays open for a future
// backend that wants to say it; a consumer must still expect it.

#include "vocabulary.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace zengine::input {

/// One translated native event, exactly one of the five locked shapes.
using InputEvent = std::variant<KeyPressed, KeyReleased, MouseButton, MouseMoved, MouseWheel>;

/// The cheap-and-obvious convenience names (SDL_GetScancodeName's spellings)
/// for the scan:: set; empty for anything else. Convenience only — never
/// authoritative, and the suite pins the scancode as the identity.
inline std::string scancode_name(std::int64_t sc) {
    if (sc >= scan::kA && sc <= scan::kZ) {
        return std::string(1, static_cast<char>('A' + (sc - scan::kA)));
    }
    if (sc >= scan::k1 && sc <= scan::k9) {
        return std::string(1, static_cast<char>('1' + (sc - scan::k1)));
    }
    switch (sc) {
    case scan::k0: return "0";
    case scan::kReturn: return "Return";
    case scan::kEscape: return "Escape";
    case scan::kBackspace: return "Backspace";
    case scan::kTab: return "Tab";
    case scan::kSpace: return "Space";
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
    case scan::kRight: return "Right";
    case scan::kLeft: return "Left";
    case scan::kDown: return "Down";
    case scan::kUp: return "Up";
    default: return "";
    }
}

namespace detail {

/// One printable-or-control terminal byte -> scancode, or kUnknown to drop.
/// (ESC and its arrow sequences are the caller's; they span bytes.)
inline std::int64_t terminal_byte_scancode(unsigned char b) {
    if (b >= 'a' && b <= 'z') {
        return scan::kA + (b - 'a');
    }
    if (b >= 'A' && b <= 'Z') {
        return scan::kA + (b - 'A');
    }
    if (b >= '1' && b <= '9') {
        return scan::k1 + (b - '1');
    }
    switch (b) {
    case '0': return scan::k0;
    case '\r':
    case '\n': return scan::kReturn;
    case '\t': return scan::kTab;
    case ' ': return scan::kSpace;
    case 0x08:
    case 0x7f: return scan::kBackspace;
    case '-': return scan::kMinus;
    case '=': return scan::kEquals;
    case '[': return scan::kLeftBracket;
    case ']': return scan::kRightBracket;
    case '\\': return scan::kBackslash;
    case ';': return scan::kSemicolon;
    case '\'': return scan::kApostrophe;
    case '`': return scan::kGrave;
    case ',': return scan::kComma;
    case '.': return scan::kPeriod;
    case '/': return scan::kSlash;
    default: return scan::kUnknown;
    }
}

/// A terminal reports completed keystrokes, not transitions, so the faithful
/// projection of one native event is a press immediately followed by a
/// synthesized release (see KeyReleased's contract comment).
inline void emit_stroke(std::vector<InputEvent>& out, std::int64_t sc, std::string name) {
    out.push_back(KeyPressed{sc, name});
    out.push_back(KeyReleased{sc, std::move(name)});
}

} // namespace detail

/// Terminal backend: one drained batch of raw-mode stdin bytes -> events.
/// The parse is batch-local (the same stance the snake host always took): an
/// ESC [ A/B/C/D arrow sequence is recognized only whole within the batch —
/// raw-mode arrow bytes arrive together — and an ESC not opening one is the
/// Escape key itself. Auto-repeat arrives as repeated bytes and therefore as
/// repeated strokes, which is the SDL stance on repeats.
inline std::vector<InputEvent> terminal_bytes_to_events(const unsigned char* bytes,
                                                        std::size_t n) {
    std::vector<InputEvent> out;
    for (std::size_t i = 0; i < n; ++i) {
        const unsigned char b = bytes[i];
        if (b == 0x03) { // ETX is uniquely Ctrl+C on a terminal; the modifier
                         // survives only in the convenience name (V1 has no
                         // modifier vocabulary — the phase's named edge). The
                         // spelling is a TEMPORARY cross-backend contract the
                         // snake host branches on, pinned as an expiring debt
                         // in the input suite; keep it identical to the win32
                         // path below until modifiers retire both.
            detail::emit_stroke(out, scan::kC, "Ctrl+C");
            continue;
        }
        if (b == 0x1b) {
            if (i + 2 < n && bytes[i + 1] == '[') {
                std::int64_t sc = scan::kUnknown;
                switch (bytes[i + 2]) {
                case 'A': sc = scan::kUp; break;
                case 'B': sc = scan::kDown; break;
                case 'C': sc = scan::kRight; break;
                case 'D': sc = scan::kLeft; break;
                default: break;
                }
                if (sc != scan::kUnknown) {
                    detail::emit_stroke(out, sc, scancode_name(sc));
                    i += 2;
                    continue;
                }
            }
            detail::emit_stroke(out, scan::kEscape, "Escape");
            continue;
        }
        const std::int64_t sc = detail::terminal_byte_scancode(b);
        if (sc != scan::kUnknown) {
            detail::emit_stroke(out, sc, scancode_name(sc));
        }
    }
    return out;
}

// ---- Win32 console -----------------------------------------------------------
// The reader hands these functions the INPUT_RECORD fields as plain integers.
// The constants below are the Win32 values, spelled locally so this header
// stays platform-free (and the suite pins them against the SDK's meanings).

namespace win32 {
inline constexpr std::uint32_t kMouseMoved = 0x0001;    // MOUSE_MOVED
inline constexpr std::uint32_t kMouseWheeled = 0x0004;  // MOUSE_WHEELED
inline constexpr std::uint32_t kMouseHWheeled = 0x0008; // MOUSE_HWHEELED
inline constexpr std::uint32_t kButtonLeft = 0x0001;    // FROM_LEFT_1ST_BUTTON_PRESSED
inline constexpr std::uint32_t kButtonRight = 0x0002;   // RIGHTMOST_BUTTON_PRESSED
inline constexpr std::uint32_t kButtonMiddle = 0x0004;  // FROM_LEFT_2ND_BUTTON_PRESSED
inline constexpr std::uint16_t kVkBack = 0x08;
inline constexpr std::uint16_t kVkTab = 0x09;
inline constexpr std::uint16_t kVkReturn = 0x0D;
inline constexpr std::uint16_t kVkEscape = 0x1B;
inline constexpr std::uint16_t kVkSpace = 0x20;
inline constexpr std::uint16_t kVkLeft = 0x25;
inline constexpr std::uint16_t kVkUp = 0x26;
inline constexpr std::uint16_t kVkRight = 0x27;
inline constexpr std::uint16_t kVkDown = 0x28;

/// A key VK -> scancode, or kUnknown to drop (modifiers land here in V1).
inline std::int64_t vk_scancode(std::uint16_t vk) {
    if (vk >= 'A' && vk <= 'Z') { // VK for letters IS the uppercase ASCII
        return scan::kA + (vk - 'A');
    }
    if (vk >= '1' && vk <= '9') {
        return scan::k1 + (vk - '1');
    }
    switch (vk) {
    case '0': return scan::k0;
    case kVkBack: return scan::kBackspace;
    case kVkTab: return scan::kTab;
    case kVkReturn: return scan::kReturn;
    case kVkEscape: return scan::kEscape;
    case kVkSpace: return scan::kSpace;
    case kVkLeft: return scan::kLeft;
    case kVkUp: return scan::kUp;
    case kVkRight: return scan::kRight;
    case kVkDown: return scan::kDown;
    default: return scan::kUnknown;
    }
}
} // namespace win32

/// Win32 console KEY_EVENT -> at most one event. Real releases exist here
/// (bKeyDown false), so nothing is synthesized. `ctrl` is whether a Ctrl key
/// is held (dwControlKeyState), used only to dress the convenience name.
inline std::vector<InputEvent> win32_key_to_events(std::uint16_t vk, bool down, bool ctrl) {
    std::vector<InputEvent> out;
    const std::int64_t sc = win32::vk_scancode(vk);
    if (sc == scan::kUnknown) {
        return out;
    }
    std::string name = scancode_name(sc);
    if (ctrl && sc == scan::kC) {
        name = "Ctrl+C"; // the one dressed name the snake host relies on —
                         // byte-identical to the terminal path above, pinned
                         // as a temporary contract in the input suite
    }
    if (down) {
        out.push_back(KeyPressed{sc, std::move(name)});
    } else {
        out.push_back(KeyReleased{sc, std::move(name)});
    }
    return out;
}

/// What the Win32 mouse path remembers between events: the previous position
/// (for dx/dy) and the previous button bits (transitions, not states, are the
/// locked vocabulary).
struct MouseTrack {
    bool has_pos = false;
    std::int64_t x = 0;
    std::int64_t y = 0;
    std::uint32_t buttons = 0;
};

/// Win32 console MOUSE_EVENT -> events. Position units are character cells
/// (the console IS the surface; see MouseMoved's contract comment). Wheel
/// deltas arrive as the signed high word of the button state, +120 per notch;
/// the wire carries SDL's +1.0-per-notch.
inline std::vector<InputEvent> win32_mouse_to_events(MouseTrack& track, std::int64_t x,
                                                     std::int64_t y, std::uint32_t button_state,
                                                     std::uint32_t flags) {
    std::vector<InputEvent> out;
    if ((flags & win32::kMouseWheeled) != 0 || (flags & win32::kMouseHWheeled) != 0) {
        const double notches =
            static_cast<std::int16_t>((button_state >> 16) & 0xFFFFu) / 120.0;
        if ((flags & win32::kMouseWheeled) != 0) {
            out.push_back(MouseWheel{0.0, notches});
        } else {
            out.push_back(MouseWheel{notches, 0.0});
        }
        return out; // a wheel record's low word repeats held buttons; no diff
    }
    if ((flags & win32::kMouseMoved) != 0) {
        const double dx = track.has_pos ? static_cast<double>(x - track.x) : 0.0;
        const double dy = track.has_pos ? static_cast<double>(y - track.y) : 0.0;
        out.push_back(
            MouseMoved{static_cast<double>(x), static_cast<double>(y), dx, dy});
        track.has_pos = true;
        track.x = x;
        track.y = y;
    }
    const std::uint32_t now =
        button_state & (win32::kButtonLeft | win32::kButtonRight | win32::kButtonMiddle);
    const std::uint32_t changed = now ^ track.buttons;
    if ((changed & win32::kButtonLeft) != 0) {
        out.push_back(MouseButton{1, (now & win32::kButtonLeft) != 0});
    }
    if ((changed & win32::kButtonMiddle) != 0) {
        out.push_back(MouseButton{2, (now & win32::kButtonMiddle) != 0});
    }
    if ((changed & win32::kButtonRight) != 0) {
        out.push_back(MouseButton{3, (now & win32::kButtonRight) != 0});
    }
    track.buttons = now;
    return out;
}

} // namespace zengine::input

#endif // ZENGINE_INPUT_TRANSLATE_HPP
