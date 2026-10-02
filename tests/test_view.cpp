// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#include "doctest.h"
#include "flow/shape.hpp"
#include "maker/weave.hpp"
#include "maker_fixture.hpp"
#include "view/host.hpp"

#include <algorithm>
#include <string>
#include <vector>

namespace {
namespace view = zengine::view;
namespace ws = zengine::workshop;
namespace shape = zengine::flow::shape;
using loom::WeaveId;

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

loom::Bytes bytes_of(const view::Description& d) {
    const auto text = view::description_bytes(d);
    return {text.begin(), text.end()};
}

/// Stands in for Workshop's office: hears what a pane says, and speaks to a view as Workshop.
class Desk final : public loom::Weave {
public:
    std::vector<loom::Message> heard;
    std::vector<std::shared_ptr<const loom::Schema>> accepted_schemas() const override {
        return {loom::schema_of<ws::v2::PaneOffered>(), loom::schema_of<ws::PaneContent>(),
                loom::schema_of<ws::PaneCanvasContent>(), loom::schema_of<ws::PaneEscapeUnspent>(),
                loom::schema_of<ws::PanePassRequested>(), loom::schema_of<ws::PaneRevealRequested>()};
    }
    void handle(const loom::Message& in, loom::Bus&) override { heard.push_back(in); }
    loom::Value snapshot() const override { return loom::Value(loom::make_schema("viewtest.Desk", 1, {})); }
    loom::Value policy() const override { return zengine::maker::default_value(loom::lifecycle_policy_schema()); }
    void revive(const loom::Value&) override {}
};

/// Hears the host's answers, and anything else it is made to accept.
class Listener final : public loom::Weave {
public:
    explicit Listener(std::vector<std::shared_ptr<const loom::Schema>> accepts) : accepts_(std::move(accepts)) {}
    std::vector<loom::Message> heard;
    std::vector<std::shared_ptr<const loom::Schema>> accepted_schemas() const override { return accepts_; }
    void handle(const loom::Message& in, loom::Bus&) override { heard.push_back(in); }
    loom::Value snapshot() const override { return loom::Value(loom::make_schema("viewtest.Listener", 1, {})); }
    loom::Value policy() const override { return zengine::maker::default_value(loom::lifecycle_policy_schema()); }
    void revive(const loom::Value&) override {}
private:
    std::vector<std::shared_ptr<const loom::Schema>> accepts_;
};

struct Rig {
    zengine::op::Catalog catalog;
    loom::Switchboard bus;
    view::Host views{bus};
    Desk* desk = nullptr;
    WeaveId desk_id{};
    Listener* client = nullptr;
    WeaveId client_id{};
    zengine::maker::Registered tally;
    std::uint64_t correlation = 0;
    std::int64_t grant = 0;

    Rig() {
        zengine::op::publish_primitives(catalog);
        views.mount();
        auto d = std::make_unique<Desk>();
        desk = d.get();
        desk_id = bus.register_weave(std::move(d), loom::Grant{}.allow_any(), view::kWorkshopRole);
        auto c = std::make_unique<Listener>(std::vector{loom::schema_of<view::ViewAnswer>()});
        client = c.get();
        loom::Grant asking;
        view::allow_view_requests(asking);
        client_id = bus.register_weave(std::move(c), std::move(asking), "viewtest.builder");
        tally = zengine::maker::register_definition(bus, catalog, hwfix::tally(catalog));
        REQUIRE_MESSAGE(tally.ok, tally.reason);
        pump();
    }
    void pump() {
        for (int n = 0; n < 64 && bus.pending() != 0; ++n) bus.pump_pending();
        REQUIRE(bus.pending() == 0);
    }
    template <class T> view::ViewAnswer ask(const T& request) {
        const auto corr = ++correlation;
        (void)bus.office_send_to_role_as(client_id, "viewtest.builder", view::kViewHostRole,
                                         loom::Message(loom::to_value(request), client_id, {}, corr));
        pump();
        for (auto it = client->heard.rbegin(); it != client->heard.rend(); ++it)
            if (it->correlation == corr) {
                CHECK(it->provenance.answers_ask());
                return loom::from_value<view::ViewAnswer>(it->payload);
            }
        FAIL("no answer from the view host");
        return {};
    }
    template <class T> void tell(const std::string& office, const T& value) {
        (void)bus.office_send_to_role_as(desk_id, view::kWorkshopRole, office,
                                         loom::Message(loom::to_value(value), desk_id, {}, ++correlation));
        pump();
    }
    /// Workshop's room: a window's 12-pixel cell and its measured face, or a terminal's cell.
    void room(const std::string& office, bool terminal = false) {
        ws::PaneCanvasRoom r{view::kPane, ++grant, 48 * 40, 48 * 16, terminal ? 48 : 4, !terminal,
                             terminal ? 0 : 8, terminal ? 0 : 16};
        tell(office, r);
    }
    const ws::PaneCanvasContent* latest(const std::string& office) const {
        for (auto it = desk->heard.rbegin(); it != desk->heard.rend(); ++it)
            if (it->provenance.authored_role() == office &&
                loom::same_identity(it->payload.schema(), *loom::schema_of<ws::PaneCanvasContent>())) {
                static ws::PaneCanvasContent kept;
                kept = loom::from_value<ws::PaneCanvasContent>(it->payload);
                return &kept;
            }
        return nullptr;
    }
    std::string words(const std::string& office) const {
        std::string out;
        if (const auto* p = latest(office))
            for (const auto& t : p->texts) out += t.text + "|";
        return out;
    }
    /// Every line of the latest picture as one sentence, a wrapped notice whole again.
    std::string said(const std::string& office) const {
        std::string out;
        if (const auto* p = latest(office))
            for (const auto& t : p->texts) out += (out.empty() ? "" : " ") + t.text;
        return out;
    }
    /// Press at the middle of an element, in the latest picture.
    void press(const std::string& office, const view::Element& e, std::int64_t button = 1) {
        const auto* p = latest(office);
        REQUIRE(p != nullptr);
        tell(office, ws::PaneCanvasPointer{view::kPane, p->grant, p->picture, 1, ws::canvas_pointer::kPress,
                                           button, zengine::surface::subs_of_pixel(e.x + e.w / 2),
                                           zengine::surface::subs_of_pixel(e.y + e.h / 2)});
    }
    /// Replace a number field's text: press it, clear it, type.
    void fill(const std::string& office, const view::Element& e, const std::string& text) {
        press(office, e);
        for (int i = 0; i < 12; ++i)
            tell(office, ws::PaneKey{view::kPane, zengine::input::scan::kBackspace, 0});
        tell(office, ws::PaneTextInput{view::kPane, text});
    }
    std::int64_t total() const { return tally.weave->state().get("total")->as_int(); }
};

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

TEST_CASE("the view host registers a view as its own participant, granted only its intents and its pane conversation") {
    Rig rig;
    const auto run = rig.ask(view::ViewRun{"builder", bytes_of(panel())});
    REQUIRE_MESSAGE(run.ok, run.reason);
    CHECK(run.fresh);
    CHECK(run.office == "tally.panel");
    CHECK(has(run.reason, "it waits until told tally.Total"));
    const auto id = rig.bus.role_holder("tally.panel");
    REQUIRE(id.valid());
    CHECK(id != rig.views.id());
    CHECK(id != rig.client_id);
    REQUIRE(rig.views.view(id.value) != nullptr);

    // It offered its pane as its own office, and asked for a seat.
    bool offered = false, seat = false;
    for (const auto& m : rig.desk->heard) {
        if (m.provenance.authored_role() != "tally.panel") continue;
        offered |= loom::same_identity(m.payload.schema(), *loom::schema_of<ws::v2::PaneOffered>());
        seat |= loom::same_identity(m.payload.schema(), *loom::schema_of<ws::PaneRevealRequested>());
    }
    CHECK(offered);
    CHECK(seat);

    // Its grant is what the description declares and the pane conversation to Workshop alone.
    const auto grant = view::view_grant(panel());
    CHECK(grant.permits("tally.panel.Count", 1, rig.tally.id));
    CHECK(grant.permits_role(ws::PaneCanvasContent::zen_name, ws::PaneCanvasContent::zen_version, view::kWorkshopRole));
    CHECK_FALSE(grant.permits_role(ws::PaneCanvasContent::zen_name, ws::PaneCanvasContent::zen_version, "tally"));
    CHECK_FALSE(grant.permits("tally.Total", 1, rig.tally.id));
    CHECK_FALSE(grant.permits("zen.PokeWrite", 1, rig.tally.id));
    CHECK_FALSE(grant.permits(view::ViewRun::zen_name, view::ViewRun::zen_version, rig.views.id()));

    // A second run in the same session is refused; a name already held is refused.
    CHECK(has(rig.ask(view::ViewRun{"builder", bytes_of(panel())}).reason, "already runs a view"));
    const auto clash = rig.ask(view::ViewRun{"second", bytes_of(panel())});
    CHECK_FALSE(clash.ok);
    CHECK(has(clash.reason, "`tally.panel` is already held"));
}

TEST_CASE("a view says it is waiting until told, says its intent as its own participant by publication, and shows what it is told") {
    Rig rig;
    REQUIRE(rig.ask(view::ViewRun{"builder", bytes_of(panel())}).ok);
    const auto id = rig.bus.role_holder("tally.panel");
    // Another accepter of the same shape: a publication reaches every accepter, not one.
    auto other = std::make_unique<Listener>(std::vector{hwfix::count_schema()});
    auto* bystander = other.get();
    (void)rig.bus.register_weave(std::move(other), loom::Grant{});
    std::vector<loom::BusEvent> to_tally;
    const auto tap = rig.bus.add_observer([&](const loom::BusEvent& e) {
        if (e.kind == loom::EventKind::Delivered && e.target == rig.tally.id) to_tally.push_back(e);
    });
    rig.room("tally.panel");
    CHECK(has(rig.words("tally.panel"), "Total: waiting|"));
    CHECK(has(rig.words("tally.panel"), "waiting to be told tally.Total|"));
    CHECK(has(rig.words("tally.panel"), "start: 0|"));

    const auto d = panel();
    rig.press("tally.panel", d.elements[3]);
    rig.bus.remove_observer(tap);
    CHECK(rig.total() == 45);
    CHECK(has(rig.words("tally.panel"), "Total: 45|"));
    REQUIRE(to_tally.size() == 1);
    CHECK(to_tally[0].sender == id);
    CHECK(to_tally[0].schema_name == "tally.panel.Count");
    CHECK(to_tally[0].addressed_role.empty());
    CHECK(to_tally[0].authored_role.empty());
    REQUIRE(bystander->heard.size() == 1);
    CHECK(bystander->heard[0].sender == id);

    rig.fill("tally.panel", d.elements[0], "10");
    rig.fill("tally.panel", d.elements[1], "0");
    rig.fill("tally.panel", d.elements[2], "-2");
    CHECK(has(rig.words("tally.panel"), "step: -2|"));
    rig.press("tally.panel", d.elements[3]);
    CHECK(rig.total() == 30);
    CHECK(has(rig.words("tally.panel"), "Total: 30|"));

    rig.fill("tally.panel", d.elements[0], "0");
    rig.fill("tally.panel", d.elements[1], "10");
    rig.fill("tally.panel", d.elements[2], "3");
    // Return in a field uses the control whose intent takes it.
    rig.tell("tally.panel", ws::PaneKey{view::kPane, zengine::input::scan::kReturn, 0});
    CHECK(rig.total() == 18);
    CHECK(has(rig.words("tally.panel"), "Total: 18|"));
}

TEST_CASE("a refusal answered to a view shows on its notice row, and what it was told stays") {
    Rig rig;
    REQUIRE(rig.ask(view::ViewRun{"builder", bytes_of(panel())}).ok);
    rig.room("tally.panel");
    const auto d = panel();
    rig.press("tally.panel", d.elements[3]);
    REQUIRE(rig.total() == 45);

    rig.fill("tally.panel", d.elements[2], "0");
    rig.press("tally.panel", d.elements[3]);
    CHECK(rig.total() == 45);
    CHECK(has(rig.said("tally.panel"), "refused: "));
    CHECK(has(rig.said("tally.panel"), "a step of 0 never moves the count from 0 toward 10"));
    CHECK(has(rig.words("tally.panel"), "Total: 45|"));
    const auto* picture = rig.latest("tally.panel");
    REQUIRE(picture != nullptr);
    CHECK(picture->texts.back().role == zengine::surface::role::kAlert);

    rig.fill("tally.panel", d.elements[2], "1");
    rig.fill("tally.panel", d.elements[1], "2000000");
    rig.press("tally.panel", d.elements[3]);
    CHECK(rig.total() == 45);
    CHECK(has(rig.said("tally.panel"), "this fold would count 2000000 times"));
    CHECK(has(rig.words("tally.panel"), "Total: 45|"));

    // A field holding no whole number is said here, and nothing is published.
    rig.fill("tally.panel", d.elements[1], "ten");
    const auto before = rig.desk->heard.size();
    rig.press("tally.panel", d.elements[3]);
    CHECK(rig.desk->heard.size() > before);
    CHECK(has(rig.words("tally.panel"), "`limit` holds `ten`, not a whole number"));
    CHECK(rig.total() == 45);
}

TEST_CASE("a label or place change at the same shapes reaches the running view in place; a change of shapes registers afresh and says so") {
    Rig rig;
    REQUIRE(rig.ask(view::ViewRun{"builder", bytes_of(panel(false))}).ok);
    const auto first = rig.bus.role_holder("tally.panel");
    rig.room("tally.panel");
    const auto d = panel();
    rig.fill("tally.panel", d.elements[1], "4");
    rig.press("tally.panel", d.elements[3]);
    CHECK(rig.total() == 6);
    CHECK(has(rig.words("tally.panel"), "|Total|"));

    // Binding Total changes the shapes the view is told: a new participant, said to be one.
    const auto bound = rig.ask(view::ViewApply{"builder", bytes_of(panel(true))});
    REQUIRE_MESSAGE(bound.ok, bound.reason);
    CHECK(bound.fresh);
    CHECK(has(bound.reason, "registered tally.panel afresh: its shapes changed"));
    const auto second = rig.bus.role_holder("tally.panel");
    REQUIRE(second.valid());
    CHECK(second != first);
    rig.room("tally.panel");
    CHECK(has(rig.words("tally.panel"), "Total: waiting|"));
    CHECK(has(rig.words("tally.panel"), "limit: 10|")); // a new participant starts from the description

    rig.fill("tally.panel", d.elements[1], "4");
    rig.press("tally.panel", d.elements[3]);
    CHECK(has(rig.words("tally.panel"), "Total: 6|"));

    // A label and a place change later is applied in place: same participant, same field text.
    auto moved = panel(true, "Sum");
    moved.elements[4].y = 120;
    const auto in_place = rig.ask(view::ViewApply{"builder", bytes_of(moved)});
    REQUIRE_MESSAGE(in_place.ok, in_place.reason);
    CHECK_FALSE(in_place.fresh);
    CHECK(has(in_place.reason, "applied in place"));
    CHECK(rig.bus.role_holder("tally.panel") == second);
    const auto words = rig.words("tally.panel");
    CHECK(has(words, "Sum: 6|"));
    CHECK(has(words, "limit: 4|"));
}

TEST_CASE("a stopped view leaves a picture that says it stopped, and its office is released after it") {
    Rig rig;
    REQUIRE(rig.ask(view::ViewRun{"builder", bytes_of(panel())}).ok);
    rig.room("tally.panel");
    const auto id = rig.bus.role_holder("tally.panel");
    const auto stop = rig.ask(view::ViewStop{"builder"});
    REQUIRE_MESSAGE(stop.ok, stop.reason);
    CHECK_FALSE(rig.bus.role_holder("tally.panel").valid());
    CHECK(rig.views.view(id.value) == nullptr);
    const auto* last = rig.latest("tally.panel");
    REQUIRE(last != nullptr);
    REQUIRE(last->texts.size() == 1);
    CHECK(has(last->texts[0].text, "tally.panel stopped"));
    CHECK(last->rects.size() == 1); // its ground alone: no field or control is left looking live
    // Its last picture was delivered while it still held the office.
    bool delivered = false;
    for (const auto& m : rig.desk->heard)
        if (m.sender == id && loom::same_identity(m.payload.schema(), *loom::schema_of<ws::PaneCanvasContent>()) &&
            has(loom::from_value<ws::PaneCanvasContent>(m.payload).texts.at(0).text, "stopped"))
            delivered = true;
    CHECK(delivered);
    CHECK(has(rig.ask(view::ViewStop{"builder"}).reason, "no view runs in this session"));
    // Run again: a fresh participant that waits until told.
    REQUIRE(rig.ask(view::ViewRun{"builder", bytes_of(panel())}).ok);
    rig.room("tally.panel");
    CHECK(has(rig.words("tally.panel"), "Total: waiting|"));
}

TEST_CASE("a view's terminal picture is the window's picture floored to cells, its text fitted to the cell") {
    const auto d = panel();
    ws::PaneCanvasRoom window{view::kPane, 1, 48 * 40, 48 * 12, 4, true, 8, 16};
    ws::PaneCanvasRoom terminal{view::kPane, 2, 48 * 40, 48 * 12, 48, false, 0, 0};
    const auto a = view::picture(d, {}, {}, window, 1);
    const auto b = view::picture(d, {}, {}, terminal, 1);
    REQUIRE(a.content.rects.size() == b.content.rects.size());
    for (std::size_t i = 0; i < a.content.rects.size(); ++i) {
        CHECK(a.content.rects[i].x == b.content.rects[i].x);
        CHECK(a.content.rects[i].y == b.content.rects[i].y);
        CHECK(a.content.rects[i].w == b.content.rects[i].w);
    }
    // The number field `start` is 144 pixels: 12 cells, so its text gets at most 12 - 1 columns.
    CHECK(b.content.rects[1].w == zengine::surface::subs_of_pixel(144));
    for (const auto& t : b.content.texts) {
        CHECK(t.x % 48 == 0);
        CHECK(t.y % 48 == 0);
    }
    view::Presentation long_text;
    long_text.fields["start"].set("123456789012345678901234567890", 30);
    const auto cut = view::picture(d, {}, long_text, terminal, 2);
    bool fitted = false;
    for (const auto& t : cut.content.texts)
        if (t.text.rfind("start: ", 0) == 0) {
            CHECK(t.text.size() <= 12);
            fitted = true;
        }
    CHECK(fitted);
    // Presses read the same map in both media.
    CHECK(a.hits.size() == b.hits.size());
    REQUIRE(b.hit(zengine::surface::subs_of_pixel(10), zengine::surface::subs_of_pixel(90)) != nullptr);
    CHECK(b.hit(zengine::surface::subs_of_pixel(10), zengine::surface::subs_of_pixel(90))->element == "count");
}

TEST_CASE("a view hands back a right press and an Escape it has no use for") {
    Rig rig;
    REQUIRE(rig.ask(view::ViewRun{"builder", bytes_of(panel())}).ok);
    rig.room("tally.panel");
    const auto d = panel();
    rig.press("tally.panel", d.elements[4], 3);
    const auto passed = std::count_if(rig.desk->heard.begin(), rig.desk->heard.end(), [](const auto& m) {
        return loom::same_identity(m.payload.schema(), *loom::schema_of<ws::PanePassRequested>());
    });
    CHECK(passed == 1);
    rig.tell("tally.panel", ws::PaneKey{view::kPane, zengine::input::scan::kEscape, 0});
    const auto unspent = std::count_if(rig.desk->heard.begin(), rig.desk->heard.end(), [](const auto& m) {
        return loom::same_identity(m.payload.schema(), *loom::schema_of<ws::PaneEscapeUnspent>());
    });
    CHECK(unspent == 1);
    // With a field focused, Escape is spent putting the focus down.
    rig.press("tally.panel", d.elements[0]);
    rig.tell("tally.panel", ws::PaneKey{view::kPane, zengine::input::scan::kEscape, 0});
    CHECK(std::count_if(rig.desk->heard.begin(), rig.desk->heard.end(), [](const auto& m) {
        return loom::same_identity(m.payload.schema(), *loom::schema_of<ws::PaneEscapeUnspent>());
    }) == 1);
}
