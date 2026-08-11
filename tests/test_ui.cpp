// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The UI suite — the authored/resolved distinction, as a thing that can be
// checked rather than believed.
//
// The package under test is two headers and one idea: what a maker AUTHORS
// (ui/vocabulary.hpp) is not what a viewport MAKES of it (ui/layout.hpp), and
// hit testing is a question only the second can answer while the answer it gives
// is always about the first. So the suite is organised as three claims:
//
//   1. THE AUTHORED SIDE IS CONTENT — the shapes derive their declared schemas,
//      and the compile-time fence discriminates an authored element from one
//      that has quietly grown resolved geometry. (That the fence FIRES is proven
//      by the compile-negative entries `ui_authored_extent_required` and
//      `ui_resolved_geometry_refused`; what is proven here is that the trait the
//      fence is built from says different things about different types, which no
//      compile-negative can show.)
//   2. RESOLUTION IS A FUNCTION OF A VIEWPORT — including for values that no
//      application's setter would ever have accepted, because authored content
//      is a ZEN_SHAPE and arrives from the wire and from a poke as well as from
//      a checked edit.
//   3. THE RESOLVED SIDE IS AN OBSERVATION — separate value, no wire form, and
//      hit testing over it answers with the authored identity.
//
// Everything is pure. There is nothing to mount, nothing to pump, and no Loom
// kernel involved: this package is vocabulary and arithmetic.

// main() and the framework live in doctest_main.cpp -- the shared one that
// refuses a run selecting zero cases (POP-01).
#include "doctest.h"

#include "ui/layout.hpp"
#include "ui/vocabulary.hpp"

#include <zen/schema.hpp>
#include <zen/weave.hpp>

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

using namespace zengine::ui;
using loom::schema_of;

namespace {

Element make(std::int64_t id, std::string label, std::int64_t x, std::int64_t y, Extent w,
             Extent h) {
    Element e;
    e.id = id;
    e.label = std::move(label);
    e.x = x;
    e.y = y;
    e.width = w;
    e.height = h;
    return e;
}

// ---- The fence's negative subjects ------------------------------------------------------
//
// Types that violate exactly one half of the fence each, so the traits can be shown to
// DISCRIMINATE rather than merely to be true of Element. They are never used as elements.

/// A resolved width, smuggled in as a bare number where an Extent belongs.
struct CollapsedExtent {
    std::int64_t id = 0;
    std::int64_t width = 0;
    std::int64_t height = 0;
};

/// Authored extents kept honestly, and a resolved rectangle cached beside them.
struct CachesResolved {
    std::int64_t id = 0;
    Extent width;
    Extent height;
    std::int64_t w = 0; ///< the cache -- the exact lie the fence exists to prevent
    std::int64_t h = 0;
};

/// Not this vocabulary's element at all: no extents to be authored.
struct NoExtents {
    std::int64_t id = 0;
    std::string label;
};

} // namespace

TEST_SUITE("ui") {

// ============================================================================
// 1 — the authored side is content
// ============================================================================

TEST_CASE("contract: the authored shapes derive their declared spellings exactly") {
    using loom::Kind;
    using loom::SchemaBuilder;

    const auto extent = SchemaBuilder("Extent", 1)
                            .field("mode", Kind::Int)
                            .field("amount", Kind::Int)
                            .build();
    CHECK(schema_of<Extent>()->content_id() == extent->content_id());

    // Version 2, because `context` joined it. A published shape is
    // immutable, and the Loom would have caught the disagreement anyway (a
    // content-id is derived from the shape, so two builds spelling `Element v1`
    // differently fail to agree rather than mis-decoding) -- the version is what
    // stops it being a lie in the meantime.
    const auto element = SchemaBuilder("Element", 2)
                             .field("id", Kind::Int)
                             .field("label", Kind::Text)
                             .field("context", Kind::Int)
                             .field("x", Kind::Int)
                             .field("y", Kind::Int)
                             .message("width", extent)
                             .message("height", extent)
                             .build();
    CHECK(schema_of<Element>()->content_id() == element->content_id());
}

TEST_CASE("the fence discriminates: it is a question about a type, not a decoration") {
    // The subject it was written for.
    CHECK(extents_are_authored_v<Element>);
    CHECK(carries_no_resolved_geometry_v<Element>);
    CHECK(authored_only_v<Element>);

    // Half one, type-aware: a width that is a NUMBER is a resolved width, whatever it is
    // called. This is the half a name-based fence cannot state -- `width` is exactly the
    // spelling an authored width wants.
    CHECK_FALSE(extents_are_authored_v<CollapsedExtent>);
    CHECK_FALSE(authored_only_v<CollapsedExtent>);

    // Half two, name-based: extents authored properly, and a resolved rectangle cached on
    // the element anyway. The type check alone would pass this one.
    CHECK(extents_are_authored_v<CachesResolved>);
    CHECK_FALSE(carries_no_resolved_geometry_v<CachesResolved>);
    CHECK_FALSE(authored_only_v<CachesResolved>);

    // A type with no extents at all is not an authored element of this vocabulary. It is
    // not "geometry-free and therefore fine" -- the trait answers about a shape it can
    // recognise, and says no rather than defaulting to yes.
    CHECK_FALSE(extents_are_authored_v<NoExtents>);
    CHECK_FALSE(authored_only_v<NoExtents>);
    CHECK(carries_no_resolved_geometry_v<NoExtents>); // the other half is still true of it
}

// ============================================================================
// 2 — resolution is a function of a viewport
// ============================================================================

TEST_CASE("an extent resolves against a span, and cells resolve to themselves") {
    CHECK(resolve_extent(Extent{kExtentPercent, 50}, 48) == 24);
    CHECK(resolve_extent(Extent{kExtentPercent, 50}, 24) == 12);
    CHECK(resolve_extent(Extent{kExtentPercent, 100}, 48) == 48);

    // A cells extent resolves to itself: the authored fact and the resolved fact
    // COINCIDE, which is not the same as being one fact -- the span is ignored,
    // and it is still an argument the caller had to have.
    CHECK(resolve_extent(Extent{kExtentCells, 4}, 48) == 4);
    CHECK(resolve_extent(Extent{kExtentCells, 4}, 12) == 4);
}

TEST_CASE("a share never rounds an element out of existence") {
    CHECK(resolve_extent(Extent{kExtentPercent, 1}, 4) == kMinCells);
    CHECK(resolve_extent(Extent{kExtentPercent, 1}, 0) == kMinCells);
    // An element a maker authored is one they meant to see: the floor is a
    // decision about their work, not an arithmetic convenience.
    CHECK(kMinCells == 1);
}

TEST_CASE("resolution is TOTAL for values no setter would have accepted") {
    // Authored content is a ZEN_SHAPE. It arrives from the wire and from a poke,
    // neither of which has been past anybody's check_extent -- so every one of
    // these is a value this function must survive rather than trust.
    constexpr std::int64_t kMax = (std::numeric_limits<std::int64_t>::max)();

    // An out-of-range share is clamped, not multiplied out.
    CHECK(resolve_extent(Extent{kExtentPercent, 1000}, 48) == 48);
    CHECK(resolve_extent(Extent{kExtentPercent, kMax}, 48) == 48);
    CHECK(resolve_extent(Extent{kExtentPercent, -5}, 48) == kMinCells);
    CHECK(resolve_extent(Extent{kExtentPercent, (std::numeric_limits<std::int64_t>::min)()}, 48) ==
          kMinCells);

    // An absurd span divides before it multiplies. `span * amount` on these
    // operands is signed overflow -- undefined behaviour produced by data.
    CHECK(resolve_extent(Extent{kExtentPercent, 100}, kMax) > 0);
    CHECK(resolve_extent(Extent{kExtentPercent, 50}, kMax) > 0);
    CHECK(resolve_extent(Extent{kExtentPercent, 50}, kMax) < kMax);

    // A viewport with no span has no share to give. The most negative span is
    // the one that matters and the one a value check cannot see: without the
    // early return, `span * pct` on it overflows -- and on this compiler it
    // wraps to exactly 0, which floors to kMinCells, so the RIGHT answer comes
    // out of undefined behaviour. Only a sanitizer can tell the two apart; the
    // report records the ad-hoc UBSan run that does.
    CHECK(resolve_extent(Extent{kExtentPercent, 50}, 0) == kMinCells);
    CHECK(resolve_extent(Extent{kExtentPercent, 50}, -10) == kMinCells);
    CHECK(resolve_extent(Extent{kExtentPercent, 100},
                         (std::numeric_limits<std::int64_t>::min)()) == kMinCells);

    // An unknown mode is not a percent, so it resolves as the number it carries.
    // (What a legal mode IS belongs to whoever accepts an extent; resolution
    // still has to answer.)
    CHECK(resolve_extent(Extent{99, 7}, 48) == 7);
}

TEST_CASE("resolve places every authored element, in authored order, against ONE viewport") {
    const std::vector<Element> authored{
        make(1, "wide", 3, 2, Extent{kExtentPercent, 50}, Extent{kExtentCells, 6}),
        make(2, "fixed", 6, 10, Extent{kExtentCells, 14}, Extent{kExtentCells, 4}),
    };

    const Scene scene = resolve(authored, Viewport{48, 16});
    REQUIRE(scene.items.size() == 2);

    // Authored order is paint order, said once.
    CHECK(scene.items[0].id == 1);
    CHECK(scene.items[1].id == 2);

    // Placement is authored and passes through untouched; extent is resolved.
    CHECK(scene.items[0].rect == Rect{3, 2, 24, 6});
    CHECK(scene.items[1].rect == Rect{6, 10, 14, 4});

    // A scene keeps the viewport it is an observation OF. A rectangle without it
    // is a number with its meaning cut off.
    CHECK(scene.viewport == Viewport{48, 16});

    // An empty document resolves to an empty scene, which is a legitimate
    // observation and not a missing one.
    CHECK(resolve({}, Viewport{48, 16}).items.empty());
}

TEST_CASE("authored and resolved are different facts, and only one of them moves") {
    const std::vector<Element> authored{
        make(1, "wide", 0, 0, Extent{kExtentPercent, 50}, Extent{kExtentCells, 4}),
    };

    const Scene wide = resolve(authored, Viewport{48, 16});
    const Scene narrow = resolve(authored, Viewport{24, 16});

    CHECK(wide.items[0].rect.w == 24);
    CHECK(narrow.items[0].rect.w == 12);
    CHECK(wide.items[0].rect.h == narrow.items[0].rect.h); // cells do not move

    // Resolving twice wrote nothing. The authored value is exactly what it was,
    // and there is nowhere on it a resolved number could have gone.
    CHECK(authored[0].width == Extent{kExtentPercent, 50});
    CHECK(authored[0].x == 0);
}

// ============================================================================
// 3 — the resolved side is an observation, and hit testing answers with identity
// ============================================================================

TEST_CASE("a resolved observation has no wire form") {
    // `loom::Shape` is the substrate's own question, and its absence on the
    // resolved side is what keeps an observation from being stored, sent or
    // poked as though it were authored content. Asserted at compile time in
    // layout.hpp; restated here so a suite run reports it as a checked claim
    // rather than an invisible one.
    CHECK(loom::Shape<Element>);
    CHECK(loom::Shape<Extent>);
    CHECK_FALSE(loom::Shape<Rect>);
    CHECK_FALSE(loom::Shape<Placed>);
    CHECK_FALSE(loom::Shape<Scene>);
    CHECK_FALSE(loom::Shape<Viewport>);
}

TEST_CASE("hit testing answers with the authored identity, topmost first") {
    const std::vector<Element> authored{
        make(7, "back", 0, 0, Extent{kExtentCells, 10}, Extent{kExtentCells, 6}),
        make(9, "front", 4, 2, Extent{kExtentCells, 4}, Extent{kExtentCells, 2}),
    };
    const Scene scene = resolve(authored, Viewport{48, 16});

    REQUIRE(hit(scene, 0, 0) != nullptr);
    CHECK(hit(scene, 0, 0)->id == 7);
    CHECK(hit(scene, 9, 5)->id == 7);

    // Overlap: the LAST authored wins, because the last authored is the last
    // painted, so the answer agrees with what a person can see.
    REQUIRE(hit(scene, 5, 3) != nullptr);
    CHECK(hit(scene, 5, 3)->id == 9);

    // What it hands back is the observation, so a caller gets the rectangle it
    // hit without re-deriving it -- and the identity, not a rectangle index, a
    // label, or a copy of a painted item.
    CHECK(hit(scene, 5, 3)->rect == Rect{4, 2, 4, 2});

    // One cell past an edge is nothing, on every edge.
    CHECK(hit(scene, 10, 0) == nullptr);
    CHECK(hit(scene, 0, 6) == nullptr);
    CHECK(hit(scene, -1, 0) == nullptr);
    CHECK(hit(scene, 0, -1) == nullptr);

    // An empty scene answers "nothing", which is an answer.
    CHECK(hit(Scene{}, 0, 0) == nullptr);
}

TEST_CASE("a share's hit area follows the viewport, because both read one scene") {
    const std::vector<Element> authored{
        make(1, "half", 0, 0, Extent{kExtentPercent, 50}, Extent{kExtentCells, 2}),
    };

    // 50% of 48 is 24 cells, so cell 20 is inside it.
    const Scene wide = resolve(authored, Viewport{48, 16});
    REQUIRE(hit(wide, 20, 0) != nullptr);
    CHECK(hit(wide, 20, 0)->id == 1);

    // 50% of 24 is 12, so the same cell is now outside the same authored
    // element. There is no second copy of the geometry able to disagree.
    const Scene narrow = resolve(authored, Viewport{24, 16});
    CHECK(hit(narrow, 20, 0) == nullptr);
    REQUIRE(hit(narrow, 5, 0) != nullptr);
    CHECK(hit(narrow, 5, 0)->id == 1);
}

TEST_CASE("where one identity landed, and null as a normal answer") {
    const std::vector<Element> authored{
        make(1, "a", 0, 0, Extent{kExtentCells, 3}, Extent{kExtentCells, 3}),
        make(2, "b", 5, 5, Extent{kExtentPercent, 25}, Extent{kExtentCells, 2}),
    };
    const Scene scene = resolve(authored, Viewport{48, 16});

    REQUIRE(placed_for(scene, 2) != nullptr);
    CHECK(placed_for(scene, 2)->rect == Rect{5, 5, 12, 2});

    // An id this scene has no element for is a normal answer, not an error: a
    // selection can outlive the element it selected.
    CHECK(placed_for(scene, 4242) == nullptr);
    CHECK(placed_for(scene, 0) == nullptr);
}

TEST_CASE("the rectangle test is TOTAL, over every rect a resolved scene can hold") {
    // The same widened-input-domain claim `resolve_extent` carries, one function
    // along. A cells extent resolves to ITSELF, authored content is a ZEN_SHAPE,
    // and a poke writes the amount past every application's check -- so a scene
    // resolved from poked content holds rects whose edges are not representable,
    // and `x + w` on one of those is undefined behaviour produced by data.
    //
    // Found by a sanitizer, through an ordinary press: `hit` is what a maker's
    // hand asks, so this is on the gesture path and not in a corner.
    // The plain lane cannot see it, which is why the assertions below are about
    // ANSWERS and the sanitizer lane is the other half of the evidence.
    constexpr std::int64_t kMax = (std::numeric_limits<std::int64_t>::max)();
    constexpr std::int64_t kMin = (std::numeric_limits<std::int64_t>::min)();

    const Rect wild{1, 1, kMax, kMax};
    CHECK(wild.contains(5, 5));       // it really is that big
    CHECK_FALSE(wild.contains(0, 5)); // ...and still bounded on the near side
    CHECK_FALSE(wild.contains(5, 0));
    // A rect starting at the most negative cell and as wide as the type allows
    // still ENDS somewhere: kMin + kMax is -1, so its last cell is -2 and 0 is
    // outside it. The unsigned difference gets that exactly right where the
    // signed addition could not have been evaluated at all.
    const Rect huge{kMin, kMin, kMax, kMax};
    CHECK(huge.contains(-2, -2));
    CHECK_FALSE(huge.contains(-1, -1));
    CHECK_FALSE(huge.contains(0, 0));
    CHECK_FALSE(Rect{kMin, kMin, 2, 2}.contains(0, 0));
    CHECK_FALSE(Rect{1, 1, kMin, kMin}.contains(5, 5));

    // An empty or inverted rectangle contains nothing -- unchanged behaviour,
    // now stated rather than falling out of arithmetic that could overflow.
    CHECK_FALSE(Rect{0, 0, 0, 4}.contains(0, 0));
    CHECK_FALSE(Rect{0, 0, 4, 0}.contains(0, 0));
    CHECK_FALSE(Rect{0, 0, -3, -3}.contains(0, 0));

    // ...and through the package's own door, which is how it is actually reached.
    std::vector<Element> authored{make(1, "poked", 1, 1, Extent{kExtentCells, kMax},
                                       Extent{kExtentCells, 4})};
    const Scene scene = resolve(authored, Viewport{48, 16});
    REQUIRE(hit(scene, 5, 2) != nullptr);
    CHECK(hit(scene, 5, 2)->id == 1);
    CHECK(hit(scene, 0, 2) == nullptr);
}

// ============================================================================
// 4 — composition: the context an authored shape is read in
// ============================================================================
//
// Everything above resolves against one root context, which is the easy case
// and was the only case. These are the claims that make it the easy case rather
// than the only one:
//
//   a FRAME is what resolution consumes -- an origin and a span -- and the root
//   supplies one just as an element does;
//   an element says which frame by IDENTITY, so the relationship survives
//   reordering, reallocation and a file;
//   the work is ordered by dependency and the ANSWERS come out in document
//   order, because document order is paint order and nothing may silently
//   change it;
//   there is no depth ceiling, and a chain that cannot reach the root produces
//   an absence rather than a guess.

TEST_CASE("a shape is resolved IN a frame, and the root supplies one like anything else") {
    // The primitive, alone. `authored shape + context = resolved shape`, with
    // the two halves of the context doing the two jobs they have always done --
    // the origin the offsets count from, the span the shares are shares of.
    const Element e = make(1, "a", 2, 1, Extent{kExtentPercent, 50}, Extent{kExtentCells, 3});

    CHECK(resolve_in(e, Rect{0, 0, 48, 16}) == Rect{2, 1, 24, 3});
    CHECK(resolve_in(e, Rect{10, 4, 20, 8}) == Rect{12, 5, 10, 3});

    // The root's frame is the whole viewport at the origin -- which is exactly
    // what `resolve` used to hard-code in one statement without naming it.
    CHECK(root_frame(Viewport{48, 16}) == Rect{0, 0, 48, 16});
    CHECK(resolve_in(e, root_frame(Viewport{48, 16})) == Rect{2, 1, 24, 3});

    // Cells ignore the span they are given, in every frame. That is the same
    // fact `resolve_extent` already carries, now visible under composition.
    const Element cells = make(2, "b", 0, 0, Extent{kExtentCells, 9}, Extent{kExtentCells, 9});
    CHECK(resolve_in(cells, Rect{0, 0, 48, 16}).w == 9);
    CHECK(resolve_in(cells, Rect{5, 5, 4, 4}).w == 9);
}

TEST_CASE("an element's values are read in the frame its source resolved to") {
    std::vector<Element> authored{
        make(1, "A", 4, 3, Extent{kExtentPercent, 60}, Extent{kExtentCells, 10}),
        make(2, "B", 2, 1, Extent{kExtentPercent, 50}, Extent{kExtentCells, 4}),
    };
    authored[1].context = 1;

    const Scene scene = resolve(authored, Viewport{40, 20});
    REQUIRE(scene.items.size() == 2);

    // A is read in the root: 60% of 40 is 24.
    CHECK(scene.items[0].rect == Rect{4, 3, 24, 10});
    // B is read in A: its 2,1 is an offset from A's ORIGIN, and its 50% is half
    // of A's RESOLVED 24 -- not half of the workspace.
    CHECK(scene.items[1].rect == Rect{6, 4, 12, 4});

    // Neither authored value moved. Resolving is an observation, and that is as
    // true one link down a chain as it was at the root.
    CHECK(authored[1].x == 2);
    CHECK(authored[1].width == Extent{kExtentPercent, 50});
    CHECK(authored[1].context == 1);

    // Change the root's span and BOTH resolutions follow, transitively, with
    // every authored number identical.
    const Scene narrow = resolve(authored, Viewport{20, 20});
    CHECK(narrow.items[0].rect.w == 12); // 60% of 20
    CHECK(narrow.items[1].rect.w == 6);  // 50% of that 12
    CHECK(authored[0].width == Extent{kExtentPercent, 60});
    CHECK(authored[1].width == Extent{kExtentPercent, 50});
}

TEST_CASE("a source may sit LATER in the document than what measures against it") {
    // The strongest ordering claim in the package. Document order means paint
    // order, hit order and list order; dependency order is a fact about how
    // resolution must sequence its own work. Making the first mean the second
    // -- by sorting the document -- would silently change which rectangle is in
    // front, so it does not happen.
    std::vector<Element> authored{
        make(3, "C", 1, 1, Extent{kExtentCells, 2}, Extent{kExtentCells, 2}),
        make(1, "A", 10, 5, Extent{kExtentCells, 20}, Extent{kExtentCells, 8}),
        make(2, "B", 1, 1, Extent{kExtentCells, 6}, Extent{kExtentCells, 3}),
    };
    authored[0].context = 2; // C -> B
    authored[2].context = 1; // B -> A

    const Scene scene = resolve(authored, Viewport{48, 16});
    REQUIRE(scene.items.size() == 3);

    // ANSWERS IN DOCUMENT ORDER, unchanged: C, A, B.
    CHECK(scene.items[0].id == 3);
    CHECK(scene.items[1].id == 1);
    CHECK(scene.items[2].id == 2);

    // ...and every rectangle correct, though the work had to run A, B, C.
    CHECK(scene.items[1].rect == Rect{10, 5, 20, 8}); // A, in the root
    CHECK(scene.items[2].rect == Rect{11, 6, 6, 3});  // B, in A
    CHECK(scene.items[0].rect == Rect{12, 7, 2, 2});  // C, in B

    // Paint order is what it always was, so the topmost at an overlapping cell
    // is the LAST in the document -- B -- even though B is what everything else
    // depends on. Dependency order is not z-order.
    REQUIRE(hit(scene, 12, 7) != nullptr);
    CHECK(hit(scene, 12, 7)->id == 2);
}

TEST_CASE("composition has no depth ceiling, and nothing here recurses to find out") {
    // The point is not the number. It is that no constant anywhere says how deep
    // a chain may be, and that the work happens on the heap -- so what a
    // document may express is not decided by how much C stack the host has.
    constexpr std::int64_t kDeep = 4096;
    std::vector<Element> authored;
    authored.reserve(static_cast<std::size_t>(kDeep));
    authored.push_back(make(1, "root-most", 1, 1, Extent{kExtentPercent, 100},
                            Extent{kExtentPercent, 100}));
    for (std::int64_t i = 2; i <= kDeep; ++i) {
        Element e = make(i, "link", 1, 0, Extent{kExtentCells, 3}, Extent{kExtentCells, 1});
        e.context = i - 1;
        authored.push_back(e);
    }

    const Scene scene = resolve(authored, Viewport{48, 16});
    CHECK(scene.items.size() == static_cast<std::size_t>(kDeep));

    // Each link is one cell right of the one it measures against, so the last
    // one has accumulated the whole chain -- which is the composed context
    // arriving intact at the far end.
    const Placed* last = placed_for(scene, kDeep);
    REQUIRE(last != nullptr);
    CHECK(last->rect.x == kDeep);
    CHECK(last->rect.y == 1);

    // ...and in document order, at the end, where it was authored.
    CHECK(scene.items.back().id == kDeep);

    // The same chain built BACKWARDS in the document resolves identically: a
    // relationship is an identity, not a position, so the order it was written
    // in is not part of its meaning.
    std::vector<Element> reversed(authored.rbegin(), authored.rend());
    const Scene other = resolve(reversed, Viewport{48, 16});
    CHECK(other.items.size() == static_cast<std::size_t>(kDeep));
    REQUIRE(placed_for(other, kDeep) != nullptr);
    CHECK(placed_for(other, kDeep)->rect == last->rect);
    CHECK(other.items.front().id == kDeep); // document order, which is now reversed
}

TEST_CASE("a chain that never reaches the root places nothing, and never guesses the root") {
    // Both faults are refused by an application's document law before they can
    // be authored (Workshop's check_document does), so these arrive the way
    // every other hostile value in this package arrives: through a poke, or from
    // an application with no such law. Resolution still has to answer, and the
    // answer is an ABSENCE -- because falling back to the root would resolve the
    // element against a DIFFERENT relationship than the one it names and then
    // draw a confident rectangle in the wrong place.
    SUBCASE("a source that does not exist") {
        std::vector<Element> authored{
            make(1, "A", 3, 3, Extent{kExtentCells, 4}, Extent{kExtentCells, 4}),
            make(2, "B", 0, 0, Extent{kExtentCells, 4}, Extent{kExtentCells, 4}),
        };
        authored[1].context = 999;

        const Scene scene = resolve(authored, Viewport{48, 16});
        REQUIRE(scene.items.size() == 1);
        CHECK(scene.items[0].id == 1);
        CHECK(placed_for(scene, 2) == nullptr);
        CHECK(hit(scene, 0, 0) == nullptr); // not sitting at the root's origin
    }
    SUBCASE("an object measured against itself") {
        std::vector<Element> authored{
            make(1, "self", 0, 0, Extent{kExtentCells, 4}, Extent{kExtentCells, 4}),
        };
        authored[0].context = 1;
        CHECK(resolve(authored, Viewport{48, 16}).items.empty());
    }
    SUBCASE("two objects measured against each other") {
        std::vector<Element> authored{
            make(1, "A", 0, 0, Extent{kExtentCells, 4}, Extent{kExtentCells, 4}),
            make(2, "B", 0, 0, Extent{kExtentCells, 4}, Extent{kExtentCells, 4}),
        };
        authored[0].context = 2;
        authored[1].context = 1;
        CHECK(resolve(authored, Viewport{48, 16}).items.empty());
    }
    SUBCASE("a longer loop, and the innocent objects hanging off it") {
        std::vector<Element> authored{
            make(1, "A", 0, 0, Extent{kExtentCells, 4}, Extent{kExtentCells, 4}),
            make(2, "B", 0, 0, Extent{kExtentCells, 4}, Extent{kExtentCells, 4}),
            make(3, "C", 0, 0, Extent{kExtentCells, 4}, Extent{kExtentCells, 4}),
            make(4, "hangs off C", 0, 0, Extent{kExtentCells, 4}, Extent{kExtentCells, 4}),
            make(5, "innocent", 7, 7, Extent{kExtentCells, 4}, Extent{kExtentCells, 4}),
        };
        authored[0].context = 3; // A -> C
        authored[1].context = 1; // B -> A
        authored[2].context = 2; // C -> B, closing it
        authored[3].context = 3; // and one that merely FEEDS the loop

        const Scene scene = resolve(authored, Viewport{48, 16});
        // Everything that cannot reach the root is absent -- including #4, which
        // is in no cycle itself but has no frame either.
        REQUIRE(scene.items.size() == 1);
        CHECK(scene.items[0].id == 5);
        CHECK(scene.items[0].rect == Rect{7, 7, 4, 4});
    }
}

TEST_CASE("one walk answers what a chain does, and it is the walk a law can ask") {
    std::vector<Element> authored{
        make(1, "A", 0, 0, Extent{kExtentCells, 4}, Extent{kExtentCells, 4}),
        make(2, "B", 0, 0, Extent{kExtentCells, 4}, Extent{kExtentCells, 4}),
        make(3, "C", 0, 0, Extent{kExtentCells, 4}, Extent{kExtentCells, 4}),
    };
    authored[1].context = 1; // B -> A
    authored[2].context = 2; // C -> B
    const ById index(authored);

    SUBCASE("it reaches the root, and reports what it passed through") {
        const ContextWalk walk = walk_context(authored, index, 3);
        CHECK(walk.reaches_root());
        CHECK(walk.chain == std::vector<std::int64_t>{3, 2, 1});
        // ...which is exactly the question an authoring operation asks before it
        // writes a relationship: would this close a loop?
        CHECK(walk.passes_through(1));
        CHECK_FALSE(walk.passes_through(4));

        // The root itself is a chain of nothing, and that is a normal answer.
        const ContextWalk root = walk_context(authored, index, kRootContext);
        CHECK(root.reaches_root());
        CHECK(root.chain.empty());
    }
    SUBCASE("it names what is missing") {
        authored[0].context = 77;
        const ContextWalk walk = walk_context(authored, index, 3);
        CHECK(walk.end == ContextEnd::Missing);
        CHECK(walk.at == 77);
    }
    SUBCASE("it names the loop, as ONE LAP rather than the road into it") {
        authored[0].context = 3; // A -> C, closing A -> C -> B -> A
        const ContextWalk walk = walk_context(authored, index, 3);
        REQUIRE(walk.end == ContextEnd::Cycle);
        // One lap, closed: the diagnostic is a loop a person can read, and it
        // does not grow with how far the walk had travelled first.
        CHECK(walk.chain.size() == 4);
        CHECK(walk.chain.front() == walk.chain.back());
    }
    SUBCASE("the memo shortens the work, not the verdict") {
        std::vector<char> settled(authored.size(), 0);
        const ContextWalk first = walk_context(authored, index, 3, &settled);
        CHECK(first.reaches_root());
        CHECK(first.chain.size() == 3);
        // A, B and C are now known to reach the root, so asking again stops at
        // once -- and still answers Root. That is what turns a whole-document
        // check from one walk per element into one visit per element.
        const ContextWalk again = walk_context(authored, index, 3, &settled);
        CHECK(again.reaches_root());
        CHECK(again.chain.empty());
    }
}

TEST_CASE("an identity is found by identity, and a repeated one answers with the first") {
    std::vector<Element> authored{
        make(5, "first", 0, 0, Extent{kExtentCells, 1}, Extent{kExtentCells, 1}),
        make(9, "other", 0, 0, Extent{kExtentCells, 1}, Extent{kExtentCells, 1}),
        make(5, "second", 0, 0, Extent{kExtentCells, 1}, Extent{kExtentCells, 1}),
    };
    const ById index(authored);
    CHECK(index.find(9) == 1);
    CHECK(index.find(404) == ById::npos);
    CHECK(index.find(kRootContext) == ById::npos); // the root is not an element

    // Distinctness is a DOCUMENT law and this is a vocabulary, so a repeated
    // identity is representable here and gets a stated answer rather than an
    // assumption: the first, which is what a linear search would have said.
    CHECK(index.find(5) == 0);
}

TEST_CASE("the frame an element was read in is asked of the resolver, not reconstructed") {
    std::vector<Element> authored{
        make(1, "A", 4, 3, Extent{kExtentCells, 20}, Extent{kExtentCells, 8}),
        make(2, "B", 1, 1, Extent{kExtentCells, 4}, Extent{kExtentCells, 2}),
        make(3, "broken", 0, 0, Extent{kExtentCells, 4}, Extent{kExtentCells, 2}),
    };
    authored[1].context = 1;
    authored[2].context = 404;
    const Scene scene = resolve(authored, Viewport{48, 16});

    CHECK(frame_in(scene, authored[0]) == Rect{0, 0, 48, 16}); // the root's
    CHECK(frame_in(scene, authored[1]) == Rect{4, 3, 20, 8});  // A's, as resolved
    // An element the scene did not place has no frame either, and says so with
    // an empty one rather than quietly handing back the root.
    CHECK(frame_in(scene, authored[2]) == Rect{});
}

TEST_CASE("composed placement is TOTAL over coordinates no setter would have produced") {
    // A resolved position that WAS the authored one has no sum to overflow. This
    // one is `the context's origin + the authored offset`, and both terms come
    // off a ZEN_SHAPE -- so a poke can write the pair that makes `a + b` signed
    // overflow, which is undefined behaviour produced by data. Only a sanitizer
    // can see the difference; these assertions are about the ANSWERS.
    constexpr std::int64_t kMax = (std::numeric_limits<std::int64_t>::max)();
    constexpr std::int64_t kMin = (std::numeric_limits<std::int64_t>::min)();

    CHECK(add_cells(kMax, 1) == kMax);
    CHECK(add_cells(kMin, -1) == kMin);
    CHECK(add_cells(kMax, kMax) == kMax);
    CHECK(add_cells(kMin, kMin) == kMin);
    CHECK(add_cells(3, -5) == -2);
    CHECK(add_cells(kMax, 0) == kMax);

    std::vector<Element> authored{
        make(1, "far", kMax - 1, 0, Extent{kExtentCells, 4}, Extent{kExtentCells, 4}),
        make(2, "further", 100, 0, Extent{kExtentCells, 4}, Extent{kExtentCells, 4}),
    };
    authored[1].context = 1;
    const Scene scene = resolve(authored, Viewport{48, 16});
    REQUIRE(scene.items.size() == 2);
    CHECK(scene.items[1].rect.x == kMax); // saturated, far outside any viewport
    CHECK(hit(scene, 0, 0) == nullptr);
}

TEST_CASE("a dependent may extend past its source: this vocabulary does not clip") {
    // Recorded as a decision. A frame supplies an origin and a span; it does not
    // impose a boundary, and nothing here trims a rectangle to its source. An
    // application that wants clipping is welcome to it -- that is a PAINTING
    // policy, and it would live wherever the painting does.
    std::vector<Element> authored{
        make(1, "small", 5, 5, Extent{kExtentCells, 4}, Extent{kExtentCells, 2}),
        make(2, "spills", -3, -2, Extent{kExtentCells, 30}, Extent{kExtentCells, 9}),
    };
    authored[1].context = 1;

    const Scene scene = resolve(authored, Viewport{48, 16});
    REQUIRE(scene.items.size() == 2);
    CHECK(scene.items[1].rect == Rect{2, 3, 30, 9});

    // ...and it is hittable everywhere it is, including well outside the
    // rectangle that gave it its frame.
    REQUIRE(hit(scene, 30, 10) != nullptr);
    CHECK(hit(scene, 30, 10)->id == 2);
    REQUIRE(hit(scene, 2, 3) != nullptr);
    CHECK(hit(scene, 2, 3)->id == 2);
}

} // TEST_SUITE
