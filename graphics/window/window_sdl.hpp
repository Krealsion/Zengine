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

/// How large a shaped window is: `want`, a side of 0 or less keeping `have`'s, each side at least
/// a pixel and no larger than `display`'s usable area holds with the frame's own `frame_w` and
/// `frame_h` around it. Unlike a growth, a shaping may make the window smaller.
inline PixelSize shaped_within(const PixelSize& want, const PixelSize& have, std::int64_t frame_w,
                               std::int64_t frame_h, const DesktopSpan& display) {
    const auto axis = [](std::int64_t want_v, std::int64_t have_v, std::int64_t room) {
        const std::int64_t asked = want_v > 0 ? want_v : have_v;
        return (std::max)(std::int64_t{1}, (std::min)(asked, room));
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

/// A medium of light over a Backend: `claim()`, `size()` in pixels, `text()`, the measure of the
/// face it draws text in, `present(frame)`, true when it drew the picture, `poll()`, and
/// `capture(frame)`, a presented picture drawn again and read back;
/// the clipboard's `clipboard_copy(text)` and `clipboard_text()`; and the desktop's `placement()`,
/// `place(want)`, `grow(want)` toward a size in pixels, `shape(title, want)`, and `maximize()`.
template <class Backend>
class LightMedium {
public:
    static constexpr const char* kWindow = kSdlWindow;
    static constexpr const zengine::manual::Manual& kManual =
        zengine::manual::pages::zengine_window_sdl::kManual;

    LightMedium() = default;
    explicit LightMedium(Backend backend) : backend_(std::move(backend)) {}

    /// The OS window taken, and measured: its size, and the face it sets text in.
    void claim() {
        backend_.claim();
        size_ = measure();
        text_ = backend_.text();
        if (text_.advance <= 0 || text_.line <= 0) {
            text_ = TextMeasure{};
        }
        dirty_ = true;
    }

    /// The OS window at another size: drawn again, and true when it changed.
    bool remeasure() {
        const PixelSize now = measure();
        if (now.width == size_.width && now.height == size_.height) {
            return false;
        }
        size_ = now;
        dirty_ = true;
        return true;
    }

    /// The room offered to the window's followers: the window's pixels, every one.
    PixelRoom offered() const { return PixelRoom{size_.width, size_.height}; }
    /// Its device pixels in one cell: every pixel shown.
    static constexpr std::int64_t kCellDevicePx = kCellPx;
    /// How it sets text: the measure of its backend's face, taken when the window was.
    TextMeasure text() const { return text_; }

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

    /// The window as text, a character an advance of its face and a row a line of it: each
    /// character of a run where it stands, inside its area, front areas over the ones behind,
    /// trailing spaces left off.
    std::vector<std::string> rows_shown(const std::vector<WinArea>& areas) const {
        const std::int64_t cols = size_.width / text_.advance;
        const std::int64_t lines = size_.height / text_.line;
        std::vector<std::string> rows(static_cast<std::size_t>(lines),
                                      std::string(static_cast<std::size_t>(cols), ' '));
        for (const WinArea* a : back_to_front(areas)) {
            for (const auto& [path, k] : painted(a->area)) {
                for (const light::TextRun& t : k->prims->texts) {
                    const std::int64_t y = a->y + t.y;
                    const std::int64_t r = floor_div(y, text_.line);
                    std::int64_t x = a->x + t.x;
                    for (const char ch : t.text) {
                        const std::int64_t c = floor_div(x, text_.advance);
                        const bool inside = x >= a->x && x + text_.advance <= a->x + a->w &&
                                            y >= a->y && y < a->y + a->h;
                        if (inside && c >= 0 && r >= 0 && c < cols && r < lines) {
                            rows[static_cast<std::size_t>(r)][static_cast<std::size_t>(c)] = ch;
                        }
                        x += text_.advance;
                    }
                }
            }
        }
        for (std::string& r : rows) {
            r.erase(r.find_last_not_of(' ') + 1);
        }
        return rows;
    }

    /// What one area shows, in its pixels (`AreaShown`): its text runs line by line, joined where
    /// one ends as the next begins, each character inside the area and the window and drawn over by
    /// no area in front but one `reader` owns; and each record naming an action as a part -- the box
    /// its subtree spans, cut to the area, and the point nearest its middle a press reaches it by,
    /// none shown, none said. "" when said, else why not.
    std::string read_area(const std::vector<WinArea>& areas, const WinArea& a,
                          const std::string& reader, std::vector<AreaWord>& words,
                          std::vector<AreaPart>& parts) const {
        const std::int64_t adv = text_.advance, line = text_.line;
        std::vector<std::pair<const WinArea*, PaintedNodes>> front;
        for (const WinArea* f : front_to_back(areas)) {
            if (f->area == a.area) {
                break;
            }
            if (f->owner != reader && f->x < a.x + a.w && f->x + f->w > a.x && f->y < a.y + a.h &&
                f->y + f->h > a.y) {
                front.emplace_back(f, painted(f->area));
            }
        }
        // Whether a point of the window, in its pixels, shows this area: inside it and the window,
        // and drawn over by no area in front.
        const auto shows = [&](std::int64_t wx, std::int64_t wy) {
            if (wx < a.x || wy < a.y || wx >= a.x + a.w || wy >= a.y + a.h || wx < 0 || wy < 0 ||
                wx >= size_.width || wy >= size_.height) {
                return false;
            }
            for (const auto& [f, nodes] : front) {
                if (wx < f->x || wy < f->y || wx >= f->x + f->w || wy >= f->y + f->h) {
                    continue;
                }
                for (const auto& [path, k] : nodes) {
                    if (light::hits_node(*k->prims, wx - f->x, wy - f->y, text_)) {
                        return false;
                    }
                }
            }
            return true;
        };
        const PaintedNodes own = painted(a.area);
        // THE WORDS: every character where it stands, a later node's over an earlier one's.
        std::map<std::pair<std::int64_t, std::int64_t>, char> chars; // (y, x) in the area
        for (const auto& [path, k] : own) {
            for (const light::TextRun& t : k->prims->texts) {
                std::int64_t x = t.x;
                for (const char ch : t.text) {
                    chars[{t.y, x}] = ch;
                    x += adv;
                }
            }
        }
        std::string run;
        std::int64_t run_x = 0, run_y = 0, next_x = 0;
        bool open = false;
        const auto flush = [&]() {
            run.erase(run.find_last_not_of(' ') + 1);
            if (!run.empty()) {
                words.push_back(AreaWord{run_x, run_y, run});
            }
            run.clear();
            open = false;
        };
        for (const auto& [at, ch] : chars) {
            const auto [y, x] = at;
            const bool inside = x >= 0 && x + adv <= a.w && y >= 0 && y < a.h &&
                                shows(a.x + x + adv / 2, a.y + y + line / 2);
            if (open && (!inside || y != run_y || x != next_x)) {
                flush();
            }
            if (!inside) {
                continue;
            }
            if (!open) {
                open = true;
                run_x = x;
                run_y = y;
            }
            run.push_back(ch);
            next_x = x + adv;
        }
        if (open) {
            flush();
        }
        if (words.size() > kMaxAreaWords) {
            return "area '" + a.area + "' shows more than " + std::to_string(kMaxAreaWords) +
                   " runs of text";
        }
        // THE PARTS: each record naming an action, by its subtree's primitives.
        const std::map<std::string, const ElementRecord*> reached = records_by_node(a);
        // The top node of this area at a point of the area, as a press finds it; none outside all.
        const auto top_at = [&](std::int64_t x, std::int64_t y) -> const std::string* {
            const std::string* top = nullptr;
            for (const auto& [path, k] : own) {
                if (light::hits_node(*k->prims, x, y, text_)) {
                    top = path; // paint order: the last that holds the point is on top
                }
            }
            return top;
        };
        for (const ElementRecord& rec : a.records) {
            if (rec.action.empty() || rec.path.empty()) {
                continue;
            }
            bool any = false;
            std::int64_t x0 = 0, y0 = 0, x1 = 0, y1 = 0; // the box, its far edges exclusive
            std::vector<std::pair<std::int64_t, std::int64_t>> points; // its own, in the area
            const auto span = [&](std::int64_t bx, std::int64_t by, std::int64_t bw,
                                  std::int64_t bh) {
                if (bw <= 0 || bh <= 0) {
                    return;
                }
                x0 = any ? (std::min)(x0, bx) : bx;
                y0 = any ? (std::min)(y0, by) : by;
                x1 = any ? (std::max)(x1, bx + bw) : bx + bw;
                y1 = any ? (std::max)(y1, by + bh) : by + bh;
                any = true;
            };
            for (const auto& [path, k] : own) {
                if (!under(*path, rec.path)) {
                    continue;
                }
                const auto r = reached.find(*path);
                const bool mine = r != reached.end() && r->second == &rec;
                const light::NodePrimitives& p = *k->prims;
                // A box's own points: five by five across it, its middle among them.
                const auto grid = [&](std::int64_t bx, std::int64_t by, std::int64_t bw,
                                      std::int64_t bh) {
                    for (std::int64_t i = 0; i < 5; ++i) {
                        for (std::int64_t j = 0; j < 5; ++j) {
                            points.emplace_back(bx + (2 * i + 1) * bw / 10,
                                                by + (2 * j + 1) * bh / 10);
                        }
                    }
                };
                for (const light::Quad& q : p.quads) {
                    span(q.x, q.y, q.w, q.h);
                    if (mine) grid(q.x, q.y, q.w, q.h);
                }
                for (const light::TextRun& t : p.texts) {
                    const auto n = static_cast<std::int64_t>(t.text.size());
                    span(t.x, t.y, n * adv, line);
                    for (std::int64_t i = 0; mine && i < n; ++i) {
                        points.emplace_back(t.x + i * adv + adv / 2, t.y + line / 2);
                    }
                }
                for (const light::ImageBlit& im : p.images) {
                    span(im.x, im.y, im.w, im.h);
                    if (mine) grid(im.x, im.y, im.w, im.h);
                }
                for (const light::Line& l : p.lines) {
                    span((std::min)(l.x1, l.x2), (std::min)(l.y1, l.y2),
                         (std::max)(l.x1, l.x2) - (std::min)(l.x1, l.x2) + 1,
                         (std::max)(l.y1, l.y2) - (std::min)(l.y1, l.y2) + 1);
                    if (mine) points.emplace_back((l.x1 + l.x2) / 2, (l.y1 + l.y2) / 2);
                }
            }
            x0 = (std::max<std::int64_t>)(x0, 0);
            y0 = (std::max<std::int64_t>)(y0, 0);
            x1 = (std::min)(x1, a.w);
            y1 = (std::min)(y1, a.h);
            if (!any || x1 <= x0 || y1 <= y0) {
                continue;
            }
            const std::int64_t cx = (x0 + x1) / 2, cy = (y0 + y1) / 2;
            std::stable_sort(points.begin(), points.end(), [&](const auto& l, const auto& r) {
                const auto reach = [&](const auto& p) {
                    return (p.first - cx) * (p.first - cx) + (p.second - cy) * (p.second - cy);
                };
                return reach(l) < reach(r);
            });
            AreaPart part{rec.action, record_available(rec), x0, y0, x1 - x0, y1 - y0, false, 0,
                          0};
            bool seen = shows(a.x + cx, a.y + cy);
            for (const auto& [px, py] : points) {
                if (px < x0 || py < y0 || px >= x1 || py >= y1 || !shows(a.x + px, a.y + py)) {
                    continue;
                }
                seen = true;
                const std::string* top = top_at(px, py);
                const auto r = top == nullptr ? reached.end() : reached.find(*top);
                if (r != reached.end() && r->second == &rec) {
                    part.pressed = true;
                    part.px = px;
                    part.py = py;
                    break;
                }
            }
            if (seen) {
                parts.push_back(std::move(part));
            }
        }
        return {};
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
                if (light::hits_node(*k->prims, x - a->x, y - a->y, text_) &&
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

    /// SHAPED by an office that arranges in it: the backend sets the title and the size, kept
    /// within its display. As after a weaver's resize, the areas grow it again only once they reach
    /// further than they did (`fit`).
    void shape(const std::string& title, const PixelSize& want) {
        backend_.shape(title, want);
        dirty_ = true;
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

    /// The backend asked to grow along an axis when the areas' reach along it grew past where it
    /// last reached and past the window: so a window a weaver made smaller is not grown back until
    /// its areas reach further, nor by areas placed again along the other axis, one at a time.
    void fit(const std::vector<WinArea>& areas) {
        PixelSize want;
        for (const WinArea& a : areas) {
            want.width = (std::max)(want.width, far_edge(a.x, a.w));
            want.height = (std::max)(want.height, far_edge(a.y, a.h));
        }
        if (want == asked_) {
            return;
        }
        const bool wider = want.width > asked_.width && want.width > size_.width;
        const bool taller = want.height > asked_.height && want.height > size_.height;
        asked_ = want;
        if (wider || taller) {
            backend_.grow(PixelSize{wider ? want.width : size_.width,
                                    taller ? want.height : size_.height});
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

    /// `v` over `d`, rounded toward the lesser.
    static std::int64_t floor_div(std::int64_t v, std::int64_t d) {
        return v >= 0 ? v / d : -((-v + d - 1) / d);
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
    TextMeasure text_;
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
