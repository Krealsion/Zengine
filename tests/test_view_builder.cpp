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
#include <fstream>
#include <iterator>
#include <set>

namespace {
namespace vb = zengine::view_builder;
namespace view = zengine::view;
namespace ws = zengine::workshop;
namespace shape = zengine::flow::shape;
constexpr const char* workshop_role = "zengine.workshop";
constexpr auto unit = ws::kPaneCanvasUnit;

bool has_words(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

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
        // ONE RENDERER: everything the view's own picture code draws in a room exactly the view's
        // size is in the builder's picture, moved by the area's corner and nothing else.
        const ws::PaneCanvasRoom inner{vb::kPane, room.grant, zengine::surface::subs_of_pixel(m.description.width),
                                       zengine::surface::subs_of_pixel(m.description.height), room.grain,
                                       room.graphical, room.text_advance_px, room.text_line_px};
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
        // THE SELECTED ELEMENT: marked, with a handle on each side and at each corner, and its
        // values in boxes.
        std::vector<std::string> handles, boxes;
        for (const auto& hit : pic.hits) {
            if (hit.action == "handle") handles.push_back(hit.args.at(1) + "," + hit.args.at(2));
            if (hit.action == "box" && hit.args.at(1) == "4") boxes.push_back(hit.args.at(0));
        }
        CHECK(handles == std::vector<std::string>{"0,-1", "0,1", "-1,0", "1,0", "-1,-1", "1,-1", "-1,1", "1,1"});
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
    // PANNED, the view is drawn by the same code in a room exactly its size, moved whole by the
    // area's corner less the pan: what lies wholly inside the area is there once, moved, and
    // nothing of the view lies outside it.
    {
        namespace sp = zengine::surface;
        auto wide = panel_model();
        wide.command("size", {"1400", "1000"});
        wide.command("element", {"4", "total", "Total", "900", "700", "192", "24"});
        vb::Presentation panned;
        panned.pan_x = 480;
        panned.pan_y = 300;
        const auto room = window_room();
        const auto pic = vb::picture(wide, panned, room, 3);
        const auto& area = pic.design;
        CHECK(panned.pan_x == 480);
        CHECK(pic.view.x == area.x - sp::subs_of_pixel(480));
        CHECK(pic.view.y == area.y - sp::subs_of_pixel(300));
        CHECK(pic.view.w == sp::subs_of_pixel(1400));
        CHECK(pic.view.h == sp::subs_of_pixel(1000));
        const ws::PaneCanvasRoom inner{vb::kPane, room.grant, pic.view.w, pic.view.h, room.grain, room.graphical,
                                       room.text_advance_px, room.text_line_px};
        const auto own = view::picture(wide.description, {}, view::Presentation{}, inner, 3);
        std::size_t inside = 0;
        for (const auto& t : own.content.texts) {
            const auto x = t.x + pic.view.x, y = t.y + pic.view.y;
            const auto count = std::count_if(pic.content.texts.begin(), pic.content.texts.end(), [&](const auto& u) {
                return u.text == t.text && u.x == x && u.y == y;
            });
            if (x >= area.x && y >= area.y && y < area.y + area.h) {
                CHECK_MESSAGE(count == 1, t.text);
                ++inside;
            } else {
                CHECK_MESSAGE(count == 0, t.text);
            }
        }
        CHECK(inside >= 1); // Total, and the view's last row
        for (const auto& t : pic.content.texts)
            CHECK_FALSE((t.text.rfind("start", 0) == 0 && t.x >= area.x)); // panned past: never drawn
        // An element across the area's right edge is cut there: nothing of the view crosses it.
        auto across = wide;
        across.command("element", {"0", "start", "start", std::to_string(480 + sp::floor_div_px(area.w, sp::kPixelGrainSubs) - 50),
                                   "320", "144", "24", "0"});
        vb::Presentation same;
        same.pan_x = 480;
        same.pan_y = 300;
        const auto cut = vb::picture(across, same, room, 5);
        const auto right = area.x + area.w;
        CHECK(std::none_of(cut.content.rects.begin(), cut.content.rects.end(), [&](const auto& r) {
            return r.x >= area.x && r.x < right && r.x + r.w > right;
        }));
        CHECK(std::any_of(cut.content.rects.begin(), cut.content.rects.end(), [&](const auto& r) {
            return r.role == zengine::surface::role::kMuted && r.x == right - sp::subs_of_pixel(50) && r.x + r.w == right;
        }));
        // A pan past its reach is held to it: the view's right edge, and a cell beyond it where
        // its handles sit, at the area's right edge.
        panned.pan_x = 9000;
        (void)vb::picture(wide, panned, room, 4);
        CHECK(panned.pan_x == 1400 + 12 - sp::floor_div_px(area.w, sp::kPixelGrainSubs));
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
    void pointer(std::int64_t phase, std::int64_t x, std::int64_t y, std::int64_t gesture, std::int64_t button = 1,
                 std::int64_t modifiers = 0) {
        const auto corr = phase == ws::canvas_pointer::kPress ? ++correlation : 0;
        host(ws::PaneCanvasPointer{vb::kPane, grant, picture().picture, gesture, phase, button, x, y, modifiers}, corr);
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
    /// A line on the design canvas beginning `start`: drawn by the view's own picture.
    const ws::PaneCanvasText* drawn(const std::string& start) const {
        const auto area = design();
        for (const auto& t : picture().texts)
            if (t.text.rfind(start, 0) == 0 && area.within({t.x, t.y, 1, 1}).w == 1) return &t;
        return nullptr;
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

TEST_CASE("an element dragged on the design canvas moves and a corner resizes it, in whole pixels and snapped to the others' edges; a value typed in its box or an arrow key places it exactly; a lost drag puts it back") {
    namespace sp = zengine::surface;
    namespace ink = zengine::surface::role;
    TempDir dir;
    Rig rig;
    for (const auto& e : panel_edits()) REQUIRE(rig.edit(e.front(), std::vector<std::string>(e.begin() + 1, e.end())).ok);
    REQUIRE(rig.edit("save", {(dir.directory / "moved.view").string()}).ok);
    auto d = rig.now();
    // MOVED by a drag from its middle, ten pixels right and six down: ten pixels right, and its
    // bottom edge, two pixels from limit's top, to that edge, whose line the canvas shows while
    // the hand holds it.
    auto [x, y] = rig.middle(d.elements[0]);
    rig.pointer(ws::canvas_pointer::kPress, x, y, 7);
    rig.pointer(ws::canvas_pointer::kMove, x + sp::subs_of_pixel(10), y + sp::subs_of_pixel(6), 7);
    CHECK(rig.now().elements[0].x == 10);
    CHECK(rig.now().elements[0].y == 4);
    const auto area = rig.design();
    const auto line = [&](std::int64_t py) {
        return std::any_of(rig.picture().rects.begin(), rig.picture().rects.end(), [&](const auto& r) {
            return r.role == ink::kAccent && r.x == area.x && r.w == area.w && r.y == area.y + sp::subs_of_pixel(py) &&
                   r.h == sp::kPixelGrainSubs;
        });
    };
    CHECK(line(28));
    rig.pointer(ws::canvas_pointer::kRelease, x + sp::subs_of_pixel(10), y + sp::subs_of_pixel(6), 7);
    CHECK_FALSE(line(28));
    d = rig.now();
    CHECK(d.elements[0].x == 10);
    CHECK(d.elements[0].y == 4);
    CHECK(d.elements[0].w == 144);
    CHECK(rig.text("[Save*]") != nullptr);
    // RESIZED by its bottom-right handle, the selected element's own, by whole pixels.
    const auto at = vb::element_area(rig.design(), d.elements[0]);
    rig.pointer(ws::canvas_pointer::kPress, at.x + at.w, at.y + at.h, 8);
    rig.pointer(ws::canvas_pointer::kMove, at.x + at.w + sp::subs_of_pixel(20), at.y + at.h + sp::subs_of_pixel(12), 8);
    rig.pointer(ws::canvas_pointer::kRelease, at.x + at.w + sp::subs_of_pixel(20), at.y + at.h + sp::subs_of_pixel(12), 8);
    d = rig.now();
    CHECK(d.elements[0].x == 10);
    CHECK(d.elements[0].w == 164);
    CHECK(d.elements[0].h == 36);
    // ...and by its top-left one, which moves the corner and keeps the opposite one where it was.
    const auto again = vb::element_area(rig.design(), d.elements[0]);
    rig.pointer(ws::canvas_pointer::kPress, again.x, again.y, 9);
    rig.pointer(ws::canvas_pointer::kMove, again.x + sp::subs_of_pixel(10), again.y - sp::subs_of_pixel(100), 9);
    rig.pointer(ws::canvas_pointer::kRelease, again.x + sp::subs_of_pixel(10), again.y - sp::subs_of_pixel(100), 9);
    d = rig.now();
    CHECK(d.elements[0].x == 20);
    CHECK(d.elements[0].y == 0); // never above the view's top
    CHECK(d.elements[0].x + d.elements[0].w == 174);
    CHECK(d.elements[0].y + d.elements[0].h == 40);
    // A VALUE TYPED INTO ITS BOX changes the same element, exactly.
    const auto* box = rig.value("20");
    REQUIRE(box != nullptr);
    rig.pointer(ws::canvas_pointer::kPress, box->x + 8, box->y + 8, 10);
    rig.pointer(ws::canvas_pointer::kRelease, box->x + 8, box->y + 8, 10);
    rig.host(ws::PaneKey{vb::kPane, zengine::input::scan::kA, zengine::input::mod::kCtrl});
    rig.host(ws::PaneTextInput{vb::kPane, "50"});
    rig.host(ws::PaneKey{vb::kPane, zengine::input::scan::kReturn, 0});
    CHECK(rig.now().elements[0].x == 50);
    // AN ARROW KEY moves it a pixel, exactly too.
    rig.host(ws::PaneKey{vb::kPane, zengine::input::scan::kRight, 0});
    rig.host(ws::PaneKey{vb::kPane, zengine::input::scan::kDown, 0});
    CHECK(rig.now().elements[0].x == 51);
    CHECK(rig.now().elements[0].y == 1);
    // A DRAG THAT ENDS LOST puts back what it moved.
    d = rig.now();
    std::tie(x, y) = rig.middle(d.elements[0]);
    rig.pointer(ws::canvas_pointer::kPress, x, y, 11);
    rig.pointer(ws::canvas_pointer::kMove, x + sp::subs_of_pixel(30), y + sp::subs_of_pixel(30), 11);
    CHECK(rig.now().elements[0].x == 81);
    rig.pointer(ws::canvas_pointer::kLost, x + sp::subs_of_pixel(30), y + sp::subs_of_pixel(30), 11);
    CHECK(rig.now().elements[0].x == 51);
    CHECK(rig.now().elements[0].y == 1);
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

TEST_CASE("a place by hand snaps: an edge it moves comes to another element's or the view's edge within reach, else to the weaver's grid when one is set, and never past the view's rules") {
    using vb::Edges;
    const std::vector<std::int64_t> none;
    const auto most = view::kMaxSizePx;
    // NO GRID unless the weaver sets one: an edge out of every other's reach stays where it was.
    CHECK(vb::snap_axis(10, 144, Edges::both, none).at == 10);
    CHECK_FALSE(vb::snap_axis(10, 144, Edges::both, none).met);
    // A GRID SET: the nearer line, the later one when they are as near.
    CHECK(vb::snap_axis(10, 144, Edges::both, none, most, 12).at == 12);
    CHECK(vb::snap_axis(17, 144, Edges::both, none, most, 12).at == 12);
    CHECK(vb::snap_axis(18, 144, Edges::both, none, most, 12).at == 24);
    CHECK(vb::snap_axis(18, 144, Edges::both, none, most, 5).at == 20);
    // ANOTHER'S EDGE within reach comes first, met by either edge that moves; past reach, the grid.
    const std::vector<std::int64_t> others{100, 300};
    auto s = vb::snap_axis(97, 50, Edges::both, others);
    CHECK(s.at == 100);
    CHECK(s.met == 100);
    s = vb::snap_axis(255, 50, Edges::both, others);
    CHECK(s.at == 250); // its right edge to 300
    CHECK(s.met == 300);
    s = vb::snap_axis(100 + vb::kSnapReach + 1, 50, Edges::both, others, most, 12);
    CHECK(s.at == 108);
    CHECK_FALSE(s.met);
    CHECK(vb::snap_axis(100 + vb::kSnapReach + 1, 50, Edges::both, others).at == 107);
    // A SIDE moves alone: the low edge keeps the high one where it was, and the high the low.
    s = vb::snap_axis(95, 105, Edges::low, others);
    CHECK(s.at == 100);
    CHECK(s.at + s.size == 200);
    s = vb::snap_axis(40, 263, Edges::high, others);
    CHECK(s.at == 40);
    CHECK(s.at + s.size == 300);
    s = vb::snap_axis(40, 50, Edges::none, others, most, 12);
    CHECK(s.at == 40);
    CHECK(s.size == 50);
    // NEVER PAST THE RULES: above the view's top, a side past the one it keeps, or past the
    // view's far edge.
    CHECK(vb::snap_axis(2, 10, Edges::both, std::vector<std::int64_t>{0}).at == 0);
    CHECK(vb::snap_axis(1, 50, Edges::both, std::vector<std::int64_t>{48}, most, 12).at == 0);
    s = vb::snap_axis(30, 3, Edges::low, none, most, 12);
    CHECK(s.at + s.size == 33);
    CHECK(s.size >= 1);
    CHECK(vb::snap_axis(view::kMaxPixels - 2, 1, Edges::both, none, most, 12).at <= view::kMaxPixels);
    CHECK(vb::snap_axis(462, 16, Edges::both, none, 480, 12).at == 456);
    CHECK(vb::snap_axis(462, 16, Edges::both, std::vector<std::int64_t>{483}, 480).at == 462);
    // BOTH AXES, against the view's edges and every element but the one placed.
    auto m = panel_model();
    auto placed = vb::snap(m.description, 0, {10, 6, 144, 24, std::nullopt, std::nullopt}, Edges::both, Edges::both);
    CHECK(placed.x == 10);
    CHECK(placed.y == 4); // its bottom edge to limit's top
    CHECK(placed.met_y == 28);
    CHECK_FALSE(placed.met_x);
    placed = vb::snap(m.description, 0, {10, 6, 144, 24, std::nullopt, std::nullopt}, Edges::both, Edges::both, 12);
    CHECK(placed.x == 12);
    placed = vb::snap(m.description, 0, {333, 6, 144, 24, std::nullopt, std::nullopt}, Edges::both, Edges::none);
    CHECK(placed.x == 336); // its right edge to the view's, at 480
    CHECK(placed.met_x == 480);
}

TEST_CASE("each side of the selected element has a handle that moves that side alone, in a window and floored to cells in a terminal") {
    namespace sp = zengine::surface;
    Rig rig;
    for (const auto& e : panel_edits()) REQUIRE(rig.edit(e.front(), std::vector<std::string>(e.begin() + 1, e.end())).ok);
    REQUIRE(rig.edit("select", {"4"}).ok);
    // THE RIGHT SIDE: its handle at the side's middle; the left side stays, the height too.
    auto d = rig.now();
    auto at = vb::element_area(rig.design(), d.elements[4]);
    const auto drag = [&](std::int64_t x, std::int64_t y, std::int64_t dx, std::int64_t dy, std::int64_t gesture) {
        rig.pointer(ws::canvas_pointer::kPress, x, y, gesture);
        rig.pointer(ws::canvas_pointer::kMove, x + dx, y + dy, gesture);
        rig.pointer(ws::canvas_pointer::kRelease, x + dx, y + dy, gesture);
    };
    drag(at.x + at.w, at.y + at.h / 2, sp::subs_of_pixel(40), sp::subs_of_pixel(30), 20);
    auto e = rig.now().elements[4];
    CHECK(e.x == 0);
    CHECK(e.y == 112);
    CHECK(e.h == 24);
    CHECK(e.w == 232); // forty pixels wider
    // THE LEFT SIDE: the right one stays where it was.
    at = vb::element_area(rig.design(), e);
    drag(at.x, at.y + at.h / 2, sp::subs_of_pixel(50), sp::subs_of_pixel(-30), 21);
    e = rig.now().elements[4];
    CHECK(e.x == 50);
    CHECK(e.x + e.w == 232);
    CHECK(e.y == 112);
    CHECK(e.h == 24);
    // THE TOP SIDE: the bottom stays, and neither side moves.
    at = vb::element_area(rig.design(), e);
    drag(at.x + at.w / 2, at.y, sp::subs_of_pixel(30), sp::subs_of_pixel(-58), 22);
    e = rig.now().elements[4];
    CHECK(e.x == 50);
    CHECK(e.w == 182);
    CHECK(e.y == 52); // to limit's bottom, within reach, where the grid would say 60
    CHECK(e.y + e.h == 136);
    // THE BOTTOM SIDE: the top stays.
    at = vb::element_area(rig.design(), e);
    drag(at.x + at.w / 2, at.y + at.h, 0, sp::subs_of_pixel(-54), 23);
    e = rig.now().elements[4];
    CHECK(e.y == 52);
    CHECK(e.y + e.h == 80); // to step's bottom, within reach, where the grid would say 84
    CHECK(e.x == 50);
    // IN A TERMINAL a side's handle is the cell beside that side's middle, outside the element, so
    // it is never a corner's; a drag of it in cells moves that side alone.
    rig.host(terminal_room(++rig.grant));
    const auto unit_px = sp::kCanvasCellPx;
    e = rig.now().elements[4];
    at = vb::element_area(rig.design(), e);
    const auto down = [](std::int64_t v) { return sp::floor_div_px(v, unit) * unit; };
    const auto right = down(at.x + at.w - 1) + unit;
    const auto middle = std::min(down(at.y + at.h / 2), down(at.y + at.h - 1));
    std::vector<std::string> sides;
    for (const auto& r : rig.picture().rects)
        if (r.role == zengine::surface::role::kAccent && r.x == right && r.y == middle && r.w == unit && r.h == unit)
            sides.push_back("right");
    CHECK(sides == std::vector<std::string>{"right"});
    drag(right, middle, 2 * unit, unit, 24);
    const auto moved = rig.now().elements[4];
    CHECK(moved.x == e.x);
    CHECK(moved.y == e.y);
    CHECK(moved.h == e.h);
    CHECK(moved.x + moved.w == 232 + 2 * unit_px);
}

TEST_CASE("the middle button pans the design canvas: an element past its edge is seen, pressed, dragged and dropped on where it is drawn, nothing of the view is drawn outside the canvas, and the pan is never saved") {
    namespace sp = zengine::surface;
    namespace ink = zengine::surface::role;
    TempDir dir;
    Rig rig;
    for (const auto& e : panel_edits()) REQUIRE(rig.edit(e.front(), std::vector<std::string>(e.begin() + 1, e.end())).ok);
    REQUIRE(rig.edit("size", {"2000", "1200"}).ok);
    REQUIRE(rig.edit("add", {"button"}).ok);
    REQUIRE(rig.edit("element", {"5", "far", "Far", "1500", "900", "96", "24"}).ok);
    REQUIRE(rig.edit("save", {(dir.directory / "far.view").string()}).ok);
    REQUIRE(rig.edit("select", {"0"}).ok);
    const auto saved = view::description_bytes(rig.now());
    const auto area = rig.design();
    REQUIRE(sp::subs_of_pixel(1500) > area.w);
    REQUIRE(sp::subs_of_pixel(900) > area.h);
    const auto far_box = [&](const vb::Area& at) {
        return std::count_if(rig.picture().rects.begin(), rig.picture().rects.end(), [&](const auto& r) {
            return r.role == ink::kMuted && r.x == at.x && r.y == at.y && r.w == at.w && r.h == at.h;
        });
    };
    // NOTHING OF THE VIEW OUTSIDE THE CANVAS: the far button is past its edges, so not drawn.
    CHECK(rig.drawn("Far") == nullptr);
    for (const auto& r : rig.picture().rects) {
        const bool starts_inside = r.x >= area.x && r.x < area.x + area.w && r.y >= area.y && r.y < area.y + area.h;
        CHECK_FALSE((starts_inside && (r.x + r.w > area.x + area.w || r.y + r.h > area.y + area.h)));
    }
    CHECK(far_box(vb::element_area(area, rig.now().elements[5])) == 0);
    // A MIDDLE DRAG on the canvas pans it: the view follows the hand.
    const auto cx = area.x + area.w / 2, cy = area.y + area.h / 2;
    const std::int64_t pan_x = 1200, pan_y = 600;
    rig.pointer(ws::canvas_pointer::kPress, cx, cy, 30, 2);
    rig.pointer(ws::canvas_pointer::kMove, cx - sp::subs_of_pixel(pan_x), cy - sp::subs_of_pixel(pan_y), 30, 2);
    rig.pointer(ws::canvas_pointer::kRelease, cx - sp::subs_of_pixel(pan_x), cy - sp::subs_of_pixel(pan_y), 30, 2);
    const vb::Area placed{area.x - sp::subs_of_pixel(pan_x), area.y - sp::subs_of_pixel(pan_y), 0, 0};
    auto distant = vb::element_area(placed, rig.now().elements[5]);
    REQUIRE(area.within(distant).w == distant.w);
    CHECK(far_box(distant) == 1);
    const auto* label = rig.drawn("Far");
    REQUIRE(label != nullptr);
    CHECK(label->x >= distant.x);
    CHECK(label->x < distant.x + distant.w);
    CHECK(rig.drawn("start: 0") == nullptr); // panned past, so not drawn
    // RESTING on it marks it where it is drawn.
    rig.hover(distant.x + distant.w / 2, distant.y + distant.h / 2);
    CHECK(rig.marked(ink::kFill, distant, sp::kPixelGrainSubs));
    // PRESSED AND DRAGGED where it is drawn: selected and moved.
    rig.pointer(ws::canvas_pointer::kPress, distant.x + distant.w / 2, distant.y + distant.h / 2, 31);
    CHECK(rig.text("far (button) ") != nullptr);
    rig.pointer(ws::canvas_pointer::kMove, distant.x + distant.w / 2 - sp::subs_of_pixel(24), distant.y + distant.h / 2 + sp::subs_of_pixel(12), 31);
    rig.pointer(ws::canvas_pointer::kRelease, distant.x + distant.w / 2 - sp::subs_of_pixel(24), distant.y + distant.h / 2 + sp::subs_of_pixel(12), 31);
    auto e = rig.now().elements[5];
    CHECK(e.x == 1476);
    CHECK(e.y == 912);
    // A KIND LET GO while panned is made where it was let go, in the view's pixels.
    const auto* chip = rig.text("[Label]");
    REQUIRE(chip != nullptr);
    rig.pointer(ws::canvas_pointer::kPress, chip->x + 8, chip->y + 8, 32);
    const auto lx = placed.x + sp::subs_of_pixel(1500), ly = placed.y + sp::subs_of_pixel(1008);
    rig.pointer(ws::canvas_pointer::kMove, lx, ly, 32);
    rig.pointer(ws::canvas_pointer::kRelease, lx, ly, 32);
    REQUIRE(rig.now().elements.size() == 7);
    CHECK(rig.now().elements[6].x == 1500);
    CHECK(rig.now().elements[6].y == 1008);
    // A VALUE DROPPED on a label while panned binds the label it was dropped on.
    const auto made = vb::element_area(placed, rig.now().elements[6]);
    const auto bytes = zengine::inventory::encode_pair(loom::encode_schema(*total_shape()), {});
    rig.host(ws::PaneCanvasValueDrop{vb::kPane, rig.grant, rig.picture().picture, made.x + made.w / 2, made.y + made.h / 2,
                                     loom::Bytes(bytes.begin(), bytes.end()), "zengine.flow", "flow", ""});
    CHECK(rig.now().shown("label1") != nullptr);
    // THE PAN IS NEVER SAVED: the far button put back and the label removed, the file is the
    // same bytes as before the pan, and nothing the builder keeps across a reload holds it.
    REQUIRE(rig.edit("remove", {"6"}).ok);
    REQUIRE(rig.edit("element", {"5", "far", "Far", "1500", "900", "96", "24"}).ok);
    REQUIRE(rig.edit("save", {(dir.directory / "far2.view").string()}).ok);
    CHECK(view::description_bytes(rig.now()) == saved);
    const auto read = [&](const char* name) {
        std::ifstream file(dir.directory / name, std::ios::binary);
        return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    };
    CHECK_FALSE(read("far.view").empty());
    CHECK(read("far2.view") == read("far.view"));
    CHECK(rig.drawn("Far") != nullptr); // an edit from elsewhere keeps the pan
    // THE PAN'S REACH: the view's far edges, and a cell beyond them where its handles sit, no
    // further left or up than the canvas's own, however far the hand goes.
    rig.pointer(ws::canvas_pointer::kPress, cx, cy, 33, 2);
    rig.pointer(ws::canvas_pointer::kMove, cx - sp::subs_of_pixel(5000), cy - sp::subs_of_pixel(5000), 33, 2);
    rig.pointer(ws::canvas_pointer::kRelease, cx - sp::subs_of_pixel(5000), cy - sp::subs_of_pixel(5000), 33, 2);
    const auto [reach_x, reach_y] = vb::pan_reach(rig.now(), area);
    const vb::Area reached{area.x - sp::subs_of_pixel(reach_x), area.y - sp::subs_of_pixel(reach_y), 0, 0};
    distant = vb::element_area(reached, rig.now().elements[5]);
    CHECK(far_box(distant) == 1);
    CHECK(std::abs(reached.x + sp::subs_of_pixel(2000 + 12) - (area.x + area.w)) <= sp::subs_of_pixel(1));
    CHECK(std::abs(reached.y + sp::subs_of_pixel(1200 + 12) - (area.y + area.h)) <= sp::subs_of_pixel(1));
    // ...and back to the view's corner, no further.
    rig.pointer(ws::canvas_pointer::kPress, cx, cy, 34, 2);
    rig.pointer(ws::canvas_pointer::kMove, cx + sp::subs_of_pixel(9000), cy + sp::subs_of_pixel(9000), 34, 2);
    rig.pointer(ws::canvas_pointer::kRelease, cx + sp::subs_of_pixel(9000), cy + sp::subs_of_pixel(9000), 34, 2);
    CHECK(rig.drawn("start: 0") != nullptr);
    CHECK(rig.drawn("Far") == nullptr);
    // A NEW VIEW, or one opened, is shown from its corner.
    rig.pointer(ws::canvas_pointer::kPress, cx, cy, 35, 2);
    rig.pointer(ws::canvas_pointer::kMove, cx - sp::subs_of_pixel(pan_x), cy - sp::subs_of_pixel(pan_y), 35, 2);
    rig.pointer(ws::canvas_pointer::kRelease, cx - sp::subs_of_pixel(pan_x), cy - sp::subs_of_pixel(pan_y), 35, 2);
    REQUIRE(rig.drawn("Far") != nullptr);
    REQUIRE(rig.edit("open", {(dir.directory / "far.view").string(), "discard"}).ok);
    CHECK(rig.drawn("start: 0") != nullptr);
    CHECK(rig.drawn("Far") == nullptr);
}

TEST_CASE("in a terminal the middle button pans the design canvas by whole cells") {
    namespace sp = zengine::surface;
    Rig rig;
    rig.host(terminal_room(++rig.grant));
    for (const auto& e : panel_edits()) REQUIRE(rig.edit(e.front(), std::vector<std::string>(e.begin() + 1, e.end())).ok);
    REQUIRE(rig.edit("size", {"1400", "700"}).ok);
    REQUIRE(rig.edit("add", {"button"}).ok);
    REQUIRE(rig.edit("element", {"5", "far", "Far", "1200", "600", "96", "24"}).ok);
    REQUIRE(rig.edit("select", {"0"}).ok);
    const auto area = rig.design();
    CHECK(rig.drawn("Far") == nullptr);
    rig.pointer(ws::canvas_pointer::kPress, area.x + 2 * unit, area.y + 2 * unit, 50, 2);
    rig.pointer(ws::canvas_pointer::kMove, area.x + 2 * unit - 60 * unit, area.y + 2 * unit - 30 * unit, 50, 2);
    rig.pointer(ws::canvas_pointer::kRelease, area.x + 2 * unit - 60 * unit, area.y + 2 * unit - 30 * unit, 50, 2);
    const auto* distant = rig.drawn("Far");
    REQUIRE(distant != nullptr);
    for (const auto& t : rig.picture().texts) {
        CHECK(t.x % unit == 0);
        CHECK(t.y % unit == 0);
    }
    const vb::Area placed{area.x - 60 * unit, area.y - 30 * unit, 0, 0};
    const auto at = vb::element_area(placed, rig.now().elements[5]);
    CHECK(distant->x >= at.x);
    CHECK(distant->y >= at.y);
    // ...and an element pressed there is the one drawn there, moved by whole cells.
    rig.pointer(ws::canvas_pointer::kPress, at.x + unit, at.y, 51);
    rig.pointer(ws::canvas_pointer::kMove, at.x + 2 * unit, at.y + unit, 51);
    rig.pointer(ws::canvas_pointer::kRelease, at.x + 2 * unit, at.y + unit, 51);
    CHECK(rig.now().elements[5].x == 1200 + sp::kCanvasCellPx);
    CHECK(rig.now().elements[5].y == 600 + sp::kCanvasCellPx);
}

namespace {
/// The bytes a view was saved as before a view had a size: version 1, without the size.
std::string first_version_bytes(const view::Description& d) {
    const auto now = view::encode(d);
    loom::Value v(view::description_schema_v1());
    for (const auto& f : view::description_schema_v1()->fields())
        if (const auto* cell = now.get(f.name)) v.set(f.name, *cell);
    v.set("format_version", loom::Cell::integer(1));
    return loom::serialize(v);
}
} // namespace

TEST_CASE("the view's size is set by its handles on the design canvas or typed into its boxes; the canvas draws the view in exactly its size; its edges snap as an element's do, and an element's to them; a view saved without a size opens with the size it needs") {
    namespace sp = zengine::surface;
    namespace ink = zengine::surface::role;
    TempDir dir;
    Rig rig;
    for (const auto& e : panel_edits()) REQUIRE(rig.edit(e.front(), std::vector<std::string>(e.begin() + 1, e.end())).ok);
    // A NEW VIEW is 480 by 240, its size in boxes, and the canvas draws it in exactly that room.
    auto d = rig.now();
    CHECK(d.width == vb::kNewViewWidth);
    CHECK(d.height == vb::kNewViewHeight);
    CHECK(rig.value("480") != nullptr);
    CHECK(rig.value("240") != nullptr);
    const auto area = rig.design();
    const auto ground = [&](std::int64_t w, std::int64_t h) {
        return std::count_if(rig.picture().rects.begin(), rig.picture().rects.end(), [&](const auto& r) {
            return r.role == ink::kGround && r.x == area.x && r.y == area.y && r.w == sp::subs_of_pixel(w) &&
                   r.h == sp::subs_of_pixel(h);
        });
    };
    CHECK(ground(480, 240) == 1);
    const auto drag = [&](std::int64_t x, std::int64_t y, std::int64_t dx, std::int64_t dy, std::int64_t gesture,
                          std::int64_t phase = ws::canvas_pointer::kRelease) {
        rig.pointer(ws::canvas_pointer::kPress, x, y, gesture);
        rig.pointer(ws::canvas_pointer::kMove, x + sp::subs_of_pixel(dx), y + sp::subs_of_pixel(dy), gesture);
        rig.pointer(phase, x + sp::subs_of_pixel(dx), y + sp::subs_of_pixel(dy), gesture);
    };
    const auto px = [](std::int64_t v) { return sp::subs_of_pixel(v); };
    // THE RIGHT EDGE'S HANDLE sets the width alone, the bottom's the height, the corner's both.
    drag(area.x + px(480), area.y + px(120), 96, 30, 60);
    d = rig.now();
    CHECK(d.width == 576);
    CHECK(d.height == 240);
    CHECK(ground(576, 240) == 1);
    drag(area.x + px(288), area.y + px(240), 40, 60, 61);
    CHECK(rig.now().width == 576);
    CHECK(rig.now().height == 300);
    drag(area.x + px(576), area.y + px(300), -60, -12, 62);
    CHECK(rig.now().width == 516);
    CHECK(rig.now().height == 288);
    // TYPED into its box, the width is exact.
    const auto* box = rig.value("516");
    REQUIRE(box != nullptr);
    rig.pointer(ws::canvas_pointer::kPress, box->x + 8, box->y + 8, 63);
    rig.pointer(ws::canvas_pointer::kRelease, box->x + 8, box->y + 8, 63);
    rig.host(ws::PaneKey{vb::kPane, zengine::input::scan::kA, zengine::input::mod::kCtrl});
    rig.host(ws::PaneTextInput{vb::kPane, "500"});
    rig.host(ws::PaneKey{vb::kPane, zengine::input::scan::kReturn, 0});
    CHECK(rig.now().width == 500);
    CHECK(rig.notice() == "The view is 500 by 288");
    // AN ELEMENT'S EDGE SNAPS TO THE VIEW'S: total's right edge, three pixels short of it, comes to it.
    d = rig.now();
    auto [x, y] = rig.middle(d.elements[4]);
    drag(x, y, 305, 0, 64);
    CHECK(rig.now().elements[4].x == 308);
    CHECK(rig.now().elements[4].x + rig.now().elements[4].w == 500);
    CHECK(rig.now().elements[4].y == 108); // its top to count's bottom, four pixels from it
    // THE SIZE'S EDGE SNAPS TO AN ELEMENT'S: the bottom, four pixels past total's, comes to it.
    drag(area.x + px(250), area.y + px(288), 0, -152, 65);
    CHECK(rig.now().height == 132);
    // A DRAG CUT SHORT puts the size back.
    drag(area.x + px(500), area.y + px(66), 120, 0, 66, ws::canvas_pointer::kLost);
    CHECK(rig.now().width == 500);
    CHECK(rig.notice() == "The view's size is back where it was: the drag ended before it was let go");
    // IN A TERMINAL each of its handles is the cell beyond its edge.
    rig.host(terminal_room(++rig.grant));
    const auto cells = rig.design();
    const auto up = [](std::int64_t v) { return (v + unit - 1) / unit * unit; };
    const auto right = cells.x + up(px(500));
    CHECK(std::any_of(rig.picture().rects.begin(), rig.picture().rects.end(), [&](const auto& r) {
        return r.role == ink::kMuted && r.x == right && r.w == unit && r.h == unit && r.y == cells.y + px(132) / 2 / unit * unit;
    }));
    rig.host(window_room(++rig.grant));

    // A VIEW SAVED WITHOUT A SIZE opens with the size its elements and notice rows need, and is
    // saved again with it.
    const auto old = (dir.directory / "old.view").string();
    {
        std::ofstream file(old, std::ios::binary);
        file << first_version_bytes(panel_model().description);
    }
    REQUIRE(rig.edit("open", {old, "discard"}).ok);
    CHECK(rig.now().width == 480);
    CHECK(rig.now().height == 192);
    CHECK(rig.now().elements.size() == 5);
    REQUIRE(rig.edit("save", {(dir.directory / "again.view").string()}).ok);
    std::ifstream again(dir.directory / "again.view", std::ios::binary);
    const std::string saved((std::istreambuf_iterator<char>(again)), std::istreambuf_iterator<char>());
    CHECK(loom::parse(saved).claimed_version() == static_cast<std::uint32_t>(view::kFormatVersion));
    CHECK(view::read_description(saved).description.height == 192);
}

TEST_CASE("nothing sits outside the view: a dragged element stops at its edge, a typed value or a key that would cross it is refused in words, and the size cannot shrink past an element") {
    namespace sp = zengine::surface;
    Rig rig;
    for (const auto& e : panel_edits()) REQUIRE(rig.edit(e.front(), std::vector<std::string>(e.begin() + 1, e.end())).ok);
    const auto area = rig.design();
    const auto px = [](std::int64_t v) { return sp::subs_of_pixel(v); };
    const auto drag = [&](std::int64_t x, std::int64_t y, std::int64_t dx, std::int64_t dy, std::int64_t gesture) {
        rig.pointer(ws::canvas_pointer::kPress, x, y, gesture);
        rig.pointer(ws::canvas_pointer::kMove, x + px(dx), y + px(dy), gesture);
        rig.pointer(ws::canvas_pointer::kRelease, x + px(dx), y + px(dy), gesture);
    };
    const auto type_into = [&](const std::string& shown, const std::string& value) {
        const auto* box = rig.value(shown);
        REQUIRE_MESSAGE(box != nullptr, shown);
        rig.pointer(ws::canvas_pointer::kPress, box->x + 8, box->y + 8, 70);
        rig.pointer(ws::canvas_pointer::kRelease, box->x + 8, box->y + 8, 70);
        rig.host(ws::PaneKey{vb::kPane, zengine::input::scan::kA, zengine::input::mod::kCtrl});
        rig.host(ws::PaneTextInput{vb::kPane, value});
        rig.host(ws::PaneKey{vb::kPane, zengine::input::scan::kReturn, 0});
    };
    // THE SIZE CANNOT SHRINK PAST AN ELEMENT: its edge, dragged in, stops at total's far edges...
    drag(area.x + px(480), area.y + px(120), -400, 0, 71);
    CHECK(rig.now().width == 192);
    drag(area.x + px(96), area.y + px(240), 0, -200, 72);
    CHECK(rig.now().height == 136);
    // ...and typed, it is refused in words, the box kept for repair.
    type_into("192", "150");
    CHECK(rig.now().width == 192);
    CHECK(rig.notice() == "`total` reaches to 192,136, past the view's size of 150 by 136");
    rig.host(ws::PaneKey{vb::kPane, zengine::input::scan::kEscape, 0});
    // A KIND MADE BELOW THE LAST, where the view has no room, is refused in words.
    const auto* chip = rig.text("[Label]");
    REQUIRE(chip != nullptr);
    rig.pointer(ws::canvas_pointer::kPress, chip->x + 8, chip->y + 8, 73);
    rig.pointer(ws::canvas_pointer::kRelease, chip->x + 8, chip->y + 8, 73);
    CHECK(rig.now().elements.size() == 5);
    CHECK(has_words(rig.notice(), "past the view's size of 192 by 136"));
    REQUIRE(rig.edit("size", {"480", "240"}).ok);

    // A DRAGGED ELEMENT STOPS AT THE VIEW'S EDGE: start, dragged far right and far down.
    auto d = rig.now();
    auto [x, y] = rig.middle(d.elements[0]);
    drag(x, y, 1000, 1000, 74);
    CHECK(rig.now().elements[0].x == 480 - 144);
    CHECK(rig.now().elements[0].y == 240 - 24);
    // ...and a corner stops there too.
    REQUIRE(rig.edit("select", {"4"}).ok);
    d = rig.now();
    const auto total = vb::element_area(rig.design(), d.elements[4]);
    drag(total.x + total.w, total.y + total.h, 1000, 1000, 75);
    CHECK(rig.now().elements[4].w == 480);
    CHECK(rig.now().elements[4].h == 240 - 112);
    // A KIND LET GO AT THE EDGE is made inside it.
    const auto* number = rig.text("[Number]");
    REQUIRE(number != nullptr);
    rig.pointer(ws::canvas_pointer::kPress, number->x + 8, number->y + 8, 76);
    rig.pointer(ws::canvas_pointer::kMove, area.x + px(470), area.y + px(10), 76);
    rig.pointer(ws::canvas_pointer::kRelease, area.x + px(470), area.y + px(10), 76);
    REQUIRE(rig.now().elements.size() == 6);
    CHECK(rig.now().elements[5].x + rig.now().elements[5].w == 480);

    // A KEY THAT WOULD CROSS AN EDGE is refused in words: start sits on the right and bottom edges.
    REQUIRE(rig.edit("select", {"0"}).ok);
    rig.host(ws::PaneKey{vb::kPane, zengine::input::scan::kRight, 0});
    CHECK(rig.now().elements[0].x == 336);
    CHECK(rig.notice() == "`start` reaches to 481,240, past the view's size of 480 by 240");
    rig.host(ws::PaneKey{vb::kPane, zengine::input::scan::kDown, 0});
    CHECK(rig.now().elements[0].y == 216);
    // ...and on the left, where the view begins.
    REQUIRE(rig.edit("place", {"0", "0", "0", "144", "24"}).ok);
    rig.host(ws::PaneKey{vb::kPane, zengine::input::scan::kLeft, 0});
    CHECK(rig.now().elements[0].x == 0);
    CHECK(has_words(rig.notice(), "`start` sits at a place and a size of whole pixels, each from 0"));
    // A TYPED VALUE THAT WOULD CROSS IT is refused in words, the box kept for repair.
    REQUIRE(rig.edit("place", {"0", "12", "0", "144", "24"}).ok);
    type_into("12", "400");
    CHECK(rig.now().elements[0].x == 12);
    CHECK(rig.notice() == "`start` reaches to 544,24, past the view's size of 480 by 240");
}

TEST_CASE("the grid is the weaver's: none by default, so a place by hand moves by whole pixels and only an edge pulls it; set, a place by hand snaps to it until it is set back to one; Alt held sets every snap aside") {
    namespace sp = zengine::surface;
    Rig rig;
    for (const auto& e : panel_edits()) REQUIRE(rig.edit(e.front(), std::vector<std::string>(e.begin() + 1, e.end())).ok);
    const auto px = [](std::int64_t v) { return sp::subs_of_pixel(v); };
    const auto drag = [&](std::int64_t x, std::int64_t y, std::int64_t dx, std::int64_t dy, std::int64_t gesture,
                          std::int64_t modifiers = 0) {
        rig.pointer(ws::canvas_pointer::kPress, x, y, gesture, 1, modifiers);
        rig.pointer(ws::canvas_pointer::kMove, x + px(dx), y + px(dy), gesture, 1, modifiers);
        rig.pointer(ws::canvas_pointer::kRelease, x + px(dx), y + px(dy), gesture, 1, modifiers);
    };
    const auto set_grid = [&](const std::string& shown, const std::string& value) {
        const auto* box = rig.value(shown);
        REQUIRE_MESSAGE(box != nullptr, shown);
        rig.pointer(ws::canvas_pointer::kPress, box->x + 8, box->y + 8, 80);
        rig.pointer(ws::canvas_pointer::kRelease, box->x + 8, box->y + 8, 80);
        rig.host(ws::PaneKey{vb::kPane, zengine::input::scan::kA, zengine::input::mod::kCtrl});
        rig.host(ws::PaneTextInput{vb::kPane, value});
        rig.host(ws::PaneKey{vb::kPane, zengine::input::scan::kReturn, 0});
    };
    // NO GRID BY DEFAULT: the grid's box says 1, and a place by hand is the pixel it was left at.
    CHECK(rig.text("px: none") != nullptr);
    auto [x, y] = rig.middle(rig.now().elements[4]);
    drag(x, y, 217, 41, 81);
    CHECK(rig.now().elements[4].x == 217);
    CHECK(rig.now().elements[4].y == 153);
    // SET, a place by hand snaps to it.
    set_grid("1", "20");
    CHECK(rig.notice() == "A place by hand snaps to a grid 20 pixels apart");
    std::tie(x, y) = rig.middle(rig.now().elements[4]);
    drag(x, y, 10, 10, 82);
    CHECK(rig.now().elements[4].x == 220);
    CHECK(rig.now().elements[4].y == 160);
    // ...a grid it cannot be is refused in words, and the grid stays.
    set_grid("20", "0");
    CHECK(rig.notice() == "the grid is whole pixels from 1, which is none, to 96; 0 is not");
    rig.host(ws::PaneKey{vb::kPane, zengine::input::scan::kEscape, 0});
    set_grid("20", "97");
    CHECK(has_words(rig.notice(), "97 is not"));
    rig.host(ws::PaneKey{vb::kPane, zengine::input::scan::kEscape, 0});
    // ...an edit from elsewhere keeps it, and it is never in the view.
    REQUIRE(rig.edit("select", {"0"}).ok);
    CHECK(rig.value("20") != nullptr);
    const auto with = view::description_bytes(rig.now());
    // ALT HELD SETS EVERY SNAP ASIDE, edges included: limit, dragged up five pixels, stops a pixel
    // below start's bottom instead of coming to it.
    std::tie(x, y) = rig.middle(rig.now().elements[1]);
    drag(x, y, 0, -5, 83, zengine::input::mod::kAlt);
    CHECK(rig.now().elements[1].y == 23);
    CHECK(rig.now().elements[1].x == 0);
    // ...and without it, the same drag comes to that edge.
    REQUIRE(rig.edit("place", {"1", "0", "28", "144", "24"}).ok);
    std::tie(x, y) = rig.middle(rig.now().elements[1]);
    drag(x, y, 0, -5, 84);
    CHECK(rig.now().elements[1].y == 24);
    // SET BACK TO ONE, there is no grid again.
    set_grid("20", "1");
    CHECK(rig.notice() == "No grid: a place by hand moves by whole pixels");
    std::tie(x, y) = rig.middle(rig.now().elements[4]);
    drag(x, y, 7, 3, 85);
    CHECK(rig.now().elements[4].x == 227);
    CHECK(rig.now().elements[4].y == 163);
    CHECK(with.find("grid") == std::string::npos);
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

TEST_CASE("a carried shape brings the shapes it nests: a label binds a field of a shape nesting another from what it carries, its own intent is carried the same way, and a weave's describe answer is no carried shape") {
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
    rig.drop(zengine::flow::shape::carried(review), "> total  label");
    const auto d = rig.now();
    REQUIRE(d.shows.size() == 1);
    CHECK(d.shows[0].field == "total");
    CHECK(loom::same_identity(*d.shows[0].shape, *review));

    // A WEAVE'S DESCRIBE ANSWER, kept and dropped here, says which shapes a weave accepts: it is
    // not read as a carried shape, and the label keeps what it shows.
    const auto counted = loom::SchemaBuilder("review.Count", 1).field("count", loom::Kind::Int).build();
    rig.drop(loom::encode_accepted_shapes({counted}), "> total  label");
    REQUIRE(rig.now().shows.size() == 1);
    CHECK(rig.now().shows[0].shape->name() == "review.Total");

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
    CHECK(item.schema().name() == "zengine.flow.CarriedShape");
    CHECK(loom::same_identity(*zengine::flow::shape::described(item), *hwfix::count_schema()));
}

TEST_CASE("a control in the values column is drawn whole inside it whatever the words before it: Unshow, Remove and the intent's box") {
    Rig rig;
    for (const auto& e : panel_edits()) REQUIRE(rig.edit(e.front(), std::vector<std::string>(e.begin() + 1, e.end())).ok);
    const auto metrics = ws::canvas_text_metrics(window_room());
    // Is exactly `words` drawn, whole, right of the design area and inside the room -- on the row
    // of `beside` when it is named?
    const auto whole = [&](const std::string& words, const ws::PaneCanvasText* beside = nullptr) -> const ws::PaneCanvasText* {
        const auto area = rig.design();
        for (const auto& t : rig.picture().texts)
            if (t.text == words && t.x >= area.x + area.w && (!beside || t.y == beside->y) &&
                t.x + static_cast<std::int64_t>(words.size()) * metrics.advance <= window_room().width)
                return &t;
        return nullptr;
    };
    std::int64_t gesture = 80;
    const auto press = [&](const ws::PaneCanvasText& t) {
        rig.pointer(ws::canvas_pointer::kPress, t.x + 8, t.y + 8, ++gesture);
        rig.pointer(ws::canvas_pointer::kRelease, t.x + 8, t.y + 8, gesture);
    };

    // UNSHOW after a long binding.
    REQUIRE(rig.edit("select", {"4"}).ok);
    const auto wide = loom::SchemaBuilder("tally.Wide", 1).field("the_running_total_so_far", loom::Kind::Int).build();
    rig.drop(loom::encode_schema(*wide), "> total  label");
    REQUIRE(rig.now().shows.size() == 1);
    const auto* unshow = whole("[Unshow]");
    REQUIRE(unshow != nullptr);
    press(*unshow);
    CHECK(rig.now().shows.empty());

    // REMOVE after the longest id.
    REQUIRE(rig.edit("set", {"4", "id", "the_label_that_shows_the_total_0"}).ok);
    const auto* remove = whole("[Remove]");
    REQUIRE(remove != nullptr);
    press(*remove);
    CHECK(rig.now().elements.size() == 4);

    // THE INTENT'S BOX after `says` and the longest view name: pressed, typed into, kept.
    REQUIRE(rig.edit("rename", {"tally.panel.with.the.longest.nm"}).ok);
    REQUIRE(rig.edit("select", {"3"}).ok);
    const auto* says = rig.text("says ");
    REQUIRE(says != nullptr);
    const auto* box = whole("Count", says);
    REQUIRE(box != nullptr);
    press(*box);
    rig.host(ws::PaneKey{vb::kPane, zengine::input::scan::kA, zengine::input::mod::kCtrl});
    rig.host(ws::PaneTextInput{vb::kPane, "Sum"});
    rig.host(ws::PaneKey{vb::kPane, zengine::input::scan::kReturn, 0});
    const auto d = rig.now();
    REQUIRE(d.intents.size() == 1);
    CHECK(d.intents[0].shape->name() == "tally.panel.with.the.longest.nm.Sum");
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
