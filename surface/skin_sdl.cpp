// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The SDL Skin — the same intent, a real window. This file is deliberately
// nothing but the SDL edge: both the snake frame and the general canvas are
// planned by skin_sdl_plan.hpp as pure math (pinned on every lane, SDL or
// not); here a plan is executed against a real renderer and the SurfaceText
// slots land in the window title. Zero snake-specific or TUI-specific fields
// were added to any intent to make this medium work — that absence is the
// phase's agnosticism proof.
//
// THIS MEDIUM DRAWS LABELS. A window owns no font the way a terminal does, so
// each label byte is drawn from a cell-sized bitmap the plan owns. What it
// promises is exactly printable ASCII — see skin_sdl_glyphs.hpp, which also
// says what any other byte draws instead of vanishing.
//
// The medium owns the SDL video subsystem RAII-style for the weave's
// lifetime: construction brings SDL up (the window itself is created lazily
// on the first frame, when the board's geometry is first known), destruction
// tears the window down and releases its claim on the subsystem — load claims
// the surface, unload releases it, the same law the terminal skins live by.
//
// Degrades gracefully with no display: SDL_Init fails, the medium stays
// disabled, frames are consumed and counted with nothing to show them on
// (the Input reader's posture, pointed at output) — and it SAYS SO, with SDL's
// own reason, instead of going quietly dark. Under SDL's dummy driver (the
// headless suite) everything below runs for real except photons.
//
// THE WINDOW IS AN EAR, and three things here are consequences of one
// architectural fact: SDL has ONE process-global event queue, and it has ONE
// owner — the SDL Input reader (input/).
//
//   - THIS MEDIUM TAKES NOTHING OUT OF THE QUEUE, AND SERVICES ITS OWN WINDOW.
//     A `pump()` that drained the queue would swallow every key, click and close
//     request the reader exists to hear, so it does not poll and does not
//     filter. What it does do is `SDL_PumpEvents` on its own beat, which
//     gathers the OS's pending input INTO the queue and removes nothing.
//     One-owner is a rule about who REMOVES, and this is still not it.
//     G-1 read it as a rule about who CALLS, emptied this function, and left a
//     window's liveness depending on which reader the host booted -- with the
//     terminal reader nothing called into SDL at all and the window was flagged
//     Not Responding. That is fixed here, and the pairing's remaining honest
//     cost is measured and said out loud (`notice_unread_queue`) rather than
//     left for a person to discover as a hang.
//   - THE WINDOW IS FOCUSABLE. It was created SDL_WINDOW_NOT_FOCUSABLE
//     (WS_EX_NOACTIVATE on Windows) because a window that cannot hear must not
//     take the keys: the terminal was the game's one ear, and keys kept dying
//     in the focused SDL window until it refused focus. It can hear now, so the
//     flag is gone. The cost is stated where it lands: with the terminal reader
//     and the SDL skin together, the window will take focus and neither ear is
//     the one you are typing at. Which reader a run uses is the host's explicit
//     choice (`--input`), not something this file may infer.
//   - THE WINDOW ENABLES NATIVE TEXT EVENTS. SDL_StartTextInput takes a WINDOW,
//     so only whoever owns the window can turn it on, and this is that. It is
//     window setup and nothing more: what a typed character MEANS stays the
//     application's, and the character itself reaches it as an ordinary
//     input::TextEntered through the reader. The SDL window pointer does not
//     leave this file.
//
// The close box is still not this package's to act on. It arrives with the rest
// of the queue at the reader, which routes it as the lifecycle fact it is
// (surface::SurfaceCloseRequested — spelled in this package's vocabulary,
// because it is a fact about this application's surface), and whoever owns quit
// policy decides. A skin still never dies on its own.

#include "skin.hpp"
#include "skin_sdl_plan.hpp"

#include <zen/kernel/export.hpp>

#include <SDL3/SDL.h>

#include <cstdio>
#include <string>

namespace {

using namespace zengine::surface;

/// What went wrong, in SDL's own words, on plain stderr.
///
/// The V1 posture was "politely dark": SDL_Init fails, `ok_` goes false, every
/// frame after that is consumed and nothing is ever shown. It is a correct
/// degradation and a terrible diagnosis — real time has been spent on a WSL
/// whose fetched SDL3 has only the dummy and offscreen video drivers, where the
/// only symptom available was "no window". A surface that cannot exist should say
/// why it cannot exist.
///
/// stderr, not a SurfaceText: the most likely thing to have failed is the
/// surface, and a message about a missing surface delivered to the surface is
/// not a message. Same argument the boot weave already makes for a refused
/// load. This is four lines and one `if` per SDL call that can fail, not a
/// diagnostic framework.
void complain(const char* what) {
    const char* why = SDL_GetError();
    std::fprintf(stderr, "zengine-skin-sdl: %s failed: %s\n", what,
                 (why != nullptr && why[0] != '\0') ? why : "(SDL gave no reason)");
    std::fflush(stderr);
}

/// Give back this weave's claim on SDL, and turn the lights off if nobody else
/// is still in the room.
///
/// SDL_Quit alone would be wrong: the SDL Input reader holds the same subsystem
/// in the same process, and SDL_Quit shuts everything down regardless
/// of who is still using it — a Skin swap would deafen the reader.
/// SDL_QuitSubSystem alone is wrong in the other direction, and MEASURED so: it
/// releases the subsystem and leaves SDL's own global state allocated, which the
/// sanitizer lane reports as a leak at exit (it did, ~8.5 KB per run, the day
/// this stopped calling SDL_Quit).
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

class SdlMedium {
public:
    SdlMedium() {
        ok_ = SDL_Init(SDL_INIT_VIDEO);
        if (!ok_) {
            complain("SDL_Init(SDL_INIT_VIDEO)");
            // A FAILED SDL_Init STILL ALLOCATED. SDL brings its own globals up
            // before it discovers it cannot bring a video driver up, and leaves
            // them behind; measured at ~7 KB by the sanitizer lane the day this
            // path first existed, which is the whole reason that lane exists.
            // The same conditional release the destructor uses, because "this
            // failed" is still "this is no longer holding anything".
            release_sdl();
        }
    }
    ~SdlMedium() {
        if (renderer_ != nullptr) {
            SDL_DestroyRenderer(renderer_);
        }
        if (window_ != nullptr) {
            SDL_DestroyWindow(window_);
        }
        if (ok_) {
            release_sdl();
        }
    }
    SdlMedium(const SdlMedium&) = delete;
    SdlMedium& operator=(const SdlMedium&) = delete;

    void frame(const zengine::snake::SnakeVisual& v, bool) {
        if (!ok_) {
            return;
        }
        if (!ensure_window(v)) {
            return;
        }
        pump();
        for (const PlanRect& r : plan_frame(v)) {
            SDL_SetRenderDrawColor(renderer_, r.r, r.g, r.b, SDL_ALPHA_OPAQUE);
            const SDL_FRect fr{static_cast<float>(r.x), static_cast<float>(r.y),
                               static_cast<float>(r.w), static_cast<float>(r.h)};
            SDL_RenderFillRect(renderer_, &fr);
        }
        SDL_RenderPresent(renderer_);
    }

    /// The general canvas, in a window — the same three lines `frame` uses, and
    /// deliberately so.
    ///
    /// `plan_canvas` hands back the rectangles AND the labels as one list of
    /// opaque quads, so there is nothing here that knows what a label is. That
    /// is the whole shape of the answer to "rectangles drawn, labels dropped":
    /// looping over `c.rects` and never reading `c.labels` is a mistake a second
    /// loop could make again — so the edge has no second thing
    /// to draw. What a glyph looks like, where it lands and what happens to a
    /// byte with no glyph are all decided in skin_sdl_plan.hpp / skin_sdl_glyphs.hpp,
    /// where every lane's suite can read them, SDL built or not.
    void canvas(const SurfaceCanvas& c, bool) {
        if (!ok_) {
            return;
        }
        if (!ensure_sized_window(canvas_window_size(c))) {
            return;
        }
        pump();
        SDL_SetRenderDrawColor(renderer_, kCanvasBackground.r, kCanvasBackground.g,
                               kCanvasBackground.b, SDL_ALPHA_OPAQUE);
        SDL_RenderClear(renderer_);
        for (const PlanRect& r : plan_canvas(c)) {
            SDL_SetRenderDrawColor(renderer_, r.r, r.g, r.b, SDL_ALPHA_OPAQUE);
            const SDL_FRect fr{static_cast<float>(r.x), static_cast<float>(r.y),
                               static_cast<float>(r.w), static_cast<float>(r.h)};
            SDL_RenderFillRect(renderer_, &fr);
        }
        SDL_RenderPresent(renderer_);
    }

    void note(std::string_view slot, std::string_view text) {
        if (slot == kSlotStatus) {
            status_ = std::string(text);
        } else if (slot == kSlotScore) {
            score_ = std::string(text);
        } else {
            return;
        }
        if (ok_ && window_ != nullptr) {
            SDL_SetWindowTitle(window_, title_of(status_, score_).c_str());
        }
    }

    /// SERVICE THIS WINDOW'S CONVERSATION WITH THE OS, AND TAKE NOTHING OUT OF THE QUEUE.
    ///
    /// TWO JOBS THAT SDL_PollEvent DOES AT ONCE, and separating them is the whole of this
    /// function. `while (SDL_PollEvent(&ev)) {}` — drain the queue and drop it — is the
    /// honest thing for an output-only medium and is the single most destructive thing this
    /// medium could do: SDL_PollEvent REMOVES what it returns, so a skin that kept calling it
    /// would not merely be a second poller, it would be a thief, and every key, click and
    /// close request the reader exists to hear would vanish here microseconds before the
    /// reader looked. That reasoning was right and it still stands.
    ///
    /// What it concluded was wrong. G-1 answered it by doing NOTHING here and leaving the
    /// window to be serviced as a side effect of the reader's own poll — which made a Skin's
    /// liveness depend on which INPUT weave the host happened to boot, two independent
    /// choices the host argues at length must stay independent (`--skin` / `--input`, see
    /// workshop.cpp). Run the SDL skin with the terminal reader and nobody calls into SDL's
    /// event machinery at all: the window comes up, never processes another OS message,
    /// Windows flags it Not Responding, and the tool looks broken. Found live, in the
    /// graphical Workshop, on exactly that pair of flags.
    ///
    /// `SDL_PumpEvents` is the half that was wanted. It gathers the pending OS input INTO
    /// the queue and removes nothing — it is the call SDL_PollEvent makes internally before
    /// it takes anything — and SDL's own header names this situation: "if you are not
    /// polling or waiting for events (e.g. you are filtering them), then you must call
    /// SDL_PumpEvents() to force an event queue update". So the queue still has exactly one
    /// OWNER, meaning one component that REMOVES from it, and that is still not this one.
    /// Whether a reader is loaded no longer decides whether a window is alive.
    ///
    /// It is safe to pump twice. When the SDL reader IS loaded its poll pumps as well; the
    /// call gathers whatever the OS has and is not a state machine anybody can get out of
    /// step. It runs on the host's one thread, which is the thread that brought the video
    /// subsystem up in this medium's constructor.
    void pump() {
        if (!ok_ || window_ == nullptr) {
            return; // nothing has an OS conversation yet
        }
        SDL_PumpEvents();
        notice_unread_queue();
    }

private:
    /// SAY, ONCE, WHEN NOTHING IS TAKING WHAT THIS WINDOW HEARS.
    ///
    /// The window is an ear as well as a surface, and it takes focus. Boot it beside a reader
    /// that is watching something else — the terminal — and a maker types into a window
    /// whose keys nobody collects, including its close box. That is a legitimate pairing of
    /// two independent flags and it is not this file's to refuse; it is this file's to make
    /// legible, the same posture the whole of `complain` above exists for.
    ///
    /// IT MEASURES, IT DOES NOT INFER. Nothing here reads a flag, asks who is registered, or
    /// guesses a reader from the skin's own name — which is the deduction the host refuses to
    /// write down and this medium is in no position to write down for it. It counts the
    /// queue, non-destructively (`SDL_PeepEvents` with a null buffer walks the whole queue and
    /// removes nothing), and a queue that has grown past a thousand events is a queue nobody
    /// is emptying: a live reader drains it completely every beat, so reaching this number
    /// with one running would take a hundred thousand events a second, sustained.
    ///
    /// Once, and then never again — a complaint on every beat would be the noise that teaches
    /// a person to stop reading stderr.
    void notice_unread_queue() {
        if (said_unread_) {
            return;
        }
        const int waiting =
            SDL_PeepEvents(nullptr, 0, SDL_PEEKEVENT, SDL_EVENT_FIRST, SDL_EVENT_LAST);
        if (waiting < kUnreadQueue) {
            return;
        }
        said_unread_ = true;
        std::fprintf(stderr,
                     "zengine-skin-sdl: %d events are queued for this window and nothing is "
                     "taking them.\n"
                     "zengine-skin-sdl: the window still draws, but it is not the ear this run "
                     "is listening with --\n"
                     "zengine-skin-sdl: type at the terminal instead, or start with `--input "
                     "zengine-input-sdl`.\n",
                     waiting);
        std::fflush(stderr);
    }

    /// How many unread events make "nobody is draining this" a measurement rather than a
    /// coincidence. A reader empties the queue on each of its own 10ms beats, so a thousand
    /// standing events cannot be a drained queue caught mid-beat.
    static constexpr int kUnreadQueue = 1000;

    bool ensure_window(const zengine::snake::SnakeVisual& v) {
        return ensure_sized_window(window_size_of(v));
    }

    bool ensure_sized_window(const PlanSize& want) {
        if (want.w <= 0 || want.h <= 0) {
            return false;
        }
        if (window_ == nullptr) {
            // No flags. Not SDL_WINDOW_NOT_FOCUSABLE, because this window is an
            // ear as well as a surface — see the header — and deliberately NOT
            // SDL_WINDOW_RESIZABLE: the
            // window is still exactly the canvas, and what a resizable graphical
            // Workshop should mean is a question nobody has evidence for yet.
            window_ = SDL_CreateWindow(title_of(status_, score_).c_str(),
                                       static_cast<int>(want.w), static_cast<int>(want.h), 0);
            if (window_ == nullptr) {
                complain("SDL_CreateWindow");
                return false;
            }
            renderer_ = SDL_CreateRenderer(window_, nullptr);
            if (renderer_ == nullptr) {
                complain("SDL_CreateRenderer");
                SDL_DestroyWindow(window_);
                window_ = nullptr;
                return false;
            }
            // The window exists, so native text events can be turned on — and
            // only whoever holds the window pointer can turn them on, which is
            // why this call is here and not in the Input package. It is window
            // setup: this medium gains no opinion about what text MEANS, and the
            // characters go to the reader like every other event.
            //
            // A failure here is not fatal to drawing, so it is reported and the
            // surface still comes up: a readable window with no typing is a
            // better answer than no window, and the difference must be legible
            // rather than "the letters just do not arrive".
            if (!SDL_StartTextInput(window_)) {
                complain("SDL_StartTextInput");
            }
            size_ = want;
            return true;
        }
        if (want.w != size_.w || want.h != size_.h) {
            SDL_SetWindowSize(window_, static_cast<int>(want.w), static_cast<int>(want.h));
            size_ = want;
        }
        return true;
    }

    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    PlanSize size_{};
    std::string status_;
    std::string score_;
    bool ok_ = false;
    bool said_unread_ = false; ///< the queue complaint is said once per incarnation
};

using SkinSdl = SkinT<SdlMedium>;

} // namespace

ZEN_EXPORT_WEAVE(SkinSdl)
