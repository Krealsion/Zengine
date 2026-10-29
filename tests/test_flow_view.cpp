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
    // ⚔ MUTATION (tests/mutants/flow_view.md#card-head-says-its-identity): a head says its identity.
    CHECK(text_has(view, "Rule (1)"));
    CHECK_FALSE(text_has(view, "view.Rule"));
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

TEST_CASE("a kept field's kind is chosen from kinds Flow takes, each the kind of one value") {
    pane::Model model;
    for (const auto& kind : pane::field_kinds()) {
        INFO(kind);
        const auto type = flow::shape::type_named(kind, model.workspace.library);
        CHECK(type.kind != loom::Kind::List);
        CHECK(type.kind != loom::Kind::Message);
    }
    model.ask("Keep a field between messages", "keep", {{"Name", "total"}, {"Kind", "Int"}});
    CHECK(model.dialog->entries.at(0).choices.empty());
    CHECK(model.dialog->entries.at(1).choices == pane::field_kinds());
    CHECK(pane::in_place("keep"));
    CHECK(pane::in_place("say"));
    CHECK(pane::in_place("message"));
    CHECK_FALSE(pane::in_place("save"));
}

TEST_CASE("a part carries a run of words only where the run's whole row stands inside it") {
    const auto at = light_room();
    pane::Drawing d;
    d.width = at.width;
    d.height = at.height;
    d.light = true;
    pane::Mark words;
    words.op = pane::Mark::Op::Text;
    words.x = 10;
    words.y = 30;
    words.w = 80;
    words.h = 18;
    words.words = "grazed";
    words.fill = gfx::by_role(gfx::role::kFill);
    d.marks.push_back(words);
    d.hits.push_back({0, 0, 200, 40, "graze", {"a"}, 0});
    d.hits.push_back({0, 20, 200, 40, "hold", {"b"}, 0});
    const auto sketched = pane::sketch_of(d, at);
    REQUIRE(sketched.over.empty());
    std::map<std::string, std::string> group_of;
    for (const auto& r : sketched.sketch.records()) group_of[r.action] = r.path;
    REQUIRE(group_of.count("graze:a") == 1);
    REQUIRE(group_of.count("hold:b") == 1);
    const auto carries = [&](const std::string& group) {
        for (const auto& n : sketched.sketch.nodes())
            if (n.op == "text" && n.path.rfind(group + "/", 0) == 0) return true;
        return false;
    };
    // ⚔ MUTATION (tests/mutants/flow_view.md#place-carries-a-grazed-row): a place carries every run
    // it meets.
    CHECK_FALSE(carries(group_of["graze:a"]));
    CHECK(carries(group_of["hold:b"]));
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
    CHECK(text_has(view, "total : Int"));
    CHECK_FALSE(text_has(view, "state.total"));
    CHECK(text_has(view, " kept"));
    const auto operators = row_of(view, "Operators"), view_ns = row_of(view, "VIEW"), math = row_of(view, "MATH"),
               sources = row_of(view, "SOURCES"), conversions = row_of(view, "CONVERSIONS");
    REQUIRE(operators >= 0);
    REQUIRE(view_ns >= 0);
    REQUIRE(math >= 0);
    REQUIRE(sources >= 0);
    REQUIRE(conversions >= 0);
    CHECK(operators < view_ns);
    CHECK(view_ns < row_of(view, "  op"));
    CHECK(row_of(view, "  op") < math);
    CHECK(math < row_of(view, "  other"));
    CHECK(row_of(view, "  other") < sources);
    CHECK(sources < row_of(view, "  src"));
    CHECK(row_of(view, "  src") < conversions);
    CHECK(conversions < row_of(view, "  edge"));

    // A PREVIEW IS THE ROW EXPANDED AT THE RAIL'S FOOT: what it is, its ports, what it is for,
    // and Add, the list above it stopping where it begins.
    model.preview = "view.op";
    const auto previewed = pane::picture(model, room());
    const auto said = row_of(previewed, "op");
    REQUIRE(said >= 0);
    // ⚔ MUTATION (tests/mutants/flow_view.md#preview-names-its-provider): it names its provider.
    CHECK(row_of(previewed, "an operator") > said);
    CHECK_FALSE(text_has(previewed, "view.provider"));
    CHECK_FALSE(text_has(previewed, "view.op"));
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
        CHECK_MESSAGE(row_of(band, "op") >= 0, height);
        CHECK_MESSAGE(row_of(band, "what this power is for") >= 0, height);
        CHECK_MESSAGE(row_of(band, "Add to the rule") >= 0, height);
    }
    // ...and a row the latest answer no longer carries is previewed by nobody.
    answered(model, {found_row("view.src", ws::kSourceKind)});
    CHECK_FALSE(action_has(pane::picture(model, room()), "add-found"));
}

TEST_CASE("Flow names a step by its operator, a port by its step and a field by its name: no step "
          "number, field spelling, revision or operator identity on screen") {
    // TWO STEPS OF ONE OPERATOR, the second reading the first's answer: each counted by its operator.
    auto m = tally();
    m.command("add-node", add_ref(m));
    m.command("bind", {"1", "0", "%0"});
    m.command("move", {"1", std::to_string(pane::kPlaceX), "720"});
    m.node = 1;
    answered(m, {found_row("math.add", ws::kOperatorKind)});
    m.preview = "math.add";
    m.preview_kind = ws::kOperatorKind;
    const auto internal = [](const pane::Picture& view) {
        for (const auto& words : view.texts()) {
            // A revision as the bar once said one: ` r` and a digit.
            bool revision = false;
            for (std::size_t at = 1; at + 1 < words.size(); ++at)
                revision = revision || (words[at - 1] == ' ' && words[at] == 'r' && words[at + 1] >= '0' &&
                                        words[at + 1] <= '9');
            if (words.find('%') != std::string::npos || words.find('$') != std::string::npos ||
                words.find("state.") != std::string::npos || words.find("input.") != std::string::npos ||
                words.find("math.add") != std::string::npos || words.find("provider") != std::string::npos ||
                revision)
                return words;
        }
        return std::string();
    };
    // ⚔ MUTATION (tests/mutants/flow_view.md#card-head-says-its-identity): a head says its identity.
    // ⚔ MUTATION (tests/mutants/flow_view.md#preview-names-its-provider): it names its provider.
    for (const auto& at : {room(), light_room()}) {
        const auto view = pane::picture(m, at);
        CHECK_MESSAGE(internal(view).empty(), internal(view));
        CHECK(text_has(view, "add (1)"));
        CHECK(text_has(view, "add (2)"));
        CHECK(text_has(view, "<- add (1)"));
        CHECK(text_has(view, "Add before add (2)"));
        CHECK(text_has(view, "an operator"));
    }
    // A PORT CHOSEN: the rail and the box at it name it by its step.
    m.filling = pane::PortChoice{m.workspace.graph.place(0, 1).id, 1};
    m.preview.clear();
    for (const auto& at : {room(), light_room()}) {
        const auto view = pane::picture(m, at);
        CHECK_MESSAGE(internal(view).empty(), internal(view));
        CHECK(text_has(view, "For add (2)'s rhs : Int"));
        CHECK(text_has(view, "from the message"));
        CHECK(text_has(view, "a step's answer"));
    }
    CHECK(pane::next_step(m).text == "give add (2) its values.");
    CHECK(pane::next_step(m).button == "Wire amount into add (2)");
    // THE BAR SAYS WHETHER WHAT RUNS IS BEHIND THE DRAFT, not at which revision.
    // ⚔ MUTATION (tests/mutants/flow_view.md#bar-says-the-revision): the bar says the revision.
    m.filling.reset();
    m.running = true;
    m.running_definition = m.workspace.graph.project.definition;
    m.command("bind", {"1", "1", "$amount"});
    for (const auto& at : {room(), light_room()}) {
        const auto view = pane::picture(m, at);
        CHECK_MESSAGE(internal(view).empty(), internal(view));
        CHECK(text_has(view, "changed, not applied"));
    }
    m.command("say", {"Again", "total"});
    CHECK(text_has(pane::picture(m, light_room()), "changed, needs a restart"));
    CHECK(internal(pane::picture(m, light_room())).empty());

    // WHAT THE DRAFT LACKS, each by its step, its port or its message.
    // ⚔ MUTATION (tests/mutants/flow_view.md#lacks-in-the-graphs-words): said in the graph's words.
    pane::Model less;
    less.palette.push_back({"compare.less",
                            loom::SchemaBuilder("compare.less.in", 1).field("lhs", loom::Kind::Int)
                                .field("rhs", loom::Kind::Int).build(),
                            loom::SchemaBuilder("compare.less.out", 1).field("result", loom::Kind::Bool).build()});
    less.command("new", {"tally", "discard"});
    less.command("message", {"tally.Set"});
    less.command("keep", {"total", "Int"});
    less.command("add-node", {"compare.less",
                              std::to_string(static_cast<std::int64_t>(less.palette.front().inputs->content_id())),
                              std::to_string(static_cast<std::int64_t>(less.palette.front().outputs->content_id()))});
    CHECK(less.lacks().front() == "less's lhs has no value yet");
    less.command("bind-all", {"0", "$total", "$total"});
    REQUIRE(less.lacks().size() == 1);
    CHECK(less.lacks().front() == "less answers Bool, and total keeps Int");
    try {
        less.ready();
        FAIL("a draft that lacks something is not ready");
    } catch (const std::invalid_argument& e) {
        CHECK(std::string(e.what()) == "less answers Bool, and total keeps Int");
    }
    less.command("message", {"tally.Reset"});
    less.command("trigger", {"1", "total"});
    CHECK(less.lacks().back() == "tally.Reset has no step yet");
    // ...where the graph's own lines keep its words, for whoever reads them as text.
    CHECK(less.workspace.graph.problems(less.palette).front() ==
          "tally.Set / node 0: result type does not match state.total");
}

TEST_CASE("the rail breaks a row's words and a heading to two lines, never cutting the first, and a "
          "wire in hand asks for and lists what takes it, each port of its type a way to add it") {
    pane::Model m;
    const flow::Ports longer{"view.longer_name",
                             loom::SchemaBuilder("view.longer.in", 1).field("a_quite_long_port", loom::Kind::Int)
                                 .field("b", loom::Kind::Int).build(),
                             loom::SchemaBuilder("view.longer.out", 1).field("value", loom::Kind::Int).build()};
    m.palette.push_back(longer);
    m.command("state-field", {"total", "Int", "required"});
    m.command("message", {"Input"});
    m.command("message-field", {"0", "n", "Int", "required"});
    m.command("trigger", {"0", "total"});
    m.workspace.graph.add(0, longer);
    m.page = pane::Page::Graph;
    auto fits = found_row("view.longer_name", ws::kOperatorKind);
    fits.about = "the larger of two integers, kept";
    auto rambles = found_row("view.rambling", ws::kOperatorKind);
    rambles.about = "a description that runs on past the two lines a row of the rail has room for, and on";
    answered(m, {fits, rambles});
    const auto mark = [](const pane::Picture& view, const std::string& words) { return text_mark(view, words); };
    // ⚔ MUTATION (tests/mutants/flow_view.md#rail-row-says-one-line): a row's words stop at one line.
    auto view = pane::picture(m, light_room());
    CHECK(mark(view, "the larger of two integers,") != nullptr);
    CHECK(mark(view, "kept") != nullptr);
    bool first = false, cut = false;
    for (const auto& k : view.marks) {
        if (k.op != pane::Mark::Op::Text) continue;
        first = first || k.words == "a description that runs on";
        cut = cut || (k.words.rfind("past the two lines", 0) == 0 && k.words.size() >= 3 &&
                      k.words.substr(k.words.size() - 3) == "...");
    }
    CHECK(first);
    CHECK(cut);
    // A PORT CHOSEN: its heading breaks where a long name would be cut.
    // ⚔ MUTATION (tests/mutants/flow_view.md#heading-cut-to-one-line): a heading stops at one line.
    m.filling = pane::PortChoice{m.workspace.graph.place(0, 0).id, 0};
    view = pane::picture(m, light_room());
    CHECK(mark(view, "For longer_name's") != nullptr);
    CHECK(mark(view, "a_quite_long_port : Int") != nullptr);

    // A WIRE IN HAND FROM THE STEP'S ANSWER: the door is asked what takes its type, the rail heads
    // its list with it, and the preview offers each port of that type.
    m.filling.reset();
    m.connecting = zengine::op::Binding::node(0);
    CHECK(pane::flow_question(m).takes == std::optional<std::string>("Int"));
    CHECK_FALSE(pane::flow_question(m).yields.has_value());
    m.preview = "view.longer_name";
    m.preview_kind = ws::kOperatorKind;
    view = pane::picture(m, light_room());
    CHECK(text_has(view, "Wire from longer_name : Int"));
    std::vector<std::string> ports;
    for (const auto& hit : view.hits)
        if (hit.action == "add-from") ports.push_back(hit.args.at(2));
    CHECK(ports == std::vector<std::string>{"0", "1"});
    CHECK(text_has(view, "Add, wired into b"));
    // ...and a field held, by its name.
    m.connecting = zengine::op::Binding::input("n");
    CHECK(pane::flow_question(m).takes == std::optional<std::string>("Int"));
    CHECK(text_has(pane::picture(m, light_room()), "Wire from n : Int"));
}

TEST_CASE("the sentence and the graph light each other: a part rested on lights its cards, steps "
          "and wires, a card or a step its parts, and a part still to choose is pressed as Next is") {
    auto m = tally();
    // When, the message, arrives, total, becomes, the step, then says, tally.Total, with, total.
    REQUIRE(pane::sentence(m).size() == 10);
    const auto add = "step:" + std::to_string(pane::place_of(m, 0));
    const auto lhs = "wire:" + std::to_string(pane::place_of(m, 0)) + ".0";
    const auto rhs = "wire:" + std::to_string(pane::place_of(m, 0)) + ".1";
    const auto lit = [&](const std::string& rested) {
        m.hover = rested;
        return pane::picture(m, light_room()).lit;
    };
    // ⚔ MUTATION (tests/mutants/flow_view.md#a-part-lights-nothing): a part lights itself alone.
    // THE MESSAGE'S PART lights its card and the wire leaving its field.
    auto on = lit("chip:1");
    CHECK(on.count("chip:1") == 1);
    CHECK(on.count("card:message:tally.panel.Count") == 1);
    CHECK(on.count(lhs) == 1);
    CHECK(on.count(rhs) == 0);
    CHECK(on.count(add) == 0);
    // THE STEP'S PART lights the step and every wire into it.
    on = lit("chip:5");
    CHECK(on.count(add) == 1);
    CHECK(on.count(lhs) == 1);
    CHECK(on.count(rhs) == 1);
    CHECK(on.count("card:write:tally.panel.Count") == 0);
    // WHAT IT BECOMES lights what it writes and the wire into it.
    on = lit("chip:3");
    CHECK(on.count("card:write:tally.panel.Count") == 1);
    CHECK(on.count("wire:write") == 1);
    // ⚔ MUTATION (tests/mutants/flow_view.md#a-card-lights-no-part): a card lights no part.
    // A CARD OR A STEP RESTED ON lights its parts: what it keeps, every kept field's part.
    on = lit("card:kept");
    CHECK(on.count("chip:3") == 1);
    CHECK(on.count("chip:9") == 1);
    CHECK(on.count(rhs) == 1);
    CHECK(lit(add).count("chip:5") == 1);
    on = lit("card:say:tally.panel.Count:tally.Total");
    CHECK(on.count("chip:7") == 1);
    CHECK(on.count("chip:9") == 1);
    CHECK(on.count("chip:1") == 0);
    // WHERE THE POINTER RESTS: a part, a step's head, a port and a wire, each found at its place.
    m.hover.reset();
    const auto view = pane::picture(m, light_room());
    CHECK(view.lit.empty());
    const auto box_of = [&](const std::string& key) {
        for (const auto& rest : view.rests)
            if (rest.key == key) return rest.box;
        FAIL("nothing rests on " << key);
        return pane::Box{};
    };
    const auto middle = [](const pane::Box& b) { return std::pair{b.x + b.w / 2, b.y + b.h / 2}; };
    auto [cx, cy] = middle(box_of("chip:1"));
    CHECK(pane::rest_at(view, cx, cy) == std::optional<std::string>("chip:1"));
    const auto head = box_of(add);
    CHECK(pane::rest_at(view, head.x + 4, head.y + 4) == std::optional<std::string>(add));
    const auto port = "port:" + std::to_string(pane::place_of(m, 0)) + ".1";
    std::tie(cx, cy) = middle(box_of(port));
    CHECK(pane::rest_at(view, cx, cy) == std::optional<std::string>(port));
    const auto* line = &view.lines.front();
    for (const auto& l : view.lines)
        if (l.key == lhs) line = &l;
    REQUIRE(line->key == lhs);
    const auto& [wx, wy] = line->points.at(line->points.size() / 2);
    CHECK(pane::rest_at(view, wx, wy) == std::optional<std::string>(lhs));
    // A PART STILL TO CHOOSE is pressed; a chosen one is not.
    pane::Model blank;
    blank.command("new", {"tally", "discard"});
    blank.page = pane::Page::Graph;
    const auto open = pane::picture(blank, light_room());
    CHECK(std::any_of(open.hits.begin(), open.hits.end(), [](const auto& hit) {
        return hit.action == "chip" && hit.args == std::vector<std::string>{"1"};
    }));
    CHECK_FALSE(std::any_of(view.hits.begin(), view.hits.end(), [](const auto& hit) { return hit.action == "chip"; }));
}

TEST_CASE("resting tells: a port its kind and what it reads, an answer or a field what reads it, a "
          "wire its two ends lit and what it last carried; a wire in the hand says why a port does not fit") {
    auto m = tally();
    const auto id = std::to_string(pane::place_of(m, 0));
    const auto told = [&](const std::string& key) {
        m.hover = key;
        return pane::view_detail::told(m);
    };
    using Lines = std::vector<std::string>;
    // ⚔ MUTATION (tests/mutants/flow_view.md#a-port-tells-nothing): a port tells nothing.
    CHECK(told("port:" + id + ".0") == Lines{"lhs : Int", "reads amount, from the message"});
    CHECK(told("port:" + id + ".1") == Lines{"rhs : Int", "reads total, kept"});
    CHECK(told("answer:" + id) == Lines{"add's answer : Int", "total becomes it"});
    CHECK(told("field:amount") == Lines{"amount : Int, from the message", "read by add's lhs"});
    CHECK(told("field:total") == Lines{"total : Int, kept", "read by add's rhs"});
    CHECK(told("card:kept").empty());
    // A WIRE: its two ends, lit, and what it last carried -- nothing until the rule took a message.
    CHECK(told("wire:" + id + ".0") == Lines{"amount -> add's lhs", "nothing carried yet"});
    // ⚔ MUTATION (tests/mutants/flow_view.md#a-wires-ends-unlit): its ends are not lit.
    m.hover = "wire:" + id + ".0";
    const auto lit = pane::picture(m, light_room()).lit;
    CHECK(lit.count("wire:" + id + ".0") == 1);
    CHECK(lit.count("field:amount") == 1);
    CHECK(lit.count("port:" + id + ".0") == 1);
    CHECK(lit.count("port:" + id + ".1") == 0);
    const auto& def = m.workspace.graph.project.definition;
    loom::Value start(def.state);
    start.set("total", loom::Cell::integer(0));
    fh::FlowAnswer answer;
    answer.events = {event(1, "arrived", valued(def.accepts.front(), "amount", 3), def.accepts.front()->name()),
                     event(2, "kept", valued(def.state, "total", 3), "after #1")};
    m.observed = pane::observe(answer, def, start);
    CHECK(told("wire:" + id + ".0") == Lines{"amount -> add's lhs", "last carried 3"});
    CHECK(told("wire:" + id + ".1") == Lines{"total -> add's rhs", "last carried 0"});
    CHECK(told("wire:write") == Lines{"add's answer -> total", "last carried 3"});
    // THE BOX stands where the pointer came to rest, in both rooms.
    m.hover = "port:" + id + ".0";
    m.hover_x = 600;
    m.hover_y = 300;
    CHECK(text_has(pane::picture(m, light_room()), "reads amount, from the message"));
    m.hover_x = 60 * cell;
    m.hover_y = 20 * cell;
    CHECK(text_has(pane::picture(m, room()), "reads amount, from the message"));
    // ...A PART OF ITS OWN, painted after every part it lies over and carrying its own words; a
    // pointer inside it rests on nothing.
    // ⚔ MUTATION (tests/mutants/flow_view.md#the-box-has-no-part): the box is no part, and the parts
    // beneath it paint over it.
    for (const auto& rest : pane::picture(m, light_room()).rests)
        if (rest.key == "port:" + id + ".0") {
            m.hover_x = rest.box.x + rest.box.w / 2;
            m.hover_y = rest.box.y + rest.box.h / 2;
        }
    const auto drawn = pane::window_picture(m, light_room());
    REQUIRE(drawn.picture.tip.has_value());
    std::vector<std::string> named;
    std::string group;
    for (const auto& r : drawn.sketch.records()) {
        named.push_back(r.action);
        if (r.action == "tip:box") group = r.path;
    }
    REQUIRE_FALSE(named.empty());
    CHECK(named.back() == "tip:box");
    CHECK(std::any_of(drawn.sketch.nodes().begin(), drawn.sketch.nodes().end(), [&](const auto& n) {
        return n.op == "text" && !group.empty() && n.path.rfind(group + "/", 0) == 0;
    }));
    const auto box = *drawn.picture.tip;
    CHECK_FALSE(pane::rest_at(drawn.picture, box.x + 2, box.y + 2).has_value());
    CHECK_FALSE(box.holds(pane::Box{m.hover_x, m.hover_y, 1, 1}));

    // A WIRE IN THE HAND over a port it does not fit: a later step, or another type, said there.
    auto two = tally();
    const flow::Ports less{"compare.less",
                           loom::SchemaBuilder("compare.less.in", 1).field("lhs", loom::Kind::Int)
                               .field("rhs", loom::Kind::Int).build(),
                           loom::SchemaBuilder("compare.less.out", 1).field("result", loom::Kind::Bool).build()};
    two.palette.push_back(less);
    two.command("add-node", {"compare.less", std::to_string(static_cast<std::int64_t>(less.inputs->content_id())),
                             std::to_string(static_cast<std::int64_t>(less.outputs->content_id())), "0"});
    const pane::PortChoice add_lhs{pane::place_of(two, 1), 0}, less_lhs{pane::place_of(two, 0), 0};
    // ⚔ MUTATION (tests/mutants/flow_view.md#another-type-fits): a wire of another type fits.
    CHECK(pane::unfit(two, zengine::op::Binding::input("amount"), add_lhs).empty());
    CHECK(pane::unfit(two, zengine::op::Binding::node(0), add_lhs) == "add's lhs takes Int, and less is Bool");
    CHECK(pane::unfit(two, zengine::op::Binding::node(1), less_lhs) ==
          "less's lhs can't read add: a step reads only the steps before it");
    two.wire_drawn = pane::WireDrawn{10, 10, 600, 300, add_lhs, pane::unfit(two, zengine::op::Binding::node(0), add_lhs)};
    CHECK(text_has(pane::picture(two, light_room()), "add's lhs takes Int, and less is Bool"));
}

TEST_CASE("what the last message changed stays lit while the rule runs, until the next: the wires "
          "whose values changed, the steps they reach, what it wrote where the kept value changed, what it said") {
    auto m = tally();
    m.running = true;
    m.running_definition = m.workspace.graph.project.definition;
    const auto& def = m.workspace.graph.project.definition;
    const auto count = def.accepts.front(), total = def.emits.front();
    const auto id = std::to_string(pane::place_of(m, 0));
    loom::Value start(def.state);
    start.set("total", loom::Cell::integer(0));
    std::vector<fh::FlowEvent> journal;
    std::int64_t sequence = 0;
    const auto message = [&](std::int64_t amount, std::int64_t kept) {
        const auto at = ++sequence;
        const auto after = "after #" + std::to_string(at);
        journal.push_back(event(at, "arrived", valued(count, "amount", amount), count->name()));
        journal.push_back(event(++sequence, "kept", valued(def.state, "total", kept), after));
        journal.push_back(event(++sequence, "output", valued(total, "total", kept), total->name() + " " + after));
        fh::FlowAnswer answer;
        answer.events = journal;
        m.observed = pane::observe(answer, def, start);
        return pane::picture(m, light_room()).happened;
    };
    using Keys = std::set<std::string>;
    const std::string came = "card:message:tally.panel.Count", wrote = "card:write:tally.panel.Count",
                      said = "card:say:tally.panel.Count:tally.Total";
    // ⚔ MUTATION (tests/mutants/flow_view.md#the-last-message-lights-nothing): nothing stays lit.
    // THE FIRST: everything it reached changed.
    CHECK(message(3, 3) == Keys{came, "wire:" + id + ".0", "wire:" + id + ".1", "step:" + id, wrote, "wire:write", said});
    // ⚔ MUTATION (tests/mutants/flow_view.md#an-unchanged-wire-lights): a wire that carried what it
    // carried before is lit.
    // THE SAME AMOUNT AGAIN: its wire carried what it carried before; the rest changed.
    CHECK(message(3, 6) == Keys{came, "wire:" + id + ".1", "step:" + id, wrote, "wire:write", said});
    // ⚔ MUTATION (tests/mutants/flow_view.md#an-unchanged-write-lights): what it wrote is lit though
    // what it keeps stayed.
    // AN AMOUNT OF NOTHING: the answer is the one before, so neither what it wrote nor its wire is lit.
    CHECK(message(0, 6) == Keys{came, "wire:" + id + ".0", "wire:" + id + ".1", "step:" + id, said});
    // STOPPED, nothing stays lit.
    m.running = false;
    CHECK(pane::picture(m, light_room()).happened.empty());
}

TEST_CASE("the then thread from what the rule writes runs on, through the dashed place for a say, onto "
          "its + say button") {
    auto m = tally(false);
    const auto view = pane::picture(m, light_room());
    const auto button = std::find_if(view.hits.begin(), view.hits.end(),
                                     [](const auto& hit) { return hit.action == "ask-say"; });
    REQUIRE(button != view.hits.end());
    const auto thread = pane::hue::thread();
    std::vector<pane::Box> runs;
    for (const auto& k : view.marks)
        if (k.op == pane::Mark::Op::Rect && k.w == 2 && k.fill.role == thread.role && k.fill.rgb == thread.rgb)
            runs.push_back(k.bounds());
    // ⚔ MUTATION (tests/mutants/flow_view.md#thread-stops-at-the-place): the thread stops at the
    // dashed place's edge.
    // ⚔ MUTATION (tests/mutants/flow_view.md#say-button-beside-the-thread): the button stands in
    // the middle of the place, beside the thread.
    // ONE RUN OF IT ENDS ON THE BUTTON'S TOP EDGE, within the button's width.
    CHECK(std::any_of(runs.begin(), runs.end(), [&](const pane::Box& r) {
        return r.bottom() == button->y && r.x >= button->x && r.right() <= button->x + button->w;
    }));
    // AND THE RUNS JOIN: each starts where one before it ends, from the card down.
    std::sort(runs.begin(), runs.end(), [](const pane::Box& a, const pane::Box& b) { return a.y < b.y; });
    REQUIRE(runs.size() >= 2);
    for (std::size_t i = 1; i < runs.size(); ++i) CHECK(runs[i].y == runs[i - 1].bottom());
}

TEST_CASE("a port's dot takes a press and a rest where it is drawn, the half outside its card's row too, "
          "doing what its row does") {
    auto m = tally();
    const auto view = pane::picture(m, light_room());
    const auto id = std::to_string(pane::place_of(m, 0));
    // Each port's dot middle, as the marks draw it: a dot 10 wide ringed in the ink of wires or of
    // what is kept.
    const auto ringed = [](const gfx::Paint& p, const gfx::Paint& q) { return p.role == q.role && p.rgb == q.rgb; };
    std::vector<std::pair<std::int64_t, std::int64_t>> dots;
    for (const auto& k : view.marks)
        if (k.op == pane::Mark::Op::Ellipse && k.w == 10 && k.h == 10 &&
            (ringed(k.stroke, pane::hue::wire()) || ringed(k.stroke, pane::hue::kept())))
            dots.emplace_back(k.x + 5, k.y + 5);
    REQUIRE(dots.size() >= 5);
    const auto pressed = [&](std::int64_t x, std::int64_t y) { return view.hit(x, y); };
    const auto rest = [&](std::int64_t x, std::int64_t y) {
        for (auto r = view.rests.rbegin(); r != view.rests.rend(); ++r)
            if (r->box.holds(pane::Box{x, y, 1, 1})) return r->key;
        return std::string();
    };
    // ⚔ MUTATION (tests/mutants/flow_view.md#dots-reach-their-row-only): a dot takes a press only
    // where it overlaps its card's row.
    // EVERY DOT: its left and right edges take the press its middle does, and a rest there names
    // the dot.
    for (const auto& [x, y] : dots) {
        const auto* middle = pressed(x, y);
        REQUIRE(middle != nullptr);
        for (const auto dx : {-4, 4}) {
            const auto* edge = pressed(x + dx, y);
            REQUIRE(edge != nullptr);
            CHECK(edge->action == middle->action);
            CHECK(edge->args == middle->args);
            CHECK_FALSE(rest(x + dx, y).empty());
        }
    }
    // THE DOTS AND WHAT THEY DO: the step's ports, its answer, the message's field, the kept field,
    // what the rule writes.
    std::set<std::string> actions;
    for (const auto& [x, y] : dots) actions.insert(pressed(x + 4, y)->action);
    CHECK(actions == std::set<std::string>{"port", "source-node", "field", "write"});
    CHECK(std::any_of(dots.begin(), dots.end(), [&](const auto& d) { return rest(d.first + 4, d.second) == "answer:" + id; }));
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
                              "add", "lhs", "rhs", "answer", "becomes the answer", "now 3", "say",
                              "tally.Total", "total = 3", "Events", "newest first", "Done."})
        CHECK_MESSAGE(text_has(light.picture, words), std::string(words));
    CHECK_FALSE(text_has(light.picture, "math.add"));
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
    // ...its wires, every one the message just changed, in the done colour until the next.
    for (const std::int64_t rgb : {0x262b35, 0x1a1d24, 0x242629, 0x20252d, 0x20252c, 0x323846, 0xa3be8c})
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
