// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The UI suite -- the authored/solved distinction, checked rather than believed: what a weaver
// AUTHORS (ui/vocabulary.hpp) is not what a room MAKES of it (ui/layout.hpp). The authored side is
// content (the fence fires in the compile-negative entries; its traits discriminate here), solving
// is a function of a room even for values no setter would accept, the solved side has no wire
// form, and the intent's own problems are asked of the intent alone. All pure: no kernel.

// main() and the framework live in doctest_main.cpp -- the shared one that
// refuses a run selecting zero cases (POP-01).
#include "doctest.h"

#include "ui/layout.hpp"
#include "ui/vocabulary.hpp"

#include <zen/schema.hpp>
#include <zen/weave.hpp>

#include <cstdint>
#include <limits>
#include <optional>
#include <ostream> // doctest prints a std::string_view through it
#include <set>
#include <string>
#include <string_view>
#include <vector>

using namespace zengine::ui;
using loom::schema_of;

namespace {

constexpr std::int64_t kMax = (std::numeric_limits<std::int64_t>::max)();
constexpr std::int64_t kMin = (std::numeric_limits<std::int64_t>::min)();

/// Workshop's widest screen in pixels (`kScreenMaxW`, workshop/screen.hpp): the widest room a
/// window here solves.
constexpr std::int64_t kWidestRoom = 640 * 12;

Size px(std::int64_t n) { return Size{kSizePixels, n}; }
Size share(std::int64_t s) { return Size{kSizeShare, s}; }
Size word(std::int64_t mode) { return Size{mode, std::nullopt}; }
Place at(std::int64_t x, std::int64_t y) { return Place{kPlacePixels, x, y}; }

Element make(std::int64_t id, std::string label, Place place, Size w, Size h,
             std::int64_t context = kRootContext) {
    Element e;
    e.id = id;
    e.label = std::move(label);
    e.context = context;
    e.place = place;
    e.width = w;
    e.height = h;
    return e;
}

/// The one rectangle a room makes of one part, with its measure.
Rect solve_one(const Element& e, Room room, Measure m = {}) {
    const Scene s = arrange({e}, room, {m});
    REQUIRE(s.items.size() == 1);
    return s.items[0].rect;
}

// ---- The fence's negative subjects ------------------------------------------------------
//
// Types that violate exactly one half of the fence each, so the traits can be shown to
// DISCRIMINATE rather than merely to be true of Element. They are never used as parts.

/// A solved width, smuggled in as a bare number where a Size belongs.
struct CollapsedExtent {
    std::int64_t id = 0;
    std::int64_t width = 0;
    std::int64_t height = 0;
};

/// Authored sizes kept honestly, and a solved rectangle cached beside them.
struct CachesResolved {
    std::int64_t id = 0;
    Size width;
    Size height;
    std::int64_t w = 0; ///< the cache -- the exact lie the fence exists to prevent
    std::int64_t h = 0;
};

/// Not this vocabulary's part at all: no sizes to be authored.
struct NoExtents {
    std::int64_t id = 0;
    std::string label;
};

} // namespace

TEST_SUITE("ui") {

// ============================================================================
// 1 — the authored side is content, said in words
// ============================================================================

TEST_CASE("contract: the authored shapes derive their declared spellings exactly") {
    using loom::Kind;
    using loom::SchemaBuilder;

    // Every number is an optional field: present exactly where its word reads it.
    const auto size = SchemaBuilder("Size", 1)
                          .field("mode", Kind::Int)
                          .field("amount", Kind::Int, false)
                          .build();
    CHECK(schema_of<Size>()->content_id() == size->content_id());

    const auto place = SchemaBuilder("Place", 1)
                           .field("mode", Kind::Int)
                           .field("x", Kind::Int, false)
                           .field("y", Kind::Int, false)
                           .build();
    CHECK(schema_of<Place>()->content_id() == place->content_id());

    // `Element` is version 3 with exactly these fields. A published shape never
    // changes under its version: two builds spelling one version differently
    // derive different content-ids and fail to agree rather than mis-decode.
    const auto element = SchemaBuilder("Element", 3)
                             .field("id", Kind::Int)
                             .field("label", Kind::Text)
                             .field("context", Kind::Int)
                             .message("place", place)
                             .message("width", size)
                             .message("height", size)
                             .field("flow", Kind::Int)
                             .field("gap", Kind::Int, false)
                             .build();
    CHECK(schema_of<Element>()->content_id() == element->content_id());
}

TEST_CASE("the fence discriminates: it is a question about a type, not a decoration") {
    // The subject it was written for.
    CHECK(extents_are_authored_v<Element>);
    CHECK(carries_no_resolved_geometry_v<Element>);
    CHECK(authored_only_v<Element>);

    // Half one, type-aware: a width that is a NUMBER is a solved width, whatever it is
    // called. This is the half a name-based fence cannot state -- `width` is exactly the
    // spelling an authored width wants.
    CHECK_FALSE(extents_are_authored_v<CollapsedExtent>);
    CHECK_FALSE(authored_only_v<CollapsedExtent>);

    // Half two, name-based: sizes authored properly, and a solved rectangle cached on
    // the part anyway. The type check alone would pass this one.
    CHECK(extents_are_authored_v<CachesResolved>);
    CHECK_FALSE(carries_no_resolved_geometry_v<CachesResolved>);
    CHECK_FALSE(authored_only_v<CachesResolved>);

    // A type with no sizes at all is not an authored part of this vocabulary. It is not
    // "geometry-free and therefore fine" -- the trait answers about a shape it can
    // recognise, and says no rather than defaulting to yes.
    CHECK_FALSE(extents_are_authored_v<NoExtents>);
    CHECK_FALSE(authored_only_v<NoExtents>);
    CHECK(carries_no_resolved_geometry_v<NoExtents>); // the other half is still true of it
}

TEST_CASE("each word is spelled by one table, and reads exactly its own numbers") {
    CHECK(size_word(kSizeDefault) == "default");
    CHECK(size_word(kSizePixels) == "pixels");
    CHECK(size_word(kSizeShare) == "share");
    CHECK(size_word(kSizeFit) == "fit");
    CHECK(size_word(kSizeFill) == "fill");
    CHECK(place_word(kPlaceDefault) == "default");
    CHECK(place_word(kPlacePixels) == "pixels");
    CHECK(place_word(kPlaceTop) == "top");
    CHECK(place_word(kPlaceTopRight) == "top-right");
    CHECK(place_word(kPlaceLeft) == "left");
    CHECK(place_word(kPlaceCenter) == "center");
    CHECK(place_word(kPlaceRight) == "right");
    CHECK(place_word(kPlaceBottomLeft) == "bottom-left");
    CHECK(place_word(kPlaceBottom) == "bottom");
    CHECK(place_word(kPlaceBottomRight) == "bottom-right");
    CHECK(place_word(kPlaceShare) == "share");
    // A number no word is has no spelling: it is not quietly some word.
    CHECK(size_word(5).empty());
    CHECK(size_word(-1).empty());
    CHECK(place_word(11).empty());
    CHECK(place_word(kMin).empty());

    // Every spelling is its own.
    std::set<std::string_view> sizes(kSizeWords.begin(), kSizeWords.end());
    std::set<std::string_view> places(kPlaceWords.begin(), kPlaceWords.end());
    CHECK(sizes.size() == kSizeWords.size());
    CHECK(places.size() == kPlaceWords.size());

    // ...and the same table reads a word back to its number, every word and only those.
    for (std::size_t i = 0; i < kSizeWords.size(); ++i) {
        CHECK(size_named(kSizeWords[i]) == static_cast<std::int64_t>(i));
    }
    for (std::size_t i = 0; i < kPlaceWords.size(); ++i) {
        CHECK(place_named(kPlaceWords[i]) == static_cast<std::int64_t>(i));
    }
    CHECK_FALSE(size_named("percent").has_value());
    CHECK_FALSE(place_named("centre").has_value());
    CHECK_FALSE(place_named("right-column").has_value());
    CHECK_FALSE(size_named("").has_value());

    // A size reads its amount where the word counts something, and nowhere else.
    CHECK(size_reads_amount(kSizePixels));
    CHECK(size_reads_amount(kSizeShare));
    CHECK_FALSE(size_reads_amount(kSizeDefault));
    CHECK_FALSE(size_reads_amount(kSizeFit));
    CHECK_FALSE(size_reads_amount(kSizeFill));

    // A place reads an offset along every axis it has an edge on; a centred axis and `default`
    // read none.
    const struct {
        std::int64_t mode;
        bool x, y;
    } reads[] = {{kPlaceDefault, false, false},    {kPlacePixels, true, true},
                 {kPlaceTop, false, true},         {kPlaceTopRight, true, true},
                 {kPlaceLeft, true, false},        {kPlaceCenter, false, false},
                 {kPlaceRight, true, false},       {kPlaceBottomLeft, true, true},
                 {kPlaceBottom, false, true},      {kPlaceBottomRight, true, true},
                 {kPlaceShare, true, true},        {77, false, false}};
    for (const auto& r : reads) {
        CAPTURE(r.mode);
        CHECK(place_reads_x(r.mode) == r.x);
        CHECK(place_reads_y(r.mode) == r.y);
        // ...and only `share` reads them as ten-thousandths of the frame.
        CHECK(place_reads_share(r.mode) == (r.mode == kPlaceShare));
    }
}

// ============================================================================
// 2 — solving is a function of a room
// ============================================================================

TEST_CASE("a size reads its frame: pixels exact, a share of the span, default and fit the measure") {
    const Room room{480, 144, 1};
    CHECK(solve_one(make(1, "a", Place{}, px(100), px(10)), room).w == 100);
    CHECK(solve_one(make(1, "a", Place{}, share(2500), px(10)), room).w == 120);

    // `default` is the owner's answer: the measure it says, else the frame's span.
    CHECK(solve_one(make(1, "a", Place{}, word(kSizeDefault), px(10)), room, Measure{77, {}}).w ==
          77);
    CHECK(solve_one(make(1, "a", Place{}, word(kSizeDefault), px(10)), room).w == 480);

    // `fit` is the part's own measure, rounded up to the window's step; a terminal's step is a
    // cell.
    CHECK(solve_one(make(1, "a", Place{}, word(kSizeFit), px(10)), room, Measure{77, {}}).w == 77);
    CHECK(solve_one(make(1, "a", Place{}, word(kSizeFit), px(10)), Room{480, 144, 12},
                    Measure{77, {}})
              .w == 84);

    // A pixel length is exact at every step: a terminal floors it when it shows it, and the
    // solve does not.
    CHECK(solve_one(make(1, "a", at(5, 7), px(100), px(10)), Room{480, 144, 12}) ==
          Rect{5, 7, 100, 10});

    // A share's far edge is the exact position floored to the step, counted from the frame's
    // start: 480 * 2/10 is 96, and 5 + 96 = 101 floors to 96 at a cell.
    CHECK(solve_one(make(1, "a", at(5, 0), share(2000), px(10)), Room{480, 144, 12}) ==
          Rect{5, 0, 91, 10});
    CHECK(solve_one(make(1, "a", at(5, 0), share(2000), px(10)), room) == Rect{5, 0, 96, 10});
}

TEST_CASE("every place word stands its part at its own point of the frame") {
    const Room room{400, 200, 1};
    const struct {
        std::int64_t mode;
        Rect want;
    } points[] = {
        {kPlaceDefault, Rect{0, 0, 40, 20}},       {kPlacePixels, Rect{10, 5, 40, 20}},
        {kPlaceTop, Rect{180, 5, 40, 20}},         {kPlaceTopRight, Rect{350, 5, 40, 20}},
        {kPlaceLeft, Rect{10, 90, 40, 20}},        {kPlaceCenter, Rect{180, 90, 40, 20}},
        {kPlaceRight, Rect{350, 90, 40, 20}},      {kPlaceBottomLeft, Rect{10, 175, 40, 20}},
        {kPlaceBottom, Rect{180, 175, 40, 20}},    {kPlaceBottomRight, Rect{350, 175, 40, 20}},
    };
    for (const auto& p : points) {
        CAPTURE(p.mode);
        // Both offsets are present on every word; each reads only what its word reads, inward
        // from the edge it names.
        CHECK(solve_one(make(1, "a", Place{p.mode, 10, 5}, px(40), px(20)), room) == p.want);
    }

    // A centring floors to the step from the frame's start: (400 - 50) / 2 is 175, 168 at a
    // cell; (200 - 20) / 2 is 90, 84 at a cell.
    CHECK(solve_one(make(1, "a", Place{kPlaceCenter, {}, {}}, px(50), px(20)), Room{400, 200, 12}) ==
          Rect{168, 84, 50, 20});
    // An absent offset reads as none.
    CHECK(solve_one(make(1, "a", Place{kPlaceBottomRight, {}, {}}, px(40), px(20)), room) ==
          Rect{360, 180, 40, 20});
}

TEST_CASE("a share place stands its part a share of the frame's width and height from its top-left, floored to the step") {
    // ⚔ MUTATION: `edges_of` reading a share place's offsets as pixels -- the bar stands 3333
    // pixels down, past the room's floor.
    // A BAR A THIRD OF THE WAY DOWN, across the whole frame: 600 * 3333 / 10000 is 199.98.
    const Element bar = make(1, "bar", Place{kPlaceShare, 0, 3333}, word(kSizeFill), px(48));
    CHECK(solve_one(bar, Room{900, 600, 1}) == Rect{0, 199, 900, 48});
    // ...a third down at every size: 960 by 720 puts it at 239.97, and a terminal's step floors
    // the edge from the frame's start, 199 to 192 at a twelve-pixel cell.
    CHECK(solve_one(bar, Room{960, 720, 1}) == Rect{0, 239, 960, 48});
    CHECK(solve_one(bar, Room{900, 600, 12}) == Rect{0, 192, 900, 48});
    // Both axes are shares, each of its own span; a share size after it is laid from there.
    CHECK(solve_one(make(1, "a", Place{kPlaceShare, 5000, 2500}, share(2500), px(10)),
                    Room{401, 200, 1}) == Rect{200, 50, 100, 10});
    // A share place's offsets are clamped to the frame, as a share size's amount is, and an
    // absent one reads as none.
    CHECK(solve_one(make(1, "a", Place{kPlaceShare, -5, 20000}, px(40), px(20)), Room{400, 200, 1}) ==
          Rect{0, 200, 40, 20});
    CHECK(solve_one(make(1, "a", Place{kPlaceShare, {}, {}}, px(40), px(20)), Room{400, 200, 1}) ==
          Rect{0, 0, 40, 20});
    // Only `share` reads its offsets so: the same numbers under `pixels` are pixels.
    CHECK(solve_one(make(1, "a", Place{kPlacePixels, 50, 25}, px(40), px(20)), Room{400, 200, 1}) ==
          Rect{50, 25, 40, 20});
}

TEST_CASE("fill runs from the part's anchored edge to the frame's far one") {
    const Room room{300, 100, 1};
    CHECK(solve_one(make(1, "a", at(40, 0), word(kSizeFill), px(10)), room) == Rect{40, 0, 260, 10});
    CHECK(solve_one(make(1, "a", Place{kPlaceTopRight, 40, 0}, word(kSizeFill), px(10)), room) ==
          Rect{0, 0, 260, 10});
    CHECK(solve_one(make(1, "a", Place{kPlaceCenter, {}, {}}, word(kSizeFill), word(kSizeFill)),
                    room) == Rect{0, 0, 300, 100});
    // An offset past the frame's far edge leaves nothing to fill.
    CHECK(solve_one(make(1, "a", at(400, 0), word(kSizeFill), px(10)), room).w == 0);
}

TEST_CASE("solving is TOTAL for values no setter would have accepted") {
    // Authored content is a ZEN_SHAPE. It arrives from the wire and from a poke, neither of
    // which has been past an application's check -- so every one of these is a value the solve
    // must survive rather than trust. The sanitizer lane is the other half of this evidence.
    const std::int64_t ends[] = {kMin, -1, 0, kMax};
    const std::optional<std::int64_t> said[] = {std::nullopt, kMin, 0, kMax};
    std::size_t solved = 0;
    for (const std::int64_t span : {std::int64_t{0}, kWidestRoom, kMax}) {
        for (const std::int64_t step : {std::int64_t{1}, std::int64_t{12}, std::int64_t{0}, kMax}) {
            for (std::int64_t size = -1; size <= kSizeFill + 1; ++size) {
                for (std::int64_t place = -1; place <= kPlaceBottomRight + 1; ++place) {
                    for (const std::int64_t n : ends) {
                        for (const auto& m : said) {
                            const Element e =
                                make(1, "poked", Place{place, n, n}, Size{size, n}, Size{size, n});
                            const Scene s = arrange({e}, Room{span, span, step}, {Measure{m, m}});
                            solved += s.items.size();
                        }
                    }
                }
            }
        }
    }
    CHECK(solved == 3 * 4 * 7 * 12 * 4 * 4);

    // An out-of-range share is clamped, not multiplied out.
    CHECK(solve_one(make(1, "a", Place{}, share(kMax), px(1)), Room{kWidestRoom, 10, 1}).w ==
          kWidestRoom);
    CHECK(solve_one(make(1, "a", Place{}, share(kMin), px(1)), Room{kWidestRoom, 10, 1}).w == 0);
    CHECK(solve_one(make(1, "a", Place{}, share(20000), px(1)), Room{kWidestRoom, 10, 1}).w ==
          kWidestRoom);

    // An absurd span divides before it multiplies. `span * amount` on these operands is signed
    // overflow -- undefined behaviour produced by data.
    CHECK(solve_one(make(1, "a", Place{}, share(5000), px(1)), Room{kMax, 10, 1}).w == kMax / 2);
    CHECK(solve_one(make(1, "a", Place{}, share(kShareWhole), px(1)), Room{kMax, 10, 1}).w == kMax);
    CHECK(solve_one(make(1, "a", Place{kPlaceCenter, {}, {}}, share(5000), px(1)),
                    Room{kMax, 10, 1})
              .x == kMax / 4);

    // A position floored to the step near int64's minimum saturates there: the step below it is not
    // a number, and `floor(p / step) * step` would pass the end of the type.
    CHECK(solve_one(make(1, "a", Place{kPlaceRight, kMax, {}}, share(5000), px(1)), Room{0, 10, 12})
              .x == kMin);

    // A room with no span has no share to give, and a negative one is none.
    CHECK(solve_one(make(1, "a", Place{}, share(5000), px(1)), Room{0, 10, 1}).w == 0);
    CHECK(solve_one(make(1, "a", Place{}, share(5000), px(1)), Room{kMin, 10, 1}).w == 0);

    // A step of none, or less, is one pixel.
    CHECK(solve_one(make(1, "a", at(5, 0), share(2000), px(1)), Room{480, 10, 0}).w == 96);
    CHECK(solve_one(make(1, "a", at(5, 0), share(2000), px(1)), Room{480, 10, -12}).w == 96);

    // A word no word is reads as `default`, in both lists.
    CHECK(solve_one(make(1, "a", Place{99, 30, 30}, Size{99, 7}, px(1)), Room{480, 10, 1},
                    Measure{55, {}}) == Rect{0, 0, 55, 1});
}

TEST_CASE("arrange places every authored part, in authored order, in ONE room") {
    const std::vector<Element> authored{
        make(1, "wide", at(36, 24), share(5000), px(72)),
        make(2, "fixed", at(72, 120), px(168), px(48)),
    };

    const Scene scene = arrange(authored, Room{576, 192, 1});
    REQUIRE(scene.items.size() == 2);

    // Authored order is paint order, said once.
    CHECK(scene.items[0].id == 1);
    CHECK(scene.items[1].id == 2);

    // A pixel place passes through untouched; a share is solved.
    CHECK(scene.items[0].rect == Rect{36, 24, 288, 72});
    CHECK(scene.items[1].rect == Rect{72, 120, 168, 48});

    // A scene keeps the room it is an observation OF. A rectangle without it is a number with
    // its meaning cut off.
    CHECK(scene.room == Room{576, 192, 1});

    // An empty sequence solves to an empty scene, which is a legitimate observation and not a
    // missing one.
    CHECK(arrange({}, Room{576, 192, 1}).items.empty());
}

TEST_CASE("authored and solved are different facts, and only one of them moves") {
    const std::vector<Element> authored{
        make(1, "wide", Place{}, share(5000), px(48)),
    };

    const Scene wide = arrange(authored, Room{576, 192, 1});
    const Scene narrow = arrange(authored, Room{288, 192, 1});

    CHECK(wide.items[0].rect.w == 288);
    CHECK(narrow.items[0].rect.w == 144);
    CHECK(wide.items[0].rect.h == narrow.items[0].rect.h); // pixels do not move

    // Solving twice wrote nothing. The authored value is exactly what it was, and there is
    // nowhere on it a solved number could have gone.
    CHECK(authored[0].width == share(5000));
    CHECK(authored[0].place == Place{});
}

// ============================================================================
// 3 — the solved side is an observation, and hit testing answers with identity
// ============================================================================

TEST_CASE("a resolved observation has no wire form") {
    // `loom::Shape` is the substrate's own question, and its absence on the solved side is what
    // keeps an observation from being stored, sent or poked as though it were authored content.
    // Asserted at compile time in layout.hpp; restated here so a suite run reports it as a
    // checked claim rather than an invisible one.
    CHECK(loom::Shape<Element>);
    CHECK(loom::Shape<Size>);
    CHECK(loom::Shape<Place>);
    CHECK_FALSE(loom::Shape<Rect>);
    CHECK_FALSE(loom::Shape<Placed>);
    CHECK_FALSE(loom::Shape<Scene>);
    CHECK_FALSE(loom::Shape<Room>);
    CHECK_FALSE(loom::Shape<Measure>);
}

TEST_CASE("hit testing answers with the authored identity, topmost first") {
    const std::vector<Element> authored{
        make(7, "back", at(0, 0), px(120), px(72)),
        make(9, "front", at(48, 24), px(48), px(24)),
    };
    const Scene scene = arrange(authored, Room{576, 192, 1});

    REQUIRE(hit(scene, 0, 0) != nullptr);
    CHECK(hit(scene, 0, 0)->id == 7);
    CHECK(hit(scene, 119, 71)->id == 7);

    // Overlap: the LAST authored wins, because the last authored is the last painted, so the
    // answer agrees with what a person can see.
    REQUIRE(hit(scene, 60, 30) != nullptr);
    CHECK(hit(scene, 60, 30)->id == 9);

    // What it hands back is the observation, so a caller gets the rectangle it hit without
    // re-deriving it -- and the identity, not a rectangle index, a label, or a copy of a painted
    // item.
    CHECK(hit(scene, 60, 30)->rect == Rect{48, 24, 48, 24});

    // One pixel past an edge is nothing, on every edge.
    CHECK(hit(scene, 120, 0) == nullptr);
    CHECK(hit(scene, 0, 72) == nullptr);
    CHECK(hit(scene, -1, 0) == nullptr);
    CHECK(hit(scene, 0, -1) == nullptr);

    // An empty scene answers "nothing", which is an answer.
    CHECK(hit(Scene{}, 0, 0) == nullptr);
}

TEST_CASE("a share's hit area follows the room, because both read one scene") {
    const std::vector<Element> authored{
        make(1, "half", Place{}, share(5000), px(24)),
    };

    // Half of 576 is 288, so pixel 250 is inside it.
    const Scene wide = arrange(authored, Room{576, 192, 1});
    REQUIRE(hit(wide, 250, 0) != nullptr);
    CHECK(hit(wide, 250, 0)->id == 1);

    // Half of 288 is 144, so the same pixel is now outside the same authored part. There is no
    // second copy of the geometry able to disagree.
    const Scene narrow = arrange(authored, Room{288, 192, 1});
    CHECK(hit(narrow, 250, 0) == nullptr);
    REQUIRE(hit(narrow, 60, 0) != nullptr);
    CHECK(hit(narrow, 60, 0)->id == 1);
}

TEST_CASE("where one identity landed, and null as a normal answer") {
    const std::vector<Element> authored{
        make(1, "a", at(0, 0), px(36), px(36)),
        make(2, "b", at(60, 60), share(2500), px(24)),
    };
    const Scene scene = arrange(authored, Room{576, 192, 1});

    REQUIRE(placed_for(scene, 2) != nullptr);
    CHECK(placed_for(scene, 2)->rect == Rect{60, 60, 144, 24});

    // An id this scene has no part for is a normal answer, not an error: a selection can
    // outlive the part it selected.
    CHECK(placed_for(scene, 4242) == nullptr);
    CHECK(placed_for(scene, 0) == nullptr);
}

TEST_CASE("the rectangle test is TOTAL, over every rect a resolved scene can hold") {
    // A pixel length solves to ITSELF, authored content is a ZEN_SHAPE, and a poke writes past
    // every application's check -- so a scene solved from poked content holds rects whose edges
    // are not representable, and `x + w` on one is undefined behaviour produced by data. `hit`
    // is a weaver's hand asking, so the assertions are about ANSWERS, and the sanitizer lane is
    // the other half of the evidence.
    const Rect wild{1, 1, kMax, kMax};
    CHECK(wild.contains(5, 5));       // it really is that big
    CHECK_FALSE(wild.contains(0, 5)); // ...and still bounded on the near side
    CHECK_FALSE(wild.contains(5, 0));
    // A rect starting at the most negative pixel and as wide as the type allows still ENDS
    // somewhere: kMin + kMax is -1, so its last pixel is -2 and 0 is outside it. The unsigned
    // difference gets that exactly right where the signed addition could not have been
    // evaluated at all.
    const Rect huge{kMin, kMin, kMax, kMax};
    CHECK(huge.contains(-2, -2));
    CHECK_FALSE(huge.contains(-1, -1));
    CHECK_FALSE(huge.contains(0, 0));
    CHECK_FALSE(Rect{kMin, kMin, 2, 2}.contains(0, 0));
    CHECK_FALSE(Rect{1, 1, kMin, kMin}.contains(5, 5));

    // An empty or inverted rectangle contains nothing -- stated, not left to arithmetic that
    // could overflow.
    CHECK_FALSE(Rect{0, 0, 0, 4}.contains(0, 0));
    CHECK_FALSE(Rect{0, 0, 4, 0}.contains(0, 0));
    CHECK_FALSE(Rect{0, 0, -3, -3}.contains(0, 0));

    // ...and through the package's own door, which is how it is actually reached.
    const std::vector<Element> authored{make(1, "poked", at(1, 1), px(kMax), px(48))};
    const Scene scene = arrange(authored, Room{576, 192, 1});
    REQUIRE(hit(scene, 5, 2) != nullptr);
    CHECK(hit(scene, 5, 2)->id == 1);
    CHECK(hit(scene, 0, 2) == nullptr);
}

// ============================================================================
// 4 — composition: the frame an authored part stands in
// ============================================================================
// A FRAME (an origin and a span) is what solving consumes, the root's like any part's; a part
// names its frame by IDENTITY; work goes in dependency order, ANSWERS in document order (paint
// order); there is no depth ceiling, and a chain that cannot reach the root is an absence.

TEST_CASE("a part stands IN a frame, and the root supplies one like anything else") {
    // `authored part + frame = solved rectangle`, with the two halves of the frame doing the two
    // jobs they always do -- the origin the offsets count from, the span the shares are shares
    // of.
    std::vector<Element> authored{
        make(1, "A", at(120, 48), px(240), px(96)),
        make(2, "b", at(24, 12), share(5000), px(36), 1),
    };
    const Scene scene = arrange(authored, Room{576, 192, 1});
    CHECK(scene.items[1].rect == Rect{144, 60, 120, 36});

    // The root's frame is the whole room at the origin, named rather than assumed.
    CHECK(root_frame(Room{576, 192, 1}) == Rect{0, 0, 576, 192});
    authored[1].context = kRootContext;
    CHECK(arrange(authored, Room{576, 192, 1}).items[1].rect == Rect{24, 12, 288, 36});

    // Pixels ignore the span they are given, in every frame.
    authored[1].width = px(108);
    CHECK(arrange(authored, Room{576, 192, 1}).items[1].rect.w == 108);
    authored[1].context = 1;
    CHECK(arrange(authored, Room{576, 192, 1}).items[1].rect.w == 108);
}

TEST_CASE("an element's values are read in the frame its source resolved to") {
    std::vector<Element> authored{
        make(1, "A", at(48, 36), share(6000), px(120)),
        make(2, "B", at(24, 12), share(5000), px(48), 1),
    };

    const Scene scene = arrange(authored, Room{480, 240, 1});
    REQUIRE(scene.items.size() == 2);

    // A stands in the root: 60% of 480 is 288.
    CHECK(scene.items[0].rect == Rect{48, 36, 288, 120});
    // B stands in A: its 24,12 is an offset from A's ORIGIN, and its share is half of A's
    // SOLVED 288 -- not half of the room.
    CHECK(scene.items[1].rect == Rect{72, 48, 144, 48});

    // Neither authored value moved. Solving is an observation, and that is as true one link
    // down a chain as it was at the root.
    CHECK(authored[1].place == at(24, 12));
    CHECK(authored[1].width == share(5000));
    CHECK(authored[1].context == 1);

    // Change the room's span and BOTH follow, transitively, with every authored number identical.
    const Scene narrow = arrange(authored, Room{240, 240, 1});
    CHECK(narrow.items[0].rect.w == 144); // 60% of 240
    CHECK(narrow.items[1].rect.w == 72);  // half of that 144
    CHECK(authored[0].width == share(6000));
    CHECK(authored[1].width == share(5000));
}

TEST_CASE("a source may sit LATER in the document than what measures against it") {
    // The strongest ordering claim in the package. Document order means paint order, hit order
    // and list order; dependency order is a fact about how solving must sequence its own work.
    // Making the first mean the second -- by sorting the document -- would silently change which
    // rectangle is in front, so it does not happen.
    const std::vector<Element> authored{
        make(3, "C", at(12, 12), px(24), px(24), 2),
        make(1, "A", at(120, 60), px(240), px(96)),
        make(2, "B", at(12, 12), px(72), px(36), 1),
    };

    const Scene scene = arrange(authored, Room{576, 192, 1});
    REQUIRE(scene.items.size() == 3);

    // ANSWERS IN DOCUMENT ORDER, unchanged: C, A, B.
    CHECK(scene.items[0].id == 3);
    CHECK(scene.items[1].id == 1);
    CHECK(scene.items[2].id == 2);

    // ...and every rectangle correct, though the work had to run A, B, C.
    CHECK(scene.items[1].rect == Rect{120, 60, 240, 96}); // A, in the root
    CHECK(scene.items[2].rect == Rect{132, 72, 72, 36});  // B, in A
    CHECK(scene.items[0].rect == Rect{144, 84, 24, 24});  // C, in B

    // Paint order is what it always was, so the topmost at an overlapping pixel is the LAST in
    // the document -- B -- even though B is what everything else depends on. Dependency order is
    // not z-order.
    REQUIRE(hit(scene, 144, 84) != nullptr);
    CHECK(hit(scene, 144, 84)->id == 2);
}

TEST_CASE("a frame that fits its parts is solved after them, authored after them or before") {
    // The other direction of dependency: a frame whose size is `fit` with no measure is the
    // reach of its parts, so its parts' sizes come first while its rectangle -- their origin --
    // still comes before their rectangles. The answer is in authored order either way.
    const Element frame = make(10, "fits", at(24, 12), word(kSizeFit), word(kSizeFit));
    const Element a = make(11, "a", at(0, 0), px(120), px(36), 10);
    const Element b = make(12, "b", at(48, 48), px(92), px(24), 10);

    for (const bool frame_first : {true, false}) {
        CAPTURE(frame_first);
        const std::vector<Element> authored =
            frame_first ? std::vector<Element>{frame, a, b} : std::vector<Element>{a, b, frame};
        const Scene scene = arrange(authored, Room{576, 192, 1});
        REQUIRE(scene.items.size() == 3);
        // Authored order out.
        CHECK(scene.items[0].id == authored[0].id);
        CHECK(scene.items[1].id == authored[1].id);
        CHECK(scene.items[2].id == authored[2].id);
        // The frame is the reach of its parts: 48 + 92 across, 48 + 24 down.
        CHECK(placed_for(scene, 10)->rect == Rect{24, 12, 140, 72});
        CHECK(placed_for(scene, 11)->rect == Rect{24, 12, 120, 36});
        CHECK(placed_for(scene, 12)->rect == Rect{72, 60, 92, 24});

        // At a cell's step the reach is rounded up to the step, as a measure is.
        const Scene cells = arrange(authored, Room{576, 192, 12});
        CHECK(placed_for(cells, 10)->rect == Rect{24, 12, 144, 72});

        // A frame that fits and is measured takes its measure, rounded up to the step.
        std::vector<Measure> measures(3);
        measures[frame_first ? 0 : 2] = Measure{50, 50};
        CHECK(placed_for(arrange(authored, Room{576, 192, 12}, measures), 10)->rect ==
              Rect{24, 12, 60, 60});
    }
}

TEST_CASE("complementary free shares meet on one pixel, at every span and every step") {
    // Two parts not solved together -- one a share from the frame's start, one the rest from its
    // end -- name one edge, and must meet on it with no gap and no overlap. Each computed edge
    // is the exact position floored to the step from the frame's start, so they do; widths
    // rounded alone would not. Every span 1 to 4,000 pixels at a pixel's step and 1 to 400 cells
    // at a cell's, every seventh share.
    std::int64_t pairs = 0;
    std::int64_t misses = 0;
    std::vector<Element> authored;
    for (std::int64_t s = 1; s < kShareWhole; s += 7) {
        authored.push_back(make(2 * s, "from the start", Place{}, share(s), px(1)));
        authored.push_back(
            make(2 * s + 1, "the rest, from the end", Place{kPlaceRight, 0, {}}, share(kShareWhole - s), px(1)));
    }
    const struct {
        std::int64_t step, last;
    } windows[] = {{1, 4000}, {12, 400 * 12}};
    for (const auto& w : windows) {
        for (std::int64_t span = w.step; span <= w.last; span += w.step) {
            const Scene scene = arrange(authored, Room{span, 10, w.step});
            for (std::size_t i = 0; i + 1 < scene.items.size(); i += 2) {
                const Rect& from_start = scene.items[i].rect;
                const Rect& from_end = scene.items[i + 1].rect;
                ++pairs;
                if (from_start.x + from_start.w != from_end.x || from_end.x + from_end.w != span) {
                    ++misses;
                }
            }
        }
    }
    CHECK(pairs == 4400 * 1429);
    CHECK(misses == 0);
}

TEST_CASE("a column lays its parts down a slot each, a gap apart, and begins again at its top") {
    // The column's slot for a part is its measured height, or its own height where none is
    // said; a part drawn taller or shorter than its slot leaves its neighbours where they are.
    Element column = make(1, "column", at(0, 24), px(288), px(192));
    column.flow = kFlowStack;
    column.gap = 12;
    const std::vector<Element> authored{
        column,
        make(2, "one", Place{}, word(kSizeDefault), word(kSizeDefault), 1),
        make(3, "two", Place{kPlaceRight, 12, {}}, px(96), px(100), 1),
        make(4, "three", Place{}, px(48), word(kSizeDefault), 1),
        make(5, "four, unmeasured", Place{}, px(48), px(36), 1),
    };
    const std::vector<Measure> measures{{}, {std::nullopt, 60}, {std::nullopt, 60},
                                        {std::nullopt, 60}};
    const Scene scene = arrange(authored, Room{576, 240, 1}, measures);
    REQUIRE(scene.items.size() == 5);
    // A part's width reads the column: its measure, else the column's span.
    CHECK(scene.items[1].rect == Rect{0, 24, 288, 60});
    // A place reads across: `right` stands it 12 in from the column's right edge. Its slot is
    // its measure, 60, though it is drawn 100 tall.
    CHECK(scene.items[2].rect == Rect{180, 96, 96, 100});
    // 144 + 60 passes the floor at 192, so the third begins the column again at its top.
    CHECK(scene.items[3].rect == Rect{0, 24, 48, 60});
    // The fourth says no measure: its slot is its own 36.
    CHECK(scene.items[4].rect == Rect{0, 96, 48, 36});

    // No gap said is no gap.
    std::vector<Element> tight = authored;
    tight[0].gap.reset();
    const Scene close = arrange(tight, Room{576, 240, 1}, measures);
    CHECK(close.items[2].rect.y == 24 + 60);
}

TEST_CASE("composition has no depth ceiling, and nothing here recurses to find out") {
    // The point is not the number. It is that no constant anywhere says how deep a chain may be,
    // and that the work happens on the heap -- so what a document may express is not decided by
    // how much C stack the host has.
    constexpr std::int64_t kDeep = 4096;
    std::vector<Element> authored;
    authored.reserve(static_cast<std::size_t>(kDeep));
    authored.push_back(make(1, "root-most", at(12, 12), share(kShareWhole), share(kShareWhole)));
    for (std::int64_t i = 2; i <= kDeep; ++i) {
        authored.push_back(make(i, "link", at(12, 0), px(36), px(12), i - 1));
    }

    const Scene scene = arrange(authored, Room{576, 192, 1});
    CHECK(scene.items.size() == static_cast<std::size_t>(kDeep));

    // Each link is twelve pixels right of the one it stands in, so the last one has accumulated
    // the whole chain -- which is the composed frame arriving intact at the far end.
    const Placed* last = placed_for(scene, kDeep);
    REQUIRE(last != nullptr);
    CHECK(last->rect.x == 12 * kDeep);
    CHECK(last->rect.y == 12);

    // ...and in document order, at the end, where it was authored.
    CHECK(scene.items.back().id == kDeep);

    // The same chain built BACKWARDS in the document solves identically: a relationship is an
    // identity, not a position, so the order it was written in is not part of its meaning.
    std::vector<Element> reversed(authored.rbegin(), authored.rend());
    const Scene other = arrange(reversed, Room{576, 192, 1});
    CHECK(other.items.size() == static_cast<std::size_t>(kDeep));
    REQUIRE(placed_for(other, kDeep) != nullptr);
    CHECK(placed_for(other, kDeep)->rect == last->rect);
    CHECK(other.items.front().id == kDeep); // document order, which is now reversed

    // A chain of frames that each fit their one part is as deep, and as flat to solve.
    std::vector<Element> fitting;
    for (std::int64_t i = 1; i <= kDeep; ++i) {
        fitting.push_back(make(i, "fits", at(1, 0), word(kSizeFit), px(12), i - 1));
    }
    fitting.back().width = px(10);
    const Scene fit = arrange(fitting, Room{576, 192, 1});
    REQUIRE(placed_for(fit, 1) != nullptr);
    CHECK(placed_for(fit, 1)->rect.w == (kDeep - 1) + 10);
}

TEST_CASE("a chain that never reaches the root places nothing, and never guesses the root") {
    // An application that checks its sequence (with `problem`) refuses both faults before they
    // are authored, so they arrive as every hostile value here does: through a poke, or from an
    // application with no such check. The answer is an ABSENCE -- falling back to the root would
    // solve the part against a DIFFERENT relationship than it names and draw a confident
    // rectangle in the wrong place.
    SUBCASE("a source that does not exist") {
        const std::vector<Element> authored{
            make(1, "A", at(36, 36), px(48), px(48)),
            make(2, "B", at(0, 0), px(48), px(48), 999),
        };
        const Scene scene = arrange(authored, Room{576, 192, 1});
        REQUIRE(scene.items.size() == 1);
        CHECK(scene.items[0].id == 1);
        CHECK(placed_for(scene, 2) == nullptr);
        CHECK(hit(scene, 0, 0) == nullptr); // not sitting at the root's origin
    }
    SUBCASE("an object measured against itself") {
        const std::vector<Element> authored{make(1, "self", at(0, 0), px(48), px(48), 1)};
        CHECK(arrange(authored, Room{576, 192, 1}).items.empty());
    }
    SUBCASE("two objects measured against each other") {
        const std::vector<Element> authored{
            make(1, "A", at(0, 0), px(48), px(48), 2),
            make(2, "B", at(0, 0), px(48), px(48), 1),
        };
        CHECK(arrange(authored, Room{576, 192, 1}).items.empty());
    }
    SUBCASE("a longer loop, and the innocent objects hanging off it") {
        const std::vector<Element> authored{
            make(1, "A", at(0, 0), px(48), px(48), 3), // A -> C
            make(2, "B", at(0, 0), px(48), px(48), 1), // B -> A
            make(3, "C", at(0, 0), px(48), px(48), 2), // C -> B, closing it
            make(4, "hangs off C", at(0, 0), px(48), px(48), 3), // merely FEEDS the loop
            make(5, "innocent", at(84, 84), px(48), px(48)),
        };
        const Scene scene = arrange(authored, Room{576, 192, 1});
        // Everything that cannot reach the root is absent -- including #4, which is in no
        // cycle itself but has no frame either.
        REQUIRE(scene.items.size() == 1);
        CHECK(scene.items[0].id == 5);
        CHECK(scene.items[0].rect == Rect{84, 84, 48, 48});
    }
}

TEST_CASE("one walk answers what a chain does, and it is the walk a law can ask") {
    std::vector<Element> authored{
        make(1, "A", Place{}, px(48), px(48)),
        make(2, "B", Place{}, px(48), px(48), 1), // B -> A
        make(3, "C", Place{}, px(48), px(48), 2), // C -> B
    };
    const ById index(authored);

    SUBCASE("it reaches the root, and reports what it passed through") {
        const ContextWalk walk = walk_context(authored, index, 3);
        CHECK(walk.reaches_root());
        CHECK(walk.chain == std::vector<std::int64_t>{3, 2, 1});
        // ...which is exactly the question an authoring operation asks before it writes a
        // relationship: would this close a loop?
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
        // One lap, closed: the diagnostic is a loop a person can read, and it does not grow with
        // how far the walk had travelled first.
        CHECK(walk.chain.size() == 4);
        CHECK(walk.chain.front() == walk.chain.back());
    }
    SUBCASE("the memo shortens the work, not the verdict") {
        std::vector<char> settled(authored.size(), 0);
        const ContextWalk first = walk_context(authored, index, 3, &settled);
        CHECK(first.reaches_root());
        CHECK(first.chain.size() == 3);
        // A, B and C are now known to reach the root, so asking again stops at once -- and still
        // answers Root. That is what turns a whole-document check from one walk per part into
        // one visit per part.
        const ContextWalk again = walk_context(authored, index, 3, &settled);
        CHECK(again.reaches_root());
        CHECK(again.chain.empty());
    }
}

TEST_CASE("an identity is found by identity, and a repeated one answers with the first") {
    const std::vector<Element> authored{
        make(5, "first", Place{}, px(12), px(12)),
        make(9, "other", Place{}, px(12), px(12)),
        make(5, "second", Place{}, px(12), px(12)),
    };
    const ById index(authored);
    CHECK(index.find(9) == 1);
    CHECK(index.find(404) == ById::npos);
    CHECK(index.find(kRootContext) == ById::npos); // the root is not a part

    // Distinctness is a DOCUMENT law and this is a vocabulary, so a repeated identity is
    // representable here and gets a stated answer rather than an assumption: the first, which is
    // what a linear search would have said.
    CHECK(index.find(5) == 0);
}

TEST_CASE("the frame an element was read in is asked of the resolver, not reconstructed") {
    const std::vector<Element> authored{
        make(1, "A", at(48, 36), px(240), px(96)),
        make(2, "B", at(12, 12), px(48), px(24), 1),
        make(3, "broken", Place{}, px(48), px(24), 404),
    };
    const Scene scene = arrange(authored, Room{576, 192, 1});

    CHECK(frame_in(scene, authored[0]) == Rect{0, 0, 576, 192}); // the root's
    CHECK(frame_in(scene, authored[1]) == Rect{48, 36, 240, 96}); // A's, as solved
    // A part the scene did not place has no frame either, and says so with an empty one rather
    // than quietly handing back the root.
    CHECK(frame_in(scene, authored[2]) == Rect{});
}

TEST_CASE("composed placement is TOTAL over coordinates no setter would have produced") {
    // A solved position is `the frame's origin + the authored offset`, and both terms come off a
    // ZEN_SHAPE -- so a poke can write the pair that makes `a + b` signed overflow, which is
    // undefined behaviour produced by data. Only a sanitizer can see the difference; these
    // assertions are about the ANSWERS.
    CHECK(add_px(kMax, 1) == kMax);
    CHECK(add_px(kMin, -1) == kMin);
    CHECK(add_px(kMax, kMax) == kMax);
    CHECK(add_px(kMin, kMin) == kMin);
    CHECK(add_px(3, -5) == -2);
    CHECK(add_px(kMax, 0) == kMax);
    CHECK(sub_px(kMin, 1) == kMin);
    CHECK(sub_px(kMax, -1) == kMax);
    CHECK(sub_px(0, kMin) == kMax);
    CHECK(sub_px(3, 5) == -2);

    const std::vector<Element> authored{
        make(1, "far", at(kMax - 1, 0), px(48), px(48)),
        make(2, "further", at(1200, 0), px(48), px(48), 1),
    };
    const Scene scene = arrange(authored, Room{576, 192, 1});
    REQUIRE(scene.items.size() == 2);
    CHECK(scene.items[1].rect.x == kMax); // saturated, far outside any room
    CHECK(hit(scene, 0, 0) == nullptr);
}

TEST_CASE("a dependent may extend past its source: this vocabulary does not clip") {
    // Recorded as a decision. A frame supplies an origin and a span; it does not impose a
    // boundary, and nothing here trims a rectangle to its source. An application that wants
    // clipping is welcome to it -- that is a PAINTING policy, and it would live wherever the
    // painting does.
    const std::vector<Element> authored{
        make(1, "small", at(60, 60), px(48), px(24)),
        make(2, "spills", at(-36, -24), px(360), px(108), 1),
    };

    const Scene scene = arrange(authored, Room{576, 192, 1});
    REQUIRE(scene.items.size() == 2);
    CHECK(scene.items[1].rect == Rect{24, 36, 360, 108});

    // ...and it is hittable everywhere it is, including well outside the rectangle that gave it
    // its frame.
    REQUIRE(hit(scene, 360, 120) != nullptr);
    CHECK(hit(scene, 360, 120)->id == 2);
    REQUIRE(hit(scene, 24, 36) != nullptr);
    CHECK(hit(scene, 24, 36)->id == 2);
}

// ============================================================================
// 5 — the intent's own problems, asked with no room
// ============================================================================

TEST_CASE("problem names a share in a frame that fits, a missing context and a looping one") {
    const Element fits = make(1, "panel", at(0, 0), word(kSizeFit), word(kSizeFit));

    SUBCASE("a share of a frame that fits its parts depends on itself") {
        const std::string said =
            problem({fits, make(2, "bar", Place{}, share(5000), px(12), 1)});
        CHECK(said == "#2 \"bar\" is a share of the width of #1 \"panel\", which fits its width "
                      "to its parts: give #1 \"panel\" a width");
    }
    SUBCASE("so does a fill, and a place counted from the far edge or the centre") {
        CHECK(problem({fits, make(2, "", Place{}, px(12), word(kSizeFill), 1)}) ==
              "#2 fills the height of #1 \"panel\", which fits its height to its parts: give #1 "
              "\"panel\" a height");
        CHECK(problem({fits, make(2, "", Place{kPlaceRight, 0, {}}, px(12), px(12), 1)})
                  .find("#2 stands at the right of #1") == 0);
        CHECK(problem({fits, make(2, "", Place{kPlaceBottom, {}, 0}, px(12), px(12), 1)})
                  .find("#2 is centred across #1") == 0);
    }
    SUBCASE("and a share place, which reads the frame's span to stand") {
        // ⚔ MUTATION: `reads_of_frame` reading a share place as its anchor alone -- a share
        // place in a frame that fits reads as standing from the start, and nothing is named.
        CHECK(problem({fits, make(2, "bar", Place{kPlaceShare, 0, 3333}, px(12), px(12), 1)}) ==
              "#2 \"bar\" stands a share of the width into #1 \"panel\", which fits its width to "
              "its parts: give #1 \"panel\" a width");
    }
    SUBCASE("a context no part carries") {
        CHECK(problem({fits, make(2, "lost", Place{}, px(12), px(12), 99)}) ==
              "#2 \"lost\" stands in #99, which no part carries");
    }
    SUBCASE("a context that loops") {
        const std::string said = problem({
            make(4, "a", Place{}, px(12), px(12), 5),
            make(5, "b", Place{}, px(12), px(12), 4),
        });
        CHECK(said.find("#4 \"a\" stands in a loop of frames, #") == 0);
        CHECK(said.find(", which never reaches the room") != std::string::npos);
    }
    SUBCASE("a column that begins again and fits its own height") {
        Element column = make(1, "column", Place{}, px(120), word(kSizeFit));
        column.flow = kFlowStack;
        CHECK(problem({column, make(2, "", Place{}, px(12), px(12), 1)}) ==
              "#1 \"column\" begins its column again at its floor, and fits its height to its "
              "parts: give it a height");
        // With no parts it begins nothing again.
        CHECK(problem({column}).empty());
    }
}

TEST_CASE("problem is the intent alone, and finds nothing in a sequence with none") {
    // A frame that fits parts placed from its start, a share of a frame with a size, and every
    // place in the root: nothing depends on itself.
    CHECK(problem({}).empty());
    CHECK(problem({
                      make(1, "panel", at(0, 0), word(kSizeFit), word(kSizeFit)),
                      make(2, "a", at(4, 4), px(12), word(kSizeFit), 1),
                      make(3, "b", Place{kPlaceBottom, {}, 0}, share(5000), word(kSizeFill)),
                      make(4, "c", Place{kPlaceCenter, {}, {}}, share(5000), px(12), 3),
                  })
              .empty());
    // The first problem in authored order is the one said.
    CHECK(problem({
                      make(1, "lost", Place{}, px(12), px(12), 99),
                      make(2, "lost too", Place{}, px(12), px(12), 98),
                  })
              .find("#1") == 0);
}

TEST_CASE("a part that reads a frame fitting it is solved as fit, and the solve never refuses") {
    // `problem` refuses this intent; `arrange` is total and solves whatever reaches it: the
    // part is read as `fit`, its measure where one is said, and its anchor from the frame's
    // start, so the frame is the reach of what the part says of itself.
    const std::vector<Element> authored{
        make(1, "fits", at(24, 0), word(kSizeFit), px(48)),
        make(2, "share", Place{kPlaceRight, 10, {}}, share(5000), px(12), 1),
        make(3, "fill", at(0, 12), word(kSizeFill), px(12), 1),
    };
    const Scene measured =
        arrange(authored, Room{576, 192, 1}, {{}, Measure{50, {}}, Measure{30, {}}});
    REQUIRE(measured.items.size() == 3);
    CHECK(measured.items[0].rect == Rect{24, 0, 60, 48});
    // Down, the frame has a height, so `right` centres it there as it would anywhere.
    CHECK(measured.items[1].rect == Rect{34, 18, 50, 12});
    CHECK(measured.items[2].rect == Rect{24, 12, 30, 12});

    // Unmeasured, each is the reach of its own parts, which is none.
    const Scene bare = arrange(authored, Room{576, 192, 1});
    REQUIRE(bare.items.size() == 3);
    CHECK(bare.items[0].rect == Rect{24, 0, 10, 48});
    CHECK(bare.items[1].rect.w == 0);
}

} // TEST_SUITE
