// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The loadable weave `zengine-window-sdl`: the OS window, through SDL3. It opens its window when it
// is activated, draws each picture through SDL's renderer, and empties SDL's queue on its beat,
// translated by Zengine's own SDL translator (input/translate_sdl_core.hpp), claiming the queue so
// an SDL Input reader beside it reads none of it (input/window_claim.hpp). Reloaded, it leaves its
// window and renderer to the instance after it through SDL's global properties. With no display
// it says why once, holds no SDL and no claim, and still keeps and composes its picture.

#include "graphics/window/window_sdl.hpp"

#include "input/translate_sdl_core.hpp"
#include "input/window_claim.hpp"

#include <zen/kernel/export.hpp>

#include <SDL3/SDL.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace {

namespace win = zengine::graphics::window;
namespace light = zengine::graphics::light;

/// SDL's global properties, shared by every instance in the process: the window a reloaded
/// instance leaves to the next, and how many instances that may take it are alive.
constexpr const char* kHanded = "zengine.window.sdl.handed";
constexpr const char* kLiving = "zengine.window.sdl.instances";

/// Give back this instance's hold on SDL's video, and shut SDL down only if nobody else holds it,
/// as the SDL Skin and the SDL Input reader do: two libraries cannot share a private counter.
void release_sdl() {
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    if (SDL_WasInit(0) == 0) {
        SDL_Quit();
    }
}

void complain(const char* what) {
    const char* why = SDL_GetError();
    std::fprintf(stderr,
                 "zengine-window-sdl: %s failed: %s\n"
                 "  no OS window is shown; the window keeps and composes its picture.\n",
                 what, (why != nullptr && why[0] != '\0') ? why : "(SDL gave no reason)");
    std::fflush(stderr);
}

std::int64_t living(std::int64_t step) {
    const SDL_PropertiesID global = SDL_GetGlobalProperties();
    const std::int64_t n = SDL_GetNumberProperty(global, kLiving, 0) + step;
    SDL_SetNumberProperty(global, kLiving, n);
    return n;
}

class ProcessSdlBackend {
public:
    /// SDL's queue claimed from the start, so a reload in place, which makes the next instance
    /// before it ends this one, never leaves the queue unclaimed; and counted among the living
    /// when SDL's video is up already: an instance before it may hold a window to leave it.
    ProcessSdlBackend() {
        zengine::input::claim_window_sdl_input();
        input_claimed_ = true;
        if (SDL_WasInit(SDL_INIT_VIDEO) != 0) {
            counted_ = true;
            (void)living(1);
        }
    }

    ~ProcessSdlBackend() {
        give_back_input();
        drop_textures();
        const bool up = counted_ && SDL_WasInit(SDL_INIT_VIDEO) != 0;
        const bool others = up && living(-1) > 0;
        if (window_ != nullptr && others) {
            SDL_SetPointerProperty(SDL_GetGlobalProperties(), kHanded, window_);
            return; // the window, its renderer and this hold on SDL's video go to the next
        }
        // No one is left to take a window an instance before this one left: it closes, and the
        // hold that came with it goes.
        SDL_Window* left = up && !others ? take_handed() : nullptr;
        if (window_ != nullptr) {
            close(window_);
        }
        if (left != nullptr) {
            close(left);
            release_sdl();
        }
        if (held_) {
            release_sdl();
        }
    }

    ProcessSdlBackend(const ProcessSdlBackend&) = delete;
    ProcessSdlBackend& operator=(const ProcessSdlBackend&) = delete;

    /// SDL's video taken, and the window an instance before this one left, or a new one.
    void claim() {
        if (claimed_) {
            return;
        }
        claimed_ = true;
        if (!SDL_Init(SDL_INIT_VIDEO)) {
            complain("SDL_Init(SDL_INIT_VIDEO)");
            release_sdl(); // a failed init still allocated: see release_sdl
            give_back_input();
            return;
        }
        held_ = true;
        if (!counted_) {
            counted_ = true;
            (void)living(1);
        }
        if (SDL_Window* handed = take_handed()) {
            window_ = handed;
            renderer_ = SDL_GetRenderer(handed);
            SDL_QuitSubSystem(SDL_INIT_VIDEO); // the hold the last instance kept came with it
            return;
        }
        open();
    }

    win::PixelSize size() const {
        int w = 0, h = 0;
        if (window_ == nullptr || !SDL_GetWindowSizeInPixels(window_, &w, &h)) {
            return {};
        }
        return win::PixelSize{w, h};
    }

    /// The picture drawn, back to front, each area clipped to its bounds: its quads as one
    /// geometry batch until a node's lines, text or images break it; then presented. False with no
    /// renderer to draw it.
    bool present(const win::ComposedLightFrame& frame) {
        if (renderer_ == nullptr) {
            return false;
        }
        SDL_SetRenderClipRect(renderer_, nullptr);
        ink(frame.ground);
        SDL_RenderClear(renderer_);
        std::set<std::string> seen;
        for (const win::LightAreaShown& a : frame.areas) {
            const SDL_Rect clip{static_cast<int>(a.x), static_cast<int>(a.y),
                                static_cast<int>(a.w), static_cast<int>(a.h)};
            SDL_SetRenderClipRect(renderer_, &clip);
            batch_.clear();
            for (const win::LightNodeShown& n : a.nodes) {
                for (const light::Quad& q : n.prims->quads) {
                    add_quad(static_cast<float>(a.x + q.x), static_cast<float>(a.y + q.y),
                             static_cast<float>(q.w), static_cast<float>(q.h), q.rgba);
                }
                const light::NodePrimitives& p = *n.prims;
                if (p.lines.empty() && p.texts.empty() && p.images.empty()) {
                    continue;
                }
                flush();
                const std::string key = a.area + '\x1f' + n.path;
                seen.insert(key);
                draw_rest(p, key, n.version, a.x, a.y);
            }
            flush();
        }
        SDL_SetRenderClipRect(renderer_, nullptr);
        for (auto it = textures_.begin(); it != textures_.end();) {
            if (seen.count(it->first) == 0) {
                destroy(it->second.made);
                it = textures_.erase(it);
            } else {
                ++it;
            }
        }
        return SDL_RenderPresent(renderer_);
    }

    /// What SDL's queue holds, emptied in queue order: keys and their releases, their text, and
    /// the pointer's moves, buttons and wheel translated; a close asked of the window or of the
    /// application is said; an uncovered or resized window asks to be drawn again.
    win::PixelPolled poll() {
        namespace in = zengine::input;
        win::PixelPolled out;
        if (window_ == nullptr) {
            return out;
        }
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            std::vector<in::InputEvent> moments;
            switch (ev.type) {
            case SDL_EVENT_QUIT:
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                out.close = true;
                break;
            case SDL_EVENT_KEY_DOWN:
            case SDL_EVENT_KEY_UP:
                moments = in::sdl_key_moments(static_cast<std::int64_t>(ev.key.scancode),
                                              static_cast<std::uint16_t>(ev.key.mod), ev.key.down,
                                              ev.key.repeat);
                break;
            case SDL_EVENT_TEXT_INPUT:
                moments = in::sdl_text_moments(ev.text.text);
                break;
            case SDL_EVENT_MOUSE_MOTION:
                moments = in::sdl_mouse_motion_moments(ev.motion.x, ev.motion.y, ev.motion.xrel,
                                                       ev.motion.yrel);
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            case SDL_EVENT_MOUSE_BUTTON_UP:
                moments = in::sdl_mouse_button_moments(static_cast<std::int64_t>(ev.button.button),
                                                       ev.button.down, ev.button.x, ev.button.y);
                break;
            case SDL_EVENT_MOUSE_WHEEL:
                moments = in::sdl_mouse_wheel_moments(
                    ev.wheel.x, ev.wheel.y, static_cast<std::uint32_t>(ev.wheel.direction),
                    ev.wheel.mouse_x, ev.wheel.mouse_y);
                break;
            case SDL_EVENT_WINDOW_EXPOSED:
            case SDL_EVENT_WINDOW_RESIZED:
            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                out.redraw = true;
                break;
            default:
                break;
            }
            out.events.insert(out.events.end(), moments.begin(), moments.end());
        }
        return out;
    }

private:
    struct Made {
        std::uint64_t version = 0;
        std::vector<SDL_Texture*> made;
    };

    /// This instance's claim on SDL's queue given back, once: when it goes, or when it finds it
    /// has no window to read the queue for.
    void give_back_input() {
        if (input_claimed_) {
            input_claimed_ = false;
            zengine::input::release_window_sdl_input();
        }
    }

    static SDL_Window* take_handed() {
        const SDL_PropertiesID global = SDL_GetGlobalProperties();
        auto* handed = static_cast<SDL_Window*>(SDL_GetPointerProperty(global, kHanded, nullptr));
        if (handed != nullptr) {
            SDL_ClearProperty(global, kHanded);
        }
        return handed;
    }

    static void close(SDL_Window* window) {
        if (SDL_Renderer* r = SDL_GetRenderer(window)) {
            SDL_DestroyRenderer(r);
        }
        SDL_DestroyWindow(window);
    }

    /// The window and its renderer. Never focusable when ZENGINE_SDL_NOT_FOCUSABLE=1, so a suite
    /// or a probe leaves the desktop's keyboard with whoever is using it.
    void open() {
        const char* unfocused = std::getenv("ZENGINE_SDL_NOT_FOCUSABLE");
        SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE;
        if (unfocused != nullptr && unfocused[0] == '1') {
            flags |= SDL_WINDOW_NOT_FOCUSABLE;
        }
        window_ = SDL_CreateWindow("Zengine", static_cast<int>(win::kDefaultWidth),
                                   static_cast<int>(win::kDefaultHeight), flags);
        if (window_ == nullptr) {
            complain("SDL_CreateWindow");
            held_ = false;
            release_sdl();
            give_back_input();
            return;
        }
        renderer_ = SDL_CreateRenderer(window_, nullptr);
        if (renderer_ == nullptr) {
            complain("SDL_CreateRenderer");
            SDL_DestroyWindow(window_);
            window_ = nullptr;
            held_ = false;
            release_sdl();
            give_back_input();
            return;
        }
        SDL_StartTextInput(window_);
    }

    void ink(std::uint32_t rgba) {
        SDL_SetRenderDrawColor(renderer_, static_cast<Uint8>((rgba >> 24) & 0xffu),
                               static_cast<Uint8>((rgba >> 16) & 0xffu),
                               static_cast<Uint8>((rgba >> 8) & 0xffu),
                               static_cast<Uint8>(rgba & 0xffu));
    }

    void add_quad(float x, float y, float w, float h, std::uint32_t rgba) {
        const SDL_FColor c{static_cast<float>((rgba >> 24) & 0xffu) / 255.0f,
                           static_cast<float>((rgba >> 16) & 0xffu) / 255.0f,
                           static_cast<float>((rgba >> 8) & 0xffu) / 255.0f,
                           static_cast<float>(rgba & 0xffu) / 255.0f};
        const int base = static_cast<int>(batch_.size());
        batch_.push_back(SDL_Vertex{SDL_FPoint{x, y}, c, SDL_FPoint{0, 0}});
        batch_.push_back(SDL_Vertex{SDL_FPoint{x + w, y}, c, SDL_FPoint{0, 0}});
        batch_.push_back(SDL_Vertex{SDL_FPoint{x + w, y + h}, c, SDL_FPoint{0, 0}});
        batch_.push_back(SDL_Vertex{SDL_FPoint{x, y + h}, c, SDL_FPoint{0, 0}});
        for (const int i : {0, 1, 2, 0, 2, 3}) {
            indices_.push_back(base + i);
        }
    }

    void flush() {
        if (!batch_.empty()) {
            SDL_RenderGeometry(renderer_, nullptr, batch_.data(), static_cast<int>(batch_.size()),
                               indices_.data(), static_cast<int>(indices_.size()));
        }
        batch_.clear();
        indices_.clear();
    }

    /// A node's lines, text and images; an image's texture made when its node was painted anew.
    void draw_rest(const light::NodePrimitives& p, const std::string& key, std::uint64_t version,
                   std::int64_t ox, std::int64_t oy) {
        for (const light::Line& l : p.lines) {
            ink(l.rgba);
            SDL_RenderLine(renderer_, static_cast<float>(ox + l.x1), static_cast<float>(oy + l.y1),
                           static_cast<float>(ox + l.x2), static_cast<float>(oy + l.y2));
        }
        for (const light::TextRun& t : p.texts) {
            ink(t.rgba);
            SDL_RenderDebugText(renderer_, static_cast<float>(ox + t.x),
                                static_cast<float>(oy + t.y + 2), t.text.c_str());
        }
        if (p.images.empty()) {
            return;
        }
        Made& m = textures_[key];
        if (m.version != version || m.made.size() != p.images.size()) {
            destroy(m.made);
            m.version = version;
            for (const light::ImageBlit& im : p.images) {
                SDL_Texture* tex = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA32,
                                                     SDL_TEXTUREACCESS_STATIC, im.pw, im.ph);
                if (tex != nullptr) {
                    SDL_UpdateTexture(tex, nullptr, im.rgba.data(), im.pw * 4);
                }
                m.made.push_back(tex);
            }
        }
        for (std::size_t i = 0; i < p.images.size(); ++i) {
            if (m.made[i] == nullptr) {
                continue;
            }
            const light::ImageBlit& im = p.images[i];
            const SDL_FRect dst{static_cast<float>(ox + im.x), static_cast<float>(oy + im.y),
                                static_cast<float>(im.w), static_cast<float>(im.h)};
            SDL_RenderTexture(renderer_, m.made[i], nullptr, &dst);
        }
    }

    static void destroy(std::vector<SDL_Texture*>& made) {
        for (SDL_Texture* t : made) {
            if (t != nullptr) {
                SDL_DestroyTexture(t);
            }
        }
        made.clear();
    }

    void drop_textures() {
        for (auto& [key, m] : textures_) {
            destroy(m.made);
        }
        textures_.clear();
    }

    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    bool claimed_ = false;
    bool input_claimed_ = false; ///< this instance holds a claim on SDL's queue
    bool held_ = false;          ///< this instance holds SDL's video
    bool counted_ = false;       ///< counted among the living
    std::vector<SDL_Vertex> batch_;
    std::vector<int> indices_;
    std::map<std::string, Made> textures_;
};

using SdlWindow = win::SdlWindowT<ProcessSdlBackend>;

} // namespace

ZEN_EXPORT_WEAVE(SdlWindow)
