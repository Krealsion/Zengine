// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#include "doctest.h"
#include "flow-pane/view.hpp"
#include "workshop/pane_canvas.hpp"

namespace {
namespace pane = zengine::flow_pane;
namespace ws = zengine::workshop;
namespace flow = zengine::flow;
constexpr auto unit = pane::unit;

ws::PaneCanvasRoom room() { return {"flow", 7, 100 * unit, 45 * unit, 4, true}; }
pane::Model graph(std::size_t nodes) {
    pane::Model model;
    flow::Ports signature{"view.Rule",
        loom::SchemaBuilder("view.In", 1).field("a", loom::Kind::Int).build(),
        loom::SchemaBuilder("view.Out", 1).field("value", loom::Kind::Int).build()};
    model.palette.push_back(signature);
    model.command("state-field", {"total", "Int", "required"});
    model.command("message", {"Input"});
    model.command("trigger", {"0", "total"});
    for (std::size_t i = 0; i < nodes; ++i)
        model.workspace.graph.add(0, signature);
    return model;
}
bool text_has(const pane::Picture& picture, std::string_view part) {
    return std::any_of(picture.content.labels.begin(), picture.content.labels.end(),
        [&](const auto& label) { return label.text.find(part) != std::string::npos; });
}
bool action_has(const pane::Picture& picture, std::string_view action) {
    return std::any_of(picture.hits.begin(), picture.hits.end(),
        [&](const auto& hit) { return hit.action == action; });
}
}

TEST_CASE("Flow clips a large offscreen graph before spending the canvas budget") {
    auto model = graph(800);
    for (std::size_t i = 1; i < model.workspace.graph.places.size(); ++i)
        model.workspace.graph.places[i].y = 500000;
    const auto view = pane::picture(model, room(), 1);
    CHECK(ws::canvas_content_problem(view.content).empty());
    CHECK(text_has(view, "%0 view.Rule"));
    CHECK_FALSE(text_has(view, "View limit reached"));
    CHECK(action_has(view, "node"));
    CHECK(std::none_of(view.content.labels.begin(), view.content.labels.end(),
                      [](const auto& l) { return l.text.empty(); }));
    CHECK(model.workspace.graph.project.definition.on.at(0).body.nodes.size() == 800);
}

TEST_CASE("Flow shows an actionable bounded picture instead of a partial dense graph") {
    auto model = graph(1100);
    for (auto& place : model.workspace.graph.places) { place.x = 24 * unit; place.y = 3 * unit; }
    const auto view = pane::picture(model, room(), 2);
    CHECK(ws::canvas_content_problem(view.content).empty());
    CHECK(text_has(view, "View limit reached"));
    CHECK(text_has(view, "Draft intact"));
    CHECK(action_has(view, "ask-save"));
    CHECK(action_has(view, "ask-export"));
    CHECK(action_has(view, "zoom-in"));
    CHECK_FALSE(action_has(view, "node"));
    CHECK_FALSE(action_has(view, "port"));
    CHECK(model.workspace.graph.project.definition.on.at(0).body.nodes.size() == 1100);
}

TEST_CASE("Flow keeps a long dialog draft editable without rejecting its canvas") {
    pane::Model model;
    const std::string contents(4096, 'x');
    model.ask("Edit a long value", "value", {{"Text", contents}});
    const auto view = pane::picture(model, room(), 3);
    CHECK(ws::canvas_content_problem(view.content).empty());
    CHECK(action_has(view, "dialog-confirm"));
    REQUIRE(model.dialog);
    CHECK(model.dialog->entries.at(0).text.text() == contents);
    CHECK(std::any_of(view.content.rects.begin(), view.content.rects.end(),
        [](const auto& r) { return r.h == 4 && r.role == zengine::surface::role::kAccent; }));
}

TEST_CASE("Flow marks shortened observations and retains their complete source") {
    pane::Model model;
    model.page = pane::Page::Events;
    model.events.push_back(std::string(5000, 'x'));
    const auto view = pane::picture(model, room(), 4);
    CHECK(ws::canvas_content_problem(view.content).empty());
    CHECK(text_has(view, "..."));
    CHECK(model.events.at(0).size() == 5000);
    for (const auto& label : view.content.labels) {
        CHECK(label.x >= 0);
        CHECK(label.x + static_cast<std::int64_t>(label.text.size()) * unit <= room().width);
    }
}

TEST_CASE("Flow pointer hits use the medium grain at subpixel and half-cell edges") {
    pane::Picture view;
    view.hits.push_back({3, 3, 49, 49, "pixel-edge", {}, 0});
    view.grain = 4;
    REQUIRE(view.hit(0, 0));
    CHECK(view.hit(0, 0)->action == "pixel-edge");
    CHECK(view.hit(48, 48) != nullptr);
    CHECK(view.hit(52, 48) == nullptr);
    view.hits = {{24, 24, 96, 96, "cell-edge", {}, 0}};
    view.grain = unit;
    CHECK(view.hit(0, 0) != nullptr);
    CHECK(view.hit(unit, unit) != nullptr);
    CHECK(view.hit(2 * unit, unit) == nullptr);
}

TEST_CASE("Flow clips content and hit maps to the same narrow pane body") {
    auto model = graph(1);
    auto narrow = room(); narrow.width = 15 * unit; narrow.height = 10 * unit;
    const auto view = pane::picture(model, narrow, 5);
    CHECK(ws::canvas_content_problem(view.content).empty());
    for (const auto& hit : view.hits) {
        CHECK(hit.x >= 0);
        CHECK(hit.y >= 0);
        CHECK(hit.x + hit.w <= narrow.width);
        CHECK(hit.y + hit.h <= narrow.height);
    }
    CHECK(view.hit(narrow.width, 0) == nullptr);
    CHECK(view.hit(0, narrow.height) == nullptr);
}

TEST_CASE("Flow keeps its catalog rail and viewport controls separate from node gestures") {
    auto model = graph(1);
    model.palette.push_back({"very.long.provider.name.that.must.not.paint.over.the.graph", {}, {}});
    const auto view = pane::picture(model, room(), 6);
    for (const auto& hit : view.hits) {
        if (hit.action == "add-node" || hit.action == "source-field" || hit.action == "select-trigger")
            CHECK(hit.x + hit.w <= 22 * unit);
    }
    const pane::Hit* reset = nullptr;
    const pane::Hit* minus = nullptr;
    const pane::Hit* plus = nullptr;
    for (const auto& hit : view.hits) {
        if (hit.action == "fit") reset = &hit;
        if (hit.action == "zoom-out") minus = &hit;
        if (hit.action == "zoom-in") plus = &hit;
    }
    REQUIRE(reset); REQUIRE(minus); REQUIRE(plus);
    CHECK(reset->x + reset->w <= minus->x);
    CHECK(minus->x + minus->w <= plus->x);
}
