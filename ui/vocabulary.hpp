// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_UI_VOCABULARY_HPP
#define ZENGINE_UI_VOCABULARY_HPP

// The UI package's authored side: a part's identity, the frame it stands in, and its place and
// sizes as a weaver said them, in words and whole pixels. No solved number exists here;
// ui/layout.hpp solves. What is wrong with the intent alone, with no room, is asked here
// (`problem`). Not a widget set or a drawing vocabulary. Reference: ui/docs/ui.md.

#include <zen/weave/shape.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace zengine::ui {

// ---- The words ------------------------------------------------------------------------
// A size and a place are each one word and the numbers that word reads. The words are numbers
// here and spelled by one table (`size_word`, `place_word`), so every owner spells them alike.

/// What a width or a height is: its word.
inline constexpr std::int64_t kSizeDefault = 0; ///< the owner's answer: the part's measure, else the frame's span
inline constexpr std::int64_t kSizePixels = 1;  ///< exactly `amount` pixels
inline constexpr std::int64_t kSizeShare = 2;   ///< `amount` ten-thousandths of the frame's span
inline constexpr std::int64_t kSizeFit = 3;     ///< the part's own measure, else the reach of its parts
inline constexpr std::int64_t kSizeFill = 4;    ///< from the part's anchored edge to the frame's far one

/// The most a share is: the whole frame, in ten-thousandths.
inline constexpr std::int64_t kShareWhole = 10000;

/// Where a part stands in its frame: the point of the frame its offsets count from.
inline constexpr std::int64_t kPlaceDefault = 0;     ///< the frame's top-left, no offset
inline constexpr std::int64_t kPlacePixels = 1;      ///< `x`, `y` from the frame's top-left
inline constexpr std::int64_t kPlaceTop = 2;         ///< centred across; `y` down from the top
inline constexpr std::int64_t kPlaceTopRight = 3;    ///< `x` in from the right, `y` down from the top
inline constexpr std::int64_t kPlaceLeft = 4;        ///< `x` in from the left; centred down
inline constexpr std::int64_t kPlaceCenter = 5;      ///< centred both ways
inline constexpr std::int64_t kPlaceRight = 6;       ///< `x` in from the right; centred down
inline constexpr std::int64_t kPlaceBottomLeft = 7;  ///< `x` in from the left, `y` up from the bottom
inline constexpr std::int64_t kPlaceBottom = 8;      ///< centred across; `y` up from the bottom
inline constexpr std::int64_t kPlaceBottomRight = 9; ///< `x` in from the right, `y` up from the bottom

/// How a part that is a frame lays its own parts.
inline constexpr std::int64_t kFlowFree = 0;  ///< each by its own place and sizes; parts may cover
inline constexpr std::int64_t kFlowStack = 1; ///< down a column, a slot each, `gap` apart, starting
                                              ///< again at the top where a slot would pass the floor

inline constexpr std::array<std::string_view, 5> kSizeWords{"default", "pixels", "share", "fit",
                                                            "fill"};
inline constexpr std::array<std::string_view, 10> kPlaceWords{
    "default", "pixels", "top",         "top-right", "left", "center",
    "right",   "bottom-left", "bottom", "bottom-right"};

/// A size's word, or empty for a number no word is.
inline constexpr std::string_view size_word(std::int64_t mode) noexcept {
    return mode >= 0 && mode < static_cast<std::int64_t>(kSizeWords.size())
               ? kSizeWords[static_cast<std::size_t>(mode)]
               : std::string_view{};
}

/// A place's word, or empty for a number no word is.
inline constexpr std::string_view place_word(std::int64_t mode) noexcept {
    return mode >= 0 && mode < static_cast<std::int64_t>(kPlaceWords.size())
               ? kPlaceWords[static_cast<std::size_t>(mode)]
               : std::string_view{};
}

/// Whether a size's word reads `amount`: `pixels` and `share` do, and no other.
inline constexpr bool size_reads_amount(std::int64_t mode) noexcept {
    return mode == kSizePixels || mode == kSizeShare;
}

/// Where a place stands along one axis: from the frame's start, from its end, or centred.
enum class Anchor { Start, End, Center };

/// A place's anchor across (left to right). A number no word is reads as `default`.
inline constexpr Anchor place_anchor_x(std::int64_t mode) noexcept {
    switch (mode) {
    case kPlaceTopRight:
    case kPlaceRight:
    case kPlaceBottomRight:
        return Anchor::End;
    case kPlaceTop:
    case kPlaceCenter:
    case kPlaceBottom:
        return Anchor::Center;
    default:
        return Anchor::Start;
    }
}

/// A place's anchor down (top to bottom). A number no word is reads as `default`.
inline constexpr Anchor place_anchor_y(std::int64_t mode) noexcept {
    switch (mode) {
    case kPlaceBottomLeft:
    case kPlaceBottom:
    case kPlaceBottomRight:
        return Anchor::End;
    case kPlaceLeft:
    case kPlaceCenter:
    case kPlaceRight:
        return Anchor::Center;
    default:
        return Anchor::Start;
    }
}

/// Whether a place's word reads `x`: every word with an edge across, and not a centred axis or
/// `default`.
inline constexpr bool place_reads_x(std::int64_t mode) noexcept {
    return mode != kPlaceDefault && !place_word(mode).empty() &&
           place_anchor_x(mode) != Anchor::Center;
}

/// Whether a place's word reads `y`: every word with an edge down, and not a centred axis or
/// `default`.
inline constexpr bool place_reads_y(std::int64_t mode) noexcept {
    return mode != kPlaceDefault && !place_word(mode).empty() &&
           place_anchor_y(mode) != Anchor::Center;
}

// ---- The authored part -------------------------------------------------------------------

/// A width or a height as authored: its word and, where the word reads one, its number.
///
/// Nothing here validates. What a legal size is belongs to the application that accepts one, the
/// only place that can also refuse; solving is total over every value either field holds
/// (`arrange`, in ui/layout.hpp). An integer does not convert to a Size, which is half of the
/// fence at the end of this header.
struct Size {
    std::int64_t mode = kSizeDefault;
    std::optional<std::int64_t> amount; ///< `pixels`' count, `share`'s ten-thousandths

    friend bool operator==(const Size&, const Size&) = default;

    ZEN_SHAPE(Size, 1, ZEN_FIELD(mode), ZEN_FIELD(amount));
};

/// Where a part stands in its frame: its word and the offsets that word reads, in whole pixels,
/// each inward from the edge the word names.
struct Place {
    std::int64_t mode = kPlaceDefault;
    std::optional<std::int64_t> x;
    std::optional<std::int64_t> y;

    friend bool operator==(const Place&, const Place&) = default;

    ZEN_SHAPE(Place, 1, ZEN_FIELD(mode), ZEN_FIELD(x), ZEN_FIELD(y));
};

/// The context that names no part: stand in the root frame, the whole room.
///
/// It is 0, so a default-constructed Element stands in the root. A part may carry identity 0 and
/// still be placed, but nothing can stand in it.
inline constexpr std::int64_t kRootContext = 0;

/// One authored part: an identity, a label, the frame it stands in, a place, two sizes, and --
/// for a part other parts stand in -- how it lays them.
///
/// `id` is the identity: a context names it, and `hit` and `placed_for` answer with it. `label`
/// is text for a person and is never looked up. This package mints no ids and does not require
/// them to be distinct: whoever holds the parts owns both policies, and an id means nothing
/// outside that holder. A repeated id is still answered, by its first position (`ById`).
///
/// `context` names, by identity and never by position, the part whose rectangle is the frame, or
/// is kRootContext. It is not containment, ownership, clipping, paint order or lifetime: an
/// application that wants any of those builds it on top (ui/docs/ui.md#context-is-a-frame-not-a-parent).
/// `flow` and `gap` say how this part lays the parts standing in it; `gap` is read by
/// kFlowStack alone.
///
/// The static assertion at the end of this header fires if Element carries a solved number; the
/// UI suite's contract case pins its wire shape, `Element` version 3.
struct Element {
    std::int64_t id = 0;
    std::string label;
    std::int64_t context = kRootContext; ///< whose frame this part stands in
    Place place;
    Size width;
    Size height;
    std::int64_t flow = kFlowFree;
    std::optional<std::int64_t> gap; ///< pixels between two slots of a kFlowStack column

    friend bool operator==(const Element&, const Element&) = default;

    ZEN_SHAPE(Element, 3, ZEN_FIELD(id), ZEN_FIELD(label), ZEN_FIELD(context), ZEN_FIELD(place),
              ZEN_FIELD(width), ZEN_FIELD(height), ZEN_FIELD(flow), ZEN_FIELD(gap));
};

// ---- Finding, and following, an authored relationship ----------------------------------
// No room and no geometry: whether a chain reaches the root is a fact about what was authored.

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

// ---- What is wrong with the intent alone -----------------------------------------------

namespace detail {

inline std::string part_name(const Element& e) {
    std::string out = "#" + std::to_string(e.id);
    if (!e.label.empty()) {
        out += " \"" + e.label + "\"";
    }
    return out;
}

/// What a part reads of its frame along one axis, in words, or empty when it reads nothing: its
/// size is a share or a fill of the frame's span, or its place counts from the frame's far edge
/// or centre, each of which needs the span first.
inline std::string reads_of_frame(const Size& size, Anchor anchor, bool across) {
    if (size.mode == kSizeShare) {
        return std::string("is a share of the ") + (across ? "width" : "height") + " of";
    }
    if (size.mode == kSizeFill) {
        return std::string("fills the ") + (across ? "width" : "height") + " of";
    }
    if (anchor == Anchor::End) {
        return std::string("stands at the ") + (across ? "right" : "bottom") + " of";
    }
    if (anchor == Anchor::Center) {
        return std::string("is centred ") + (across ? "across" : "down");
    }
    return {};
}

} // namespace detail

/// The intent's own first problem, with no room and no measure, in authored order; empty when
/// there is none. The application that accepts a sequence of parts calls it and refuses on an
/// answer; `arrange` is total and solves whatever it is given.
///
/// It names, for the first part that has one:
/// - a context no part carries, and a context that loops (`walk_context`'s two faults);
/// - a size that depends on itself: a share, a fill, or a place counted from the far edge or the
///   centre, along an axis where the frame fits its size to its parts (`kSizeFit`), so the frame
///   is measured from the part and the part from the frame;
/// - a column that begins again at its floor and fits its own height to its parts.
inline std::string problem(const std::vector<Element>& parts) {
    const ById index(parts);
    std::vector<char> settled(parts.size(), 0);
    // Whether any part stands in the part at each position (a repeated id frames by its first).
    std::vector<char> framing(parts.size(), 0);
    for (const Element& e : parts) {
        const std::size_t at = e.context == kRootContext ? ById::npos : index.find(e.context);
        if (at != ById::npos) {
            framing[at] = 1;
        }
    }
    for (std::size_t i = 0; i < parts.size(); ++i) {
        const Element& e = parts[i];
        const ContextWalk walk = walk_context(parts, index, e.context, &settled);
        if (walk.end == ContextEnd::Missing) {
            return detail::part_name(e) + " stands in #" + std::to_string(walk.at) +
                   ", which no part carries";
        }
        if (walk.end == ContextEnd::Cycle) {
            std::string lap;
            for (const std::int64_t id : walk.chain) {
                lap += (lap.empty() ? "#" : " -> #") + std::to_string(id);
            }
            return detail::part_name(e) + " stands in a loop of frames, " + lap +
                   ", which never reaches the room";
        }
        if (e.flow == kFlowStack && e.height.mode == kSizeFit && framing[i] != 0) {
            return detail::part_name(e) +
                   " begins its column again at its floor, and fits its height to its parts: "
                   "give it a height";
        }
        if (e.context == kRootContext) {
            continue;
        }
        const Element& frame = parts[index.find(e.context)];
        for (const bool across : {true, false}) {
            const Size& frame_size = across ? frame.width : frame.height;
            if (frame_size.mode != kSizeFit) {
                continue;
            }
            const std::string reads = detail::reads_of_frame(
                across ? e.width : e.height,
                across ? place_anchor_x(e.place.mode) : place_anchor_y(e.place.mode), across);
            if (!reads.empty()) {
                const char* axis = across ? "width" : "height";
                return detail::part_name(e) + " " + reads + " " + detail::part_name(frame) +
                       ", which fits its " + axis + " to its parts: give " +
                       detail::part_name(frame) + " a " + axis;
            }
        }
    }
    return {};
}

// ---- The authored/solved fence, at compile time ----------------------------------------
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
          std::is_same_v<std::remove_cvref_t<decltype(std::declval<T&>().width)>, Size> &&
          std::is_same_v<std::remove_cvref_t<decltype(std::declval<T&>().height)>, Size>> {};

} // namespace detail

/// The type-aware half, airtight for what it names: `T` has a `width` and a `height` and both
/// are Sizes, so a solved number cannot be stored as an authored width. False for a type with no
/// such members: a part without sizes is not this vocabulary's part.
template <class T>
inline constexpr bool extents_are_authored_v = detail::extents_authored<T>::value;

/// The name-based half, and not airtight: `T` has no member named `w`, `h`, `right`, `bottom`,
/// `rect`, `resolved`, `cells` or `pixels`. A solved number under any other name passes it.
template <class T>
inline constexpr bool carries_no_resolved_geometry_v =
    !detail::has_w<T>::value && !detail::has_h<T>::value && !detail::has_right<T>::value &&
    !detail::has_bottom<T>::value && !detail::has_rect<T>::value &&
    !detail::has_resolved<T>::value && !detail::has_cells<T>::value &&
    !detail::has_pixels<T>::value;

/// The fence as one question about a type: is every dimension on it something a weaver said,
/// rather than something a room worked out? Both halves.
template <class T>
inline constexpr bool authored_only_v =
    extents_are_authored_v<T> && carries_no_resolved_geometry_v<T>;

static_assert(authored_only_v<Element>,
              "An authored UI part must carry authored intent only: its width and height are "
              "Sizes (never solved numbers), and no solved rectangle may live on it. Solving "
              "needs a room and produces a separate value -- see ui/layout.hpp.");

} // namespace zengine::ui

#endif // ZENGINE_UI_VOCABULARY_HPP
