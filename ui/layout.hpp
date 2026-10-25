// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_UI_LAYOUT_HPP
#define ZENGINE_UI_LAYOUT_HPP

// The UI package's solved side: what a room makes of authored parts, in whole pixels, and which
// part is under a pixel. Everything here is an observation, recomputed when wanted: never stored
// on the parts, never cached, and without a wire form. Reference: ui/docs/ui.md.

#include "ui/vocabulary.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <type_traits>
#include <vector>

namespace zengine::ui {

/// The root frame and the window's step, in whole pixels: the room's width and height, and the
/// smallest span the window can show -- 1 in a window of pixels, a cell in a terminal. Plain
/// numbers, because a room is measured, not authored.
struct Room {
    std::int64_t w = 0;
    std::int64_t h = 0;
    std::int64_t step = 1;

    friend bool operator==(const Room&, const Room&) = default;
};

/// A part's own measure, in pixels, said by its owner before the solve: what `default` and `fit`
/// read. An axis with none is not measured, and the part's word answers without it.
struct Measure {
    std::optional<std::int64_t> w;
    std::optional<std::int64_t> h;

    friend bool operator==(const Measure&, const Measure&) = default;
};

/// A solved rectangle, in pixels. It exists only on this side of the fence.
struct Rect {
    std::int64_t x = 0;
    std::int64_t y = 0;
    std::int64_t w = 0;
    std::int64_t h = 0;

    friend bool operator==(const Rect&, const Rect&) = default;

    /// Whether the pixel is inside. Total over every value: an empty or inverted rectangle
    /// contains nothing, and the far edge is compared in unsigned arithmetic, so a rectangle
    /// whose right edge `x + w` is past the int64 range, which poked content can solve to,
    /// answers without signed overflow.
    bool contains(std::int64_t px, std::int64_t py) const noexcept {
        if (w <= 0 || h <= 0 || px < x || py < y) {
            return false;
        }
        using U = std::uint64_t;
        return (static_cast<U>(px) - static_cast<U>(x)) < static_cast<U>(w) &&
               (static_cast<U>(py) - static_cast<U>(y)) < static_cast<U>(h);
    }
};

/// One part as a scene places it: its authored identity and the rectangle it occupies. It
/// carries the id, not a pointer into the parts, which a vector reallocates under.
struct Placed {
    std::int64_t id = 0;
    Rect rect;

    friend bool operator==(const Placed&, const Placed&) = default;
};

/// A solved scene: the room it observes, and each placed part in authored order, which is paint
/// order (later is in front) and hit order. The order `arrange` works in never reaches `items`.
/// A part whose context does not reach the root is absent, so `items` can be shorter than the
/// sequence it observes.
struct Scene {
    Room room;
    std::vector<Placed> items;

    friend bool operator==(const Scene&, const Scene&) = default;
};

// The solved side has no wire form and the authored side has one, asked through Loom's own
// `loom::Shape`: the first fires if a solved type becomes a ZEN_SHAPE, the second if an
// authored one stops being one.
static_assert(!loom::Shape<Rect> && !loom::Shape<Placed> && !loom::Shape<Scene> &&
                  !loom::Shape<Room> && !loom::Shape<Measure>,
              "A solved observation must have no wire form: it is not content, and a "
              "serializable one would eventually be stored beside the authored intent it is "
              "only a view of.");
static_assert(loom::Shape<Element> && loom::Shape<Size> && loom::Shape<Place>,
              "The authored side IS content, and travels as ordinary Zen shapes.");

// ---- Whole-pixel arithmetic, total over int64 ------------------------------------------

/// `a + b`, saturating at the ends of int64 instead of overflowing. A solved position is a
/// frame's origin plus an authored offset, and either can come from poked content; a saturated
/// position is outside every room.
inline constexpr std::int64_t add_px(std::int64_t a, std::int64_t b) noexcept {
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

/// `a - b`, saturating as `add_px` does.
inline constexpr std::int64_t sub_px(std::int64_t a, std::int64_t b) noexcept {
    constexpr std::int64_t kMax = (std::numeric_limits<std::int64_t>::max)();
    constexpr std::int64_t kMin = (std::numeric_limits<std::int64_t>::min)();
    if (b < 0) {
        return a > kMax + b ? kMax : a - b;
    }
    if (b > 0) {
        return a < kMin + b ? kMin : a - b;
    }
    return a;
}

namespace detail {

/// `floor(a / b)` for `b > 0`, rounding toward negative infinity.
inline constexpr std::int64_t floor_div(std::int64_t a, std::int64_t b) noexcept {
    const std::int64_t q = a / b;
    return (a % b != 0 && a < 0) ? q - 1 : q;
}

/// The step a room solves at: at least one pixel.
inline constexpr std::int64_t step_of(const Room& room) noexcept {
    return room.step > 0 ? room.step : 1;
}

/// A position floored to the step, counted from the frame's start (`p` is relative to it):
/// `p` less its remainder, saturating at int64's minimum, where the step below `p` is not a number.
inline constexpr std::int64_t floor_to_step(std::int64_t p, std::int64_t step) noexcept {
    const std::int64_t r = p % step;
    return sub_px(p, r < 0 ? r + step : r);
}

/// A length rounded up to the step, saturating.
inline constexpr std::int64_t ceil_to_step(std::int64_t v, std::int64_t step) noexcept {
    const std::int64_t down = floor_to_step(v, step);
    return down == v ? v : add_px(down, step);
}

/// `floor(span * k / whole)` for `span >= 0` and `0 <= k <= whole`, divided before it is
/// multiplied, so no `span` overflows: span = a * whole + b, and `a * k <= span`.
inline constexpr std::int64_t part_of(std::int64_t span, std::int64_t k,
                                      std::int64_t whole) noexcept {
    return span / whole * k + (span % whole) * k / whole;
}

/// Whether `span * k / whole` is a whole number, for the same operands as `part_of`.
inline constexpr bool part_is_whole(std::int64_t span, std::int64_t k,
                                    std::int64_t whole) noexcept {
    return (span % whole) * k % whole == 0;
}

inline constexpr std::int64_t clamp_share(std::optional<std::int64_t> amount) noexcept {
    const std::int64_t s = amount.value_or(0);
    return s < 0 ? 0 : (s > kShareWhole ? kShareWhole : s);
}

/// How one axis of a part's size is answered once the words are read against the measures and
/// the frames: a number of pixels known before the frame is (`Fixed`), the reach of the part's
/// own parts (`Reach`), or a reading of the frame's span (`Share`, `Fill`, `Span`).
enum class Reads : unsigned char { Fixed, Reach, Share, Fill, Span };

/// One axis of one part, on its way to a rectangle.
struct Axis {
    Reads extent = Reads::Fixed;
    std::int64_t length = 0; ///< `Fixed`'s pixels, or `Reach`'s once known
    std::int64_t share = 0;  ///< `Share`'s ten-thousandths, clamped
    Anchor anchor = Anchor::Start;
    std::int64_t offset = 0;   ///< inward from the anchored edge; 0 where the word reads none
    bool offset_share = false; ///< `offset` is ten-thousandths of the span, clamped, not pixels
};

/// A part's near and far edges along one axis, relative to its frame's start.
struct Edges {
    std::int64_t from = 0;
    std::int64_t to = 0;
};

/// The edges of a part along one axis of a frame of `span` (>= 0). A pixel offset or a pixel
/// length is exact; an edge a share or a centring computes -- a share place's start among them
/// -- is the exact rational position floored to the step from the frame's start, so two parts
/// that name one position meet on one pixel. `read_from_start` reads every anchor from the start:
/// the frame's span is not known to its parts (it fits them).
inline Edges edges_of(const Axis& a, std::int64_t span, std::int64_t step,
                      bool read_from_start) noexcept {
    const Anchor anchor = read_from_start ? Anchor::Start : a.anchor;
    std::int64_t o = a.offset;
    if (read_from_start && a.anchor == Anchor::Center) {
        o = 0;
    } else if (a.offset_share) {
        o = floor_to_step(part_of(span, a.offset, kShareWhole), step);
    }
    Edges e;
    switch (a.extent) {
    case Reads::Fill:
        if (anchor == Anchor::Start) {
            e.from = o;
            e.to = span;
        } else if (anchor == Anchor::End) {
            e.from = 0;
            e.to = sub_px(span, o);
        } else {
            e.from = 0;
            e.to = span;
        }
        if (e.to < e.from) {
            e.to = e.from; // past the frame's far edge: nothing left to fill
        }
        return e;
    case Reads::Share: {
        const std::int64_t s = a.share;
        if (anchor == Anchor::Start) {
            e.from = o;
            e.to = floor_to_step(add_px(o, part_of(span, s, kShareWhole)), step);
        } else if (anchor == Anchor::End) {
            e.to = sub_px(span, o);
            // floor(span - o - span*s/whole) = span - o - ceil(span*s/whole)
            const std::int64_t whole_part = part_of(span, s, kShareWhole);
            const std::int64_t up =
                part_is_whole(span, s, kShareWhole) ? whole_part : add_px(whole_part, 1);
            e.from = floor_to_step(sub_px(e.to, up), step);
        } else {
            e.from = floor_to_step(part_of(span, kShareWhole - s, 2 * kShareWhole), step);
            e.to = floor_to_step(part_of(span, kShareWhole + s, 2 * kShareWhole), step);
        }
        return e;
    }
    case Reads::Fixed:
    case Reads::Reach:
    case Reads::Span:
    default: {
        const std::int64_t len = a.extent == Reads::Span ? span : a.length;
        if (anchor == Anchor::Start) {
            e.from = o;
            e.to = add_px(o, len);
        } else if (anchor == Anchor::End) {
            e.to = sub_px(span, o);
            e.from = sub_px(e.to, len);
        } else {
            e.from = floor_to_step(floor_div(sub_px(span, len), 2), step);
            e.to = add_px(e.from, len);
        }
        return e;
    }
    }
}

/// How one axis of a part reads, before any frame is known: its extent from its size word and
/// measure, its anchor and offset from its place word -- a share of the span, clamped, where the
/// word reads one (`offset_share`).
inline Axis axis_of(const Size& size, std::optional<std::int64_t> measure, Anchor anchor,
                    bool reads_offset, std::optional<std::int64_t> offset, std::int64_t step,
                    bool offset_share = false) noexcept {
    Axis a;
    a.anchor = anchor;
    a.offset_share = reads_offset && offset_share;
    if (a.offset_share) {
        a.offset = clamp_share(offset);
    } else {
        a.offset = reads_offset ? offset.value_or(0) : 0;
    }
    switch (size.mode) {
    case kSizePixels:
        a.extent = Reads::Fixed;
        a.length = size.amount.value_or(0);
        break;
    case kSizeShare:
        a.extent = Reads::Share;
        a.share = clamp_share(size.amount);
        break;
    case kSizeFill:
        a.extent = Reads::Fill;
        break;
    case kSizeFit:
        if (measure) {
            a.extent = Reads::Fixed;
            a.length = ceil_to_step(*measure, step);
        } else {
            a.extent = Reads::Reach;
        }
        break;
    case kSizeDefault:
    default: // a number no word is reads as `default`
        if (measure) {
            a.extent = Reads::Fixed;
            a.length = *measure;
        } else {
            a.extent = Reads::Span;
        }
        break;
    }
    return a;
}

/// An axis that reads its frame's span, in a frame whose span is its parts' reach, reads as
/// `fit` instead: its measure rounded up to the step where one is said, else its own parts'
/// reach. The frame is measured from the part, so the part cannot be measured from the frame
/// (`problem` names the intent; this is what the solve does with it).
inline void read_as_fit(Axis& a, std::optional<std::int64_t> measure, std::int64_t step) noexcept {
    if (a.extent == Reads::Share || a.extent == Reads::Fill || a.extent == Reads::Span) {
        if (measure) {
            a.extent = Reads::Fixed;
            a.length = ceil_to_step(*measure, step);
        } else {
            a.extent = Reads::Reach;
            a.length = 0;
        }
    }
}

} // namespace detail

/// The root's frame: the whole room, at the origin. A part whose context is kRootContext stands
/// in it.
inline constexpr Rect root_frame(const Room& room) noexcept {
    return Rect{0, 0, room.w, room.h};
}

/// Solve a whole authored sequence in a room: the one place authored intent becomes geometry,
/// so whatever paints, reads or hit-tests one Scene agrees. `measures[i]` is `parts[i]`'s own
/// measure; a part past the end of `measures` has none.
///
/// Total over every value the types hold, validated or not. It works once, on the heap, in
/// dependency order -- the reach of a frame that fits its parts after those parts, every frame's
/// rectangle before the parts that stand in it -- and answers in authored order. A part whose
/// chain does not reach the root, through a loop or a context no part carries, is not placed:
/// absent, never placed in the root instead.
///
/// A free frame places each part by its own words: a pixel offset and a pixel length are exact;
/// an edge a share or a centring computes is the exact position floored to the step from the
/// frame's start; `fill` runs from the anchored edge to the frame's far one. A kFlowStack frame
/// lays its parts down in authored order, a slot each: a slot is the part's measured height, or
/// its own height where none is said, `gap` below the one before, beginning again at the top
/// where a slot would pass the floor; across, each part stands as its place says. A size read
/// from a frame that fits it to its parts reads as `fit` (`detail::read_as_fit`).
inline Scene arrange(const std::vector<Element>& parts, const Room& room,
                     const std::vector<Measure>& measures = {}) {
    using detail::Axis;
    using detail::Reads;
    constexpr std::size_t kRoot = static_cast<std::size_t>(-1);
    const std::int64_t step = detail::step_of(room);
    const std::size_t n = parts.size();

    Scene scene;
    scene.room = room;

    // Each part's frame, by position: the first part carrying its context, or the root.
    const ById index(parts);
    std::vector<std::size_t> frame(n, kRoot);
    std::vector<char> missing(n, 0);
    for (std::size_t i = 0; i < n; ++i) {
        if (parts[i].context != kRootContext) {
            frame[i] = index.find(parts[i].context);
            missing[i] = frame[i] == ById::npos ? 1 : 0;
        }
    }

    // The parts standing in each frame, in authored order; the root's first. A part whose
    // context nothing carries stands in nothing, and a loop is never reached from the root, so
    // the walk below meets exactly the parts whose chains reach it.
    std::vector<std::vector<std::size_t>> inside(n);
    std::vector<std::size_t> in_root;
    for (std::size_t i = 0; i < n; ++i) {
        if (missing[i] != 0) {
            continue;
        }
        (frame[i] == kRoot ? in_root : inside[frame[i]]).push_back(i);
    }
    std::vector<std::size_t> order; // frames before the parts in them
    order.reserve(n);
    for (const std::size_t i : in_root) {
        order.push_back(i);
    }
    for (std::size_t k = 0; k < order.size(); ++k) {
        for (const std::size_t i : inside[order[k]]) {
            order.push_back(i);
        }
    }

    // How each axis reads, frames first, so a part knows whether its frame fits it.
    const auto measure_of = [&](std::size_t i, bool across) -> std::optional<std::int64_t> {
        if (i >= measures.size()) {
            return std::nullopt;
        }
        return across ? measures[i].w : measures[i].h;
    };
    std::vector<Axis> ax(n), ay(n);
    for (const std::size_t i : order) {
        const Element& e = parts[i];
        const std::int64_t mode = e.place.mode;
        ax[i] = detail::axis_of(e.width, measure_of(i, true), place_anchor_x(mode),
                                place_reads_x(mode), e.place.x, step, place_reads_share(mode));
        ay[i] = detail::axis_of(e.height, measure_of(i, false), place_anchor_y(mode),
                                place_reads_y(mode), e.place.y, step, place_reads_share(mode));
        if (frame[i] != kRoot) {
            if (ax[frame[i]].extent == Reads::Reach) {
                detail::read_as_fit(ax[i], measure_of(i, true), step);
            }
            if (ay[frame[i]].extent == Reads::Reach) {
                detail::read_as_fit(ay[i], measure_of(i, false), step);
            }
        }
    }

    // The reaches, each frame after the parts in it. A part read in a frame that fits it reads
    // its anchor from the frame's start, as `edges_of` does with `read_from_start`.
    const auto slot_of = [&](std::size_t i) {
        const std::optional<std::int64_t> said = measure_of(i, false);
        return said ? *said : ay[i].length;
    };
    for (std::size_t k = order.size(); k > 0; --k) {
        const std::size_t f = order[k - 1];
        const bool stack = parts[f].flow == kFlowStack;
        const std::int64_t gap = parts[f].gap.value_or(0);
        if (ax[f].extent == Reads::Reach) {
            std::int64_t reach = 0;
            for (const std::size_t i : inside[f]) {
                const detail::Edges e = detail::edges_of(ax[i], 0, step, true);
                reach = e.to > reach ? e.to : reach;
            }
            ax[f].length = detail::ceil_to_step(reach, step);
        }
        if (ay[f].extent == Reads::Reach) {
            std::int64_t reach = 0;
            if (stack) {
                for (std::size_t j = 0; j < inside[f].size(); ++j) {
                    reach = add_px(reach, slot_of(inside[f][j]));
                    if (j + 1 < inside[f].size()) {
                        reach = add_px(reach, gap);
                    }
                }
            } else {
                for (const std::size_t i : inside[f]) {
                    const detail::Edges e = detail::edges_of(ay[i], 0, step, true);
                    reach = e.to > reach ? e.to : reach;
                }
            }
            ay[f].length = detail::ceil_to_step(reach, step);
        }
    }

    // The rectangles, each frame before the parts in it.
    std::vector<Rect> rect(n);
    std::vector<char> placed(n, 0);
    const auto lay = [&](const Rect& f, bool fits_x, bool fits_y, bool stack, std::int64_t gap,
                         const std::vector<std::size_t>& in) {
        const std::int64_t span_x = f.w > 0 ? f.w : 0;
        const std::int64_t span_y = f.h > 0 ? f.h : 0;
        std::int64_t cursor = 0;
        for (const std::size_t i : in) {
            const detail::Edges ex = detail::edges_of(ax[i], span_x, step, fits_x);
            detail::Edges ey;
            if (stack) {
                // Down a column, a part stands at its slot's top; its slot is its measured
                // height, or its own height where none is said.
                Axis down = ay[i];
                down.anchor = Anchor::Start;
                down.offset = 0;
                const std::optional<std::int64_t> said = measure_of(i, false);
                const detail::Edges own = detail::edges_of(down, span_y, step, fits_y);
                const std::int64_t slot = said ? *said : sub_px(own.to, own.from);
                if (!fits_y && cursor > 0 && add_px(cursor, slot) > span_y) {
                    cursor = 0; // the column begins again at its top
                }
                down.offset = cursor;
                ey = detail::edges_of(down, span_y, step, fits_y);
                cursor = add_px(add_px(cursor, slot), gap);
            } else {
                ey = detail::edges_of(ay[i], span_y, step, fits_y);
            }
            rect[i] = Rect{add_px(f.x, ex.from), add_px(f.y, ey.from), sub_px(ex.to, ex.from),
                           sub_px(ey.to, ey.from)};
            placed[i] = 1;
        }
    };
    lay(root_frame(room), false, false, false, 0, in_root);
    for (const std::size_t f : order) {
        if (!inside[f].empty()) {
            lay(rect[f], ax[f].extent == Reads::Reach, ay[f].extent == Reads::Reach,
                parts[f].flow == kFlowStack, parts[f].gap.value_or(0), inside[f]);
        }
    }

    scene.items.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        if (placed[i] != 0) {
            scene.items.push_back(Placed{parts[i].id, rect[i]});
        }
    }
    return scene;
}

/// What is under this pixel: the topmost placed part containing it, or null. Topmost is last in
/// authored order, which is last painted, so the answer is what a person sees there.
inline const Placed* hit(const Scene& scene, std::int64_t px, std::int64_t py) noexcept {
    for (std::size_t i = scene.items.size(); i > 0; --i) {
        const Placed& p = scene.items[i - 1];
        if (p.rect.contains(px, py)) {
            return &p;
        }
    }
    return nullptr;
}

/// Where one identity landed, or null, which is a normal answer: a selection can outlive its
/// part, and a part whose chain does not reach the root is never placed. A repeated identity
/// answers with the first placed part carrying it.
inline const Placed* placed_for(const Scene& scene, std::int64_t id) noexcept {
    for (const Placed& p : scene.items) {
        if (p.id == id) {
            return &p;
        }
    }
    return nullptr;
}

/// The frame `e` stood in, as this scene solved it: the root frame for kRootContext, otherwise
/// its frame's placed rectangle. Ask it instead of reconstructing a frame's position, which would
/// be a second copy of the geometry.
///
/// Total: a frame this scene did not place gives an empty frame. The frame is found as
/// `placed_for` finds it, so with a repeated identity it can be a frame `arrange` did not use.
inline Rect frame_in(const Scene& scene, const Element& e) noexcept {
    if (e.context == kRootContext) {
        return root_frame(scene.room);
    }
    const Placed* source = placed_for(scene, e.context);
    return source == nullptr ? Rect{} : source->rect;
}

} // namespace zengine::ui

#endif // ZENGINE_UI_LAYOUT_HPP
