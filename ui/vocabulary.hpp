// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_UI_VOCABULARY_HPP
#define ZENGINE_UI_VOCABULARY_HPP

// The UI package's AUTHORED side — what a maker says, before anything has
// decided where it goes.
//
// This package exists because W-0 built a maker tool and discovered it was
// keeping its own private answer to a question every visual Zengine application
// asks: "the maker authored 60% wide -- how many cells is that, and which object
// is under this cell?" Workshop answered it in workshop/document.hpp, for its own
// material only. W-1 moved that answer here, where the next application can have
// it without inventing a second one.
//
// THE ONE DISTINCTION THIS PACKAGE OWNS, and the reason it is two headers:
//
//     vocabulary.hpp  (here)     what was AUTHORED.       No resolved number
//                                                         exists in this file.
//     layout.hpp                 what a viewport MAKES of
//                                it -- the resolved observation, and hit
//                                testing over that observation.
//
// Authored intent and resolved geometry are two different true facts about one
// object, and a tool that shows only the second has silently thrown the maker's
// work away. Keeping them in separate headers is the cheapest possible reminder
// of which one you are holding.
//
// WHAT THIS IS NOT. It is not a widget set, not a layout ENGINE, and not a
// drawing vocabulary:
//
//   - no widget kinds, no stacks, no relational arrangement. An Element says
//     where it is and how big it is; it does not say "beside" or "inside". The
//     Loom's loom::Widget is that other, higher thing (intent + RELATIONSHIP,
//     resolved by a renderer) and it stays where it is -- see W-1-RB for the
//     measurement that separated the two models.
//   - no parent/child. W-0 authored a flat document and W-1 moved exactly that.
//     Nesting is the named seam: it is where this model grows, and it grows on
//     the day an application authors a child, not before.
//   - no colour, no z, no style. Paint order is list order, said once.
//   - nothing about PAINTING. zengine::surface::SurfaceCanvas is the drawing
//     vocabulary; a resolved scene is what you paint FROM. This package must
//     never learn what a canvas is (layout.hpp includes nothing of surface's,
//     which is the enforcement).

#include <zen/weave/shape.hpp>

#include <cstdint>
#include <string>
#include <type_traits>
#include <utility>

namespace zengine::ui {

/// How an extent was authored — the reason an extent is a shape and not a
/// number. Two spellings of one intent.
inline constexpr std::int64_t kExtentCells = 0;   ///< an absolute count of cells
inline constexpr std::int64_t kExtentPercent = 1; ///< a share of the viewport, 0..100

/// A width or a height AS AUTHORED, carrying both halves of the intent.
///
/// `amount` means cells when `mode == kExtentCells` and percent when it is
/// kExtentPercent. Nothing here VALIDATES: what a legal extent is belongs to
/// whichever application accepts one, because that is the only place that can
/// also refuse (Workshop's doc::check_extent is one such policy). Resolution,
/// on the other hand, must be TOTAL — see layout.hpp.
///
/// It is one property, not two. A maker does not author a type and then author
/// a value; they author a width. The historical builder presented "Width Type"
/// and "Width Value" as separate inspector rows because that is how the two were
/// STORED, and W-0's refusal to repeat that is the reason this struct exists at
/// all rather than a bare `std::int64_t width` plus a mode field somewhere else.
struct Extent {
    std::int64_t mode = kExtentCells;
    std::int64_t amount = 0;

    friend bool operator==(const Extent&, const Extent&) = default;

    ZEN_SHAPE(Extent, 1, ZEN_FIELD(mode), ZEN_FIELD(amount));
};

/// One authored element of a UI: an identity, a display label, an authored
/// placement, and two authored extents.
///
/// `id` IS the identity; `label` is text for a human and nothing more. They are
/// separate fields on purpose, and the separation is proven rather than claimed:
/// Workshop's opening document is two elements that share a label, renaming does
/// not refuse a duplicate, and hit testing answers with an id. A name used as an
/// identifier is the mistake this vocabulary is shaped to make unrepresentable.
///
/// What `id` is NOT: durable. Whoever holds the elements mints it, and it means
/// nothing outside that holder's lifetime. This package deliberately does not
/// mint ids — an identity policy belongs to whatever owns the document, and
/// durable identity is a real question no consumer has yet asked.
///
/// PLACEMENT IS AUTHORED, EXTENT IS RESOLVABLE, and that asymmetry is the honest
/// shape of the model rather than an oversight. `x`/`y` are what the maker said,
/// in viewport cells, and resolution never changes them: there is no such thing
/// as "50% across" here because no consumer has authored one. `width`/`height`
/// carry intent a viewport must interpret, so they are Extents and the fence
/// below makes them impossible to spell as bare numbers.
struct Element {
    std::int64_t id = 0;
    std::string label;
    std::int64_t x = 0; ///< authored placement, in viewport cells, from the top-left
    std::int64_t y = 0;
    Extent width;
    Extent height;

    friend bool operator==(const Element&, const Element&) = default;

    ZEN_SHAPE(Element, 1, ZEN_FIELD(id), ZEN_FIELD(label), ZEN_FIELD(x), ZEN_FIELD(y),
              ZEN_FIELD(width), ZEN_FIELD(height));
};

// ---- The authored/resolved fence, at compile time --------------------------------------
//
// The Loom's loom::Widget carries the same bet and enforces it with a NAME-BASED
// member-detection fence (no member may be spelled x/y/w/h/width/height/...), because in
// that model there is no authored geometry at all: any width on a Widget would be a
// resolved one. This model is different -- a width IS authored here -- so a name-only
// fence would either forbid the field the vocabulary needs or permit the collapse it
// exists to prevent. So the fence here is two claims, and only the first is airtight:
//
//   1. TYPE-AWARE (airtight for what it names). A resolvable dimension is an Extent, never
//      a number. `int64_t width` does not compile as an authored width, because an Extent
//      is not constructible from an integer -- so "just write the resolved 28 into the
//      authored width" cannot be spelled by accident.
//   2. NAME-BASED (defense in depth, and NOT airtight -- the same honesty the Loom's fence
//      states about itself). No member spelled like a resolved rectangle exists. It
//      catches only the enumerated names: a resolved number smuggled in as `extent_now`
//      would pass, so this layer is paired with review, not sold as unrepresentability.
//
// Both are exposed as traits over an arbitrary T rather than being buried in a
// static_assert about Element, so an application's OWN authored type can be held to the
// same claim -- and so the fence can be shown to FIRE, which a static_assert nobody ever
// violates cannot show. tests/ui_fence.cpp is that demonstration.

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

/// Claim 1: `T` has a width and a height, and BOTH are authored Extents rather than
/// resolved numbers. False for a type that has no such members at all — an authored
/// element without extents is not this vocabulary's element.
template <class T>
inline constexpr bool extents_are_authored_v = detail::extents_authored<T>::value;

/// Claim 2: `T` carries no member spelled like a resolved rectangle.
template <class T>
inline constexpr bool carries_no_resolved_geometry_v =
    !detail::has_w<T>::value && !detail::has_h<T>::value && !detail::has_right<T>::value &&
    !detail::has_bottom<T>::value && !detail::has_rect<T>::value &&
    !detail::has_resolved<T>::value && !detail::has_cells<T>::value &&
    !detail::has_pixels<T>::value;

/// The fence, as one question an application can ask about its own authored type:
/// "is every dimension on this thing something a maker SAID, rather than something a
/// viewport WORKED OUT?"
template <class T>
inline constexpr bool authored_only_v =
    extents_are_authored_v<T> && carries_no_resolved_geometry_v<T>;

static_assert(authored_only_v<Element>,
              "An authored UI element must carry authored intent only: extents are Extents "
              "(never resolved numbers), and no resolved rectangle may live on it. Resolution "
              "needs a viewport and produces a separate value — see ui/layout.hpp.");

} // namespace zengine::ui

#endif // ZENGINE_UI_VOCABULARY_HPP
