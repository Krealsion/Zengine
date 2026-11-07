// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_GRAPHICS_WINDOW_WINDOW_SDL_FACE_HPP
#define ZENGINE_GRAPHICS_WINDOW_WINDOW_SDL_FACE_HPP

// The faces the OS window sets its text in: embedded JetBrains Mono, regular and bold, at every
// size a palette names, measured and drawn a character a column; what it lacks drawn from the
// platform's fonts, then from embedded GNU Unifont, the last resort, each such glyph in its own
// columns. `ZENGINE_SDL_EMBEDDED_FACES=1` keeps it to the embedded faces. Reference:
// graphics/docs/zengine.window.sdl.md, "Text"; provenance: graphics/window/fonts/PROVENANCE.md.

#include "component/text.hpp"
#include "graphics/vocabulary.hpp"

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <string>
#include <system_error>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

namespace zengine::graphics::window {

/// The embedded faces, generated from `graphics/window/fonts/` by cmake/EmbedBinary.cmake:
/// JetBrains Mono Regular and Bold, and GNU Unifont and Unifont Upper, the last resort.
extern const unsigned char kWindowFontBytes[];
extern const std::size_t kWindowFontBytes_size;
extern const unsigned char kWindowBoldFontBytes[];
extern const std::size_t kWindowBoldFontBytes_size;
extern const unsigned char kLastResortFontBytes[];
extern const std::size_t kLastResortFontBytes_size;
extern const unsigned char kLastResortUpperFontBytes[];
extern const std::size_t kLastResortUpperFontBytes_size;

/// The family the window embeds, and the one it sets a face it does not know in.
inline constexpr const char* kEmbeddedFace = "JetBrains Mono";

/// The point size text stands at where nothing names one: at 13 points the embedded face advances
/// 8 pixels with an 18-pixel line.
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

/// The environment variable that keeps the window to its embedded faces: `1` draws every
/// character from them, none from the platform's fonts.
inline constexpr const char* kEmbeddedFacesVariable = "ZENGINE_SDL_EMBEDDED_FACES";

/// The most platform font files the window looks in for a character, in the order it looks.
inline constexpr std::size_t kMaxPlatformFaces = 256;

/// The most glyphs the window keeps made for one renderer; past it, it makes them again.
inline constexpr std::size_t kMaxGlyphs = 8192;

/// THE PLATFORM'S FONT FILES THE WINDOW LOOKS IN, in order: on Windows its font folder's, a few
/// faces known to hold many characters first; elsewhere the folders fontconfig and macOS read,
/// monospaced faces first. At most `kMaxPlatformFaces`; none where none is found.
inline std::vector<std::string> platform_font_files() {
    namespace fs = std::filesystem;
    std::vector<fs::path> roots;
#if defined(_WIN32)
    const char* windir = std::getenv("WINDIR");
    roots.emplace_back(fs::path(windir != nullptr ? windir : "C:\\Windows") / "Fonts");
    const char* local = std::getenv("LOCALAPPDATA");
    if (local != nullptr) {
        roots.emplace_back(fs::path(local) / "Microsoft" / "Windows" / "Fonts");
    }
    const std::vector<std::string> first = {"consola.ttf", "cour.ttf",    "seguisym.ttf",
                                            "msgothic.ttc", "msyh.ttc",   "simsun.ttc",
                                            "malgun.ttf",   "Nirmala.ttc", "seguiemj.ttf"};
#else
    const char* home = std::getenv("HOME");
#if defined(__APPLE__)
    roots.emplace_back("/System/Library/Fonts");
    roots.emplace_back("/Library/Fonts");
    if (home != nullptr) {
        roots.emplace_back(fs::path(home) / "Library" / "Fonts");
    }
#else
    roots.emplace_back("/usr/share/fonts");
    roots.emplace_back("/usr/local/share/fonts");
    const char* data = std::getenv("XDG_DATA_HOME");
    if (data != nullptr && data[0] != '\0') {
        roots.emplace_back(fs::path(data) / "fonts");
    } else if (home != nullptr) {
        roots.emplace_back(fs::path(home) / ".local" / "share" / "fonts");
    }
    if (home != nullptr) {
        roots.emplace_back(fs::path(home) / ".fonts");
    }
#endif
    const std::vector<std::string> first = {};
#endif
    std::vector<std::string> found;
    for (const fs::path& root : roots) {
        std::error_code ec;
        fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec);
        for (; !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
            if (!it->is_regular_file(ec)) {
                continue;
            }
            std::string ext = it->path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (ext == ".ttf" || ext == ".otf" || ext == ".ttc") {
                found.push_back(it->path().string());
            }
            if (found.size() >= 4 * kMaxPlatformFaces) {
                break;
            }
        }
    }
    const auto rank = [&first](const std::string& path) {
        const std::string name = std::filesystem::path(path).filename().string();
        for (std::size_t i = 0; i < first.size(); ++i) {
            if (name == first[i]) {
                return static_cast<int>(i);
            }
        }
        return name.find("Mono") != std::string::npos || name.find("mono") != std::string::npos
                   ? static_cast<int>(first.size())
                   : static_cast<int>(first.size()) + 1;
    };
    std::stable_sort(found.begin(), found.end(), [&](const std::string& a, const std::string& b) {
        const int ra = rank(a), rb = rank(b);
        return ra != rb ? ra < rb : a < b;
    });
    if (found.size() > kMaxPlatformFaces) {
        found.resize(kMaxPlatformFaces);
    }
    return found;
}

/// The OS window's faces for one window's life: opened and measured with no renderer needed, their
/// glyphs made for the renderer they are drawn by, and dropped with it.
class SdlFace {
public:
    SdlFace() = default;
    ~SdlFace() { close(); }
    SdlFace(const SdlFace&) = delete;
    SdlFace& operator=(const SdlFace&) = delete;

    /// The faces opened and the body size measured, once; a face that will not open, or measures to
    /// nothing, says why and leaves the debug face standing.
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
        const Sized* body = sized(static_cast<std::int64_t>(kWindowFontPt));
        if (body == nullptr) {
            close();
            return;
        }
        measure_ = body->measure;
        fallbacks_.push_back(Fallback{{}, kLastResortFontBytes, kLastResortFontBytes_size, {}});
        fallbacks_.push_back(
            Fallback{{}, kLastResortUpperFontBytes, kLastResortUpperFontBytes_size, {}});
        const char* only = std::getenv(kEmbeddedFacesVariable);
        if (only == nullptr || std::string(only) != "1") {
            std::vector<Fallback> platform;
            for (std::string& path : platform_font_files()) {
                platform.push_back(Fallback{std::move(path), nullptr, 0, {}});
            }
            fallbacks_.insert(fallbacks_.begin(), std::make_move_iterator(platform.begin()),
                              std::make_move_iterator(platform.end()));
        }
    }

    /// The body size's measure: the embedded face's, or the debug face's where it did not open.
    TextMeasure measure() const noexcept { return opened_face() ? measure_ : kDebugFace; }

    /// How text is set in `face` at `points`, regular: the embedded face measured at that size,
    /// which sets a face it does not know too; the body's measure where `points` names none, and
    /// the debug face's where no face opened.
    TextMeasure measure(const std::string& face, std::int64_t points) {
        (void)face;
        if (!opened_face()) {
            return kDebugFace;
        }
        if (points <= 0) {
            return measure_;
        }
        const Sized* s = sized(points);
        return s != nullptr ? s->measure : measure_;
    }

    /// A run of text at (`x`, `y`), the top left of its line, in 0xRRGGBBAA: each character at its
    /// column, `advance` pixels a column (the size's own where 0), set at `points` (the body where
    /// 0), regular or bold; the debug face's glyphs, one a column, where no face opened.
    void draw(SDL_Renderer* renderer, float x, float y, std::uint32_t rgba, const std::string& text,
              std::int64_t advance = 0, std::int64_t line = 0, std::int64_t points = 0,
              std::int64_t weight = text_weight::kRegular, const std::string& face = {}) {
        (void)face;
        const auto r = static_cast<Uint8>((rgba >> 24) & 0xffu);
        const auto g = static_cast<Uint8>((rgba >> 16) & 0xffu);
        const auto b = static_cast<Uint8>((rgba >> 8) & 0xffu);
        const auto a = static_cast<Uint8>(rgba & 0xffu);
        if (!opened_face()) {
            const TextMeasure m = kDebugFace;
            SDL_SetRenderDrawColor(renderer, r, g, b, a);
            const auto inset_x =
                static_cast<float>(m.advance - SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE) / 2.0F;
            const auto inset_y =
                static_cast<float>(m.line - SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE) / 2.0F;
            for (std::size_t i = 0; i < text.size(); i = component::character_end(text, i)) {
                const int w = component::character_columns(text, i);
                if (w == 0) {
                    continue;
                }
                const auto c = static_cast<unsigned char>(text[i]);
                const char one[2] = {c >= 0x20 && c < 0x7f ? static_cast<char>(c) : '?', '\0'};
                SDL_RenderDebugText(renderer, x + inset_x, y + inset_y, one);
                x += static_cast<float>(m.advance * w);
            }
            return;
        }
        if (renderer != renderer_) {
            drop();
            renderer_ = renderer;
        }
        const std::int64_t pt = points > 0 ? points : static_cast<std::int64_t>(kWindowFontPt);
        const Sized* s = sized(pt);
        if (s == nullptr) {
            return;
        }
        const std::int64_t adv = advance > 0 ? advance : s->measure.advance;
        const std::int64_t tall = line > 0 ? line : s->measure.line;
        const bool bold = weight == text_weight::kBold;
        std::size_t i = 0;
        while (i < text.size()) {
            // ONE CHARACTER AND THE CHARACTERS OF NO COLUMN AFTER IT, drawn as one glyph.
            const int w = component::character_columns(text, i);
            std::size_t end = component::character_end(text, i);
            while (end < text.size() && component::character_columns(text, end) == 0) {
                end = component::character_end(text, end);
            }
            const std::string cluster = text.substr(i, end - i);
            const char32_t cp = component::code_point_at(text, i);
            i = end;
            const int cols = w > 0 ? w : 1;
            if (w > 0 && cp != U' ' && cp != U'\u3000') {
                if (const Glyph* gl = glyph(cluster, cp, pt, bold)) {
                    SDL_SetTextureColorMod(gl->texture, r, g, b);
                    SDL_SetTextureAlphaMod(gl->texture, a);
                    const SDL_FRect at = place(*gl, x, y, adv * cols, tall);
                    SDL_RenderTexture(renderer, gl->texture, nullptr, &at);
                }
            }
            x += static_cast<float>(adv * (w > 0 ? w : 0));
        }
    }

    /// Every glyph made for the renderer dropped, before it goes.
    void drop() {
        for (auto& [key, g] : glyphs_) {
            if (g.texture != nullptr) {
                SDL_DestroyTexture(g.texture);
            }
        }
        glyphs_.clear();
        renderer_ = nullptr;
    }

private:
    /// The embedded face at one size: regular and bold, and its measure.
    struct Sized {
        TTF_Font* regular = nullptr;
        TTF_Font* bold = nullptr;
        TextMeasure measure;
    };

    /// A face the window draws what the embedded one lacks from: a platform file, or embedded bytes;
    /// opened at a size the first time it is asked, `probe` at the body's to say what it holds.
    struct Fallback {
        std::string path;
        const unsigned char* bytes = nullptr;
        std::size_t size = 0;
        std::map<std::pair<std::int64_t, bool>, TTF_Font*> fonts;
        TTF_Font* probe = nullptr;
        bool dead = false;
    };

    /// A glyph made for the renderer: its texture and size, and whether a fallback face drew it.
    struct Glyph {
        SDL_Texture* texture = nullptr;
        float w = 0, h = 0;
        bool fallback = false;
    };

    bool opened_face() const noexcept { return !sizes_.empty(); }

    static void complain(const char* what) {
        const char* why = SDL_GetError();
        std::fprintf(stderr,
                     "zengine-window-sdl: %s failed: %s\n  text is set in SDL's debug face.\n",
                     what, (why != nullptr && why[0] != '\0') ? why : "(SDL gave no reason)");
        std::fflush(stderr);
    }

    static TTF_Font* open_bytes(const unsigned char* bytes, std::size_t size, std::int64_t pt) {
        SDL_IOStream* io = SDL_IOFromConstMem(bytes, size);
        return io == nullptr ? nullptr
                             : TTF_OpenFontIO(io, /*closeio=*/true, static_cast<float>(pt));
    }

    /// The embedded face at `pt` points, opened and measured the first time it is asked; nothing,
    /// said once, where it will not open or measures to nothing.
    const Sized* sized(std::int64_t pt) {
        if (!inited_) {
            return nullptr;
        }
        pt = std::clamp<std::int64_t>(pt, 6, 96);
        if (const auto it = sizes_.find(pt); it != sizes_.end()) {
            return &it->second;
        }
        if (refused_sizes_.count(pt) != 0) {
            return nullptr;
        }
        Sized s;
        s.regular = open_bytes(kWindowFontBytes, kWindowFontBytes_size, pt);
        if (s.regular == nullptr) {
            complain("TTF_OpenFontIO(the embedded face)");
            refused_sizes_.insert({pt, true});
            return nullptr;
        }
        int w = 0;
        int h = 0;
        if (!TTF_GetStringSize(s.regular, "MMMMMMMMMM", static_cast<std::size_t>(kAdvanceSample), &w,
                               &h)) {
            complain("TTF_GetStringSize(measuring the face)");
            TTF_CloseFont(s.regular);
            refused_sizes_.insert({pt, true});
            return nullptr;
        }
        s.measure = TextMeasure{(w + kAdvanceSample / 2) / kAdvanceSample,
                                TTF_GetFontLineSkip(s.regular)};
        if (s.measure.advance <= 0 || s.measure.line <= 0) {
            std::fprintf(stderr,
                         "zengine-window-sdl: the embedded face measured %d px advance and %d px "
                         "line height at %d points, which is not a usable measure.\n",
                         static_cast<int>(s.measure.advance), static_cast<int>(s.measure.line),
                         static_cast<int>(pt));
            std::fflush(stderr);
            TTF_CloseFont(s.regular);
            refused_sizes_.insert({pt, true});
            return nullptr;
        }
        s.bold = open_bytes(kWindowBoldFontBytes, kWindowBoldFontBytes_size, pt);
        return &sizes_.emplace(pt, s).first->second;
    }

    /// The fallback face that holds `cp`, looked for once a character: -1 where none does.
    int fallback_for(char32_t cp) {
        if (const auto it = which_.find(cp); it != which_.end()) {
            return it->second;
        }
        int found = -1;
        for (std::size_t k = 0; k < fallbacks_.size() && found < 0; ++k) {
            Fallback& f = fallbacks_[k];
            if (f.dead) {
                continue;
            }
            if (f.probe == nullptr) {
                f.probe = f.bytes != nullptr
                              ? open_bytes(f.bytes, f.size, static_cast<std::int64_t>(kWindowFontPt))
                              : TTF_OpenFont(f.path.c_str(), kWindowFontPt);
                if (f.probe == nullptr) {
                    f.dead = true;
                    continue;
                }
            }
            if (TTF_FontHasGlyph(f.probe, static_cast<Uint32>(cp))) {
                found = static_cast<int>(k);
            }
        }
        which_[cp] = found;
        return found;
    }

    /// A fallback face at `pt`, bold or not, opened the first time it is asked.
    TTF_Font* fallback_font(Fallback& f, std::int64_t pt, bool bold) {
        const auto key = std::make_pair(pt, bold);
        if (const auto it = f.fonts.find(key); it != f.fonts.end()) {
            return it->second;
        }
        TTF_Font* font = f.bytes != nullptr ? open_bytes(f.bytes, f.size, pt)
                                            : TTF_OpenFont(f.path.c_str(), static_cast<float>(pt));
        if (font != nullptr && bold) {
            TTF_SetFontStyle(font, TTF_STYLE_BOLD);
        }
        f.fonts.emplace(key, font);
        return font;
    }

    /// A character's glyph, white, made the first time it is drawn at a size and weight: from the
    /// embedded face where it holds the character, else from the first fallback that does; none
    /// where no face will make one.
    const Glyph* glyph(const std::string& cluster, char32_t cp, std::int64_t pt, bool bold) {
        const auto key = std::make_tuple(pt, bold, cluster);
        if (const auto it = glyphs_.find(key); it != glyphs_.end()) {
            return &it->second;
        }
        if (glyphs_.size() >= kMaxGlyphs) {
            drop_glyphs();
        }
        const Sized* s = sized(pt);
        if (s == nullptr) {
            return nullptr;
        }
        TTF_Font* font = bold && s->bold != nullptr ? s->bold : s->regular;
        bool fallback = false;
        std::string shown = cluster;
        if (cp == component::kNotACharacter || cp < 0x20u || (cp >= 0x7Fu && cp < 0xA0u)) {
            shown = "?";
        } else if (!TTF_FontHasGlyph(s->regular, static_cast<Uint32>(cp))) {
            const int k = fallback_for(cp);
            if (k >= 0) {
                if (TTF_Font* f = fallback_font(fallbacks_[static_cast<std::size_t>(k)], pt, bold)) {
                    font = f;
                    fallback = true;
                }
            }
        }
        Glyph g;
        g.fallback = fallback;
        if (SDL_Surface* surface = TTF_RenderText_Blended(font, shown.data(), shown.size(),
                                                          SDL_Color{255, 255, 255, 255})) {
            g.texture = SDL_CreateTextureFromSurface(renderer_, surface);
            SDL_DestroySurface(surface);
            if (g.texture != nullptr) {
                SDL_SetTextureBlendMode(g.texture, SDL_BLENDMODE_BLEND);
                (void)SDL_GetTextureSize(g.texture, &g.w, &g.h);
            }
        }
        return &glyphs_.emplace(key, g).first->second;
    }

    /// Where a glyph is drawn in a character's room `cols_px` wide and `tall` high from (x, y): an
    /// embedded face's glyph as the face set it; a fallback's in its own columns, centred across
    /// and down, scaled down where it would not fit them.
    static SDL_FRect place(const Glyph& g, float x, float y, std::int64_t cols_px,
                           std::int64_t tall) {
        if (!g.fallback || g.w <= 0 || g.h <= 0) {
            return SDL_FRect{x, y, g.w, g.h};
        }
        const auto room_w = static_cast<float>(cols_px);
        const auto room_h = static_cast<float>(tall);
        const float scale = (std::min)({1.0F, room_w / g.w, room_h / g.h});
        const float w = g.w * scale;
        const float h = g.h * scale;
        return SDL_FRect{x + (room_w - w) / 2.0F, y + (room_h - h) / 2.0F, w, h};
    }

    void drop_glyphs() {
        for (auto& [key, g] : glyphs_) {
            if (g.texture != nullptr) {
                SDL_DestroyTexture(g.texture);
            }
        }
        glyphs_.clear();
    }

    void close() {
        drop();
        for (auto& [pt, s] : sizes_) {
            if (s.regular != nullptr) {
                TTF_CloseFont(s.regular);
            }
            if (s.bold != nullptr) {
                TTF_CloseFont(s.bold);
            }
        }
        sizes_.clear();
        for (Fallback& f : fallbacks_) {
            for (auto& [key, font] : f.fonts) {
                if (font != nullptr) {
                    TTF_CloseFont(font);
                }
            }
            if (f.probe != nullptr) {
                TTF_CloseFont(f.probe);
            }
        }
        fallbacks_.clear();
        which_.clear();
        if (inited_) {
            TTF_Quit();
            inited_ = false;
        }
    }

    bool opened_ = false;
    bool inited_ = false;
    TextMeasure measure_ = kDebugFace;
    SDL_Renderer* renderer_ = nullptr;
    std::map<std::int64_t, Sized> sizes_;
    std::map<std::int64_t, bool> refused_sizes_;
    std::vector<Fallback> fallbacks_;
    std::unordered_map<char32_t, int> which_;
    std::map<std::tuple<std::int64_t, bool, std::string>, Glyph> glyphs_;
};

} // namespace zengine::graphics::window

#endif // ZENGINE_GRAPHICS_WINDOW_WINDOW_SDL_FACE_HPP
