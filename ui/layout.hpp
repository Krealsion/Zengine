// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_UI_LAYOUT_HPP
#define ZENGINE_UI_LAYOUT_HPP

// The UI package's RESOLVED side — what a viewport makes of authored intent, and
// which authored element is under a cell.
//
// Everything here is an OBSERVATION. Nothing here is content. That distinction is
// carried by three structural facts rather than by discipline:
//
//   1. Resolution cannot be performed without a viewport. `resolve()` takes one,
//      so "what size is this?" is not an answerable question about an element
//      alone -- which is the truth, and the reason W-0's inspector could show
//      `60%` and `28 x 6 cells` as two rows without either being wrong.
//   2. The result is a SEPARATE VALUE. A Scene is not stored on the elements it
//      observes; the authored side has no field able to hold one (see the fence
//      in vocabulary.hpp). Nothing caches: a resolved number that outlived the
//      viewport it was resolved against is exactly the stale lie the split
//      exists to prevent, so the answer is recomputed wherever it is wanted.
//   3. The resolved shapes are deliberately NOT ZEN_SHAPEs (asserted below).
//      They have no wire form, cannot be poked, and cannot be published as a
//      message. An observation that could be serialized alongside the authored
//      content would eventually be mistaken for it.
//
// Hit testing lives here, and not on the authored side, for the same reason: the
// question "what is under this cell" is only answerable about a resolved scene.
// What it ANSWERS with is the authored identity -- never a rectangle index, a
// label, or a copy of a painted item.

#include "ui/vocabulary.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <vector>

namespace zengine::ui {

/// What authored intent is resolved AGAINST, in cells — the same square unit
/// `zengine::surface::SurfaceCanvas` paints in, so a scene resolved here lands on
/// a canvas without a second conversion. (This package still knows nothing about
/// canvases; it shares a UNIT with one, which is not a dependency.)
///
/// Its members are plain numbers, and that is correct: a viewport is not authored
/// intent. It is the concrete thing intent is measured against, which is why
/// resolution takes one and an Element does not carry one.
struct Viewport {
    std::int64_t cells_w = 0;
    std::int64_t cells_h = 0;

    friend bool operator==(const Viewport&, const Viewport&) = default;
};

/// A resolved rectangle, in cells. Exists only on this side of the fence.
struct Rect {
    std::int64_t x = 0;
    std::int64_t y = 0;
    std::int64_t w = 0;
    std::int64_t h = 0;

    friend bool operator==(const Rect&, const Rect&) = default;

    /// TOTAL, for every rectangle this package can produce — and it has to be,
    /// for exactly the reason `resolve_extent` below does.
    ///
    /// The obvious spelling is `px >= x && px < x + w`, and `x + w` is signed
    /// overflow — UNDEFINED BEHAVIOUR, produced by data — as soon as an extent
    /// carries a large amount. That is not hypothetical: a cells extent resolves
    /// to itself, authored content is a ZEN_SHAPE, and a poke writes the amount
    /// past every application's check (Workshop says so about its own document).
    /// So a scene resolved from poked content can hold a rect whose right edge is
    /// not representable, and W-3 measured this one with a sanitizer, reached
    /// through an ordinary press: `hit` is what a maker's hand asks.
    ///
    /// The repair is the comparison, not the geometry. An empty or inverted
    /// rectangle contains nothing (which is what the old spelling already said),
    /// and past that the difference is taken in UNSIGNED arithmetic, which wraps
    /// by definition rather than being undefined — and cannot be wrong here,
    /// because the branch above has already established `px >= x`.
    bool contains(std::int64_t px, std::int64_t py) const noexcept {
        if (w <= 0 || h <= 0 || px < x || py < y) {
            return false;
        }
        using U = std::uint64_t;
        return (static_cast<U>(px) - static_cast<U>(x)) < static_cast<U>(w) &&
               (static_cast<U>(py) - static_cast<U>(y)) < static_cast<U>(h);
    }
};

/// One authored element, as this viewport places it: the AUTHORED IDENTITY plus
/// the rectangle that identity currently occupies.
///
/// It carries the id and not a pointer to the Element, and that is a deliberate
/// divergence from the Loom's PxTarget (which carries `const Widget*` into the
/// caller's tree). The reason is measured, not stylistic: an application's
/// elements live in a vector that reallocates the moment one is added, so a
/// pointer captured during layout is a dangling pointer one authoring gesture
/// later. The id is the identity — it is what survives the vector, and it is
/// already what the vocabulary says identity IS.
struct Placed {
    std::int64_t id = 0;
    Rect rect;

    friend bool operator==(const Placed&, const Placed&) = default;
};

/// A resolved scene: the viewport it was resolved against, and every element's
/// place in it, in AUTHORED ORDER — which is paint order, said once (the same
/// rule SurfaceCanvas states about its own rects). Later is in front.
///
/// It keeps its viewport so a scene can be asked what it is an observation OF.
/// A rectangle without the viewport that produced it is a number with its
/// meaning cut off.
struct Scene {
    Viewport viewport;
    std::vector<Placed> items;

    friend bool operator==(const Scene&, const Scene&) = default;
};

// The third fence: the resolved side has no wire form. `loom::Shape` is the substrate's own
// question ("does this carry a ZEN_SHAPE registration?"), asked here rather than a
// hand-rolled trait that could come to disagree with it. This is what keeps an observation
// from being stored, sent, or poked as though it were authored content.
static_assert(!loom::Shape<Rect> && !loom::Shape<Placed> && !loom::Shape<Scene> &&
                  !loom::Shape<Viewport>,
              "A resolved observation must have no wire form: it is not content, and a "
              "serializable one would eventually be stored beside the authored intent it is "
              "only a view of.");
static_assert(loom::Shape<Element> && loom::Shape<Extent>,
              "The authored side IS content, and travels as ordinary Zen shapes.");

/// The floor a share resolves to. A share never rounds an element out of existence: an
/// element a maker authored is one they meant to see, and losing it to arithmetic in a
/// narrow viewport would be the tool discarding their work.
inline constexpr std::int64_t kMinCells = 1;

/// Resolve one authored extent against one viewport span, in cells.
///
/// TOTAL, for every value the type can hold. That is a requirement of this being a shared
/// vocabulary rather than one application's private helper: authored content is a
/// ZEN_SHAPE, so it arrives from the wire and from a poke as well as from a validated
/// setter, and neither of those has been past anybody's check_extent. An out-of-range
/// share is clamped rather than trusted, and an absurd span divides before it multiplies —
/// `span * amount` on unvalidated int64 is signed overflow, which is undefined behaviour
/// produced by data. (W-0's Workshop-local resolve() did exactly that; the relocation is
/// what made it worth fixing, because a private helper's exposure is one document's and a
/// package's is every consumer's.)
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

/// Resolve a whole authored sequence against a viewport.
///
/// The ONE place authored intent becomes geometry. Everything downstream — painting, the
/// inspector's resolved reading, hit testing — reads the scene this produced, so there is
/// no second copy of the geometry able to fall out of step with the first. That single
/// path is the whole point of the package: W-0 had three call sites resolving extents
/// themselves, agreeing only because one person wrote all three.
inline Scene resolve(const std::vector<Element>& elements, Viewport viewport) {
    Scene scene;
    scene.viewport = viewport;
    scene.items.reserve(elements.size());
    for (const Element& e : elements) {
        scene.items.push_back(Placed{e.id, Rect{e.x, e.y,
                                                resolve_extent(e.width, viewport.cells_w),
                                                resolve_extent(e.height, viewport.cells_h)}});
    }
    return scene;
}

/// What is under this cell: the TOPMOST placed element containing it, or null for none.
///
/// Topmost means last in authored order, which is last painted — so the answer agrees
/// with what a person can actually see. Returns the Placed, so a caller gets both the
/// authored identity (`->id`) and the rectangle it hit, and never has to re-derive either.
inline const Placed* hit(const Scene& scene, std::int64_t cx, std::int64_t cy) noexcept {
    for (std::size_t i = scene.items.size(); i > 0; --i) {
        const Placed& p = scene.items[i - 1];
        if (p.rect.contains(cx, cy)) {
            return &p;
        }
    }
    return nullptr;
}

/// Where one authored identity landed, or null if this scene has no such element —
/// a normal answer, not an error: a selection can outlive its element.
inline const Placed* placed_for(const Scene& scene, std::int64_t id) noexcept {
    for (const Placed& p : scene.items) {
        if (p.id == id) {
            return &p;
        }
    }
    return nullptr;
}

} // namespace zengine::ui

#endif // ZENGINE_UI_LAYOUT_HPP
