// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#include "doctest.h"
#include "flow/shape.hpp"
#include "input/vocabulary.hpp"
#include "inventory/codec.hpp"
#include "lifecycle_door.hpp"
#include "maker/weave.hpp"
#include "maker_fixture.hpp"
#include "message-draft/transfer.hpp"
#include "view-builder/model.hpp"
#include "view-builder/picture.hpp"
#include "view-builder/vocabulary.hpp"
#include "view/host.hpp"
#include "workshop/pane_carry.hpp"
#include "workshop/pane_operation.hpp"
#include "workshop/pane_vocabulary.hpp"

#include <zen/kernel/kernel.hpp>

#include <chrono>
#include <filesystem>
#include <set>

namespace {
namespace vb = zengine::view_builder;
namespace view = zengine::view;
namespace ws = zengine::workshop;
namespace shape = zengine::flow::shape;
constexpr const char* workshop_role = "zengine.workshop";
constexpr auto unit = ws::kPaneCanvasUnit;

std::shared_ptr<const loom::Schema> total_shape() {
    return loom::SchemaBuilder("tally.Total", 1).field("total", loom::Kind::Int).build();
}

/// The walk's first step, as edits: three number fields, a button, a label, and the intent made
/// from the fields.
std::vector<std::vector<std::string>> panel_edits() {
    return {{"new", "tally.panel", "discard"},
            {"add", "number"}, {"element", "0", "start", "start", "0", "0", "144", "24", "0"},
            {"add", "number"}, {"element", "1", "limit", "limit", "0", "28", "144", "24", "10"},
            {"add", "number"}, {"element", "2", "step", "step", "0", "56", "144", "24", "1"},
            {"add", "button"}, {"element", "3", "count", "Count", "0", "84", "96", "24"},
            {"add", "label"}, {"element", "4", "total", "Total", "0", "112", "192", "24"},
            {"intent", "3", "Count"}};
}

vb::Model panel_model() {
    vb::Model m;
    for (const auto& edit : panel_edits())
        m.command(edit.front(), std::vector<std::string>(edit.begin() + 1, edit.end()));
    return m;
}

struct TempDir {
    std::filesystem::path directory;
    TempDir() {
        directory = std::filesystem::temp_directory_path() /
                    ("zengine-view-builder-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directory(directory);
    }
    ~TempDir() {
        std::error_code ignored;
        std::filesystem::remove_all(directory, ignored);
    }
};
} // namespace

TEST_CASE("the View Builder makes a view from its kinds: number fields, a button, a label, and an intent made from the fields through the one shape model") {
    const auto m = panel_model();
    const auto& d = m.description;
    CHECK(d.name == "tally.panel");
    REQUIRE(d.elements.size() == 5);
    CHECK(d.elements[2].text == "1");
    CHECK(d.elements[3].kind == view::Kind::button);
    REQUIRE(d.intents.size() == 1);
    auto made = shape::make(shape::qualified("tally.panel", "Count"));
    for (const char* f : {"start", "limit", "step"})
        made = shape::with_field(*made, f, loom::type_of(loom::Kind::Int));
    CHECK(loom::same_identity(*d.intents[0].shape, *made));
    CHECK(loom::same_identity(*d.intents[0].shape, *hwfix::count_schema()));
    CHECK(d.intents[0].control == "count");
    CHECK(view::problem(d).empty());
    CHECK(m.dirty);
    // A new element lands below the last, and a fresh id is its kind's.
    auto more = m;
    more.command("add", {"label"});
    CHECK(more.description.elements.back().id == "label1");
    CHECK(more.description.elements.back().y == 140);
}

namespace {
/// A window's room and a terminal's, each as Workshop grants it: subunits, the medium's grain and
/// its measured text.
ws::PaneCanvasRoom window_room(std::int64_t grant = 1) {
    return {vb::kPane, grant, zengine::surface::subs_of_pixel(1320), zengine::surface::subs_of_pixel(640),
            zengine::surface::kPixelGrainSubs, true, 8, 16};
}
ws::PaneCanvasRoom terminal_room(std::int64_t grant = 1) { return {vb::kPane, grant, 110 * unit, 30 * unit, unit, false}; }
} // namespace

TEST_CASE("the design canvas is the view's own picture at its own pixels, moved there whole; the builder marks the selected element and its handles over it") {
    auto m = panel_model();
    m.selected = 4;
    for (const auto& room : {window_room(), terminal_room()}) {
        INFO("graphical: " << room.graphical);
        vb::Presentation none;
        const auto pic = vb::picture(m, none, room, 1);
        const auto& area = pic.design;
        REQUIRE_FALSE(area.empty());
        // On a cell boundary, so the view keeps its lattice in a window and a terminal alike.
        CHECK(area.x % unit == 0);
        CHECK(area.y % unit == 0);
        // ONE RENDERER: everything the view's own picture code draws in a room the area's size is
        // in the builder's picture, moved by the area's corner and nothing else.
        const ws::PaneCanvasRoom inner{vb::kPane, room.grant, area.w, area.h, room.grain, room.graphical,
                                       room.text_advance_px, room.text_line_px};
        const auto own = view::picture(m.description, {}, view::Presentation{}, inner, 1);
        REQUIRE_FALSE(own.content.texts.empty());
        for (const auto& t : own.content.texts) {
            const auto moved = std::count_if(pic.content.texts.begin(), pic.content.texts.end(), [&](const auto& u) {
                return u.text == t.text && u.role == t.role && u.x == t.x + area.x && u.y == t.y + area.y;
            });
            CHECK_MESSAGE(moved == 1, t.text);
        }
        for (const auto& r : own.content.rects) {
            const auto moved = std::count_if(pic.content.rects.begin(), pic.content.rects.end(), [&](const auto& u) {
                return u.role == r.role && u.x == r.x + area.x && u.y == r.y + area.y && u.w == r.w && u.h == r.h;
            });
            CHECK(moved >= 1);
        }
        // ...and no element is drawn twice on the canvas: the builder only marks it.
        CHECK(std::count_if(pic.content.texts.begin(), pic.content.texts.end(),
                            [&](const auto& t) { return t.text == "Total" && t.x >= area.x && t.x < area.x + area.w; }) == 1);
        // Each element is pressed where the view draws it, at its own pixels.
        for (std::size_t i = 0; i < m.description.elements.size(); ++i) {
            const auto at = vb::element_area(area, m.description.elements[i]);
            CHECK(at.x == area.x + zengine::surface::subs_of_pixel(m.description.elements[i].x));
            const auto* hit = pic.hit(at.x + at.w / 2, at.y + at.h / 2);
            REQUIRE(hit != nullptr);
            CHECK(hit->action == "element");
            CHECK(hit->args.at(0) == std::to_string(i));
        }
        // THE SELECTED ELEMENT: marked, with a handle at each corner, and its values in boxes.
        std::vector<std::string> handles, boxes;
        for (const auto& hit : pic.hits) {
            if (hit.action == "handle") handles.push_back(hit.args.at(1));
            if (hit.action == "box" && hit.args.at(1) == "4") boxes.push_back(hit.args.at(0));
        }
        CHECK(handles == std::vector<std::string>{"0", "1", "2", "3"});
        CHECK(boxes == std::vector<std::string>{"id", "label", "x", "y", "w", "h"});
        const auto total = vb::element_area(area, m.description.elements[4]);
        CHECK(std::any_of(pic.content.rects.begin(), pic.content.rects.end(), [&](const auto& r) {
            return r.role == zengine::surface::role::kAccent && r.y + r.h == total.y && r.x <= total.x;
        }));
        const auto listed = [&](const std::string& start) {
            return std::any_of(pic.content.texts.begin(), pic.content.texts.end(),
                               [&](const auto& t) { return t.text.rfind(start, 0) == 0 && t.x < area.x; });
        };
        CHECK(listed("> total  label"));
        CHECK(listed("  start  number"));
    }
    // An empty view says what to do, in the list's place.
    vb::Presentation none;
    const auto empty = vb::picture(vb::Model{}, none, terminal_room(), 2);
    CHECK(std::any_of(empty.content.texts.begin(), empty.content.texts.end(),
                      [](const auto& t) { return t.text == "  none yet: drag a kind in"; }));
}

TEST_CASE("a shape or a field carried onto a label is what it shows; a shape of several fields asks which") {
    auto m = panel_model();
    m.carried(4, total_shape(), std::nullopt);
    REQUIRE(m.description.shows.size() == 1);
    CHECK(m.description.shows[0].field == "total");
    CHECK(m.notice == "`total` shows tally.Total.total");

    const auto pair = loom::SchemaBuilder("tally.Pair", 1).field("a", loom::Kind::Int).field("b", loom::Kind::Text).build();
    m.carried(4, pair, std::nullopt);
    REQUIRE(m.choosing.has_value());
    CHECK(m.description.shows[0].shape->name() == "tally.Total"); // nothing changed until chosen
    m.command("show", {"4", "b"});
    CHECK(m.description.shows[0].shape->name() == "tally.Pair");
    CHECK(m.description.shows[0].field == "b");
    CHECK_FALSE(m.choosing.has_value());

    m.carried(4, pair, std::string("a"));
    CHECK(m.description.shows[0].field == "a");
    CHECK_THROWS_WITH(m.carried(3, total_shape(), std::nullopt),
                      "`count` is a button; carry a shape onto a label to show one of its fields");
    CHECK_THROWS_WITH(m.command("show", {"4", "a"}), "carry a shape or a field onto `total` first");
}

TEST_CASE("an edit that would break a view's rules is refused whole, and the draft is as it was") {
    auto m = panel_model();
    const auto before = view::description_bytes(m.description);
    CHECK_THROWS_WITH(m.command("element", {"0", "1st", "start", "0", "0", "144", "24", "0"}),
                      doctest::Contains("`1st` is not"));
    CHECK_THROWS_WITH(m.command("element", {"0", "start", "start", "-4", "0", "144", "24", "0"}),
                      doctest::Contains("whole pixels"));
    CHECK_THROWS_WITH(m.command("element", {"0", "start", "start", "x", "0", "144", "24", "0"}),
                      "x is a whole number; `x` is not");
    CHECK_THROWS_WITH(m.command("intent", {"3", "other.Count"}),
                      "an intent's name stays inside the view's: write it without a `.`");
    CHECK_THROWS_WITH(m.command("intent", {"4", "Count"}), "`total` is a label; only a button says an intent");
    // A value typed into one box, and a drag's place, are whole edits too.
    CHECK_THROWS_WITH(m.command("set", {"0", "x", "-4"}), doctest::Contains("whole pixels"));
    CHECK_THROWS_WITH(m.command("set", {"0", "w", "wide"}), "w is a whole number; `wide` is not");
    CHECK_THROWS_WITH(m.command("set", {"3", "text", "9"}), "`count` is a button; only a number field starts with text");
    CHECK_THROWS_WITH(m.command("set", {"0", "colour", "red"}), "an element's values are id, label, text, x, y, w and h");
    CHECK_THROWS_WITH(m.command("place", {"0", "0", "0", "0", "24"}), doctest::Contains("whole pixels"));
    CHECK_THROWS_WITH(m.command("set", {"1", "id", "start"}), "two elements are both `start`");
    CHECK(view::description_bytes(m.description) == before);
    m.command("set", {"0", "label", "From"});
    CHECK(m.description.elements[0].label == "From");
    m.command("place", {"0", "12", "6", "150", "30"});
    CHECK(m.description.elements[0].x == 12);
    CHECK(m.description.elements[0].h == 30);
    // A kind made where it was dropped, in whole pixels.
    auto dropped = m;
    dropped.command("add", {"button", "200", "36"});
    CHECK(dropped.description.elements.back().x == 200);
    CHECK(dropped.description.elements.back().y == 36);
    CHECK(dropped.description.elements.back().w == 96);

    // Renaming an element keeps what names it; an intent field keeps its own name.
    m.command("element", {"0", "first", "first", "0", "0", "144", "24", "0"});
    CHECK(m.description.intents[0].fields[0].element == "first");
    CHECK(m.description.intents[0].fields[0].field == "start");
    // Renaming the view makes each intent again inside the new name.
    m.command("rename", {"counter"});
    CHECK(m.description.intents[0].shape->name() == "counter.Count");
    // Removing a number field drops the intent made from it, and says so.
    m.command("remove", {"0"});
    CHECK(m.description.intents.empty());
    CHECK(m.notice == "Removed first and the intent it was part of: counter.Count");
}

TEST_CASE("a view saves and opens whole, and nothing running is saved") {
    TempDir dir;
    auto m = panel_model();
    m.carried(4, total_shape(), std::nullopt);
    m.running = true;
    const auto path = (dir.directory / "tally.view").string();
    m.command("save", {path});
    CHECK_FALSE(m.dirty);
    vb::Model again;
    again.command("open", {path, "discard"});
    CHECK(view::description_bytes(again.description) == view::description_bytes(m.description));
    CHECK_FALSE(again.running);
    CHECK_FALSE(again.dirty);
    for (const auto& f : view::description_schema()->fields()) {
        CHECK(f.name != "running");
        CHECK(f.name != "session");
    }
    CHECK_THROWS_WITH(again.command("open", {(dir.directory / "missing.view").string(), "discard"}),
                      doctest::Contains("cannot read"));
}

namespace {
/// Workshop's office and its menu presenter, on the real bus: what the builder and its view say,
/// and the answers the builder needs to carry.
class Desk final : public loom::Weave {
public:
    std::vector<loom::Message> heard;
    std::vector<ws::PaneCanvasContent> pictures;
    bool allow = true;
    loom::WeaveId self{};
    std::vector<std::shared_ptr<const loom::Schema>> accepted_schemas() const override {
        return {loom::schema_of<ws::v2::PaneOffered>(), loom::schema_of<ws::PaneActions>(),
                loom::schema_of<ws::PaneContent>(), loom::schema_of<ws::PaneCanvasContent>(),
                loom::schema_of<ws::PaneEscapeUnspent>(), loom::schema_of<ws::PanePassRequested>(),
                loom::schema_of<ws::PaneRevealRequested>(), loom::schema_of<ws::PaneMenuRequested>(),
                loom::schema_of<ws::PaneOperationRequested>(), loom::schema_of<ws::PaneValueCarryRequested>(),
                loom::schema_of<ws::PaneQuitAnswered>(), loom::schema_of<vb::ViewEdited>()};
    }
    void handle(const loom::Message& in, loom::Bus& bus) override {
        if (loom::same_identity(in.payload.schema(), *loom::schema_of<ws::PaneOperationRequested>()))
            (void)bus.answer(loom::Message(loom::to_value(ws::PaneOperationAnswered{allow, allow ? "" : "not now"}), self));
        if (loom::same_identity(in.payload.schema(), *loom::schema_of<ws::PaneCanvasContent>()) &&
            in.provenance.authored_role() == vb::kRole)
            pictures.push_back(loom::from_value<ws::PaneCanvasContent>(in.payload));
        heard.push_back(in);
    }
    loom::Value snapshot() const override { return loom::Value(loom::make_schema("vbtest.Desk", 1, {})); }
    loom::Value policy() const override { return zengine::maker::default_value(loom::lifecycle_policy_schema()); }
    void revive(const loom::Value&) override {}
};

class Menus final : public loom::Weave {
public:
    std::vector<std::shared_ptr<const loom::Schema>> accepted_schemas() const override { return {}; }
    void handle(const loom::Message&, loom::Bus&) override {}
    loom::Value snapshot() const override { return loom::Value(loom::make_schema("vbtest.Menus", 1, {})); }
    loom::Value policy() const override { return zengine::maker::default_value(loom::lifecycle_policy_schema()); }
    void revive(const loom::Value&) override {}
};

struct Rig {
    zengine::op::Catalog catalog;
    loom::Switchboard bus;
    loom::Kernel kernel{bus, loom::trust_every_artifact("the fixture loads one View Builder artifact")};
    view::Host views{bus};
    Desk* desk = nullptr;
    loom::WeaveId workshop{}, menus{}, pane{}, door{};
    zengine::maker::Registered tally;
    std::uint64_t correlation = 0;
    std::int64_t grant = 1;

    Rig() {
        zengine::op::publish_primitives(catalog);
        views.mount();
        auto d = std::make_unique<Desk>();
        desk = d.get();
        workshop = bus.register_weave(std::move(d), loom::Grant{}.allow_any(), workshop_role);
        desk->self = workshop;
        menus = bus.register_weave(std::make_unique<Menus>(), loom::Grant{}.allow_any(), ws::kPresenterRole);
        tally = zengine::maker::register_definition(bus, catalog, hwfix::tally(catalog));
        REQUIRE(tally.ok);
        const auto loaded = kernel.load("view-builder", VIEW_BUILDER_ARTIFACT, vb::kRole, loom::Grant{}.allow_any());
        INFO(loaded.error);
        REQUIRE(loaded.ok);
        pane = loaded.id;
        door = zengine::testing::mount_door(bus);
        zengine::testing::order_activation(bus, door, pane, 1);
        pump();
        host(window_room(grant));
    }
    void pump() {
        for (int n = 0; n < 64 && bus.pending() != 0; ++n) bus.pump_pending();
        REQUIRE(bus.pending() == 0);
    }
    template <class T> void host(const T& value, std::uint64_t corr = 0, const char* to = vb::kRole) {
        REQUIRE(bus.office_send_to_role_as(workshop, workshop_role, to,
                                           loom::Message(loom::to_value(value), workshop, {}, corr)).valid());
        pump();
    }
    vb::ViewEdited edit(const std::string& action, std::vector<std::string> args = {}) {
        const auto corr = ++correlation;
        host(vb::ViewEdit{action, std::move(args)}, corr);
        for (auto it = desk->heard.rbegin(); it != desk->heard.rend(); ++it)
            if (it->correlation == corr && loom::same_identity(it->payload.schema(), *loom::schema_of<vb::ViewEdited>()))
                return loom::from_value<vb::ViewEdited>(it->payload);
        FAIL("no answer to the edit");
        return {};
    }
    const ws::PaneCanvasContent& picture() const {
        REQUIRE_FALSE(desk->pictures.empty());
        return desk->pictures.back();
    }
    const ws::PaneCanvasText* text(const std::string& start) const {
        for (const auto& t : picture().texts)
            if (t.text.rfind(start, 0) == 0) return &t;
        return nullptr;
    }
    std::string notice() const { return picture().texts.back().text; }
    void press(const std::string& start, std::int64_t button = 1) {
        const auto* t = text(start);
        REQUIRE_MESSAGE(t != nullptr, start);
        ++correlation;
        host(ws::PaneCanvasPointer{vb::kPane, grant, picture().picture, static_cast<std::int64_t>(correlation),
                                   ws::canvas_pointer::kPress, button, t->x + 4, t->y + 4}, correlation);
    }
    /// A pointer event as Workshop sends it, at a local place on the latest picture; a press
    /// carries a fresh number, which an acquisition it begins echoes.
    void pointer(std::int64_t phase, std::int64_t x, std::int64_t y, std::int64_t gesture, std::int64_t button = 1) {
        const auto corr = phase == ws::canvas_pointer::kPress ? ++correlation : 0;
        host(ws::PaneCanvasPointer{vb::kPane, grant, picture().picture, gesture, phase, button, x, y}, corr);
    }
    void hover(std::int64_t x, std::int64_t y, bool over = true, bool carrying = false) {
        host(ws::PaneCanvasHover{vb::kPane, grant, picture().picture, x, y, over, carrying});
    }
    /// The design area, read off the latest picture: the ground the view's own picture brought.
    vb::Area design() const {
        for (const auto& r : picture().rects)
            if (r.role == zengine::surface::role::kGround && (r.x != 0 || r.y != 0)) return {r.x, r.y, r.w, r.h};
        FAIL("no design area in the picture");
        return {};
    }
    /// The middle of element `i` on the design canvas, at its own pixels.
    std::pair<std::int64_t, std::int64_t> middle(const view::Element& e) const {
        const auto at = vb::element_area(design(), e);
        return {at.x + at.w / 2, at.y + at.h / 2};
    }
    /// The description the builder holds now, read from what it keeps across a reload.
    view::Description now() {
        const auto admitted = loom::admit(loom::parse(bus.snapshot_bytes(pane)), loom::schema_of<vb::BuilderState>());
        REQUIRE(admitted);
        const auto state = loom::from_value<vb::BuilderState>(admitted.value());
        auto read = view::read_description(std::string_view(
            reinterpret_cast<const char*>(state.description.data()), state.description.size()));
        REQUIRE_MESSAGE(read, read.reason);
        return read.description;
    }
    /// A line beside the design area that is exactly `words`: a value in its box.
    const ws::PaneCanvasText* value(const std::string& words) const {
        const auto area = design();
        for (const auto& t : picture().texts)
            if (t.text == words && (t.x < area.x || t.x >= area.x + area.w)) return &t;
        return nullptr;
    }
    /// Is the element at `at` outlined in `role`, `thick` subunits deep: its bottom edge's rule.
    bool marked(std::int64_t role, const vb::Area& at, std::int64_t thick) const {
        return std::any_of(picture().rects.begin(), picture().rects.end(), [&](const auto& r) {
            return r.role == role && r.y == at.y + at.h && r.h == thick && r.x <= at.x && r.x + r.w >= at.x + at.w;
        });
    }
    void drop(const loom::Value& value, const std::string& start) {
        const auto* t = text(start);
        REQUIRE_MESSAGE(t != nullptr, start);
        const auto bytes = zengine::inventory::encode_pair(value, {});
        host(ws::PaneCanvasValueDrop{vb::kPane, grant, picture().picture, t->x + 4, t->y + 4,
                                     loom::Bytes(bytes.begin(), bytes.end()), "zengine.flow", "flow", ""});
    }
};
} // namespace

TEST_CASE("the View Builder runs its view through the view host, applies a label in place, registers afresh when its shapes change, and stops it") {
    Rig rig;
    for (const auto& e : panel_edits()) {
        const auto answer = rig.edit(e.front(), std::vector<std::string>(e.begin() + 1, e.end()));
        REQUIRE_MESSAGE(answer.ok, answer.reason);
    }
    REQUIRE(rig.edit("run").ok);
    const auto first = rig.bus.role_holder("tally.panel");
    REQUIRE(first.valid());
    CHECK(first != rig.pane);
    CHECK(rig.text("tally.panel / running") != nullptr);
    CHECK(rig.notice().find("registered tally.panel") == 0);
    // The builder lists its elements, and its design canvas is the view's own picture.
    CHECK(rig.text("Elements (5)") != nullptr);
    CHECK(rig.text("Total") != nullptr);

    // Bind Total by carrying tally.Total's description onto its row: the shapes change.
    REQUIRE(rig.edit("select", {"4"}).ok);
    rig.drop(loom::encode_schema(*total_shape()), "> total  label");
    CHECK(rig.text("shows tally.Total.total") != nullptr);
    REQUIRE(rig.edit("apply").ok);
    const auto second = rig.bus.role_holder("tally.panel");
    CHECK(second != first);
    CHECK(rig.notice().find("registered tally.panel afresh") == 0);

    // A label change later is applied in place.
    REQUIRE(rig.edit("element", {"4", "total", "Sum", "0", "112", "192", "24"}).ok);
    REQUIRE(rig.edit("apply").ok);
    CHECK(rig.bus.role_holder("tally.panel") == second);
    CHECK(rig.notice().find("applied in place") == 0);

    REQUIRE(rig.edit("stop").ok);
    CHECK_FALSE(rig.bus.role_holder("tally.panel").valid());
    CHECK(rig.text("tally.panel / stopped") != nullptr);
    // A second stop is the host's refusal, said by the builder.
    REQUIRE(rig.edit("stop").ok);
    CHECK(rig.notice().find("stop refused: no view runs") == 0);
}

TEST_CASE("a right press on what a button says offers to carry its intent, and the choice carries the shape as a description") {
    Rig rig;
    for (const auto& e : panel_edits()) REQUIRE(rig.edit(e.front(), std::vector<std::string>(e.begin() + 1, e.end())).ok);
    rig.press("> count  button");
    REQUIRE(rig.text("says tally.panel.") != nullptr);
    REQUIRE(rig.value("Count") != nullptr);
    REQUIRE(rig.text("  {start: Int, limit: Int, step: Int}") != nullptr);
    const auto pressed = rig.correlation + 1;
    rig.press("says tally.panel.", 3);
    const loom::Message* menu = nullptr;
    for (const auto& m : rig.desk->heard)
        if (loom::same_identity(m.payload.schema(), *loom::schema_of<ws::PaneMenuRequested>())) menu = &m;
    REQUIRE(menu != nullptr);
    CHECK(menu->correlation == pressed); // it continues the right press
    const auto asked = loom::from_value<ws::PaneMenuRequested>(menu->payload);
    REQUIRE(asked.rows.size() == 1);
    CHECK(asked.rows[0].id == "carry");
    CHECK(asked.subject == "tally.panel.Count");

    // The presenter's answer, as its office, under the menu's number.
    REQUIRE(rig.bus.office_send_to_role_as(rig.menus, ws::kPresenterRole, vb::kRole,
        loom::Message(loom::to_value(ws::PaneMenuAnswered{vb::kPane, "tally.panel.Count", true, "carry", ""}),
                      rig.menus, {}, pressed)).valid());
    rig.pump();
    const loom::Message* carry = nullptr;
    for (const auto& m : rig.desk->heard)
        if (loom::same_identity(m.payload.schema(), *loom::schema_of<ws::PaneValueCarryRequested>())) carry = &m;
    REQUIRE(carry != nullptr);
    CHECK(carry->correlation == pressed); // the acquisition continues the same gesture
    const auto carried = loom::from_value<ws::PaneValueCarryRequested>(carry->payload);
    CHECK_FALSE(carried.drag);
    const auto item = zengine::inventory::decode_pair(
        std::string_view(reinterpret_cast<const char*>(carried.data.data()), carried.data.size())).item;
    REQUIRE(zengine::flow::shape::is_description(item));
    CHECK(loom::same_identity(*zengine::flow::shape::described(item), *hwfix::count_schema()));

    // A right press elsewhere means nothing here: it is handed back.
    rig.press("Elements (5)", 3);
    CHECK(loom::same_identity(rig.desk->heard.back().payload.schema(), *loom::schema_of<ws::PanePassRequested>()));
}

TEST_CASE("a kind dragged from the palette is made where it is let go on the design canvas, in whole pixels; a click makes one below the last; let go elsewhere, nothing is made") {
    namespace sp = zengine::surface;
    Rig rig;
    REQUIRE(rig.edit("new", {"tally.panel", "discard"}).ok);
    const auto chip = [&](const std::string& title) {
        const auto* t = rig.text("[" + title + "]");
        REQUIRE(t != nullptr);
        return std::pair<std::int64_t, std::int64_t>{t->x + 8, t->y + 8};
    };
    const auto area = rig.design();
    auto [x, y] = chip("Number");
    rig.pointer(ws::canvas_pointer::kPress, x, y, 1);
    rig.pointer(ws::canvas_pointer::kMove, area.x + sp::subs_of_pixel(30), area.y + sp::subs_of_pixel(40), 1);
    // Where it would be made is marked while the hand holds it, and nothing is made yet.
    CHECK(rig.now().elements.empty());
    CHECK(rig.marked(zengine::surface::role::kMuted,
                     vb::element_area(area, view::Element{"", view::Kind::number, "", 30, 40, 144, 24, ""}),
                     sp::kPixelGrainSubs));
    rig.pointer(ws::canvas_pointer::kRelease, area.x + sp::subs_of_pixel(30), area.y + sp::subs_of_pixel(40), 1);
    auto d = rig.now();
    REQUIRE(d.elements.size() == 1);
    CHECK(d.elements[0].kind == view::Kind::number);
    CHECK(d.elements[0].x == 30);
    CHECK(d.elements[0].y == 40);
    CHECK(d.elements[0].w == 144);
    // A CLICK on a kind makes one below the last.
    std::tie(x, y) = chip("Button");
    rig.pointer(ws::canvas_pointer::kPress, x, y, 2);
    rig.pointer(ws::canvas_pointer::kRelease, x, y, 2);
    d = rig.now();
    REQUIRE(d.elements.size() == 2);
    CHECK(d.elements[1].kind == view::Kind::button);
    CHECK(d.elements[1].y == 40 + 24 + 4);
    // LET GO OVER THE LEFT COLUMN, nothing is made, and the builder says where to let go.
    std::tie(x, y) = chip("Label");
    rig.pointer(ws::canvas_pointer::kPress, x, y, 3);
    rig.pointer(ws::canvas_pointer::kMove, x + sp::subs_of_pixel(40), y + sp::subs_of_pixel(40), 3);
    rig.pointer(ws::canvas_pointer::kRelease, x + sp::subs_of_pixel(40), y + sp::subs_of_pixel(40), 3);
    CHECK(rig.now().elements.size() == 2);
    CHECK(rig.notice() == "Let go over the canvas to make a label there");
}

TEST_CASE("an element dragged on the design canvas moves in whole pixels and a corner resizes it, a value typed in its box changes the same element, and a lost drag puts it back") {
    namespace sp = zengine::surface;
    TempDir dir;
    Rig rig;
    for (const auto& e : panel_edits()) REQUIRE(rig.edit(e.front(), std::vector<std::string>(e.begin() + 1, e.end())).ok);
    REQUIRE(rig.edit("save", {(dir.directory / "moved.view").string()}).ok);
    auto d = rig.now();
    // MOVED by a drag from its middle: ten pixels right and six down, the press's own picture.
    auto [x, y] = rig.middle(d.elements[0]);
    rig.pointer(ws::canvas_pointer::kPress, x, y, 7);
    rig.pointer(ws::canvas_pointer::kMove, x + sp::subs_of_pixel(10), y + sp::subs_of_pixel(6), 7);
    CHECK(rig.now().elements[0].x == 10);
    rig.pointer(ws::canvas_pointer::kRelease, x + sp::subs_of_pixel(10), y + sp::subs_of_pixel(6), 7);
    d = rig.now();
    CHECK(d.elements[0].x == 10);
    CHECK(d.elements[0].y == 6);
    CHECK(d.elements[0].w == 144);
    CHECK(rig.text("[Save*]") != nullptr);
    // RESIZED by its bottom-right handle, the selected element's own.
    const auto at = vb::element_area(rig.design(), d.elements[0]);
    rig.pointer(ws::canvas_pointer::kPress, at.x + at.w, at.y + at.h, 8);
    rig.pointer(ws::canvas_pointer::kMove, at.x + at.w + sp::subs_of_pixel(20), at.y + at.h + sp::subs_of_pixel(4), 8);
    rig.pointer(ws::canvas_pointer::kRelease, at.x + at.w + sp::subs_of_pixel(20), at.y + at.h + sp::subs_of_pixel(4), 8);
    d = rig.now();
    CHECK(d.elements[0].x == 10);
    CHECK(d.elements[0].w == 164);
    CHECK(d.elements[0].h == 28);
    // ...and by its top-left one, which moves the corner and keeps the opposite one where it was.
    const auto again = vb::element_area(rig.design(), d.elements[0]);
    rig.pointer(ws::canvas_pointer::kPress, again.x, again.y, 9);
    rig.pointer(ws::canvas_pointer::kMove, again.x + sp::subs_of_pixel(4), again.y - sp::subs_of_pixel(100), 9);
    rig.pointer(ws::canvas_pointer::kRelease, again.x + sp::subs_of_pixel(4), again.y - sp::subs_of_pixel(100), 9);
    d = rig.now();
    CHECK(d.elements[0].x == 14);
    CHECK(d.elements[0].y == 0); // never above the view's top
    CHECK(d.elements[0].w == 160);
    CHECK(d.elements[0].h == 34);
    // A VALUE TYPED INTO ITS BOX changes the same element.
    const auto* box = rig.value("14");
    REQUIRE(box != nullptr);
    rig.pointer(ws::canvas_pointer::kPress, box->x + 8, box->y + 8, 10);
    rig.pointer(ws::canvas_pointer::kRelease, box->x + 8, box->y + 8, 10);
    rig.host(ws::PaneKey{vb::kPane, zengine::input::scan::kA, zengine::input::mod::kCtrl});
    rig.host(ws::PaneTextInput{vb::kPane, "50"});
    rig.host(ws::PaneKey{vb::kPane, zengine::input::scan::kReturn, 0});
    CHECK(rig.now().elements[0].x == 50);
    // A DRAG THAT ENDS LOST puts back what it moved.
    d = rig.now();
    std::tie(x, y) = rig.middle(d.elements[0]);
    rig.pointer(ws::canvas_pointer::kPress, x, y, 11);
    rig.pointer(ws::canvas_pointer::kMove, x + sp::subs_of_pixel(30), y + sp::subs_of_pixel(30), 11);
    CHECK(rig.now().elements[0].x == 80);
    rig.pointer(ws::canvas_pointer::kLost, x + sp::subs_of_pixel(30), y + sp::subs_of_pixel(30), 11);
    CHECK(rig.now().elements[0].x == 50);
    CHECK(rig.now().elements[0].y == 0);
    CHECK(rig.notice() == "start is back where it was: the drag ended before it was let go");
    // AN ELEMENT REMOVED MID-DRAG ends the drag: the one that takes its index is not moved.
    std::tie(x, y) = rig.middle(rig.now().elements[0]);
    rig.pointer(ws::canvas_pointer::kPress, x, y, 12);
    rig.host(ws::PaneKey{vb::kPane, zengine::input::scan::kDelete, 0});
    const auto left = rig.now();
    REQUIRE(left.elements[0].id == "limit");
    rig.pointer(ws::canvas_pointer::kMove, x + sp::subs_of_pixel(30), y + sp::subs_of_pixel(30), 12);
    rig.pointer(ws::canvas_pointer::kLost, x + sp::subs_of_pixel(30), y + sp::subs_of_pixel(30), 12);
    CHECK(rig.now().elements[0].x == left.elements[0].x);
    CHECK(rig.now().elements[0].y == left.elements[0].y);
}

TEST_CASE("the pointer resting on an element marks it and its row; a carried value marks the label it would land on; leaving puts the marks down") {
    namespace ink = zengine::surface::role;
    const auto thin = zengine::surface::kPixelGrainSubs;
    Rig rig;
    for (const auto& e : panel_edits()) REQUIRE(rig.edit(e.front(), std::vector<std::string>(e.begin() + 1, e.end())).ok);
    REQUIRE(rig.edit("select", {"3"}).ok);
    const auto d = rig.now();
    const auto start = vb::element_area(rig.design(), d.elements[0]);
    CHECK_FALSE(rig.marked(ink::kFill, start, thin));
    auto [x, y] = rig.middle(d.elements[0]);
    rig.hover(x, y);
    CHECK(rig.marked(ink::kFill, start, thin));
    // ...and its row in the list.
    const auto* row = rig.text("  start  number");
    REQUIRE(row != nullptr);
    CHECK(std::any_of(rig.picture().rects.begin(), rig.picture().rects.end(),
                      [&](const auto& r) { return r.role == ink::kMuted && r.x == 0 && r.y == row->y; }));
    // Resting moves nothing: the selection is still `count`.
    CHECK(rig.text("count (button) ") != nullptr);
    // A CARRIED VALUE over the label marks where it would land; over a number field it marks nothing.
    const auto total = vb::element_area(rig.design(), d.elements[4]);
    std::tie(x, y) = rig.middle(d.elements[4]);
    rig.hover(x, y, true, true);
    CHECK(rig.marked(ink::kAccent, total, 2 * thin));
    CHECK_FALSE(rig.marked(ink::kFill, start, thin));
    std::tie(x, y) = rig.middle(d.elements[1]);
    rig.hover(x, y, true, true);
    CHECK_FALSE(rig.marked(ink::kAccent, total, 2 * thin));
    CHECK_FALSE(rig.marked(ink::kAccent, vb::element_area(rig.design(), d.elements[1]), 2 * thin));
    // LEAVING puts every mark down.
    std::tie(x, y) = rig.middle(d.elements[0]);
    rig.hover(x, y);
    REQUIRE(rig.marked(ink::kFill, start, thin));
    rig.hover(x, y, false);
    CHECK_FALSE(rig.marked(ink::kFill, start, thin));
}

TEST_CASE("a value typed into its box is kept by Return or Tab, refused in words with its box kept for repair, and put back by Escape") {
    Rig rig;
    for (const auto& e : panel_edits()) REQUIRE(rig.edit(e.front(), std::vector<std::string>(e.begin() + 1, e.end())).ok);
    REQUIRE(rig.edit("select", {"0"}).ok);
    const auto press_value = [&](const std::string& words) {
        const auto* box = rig.value(words);
        REQUIRE_MESSAGE(box != nullptr, words);
        const auto x = box->x + 8, y = box->y + 8;
        rig.pointer(ws::canvas_pointer::kPress, x, y, 20);
        rig.pointer(ws::canvas_pointer::kRelease, x, y, 20);
    };
    const auto key = [&](std::int64_t scancode, std::int64_t mods = 0) { rig.host(ws::PaneKey{vb::kPane, scancode, mods}); };
    namespace scan = zengine::input::scan;
    // REFUSED: the id is said not to be one, the element keeps its id, and the box keeps the text.
    press_value("start");
    key(scan::kA, zengine::input::mod::kCtrl);
    rig.host(ws::PaneTextInput{vb::kPane, "1st"});
    key(scan::kReturn);
    CHECK(rig.notice().find("`1st` is not") != std::string::npos);
    CHECK(rig.now().elements[0].id == "start");
    CHECK(rig.value("1st") != nullptr);
    // ESCAPE puts it back.
    key(scan::kEscape);
    CHECK(rig.value("1st") == nullptr);
    CHECK(rig.value("start") != nullptr);
    // TAB keeps the label and moves to the starting text, which takes typing in turn.
    press_value("start"); // the id box; Tab walks from it
    key(scan::kTab);
    rig.host(ws::PaneTextInput{vb::kPane, "From"}); // Tab selected the label whole: typing replaces it
    key(scan::kTab);
    CHECK(rig.now().elements[0].label == "From");
    rig.host(ws::PaneTextInput{vb::kPane, "5"});
    key(scan::kReturn);
    CHECK(rig.now().elements[0].text == "5");
    // A press elsewhere keeps a value being typed before it acts.
    press_value("0");
    key(scan::kEnd);
    rig.host(ws::PaneTextInput{vb::kPane, "8"});
    const auto* list = rig.text("  limit  number");
    REQUIRE(list != nullptr);
    rig.pointer(ws::canvas_pointer::kPress, list->x + 8, list->y + 8, 21);
    const auto d = rig.now();
    CHECK(d.elements[0].x == 8);
    CHECK(rig.text("limit (number) ") != nullptr);
}

TEST_CASE("a box scrolled to show its caret is pressed where it shows: the caret lands at the byte under the press") {
    Rig rig;
    for (const auto& e : panel_edits()) REQUIRE(rig.edit(e.front(), std::vector<std::string>(e.begin() + 1, e.end())).ok);
    const std::string long_label = "abcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMN";
    REQUIRE(rig.edit("set", {"4", "label", long_label}).ok);
    REQUIRE(rig.edit("select", {"4"}).ok);
    // The label's box, beside the design area, holding the front of the label.
    const auto boxed = [&](const std::string& start) -> const ws::PaneCanvasText* {
        const auto area = rig.design();
        for (const auto& t : rig.picture().texts)
            if (t.text.rfind(start, 0) == 0 && t.x >= area.x + area.w) return &t;
        return nullptr;
    };
    const auto* box = boxed("abcdefgh");
    REQUIRE(box != nullptr);
    rig.pointer(ws::canvas_pointer::kPress, box->x + 8, box->y + 8, 50);
    rig.host(ws::PaneKey{vb::kPane, zengine::input::scan::kEnd, 0});
    // Typing at its end scrolls the box: it shows the label's tail, not its front.
    const auto* scrolled = [&]() -> const ws::PaneCanvasText* {
        for (const auto& t : rig.picture().texts)
            if (t.role == zengine::surface::role::kAccent && !t.text.empty() &&
                long_label.size() >= t.text.size() &&
                long_label.compare(long_label.size() - t.text.size(), t.text.size(), t.text) == 0)
                return &t;
        return nullptr;
    }();
    REQUIRE(scrolled != nullptr);
    const auto first = long_label.size() - scrolled->text.size();
    REQUIRE(first > 5);
    // A PRESS ON ITS SIXTH SHOWN COLUMN puts the caret before the byte drawn there.
    const auto metrics = ws::canvas_text_metrics(window_room());
    const auto x = scrolled->x + metrics.inset + 5 * metrics.advance + metrics.advance / 2;
    rig.pointer(ws::canvas_pointer::kPress, x, scrolled->y + 8, 51);
    rig.host(ws::PaneTextInput{vb::kPane, "#"});
    rig.host(ws::PaneKey{vb::kPane, zengine::input::scan::kReturn, 0});
    auto expected = long_label;
    expected.insert(first + 5, "#");
    CHECK(rig.now().elements[4].label == expected);
}

TEST_CASE("every field a label could show can be chosen: the choice wraps within its column, and what the rows cannot hold is reached by More") {
    Rig rig;
    for (const auto& e : panel_edits()) REQUIRE(rig.edit(e.front(), std::vector<std::string>(e.begin() + 1, e.end())).ok);
    REQUIRE(rig.edit("select", {"4"}).ok);
    // Is `[name]` drawn whole inside its column, so a press on it chooses that field? The values
    // sit right of the design area in a wide pane, and left of it in a narrow one.
    auto room = window_room();
    const auto offered = [&](const std::string& name) -> const ws::PaneCanvasText* {
        const auto* t = rig.text("[" + name + "]");
        if (!t || t->text != "[" + name + "]") return nullptr;
        const auto area = rig.design();
        const auto right = t->x >= area.x + area.w ? room.width : area.x;
        const auto metrics = ws::canvas_text_metrics(room);
        return t->x + static_cast<std::int64_t>(t->text.size()) * metrics.advance <= right ? t : nullptr;
    };
    std::int64_t gesture = 60;
    const auto choose = [&](const std::string& name) {
        const auto* t = offered(name);
        REQUIRE_MESSAGE(t != nullptr, name);
        rig.pointer(ws::canvas_pointer::kPress, t->x + 4, t->y + 4, ++gesture);
    };
    const auto shown = [&] {
        const auto d = rig.now();
        const auto* s = d.shown("total");
        return s ? s->shape->name() + "." + s->field : std::string();
    };

    // LONG NAMES: three that one row of the values column cannot hold side by side.
    loom::SchemaBuilder wide("tally.Wide", 1);
    const std::vector<std::string> names = {"the_running_total_so_far", "the_number_of_steps_taken",
                                            "the_last_value_counted_in"};
    for (const auto& n : names) wide.field(n, loom::Kind::Int);
    rig.drop(loom::encode_schema(*wide.build()), "> total  label");
    REQUIRE(rig.text("show which field of tally.Wide?") != nullptr);
    for (const auto& n : names) CHECK_MESSAGE(offered(n) != nullptr, n);
    choose(names.back());
    CHECK(shown() == "tally.Wide.the_last_value_counted_in");
    // ...and in a pane too narrow for a values column, where they sit below the list.
    room = window_room(++rig.grant);
    room.width = zengine::surface::subs_of_pixel(800);
    rig.host(room);
    rig.drop(loom::encode_schema(*wide.build()), "> total  label");
    REQUIRE(rig.text("show which field of tally.Wide?") != nullptr);
    REQUIRE(rig.text("show which field of tally.Wide?")->x < rig.design().x);
    for (const auto& n : names) CHECK_MESSAGE(offered(n) != nullptr, n);
    choose(names.front());
    CHECK(shown() == "tally.Wide.the_running_total_so_far");
    room = window_room(++rig.grant);
    rig.host(room);

    // MORE FIELDS THAN THE ROWS HOLD: More pages through them, and the last one is chosen.
    loom::SchemaBuilder many("tally.Many", 1);
    std::vector<std::string> fields;
    for (int i = 0; i < 40; ++i) {
        fields.push_back("field_number_" + std::to_string(100 + i) + "_of_the_shape");
        many.field(fields.back(), loom::Kind::Int);
    }
    rig.drop(loom::encode_schema(*many.build()), "> total  label");
    REQUIRE(rig.text("show which field of tally.Many?") != nullptr);
    std::set<std::string> seen;
    for (int page = 0; page < 40 && !offered(fields.back()); ++page) {
        for (const auto& f : fields)
            if (offered(f)) seen.insert(f);
        REQUIRE(offered("More") != nullptr);
        choose("More");
    }
    for (const auto& f : fields)
        if (offered(f)) seen.insert(f);
    CHECK(seen.size() == fields.size());
    choose(fields.back());
    CHECK(shown() == "tally.Many." + fields.back());
}

TEST_CASE("a value dropped on no label is refused in words whatever is selected; dropped on a label on the canvas, it binds that label") {
    Rig rig;
    for (const auto& e : panel_edits()) REQUIRE(rig.edit(e.front(), std::vector<std::string>(e.begin() + 1, e.end())).ok);
    REQUIRE(rig.edit("select", {"4"}).ok);
    const auto total = loom::encode_schema(*total_shape());
    const auto drop_at = [&](std::int64_t x, std::int64_t y) {
        const auto bytes = zengine::inventory::encode_pair(total, {});
        rig.host(ws::PaneCanvasValueDrop{vb::kPane, rig.grant, rig.picture().picture, x, y,
                                         loom::Bytes(bytes.begin(), bytes.end()), "zengine.flow", "flow", ""});
    };
    const std::string refused = "Not shown: drop a shape or a field on a label, on the canvas or in the list";
    // ON A HEADING: no label is there, so none is bound, though Total is selected.
    rig.drop(total, "Elements (5)");
    CHECK(rig.now().shows.empty());
    CHECK(rig.notice() == refused);
    // ON THE CANVAS WHERE NO ELEMENT IS: the same.
    const auto area = rig.design();
    drop_at(area.x + area.w - zengine::surface::subs_of_pixel(8), area.y + area.h - zengine::surface::subs_of_pixel(8));
    CHECK(rig.now().shows.empty());
    CHECK(rig.notice() == refused);
    // ON A NUMBER FIELD: refused in words that name it.
    auto [x, y] = rig.middle(rig.now().elements[0]);
    drop_at(x, y);
    CHECK(rig.now().shows.empty());
    CHECK(rig.notice() == "`start` is a number; carry a shape onto a label to show one of its fields");
    // ON THE LABEL ON THE CANVAS: bound.
    std::tie(x, y) = rig.middle(rig.now().elements[4]);
    drop_at(x, y);
    const auto d = rig.now();
    REQUIRE(d.shows.size() == 1);
    CHECK(d.shows[0].element == "total");
    CHECK(d.shows[0].field == "total");
}

TEST_CASE("a carried description brings the shapes it nests: a label binds a field of a shape nesting another from what it carries, and its own intent is carried the same way") {
    Rig rig;
    for (const auto& e : panel_edits()) REQUIRE(rig.edit(e.front(), std::vector<std::string>(e.begin() + 1, e.end())).ok);
    REQUIRE(rig.edit("select", {"4"}).ok);
    const auto detail = loom::SchemaBuilder("review.Detail", 1).field("note", loom::Kind::Text).build();
    const auto review = loom::SchemaBuilder("review.Total", 1)
        .field("total", loom::Kind::Int).message("detail", detail).build();

    // A BARE DESCRIPTION of it holds no review.Detail: refused in Loom's words, nothing bound.
    rig.drop(loom::encode_schema(*review), "> total  label");
    CHECK(rig.now().shows.empty());
    CHECK(rig.notice().find("unresolved nested schema 'review.Detail'") != std::string::npos);

    // AS A PANE CARRIES IT: the shape, and beside it the shapes it nests. `total` is bound, and
    // the binding survives the description's own round trip.
    rig.drop(loom::encode_accepted_shapes({review}), "> total  label");
    const auto d = rig.now();
    REQUIRE(d.shows.size() == 1);
    CHECK(d.shows[0].field == "total");
    CHECK(loom::same_identity(*d.shows[0].shape, *review));

    // ITS OWN INTENT, carried out by a press on what the button says, holds its shape as a root.
    REQUIRE(rig.edit("select", {"3"}).ok);
    const auto* says = rig.text("says tally.panel.");
    REQUIRE(says != nullptr);
    rig.pointer(ws::canvas_pointer::kPress, says->x + 8, says->y + 8, 70);
    const loom::Message* carried = nullptr;
    for (const auto& m : rig.desk->heard)
        if (loom::same_identity(m.payload.schema(), *loom::schema_of<ws::PaneValueCarryRequested>())) carried = &m;
    REQUIRE(carried != nullptr);
    const auto bytes = loom::from_value<ws::PaneValueCarryRequested>(carried->payload).data;
    const auto item = zengine::inventory::decode_pair(
        std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size())).item;
    REQUIRE(loom::same_identity(item.schema(), *loom::accepted_shapes_schema()));
    loom::Registry carried_shapes;
    loom::decode_accepted_referenced(item, carried_shapes);
    const auto roots = loom::decode_accepted_roots(item, carried_shapes);
    REQUIRE(roots.size() == 1);
    CHECK(loom::same_identity(*roots[0], *hwfix::count_schema()));
}

TEST_CASE("a press on what a button says asks under that press to drag its intent's shape out") {
    Rig rig;
    for (const auto& e : panel_edits()) REQUIRE(rig.edit(e.front(), std::vector<std::string>(e.begin() + 1, e.end())).ok);
    REQUIRE(rig.edit("select", {"3"}).ok);
    const auto* says = rig.text("says tally.panel.");
    REQUIRE(says != nullptr);
    rig.pointer(ws::canvas_pointer::kPress, says->x + 8, says->y + 8, 30);
    const auto pressed = rig.correlation;
    const loom::Message* asked = nullptr;
    const loom::Message* carried = nullptr;
    for (const auto& m : rig.desk->heard) {
        if (loom::same_identity(m.payload.schema(), *loom::schema_of<ws::PaneOperationRequested>())) asked = &m;
        if (loom::same_identity(m.payload.schema(), *loom::schema_of<ws::PaneValueCarryRequested>())) carried = &m;
    }
    REQUIRE(asked != nullptr);
    CHECK(loom::from_value<ws::PaneOperationRequested>(asked->payload).gesture == static_cast<std::int64_t>(pressed));
    REQUIRE(carried != nullptr);
    CHECK(carried->correlation == pressed);
    const auto value = loom::from_value<ws::PaneValueCarryRequested>(carried->payload);
    CHECK(value.drag);
    CHECK(value.label == "tally.panel.Count");
}

TEST_CASE("in a terminal the builder's picture and its drags are floored to cells") {
    namespace sp = zengine::surface;
    Rig rig;
    rig.host(terminal_room(++rig.grant));
    for (const auto& e : panel_edits()) REQUIRE(rig.edit(e.front(), std::vector<std::string>(e.begin() + 1, e.end())).ok);
    const auto area = rig.design();
    CHECK(area.x % unit == 0);
    for (const auto& t : rig.picture().texts) {
        CHECK(t.x % unit == 0);
        CHECK(t.y % unit == 0);
    }
    // A drag in cells moves an element by whole cells' worth of pixels.
    const auto d = rig.now();
    const auto at = vb::element_area(area, d.elements[0]);
    const auto x = at.x + unit, y = at.y;
    rig.pointer(ws::canvas_pointer::kPress, x, y, 40);
    rig.pointer(ws::canvas_pointer::kMove, x + 2 * unit, y + unit, 40);
    rig.pointer(ws::canvas_pointer::kRelease, x + 2 * unit, y + unit, 40);
    CHECK(rig.now().elements[0].x == 2 * sp::kCanvasCellPx);
    CHECK(rig.now().elements[0].y == sp::kCanvasCellPx);
}
