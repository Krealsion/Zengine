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
// G-0 gave the canvas its labels. `SurfaceLabel` used to arrive here and go
// nowhere, because a window owns no font the way a terminal does; the medium
// now draws each label byte from a cell-sized bitmap the plan owns. What it
// promises is exactly printable ASCII — see skin_sdl_glyphs.hpp, which also
// says what any other byte draws instead of vanishing.
//
// The medium owns the SDL video subsystem RAII-style for the weave's
// lifetime: construction brings SDL up (the window itself is created lazily
// on the first frame, when the board's geometry is first known), destruction
// tears the window down and quits SDL — load claims the surface, unload
// releases it, the same law the terminal skins live by.
//
// Degrades gracefully with no display: SDL_Init fails, the medium stays
// disabled, frames are consumed and counted with nothing to show them on
// (the Input reader's posture, pointed at output). Under SDL's dummy driver
// (the headless suite) everything below runs for real except photons.
//
// V1 is OUTPUT-ONLY, and the window is shaped to tell that truth twice:
//   - the event queue is drained and dropped — on the host's PumpSurface lap
//     message, not only per frame, because a dead-quiet world publishes no
//     frames and an unserviced Windows window is flagged unresponsive (the
//     live busy-cursor find). Execution time and intent are different
//     things; the pump carries the first.
//   - the window is created NOT_FOCUSABLE (WS_EX_NOACTIVATE on Windows): a
//     window that cannot hear must not take the keys. The terminal remains
//     the game's one ear — keys kept dying in the focused SDL window until
//     it refused focus. The flag comes off the day the SDL Reader makes the
//     window an ear too.
// The window's own input — including its close button — is the SDL Reader's
// ground (the Input package names that follow-on), not a drawing package's;
// wiring a close box to "quit" belongs to whoever owns lifecycle, reached by
// input vocabulary, never by a skin deciding to die on its own.

#include "skin.hpp"
#include "skin_sdl_plan.hpp"

#include <zen/kernel/export.hpp>

#include <SDL3/SDL.h>

#include <string>

namespace {

using namespace zengine::surface;

class SdlMedium {
public:
    SdlMedium() { ok_ = SDL_Init(SDL_INIT_VIDEO); }
    ~SdlMedium() {
        if (renderer_ != nullptr) {
            SDL_DestroyRenderer(renderer_);
        }
        if (window_ != nullptr) {
            SDL_DestroyWindow(window_);
        }
        if (ok_) {
            SDL_Quit();
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
    /// is the whole shape of G-0's answer to P8: this medium used to loop over
    /// `c.rects` and never read `c.labels`, and the fix was not to add a second
    /// loop it could lose again — it was to leave the edge with no second thing
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

    /// Keep the OS talking to the window; say nothing back (see header).
    /// Runs on every PumpSurface lap — frames or no frames.
    void pump() {
        if (!ok_ || window_ == nullptr) {
            return;
        }
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
        }
    }

private:
    bool ensure_window(const zengine::snake::SnakeVisual& v) {
        return ensure_sized_window(window_size_of(v));
    }

    bool ensure_sized_window(const PlanSize& want) {
        if (want.w <= 0 || want.h <= 0) {
            return false;
        }
        if (window_ == nullptr) {
            window_ = SDL_CreateWindow(title_of(status_, score_).c_str(),
                                       static_cast<int>(want.w), static_cast<int>(want.h),
                                       SDL_WINDOW_NOT_FOCUSABLE);
            if (window_ == nullptr) {
                return false;
            }
            renderer_ = SDL_CreateRenderer(window_, nullptr);
            if (renderer_ == nullptr) {
                SDL_DestroyWindow(window_);
                window_ = nullptr;
                return false;
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
};

using SkinSdl = SkinT<SdlMedium>;

} // namespace

ZEN_EXPORT_WEAVE(SkinSdl)
