// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Workshop panes suite -- a pane's own picture: the measured text it sets runs with, and a
// described view seated and drawn in the area of its own a graphics window shows.

#include "doctest.h"
#include "workshop_support.hpp"
#include "workshop/pane_canvas_text.hpp"
#include "workshop/pane_picture.hpp"
#include "view/host.hpp"
#include "flow/shape.hpp"
#include <limits>

namespace {
/// Every row a pane drew in its area, each ended by a bar: what a case reads of a view's words.
std::string drawn_words(std::int64_t kind) {
    std::string out;
    for (const std::string& row : held_reading(kind).rows) {
        std::size_t at = 0;
        while (at < row.size()) {
            const std::size_t word = row.find_first_not_of(' ', at);
            if (word == std::string::npos) break;
            std::size_t end = row.find("  ", word);
            if (end == std::string::npos) end = row.size();
            out += row.substr(word, end - word) + "|";
            at = end;
        }
    }
    return out;
}
} // namespace

TEST_CASE("a pane's drawings judge a press by what the window drew under the hand: a named part redrawn takes it over its whole face, another part or none refuses it, and an unnamed place takes it only under the drawing's number") {
    using zengine::workshop::press_moved;
    const gfx::TextMeasure text{12, 12};
    const std::string area = "test.area";
    // A BUTTON: a filled face, its label inside, the group named `save`; and an unnamed note.
    const auto drawing = [](const std::string& label, const std::string& action,
                            std::int64_t number) {
        gfx::Sketch s;
        s.fill_rect("room", 0, 0, 240, 120, gfx::by_role(gfx::role::kGround));
        s.group("button", 24, 24);
        s.fill_rect("button/face", 0, 0, 96, 36, gfx::by_role(gfx::role::kAccent));
        s.text("button/label", 12, 12, label, gfx::by_role(gfx::role::kFill));
        if (!action.empty()) s.action("button", action);
        s.text("note", 0, 84, "a note", gfx::by_role(gfx::role::kMuted));
        s.number(number);
        return s;
    };
    const gfx::Sketch drawn = drawing("Save", "save", 7);
    // OVER THE ELEMENT'S WHOLE FACE: its corner, beside its label, names it as its label does.
    CHECK(gfx::part_at(drawn, 25, 25, text) == "save");
    CHECK(gfx::part_at(drawn, 24 + 95, 24 + 35, text) == "save");
    CHECK(gfx::part_at(drawn, 24 + 13, 24 + 13, text) == "save");
    CHECK(gfx::part_at(drawn, 24 + 96, 24 + 36, text).empty());
    // THE SAME BUTTON REDRAWN -- its words, its colour, the drawing's number -- takes the press.
    const gfx::Sketch again = drawing("Saved!", "save", 8);
    CHECK_FALSE(press_moved(again, area, area, "save", 7, 25, 25, text));
    CHECK_FALSE(press_moved(again, area, area, "save", 7, 24 + 30, 24 + 18, text));
    // ANOTHER ELEMENT THERE NOW, or none: refused.
    CHECK(press_moved(drawing("Delete", "delete", 7), area, area, "save", 7, 25, 25, text));
    CHECK(press_moved(drawing("Save", "", 7), area, area, "save", 7, 25, 25, text));
    // AN UNNAMED PLACE: taken under the drawing's number, refused under another, and refused
    // where a named part stands now.
    CHECK_FALSE(press_moved(drawn, area, area, "", 7, 6, 90, text));
    CHECK(press_moved(again, area, area, "", 7, 6, 90, text));
    CHECK(press_moved(drawn, area, area, "", 7, 25, 25, text));
    // NO WORD FROM THE WINDOW, OR ONE ABOUT ANOTHER AREA: nothing to judge.
    CHECK_FALSE(press_moved(drawing("Delete", "delete", 9), area, "", "save", 7, 25, 25, text));
    CHECK_FALSE(press_moved(drawing("Delete", "delete", 9), area, "elsewhere", "save", 7, 25, 25,
                            text));
}

TEST_CASE("pane canvas text clipping preserves surviving positions through both edges") {
    CanvasRoom room{36, 44, 1, true, 8, 18};
    const CanvasText source{-5, 1, "ABCDE", gfx::role::kFill, 2, 0, 5};
    const auto text = clip_canvas_text(source, {0, 0, room.width, room.height}, room);
    REQUIRE(text.visible());
    CHECK(text.first_column == 1);
    CHECK(text.text.text == "BCD");
    CHECK(text.bounds.x == 3);  // original -5 plus one 8-pixel advance
    CHECK(text.bounds.w == 28); // three advances plus both insets
    CHECK(text.text.caret_col == 1);
    CHECK(text.text.sel_begin_col == 0);
    CHECK(text.text.sel_end_col == 3);
    CHECK(text.fit.columns == 3);
    CHECK_FALSE(clip_canvas_text(source, {0, 2, room.width, room.height - 2}, room).visible());
    const auto empty_caret = clip_canvas_text({0, 0, "", gfx::role::kFill, 0},
                                               {0, 0, room.width, room.height}, room);
    REQUIRE(empty_caret.visible());
    CHECK(empty_caret.fit.graphical());
    CHECK(empty_caret.fit.columns == 1);
    CHECK(empty_caret.text.caret_col == 0);
    for (const auto& metrics : {std::pair<std::int64_t, std::int64_t>{8, 18}, {11, 19}, {0, 0}}) {
        room.text_advance_px = metrics.first;
        room.text_line_px = metrics.second;
        for (std::int64_t x = -48; x <= 48; x += 2) {
            for (std::int64_t y = -12; y <= 44; y += 3) {
                auto candidate = source;
                candidate.x = x; candidate.y = y;
                const auto clipped = clip_canvas_text(candidate, {2, 2, 31, 37}, room);
                if (!clipped.visible()) continue;
                CHECK(clipped.bounds.x >= 2);
                CHECK(clipped.bounds.y >= 2);
                CHECK(clipped.bounds.x + clipped.bounds.w <= 33);
                CHECK(clipped.bounds.y + clipped.bounds.h <= 39);
                CHECK(clipped.fit.columns > 0);
                CHECK(clipped.fit.rows == 1);
            }
        }
    }
    const auto lo = (std::numeric_limits<std::int64_t>::min)();
    const auto hi = (std::numeric_limits<std::int64_t>::max)();
    for (const auto x : {lo, lo + 1, hi - 1, hi}) {
        auto distant = source; distant.x = x;
        CHECK_FALSE(clip_canvas_text(distant, {0, 0, room.width, room.height}, room).visible());
    }
    room.text_advance_px = hi; room.text_line_px = hi; room.grain = hi;
    CHECK_FALSE(clip_canvas_text(source, {0, 0, room.width, room.height}, room).visible());
    room.text_advance_px = lo; room.text_line_px = 0; room.grain = lo;
    CHECK(canvas_text_metrics(room).advance == kPaneCanvasUnit);
    CHECK(canvas_text_metrics(room).grain == kPaneCanvasUnit);
}

namespace {
/// Asks the view host as a builder would, and keeps its answers.
class ViewAsker : public loom::WeaveBase<ViewAsker, SeatState, loom::Accept<zengine::view::ViewAnswer>,
                                         loom::Emit<zengine::view::ViewRun, zengine::view::ViewStop>> {
public:
    std::vector<zengine::view::ViewAnswer> answers;
    void on(const zengine::view::ViewAnswer& a, loom::Mail&) { answers.push_back(a); }
};
}

TEST_CASE("a described view of the greatest size its rules allow asks for a pane Workshop admits, and is seated and drawn cut to it") {
    namespace view = zengine::view;
    PaneRig r;
    r.mount_workshop();
    r.host.role_holder = [&r](std::string_view office) { return r.bus.role_holder(office); };
    r.ready();
    r.extent(150, 65);
    view::Host views(r.bus);
    views.mount();
    auto asker = std::make_unique<ViewAsker>();
    auto* client = asker.get();
    loom::Grant asking;
    view::allow_view_requests(asking);
    const auto client_id = r.bus.register_weave(std::move(asker), std::move(asking));
    client->zen_set_self(client_id);

    view::Description d;
    d.name = "wide.panel";
    d.width = view::kMaxSizePx;
    d.height = view::kMaxSizePx;
    d.elements = {{"far", view::Kind::label, "Far", view::kMaxPixels, view::kMaxPixels, 192, 24, ""},
                  {"near", view::Kind::label, "Near", 0, 0, 192, 24, ""}};
    const auto asked = view::offered(d);
    CHECK(asked.width == kMaxPaneBodyPx);
    CHECK(asked.height == kMaxPaneBodyPx);
    const auto bytes = view::description_bytes(d);
    (void)r.bus.send_as_to_role(client_id, view::kViewHostRole,
        loom::Message(loom::to_value(view::ViewRun{"builder", loom::Bytes(bytes.begin(), bytes.end())}), client_id, {}, 1));
    r.bus.drain_until_idle();
    REQUIRE(client->answers.size() == 1);
    REQUIRE_MESSAGE(client->answers[0].ok, client->answers[0].reason);
    // ADMITTED AND SEATED: the catalog lists it, the desk holds it, and it drew in its area.
    const auto* row = r.session().panes.runtime.find("wide.panel", view::kPane);
    REQUIRE(row);
    CHECK(r.session().panes.has(row->kind));
    auto* pane = r.session().panes.external_pane(row->kind);
    REQUIRE(pane);
    REQUIRE(shows_canvas(*pane));
    const std::string words = drawn_words(row->kind);
    CHECK(words.find("Near|") != std::string::npos);
    CHECK(words.find("Far|") == std::string::npos); // past the pane, cut
}

namespace {
/// Hears one shape, whoever says it: a described view's intent, as its accepter would.
class Hears final : public loom::Weave {
public:
    explicit Hears(std::shared_ptr<const loom::Schema> shape) : shape_(std::move(shape)) {}
    std::vector<loom::Message> heard;
    std::vector<std::shared_ptr<const loom::Schema>> accepted_schemas() const override { return {shape_}; }
    void handle(const loom::Message& in, loom::Bus&) override { heard.push_back(in); }
    loom::Value snapshot() const override { return loom::Value(loom::make_schema("canvastest.Hears", 1, {})); }
    loom::Value policy() const override {
        loom::Value v(loom::lifecycle_policy_schema());
        v.set("max_reloads", loom::Cell::integer(0));
        v.set("revive_from_last_good", loom::Cell::boolean(false));
        return v;
    }
    void revive(const loom::Value&) override {}

private:
    std::shared_ptr<const loom::Schema> shape_;
};
}

TEST_CASE("a weaver keys a described view's button by naming its id in their keymap file: the key "
          "says the button's intent, the keymap says what it causes, and its part is the action's") {
    namespace view = zengine::view;
    TempDir dir("view-key");
    const std::string path = (dir.path() / "keymap.json").generic_string();
    write_keymap_file(path, keymap_file_text("default", {{"go", "g"}}));
    PaneRig r;
    r.host.keymap_path = path;
    r.mount_workshop();
    r.host.role_holder = [&r](std::string_view office) { return r.bus.role_holder(office); };
    r.ready();
    r.extent(160, 90, 8, 18, gfx::kCellPx);
    view::Host views(r.bus);
    views.mount();
    auto asker = std::make_unique<ViewAsker>();
    auto* client = asker.get();
    loom::Grant asking;
    view::allow_view_requests(asking);
    const auto client_id = r.bus.register_weave(std::move(asker), std::move(asking));
    client->zen_set_self(client_id);
    const auto said = zengine::flow::shape::make(zengine::flow::shape::qualified("keyed.panel", "Go"));
    auto hears = std::make_unique<Hears>(said);
    auto* heard = hears.get();
    (void)r.bus.register_weave(std::move(hears), loom::Grant{});

    view::Description d;
    d.name = "keyed.panel";
    d.width = 240;
    d.height = 96;
    d.elements = {{"go", view::Kind::button, "Go", 0, 0, 96, 24, ""},
                  {"note", view::Kind::label, "Note", 0, 28, 96, 24, ""}};
    d.intents = {{"go", said, {}}};
    const auto bytes = view::description_bytes(d);
    (void)r.bus.send_as_to_role(client_id, view::kViewHostRole,
        loom::Message(loom::to_value(view::ViewRun{"builder", loom::Bytes(bytes.begin(), bytes.end())}), client_id, {}, 1));
    r.bus.drain_until_idle();
    REQUIRE_FALSE(client->answers.empty());
    REQUIRE_MESSAGE(client->answers.back().ok, client->answers.back().reason);
    const auto* row = r.session().panes.runtime.find("keyed.panel", view::kPane);
    REQUIRE(row);
    REQUIRE(r.session().panes.has(row->kind));
    // DECLARED AND HELD: one row, the button's id, causing the intent's shape.
    REQUIRE(row->actions.size() == 1);
    CHECK(row->actions[0].id == "go");
    REQUIRE(row->actions[0].causes.size() == 1);
    CHECK(row->actions[0].causes[0].shape == "keyed.panel.Go");
    // ...ITS PART THE ACTION'S, AND AVAILABLE: the drawing in its area names the button by the
    // action a press on it asks.
    const auto parts = held_parts(r.session(), row->kind);
    REQUIRE(parts.count("go") == 1);
    CHECK(parts.at("go").available);
    // ...AND THE KEYMAP SAYS IT WHERE A READER LOOKS: its address and its cause.
    bool shown = false;
    for (const ShownBinding& b : keymap_shown(r.session(), std::string(), std::string()).rows) {
        if (b.office != "keyed.panel" || b.id != "go") continue;
        shown = true;
        CHECK(b.pane == view::kPane);
        CHECK(b.gesture == "g");
        REQUIRE(b.causes.size() == 1);
        CHECK(b.causes[0].shape == "keyed.panel.Go");
    }
    CHECK(shown);
    // THE WEAVER'S KEY, WITH THE VIEW HOLDING THE KEYS, SAYS THE INTENT.
    press_body(r, row->kind);
    REQUIRE(r.session().panes.keyboard == row->kind);
    r.key(input::scan::kG);
    REQUIRE(heard->heard.size() == 1);
    CHECK(heard->heard[0].provenance.authored_role().empty()); // said as the view, by publication
}

TEST_CASE("a view asks for its size in pixels and its pane grants exactly that room: to the pixel in a window, to the cells that hold it in a terminal") {
    namespace view = zengine::view;
    PaneRig r;
    r.mount_workshop();
    r.host.role_holder = [&r](std::string_view office) { return r.bus.role_holder(office); };
    r.ready();
    r.extent(160, 90, 8, 18, gfx::kCellPx); // the shipped window's face
    view::Host views(r.bus);
    views.mount();
    auto asker = std::make_unique<ViewAsker>();
    auto* client = asker.get();
    loom::Grant asking;
    view::allow_view_requests(asking);
    const auto client_id = r.bus.register_weave(std::move(asker), std::move(asking));
    client->zen_set_self(client_id);

    struct Size { const char* name; std::int64_t w, h; };
    for (const Size& size : {Size{"even.view", 680, 360}, Size{"odd.view", 683, 361}}) {
        const std::string named = size.name;
        CAPTURE(named);
        view::Description d;
        d.name = size.name;
        d.width = size.w;
        d.height = size.h;
        d.elements = {{"note", view::Kind::label, "Note", 0, 0, 96, 24, ""}};
        const auto bytes = view::description_bytes(d);
        (void)r.bus.send_as_to_role(client_id, view::kViewHostRole,
            loom::Message(loom::to_value(view::ViewRun{size.name, loom::Bytes(bytes.begin(), bytes.end())}), client_id, {}, 1));
        r.bus.drain_until_idle();
        REQUIRE_FALSE(client->answers.empty());
        REQUIRE_MESSAGE(client->answers.back().ok, client->answers.back().reason);
        const auto* row = r.session().panes.runtime.find(size.name, view::kPane);
        REQUIRE(row);
        REQUIRE(r.session().panes.has(row->kind));

        // IN THE WINDOW: the size to the pixel, and the notice's three rows of the face beneath it.
        const auto band = 3 * (18 + 2 * kTextInsetPx);
        const auto* pane = r.session().panes.external_pane(row->kind);
        REQUIRE(pane);
        CHECK(pane->placed_w == size.w);
        CHECK(pane->placed_h == size.h + band);
    }

    // IN A TERMINAL, the same panes derived: each size rounded up to the whole cells that hold
    // it, and three rows of cells beneath.
    r.extent(160, 90);
    for (const Size& size : {Size{"even.view", 680, 360}, Size{"odd.view", 683, 361}}) {
        const std::string named = size.name;
        CAPTURE(named);
        const auto* row = r.session().panes.runtime.find(size.name, view::kPane);
        REQUIRE(row);
        const auto* pane = r.session().panes.external_pane(row->kind);
        REQUIRE(pane);
        const auto up = [](std::int64_t px) { return (px + 11) / 12 * 12; };
        CHECK(pane->placed_w == up(size.w));
        CHECK(pane->placed_h == up(size.h) + 3 * gfx::kCellPx);
    }
}

TEST_CASE("a view run when the stack's column is spent is shown at the column's top, in front") {
    // ⚔ MUTATION: the view's ask to be shown judged by a stack rationed to its column's height --
    // refused for room, as a view run from the View Builder beside the Pane Manager was.
    namespace view = zengine::view;
    PaneRig r;
    r.mount_workshop();
    r.host.role_holder = [&r](std::string_view office) { return r.bus.role_holder(office); };
    r.ready();
    ProviderSeat* seat = r.mount_provider(kHelloOffice);
    r.drive(seat, [](ProviderSeat& s, loom::Mail& m) { s.offer(m, good_offer()); });
    r.pick(hello_ref());
    const std::int64_t hello = r.session().panes.runtime.entries[0].kind;
    REQUIRE(r.session().panes.has(hello));
    const Screen sc = screen_of(r.session());
    REQUIRE(stack_slots_that_fit(sc) == 1);
    const PixelRect first = bounds_of(r.session().panes, r.session().setup.active, hello, sc).rect;

    view::Host views(r.bus);
    views.mount();
    auto asker = std::make_unique<ViewAsker>();
    auto* client = asker.get();
    loom::Grant asking;
    view::allow_view_requests(asking);
    const auto client_id = r.bus.register_weave(std::move(asker), std::move(asking));
    client->zen_set_self(client_id);
    view::Description d;
    d.name = "greeting";
    d.width = 192;
    d.height = 48;
    d.elements = {{"hello", view::Kind::label, "Hello", 0, 0, 192, 24, ""}};
    const auto bytes = view::description_bytes(d);
    (void)r.bus.send_as_to_role(client_id, view::kViewHostRole,
        loom::Message(loom::to_value(view::ViewRun{"builder", loom::Bytes(bytes.begin(), bytes.end())}), client_id, {}, 1));
    r.bus.drain_until_idle();
    REQUIRE(client->answers.size() == 1);
    REQUIRE_MESSAGE(client->answers[0].ok, client->answers[0].reason);
    // SHOWN: the column begins again at its top, the view stands there in front of the pane it
    // covers, and that pane did not move.
    const auto* row = r.session().panes.runtime.find("greeting", view::kPane);
    REQUIRE(row);
    REQUIRE(r.session().panes.has(row->kind));
    CHECK(has_pane(r.session().setup.active, PaneRef{"greeting", view::kPane}));
    const PixelRect landed = bounds_of(r.session().panes, r.session().setup.active, row->kind, sc).rect;
    CHECK(landed.x == first.x);
    CHECK(landed.y == sc.room_y);
    CHECK(effective_pane_order(r.session().setup.active, r.session().panes).back() == row->kind);
    CHECK(bounds_of(r.session().panes, r.session().setup.active, hello, sc).rect == first);
    CHECK(r.last_notice().find("showing greeting") != std::string::npos);
}


TEST_CASE("a view shown where its own place stands off this screen stays there, and the band says so and how to bring it back") {
    // ⚔ MUTATION: the reveal saying the view is shown and has the keys, of a pane no one can see.
    namespace view = zengine::view;
    PaneRig r;
    r.mount_workshop();
    r.host.role_holder = [&r](std::string_view office) { return r.bus.role_holder(office); };
    r.ready();
    // THE VIEW'S ROW ON THE DESK BEFORE IT RUNS, its own place past the room's right edge.
    const PaneRef greeting{"greeting", view::kPane};
    REQUIRE(add_pane(r.session().setup.active, greeting));
    REQUIRE(author_pane_place(r.session().setup.active, greeting, cells_px(300), 0).accepted);
    const PanePlace authored = pane_of(r.session().setup.active, greeting)->place;

    view::Host views(r.bus);
    views.mount();
    auto asker = std::make_unique<ViewAsker>();
    auto* client = asker.get();
    loom::Grant asking;
    view::allow_view_requests(asking);
    const auto client_id = r.bus.register_weave(std::move(asker), std::move(asking));
    client->zen_set_self(client_id);
    view::Description d;
    d.name = "greeting";
    d.width = 192;
    d.height = 48;
    d.elements = {{"hello", view::Kind::label, "Hello", 0, 0, 192, 24, ""}};
    const auto bytes = view::description_bytes(d);
    (void)r.bus.send_as_to_role(client_id, view::kViewHostRole,
        loom::Message(loom::to_value(view::ViewRun{"builder", loom::Bytes(bytes.begin(), bytes.end())}), client_id, {}, 1));
    r.bus.drain_until_idle();
    REQUIRE(client->answers.size() == 1);
    REQUIRE_MESSAGE(client->answers[0].ok, client->answers[0].reason);
    // SEATED WHERE IT WAS PUT, OFF THIS SCREEN, and said so: never "showing ... it has the keys".
    const auto* row = r.session().panes.runtime.find("greeting", view::kPane);
    REQUIRE(row);
    REQUIRE(r.session().panes.has(row->kind));
    CHECK(pane_of(r.session().setup.active, greeting)->place == authored);
    CHECK(bounds_of(r.session().panes, r.session().setup.active, row->kind, screen_of(r.session()))
              .rect.empty());
    CHECK(r.last_notice() == "greeting is off this screen -- Pane Manager > manage it > Reset > "
                             "place, or hide and show it");
    CHECK(r.session().notice_is_bad);
}


TEST_CASE("a described view offers its own pane through the view host, Workshop seats it and it draws in its area, a press reaches it, and a stop leaves a picture that says so") {
    namespace view = zengine::view;
    PaneRig r;
    r.mount_workshop();
    r.host.role_holder = [&r](std::string_view office) { return r.bus.role_holder(office); };
    r.ready();
    r.extent(150, 65);
    std::vector<zengine::graphics::AreaPlaced> placed;
    const auto placing = r.bus.add_observer([&](const loom::BusEvent& e) {
        if (e.kind == loom::EventKind::Delivered && e.payload != nullptr &&
            e.schema_name == zengine::graphics::AreaPlaced::zen_name) {
            const auto p = loom::from_value<zengine::graphics::AreaPlaced>(*e.payload);
            if (p.area == zengine::workshop::pane_area("tally.panel", view::kPane))
                placed.push_back(p);
        }
    });
    view::Host views(r.bus);
    views.mount();
    auto asker = std::make_unique<ViewAsker>();
    auto* client = asker.get();
    loom::Grant asking;
    view::allow_view_requests(asking);
    const auto client_id = r.bus.register_weave(std::move(asker), std::move(asking));
    client->zen_set_self(client_id);

    view::Description d;
    d.name = "tally.panel";
    d.width = 480;
    d.height = 120;
    d.elements = {{"step", view::Kind::number, "step", 0, 0, 144, 24, "1"},
                  {"count", view::Kind::button, "Count", 0, 28, 96, 24, ""},
                  {"total", view::Kind::label, "Total", 0, 56, 192, 24, ""}};
    const auto said = loom::SchemaBuilder("tally.panel.Count", 1).field("step", loom::Kind::Int).build();
    d.intents = {{"count", said, {{"step", "step"}}}};
    d.shows = {{"total", loom::SchemaBuilder("tally.Total", 1).field("total", loom::Kind::Int).build(), "total"}};
    const auto bytes = view::description_bytes(d);
    (void)r.bus.send_as_to_role(client_id, view::kViewHostRole,
        loom::Message(loom::to_value(view::ViewRun{"builder", loom::Bytes(bytes.begin(), bytes.end())}), client_id, {}, 1));
    r.bus.drain_until_idle();
    REQUIRE(client->answers.size() == 1);
    REQUIRE_MESSAGE(client->answers[0].ok, client->answers[0].reason);

    // Offered as its own office, seated on the desk by its own ask, and drawn in its area.
    const auto* row = r.session().panes.runtime.find("tally.panel", view::kPane);
    REQUIRE(row);
    CHECK(row->name == "tally.panel");
    CHECK(r.session().panes.has(row->kind));
    const auto* pane = r.session().panes.external_pane(row->kind);
    REQUIRE(pane);
    REQUIRE(shows_canvas(*pane));
    CHECK(drawn_words(row->kind).find("Total: waiting|") != std::string::npos);
    CHECK(drawn_words(row->kind).find("Count|") != std::string::npos);

    // A press on Count, in the window that shows its area, is the view's to use.
    const auto at = [&](std::int64_t px_x, std::int64_t px_y) {
        const auto* seat = r.session().panes.external_pane(row->kind);
        r.window_click((seat->placed_x + px_x) / kPaneCanvasUnit,
                       (seat->placed_y + px_y) / kPaneCanvasUnit);
    };
    // A view has no hover door: resting over it tells it nothing.
    std::size_t hovers = 0;
    const auto tap = r.bus.add_observer([&](const loom::BusEvent& e) {
        if (e.schema_name == PaneCanvasHover::zen_name) ++hovers;
    });
    r.come_up();
    r.windows.hand({input::PointerMoved{(pane->placed_x + 12) / kPaneCanvasUnit,
                                        (pane->placed_y + 36) / kPaneCanvasUnit, 0, 0,
                                        input::space::kCells, input::mod::kNone}});
    r.bus.drain_until_idle();
    r.bus.remove_observer(tap);
    CHECK(hovers == 0);
    at(12, 36);
    // The notice may wrap at the pane's width: read it as one sentence.
    std::string sentence;
    for (const std::string& line : held_reading(row->kind).rows) sentence += line + " ";
    CHECK(sentence.find("said tally.panel.Count; nothing accepts it") != std::string::npos);

    // ITS AREA IN WORKSHOP'S WINDOW HEARS THE POINTER, placed by Workshop as its office accepts it.
    r.bus.remove_observer(placing);
    REQUIRE_FALSE(placed.empty());
    CHECK(placed.back().pointer);
    CHECK(placed.back().owner == "tally.panel");

    // Stopped: the last picture in its area says so, and no control is left looking live.
    (void)r.bus.send_as_to_role(client_id, view::kViewHostRole,
        loom::Message(loom::to_value(view::ViewStop{"builder"}), client_id, {}, 2));
    r.bus.drain_until_idle();
    REQUIRE(client->answers.size() == 2);
    CHECK(client->answers[1].ok);
    CHECK_FALSE(r.bus.role_holder("tally.panel").valid());
    REQUIRE(r.session().panes.external_pane(row->kind) != nullptr);
    CHECK(drawn_words(row->kind).find("tally.panel stopped") == 0);
    CHECK(held_reading(row->kind).parts.empty());
    // ...and once Workshop repaints and sees the provider gone, its frame says the pane waits for
    // one, in the window: no field or button is left looking live.
    r.key(input::scan::kUnknown);
    const auto* waiting = r.session().panes.external_pane(row->kind);
    REQUIRE(waiting);
    CHECK_FALSE(waiting->held);
    CHECK(rect_text(r.shown(), external_body_rect(r.session(), row->kind))
              .find(std::string(kExternalWaiting) + "tally.panel") != std::string::npos);
}
