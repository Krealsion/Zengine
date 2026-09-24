// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_TESTS_EDITOR_TRANSFER_STORY_HPP
#define ZENGINE_TESTS_EDITOR_TRANSFER_STORY_HPP

// THE EDITOR IN WORKSHOP'S TYPED CARRY, AS A RIG: a real loaded Workshop whose Editor office is
// held by the image a case names, with Inventory, its pane, the menu presenter, the opening manager
// and an input actor whose Loom authority each case chooses, driven through the real Input weave.
// Shared by the standard Editor's transfer cases (`panes`) and the Neovim-backed Editor's
// (`workshop_neovim`); what reads one implementation's own state stays in its suite.

#include "inventory_story.hpp"

#include "editor-pane/vocabulary.hpp"
#include "message-draft/transfer.hpp"
#include "source-transfer/vocabulary.hpp"
#include "timer/vocabulary.hpp"
#include "workshop/editor_handoff_vocabulary.hpp"
#include "workshop/editor_switch_vocabulary.hpp"
#include "workshop/pane_doors.hpp"

#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace editor_transfer_story {

using namespace inventory_story;
namespace st = zengine::source_transfer;
namespace ed = zengine::editor_pane;
namespace md = zengine::message_draft;


/// The actor's grants, as bits (the Inventory story's own numbering, plus opening a source).
inline constexpr int kLocate = 1, kRead = 2, kStore = 8, kCarry = 64, kViews = 128, kOpen = 8192;
inline constexpr int kEverything = kLocate | kRead | 4 | kStore | 16 | 32 | kCarry | kViews | kOpen;

inline PaneRef editor_ref() { return PaneRef{ed::kEditorPaneRole, ed::kEditorPane}; }

/// A PARTY THAT HOLDS THE SWITCH'S OFFICE, only to ask the Editor the boundary's first question.
struct SwitchAskerState { ZEN_SHAPE(SwitchAskerState, 1); };
class SwitchAsker : public loom::WeaveBase<SwitchAsker, SwitchAskerState,
                                           loom::Accept<SeatDo, EditorHandoffJudged>,
                                           loom::Emit<EditorHandoffJudgeRequested>> {
public:
    std::vector<EditorHandoffJudged> judged;
    void on(const SeatDo&, loom::Mail& mail) {
        (void)mail.as_role(kEditorSwitchRole)
            .send_to_role(ed::kEditorPaneRole, EditorHandoffJudgeRequested{++asks}, 7);
    }
    void on(const EditorHandoffJudged& j, loom::Mail&) { judged.push_back(j); }
    std::int64_t asks = 0;
};

struct TransferStory {
    TempDir dir;
    std::filesystem::path root;
    std::string marks;
    PaneRig r;
    InventoryHand* hand = nullptr;
    loom::WeaveId hand_id;
    DoorAsker* asker = nullptr;
    std::int64_t editor = 0, inventory = 0;
    std::shared_ptr<std::vector<QuietReader::Event>> physical =
        std::make_shared<std::vector<QuietReader::Event>>();

    /// `stem` is the image that holds the Editor's office: the standard Editor, or another
    /// implementation of it (the Neovim-backed Editor's suite passes its own).
    explicit TransferStory(const char* tag, int permissions = kEverything, std::string project = {},
                           std::string stem = ed::kEditorPaneStem)
        : dir(tag) {
        root = dir.path();
        marks = (root / "workshop-marks.json").generic_string();
        r.host.project_dir = project.empty() ? root.generic_string() : project;
        r.host.managed_pane = editor_ref();
        r.mount_workshop();
        r.mount_opening();
        auto door = std::make_unique<ProjectDoor>(r.host.project_dir, marks);
        ProjectDoor* raw = door.get();
        loom::Grant say;
        say.allow_to_any(ProjectRoot::zen_name, ProjectRoot::zen_version);
        raw->zen_set_self(r.bus.register_weave(std::move(door), std::move(say), std::string(kProjectRole)));
        r.host.input_authority = [&](loom::WeaveId actor) {
            return r.bus.alive(actor) ? loom::host_grant_authority(r.bus, actor, loom::LiveAuthority::nothing())
                                      : loom::GrantAuthority{};
        };
        load::LoadPlan plan;
        for (const auto& [image, role] : std::vector<std::pair<std::string, std::string>>{
                 {stem, ed::kEditorPaneRole}, {"zengine-inventory", inv::kInventoryRole},
                 {"zengine-inventory-pane", "zengine.inventory-pane"},
                 {"zengine-menu-presenter", kPresenterRole}}) {
            load::ArtifactIntent artifact;
            artifact.stem = image;
            artifact.weave = load::WeaveIntent{role};
            plan.artifacts.push_back(artifact);
        }
        const auto done = r.run_plan(plan);
        REQUIRE_MESSAGE(done.ok, done.refusal);
        r.ready();
        r.extent(180, 60);
        r.pick(editor_ref());
        r.pick({"zengine.inventory-pane", "inventory"});
        editor = r.session().panels.runtime.find(ed::kEditorPaneRole, ed::kEditorPane)->kind;
        inventory = r.session().panels.runtime.find("zengine.inventory-pane", "inventory")->kind;
        using Input = input::InputWeaveT<QuietReader>;
        auto reader = std::make_unique<Input>(QuietReader{physical});
        auto* reader_ptr = reader.get();
        auto grant = loom::emit_default_grant(*reader);
        reader_ptr->zen_set_self(r.bus.register_weave(std::move(reader), grant, input::kInputRole));
        auto actor = std::make_unique<InventoryHand>();
        hand = actor.get();
        loom::Grant g;
        g.allow_to_role(input::InputSessionRequested::zen_name, 1, input::kInputRole);
        g.allow_to_role(input::InjectInput::zen_name, 1, input::kInputRole);
        g.allow_to_role(inv::InventoryList::zen_name, 1, inv::kInventoryRole);
        g.allow_to_role(inv::v2::InventoryList::zen_name, 2, inv::kInventoryRole);
        if (permissions & kLocate) g.allow_to_role(inv::InventoryLocate::zen_name, 1, inv::kInventoryRole);
        if (permissions & kRead) g.allow_to_role(inv::InventoryRead::zen_name, 1, inv::kInventoryRole);
        if (permissions & kStore) {
            g.allow_to_role(inv::InventoryAdd::zen_name, 1, inv::kInventoryRole);
            g.allow_to_role(inv::v2::InventoryAdd::zen_name, 2, inv::kInventoryRole);
        }
        if (permissions & 4) g.allow_to_role(inv::InventoryWrite::zen_name, 1, inv::kInventoryRole);
        if (permissions & 16) g.allow_to_role(inv::InventoryRename::zen_name, 1, inv::kInventoryRole);
        if (permissions & 32) g.allow_to_role(inv::InventoryRemove::zen_name, 1, inv::kInventoryRole);
        if (permissions & kCarry) g.allow_to_role(PaneValueCarryRequested::zen_name, 1, "zengine.workshop");
        if (permissions & kViews) g.allow_to_role(zengine::inventory_pane::InventoryViewEdit::zen_name, 1, "zengine.inventory-pane");
        if (permissions & kOpen) g.allow_to_role(OpenSourceRequested::zen_name, 1, kOpeningRole);
        hand_id = r.bus.register_weave(std::move(actor), g);
        hand->zen_set_self(hand_id);
        act([](loom::Mail& m) { m.send_to_role(input::kInputRole, input::InputSessionRequested{"editor transfers"}); });
        REQUIRE(hand->session > 0);
        auto held = std::make_unique<DoorAsker>(std::string(kDoorAskerOffice));
        asker = held.get();
        loom::Grant ask;
        ask.allow_to_any(OpenSourceRequested::zen_name, OpenSourceRequested::zen_version);
        asker->id = r.bus.register_weave(std::move(held), std::move(ask), std::string(kDoorAskerOffice));
        asker->zen_set_self(asker->id);
    }

    // ---- the hand ---------------------------------------------------------------------------
    void act(std::function<void(loom::Mail&)> action) {
        hand->next = std::move(action);
        r.bus.send(hand_id, loom::Message(loom::to_value(InventoryHandDo{})));
        r.bus.drain_until_idle();
        hand->next = {};
    }
    void batch(std::vector<input::InjectedEvent> events) {
        act([&](loom::Mail& m) { m.send_to_role(input::kInputRole, input::InjectInput{hand->session, events}); });
    }
    input::InjectedEvent at(std::int64_t kind, std::int64_t row, std::int64_t column, const char* what,
                            bool down = true, std::int64_t button = 1) {
        const auto rect = external_body_rect(r.session(), kind);
        input::InjectedEvent e;
        e.kind = what;
        e.button = button;
        e.pressed = down;
        e.space = input::space::kCells;
        e.x = rect.x + column;
        e.y = rect.y + row + surface::kTuiCanvasTopRow +
              external_title_rows(r.session().panels, kind, r.session().pane_titles);
        return e;
    }
    void click(std::int64_t kind, std::int64_t row, std::int64_t column, std::int64_t button = 1) {
        batch({at(kind, row, column, "PointerButton", true, button), at(kind, row, column, "PointerButton", false, button)});
    }
    void key(std::int64_t scan, std::int64_t mods = input::mod::kNone) {
        input::InjectedEvent e;
        e.kind = "KeyPressed";
        e.scancode = scan;
        e.modifiers = mods;
        input::InjectedEvent up = e;
        up.kind = "KeyReleased";
        batch({e, up});
    }
    void text(const std::string& t) {
        input::InjectedEvent e;
        e.kind = "TextEntered";
        e.text = t;
        batch({e});
    }
    /// One primary drag in one batch: press, a motion to the release point, the release.
    void drag(std::int64_t from, std::int64_t from_row, std::int64_t from_col, std::int64_t to, std::int64_t to_row,
              std::int64_t to_col) {
        auto press = at(from, from_row, from_col, "PointerButton", true);
        auto release = at(to, to_row, to_col, "PointerButton", false);
        auto move = release;
        move.kind = "PointerMoved";
        move.dx = release.x - press.x;
        move.dy = release.y - press.y;
        batch({press, move, release});
    }

    // ---- the Editor ---------------------------------------------------------------------------
    std::string write(const std::string& rel, const std::string& bytes, const std::filesystem::path& base = {}) {
        const std::filesystem::path p = (base.empty() ? root : base) / rel;
        std::filesystem::create_directories(p.parent_path());
        std::ofstream(p, std::ios::binary) << bytes;
        return p.lexically_normal().generic_string();
    }
    SourceOpened open(const std::string& path) {
        const std::size_t before = asker->opens.size();
        asker->next = [path](DoorAsker& a, loom::Mail& mail) { a.ask(mail, kOpeningRole, OpenSourceRequested{path}); };
        r.bus.send(asker->id, loom::Message(loom::to_value(SeatDo{}), loom::WeaveId{}, loom::WeaveId{}, 0));
        r.bus.drain_until_idle();
        REQUIRE(asker->opens.size() == before + 1);
        if (!asker->opens.back().accepted) MESSAGE("the open was refused: " << asker->opens.back().refusal);
        return asker->opens.back();
    }
    std::vector<std::string> rows(std::int64_t kind) { return pane_rows(r, kind); }

    // ---- Inventory ------------------------------------------------------------------------------
    std::string make_folder(const std::string& name) {
        act([](loom::Mail& m) { m.send_to_role(inv::kInventoryRole, inv::v2::InventoryList{}); });
        const std::string owner = hand->organized.owner;
        r.bus.send_to_role(inv::kInventoryRole,
                           loom::Message(loom::to_value(inv::InventoryFolderCreate{{owner, ""}, name})));
        r.bus.drain_until_idle();
        for (const auto& f : listed().folders) if (f.name == name) return f.folder.folder;
        FAIL("no folder " << name);
        return {};
    }
    inv::v2::InventoryListed listed() {
        act([](loom::Mail& m) { m.send_to_role(inv::kInventoryRole, inv::v2::InventoryList{}); });
        return hand->organized;
    }
    /// ANSWER INVENTORY'S NAME LINE for a copy just placed there: the name, then Enter.
    void name(const std::string& label) {
        text(label);
        key(input::scan::kReturn);
    }
    void add(const std::string& pair, const std::string& label) {
        r.bus.send_to_role(inv::kInventoryRole, loom::Message(loom::to_value(inv::InventoryAdd{
                                                    loom::Bytes(pair.begin(), pair.end()), label})));
        r.bus.drain_until_idle();
    }
    /// Every stored pair, with its label and folder, from the owner's own state.
    struct Stored {
        std::string label, folder;
        inv::DecodedPair pair;
    };
    std::vector<Stored> stored() {
        std::vector<Stored> out;
        const auto state = r.bus.weave(r.bus.role_holder(inv::kInventoryRole))->snapshot();
        for (const auto& cell : state.get("entries")->as_list()) {
            const auto& e = *cell.as_message();
            const auto& bytes = e.get("pair")->as_bytes();
            out.push_back(Stored{e.get("label")->as_text(), e.get("folder")->as_text(),
                                 inv::decode_pair({reinterpret_cast<const char*>(bytes.data()), bytes.size()})});
        }
        return out;
    }
    std::int64_t row_of(std::int64_t kind, const std::string& text) {
        const auto rs = rows(kind);
        for (std::size_t i = 0; i < rs.size(); ++i) if (rs[i].find(text) != std::string::npos) return static_cast<std::int64_t>(i);
        std::string all;
        for (const auto& row : rs) all += "  | " + row + "\n";
        FAIL_CHECK("no row reads " << text << " (pane on the desk: " << r.session().panels.has(kind)
                                   << ", notice: " << r.last_notice() << "); the rows are:\n" << all);
        return -1;
    }
};

inline std::string text_pair(const std::string& text) {
    return zengine::inventory::encode_pair(loom::to_value(st::SourceText{text}), {});
}

inline zengine::timer::EnsureTimer beat() {
    zengine::timer::EnsureTimer t;
    t.id = "editor-materials.beat";
    t.delay_ms = 250;
    t.repeat = true;
    t.preferred = "keep-remaining";
    t.fallback = "restart";
    return t;
}

inline std::string command_pair() {
    TerminalCaptureFacts facts;
    facts.participant = 3;
    facts.observation = 9;
    facts.kind = "submitted";
    facts.addressing = "role";
    facts.role = "zengine.timer";
    return zengine::inventory::encode_pair(loom::to_value(beat()), {loom::to_value(facts)});
}

} // namespace editor_transfer_story

#endif // ZENGINE_TESTS_EDITOR_TRANSFER_STORY_HPP
