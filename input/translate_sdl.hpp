// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_INPUT_TRANSLATE_SDL_HPP
#define ZENGINE_INPUT_TRANSLATE_SDL_HPP

// The SDL Input reader's translation: Input's moments from translate_sdl_core.hpp, the pure SDL
// translation both readers of SDL's queue share, plus the window's close request as the Surface
// fact it is. Its own header because it is the one file in Input that includes the Surface
// vocabulary (`SdlEvent`).
// Surface law: agents/surface.md

#include "translate_sdl_core.hpp"
#include "vocabulary.hpp"

#include "surface/vocabulary.hpp"

#include <cstdint>
#include <utility>
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

/// Input's moments as the SDL reader's events, in order.
inline std::vector<SdlEvent> sdl_events_of(std::vector<InputEvent> moments) {
    std::vector<SdlEvent> out;
    out.reserve(moments.size());
    for (InputEvent& m : moments) {
        std::visit([&out](auto& moment) { out.emplace_back(std::move(moment)); }, m);
    }
    return out;
}

/// SDL_EVENT_KEY_DOWN / SDL_EVENT_KEY_UP -> the key transition (`sdl_key_moments`).
inline std::vector<SdlEvent> sdl_key_to_events(std::int64_t scancode, std::uint16_t keymod,
                                               bool down, bool repeat) {
    return sdl_events_of(sdl_key_moments(scancode, keymod, down, repeat));
}

/// SDL_EVENT_TEXT_INPUT -> the text (`sdl_text_moments`); empty text is no event.
inline std::vector<SdlEvent> sdl_text_to_events(const char* text) {
    return sdl_events_of(sdl_text_moments(text));
}

/// SDL_EVENT_MOUSE_MOTION -> the motion, with SDL's own delta (`sdl_mouse_motion_moments`).
inline std::vector<SdlEvent> sdl_mouse_motion_to_events(float x, float y, float xrel,
                                                        float yrel) {
    return sdl_events_of(sdl_mouse_motion_moments(x, y, xrel, yrel));
}

/// SDL_EVENT_MOUSE_BUTTON_DOWN / _UP -> the transition, X1 and X2 silent
/// (`sdl_mouse_button_moments`).
inline std::vector<SdlEvent> sdl_mouse_button_to_events(std::int64_t button, bool down, float x,
                                                        float y) {
    return sdl_events_of(sdl_mouse_button_moments(button, down, x, y));
}

/// SDL_EVENT_MOUSE_WHEEL -> the wheel, flipped put back (`sdl_mouse_wheel_moments`).
inline std::vector<SdlEvent> sdl_mouse_wheel_to_events(float dx, float dy,
                                                       std::uint32_t direction, float mouse_x,
                                                       float mouse_y) {
    return sdl_events_of(sdl_mouse_wheel_moments(dx, dy, direction, mouse_x, mouse_y));
}

/// SDL_EVENT_QUIT / SDL_EVENT_WINDOW_CLOSE_REQUESTED -> the lifecycle fact. Both: some platforms
/// send an application quit and never the window event, and quitting is idempotent, so a
/// duplicate is harmless while a close that did nothing on one platform is not.
inline std::vector<SdlEvent> sdl_close_to_events() {
    std::vector<SdlEvent> out;
    out.push_back(zengine::surface::SurfaceCloseRequested{});
    return out;
}

} // namespace zengine::input

#endif // ZENGINE_INPUT_TRANSLATE_SDL_HPP
