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

/// The tally panel: three number fields, a button saying `tally.panel.Count`, and a label, in a
/// view 480 by 192; `bound` shows `tally.Total.total` on it.
view::Description panel(bool bound = true, std::string total_label = "Total") {
    view::Description d;
    d.name = "tally.panel";
    d.width = 480;
    d.height = 192;
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

/// The bytes a view description was saved as before a view had a size: version 1, every field
/// of the current form but the size.
std::string first_version_bytes(const view::Description& d) {
    const auto now = view::encode(d);
    loom::Value v(view::description_schema_v1());
    for (const auto& f : view::description_schema_v1()->fields())
        if (const auto* cell = now.get(f.name)) v.set(f.name, *cell);
    v.set("format_version", loom::Cell::integer(1));
    return loom::serialize(v);
}

/// Stands in for Workshop's office: hears what a pane says, and speaks to a view as Workshop.
class Desk final : public loom::Weave {
public:
    std::vector<loom::Message> heard;
    std::vector<std::shared_ptr<const loom::Schema>> accepted_schemas() const override {
        return {loom::schema_of<ws::v3::PaneOffered>(), loom::schema_of<ws::PaneContent>(),
                loom::schema_of<ws::PaneCanvasContent>(), loom::schema_of<ws::v4::PaneCanvasContent>(),
                loom::schema_of<ws::PaneEscapeUnspent>(),
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
        ws::PaneCanvasRoom r{view::kPane, ++grant, 12 * 40, 12 * 16, terminal ? 12 : 1, !terminal,
                             terminal ? 0 : 8, terminal ? 0 : 16};
        tell(office, r);
    }
    /// The latest picture a view said, as its parts name it (none for a picture naming nothing).
    const ws::v4::PaneCanvasContent* latest_named(const std::string& office) const {
        static ws::v4::PaneCanvasContent kept;
        for (auto it = desk->heard.rbegin(); it != desk->heard.rend(); ++it) {
            if (it->provenance.authored_role() != office) continue;
            if (loom::same_identity(it->payload.schema(), *loom::schema_of<ws::v4::PaneCanvasContent>())) {
                kept = loom::from_value<ws::v4::PaneCanvasContent>(it->payload);
                return &kept;
            }
            if (loom::same_identity(it->payload.schema(), *loom::schema_of<ws::PaneCanvasContent>())) {
                const auto said = loom::from_value<ws::PaneCanvasContent>(it->payload);
                kept = ws::v4::PaneCanvasContent{said.pane, said.grant, said.picture, said.rects,
                                                 said.labels, said.texts, {}};
                return &kept;
            }
        }
        return nullptr;
    }
    const ws::PaneCanvasContent* latest(const std::string& office) const {
        static ws::PaneCanvasContent kept;
        const auto* named = latest_named(office);
        if (named == nullptr) return nullptr;
        kept = ws::PaneCanvasContent{named->pane, named->grant, named->picture, named->rects,
                                     named->labels, named->texts};
        return &kept;
    }
    /// Every line of the latest picture, as a weaver reads it: its characters without the blanks
    /// after the last (where a caret after a value stands), each closed by `|`.
    std::string words(const std::string& office) const {
        std::string out;
        if (const auto* p = latest(office))
            for (const auto& t : p->texts) out += t.text.substr(0, t.text.find_last_not_of(' ') + 1) + "|";
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
                                           button, (e.x + e.w / 2),
                                           (e.y + e.h / 2)});
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

    CHECK(read.description.width == 480);
    CHECK(read.description.height == 192);

    // The same fields under a newer envelope: refused by the number it claims, before a field.
    auto later = loom::SchemaBuilder("zengine.view.Description", 3).field("format", loom::Kind::Text).build();
    loom::Value v(later);
    v.set("format", loom::Cell::text(view::kFormat));
    const auto refused = view::read_description(loom::serialize(v));
    CHECK_FALSE(refused.ok);
    CHECK(has(refused.reason, "a view description of version 3; this build reads versions 1 to 2"));
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

TEST_CASE("a running view asks its pane for its size, and draws in its size whatever room its pane is granted") {
    Rig rig;
    auto d = panel();
    d.width = 600;
    d.height = 300;
    const auto run = rig.ask(view::ViewRun{"builder", bytes_of(d)});
    REQUIRE_MESSAGE(run.ok, run.reason);
    // ASKED FOR ITS SIZE, in canvas pixels, and the notice's three rows of text beneath it.
    std::optional<ws::v3::PaneOffered> offered;
    for (const auto& m : rig.desk->heard)
        if (loom::same_identity(m.payload.schema(), *loom::schema_of<ws::v3::PaneOffered>()))
            offered = loom::from_value<ws::v3::PaneOffered>(m.payload);
    REQUIRE(offered);
    CHECK(offered->width == 600);
    CHECK(offered->height == 300);
    CHECK(offered->text_rows == 3); // its notice rows beneath
    // GRANTED MORE, it is laid out in its size: its elements end where the size does, its notice
    // rows lie beneath it, and the rest of the room is its ground.
    rig.tell("tally.panel", ws::PaneCanvasRoom{view::kPane, ++rig.grant, 12 * 80, 12 * 40, 1, true, 8, 16});
    const auto* p = rig.latest("tally.panel");
    REQUIRE(p != nullptr);
    const auto size_w = 600, size_h = 300;
    CHECK(std::any_of(p->rects.begin(), p->rects.end(), [&](const auto& r) {
        return r.role == zengine::surface::role::kGround && r.x == 0 && r.y == 0 && r.w == 12 * 80 && r.h == 12 * 40;
    }));
    for (const auto& r : p->rects) {
        if (r.role == zengine::surface::role::kGround) continue;
        CHECK(r.x + r.w <= size_w);
        CHECK(r.y + r.h <= size_h);
    }
    REQUIRE_FALSE(p->texts.empty());
    const auto& last = p->texts.back(); // what it still waits to be told, on its last row
    CHECK(has(last.text, "waiting to be told"));
    CHECK(last.y + (16 + 2 * 2) == size_h + 3 * (16 + 2 * 2)); // the notice rows beneath the size
    // GRANTED LESS, it draws in what it was granted.
    rig.tell("tally.panel", ws::PaneCanvasRoom{view::kPane, ++rig.grant, 12 * 20, 12 * 10, 1, true, 8, 16});
    p = rig.latest("tally.panel");
    REQUIRE(p != nullptr);
    for (const auto& r : p->rects) {
        CHECK(r.x + r.w <= 12 * 20);
        CHECK(r.y + r.h <= 12 * 10);
    }
}

TEST_CASE("no size the rules accept puts an element under the notice: its rows lie beneath the view's size, lines of the medium's text, in a window and in a terminal") {
    // The tally panel shrunk to its elements, Total bound and not yet told: 192 by 136, Total
    // from 112 to 136, and a notice that says what it waits for.
    auto d = panel();
    d.width = 192;
    d.height = 136;
    REQUIRE(view::problem(d).empty());
    const ws::PaneCanvasRoom window{view::kPane, 1, 12 * 40, 12 * 40, 1, true, 8, 16};
    const ws::PaneCanvasRoom terminal{view::kPane, 1, 12 * 40, 12 * 40, 12, false, 0, 0};
    for (const auto& room : {window, terminal}) {
        INFO("graphical: " << room.graphical);
        const auto p = view::picture(d, {}, view::Presentation{}, room, 1);
        const auto size_h = 136;
        bool total = false, notice = false;
        for (const auto& t : p.content.texts) {
            if (t.text.rfind("Total", 0) == 0) {
                total = true;
                CHECK(t.y < size_h);
            }
            if (t.text.rfind("waiting to", 0) == 0) {
                notice = true;
                CHECK(t.y >= size_h); // beneath the size, never over an element
            }
        }
        CHECK(total);
        CHECK(notice);
        // The notice's rows are the medium's lines: three of them beneath the size.
        const auto line = room.graphical ? (16 + 2 * 2) : 12;
        CHECK(view::notice_band(room) == 3 * line);
    }
    // ...and the pane it asks for holds the size, to the pixel, and the notice rows beneath it.
    CHECK(view::offered(d).width == d.width);
    CHECK(view::offered(d).height == d.height);
    CHECK(view::offered(d).text_rows == 3);
}

TEST_CASE("a view has a size that holds its elements; one saved without a size reads with the size its elements and notice need, and is written with it") {
    // THE SIZE IS SAVED with the view, and every element sits inside it.
    auto d = panel();
    d.width = 600;
    d.height = 300;
    auto read = view::read_description(view::description_bytes(d));
    REQUIRE_MESSAGE(read.ok, read.reason);
    CHECK(read.description.width == 600);
    CHECK(read.description.height == 300);
    auto past = panel();
    past.elements[4].x = 300; // total, 192 wide, to 492 in a view 480 wide
    CHECK(has(view::problem(past), "`total` reaches to 492,136, past the view's size of 480 by 192"));
    auto low = panel();
    low.height = 120;
    CHECK(has(view::problem(low), "`total` reaches to 192,136, past the view's size of 480 by 120"));
    auto tiny = panel();
    tiny.elements.clear();
    tiny.intents.clear();
    tiny.shows.clear();
    tiny.width = view::kMinWidthPx - 1;
    CHECK(has(view::problem(tiny), "a view's size is whole pixels from 120 by 48 to 16384 by 16384; 119 by 192 is not"));
    tiny.width = view::kMinWidthPx;
    tiny.height = view::kMaxSizePx + 1;
    CHECK_FALSE(view::problem(tiny).empty());
    tiny.height = view::kMinHeightPx;
    CHECK(view::problem(tiny).empty());

    // A VIEW SAVED BEFORE A VIEW HAD A SIZE reads whole, with the size its elements and notice
    // rows need: rows below its lowest element, two columns past its rightmost, at least 40
    // columns, as its pane was asked for then.
    const auto old = first_version_bytes(panel());
    CHECK(loom::parse(old).claimed_version() == 1);
    read = view::read_description(old);
    REQUIRE_MESSAGE(read.ok, read.reason);
    CHECK(read.description.width == 40 * 12);
    CHECK(read.description.height == (12 + 1) * 12); // the rows asked for then, less the notice's
    CHECK(read.description.elements.size() == 5);
    CHECK(view::same_shapes(read.description, panel()));
    // ...one reaching far gets a size that holds it,
    auto far_out = panel();
    far_out.width = view::kMaxSizePx;
    far_out.height = view::kMaxSizePx;
    far_out.elements[4].x = view::kMaxPixels;
    far_out.elements[4].y = 1000;
    read = view::read_description(first_version_bytes(far_out));
    REQUIRE_MESSAGE(read.ok, read.reason);
    CHECK(read.description.width == view::kMaxPixels + 192);
    CHECK(read.description.height == 1024);
    // ...and it is written again as the current version, with that size.
    const auto again = view::description_bytes(read.description);
    CHECK(loom::parse(again).claimed_version() == static_cast<std::uint32_t>(view::kFormatVersion));
    CHECK(view::read_description(again).description.width == view::kMaxPixels + 192);
}

TEST_CASE("a resumed view offers its pane and asks nothing of the desk; a run asks to be shown") {
    // ⚔ MUTATION: `start` revealing whatever it is told -- the resumed view asks for a seat, and
    // a pane a weaver hid comes back on the desk at every relaunch; the second check goes red.
    Rig rig;
    const auto resumed = rig.ask(view::ViewResume{"builder", bytes_of(panel())});
    REQUIRE_MESSAGE(resumed.ok, resumed.reason);
    CHECK(resumed.action == "resume");
    CHECK(resumed.fresh);
    CHECK(resumed.office == "tally.panel");
    bool offered = false, seat = false;
    for (const auto& m : rig.desk->heard) {
        if (m.provenance.authored_role() != "tally.panel") continue;
        offered |= loom::same_identity(m.payload.schema(), *loom::schema_of<ws::v3::PaneOffered>());
        seat |= loom::same_identity(m.payload.schema(), *loom::schema_of<ws::PaneRevealRequested>());
    }
    CHECK(offered);
    CHECK_FALSE(seat);
    // IT IS AN ORDINARY RUNNING VIEW OF THIS SESSION: a second run or resume is refused, and a stop
    // ends it.
    CHECK(has(rig.ask(view::ViewResume{"builder", bytes_of(panel())}).reason, "already runs a view"));
    CHECK(has(rig.ask(view::ViewRun{"builder", bytes_of(panel())}).reason, "already runs a view"));
    REQUIRE(rig.ask(view::ViewStop{"builder"}).ok);
    // ...AND A RUN, ITS SIBLING, STILL ASKS TO BE SHOWN.
    rig.desk->heard.clear();
    REQUIRE(rig.ask(view::ViewRun{"builder", bytes_of(panel())}).ok);
    seat = false;
    for (const auto& m : rig.desk->heard)
        if (m.provenance.authored_role() == "tally.panel")
            seat |= loom::same_identity(m.payload.schema(), *loom::schema_of<ws::PaneRevealRequested>());
    CHECK(seat);
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
        offered |= loom::same_identity(m.payload.schema(), *loom::schema_of<ws::v3::PaneOffered>());
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

    // Bytes that are no description are refused in the reader's words, and nothing registers.
    const auto garbage = rig.ask(view::ViewRun{"other", loom::Bytes{1, 2, 3}});
    CHECK_FALSE(garbage.ok);
    CHECK(garbage.action == "run");
    CHECK(has(garbage.reason, "these bytes are not a Zen value"));
    // A host without a canvas grants prose: the view says, in one row, that it needs a canvas.
    rig.tell("tally.panel", ws::PaneRoom{view::kPane, 4, 60});
    bool prose = false;
    for (const auto& m : rig.desk->heard)
        if (m.provenance.authored_role() == "tally.panel" &&
            loom::same_identity(m.payload.schema(), *loom::schema_of<ws::PaneContent>())) {
            const auto rows = loom::from_value<ws::PaneContent>(m.payload).rows;
            prose = rows.size() == 1 && has(rows[0].text, "draws on a canvas-capable Workshop");
        }
    CHECK(prose);
    // A second run in the same session is refused; a name already held is refused.
    CHECK(has(rig.ask(view::ViewRun{"builder", bytes_of(panel())}).reason, "already runs a view"));
    const auto clash = rig.ask(view::ViewRun{"second", bytes_of(panel())});
    CHECK_FALSE(clash.ok);
    CHECK(has(clash.reason, "`tally.panel` is already held"));
    // The host runs at most kMaxViews views: the next is refused out loud.
    for (std::size_t n = 1; n < view::kMaxViews; ++n) {
        auto another = panel();
        another.name = "panel" + std::to_string(n);
        another.intents.clear();
        REQUIRE(rig.ask(view::ViewRun{"s" + std::to_string(n), bytes_of(another)}).ok);
    }
    auto one_more = panel();
    one_more.name = "panel.extra";
    one_more.intents.clear();
    const auto full = rig.ask(view::ViewRun{"extra", bytes_of(one_more)});
    CHECK_FALSE(full.ok);
    CHECK(has(full.reason, "already runs " + std::to_string(view::kMaxViews) + " views"));
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

TEST_CASE("WL-HAND-06: a running view names each element by its id over the place a press on it lands, and a press there uses it") {
    Rig rig;
    const auto d = panel();
    REQUIRE(rig.ask(view::ViewRun{"builder", bytes_of(d)}).ok);
    rig.room("tally.panel");
    const auto* p = rig.latest_named("tally.panel");
    REQUIRE(p != nullptr);
    REQUIRE(p->parts.size() == d.elements.size());
    for (const auto& e : d.elements) {
        CAPTURE(e.id);
        const auto part = std::find_if(p->parts.begin(), p->parts.end(),
                                       [&](const auto& q) { return q.name == "element:" + e.id; });
        REQUIRE(part != p->parts.end());
        CHECK(part->x == e.x);
        CHECK(part->y == e.y);
        CHECK(part->w == e.w);
        CHECK(part->h == e.h);
    }
    // ...LISTED AS THE VIEW DRAWS THEM, which is how it reads a press: where two lie over one
    // place, the later is listed after and takes the press.
    for (std::size_t i = 0; i < d.elements.size(); ++i) {
        CAPTURE(i);
        CHECK(p->parts[i].name == "element:" + d.elements[i].id);
    }
    // PRESSED AT THE MIDDLE OF THE PLACE ITS NAME GIVES, the button says its intent.
    const auto count = std::find_if(p->parts.begin(), p->parts.end(),
                                    [](const auto& q) { return q.name == "element:count"; });
    REQUIRE(count != p->parts.end());
    rig.tell("tally.panel", ws::PaneCanvasPointer{view::kPane, p->grant, p->picture, 1,
                                                  ws::canvas_pointer::kPress, 1,
                                                  count->x + count->w / 2, count->y + count->h / 2});
    CHECK(rig.total() == 45);
}

TEST_CASE("a refusal answered to a view shows on its notice row, and what it was told stays") {
    Rig rig;
    REQUIRE(rig.ask(view::ViewRun{"builder", bytes_of(panel())}).ok);
    rig.room("tally.panel");
    const auto d = panel();
    rig.press("tally.panel", d.elements[3]);
    REQUIRE(rig.total() == 45);

    // A refusal that answers none of the view's own intents is not shown: a stranger's word,
    // whatever it says, does not reach the notice row.
    (void)rig.bus.send_as(rig.desk_id, rig.bus.role_holder("tally.panel"),
                          loom::Message(loom::to_value(loom::Refused{"a stranger says no"}), rig.desk_id, {}, 999));
    rig.pump();
    rig.room("tally.panel");
    CHECK_FALSE(has(rig.said("tally.panel"), "a stranger says no"));

    rig.fill("tally.panel", d.elements[2], "0");
    rig.press("tally.panel", d.elements[3]);
    CHECK(rig.total() == 45);
    // The owner's whole sentence, which names where it happened as its weaver composed it.
    CHECK(has(rig.said("tally.panel"), "refused: tally on tally.panel.Count at %0 fold math.add: "
                                       "a step of 0 never moves the count from 0 toward 10"));
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

    // The pane a view asks for is wide enough that the refusal of a step of 0 reads whole on its
    // notice rows, in a terminal's cells: its size rounded up to whole cells, and three rows.
    const auto o = view::offered(d);
    view::Presentation refusal;
    refusal.notice = "refused: tally on tally.panel.Count at %0 fold math.add: a step of 0 never moves the count from 0 toward 10";
    refusal.alert = true;
    const auto up = [](std::int64_t px) { return (px + 11) / 12 * 12; };
    const ws::PaneCanvasRoom asked{view::kPane, 9, up(o.width), up(o.height) + 12 * o.text_rows,
                                   12, false, 0, 0};
    std::string notice;
    for (const auto& t : view::picture(d, {}, refusal, asked, 1).content.texts)
        if (t.role == zengine::surface::role::kAlert) notice += t.text + " ";
    CHECK(has(notice, "from 0 toward 10"));
    CHECK_FALSE(has(notice, "..."));
}

TEST_CASE("a refusal reaches the notice row only when Loom attests it answers the view's own intent; another participant's at that intent's correlation does not") {
    Rig rig;
    REQUIRE(rig.ask(view::ViewRun{"builder", bytes_of(panel())}).ok);
    const auto id = rig.bus.role_holder("tally.panel");
    auto heard = std::make_unique<Listener>(std::vector{hwfix::count_schema()});
    auto* bystander = heard.get();
    const auto other = rig.bus.register_weave(std::move(heard), loom::Grant{}.allow_any());
    rig.room("tally.panel");
    const auto d = panel();
    rig.press("tally.panel", d.elements[3]);
    REQUIRE(rig.total() == 45);
    REQUIRE(bystander->heard.size() == 1);
    const auto said = bystander->heard[0].correlation;
    REQUIRE(said != 0);

    // The correlation names the intent; it authenticates nothing. Said to the view by ordinary
    // send, a refusal under it is not the answer to the view's delivery, and is not shown.
    (void)rig.bus.send_as(other, id, loom::Message(loom::to_value(loom::Refused{"another says no"}), other, {}, said));
    rig.pump();
    rig.room("tally.panel");
    CHECK_FALSE(has(rig.said("tally.panel"), "another says no"));
    CHECK(has(rig.words("tally.panel"), "Total: 45|"));

    // The tally's own refusal is Loom's answer to the view's intent, and is shown.
    rig.fill("tally.panel", d.elements[2], "0");
    rig.press("tally.panel", d.elements[3]);
    CHECK(has(rig.said("tally.panel"), "refused: tally on tally.panel.Count at %0 fold math.add: "
                                       "a step of 0 never moves the count from 0 toward 10"));
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

TEST_CASE("a replacement that cannot register leaves the running view and its session as they were and says why; a rename at the bound takes the place of the view it renames") {
    Rig rig;
    REQUIRE(rig.ask(view::ViewRun{"builder", bytes_of(panel(false))}).ok);
    const auto first = rig.bus.role_holder("tally.panel");
    rig.room("tally.panel");
    const auto d = panel();

    // The intent changed at the same name and version, which the tally still means otherwise:
    // Loom refuses the successor, and the running view goes on as it was.
    auto narrower = panel(true);
    auto fewer = shape::make(shape::qualified("tally.panel", "Count"));
    for (const char* f : {"start", "limit"})
        fewer = shape::with_field(*fewer, f, loom::type_of(loom::Kind::Int), true);
    narrower.intents[0].shape = fewer;
    narrower.intents[0].fields.pop_back();
    const auto conflict = rig.ask(view::ViewApply{"builder", bytes_of(narrower)});
    CHECK_FALSE(conflict.ok);
    CHECK(conflict.action == "apply");
    CHECK(has(conflict.reason, "'tally.panel.Count' v1 is already published with a different shape"));
    CHECK(rig.bus.role_holder("tally.panel") == first);
    REQUIRE(rig.views.view(first.value) != nullptr);
    rig.fill("tally.panel", d.elements[1], "4");
    rig.press("tally.panel", d.elements[3]);
    CHECK(rig.total() == 6);

    // Renamed into a name whose intent another participant means otherwise: the same, and the
    // view keeps its office and its picture.
    (void)rig.bus.register_weave(std::make_unique<Listener>(std::vector{
                                     loom::SchemaBuilder("tally.other.Count", 1).field("n", loom::Kind::Text).build()}),
                                 loom::Grant{});
    auto other = panel(false);
    other.name = "tally.other";
    other.intents[0].shape = shape::make(shape::qualified("tally.other", "Count"), count_shape()->fields());
    const auto clash = rig.ask(view::ViewApply{"builder", bytes_of(other)});
    CHECK_FALSE(clash.ok);
    CHECK(has(clash.reason, "'tally.other.Count' v1 is already published with a different shape"));
    CHECK(rig.bus.role_holder("tally.panel") == first);
    CHECK_FALSE(rig.bus.role_holder("tally.other").valid());
    CHECK(has(rig.words("tally.panel"), "limit: 4|"));
    CHECK_FALSE(has(rig.said("tally.panel"), "renamed"));

    // AT THE BOUND a rename takes the place of the view it renames: the count stays where it was.
    for (std::size_t n = 1; n < view::kMaxViews; ++n) {
        auto another = panel();
        another.name = "panel" + std::to_string(n);
        another.intents.clear();
        REQUIRE(rig.ask(view::ViewRun{"s" + std::to_string(n), bytes_of(another)}).ok);
    }
    auto renamed = panel(false);
    renamed.name = "tally.renamed";
    renamed.intents[0].shape = shape::make(shape::qualified("tally.renamed", "Count"), count_shape()->fields());
    const auto moved = rig.ask(view::ViewApply{"builder", bytes_of(renamed)});
    REQUIRE_MESSAGE(moved.ok, moved.reason);
    CHECK(moved.fresh);
    CHECK(moved.office == "tally.renamed");
    CHECK(rig.bus.role_holder("tally.renamed").valid());
    CHECK_FALSE(rig.bus.role_holder("tally.panel").valid());
    CHECK(rig.views.view(first.value) == nullptr);
    auto extra = panel();
    extra.name = "panel.extra";
    extra.intents.clear();
    CHECK(has(rig.ask(view::ViewRun{"extra", bytes_of(extra)}).reason,
              "already runs " + std::to_string(view::kMaxViews) + " views"));
    // ...and the session names the view it runs.
    const auto stop = rig.ask(view::ViewStop{"builder"});
    REQUIRE_MESSAGE(stop.ok, stop.reason);
    CHECK(stop.office == "tally.renamed");
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
    // A refusal names the ask it refuses, so the asker can settle it.
    const auto again = rig.ask(view::ViewStop{"builder"});
    CHECK_FALSE(again.ok);
    CHECK(again.action == "stop");
    CHECK(has(again.reason, "no view runs in this session"));
    // Run again: a fresh participant that waits until told.
    REQUIRE(rig.ask(view::ViewRun{"builder", bytes_of(panel())}).ok);
    rig.room("tally.panel");
    CHECK(has(rig.words("tally.panel"), "Total: waiting|"));
}

TEST_CASE("a view's terminal picture is the window's picture floored to cells, its text fitted to the cell") {
    const auto d = panel();
    ws::PaneCanvasRoom window{view::kPane, 1, 12 * 40, 12 * 12, 1, true, 8, 16};
    ws::PaneCanvasRoom terminal{view::kPane, 2, 12 * 40, 12 * 12, 12, false, 0, 0};
    const auto a = view::picture(d, {}, {}, window, 1);
    const auto b = view::picture(d, {}, {}, terminal, 1);
    REQUIRE(a.content.rects.size() == b.content.rects.size());
    for (std::size_t i = 0; i < a.content.rects.size(); ++i) {
        CHECK(a.content.rects[i].x == b.content.rects[i].x);
        CHECK(a.content.rects[i].y == b.content.rects[i].y);
        CHECK(a.content.rects[i].w == b.content.rects[i].w);
    }
    // The number field `start` is 144 pixels: 12 cells, so its text gets at most 12 - 1 columns.
    CHECK(b.content.rects[1].w == 144);
    for (const auto& t : b.content.texts) {
        CHECK(t.x % 12 == 0);
        CHECK(t.y % 12 == 0);
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
    // A control is a quiet box whose words stay readable in both media: focus is the caret and
    // the accent of a field's words, and a button's words are accented.
    view::Presentation focused;
    focused.focus = "step";
    const auto marked = view::picture(d, {}, focused, terminal, 3);
    for (const auto& r : marked.content.rects)
        if (r.role != zengine::surface::role::kGround) CHECK(r.role == zengine::surface::role::kMuted);
    for (const auto& t : marked.content.texts) {
        if (t.text.rfind("step: ", 0) == 0) {
            CHECK(t.role == zengine::surface::role::kAccent);
            CHECK(t.caret_col >= 0);
        }
        if (t.text == "Count") CHECK(t.role == zengine::surface::role::kAccent);
        if (t.text.rfind("start: ", 0) == 0) CHECK(t.role == zengine::surface::role::kFill);
    }
    // Presses read the same map in both media.
    CHECK(a.hits.size() == b.hits.size());
    REQUIRE(b.hit(10, 90) != nullptr);
    CHECK(b.hit(10, 90)->element == "count");
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
