// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_INPUT_TRANSLATE_SDL_HPP
#define ZENGINE_INPUT_TRANSLATE_SDL_HPP

// SDL native events -> the public shapes, as pure code. The house pattern from
// translate.hpp, applied to a third backend: NO SDL HEADERS. The reader
// (input_sdl.cpp) hands these functions the event fields as plain integers,
// floats and bytes, with SDL's own constants spelled locally — so the WSL lane
// that never opens a window still pins the whole SDL translation, and an
// SDL-gated case pins the local constants against the real SDL headers.
//
// It is its own header rather than another 250 lines of translate.hpp for one
// reason, and it is a package-boundary reason: this is the only file in the
// Input package that includes the SURFACE vocabulary. See `SdlEvent` below.
//
// WHAT SDL GENUINELY KNOWS, source-traced against SDL 3.4.12's headers, because
// the vocabulary is only as honest as this table (compare translate.hpp's):
//
//   SDL 3
//     key identity   YES, and uniquely: SDL_KeyboardEvent::scancode IS the
//                    numeric space `scancode` was defined in (input/
//                    vocabulary.hpp: "scancode carries SDL scancode values").
//                    Every other backend TRANSLATES into that space; this one
//                    is already in it, so the mapping is the identity — and a
//                    key outside scan::'s named set is passed through rather
//                    than dropped, because it is a real SDL scancode and this
//                    is the backend that defines what those mean.
//     entered text   YES, SDL_EVENT_TEXT_INPUT, UTF-8, already through the
//                    platform's keyboard layout, IME and dead keys. This is the
//                    truthful route `%` arrives by.
//     Shift/Ctrl/Alt YES, per key event, SDL_KeyboardEvent::mod, left and right
//                    separately (both fold to one semantic bit).
//     Super          YES — SDL_KMOD_LGUI/RGUI. The FIRST backend that can set
//                    mod::kSuper at all; the terminal cannot see it and the
//                    Win32 console has no bit for it.
//     key repeat     SDL_KeyboardEvent::repeat. Passed through as an ordinary
//                    press, which is what the vocabulary already promises
//                    ("Auto-repeat arrives as repeated presses (SDL's own
//                    stance); there is deliberately no repeat flag").
//     pointer        position YES, on the motion event, on the BUTTON event,
//                    and on the wheel event — SDL states x/y with each, so
//                    nothing is reconstructed. Motion additionally carries
//                    SDL's own xrel/yrel, so even the delta is stated rather
//                    than derived.
//     pointer mods   *** NO. *** SDL_MouseMotionEvent, SDL_MouseButtonEvent and
//                    SDL_MouseWheelEvent carry no modifier field at all. The
//                    only way to get one is SDL_GetModState(), which reads
//                    CURRENT keyboard state at some later instant and is
//                    exactly the reconstruction this package exists to refuse.
//                    So pointer events from this backend carry mod::kNone, and
//                    that is the documented meaning of a clear bit: "nothing
//                    this backend can see was held". A shift-click is not
//                    expressible here, and saying so is the honest answer.
//     coordinates    FLOATS, window-relative. See `sdl_pixel` for what that
//                    costs and what it does not.
//
// Neither of the other backends reports pixels; this one does, and it is the
// first producer `space::kPixels` has ever had.

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

/// One translated SDL event.
///
/// It is the Input variant PLUS TWO, and neither of the two is an input moment:
/// `surface::SurfaceCloseRequested` and, since TEXT-0,
/// `surface::ClipboardChanged`. SDL reports window lifecycle, clipboard changes
/// and input through a single process-global queue, so the weave that owns that
/// queue is the only thing that can see either fact — and the whole point of
/// this variant is that each is carried as the SURFACE FACT it is, in the
/// vocabulary that owns the application's surface, rather than being smuggled
/// through as a fake key. Owning a queue does not entitle a package to rename
/// what is on it.
///
/// The weave (input_weave.hpp) derives its Emit set from this variant, so the
/// terminal and Win32 readers declare exactly the six input shapes they had
/// before and only the SDL reader declares the extra two.
using SdlEvent = std::variant<KeyPressed, KeyReleased, TextEntered, PointerMoved, PointerButton,
                              PointerWheel, zengine::surface::SurfaceCloseRequested,
                              zengine::surface::ClipboardChanged>;

/// SDL's own numbers, spelled locally so this header stays SDL-free — and
/// pinned against the real headers by an SDL-gated case in the suite, exactly
/// as `scan::` is. A typo here is a red test, not a silently different world.
namespace sdl {

// SDL_EventType (SDL_events.h). The window events are a contiguous block from
// SDL_EVENT_WINDOW_SHOWN = 0x202; CLOSE_REQUESTED is the fifteenth of them.
inline constexpr std::uint32_t kEventQuit = 0x100;
inline constexpr std::uint32_t kEventWindowCloseRequested = 0x210;
inline constexpr std::uint32_t kEventClipboardUpdate = 0x900; // SDL_EVENT_CLIPBOARD_UPDATE
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

/// One SDL float coordinate as an int64 pixel.
///
/// WHAT IS TRUE AND WHAT IS LOST, because "measure the real domain" is the only
/// honest way to answer whether an int64 field may carry a float fact:
///
///   SDL's pointer coordinates are floats and are window-relative. For the
///   window the Surface package creates — no SDL_WINDOW_HIGH_PIXEL_DENSITY, no
///   SDL_SetRenderLogicalPresentation, no SDL_SetRenderScale — window
///   coordinates and framebuffer pixels are 1:1 and every value SDL delivers is
///   integral. On that domain this conversion is EXACT, and the report says so
///   with the measurement rather than the assumption.
///
///   THAT LIST USED TO END "not resizable", AND IT WAS STALE (HD-0 found it,
///   HD-1 repaired it). G-2 made the window SDL_WINDOW_RESIZABLE, and the
///   conclusion survived the change because resizability is not one of the
///   things that introduces a scale: a bigger window is more 1:1 pixels, not
///   differently-sized ones. The three flags named above are the ones that
///   actually would, and none of them is set — which is the sentence a DPI phase
///   will read, so it now names properties that are still true.
///
///   Outside it — a logical presentation, a high-density display, relative
///   mode — SDL genuinely can report a fraction, and int64 cannot hold one.
///   This FLOORS, which is the only rule that agrees with what a maker sees (a
///   coordinate of 3.7 is inside pixel 3, and inside cell 0 of a 12-pixel
///   cell), and the loss is named here rather than described as preservation.
///
/// Total over every float, including the ones SDL will never send: NaN and
/// anything outside int64 saturate rather than invoking the undefined behaviour
/// a bare cast would. The saturated ends are far outside any window, which
/// already means "nothing there" — the same posture the rest of this package
/// takes to a wire value it cannot use.
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

/// SDL_EVENT_KEY_DOWN / SDL_EVENT_KEY_UP -> the key transition.
///
/// The scancode is PASSED THROUGH. Every other backend in this package has a
/// translation table because it starts from a native key identity that is not
/// SDL's; this one starts from SDL's, which is the wire's, so a table would be
/// a copy of the identity function that could drift from it. A key with no
/// courtesy name simply has an empty `name`, which the vocabulary already
/// permits ("`name` is optional convenience only, never authoritative").
///
/// A REPEAT IS A PRESS. SDL sets `repeat` on an auto-repeated key-down and the
/// vocabulary deliberately has no repeat flag, so a held key produces a stream
/// of ordinary presses. Nothing here invents a category the wire cannot say.
/// The flag is taken as a parameter rather than ignored at the edge so that
/// this decision is visible in the one place it is made.
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

/// SDL_EVENT_MOUSE_BUTTON_DOWN / _UP -> the transition AND WHERE IT HAPPENED.
///
/// The position is the one SDL put on THIS event. Nothing here consults a
/// remembered pointer, and there is no tracker in this backend to consult.
///
/// AN UNSUPPORTED BUTTON PRODUCES NOTHING. SDL numbers X1 and X2 4 and 5, and
/// `PointerButton::button` states three values (1 left, 2 middle, 3 right).
/// Silence is the honest answer: mapping a thumb button onto Left would report
/// a click that did not happen, and widening the vocabulary for a button no
/// consumer asks about would be building for a mouse nobody here owns. The
/// bound is stated rather than discovered.
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

/// SDL_EVENT_MOUSE_WHEEL -> the wheel.
///
/// The deltas stay FRACTIONAL and that is not luck: `PointerWheel::dx/dy` are
/// doubles precisely because "a high-resolution wheel genuinely reports a
/// fraction of a detent", so SDL's floats cross this boundary without losing
/// anything. This is the one pointer fact where the existing vocabulary is
/// wider than the backend rather than narrower.
///
/// SDL_MOUSEWHEEL_FLIPPED means the values are already inverted (natural
/// scrolling); SDL's own advice is to multiply by -1 to get back to the
/// convention, and the wire's convention is SDL's normal one (+1 per notch away
/// from the user), so that is what happens. A consumer must not have to know
/// which way a maker's trackpad is configured.
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

/// SDL_EVENT_QUIT / SDL_EVENT_WINDOW_CLOSE_REQUESTED -> the lifecycle fact.
///
/// BOTH, on purpose. SDL_EVENT_WINDOW_CLOSE_REQUESTED is the close box; SDL
/// additionally posts SDL_EVENT_QUIT when the last window goes away, and on
/// some platforms an application-level quit arrives that way and never as a
/// window event at all. Translating only one of them would make "the maker
/// closed it" depend on which path the platform took. A duplicate is harmless
/// because quitting is idempotent — the alternative, a close request that
/// silently does nothing on one platform, is not.
inline std::vector<SdlEvent> sdl_close_to_events() {
    std::vector<SdlEvent> out;
    out.push_back(zengine::surface::SurfaceCloseRequested{});
    return out;
}

/// SDL_EVENT_CLIPBOARD_UPDATE -> the surface fact (TEXT-0).
///
/// The event itself carries no text; the reader asks `SDL_GetClipboardText` at the moment
/// it sees one and hands the answer here, so what travels is the fact a consumer needs
/// ("the platform clipboard now holds this") rather than a notification it would have to
/// chase. An EMPTY answer still travels: a clipboard emptied — or holding something that is
/// not text — is a real state, and a consumer whose mirror kept yesterday's text over it
/// would paste text the platform no longer has.
///
/// This includes the echo of this process's own copies (the Skin's SDL_SetClipboardText
/// lands back here through the platform); a mirror that updates to the text it already
/// holds is the cheap, correct answer, and no de-echo bookkeeping is invented for it.
inline std::vector<SdlEvent> sdl_clipboard_to_events(const char* text) {
    std::vector<SdlEvent> out;
    out.push_back(zengine::surface::ClipboardChanged{
        text != nullptr ? std::string(text) : std::string()});
    return out;
}

/// Is this SDL event type one this backend translates?
///
/// The complement is the ignored set, and it is large and deliberately so: SDL
/// speaks about joysticks, gamepads, sensors, displays, drops, clipboards,
/// audio devices, pens, touch and camera hardware, and Zen has no current
/// semantic obligation to any of them. Ignoring is not dropping something that
/// was owed — but the four populations this application lives on (keyboard,
/// text, pointer, close) must never be in the ignored set, which is what this
/// predicate exists to make assertable.
inline constexpr bool sdl_event_is_translated(std::uint32_t type) noexcept {
    switch (type) {
    case sdl::kEventQuit:
    case sdl::kEventWindowCloseRequested:
    case sdl::kEventClipboardUpdate:
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
