// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The SDL Input weave library — the SDL event queue's ONE owner.
//
// The same shape input.cpp has: this file is nothing but the platform edge. It
// FETCHES native events and hands their fields to the pure translators in
// translate_sdl.hpp; the weave (input_weave.hpp) publishes whatever comes back.
// Everything that decides what an event MEANS is in a header every lane pins,
// SDL built or not.
//
// WHY THIS IS A SECOND WEAVE LIBRARY AND NOT A SECOND READER INSIDE
// zengine-input. Because a reader is chosen at BUILD time there (`using
// PlatformReader = ...` behind an #if), and which surface a run is being shown
// on is not a property of the platform: the same Linux box runs the terminal
// Workshop and the graphical one. A separate artifact makes the choice a LAUNCH
// decision the host states out loud (`--input <stem>`), and leaves the terminal
// reader untouched on every platform it already worked on.
//
// WHY IT MAY POLL AND THE SKIN MAY NOT. SDL has one process-global event queue
// and SDL_PollEvent REMOVES what it returns, so two components polling it steal
// from each other. A Skin that drained that queue every 10ms and threw
// everything away would be correct for an output-only medium and fatal for an
// ear. The Skin's `pump()` is empty and this is the only code in the process
// that touches the queue. Servicing the OS conversation for the Skin's window
// comes free with the same call: SDL_PollEvent pumps the platform's own queue
// on the way past, on this weave's own 10ms beat, which is the cadence the
// Skin's beat used to supply.
//
// AND WHY BOTH OF THEM NEED ONE SDL. The Loom loads weaves with
// dlopen(RTLD_LOCAL) / LoadLibraryA, so a statically archived SDL would give
// this library its own event queue, its own video subsystem and its own
// SDL_Init refcount — and this reader would poll a queue the Skin's window
// never posts to, with nothing failing until a real window sat there deaf.
// cmake/ZengineSdl.cmake requires a SHARED SDL3 and refuses a static one out
// loud for exactly that reason.
//
// THE SUBSYSTEM IS SHARED, SO IT IS RELEASED AND NOT QUIT. SDL_Init is
// refcounted per subsystem; SDL_Quit is not — it shuts everything down whoever
// else is still using it. This reader and the Skin each take a claim and each
// give its own back (SDL_QuitSubSystem), so whichever unloads second is the one
// that actually puts the video subsystem away.
//
// Degrades honestly with no display: SDL_Init fails, the reader stays disabled,
// poll() yields nothing — and it SAYS SO with SDL's own reason, because a
// focusable window that hears nothing and explains nothing is the exact failure
// the skin's silent-darkness pressure names.

#include "input_weave.hpp"
#include "translate_sdl.hpp"

#include <zen/kernel/export.hpp>

#include <SDL3/SDL.h>

#include <cstdint>
#include <cstdio>
#include <vector>

namespace {

using namespace zengine::input;

/// Give back this weave's claim on SDL, and turn the lights off if nobody else
/// is still in the room. The Skin's `release_sdl` exactly (surface/skin_sdl.cpp)
/// and it must be exactly it, because the two weaves are the two holders.
///
/// SDL_Quit alone would be wrong: it shuts everything down regardless of who is
/// still using it, so unloading the reader would deafen a live Skin — and blank
/// it. SDL_QuitSubSystem alone is wrong in the other direction and MEASURED so:
/// it releases the subsystem and leaves SDL's own globals allocated, which the
/// sanitizer lane reports as a leak at exit.
///
/// So the release is refcounted and the final shutdown is conditional, and the
/// condition is asked of SDL rather than tracked privately: `SDL_WasInit(0)` is
/// the mask of subsystems still up, so an empty mask means this was the last
/// holder. A private counter shared between two separately-loaded weave
/// libraries is exactly the thing that cannot exist here; SDL already keeps the
/// only copy that could be right.
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

    /// One drain, IN QUEUE ORDER.
    ///
    /// SDL_PollEvent returns events in the order the platform queued them, and
    /// they are appended in that order, so `KeyPressed, TextEntered,
    /// KeyReleased` reaches the bus as `KeyPressed, TextEntered, KeyReleased`.
    /// Nothing here sorts, batches by kind, or defers one population to serve
    /// another — the stream is the platform's and this function's only freedom
    /// is which events it does not understand.
    ///
    /// It drains everything pending, which is the Win32 console reader's shape
    /// beside it. That is unbounded in principle: a turn's work is whatever the
    /// platform queued. It is bounded in practice by a 10ms beat and by a human
    /// hand, and no cap is invented for it here — an arbitrary number would
    /// silently drop a maker's input to defend against a queue nothing has been
    /// measured to produce. The pressure is reported rather than papered over.
    std::vector<SdlEvent> poll() {
        std::vector<SdlEvent> out;
        if (!ok_) {
            return out;
        }
        // THIS READER NEVER TOUCHES THE CLIPBOARD (QR-11). TEXT-0 had it read the
        // pre-existing clipboard on the first poll and fetch the payload of every
        // SDL_EVENT_CLIPBOARD_UPDATE — ambient system-clipboard text imported merely
        // because the application was running. The motivation ("the first paste of a run
        // wants text copied before this process started") is answered properly by the
        // read-on-intent road: the Medium reads the clipboard's CURRENT value when a paste
        // asks (surface/vocabulary.hpp, ClipboardTextRequested), which serves the first
        // paste of a run and every later one, staleness-free, and serves nothing else.
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
            // Everything else SDL speaks about: displays, joysticks, gamepads,
            // sensors, pens, touch, drops, audio devices, cameras, and the rest
            // of the window events (shown, moved, focus, exposed). None of them
            // carries a current Zen semantic obligation, so they are ignored —
            // and ignored is not the same as dropped: the populations this
            // application lives on are all above, and translate_sdl.hpp's
            // `sdl_event_is_translated` is the assertable statement of exactly
            // which those are.
            return;
        }
        out.insert(out.end(), batch.begin(), batch.end());
    }

    bool ok_ = false;
};

/// The local constants in translate_sdl.hpp ARE SDL's, checked by the compiler
/// against the real headers in the one translation unit that has them.
///
/// The pure translator must not include SDL — that is what lets every lane pin
/// it — so its numbers are spelled by hand, and a hand-spelled constant is a
/// thing that can be wrong. `scan::` earned this pattern the same way: pinned
/// as literals, checked against the SDL headers where they are available. Here
/// the check is a static_assert rather than a test case, because this file is
/// the only place both spellings exist at once and a build is a stronger place
/// to fail than a run.
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
// HD-3's three, pinned the same way and for a sharper reason: their whole claim is that they
// are the values this backend ALREADY delivers, so an identity that drifted would make a
// named key silently mean a different one.
static_assert(scan::kHome == SDL_SCANCODE_HOME);
static_assert(scan::kDelete == SDL_SCANCODE_DELETE);
static_assert(scan::kEnd == SDL_SCANCODE_END);
static_assert(scan::kDown == SDL_SCANCODE_DOWN);

using SdlInputWeave = InputWeaveT<SdlReader>;

} // namespace

ZEN_EXPORT_WEAVE(SdlInputWeave)
