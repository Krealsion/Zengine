// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#include "doctest.h"
#include "flow/flow-pane/gesture.hpp"
#include "flow/flow-pane/view.hpp"
#include "workshop/pane_canvas_drawing.hpp"
#include "workshop/pane_picture.hpp"
#include "workshop/screen_canvas.hpp"

namespace {
namespace pane = zengine::flow_pane;
namespace ws = zengine::workshop;
namespace flow = zengine::flow;
namespace gfx = zengine::graphics;
namespace fh = zengine::flow_host;
constexpr auto unit = pane::unit;
/// A room's cell, in its pixels: one character where no face is measured.
constexpr std::int64_t cell = gfx::kCellPx;

ws::CanvasRoom room() { return {100 * cell, 45 * cell, 1, true}; }
/// A window of light, its text set in a face eight pixels across and eighteen tall.
ws::CanvasRoom light_room() { return {1080, 840, 1, true, 8, 18}; }

/// What a graphics window would refuse of a picture drawn for `room`: none when it keeps it all.
std::string window_refuses(const pane::Picture& p, const ws::CanvasRoom& at = room()) {
    return pane::sketch_of(p, at).over;
}
pane::Model graph(std::size_t nodes) {
    pane::Model model;
    flow::Ports signature{"view.Rule",
        loom::SchemaBuilder("view.In", 1).field("a", loom::Kind::Int).build(),
        loom::SchemaBuilder("view.Out", 1).field("value", loom::Kind::Int).build()};
    model.palette.push_back(signature);
    model.command("state-field", {"total", "Int", "required"});
    model.command("message", {"Input"});
    model.command("trigger", {"0", "total"});
    model.page = pane::Page::Graph;
    for (std::size_t i = 0; i < nodes; ++i)
        model.workspace.graph.add(0, signature);
    return model;
}
bool text_has(const pane::Picture& picture, std::string_view part) {
    const auto texts = picture.texts();
    return std::any_of(texts.begin(), texts.end(),
        [&](const auto& text) { return text.find(part) != std::string::npos; });
}
bool action_has(const pane::Picture& picture, std::string_view action) {
    return std::any_of(picture.hits.begin(), picture.hits.end(),
        [&](const auto& hit) { return hit.action == action; });
}
const pane::Mark* text_mark(const pane::Picture& picture, std::string_view text) {
    for (const auto& k : picture.marks)
        if (k.op == pane::Mark::Op::Text && k.words == text) return &k;
    return nullptr;
}
std::int64_t row_of(const pane::Picture& picture, std::string_view text) {
    const auto* k = text_mark(picture, text);
    return k ? k->y : -1;
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
flow::Ports add_ports() {
    return {"math.add",
            loom::SchemaBuilder("math.add.in", 1).field("lhs", loom::Kind::Int).field("rhs", loom::Kind::Int).build(),
            loom::SchemaBuilder("math.add.out", 1).field("result", loom::Kind::Int).build()};
}
std::vector<std::string> add_ref(const pane::Model& model) {
    const auto& add = model.palette.front();
    return {"math.add", std::to_string(static_cast<std::int64_t>(add.inputs->content_id())),
            std::to_string(static_cast<std::int64_t>(add.outputs->content_id()))};
}
/// THE MOCKUP'S TALLY as the Next line builds it: a message bringing amount, total kept, add on the
/// two, its answer what total becomes, and tally.Total said with total.
pane::Model tally(bool said = true, bool wired = true) {
    pane::Model m;
    m.palette.push_back(add_ports());
    m.command("new", {"tally", "discard"});
    m.command("message", {"tally.panel.Count"});
    m.command("message-field", {"0", "amount", "Int", "required"});
    m.command("keep", {"total", "Int"});
    m.command("add-node", add_ref(m));
    m.command("bind-all", {"0", "$amount", wired ? "$total" : ""});
    if (said) m.command("say", {"Total", "total"});
    m.node.reset();
    return m;
}
/// A JOURNAL EVENT as the Flow host records one.
fh::FlowEvent event(std::int64_t sequence, std::string kind, const loom::Value& payload, std::string detail) {
    const auto bytes = loom::serialize(payload);
    return {sequence, std::move(kind), "0", loom::Bytes(bytes.begin(), bytes.end()), std::move(detail)};
}
loom::Value valued(const std::shared_ptr<const loom::Schema>& shape, const std::string& field, std::int64_t v) {
    loom::Value out(shape);
    out.set(field, loom::Cell::integer(v));
    return out;
}
}

TEST_CASE("Flow clips a large offscreen graph before spending the picture's budget") {
    auto model = graph(800);
    for (std::size_t i = 1; i < model.workspace.graph.places.size(); ++i)
        model.workspace.graph.places[i].y = 500000;
    const auto view = pane::picture(model, room());
    CHECK(window_refuses(view).empty());
    CHECK(text_has(view, "view.Rule"));
    CHECK_FALSE(text_has(view, "View limit reached"));
    CHECK(action_has(view, "node"));
    CHECK(std::none_of(view.marks.begin(), view.marks.end(), [](const auto& k) {
        return k.op == pane::Mark::Op::Text && k.words.empty() && k.caret_col < 0;
    }));
    CHECK(std::count_if(view.hits.begin(), view.hits.end(), [](const auto& h) { return h.action == "node"; }) == 1);
    CHECK(model.workspace.graph.project.definition.on.at(0).body.nodes.size() == 800);
}

TEST_CASE("Flow shows an actionable bounded picture instead of a partial dense graph") {
    auto model = graph(1100);
    for (auto& place : model.workspace.graph.places) { place.x = pane::kPlaceX; place.y = pane::kPlaceY + unit; }
    const auto view = pane::picture(model, room());
    CHECK(window_refuses(view).empty());
    CHECK(text_has(view, "View limit reached"));
    CHECK(text_has(view, "Draft intact"));
    CHECK(action_has(view, "ask-save"));
    CHECK(action_has(view, "ask-export"));
    CHECK(action_has(view, "zoom-in"));
    CHECK_FALSE(action_has(view, "node"));
    CHECK_FALSE(action_has(view, "port"));
    CHECK(model.workspace.graph.project.definition.on.at(0).body.nodes.size() == 1100);

    SUBCASE("in a graphics window, a graph the picture takes but a window would not keep is the "
            "recovery picture there; a picture past a window's bounds is the bound, said") {
        auto stacked = graph(60);
        for (auto& place : stacked.workspace.graph.places) { place.x = pane::kPlaceX; place.y = pane::kPlaceY + unit; }
        CHECK_FALSE(text_has(pane::picture(stacked, room()), "View limit reached"));
        const auto window = pane::window_picture(
            stacked, ws::canvas_window_room(100 * cell, 45 * cell, 0, {}));
        CHECK(text_has(window.picture, "View limit reached"));
        CHECK(window.sketch.nodes().size() <= gfx::kMaxNodesPerArea);
        // THE ROOM'S GROUND FIRST, all of it, so nothing behind the area shows through.
        REQUIRE_FALSE(window.sketch.nodes().empty());
        const auto ground = gfx::from_body<gfx::RectArgs>(window.sketch.nodes().front().args);
        REQUIRE(ground.has_value());
        CHECK(window.sketch.nodes().front().op == gfx::op::kRect);
        CHECK((ground->x == 0 && ground->y == 0 && ground->w == 100 * cell &&
               ground->h == 45 * cell));
        CHECK(ground->fill.role == gfx::role::kGround);
        CHECK(window.parts.count("control:ask-save") == 1);
        CHECK(window.parts.count("control:zoom-in") == 1);
        CHECK(std::none_of(window.parts.begin(), window.parts.end(),
                           [](const auto& part) { return part.second.action == "node"; }));
        ws::CanvasPicture many;
        many.rects.push_back({0, 0, 120, 120, gfx::role::kGround});
        for (std::size_t i = 0; i <= gfx::kMaxElementsPerArea; ++i)
            many.parts.push_back({"part " + std::to_string(i), 0, 0, cell, cell});
        const auto said = ws::canvas_sketch(many, ws::canvas_window_room(120, 120, 0, {}));
        CHECK(said.over.find("more than a graphics window keeps") != std::string::npos);
        CHECK(said.sketch.records().empty());
        CHECK(said.sketch.nodes().size() == 3);
    }
}

TEST_CASE("a step is named by its operator, and which one where two share it; a whole number is "
          "the whole of the text") {
    std::vector<zengine::op::Node> nodes(3);
    nodes[0].identity = "math.add";
    nodes[1].identity = "math.max";
    nodes[2].identity = "math.add";
    CHECK(pane::step_name(nodes, 0) == "add (1)");
    CHECK(pane::step_name(nodes, 1) == "max");
    CHECK(pane::step_name(nodes, 2) == "add (2)");
    // ⚔ MUTATION (tests/mutants/flow_view.md#whole-number-takes-a-start): a number's start is enough.
    CHECK(pane::whole_number("12"));
    CHECK(pane::whole_number("-3"));
    CHECK_FALSE(pane::whole_number(""));
    CHECK_FALSE(pane::whole_number("12x"));
    CHECK_FALSE(pane::whole_number("x12"));
    CHECK_FALSE(pane::whole_number("99999999999999999999"));
}

TEST_CASE("Flow keeps a long dialog draft editable without rejecting its picture") {
    pane::Model model;
    const std::string contents(4096, 'x');
    model.ask("Edit a long value", "value", {{"Text", contents}});
    for (const auto& at : {room(), light_room()}) {
        const auto view = pane::picture(model, at);
        CHECK(window_refuses(view, at).empty());
        CHECK(action_has(view, "dialog-confirm"));
        CHECK(std::any_of(view.marks.begin(), view.marks.end(),
                          [](const auto& k) { return k.op == pane::Mark::Op::Text && k.caret_col >= 0; }));
    }
    REQUIRE(model.dialog);
    CHECK(model.dialog->entries.at(0).text.text() == contents);
}

TEST_CASE("Flow marks shortened observations and retains their complete source") {
    pane::Model model;
    model.page = pane::Page::Events;
    model.events.push_back(std::string(5000, 'x'));
    const auto view = pane::picture(model, room());
    CHECK(window_refuses(view).empty());
    CHECK(text_has(view, "..."));
    CHECK(model.events.at(0).size() == 5000);
    for (const auto& k : view.marks) {
        if (k.op != pane::Mark::Op::Text) continue;
        CHECK(k.x >= 0);
        CHECK(k.x + static_cast<std::int64_t>(k.words.size()) * cell <= room().width);
    }
}

TEST_CASE("Flow pointer hits use the medium grain at pixel and half-cell edges") {
    pane::Picture view;
    view.hits.push_back({3, 3, 49, 49, "pixel-edge", {}, 0});
    view.grain = 1;
    CHECK(view.hit(3, 3) != nullptr);
    CHECK(view.hit(3, 3)->action == "pixel-edge");
    CHECK(view.hit(2, 3) == nullptr);
    CHECK(view.hit(51, 51) != nullptr);
    CHECK(view.hit(52, 51) == nullptr);
    view.hits = {{6, 6, 24, 24, "cell-edge", {}, 0}};
    view.grain = cell;
    CHECK(view.hit(0, 0) != nullptr);
    CHECK(view.hit(cell, cell) != nullptr);
    CHECK(view.hit(2 * cell, cell) == nullptr);
}

TEST_CASE("Flow clips its marks and hit map to the room, and says a room too small is one") {
    auto model = graph(1);
    auto narrow = room(); narrow.width = 42 * cell; narrow.height = 12 * cell;
    const auto view = pane::picture(model, narrow);
    CHECK(window_refuses(view, narrow).empty());
    REQUIRE(action_has(view, "run"));
    for (const auto& hit : view.hits) {
        CHECK(hit.x >= 0);
        CHECK(hit.y >= 0);
        CHECK(hit.x + hit.w <= narrow.width);
        CHECK(hit.y + hit.h <= narrow.height);
    }
    for (const auto& k : view.marks) {
        const auto at = k.clip ? k.bounds().cut(*k.clip) : k.bounds();
        CHECK(at.x >= 0);
        CHECK(at.right() <= narrow.width);
    }
    CHECK(view.hit(narrow.width, 0) == nullptr);
    CHECK(view.hit(0, narrow.height) == nullptr);
    auto tiny = narrow; tiny.width = 15 * cell; tiny.height = 10 * cell;
    const auto small = pane::picture(model, tiny);
    CHECK(text_has(small, "Flow needs"));
    CHECK(small.hits.empty());
}

TEST_CASE("Flow keeps its rail apart from the graph, and nothing of the graph page over the Next line") {
    auto model = graph(1);
    std::vector<ws::PowerRow> rows{
        found_row("very.long.provider.name.that.must.not.paint.over.the.graph", ws::kOperatorKind)};
    answered(model, rows);
    const auto view = pane::picture(model, room());
    REQUIRE(action_has(view, "found"));
    REQUIRE(view.graph.has_value());
    for (const auto& hit : view.hits)
        if (hit.action == "found" || hit.action == "source-field") CHECK(hit.x + hit.w <= view.graph->x);
    for (const auto& k : view.marks)
        if (k.op == pane::Mark::Op::Text && k.words.find("very.long") != std::string::npos)
            CHECK(k.x + k.w <= view.graph->x);

    // A fractional final body row must not paint over the Next line after a resize -- the defect
    // this case guards, on a rail with actions and on an observation page without them.
    for (int i = 0; i < 40; ++i) {
        rows.push_back(found_row("palette." + std::to_string(i), ws::kOperatorKind));
        model.events.push_back("event " + std::to_string(i));
    }
    answered(model, rows);
    auto shorter = room();
    shorter.text_advance_px = 8; shorter.text_line_px = 18;
    shorter.width = 150 * 8; shorter.height = 23 * 22 + 11;
    const auto rail = pane::picture(model, shorter);
    REQUIRE(rail.graph.has_value());
    const auto floor = rail.graph->bottom();
    bool palette_visible = false;
    for (const auto& hit : rail.hits) {
        if (hit.action != "found") continue;
        palette_visible = true;
        CHECK(hit.y + hit.h <= floor);
    }
    REQUIRE(palette_visible);
    for (const auto& k : rail.marks)
        if (k.op == pane::Mark::Op::Text && k.words.find("palette.") != std::string::npos)
            CHECK(k.y + k.h <= floor);
    model.page = pane::Page::Events;
    model.notice = "the latest notice";
    const auto events = pane::picture(model, shorter);
    const auto* notice = text_mark(events, model.notice);
    REQUIRE(notice != nullptr);
    bool event_visible = false;
    for (const auto& k : events.marks) {
        if (k.op != pane::Mark::Op::Text || k.words.rfind("event ", 0) != 0) continue;
        event_visible = true;
        CHECK(k.y + k.h <= notice->y);
    }
    REQUIRE(event_visible);
}

TEST_CASE("Flow projects node places and hit regions through the room's independent measured axes") {
    auto model = graph(1);
    const auto saved = flow::workspace_bytes(model.workspace);
    const auto cell_view = pane::picture(model, room());
    auto native_room = room();
    native_room.text_advance_px = 9;
    native_room.text_line_px = 18;
    native_room.width = 120 * 9;
    native_room.height = 45 * 22;
    const auto view = pane::picture(model, native_room);
    CHECK(window_refuses(view, native_room).empty());
    REQUIRE(action_has(view, "node"));
    const auto node = std::find_if(view.hits.begin(), view.hits.end(),
        [](const auto& hit) { return hit.action == "node"; });
    const auto cell_node = std::find_if(cell_view.hits.begin(), cell_view.hits.end(),
        [](const auto& hit) { return hit.action == "node"; });
    REQUIRE(cell_node != cell_view.hits.end());
    REQUIRE(view.canvas.has_value());
    REQUIRE(cell_view.canvas.has_value());
    // A node's place stands as many characters and rows into the canvas in either room.
    CHECK((node->x - view.canvas->x) / 9 == (cell_node->x - cell_view.canvas->x) / cell);
    CHECK((node->y - view.canvas->y) / 22 == (cell_node->y - cell_view.canvas->y) / cell);
    CHECK(view.hit(node->x + 2, node->y + 2)->action == "node");
    // THE ROOM'S GROUND FIRST in what a window is given, all of it.
    const auto drawn = pane::sketch_of(view, native_room);
    REQUIRE(drawn.over.empty());
    const auto ground = gfx::from_body<gfx::RectArgs>(drawn.sketch.nodes().front().args);
    REQUIRE(ground.has_value());
    CHECK((ground->w == native_room.width && ground->h == native_room.height));
    CHECK(ground->fill.role == gfx::role::kGround);
    for (const auto& k : view.marks) {
        if (k.op != pane::Mark::Op::Text || k.words.empty()) continue;
        ws::CanvasText run{k.x, k.y, k.words, k.fill.role};
        const auto layout = ws::clip_canvas_text(run, {0, 0, native_room.width, native_room.height}, native_room);
        REQUIRE(layout.visible());
        CHECK(layout.text.text == k.words);
    }
    CHECK(flow::workspace_bytes(model.workspace) == saved);
    // A NODE PANNED HALF BEHIND THE WHEN LANE is cut where the canvas begins, its words marked so.
    model.workspace.graph.places.front().x = pane::kPlaceX - 5 * unit;
    const auto clipped = pane::picture(model, native_room);
    CHECK(window_refuses(clipped, native_room).empty());
    CHECK(text_has(clipped, "..."));
    const auto clipped_node = std::find_if(clipped.hits.begin(), clipped.hits.end(),
        [](const auto& hit) { return hit.action == "node"; });
    REQUIRE(clipped_node != clipped.hits.end());
    CHECK(clipped_node->x == clipped.canvas->x);

    auto extreme = native_room;
    extreme.text_advance_px = (std::numeric_limits<std::int64_t>::max)();
    extreme.text_line_px = (std::numeric_limits<std::int64_t>::max)();
    const auto bounded = pane::picture(model, extreme);
    CHECK(window_refuses(bounded, extreme).empty());
    CHECK_FALSE(action_has(bounded, "node"));

    model.ask("Edit text", "value", {{"Value", "selected"}});
    model.dialog->entries.front().text.set("selected", 0);
    model.dialog->entries.front().text.drag_to_column(8);
    const auto dialog = pane::picture(model, native_room);
    const auto field = std::find_if(dialog.marks.begin(), dialog.marks.end(),
        [](const auto& k) { return k.op == pane::Mark::Op::Text && k.caret_col >= 0; });
    REQUIRE(field != dialog.marks.end());
    CHECK(field->sel_end_col - field->sel_begin_col == 8);
    CHECK(field->caret_col == field->sel_end_col);
}

TEST_CASE("the fold node's picture fits: its words inside its card, and a node just added is seen whole in the canvas") {
    // The tally's fold, its body chosen, in the room Flow offers to take and at the default zoom.
    pane::Model model;
    model.palette.push_back(add_ports());
    model.command("state-field", {"total", "Int", "required"});
    model.command("message", {"Count"});
    for (const char* f : {"start", "limit", "step"}) model.command("message-field", {"0", f, "Int", "required"});
    model.command("trigger", {"0", "total"});
    model.command("add-fold");
    const auto& add = model.palette.front();
    model.command("fold-body", {"0", "math.add", std::to_string(static_cast<std::int64_t>(add.inputs->content_id())),
                                std::to_string(static_cast<std::int64_t>(add.outputs->content_id())), "rhs", "lhs"});
    answered(model, {found_row("math.add", ws::kOperatorKind)});
    model.preview = "math.add";
    model.page = pane::Page::Graph;
    REQUIRE(model.node.has_value());
    ws::CanvasRoom offered{88 * cell, 22 * cell, cell, false};
    // Placed low, its answer row is under the canvas's floor: brought into view.
    model.workspace.graph.places.front().y = pane::kPlaceY + 12 * unit;
    const auto before = pane::picture(model, offered);
    REQUIRE(before.node_extent.has_value());
    REQUIRE(before.canvas.has_value());
    CHECK(before.node_extent->second > before.canvas->bottom());
    pane::reveal_selected(model, offered);
    const auto after = pane::picture(model, offered);
    REQUIRE(after.node_extent.has_value());
    CHECK(after.node_extent->first >= after.canvas->y);
    CHECK(after.node_extent->second <= after.canvas->bottom());
    CHECK(action_has(after, "source-node"));
    CHECK(text_has(after, "fold add"));
    CHECK(text_has(after, "count rhs, acc lhs"));
    // OPENED IN A SHORT PANE, the graph is panned up as far as its topmost node allows.
    auto opened = model;
    opened.node.reset();
    opened.preview.clear();
    opened.workspace.pan_y = 0;
    ws::CanvasRoom short_room{88 * cell, 22 * cell, cell, false};
    const auto low = pane::picture(opened, short_room);
    REQUIRE(low.graph_extent.has_value());
    CHECK(low.graph_extent->second > low.canvas->bottom());
    pane::reveal_graph(opened, short_room);
    const auto raised = pane::picture(opened, short_room);
    REQUIRE(raised.graph_extent.has_value());
    CHECK(raised.graph_extent->first >= raised.canvas->y);
    CHECK(raised.graph_extent->second <= raised.canvas->bottom());
    CHECK(opened.workspace.pan_y < 0);
    // Every word of the node stays inside its card, however long its title: shortened and marked.
    auto long_name = model;
    long_name.node.reset();
    long_name.palette.front().identity = "math.add_with_a_very_long_identity";
    long_name.workspace.graph.project.definition.on.front().body.nodes.front().identity = "math.add_with_a_very_long_identity";
    const auto clipped = pane::picture(long_name, offered);
    const auto card = std::find_if(clipped.hits.begin(), clipped.hits.end(),
        [](const auto& hit) { return hit.action == "node"; });
    REQUIRE(card != clipped.hits.end());
    bool marked = false;
    for (const auto& k : clipped.marks)
        if (k.op == pane::Mark::Op::Text && k.y >= card->y && k.y < card->y + card->h &&
            k.words.find("add_wi") != std::string::npos) {
            CHECK(k.x + k.w <= card->x + card->w);
            marked = marked || (k.words.size() >= 3 && k.words.substr(k.words.size() - 3) == "...");
        }
    CHECK(marked);
    // ...CHOSEN, its words stop short of the x on its corner, still shortened and marked.
    long_name.node = 0;
    const auto chosen = pane::picture(long_name, offered);
    const auto head = std::find_if(chosen.hits.begin(), chosen.hits.end(),
        [](const auto& hit) { return hit.action == "node"; });
    const auto x = std::find_if(chosen.hits.begin(), chosen.hits.end(),
        [](const auto& hit) { return hit.action == "remove"; });
    REQUIRE(head != chosen.hits.end());
    REQUIRE(x != chosen.hits.end());
    bool short_of_x = false;
    for (const auto& k : chosen.marks)
        if (k.op == pane::Mark::Op::Text && k.y >= head->y && k.y < head->y + head->h && k.x < x->x) {
            CHECK(k.x + k.w <= x->x);
            short_of_x = short_of_x || (k.words.size() >= 3 && k.words.substr(k.words.size() - 3) == "...");
        }
    CHECK(short_of_x);
}

TEST_CASE("Flow's rail says what is in scope and where it comes from, and groups the door's rows: operators by their first word, then Sources, Conversions and Forms") {
    auto model = graph(1);
    // THE ANSWER COMES IN THE CATALOG'S ORDER; the rail groups it, reading each row's
    // classification as the door gave it and an operator's name as it is spelled.
    answered(model, {found_row("view.edge", ws::kConversionKind), found_row("view.op", ws::kOperatorKind),
                     found_row("view.src", ws::kSourceKind), found_row("math.other", ws::kOperatorKind)});
    const auto view = pane::picture(model, room());
    CHECK(row_of(view, "In scope") >= 0);
    CHECK(text_has(view, "state.total"));
    CHECK(text_has(view, " kept"));
    const auto operators = row_of(view, "Operators"), view_ns = row_of(view, "VIEW"), math = row_of(view, "MATH"),
               sources = row_of(view, "SOURCES"), conversions = row_of(view, "CONVERSIONS");
    REQUIRE(operators >= 0);
    REQUIRE(view_ns >= 0);
    REQUIRE(math >= 0);
    REQUIRE(sources >= 0);
    REQUIRE(conversions >= 0);
    CHECK(operators < view_ns);
    CHECK(view_ns < row_of(view, "  view.op"));
    CHECK(row_of(view, "  view.op") < math);
    CHECK(math < row_of(view, "  math.other"));
    CHECK(row_of(view, "  math.other") < sources);
    CHECK(sources < row_of(view, "  view.src"));
    CHECK(row_of(view, "  view.src") < conversions);
    CHECK(conversions < row_of(view, "  view.edge"));

    // A PREVIEW IS THE ROW EXPANDED AT THE RAIL'S FOOT: what it is, its ports, what it is for,
    // and Add, the list above it stopping where it begins.
    model.preview = "view.op";
    const auto previewed = pane::picture(model, room());
    const auto said = row_of(previewed, "view.op");
    REQUIRE(said >= 0);
    CHECK(row_of(previewed, "operator, native, from") > said);
    CHECK(row_of(previewed, "view.provider") > said);
    CHECK(row_of(previewed, "(a: Int) -> value: Int") > said);
    CHECK(row_of(previewed, "what this power is for") > said);
    REQUIRE(action_has(previewed, "add-found"));
    for (const auto& hit : previewed.hits)
        if (hit.action == "found") CHECK(hit.y + hit.h <= said);
    // ...IN EVERY ROOM WITH MEASURED TEXT TOO, the light rail's two-line rows and their preview.
    for (std::int64_t height = 30 * 22; height < 31 * 22; ++height) {
        auto measured = room();
        measured.text_advance_px = 9;
        measured.text_line_px = 18;
        measured.width = 120 * 9;
        measured.height = height;
        const auto band = pane::picture(model, measured);
        CHECK_MESSAGE(row_of(band, "view.op") >= 0, height);
        CHECK_MESSAGE(row_of(band, "what this power is for") >= 0, height);
        CHECK_MESSAGE(row_of(band, "Add to the rule") >= 0, height);
    }
    // ...and a row the latest answer no longer carries is previewed by nobody.
    answered(model, {found_row("view.src", ws::kSourceKind)});
    CHECK_FALSE(action_has(pane::picture(model, room()), "add-found"));
}

TEST_CASE("the rule reads as one sentence, each part still to choose open") {
    using K = pane::Chip::Kind;
    const auto kinds = [](const pane::Model& m) {
        std::vector<std::pair<K, std::string>> out;
        for (const auto& chip : pane::sentence(m)) out.emplace_back(chip.kind, chip.text);
        return out;
    };
    pane::Model empty;
    empty.command("new", {"tally", "discard"});
    CHECK(kinds(empty) == std::vector<std::pair<K, std::string>>{
                             {K::Plain, "When"}, {K::Open, "a message"}, {K::Plain, "arrives,"},
                             {K::Open, "something"}, {K::Plain, "becomes"}, {K::Open, "..."}});
    const auto half = kinds(tally(false, false));
    CHECK(half.at(1) == std::pair<K, std::string>{K::Message, "tally.panel.Count"});
    CHECK(half.at(3) == std::pair<K, std::string>{K::Kept, "total"});
    CHECK(half.at(5) == std::pair<K, std::string>{K::StepOpen, "add(amount, ?)"});
    const auto whole = kinds(tally());
    REQUIRE(whole.size() == 10);
    CHECK(whole.at(5) == std::pair<K, std::string>{K::Step, "add(amount, total)"});
    CHECK(whole.at(6) == std::pair<K, std::string>{K::Plain, ", then says"});
    CHECK(whole.at(7) == std::pair<K, std::string>{K::Message, "tally.Total"});
    CHECK(whole.at(9) == std::pair<K, std::string>{K::Kept, "total"});
}

TEST_CASE("the Next line names the one thing the rule still needs, with the press that does it, in place of Run's refusals") {
    using T = pane::NextStep::Tone;
    pane::Model m;
    m.palette.push_back(add_ports());
    m.command("new", {"tally", "discard"});
    auto step = pane::next_step(m);
    CHECK(step.lead == "Next:");
    CHECK(step.text == "start with a message.");
    CHECK(step.button == "New message");
    CHECK(step.action == "ask-message");

    m.command("message", {"tally.panel.Count"});
    m.command("message-field", {"0", "amount", "Int", "required"});
    step = pane::next_step(m);
    CHECK(step.text == "give the rule something to keep between messages.");
    CHECK(step.action == "ask-keep");

    // KEEPING A FIELD WITH A MESSAGE DECLARED MAKES THE RULE: that message wakes it, writing it.
    m.command("keep", {"total", "Int"});
    REQUIRE(m.workspace.graph.project.definition.on.size() == 1);
    CHECK(m.workspace.graph.project.definition.on.front().output == "total");
    step = pane::next_step(m);
    CHECK(step.text == "choose what total becomes. Pick a step in the rail, then Add to the rule.");
    CHECK(step.button.empty());
    answered(m, {found_row("math.add", ws::kOperatorKind)});
    m.preview = "math.add";
    step = pane::next_step(m);
    CHECK(step.button == "Add add");
    CHECK(step.action == "add-found");
    CHECK(step.args == std::vector<std::string>{ws::kOperatorKind, "math.add"});

    // A REFUSED RUN SAYS WHAT IT NEEDS FIRST, in the warning's colour, until an edit is taken.
    m.command("add-node", add_ref(m));
    m.nudge = true;
    step = pane::next_step(m);
    CHECK(step.lead == "Run needs this first:");
    CHECK(step.tone == T::Warn);
    CHECK(step.text == "give add its values.");
    CHECK(step.button == "Wire amount and total into add");
    CHECK(step.action == "bind-all");
    CHECK(step.args == std::vector<std::string>{"0", "$amount", "$total"});
    m.command(step.action, step.args);
    CHECK_FALSE(m.nudge);
    step = pane::next_step(m);
    CHECK(step.lead == "Ready.");
    CHECK(step.tone == T::Good);
    CHECK(step.action == "run");

    // RUNNING, WITH NOTHING SAID: say what it keeps. A NEW MESSAGE WAITS FOR A RESTART, said up front.
    m.running = true;
    m.running_definition = m.workspace.graph.project.definition;
    step = pane::next_step(m);
    CHECK(step.lead == "Running.");
    CHECK(step.button == "+ say tally.Total");
    CHECK(step.action == "ask-say");
    m.command("say", {"Total", "total"});
    step = pane::next_step(m);
    CHECK(step.text == "a new message needs a restart: Apply keeps a running rule's messages as they are.");
    CHECK(step.button == "Restart");
    CHECK(step.action == "restart");
    CHECK(step.hint == "total carries on from its saved value");
    m.running_definition = m.workspace.graph.project.definition;
    step = pane::next_step(m);
    CHECK(step.lead == "Done.");
    CHECK(step.hint == "carry tally.Total onto a label to show it");
    // A STEP CHANGED WHILE IT RUNS IS APPLIED; STOPPED, IT IS PUT AWAY.
    m.command("bind", {"0", "0", "$total"});
    CHECK(pane::next_step(m).action == "apply");
    m.running = false;
    m.running_definition.reset();
    m.ran = true;
    step = pane::next_step(m);
    CHECK(step.lead == "Stopped.");
    CHECK(step.action == "run");
}

TEST_CASE("a run's journal reads as one line a message, each output with the message that caused it, and each wire's last value") {
    auto m = tally();
    const auto& def = m.workspace.graph.project.definition;
    const auto count = def.accepts.front();
    const auto total = def.emits.front();
    loom::Value start(def.state);
    start.set("total", loom::Cell::integer(0));
    fh::FlowAnswer answer;
    // TWO ARRIVE BEFORE EITHER IS SAID: each output names the arrival whose handling said it.
    answer.events = {event(1, "arrived", valued(count, "amount", 3), count->name()),
                     event(2, "kept", valued(def.state, "total", 3), "after #1"),
                     event(3, "arrived", valued(count, "amount", 4), count->name()),
                     event(4, "kept", valued(def.state, "total", 7), "after #3"),
                     event(5, "output", valued(total, "total", 3), total->name() + " after #1"),
                     event(6, "output", valued(total, "total", 7), total->name() + " after #3")};
    const auto seen = pane::observe(answer, def, start);
    REQUIRE(seen.arrivals.size() == 2);
    CHECK(pane::line_of(seen.arrivals[0]) ==
          "tally.panel.Count  amount 3  ->  total 0 -> 3  ->  said tally.Total 3");
    CHECK(pane::line_of(seen.arrivals[1]) ==
          "tally.panel.Count  amount 4  ->  total 3 -> 7  ->  said tally.Total 7");
    const auto carried = pane::carried(seen);
    CHECK(carried.message.at("amount") == "4");
    CHECK(carried.before.at("total") == "3");
    CHECK(carried.after.at("total") == "7");
    // A JOURNAL THAT DROPPED ITS OLDEST knows no state before the first arrival it keeps.
    answer.dropped = 1;
    answer.events.erase(answer.events.begin(), answer.events.begin() + 2);
    const auto later = pane::observe(answer, def, start);
    REQUIRE(later.arrivals.size() == 1);
    CHECK(pane::line_of(later.arrivals[0]) == "tally.panel.Count  amount 4  ->  total -> 7  ->  said tally.Total 7");
    // THE STATE A RUN STARTED WITH, AS A RELOAD KEEPS IT, reads back against the state the rule
    // keeps, and against no other.
    const auto kept = pane::kept_start(start);
    const auto back = pane::start_of(kept, def);
    REQUIRE(back);
    CHECK(back->get("total")->as_int() == 0);
    auto other = def;
    other.state = loom::SchemaBuilder("tally.Other", 1).field("count", loom::Kind::Int).build();
    CHECK_FALSE(pane::start_of(kept, other));
    // ANOTHER RULE IN THE DRAFT'S PLACE shows nothing of what this one did.
    m.observed = seen;
    m.earlier = {pane::line_of(later.arrivals[0])};
    m.started_kept = kept;
    m.events = {"tally.panel.Count  amount 3"};
    m.command("new", {"other", "discard"});
    CHECK(m.observed.arrivals.empty());
    CHECK(m.earlier.empty());
    CHECK(m.started_kept.empty());
    CHECK(m.events.empty());
    CHECK(text_has(pane::picture(m, room()), "nothing yet: the rule is not running"));
}

TEST_CASE("Flow made over draws its three lanes, its cards, ports and curved wires in the mockup's colours in a window of light, and the same rule in characters in a terminal") {
    auto m = tally();
    m.running = true;
    m.running_definition = m.workspace.graph.project.definition;
    const auto& def = m.workspace.graph.project.definition;
    loom::Value start(def.state);
    start.set("total", loom::Cell::integer(0));
    m.started = start;
    fh::FlowAnswer answer;
    answer.events = {event(1, "arrived", valued(def.accepts.front(), "amount", 3), def.accepts.front()->name()),
                     event(2, "kept", valued(def.state, "total", 3), "after #1"),
                     event(3, "output", valued(def.emits.front(), "total", 3), def.emits.front()->name() + " after #1")};
    m.observed = pane::observe(answer, def, m.started);
    m.workspace.graph.project.state.set("total", loom::Cell::integer(3));
    const auto light = pane::window_picture(m, light_room());
    REQUIRE(light.picture.canvas.has_value());
    CHECK(light.sketch.nodes().size() <= gfx::kMaxNodesPerArea);
    for (const char* words : {"WHEN", "DO", "THEN", "tally.panel.Count", "amount", "kept between messages",
                              "add", "math.add", "lhs", "rhs", "answer", "becomes the answer", "now 3", "say",
                              "tally.Total", "total = 3", "Events", "newest first", "Done."})
        CHECK_MESSAGE(text_has(light.picture, words), std::string(words));
    CHECK(text_has(light.picture, "tally.panel.Count  amount 3  ->  total 0 -> 3  ->  said tally.Total 3"));
    std::size_t curves = 0, ports = 0;
    std::set<std::int64_t> colours;
    for (const auto& node : light.sketch.nodes()) {
        if (node.op == gfx::op::kPath) {
            if (const auto p = gfx::from_body<gfx::PathArgs>(node.args)) {
                curves += p->d.find(" C") != std::string::npos ? std::size_t{1} : std::size_t{0};
                colours.insert(p->stroke.rgb);
                colours.insert(p->fill.rgb);
            }
        } else if (node.op == gfx::op::kEllipse) {
            ++ports;
        } else if (node.op == gfx::op::kRect) {
            if (const auto r = gfx::from_body<gfx::RectArgs>(node.args)) colours.insert(r->fill.rgb);
        }
    }
    CHECK(curves >= 3); // amount and total into add, add's answer into total
    CHECK(ports >= 6);
    // THE VALUE EACH WIRE LAST CARRIED: amount 3 in, total 0 before it, 3 after.
    const auto texts = light.picture.texts();
    CHECK(std::count(texts.begin(), texts.end(), "3") == 2);
    CHECK(std::count(texts.begin(), texts.end(), "0") == 1);
    for (const std::int64_t rgb : {0x262b35, 0x1a1d24, 0x242629, 0x20252d, 0x20252c, 0x323846, 0x88c0d0})
        CHECK_MESSAGE(colours.count(rgb) == 1, rgb);
    CHECK(light.parts.count("port:1.0") == 1);
    CHECK(light.parts.count("write:0") == 1);
    CHECK(light.parts.count("say:0") == 1);

    // IN A TERMINAL, THE SAME RULE IN CHARACTERS: no curve and no ellipse, its parts named alike.
    const auto cells = pane::window_picture(m, ws::canvas_window_room(170 * cell, 60 * cell, 0, {}));
    CHECK(cells.sketch.nodes().size() <= gfx::kMaxNodesPerArea);
    for (const auto& node : cells.sketch.nodes()) {
        CHECK(node.op != gfx::op::kPath);
        CHECK(node.op != gfx::op::kEllipse);
    }
    CHECK(text_has(cells.picture, "[Run"));
    CHECK(text_has(cells.picture, "[File v]"));
    CHECK(text_has(cells.picture, "[tally.panel.Count]"));
    CHECK(cells.parts.count("port:1.0") == 1);
    CHECK(cells.parts.count("menu:file") == 1);
}

TEST_CASE("a chosen When or Then card is edged as a chosen step is, and a card put lower stands lower, "
          "never past its column's floor") {
    auto m = tally();
    const auto room = light_room();
    const auto edge = [&](const pane::Picture& p, const std::string& name) -> std::int64_t {
        const auto box = p.cards.at(name);
        for (const auto& k : p.marks)
            if (k.op == pane::Mark::Op::Rect && k.x == box.x && k.y == box.y && k.w == box.w && k.h == box.h)
                return k.stroke.rgb;
        return -1;
    };
    auto pic = pane::picture(m, room);
    REQUIRE(pic.cards.count("kept") == 1);
    REQUIRE(pic.docks.has_value());
    const auto stacked = pic.cards.at("kept");
    CHECK(edge(pic, "kept") != 0x88c0d0);
    m.dock = "kept";
    pic = pane::picture(m, room);
    CHECK(edge(pic, "kept") == 0x88c0d0);
    // PUT LOWER, it stands lower, in its own column; put past the column's floor, it stands on it.
    m.place_dock("kept", 10 * pane::unit);
    pic = pane::picture(m, room);
    CHECK(pic.cards.at("kept").y > stacked.y);
    CHECK(pic.cards.at("kept").x == stacked.x);
    m.place_dock("kept", 1000 * pane::unit);
    pic = pane::picture(m, room);
    CHECK(pic.cards.at("kept").bottom() <= pic.docks->bottom());
}

TEST_CASE("two presses are one double click on one target, near where the first landed, within the "
          "interval, and time is its argument") {
    const pane::ClickMemory armed{true, "column:do", 100, 100, 1000};
    // SOON ENOUGH, the boundary included; LATER, or a reading that went back, is two presses.
    // ⚔ MUTATION (tests/mutants/flow_view.md#doubles-at-any-time): any time doubles.
    CHECK(pane::doubles(armed, "column:do", 100, 100, 8, 1000));
    CHECK(pane::doubles(armed, "column:do", 100, 100, 8, 1000 + pane::kDoublePressMs));
    CHECK_FALSE(pane::doubles(armed, "column:do", 100, 100, 8, 1001 + pane::kDoublePressMs));
    CHECK_FALSE(pane::doubles(armed, "column:do", 100, 100, 8, 999));
    // THE SAME TARGET, within the slack either way.
    // ⚔ MUTATION (tests/mutants/flow_view.md#doubles-on-any-target): another target doubles.
    CHECK_FALSE(pane::doubles(armed, "column:when", 100, 100, 8, 1100));
    CHECK(pane::doubles(armed, "column:do", 108, 92, 8, 1100));
    CHECK_FALSE(pane::doubles(armed, "column:do", 109, 100, 8, 1100));
    CHECK_FALSE(pane::doubles(armed, "column:do", 100, 91, 8, 1100));
    // NOTHING ARMED, or a press that named nothing, doubles nothing.
    auto spent = armed;
    spent.armed = false;
    CHECK_FALSE(pane::doubles(spent, "column:do", 100, 100, 8, 1100));
    CHECK_FALSE(pane::doubles(pane::ClickMemory{true, "", 100, 100, 1000}, "", 100, 100, 8, 1100));
}
