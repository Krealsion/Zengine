// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#include "doctest.h"
#include "flow/shape.hpp"
#include "maker_fixture.hpp"
#include "view/description.hpp"

#include <algorithm>
#include <string>
#include <vector>

namespace {
namespace view = zengine::view;
namespace shape = zengine::flow::shape;

/// `tally.panel.Count`, made through the one shape model as the View Builder makes it.
std::shared_ptr<const loom::Schema> count_shape() {
    auto made = shape::make(shape::qualified("tally.panel", "Count"));
    for (const char* f : {"start", "limit", "step"})
        made = shape::with_field(*made, f, loom::type_of(loom::Kind::Int), true);
    return made;
}

std::shared_ptr<const loom::Schema> total_shape() {
    return loom::SchemaBuilder("tally.Total", 1).field("total", loom::Kind::Int).build();
}

/// The tally panel: three number fields, a button saying `tally.panel.Count`, and a label;
/// `bound` shows `tally.Total.total` on it.
view::Description panel(bool bound = true, std::string total_label = "Total") {
    view::Description d;
    d.name = "tally.panel";
    d.elements = {{"start", view::Kind::number, "start", 0, 0, 144, 24, "0"},
                  {"limit", view::Kind::number, "limit", 0, 28, 144, 24, "10"},
                  {"step", view::Kind::number, "step", 0, 56, 144, 24, "1"},
                  {"count", view::Kind::button, "Count", 0, 84, 96, 24, ""},
                  {"total", view::Kind::label, total_label, 0, 112, 192, 24, ""}};
    d.intents = {{"count", count_shape(), {{"start", "start"}, {"limit", "limit"}, {"step", "step"}}}};
    if (bound) d.shows = {{"total", total_shape(), "total"}};
    return d;
}

bool has(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }
} // namespace

TEST_CASE("a view description saves and reads back whole, and another version is refused by its number") {
    const auto d = panel();
    const auto read = view::read_description(view::description_bytes(d));
    REQUIRE_MESSAGE(read.ok, read.reason);
    CHECK(read.description.name == "tally.panel");
    REQUIRE(read.description.elements.size() == 5);
    CHECK(read.description.elements[2].text == "1");
    CHECK(read.description.elements[4].y == 112);
    REQUIRE(read.description.intents.size() == 1);
    CHECK(loom::same_identity(*read.description.intents[0].shape, *hwfix::count_schema()));
    REQUIRE(read.description.shows.size() == 1);
    CHECK(loom::same_identity(*read.description.shows[0].shape, *total_shape()));
    CHECK(view::same_shapes(d, read.description));

    // The same fields under a newer envelope: refused by the number it claims, before a field.
    auto later = loom::SchemaBuilder("zengine.view.Description", 2).field("format", loom::Kind::Text).build();
    loom::Value v(later);
    v.set("format", loom::Cell::text(view::kFormat));
    const auto refused = view::read_description(loom::serialize(v));
    CHECK_FALSE(refused.ok);
    CHECK(has(refused.reason, "a view description of version 2; this build reads version 1"));
    CHECK(has(view::read_description("not a value").reason, "not a Zen value"));
}

TEST_CASE("a description that breaks a rule is refused in words, and none is written") {
    auto outside = panel();
    outside.intents[0].shape = shape::make("other.Count", count_shape()->fields());
    CHECK(has(view::problem(outside), "is outside the view's name; its name must begin `tally.panel.`"));
    CHECK_THROWS_WITH_AS(view::description_bytes(outside), doctest::Contains("outside the view's name"),
                         std::invalid_argument);

    auto from_label = panel();
    from_label.intents[0].fields[0].element = "total";
    CHECK(has(view::problem(from_label), "comes from `total`, which is no number field"));

    auto shown_by_button = panel();
    shown_by_button.shows[0].element = "count";
    CHECK(has(view::problem(shown_by_button), "only a label shows a field"));

    auto missing_field = panel();
    missing_field.shows[0].field = "sum";
    CHECK(has(view::problem(missing_field), "shows `sum`, which `tally.Total v1` does not declare"));

    auto twice = panel();
    twice.elements[1].id = "start";
    CHECK(has(view::problem(twice), "two elements are both `start`"));

    auto named = panel();
    named.name = "tally panel";
    CHECK(has(view::problem(named), "`tally panel` is not"));

    auto placed = panel();
    placed.elements[0].w = 0;
    CHECK(has(view::problem(placed), "sits at a place and a size of whole pixels"));
    CHECK(view::problem(panel()).empty());
}
