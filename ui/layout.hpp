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
/// AUTHORED ORDER SURVIVED W-6, and that is one of the phase's findings rather
/// than an accident. Composition introduces a second ordering concern -- an
/// element cannot be resolved before the element it measures against -- and the
/// obvious implementation is to sort the document into dependency order and walk
/// it. That would have made document order mean dependency order, silently
/// changing which rectangle paints over which and which one a click finds. So
/// resolution orders its own WORK internally and emits its ANSWERS in document
/// order (see `resolve`), and the two concepts stay separate: dependency order
/// is an implementation detail of one function, presentation order is the
/// document's and is still what a maker arranged.
///
/// An element whose context could not be resolved is NOT IN THE SCENE at all, so
/// `items` is not necessarily one-to-one with the elements it observes. See
/// `resolve` for what makes that reachable and why it is an absence rather than
/// a guess.
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

/// `a + b` in cells, without leaving the number line.
///
/// W-6 needs it and W-5 did not, which is the arithmetic shape of what the phase
/// changed. A resolved position used to BE the authored one -- `resolve` copied
/// x/y through, so there was no sum to overflow. It is now `the context's origin
/// + the authored offset`, and both terms are values this package does not own:
/// authored content is a ZEN_SHAPE, so it arrives from a poke, and a context's
/// origin is itself the result of one of these sums further down a chain.
/// `INT64_MAX + 1` is undefined behaviour produced by data. The saturated ends
/// are far outside any viewport, which already means "nothing reachable there".
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

/// The frame the ROOT supplies: the whole viewport, at the origin.
///
/// This is the line W-6 went looking for. Before it, the viewport WAS resolution
/// context -- not as a value anything could name or replace, but as two
/// hard-coded assumptions inside one statement of `resolve`: an origin of 0,0
/// that authored placement was added to (invisibly, because adding zero looks
/// like copying), and a span that every extent took its share of. Naming it as a
/// frame is most of the mechanism; the rest is letting an element say a
/// different one.
inline Rect root_frame(Viewport viewport) noexcept {
    return Rect{0, 0, viewport.cells_w, viewport.cells_h};
}

/// ONE authored shape, in ONE context. The whole of what resolution means.
///
///     authored shape + resolution context = resolved shape
///
/// A frame supplies both halves of the context because both halves are one
/// measurement: the origin the offsets are counted from, and the span the shares
/// are shares OF. They are not welded together for tradition's sake -- they are
/// the two things you need in order to read `x = 2, width = 50%` as a rectangle,
/// and a frame is exactly the smallest value that carries them. An element that
/// wanted its position from one source and its size from another would be a
/// SECOND context field on the element, not a different shape of frame; the
/// arithmetic below would not change at all. Nothing here forecloses that, and
/// nothing here builds it, because no consumer has asked.
///
/// TOTAL, for the same reason `resolve_extent` is: every operand can come from a
/// poke or off the wire.
inline Rect resolve_in(const Element& e, const Rect& context) noexcept {
    return Rect{add_cells(context.x, e.x), add_cells(context.y, e.y),
                resolve_extent(e.width, context.w), resolve_extent(e.height, context.h)};
}

/// Resolve a whole authored sequence against a viewport.
///
/// The ONE place authored intent becomes geometry. Everything downstream — painting, the
/// inspector's resolved reading, hit testing — reads the scene this produced, so there is
/// no second copy of the geometry able to fall out of step with the first. That single
/// path is the whole point of the package: W-0 had three call sites resolving extents
/// themselves, agreeing only because one person wrote all three.
///
/// HOW IT ORDERS ITS WORK, and why the answers come out in a different order than the
/// work was done. Each element is resolved once, memoised, by walking UP its context
/// chain onto an explicit stack and then unwinding it — so a source is resolved before
/// whatever measures against it no matter where either of them sits in the document.
/// The `items` it emits are in DOCUMENT order regardless, because document order is
/// paint order, hit order and list order, and quietly reordering the document to make
/// resolution easier would have changed all three (see Scene).
///
/// THE STACK IS ON THE HEAP, and that is a semantic claim rather than an implementation
/// note: nothing here recurses, so how deep a composition may go is not decided by how
/// much C++ stack the host happens to have. There is no depth ceiling in this function,
/// and none anywhere else either.
///
/// WHAT IT DOES ABOUT A CHAIN THAT DOES NOT REACH THE ROOT, which is the interesting
/// half. A cycle, or a context naming an identity nothing carries, has no resolved
/// geometry — there is no frame to read the numbers in. So such an element is simply
/// NOT PLACED: it is absent from the scene, and therefore unpainted, unhittable, and
/// answered about with the null that `placed_for` already documents as a normal answer.
///
/// It is an ABSENCE and never a guess, and the difference is load-bearing. Falling back
/// to the root would resolve the element against a DIFFERENT relationship than the one
/// it names and show a confident rectangle in the wrong place; there is no "nearest
/// legal" reading of a broken reference the way there is of a 500% share. Nor could
/// this function refuse, because it has no way to say so: it is a shared vocabulary's
/// total function, and refusing is the DOCUMENT's job (Workshop's `check_document`
/// refuses both faults, so an element can only get here through a poke or through an
/// application that has no such law — the same widened input domain `resolve_extent`
/// already answers for).
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
        // Up the chain. Each element is pushed at most once in the whole pass --
        // pushing marks it -- so the total work is one visit per element plus one
        // lookup each, and the shape is O(n log n) rather than O(n * depth).
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
/// a normal answer, not an error: a selection can outlive its element, and since
/// W-6 an element whose context does not reach the root is never placed at all.
inline const Placed* placed_for(const Scene& scene, std::int64_t id) noexcept {
    for (const Placed& p : scene.items) {
        if (p.id == id) {
            return &p;
        }
    }
    return nullptr;
}

/// The frame ONE element's authored values were read in, as this scene resolved
/// it — the root's rectangle when it measures against the root, the source's
/// resolved rectangle when it measures against another element.
///
/// It exists so that a caller who needs the context (to turn a pointer's global
/// position into an authored local one, or to ask what span a share is a share
/// OF) asks the resolver rather than reconstructing the answer. That is W-1's
/// one-place-resolves lesson spent a second time: a gesture that computed
/// "well, the parent is at 3,2" for itself would be a second copy of the
/// geometry, and the copy is the one that goes stale.
///
/// TOTAL. An element naming a source this scene has no placement for gets an
/// EMPTY frame — which is consistent with what `resolve` did about the same
/// element, namely not place it, since resolving anything in an empty frame is
/// exactly as meaningless as the reference was.
inline Rect frame_in(const Scene& scene, const Element& e) noexcept {
    if (e.context == kRootContext) {
        return root_frame(scene.viewport);
    }
    const Placed* source = placed_for(scene, e.context);
    return source == nullptr ? Rect{} : source->rect;
}

} // namespace zengine::ui

#endif // ZENGINE_UI_LAYOUT_HPP
