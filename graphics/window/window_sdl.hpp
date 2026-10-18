// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_GRAPHICS_WINDOW_WINDOW_SDL_HPP
#define ZENGINE_GRAPHICS_WINDOW_WINDOW_SDL_HPP

// The OS window: the window (graphics/window/window.hpp) over a medium of light and an injected
// pixel Backend. It keeps each node painted as pixel primitives (graphics/light_paint.hpp), and
// when anything changed hands the backend the whole picture, back to front, each area in its own
// bounds; a press lands on the primitives shown. No SDL here: the SDL backend is the weave's
// (graphics/window/window_sdl.cpp). Reference: graphics/docs/zengine.window.sdl.md.

#include "graphics/light_paint.hpp"
#include "graphics/window/compose.hpp"
#include "graphics/window/window.hpp"

#include "input/translate.hpp"
#include "input/vocabulary.hpp"
#include "manuals/zengine.window.sdl.hpp" // this office's manual, compiled in from its page

#include <algorithm>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
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
};

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
/// What it points at is the window's, valid for the one call it is handed to.
struct ComposedLightFrame {
    std::int64_t width = 0, height = 0;
    std::uint32_t ground = 0;
    std::vector<LightAreaShown> areas;
};

/// What a backend read: its keys, text and presses, and whether its picture must be drawn again
/// (the desktop uncovered it).
struct PixelPolled {
    std::vector<zengine::input::InputEvent> events;
    bool redraw = false;
};

/// A medium of light over a Backend: `claim()`, `size()` in pixels, `present(frame)`, true when it
/// drew the picture, and `poll()`.
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
            kept[np.path] = Kept{n->second->key, std::move(*prims), ++versions_};
        }
        dirty_ = true;
    }

    /// The whole picture handed to the backend, back to front, once anything changed: one showing
    /// when the backend drew it (with no OS window, it keeps the picture and shows nothing).
    void show(const std::vector<WinArea>& areas) {
        if (!dirty_) {
            return;
        }
        dirty_ = false;
        ComposedLightFrame frame;
        frame.width = size_.width;
        frame.height = size_.height;
        const auto order = back_to_front(areas);
        frame.ground = light::rgba_of(order.empty() ? light::night_look().ground
                                                    : look_of(*order.front()).ground);
        std::int64_t bytes = 0;
        for (const WinArea* a : order) {
            LightAreaShown shown{a->area, a->x, a->y, a->w, a->h, {}};
            for (const auto& [path, k] : painted(a->area)) {
                shown.nodes.push_back(LightNodeShown{*path, k->version, &k->prims});
                bytes += weight(k->prims);
            }
            frame.areas.push_back(std::move(shown));
        }
        if (backend_.present(frame)) {
            ++showings_;
            device_bytes_ += bytes;
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
                for (const light::TextRun& t : k->prims.texts) {
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

    /// A press at a pixel of the window (a press in cells at its cell's centre): the front-most
    /// area there, and the top node whose primitives hold the point; an area shows what lies
    /// behind it wherever it painted nothing there.
    Landed land(const std::vector<WinArea>& areas, const zengine::input::PointerButton& b) const {
        const bool cells = b.space == zengine::input::space::kCells;
        const std::int64_t x = cells ? b.x * kCellPx + kCellPx / 2 : b.x;
        const std::int64_t y = cells ? b.y * kCellPx + kCellPx / 2 : b.y;
        Landed out;
        for (const WinArea* a : front_to_back(areas)) {
            if (x < a->x || y < a->y || x >= a->x + a->w || y >= a->y + a->h) {
                continue;
            }
            if (out.area == nullptr) {
                out.area = a;
            }
            const Kept* best = nullptr;
            const std::string* best_path = nullptr;
            for (const auto& [path, k] : painted(a->area)) {
                if (light::hits_node(k->prims, x - a->x, y - a->y) &&
                    (best == nullptr || best->key < k->key)) {
                    best = k;
                    best_path = path;
                }
            }
            if (best != nullptr) {
                out.area = a;
                out.path = *best_path;
                break;
            }
        }
        if (out.area != nullptr) {
            out.x = x - out.area->x;
            out.y = y - out.area->y;
        }
        return out;
    }

    std::vector<zengine::input::InputEvent> poll() {
        PixelPolled polled = backend_.poll();
        if (polled.redraw) {
            dirty_ = true;
        }
        return std::move(polled.events);
    }

    std::int64_t showings() const { return showings_; }
    std::int64_t device_bytes() const { return device_bytes_; }

private:
    struct Kept {
        std::vector<std::int64_t> key;
        light::NodePrimitives prims;
        std::uint64_t version = 0;
    };

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
    bool dirty_ = false;
    std::uint64_t versions_ = 0;
    std::int64_t showings_ = 0;
    std::int64_t device_bytes_ = 0;
};

/// The OS window over `Backend`.
template <class Backend>
using SdlWindowT = Window<LightMedium<Backend>>;

} // namespace zengine::graphics::window

#endif // ZENGINE_GRAPHICS_WINDOW_WINDOW_SDL_HPP
