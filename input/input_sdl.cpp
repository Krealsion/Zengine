// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The SDL Input weave library: the SDL event queue's one owner. Like input.cpp it is only the
// platform edge -- it fetches native events and hands their fields to the pure translators in
// translate_sdl.hpp, and the weave publishes what comes back. A separate artifact rather than a
// build-time reader in zengine-input, so which surface a run is shown on is a launch decision
// the host states (`--input <stem>`), and the terminal reader is untouched.
// Surface law: agents/surface.md

// SDL has one process-global queue and `SDL_PollEvent` removes what it returns, so this is the
// only code that touches it (the Skin's `pump()` is empty), and its 10 ms beat also pumps the
// OS conversation for the Skin's window. Both need one shared SDL: a static SDL in an
// `RTLD_LOCAL` library would give this reader its own queue, deaf to the window, and
// cmake/ZengineSdl.cmake refuses one. With no display the reader stays disabled and says why.

#include "input_weave.hpp"
#include "translate_sdl.hpp"

#include <zen/kernel/export.hpp>

#include <SDL3/SDL.h>

#include <cstdint>
#include <cstdio>
#include <vector>

namespace {

using namespace zengine::input;

/// Give back this weave's claim on SDL, and shut SDL down only if nobody else holds it: the
/// Skin's `release_sdl` exactly (surface/skin_sdl.cpp), since the two weaves are the two
/// holders. `SDL_Quit` alone would deafen and blank a live Skin; `SDL_QuitSubSystem` alone
/// leaves SDL's globals allocated, which the sanitizer lane reports as a leak. `SDL_WasInit(0)`
/// is asked rather than a private counter, which two separately loaded libraries could not share.
void release_sdl() {
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    if (SDL_WasInit(0) == 0) {
        SDL_Quit();
    }
}

class SdlReader {
public:
    SdlReader() {
        ok_ = SDL_Init(SDL_INIT_VIDEO);
        if (!ok_) {
            const char* why = SDL_GetError();
            std::fprintf(stderr, "zengine-input-sdl: SDL_Init(SDL_INIT_VIDEO) failed: %s\n"
                                 "  no SDL events will be read; the window (if any) is deaf.\n",
                         (why != nullptr && why[0] != '\0') ? why : "(SDL gave no reason)");
            std::fflush(stderr);
            release_sdl(); // a failed init still allocated — see release_sdl
        }
    }
    ~SdlReader() {
        if (ok_) {
            release_sdl();
        }
    }
    SdlReader(const SdlReader&) = delete;
    SdlReader& operator=(const SdlReader&) = delete;

    /// One drain, in queue order: events are appended as the platform queued them, so
    /// `KeyPressed, TextEntered, KeyReleased` reaches the bus in that order, and nothing sorts,
    /// batches by kind or defers one population for another. It drains everything pending: no
    /// cap is invented, since an arbitrary number would drop a maker's input to defend against
    /// a queue nothing has been measured to produce.
    std::vector<SdlEvent> poll() {
        std::vector<SdlEvent> out;
        if (!ok_) {
            return out;
        }
        // This reader never touches the clipboard: reading it on every update would import
        // ambient system-clipboard text merely because the application runs. The Medium reads the
        // clipboard's current value when a paste asks (`ClipboardTextRequested`,
        // surface/vocabulary.hpp), which serves the first paste of a run and every later one.
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            append(out, ev);
        }
        return out;
    }

private:
    /// One native event -> zero or more translated ones. The whole body is a
    /// switch that pulls fields out of the union and hands them to a pure
    /// function; there is no decision here that a test cannot reach.
    static void append(std::vector<SdlEvent>& out, const SDL_Event& ev) {
        std::vector<SdlEvent> batch;
        switch (ev.type) {
        case SDL_EVENT_QUIT:
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            batch = sdl_close_to_events();
            break;
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP:
            batch = sdl_key_to_events(static_cast<std::int64_t>(ev.key.scancode),
                                      static_cast<std::uint16_t>(ev.key.mod), ev.key.down,
                                      ev.key.repeat);
            break;
        case SDL_EVENT_TEXT_INPUT:
            // ev.text.text is SDL's buffer and is only valid for this call, so
            // the translator COPIES it into the message. Nothing downstream ever
            // sees the pointer.
            batch = sdl_text_to_events(ev.text.text);
            break;
        case SDL_EVENT_MOUSE_MOTION:
            batch = sdl_mouse_motion_to_events(ev.motion.x, ev.motion.y, ev.motion.xrel,
                                               ev.motion.yrel);
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP:
            batch = sdl_mouse_button_to_events(static_cast<std::int64_t>(ev.button.button),
                                               ev.button.down, ev.button.x, ev.button.y);
            break;
        case SDL_EVENT_MOUSE_WHEEL:
            batch = sdl_mouse_wheel_to_events(ev.wheel.x, ev.wheel.y,
                                              static_cast<std::uint32_t>(ev.wheel.direction),
                                              ev.wheel.mouse_x, ev.wheel.mouse_y);
            break;
        default:
            // Everything else SDL speaks about (displays, joysticks, gamepads, sensors, pens,
            // touch, drops, audio, cameras, the other window events) carries no Zen obligation
            // and is ignored; `sdl_event_is_translated` states which populations are not.
            return;
        }
        out.insert(out.end(), batch.begin(), batch.end());
    }

    bool ok_ = false;
};

/// The local constants in translate_sdl.hpp are SDL's, checked by the compiler against the real
/// headers in the one translation unit that has both spellings: the pure translator must not
/// include SDL, and a hand-spelled constant can be wrong. A build is a stronger place to fail
/// than a run.
static_assert(sdl::kEventQuit == SDL_EVENT_QUIT);
static_assert(sdl::kEventWindowCloseRequested == SDL_EVENT_WINDOW_CLOSE_REQUESTED);
static_assert(sdl::kEventKeyDown == SDL_EVENT_KEY_DOWN);
static_assert(sdl::kEventKeyUp == SDL_EVENT_KEY_UP);
static_assert(sdl::kEventTextEditing == SDL_EVENT_TEXT_EDITING);
static_assert(sdl::kEventTextInput == SDL_EVENT_TEXT_INPUT);
static_assert(sdl::kEventMouseMotion == SDL_EVENT_MOUSE_MOTION);
static_assert(sdl::kEventMouseButtonDown == SDL_EVENT_MOUSE_BUTTON_DOWN);
static_assert(sdl::kEventMouseButtonUp == SDL_EVENT_MOUSE_BUTTON_UP);
static_assert(sdl::kEventMouseWheel == SDL_EVENT_MOUSE_WHEEL);
static_assert(sdl::kModLShift == SDL_KMOD_LSHIFT);
static_assert(sdl::kModRShift == SDL_KMOD_RSHIFT);
static_assert(sdl::kModLCtrl == SDL_KMOD_LCTRL);
static_assert(sdl::kModRCtrl == SDL_KMOD_RCTRL);
static_assert(sdl::kModLAlt == SDL_KMOD_LALT);
static_assert(sdl::kModRAlt == SDL_KMOD_RALT);
static_assert(sdl::kModLGui == SDL_KMOD_LGUI);
static_assert(sdl::kModRGui == SDL_KMOD_RGUI);
static_assert(sdl::kButtonLeft == SDL_BUTTON_LEFT);
static_assert(sdl::kButtonMiddle == SDL_BUTTON_MIDDLE);
static_assert(sdl::kButtonRight == SDL_BUTTON_RIGHT);
static_assert(sdl::kWheelNormal == SDL_MOUSEWHEEL_NORMAL);
static_assert(sdl::kWheelFlipped == SDL_MOUSEWHEEL_FLIPPED);

/// And the identity claim the whole key path rests on: SDL's scancode space IS
/// the wire's, so this backend translates nothing. If SDL ever renumbered a key,
/// this is where it would stop compiling.
static_assert(scan::kQ == SDL_SCANCODE_Q);
static_assert(scan::kReturn == SDL_SCANCODE_RETURN);
static_assert(scan::kEscape == SDL_SCANCODE_ESCAPE);
static_assert(scan::kBackspace == SDL_SCANCODE_BACKSPACE);
static_assert(scan::kTab == SDL_SCANCODE_TAB);
static_assert(scan::kLeftBracket == SDL_SCANCODE_LEFTBRACKET);
static_assert(scan::kUp == SDL_SCANCODE_UP);
// Home, Delete and End are pinned for a sharper reason: their whole claim is that they are the
// values this backend already delivers.
static_assert(scan::kHome == SDL_SCANCODE_HOME);
static_assert(scan::kDelete == SDL_SCANCODE_DELETE);
static_assert(scan::kEnd == SDL_SCANCODE_END);
static_assert(scan::kDown == SDL_SCANCODE_DOWN);

using SdlInputWeave = InputWeaveT<SdlReader>;

} // namespace

ZEN_EXPORT_WEAVE(SdlInputWeave)
