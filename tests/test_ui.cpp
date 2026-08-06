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

    const auto element = SchemaBuilder("Element", 1)
                             .field("id", Kind::Int)
                             .field("label", Kind::Text)
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

    // A viewport with no span has no share to give.
    CHECK(resolve_extent(Extent{kExtentPercent, 50}, 0) == kMinCells);
    CHECK(resolve_extent(Extent{kExtentPercent, 50}, -10) == kMinCells);

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

} // TEST_SUITE
