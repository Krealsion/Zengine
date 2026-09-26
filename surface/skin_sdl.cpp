// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The SDL Skin: the same intent, a real window. Only the SDL edge lives here: skin_sdl_plan.hpp
// plans every frame as pure arithmetic, and this file executes the plan against a renderer and
// puts the text slots in the window title. Labels are bitmap cells; text regions are set in the
// embedded face (skin_sdl_text.hpp) once it opens. With no display, frames are consumed with
// nothing shown and stderr says why. The window is an ear too: SDL's one event queue is the
// SDL Input reader's, so nothing here takes an event out of it.
// Surface law: agents/surface.md

#include "skin.hpp"
#include "skin_sdl_plan.hpp"
#include "skin_sdl_text.hpp"

#include <zen/kernel/export.hpp>

#include <SDL3/SDL.h>

#include <cstdint>
#include <cstdio>
#include <optional>
#include <string>
#include <vector>

namespace {

using namespace zengine::surface;

/// What went wrong, in SDL's own words, on stderr: a surface that cannot exist says why (a WSL
/// SDL3 with only the dummy and offscreen drivers once showed nothing but "no window"). Not a
/// SurfaceText: a message about a missing surface delivered to the surface is not a message.
void complain(const char* what) {
    const char* why = SDL_GetError();
    std::fprintf(stderr, "zengine-skin-sdl: %s failed: %s\n", what,
                 (why != nullptr && why[0] != '\0') ? why : "(SDL gave no reason)");
    std::fflush(stderr);
}

/// Give back this weave's claim on SDL, and shut SDL down only if nobody else holds it.
/// SDL_Quit alone would deafen the SDL Input reader, which holds the same subsystem in this
/// process; SDL_QuitSubSystem alone leaves SDL's globals allocated (the sanitizer lane measured
/// ~8.5 KB leaked per run). `SDL_WasInit(0)` is the one count two separately loaded weaves can
/// share, because SDL keeps it.
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
            // A failed SDL_Init still allocated its globals (~7 KB, measured by the sanitizer
            // lane): release them as the destructor does.
            release_sdl();
        }
    }
    ~SdlMedium() {
        // The face first: its glyph atlas lives in textures this renderer owns, and member order
        // would close it after the renderer is gone, a double free only a sanitizer would name.
        text_.close();
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
        // The window is resizable, so the drawable can be larger than the board: clear it, or
        // the pixels outside the board are whatever was there before.
        SDL_SetRenderDrawColor(renderer_, kCanvasBackground.r, kCanvasBackground.g,
                               kCanvasBackground.b, SDL_ALPHA_OPAQUE);
        SDL_RenderClear(renderer_);
        for (const PlanRect& r : plan_frame(v)) {
            SDL_SetRenderDrawColor(renderer_, r.r, r.g, r.b, SDL_ALPHA_OPAQUE);
            const SDL_FRect fr{static_cast<float>(r.x), static_cast<float>(r.y),
                               static_cast<float>(r.w), static_cast<float>(r.h)};
            SDL_RenderFillRect(renderer_, &fr);
        }
        SDL_RenderPresent(renderer_);
        note_room_given();
    }

    /// The general canvas, in a window. `plan_canvas` hands back each layer's rects and labels
    /// as one list of opaque quads, so nothing here knows what a label is and no second loop can
    /// drop one; glyphs are decided in skin_sdl_plan.hpp and skin_sdl_glyphs.hpp.
    void canvas(const SurfaceCanvas& c, bool) {
        if (!ok_) {
            return;
        }
        if (!ensure_sized_window(canvas_window_size(c))) {
            return;
        }
        pump();
        last_canvas_ = c;
        paint(c);
        SDL_RenderPresent(renderer_);
        note_room_given();
    }

    /// What this window presents, read back from the renderer as a 24-bit BMP. A presented
    /// backbuffer is not readable on every backend, so the last canvas is drawn again, read
    /// before presenting, then presented: the pixels a person sees and the pixels handed back are
    /// one drawing. A window that has painted nothing has nothing to read.
    std::optional<CapturedPicture> capture() {
        if (!ok_ || renderer_ == nullptr || window_ == nullptr || !last_canvas_.has_value()) {
            return std::nullopt;
        }
        paint(*last_canvas_);
        SDL_Surface* read = SDL_RenderReadPixels(renderer_, nullptr);
        if (read == nullptr) {
            complain("SDL_RenderReadPixels");
            SDL_RenderPresent(renderer_);
            return std::nullopt;
        }
        SDL_Surface* bgr = SDL_ConvertSurface(read, SDL_PIXELFORMAT_BGR24);
        SDL_DestroySurface(read);
        if (bgr == nullptr) {
            complain("SDL_ConvertSurface");
            SDL_RenderPresent(renderer_);
            return std::nullopt;
        }
        CapturedPicture p;
        p.width = bgr->w;
        p.height = bgr->h;
        p.cell_px = kCanvasCellPx;
        p.format = "image/bmp";
        p.bytes = bmp_of(*bgr);
        SDL_DestroySurface(bgr);
        SDL_RenderPresent(renderer_);
        return p;
    }

private:
    /// A 24-bit bottom-up BMP of a BGR24 surface: the plainest image a reader without an
    /// image library can open. Rows are padded to four bytes as the format requires.
    static std::string bmp_of(const SDL_Surface& src) {
        const std::uint32_t w = static_cast<std::uint32_t>(src.w > 0 ? src.w : 0);
        const std::uint32_t h = static_cast<std::uint32_t>(src.h > 0 ? src.h : 0);
        const std::uint32_t row = (w * 3 + 3) & ~3u;
        const std::uint32_t pixels = row * h;
        const std::uint32_t total = 14 + 40 + pixels;
        std::string out;
        out.reserve(total);
        const auto u16 = [&out](std::uint16_t v) {
            out.push_back(static_cast<char>(v & 0xffu));
            out.push_back(static_cast<char>((v >> 8) & 0xffu));
        };
        const auto u32 = [&out](std::uint32_t v) {
            for (int i = 0; i < 4; ++i) {
                out.push_back(static_cast<char>((v >> (8 * i)) & 0xffu));
            }
        };
        out += "BM";
        u32(total);
        u16(0);
        u16(0);
        u32(14 + 40);
        u32(40); // BITMAPINFOHEADER
        u32(w);
        u32(h); // positive: bottom-up
        u16(1);
        u16(24);
        u32(0); // BI_RGB
        u32(pixels);
        u32(2835);
        u32(2835);
        u32(0);
        u32(0);
        const auto* base = static_cast<const std::uint8_t*>(src.pixels);
        for (std::uint32_t y = 0; y < h; ++y) {
            const std::uint32_t src_row = h - 1 - y;
            const std::uint8_t* line = base + static_cast<std::size_t>(src_row) *
                                                  static_cast<std::size_t>(src.pitch);
            out.append(reinterpret_cast<const char*>(line), static_cast<std::size_t>(w) * 3u);
            for (std::uint32_t pad = w * 3; pad < row; ++pad) {
                out.push_back('\0');
            }
        }
        return out;
    }

    /// DRAW ONE CANVAS INTO THE RENDERER, presenting nothing: the picture is the same whether
    /// it is presented next or read back first.
    void paint(const SurfaceCanvas& c) {
        SDL_SetRenderDrawColor(renderer_, kCanvasBackground.r, kCanvasBackground.g,
                               kCanvasBackground.b, SDL_ALPHA_OPAQUE);
        SDL_RenderClear(renderer_);
        const SurfaceExtent metric = extent();
        // One whole plane at a time, in the publisher's order: `plan_canvas` hands the layers
        // back ordered, each with its quads and its real-face regions, so the only ordering left
        // here is a layer's quads, then its regions, then the next layer.
        const auto execute = [this](const PlanLayer& layer) {
            for (const PlanRect& r : layer.quads) {
                SDL_SetRenderDrawColor(renderer_, r.r, r.g, r.b, SDL_ALPHA_OPAQUE);
                const SDL_FRect fr{static_cast<float>(r.x), static_cast<float>(r.y),
                                   static_cast<float>(r.w), static_cast<float>(r.h)};
                SDL_RenderFillRect(renderer_, &fr);
            }
            for (const PlanTextRegion& p : layer.regions) {
                text_.draw(renderer_, p);
            }
        };
        for (const PlanLayer& layer : plan_canvas(c, metric, drawable())) {
            execute(layer);
        }
        // The attention chip, after the whole picture: furniture, composed here rather than by
        // a publisher, through the same two lists. An empty slot draws nothing.
        execute(plan_attention_chip(score_, c, metric, drawable()));
    }

public:
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

    /// Service this window's conversation with the OS, and take nothing out of the queue.
    /// SDL_PollEvent removes what it returns, so a Skin that polled would steal every key and
    /// click from the reader; doing nothing left the window Not Responding whenever the reader
    /// was the terminal's. SDL_PumpEvents gathers pending OS input into the queue and removes
    /// nothing, so the queue keeps one owner and the window lives whichever reader runs. Pumping
    /// twice is safe; this runs on the thread that brought the video subsystem up.
    void pump() {
        if (!ok_ || window_ == nullptr) {
            return; // nothing has an OS conversation yet
        }
        SDL_PumpEvents();
        apply_offered_maximize();
        notice_unread_queue();
    }

    /// A maker copied text: onto the real platform clipboard. A failure is complained about and
    /// costs nothing else; the copy is already true in this process, having travelled the bus.
    void clipboard_copy(const std::string& text) {
        if (!ok_) {
            return;
        }
        if (!SDL_SetClipboardText(text.c_str())) {
            complain("SDL_SetClipboardText");
        }
    }

    /// A maker asked to paste: the platform clipboard's text now. The one place in the process
    /// that reads the system clipboard, and only under `ClipboardTextRequested`. An empty or
    /// non-text clipboard answers an empty string (the platform was read); nullopt means no read
    /// exists at all.
    std::optional<std::string> clipboard_text() {
        if (!ok_) {
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

    /// Where this window sits, always about the normal window: `SDL_GetWindowPosition` reports
    /// the work area's corner while maximized, so the normal position is sampled only while
    /// unmaximized and remembered across a maximized stretch. A window never seen unmaximized
    /// answers nothing.
    std::optional<SurfacePlacement> placement() {
        if (!ok_ || window_ == nullptr) {
            return std::nullopt;
        }
        const bool maximized = (SDL_GetWindowFlags(window_) & SDL_WINDOW_MAXIMIZED) != 0;
        if (!maximized) {
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
        return SurfacePlacement{normal_x_, normal_y_, maximized};
    }

    /// A remembered placement offered back: judged by `placement_within` against every current
    /// display's usable bounds and the window's size in window coordinates (not the drawable).
    /// No display truth, no move. The maximize waits for the room, which only a picture gives:
    /// maximizing now would freeze the normal rectangle at the window's creation size, since the
    /// platform refuses to resize a maximized window, and unmaximizing would land on the floor.
    /// So the want is recorded here and lands in `pump`.
    void place(const SurfacePlacementRemembered& want) {
        if (!ok_ || window_ == nullptr) {
            return;
        }
        std::vector<DesktopSpan> usable;
        int display_count = 0;
        if (SDL_DisplayID* ids = SDL_GetDisplays(&display_count)) {
            for (int i = 0; i < display_count; ++i) {
                SDL_Rect r{};
                if (SDL_GetDisplayUsableBounds(ids[i], &r)) {
                    usable.push_back(DesktopSpan{r.x, r.y, r.w, r.h});
                }
            }
            SDL_free(ids);
        }
        int w = 0;
        int h = 0;
        if (!SDL_GetWindowSize(window_, &w, &h)) {
            return; // a window whose size cannot be asked is not one to move blind
        }
        const std::optional<DesktopPoint> at =
            placement_within(want.x, want.y, w, h, usable);
        if (at.has_value() &&
            !SDL_SetWindowPosition(window_, static_cast<int>(at->x),
                                   static_cast<int>(at->y))) {
            complain("SDL_SetWindowPosition");
        }
        if (want.maximized) {
            offered_max_ = OfferedMaximize::kWaitingForRoom;
        }
    }

    /// How much room this window has, in canvas cells, measured from the renderer's output size
    /// each time; no window answers {0,0}, which the shell turns into silence. The text metric
    /// comes from the opened face (`SdlTypeface`), zero without one: "text is a cell", which is
    /// what the bitmap face draws. `cell_px` is reported whether or not a face opened.
    SurfaceExtent extent() const {
        if (!ok_ || window_ == nullptr) {
            return SurfaceExtent{};
        }
        SurfaceExtent e = extent_of_drawable(drawable());
        e.text_advance_px = text_.advance_px();
        e.text_line_px = text_.line_px();
        // `kCanvasCellPx` is what `plan_canvas` lays this canvas out at, consulted rather than
        // restated, so geometry spelled in pixels and a quad drawn in pixels agree.
        e.cell_px = kCanvasCellPx;
        return e;
    }

private:
    /// Say, once, when nothing is taking what this window hears: beside a reader watching the
    /// terminal, a maker types into a window whose keys nobody collects. It measures rather than
    /// infers: `SDL_PeepEvents` with no buffer counts the queue and removes nothing, and a queue
    /// past `kUnreadQueue` is one nobody empties. Once, because a complaint every beat teaches a
    /// person to stop reading stderr.
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
                     // what to load rather than a host's flag: the role, and what holds it
                     "zengine-skin-sdl: type at the terminal instead, or run a composition "
                     "that loads `zengine-input-sdl` as `zengine.input`.\n",
                     waiting);
        std::fflush(stderr);
    }

    /// How many unread events make "nobody is draining this" a measurement rather than a
    /// coincidence. A reader empties the queue on each of its own 10ms beats, so a thousand
    /// standing events cannot be a drained queue caught mid-beat.
    static constexpr int kUnreadQueue = 1000;

    /// An offered maximize is a two-part restore: `place` supplies the normal window's position,
    /// only a picture supplies its room, and the maximize comes after both.
    enum class OfferedMaximize {
        kNone,           ///< nothing offered, or the offer has already landed
        kWaitingForRoom, ///< offered; no picture has sized the normal window since
        kRoomGiven,      ///< a picture has sized it AND been reported: land on the next beat
    };

    /// A picture has been drawn and is about to be reported, so a waiting maximize may land --
    /// on the next beat, not now: the shell reports placement and extent after this returns, and
    /// a maximize landing now would replace the restored room before anyone heard of it.
    void note_room_given() {
        if (offered_max_ == OfferedMaximize::kWaitingForRoom) {
            offered_max_ = OfferedMaximize::kRoomGiven;
        }
    }

    /// ...and here it lands, on the beat, once. A refusal is complained about and clears the
    /// want: a maximize the platform will not perform is not retried every beat.
    void apply_offered_maximize() {
        if (offered_max_ != OfferedMaximize::kRoomGiven) {
            return;
        }
        offered_max_ = OfferedMaximize::kNone;
        if (!SDL_MaximizeWindow(window_)) {
            complain("SDL_MaximizeWindow");
        }
    }

    bool ensure_window(const zengine::snake::SnakeVisual& v) {
        return ensure_sized_window(window_size_of(v));
    }

    /// The window never shows less than the picture asks for, and is otherwise the person's.
    /// Created at the first picture's size, which becomes its minimum, and resizable; grown only
    /// by a picture that does not fit. A canvas publisher sizes itself to the reported extent,
    /// so sizing the window to the canvas would be two parties resizing each other (a canvas
    /// rounds down, so the window would shrink a little at every drag). The minimum never moves,
    /// or a canvas following the window would ratchet it up.
    bool ensure_sized_window(const PlanSize& want) {
        if (want.w <= 0 || want.h <= 0) {
            return false;
        }
        if (window_ == nullptr) {
            // Resizable: a larger window is a larger surface the publisher is told about.
            // Focusable, because this window is an ear as well as a surface.
            window_ = SDL_CreateWindow(title_of(status_, score_).c_str(),
                                       static_cast<int>(want.w), static_cast<int>(want.h),
                                       SDL_WINDOW_RESIZABLE);
            if (window_ == nullptr) {
                complain("SDL_CreateWindow");
                return false;
            }
            // The floor under every later drag. A failure only lets the window be dragged
            // smaller than its picture, which is clipped as the canvas contract says.
            SDL_SetWindowMinimumSize(window_, static_cast<int>(want.w),
                                     static_cast<int>(want.h));
            renderer_ = SDL_CreateRenderer(window_, nullptr);
            if (renderer_ == nullptr) {
                complain("SDL_CreateRenderer");
                SDL_DestroyWindow(window_);
                window_ = nullptr;
                return false;
            }
            // Native text events need the window, so only this file can turn them on: window
            // setup, with no opinion about what text means. A failure leaves a readable window
            // with no typing, and says so.
            if (!SDL_StartTextInput(window_)) {
                complain("SDL_StartTextInput");
            }
            // The face opens with the renderer because SDL_ttf caches its atlas in this
            // renderer's textures: one lifetime between them. A failure is reported by `open`,
            // and the window still comes up, in the bitmap face.
            (void)text_.open(renderer_);
            return true;
        }
        const PlanSize have = drawable();
        if (want.w > have.w || want.h > have.h) {
            SDL_SetWindowSize(window_, static_cast<int>(want.w > have.w ? want.w : have.w),
                              static_cast<int>(want.h > have.h ? want.h : have.h));
        }
        return true;
    }

    /// What this medium draws on, in pixels, asked of SDL and never remembered: a person
    /// dragging the edge changes it and no message says so. The renderer's output size, the
    /// space `plan_canvas`'s quads are in.
    PlanSize drawable() const {
        int w = 0;
        int h = 0;
        if (renderer_ == nullptr || !SDL_GetRenderOutputSize(renderer_, &w, &h)) {
            return PlanSize{};
        }
        return PlanSize{w, h};
    }

    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    std::int64_t normal_x_ = 0; ///< the normal window's last observed position...
    std::int64_t normal_y_ = 0;
    bool have_normal_ = false;  ///< ...and whether it has ever been observed at all
    OfferedMaximize offered_max_ = OfferedMaximize::kNone; ///< a maximize owed a room
    SdlTypeface text_; ///< the real face, when there is one; see skin_sdl_text.hpp
    std::optional<SurfaceCanvas> last_canvas_; ///< what the window shows, kept for `capture`
    std::string status_;
    std::string score_;
    bool ok_ = false;
    bool said_unread_ = false; ///< the queue complaint is said once per incarnation
};

using SkinSdl = SkinT<SdlMedium>;

} // namespace

ZEN_EXPORT_WEAVE(SkinSdl)
