// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_UI_VOCABULARY_HPP
#define ZENGINE_UI_VOCABULARY_HPP

// The UI package's authored side: an element's identity, the frame it is measured in, and its
// place and size as a weaver said them. No resolved number exists here; ui/layout.hpp resolves.
// Not a widget set, a layout engine or a drawing vocabulary. Reference: ui/docs/ui.md.

#include <zen/weave/shape.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace zengine::ui {

/// How an extent's `amount` is read. `resolve_extent` reads any other mode as cells.
inline constexpr std::int64_t kExtentCells = 0;   ///< an absolute count of cells
inline constexpr std::int64_t kExtentPercent = 1; ///< a share of the frame's span, 0..100

/// A width or a height as authored: the mode and the amount together, one property.
///
/// Nothing here validates. What a legal extent is belongs to the application that accepts one,
/// the only place that can also refuse; resolution is total over every value either field holds
/// (`resolve_extent`, in ui/layout.hpp). An integer does not convert to an Extent, which is half
/// of the fence at the end of this header.
struct Extent {
    std::int64_t mode = kExtentCells;
    std::int64_t amount = 0;

    friend bool operator==(const Extent&, const Extent&) = default;

    ZEN_SHAPE(Extent, 1, ZEN_FIELD(mode), ZEN_FIELD(amount));
};

/// The context that names no element: measure against the root frame, the whole viewport.
///
/// It is 0, so a default-constructed Element measures against the root. An element may carry
/// identity 0 and still be placed, but nothing can measure against it.
inline constexpr std::int64_t kRootContext = 0;

/// One authored element: an identity, a label, the context its values are read in, an authored
/// placement and two authored extents.
///
/// `id` is the identity: a context names it, and `hit` and `placed_for` answer with it. `label`
/// is text for a person and is never looked up. This package mints no ids and does not require
/// them to be distinct: whoever holds the elements owns both policies, and an id means nothing
/// outside that holder. A repeated id is still answered, by its first position (`ById`).
///
/// `x`/`y` are cells from the frame's top-left, never reinterpreted, and have no share form;
/// `width`/`height` are Extents, which the frame's span resolves. `context` names, by identity
/// and never by position, the element whose resolved rectangle is the frame, or is kRootContext.
/// It is not containment, ownership, clipping, paint order or lifetime: an application that
/// wants any of those builds it on top (ui/docs/ui.md#context-is-a-frame-not-a-parent).
///
/// The static assertion at the end of this header fires if Element carries a resolved number;
/// the UI suite's contract case pins its wire shape, `Element` version 2.
struct Element {
    std::int64_t id = 0;
    std::string label;
    std::int64_t context = kRootContext; ///< whose frame this element's values are read in
    std::int64_t x = 0; ///< authored placement, in cells, from that frame's top-left
    std::int64_t y = 0;
    Extent width;
    Extent height;

    friend bool operator==(const Element&, const Element&) = default;

    ZEN_SHAPE(Element, 2, ZEN_FIELD(id), ZEN_FIELD(label), ZEN_FIELD(context), ZEN_FIELD(x),
              ZEN_FIELD(y), ZEN_FIELD(width), ZEN_FIELD(height));
};

// ---- Finding, and following, an authored relationship ----------------------------------
// No viewport and no geometry: whether a chain reaches the root is a fact about what was authored.

/// A by-identity index over an authored sequence: O(n log n) to build, O(log n) to look up.
///
/// It records positions, so it describes the sequence it was built from and is stale once that
/// sequence changes. A repeated identity answers with its first position, as a linear search
/// would.
class ById {
public:
    static constexpr std::size_t npos = static_cast<std::size_t>(-1);

    explicit ById(const std::vector<Element>& elements) {
        at_.reserve(elements.size());
        for (std::size_t i = 0; i < elements.size(); ++i) {
            at_.push_back(Entry{elements[i].id, i});
        }
        // Stable, so equal identities keep their document order and `find`, returning the
        // range's first entry, returns the first element.
        std::stable_sort(at_.begin(), at_.end(),
                         [](const Entry& a, const Entry& b) { return a.id < b.id; });
    }

    /// Where the first element carrying `id` is, or npos.
    std::size_t find(std::int64_t id) const noexcept {
        const auto it = std::lower_bound(at_.begin(), at_.end(), id,
                                         [](const Entry& e, std::int64_t v) { return e.id < v; });
        return (it == at_.end() || it->id != id) ? npos : it->at;
    }

    std::size_t size() const noexcept { return at_.size(); }

private:
    struct Entry {
        std::int64_t id;
        std::size_t at;
    };
    std::vector<Entry> at_;
};

/// Where a context chain ends. A name nothing carries and a loop are different mistakes with
/// different repairs, so they are two outcomes.
enum class ContextEnd {
    Root,    ///< it reaches the root: the relationship is resolvable
    Missing, ///< it names an identity no element in this sequence carries
    Cycle,   ///< it comes back to somewhere it has already been
};

/// One walk up a context chain, and what it found.
struct ContextWalk {
    ContextEnd end = ContextEnd::Root;
    std::int64_t at = kRootContext;  ///< the missing identity, or one inside the cycle
    std::vector<std::int64_t> chain; ///< the identities visited, from `start` outward

    bool reaches_root() const noexcept { return end == ContextEnd::Root; }

    /// Whether this chain runs through `id`. Walked from a proposed source, it answers whether
    /// giving element `id` that source as its context would close a loop.
    bool passes_through(std::int64_t id) const noexcept {
        return std::find(chain.begin(), chain.end(), id) != chain.end();
    }
};

/// Follow a context chain from `start` toward the root, and say where it ends.
///
/// Iterative, with no depth ceiling: a walk that visits more elements than the sequence holds
/// has visited one twice, so a cycle is detected exactly, never by running out of stack. A cycle's
/// `chain` is one lap, closed (`#7 -> #9 -> #7`), not the road into it; a missing identity's is
/// what was visited before it. Starting at kRootContext reaches the root with an empty chain.
///
/// `settled`, when given, is a memo indexed by position: elements already known to reach the
/// root. The walk stops at the first settled element and marks what it proved, so a sequence whose
/// chains all reach the root costs one visit per element to check. It shortens `chain`, so a walk
/// for `passes_through` passes none.
inline ContextWalk walk_context(const std::vector<Element>& elements, const ById& index,
                                std::int64_t start, std::vector<char>* settled = nullptr) {
    ContextWalk walk;
    std::vector<std::size_t> path;
    std::int64_t here = start;
    while (here != kRootContext) {
        const std::size_t at = index.find(here);
        if (at == ById::npos) {
            walk.end = ContextEnd::Missing;
            walk.at = here;
            return walk;
        }
        if (settled != nullptr && at < settled->size() && (*settled)[at] != 0) {
            break; // already proven to reach the root; so does everything behind us
        }
        if (walk.chain.size() > elements.size()) {
            // The budget is spent, so a node has certainly been visited twice -- and a walk
            // whose every step is determined by the element it is on is periodic from its first
            // repeat, so `here` is inside the loop. Take exactly one lap from it.
            walk.end = ContextEnd::Cycle;
            walk.at = here;
            walk.chain.clear();
            std::int64_t lap = here;
            do {
                walk.chain.push_back(lap);
                const std::size_t on = index.find(lap);
                if (on == ById::npos) {
                    break; // unreachable inside a cycle; never trusted anyway
                }
                lap = elements[on].context;
            } while (lap != here && walk.chain.size() <= elements.size());
            walk.chain.push_back(here); // close it, so the text reads as a loop
            return walk;
        }
        walk.chain.push_back(here);
        path.push_back(at);
        here = elements[at].context;
    }
    walk.end = ContextEnd::Root;
    if (settled != nullptr) {
        for (const std::size_t at : path) {
            if (at < settled->size()) {
                (*settled)[at] = 1;
            }
        }
    }
    return walk;
}

// ---- The authored/resolved fence, at compile time --------------------------------------
// Two traits over any type, so an application can hold its own authored type to the rule the
// assertion below holds Element to (ui/docs/ui.md#the-fence). The compile entries
// `ui_authored_extent_required` and `ui_resolved_geometry_refused` show each half refusing, beside
// the control `ui_authored_element_compiles` (tests/compile_negative/ui_fence.cpp).

namespace detail {

#define ZENGINE_UI_HAS_MEMBER(NAME)                                                                \
    template <class T, class = void> struct has_##NAME : std::false_type {};                       \
    template <class T>                                                                             \
    struct has_##NAME<T, std::void_t<decltype(std::declval<T&>().NAME)>> : std::true_type {};

ZENGINE_UI_HAS_MEMBER(w)
ZENGINE_UI_HAS_MEMBER(h)
ZENGINE_UI_HAS_MEMBER(right)
ZENGINE_UI_HAS_MEMBER(bottom)
ZENGINE_UI_HAS_MEMBER(rect)
ZENGINE_UI_HAS_MEMBER(resolved)
ZENGINE_UI_HAS_MEMBER(cells)
ZENGINE_UI_HAS_MEMBER(pixels)
#undef ZENGINE_UI_HAS_MEMBER

template <class T, class = void> struct extents_authored : std::false_type {};
template <class T>
struct extents_authored<T, std::void_t<decltype(std::declval<T&>().width),
                                       decltype(std::declval<T&>().height)>>
    : std::bool_constant<
          std::is_same_v<std::remove_cvref_t<decltype(std::declval<T&>().width)>, Extent> &&
          std::is_same_v<std::remove_cvref_t<decltype(std::declval<T&>().height)>, Extent>> {};

} // namespace detail

/// The type-aware half, airtight for what it names: `T` has a `width` and a `height` and both
/// are Extents, so a resolved number cannot be stored as an authored width. False for a type with
/// no such members: an element without extents is not this vocabulary's element.
template <class T>
inline constexpr bool extents_are_authored_v = detail::extents_authored<T>::value;

/// The name-based half, and not airtight: `T` has no member named `w`, `h`, `right`, `bottom`,
/// `rect`, `resolved`, `cells` or `pixels`. A resolved number under any other name passes it.
template <class T>
inline constexpr bool carries_no_resolved_geometry_v =
    !detail::has_w<T>::value && !detail::has_h<T>::value && !detail::has_right<T>::value &&
    !detail::has_bottom<T>::value && !detail::has_rect<T>::value &&
    !detail::has_resolved<T>::value && !detail::has_cells<T>::value &&
    !detail::has_pixels<T>::value;

/// The fence as one question about a type: is every dimension on it something a weaver said,
/// rather than something a viewport worked out? Both halves.
template <class T>
inline constexpr bool authored_only_v =
    extents_are_authored_v<T> && carries_no_resolved_geometry_v<T>;

static_assert(authored_only_v<Element>,
              "An authored UI element must carry authored intent only: extents are Extents "
              "(never resolved numbers), and no resolved rectangle may live on it. Resolution "
              "needs a viewport and produces a separate value — see ui/layout.hpp.");

} // namespace zengine::ui

#endif // ZENGINE_UI_VOCABULARY_HPP
