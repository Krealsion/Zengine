// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_UI_LAYOUT_HPP
#define ZENGINE_UI_LAYOUT_HPP

// The UI package's resolved side: what a viewport makes of authored elements, and which element
// is under a cell. Everything here is an observation, recomputed when wanted: never stored on the
// elements, never cached, and without a wire form. Reference: ui/docs/ui.md.

#include "ui/vocabulary.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <vector>

namespace zengine::ui {

/// The root frame's size, in cells: the square unit `zengine::surface::SurfaceCanvas` paints in,
/// so a scene lands on a canvas without conversion. The unit is shared; nothing here depends on
/// the surface package. Plain numbers, because a viewport is measured, not authored.
struct Viewport {
    std::int64_t cells_w = 0;
    std::int64_t cells_h = 0;

    friend bool operator==(const Viewport&, const Viewport&) = default;
};

/// A resolved rectangle, in cells. It exists only on this side of the fence.
struct Rect {
    std::int64_t x = 0;
    std::int64_t y = 0;
    std::int64_t w = 0;
    std::int64_t h = 0;

    friend bool operator==(const Rect&, const Rect&) = default;

    /// Whether the cell is inside. Total over every value: an empty or inverted rectangle contains
    /// nothing, and the far edge is compared in unsigned arithmetic, so a rectangle whose right
    /// edge `x + w` is past the int64 range, which poked content can resolve to, answers without
    /// signed overflow.
    bool contains(std::int64_t px, std::int64_t py) const noexcept {
        if (w <= 0 || h <= 0 || px < x || py < y) {
            return false;
        }
        using U = std::uint64_t;
        return (static_cast<U>(px) - static_cast<U>(x)) < static_cast<U>(w) &&
               (static_cast<U>(py) - static_cast<U>(y)) < static_cast<U>(h);
    }
};

/// One element as a scene places it: its authored identity and the rectangle it occupies. It
/// carries the id, not a pointer into the elements, which a vector reallocates under.
struct Placed {
    std::int64_t id = 0;
    Rect rect;

    friend bool operator==(const Placed&, const Placed&) = default;
};

/// A resolved scene: the viewport it observes, and each placed element in authored order, which
/// is paint order (later is in front) and hit order. The order `resolve` works in never reaches
/// `items`. An element whose context does not reach the root is absent, so `items` can be shorter
/// than the sequence it observes.
struct Scene {
    Viewport viewport;
    std::vector<Placed> items;

    friend bool operator==(const Scene&, const Scene&) = default;
};

// The resolved side has no wire form and the authored side has one, asked through Loom's own
// `loom::Shape`: the first fires if a resolved type becomes a ZEN_SHAPE, the second if an
// authored one stops being one.
static_assert(!loom::Shape<Rect> && !loom::Shape<Placed> && !loom::Shape<Scene> &&
                  !loom::Shape<Viewport>,
              "A resolved observation must have no wire form: it is not content, and a "
              "serializable one would eventually be stored beside the authored intent it is "
              "only a view of.");
static_assert(loom::Shape<Element> && loom::Shape<Extent>,
              "The authored side IS content, and travels as ordinary Zen shapes.");

/// The fewest cells a share resolves to, so a narrow frame never resolves an authored element out
/// of existence. A cells extent is not floored.
inline constexpr std::int64_t kMinCells = 1;

/// Resolve one authored extent against one span, in cells.
///
/// Total over every value the type can hold, validated or not: cells, and any mode but
/// kExtentPercent, resolve to their amount; a share is clamped to 0..100, taken of a span too
/// large to multiply by dividing first, and floored at kMinCells; a span of zero or less gives
/// kMinCells.
inline std::int64_t resolve_extent(const Extent& e, std::int64_t span) noexcept {
    if (e.mode != kExtentPercent) {
        return e.amount; // cells (and any unknown mode) resolve to themselves
    }
    if (span <= 0) {
        return kMinCells; // no viewport to take a share of
    }
    std::int64_t pct = e.amount;
    if (pct < 0) {
        pct = 0;
    } else if (pct > 100) {
        pct = 100;
    }
    constexpr std::int64_t kSafeSpan = (std::numeric_limits<std::int64_t>::max)() / 100;
    const std::int64_t cells = (span <= kSafeSpan) ? (span * pct / 100) : (span / 100 * pct);
    return cells < kMinCells ? kMinCells : cells;
}

/// `a + b` in cells, saturating at the ends of int64 instead of overflowing. A resolved position
/// is a frame's origin plus an authored offset, and either can come from poked content; a
/// saturated position is outside every viewport.
inline std::int64_t add_cells(std::int64_t a, std::int64_t b) noexcept {
    constexpr std::int64_t kMax = (std::numeric_limits<std::int64_t>::max)();
    constexpr std::int64_t kMin = (std::numeric_limits<std::int64_t>::min)();
    if (b > 0) {
        return a > kMax - b ? kMax : a + b;
    }
    if (b < 0) {
        return a < kMin - b ? kMin : a + b;
    }
    return a;
}

/// The root's frame: the whole viewport, at the origin. An element whose context is kRootContext
/// is read in it.
inline Rect root_frame(Viewport viewport) noexcept {
    return Rect{0, 0, viewport.cells_w, viewport.cells_h};
}

/// One authored element read in one frame: the frame's origin plus the authored offset, and each
/// extent resolved against the frame's span. Total, as `add_cells` and `resolve_extent` are.
inline Rect resolve_in(const Element& e, const Rect& context) noexcept {
    return Rect{add_cells(context.x, e.x), add_cells(context.y, e.y),
                resolve_extent(e.width, context.w), resolve_extent(e.height, context.h)};
}

/// Resolve a whole authored sequence against a viewport: the one place authored intent becomes
/// geometry, so whatever paints, reads or hit-tests one Scene agrees.
///
/// Each element is resolved once, a source before whatever measures against it wherever either
/// sits, iteratively and with no depth ceiling; `items` come out in authored order regardless.
/// An element whose chain does not reach the root, through a cycle or a context no element
/// carries, is not placed: absent, never resolved against the root instead. Total, so it cannot
/// refuse such a sequence; an application that wants one refused checks it with `walk_context`
/// (ui/docs/ui.md#a-broken-chain-is-absent).
inline Scene resolve(const std::vector<Element>& elements, Viewport viewport) {
    enum : unsigned char { kTodo = 0, kWorking = 1, kDone = 2, kUnplaceable = 3 };

    Scene scene;
    scene.viewport = viewport;

    const ById index(elements);
    const Rect root = root_frame(viewport);
    std::vector<unsigned char> state(elements.size(), kTodo);
    std::vector<Rect> rect(elements.size());
    std::vector<std::size_t> stack;

    for (std::size_t i = 0; i < elements.size(); ++i) {
        if (state[i] != kTodo) {
            continue;
        }
        // Up the chain. Each element is pushed at most once in the whole pass -- pushing marks
        // it -- so the work is one visit and one lookup per element: O(n log n).
        stack.clear();
        Rect base{};
        bool broken = false;
        std::size_t at = i;
        while (true) {
            if (state[at] == kWorking) {
                broken = true; // we have walked back onto our own path: a cycle
                break;
            }
            if (state[at] == kDone) {
                base = rect[at];
                break;
            }
            if (state[at] == kUnplaceable) {
                broken = true;
                break;
            }
            state[at] = kWorking;
            stack.push_back(at);
            const std::int64_t context = elements[at].context;
            if (context == kRootContext) {
                base = root;
                break;
            }
            const std::size_t source = index.find(context);
            if (source == ById::npos) {
                broken = true; // it names an identity nothing carries
                break;
            }
            at = source;
        }
        // Down again. Everything on the stack shares one verdict: an element
        // whose source cannot be placed cannot be placed either.
        if (broken) {
            for (const std::size_t on : stack) {
                state[on] = kUnplaceable;
            }
            continue;
        }
        for (std::size_t k = stack.size(); k > 0; --k) {
            const std::size_t on = stack[k - 1];
            rect[on] = resolve_in(elements[on], base);
            state[on] = kDone;
            base = rect[on];
        }
    }

    scene.items.reserve(elements.size());
    for (std::size_t i = 0; i < elements.size(); ++i) {
        if (state[i] == kDone) {
            scene.items.push_back(Placed{elements[i].id, rect[i]});
        }
    }
    return scene;
}

/// What is under this cell: the topmost placed element containing it, or null. Topmost is last
/// in authored order, which is last painted, so the answer is what a person sees there.
inline const Placed* hit(const Scene& scene, std::int64_t cx, std::int64_t cy) noexcept {
    for (std::size_t i = scene.items.size(); i > 0; --i) {
        const Placed& p = scene.items[i - 1];
        if (p.rect.contains(cx, cy)) {
            return &p;
        }
    }
    return nullptr;
}

/// Where one identity landed, or null, which is a normal answer: a selection can outlive its
/// element, and an element whose chain does not reach the root is never placed. A repeated
/// identity answers with the first placed element carrying it.
inline const Placed* placed_for(const Scene& scene, std::int64_t id) noexcept {
    for (const Placed& p : scene.items) {
        if (p.id == id) {
            return &p;
        }
    }
    return nullptr;
}

/// The frame `e`'s values were read in, as this scene resolved it: the root frame for
/// kRootContext, otherwise its source's placed rectangle. Ask it instead of reconstructing a
/// source's position, which would be a second copy of the geometry.
///
/// Total: a source this scene did not place gives an empty frame. The source is found as
/// `placed_for` finds it, so with a repeated identity it can be a frame `resolve` did not use.
inline Rect frame_in(const Scene& scene, const Element& e) noexcept {
    if (e.context == kRootContext) {
        return root_frame(scene.viewport);
    }
    const Placed* source = placed_for(scene, e.context);
    return source == nullptr ? Rect{} : source->rect;
}

} // namespace zengine::ui

#endif // ZENGINE_UI_LAYOUT_HPP
