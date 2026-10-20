// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_GRAPHICS_WINDOW_WINDOW_SDL_HPP
#define ZENGINE_GRAPHICS_WINDOW_WINDOW_SDL_HPP

// The OS window: the window (graphics/window/window.hpp) over a medium of light and an injected
// pixel Backend. It keeps each node painted as pixel primitives (graphics/light_paint.hpp), and
// when anything changed hands the backend the whole picture, back to front, each area in its own
// bounds; a press lands on the primitives shown. The desktop's arithmetic and the picture read
// back are here, pure; SDL is the backend's (graphics/window/window_sdl_backend.hpp). Reference:
// graphics/docs/zengine.window.sdl.md.

#include "graphics/light_paint.hpp"
#include "graphics/window/compose.hpp"
#include "graphics/window/window.hpp"

#include "input/translate.hpp"
#include "input/vocabulary.hpp"
#include "manuals/zengine.window.sdl.hpp" // this office's manual, compiled in from its page

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace zengine::graphics::window {

/// The size an OS window opens at, and the size it keeps its picture at when it has none.
inline constexpr std::int64_t kDefaultWidth = 1600;
inline constexpr std::int64_t kDefaultHeight = 900;

/// A window's size in pixels; non-positive is no window to measure.
struct PixelSize {
    std::int64_t width = 0, height = 0;
    bool measured() const { return width > 0 && height > 0; }
    friend bool operator==(const PixelSize&, const PixelSize&) = default;
};

// ---- The window on its desktop ------------------------------------------------------------------
//
// Where an OS window sits and how large it grows, as pure arithmetic every lane pins: the backend
// supplies the displays and the window's frame, and applies the answer.

/// One display's usable area -- its bounds less what the platform keeps for itself (a taskbar, a
/// dock, a menu bar) -- in the desktop's units, where a monitor left of the primary is negative.
struct DesktopSpan {
    std::int64_t x = 0, y = 0, w = 0, h = 0;
};

/// A point of the desktop, in the same units.
struct DesktopPoint {
    std::int64_t x = 0, y = 0;
    friend bool operator==(const DesktopPoint&, const DesktopPoint&) = default;
};

/// The display a window belongs to: the one it overlaps most, else the one whose centre is
/// nearest its own. `usable` is not empty.
inline const DesktopSpan& home_display(std::int64_t x, std::int64_t y, std::int64_t w,
                                       std::int64_t h, const std::vector<DesktopSpan>& usable) {
    const DesktopSpan* home = &usable.front();
    std::int64_t best_overlap = -1;
    std::int64_t best_distance = (std::numeric_limits<std::int64_t>::max)();
    for (const DesktopSpan& s : usable) {
        const std::int64_t ox = (std::min)(x + w, s.x + s.w) - (std::max)(x, s.x);
        const std::int64_t oy = (std::min)(y + h, s.y + s.h) - (std::max)(y, s.y);
        const std::int64_t overlap = (ox > 0 && oy > 0) ? ox * oy : 0;
        // Centres, times two, so the arithmetic stays integral.
        const std::int64_t dx = (2 * x + w) - (2 * s.x + s.w);
        const std::int64_t dy = (2 * y + h) - (2 * s.y + s.h);
        const std::int64_t distance = (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
        if (overlap > best_overlap || (overlap == best_overlap && distance < best_distance)) {
            best_overlap = overlap;
            best_distance = distance;
            home = &s;
        }
    }
    return *home;
}

/// One coordinate moved in only as far as it must for `[v, v + size)` to lie within
/// `[lo, lo + room)`; a size larger than the room keeps its near edge at `lo`.
inline std::int64_t moved_within(std::int64_t v, std::int64_t size, std::int64_t lo,
                                 std::int64_t room) {
    if (size >= room || v < lo) {
        return lo;
    }
    return v + size > lo + room ? lo + room - size : v;
}

/// Where a window, its frame included, sits whole on a display: moved in only as far as it must
/// to lie inside its home display's usable area. One larger than that area keeps its top-left
/// there, so the strip a hand drags it by stays on the screen. With no display truth there is no
/// answer, since an uninformed move is a blind replay. It moves a window and never sizes one.
inline std::optional<DesktopPoint> placement_within(std::int64_t x, std::int64_t y,
                                                    std::int64_t w, std::int64_t h,
                                                    const std::vector<DesktopSpan>& usable) {
    if (usable.empty()) {
        return std::nullopt;
    }
    const DesktopSpan& home = home_display(x, y, w, h, usable);
    return DesktopPoint{moved_within(x, w, home.x, home.w), moved_within(y, h, home.y, home.h)};
}

/// Where a window, its frame included, sits centred on `display`'s usable area; one larger than
/// that area keeps its top-left there, as `placement_within` does.
inline DesktopPoint centred_within(std::int64_t w, std::int64_t h, const DesktopSpan& display) {
    return DesktopPoint{moved_within(display.x + (display.w - w) / 2, w, display.x, display.w),
                        moved_within(display.y + (display.h - h) / 2, h, display.y, display.h)};
}

/// How large a window grows for what it must show: to `want`, but no further than `display`'s
/// usable area holds with the frame's own `frame_w` and `frame_h` around it, and never smaller
/// than the window already is.
inline PixelSize grown_within(const PixelSize& want, const PixelSize& have, std::int64_t frame_w,
                              std::int64_t frame_h, const DesktopSpan& display) {
    const auto axis = [](std::int64_t want_v, std::int64_t have_v, std::int64_t room) {
        const std::int64_t grown = (std::min)(want_v, room);
        return (std::max)(grown, have_v);
    };
    return PixelSize{axis(want.width, have.width, display.w - frame_w),
                     axis(want.height, have.height, display.h - frame_h)};
}

/// A 24-bit bottom-up BMP of `w` by `h` pixels of three bytes each, blue, green, red, a row every
/// `pitch` bytes from `pixels`: the plainest picture a reader with no image library opens. Each
/// row is padded to four bytes, as the format asks, and the last row comes first. A size the
/// format's 32-bit fields cannot hold, or pixels missing, answers nothing (empty).
inline std::string bmp_of_bgr24(std::int32_t w, std::int32_t h, std::int32_t pitch,
                                const std::uint8_t* pixels) {
    const std::uint64_t width = w > 0 ? static_cast<std::uint64_t>(w) : 0;
    const std::uint64_t height = h > 0 ? static_cast<std::uint64_t>(h) : 0;
    const std::uint64_t row = (width * 3 + 3) & ~std::uint64_t{3};
    const std::uint64_t image = row * height;
    const std::uint64_t total = 14 + 40 + image;
    if (total > 0xffffffffu || (pixels == nullptr && image != 0)) {
        return {};
    }
    std::string out;
    out.reserve(static_cast<std::size_t>(total));
    const auto u16 = [&out](std::uint64_t v) {
        for (int i = 0; i < 2; ++i) {
            out.push_back(static_cast<char>((v >> (8 * i)) & 0xffu));
        }
    };
    const auto u32 = [&out](std::uint64_t v) {
        for (int i = 0; i < 4; ++i) {
            out.push_back(static_cast<char>((v >> (8 * i)) & 0xffu));
        }
    };
    out += "BM";
    u32(total);
    u16(0);
    u16(0);
    u32(14 + 40); // where the pixels start
    u32(40);      // BITMAPINFOHEADER
    u32(width);
    u32(height); // positive: bottom-up
    u16(1);
    u16(24);
    u32(0); // BI_RGB
    u32(image);
    u32(2835); // 72 dots an inch, as pixels a metre
    u32(2835);
    u32(0);
    u32(0);
    for (std::uint64_t y = 0; y < height; ++y) {
        const std::uint8_t* line = pixels + static_cast<std::ptrdiff_t>(height - 1 - y) * pitch;
        out.append(reinterpret_cast<const char*>(line), static_cast<std::size_t>(width * 3));
        out.append(static_cast<std::size_t>(row - width * 3), '\0');
    }
    return out;
}

// ---- The medium of light ------------------------------------------------------------------------

/// One node as the backend draws it: which node, a version that changes each time it is painted
/// (an image's texture is made again only then), and its primitives in its area's pixels.
struct LightNodeShown {
    std::string path;
    std::uint64_t version = 0;
    const light::NodePrimitives* prims = nullptr;
};

/// One area as the backend draws it: where it stands, clipped to, and its nodes in paint order.
struct LightAreaShown {
    std::string area;
    std::int64_t x = 0, y = 0, w = 0, h = 0;
    std::vector<LightNodeShown> nodes;
};

/// The whole picture: the window's size and ground (0xRRGGBBAA), and its areas back to front.
/// What it points at is the window's, valid for the one call it is handed to; the last picture
/// presented is kept whole, with what it points at, until another is presented.
struct ComposedLightFrame {
    std::int64_t width = 0, height = 0;
    std::uint32_t ground = 0;
    std::vector<LightAreaShown> areas;
};

/// What a backend read: its keys, text and pointer, whether its picture must be drawn again (the
/// desktop uncovered it), and whether the OS window was asked to close.
struct PixelPolled {
    std::vector<zengine::input::InputEvent> events;
    bool redraw = false;
    bool close = false;
};

/// A medium of light over a Backend: `claim()`, `size()` in pixels, `present(frame)`, true when it
/// drew the picture, `poll()`, and `capture(frame)`, a presented picture drawn again and read back;
/// the clipboard's `clipboard_copy(text)` and `clipboard_text()`; and the desktop's `placement()`,
/// `place(want)`, `grow(want)` toward a size in pixels, and `maximize()`.
template <class Backend>
class LightMedium {
public:
    static constexpr const char* kWindow = kSdlWindow;
    static constexpr const zengine::manual::Manual& kManual =
        zengine::manual::pages::zengine_window_sdl::kManual;

    LightMedium() = default;
    explicit LightMedium(Backend backend) : backend_(std::move(backend)) {}

    /// The OS window taken, and measured.
    void claim() {
        backend_.claim();
        size_ = measure();
        dirty_ = true;
    }

    /// The OS window at another size: drawn again, and true when its size in cells changed.
    bool remeasure() {
        const PixelSize now = measure();
        if (now.width == size_.width && now.height == size_.height) {
            return false;
        }
        const CellExtent was = extent();
        size_ = now;
        dirty_ = true;
        return was.cols != extent().cols || was.rows != extent().rows;
    }

    CellExtent extent() const {
        return CellExtent{size_.width / kCellPx, size_.height / kCellPx};
    }
    CellExtent offered() const { return extent(); }
    /// Its device pixels in one cell: every pixel shown.
    static constexpr std::int64_t kCellDevicePx = kCellPx;

    void clear() {
        cache_.clear();
        dirty_ = true;
    }
    void cover(const WinArea&, const std::string&) { dirty_ = true; }
    void cover_area(const WinArea&) { dirty_ = true; }
    void drop(const WinArea& a, const std::string& path) {
        const auto it = cache_.find(a.area);
        if (it != cache_.end()) {
            it->second.erase(path);
        }
        dirty_ = true;
    }
    void drop_area(const std::string& area) {
        cache_.erase(area);
        dirty_ = true;
    }
    void forget(const std::string& library) { cell_libraries_.erase(library); }

    /// The nodes at `paths` painted for light and kept. The area's skin paints them when it paints
    /// light; with no library, or one whose skin paints characters, they are painted here, in the
    /// day look for the skin named `day` and the night look for any other. A library that will
    /// not paint is said in `why`, and its nodes are painted here.
    void paint(Painters& painters, const WinArea& a, const PaintTask& task,
               const std::set<std::string>& paths, std::string& why) {
        std::optional<PaintResult> result;
        if (!a.library.empty() && cell_libraries_.count(a.library) == 0) {
            if (const ZenginePainterV1* skin = painters.get(a.library, why)) {
                result = painters.paint(skin, task, why);
            }
            if (result && !all_light(*result)) {
                cell_libraries_.insert(a.library);
                result.reset();
            }
        }
        if (!result) {
            result = light::paint_task(task, look_of(a));
        }
        std::map<std::string, const PaintNode*> asked;
        for (const PaintNode& n : task.nodes) {
            asked[n.path] = &n;
        }
        auto& kept = cache_[a.area];
        for (const std::string& p : paths) {
            kept.erase(p);
        }
        for (const NodePaint& np : result->nodes) {
            const auto n = asked.find(np.path);
            if (paths.count(np.path) == 0 || n == asked.end()) {
                continue;
            }
            auto prims = light::unpack_node(np.cells);
            if (!prims) {
                why = "the skin in '" + a.library + "' painted '" + np.path +
                      "' in light that does not read";
                continue;
            }
            kept[np.path] =
                Kept{n->second->key,
                     std::make_shared<const light::NodePrimitives>(std::move(*prims)), ++versions_};
        }
        dirty_ = true;
    }

    /// The whole picture handed to the backend, back to front, once anything changed: one showing
    /// when the backend drew it (with no OS window, it keeps the picture and shows nothing), and
    /// that picture kept to be read back. Areas that reach past the window ask it to grow first.
    void show(const std::vector<WinArea>& areas) {
        if (!dirty_) {
            return;
        }
        dirty_ = false;
        fit(areas);
        ComposedLightFrame frame;
        frame.width = size_.width;
        frame.height = size_.height;
        const auto order = back_to_front(areas);
        frame.ground = light::rgba_of(order.empty() ? light::night_look().ground
                                                    : look_of(*order.front()).ground);
        std::vector<std::shared_ptr<const light::NodePrimitives>> held;
        std::int64_t bytes = 0;
        for (const WinArea* a : order) {
            LightAreaShown shown{a->area, a->x, a->y, a->w, a->h, {}};
            for (const auto& [path, k] : painted(a->area)) {
                shown.nodes.push_back(LightNodeShown{*path, k->version, k->prims.get()});
                held.push_back(k->prims);
                bytes += weight(*k->prims);
            }
            frame.areas.push_back(std::move(shown));
        }
        if (backend_.present(frame)) {
            ++showings_;
            device_bytes_ += bytes;
            last_frame_ = std::move(frame);
            last_held_ = std::move(held);
            if (maximize_ == Maximize::kWaitingForRoom) {
                maximize_ = Maximize::kRoomGiven;
            }
        }
    }

    /// The window as a grid of cells, as text: each run of text one cell a character from the
    /// cell its run starts in, front areas over the ones behind, trailing spaces left off.
    std::vector<std::string> rows_shown(const std::vector<WinArea>& areas) const {
        const CellExtent e = extent();
        std::vector<std::string> rows(static_cast<std::size_t>(e.rows),
                                      std::string(static_cast<std::size_t>(e.cols), ' '));
        for (const WinArea* a : back_to_front(areas)) {
            const Place pl = place_of(*a);
            for (const auto& [path, k] : painted(a->area)) {
                for (const light::TextRun& t : k->prims->texts) {
                    const std::int64_t r = term::floor_cell(a->y + t.y);
                    std::int64_t c = term::floor_cell(a->x + t.x);
                    for (const char ch : t.text) {
                        if (pl.holds(c, r) && c >= 0 && r >= 0 && c < e.cols && r < e.rows) {
                            rows[static_cast<std::size_t>(r)][static_cast<std::size_t>(c)] = ch;
                        }
                        ++c;
                    }
                }
            }
        }
        for (std::string& r : rows) {
            r.erase(r.find_last_not_of(' ') + 1);
        }
        return rows;
    }

    /// A point of the window in pixels (a point in cells at its cell's centre): the front-most
    /// area there that takes it, and the top node whose primitives hold the point, as the
    /// terminal lands one (`window::land`).
    Landed land(const std::vector<WinArea>& areas, std::int64_t at_x, std::int64_t at_y,
                std::int64_t space) const {
        const bool cells = space == zengine::input::space::kCells;
        const std::int64_t x = cells ? at_x * kCellPx + kCellPx / 2 : at_x;
        const std::int64_t y = cells ? at_y * kCellPx + kCellPx / 2 : at_y;
        Landed out;
        for (const WinArea* a : front_to_back(areas)) {
            if (x < a->x || y < a->y || x >= a->x + a->w || y >= a->y + a->h) {
                continue;
            }
            const Kept* best = nullptr;
            const std::string* best_path = nullptr;
            for (const auto& [path, k] : painted(a->area)) {
                if (light::hits_node(*k->prims, x - a->x, y - a->y) &&
                    (best == nullptr || best->key < k->key)) {
                    best = k;
                    best_path = path;
                }
            }
            if (a->pointer || (best != nullptr && takes_press(*a, *best_path))) {
                out.area = a;
                out.path = best != nullptr ? *best_path : std::string();
                break;
            }
        }
        if (out.area != nullptr) {
            out.x = x - out.area->x;
            out.y = y - out.area->y;
        }
        return out;
    }

    /// What the backend read on this beat. A remembered maximize given its room lands first.
    std::vector<zengine::input::InputEvent> poll() {
        if (maximize_ == Maximize::kRoomGiven) {
            maximize_ = Maximize::kNone;
            backend_.maximize();
        }
        PixelPolled polled = backend_.poll();
        if (polled.redraw) {
            dirty_ = true;
        }
        if (polled.close) {
            close_requested_ = true;
        }
        return std::move(polled.events);
    }

    /// Whether the OS window was asked to close since the request was last taken.
    bool close_requested() const noexcept { return close_requested_; }
    bool take_close() noexcept { return std::exchange(close_requested_, false); }

    /// Text onto the platform's clipboard, and the clipboard's text now, both the backend's: the
    /// clipboard is read only when asked.
    void clipboard_copy(std::string_view text) { backend_.clipboard_copy(text); }
    std::optional<std::string> clipboard_text() { return backend_.clipboard_text(); }

    /// Where the OS window sits on its desktop, as the backend reads it.
    std::optional<WindowPlacementValue> placement() { return backend_.placement(); }

    /// A remembered placement offered back: the backend moves the window there, whole on a
    /// display. A maximize waits for the room only a picture gives -- a maximized window is not
    /// grown, so maximizing first would hold it at the size it opened at -- and lands on the beat
    /// after the next picture presented. The latest want is the one that counts.
    void place(const WindowPlacementValue& want) {
        backend_.place(want);
        maximize_ = want.maximized ? Maximize::kWaitingForRoom : Maximize::kNone;
    }

    /// The last picture presented, drawn again by the backend and read back; none before one.
    std::optional<CapturedWindowPicture> capture() {
        if (!last_frame_.has_value()) {
            return std::nullopt;
        }
        return backend_.capture(*last_frame_);
    }

    std::int64_t showings() const { return showings_; }
    std::int64_t device_bytes() const { return device_bytes_; }

private:
    struct Kept {
        std::vector<std::int64_t> key;
        std::shared_ptr<const light::NodePrimitives> prims; ///< shared with the last picture shown
        std::uint64_t version = 0;
    };

    /// A remembered maximize, owed the room a picture gives.
    enum class Maximize {
        kNone,           ///< nothing waits, or it has landed
        kWaitingForRoom, ///< offered; no picture presented since
        kRoomGiven,      ///< a picture presented: it lands on the next beat
    };

    /// The backend asked to grow when the areas' extent is new and reaches past the window: once
    /// an extent, so a window a weaver made smaller is not grown back until its areas move.
    void fit(const std::vector<WinArea>& areas) {
        PixelSize want;
        for (const WinArea& a : areas) {
            want.width = (std::max)(want.width, far_edge(a.x, a.w));
            want.height = (std::max)(want.height, far_edge(a.y, a.h));
        }
        if (want == asked_) {
            return;
        }
        asked_ = want;
        if (want.width > size_.width || want.height > size_.height) {
            backend_.grow(want);
        }
    }

    /// Where an area ends along one axis, held at int64's end rather than past it.
    static std::int64_t far_edge(std::int64_t at, std::int64_t span) {
        constexpr std::int64_t kMax = (std::numeric_limits<std::int64_t>::max)();
        if (span <= 0) {
            return at;
        }
        return at > kMax - span ? kMax : at + span;
    }

    static bool all_light(const PaintResult& r) {
        return std::all_of(r.nodes.begin(), r.nodes.end(),
                           [](const NodePaint& n) { return light::is_light(n.cells); });
    }

    static light::Look look_of(const WinArea& a) {
        return a.skin == "day" ? light::day_look() : light::night_look();
    }

    /// The bytes a node's primitives hand the backend.
    static std::int64_t weight(const light::NodePrimitives& p) {
        std::int64_t n = static_cast<std::int64_t>(p.quads.size() + p.lines.size()) * 20;
        for (const light::TextRun& t : p.texts) {
            n += 16 + static_cast<std::int64_t>(t.text.size());
        }
        for (const light::ImageBlit& im : p.images) {
            n += 28 + static_cast<std::int64_t>(im.rgba.size());
        }
        return n;
    }

    static std::vector<const WinArea*> back_to_front(const std::vector<WinArea>& areas) {
        std::vector<const WinArea*> out;
        for (const WinArea& a : areas) {
            out.push_back(&a);
        }
        std::stable_sort(out.begin(), out.end(),
                         [](const WinArea* l, const WinArea* r) { return l->order < r->order; });
        return out;
    }

    /// An area's painted nodes in paint order.
    using PaintedNodes = std::vector<std::pair<const std::string*, const Kept*>>;
    PaintedNodes painted(const std::string& area) const {
        PaintedNodes out;
        const auto it = cache_.find(area);
        if (it == cache_.end()) {
            return out;
        }
        for (const auto& [path, k] : it->second) {
            out.emplace_back(&path, &k);
        }
        std::stable_sort(out.begin(), out.end(), [](const auto& l, const auto& r) {
            return l.second->key < r.second->key;
        });
        return out;
    }

    PixelSize measure() const {
        const PixelSize s = backend_.size();
        return s.measured() ? s : PixelSize{kDefaultWidth, kDefaultHeight};
    }

    Backend backend_;
    std::map<std::string, std::map<std::string, Kept>> cache_;
    std::set<std::string> cell_libraries_; ///< libraries whose skin paints characters
    PixelSize size_{kDefaultWidth, kDefaultHeight};
    PixelSize asked_; ///< the areas' extent the backend was last asked about
    std::optional<ComposedLightFrame> last_frame_; ///< the last picture presented...
    std::vector<std::shared_ptr<const light::NodePrimitives>> last_held_; ///< ...and what it shows
    Maximize maximize_ = Maximize::kNone;
    bool dirty_ = false;
    bool close_requested_ = false;
    std::uint64_t versions_ = 0;
    std::int64_t showings_ = 0;
    std::int64_t device_bytes_ = 0;
};

/// The OS window over `Backend`.
template <class Backend>
using SdlWindowT = Window<LightMedium<Backend>>;

} // namespace zengine::graphics::window

#endif // ZENGINE_GRAPHICS_WINDOW_WINDOW_SDL_HPP
