// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
// NAMED INVENTORY FOLDERS through the real loaded Inventory, Inventory pane and Info images: a
// maker's keys and presses, drops that file entries, batched input against a pending operation,
// and organization beside a linked draft, a portable command and a restored toolbox. The owner's
// own rules (names, bounds, trees, archives) are pinned in test_inventory.cpp.
#include "workshop_support.hpp"
#include "inventory_story.hpp"
#include "inventory-pane/toolbox_file.hpp"
namespace slots = zengine::inventory_pane;

namespace {
namespace inv = zengine::inventory;
using namespace inventory_story;
constexpr int kOrganizer = 191 | InventoryStory::kOrganize;

/// Deliveries of one shape to the inventory owner, counted while the case runs: organizing,
/// browsing and restoring must deliver no stored command.
struct Deliveries {
    loom::Switchboard& bus;
    std::string shape;
    int count = 0;
    loom::ObserverId id{};
    Deliveries(loom::Switchboard& b, std::string s) : bus(b), shape(std::move(s)) {
        id = bus.add_observer([this](const loom::BusEvent& e) {
            if (e.kind == loom::EventKind::Delivered && e.schema_name == shape) ++count;
        });
    }
    ~Deliveries() { bus.remove_observer(id); }
};
/// Seat a portable view away from Inventory and Info. A same-size extent reseats nothing, so the
/// room is changed and changed back (the Info views suite's technique).
void place(InventoryStory& s, const std::string& key) {
    for (auto& p : s.r.session().setup.active.panes) if (p.ref.pane == key) {
        p.place = {pane_unit::kSubcells, 2 * surface::kCellSubs, 34 * surface::kCellSubs};
        p.width = {pane_unit::kSubcells, 72 * surface::kCellSubs}; p.height = {pane_unit::kSubcells, 10 * surface::kCellSubs};
    }
    s.r.extent(179, 60); s.r.extent(180, 60);
}
std::string create_folder(InventoryStory& s, const std::string& name) {
    s.key(input::scan::kD, input::mod::kCtrl);
    REQUIRE_MESSAGE(s.shown(s.source).find("New folder:") != std::string::npos, s.shown(s.source));
    s.text(name); s.key(input::scan::kReturn);
    return s.shown(s.source);
}
/// Give Info the keyboard, as a maker working there before pointing at Inventory would.
void focus_info(InventoryStory& s) {
    s.click(s.info);
    REQUIRE(s.r.session().panels.keyboard == s.info);
}
/// Right-press `row` of a pane and walk its open menu to the line reading `label`, leaving the
/// choosing key to the case. An open menu's presenter takes every key, wherever the keys are.
void point_menu_at(InventoryStory& s, std::int64_t kind, std::int64_t row, const std::string& label) {
    s.click(kind, row, 3);
    REQUIRE_MESSAGE(menu_shown(s.r.session()), s.shown(kind));
    const auto at = presented_line_of(s.r.session(), label);
    REQUIRE_MESSAGE(at >= 0, label);
    for (std::int64_t n = 0; n < at; ++n) s.key(input::scan::kDown);
    REQUIRE(presented_texts(s.r.session())[static_cast<std::size_t>(at)] == "> " + label);
}
/// ...and choose it with Return.
void choose(InventoryStory& s, std::int64_t kind, std::int64_t row, const std::string& label) {
    point_menu_at(s, kind, row, label);
    s.key(input::scan::kReturn);
    REQUIRE_FALSE(s.r.session().presented.open);
}
}

TEST_CASE("inventory folders: a maker creates, opens, climbs, renames and jumps with keys and presses") {
    InventoryStory s(kOrganizer);
    // A collection without folders looks exactly as it did.
    CHECK(s.row_of(s.source, "(Up)") < 0);
    s.click(s.source);
    create_folder(s, "Workbench");
    const auto workbench = s.folder_id("Workbench");
    CHECK_MESSAGE(s.row_of(s.source, "(Up) Root") == 1, s.shown(s.source));
    CHECK(s.row_of(s.source, "> Workbench/  (empty)") >= 0); // created where the maker is, and selected
    s.key(input::scan::kReturn);
    CHECK_MESSAGE(s.row_of(s.source, "[Up] Root > Workbench") == 1, s.shown(s.source));
    CHECK(s.shown(s.source).find("Empty folder") != std::string::npos);
    create_folder(s, "Samples");
    create_folder(s, "Commands");
    const auto commands = s.folder_id("Commands");
    CHECK(s.row_of(s.source, "> Commands/") >= 0);
    CHECK(s.row_of(s.source, "Commands/") < s.row_of(s.source, "Samples/")); // folders by name
    s.key(input::scan::kReturn);
    create_folder(s, "Drafts");
    CHECK(s.row_of(s.source, "Root > Workbench > Commands") == 1);
    // Backspace climbs and selects the folder it came from; one press on that remembered
    // selection only selects it, so pointing at a folder to act on it never opens it.
    s.key(input::scan::kBackspace);
    CHECK_MESSAGE(s.row_of(s.source, "> Commands/  (1)") >= 0, s.shown(s.source));
    s.click(s.source, s.row_of(s.source, "Commands/"));
    CHECK_MESSAGE(s.row_of(s.source, "Root > Workbench > Commands") < 0, s.shown(s.source));
    // Renaming keeps the folder's identity; an open name line keeps Backspace as text.
    s.key(input::scan::kN, input::mod::kCtrl);
    REQUIRE(s.shown(s.source).find("Folder name:") != std::string::npos);
    for (int n = 0; n < 7; ++n) s.key(input::scan::kBackspace); // "Commands" -> "C"
    s.text("Tools"); s.key(input::scan::kReturn);
    CHECK_MESSAGE(s.folder_id("CTools") == commands, s.shown(s.source));
    CHECK(s.row_of(s.source, "Root > Workbench") == 1); // the name line's Backspace did not climb
    // A press selects a folder; pressing it again opens it.
    s.click(s.source, s.row_of(s.source, "Samples/"));
    CHECK(s.row_of(s.source, "Root > Workbench > Samples") < 0);
    s.click(s.source, s.row_of(s.source, "Samples/"));
    CHECK_MESSAGE(s.row_of(s.source, "[Up] Root > Workbench > Samples") == 1, s.shown(s.source));
    // A crumb jumps to that ancestor and selects the child the maker came through.
    s.click_at(s.source, 1, s.column_of(s.source, 1, "Root"));
    CHECK_MESSAGE(s.row_of(s.source, "(Up) Root") == 1, s.shown(s.source));
    CHECK(s.row_of(s.source, "> Workbench/") >= 0);
    // [Up] is a control; Alt+Home returns to Root; at Root, Up changes nothing and says so.
    s.key(input::scan::kReturn); s.key(input::scan::kDown); s.key(input::scan::kReturn);
    CHECK(s.row_of(s.source, "Root > Workbench > Samples") == 1);
    s.click_at(s.source, 1, s.column_of(s.source, 1, "[Up]"));
    CHECK(s.row_of(s.source, "Root > Workbench") == 1);
    s.key(input::scan::kHome, input::mod::kAlt);
    CHECK(s.row_of(s.source, "(Up) Root") == 1);
    const auto at_root = s.folders();
    s.key(input::scan::kBackspace);
    CHECK(s.shown(s.source).find("Already at Root") != std::string::npos);
    CHECK(s.folders().folders.size() == at_root.folders.size());
    (void)workbench;
}

TEST_CASE("inventory folders: names conflict only among siblings, and a refused name changes nothing") {
    InventoryStory s(kOrganizer);
    const auto workbench = s.make_folder("", "Workbench");
    const auto tools = s.make_folder(workbench, "Tools");
    (void)s.make_folder(workbench, "Samples");
    s.click(s.source, s.row_of(s.source, "Workbench/")); s.key(input::scan::kReturn);
    REQUIRE(s.row_of(s.source, "Root > Workbench") == 1);
    const auto count = s.folders().folders.size();
    create_folder(s, "samples"); // a sibling, ignoring case
    CHECK_MESSAGE(s.shown(s.source).find("already a folder") != std::string::npos, s.shown(s.source));
    CHECK(s.folders().folders.size() == count);
    // An invalid name refuses before any request, keeps the line open, and Escape leaves it.
    const auto shown = create_folder(s, "a/b");
    CHECK(shown.find("cannot contain '/'") != std::string::npos);
    CHECK(shown.find("New folder:") != std::string::npos);
    s.key(input::scan::kEscape);
    CHECK(s.folders().folders.size() == count);
    // The same name under another parent is fine.
    s.click(s.source, s.row_of(s.source, "Tools/")); s.key(input::scan::kReturn);
    create_folder(s, "Samples");
    CHECK(s.folders().folders.size() == count + 1);
    for (const auto& f : s.folders().folders) if (f.name == "Samples" && f.parent == tools) return;
    FAIL("no Samples inside Tools");
}

TEST_CASE("inventory folders: an actor without the organizing grant is refused and the next operation keeps its own meaning") {
    InventoryStory s(191); // may read, add, rename and remove entries, but not organize
    s.append(1, "Victim");
    const auto workbench = s.make_folder("", "Workbench");
    s.click(s.source);
    create_folder(s, "Refused");
    CHECK_MESSAGE(s.shown(s.source).find("authority") != std::string::npos, s.shown(s.source));
    CHECK(s.folders().folders.size() == 1);
    s.drag_to(s.source, s.row_of(s.source, "Victim"), s.source, s.row_of(s.source, "Workbench/"));
    CHECK(s.member_of("Victim").empty());
    CHECK(s.shown(s.source).find("authority") != std::string::npos);
    // The press that began the drag left Victim selected. Removing it next is reported as itself,
    // never with the refused filing's words.
    s.key(input::scan::kDelete); s.key(input::scan::kDelete);
    CHECK_MESSAGE(s.shown(s.source).find("Entry updated") != std::string::npos, s.shown(s.source));
    CHECK(s.shown(s.source).find("Filed") == std::string::npos);
    CHECK(s.row_of(s.source, "Victim") < 0);
    (void)workbench;
}

TEST_CASE("inventory folders: drops on a folder row, a crumb or Up file an entry without copying it") {
    InventoryStory s(kOrganizer);
    s.append(1, "Sample A");
    const auto workbench = s.make_folder("", "Workbench");
    const auto samples = s.make_folder(workbench, "Samples");
    const auto before = s.entry("Sample A");
    const auto entries = s.saved_entries().size();
    s.drag_to(s.source, s.row_of(s.source, "Sample A"), s.source, s.row_of(s.source, "Workbench/"));
    CHECK_MESSAGE(s.member_of("Sample A") == workbench, s.shown(s.source));
    CHECK(s.shown(s.source).find("Filed 'Sample A' in Root > Workbench") != std::string::npos);
    CHECK(s.row_of(s.source, " Sample A : ") < 0); // it left the displayed folder
    s.click(s.source, s.row_of(s.source, "Workbench/")); s.key(input::scan::kReturn);
    s.drag_to(s.source, s.row_of(s.source, "Sample A"), s.source, s.row_of(s.source, "Samples/"));
    CHECK(s.member_of("Sample A") == samples);
    s.click(s.source, s.row_of(s.source, "Samples/")); s.key(input::scan::kReturn);
    // [Up] is the parent as a drop target; a crumb is that ancestor.
    s.drag_to(s.source, s.row_of(s.source, "Sample A"), s.source, 1, s.column_of(s.source, 1, "[Up]"));
    CHECK_MESSAGE(s.member_of("Sample A") == workbench, s.shown(s.source));
    s.key(input::scan::kBackspace);
    s.drag_to(s.source, s.row_of(s.source, "Sample A"), s.source, 1, s.column_of(s.source, 1, "Root"));
    CHECK(s.member_of("Sample A").empty());
    // Same entry throughout: no copy, no new identity, no content revision.
    CHECK(s.saved_entries().size() == entries);
    const auto after = s.entry("Sample A");
    CHECK(slots::same(after.reference, before.reference));
    CHECK(after.revision == before.revision);
}

TEST_CASE("inventory folders: a queued press aimed at an earlier picture never acts on the row's next occupant") {
    InventoryStory s(kOrganizer);
    const auto a = s.make_folder("", "A");
    (void)s.make_folder("", "B");
    s.append(5, "Inside A");
    s.r.bus.send_to_role(inv::kInventoryRole, loom::Message(loom::to_value(inv::InventoryFile{
        s.entry("Inside A").reference, {s.folders().owner, ""}, {s.folders().owner, a}})));
    s.r.bus.drain_until_idle();
    const auto folder_row = s.row_of(s.source, "A/");
    s.click(s.source, folder_row); // selects A
    // One batch: Enter opens A, then a press, motion and release aimed where "A/" was painted.
    auto press = s.button_at(s.source, folder_row, true), release = s.button_at(s.info, 0, false);
    auto move = release; move.kind = "PointerMoved"; move.dx = release.x - press.x; move.dy = release.y - press.y;
    s.batch({s.key_down(input::scan::kReturn), press, move, release});
    REQUIRE_MESSAGE(s.row_of(s.source, "Root > A") == 1, s.shown(s.source));
    REQUIRE(s.row_of(s.source, "Inside A") == folder_row); // the next occupant of that row
    CHECK_MESSAGE(s.shown(s.info).find("story.RuntimeItem") == std::string::npos, s.shown(s.info));
    // The same drag against the current picture does reach it: only the stale aim was refused.
    s.drag(folder_row);
    CHECK_MESSAGE(s.shown(s.info).find("count: 5") != std::string::npos, s.shown(s.info));
}

TEST_CASE("inventory folders: a removal overtaken by newer gestures in one batch is refused, and navigation still works") {
    InventoryStory s(kOrganizer);
    (void)s.make_folder("", "Old");
    const auto keep = s.make_folder("", "Keep");
    s.append(3, "Kept");
    s.r.bus.send_to_role(inv::kInventoryRole, loom::Message(loom::to_value(inv::InventoryFile{
        s.entry("Kept").reference, {s.folders().owner, ""}, {s.folders().owner, keep}})));
    s.r.bus.drain_until_idle();
    s.click(s.source, s.row_of(s.source, "Old/"));
    // One batch: confirm the removal, then move and open another folder before it is approved.
    // Workshop approves only the latest gesture, so the removal is refused; nothing retargets.
    s.batch({s.key_down(input::scan::kDelete), s.key_down(input::scan::kDelete),
             s.key_down(input::scan::kUp), s.key_down(input::scan::kReturn)});
    CHECK(s.folders().folders.size() == 2);
    CHECK_MESSAGE(s.row_of(s.source, "Root > Keep") == 1, s.shown(s.source));
    CHECK(s.row_of(s.source, " Kept : ") >= 0);
    CHECK(s.shown(s.source).find("current attributed input gesture") != std::string::npos);
    CHECK(s.shown(s.source).find("Removed") == std::string::npos);
    // The next valid operation works, and names what it removed.
    s.key(input::scan::kBackspace);
    s.click(s.source, s.row_of(s.source, "Old/"));
    s.key(input::scan::kDelete); s.key(input::scan::kDelete);
    REQUIRE(s.folders().folders.size() == 1);
    CHECK(s.folders().folders.front().folder.folder == keep);
    CHECK(s.shown(s.source).find("Removed empty folder 'Old'") != std::string::npos);
    // A folder that holds anything refuses before any request, naming why.
    s.click(s.source, s.row_of(s.source, "Keep/"));
    s.key(input::scan::kDelete);
    CHECK(s.shown(s.source).find("still holds 1 item") != std::string::npos);
    s.key(input::scan::kDelete);
    CHECK(s.folders().folders.size() == 1);
    CHECK(s.member_of("Kept") == keep);
}

namespace {
/// AN INVENTORY OWNER THAT HOLDS ITS ANSWERS until the case releases them: a slow or silent
/// owner, through the real Inventory pane (docs/contributing/testing-workshop-panes.md). It holds
/// folder answers and, while `hold_reads`, entry reads. It files at once under the real owner's
/// rule -- the entry must still be in `from` (pinned for `InventoryWeave` in test_inventory.cpp)
/// -- and keeps every filing as it arrived, so a case can read the `from` a pane sent.
struct HeldState { ZEN_SHAPE(HeldState, 1); };
class HeldOwner : public loom::WeaveBase<HeldOwner, HeldState,
    loom::Accept<InventoryHandDo, inv::v2::InventoryList, inv::InventoryFolderCreate, inv::InventoryFolderRemove,
        inv::InventoryRead, inv::InventoryFile>,
    loom::Emit<inv::v2::InventoryListed, inv::InventoryFolderState, inv::InventoryEntry, inv::InventoryChanged,
        loom::Ack, loom::Refused>> {
public:
    inv::v2::InventoryListed listing{"held-owner", 0, {}, {{{"held-owner", "keep"}, 1, "Keep", ""}, {{"held-owner", "old"}, 1, "Old", ""}}};
    std::map<std::string, loom::Bytes> pairs; ///< each listed entry's stored bytes, by entry id
    std::vector<std::pair<std::string, loom::DeferredAnswer>> held;
    std::vector<inv::InventoryFile> filed;
    bool hold_reads = true;
    std::function<void(HeldOwner&, loom::Mail&)> next;
    void on(const InventoryHandDo&, loom::Mail& m) { next(*this, m); }
    void on(const inv::v2::InventoryList&, loom::Mail& m) { (void)m.answer(listing); }
    void on(const inv::InventoryFolderCreate& c, loom::Mail& m) { held.push_back({"create " + c.name, m.defer_answer()}); }
    void on(const inv::InventoryFolderRemove& r, loom::Mail& m) { held.push_back({"remove " + r.folder.folder, m.defer_answer()}); }
    void on(const inv::InventoryRead& r, loom::Mail& m) {
        if (hold_reads) { held.push_back({"read " + r.reference.entry, m.defer_answer()}); return; }
        if (const auto* e = entry(r.reference); e) (void)m.answer(stored(*e));
        else (void)m.answer(loom::Refused{"this inventory entry is no longer here"});
    }
    void on(const inv::InventoryFile& f, loom::Mail& m) {
        filed.push_back(f);
        auto* e = entry(f.reference);
        const bool into = f.into.owner == listing.owner && (f.into.folder.empty() ||
            std::any_of(listing.folders.begin(), listing.folders.end(), [&](const auto& s) { return s.folder.folder == f.into.folder; }));
        if (!e) (void)m.answer(loom::Refused{"this inventory entry is no longer here"});
        else if (f.from.owner != listing.owner || f.from.folder != e->folder)
            (void)m.answer(loom::Refused{"the entry is no longer in that folder; look again"});
        else if (!into) (void)m.answer(loom::Refused{"the destination folder is no longer here"});
        else { e->folder = f.into.folder; ++listing.revision; (void)m.answer(stored(*e)); changed(m); }
    }
    inv::v2::InventorySummary* entry(const inv::InventoryReference& ref) {
        for (auto& e : listing.entries) if (e.reference.owner == ref.owner && e.reference.entry == ref.entry) return &e;
        return nullptr;
    }
    inv::InventoryEntry stored(const inv::v2::InventorySummary& e) { return {e.reference, e.revision, pairs[e.reference.entry]}; }
    /// Answer the oldest held read with the entry as it is now, or refuse it in `refusal`'s words.
    void answer_read(loom::Mail& m, const std::string& refusal = {}) {
        REQUIRE(!held.empty()); REQUIRE(held.front().first.starts_with("read "));
        const auto* e = entry({listing.owner, held.front().first.substr(5)});
        REQUIRE(e != nullptr);
        if (refusal.empty()) (void)loom::answer_deferred(held.front().second, m, stored(*e));
        else (void)loom::answer_deferred(held.front().second, m, loom::Refused{refusal});
        held.erase(held.begin());
    }
    void changed(loom::Mail& m) { m.as_role(inv::kInventoryRole).publish(inv::InventoryChanged{}); }
};
struct HeldStory : InventoryStory {
    HeldOwner* owner = nullptr;
    loom::WeaveId id;
    HeldStory() : InventoryStory(kOrganizer) {
        REQUIRE(r.kernel.unload_role(inv::kInventoryRole));
        auto owned = std::make_unique<HeldOwner>(); owner = owned.get();
        loom::Grant grant;
        grant.allow_to_any(inv::v2::InventoryListed::zen_name, 2);
        for (const char* shape : {inv::InventoryFolderState::zen_name, inv::InventoryEntry::zen_name,
                                  inv::InventoryChanged::zen_name, loom::Ack::zen_name, loom::Refused::zen_name})
            grant.allow_to_any(shape, 1);
        id = r.bus.register_weave(std::move(owned), grant, inv::kInventoryRole);
        owner->zen_set_self(id);
        with_owner([](HeldOwner&, loom::Mail& m) { m.as_role(inv::kInventoryRole).publish(inv::InventoryChanged{}); });
    }
    void with_owner(std::function<void(HeldOwner&, loom::Mail&)> f) {
        owner->next = std::move(f);
        r.bus.send(id, loom::Message(loom::to_value(InventoryHandDo{})));
        r.bus.drain_until_idle(); owner->next = {};
    }
};
}

TEST_CASE("inventory folders: while an owner is silent the maker still browses, other acts refuse visibly, and a late answer names its own folder") {
    HeldStory s;
    REQUIRE_MESSAGE(s.row_of(s.source, "Old/") >= 0, s.shown(s.source));
    s.click(s.source, s.row_of(s.source, "Old/"));
    s.key(input::scan::kDelete); s.key(input::scan::kDelete);
    REQUIRE(s.owner->held.size() == 1); // approved and sent; the owner has not answered
    // Browsing is local: select, open, climb.
    s.key(input::scan::kUp); s.key(input::scan::kReturn);
    CHECK_MESSAGE(s.row_of(s.source, "Root > Keep") == 1, s.shown(s.source));
    // Another operation says it waits, and starts nothing.
    s.key(input::scan::kD, input::mod::kCtrl);
    CHECK(s.shown(s.source).find("Still waiting") != std::string::npos);
    CHECK(s.shown(s.source).find("New folder:") == std::string::npos);
    CHECK(s.owner->held.size() == 1);
    // The late success names the folder it removed, wherever the maker is now.
    s.with_owner([](HeldOwner& o, loom::Mail& m) {
        std::erase_if(o.listing.folders, [](const auto& f) { return f.folder.folder == "old"; });
        (void)loom::answer_deferred(o.held.front().second, m, loom::Ack{}); o.held.clear();
        m.as_role(inv::kInventoryRole).publish(inv::InventoryChanged{});
    });
    CHECK(s.shown(s.source).find("Removed empty folder 'Old'") != std::string::npos);
    CHECK(s.row_of(s.source, "Root > Keep") == 1);
    // A refused operation settles, claims nothing, and the next one starts.
    create_folder(s, "Refused here");
    REQUIRE(s.owner->held.size() == 1);
    s.key(input::scan::kBackspace);
    s.with_owner([](HeldOwner& o, loom::Mail& m) {
        (void)loom::answer_deferred(o.held.front().second, m, loom::Refused{"the scripted owner refused"}); o.held.clear();
    });
    CHECK(s.shown(s.source).find("the scripted owner refused") != std::string::npos);
    CHECK(s.shown(s.source).find("Created") == std::string::npos);
    CHECK(s.row_of(s.source, "(Up) Root") == 1);
    create_folder(s, "Next");
    CHECK(s.owner->held.size() == 1);
    CHECK(s.owner->held.front().first == "create Next");
}

TEST_CASE("inventory folders: a folder removed before a filed drop is approved leaves the entry where it was") {
    InventoryStory s(kOrganizer);
    s.append(1, "Moving");
    const auto doomed = s.make_folder("", "Doomed");
    const auto owner = s.folders().owner;
    auto press = s.button_at(s.source, s.row_of(s.source, "Moving"), true);
    auto release = s.button_at(s.source, s.row_of(s.source, "Doomed/"), false);
    auto move = release; move.kind = "PointerMoved"; move.dx = release.x - press.x; move.dy = release.y - press.y;
    s.batch({press, move});
    // Stop right after the drop's filing request reaches Workshop for approval, then remove the
    // destination: the approved request must not retarget another folder or the root.
    REQUIRE(s.until_delivered(release, PaneOperationRequested::zen_name, s.r.bus.role_holder(kWorkshopProvider)));
    s.r.bus.send_to_role(inv::kInventoryRole, loom::Message(loom::to_value(inv::InventoryFolderRemove{{owner, doomed}, 1})));
    s.r.bus.drain_until_idle();
    CHECK(s.folders().folders.empty());
    CHECK(s.member_of("Moving").empty());
    CHECK_MESSAGE(s.shown(s.source).find("no longer here") != std::string::npos, s.shown(s.source));
    // The next valid operation works.
    const auto kept = s.make_folder("", "Kept");
    s.drag_to(s.source, s.row_of(s.source, "Moving"), s.source, s.row_of(s.source, "Kept/"));
    CHECK(s.member_of("Moving") == kept);
}

TEST_CASE("inventory folders: organizing keeps a linked Info draft and a portable command, and runs nothing until asked") {
    InventoryStory s(kOrganizer, true, true);
    Deliveries renames(s.r.bus, inv::InventoryRename::zen_name);
    s.append(1, "victim"); s.append(7, "Sample");
    const auto victim = s.entry("victim");
    const auto bytes = inv::encode_pair(loom::to_value(inv::InventoryRename{victim.reference, victim.revision, "executed"}), {});
    s.r.bus.send_to_role(inv::kInventoryRole, loom::Message(loom::to_value(inv::InventoryAdd{loom::Bytes(bytes.begin(), bytes.end()), "command"})));
    s.r.bus.drain_until_idle();
    const auto command = s.entry("command");
    const auto row = s.create("row", command.reference);
    place(s, row);
    s.bind(command.reference); s.context(row, true);
    // A linked draft of Sample with an unsaved edit.
    s.click(s.source, s.row_of(s.source, " Sample : "));
    s.key(input::scan::kReturn, input::mod::kCtrl); s.place();
    s.edit("42");
    REQUIRE(s.shown(s.info).find("LINKED") != std::string::npos);
    // Organize: folders, filing both entries, a rename and a folder move by pick and paste.
    const auto workbench = s.make_folder("", "Workbench");
    const auto samples = s.make_folder(workbench, "Samples");
    const auto commands = s.make_folder(workbench, "Commands");
    s.drag_to(s.source, s.row_of(s.source, " Sample : "), s.source, s.row_of(s.source, "Workbench/"));
    const auto row_kind = s.r.session().panels.runtime.find(slots::kRole, row)->kind;
    s.click(s.source, s.row_of(s.source, "Workbench/")); s.key(input::scan::kReturn);
    // The tile's drop onto a folder row files it; its placement is untouched.
    s.drag_to(row_kind, 3, s.source, s.row_of(s.source, "Commands/"));
    CHECK_MESSAGE(s.member_of("command") == commands, (s.shown(s.source) + s.shown(row_kind)));
    CHECK(slots::placed(s.layout(), command.reference) == row);
    s.drag_to(s.source, s.row_of(s.source, " Sample : "), s.source, s.row_of(s.source, "Samples/"));
    s.click(s.source, s.row_of(s.source, "Commands/"));
    s.key(input::scan::kN, input::mod::kCtrl); s.key(input::scan::kA, input::mod::kCtrl); s.text("Tools"); s.key(input::scan::kReturn);
    s.click(s.source, s.row_of(s.source, "Samples/")); s.key(input::scan::kX, input::mod::kCtrl);
    CHECK(s.shown(s.source).find("[moving]") != std::string::npos);
    s.click(s.source, s.row_of(s.source, "Tools/")); s.key(input::scan::kReturn);
    s.click_at(s.source, 1, s.column_of(s.source, 1, "[Move here]"));
    const auto moved = s.folders();
    for (const auto& f : moved.folders) if (f.folder.folder == samples) CHECK(f.parent == commands);
    CHECK(s.member_of("Sample") == samples);
    // Nothing ran; the command is the same entry, in the same row, with the same binding.
    CHECK(renames.count == 0);
    CHECK(s.entry("victim").revision == 1);
    CHECK(slots::placed(s.layout(), command.reference) == row);
    REQUIRE(slots::binding(s.layout(), command.reference) != nullptr);
    CHECK(slots::same(s.entry("command").reference, command.reference));
    // The linked draft kept its edit and its link; its conditional save still succeeds.
    CHECK(s.shown(s.info).find("LINKED") != std::string::npos);
    CHECK(s.shown(s.info).find("count: 42") != std::string::npos);
    s.click(s.info); s.key(input::scan::kS, input::mod::kCtrl);
    CHECK_MESSAGE(s.saved_entries()[1].item.get("count")->as_int() == 42, s.shown(s.info));
    // A deliberate, authorized invocation delivers once.
    s.r.pick({"zengine.info", "info"});
    s.key(input::scan::k1, input::mod::kAlt);
    CHECK_MESSAGE(s.entry("executed").revision == 2, s.shown(s.source));
    CHECK(renames.count == 1);
    // Returned to main Inventory, the command reappears in its own folder, not where Inventory is.
    slots::InventoryViewEdit back; back.operation = "move"; back.view = "inventory"; back.entry = command.reference;
    s.change(back);
    CHECK(slots::placed(s.layout(), command.reference) == "inventory");
    CHECK(s.member_of("command") == commands);
    CHECK_MESSAGE(s.row_of(s.source, " command : ") >= 0, s.shown(s.source)); // Inventory is showing Tools
}

TEST_CASE("inventory folders: a flat toolbox restores at the root and a nested one restores folders with hotkeys off") {
    TempDir files("inventory-folders");
    InventoryStory s(kOrganizer | 512, true);
    Deliveries renames(s.r.bus, inv::InventoryRename::zen_name);
    // A file saved before folders existed, written exactly as version 1 wrote it.
    const auto flat_path = files.file("flat.toolbox");
    {
        const auto pair = s.pair(11);
        slots::InventoryToolbox flat{{{{std::string(32, 'a'), "Flat entry", pair, false}}}, {}, {}};
        std::ofstream out(flat_path, std::ios::binary);
        const auto bytes = loom::serialize(loom::to_value(flat));
        out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    }
    s.act([&](loom::Mail& m) { m.send_to_role(slots::kRole, slots::InventoryToolboxRestore{flat_path, true}); });
    REQUIRE_MESSAGE(s.hand->toolboxes.size() == 1, s.shown(s.source));
    CHECK(s.folders().folders.empty());
    CHECK(s.member_of("Flat entry").empty());
    CHECK(s.row_of(s.source, "(Up)") < 0);
    // Organize, bind a command in a row, and save the nested toolbox.
    const auto workbench = s.make_folder("", "Workbench");
    const auto drafts = s.make_folder(workbench, "Drafts");
    (void)s.make_folder("", "Empty");
    const auto owner = s.folders().owner;
    const auto flat = s.entry("Flat entry");
    s.r.bus.send_to_role(inv::kInventoryRole, loom::Message(loom::to_value(inv::InventoryFile{flat.reference, {owner, ""}, {owner, drafts}})));
    s.r.bus.drain_until_idle();
    const auto row = s.create("row", flat.reference);
    place(s, row);
    s.bind(flat.reference); s.context(row, true);
    const auto path = files.file("nested.toolbox");
    s.act([&](loom::Mail& m) { m.send_to_role(slots::kRole, slots::InventoryToolboxSave{path}); });
    REQUIRE(s.hand->toolboxes.size() == 2);
    // A malformed candidate is refused whole and the organized collection stays usable.
    auto broken = slots::read_toolbox(path);
    for (auto& f : broken.archive.folders) if (f.key == workbench) f.parent = drafts;
    const auto broken_path = files.file("cycle.toolbox");
    {
        std::ofstream out(broken_path, std::ios::binary);
        const auto bytes = loom::serialize(loom::to_value(broken));
        out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    }
    s.hand->expect_refusal = true;
    s.act([&](loom::Mail& m) { m.send_to_role(slots::kRole, slots::InventoryToolboxRestore{broken_path, true}); });
    REQUIRE(s.hand->refusals.size() == 1);
    s.hand->expect_refusal = false;
    CHECK(s.folders().owner == owner);
    CHECK(s.folders().folders.size() == 3);
    // Browse into Drafts, then change the collection before restoring the saved one.
    s.click(s.source, s.row_of(s.source, "Workbench/")); s.key(input::scan::kReturn);
    s.click(s.source, s.row_of(s.source, "Drafts/")); s.key(input::scan::kReturn);
    (void)s.make_folder("", "Later");
    s.act([&](loom::Mail& m) { m.send_to_role(slots::kRole, slots::InventoryToolboxRestore{path, true}); });
    REQUIRE(s.hand->toolboxes.size() == 3);
    const auto restored = s.folders();
    CHECK(restored.owner != owner);
    CHECK(restored.folders.size() == 3);
    CHECK(s.member_of("Flat entry") == drafts);
    CHECK_MESSAGE(s.row_of(s.source, "(Up) Root") == 1, s.shown(s.source)); // browsing starts again at Root
    const auto layout = s.layout();
    CHECK(slots::placed(layout, s.entry("Flat entry").reference) == row);
    REQUIRE(layout.bindings.size() == 1);
    CHECK_FALSE(layout.bindings.front().enabled);
    CHECK(slots::shortcuts(layout).empty());
    CHECK(renames.count == 0);
}

TEST_CASE("inventory folders: a narrow short room keeps Up, elides middle crumbs and keeps the selection visible") {
    slots::InventoryViews state;
    std::vector<inv::InventoryFolderState> folders;
    for (int i = 0; i < 6; ++i) folders.push_back({{"o", "f" + std::to_string(i)}, 1, "Folder" + std::to_string(i), ""});
    slots::Browse b; b.show = true; b.owner = "o";
    b.crumbs = {{"", "Root"}, {"w", "Workbench"}, {"c", "Commands"}, {"d", "Drafts"}};
    for (const auto& f : folders) b.folders.push_back({&f, 0});
    std::vector<inv::InventorySummary> entries = {{{"o", "e1"}, 1, "Entry", "Item", 1, false}};
    slots::View view; view.rows = 6; view.columns = 30; view.selected = slots::folder_row("o", "f5");
    const auto rows = slots::render(state, "inventory", view, entries, {}, {}, {}, b); view.map.settle();
    REQUIRE(rows.size() == 6);
    CHECK(rows[1].text == "[Up] Root > ... > Drafts");
    REQUIRE(view.map.at(1, 1)); CHECK(*view.map.at(1, 1) == slots::kUpControl);
    REQUIRE(view.map.at(1, 6)); CHECK(*view.map.at(1, 6) == slots::crumb("o", ""));
    REQUIRE(view.map.at(1, 20)); CHECK(*view.map.at(1, 20) == slots::crumb("o", "d"));
    CHECK(view.map.at(1, 13) == nullptr); // the elision is not a target
    // Seven rows of content in two: the selected folder is painted and the rest are counted.
    std::string all; for (const auto& r : rows) all += r.text + "\n";
    CHECK(all.find("> Folder5/  (empty)") != std::string::npos);
    CHECK(all.find("earlier") != std::string::npos);
    // Members placed in portable views are counted after the crumbs, shortened before any crumb is cut.
    b.placed = 2;
    const auto counted = slots::render(state, "inventory", view, entries, {}, {}, {}, b); view.map.settle();
    CHECK(counted[1].text == "[Up] Root > ... > Drafts +2");
    b.placed = 0;
    // Too narrow for any folder crumb: Up and Root remain, and nothing clipped is a target.
    view.columns = 12; (void)slots::render(state, "inventory", view, entries, {}, {}, {}, b); view.map.settle();
    REQUIRE(view.map.at(1, 1)); CHECK(*view.map.at(1, 1) == slots::kUpControl);
    CHECK(view.map.at(1, 11) == nullptr);
}

TEST_CASE("inventory folders: new copies land where they were dropped, and a duplicate stays beside its source") {
    InventoryStory s(kOrganizer | 64);
    s.append(4, "Original");
    const auto shelf = s.make_folder("", "Shelf");
    const auto in_shelf = [&] {
        std::size_t n = 0;
        for (const auto& e : s.folders().entries) n += e.folder == shelf;
        return n;
    };
    // Sorting orders entries; folders stay first.
    s.click(s.source); s.key(input::scan::kS, input::mod::kCtrl);
    CHECK(s.row_of(s.source, "Shelf/") < s.row_of(s.source, " Original : "));
    // Enter picks up a copy; a click on a folder row stores the copy in that folder.
    s.click(s.source, s.row_of(s.source, " Original : "));
    s.key(input::scan::kReturn);
    s.click(s.source, s.row_of(s.source, "Shelf/"));
    CHECK_MESSAGE(in_shelf() == 1, s.shown(s.source));
    s.key(input::scan::kEscape); // keeps the generated name
    CHECK(s.member_of("Original").empty());
    // A field copied from Info lands in the folder Inventory is showing.
    s.drag(s.row_of(s.source, " Original : "));
    REQUIRE_MESSAGE(s.shown(s.info).find("count: 4") != std::string::npos, s.shown(s.info));
    s.click(s.source, s.row_of(s.source, "Shelf/")); s.key(input::scan::kReturn);
    REQUIRE(s.row_of(s.source, "Root > Shelf") == 1);
    const auto field = s.row_of(s.info, "count:");
    auto press = s.button_at(s.info, field, 3, true), release = s.button_at(s.source, s.row_of(s.source, "Root > Shelf") + 1, 3, false);
    auto move = release; move.kind = "PointerMoved"; move.dx = release.x - press.x; move.dy = release.y - press.y;
    s.batch({press, move, release});
    CHECK_MESSAGE(in_shelf() == 2, (s.shown(s.source) + s.shown(s.info)));
    s.key(input::scan::kEscape);
    // A duplicate is filed beside its source.
    s.menu(s.source, s.row_of(s.source, "Root > Shelf") + 1, 3);
    CHECK_MESSAGE(in_shelf() == 3, s.shown(s.source));
    CHECK(s.member_of("Original").empty());
}

TEST_CASE("inventory folders: a shown folder that moves is followed and one that goes falls back to its nearest ancestor") {
    InventoryStory s(kOrganizer);
    const auto outer = s.make_folder("", "Outer");
    const auto inner = s.make_folder(outer, "Inner");
    s.click(s.source, s.row_of(s.source, "Outer/")); s.key(input::scan::kReturn);
    s.click(s.source, s.row_of(s.source, "Inner/")); s.key(input::scan::kReturn);
    REQUIRE(s.row_of(s.source, "Root > Outer > Inner") == 1);
    // Another actor moves the folder being shown: the view follows it by identity and says so.
    const auto owner = s.folders().owner;
    s.r.bus.send_to_role(inv::kInventoryRole, loom::Message(loom::to_value(inv::InventoryFolderMove{{owner, inner}, 1, {owner, ""}})));
    s.r.bus.drain_until_idle();
    CHECK_MESSAGE(s.row_of(s.source, "[Up] Root > Inner") == 1, s.shown(s.source));
    CHECK(s.shown(s.source).find("This folder moved") != std::string::npos);
    // Moved back under Outer and then removed: the view falls back to Outer, never to a stranger.
    s.r.bus.send_to_role(inv::kInventoryRole, loom::Message(loom::to_value(inv::InventoryFolderMove{{owner, inner}, 2, {owner, outer}})));
    s.r.bus.drain_until_idle();
    REQUIRE(s.row_of(s.source, "Root > Outer > Inner") == 1);
    s.r.bus.send_to_role(inv::kInventoryRole, loom::Message(loom::to_value(inv::InventoryFolderRemove{{owner, inner}, 3})));
    s.r.bus.drain_until_idle();
    CHECK_MESSAGE(s.row_of(s.source, "[Up] Root > Outer") == 1, s.shown(s.source));
    CHECK(s.shown(s.source).find("is gone; now at Root > Outer") != std::string::npos);
    CHECK(s.row_of(s.source, "Inner/") < 0);
}

TEST_CASE("inventory folders: a folder menu choice that waits for Delete takes the keyboard from the pane that had it, and Open leaves the keys alone") {
    InventoryStory s(kOrganizer);
    s.append(2, "Kept");
    (void)s.make_folder("", "Empty");
    const auto full = s.make_folder("", "Full");
    const auto owner = s.folders().owner;
    s.r.bus.send_to_role(inv::kInventoryRole, loom::Message(loom::to_value(inv::InventoryFile{
        s.entry("Kept").reference, {owner, ""}, {owner, full}})));
    s.r.bus.drain_until_idle();
    // Open is navigation and waits for no key: the keys stay where the maker put them, as a Files
    // or Builder menu choice that opens no edit leaves them.
    focus_info(s);
    choose(s, s.source, s.row_of(s.source, "Full/"), "Open folder");
    CHECK_MESSAGE(s.row_of(s.source, "[Up] Root > Full") == 1, s.shown(s.source));
    CHECK(s.r.session().panels.keyboard == s.info);
    s.click_at(s.source, 1, s.column_of(s.source, 1, "[Up]"));
    REQUIRE(s.row_of(s.source, "(Up) Root") == 1);
    // A folder that still holds an entry refuses at once and begins nothing, so the keys stay in
    // Info: the Delete that follows is Info's, and the folder and its member stay.
    focus_info(s);
    choose(s, s.source, s.row_of(s.source, "Full/"), "Remove empty folder");
    CHECK_MESSAGE(s.shown(s.source).find("still holds 1 item") != std::string::npos, s.shown(s.source));
    CHECK(s.r.session().panels.keyboard == s.info);
    s.key(input::scan::kDelete);
    CHECK(s.folders().folders.size() == 2);
    CHECK(s.member_of("Kept") == full);
    // The empty folder's removal waits for Delete, so the choice takes the keys and Delete removes it.
    choose(s, s.source, s.row_of(s.source, "Empty/"), "Remove empty folder");
    CHECK(s.r.session().panels.keyboard == s.source);
    CHECK_MESSAGE(s.shown(s.source).find("remove the empty folder 'Empty'") != std::string::npos, s.shown(s.source));
    s.key(input::scan::kDelete);
    REQUIRE(s.folders().folders.size() == 1);
    CHECK(s.folders().folders.front().folder.folder == full);
    CHECK_MESSAGE(s.shown(s.source).find("Removed empty folder 'Empty'") != std::string::npos, s.shown(s.source));
}

TEST_CASE("inventory folders: Move from a folder's menu takes the keyboard, so Escape cancels and the ordinary keys place it") {
    InventoryStory s(kOrganizer);
    const auto dest = s.make_folder("", "Dest");
    const auto tools = s.make_folder("", "Tools");
    const auto parent_of_tools = [&] {
        for (const auto& f : s.folders().folders) if (f.folder.folder == tools) return f.parent;
        FAIL("no Tools"); return std::string{};
    };
    // The pick takes the keys the menu left in Info, so Escape reaches Inventory.
    focus_info(s);
    choose(s, s.source, s.row_of(s.source, "Tools/"), "Move to another folder...");
    CHECK(s.r.session().panels.keyboard == s.source);
    CHECK_MESSAGE(s.row_of(s.source, "Tools/  (empty) [moving]") >= 0, s.shown(s.source));
    s.key(input::scan::kEscape);
    CHECK(s.shown(s.source).find("[moving]") == std::string::npos);
    CHECK(s.shown(s.source).find("Move cancelled; nothing changed") != std::string::npos);
    CHECK(parent_of_tools().empty());
    // Picked again, the ordinary keys finish it: Up selects Dest, Enter opens it, Ctrl+V moves.
    focus_info(s);
    choose(s, s.source, s.row_of(s.source, "Tools/"), "Move to another folder...");
    s.key(input::scan::kUp); s.key(input::scan::kReturn);
    REQUIRE_MESSAGE(s.row_of(s.source, "[Up] [Move here] Root > Dest") == 1, s.shown(s.source));
    s.key(input::scan::kV, input::mod::kCtrl);
    CHECK_MESSAGE(parent_of_tools() == dest, s.shown(s.source));
    CHECK(s.shown(s.source).find("Moved 'Tools' into Root > Dest") != std::string::npos);
}

TEST_CASE("inventory folders: Move from an entry's menu takes the keyboard, so Escape cancels and Ctrl+V files it") {
    InventoryStory s(kOrganizer);
    s.append(3, "Loose");
    const auto dest = s.make_folder("", "Dest");
    const auto before = s.entry("Loose");
    focus_info(s);
    choose(s, s.source, s.row_of(s.source, " Loose : "), "Move to another folder...");
    CHECK(s.r.session().panels.keyboard == s.source);
    CHECK_MESSAGE(s.shown(s.source).find(" Loose : story.RuntimeItem [moving]") != std::string::npos, s.shown(s.source));
    s.key(input::scan::kEscape);
    CHECK(s.shown(s.source).find("[moving]") == std::string::npos);
    CHECK(s.shown(s.source).find("Move cancelled; nothing changed") != std::string::npos);
    CHECK(s.member_of("Loose").empty());
    // Picked again, the keys finish it: select Dest, open it, Ctrl+V. The same entry is filed.
    focus_info(s);
    choose(s, s.source, s.row_of(s.source, " Loose : "), "Move to another folder...");
    for (int n = 0; n < 8 && s.row_of(s.source, "> Dest/") < 0; ++n) s.key(input::scan::kUp);
    s.key(input::scan::kReturn);
    REQUIRE_MESSAGE(s.row_of(s.source, "[Up] [Move here] Root > Dest") == 1, s.shown(s.source));
    s.key(input::scan::kV, input::mod::kCtrl);
    CHECK_MESSAGE(s.member_of("Loose") == dest, s.shown(s.source));
    const auto after = s.entry("Loose");
    CHECK(slots::same(after.reference, before.reference));
    CHECK(after.revision == before.revision);
}

TEST_CASE("inventory folders: Move from a portable view's menu takes that view's keyboard, so Escape cancels there and main Inventory places it") {
    InventoryStory s(kOrganizer);
    s.append(4, "Placed");
    const auto dest = s.make_folder("", "Dest");
    const auto placed = s.entry("Placed");
    const auto row = s.create("row", placed.reference);
    place(s, row);
    const auto row_kind = s.r.session().panels.runtime.find(slots::kRole, row)->kind;
    // The keys go to the view the menu was about, and Escape cancels there. (The pane's one notice
    // is read in main Inventory: this row's room has no line left for it.)
    focus_info(s);
    choose(s, row_kind, 3, "Move to another folder...");
    CHECK(s.r.session().panels.keyboard == row_kind);
    CHECK_MESSAGE(s.shown(s.source).find("Moving 'Placed'") != std::string::npos, s.shown(s.source));
    s.key(input::scan::kEscape);
    CHECK_MESSAGE(s.shown(s.source).find("Move cancelled; nothing changed") != std::string::npos, s.shown(s.source));
    s.click(s.source, s.row_of(s.source, "Dest/")); s.click(s.source, s.row_of(s.source, "Dest/"));
    REQUIRE_MESSAGE(s.row_of(s.source, "[Up] Root > Dest") == 1, s.shown(s.source));
    s.key(input::scan::kV, input::mod::kCtrl);
    CHECK(s.shown(s.source).find("Pick an entry or folder to move first") != std::string::npos);
    CHECK(s.member_of("Placed").empty());
    // Picked again and placed from main Inventory: filed, and its tile stays in the row.
    s.key(input::scan::kBackspace);
    focus_info(s);
    choose(s, row_kind, 3, "Move to another folder...");
    s.click(s.source, s.row_of(s.source, "Dest/")); s.click(s.source, s.row_of(s.source, "Dest/"));
    s.key(input::scan::kV, input::mod::kCtrl);
    CHECK_MESSAGE(s.member_of("Placed") == dest, s.shown(s.source));
    CHECK(slots::placed(s.layout(), placed.reference) == row);
}

TEST_CASE("inventory folders: a Move choice overtaken by a newer act before its keyboard request reaches Workshop leaves the keys where the maker has them") {
    InventoryStory s(kOrganizer);
    (void)s.make_folder("", "Dest");
    (void)s.make_folder("", "Tools");
    focus_info(s);
    // Every keyboard request that reaches Workshop is counted, so the outcome below is the host's
    // guard judging the pane's request, never a request the pane did not make.
    Deliveries asked(s.r.bus, PaneKeyboardRequested::zen_name);
    point_menu_at(s, s.source, s.row_of(s.source, "Tools/"), "Move to another folder...");
    // One reader batch: Return chooses, then a press in Info, both handled by Workshop before the
    // presenter's answer and the pane's keyboard request can arrive (the batching guide's case).
    s.batch({s.key_down(input::scan::kReturn), s.button_at(s.info, 0, true), s.button_at(s.info, 0, false)});
    CHECK_FALSE(s.r.session().presented.open);
    CHECK(asked.count == 1);                        // the pane asked, continuing its choice,
    CHECK(s.r.session().panels.keyboard == s.info); // and the newer act defeated the request
    CHECK_MESSAGE(s.row_of(s.source, "Tools/  (empty) [moving]") >= 0, s.shown(s.source)); // the pick stands
    // Escape is Info's now; the pick waits for Inventory's own keys, and a click there cancels it.
    s.key(input::scan::kEscape);
    CHECK(s.row_of(s.source, "[moving]") >= 0);
    s.click(s.source); s.key(input::scan::kEscape);
    CHECK_MESSAGE(s.row_of(s.source, "[moving]") < 0, s.shown(s.source));
}

TEST_CASE("inventory folders: a drag keeps the folder it was picked up from while its read waits, so a filing meanwhile refuses it") {
    HeldStory s;
    const auto owner = s.owner->listing.owner;
    // Item and folder B at Root; A inside B.
    s.with_owner([&](HeldOwner& o, loom::Mail& m) {
        o.listing.folders = {{{owner, "b"}, 1, "B", ""}, {{owner, "a"}, 1, "A", "b"}};
        o.listing.entries = {{{owner, "item"}, 1, "Item", "story.RuntimeItem", 1, false, ""}};
        o.pairs["item"] = s.pair(5);
        o.changed(m);
    });
    REQUIRE_MESSAGE(s.row_of(s.source, " Item : ") >= 0, s.shown(s.source));
    // A pickup the owner refuses settles in its words and carries nothing.
    s.event(s.button_at(s.source, s.row_of(s.source, " Item : "), true));
    s.with_owner([](HeldOwner& o, loom::Mail& m) { o.answer_read(m, "the scripted owner refused the read"); });
    CHECK(s.shown(s.source).find("the scripted owner refused the read") != std::string::npos);
    s.event(s.button_at(s.source, s.row_of(s.source, " Item : "), false));
    CHECK(s.owner->filed.empty());
    // 1. The press picks Item up in Root, and the owner holds the read.
    auto press = s.button_at(s.source, s.row_of(s.source, " Item : "), true);
    s.event(press);
    REQUIRE(s.owner->held.size() == 1);
    // 2. Another actor files Item in A, and Inventory lists again before the read answers.
    s.with_owner([](HeldOwner& o, loom::Mail& m) {
        o.entry({o.listing.owner, "item"})->folder = "a"; ++o.listing.revision; o.changed(m);
    });
    REQUIRE_MESSAGE(s.row_of(s.source, " Item : ") < 0, s.shown(s.source));
    // 3. The read answers; the drag finishes on B.
    s.with_owner([](HeldOwner& o, loom::Mail& m) { o.answer_read(m); });
    auto release = s.button_at(s.source, s.row_of(s.source, "B/"), false);
    auto move = release; move.kind = "PointerMoved"; move.dx = release.x - press.x; move.dy = release.y - press.y;
    s.batch({move, release});
    // 4. The filing names Root, where the pickup began; the owner refuses, and Item stays in A.
    REQUIRE_MESSAGE(s.owner->filed.size() == 1, s.shown(s.source));
    CHECK(s.owner->filed.front().from.owner == owner);
    CHECK(s.owner->filed.front().from.folder.empty());
    CHECK(s.owner->entry({owner, "item"})->folder == "a");
    CHECK_MESSAGE(s.shown(s.source).find("no longer in that folder") != std::string::npos, s.shown(s.source));
    CHECK(s.shown(s.source).find("Filed") == std::string::npos);
    // 5. A fresh drag from A, where Item is now, onto its parent B succeeds.
    s.owner->hold_reads = false;
    s.click(s.source, s.row_of(s.source, "B/")); s.click(s.source, s.row_of(s.source, "B/"));
    s.click(s.source, s.row_of(s.source, "A/")); s.click(s.source, s.row_of(s.source, "A/"));
    REQUIRE_MESSAGE(s.row_of(s.source, "[Up] Root > B > A") == 1, s.shown(s.source));
    s.drag_to(s.source, s.row_of(s.source, " Item : "), s.source, 1, s.column_of(s.source, 1, "[Up]"));
    REQUIRE(s.owner->filed.size() == 2);
    CHECK(s.owner->filed.back().from.folder == "a");
    CHECK(s.owner->entry({owner, "item"})->folder == "b");
    CHECK_MESSAGE(s.shown(s.source).find("Filed 'Item' in Root > B") != std::string::npos, s.shown(s.source));
}
