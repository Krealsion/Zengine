// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// INDEPENDENT INFO VIEWS, through the real loaded Info, Inventory and Inventory pane artifacts:
// per-view custody of drafts, pictures and pending requests; field-to-field composition; the
// scoped observation lease a watch spends; Sample Source; and the bounded view lifecycle.
// agents/inventory.md owns the law; docs/workshop/info-views.md is the maker's guide.

#include "inventory_story.hpp"
#include "info-pane/vocabulary.hpp"
#include "inventory/observation.hpp"
#include "workshop/setup_control.hpp"
#include <zen/weave/poke.hpp>

#include <map>

using namespace inventory_story;
namespace info = zengine::info_pane;

namespace {

/// A LOADED STORY WITH INFO VIEWS PLACED WHERE A CASE CAN PRESS THEM.
struct Views : InventoryStory {
    explicit Views(int permissions = 191, bool composer = false) : InventoryStory(permissions, composer) {}
    ~Views() { if (tapped) r.bus.remove_observer(tap); }

    std::int64_t kind_of(const std::string& key) {
        const auto* p = r.session().panels.runtime.find(info::kInfoPaneRole, key);
        return p ? p->kind : -1;
    }
    /// PUT ONE PANE AT A CELL RECTANGLE and reseat the desk.
    void place(const std::string& provider, const std::string& key, std::int64_t x, std::int64_t y,
               std::int64_t w, std::int64_t h) {
        for (auto& p : r.session().setup.active.panes) {
            if (p.ref.provider != provider || p.ref.pane != key) continue;
            p.place = {pane_unit::kSubcells, x * surface::kCellSubs, y * surface::kCellSubs};
            p.width = {pane_unit::kSubcells, w * surface::kCellSubs};
            p.height = {pane_unit::kSubcells, h * surface::kCellSubs};
        }
        // A SAME-SIZE EXTENT RESEATS NOTHING, so the room is changed and changed back: the desk
        // re-seats every pane at its authored place and grants the new rooms.
        r.extent(179, 60);
        r.extent(180, 60);
    }
    /// THE X OF ONE PROSE COLUMN, found through the press measurer itself.
    std::int64_t x_of(std::int64_t kind, std::int64_t row, std::int64_t column) {
        const auto rect = external_body_rect(r.session(), kind);
        if (debug_x) {
            MESSAGE("x_of kind=" << kind << " row=" << row << " rect=" << rect.x << "," << rect.y << " " << rect.w << "x" << rect.h
                    << " has=" << r.session().panels.has(kind) << " titles=" << external_title_rows(r.session().panels, kind, r.session().pane_titles));
            for (const auto& p : r.session().setup.active.panes)
                MESSAGE("row " << p.ref.provider << "/" << p.ref.pane << " mode=" << static_cast<int>(p.place.mode) << " x=" << p.place.x << " y=" << p.place.y);
            for (const auto& line : pane_rows(r, kind)) MESSAGE("| " << line);
        }
        const auto y = rect.y + row + surface::kTuiCanvasTopRow +
                       external_title_rows(r.session().panels, kind, r.session().pane_titles);
        for (std::int64_t dx = -2; dx < 6; ++dx) {
            const auto at = external_press_at(r.session().panels, r.session().setup.active, screen_of(r.session()),
                kind, r.session().pane_titles, input::space::kCells, rect.x + dx, y);
            if (at.named && at.column == 0) return rect.x + dx + column;
        }
        FAIL("no column 0 in pane " << kind);
        return 0;
    }
    input::InjectedEvent at(std::int64_t kind, std::int64_t row, std::int64_t column, bool down) {
        auto e = button_at(kind, row, down);
        e.x = x_of(kind, row, column);
        return e;
    }
    /// BOTH HALVES MEASURED BEFORE EITHER IS SENT: a press may close the pane it lands in.
    void press_at(std::int64_t kind, std::int64_t row, std::int64_t column) {
        const auto down = at(kind, row, column, true), up = at(kind, row, column, false);
        event(down); event(up);
    }
    /// WHERE `needle` IS PAINTED in a pane: its row and first column, or {-1,-1}.
    std::pair<std::int64_t, std::int64_t> where(std::int64_t kind, const std::string& needle) {
        const auto rows = pane_rows(r, kind);
        for (std::size_t i = 0; i < rows.size(); ++i) {
            const auto c = rows[i].find(needle);
            if (c != std::string::npos) return {static_cast<std::int64_t>(i), static_cast<std::int64_t>(c)};
        }
        return {-1, -1};
    }
    bool shows(std::int64_t kind, const std::string& needle) { return where(kind, needle).first >= 0; }
    /// THE ROW A FIELD IS PAINTED ON: selection mark, change mark, then `label:`.
    std::pair<std::int64_t, std::int64_t> field_at(std::int64_t kind, const std::string& label) {
        const auto rows = pane_rows(r, kind);
        const auto want = label + ":";
        for (std::size_t i = 0; i < rows.size(); ++i)
            if (rows[i].size() > 2 + want.size() && rows[i].compare(2, want.size(), want) == 0)
                return {static_cast<std::int64_t>(i), 3};
        FAIL("no field " << label << " in:\n" << shown(kind));
        return {-1, -1};
    }
    /// PRESS A VISIBLE CONTROL BY ITS LABEL, on its middle column.
    void button(std::int64_t kind, const std::string& label) {
        const auto [row, column] = where(kind, "[" + label + "]");
        REQUIRE_MESSAGE(row >= 0, "no [" << label << "] in:\n" << shown(kind));
        press_at(kind, row, column + 1 + static_cast<std::int64_t>(label.size()) / 2);
    }
    /// DRAG FROM ONE PAINTED PLACE TO ANOTHER in one batch (press, motion, release).
    void drag_between(std::int64_t from_kind, std::pair<std::int64_t, std::int64_t> from,
                      std::int64_t to_kind, std::pair<std::int64_t, std::int64_t> to) {
        auto press = at(from_kind, from.first, from.second, true);
        auto release = at(to_kind, to.first, to.second, false);
        auto move = release; move.kind = "PointerMoved";
        move.dx = release.x - press.x; move.dy = release.y - press.y;
        batch({press, move, release});
    }
    /// A NEW VIEW FROM THE DEFAULT PANE'S KEYS, placed where the case says.
    std::int64_t new_view(const std::string& pane_key, std::int64_t x, std::int64_t y,
                          std::int64_t w = 86, std::int64_t h = 24) {
        click(info);
        key(input::scan::kN, input::mod::kCtrl);
        const auto kind = kind_of(pane_key);
        REQUIRE_MESSAGE(kind >= 0, "no " << pane_key << " offered: " << shown(info));
        place(info::kInfoPaneRole, pane_key, x, y, w, h);
        return kind;
    }
    /// THE TWO LOWER VIEWS most cases use, side by side under Inventory and Info.
    std::pair<std::int64_t, std::int64_t> two_views() {
        const auto a = new_view("info.2", 2, 30);
        const auto b = new_view("info.3", 92, 30);
        return {a, b};
    }
    /// A COPY OF A LABELLED ENTRY dragged into a view.
    void copy_into(std::int64_t view, const std::string& label) {
        const auto row = where(source, label).first;
        REQUIRE_MESSAGE(row >= 0, "no entry " << label << " in:\n" << shown(source));
        drag_between(source, {row, 2}, view, {3, 2});
    }
    /// A LIVE REFERENCE to a labelled entry, placed into a view: the entry is linked there.
    void link_into(std::int64_t view, const std::string& label) {
        const auto row = where(source, label).first;
        REQUIRE_MESSAGE(row >= 0, "no entry " << label << " in:\n" << shown(source));
        click(source, row);
        key(input::scan::kReturn, input::mod::kCtrl);
        click(view, 3);
    }
    /// EDIT THE FIELD WHOSE ROW SHOWS `label:` in a view.
    void edit_field(std::int64_t view, const std::string& label, const std::string& value) {
        const auto [row, column] = where(view, label + ":");
        REQUIRE_MESSAGE(row >= 0, "no field " << label << " in:\n" << shown(view));
        press_at(view, row, column);
        key(input::scan::kReturn); key(input::scan::kA, input::mod::kCtrl);
        text(value); key(input::scan::kReturn);
    }
    /// THE HOST'S OWN MAKER: physical input, which needs no Loom grant of its own.
    void physical_press(std::int64_t kind, std::int64_t row, std::int64_t column) {
        const auto e = at(kind, row, column, true);
        physical->push_back(input::PointerButton{1, true, e.x, e.y, input::space::kCells, 0});
        physical->push_back(input::PointerButton{1, false, e.x, e.y, input::space::kCells, 0});
        pump_physical();
    }
    void physical_key(std::int64_t scan, std::int64_t mods = input::mod::kNone) {
        physical->push_back(input::KeyPressed{scan, "", mods});
        pump_physical();
    }
    void write(const inv::InventoryReference& ref, std::int64_t revision, std::int64_t value) {
        r.bus.send_to_role(inv::kInventoryRole, loom::Message(loom::to_value(inv::InventoryWrite{ref, revision, pair(value)})));
        r.bus.drain_until_idle();
    }
    std::size_t sends(const std::string& shape) {
        std::size_t n = 0;
        for (const auto& s : sent) if (s == shape) ++n;
        return n;
    }
    /// EVERY SHAPE DELIVERED SINCE `watch()` -- the tap a case reads for "nothing was asked".
    void watch() {
        sent.clear();
        if (tapped) return;
        tapped = true;
        tap = r.bus.add_observer([this](const loom::BusEvent& e) {
            if (e.kind == loom::EventKind::Delivered) sent.push_back(e.schema_name);
        });
    }
    std::vector<std::string> sent;
    loom::ObserverId tap{};
    bool tapped = false;
    bool debug_x = false;
};

/// A TEST-OWNED INVENTORY OFFICE whose answers a case releases in any order -- the one way to
/// make a read or a write slow, silent or late through the real Info pane.
struct ScriptedState { ZEN_SHAPE(ScriptedState, 1); };
class ScriptedInventory : public loom::WeaveBase<ScriptedInventory, ScriptedState,
    loom::Accept<InventoryHandDo, inv::InventoryRead, inv::InventoryWrite, inv::InventoryAdd, inv::InventoryList,
        inv::v2::InventoryList>,
    loom::Emit<inv::InventoryEntry, inv::InventoryListed, inv::v2::InventoryListed, inv::InventoryChanged, loom::Refused>> {
public:
    struct Row { std::int64_t revision = 1; loom::Bytes pair; std::string label; };
    struct Held {
        std::string kind;
        inv::InventoryReference ref;
        std::int64_t revision = 0;
        loom::Bytes pair;
        loom::DeferredAnswer due;
    };
    std::map<std::string, Row> rows;
    std::vector<Held> held;
    std::string hold; ///< an entry whose reads and writes wait for the case; "*" holds all
    loom::WeaveId only{}; ///< when set, only this sender's requests are held (Info, not the Inventory pane)
    std::string refuse_add; ///< when set, an Add is refused by this owner with these words
    std::function<void(loom::Mail&)> next;
    std::size_t reads = 0;

    void on(const InventoryHandDo&, loom::Mail& m) { next(m); }
    void on(const inv::InventoryList&, loom::Mail& m) {
        inv::InventoryListed out;
        for (const auto& [key, row] : rows) {
            const auto item = inv::decode_pair({reinterpret_cast<const char*>(row.pair.data()), row.pair.size()}).item;
            out.entries.push_back({{"scripted", key}, row.revision, row.label, item.schema().name(),
                                   static_cast<std::int64_t>(item.schema().version()), false});
        }
        (void)m.answer(out);
    }
    /// The Inventory pane's listing: the same rows, all at the root, with no folders.
    void on(const inv::v2::InventoryList&, loom::Mail& m) {
        inv::v2::InventoryListed out{"scripted", 0, {}, {}};
        for (const auto& [key, row] : rows) {
            const auto item = inv::decode_pair({reinterpret_cast<const char*>(row.pair.data()), row.pair.size()}).item;
            out.entries.push_back({{"scripted", key}, row.revision, row.label, item.schema().name(),
                                   static_cast<std::int64_t>(item.schema().version()), false, {}});
        }
        (void)m.answer(out);
    }
    void on(const inv::InventoryRead& r, loom::Mail& m) {
        ++reads;
        if (held_for(r.reference.entry, m)) { held.push_back({"read", r.reference, 0, {}, m.defer_answer()}); return; }
        Held now{"read", r.reference, 0, {}, {}};
        reply(now, m, false);
    }
    void on(const inv::InventoryWrite& w, loom::Mail& m) {
        if (held_for(w.reference.entry, m)) { held.push_back({"write", w.reference, w.revision, w.pair, m.defer_answer()}); return; }
        Held now{"write", w.reference, w.revision, w.pair, {}};
        reply(now, m, false);
    }
    void on(const inv::InventoryAdd& a, loom::Mail& m) {
        if (!refuse_add.empty()) { (void)m.answer(loom::Refused{refuse_add}); return; }
        const auto key = "added" + std::to_string(rows.size());
        rows[key] = {1, a.pair, a.label};
        (void)m.answer(inv::InventoryEntry{{"scripted", key}, 1, a.pair});
        m.as_role(inv::kInventoryRole).publish(inv::InventoryChanged{});
    }
    /// ANSWER held[i] NOW, from the delivery `m` -- a deferred answer, as a slow owner gives it.
    void release(std::size_t i, loom::Mail& m) {
        Held h = std::move(held.at(i));
        held.erase(held.begin() + static_cast<std::ptrdiff_t>(i));
        reply(h, m, true);
    }
    void changed(loom::Mail& m) { m.as_role(inv::kInventoryRole).publish(inv::InventoryChanged{}); }

private:
    bool held_for(const std::string& entry, const loom::Mail& m) const {
        if (only.valid() && m.sender() != only) return false;
        return hold == "*" || (!hold.empty() && hold == entry);
    }
    template <class T> void send(Held& h, loom::Mail& m, bool deferred, const T& value) {
        if (deferred) (void)loom::answer_deferred(h.due, m, value);
        else (void)m.answer(value);
    }
    void reply(Held& h, loom::Mail& m, bool deferred) {
        const auto it = rows.find(h.ref.entry);
        if (h.ref.owner != "scripted" || it == rows.end()) {
            send(h, m, deferred, loom::Refused{"this inventory entry is no longer here"}); return;
        }
        if (h.kind == "read") {
            send(h, m, deferred, inv::InventoryEntry{h.ref, it->second.revision, it->second.pair}); return;
        }
        if (it->second.revision != h.revision) {
            send(h, m, deferred, loom::Refused{"the entry changed or its revision is exhausted; fetch a fresh copy"}); return;
        }
        it->second.pair = h.pair; ++it->second.revision;
        send(h, m, deferred, inv::InventoryEntry{h.ref, it->second.revision, it->second.pair});
        m.as_role(inv::kInventoryRole).publish(inv::InventoryChanged{});
    }
};

loom::Bytes record(std::int64_t count) {
    auto shape = loom::SchemaBuilder("story.RuntimeItem", 1).field("count", loom::Kind::Int).build();
    loom::Value value(shape); value.set("count", loom::Cell::integer(count));
    const auto encoded = inv::encode_pair(value, {});
    return {encoded.begin(), encoded.end()};
}

/// AN INVENTORY OFFICE WITH NO DOORS, held for an interval: what Info sends it is still queued,
/// because Info declares those shapes, and Loom refuses it at dispatch as NotAccepted -- the
/// notice a replaced or half-loaded owner produces. It can still say something changed.
class DoorlessInventory : public loom::WeaveBase<DoorlessInventory, ScriptedState,
    loom::Accept<InventoryHandDo>, loom::Emit<inv::InventoryChanged>> {
public:
    void on(const InventoryHandDo&, loom::Mail& m) { m.as_role(inv::kInventoryRole).publish(inv::InventoryChanged{}); }
};

/// THE STORY WITH ITS INVENTORY OFFICE REPLACED BY A SCRIPTED ONE holding two entries.
struct ScriptedViews : Views {
    ScriptedInventory* inventory = nullptr;
    loom::WeaveId id;
    std::unique_ptr<loom::Weave> parked; ///< the scripted owner while a doorless office holds the role
    loom::WeaveId doorless_id;
    static loom::Grant scripted_grant() {
        loom::Grant grant;
        for (const char* shape : {inv::InventoryEntry::zen_name, inv::InventoryListed::zen_name,
                                  inv::InventoryChanged::zen_name, loom::Refused::zen_name})
            grant.allow_to_any(shape, 1);
        grant.allow_to_any(inv::v2::InventoryListed::zen_name, 2);
        return grant;
    }
    explicit ScriptedViews(int permissions = 191) : Views(permissions) {
        REQUIRE(r.kernel.unload_role(inv::kInventoryRole));
        auto owned = std::make_unique<ScriptedInventory>();
        inventory = owned.get();
        inventory->rows["alpha"] = {1, record(1), "Alpha"};
        inventory->rows["beta"] = {1, record(2), "Beta"};
        id = r.bus.register_weave(std::move(owned), scripted_grant(), inv::kInventoryRole);
        inventory->zen_set_self(id);
        act_scripted([](ScriptedInventory& s, loom::Mail& m) { s.changed(m); });
        inventory->only = r.bus.role_holder(info::kInfoPaneRole);
    }
    /// THE OWNER LOSES ITS DOORS: the scripted office comes off the bus with its rows, and a
    /// doorless one holds the role until `doors_back`.
    void doorless() {
        parked = r.bus.unregister_weave(id);
        REQUIRE(parked != nullptr);
        auto owned = std::make_unique<DoorlessInventory>();
        auto* raw = owned.get();
        loom::Grant grant;
        grant.allow_to_any(inv::InventoryChanged::zen_name, 1);
        doorless_id = r.bus.register_weave(std::move(owned), grant, inv::kInventoryRole);
        raw->zen_set_self(doorless_id);
    }
    /// ...and the doorless office says something changed, as the role's holder.
    void doorless_changed() {
        r.bus.send(doorless_id, loom::Message(loom::to_value(InventoryHandDo{})));
        r.bus.drain_until_idle();
    }
    void doors_back() {
        REQUIRE(r.bus.unregister_weave(doorless_id) != nullptr);
        id = r.bus.register_weave(std::move(parked), scripted_grant(), inv::kInventoryRole);
        inventory->zen_set_self(id);
    }
    void act_scripted(std::function<void(ScriptedInventory&, loom::Mail&)> f) {
        inventory->next = [this, f](loom::Mail& m) { f(*inventory, m); };
        r.bus.send(id, loom::Message(loom::to_value(InventoryHandDo{})));
        r.bus.drain_until_idle();
        inventory->next = {};
    }
    void release(std::size_t i) { act_scripted([i](ScriptedInventory& s, loom::Mail& m) { s.release(i, m); }); }
    std::int64_t count(const std::string& key) {
        const auto& p = inventory->rows.at(key).pair;
        return inv::decode_pair({reinterpret_cast<const char*>(p.data()), p.size()}).item.get("count")->as_int();
    }
};

/// A PARTICIPANT THAT IS NOT WORKSHOP AND NOT INVENTORY, for forged answers and continuations.
class Stranger : public loom::WeaveBase<Stranger, InventoryHandState,
    loom::Accept<InventoryHandDo, PaneObservationAnswered>,
    loom::Emit<inv::InventoryEntry, PaneObservationContinued, PaneObservationRequested>> {
public:
    std::function<void(loom::Mail&)> next;
    std::vector<PaneObservationAnswered> answers;
    void on(const InventoryHandDo&, loom::Mail& m) { next(m); }
    void on(const PaneObservationAnswered& a, loom::Mail&) { answers.push_back(a); }
};
struct StrangerRig {
    Stranger* self = nullptr;
    loom::WeaveId id;
    PaneRig& r;
    explicit StrangerRig(PaneRig& rig) : r(rig) {
        auto owned = std::make_unique<Stranger>(); self = owned.get();
        loom::Grant grant;
        grant.allow_to_role(inv::InventoryEntry::zen_name, 1, info::kInfoPaneRole);
        grant.allow_to_role(PaneObservationContinued::zen_name, 1, "zengine.workshop");
        grant.allow_to_role(PaneObservationRequested::zen_name, 1, "zengine.workshop");
        id = r.bus.register_weave(std::move(owned), grant);
        self->zen_set_self(id);
    }
    void act(std::function<void(loom::Mail&)> f) {
        self->next = std::move(f);
        r.bus.send(id, loom::Message(loom::to_value(InventoryHandDo{})));
        r.bus.drain_until_idle();
        self->next = {};
    }
};

} // namespace

TEST_CASE("info views: a new view is its own pane with visible controls, subject and state") {
    Views s;
    const auto second = s.new_view("info.2", 2, 30);
    REQUIRE(s.r.session().panels.has(second));
    const auto text = s.shown(second);
    INFO(text);
    CHECK(text.find("Info 2 | empty") != std::string::npos);
    CHECK(text.find("(Save)") != std::string::npos);
    CHECK(text.find("[New]") != std::string::npos);
    CHECK(text.find("[...]") != std::string::npos);
    s.copy_into(second, "story.RuntimeItem");
    const auto now = s.shown(second);
    CHECK_MESSAGE(now.find("COPY story.RuntimeItem v1") != std::string::npos, now);
    CHECK(now.find("not stored") != std::string::npos);
    CHECK(s.shown(s.info).find("story.RuntimeItem") == std::string::npos);
}

TEST_CASE("info views: two views keep their own subjects and drafts through focus and other panes") {
    Views s;
    s.append(20, "Second");
    const auto [a, b] = s.two_views();
    s.copy_into(a, "story.RuntimeItem");
    s.copy_into(b, "Second");
    s.edit_field(a, "count", "41");
    s.click(s.source, 1);  // another pane takes the keys and the selection
    s.click(s.info);
    CHECK_MESSAGE(s.shows(a, "count: 41"), s.shown(a));
    CHECK(s.shows(a, "UNSAVED"));
    CHECK_MESSAGE(s.shows(b, "count: 20"), s.shown(b));
    CHECK_FALSE(s.shows(b, "UNSAVED"));
    s.edit_field(b, "count", "22");
    CHECK(s.shows(a, "count: 41"));
    CHECK(s.shows(b, "count: 22"));
    CHECK(s.saved_entries().size() == 1); // edits are drafts; nothing was stored
}

TEST_CASE("info views: interleaved answers settle only the view that asked; forged and retired answers settle nothing") {
    ScriptedViews s;
    const auto [a, b] = s.two_views();
    s.link_into(a, "Alpha");
    s.link_into(b, "Beta");
    REQUIRE_MESSAGE(s.shows(a, "LINKED 'Alpha'"), s.shown(a));
    REQUIRE_MESSAGE(s.shows(b, "LINKED 'Beta'"), s.shown(b));
    s.inventory->hold = "*";
    s.edit_field(a, "count", "11"); s.button(a, "Save");
    s.edit_field(b, "count", "22"); s.button(b, "Save");
    REQUIRE(s.inventory->held.size() == 2);
    CHECK(s.shows(a, "saving"));
    // A FORGED ANSWER: ordinary speech from a stranger, shaped like the owner's reply to Alpha.
    StrangerRig stranger(s.r);
    stranger.act([&](loom::Mail& m) {
        m.send_to_role(info::kInfoPaneRole, inv::InventoryEntry{{"scripted", "alpha"}, 9, record(99)});
    });
    CHECK(s.shows(a, "saving"));
    CHECK_FALSE(s.shows(a, "count: 99"));
    // OUT OF ORDER: Beta's answer first, then Alpha's. Each settles its own view only.
    s.release(1);
    CHECK_MESSAGE(s.shows(b, "saved rev 2"), s.shown(b));
    CHECK(s.shows(a, "saving"));
    s.release(0);
    CHECK_MESSAGE(s.shows(a, "saved rev 2"), s.shown(a));
    CHECK(s.count("alpha") == 11);
    CHECK(s.count("beta") == 22);
    // A RETIRED INCARNATION'S ANSWER: a read held for a view that is closed and whose slot is
    // reused by a new view with its own held read.
    s.button(a, "Refresh");
    REQUIRE(s.inventory->held.size() == 1);
    s.button(a, "Close"); s.button(a, "Close");
    CHECK_FALSE(s.r.session().panels.has(a));
    const auto again = s.new_view("info.2", 2, 30);
    CHECK(again == a); // the slot is reused, not minted
    s.link_into(again, "Beta");
    REQUIRE(s.inventory->held.size() == 2);
    s.release(0); // the old incarnation's read: nothing waits for it
    CHECK_FALSE(s.shows(again, "LINKED 'Beta'"));
    s.release(0);
    CHECK_MESSAGE(s.shows(again, "LINKED 'Beta'"), s.shown(again));
}

TEST_CASE("info views: two linked views of one entry: the stale writer keeps its text and Save copy stores it") {
    Views s;
    s.append(5, "Shared");
    const auto [a, b] = s.two_views();
    s.link_into(a, "Shared");
    s.link_into(b, "Shared");
    s.edit_field(a, "count", "6");
    s.edit_field(b, "count", "7");
    s.button(a, "Save");
    CHECK_MESSAGE(s.shows(a, "saved rev 2"), s.shown(a));
    s.button(b, "Save");
    CHECK_MESSAGE(s.shows(b, "Save refused"), s.shown(b));
    CHECK(s.shows(b, "count: 7"));
    CHECK(s.shows(b, "UNSAVED"));
    CHECK(s.saved_entries()[0].item.get("count")->as_int() == 6);
    s.button(b, "Save copy");
    const auto entries = s.saved_entries();
    REQUIRE(entries.size() == 2);
    CHECK(entries[0].item.get("count")->as_int() == 6);
    CHECK(entries[1].item.get("count")->as_int() == 7);
    CHECK_MESSAGE(s.shows(b, "LINKED 'Shared'"), s.shown(b)); // Save copy kept the link
    CHECK(s.shows(b, "UNSAVED"));
    CHECK(s.entry("Shared copy").revision == 1);
}

TEST_CASE("info views: a save answered after newer typing keeps the newer edits unsaved on the new revision") {
    ScriptedViews s;
    const auto [a, b] = s.two_views();
    (void)b;
    s.link_into(a, "Alpha");
    s.inventory->hold = "alpha";
    s.edit_field(a, "count", "42");
    s.button(a, "Save");
    s.edit_field(a, "count", "43");
    REQUIRE(s.inventory->held.size() == 1);
    s.release(0);
    CHECK(s.count("alpha") == 42);
    CHECK_MESSAGE(s.shows(a, "count: 43"), s.shown(a));
    CHECK(s.shows(a, "UNSAVED"));
    CHECK(s.shows(a, "newer edits remain unsaved"));
    s.inventory->hold.clear();
    s.button(a, "Save");
    CHECK(s.count("alpha") == 43);
}

TEST_CASE("info views: a refused refresh keeps the unsaved text dirty, so no later observation replaces it") {
    ScriptedViews s;
    const auto [a, b] = s.two_views();
    (void)b;
    s.link_into(a, "Alpha");
    s.edit_field(a, "count", "61");
    s.inventory->rows.erase("alpha"); // the entry disappears before the maker refreshes
    s.button(a, "Refresh"); s.button(a, "Refresh");
    CHECK_MESSAGE(s.shows(a, "Read refused"), s.shown(a));
    CHECK(s.shows(a, "count: 61"));
    CHECK(s.shows(a, "UNSAVED"));
    CHECK(s.shows(a, "LINK STALE"));
    CHECK(s.shows(a, "(Watch)")); // a stale link has nothing to watch
    s.button(a, "Save copy");
    CHECK_MESSAGE(s.shows(a, "Saved a new entry"), s.shown(a));
    std::string copy;
    for (const auto& [key, row] : s.inventory->rows) if (row.label == "Alpha copy") copy = key;
    REQUIRE_FALSE(copy.empty());
    CHECK(s.count(copy) == 61);
}

TEST_CASE("info views: a watch adopts clean changes, holds newer data while dirty and never advances a dirty base") {
    Views s;
    s.append(5, "Watched");
    const auto [a, b] = s.two_views();
    (void)b;
    s.link_into(a, "Watched");
    s.button(a, "Watch");
    REQUIRE_MESSAGE(s.shows(a, "watch ON"), s.shown(a));
    CHECK(s.r.w->observation_leases() == 1);
    CHECK(s.shows(a, "[Pause]"));
    const auto ref = s.entry("Watched").reference;
    s.write(ref, 1, 8);
    CHECK_MESSAGE(s.shows(a, "~count: 8"), s.shown(a)); // adopted, marked as observed
    CHECK(s.shows(a, "saved rev 2"));
    s.edit_field(a, "count", "50");
    CHECK(s.shows(a, "*count: 50"));
    s.write(ref, 2, 9);
    CHECK_MESSAGE(s.shows(a, "count: 50"), s.shown(a));
    CHECK(s.shows(a, "NEWER rev 3 waiting"));
    CHECK(s.shows(a, "UNSAVED"));
    s.button(a, "Save");
    CHECK_MESSAGE(s.shows(a, "Save refused"), s.shown(a)); // the base never moved under the edit
    CHECK(s.shows(a, "count: 50"));
    CHECK(s.saved_entries()[0].item.get("count")->as_int() == 9);
    s.button(a, "Refresh"); s.button(a, "Refresh"); // explicit acceptance, after confirming discard
    CHECK_MESSAGE(s.shows(a, "count: 9"), s.shown(a));
    CHECK(s.shows(a, "saved rev 3"));
}

TEST_CASE("info views: a silent watch read never queues, never stalls another view, and changes coalesce") {
    ScriptedViews s;
    const auto [a, b] = s.two_views();
    s.link_into(a, "Alpha");
    s.link_into(b, "Beta");
    s.button(a, "Watch");
    REQUIRE_MESSAGE(s.shows(a, "watch ON"), s.shown(a));
    s.inventory->hold = "alpha";
    const auto before = s.inventory->reads;
    for (int i = 0; i < 6; ++i) s.act_scripted([](ScriptedInventory& office, loom::Mail& m) { office.changed(m); });
    CHECK(s.inventory->reads == before + 1); // one read outstanding, the rest remembered once
    CHECK(s.inventory->held.size() == 1);
    // THE OTHER VIEW IS UNTOUCHED BY THE SILENCE: it edits and saves.
    s.edit_field(b, "count", "30");
    s.button(b, "Save");
    CHECK_MESSAGE(s.shows(b, "saved rev 2"), s.shown(b));
    CHECK(s.count("beta") == 30);
    // Releasing the silent read runs exactly one more cycle for everything that happened.
    const auto during = s.inventory->reads;
    REQUIRE(s.inventory->held.size() == 1);
    s.release(0);
    CHECK(s.inventory->reads == during + 1);
    REQUIRE(s.inventory->held.size() == 1);
    s.release(0);
    CHECK(s.inventory->reads == during + 1);
    CHECK(s.inventory->held.empty());
}

TEST_CASE("info views: pause, hide and close end the watch and its lease; nothing is scheduled after") {
    Views s;
    s.append(5, "Watched");
    const auto [a, b] = s.two_views();
    (void)b;
    s.link_into(a, "Watched");
    const auto ref = s.entry("Watched").reference;
    s.button(a, "Watch");
    REQUIRE(s.r.w->observation_leases() == 1);
    s.button(a, "Pause");
    CHECK(s.r.w->observation_leases() == 0);
    s.watch();
    s.write(ref, 1, 6);
    CHECK(s.sends(PaneObservationContinued::zen_name) == 0);
    CHECK_MESSAGE(s.shows(a, "count: 5"), s.shown(a)); // paused: nothing adopted
    // HIDDEN: the desk closes the pane; the watch ends, the draft stays.
    s.button(a, "Watch");
    REQUIRE(s.r.w->observation_leases() == 1);
    s.r.bus.office_send_to_role_as(s.r.bus.role_holder(info::kInfoPaneRole), info::kInfoPaneRole, kWorkshopProvider,
        loom::Message(loom::to_value(PaneCloseRequested{info::kInfoPaneRole, "info.2"})));
    s.r.bus.drain_until_idle();
    CHECK_FALSE(s.r.session().panels.has(a));
    CHECK(s.r.w->observation_leases() == 0);
    s.watch();
    s.write(ref, 2, 7);
    CHECK(s.sends(PaneObservationContinued::zen_name) == 0);
    // CLOSED: the view is retired; its lease is gone and no observation follows.
    s.r.pick({info::kInfoPaneRole, "info.2"});
    s.place(info::kInfoPaneRole, "info.2", 2, 30, 86, 24);
    CHECK_MESSAGE(s.shows(a, "watch off"), s.shown(a));
    CHECK(s.shows(a, "count: 6")); // what the second watch read before the view was hidden
    s.button(a, "Watch");
    REQUIRE(s.r.w->observation_leases() == 1);
    s.button(a, "Close");
    CHECK(s.r.w->observation_leases() == 0);
    s.watch();
    s.write(ref, 3, 8);
    CHECK(s.sends(PaneObservationContinued::zen_name) == 0);
    CHECK(s.sends(inv::InventoryRead::zen_name) == 0);
}

TEST_CASE("info views: a watch needs the actor's own read authority and each observation is judged again") {
    SUBCASE("no read authority") {
        Views s(191 & ~2);
        s.append(5, "Watched");
        const auto [a, b] = s.two_views();
        (void)b;
        // THE HOST'S MAKER LINKS THE ENTRY; the injected actor, lacking read authority, asks to watch.
        s.physical_press(s.source, s.where(s.source, "Watched").first, 2);
        s.physical_key(input::scan::kReturn, input::mod::kCtrl);
        s.physical_press(a, 3, 2);
        REQUIRE_MESSAGE(s.shows(a, "LINKED 'Watched'"), s.shown(a));
        s.button(a, "Watch");
        CHECK_MESSAGE(s.shows(a, "Watch refused"), s.shown(a));
        CHECK(s.shows(a, "no authority"));
        CHECK(s.r.w->observation_leases() == 0);
    }
    SUBCASE("forged continuation and a departed actor") {
        Views s;
        s.append(5, "Watched");
        const auto [a, b] = s.two_views();
        (void)b;
        s.link_into(a, "Watched");
        const auto ref = s.entry("Watched").reference;
        s.button(a, "Watch");
        REQUIRE(s.r.w->observation_leases() == 1);
        StrangerRig stranger(s.r);
        for (std::int64_t lease = 1; lease <= 4; ++lease)
            stranger.act([&](loom::Mail& m) {
                m.send_to_role("zengine.workshop", PaneObservationContinued{"info.2", lease, "anything"}, 900);
            });
        // NOR CAN IT OPEN ONE: it holds no office that offered the pane, and no gesture of its own.
        stranger.act([&](loom::Mail& m) {
            m.send_to_role("zengine.workshop", PaneObservationRequested{"info.2", inv::kInventoryRole,
                inv::InventoryRead::zen_name, 1, 1, "anything"}, 901);
        });
        REQUIRE(stranger.self->answers.size() == 5);
        for (const auto& answer : stranger.self->answers) CHECK_FALSE(answer.allowed);
        CHECK(s.r.w->observation_leases() == 1); // a stranger forgets nothing on the holder's behalf
        // The actor who started the watch leaves: the next observation is refused and forgotten.
        s.r.bus.unregister_weave(s.hand_id).reset();
        s.hand = nullptr;
        s.write(ref, 1, 6);
        CHECK(s.r.w->observation_leases() == 0);
        CHECK_MESSAGE(s.shows(a, "Watch ended"), s.shown(a));
        CHECK(s.shows(a, "count: 5"));
    }
}

namespace {

/// A WEAVE WITH EXPOSED STRUCTURE whose role a capture records and a sample asks again.
struct SourceState {
    std::int64_t level = 3;
    std::string mood = "calm";
    ZEN_SHAPE(SourceState, 1, ZEN_FIELD(level), ZEN_FIELD(mood));
};
struct Nudge { ZEN_SHAPE(Nudge, 1); };
class StructureSource : public loom::WeaveBase<StructureSource, SourceState, loom::Accept<Nudge>, loom::Emit<loom::Ack>> {
public:
    void on(const Nudge&, loom::Mail& m) { (void)m.answer(loom::Ack{}); }
};
struct OtherState {
    std::string label = "replacement";
    bool fresh = true;
    ZEN_SHAPE(OtherState, 1, ZEN_FIELD(label), ZEN_FIELD(fresh));
};
class OtherSource : public loom::WeaveBase<OtherSource, OtherState, loom::Accept<Nudge>, loom::Emit<loom::Ack>> {
public:
    void on(const Nudge&, loom::Mail& m) { (void)m.answer(loom::Ack{}); }
};
/// A PARTICIPANT THAT TAKES A DESCRIBE REQUEST AND NEVER ANSWERS IT.
struct SilentState { ZEN_SHAPE(SilentState, 1); };
class SilentSource final : public loom::Weave {
public:
    std::vector<std::shared_ptr<const loom::Schema>> accepted_schemas() const override {
        return {loom::schema_of<loom::PokeDescribe>()};
    }
    void handle(const loom::Message&, loom::Bus&) override { ++asked; }
    loom::Value snapshot() const override { return loom::to_value(SilentState{}); }
    loom::Value policy() const override {
        loom::Value v(loom::lifecycle_policy_schema());
        v.set("max_reloads", loom::Cell::integer(0));
        v.set("revive_from_last_good", loom::Cell::boolean(false));
        return v;
    }
    void revive(const loom::Value&) override {}
    std::size_t asked = 0;
};
/// A SOURCE THAT ANSWERS ITS DESCRIBE REQUEST WHEN THE CASE SAYS: a slow provider. Raw, because
/// WeaveBase leaves PokeDescribe to the substrate; the answer is Loom's deferred one, which only
/// this participant can spend, during a later delivery of its own (a root send cannot).
class DeferredSource final : public loom::Weave {
public:
    std::vector<std::shared_ptr<const loom::Schema>> accepted_schemas() const override {
        return {loom::schema_of<loom::PokeDescribe>(), loom::schema_of<Nudge>()};
    }
    void handle(const loom::Message& m, loom::Bus& bus) override {
        if (m.payload.schema().name() == loom::PokeDescribe::zen_name) { due = bus.make_deferred_answer(); ++asked; return; }
        if (answer) (void)bus.spend_deferred(due, loom::Message(loom::to_value(*answer)));
        answer.reset();
    }
    loom::Value snapshot() const override { return loom::to_value(SilentState{}); }
    loom::Value policy() const override {
        loom::Value v(loom::lifecycle_policy_schema());
        v.set("max_reloads", loom::Cell::integer(0));
        v.set("revive_from_last_good", loom::Cell::boolean(false));
        return v;
    }
    void revive(const loom::Value&) override {}
    loom::DeferredAnswer due;
    std::size_t asked = 0;
    std::optional<loom::PokeStructure> answer;
};
struct DeferredRig {
    DeferredSource* self = nullptr;
    loom::WeaveId id;
    PaneRig& r;
    DeferredRig(PaneRig& rig, const std::string& role) : r(rig) {
        auto owned = std::make_unique<DeferredSource>();
        self = owned.get();
        loom::Grant grant;
        loom::allow_poke_answers(grant);
        id = r.bus.register_weave(std::move(owned), grant, role);
    }
    /// SPEND THE HELD ANSWER with this structure, from the source's own delivery.
    void release(loom::PokeStructure structure) {
        self->answer = std::move(structure);
        r.bus.send(id, loom::Message(loom::to_value(Nudge{})));
        r.bus.drain_until_idle();
    }
};

template <class W>
loom::WeaveId mount(PaneRig& r, const std::string& role) {
    auto owned = std::make_unique<W>();
    auto* raw = owned.get();
    auto grant = loom::emit_default_grant(*owned);
    loom::allow_poke_answers(grant);
    const auto id = r.bus.register_weave(std::move(owned), grant, role);
    raw->zen_set_self(id);
    return id;
}

/// CAPTURE `role`'s structure into Inventory under `label`, as the capture adapter records it.
void capture(Views& s, const std::string& role, const std::string& label) {
    s.act([&](loom::Mail& m) { m.send_to_role(inv::kInventoryRole, inv::InventoryCaptureAdd{role, label}); });
    REQUIRE_MESSAGE(s.where(s.source, label).first >= 0, s.shown(s.source));
}

loom::Bytes stored_bytes(Views& s, const std::string& label) {
    const auto e = s.entry(label);
    s.hand->entries.clear();
    s.act([&](loom::Mail& m) { m.send_to_role(inv::kInventoryRole, inv::InventoryRead{e.reference}); });
    REQUIRE(!s.hand->entries.empty());
    return s.hand->entries.back().pair;
}

/// A NESTED RECORD whose fields exercise every transfer meaning: a nested message, an empty list,
/// two list element kinds, and an optional scalar.
std::shared_ptr<const loom::Schema> inner_schema() {
    static const auto s = loom::SchemaBuilder("story.Inner", 1).field("a", loom::Kind::Int)
        .field("b", loom::Kind::Text, false).build();
    return s;
}
std::shared_ptr<const loom::Schema> record_schema() {
    static const auto s = loom::SchemaBuilder("story.Record", 1).field("name", loom::Kind::Text)
        .add({"inner", loom::type_message(inner_schema()), true})
        .list("tags", loom::type_of(loom::Kind::Text), false)
        .list("numbers", loom::type_of(loom::Kind::Int), false)
        .field("extra", loom::Kind::Int, false).build();
    return s;
}
void add_value(Views& s, const loom::Value& value, const std::string& label, std::vector<loom::Value> meta = {}) {
    const auto encoded = inv::encode_pair(value, meta);
    s.r.bus.send_to_role(inv::kInventoryRole, loom::Message(loom::to_value(inv::InventoryAdd{
        loom::Bytes(encoded.begin(), encoded.end()), label})));
    s.r.bus.drain_until_idle();
}
void add_records(Views& s) {
    loom::Value inner(inner_schema());
    inner.set("a", loom::Cell::integer(5)); inner.set("b", loom::Cell::text("x"));
    loom::Value source(record_schema());
    source.set("name", loom::Cell::text("src"));
    source.set("inner", loom::Cell::message(inner));
    source.set("tags", loom::Cell::list({}));
    source.set("numbers", loom::Cell::list({loom::Cell::integer(1), loom::Cell::integer(2)}));
    source.set("extra", loom::Cell::integer(9));
    add_value(s, source, "Source record");
    // A PRESET WHOSE NESTED MESSAGE IS PARTIAL: `inner.a` is required and absent.
    zengine::message_draft::Draft partial(record_schema());
    partial.set_text({"name"}, "half");
    partial.create_message({"inner"});
    partial.set_text({"inner", "b"}, "y");
    add_value(s, zengine::message_draft::store_draft("Half record", partial), "Half record");
    // THE DESTINATION: a preset with only its name.
    zengine::message_draft::Draft target(record_schema());
    target.set_text({"name"}, "dst");
    add_value(s, zengine::message_draft::store_draft("Target record", target), "Target record");
}

} // namespace

TEST_CASE("info views: Sample source records a fresh observation, says who answered and changes nothing stored") {
    Views s(191 | 1024);
    const auto source = mount<StructureSource>(s.r, "story.source");
    capture(s, "story.source", "Sampled");
    const auto before = stored_bytes(s, "Sampled");
    const auto [a, b] = s.two_views();
    (void)b;
    s.copy_into(a, "Sampled");
    REQUIRE_MESSAGE(s.shows(a, "[Sample]"), s.shown(a));
    s.watch();
    s.button(a, "Sample");
    CHECK_MESSAGE(s.shows(a, "UNSAVED SAMPLE"), s.shown(a));
    CHECK(s.shows(a, "#" + std::to_string(source.value) + " answered"));
    CHECK(s.sends(loom::PokeDescribe::zen_name) == 1);
    CHECK(s.sends(inv::InventoryCaptureAdd::zen_name) == 0); // the stored request is never replayed
    CHECK(stored_bytes(s, "Sampled") == before);
    s.button(a, "Save copy");
    const auto entries = s.saved_entries();
    REQUIRE(entries.size() == 2);
    const auto ctx = zengine::inventory::recorded_structure_observation(entries.back().metadata);
    REQUIRE(ctx.has_value());
    CHECK(ctx->answered_by == std::to_string(source.value));
    CHECK(stored_bytes(s, "Sampled") == before);
}

TEST_CASE("info views: a replaced, absent, silent, unsupported or unauthorized source each says so") {
    SUBCASE("replaced and then absent") {
        Views s(191 | 1024);
        const auto first = mount<StructureSource>(s.r, "story.source");
        capture(s, "story.source", "Sampled");
        const auto [a, b] = s.two_views();
        (void)b;
        s.copy_into(a, "Sampled");
        REQUIRE(s.r.bus.unregister_weave(first));
        const auto second = mount<OtherSource>(s.r, "story.source");
        s.button(a, "Sample");
        CHECK_MESSAGE(s.shows(a, "Different incarnation: #" + std::to_string(second.value)), s.shown(a));
        CHECK(s.shows(a, "~state_schema: OtherState"));
        REQUIRE(s.r.bus.unregister_weave(second));
        s.button(a, "Sample");
        CHECK_MESSAGE(s.shows(a, "is unavailable"), s.shown(a));
        CHECK(s.shows(a, "source unavailable"));
        CHECK(s.shows(a, "state_schema: OtherState")); // the last good sample stays, marked stale
    }
    SUBCASE("silent: pending in one view, the other view works and closes nothing of it") {
        Views s(191 | 1024);
        const auto first = mount<StructureSource>(s.r, "story.source");
        capture(s, "story.source", "Sampled");
        s.append(4, "Other");
        const auto [a, b] = s.two_views();
        s.copy_into(a, "Sampled");
        s.copy_into(b, "Other");
        REQUIRE(s.r.bus.unregister_weave(first));
        auto silent = std::make_unique<SilentSource>();
        auto* quiet = silent.get();
        loom::Grant none;
        (void)s.r.bus.register_weave(std::move(silent), none, "story.source");
        s.button(a, "Sample");
        CHECK(quiet->asked == 1);
        CHECK_MESSAGE(s.shows(a, "sampling story.source since"), s.shown(a));
        CHECK(s.shows(a, "(Sample)"));
        s.edit_field(b, "count", "44");
        s.button(b, "Save");
        CHECK_MESSAGE(s.shows(b, "LINKED"), s.shown(b));
        s.button(a, "Close");
        CHECK(s.shows(a, "Pending work"));
        s.button(a, "Close");
        CHECK_FALSE(s.r.session().panels.has(a));
        CHECK(quiet->asked == 1);
    }
    SUBCASE("unsupported provenance") {
        Views s(191 | 1024);
        const auto [a, b] = s.two_views();
        (void)b;
        s.copy_into(a, "story.RuntimeItem");
        CHECK_MESSAGE(s.shows(a, "(Sample)"), s.shown(a));
        s.watch();
        s.button(a, "Rename"); s.key(input::scan::kEscape);
        s.key(input::scan::kE, input::mod::kCtrl);
        CHECK_MESSAGE(s.shows(a, "no supported source is recorded"), s.shown(a));
        CHECK(s.sends(loom::PokeDescribe::zen_name) == 0);
    }
    SUBCASE("withheld observation permission") {
        Views s(191);
        (void)mount<StructureSource>(s.r, "story.source");
        capture(s, "story.source", "Sampled");
        const auto [a, b] = s.two_views();
        (void)b;
        s.copy_into(a, "Sampled");
        s.watch();
        s.button(a, "Sample");
        CHECK_MESSAGE(s.shows(a, "Sample refused"), s.shown(a));
        CHECK(s.shows(a, "no authority"));
        CHECK(s.sends(loom::PokeDescribe::zen_name) == 0);
    }
}

TEST_CASE("info views: a field dragged between views fills a compatible field and keeps everything else") {
    Views s(255);
    add_records(s);
    const auto [a, b] = s.two_views();
    s.copy_into(a, "Source record");
    s.link_into(b, "Target record");
    REQUIRE_MESSAGE(s.shows(b, "PRESET LINKED"), s.shown(b));
    REQUIRE(s.shows(b, "inner: absent (required)"));
    auto field = [&](std::int64_t view, const std::string& label) { return s.field_at(view, label); };
    // A NESTED MESSAGE: its whole branch, typed, into the absent required field.
    s.drag_between(a, field(a, "inner"), b, field(b, "inner"));
    CHECK_MESSAGE(s.shows(b, "*inner.a: 5"), s.shown(b));
    CHECK(s.shows(b, "*inner.b: x"));
    CHECK(s.shows(b, "name: dst")); // unrelated values stay
    CHECK(s.shows(b, "extra: absent"));
    // AN EMPTY LIST IS PRESENT AND EMPTY, not absent.
    s.drag_between(a, field(a, "tags"), b, field(b, "tags"));
    CHECK_MESSAGE(s.shows(b, "*tags: 0 items"), s.shown(b));
    const auto before = s.shown(b);
    // MISMATCHES REFUSE WITHOUT MUTATION: an empty Text list is not an Int list; Text is not Int.
    s.drag_between(a, field(a, "tags"), b, field(b, "numbers"));
    CHECK_MESSAGE(s.shows(b, "Field drop refused"), s.shown(b));
    s.drag_between(a, field(a, "name"), b, field(b, "extra"));
    CHECK(s.shows(b, "Field drop refused"));
    CHECK(s.shows(b, "extra: absent"));
    CHECK(s.shows(b, "numbers: absent"));
    // A STALE PICTURE REFUSES: the destination moved since the hand aimed.
    const auto picture = s.r.session().panels.external_pane(b)->picture;
    zengine::message_draft::Draft named(record_schema());
    named.set_text({"name"}, "forged");
    const auto encoded = inv::encode_pair(zengine::message_draft::grab_field(named, {"name"}), {});
    s.r.bus.office_send_to_role_as(s.r.bus.role_holder(kWorkshopProvider), kWorkshopProvider, info::kInfoPaneRole,
        loom::Message(loom::to_value(PaneValueDrop{"info.3", loom::Bytes(encoded.begin(), encoded.end()),
            field(b, "name").first, 3, picture - 1})));
    s.r.bus.drain_until_idle();
    CHECK_MESSAGE(s.shows(b, "nothing was filled"), s.shown(b));
    CHECK(s.shows(b, "name: dst"));
    // A PARTIAL NESTED VALUE KEEPS ITS ABSENCE: a preset's `inner` with `a` unset.
    s.button(a, "Close"); // a clean copy closes at once
    const auto c = s.new_view("info.2", 2, 30);
    s.link_into(c, "Half record");
    s.drag_between(c, field(c, "inner"), b, field(b, "inner"));
    CHECK_MESSAGE(s.shows(b, "inner.a: absent (required)"), s.shown(b));
    CHECK(s.shows(b, "*inner.b: y"));
    // SAVED THROUGH THE ENTRY'S OWNER: the stored preset holds exactly the composed draft.
    s.button(b, "Save");
    const auto saved = zengine::message_draft::read_draft(s.saved_entries()[2].item).draft;
    CHECK(saved.get({"name"})->as_text() == "dst");
    CHECK(saved.get({"inner", "a"}) == nullptr);
    CHECK(saved.get({"inner", "b"})->as_text() == "y");
    CHECK(saved.get({"tags"})->as_list().empty());
    CHECK(saved.get({"numbers"}) == nullptr);
    CHECK(saved.get({"extra"}) == nullptr);
}

TEST_CASE("info views: a whole value never replaces unsaved work, and metadata is never a drop target") {
    Views s(255);
    s.append(20, "Second");
    const auto [a, b] = s.two_views();
    s.copy_into(a, "story.RuntimeItem");
    s.edit_field(a, "count", "77");
    s.copy_into(a, "Second");
    CHECK_MESSAGE(s.shows(a, "whole value would replace this draft"), s.shown(a));
    CHECK(s.shows(a, "count: 77"));
    // A field onto a read-only metadata row is refused.
    s.copy_into(b, "story.RuntimeItem");
    s.drag_between(a, s.field_at(a, "count"), b, s.field_at(b, "meta[0].source"));
    CHECK_MESSAGE(s.shows(b, "metadata is read-only"), s.shown(b));
    CHECK(s.shows(b, "meta[0].source: capture observation"));
}

TEST_CASE("info views: new, fork, save copy and close have distinct custody effects") {
    Views s;
    s.append(5, "Kept");
    const auto a = s.new_view("info.2", 2, 30);
    s.link_into(a, "Kept");
    s.edit_field(a, "count", "6");
    const auto entries = s.saved_entries().size();
    s.button(a, "Fork");
    const auto fork = s.kind_of("info.3");
    REQUIRE(fork >= 0);
    s.place(info::kInfoPaneRole, "info.3", 92, 30, 86, 24);
    CHECK_MESSAGE(s.shows(fork, "count: 6"), s.shown(fork));
    CHECK(s.shows(fork, "LINKED 'Kept'"));
    CHECK(s.shows(fork, "watch off"));
    CHECK(s.saved_entries().size() == entries); // a fork stores nothing
    s.button(a, "Save copy");
    CHECK(s.saved_entries().size() == entries + 1);
    CHECK(s.shows(a, "LINKED 'Kept'"));
    s.button(fork, "Close");
    CHECK(s.shows(fork, "Unsaved edits"));
    s.button(fork, "Close");
    CHECK_FALSE(s.r.session().panels.has(fork));
    CHECK(s.saved_entries().size() == entries + 1); // closing deletes nothing
    CHECK(s.entry("Kept").revision == 1);
}

TEST_CASE("info views: repeated create and close stays within four offers and leaves no pending work") {
    Views s;
    for (int cycle = 0; cycle < 8; ++cycle) {
        std::vector<std::int64_t> made;
        std::int64_t x = 2;
        for (const char* key : {"info.2", "info.3", "info.4"}) { made.push_back(s.new_view(key, x, 30, 58, 24)); x += 59; }
        s.click(s.info);
        s.key(input::scan::kN, input::mod::kCtrl);
        CHECK_MESSAGE(s.shows(s.info, "All four Info views are in use"), s.shown(s.info));
        for (const auto kind : made) {
            s.button(kind, "Close");
            CHECK_FALSE(s.r.session().panels.has(kind));
        }
    }
    std::size_t offered = 0;
    for (const auto& row : s.r.session().panels.runtime.entries) if (row.provider == info::kInfoPaneRole) ++offered;
    CHECK(offered == 4);
    CHECK(s.r.w->observation_leases() == 0);
}

TEST_CASE("info views: a stale or clipped control never acts, and a press names its own picture") {
    Views s;
    const auto [a, b] = s.two_views();
    (void)b;
    s.copy_into(a, "story.RuntimeItem");
    const auto entries = s.saved_entries().size();
    const auto at = s.where(a, "[Save copy]");
    const auto picture = s.r.session().panels.external_pane(a)->picture;
    // A PRESS STAMPED WITH AN OLDER PICTURE is refused, and the copy is not saved.
    s.r.bus.office_send_to_role_as(s.r.bus.role_holder(kWorkshopProvider), kWorkshopProvider, info::kInfoPaneRole,
        loom::Message(loom::to_value(v3::PanePressed{"info.2", at.first, at.second + 2, true, picture - 1})));
    s.r.bus.drain_until_idle();
    CHECK_MESSAGE(s.shows(a, "This view changed -- press again"), s.shown(a));
    CHECK(s.saved_entries().size() == entries);
    // A NARROW ROOM DRAWS NO PARTIAL CONTROL: every bracket it paints is closed.
    s.place(info::kInfoPaneRole, "info.2", 2, 30, 24, 14);
    for (const auto& row : pane_rows(s.r, a)) {
        if (row.find('[') == std::string::npos && row.find('(') == std::string::npos) continue;
        std::int64_t depth = 0;
        for (const char c : row) { if (c == '[' || c == '(') ++depth; if (c == ']' || c == ')') --depth; }
        CHECK_MESSAGE(depth == 0, row);
    }
    CHECK_MESSAGE(s.shows(a, "[...]"), s.shown(a)); // everything stays one press away
    s.button(a, "...");
    CHECK(s.r.session().presented.open);
}

TEST_CASE("pane point: a point names the cell a control is painted on, and a moved picture refuses") {
    Views s(191 | 2048);
    const auto [a, b] = s.two_views();
    (void)b;
    s.copy_into(a, "story.RuntimeItem");
    const auto entries = s.saved_entries().size();
    const auto at = s.where(a, "[Save copy]");
    const auto picture = s.r.session().panels.external_pane(a)->stamp.aimed;
    s.act([&](loom::Mail& m) {
        m.send_to_role("zengine.workshop", PanePointRequested{info::kInfoPaneRole, "info.2", picture, at.first, at.second + 2});
    });
    REQUIRE(s.hand->points.size() == 1);
    const auto point = s.hand->points.back();
    CHECK(point.space == input::space::kCells);
    input::InjectedEvent e; e.kind = "PointerButton"; e.button = 1; e.space = point.space; e.x = point.x; e.y = point.y;
    e.pressed = true; s.event(e); e.pressed = false; s.event(e);
    CHECK_MESSAGE(s.saved_entries().size() == entries + 1, s.shown(a));
    s.hand->expect_refusal = true;
    const auto now = s.r.session().panels.external_pane(a)->stamp.aimed;
    s.act([&](loom::Mail& m) {
        m.send_to_role("zengine.workshop", PanePointRequested{info::kInfoPaneRole, "info.2", now + 1, at.first, at.second + 2});
    });
    REQUIRE(s.hand->refusals.size() == 1);
    CHECK(s.hand->refusals.back().find("picture moved") != std::string::npos);
}

TEST_CASE("info views: a toolbox restore makes a linked view's link stale and keeps its draft") {
    TempDir files("info-views-restore");
    const auto path = files.file("views.toolbox");
    Views s(191 | 512, true);
    const auto [a, b] = s.two_views();
    (void)b;
    s.link_into(a, "story.RuntimeItem");
    REQUIRE(s.shows(a, "LINKED"));
    s.act([&](loom::Mail& m) { m.send_to_role(slots::kRole, slots::InventoryToolboxSave{path}); });
    s.act([&](loom::Mail& m) { m.send_to_role(slots::kRole, slots::InventoryToolboxRestore{path, true}); });
    REQUIRE(s.hand->toolboxes.size() == 2);
    s.edit_field(a, "count", "70");
    s.button(a, "Save");
    CHECK_MESSAGE(s.shows(a, "no longer here"), s.shown(a));
    CHECK(s.shows(a, "LINK STALE"));
    CHECK(s.shows(a, "count: 70"));
    s.button(a, "Save copy");
    CHECK(s.saved_entries().back().item.get("count")->as_int() == 70);
}

TEST_CASE("info views: an incomplete preset is finished from a sample in another view and runs through Compose") {
    Views s(255 | 1024, true);
    (void)mount<StructureSource>(s.r, "story.source");
    capture(s, "story.source", "Workbench sample");
    zengine::message_draft::Draft preset(loom::schema_of<inv::InventoryCaptureAdd>());
    preset.set_text({"label"}, "Workbench result");
    add_value(s, zengine::message_draft::store_draft("Capture preset", preset), "Capture preset");
    const auto [a, b] = s.two_views();
    s.copy_into(a, "Workbench sample");
    s.link_into(b, "Capture preset");
    REQUIRE_MESSAGE(s.shows(b, "target_role: absent (required)"), s.shown(b));
    // CLOSE AND REOPEN: the incomplete preset is stored data, not view state.
    s.button(b, "Close");
    CHECK_FALSE(s.r.session().panels.has(b));
    const auto again = s.new_view("info.3", 92, 30);
    s.link_into(again, "Capture preset");
    REQUIRE_MESSAGE(s.shows(again, "target_role: absent (required)"), s.shown(again));
    s.drag_between(a, s.field_at(a, "meta[0].requested_role"), again, s.field_at(again, "target_role"));
    CHECK_MESSAGE(s.shows(again, "*target_role: story.source"), s.shown(again));
    s.button(again, "Save");
    // EXPLICIT USE: Compose submits the finished preset under the actor's own authority.
    auto& r = s.r;
    r.pick({info::kInfoPaneRole, info::kInfoPane}); // Compose takes the default Info pane's corner
    r.pick(composer_ref());
    const auto compose = r.session().panels.runtime.find(kComposerOffice, "compose")->kind;
    s.place(kComposerOffice, "compose", 85, 4, 80, 24);
    auto selector = std::make_unique<InventoryHand>(); auto* raw = selector.get();
    loom::Grant grant; grant.allow_to_any(intro::LoadedSelected::zen_name, 1);
    const auto id = r.bus.register_weave(std::move(selector), grant, kIntroOffice); raw->zen_set_self(id);
    raw->next = [](loom::Mail& m) { m.as_role(kIntroOffice).publish(
        intro::LoadedSelected{"loaded", "zengine-inventory", inv::kInventoryRole}); };
    r.bus.send(id, loom::Message(loom::to_value(InventoryHandDo{}))); r.bus.drain_until_idle();
    const auto row = s.where(s.source, "Capture preset").first;
    s.drag_between(s.source, {row, 2}, compose, {0, 2});
    REQUIRE_MESSAGE(s.shown(compose).find("Copied data into form") != std::string::npos, s.shown(compose));
    const auto before = s.saved_entries().size();
    s.key(input::scan::kReturn, input::mod::kCtrl);
    CHECK_MESSAGE(s.saved_entries().size() == before + 1, s.shown(compose));
    CHECK(s.entry("Workbench result").revision == 1);
}

// ---- A DRAFT'S TRANSITIONS: an answer can answer its request and still not be safe to apply ----
//
// Refresh, Link and Sample replace a draft the maker already agreed to replace, so while one waits
// the draft is frozen; a save leaves it editable and its answer keeps newer edits. Case names
// avoid commas: doctest's `-tc` splits on them.

TEST_CASE("info views: a view waiting for its sample keeps its draft frozen and edits the sample once it arrives") {
    Views s(255 | 1024);
    const auto first = mount<StructureSource>(s.r, "story.source");
    capture(s, "story.source", "Sampled");
    const auto [a, b] = s.two_views();
    s.copy_into(a, "Sampled");
    s.copy_into(b, "Sampled");
    s.edit_field(b, "state_schema", "from_b"); // a typed value to drag, unlike a's
    REQUIRE_MESSAGE(s.shows(b, "*state_schema: from_b"), s.shown(b));
    // THE SOURCE IS SLOW NOW: the next PokeDescribe is taken and its answer held.
    REQUIRE(s.r.bus.unregister_weave(first));
    DeferredRig source(s.r, "story.source");
    s.button(a, "Sample");
    REQUIRE(source.self->asked == 1);
    REQUIRE_MESSAGE(s.shows(a, "sampling story.source since"), s.shown(a));
    SUBCASE("an accepted keyboard edit is refused visibly") {
        s.edit_field(a, "state_schema", "my_local_edit");
        CHECK_MESSAGE(s.shows(a, "Edit field unavailable: wait"), s.shown(a));
    }
    SUBCASE("no edit opens so no unaccepted text waits under the answer") {
        const auto [row, column] = s.where(a, "state_schema:");
        s.press_at(a, row, column);
        s.key(input::scan::kReturn);
        CHECK_MESSAGE(!s.shows(a, "Enter keeps it"), s.shown(a));
        s.text("typed_not_accepted");
    }
    SUBCASE("a typed field drop is refused") {
        s.drag_between(b, s.field_at(b, "state_schema"), a, s.field_at(a, "state_schema"));
        CHECK_MESSAGE(s.shows(a, "Field drop refused, nothing changed"), s.shown(a));
    }
    CHECK_MESSAGE(s.shows(a, "state_schema: SourceState"), s.shown(a)); // the draft as it was
    CHECK_FALSE(s.shows(a, "*state_schema"));
    CHECK_FALSE(s.shows(a, "UNSAVED"));
    source.release(loom::PokeStructure{"fresh_source", 2, {}});
    CHECK_MESSAGE(s.shows(a, "state_schema: fresh_source"), s.shown(a));
    CHECK_FALSE(s.shows(a, "*state_schema")); // no mark claims an edit the value does not hold
    CHECK(s.shows(a, "UNSAVED SAMPLE"));
    CHECK_FALSE(s.shows(a, "typed_not_accepted"));
    // SETTLED: the sample is the draft, and editing it works.
    s.edit_field(a, "state_schema", "my_local_edit");
    CHECK_MESSAGE(s.shows(a, "*state_schema: my_local_edit"), s.shown(a));
    CHECK(s.shows(a, "UNSAVED 1 edit(s)"));
}

TEST_CASE("info views: a view opening another entry keeps the value it shows frozen until that entry arrives") {
    ScriptedViews s(255);
    const auto [a, b] = s.two_views();
    s.link_into(a, "Alpha");
    s.link_into(b, "Beta");
    s.edit_field(b, "count", "5"); // a typed value to drag, unlike either entry
    REQUIRE_MESSAGE(s.shows(a, "LINKED 'Alpha'"), s.shown(a));
    s.inventory->hold = "beta";
    s.link_into(a, "Beta"); // the read of Beta waits
    REQUIRE(s.inventory->held.size() == 1);
    CHECK_MESSAGE(s.shows(a, "opening 'Beta'"), s.shown(a));
    SUBCASE("an accepted keyboard edit is refused visibly") {
        s.edit_field(a, "count", "77");
        CHECK_MESSAGE(s.shows(a, "Edit field unavailable: wait"), s.shown(a));
    }
    SUBCASE("no edit opens") {
        const auto [row, column] = s.where(a, "count:");
        s.press_at(a, row, column);
        s.key(input::scan::kReturn);
        CHECK_MESSAGE(!s.shows(a, "Enter keeps it"), s.shown(a));
        s.text("88");
    }
    SUBCASE("a typed field drop is refused") {
        s.drag_between(b, s.field_at(b, "count"), a, s.field_at(a, "count"));
        CHECK_MESSAGE(s.shows(a, "Field drop refused, nothing changed"), s.shown(a));
    }
    CHECK_MESSAGE(s.shows(a, "LINKED 'Alpha'"), s.shown(a)); // still what it shows
    CHECK(s.shows(a, "count: 1"));
    CHECK_FALSE(s.shows(a, "UNSAVED"));
    s.release(0);
    CHECK_MESSAGE(s.shows(a, "LINKED 'Beta'"), s.shown(a));
    CHECK(s.shows(a, "count: 2"));
    CHECK_FALSE(s.shows(a, "*count"));
    CHECK(s.shows(a, "saved rev 1"));
    s.inventory->hold.clear();
    s.edit_field(a, "count", "77");
    CHECK_MESSAGE(s.shows(a, "*count: 77"), s.shown(a));
    s.button(a, "Save");
    CHECK(s.count("beta") == 77);
    CHECK(s.count("alpha") == 1);
}

TEST_CASE("info views: a view reading its entry again keeps its draft frozen and takes the read when it answers") {
    ScriptedViews s;
    const auto [a, b] = s.two_views();
    (void)b;
    s.link_into(a, "Alpha");
    s.edit_field(a, "count", "61");
    s.inventory->hold = "alpha";
    s.button(a, "Refresh"); s.button(a, "Refresh"); // the discard is confirmed; the read waits
    REQUIRE(s.inventory->held.size() == 1);
    s.edit_field(a, "count", "62");
    CHECK_MESSAGE(s.shows(a, "Edit field unavailable: wait"), s.shown(a));
    CHECK(s.shows(a, "count: 61")); // the confirmed draft stands until the read answers
    CHECK(s.shows(a, "UNSAVED"));
    s.inventory->rows["alpha"] = {2, record(9), "Alpha"}; // another writer
    s.release(0);
    CHECK_MESSAGE(s.shows(a, "count: 9"), s.shown(a));
    CHECK(s.shows(a, "saved rev 2"));
    CHECK_FALSE(s.shows(a, "UNSAVED"));
    s.inventory->hold.clear();
    s.edit_field(a, "count", "63");
    CHECK_MESSAGE(s.shows(a, "*count: 63"), s.shown(a));
}

TEST_CASE("info views: an edit opened while a save waits stays open when the save answers") {
    ScriptedViews s;
    const auto [a, b] = s.two_views();
    (void)b;
    s.link_into(a, "Alpha");
    s.inventory->hold = "alpha";
    s.edit_field(a, "count", "42");
    s.button(a, "Save");
    REQUIRE(s.inventory->held.size() == 1);
    const auto [row, column] = s.where(a, "count:");
    s.press_at(a, row, column);
    s.key(input::scan::kReturn);
    REQUIRE_MESSAGE(s.shows(a, "Enter keeps it"), s.shown(a));
    s.release(0);
    CHECK(s.count("alpha") == 42);
    CHECK_MESSAGE(s.shows(a, "Enter keeps it"), s.shown(a)); // the answer closed nothing
    s.key(input::scan::kA, input::mod::kCtrl);
    s.text("43");
    s.key(input::scan::kReturn);
    CHECK_MESSAGE(s.shows(a, "*count: 43"), s.shown(a));
    s.inventory->hold.clear();
    s.button(a, "Save");
    CHECK(s.count("alpha") == 43);
}

TEST_CASE("info views: Stop waiting leaves a silent link or sample with the draft as it was") {
    SUBCASE("the default view has no Close and opens an entry that never answers") {
        ScriptedViews s;
        s.link_into(s.info, "Alpha");
        REQUIRE_MESSAGE(s.shows(s.info, "LINKED 'Alpha'"), s.shown(s.info));
        s.inventory->hold = "beta";
        s.link_into(s.info, "Beta");
        REQUIRE(s.inventory->held.size() == 1);
        CHECK_MESSAGE(s.shows(s.info, "[Stop]"), s.shown(s.info));
        s.key(input::scan::kEscape);
        CHECK_MESSAGE(s.shows(s.info, "Stopped waiting"), s.shown(s.info));
        CHECK(s.shows(s.info, "LINKED 'Alpha'"));
        s.edit_field(s.info, "count", "3");
        CHECK_MESSAGE(s.shows(s.info, "*count: 3"), s.shown(s.info));
        s.release(0); // Beta's answer arrives after all, and is not this view's any more
        CHECK_MESSAGE(s.shows(s.info, "LINKED 'Alpha'"), s.shown(s.info));
        CHECK(s.shows(s.info, "*count: 3"));
        s.inventory->hold.clear();
        s.button(s.info, "Save");
        CHECK(s.count("alpha") == 3);
        CHECK(s.count("beta") == 2);
    }
    SUBCASE("a sample from a source that has not answered") {
        Views s(191 | 1024);
        const auto first = mount<StructureSource>(s.r, "story.source");
        capture(s, "story.source", "Sampled");
        const auto a = s.new_view("info.2", 2, 30);
        s.copy_into(a, "Sampled");
        REQUIRE(s.r.bus.unregister_weave(first));
        DeferredRig source(s.r, "story.source");
        s.button(a, "Sample");
        REQUIRE(source.self->asked == 1);
        s.button(a, "Stop");
        CHECK_MESSAGE(s.shows(a, "Stopped waiting"), s.shown(a));
        CHECK(s.shows(a, "state_schema: SourceState"));
        source.release(loom::PokeStructure{"fresh_source", 2, {}});
        CHECK_MESSAGE(!s.shows(a, "fresh_source"), s.shown(a));
        s.edit_field(a, "state_schema", "kept");
        CHECK_MESSAGE(s.shows(a, "*state_schema: kept"), s.shown(a));
    }
}

// ---- OPERATION IDENTITY: every outcome settles the record, and a later operation inherits none -

TEST_CASE("info views: a refused Save copy settles its record and the next Refresh reads and says so") {
    Views s(191 & ~8); // the actor may read and write an entry but never add one
    s.append(5, "Kept");
    const auto a = s.new_view("info.2", 2, 30);
    s.link_into(a, "Kept");
    REQUIRE_MESSAGE(s.shows(a, "LINKED 'Kept'"), s.shown(a));
    const auto entries = s.saved_entries().size();
    s.button(a, "Save copy");
    CHECK_MESSAGE(s.shows(a, "no authority"), s.shown(a));
    CHECK(s.saved_entries().size() == entries);
    s.write(s.entry("Kept").reference, 1, 9); // another writer, with its own authority
    s.button(a, "Refresh");
    CHECK_MESSAGE(s.shows(a, "count: 9"), s.shown(a));
    CHECK(s.shows(a, "saved rev 2"));
    CHECK_FALSE(s.shows(a, "Saved a new entry"));
    CHECK(s.saved_entries().size() == entries);
    // THE NEXT SAVE IS A SAVE, and says only what it did.
    s.edit_field(a, "count", "10");
    s.button(a, "Save");
    CHECK_MESSAGE(s.shows(a, "Saved rev 3"), s.shown(a));
    CHECK(s.entry("Kept").revision == 3);
    CHECK(s.saved_entries().size() == entries);
}

TEST_CASE("info views: each refused operation settles and the next operation claims only its own result") {
    SUBCASE("the owner refuses Save copy and a Refresh follows") {
        ScriptedViews s;
        const auto [a, b] = s.two_views();
        (void)b;
        s.link_into(a, "Alpha");
        s.inventory->refuse_add = "the collection is full";
        s.button(a, "Save copy");
        CHECK_MESSAGE(s.shows(a, "the collection is full"), s.shown(a));
        CHECK(s.inventory->rows.size() == 2);
        s.inventory->refuse_add.clear();
        s.inventory->rows["alpha"] = {2, record(9), "Alpha"}; // another writer
        s.button(a, "Refresh");
        CHECK_MESSAGE(s.shows(a, "count: 9"), s.shown(a));
        CHECK(s.shows(a, "saved rev 2"));
        CHECK_FALSE(s.shows(a, "Saved a new entry"));
        s.button(a, "Save copy");
        CHECK_MESSAGE(s.shows(a, "Saved a new entry 'Alpha copy'"), s.shown(a));
        CHECK(s.inventory->rows.size() == 3);
    }
    SUBCASE("Save copy is refused at dispatch and a Refresh follows") {
        ScriptedViews s;
        const auto [a, b] = s.two_views();
        (void)b;
        s.link_into(a, "Alpha");
        s.doorless();
        s.button(a, "Save copy");
        CHECK_MESSAGE(s.shows(a, "not delivered"), s.shown(a));
        s.doors_back();
        s.inventory->rows["alpha"] = {2, record(9), "Alpha"};
        s.button(a, "Refresh");
        CHECK_MESSAGE(s.shows(a, "count: 9"), s.shown(a));
        CHECK(s.shows(a, "saved rev 2"));
        CHECK_FALSE(s.shows(a, "Saved a new entry"));
        CHECK(s.inventory->rows.size() == 2);
    }
    SUBCASE("the owner refuses a link and the view keeps its entry unstale and saves it") {
        ScriptedViews s;
        const auto [a, b] = s.two_views();
        (void)b;
        s.link_into(a, "Alpha");
        s.inventory->hold = "beta";
        s.link_into(a, "Beta");
        REQUIRE(s.inventory->held.size() == 1);
        s.inventory->rows.erase("beta"); // gone before its read is answered
        s.release(0);
        CHECK_MESSAGE(s.shows(a, "no longer here"), s.shown(a));
        CHECK(s.shows(a, "LINKED 'Alpha'"));
        CHECK_FALSE(s.shows(a, "LINK STALE"));
        CHECK(s.shows(a, "[Watch]"));
        s.inventory->hold.clear();
        s.edit_field(a, "count", "3");
        s.button(a, "Save");
        CHECK_MESSAGE(s.shows(a, "Saved rev 2"), s.shown(a));
        CHECK(s.count("alpha") == 3);
    }
    SUBCASE("a Refresh refused at dispatch keeps the draft and a Save copy follows") {
        ScriptedViews s;
        const auto [a, b] = s.two_views();
        (void)b;
        s.link_into(a, "Alpha");
        s.edit_field(a, "count", "12");
        s.doorless();
        s.button(a, "Refresh"); s.button(a, "Refresh");
        CHECK_MESSAGE(s.shows(a, "not delivered"), s.shown(a));
        CHECK(s.shows(a, "count: 12"));
        CHECK(s.shows(a, "UNSAVED"));
        s.doors_back();
        s.button(a, "Save copy");
        CHECK_MESSAGE(s.shows(a, "Saved a new entry 'Alpha copy'"), s.shown(a));
        std::string copy;
        for (const auto& [key, row] : s.inventory->rows) if (row.label == "Alpha copy") copy = key;
        REQUIRE_FALSE(copy.empty());
        CHECK(s.count(copy) == 12);
    }
}

// ---- WATCH CUSTODY: every way out of a watch ends it at Workshop, and nothing late lands ---------

TEST_CASE("info views: a watched view given a whole value ends its lease and a late watch read never lands") {
    SUBCASE("a clean watched view receives a copy") {
        Views s;
        s.append(5, "Kept");
        const auto a = s.new_view("info.2", 2, 30);
        s.link_into(a, "Kept");
        const auto ref = s.entry("Kept").reference;
        s.button(a, "Watch");
        REQUIRE(s.r.w->observation_leases() == 1);
        s.copy_into(a, "story.RuntimeItem");
        CHECK_MESSAGE(s.shows(a, "COPY story.RuntimeItem"), s.shown(a));
        CHECK(s.r.w->observation_leases() == 0);
        s.watch();
        s.write(ref, 1, 9);
        CHECK(s.sends(PaneObservationContinued::zen_name) == 0);
    }
    SUBCASE("a watch read is in flight when the whole value arrives") {
        ScriptedViews s;
        const auto [a, b] = s.two_views();
        (void)b;
        s.link_into(a, "Alpha");
        s.button(a, "Watch");
        REQUIRE(s.r.w->observation_leases() == 1);
        s.inventory->hold = "alpha";
        s.act_scripted([](ScriptedInventory& office, loom::Mail& m) { office.changed(m); });
        REQUIRE(s.inventory->held.size() == 1); // the watch's read waits
        s.copy_into(a, "Beta");
        CHECK_MESSAGE(s.shows(a, "COPY"), s.shown(a));
        CHECK(s.r.w->observation_leases() == 0);
        s.inventory->rows["alpha"] = {2, record(40), "Alpha"};
        s.release(0);
        CHECK_MESSAGE(s.shows(a, "count: 2"), s.shown(a)); // Beta's copy, not Alpha's late read
        CHECK_FALSE(s.shows(a, "count: 40"));
        CHECK_FALSE(s.shows(a, "~count"));
    }
    SUBCASE("a watch read is in flight when the view closes and its slot is reused") {
        ScriptedViews s;
        const auto [a, b] = s.two_views();
        (void)b;
        s.link_into(a, "Alpha");
        s.button(a, "Watch");
        s.inventory->hold = "alpha";
        s.act_scripted([](ScriptedInventory& office, loom::Mail& m) { office.changed(m); });
        REQUIRE(s.inventory->held.size() == 1);
        s.button(a, "Close");
        CHECK_FALSE(s.r.session().panels.has(a));
        CHECK(s.r.w->observation_leases() == 0);
        const auto again = s.new_view("info.2", 2, 30);
        s.link_into(again, "Beta");
        REQUIRE_MESSAGE(s.shows(again, "LINKED 'Beta'"), s.shown(again));
        s.inventory->rows["alpha"] = {2, record(40), "Alpha"};
        s.release(0);
        CHECK_MESSAGE(s.shows(again, "count: 2"), s.shown(again));
        CHECK_FALSE(s.shows(again, "count: 40"));
        CHECK_FALSE(s.shows(again, "~count"));
    }
}

TEST_CASE("info views: a watch the reset door retires before its approval has the granted lease ended and never restarts") {
    Views s;
    s.append(5, "Watched");
    const auto a = s.new_view("info.2", 2, 30);
    s.link_into(a, "Watched");
    const auto ref = s.entry("Watched").reference;
    s.click(a); // the keys go to the view
    // CTRL+L REACHES THE VIEW: its lease request is queued, and nothing has answered it yet.
    REQUIRE(s.until_delivered(s.key_down(input::scan::kL, input::mod::kCtrl), PaneActionRequested::zen_name,
                              s.r.bus.role_holder(info::kInfoPaneRole)));
    // THE RESET DOOR RETIRES THE VIEW behind that request and ahead of its approval.
    s.r.bus.send_to_role(info::kInfoPaneRole, loom::Message(loom::to_value(PaneResetRequested{"info.2"})));
    s.r.bus.drain_until_idle();
    CHECK(s.r.w->observation_leases() == 0);
    CHECK_MESSAGE(s.shows(a, "Info 2 | empty"), s.shown(a));
    s.watch();
    s.write(ref, 1, 6);
    CHECK(s.sends(PaneObservationContinued::zen_name) == 0); // the approval restarted nothing
    // THE SLOT'S NEXT WATCH IS ITS OWN, and ending the old one did not touch it.
    s.link_into(a, "Watched");
    s.button(a, "Watch");
    CHECK(s.r.w->observation_leases() == 1);
    s.write(ref, 2, 7);
    CHECK_MESSAGE(s.shows(a, "~count: 7"), s.shown(a));
}

TEST_CASE("info views: a reloaded Info holds no observation and its predecessor's leases end") {
    TempDir copy("info-views-reload");
    const std::string image = copy.file(("zengine-info-views-again" +
        std::filesystem::path(WORKSHOP_SO_INFO_PANE).extension().string()).c_str());
    std::filesystem::copy_file(WORKSHOP_SO_INFO_PANE, image);
    Views s;
    s.append(5, "Watched");
    const auto a = s.new_view("info.2", 2, 30);
    s.link_into(a, "Watched");
    SUBCASE("a watch that was on") {
        s.button(a, "Watch");
        REQUIRE(s.r.w->observation_leases() == 1);
    }
    SUBCASE("a lease request whose approval is still to come") {
        s.click(a);
        REQUIRE(s.until_delivered(s.key_down(input::scan::kL, input::mod::kCtrl), PaneActionRequested::zen_name,
                                  s.r.bus.role_holder(info::kInfoPaneRole)));
    }
    s.r.enqueue_reload(info::kInfoPaneStem, image); // the image is replaced at the same address
    s.r.bus.drain_until_idle();
    REQUIRE(s.r.load_refusals.empty());
    CHECK(s.r.w->observation_leases() == 0);
}

namespace {
/// THE KERNEL'S TWO DOORS BESIDE A REALIZED PLAN. The rig's `Booter` publishes the plan booter's
/// state shape again, which a realized plan refuses; this seat's state is its own.
struct FreshSeatState { std::int64_t n = 0; ZEN_SHAPE(FreshSeatState, 1, ZEN_FIELD(n)); };
class FreshSeat : public loom::WeaveBase<FreshSeat, FreshSeatState,
    loom::Accept<loom::Result, loom::Ack, loom::Refused>, loom::Emit<loom::LoadWeave, loom::UnloadLibrary>> {
public:
    FreshSeat(std::vector<std::string>& ok, std::vector<std::string>& no) : ok_(&ok), no_(&no) {}
    void on(const loom::Result& r, loom::Mail&) { ok_->push_back(r.value); }
    void on(const loom::Ack&, loom::Mail&) {}
    void on(const loom::Refused& r, loom::Mail&) { no_->push_back(r.reason); }
private:
    std::vector<std::string>* ok_;
    std::vector<std::string>* no_;
};
/// UNLOAD A LIBRARY, THEN LOAD IT AGAIN AS A NEW PARTICIPANT (a new WeaveId, unlike a reload).
loom::WeaveId unload_then_load(PaneRig& r, const char* name, const char* path, const char* role,
                               const std::function<void()>& between) {
    auto grant = loom::load_capability(r.control);
    grant.allow(loom::LoadWeave::zen_name, loom::LoadWeave::zen_version, r.manager);
    const auto seat = loom::mount_granted<FreshSeat>(r.bus, std::move(grant), r.loaded, r.load_refusals);
    r.bus.send_as(seat, r.control, loom::Message(loom::to_value(loom::UnloadLibrary{name}), seat, seat, 0));
    r.bus.drain_until_idle();
    between();
    const std::size_t before = r.loaded.size();
    r.bus.send_as(seat, r.manager, loom::Message(loom::to_value(loom::LoadWeave{name, path, role}), seat, seat, 0));
    r.bus.drain_until_idle();
    return r.loaded.size() > before ? loom::WeaveId{static_cast<std::uint64_t>(std::stoll(r.loaded.back()))}
                                    : loom::WeaveId{};
}
} // namespace

TEST_CASE("info views: a lease whose Info left the bus is forgotten when the next Info asks for one") {
    Views s;
    s.append(5, "Watched");
    const auto a = s.new_view("info.2", 2, 30);
    s.link_into(a, "Watched");
    s.button(a, "Watch");
    REQUIRE(s.r.w->observation_leases() == 1);
    const auto before = s.r.bus.role_holder(info::kInfoPaneRole);
    // THE IMAGE LEAVES WITH NO SUCCESSOR, and nothing is left to end its lease; then a NEW image
    // arrives at a new WeaveId, whose own ending names only its own leases.
    const auto fresh = unload_then_load(s.r, info::kInfoPaneStem, WORKSHOP_SO_INFO_PANE, info::kInfoPaneRole,
                                        [&] { CHECK(s.r.w->observation_leases() == 1); });
    REQUIRE_MESSAGE(fresh.valid(), (s.r.load_refusals.empty() ? std::string() : s.r.load_refusals.back()));
    REQUIRE(fresh != before);
    CHECK(s.r.w->observation_leases() == 1);
    // ...and its first request, from another pane, forgets the lease nobody can continue.
    const auto second = s.new_view("info.2", 2, 30);
    const auto third = s.new_view("info.3", 92, 30);
    (void)second;
    s.link_into(third, "Watched");
    s.button(third, "Watch");
    CHECK_MESSAGE(s.shows(third, "watch ON"), s.shown(third));
    CHECK(s.r.w->observation_leases() == 1);
}

TEST_CASE("info views: the default view's watch ends when it returns to pane properties") {
    Views s;
    s.append(5, "Watched");
    s.link_into(s.info, "Watched");
    REQUIRE_MESSAGE(s.shows(s.info, "LINKED 'Watched'"), s.shown(s.info));
    const auto ref = s.entry("Watched").reference;
    s.button(s.info, "Watch");
    REQUIRE(s.r.w->observation_leases() == 1);
    s.button(s.info, "Panes");
    CHECK_MESSAGE(s.shows(s.info, "PANES"), s.shown(s.info));
    CHECK(s.r.w->observation_leases() == 0);
    s.watch();
    s.write(ref, 1, 6);
    CHECK(s.sends(PaneObservationContinued::zen_name) == 0);
}

TEST_CASE("info views: a watch whose read Loom refuses at dispatch ends at Workshop too") {
    ScriptedViews s;
    const auto [a, b] = s.two_views();
    (void)b;
    s.link_into(a, "Alpha");
    s.button(a, "Watch");
    REQUIRE(s.r.w->observation_leases() == 1);
    s.doorless();
    s.doorless_changed(); // the continuation is approved; its read cannot be dispatched
    CHECK_MESSAGE(s.shows(a, "not delivered"), s.shown(a));
    CHECK(s.r.w->observation_leases() == 0);
    CHECK(s.shows(a, "count: 1")); // the last good value stays
    s.doors_back();
}
