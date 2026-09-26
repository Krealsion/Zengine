// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_INPUT_TRANSLATE_SDL_HPP
#define ZENGINE_INPUT_TRANSLATE_SDL_HPP

// SDL native events -> the public shapes, as pure code with no SDL headers: the reader
// (input_sdl.cpp) passes event fields as plain values and SDL's constants are spelled locally,
// so the lane that never opens a window pins the whole translation, and an SDL-gated case pins
// the constants against the real headers. Its own header because it is the one file in Input
// that includes the Surface vocabulary (`SdlEvent`).
// Surface law: agents/surface.md

// What SDL 3 knows (source-traced against 3.4.12): the key identity is SDL's scancode, the
// wire's own space, so it passes through; entered text arrives committed (layout, dead keys,
// IME); Shift, Ctrl, Alt and Super ride each key event; a repeat is an ordinary press; pointer
// events state their position, and motion its own delta, in float window pixels. Pointer
// events carry no modifiers (`SDL_GetModState` would read a later instant), so they say
// `mod::kNone`: a shift-click is not expressible here.

#include "translate.hpp" // scancode_name — the courtesy names, shared with every backend
#include "vocabulary.hpp"

#include "surface/vocabulary.hpp"

#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <variant>
#include <vector>

namespace zengine::input {

/// One translated SDL event: the Input variant plus one, `surface::SurfaceCloseRequested` -- a
/// window fact on SDL's one process-global queue, carried as the surface fact it is rather than
/// as a fake key. The weave derives its Emit set from this variant, so only the SDL reader
/// declares the extra shape. No clipboard shape: clipboard read follows paste intent, answered
/// by the Medium (`ClipboardTextRequested`, surface/vocabulary.hpp), and this reader ignores
/// the clipboard events.
using SdlEvent = std::variant<KeyPressed, KeyReleased, TextEntered, PointerMoved, PointerButton,
                              PointerWheel, zengine::surface::SurfaceCloseRequested>;

/// SDL's own numbers, spelled locally so this header stays SDL-free — and
/// pinned against the real headers by an SDL-gated case in the suite, exactly
/// as `scan::` is. A typo here is a red test, not a silently different world.
namespace sdl {

// SDL_EventType (SDL_events.h). The window events are a contiguous block from
// SDL_EVENT_WINDOW_SHOWN = 0x202; CLOSE_REQUESTED is the fifteenth of them.
inline constexpr std::uint32_t kEventQuit = 0x100;
inline constexpr std::uint32_t kEventWindowCloseRequested = 0x210;
inline constexpr std::uint32_t kEventKeyDown = 0x300;
inline constexpr std::uint32_t kEventKeyUp = 0x301;
inline constexpr std::uint32_t kEventTextEditing = 0x302;
inline constexpr std::uint32_t kEventTextInput = 0x303;
inline constexpr std::uint32_t kEventMouseMotion = 0x400;
inline constexpr std::uint32_t kEventMouseButtonDown = 0x401;
inline constexpr std::uint32_t kEventMouseButtonUp = 0x402;
inline constexpr std::uint32_t kEventMouseWheel = 0x403;

// SDL_Keymod (SDL_keycode.h). Left and right are separate keys and one
// modifier, the same fold the Win32 path already performs.
inline constexpr std::uint16_t kModLShift = 0x0001;
inline constexpr std::uint16_t kModRShift = 0x0002;
inline constexpr std::uint16_t kModLCtrl = 0x0040;
inline constexpr std::uint16_t kModRCtrl = 0x0080;
inline constexpr std::uint16_t kModLAlt = 0x0100;
inline constexpr std::uint16_t kModRAlt = 0x0200;
inline constexpr std::uint16_t kModLGui = 0x0400;
inline constexpr std::uint16_t kModRGui = 0x0800;

// SDL_BUTTON_* (SDL_mouse.h). SDL numbers left/middle/right 1/2/3, which is
// EXACTLY what PointerButton::button already meant, so the mapping is the
// identity and no table is written for it. X1 (4) and X2 (5) are outside the
// vocabulary's stated set — see `sdl_mouse_button_to_events`.
inline constexpr std::int64_t kButtonLeft = 1;
inline constexpr std::int64_t kButtonMiddle = 2;
inline constexpr std::int64_t kButtonRight = 3;

// SDL_MouseWheelDirection.
inline constexpr std::uint32_t kWheelNormal = 0;
inline constexpr std::uint32_t kWheelFlipped = 1;

} // namespace sdl

/// One SDL float coordinate as an int64 pixel. For the window the Surface package creates -- no
/// SDL_WINDOW_HIGH_PIXEL_DENSITY, logical presentation or render scale; resizable is fine -- SDL
/// delivers integral window coordinates 1:1 with framebuffer pixels, and this is exact. Outside
/// that SDL can report a fraction, and this floors: 3.7 is inside pixel 3. Total over every
/// float: NaN and values outside int64 saturate rather than invoke undefined behaviour.
inline std::int64_t sdl_pixel(float v) noexcept {
    constexpr std::int64_t kMax = (std::numeric_limits<std::int64_t>::max)();
    constexpr std::int64_t kMin = (std::numeric_limits<std::int64_t>::min)();
    // 2^63 exactly, as a double: the first value int64 cannot hold.
    constexpr double kLimit = 9223372036854775808.0;
    const double d = std::floor(static_cast<double>(v));
    if (!(d >= -kLimit)) {
        return kMin; // includes NaN, which is not a coordinate
    }
    if (!(d < kLimit)) {
        return kMax;
    }
    return static_cast<std::int64_t>(d);
}

/// SDL_Keymod -> the semantic modifier bits. Left and right fold; SDL is the
/// one backend that can honestly set kSuper. Lock states (Num/Caps/Scroll) and
/// AltGr's MODE bit are deliberately NOT mapped: the vocabulary has four
/// semantic modifiers and "CapsLock is on" is not one of them.
inline constexpr std::int64_t sdl_modifiers_of(std::uint16_t keymod) noexcept {
    std::int64_t m = mod::kNone;
    if ((keymod & (sdl::kModLShift | sdl::kModRShift)) != 0) {
        m |= mod::kShift;
    }
    if ((keymod & (sdl::kModLCtrl | sdl::kModRCtrl)) != 0) {
        m |= mod::kCtrl;
    }
    if ((keymod & (sdl::kModLAlt | sdl::kModRAlt)) != 0) {
        m |= mod::kAlt;
    }
    if ((keymod & (sdl::kModLGui | sdl::kModRGui)) != 0) {
        m |= mod::kSuper;
    }
    return m;
}

/// SDL_EVENT_KEY_DOWN / SDL_EVENT_KEY_UP -> the key transition. The scancode passes through: it
/// is already the wire's identity, so a table would be a copy of the identity function that
/// could drift, and a key with no courtesy name has an empty `name`. A repeat is a press (the
/// vocabulary has no repeat flag); the flag is a parameter so that decision is made here.
inline std::vector<SdlEvent> sdl_key_to_events(std::int64_t scancode, std::uint16_t keymod,
                                               bool down, bool /*repeat*/) {
    std::vector<SdlEvent> out;
    const std::int64_t mods = sdl_modifiers_of(keymod);
    if (down) {
        out.push_back(KeyPressed{scancode, scancode_name(scancode), mods});
    } else {
        out.push_back(KeyReleased{scancode, scancode_name(scancode), mods});
    }
    return out;
}

/// SDL_EVENT_TEXT_INPUT -> the text.
///
/// SDL's `text` is UTF-8 that the platform has already committed — layout, dead
/// keys and IME all applied — so there is nothing to decode and nothing to
/// guess. An empty string yields no message: an empty TextEntered is not a
/// thing that happened.
inline std::vector<SdlEvent> sdl_text_to_events(const char* text) {
    std::vector<SdlEvent> out;
    if (text != nullptr && text[0] != '\0') {
        out.push_back(TextEntered{std::string(text)});
    }
    return out;
}

/// SDL_EVENT_MOUSE_MOTION -> the motion, with SDL'S OWN delta.
///
/// The terminal and Win32 paths derive dx/dy by remembering the last position,
/// because their platforms report only a position. SDL states xrel/yrel on the
/// event itself, so this backend keeps no tracker at all — the fact is the
/// platform's and is preserved rather than recomputed.
inline std::vector<SdlEvent> sdl_mouse_motion_to_events(float x, float y, float xrel,
                                                        float yrel) {
    std::vector<SdlEvent> out;
    out.push_back(PointerMoved{sdl_pixel(x), sdl_pixel(y), sdl_pixel(xrel), sdl_pixel(yrel),
                               space::kPixels, mod::kNone});
    return out;
}

/// SDL_EVENT_MOUSE_BUTTON_DOWN / _UP -> the transition and where it happened, from the position
/// SDL put on this event. An unsupported button produces nothing: X1 and X2 (4, 5) are outside
/// the three `PointerButton::button` states, and mapping a thumb button to Left would report a
/// click that did not happen.
inline std::vector<SdlEvent> sdl_mouse_button_to_events(std::int64_t button, bool down, float x,
                                                        float y) {
    std::vector<SdlEvent> out;
    if (button != sdl::kButtonLeft && button != sdl::kButtonMiddle &&
        button != sdl::kButtonRight) {
        return out;
    }
    out.push_back(
        PointerButton{button, down, sdl_pixel(x), sdl_pixel(y), space::kPixels, mod::kNone});
    return out;
}

/// SDL_EVENT_MOUSE_WHEEL -> the wheel. The deltas stay fractional: `PointerWheel::dx/dy` are
/// doubles because a high-resolution wheel reports fractions of a detent. Flipped values
/// (SDL_MOUSEWHEEL_FLIPPED) are inverted back to the wire's convention, +1 per notch away from
/// the user, so a consumer need not know how a maker's trackpad is configured.
inline std::vector<SdlEvent> sdl_mouse_wheel_to_events(float dx, float dy,
                                                       std::uint32_t direction, float mouse_x,
                                                       float mouse_y) {
    std::vector<SdlEvent> out;
    const double sign = direction == sdl::kWheelFlipped ? -1.0 : 1.0;
    out.push_back(PointerWheel{static_cast<double>(dx) * sign, static_cast<double>(dy) * sign,
                               sdl_pixel(mouse_x), sdl_pixel(mouse_y), space::kPixels,
                               mod::kNone});
    return out;
}

/// SDL_EVENT_QUIT / SDL_EVENT_WINDOW_CLOSE_REQUESTED -> the lifecycle fact. Both: some platforms
/// send an application quit and never the window event, and quitting is idempotent, so a
/// duplicate is harmless while a close that did nothing on one platform is not.
inline std::vector<SdlEvent> sdl_close_to_events() {
    std::vector<SdlEvent> out;
    out.push_back(zengine::surface::SurfaceCloseRequested{});
    return out;
}

/// Is this SDL event type one this backend translates? The complement is the ignored set --
/// joysticks, gamepads, sensors, displays, drops, clipboards, audio, pens, touch, cameras -- and
/// the four populations this application lives on (keyboard, text, pointer, close) must never
/// be in it, which this predicate makes assertable. Clipboard events are ignored on purpose:
/// clipboard read follows paste intent, and the suite pins 0x900 in the ignored set.
inline constexpr bool sdl_event_is_translated(std::uint32_t type) noexcept {
    switch (type) {
    case sdl::kEventQuit:
    case sdl::kEventWindowCloseRequested:
    case sdl::kEventKeyDown:
    case sdl::kEventKeyUp:
    case sdl::kEventTextInput:
    case sdl::kEventMouseMotion:
    case sdl::kEventMouseButtonDown:
    case sdl::kEventMouseButtonUp:
    case sdl::kEventMouseWheel: return true;
    default: return false;
    }
}

} // namespace zengine::input

#endif // ZENGINE_INPUT_TRANSLATE_SDL_HPP
