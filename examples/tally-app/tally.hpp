// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_EXAMPLES_TALLY_APP_TALLY_HPP
#define ZENGINE_EXAMPLES_TALLY_APP_TALLY_HPP

// The tally app's two files, as values (examples/tally-app/README.md): the tally panel the view
// suite runs (`tests/test_view.cpp`) and the tally flow the maker suites fold
// (`tests/maker_fixture.hpp`), its state starting at 0. `fixture.cpp` writes them; the `app_host`
// suite reads the committed files against them byte for byte.

#include "tests/maker_fixture.hpp"

#include "flow/project.hpp"
#include "flow/shape.hpp"
#include "operator/catalog.hpp"
#include "view/description.hpp"

#include <zen/schema.hpp>
#include <zen/value.hpp>

#include <memory>
#include <string>

namespace zengine::examples::tally_app {

/// `tally.panel.Count`, made through the one shape model as the View Builder makes it.
inline std::shared_ptr<const loom::Schema> count_shape() {
    namespace shape = zengine::flow::shape;
    auto made = shape::make(shape::qualified("tally.panel", "Count"));
    for (const char* f : {"start", "limit", "step"}) {
        made = shape::with_field(*made, f, loom::type_of(loom::Kind::Int), true);
    }
    return made;
}

inline std::shared_ptr<const loom::Schema> total_shape() {
    return loom::SchemaBuilder("tally.Total", 1).field("total", loom::Kind::Int).build();
}

/// The tally panel: three number fields starting at 0, 10 and 1, a button saying
/// `tally.panel.Count`, and a label showing `tally.Total.total`, in a view 480 by 192.
inline zengine::view::Description panel() {
    namespace view = zengine::view;
    view::Description d;
    d.name = "tally.panel";
    d.width = 480;
    d.height = 192;
    d.elements = {{"start", view::Kind::number, "start", 0, 0, 144, 24, "0"},
                  {"limit", view::Kind::number, "limit", 0, 28, 144, 24, "10"},
                  {"step", view::Kind::number, "step", 0, 56, 144, 24, "1"},
                  {"count", view::Kind::button, "Count", 0, 84, 96, 24, ""},
                  {"total", view::Kind::label, "Total", 0, 112, 192, 24, ""}};
    d.intents = {{"count", count_shape(), {{"start", "start"}, {"limit", "limit"}, {"step", "step"}}}};
    d.shows = {{"total", total_shape(), "total"}};
    return d;
}

/// The tally flow: the definition `tally`, folding `math.add` from `catalog`, and its state
/// `tally.State{total: 0}`.
inline zengine::flow::Project project(const zengine::op::Catalog& catalog) {
    zengine::maker::Definition d = hwfix::tally(catalog);
    loom::Value state(d.state);
    state.set("total", loom::Cell::integer(0));
    return zengine::flow::make_project(std::move(d), std::move(state));
}

} // namespace zengine::examples::tally_app

#endif // ZENGINE_EXAMPLES_TALLY_APP_TALLY_HPP
