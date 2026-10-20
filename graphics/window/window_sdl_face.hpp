// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_GRAPHICS_WINDOW_WINDOW_SDL_FACE_HPP
#define ZENGINE_GRAPHICS_WINDOW_WINDOW_SDL_FACE_HPP

// The face the OS window sets its text in: the embedded JetBrains Mono, opened and measured -- a
// character's advance and a line's height, in pixels, which the window offers -- and each run drawn
// a character every advance, each glyph made once a renderer and inked as it is drawn. A face that
// will not open says why on standard error, and the window sets its text in SDL's own debug face,
// measured as SDL states it. Provenance and licence: graphics/window/fonts/PROVENANCE.md.

#include "graphics/vocabulary.hpp"

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>

namespace zengine::graphics::window {

/// The embedded face, generated from `graphics/window/fonts/JetBrainsMono-Regular.ttf` by
/// cmake/EmbedBinary.cmake.
extern const unsigned char kWindowFontBytes[];
extern const std::size_t kWindowFontBytes_size;

/// The point size, the one number here a person chose: at 13 points this face advances 8 pixels
/// with an 18-pixel line.
inline constexpr float kWindowFontPt = 13.0F;

/// How many characters are measured to derive one advance, so a face whose advance is not a whole
/// number of pixels rounds once.
inline constexpr int kAdvanceSample = 10;

/// The gap below SDL's debug face's character, so its lines do not touch.
inline constexpr std::int64_t kDebugFaceLeading = 4;

/// SDL's debug face, measured as SDL states its character: what the window sets text in when its
/// own face will not open.
inline constexpr TextMeasure kDebugFace{SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE,
                                        SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE + kDebugFaceLeading};

/// The OS window's face for one window's life: opened and measured with no renderer needed, its
/// glyphs made for the renderer they are drawn by, and dropped with it.
class SdlFace {
public:
    SdlFace() = default;
    ~SdlFace() { close(); }
    SdlFace(const SdlFace&) = delete;
    SdlFace& operator=(const SdlFace&) = delete;

    /// The face opened and measured, once; a face that will not open, or measures to nothing, says
    /// why and leaves the debug face standing.
    void open() {
        if (opened_) {
            return;
        }
        opened_ = true;
        if (!TTF_Init()) {
            complain("TTF_Init");
            return;
        }
        inited_ = true;
        SDL_IOStream* io = SDL_IOFromConstMem(kWindowFontBytes, kWindowFontBytes_size);
        if (io == nullptr) {
            complain("SDL_IOFromConstMem(the embedded face)");
            close();
            return;
        }
        font_ = TTF_OpenFontIO(io, /*closeio=*/true, kWindowFontPt);
        if (font_ == nullptr) {
            complain("TTF_OpenFontIO(the embedded face)");
            close();
            return;
        }
        int w = 0;
        int h = 0;
        if (!TTF_GetStringSize(font_, "MMMMMMMMMM", static_cast<std::size_t>(kAdvanceSample), &w,
                               &h)) {
            complain("TTF_GetStringSize(measuring the face)");
            close();
            return;
        }
        const std::int64_t advance = (w + kAdvanceSample / 2) / kAdvanceSample;
        const std::int64_t line = TTF_GetFontLineSkip(font_);
        if (advance <= 0 || line <= 0) {
            std::fprintf(stderr,
                         "zengine-window-sdl: the embedded face measured %d px advance and %d px "
                         "line height, which is not a usable measure; text is set in SDL's debug "
                         "face.\n",
                         static_cast<int>(advance), static_cast<int>(line));
            std::fflush(stderr);
            close();
            return;
        }
        measure_ = TextMeasure{advance, line};
    }

    /// The face's measure: the embedded face's, or the debug face's where it did not open.
    TextMeasure measure() const noexcept { return font_ != nullptr ? measure_ : kDebugFace; }

    /// A run of text at (`x`, `y`), the top left of its line, a character every advance, in
    /// 0xRRGGBBAA: the embedded face's glyphs where it opened, else the debug face's, centred in
    /// each character's room.
    void draw(SDL_Renderer* renderer, float x, float y, std::uint32_t rgba,
              const std::string& text) {
        const TextMeasure m = measure();
        const auto r = static_cast<Uint8>((rgba >> 24) & 0xffu);
        const auto g = static_cast<Uint8>((rgba >> 16) & 0xffu);
        const auto b = static_cast<Uint8>((rgba >> 8) & 0xffu);
        const auto a = static_cast<Uint8>(rgba & 0xffu);
        if (font_ == nullptr) {
            SDL_SetRenderDrawColor(renderer, r, g, b, a);
            const auto inset_x =
                static_cast<float>(m.advance - SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE) / 2.0F;
            const auto inset_y =
                static_cast<float>(m.line - SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE) / 2.0F;
            for (const char ch : text) {
                const char glyph[2] = {ch, '\0'};
                SDL_RenderDebugText(renderer, x + inset_x, y + inset_y, glyph);
                x += static_cast<float>(m.advance);
            }
            return;
        }
        if (renderer != renderer_) {
            drop();
            renderer_ = renderer;
        }
        for (const char ch : text) {
            if (SDL_Texture* t = glyph(ch)) {
                SDL_SetTextureColorMod(t, r, g, b);
                SDL_SetTextureAlphaMod(t, a);
                float w = 0;
                float h = 0;
                (void)SDL_GetTextureSize(t, &w, &h);
                const SDL_FRect at{x, y, w, h};
                SDL_RenderTexture(renderer, t, nullptr, &at);
            }
            x += static_cast<float>(m.advance);
        }
    }

    /// Every glyph made for the renderer dropped, before it goes.
    void drop() {
        for (SDL_Texture*& t : glyphs_) {
            if (t != nullptr) {
                SDL_DestroyTexture(t);
                t = nullptr;
            }
        }
        renderer_ = nullptr;
    }

private:
    static constexpr unsigned char kFirst = 0x21; ///< '!': a space draws nothing
    static constexpr unsigned char kLast = 0x7e;  ///< '~'

    static void complain(const char* what) {
        const char* why = SDL_GetError();
        std::fprintf(stderr,
                     "zengine-window-sdl: %s failed: %s\n  text is set in SDL's debug face.\n",
                     what, (why != nullptr && why[0] != '\0') ? why : "(SDL gave no reason)");
        std::fflush(stderr);
    }

    /// A printable character's glyph, white, made the first time it is drawn; none for a space or
    /// a glyph the face will not make.
    SDL_Texture* glyph(char ch) {
        const auto c = static_cast<unsigned char>(ch);
        if (c < kFirst || c > kLast) {
            return nullptr;
        }
        SDL_Texture*& t = glyphs_[c - kFirst];
        if (t == nullptr) {
            SDL_Surface* s = TTF_RenderText_Blended(font_, &ch, 1, SDL_Color{255, 255, 255, 255});
            if (s == nullptr) {
                return nullptr;
            }
            t = SDL_CreateTextureFromSurface(renderer_, s);
            SDL_DestroySurface(s);
            if (t != nullptr) {
                SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
            }
        }
        return t;
    }

    void close() {
        drop();
        if (font_ != nullptr) {
            TTF_CloseFont(font_);
            font_ = nullptr;
        }
        if (inited_) {
            TTF_Quit();
            inited_ = false;
        }
    }

    TTF_Font* font_ = nullptr;
    bool opened_ = false;
    bool inited_ = false;
    TextMeasure measure_ = kDebugFace;
    SDL_Renderer* renderer_ = nullptr;
    std::array<SDL_Texture*, kLast - kFirst + 1> glyphs_{};
};

} // namespace zengine::graphics::window

#endif // ZENGINE_GRAPHICS_WINDOW_WINDOW_SDL_FACE_HPP
