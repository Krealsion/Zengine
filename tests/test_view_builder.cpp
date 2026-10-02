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
#include "view-builder/vocabulary.hpp"
#include "view/host.hpp"
#include "workshop/pane_carry.hpp"
#include "workshop/pane_operation.hpp"
#include "workshop/pane_vocabulary.hpp"

#include <zen/kernel/kernel.hpp>

#include <chrono>
#include <filesystem>

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
    CHECK(view::description_bytes(m.description) == before);

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
        host(ws::PaneCanvasRoom{vb::kPane, grant, 100 * unit, 40 * unit, unit, true});
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
    // The builder draws its own list, never the view: no picture of the view is the builder's.
    CHECK(rig.text("Elements (5)") != nullptr);
    CHECK(rig.text("Total: waiting") == nullptr);

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
    rig.press("  count  button");
    REQUIRE(rig.text("says tally.panel.Count {start: Int, limit: Int, step: Int}") != nullptr);
    const auto pressed = rig.correlation + 1;
    rig.press("says tally.panel.Count", 3);
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
    REQUIRE(loom::same_identity(item.schema(), *loom::schema_desc_schema()));
    loom::Registry none;
    CHECK(loom::same_identity(*loom::decode_schema(item, none), *hwfix::count_schema()));

    // A right press elsewhere means nothing here: it is handed back.
    rig.press("Elements (5)", 3);
    CHECK(loom::same_identity(rig.desk->heard.back().payload.schema(), *loom::schema_of<ws::PanePassRequested>()));
}
