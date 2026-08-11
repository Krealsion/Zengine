// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_UI_VOCABULARY_HPP
#define ZENGINE_UI_VOCABULARY_HPP

// The UI package's AUTHORED side — what a maker says, before anything has
// decided where it goes.
//
// This package exists because every visual Zengine application asks one
// question: "the maker authored 60% wide -- how many cells is that, and which
// object is under this cell?" An application that answers it privately, for its
// own material only, leaves the next one to invent a second answer. The answer
// is here (README.md#ui--the-authoredresolved-vocabulary).
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
//     resolved by a renderer) and it stays where it is: the two are not
//     competitors and neither replaces the other.
//   - no parent/child, and the line means what it says rather than being a
//     deferral. An element may say what its authored values are measured AGAINST
//     (`context`, below); it still does not say that anything CONTAINS it, owns
//     it, clips it, paints it or dies with it. A
//     source supplies a frame. That is the whole relationship, and parent/child
//     is one thing an application could BUILD out of it rather than the thing
//     this vocabulary provides.
//   - no colour, no z, no style. Paint order is list order, said once.
//   - nothing about PAINTING. zengine::surface::SurfaceCanvas is the drawing
//     vocabulary; a resolved scene is what you paint FROM. This package must
//     never learn what a canvas is (layout.hpp includes nothing of surface's,
//     which is the enforcement).

#include <zen/weave/shape.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

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
/// STORED. Refusing to repeat that is the reason this struct exists at all
/// rather than a bare `std::int64_t width` plus a mode field somewhere else.
struct Extent {
    std::int64_t mode = kExtentCells;
    std::int64_t amount = 0;

    friend bool operator==(const Extent&, const Extent&) = default;

    ZEN_SHAPE(Extent, 1, ZEN_FIELD(mode), ZEN_FIELD(amount));
};

/// The identity that is not an identity: "measure me against the root".
///
/// Zero, and it is zero for a reason that already existed. No element may carry
/// identity 0 -- a document's mint starts at 1 and a session spells "nothing is
/// selected" as 0 -- so the value is unavailable to mean anything else, and a
/// default-constructed Element already says the ordinary thing. That is what
/// keeps the flat case free: an element resolves against the root because its
/// author said nothing, not because they filled in a node.
inline constexpr std::int64_t kRootContext = 0;

/// One authored element of a UI: an identity, a display label, what its values
/// are measured against, an authored placement, and two authored extents.
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
/// in cells, and resolution never reinterprets them: there is no such thing as
/// "50% across" here because no consumer has authored one. `width`/`height`
/// carry intent a context must interpret, so they are Extents and the fence
/// below makes them impossible to spell as bare numbers.
///
/// AND EVERY ONE OF THOSE FOUR NUMBERS IS MEASURED AGAINST SOMETHING, which is
/// what `context` says out loud. Left unsaid, the something is always the
/// viewport, implicitly, in one hard-coded line of `resolve` -- an origin of 0,0
/// and a span of the whole workspace. Here it is a value the maker authors:
///
///     context == kRootContext   x/y are offsets from the root's origin and an
///                               extent's share is a share of the root's span.
///                               The unchanged, default, ceremony-free case.
///     context == some id        x/y are offsets from THAT element's resolved
///                               origin and a share is a share of ITS resolved
///                               span. See ui::resolve_in, which is that
///                               sentence as four lines of arithmetic.
///
/// IT IS AN IDENTITY AND NEVER A POSITION. Not an index into the sequence, not a
/// pointer, not a place in a Scene: those are facts about storage, and storage
/// changes under every insertion, every reallocation, every save and every load,
/// while an identity is the one thing a selection, a list marker, a hit test and
/// a file already agree about. `#4` means the element carrying identity 4, not
/// the fourth element.
///
/// WHAT IT DOES NOT SAY, listed because a reader arriving from any other UI
/// toolkit will assume at least three of them: it does not say the source OWNS
/// this element, contains it, clips it, paints it, must outlive it, or sits
/// behind or in front of it. It says where this element's numbers are measured
/// from. Every other relationship an application might want -- containment,
/// ownership, z-order, a delete policy -- is that application's to author on
/// top, and Workshop authors exactly one of them (a source may not be deleted
/// while something still measures against it) as a POLICY of its own document,
/// not as a property of this field.
struct Element {
    std::int64_t id = 0;
    std::string label;
    std::int64_t context = kRootContext; ///< whose frame this element's values are read in
    std::int64_t x = 0; ///< authored placement, in cells, from that frame's top-left
    std::int64_t y = 0;
    Extent width;
    Extent height;

    friend bool operator==(const Element&, const Element&) = default;

    /// Version 2, because a published shape is immutable and this one grew a
    /// field. The Loom would catch the disagreement anyway -- a content-id is
    /// derived from the shape, so two builds spelling `Element v1` differently
    /// simply fail to agree rather than mis-decoding -- but a version that says
    /// "the same shape" about a different shape is a lie the mechanism does not
    /// need told. (`input::KeyPressed` is at v2 for the same reason.)
    ZEN_SHAPE(Element, 2, ZEN_FIELD(id), ZEN_FIELD(label), ZEN_FIELD(context), ZEN_FIELD(x),
              ZEN_FIELD(y), ZEN_FIELD(width), ZEN_FIELD(height));
};

// ---- Finding, and following, an authored relationship ----------------------------------
//
// Everything below answers questions about AUTHORED relationships only. Not one
// of these functions takes a viewport, produces a number, or knows what a
// rectangle is -- which is why they live on this side of the split. "Does this
// chain reach the root?" is a fact about what a maker wrote; "how many cells is
// it?" is a fact about a viewport, and that one is layout.hpp's.

/// A by-identity index over an authored sequence.
///
/// It exists because a relationship names an IDENTITY, and every use of one has
/// to find the element carrying it: resolution walks a context chain, and a
/// document law checks every chain. A linear search per step makes both
/// quadratic over a sequence whose length a FILE gets to choose -- the same
/// reasoning that made Workshop's distinctness check sort a copy instead of
/// nesting two loops. One build is O(n log n) and one lookup is O(log n).
///
/// DUPLICATE IDENTITIES ARE REPRESENTABLE HERE, and the index says what it does
/// about them rather than assuming they are gone. Distinctness is a DOCUMENT
/// law, and authored content arrives from a poke as well as from a checked edit,
/// so a sequence carrying one identity twice is a thing this vocabulary must
/// still answer about. It answers with the FIRST position carrying the identity
/// -- the same answer a linear search would have given, and the same one every
/// other lookup over these elements gives.
class ById {
public:
    static constexpr std::size_t npos = static_cast<std::size_t>(-1);

    explicit ById(const std::vector<Element>& elements) {
        at_.reserve(elements.size());
        for (std::size_t i = 0; i < elements.size(); ++i) {
            at_.push_back(Entry{elements[i].id, i});
        }
        // STABLE, so equal identities keep their document order and `find`
        // returning the range's first entry returns the first ELEMENT.
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

/// Where a context chain ends. Three outcomes and not two, because "it names
/// nothing" and "it names something that comes back here" are different mistakes
/// with different repairs.
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

    /// Whether this chain passes through `id` — the question an authoring
    /// operation asks before it hands `id` a new context, and the whole of the
    /// cycle test: a relationship closes a loop exactly when the proposed
    /// source's own chain already runs through the element being changed.
    bool passes_through(std::int64_t id) const noexcept {
        return std::find(chain.begin(), chain.end(), id) != chain.end();
    }
};

/// Follow a context chain from one identity to the root, or to the reason it
/// does not get there.
///
/// THE ONE WALK. A document law asks it about every element it holds; an
/// authoring operation asks it about the one relationship being proposed. There
/// is no second implementation of "what does this chain do", so the rule an edit
/// is judged by and the rule a loaded file is judged by cannot come to disagree
/// -- which is the same argument `check_extent` already carries one layer up.
///
/// IT IS ITERATIVE, AND THE DEPTH LIMIT IS THE DOCUMENT'S OWN SIZE. Nothing here
/// recurses, so how deep a legal composition may be is not secretly decided by
/// how much C++ stack the host happens to have. There is no authored ceiling at
/// all: a chain of a thousand elements is as legal as a chain of one, and the
/// only bound is that a walk visiting more elements than the sequence contains
/// must have visited one twice, which is what a cycle IS. That bound is exact,
/// costs one comparison per step, and is why a cycle is DETECTED rather than
/// discovered by running out of stack.
///
/// THE CYCLE IT REPORTS IS ONE LAP. Having proven a repeat, it walks forward
/// from where it stands until it returns there, so the diagnostic names the loop
/// (`#7 -> #9 -> #7`) instead of the long road that led into it. That is
/// deliberate: "invalid graph" tells a maker nothing they can act on.
///
/// `settled`, when given, is a memo indexed by POSITION: positions already known
/// to reach the root. It turns a whole-document check from one walk per element
/// into one visit per element, and it is why checking a document is O(n log n)
/// rather than O(n * depth). It also SHORTENS the reported chain -- the walk
/// stops at the first settled element -- so an operation that needs the complete
/// chain (`passes_through`) must not pass one.
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
            // The budget is spent, so a node has certainly been visited twice --
            // and a walk whose every step is determined by the element it is on
            // is periodic from its first repeat, so `here` is INSIDE the loop.
            // Take exactly one lap from it.
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
