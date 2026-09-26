// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_SURFACE_SKIN_SDL_TEXT_HPP
#define ZENGINE_SURFACE_SKIN_SDL_TEXT_HPP

// The graphical medium's type, the one part of this package that owns a font: the embedded face,
// its metric (measured from the opened face, never assumed), and the drawing of a resolved
// `PlanTextRegion`. It never decides how much prose fits, wraps or truncates: `fit_region` did,
// so one party measures. A face that will not open says why on stderr and publishes no metric,
// "text is a cell", so a pane degrades to the bitmap face rather than to a blank rectangle.
// Surface law: agents/surface.md

#include "skin_sdl_plan.hpp"

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>

namespace zengine::surface {

/// The embedded face, generated from `surface/fonts/JetBrainsMono-Regular.ttf` by
/// cmake/EmbedBinary.cmake. Provenance and licence: `surface/fonts/PROVENANCE.md`.
extern const unsigned char kSkinFontBytes[];
extern const std::size_t kSkinFontBytes_size;

/// The point size, the only number in this file a person chose. Measured, not guessed: at 13 pt
/// this face advances 8 device pixels with an 18-pixel line, larger than the bitmap letterform
/// and 83 characters to the minimum pane's row instead of 56, paid for in rows (eight lines in
/// the minimum pane's thirteen cells). No picker, no scaling and no DPI query: each is its own
/// decision.
inline constexpr float kSkinFontPt = 13.0F;

/// How many characters are measured to derive one advance: ten round once between them, so the
/// answer is stable on a face whose advance is not a whole number of pixels.
inline constexpr int kAdvanceSample = 10;

/// The graphical medium's text, as an object with a lifetime tied to one SDL_Renderer (the engine
/// dies with it), which is why `open` takes the renderer and `close` is idempotent. SDL_ttf
/// refcounts TTF_Init/TTF_Quit itself, so this pairs them and tracks nothing.
class SdlTypeface {
public:
    SdlTypeface() = default;
    ~SdlTypeface() { close(); }
    SdlTypeface(const SdlTypeface&) = delete;
    SdlTypeface& operator=(const SdlTypeface&) = delete;

    /// Open the embedded face for this renderer, and measure it: a face that opens but measures
    /// to nothing is not usable, and a zero advance would reach every division downstream. Every
    /// failure leaves this not-live, having said why; the caller carries on in the bitmap face.
    bool open(SDL_Renderer* renderer) {
        close();
        if (renderer == nullptr) {
            return false;
        }
        if (!TTF_Init()) {
            complain_text("TTF_Init");
            return false;
        }
        inited_ = true;
        // The face is bytes, not a path: nothing to search for, half-install or miss. SDL_ttf
        // closes the stream with the font; the bytes are static and outlive everything.
        SDL_IOStream* io = SDL_IOFromConstMem(kSkinFontBytes, kSkinFontBytes_size);
        if (io == nullptr) {
            complain_text("SDL_IOFromConstMem(the embedded face)");
            close();
            return false;
        }
        font_ = TTF_OpenFontIO(io, /*closeio=*/true, kSkinFontPt);
        if (font_ == nullptr) {
            complain_text("TTF_OpenFontIO(the embedded face)");
            close();
            return false;
        }
        int w = 0;
        int h = 0;
        if (!TTF_GetStringSize(font_, "MMMMMMMMMM", static_cast<std::size_t>(kAdvanceSample), &w,
                               &h)) {
            complain_text("TTF_GetStringSize(measuring the face)");
            close();
            return false;
        }
        advance_ = (w + kAdvanceSample / 2) / kAdvanceSample;
        line_ = TTF_GetFontLineSkip(font_);
        if (advance_ <= 0 || line_ <= 0) {
            std::fprintf(stderr,
                         "zengine-skin-sdl: the embedded face measured %d px advance and %d px "
                         "line height, which is not a usable metric; falling back to the "
                         "bitmap face.\n",
                         static_cast<int>(advance_), static_cast<int>(line_));
            std::fflush(stderr);
            close();
            return false;
        }
        engine_ = TTF_CreateRendererTextEngine(renderer);
        if (engine_ == nullptr) {
            complain_text("TTF_CreateRendererTextEngine");
            close();
            return false;
        }
        return true;
    }

    void close() {
        if (engine_ != nullptr) {
            TTF_DestroyRendererTextEngine(engine_);
            engine_ = nullptr;
        }
        if (font_ != nullptr) {
            TTF_CloseFont(font_);
            font_ = nullptr;
        }
        if (inited_) {
            TTF_Quit();
            inited_ = false;
        }
        advance_ = 0;
        line_ = 0;
    }

    /// Is a real face drawing? The metric answers zero whenever it is not, so "no font" and "text
    /// is a cell" are one sentence.
    bool live() const noexcept { return engine_ != nullptr && font_ != nullptr; }

    std::int64_t advance_px() const noexcept { return live() ? advance_ : 0; }
    std::int64_t line_px() const noexcept { return live() ? line_ : 0; }

    /// Draw one resolved region inside its own viewport, which is the clip and the local origin
    /// at once, so no window coordinate reaches this loop. The previous viewport is always
    /// restored, and "no viewport" is restored as none: `SDL_GetRenderViewport` answers the whole
    /// target when none is set, and setting that back pins the viewport, which then stops growing
    /// with the window (a window dragged larger would keep clipping to its old width). Hence
    /// `SDL_RenderViewportSet` first.
    void draw(SDL_Renderer* renderer, const PlanTextRegion& p) {
        if (!live() || renderer == nullptr || p.view.empty()) {
            return;
        }
        SDL_Rect previous{};
        const bool had = SDL_RenderViewportSet(renderer) &&
                         SDL_GetRenderViewport(renderer, &previous);
        const SDL_Rect vp{static_cast<int>(p.view.x), static_cast<int>(p.view.y),
                          static_cast<int>(p.view.w), static_cast<int>(p.view.h)};
        if (!SDL_SetRenderViewport(renderer, &vp)) {
            complain_text("SDL_SetRenderViewport(a text region)");
            return; // nothing was drawn and nothing was disturbed
        }
        // The region takes its rectangle first, since it is an overlay -- unless its publisher
        // said the rectangle is not its to take (`kGroundBeneath`): then the rows are set on
        // the material this layer already drew.
        if (p.ground != kGroundBeneath) {
            SDL_SetRenderDrawColor(renderer, p.background.r, p.background.g, p.background.b,
                                   SDL_ALPHA_OPAQUE);
            const SDL_FRect whole{0.0F, 0.0F, static_cast<float>(p.view.w),
                                  static_cast<float>(p.view.h)};
            SDL_RenderFillRect(renderer, &whole);
        }
        for (std::size_t i = 0; i < p.rows.size(); ++i) {
            const PlanTextRow& row = p.rows[i];
            const float top = static_cast<float>(p.origin_y +
                                                 static_cast<std::int64_t>(i) * p.line_px);
            // A row's own ground, only where it differs from the region's (a row that asked for
            // none resolved to it). The strip spans the region's width: "this row, all of it".
            if (!(row.background == p.background)) {
                SDL_SetRenderDrawColor(renderer, row.background.r, row.background.g,
                                       row.background.b, SDL_ALPHA_OPAQUE);
                const SDL_FRect strip{0.0F, top, static_cast<float>(p.view.w),
                                      static_cast<float>(p.line_px)};
                SDL_RenderFillRect(renderer, &strip);
            }
        }
        // The selection bands, after every row's ground and before any text, so the glyphs keep
        // their ink and sit on the band: this face's reverse video.
        if (!p.selection.empty()) {
            SDL_SetRenderDrawColor(renderer, kSelectionBand.r, kSelectionBand.g,
                                   kSelectionBand.b, SDL_ALPHA_OPAQUE);
            for (const PlanSelectionBand& band : p.selection) {
                const SDL_FRect fill{static_cast<float>(band.x), static_cast<float>(band.y),
                                     static_cast<float>(band.w), static_cast<float>(band.h)};
                SDL_RenderFillRect(renderer, &fill);
            }
        }
        for (std::size_t i = 0; i < p.rows.size(); ++i) {
            const PlanTextRow& row = p.rows[i];
            const float top = static_cast<float>(p.origin_y +
                                                 static_cast<std::int64_t>(i) * p.line_px);
            if (row.text.empty()) {
                continue; // a blank row is a row with nothing in it, not a row to draw
            }
            TTF_Text* t = TTF_CreateText(engine_, font_, row.text.c_str(), row.text.size());
            if (t == nullptr) {
                complain_text("TTF_CreateText");
                continue;
            }
            TTF_SetTextColor(t, row.ink.r, row.ink.g, row.ink.b, SDL_ALPHA_OPAQUE);
            TTF_DrawRendererText(t, static_cast<float>(p.origin_x), top);
            TTF_DestroyText(t);
        }
        // The caret last, on top of the text it sits in: a bar `plan_caret` placed from the rows'
        // own fit, clipped by the viewport like everything else. Nothing blinks: a blink is a
        // clock, and this Skin paints when a canvas arrives.
        if (p.caret.present) {
            SDL_SetRenderDrawColor(renderer, p.caret.ink.r, p.caret.ink.g, p.caret.ink.b,
                                   SDL_ALPHA_OPAQUE);
            const SDL_FRect bar{static_cast<float>(p.caret.x), static_cast<float>(p.caret.y),
                                static_cast<float>(p.caret.w), static_cast<float>(p.caret.h)};
            SDL_RenderFillRect(renderer, &bar);
        }
        SDL_SetRenderViewport(renderer, had ? &previous : nullptr);
    }

private:
    /// skin_sdl.cpp's `complain`, restated: this file is included by exactly one translation
    /// unit, and a helper shared between the two halves of one edge would be ceremony.
    static void complain_text(const char* what) {
        const char* why = SDL_GetError();
        std::fprintf(stderr, "zengine-skin-sdl: %s failed: %s\n", what,
                     (why != nullptr && why[0] != '\0') ? why : "(SDL gave no reason)");
        std::fflush(stderr);
    }

    TTF_Font* font_ = nullptr;
    TTF_TextEngine* engine_ = nullptr;
    bool inited_ = false;
    std::int64_t advance_ = 0;
    std::int64_t line_ = 0;
};

} // namespace zengine::surface

#endif // ZENGINE_SURFACE_SKIN_SDL_TEXT_HPP
