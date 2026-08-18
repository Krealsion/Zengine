// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_SURFACE_SKIN_SDL_TEXT_HPP
#define ZENGINE_SURFACE_SKIN_SDL_TEXT_HPP

// THE GRAPHICAL MEDIUM'S TYPE — the one part of this package that owns a font.
//
// It is behind the SDL gate and nothing else in the repository includes it, which
// is the same split skin_sdl_plan.hpp / skin_sdl.cpp already keeps: every
// arithmetic decision about a bounded region is pure and pinned on every lane,
// and only the rasterization is here, where a real dependency lives.
//
// WHAT IT OWNS, AND THE WHOLE OF IT:
//
//   the face          embedded bytes, opened once per renderer
//   the metric        MEASURED from that face -- never assumed, never authored
//   the drawing       a resolved PlanTextRegion executed against a real renderer
//
// WHAT IT DELIBERATELY DOES NOT OWN. It never decides how much prose fits, never
// wraps, never truncates by its own judgement, and never learns what a transcript
// is. `fit_region` decided the capacity, in pure code both this medium and the
// publisher call; this file draws what it is handed, where the plan said, and
// clips. That division is G-2's one-measurer rule applied to type: the pane's
// "... 12 earlier" is only true if exactly one party did the measuring, and the
// party that must do it is the one that owns the sentence.
//
// THE POINT SIZE IS THE ONLY AUTHORED NUMBER IN HERE. Everything else -- advance,
// line height, where a baseline sits -- is asked of the opened face, because a
// second opinion about a font's metrics is a second answer with nothing to
// arbitrate it.
//
// FAILURE IS VISIBLE AND IS NOT FATAL. A face that will not open says why, on
// stderr, in SDL's own words (skin_sdl.cpp's `complain` posture), and leaves this
// object not-live. A not-live text engine publishes no metric, which the
// vocabulary already spells "text is a cell", which is exactly what the bitmap
// face draws -- so the pane degrades to the Workshop of before HD-1 rather than
// to a blank rectangle, and the publisher's wrapping follows it there because it
// is wrapping against the metric it was told.

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

/// THE POINT SIZE, and the only number in this file a person chose.
///
/// Bounded and fixed for HD-1: there is no font picker, no scaling gesture and no
/// DPI query, because none of those is readability and each is its own decision
/// with its own evidence. Thirteen was measured rather than guessed -- at this
/// size and this face the advance is 8 device pixels and the line is 18, so a
/// character is materially LARGER than the 5x5-in-12px letterform it replaces
/// (cap height ~12 px against 10) while a row of the minimum pane holds 83
/// characters instead of 56.
///
/// It buys that with rows: the minimum pane is 156 device pixels tall, which is
/// thirteen cells and eight lines of real type. That trade is the honest cost of
/// this phase and it is recorded rather than hidden -- and it moves the right way
/// with the window, because every pixel a person drags the edge by is a pixel this
/// number spends on the record.
inline constexpr float kSkinFontPt = 13.0F;

/// How many characters are measured to derive one character's advance. A single
/// glyph would round once; ten round once between them, which is what makes the
/// answer stable across point sizes on a face whose advance is not a whole number
/// of pixels. (Both candidate faces were measured through this; see
/// `surface/fonts/PROVENANCE.md` on why the one that never needed it was chosen
/// anyway.)
inline constexpr int kAdvanceSample = 10;

/// The graphical medium's text, as an object with a lifetime.
///
/// RAII against the renderer, not against the process: the engine belongs to one
/// SDL_Renderer and dies with it, which is why `open` takes the renderer and
/// `close` is idempotent. TTF_Init/TTF_Quit are refcounted by SDL_ttf itself, so
/// this pairs them and does not track them.
class SdlTypeface {
public:
    SdlTypeface() = default;
    ~SdlTypeface() { close(); }
    SdlTypeface(const SdlTypeface&) = delete;
    SdlTypeface& operator=(const SdlTypeface&) = delete;

    /// OPEN THE EMBEDDED FACE FOR THIS RENDERER, and measure it.
    ///
    /// Answers whether text is live. Every failure path leaves this object
    /// not-live and has already said why: a caller's only correct response is to
    /// carry on with the bitmap face, which is what it does.
    ///
    /// The measurement is part of opening, deliberately. A face that opened but
    /// cannot be measured -- or measures to nothing -- is not a usable face, and
    /// treating it as one would publish a zero advance that every division
    /// downstream would have to defend against.
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
        // The face is BYTES, not a path: nothing is opened from the filesystem,
        // so there is no directory to search, no install to half-arrive and no
        // host font to be missing. `closeio=true` hands the stream to SDL_ttf,
        // which closes it with the font; the bytes themselves are static and
        // outlive everything.
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

    /// Is there a real face drawing? Everything downstream keys off this one
    /// question, and the metric answers zero whenever it is false -- so "no font"
    /// and "text is a cell" are the same sentence rather than two states that
    /// could disagree.
    bool live() const noexcept { return engine_ != nullptr && font_ != nullptr; }

    std::int64_t advance_px() const noexcept { return live() ? advance_ : 0; }
    std::int64_t line_px() const noexcept { return live() ? line_ : 0; }

    /// DRAW ONE RESOLVED REGION, INSIDE ITS OWN VIEWPORT.
    ///
    /// The viewport is the clip AND the local origin in one call, which is why it
    /// is the mechanism this uses rather than a clip rectangle: SDL translates
    /// drawing into it, so a row's coordinates are the ones the plan computed
    /// relative to the region's own upper-left, and no global window coordinate
    /// reaches this loop. That is the property a future background, control or
    /// primitive inside a bounded region would need, and it is not
    /// terminal-shaped in any way.
    ///
    /// THE PREVIOUS VIEWPORT IS RESTORED, always, including when a row fails to
    /// materialize. A renderer left with a region's viewport on it would silently
    /// clip everything drawn afterwards to a rectangle that no longer means
    /// anything -- and the symptom would be somebody else's picture missing, not
    /// this one's.
    ///
    /// AND "NO VIEWPORT" IS RESTORED AS NO VIEWPORT, not as the rectangle it
    /// currently happens to be (HD-2). SDL keeps two different states here and
    /// `SDL_GetRenderViewport` flattens them: a renderer with no viewport of its
    /// own answers with the whole target's rectangle, and setting THAT back makes
    /// the viewport explicit -- after which SDL stops growing it when the output
    /// does. The picture that produced was a Workshop dragged larger whose panels
    /// were still clipped to the old window's width, one frame after the pane had
    /// already reflowed to the new one; measured on HD-1's own shipped code, with
    /// nothing typed, and it needed only that a region be drawn once before the
    /// drag. `SDL_RenderViewportSet` is SDL's own answer to exactly this question,
    /// and asking it is the whole repair.
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
        // THE REGION TAKES ITS RECTANGLE FIRST. See PlanTextRegion: a region is an
        // overlay, and the canvas's painter's order draws every label after every
        // rect -- so without this the panels underneath show straight through it.
        //
        // UNLESS ITS PUBLISHER SAID THE RECTANGLE IS NOT ITS TO TAKE (TYPE-1), which
        // is this one `if` and nothing else. Showing straight through is precisely
        // what `kGroundBeneath` asks for: the rows are set in the real face over the
        // material this layer already drew, so a maker's name reads as type ON the
        // object rather than as a panel laid over the hole where it used to be.
        if (p.ground == kGroundOwn) {
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
            // A ROW'S OWN GROUND, and only when it has one (HD-2). "Has one" is
            // spelled as "differs from the region's", which is what makes this an
            // absence rather than a second flag to keep in step -- a row that asked
            // for nothing was resolved to the region's ground and is already
            // painted. The strip spans the region's whole WIDTH rather than the
            // row's text, because the thing a selected row has to say is "this row,
            // all of it" and a bar the length of the longest candidate would say
            // something about the text instead.
            if (!(row.background == p.background)) {
                SDL_SetRenderDrawColor(renderer, row.background.r, row.background.g,
                                       row.background.b, SDL_ALPHA_OPAQUE);
                const SDL_FRect strip{0.0F, top, static_cast<float>(p.view.w),
                                      static_cast<float>(p.line_px)};
                SDL_RenderFillRect(renderer, &strip);
            }
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
        // THE CARET, LAST, SO IT IS ON TOP OF THE TEXT IT SITS IN (HD-3). It is a filled
        // bar and nothing else: `plan_caret` already decided whether there is one and
        // exactly where, from the SAME `RegionFit` the rows above were positioned with, so
        // this loop cannot put the caret anywhere the text does not agree with. The
        // rectangle is local to the viewport like every other coordinate here, and the
        // viewport is the clip -- a caret past the region's edge is cut by SDL rather than
        // by an arithmetic special case.
        //
        // NOTHING BLINKS. A blink is a clock, and this Skin paints when a canvas arrives.
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
    /// The same four lines and one `if` per fallible call that skin_sdl.cpp's
    /// `complain` is, said here because this file is included by exactly one
    /// translation unit and a shared diagnostic helper between two files that are
    /// really one edge would be ceremony. A surface that cannot exist should say
    /// why it cannot exist; so should a face.
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
