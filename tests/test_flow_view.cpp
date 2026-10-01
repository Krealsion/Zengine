// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#include "doctest.h"
#include "flow-pane/view.hpp"
#include "workshop/pane_canvas.hpp"
#include "workshop/screen_canvas.hpp"
#include "surface/skin_sdl_plan.hpp"

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
    return std::any_of(picture.content.texts.begin(), picture.content.texts.end(),
        [&](const auto& label) { return label.text.find(part) != std::string::npos; });
}
bool action_has(const pane::Picture& picture, std::string_view action) {
    return std::any_of(picture.hits.begin(), picture.hits.end(),
        [&](const auto& hit) { return hit.action == action; });
}
/// One row of the discovery door's answer, as the door would read it off a contribution.
ws::PowerRow found_row(std::string identity, std::string kind) {
    ws::PowerRow row;
    row.identity = std::move(identity);
    row.kind = std::move(kind);
    row.construction = ws::kNativeConstruction;
    row.provider = "view.provider";
    row.about = "what this power is for";
    row.signature = "(a: Int) -> value: Int";
    return row;
}
void answered(pane::Model& model, std::vector<ws::PowerRow> rows) {
    model.discovered = ws::PowersFound{};
    model.discovered.ok = true;
    model.discovered.total = static_cast<std::int64_t>(rows.size());
    model.discovered.rows = std::move(rows);
    model.discovered_read = true;
}
std::int64_t row_of(const pane::Picture& picture, std::string_view text) {
    for (const auto& label : picture.content.texts)
        if (label.text == text) return label.y;
    return -1;
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
    CHECK(std::none_of(view.content.texts.begin(), view.content.texts.end(),
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
    CHECK(std::any_of(view.content.texts.begin(), view.content.texts.end(),
        [](const auto& text) { return text.caret_col >= 0; }));
}

TEST_CASE("Flow marks shortened observations and retains their complete source") {
    pane::Model model;
    model.page = pane::Page::Events;
    model.events.push_back(std::string(5000, 'x'));
    const auto view = pane::picture(model, room(), 4);
    CHECK(ws::canvas_content_problem(view.content).empty());
    CHECK(text_has(view, "..."));
    CHECK(model.events.at(0).size() == 5000);
    for (const auto& label : view.content.texts) {
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

TEST_CASE("Flow keeps its discovery rail and viewport controls separate from node gestures") {
    auto model = graph(1);
    std::vector<ws::PowerRow> rows{
        found_row("very.long.provider.name.that.must.not.paint.over.the.graph", ws::kOperatorKind)};
    answered(model, rows);
    const auto view = pane::picture(model, room(), 6);
    REQUIRE(action_has(view, "found"));
    for (const auto& hit : view.hits) {
        if (hit.action == "found" || hit.action == "source-field" || hit.action == "select-trigger")
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

    // A fractional final body row must not paint over the footer after a resize -- the defect
    // this case guards, on a rail with actions and on an observation page without them.
    for (int i = 0; i < 40; ++i) {
        rows.push_back(found_row("palette." + std::to_string(i), ws::kOperatorKind));
        model.events.push_back("event " + std::to_string(i));
    }
    answered(model, rows);
    auto shorter = room();
    shorter.text_advance_px = 8; shorter.text_line_px = 18;
    shorter.width = 150 * 32; shorter.height = 23 * 88 + 44;
    const pane::GridProjection grid(shorter);
    const auto footer = grid.y(grid.grid_y(shorter.height) - 2 * unit);
    const auto rail = pane::picture(model, shorter, 7);
    bool palette_visible = false;
    for (const auto& hit : rail.hits) {
        if (hit.action != "found") continue;
        palette_visible = true;
        CHECK(hit.y + hit.h <= footer);
    }
    REQUIRE(palette_visible);
    for (const auto& text : rail.content.texts) {
        if (text.text.find("  palette.") == std::string::npos) continue;
        const auto layout = ws::clip_canvas_text(text,
            {0, 0, shorter.width, shorter.height}, shorter);
        REQUIRE(layout.visible());
        CHECK(layout.bounds.y + layout.bounds.h <= footer);
    }
    model.page = pane::Page::Events;
    const auto events = pane::picture(model, shorter, 8);
    bool event_visible = false;
    for (const auto& text : events.content.texts) {
        if (!text.text.starts_with("event ")) continue;
        event_visible = true;
        const auto layout = ws::clip_canvas_text(text,
            {0, 0, shorter.width, shorter.height}, shorter);
        REQUIRE(layout.visible());
        CHECK(layout.bounds.y + layout.bounds.h <= footer);
    }
    REQUIRE(event_visible);
}

TEST_CASE("Flow projects native text and hit regions through independent measured axes") {
    auto model = graph(1);
    const auto saved = flow::workspace_bytes(model.workspace);
    const auto cell_view = pane::picture(model, room(), 1);
    auto native_room = room();
    native_room.text_advance_px = 9;
    native_room.text_line_px = 18;
    native_room.width = 100 * 36;
    native_room.height = 45 * 88;
    const auto view = pane::picture(model, native_room, 2);
    CHECK(ws::canvas_content_problem(view.content).empty());
    CHECK(view.content.labels.empty());
    REQUIRE_FALSE(view.content.texts.empty());
    REQUIRE_FALSE(view.content.rects.empty());
    CHECK(view.content.rects.front().role == zengine::surface::role::kGround);
    CHECK(view.content.rects.front().w == native_room.width);
    CHECK(view.content.rects.front().h == native_room.height);
    REQUIRE(action_has(view, "node"));
    const auto node = std::find_if(view.hits.begin(), view.hits.end(),
        [](const auto& hit) { return hit.action == "node"; });
    const auto cell_node = std::find_if(cell_view.hits.begin(), cell_view.hits.end(),
        [](const auto& hit) { return hit.action == "node"; });
    REQUIRE(cell_node != cell_view.hits.end());
    CHECK(node->x == cell_node->x * 36 / unit);
    CHECK(node->y == cell_node->y * 88 / unit);
    CHECK(node->w == cell_node->w * 36 / unit);
    CHECK(node->h == 88);
    CHECK(view.hit(node->x + 8, node->y + 8)->action == "node");
    zengine::surface::SurfaceLayer layer;
    ws::paint_pane_canvas(layer, {0, 0, native_room.width, native_room.height},
        view.content, native_room.text_advance_px, native_room.text_line_px, native_room.grain);
    const auto quads = zengine::surface::plan_layer_quads(layer, 100, 100,
        zengine::surface::SurfaceExtent{0, 0, 9, 18, 12});
    REQUIRE_FALSE(quads.empty());
    CHECK(quads.front().r == 0);
    CHECK(quads.front().g == 0);
    CHECK(quads.front().b == 0);
    const auto px = node->x / 4, py = node->y / 4, pw = node->w / 4;
    // One input yields a four-row box, and all four authored edges must survive the actual SDL
    // plan: a subpixel vertical strip that vanishes here is the defect this case guards.
    const auto edge = [&](std::int64_t x, std::int64_t y, std::int64_t w, std::int64_t h) {
        return std::any_of(quads.begin(), quads.end(), [&](const auto& quad) {
            return quad.x == x && quad.y == y && quad.w >= w && quad.h >= h;
        });
    };
    CHECK(edge(px, py, pw, 1));
    CHECK(edge(px, py + 4 * 22, pw, 1));
    CHECK(edge(px, py, 1, 4 * 22));
    CHECK(edge(px + pw, py, 1, 4 * 22));
    for (const auto& text : view.content.texts) {
        const auto layout = ws::clip_canvas_text(text,
            {0, 0, native_room.width, native_room.height}, native_room);
        REQUIRE(layout.visible());
        CHECK(layout.text.text == text.text);
        CHECK(layout.bounds.x >= 0);
        CHECK(layout.bounds.y >= 0);
        CHECK(layout.bounds.x + layout.bounds.w <= native_room.width);
        CHECK(layout.bounds.y + layout.bounds.h <= native_room.height);
    }
    CHECK(flow::workspace_bytes(model.workspace) == saved);
    model.workspace.graph.places.front().x = 20 * unit;
    const auto clipped = pane::picture(model, native_room, 3);
    CHECK(ws::canvas_content_problem(clipped.content).empty());
    CHECK_FALSE(text_has(clipped, "%0 view.Rule"));
    CHECK(text_has(clipped, "...ew.Rule"));
    const auto clipped_node = std::find_if(clipped.hits.begin(), clipped.hits.end(),
        [](const auto& hit) { return hit.action == "node"; });
    REQUIRE(clipped_node != clipped.hits.end());
    CHECK(clipped_node->x == 22 * 36);

    auto extreme = native_room;
    extreme.text_advance_px = (std::numeric_limits<std::int64_t>::max)();
    extreme.text_line_px = (std::numeric_limits<std::int64_t>::max)();
    const auto bounded = pane::picture(model, extreme, 4);
    CHECK(ws::canvas_content_problem(bounded.content).empty());
    CHECK_FALSE(action_has(bounded, "node"));

    model.ask("Edit text", "value", {{"Value", "selected"}});
    model.dialog->entries.front().text.set("selected", 0);
    model.dialog->entries.front().text.drag_to_column(8);
    const auto dialog = pane::picture(model, native_room, 3);
    const auto field = std::find_if(dialog.content.texts.begin(), dialog.content.texts.end(),
        [](const auto& text) { return text.caret_col >= 0; });
    REQUIRE(field != dialog.content.texts.end());
    CHECK(field->sel_end_col - field->sel_begin_col == 8);
    CHECK(field->caret_col == field->sel_end_col);
}

TEST_CASE("Flow's rail is called In scope, and the door's rows are grouped by classification") {
    auto model = graph(1);
    // THE ANSWER COMES IN THE CATALOG'S ORDER; the rail groups it in the order a maker reaches for
    // it, reading each row's classification as the door gave it.
    answered(model, {found_row("view.edge", ws::kConversionKind),
                     found_row("view.op", ws::kOperatorKind), found_row("view.src", ws::kSourceKind)});
    const auto view = pane::picture(model, room(), 9);
    CHECK(row_of(view, "In scope") >= 0);
    CHECK_FALSE(text_has(view, "(click to wire)"));
    const auto sources = row_of(view, "Sources"), operators = row_of(view, "Operators"),
               conversions = row_of(view, "Conversions");
    REQUIRE(sources >= 0);
    REQUIRE(operators >= 0);
    REQUIRE(conversions >= 0);
    CHECK(sources < row_of(view, "  view.src"));
    CHECK(row_of(view, "  view.src") < operators);
    CHECK(operators < row_of(view, "  view.op"));
    CHECK(row_of(view, "  view.op") < conversions);
    CHECK(conversions < row_of(view, "  view.edge"));

    // A PREVIEW IS THE ROW EXPANDED BENEATH THE GRAPH, which gives it the room.
    model.preview = "view.op";
    const auto previewed = pane::picture(model, room(), 10);
    const auto said = row_of(previewed, "view.op -- operator, native, from view.provider");
    REQUIRE(said >= 0);
    CHECK(row_of(previewed, "(a: Int) -> value: Int") > said);
    CHECK(row_of(previewed, "what this power is for") > said);
    REQUIRE(action_has(previewed, "add-found"));
    for (const auto& hit : previewed.hits)
        if (hit.action == "node" || hit.action == "port") CHECK(hit.y + hit.h <= said);
    // ...and a row the latest answer no longer carries is previewed by nobody.
    answered(model, {found_row("view.src", ws::kSourceKind)});
    CHECK_FALSE(action_has(pane::picture(model, room(), 11), "add-found"));
}
