// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_GRAPHICS_WINDOW_WINDOW_SDL_BACKEND_HPP
#define ZENGINE_GRAPHICS_WINDOW_WINDOW_SDL_BACKEND_HPP

// The OS window's pixel backend, through SDL3: the window it opens, centred and kept whole on a
// display; each picture drawn, in its face (graphics/window/window_sdl_face.hpp), and presented or
// read back; SDL's queue emptied and translated by input/translate_sdl_core.hpp under its claim on
// the queue (input/window_claim.hpp); and the clipboard, read only when asked. Reloaded, it leaves
// its window and renderer to the instance after it through SDL's global properties. The weave is
// graphics/window/window_sdl.cpp.

#include "graphics/window/window_sdl.hpp"
#include "graphics/window/window_sdl_face.hpp"

#include "input/translate_sdl_core.hpp"
#include "input/window_claim.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

// The local constants in input/translate_sdl_core.hpp are SDL's, checked by the compiler against
// the real headers here, where both spellings meet: the pure translator includes no SDL. SDL's
// scancode space is the wire's, so a key is translated not at all, and a renumbered key stops
// compiling here.
namespace sdl_constants {
namespace sdl = zengine::input::sdl;
namespace scan = zengine::input::scan;
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
static_assert(scan::kQ == SDL_SCANCODE_Q);
static_assert(scan::kReturn == SDL_SCANCODE_RETURN);
static_assert(scan::kEscape == SDL_SCANCODE_ESCAPE);
static_assert(scan::kBackspace == SDL_SCANCODE_BACKSPACE);
static_assert(scan::kTab == SDL_SCANCODE_TAB);
static_assert(scan::kLeftBracket == SDL_SCANCODE_LEFTBRACKET);
static_assert(scan::kUp == SDL_SCANCODE_UP);
static_assert(scan::kHome == SDL_SCANCODE_HOME);
static_assert(scan::kDelete == SDL_SCANCODE_DELETE);
static_assert(scan::kEnd == SDL_SCANCODE_END);
static_assert(scan::kDown == SDL_SCANCODE_DOWN);
} // namespace sdl_constants

namespace zengine::graphics::window {

/// The backend `zengine-window-sdl` runs over: one OS window and its renderer, for its life.
class ProcessSdlBackend {
public:
    /// SDL's queue claimed from the start, so a reload in place, which makes the next instance
    /// before it ends this one, never leaves the queue unclaimed. With SDL's video up already, an
    /// instance before this one may hold a window to leave it, and this one is counted among the
    /// living. Otherwise SDL's video is taken and the window and its renderer open here; where they
    /// cannot -- no display, no driver SDL can use -- this one is refused whole: it says why on
    /// standard error, gives back SDL and the claim, and throws, so a load of it fails and nothing
    /// of it stays loaded.
    ProcessSdlBackend() {
        zengine::input::claim_window_sdl_input();
        input_claimed_ = true;
        if (SDL_WasInit(SDL_INIT_VIDEO) != 0) {
            counted_ = true;
            (void)living(1);
            return;
        }
        if (!SDL_Init(SDL_INIT_VIDEO)) {
            const std::string why = complain("SDL_Init(SDL_INIT_VIDEO)", kRefused);
            release_sdl(); // a failed init still allocated: see release_sdl
            give_back_input();
            throw std::runtime_error(why);
        }
        held_ = true;
        counted_ = true;
        (void)living(1);
        if (const std::string why = open(kRefused); !why.empty()) {
            (void)living(-1);
            counted_ = false;
            held_ = false;
            release_sdl();
            give_back_input();
            throw std::runtime_error(why);
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

    /// The face opened and measured, which needs no window; and the window this instance opened
    /// when it was made, or, made while SDL's video was up, the window an instance before it left,
    /// else a new one.
    void claim() {
        if (claimed_) {
            return;
        }
        claimed_ = true;
        face_.open();
        if (window_ != nullptr) {
            return;
        }
        if (!SDL_Init(SDL_INIT_VIDEO)) {
            complain("SDL_Init(SDL_INIT_VIDEO)", kNoWindow);
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
        if (!open(kNoWindow).empty()) {
            held_ = false;
            release_sdl();
            give_back_input();
        }
    }

    /// The measure of the face text is set in.
    TextMeasure text() const { return face_.measure(); }

    PixelSize size() const {
        int w = 0, h = 0;
        if (window_ == nullptr || !SDL_GetWindowSizeInPixels(window_, &w, &h)) {
            return {};
        }
        return PixelSize{w, h};
    }

    /// The picture drawn and presented. False with no renderer to draw it.
    bool present(const ComposedLightFrame& frame) {
        if (renderer_ == nullptr) {
            return false;
        }
        draw(frame);
        return SDL_RenderPresent(renderer_);
    }

    /// A picture presented before, drawn again and read back before it is presented again, so the
    /// pixels handed back and the pixels on the desktop are one drawing (a presented backbuffer is
    /// not readable on every renderer): a 24-bit BMP at the window's size in pixels. None with no
    /// window, or when SDL cannot read the drawing back.
    std::optional<CapturedWindowPicture> capture(const ComposedLightFrame& frame) {
        if (renderer_ == nullptr || window_ == nullptr) {
            return std::nullopt;
        }
        draw(frame);
        std::optional<CapturedWindowPicture> out;
        if (SDL_Surface* read = SDL_RenderReadPixels(renderer_, nullptr)) {
            if (SDL_Surface* bgr = SDL_ConvertSurface(read, SDL_PIXELFORMAT_BGR24)) {
                std::string bytes = bmp_of_bgr24(bgr->w, bgr->h, bgr->pitch,
                                                 static_cast<const std::uint8_t*>(bgr->pixels));
                if (!bytes.empty()) {
                    out = CapturedWindowPicture{bgr->w, bgr->h, kCellPx, "image/bmp",
                                                std::move(bytes)};
                }
                SDL_DestroySurface(bgr);
            } else {
                complain("SDL_ConvertSurface", kNoPicture);
            }
            SDL_DestroySurface(read);
        } else {
            complain("SDL_RenderReadPixels", kNoPicture);
        }
        SDL_RenderPresent(renderer_);
        return out;
    }

    /// What SDL's queue holds, emptied in queue order: keys and their releases, their text, and
    /// the pointer's moves, buttons and wheel translated; a close asked of the window or of the
    /// application is said; an uncovered or resized window asks to be drawn again. Nothing the
    /// queue says of the clipboard is read.
    PixelPolled poll() {
        namespace in = zengine::input;
        PixelPolled out;
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

    /// Text onto the platform's clipboard. A refusal is said, and costs nothing else.
    void clipboard_copy(std::string_view text) {
        if (window_ == nullptr) {
            return;
        }
        if (!SDL_SetClipboardText(std::string(text).c_str())) {
            complain("SDL_SetClipboardText", "the text did not reach the clipboard.");
        }
    }

    /// The platform clipboard's text now: the one read of the system's clipboard, made only when
    /// asked and never on the beat. An empty or non-text clipboard is an empty string (the
    /// platform was read); none with no window, since then no read exists.
    std::optional<std::string> clipboard_text() {
        if (window_ == nullptr) {
            return std::nullopt;
        }
        if (!SDL_HasClipboardText()) {
            return std::string();
        }
        char* text = SDL_GetClipboardText(); // SDL's buffer; freed here, never handed on
        std::string out = text != nullptr ? std::string(text) : std::string();
        SDL_free(text);
        return out;
    }

    /// Where the window sits, always as the normal window: SDL reports the work area's corner
    /// while it is maximized, so the normal position is sampled only while it is not, and kept
    /// across a maximized stretch. None with no window, or one never seen unmaximized.
    std::optional<WindowPlacementValue> placement() {
        if (window_ == nullptr) {
            return std::nullopt;
        }
        const bool is_maximized = maximized();
        if (!is_maximized) {
            int x = 0;
            int y = 0;
            if (SDL_GetWindowPosition(window_, &x, &y)) {
                normal_x_ = x;
                normal_y_ = y;
                have_normal_ = true;
            }
        }
        if (!have_normal_) {
            return std::nullopt;
        }
        return WindowPlacementValue{normal_x_, normal_y_, is_maximized};
    }

    /// A remembered placement offered back: the window's frame moved to it, whole on a display
    /// (`placement_within`); with no display truth it is not moved. From here the window is no
    /// longer centred as it grows. Whether to maximize is the medium's to say (`maximize`).
    void place(const WindowPlacementValue& want) {
        if (window_ == nullptr) {
            return;
        }
        centring_ = false;
        const std::optional<Frame> now = frame_of();
        if (!now.has_value()) {
            return; // a window whose size cannot be asked is not one to move blind
        }
        Frame f = *now;
        f.x = as_int(want.x) - f.left;
        f.y = as_int(want.y) - f.top;
        if (const std::optional<DesktopPoint> at =
                placement_within(f.x, f.y, f.w, f.h, usable_displays())) {
            move_frame(f, *at);
        }
    }

    /// The window grown toward `want`, the reach of areas it cannot show whole, as far as its
    /// display holds it with its frame (`grown_within`), then kept whole there. Never shrunk; a
    /// maximized window is the platform's. It opens without high pixel density, so its pixels are
    /// its units.
    void grow(const PixelSize& want) {
        if (window_ == nullptr || maximized()) {
            return;
        }
        const std::optional<Frame> f = frame_of();
        if (!f.has_value()) {
            return;
        }
        const PixelSize have{f->w - f->left - f->right, f->h - f->top - f->bottom};
        PixelSize grown{(std::max)(want.width, have.width), (std::max)(want.height, have.height)};
        const std::vector<DesktopSpan> usable = usable_displays();
        if (!usable.empty()) {
            grown = grown_within(want, have, f->left + f->right, f->top + f->bottom,
                                 home_display(f->x, f->y, f->w, f->h, usable));
        }
        if (grown == have) {
            return;
        }
        if (!SDL_SetWindowSize(window_, as_int(grown.width), as_int(grown.height))) {
            complain("SDL_SetWindowSize", "the window keeps its size.");
            return;
        }
        (void)SDL_SyncWindow(window_); // the grown size, not the old one, is kept whole
        keep_whole();
    }

    /// SHAPED: the title set, and the window sized to `want`, a side of 0 keeping its own, within
    /// its display's usable area with its frame (`shaped_within`), then kept whole there. A
    /// maximized window takes the title and keeps the platform's size.
    void shape(const std::string& title, const PixelSize& want) {
        if (window_ == nullptr) {
            return;
        }
        if (!SDL_SetWindowTitle(window_, title.c_str())) {
            complain("SDL_SetWindowTitle", "the window keeps its title.");
        }
        if (maximized()) {
            return;
        }
        const std::optional<Frame> f = frame_of();
        if (!f.has_value()) {
            return;
        }
        const PixelSize have{f->w - f->left - f->right, f->h - f->top - f->bottom};
        PixelSize sized{want.width > 0 ? want.width : have.width,
                        want.height > 0 ? want.height : have.height};
        const std::vector<DesktopSpan> usable = usable_displays();
        if (!usable.empty()) {
            sized = shaped_within(want, have, f->left + f->right, f->top + f->bottom,
                                  home_display(f->x, f->y, f->w, f->h, usable));
        }
        if (sized == have) {
            return;
        }
        if (!SDL_SetWindowSize(window_, as_int(sized.width), as_int(sized.height))) {
            complain("SDL_SetWindowSize", "the window keeps its size.");
            return;
        }
        (void)SDL_SyncWindow(window_);
        keep_whole();
    }

    /// The window maximized, once the medium has given it its room. A refusal is said once a want:
    /// the medium does not ask again.
    void maximize() {
        if (window_ != nullptr && !SDL_MaximizeWindow(window_)) {
            complain("SDL_MaximizeWindow", "the window stays as it is.");
        }
    }

private:
    struct Made {
        std::uint64_t version = 0;
        std::vector<SDL_Texture*> made;
    };

    /// The window's frame in the desktop's units: its rectangle with the platform's borders, and
    /// the borders themselves, which turn a frame position back into the window's own.
    struct Frame {
        std::int64_t x = 0, y = 0, w = 0, h = 0;
        std::int64_t left = 0, top = 0, right = 0, bottom = 0;
    };

    /// SDL's global properties, shared by every instance in the process: the window a reloaded
    /// instance leaves to the next, and how many instances that may take it are alive.
    static constexpr const char* kHanded = "zengine.window.sdl.handed";
    static constexpr const char* kLiving = "zengine.window.sdl.instances";

    static constexpr const char* kRefused =
        "the OS window does not open, and its weave does not load.";
    static constexpr const char* kNoWindow =
        "no OS window is shown; the window keeps and composes its picture.";
    static constexpr const char* kNoPicture = "no picture is read back.";

    /// What went wrong, in SDL's own words, and what it costs, on standard error; the first line,
    /// returned.
    static std::string complain(const char* what, const char* then) {
        const char* why = SDL_GetError();
        const bool given = why != nullptr && why[0] != '\0';
        const std::string said = std::string("zengine-window-sdl: ") + what + " failed: " +
                                 (given ? why : "(SDL gave no reason)");
        std::fprintf(stderr, "%s\n  %s\n", said.c_str(), then);
        std::fflush(stderr);
        return said;
    }

    /// Give back this instance's hold on SDL's video, and shut SDL down only if nobody else holds
    /// it: two libraries in one process cannot share a private count.
    static void release_sdl() {
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        if (SDL_WasInit(0) == 0) {
            SDL_Quit();
        }
    }

    static std::int64_t living(std::int64_t step) {
        const SDL_PropertiesID global = SDL_GetGlobalProperties();
        const std::int64_t n = SDL_GetNumberProperty(global, kLiving, 0) + step;
        SDL_SetNumberProperty(global, kLiving, n);
        return n;
    }

    /// A desktop number as SDL's `int` takes it, held at its ends rather than past them.
    static int as_int(std::int64_t v) {
        return static_cast<int>(std::clamp<std::int64_t>(v, INT_MIN, INT_MAX));
    }

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

    /// The window and its renderer. Hidden until it is centred on the display the platform opened
    /// it on, so it is never seen anywhere else. Never focusable when ZENGINE_SDL_NOT_FOCUSABLE=1,
    /// so a suite or a probe leaves the desktop's keyboard with whoever is using it. Where either
    /// cannot be made, neither is kept, and what SDL said is returned, `then` said after it on
    /// standard error; giving back SDL and the claim is the caller's.
    std::string open(const char* then) {
        const char* unfocused = std::getenv("ZENGINE_SDL_NOT_FOCUSABLE");
        SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN;
        if (unfocused != nullptr && unfocused[0] == '1') {
            flags |= SDL_WINDOW_NOT_FOCUSABLE;
        }
        window_ = SDL_CreateWindow("Zengine", static_cast<int>(kDefaultWidth),
                                   static_cast<int>(kDefaultHeight), flags);
        if (window_ == nullptr) {
            return complain("SDL_CreateWindow", then);
        }
        centring_ = true;
        keep_whole();
        if (!SDL_ShowWindow(window_)) {
            complain("SDL_ShowWindow", "the window may stay hidden.");
        }
        renderer_ = SDL_CreateRenderer(window_, nullptr);
        if (renderer_ == nullptr) {
            const std::string why = complain("SDL_CreateRenderer", then);
            SDL_DestroyWindow(window_);
            window_ = nullptr;
            return why;
        }
        SDL_StartTextInput(window_);
        return std::string();
    }

    bool maximized() const { return (SDL_GetWindowFlags(window_) & SDL_WINDOW_MAXIMIZED) != 0; }

    /// Every current display's usable area; empty when SDL can say nothing, and then nothing moves.
    static std::vector<DesktopSpan> usable_displays() {
        std::vector<DesktopSpan> usable;
        int count = 0;
        if (SDL_DisplayID* ids = SDL_GetDisplays(&count)) {
            for (int i = 0; i < count; ++i) {
                SDL_Rect r{};
                if (SDL_GetDisplayUsableBounds(ids[i], &r)) {
                    usable.push_back(DesktopSpan{r.x, r.y, r.w, r.h});
                }
            }
            SDL_free(ids);
        }
        return usable;
    }

    std::optional<Frame> frame_of() const {
        int x = 0, y = 0, w = 0, h = 0;
        if (!SDL_GetWindowPosition(window_, &x, &y) || !SDL_GetWindowSize(window_, &w, &h)) {
            return std::nullopt;
        }
        int top = 0, left = 0, bottom = 0, right = 0;
        // A window the platform draws no borders for reports none, and its frame is itself.
        (void)SDL_GetWindowBordersSize(window_, &top, &left, &bottom, &right);
        return Frame{x - left, y - top, w + left + right, h + top + bottom,
                     left, top, right, bottom};
    }

    /// The frame's top-left put at `at`, and where that put the window remembered.
    void move_frame(const Frame& f, const DesktopPoint& at) {
        const int x = as_int(at.x + f.left);
        const int y = as_int(at.y + f.top);
        if (!SDL_SetWindowPosition(window_, x, y)) {
            complain("SDL_SetWindowPosition", "the window stays where it is.");
            return;
        }
        (void)SDL_SyncWindow(window_);
        seated_x_ = x;
        seated_y_ = y;
    }

    /// The window whole on a screen. While this backend placed it and nobody has moved it since,
    /// it is centred on its display; once a weaver or a remembered placement has placed it, it is
    /// moved in only until the whole window is on its display. A maximized window is the
    /// platform's to place.
    void keep_whole() {
        if (maximized()) {
            return;
        }
        const std::optional<Frame> f = frame_of();
        const std::vector<DesktopSpan> usable = usable_displays();
        if (!f.has_value() || usable.empty()) {
            return;
        }
        if (centring_ && seated_x_.has_value() &&
            (f->x + f->left != *seated_x_ || f->y + f->top != *seated_y_)) {
            centring_ = false; // moved since this backend placed it: the weaver's place now
        }
        if (centring_) {
            const DesktopSpan& home = home_display(f->x, f->y, f->w, f->h, usable);
            move_frame(*f, centred_within(f->w, f->h, home));
            return;
        }
        const std::optional<DesktopPoint> at = placement_within(f->x, f->y, f->w, f->h, usable);
        if (at.has_value() && (at->x != f->x || at->y != f->y)) {
            move_frame(*f, *at);
        }
    }

    /// The picture drawn, presenting nothing: back to front, each area clipped to its bounds, its
    /// quads as one geometry batch until a node's lines, text or images break it.
    void draw(const ComposedLightFrame& frame) {
        SDL_SetRenderClipRect(renderer_, nullptr);
        ink(frame.ground);
        SDL_RenderClear(renderer_);
        std::set<std::string> seen;
        for (const LightAreaShown& a : frame.areas) {
            const SDL_Rect clip{static_cast<int>(a.x), static_cast<int>(a.y),
                                static_cast<int>(a.w), static_cast<int>(a.h)};
            SDL_SetRenderClipRect(renderer_, &clip);
            batch_.clear();
            for (const LightNodeShown& n : a.nodes) {
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
            face_.draw(renderer_, static_cast<float>(ox + t.x), static_cast<float>(oy + t.y),
                       t.rgba, t.text);
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
        face_.drop();
    }

    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    bool claimed_ = false;
    bool input_claimed_ = false; ///< this instance holds a claim on SDL's queue
    bool held_ = false;          ///< this instance holds SDL's video
    bool counted_ = false;       ///< counted among the living
    bool centring_ = false;      ///< centred on its display until someone else places it
    std::optional<std::int64_t> seated_x_; ///< where this backend last put the window...
    std::optional<std::int64_t> seated_y_;
    std::int64_t normal_x_ = 0; ///< the normal window's last observed position...
    std::int64_t normal_y_ = 0;
    bool have_normal_ = false; ///< ...and whether it was ever observed
    std::vector<SDL_Vertex> batch_;
    std::vector<int> indices_;
    std::map<std::string, Made> textures_;
    SdlFace face_;
};

} // namespace zengine::graphics::window

#endif // ZENGINE_GRAPHICS_WINDOW_WINDOW_SDL_BACKEND_HPP
