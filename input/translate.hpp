// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_INPUT_TRANSLATE_HPP
#define ZENGINE_INPUT_TRANSLATE_HPP

// Native events -> the public shapes, as pure code. No platform headers: the
// Win32 paths take the record fields as plain integers (spelled as local
// constants), so every lane — including the WSL suite that will never run a
// Windows console — pins the Windows translation, and vice versa. The thin
// platform readers in input.cpp only *fetch* native events; everything that
// decides what they *mean* lives here, under test.
//
// THE JOB, in one sentence: preserve what the backend already knew, and claim
// nothing else. Untranslatable events are still DROPPED on both backends —
// silence is the honest answer to "a key you cannot name" — but a fact that
// arrived in the platform's own record is no longer allowed to fall on the
// floor between here and the wire.
//
// WHAT EACH BACKEND GENUINELY KNOWS, source-traced, because the vocabulary is
// only as honest as this table:
//
//   POSIX terminal (raw-mode stdin bytes)
//     key identity   yes, for the bytes terminal_byte_scancode names
//     entered text   YES, and authoritatively: the terminal has already applied
//                    the keyboard layout, so `%` arrives as the byte 0x25. This
//                    is the canonical route Workshop was missing.
//     Shift          INFERRED, letters only, from the byte's case. CapsLock
//                    produces the same byte, so this is "the terminal delivered
//                    the shifted form", not "the Shift key was down". Named as
//                    a limit rather than dressed up.
//     Ctrl           yes, for Ctrl+letter — a terminal sends control byte 1..26
//     Alt            NO. The ESC-prefix convention is byte-identical to Escape
//                    followed by a key, and Escape is load-bearing (it cancels
//                    an edit), so Alt is not claimed here.
//     Super          NO. A terminal cannot report it.
//     pointer        yes, ONCE SOMETHING TURNS REPORTING ON — see the Skin
//                    (surface/skin_tui.hpp), which owns terminal modes. The
//                    reports are SGR (ESC [ < b ; x ; y M/m) and carry position
//                    AND modifiers at event time. Coordinates are 1-based and
//                    are translated to the 0-based public contract here.
//
//   Win32 console (INPUT_RECORD)
//     key identity   yes, wVirtualKeyCode
//     entered text   YES, uChar.UnicodeChar — the layout's own answer, read as
//                    UTF-16 and re-encoded to UTF-8 (surrogate pairs joined).
//     Shift          yes, one bit; the console does not separate left/right.
//     Ctrl / Alt     yes, and separately for left/right — both fold to one
//                    semantic bit, which is the question consumers ask.
//     Super          NO. dwControlKeyState has no GUI bit.
//     pointer        yes, unconditionally. dwMousePosition is on EVERY mouse
//                    record including a pure button transition, and
//                    dwControlKeyState is on it too.
//
// Neither backend reports pixels, so both stamp space::kCells. The field exists
// so that the day one does, it cannot arrive by quietly changing what the
// number meant.

#include "vocabulary.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace zengine::input {

/// One translated native event, exactly one of the public shapes.
using InputEvent =
    std::variant<KeyPressed, KeyReleased, TextEntered, PointerMoved, PointerButton, PointerWheel>;

/// The cheap-and-obvious convenience names (SDL_GetScancodeName's spellings)
/// for the scan:: set; empty for anything else. Convenience only — never
/// authoritative, and the suite pins the scancode as the identity. It is NOT
/// dressed with modifiers any more: V1's "Ctrl+C" spelling existed only because
/// there was nowhere else to put the modifier, and now there is.
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

/// One printable-or-control terminal byte -> scancode, or kUnknown for "this
/// backend cannot name the key". Case is deliberately folded: 'H' and 'h' are
/// the same PHYSICAL key, and the fact that distinguishes them (Shift) is a
/// modifier, not a different key. The character itself travels as text.
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
/// synthesized release (see KeyReleased's contract comment). The modifiers ride
/// BOTH halves, because both halves happened at the same moment.
inline void emit_stroke(std::vector<InputEvent>& out, std::int64_t sc, std::string name,
                        std::int64_t mods) {
    out.push_back(KeyPressed{sc, name, mods});
    out.push_back(KeyReleased{sc, std::move(name), mods});
}

/// One Unicode code point as UTF-8. Total over every std::uint32_t: anything
/// outside the encodable range yields the empty string rather than garbage.
inline std::string utf8_of(std::uint32_t cp) {
    std::string s;
    if (cp < 0x80u) {
        s.push_back(static_cast<char>(cp));
    } else if (cp < 0x800u) {
        s.push_back(static_cast<char>(0xC0u | (cp >> 6)));
        s.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
    } else if (cp < 0x10000u) {
        s.push_back(static_cast<char>(0xE0u | (cp >> 12)));
        s.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
        s.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
    } else if (cp <= 0x10FFFFu) {
        s.push_back(static_cast<char>(0xF0u | (cp >> 18)));
        s.push_back(static_cast<char>(0x80u | ((cp >> 12) & 0x3Fu)));
        s.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
        s.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
    }
    return s;
}

/// How many bytes the UTF-8 character starting with `b` occupies, or 0 if `b`
/// is not a legal lead byte (a stray continuation byte, or one of the two
/// values UTF-8 never uses).
inline std::size_t utf8_len_of_lead(unsigned char b) {
    if (b < 0x80u) {
        return 1;
    }
    if ((b & 0xE0u) == 0xC0u) {
        return 2;
    }
    if ((b & 0xF0u) == 0xE0u) {
        return 3;
    }
    if ((b & 0xF8u) == 0xF0u) {
        return 4;
    }
    return 0;
}

} // namespace detail

// ---- POSIX terminal ----------------------------------------------------------
//
// THE PARSER IS INCREMENTAL AND STATEFUL, and that is not a refinement — it is
// the difference between a pointer that works and a pointer that types. An OS
// read boundary falls wherever the kernel put it, so `\x1b[<0;10;5M` can arrive
// as `\x1b` then `[<0;10;5M`, or split inside the numbers. The batch-local
// parser this replaces would have turned each fragment into ordinary
// keystrokes: Escape, then `[` — which is a key Workshop binds to "narrow the
// workspace". A single click would silently have resized a maker's workspace.
//
// So an incomplete sequence is HELD, never guessed at. The one genuinely
// ambiguous byte is a lone ESC, which is both the Escape key and the start of
// every sequence, and it is resolved by the only clock this package already
// has: the pump. A pending lone ESC survives until a poll that reads NOTHING,
// and that poll releases it as the Escape key. At the 10ms pump beat a maker
// pressing Escape sees it within one beat, and a sequence split across reads is
// rejoined, because the poll that carries its tail is not an empty one.
//
// The honest limit, stated rather than papered over: Escape followed within the
// same instant by `[` is indistinguishable from a CSI introducer, on every
// terminal, and this parser does not distinguish it either.

/// The SGR mouse protocol's own numbers, spelled once. `Cb`'s low two bits pick
/// the button; the rest are flags.
namespace sgr {
inline constexpr std::int64_t kButtonMask = 0x03;
inline constexpr std::int64_t kShift = 0x04;
inline constexpr std::int64_t kAlt = 0x08; // "meta"
inline constexpr std::int64_t kCtrl = 0x10;
inline constexpr std::int64_t kMotion = 0x20;
inline constexpr std::int64_t kWheel = 0x40;
} // namespace sgr

/// The terminal byte parser: one per reader, fed every drained read.
class TerminalParser {
public:
    /// One drained read -> the events that are COMPLETE within it. Bytes that
    /// begin a sequence the read cut short are retained for the next call.
    std::vector<InputEvent> feed(const unsigned char* bytes, std::size_t n) {
        std::vector<InputEvent> out;
        for (std::size_t i = 0; i < n; ++i) {
            const unsigned char b = bytes[i];
            if (resync_) {
                resync_byte(b);
            } else if (!pending_.empty()) {
                escape_byte(b, out);
            } else if (!utf8_.empty()) {
                utf8_byte(b, out);
            } else if (b == 0x1b) {
                pending_.push_back(b);
            } else {
                ground_byte(b, out);
            }
        }
        return out;
    }

    /// A poll that read nothing. This is the parser's only clock, and it does
    /// exactly one thing: release a pending LONE Escape as the Escape key. A
    /// longer fragment is unambiguously an unfinished sequence and keeps
    /// waiting — silence is the correct output for "half a mouse report".
    std::vector<InputEvent> idle() {
        std::vector<InputEvent> out;
        if (pending_.size() == 1 && pending_[0] == 0x1b) {
            detail::emit_stroke(out, scan::kEscape, "Escape", mod::kNone);
            pending_.clear();
        }
        return out;
    }

    /// Sequences abandoned because they could not be what they claimed: an
    /// unknown CSI final byte, a bad SGR field, an overlong fragment, an
    /// invalid UTF-8 lead. Counted rather than silently swallowed, so
    /// "malformed input produces nothing" is an assertion a test can make about
    /// something OTHER than an empty vector.
    std::int64_t malformed() const { return malformed_; }

    /// True while the parser is anywhere other than ground: a sequence is
    /// half-arrived, a character is half-arrived, or a sequence that could not
    /// be what it claimed is being swallowed to its end. Diagnostic only.
    bool mid_sequence() const { return !pending_.empty() || !utf8_.empty() || resync_; }

private:
    /// Longer than any sequence this parser recognises. A stream that keeps
    /// feeding digits is garbage, and garbage must not grow without bound.
    static constexpr std::size_t kMaxPending = 32;
    /// Wide enough for any real terminal, narrow enough that accumulation
    /// cannot overflow. A field beyond it is malformed, not clamped: a
    /// coordinate this large is not a coordinate.
    static constexpr std::int64_t kMaxField = 1000000;

    void drop() {
        ++malformed_;
        pending_.clear();
    }

    /// Swallowing the rest of a sequence that has already proven itself
    /// garbage. A CSI ends at a final byte (0x40..0x7E), so that is where the
    /// parser can rejoin the stream — and NOT one byte sooner.
    ///
    /// Returning to ground in the middle of the garbage is the same defect this
    /// whole parser exists to remove, one level down: an over-long report's tail
    /// digits would arrive as ordinary keystrokes. Measured, before this
    /// existed: a flood of `1`s after `ESC [ <` typed sixty-odd `1`s into
    /// whatever had focus.
    void resync_byte(unsigned char b) {
        if (b >= 0x40 && b <= 0x7e) {
            resync_ = false;
        }
    }

    /// An ordinary byte, outside any sequence.
    void ground_byte(unsigned char b, std::vector<InputEvent>& out) {
        // The named control bytes first: each is a KEY, and none of them is
        // text. Ctrl+H, Ctrl+I, Ctrl+J and Ctrl+M are byte-identical to
        // Backspace, Tab, newline and Return, so the named reading wins — a
        // terminal cannot tell them apart and neither may this parser claim to.
        switch (b) {
        case 0x08:
        case 0x7f: detail::emit_stroke(out, scan::kBackspace, "Backspace", mod::kNone); return;
        case '\t': detail::emit_stroke(out, scan::kTab, "Tab", mod::kNone); return;
        case '\r':
        case '\n': detail::emit_stroke(out, scan::kReturn, "Return", mod::kNone); return;
        default: break;
        }
        // Every other control byte in 1..26 is Ctrl + that letter, and the
        // terminal genuinely reports it — so Ctrl is a MEASURED modifier here,
        // not an inference. Ctrl+C is byte 0x03 and needs no special case any
        // more; it is simply scancode C with kCtrl set.
        if (b >= 1 && b <= 26) {
            const std::int64_t sc = scan::kA + (b - 1);
            detail::emit_stroke(out, sc, scancode_name(sc), mod::kCtrl);
            return;
        }
        if (b < 0x20) {
            return; // a control byte with no key this backend can name
        }
        if (b >= 0x80) {
            const std::size_t want = detail::utf8_len_of_lead(b);
            if (want == 0) {
                ++malformed_; // a stray continuation byte: not a character
                return;
            }
            utf8_want_ = want;
            utf8_.push_back(b);
            return;
        }
        // Printable ASCII: a key transition when this backend can name the key,
        // and ALWAYS the text itself. The two are emitted as one moment, in the
        // order a consumer reads them: down, what it produced, up.
        const std::int64_t sc = detail::terminal_byte_scancode(b);
        const std::int64_t mods = (b >= 'A' && b <= 'Z') ? mod::kShift : mod::kNone;
        if (sc != scan::kUnknown) {
            out.push_back(KeyPressed{sc, scancode_name(sc), mods});
        }
        out.push_back(TextEntered{std::string(1, static_cast<char>(b))});
        if (sc != scan::kUnknown) {
            out.push_back(KeyReleased{sc, scancode_name(sc), mods});
        }
    }

    /// A continuation byte of a multi-byte character. A byte that is not a
    /// continuation abandons the partial character and is re-read from ground,
    /// so a truncated character cannot eat the key that follows it.
    void utf8_byte(unsigned char b, std::vector<InputEvent>& out) {
        if ((b & 0xC0u) != 0x80u) {
            ++malformed_;
            utf8_.clear();
            if (b == 0x1b) {
                pending_.push_back(b);
            } else {
                ground_byte(b, out);
            }
            return;
        }
        utf8_.push_back(b);
        if (utf8_.size() < utf8_want_) {
            return; // still arriving — possibly across a read boundary
        }
        out.push_back(TextEntered{std::string(utf8_.begin(), utf8_.end())});
        utf8_.clear();
    }

    /// A byte inside a pending escape sequence.
    void escape_byte(unsigned char b, std::vector<InputEvent>& out) {
        if (pending_.size() >= kMaxPending) {
            drop();
            resync_ = true; // and swallow to the sequence's end, not to here
            resync_byte(b);
            return;
        }
        if (pending_.size() == 1) { // just ESC so far
            if (b == '[') {
                pending_.push_back(b);
                return;
            }
            // Not a CSI: the ESC was the Escape key, and this byte is its own
            // keystroke. NOT read as Alt+key — see the backend table above.
            detail::emit_stroke(out, scan::kEscape, "Escape", mod::kNone);
            pending_.clear();
            if (b == 0x1b) {
                pending_.push_back(b);
            } else {
                ground_byte(b, out);
            }
            return;
        }
        if (pending_.size() == 2) { // ESC [
            switch (b) {
            case 'A': arrow(out, scan::kUp); return;
            case 'B': arrow(out, scan::kDown); return;
            case 'C': arrow(out, scan::kRight); return;
            case 'D': arrow(out, scan::kLeft); return;
            case '<': pending_.push_back(b); return; // an SGR mouse report opens
            default: break;
            }
            if ((b >= '0' && b <= '9') || b == ';' || b == '?') {
                pending_.push_back(b); // some other CSI: consume it whole
                return;
            }
            drop(); // an unrecognised CSI final byte
            return;
        }
        // Inside the body. Either an SGR mouse report (pending_[2] == '<') or
        // another CSI we are consuming only so that it cannot leak.
        const bool mouse = pending_[2] == '<';
        if ((b >= '0' && b <= '9') || b == ';') {
            pending_.push_back(b);
            return;
        }
        if (mouse && (b == 'M' || b == 'm')) {
            sgr_report(b == 'M', out);
            return;
        }
        if (b >= 0x40 && b <= 0x7e) { // any other CSI final byte: consumed, dropped
            drop();
            return;
        }
        drop();
    }

    void arrow(std::vector<InputEvent>& out, std::int64_t sc) {
        detail::emit_stroke(out, sc, scancode_name(sc), mod::kNone);
        pending_.clear();
    }

    /// `ESC [ < Cb ; Cx ; Cy M|m` — decoded into the moment it describes.
    void sgr_report(bool press, std::vector<InputEvent>& out) {
        std::int64_t field[3] = {0, 0, 0};
        int at = 0;
        bool digits = false;
        for (std::size_t i = 3; i < pending_.size(); ++i) {
            const unsigned char c = pending_[i];
            if (c == ';') {
                if (!digits || at == 2) {
                    drop();
                    return;
                }
                ++at;
                digits = false;
                continue;
            }
            const std::int64_t d = c - '0';
            if (field[at] > (kMaxField - d) / 10) { // bounded before it grows
                drop();
                return;
            }
            field[at] = field[at] * 10 + d;
            digits = true;
        }
        if (at != 2 || !digits) {
            drop(); // an SGR report is exactly three fields
            return;
        }
        const std::int64_t cb = field[0];
        // The terminal counts from 1; the public contract counts from 0, which
        // is what the Win32 console already reports and what a canvas already
        // means.
        const std::int64_t x = field[1] - 1;
        const std::int64_t y = field[2] - 1;
        std::int64_t mods = mod::kNone;
        if ((cb & sgr::kShift) != 0) {
            mods |= mod::kShift;
        }
        if ((cb & sgr::kAlt) != 0) {
            mods |= mod::kAlt;
        }
        if ((cb & sgr::kCtrl) != 0) {
            mods |= mod::kCtrl;
        }
        const std::int64_t low = cb & sgr::kButtonMask;
        if ((cb & sgr::kWheel) != 0) {
            const double up = low == 0 ? 1.0 : 0.0;
            const double down = low == 1 ? -1.0 : 0.0;
            const double left = low == 2 ? -1.0 : 0.0;
            const double right = low == 3 ? 1.0 : 0.0;
            out.push_back(PointerWheel{left + right, up + down, x, y, space::kCells, mods});
            advance(x, y);
            pending_.clear();
            return;
        }
        if ((cb & sgr::kMotion) != 0) {
            out.push_back(moved(x, y, mods));
            pending_.clear();
            return;
        }
        if (low == 3) {
            drop(); // "no button" is not a transition
            return;
        }
        out.push_back(PointerButton{low + 1, press, x, y, space::kCells, mods});
        advance(x, y);
        pending_.clear();
    }

    /// A motion event, with the delta this parser owns. The terminal reports
    /// only a position, so Input derives the transition centrally — once, here —
    /// rather than leaving every consumer to remember the last one.
    PointerMoved moved(std::int64_t x, std::int64_t y, std::int64_t mods) {
        const std::int64_t dx = has_pos_ ? x - last_x_ : 0;
        const std::int64_t dy = has_pos_ ? y - last_y_ : 0;
        advance(x, y);
        return PointerMoved{x, y, dx, dy, space::kCells, mods};
    }

    /// EVERY report that carries a position advances the tracker, buttons and
    /// wheels included. Advancing only on motion is what made V1's Win32 deltas
    /// measure from before the press.
    void advance(std::int64_t x, std::int64_t y) {
        has_pos_ = true;
        last_x_ = x;
        last_y_ = y;
    }

    std::vector<unsigned char> pending_;
    std::vector<unsigned char> utf8_;
    std::size_t utf8_want_ = 0;
    bool resync_ = false;
    std::int64_t malformed_ = 0;
    bool has_pos_ = false;
    std::int64_t last_x_ = 0;
    std::int64_t last_y_ = 0;
};

/// One SELF-CONTAINED read: fed and then idled, which is what "these bytes are
/// the whole of what arrived" means. Convenience for callers and for tests
/// about complete input; anything asking about read boundaries must drive a
/// TerminalParser directly, because that is the question.
inline std::vector<InputEvent> terminal_bytes_to_events(const unsigned char* bytes,
                                                        std::size_t n) {
    TerminalParser p;
    std::vector<InputEvent> out = p.feed(bytes, n);
    for (InputEvent& e : p.idle()) {
        out.push_back(std::move(e));
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

// dwControlKeyState bits. Left and right are separate keys and one modifier.
inline constexpr std::uint32_t kRightAlt = 0x0001;  // RIGHT_ALT_PRESSED
inline constexpr std::uint32_t kLeftAlt = 0x0002;   // LEFT_ALT_PRESSED
inline constexpr std::uint32_t kRightCtrl = 0x0004; // RIGHT_CTRL_PRESSED
inline constexpr std::uint32_t kLeftCtrl = 0x0008;  // LEFT_CTRL_PRESSED
inline constexpr std::uint32_t kShift = 0x0010;     // SHIFT_PRESSED

inline constexpr std::uint16_t kVkBack = 0x08;
inline constexpr std::uint16_t kVkTab = 0x09;
inline constexpr std::uint16_t kVkReturn = 0x0D;
inline constexpr std::uint16_t kVkEscape = 0x1B;
inline constexpr std::uint16_t kVkSpace = 0x20;
inline constexpr std::uint16_t kVkLeft = 0x25;
inline constexpr std::uint16_t kVkUp = 0x26;
inline constexpr std::uint16_t kVkRight = 0x27;
inline constexpr std::uint16_t kVkDown = 0x28;
// The OEM punctuation keys, on the US layout the console reports them for.
// V1 dropped every one of these, so `[` and `]` — the two keys Workshop binds
// to its workspace width — were simply not deliverable on the Windows lane.
inline constexpr std::uint16_t kVkOem1 = 0xBA;      // ;
inline constexpr std::uint16_t kVkOemPlus = 0xBB;   // =
inline constexpr std::uint16_t kVkOemComma = 0xBC;  // ,
inline constexpr std::uint16_t kVkOemMinus = 0xBD;  // -
inline constexpr std::uint16_t kVkOemPeriod = 0xBE; // .
inline constexpr std::uint16_t kVkOem2 = 0xBF;      // /
inline constexpr std::uint16_t kVkOem3 = 0xC0;      // `
inline constexpr std::uint16_t kVkOem4 = 0xDB;      // [
inline constexpr std::uint16_t kVkOem5 = 0xDC;      // backslash
inline constexpr std::uint16_t kVkOem6 = 0xDD;      // ]
inline constexpr std::uint16_t kVkOem7 = 0xDE;      // '

/// A key VK -> scancode, or kUnknown to drop. The modifier keys themselves land
/// here: a consumer learns Shift is held from `modifiers` on the key that was
/// actually typed, which is the fact it wants, and not from a transition on the
/// Shift key itself.
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
    case kVkOem1: return scan::kSemicolon;
    case kVkOemPlus: return scan::kEquals;
    case kVkOemComma: return scan::kComma;
    case kVkOemMinus: return scan::kMinus;
    case kVkOemPeriod: return scan::kPeriod;
    case kVkOem2: return scan::kSlash;
    case kVkOem3: return scan::kGrave;
    case kVkOem4: return scan::kLeftBracket;
    case kVkOem5: return scan::kBackslash;
    case kVkOem6: return scan::kRightBracket;
    case kVkOem7: return scan::kApostrophe;
    default: return scan::kUnknown;
    }
}

/// dwControlKeyState -> the semantic modifier bits. Left and right fold; there
/// is no Super/GUI bit to read, so kSuper is never set on this backend.
inline std::int64_t modifiers_of(std::uint32_t control_key_state) {
    std::int64_t m = mod::kNone;
    if ((control_key_state & kShift) != 0) {
        m |= mod::kShift;
    }
    if ((control_key_state & (kLeftCtrl | kRightCtrl)) != 0) {
        m |= mod::kCtrl;
    }
    if ((control_key_state & (kLeftAlt | kRightAlt)) != 0) {
        m |= mod::kAlt;
    }
    return m;
}
} // namespace win32

/// What the Win32 key path remembers between records: a high surrogate waiting
/// for its partner, so a character outside the BMP arrives as one character
/// rather than two broken halves.
struct KeyTrack {
    std::uint16_t pending_high = 0;
};

/// Win32 console KEY_EVENT -> the key transition and, when the layout produced
/// one, the text.
///
/// `ch` is uChar.UnicodeChar — the console's own answer to "what did this
/// keystroke type", already through the active keyboard layout. That is the
/// route by which `%` reaches an application without anyone computing
/// `Shift+5`. A control character is never text: Ctrl+C's uChar is 0x03 and is
/// excluded by the printability test, so it stays a key with kCtrl set.
inline std::vector<InputEvent> win32_key_to_events(KeyTrack& track, std::uint16_t vk,
                                                   std::uint32_t ch, bool down,
                                                   std::uint32_t control_key_state) {
    std::vector<InputEvent> out;
    const std::int64_t mods = win32::modifiers_of(control_key_state);
    const std::int64_t sc = win32::vk_scancode(vk);
    if (sc != scan::kUnknown) {
        if (down) {
            out.push_back(KeyPressed{sc, scancode_name(sc), mods});
        } else {
            out.push_back(KeyReleased{sc, scancode_name(sc), mods});
        }
    }
    if (!down) {
        return out; // a release types nothing
    }
    std::uint32_t cp = ch & 0xFFFFu;
    if (cp >= 0xD800u && cp <= 0xDBFFu) {
        track.pending_high = static_cast<std::uint16_t>(cp);
        return out; // half a character: wait for the partner
    }
    if (cp >= 0xDC00u && cp <= 0xDFFFu) {
        if (track.pending_high == 0) {
            return out; // an orphan low surrogate is not a character
        }
        cp = 0x10000u + ((static_cast<std::uint32_t>(track.pending_high) - 0xD800u) << 10) +
             (cp - 0xDC00u);
        track.pending_high = 0;
    } else {
        track.pending_high = 0;
    }
    if (cp >= 0x20u && cp != 0x7Fu) {
        out.push_back(TextEntered{detail::utf8_of(cp)});
    }
    return out;
}

/// What the Win32 mouse path remembers between events: the previous position
/// (for dx/dy) and the previous button bits (transitions, not states, are the
/// vocabulary).
struct PointerTrack {
    bool has_pos = false;
    std::int64_t x = 0;
    std::int64_t y = 0;
    std::uint32_t buttons = 0;
};

/// Win32 console MOUSE_EVENT -> events. Position units are character cells (the
/// console IS the surface), stamped space::kCells. Wheel deltas arrive as the
/// signed high word of the button state, +120 per notch; the wire carries SDL's
/// +1.0-per-notch.
///
/// The record's dwMousePosition and dwControlKeyState are on EVERY mouse event
/// including a pure button transition, so every message below carries the
/// position and the modifiers that were true when it happened. Nothing is left
/// for a consumer to join up afterwards.
inline std::vector<InputEvent> win32_mouse_to_events(PointerTrack& track, std::int64_t x,
                                                     std::int64_t y, std::uint32_t button_state,
                                                     std::uint32_t flags,
                                                     std::uint32_t control_key_state) {
    std::vector<InputEvent> out;
    const std::int64_t mods = win32::modifiers_of(control_key_state);
    const auto advance = [&track, x, y] {
        track.has_pos = true;
        track.x = x;
        track.y = y;
    };
    if ((flags & win32::kMouseWheeled) != 0 || (flags & win32::kMouseHWheeled) != 0) {
        const double notches =
            static_cast<std::int16_t>((button_state >> 16) & 0xFFFFu) / 120.0;
        if ((flags & win32::kMouseWheeled) != 0) {
            out.push_back(PointerWheel{0.0, notches, x, y, space::kCells, mods});
        } else {
            out.push_back(PointerWheel{notches, 0.0, x, y, space::kCells, mods});
        }
        advance();
        return out; // a wheel record's low word repeats held buttons; no diff
    }
    if ((flags & win32::kMouseMoved) != 0) {
        const std::int64_t dx = track.has_pos ? x - track.x : 0;
        const std::int64_t dy = track.has_pos ? y - track.y : 0;
        out.push_back(PointerMoved{x, y, dx, dy, space::kCells, mods});
        advance();
    }
    const std::uint32_t now =
        button_state & (win32::kButtonLeft | win32::kButtonRight | win32::kButtonMiddle);
    const std::uint32_t changed = now ^ track.buttons;
    if ((changed & win32::kButtonLeft) != 0) {
        out.push_back(
            PointerButton{1, (now & win32::kButtonLeft) != 0, x, y, space::kCells, mods});
    }
    if ((changed & win32::kButtonMiddle) != 0) {
        out.push_back(
            PointerButton{2, (now & win32::kButtonMiddle) != 0, x, y, space::kCells, mods});
    }
    if ((changed & win32::kButtonRight) != 0) {
        out.push_back(
            PointerButton{3, (now & win32::kButtonRight) != 0, x, y, space::kCells, mods});
    }
    track.buttons = now;
    // A button-only record carries a position too, and forgetting it is what
    // made the next motion's delta measure from before the press.
    advance();
    return out;
}

} // namespace zengine::input

#endif // ZENGINE_INPUT_TRANSLATE_HPP
