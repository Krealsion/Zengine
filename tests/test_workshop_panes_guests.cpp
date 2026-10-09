// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Workshop panes suite -- a guest's hand on the desk: what the keys, text and presses of a
// participant a guest door admitted reach on a weaver's host and on a development host, through
// the real Workshop and the real panes. A story's own injected hand is made a guest by the host
// seam (`guest_row`, `host_fact`); a rig with no hand of its own takes `guest_hand.hpp`'s. Each
// refusal is read in words and by the absence of its effect, beside the act that succeeds.

#include "doctest.h"

#include "editor_transfer_story.hpp"
#include "guest_hand.hpp"

#include "desktop-pane/vocabulary.hpp"
#include "inventory-pane/vocabulary.hpp"
#include "terminal-pane/vocabulary.hpp"
#include "view-builder/vocabulary.hpp"
#include "workshop/actor_scope.hpp"
#include "workshop/editor_switch.hpp"
#include "workshop/editor_switch_vocabulary.hpp"
#include "workshop/pane_operation.hpp"
#include "workshop/terminal_seam_vocabulary.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

using namespace editor_transfer_story;
namespace dp = zengine::desktop_pane;
namespace tp = zengine::terminal_pane;
namespace vb = zengine::view_builder;

/// THE WORDS EACH REFUSAL NAMES ITS RULE IN, as `workshop/actor_scope.hpp` says them.
constexpr const char* kTextRefused = "a guest's typed text rests in no pane";
constexpr const char* kWriteRefused = "only the weaver's hand writes a file here";
constexpr const char* kEditorRefused = "the editor answers only the weaver's hand";
constexpr const char* kTerminalRefused = "the Terminal answers only the weaver's hand";
constexpr const char* kHotkeysRefused =
    "the Hotkeys pane, which edits the keymap file, answers only the weaver's hand";

bool holds_words(const std::string& said, const char* part) {
    return said.find(part) != std::string::npos;
}

/// THE HOST SEAM ANSWERING ONE PARTICIPANT AS A ROW A GUEST DOOR ADMITTED, on a weaver's host or
/// a development host, as the door answers a session it admitted. Every other participant is
/// answered as no guest, so the weaver's own hand is never narrowed.
scope::GuestRowFacts admit_as_guest(PaneRig& r, loom::WeaveId who, bool development,
                                    std::vector<std::string> powers = {"input"}) {
    scope::GuestRowFacts row;
    row.admitted = true;
    row.name = "agent";
    row.powers = std::move(powers);
    row.version = 2;
    r.host.host_fact.development = development;
    r.host.guest_row = [who, row](loom::WeaveId asked) {
        return asked == who ? row : scope::GuestRowFacts{};
    };
    return row;
}

/// ...AND ONE MORE PARTICIPANT ANSWERED AS THE SAME ADMITTED ROW: a guest's session is one row
/// whichever of its doors it speaks through.
void answer_as_guest_too(PaneRig& r, loom::WeaveId who, const scope::GuestRowFacts& row) {
    const auto before = r.host.guest_row;
    r.host.guest_row = [before, who, row](loom::WeaveId asked) {
        return asked == who ? row : before(asked);
    };
}

/// WHAT WORKSHOP SAID OF ONE ACT: the notice is blanked first, so the words read after it are
/// that act's own and never a sentence an earlier act left standing.
template <class Act>
std::string said_for(PaneRig& r, Act&& act) {
    r.session().notice.clear();
    act();
    return r.last_notice();
}

/// EVERY RUN A CANVAS PANE DREW, a line each: the pane's own words, read where it drew them.
std::string picture_text(PaneRig& r, std::int64_t kind) {
    const ExternalPane* pane = r.session().panes.external_pane(kind);
    REQUIRE(pane != nullptr);
    std::string all;
    for (const auto& run : pane->canvas.content.texts) all += run.text + "\n";
    return all;
}

bool pictured(PaneRig& r, std::int64_t kind, const std::string& part) {
    return picture_text(r, kind).find(part) != std::string::npos;
}

/// THE CANVAS CELL `into` CELLS PAST THE FIRST OF A RUN A CANVAS PANE DREW BEGINNING `start`.
std::pair<std::int64_t, std::int64_t> cell_of_run(PaneRig& r, std::int64_t kind,
                                                  const std::string& start, std::int64_t into) {
    const ExternalPane* pane = r.session().panes.external_pane(kind);
    REQUIRE(pane != nullptr);
    const auto& c = pane->canvas;
    for (const auto& run : c.content.texts) {
        if (run.text.rfind(start, 0) == 0) {
            return {(c.x + run.x) / surface::kCanvasCellPx + into,
                    (c.y + run.y) / surface::kCanvasCellPx};
        }
    }
    FAIL("no run beginning `" << start << "` in\n" << picture_text(r, kind));
    return {0, 0};
}

/// A CELL OF THE ROOM NO PANE STANDS ON, found by the walk a press spends: a press there hands
/// the keys back to the room.
std::pair<std::int64_t, std::int64_t> free_room_cell(PaneRig& r) {
    const Screen sc = screen_of(r.session());
    const ScreenCells cells = cells_of(sc);
    for (std::int64_t y = cells.room_h - 1; y >= 0; --y) {
        for (std::int64_t x = cells.room_w - 1; x >= 0; --x) {
            if (!occupied_at(r.session().panes, r.session().setup.active, sc, x, y).occupied) {
                return {x, y};
            }
        }
    }
    FAIL("this screen has no empty room to press");
    return {0, 0};
}

/// THE WEAVER'S OWN HAND IN A STORY'S RIG: the platform's moments queued on the story's reader
/// and pumped, so Workshop hears them attributed to the local hand and never to the guest's.
struct Weaver {
    PaneRig& r;
    std::shared_ptr<std::vector<QuietReader::Event>> physical;

    void pump() {
        r.bus.send_to_role(input::kInputRole, loom::Message(loom::to_value(input::PumpInput{})));
        r.bus.drain_until_idle();
    }
    void key(std::int64_t scan, std::int64_t mods = input::mod::kNone) {
        physical->push_back(input::KeyPressed{scan, "", mods});
        pump();
    }
    void text(const std::string& typed) {
        physical->push_back(input::TextEntered{typed});
        pump();
    }
    /// A primary press and its release at a canvas cell, as the terminal medium reports them.
    void click_cell(std::int64_t x, std::int64_t y) {
        for (const bool down : {true, false}) {
            physical->push_back(input::PointerButton{1, down, x, y + surface::kTuiCanvasTopRow,
                                                     input::space::kCells, input::mod::kNone});
        }
        pump();
    }
    /// ...at column `column` of an external pane's body row `row`, the place the story's hand
    /// would aim at.
    void click(std::int64_t kind, std::int64_t row, std::int64_t column = 1) {
        const ui::Rect body = external_body_rect(r.session(), kind);
        click_cell(body.x + pane_cell_of(r.session(), kind, row, column),
                   body.y + row +
                       external_title_rows(r.session().panes, kind, r.session().pane_titles));
    }
    void click_on(std::int64_t kind, const std::string& start, std::int64_t into = 1) {
        const auto [x, y] = cell_of_run(r, kind, start, into);
        click_cell(x, y);
    }
    void press_room() {
        const auto [x, y] = free_room_cell(r);
        click_cell(x, y);
    }
};

/// A PRESS AND ITS RELEASE BY THE STORY'S OWN HAND where a canvas pane drew a run beginning
/// `start`.
void guest_click_on(InventoryStory& s, std::int64_t kind, const std::string& start,
                    std::int64_t into = 1) {
    const auto [x, y] = cell_of_run(s.r, kind, start, into);
    input::InjectedEvent e;
    e.kind = "PointerButton";
    e.button = 1;
    e.pressed = true;
    e.space = input::space::kCells;
    e.x = x;
    e.y = y + surface::kTuiCanvasTopRow;
    s.event(e);
    e.pressed = false;
    s.event(e);
}

/// WORKSHOP'S ANSWERS TO THE ACTS A PANE ASKS IT ABOUT, as they are delivered: how many it
/// allowed, and the words of each it refused.
struct OperationAnswers {
    std::vector<std::string> refused;
    std::size_t allowed = 0;
};

loom::ObserverId watch_answers(PaneRig& r, OperationAnswers& into) {
    return r.bus.add_observer([&into](const loom::BusEvent& seen) {
        if (seen.kind != loom::EventKind::Delivered || seen.payload == nullptr ||
            seen.schema_name != PaneOperationAnswered::zen_name) {
            return;
        }
        const PaneOperationAnswered said = loom::from_value<PaneOperationAnswered>(*seen.payload);
        if (said.allowed) {
            ++into.allowed;
        } else {
            into.refused.push_back(said.reason);
        }
    });
}

// ---- the Editor, held by the standard Editor's image -----------------------------------------

ed::EditorPaneState editor_doc(TransferStory& s) {
    return loom::from_value<ed::EditorPaneState>(
        s.r.bus.weave(s.r.bus.role_holder(ed::kEditorPaneRole))->snapshot());
}

/// The Editor's rows above the document: the status row, and a notice row when one stands.
std::int64_t editor_chrome(TransferStory& s) { return editor_doc(s).notice.empty() ? 1 : 2; }

// ---- the Terminal, and the switch's office it can ask ----------------------------------------

struct GuestsSwitchOfficeState {
    ZEN_SHAPE(GuestsSwitchOfficeState, 1);
};

/// THE EDITOR SWITCH'S OFFICE, STOOD IN FOR: it keeps the destination of every switch asked of it
/// and answers none, so a line that reached it is counted and nothing switches.
class SwitchOffice : public loom::WeaveBase<SwitchOffice, GuestsSwitchOfficeState,
                                            loom::Accept<EditorSwitchRequested>> {
public:
    std::vector<std::string> asked;
    void on(const EditorSwitchRequested& wanted, loom::Mail&) { asked.push_back(wanted.destination); }
};

std::string switch_line(const std::string& destination) {
    return "ask @" + std::string(kEditorSwitchRole) + " EditorSwitchRequested 1 destination=" +
           destination;
}

/// A LIVE WORKSHOP WITH THE REAL TERMINAL PANE OPEN, its participant widened for the switch by
/// the host's own call, and the switch's office stood in for. No Input weave is mounted: a case
/// brings the guest's and the weaver's hands with `guest_hand::GuestHand`.
struct TerminalDesk {
    PaneRig r;
    loom::TerminalSession* me = nullptr;
    SwitchOffice* office = nullptr;
    std::int64_t kind = 0;

    TerminalDesk() {
        r.mount_workshop();
        loom::TerminalVocabulary vocab;
        vocab.knows(loom::schema_of<surface::SurfaceText>())
            .accepts(loom::schema_of<loom::Ack>())
            .accepts(loom::schema_of<loom::Result>())
            .accepts(loom::schema_of<loom::Refused>());
        loom::Grant grant;
        grant.allow_to_role(surface::SurfaceText::zen_name, surface::SurfaceText::zen_version,
                            surface::kSkinRole);
        let_terminal_switch_editors(vocab, grant);
        const loom::MountedTerminal mounted = loom::host_mount_terminal(
            r.bus, std::make_unique<loom::TerminalSession>("workshop", std::move(vocab)),
            std::move(grant));
        r.host.terminal = mounted.session;
        r.terminal_id = mounted.id;
        me = mounted.session;
        auto stand_in = std::make_unique<SwitchOffice>();
        office = stand_in.get();
        const loom::WeaveId office_id =
            r.bus.register_weave(std::move(stand_in), loom::Grant{}, std::string(kEditorSwitchRole));
        office->zen_set_self(office_id);
        load::LoadPlan plan;
        load::ArtifactIntent seat;
        seat.stem = tp::kTerminalPaneStem;
        seat.weave = load::WeaveIntent{tp::kTerminalPaneRole};
        plan.artifacts.push_back(seat);
        const load::Executed done = r.run_plan(plan);
        REQUIRE_MESSAGE(done.ok, done.refusal);
        r.ready();
        r.extent(160, 48);
        const RuntimePane* row = r.session().panes.runtime.find(tp::kTerminalPaneRole, tp::kTerminalPane);
        REQUIRE_MESSAGE(row != nullptr, "the loaded image offered no `terminal` pane");
        kind = row->kind;
        r.pick(PaneRef{tp::kTerminalPaneRole, tp::kTerminalPane});
        REQUIRE(r.session().panes.has(kind));
    }

    std::string shown() {
        std::string all;
        for (const std::string& line : pane_rows(r, kind)) all += line + "\n";
        return all;
    }
    /// The participant's own record: what it authored, and what came back.
    std::vector<loom::TranscriptEntry> record() const { return me->transcript().entries(); }
};

// ---- Workshop's own quit, asked by message ---------------------------------------------------

constexpr const char* kQuitAskerOffice = "zengine.test.guest-quit";

struct GuestsQuitAskerState {
    ZEN_SHAPE(GuestsQuitAskerState, 1);
};

/// A PARTICIPANT ASKING WORKSHOP TO END, as `demo.py stop` asks it, keeping the answer it is given.
class QuitAsker : public loom::WeaveBase<QuitAsker, GuestsQuitAskerState,
                                         loom::Accept<loom::Ack, loom::Refused, SeatDo>,
                                         loom::Emit<WorkshopQuitRequested>> {
public:
    void on(const SeatDo&, loom::Mail& mail) {
        (void)mail.as_role(kQuitAskerOffice).send_to_role(kWorkshopProvider, WorkshopQuitRequested{});
    }
    void on(const loom::Ack&, loom::Mail&) { ++acks; }
    void on(const loom::Refused& said, loom::Mail&) { refusals.push_back(said.reason); }
    int acks = 0;
    std::vector<std::string> refusals;
};

QuitAsker* mount_quit_asker(PaneRig& r, loom::WeaveId& id) {
    auto made = std::make_unique<QuitAsker>();
    QuitAsker* asker = made.get();
    loom::Grant grant;
    grant.allow_to_role(WorkshopQuitRequested::zen_name, WorkshopQuitRequested::zen_version,
                        kWorkshopProvider);
    id = r.bus.register_weave(std::move(made), std::move(grant), std::string(kQuitAskerOffice));
    asker->zen_set_self(id);
    return asker;
}

// ---- the Composer, aimed at a target of one text field ---------------------------------------

constexpr const char* kNotesOffice = "zengine.test.guest-notes";

struct GuestsNote {
    std::string words;
    ZEN_SHAPE(GuestsNote, 1, ZEN_FIELD(words));
};

struct GuestsNoteDeskState {
    ZEN_SHAPE(GuestsNoteDeskState, 1);
};

/// A TARGET WHOSE ONE MESSAGE IS ONE TEXT FIELD, keeping the words of every note it is sent. It
/// answers `zen.DescribeAccepted` from its own accept-set, as every woven weave does.
class NoteDesk : public loom::WeaveBase<NoteDesk, GuestsNoteDeskState, loom::Accept<GuestsNote>> {
public:
    std::vector<std::string> heard;
    void on(const GuestsNote& note, loom::Mail&) { heard.push_back(note.words); }
};

NoteDesk* mount_note_desk(PaneRig& r) {
    auto weave = std::make_unique<NoteDesk>();
    NoteDesk* raw = weave.get();
    loom::Grant grant;
    (void)loom::allow_describe_answers(grant);
    const loom::WeaveId id =
        r.bus.register_weave(std::move(weave), std::move(grant), std::string(kNotesOffice));
    raw->zen_set_self(id);
    return raw;
}

/// COMPOSE AT THE DESK'S RIGHT, AIMED AT `office` (the notes desk by default): Info put down to
/// give it the room, then the target chosen the way the Loaded pane chooses one.
std::int64_t compose_at_notes(InventoryStory& s, const char* office = kNotesOffice) {
    auto& r = s.r;
    REQUIRE(r.load_refusals.empty());
    r.pick({"zengine.info", "info"});
    r.pick(composer_ref());
    const RuntimePane* row = r.session().panes.runtime.find(kComposerOffice, kComposePane);
    REQUIRE(row != nullptr);
    const std::int64_t compose = row->kind;
    for (auto& p : r.session().setup.active.panes) {
        if (p.ref.provider != kComposerOffice) continue;
        p.place = {pane_unit::kPixels, 85 * surface::kCanvasCellPx, 4 * surface::kCanvasCellPx};
        p.width = {pane_unit::kPixels, 80 * surface::kCanvasCellPx};
        p.height = {pane_unit::kPixels, 24 * surface::kCanvasCellPx};
    }
    // An unchanged room repaints nothing, so the room moves and comes back.
    r.extent(180, 59);
    r.extent(180, 60);
    auto selector = std::make_unique<InventoryHand>();
    InventoryHand* raw = selector.get();
    loom::Grant grant;
    grant.allow_to_any(intro::LoadedSelected::zen_name, 1);
    const loom::WeaveId id = r.bus.register_weave(std::move(selector), grant, kIntroOffice);
    raw->zen_set_self(id);
    raw->next = [office](loom::Mail& m) {
        m.as_role(kIntroOffice).publish(intro::LoadedSelected{"loaded", "a-guest-notes", office});
    };
    r.bus.send(id, loom::Message(loom::to_value(InventoryHandDo{})));
    r.bus.drain_until_idle();
    REQUIRE(shows_canvas(*r.session().panes.external_pane(compose)));
    return compose;
}

// ---- the View Builder -------------------------------------------------------------------------

/// The name in the View Builder's File box, from the builder's own state.
std::string builder_file(InventoryStory& s) {
    return loom::from_value<vb::ViewBuilderState>(
               s.r.bus.weave(s.r.bus.role_holder(vb::kRole))->snapshot())
        .path;
}

// ---- the Hotkeys pane -------------------------------------------------------------------------

/// THE REAL DESKTOP WITH ITS HOTKEYS PANE OPEN, tall and holding the keys, over a keymap file this
/// rig wrote, and the menu presenter. Driven raw, its input is the weaver's.
struct HotkeysDesk {
    TempDir dir;
    std::string path;
    PaneRig r;
    std::int64_t hotkeys = kNoPaneKind;

    explicit HotkeysDesk(const char* name) : dir(name), path(dir.file("keymap.json")) {
        write_keymap_file(path, keymap_file_text("default", {{"layout.new", "ctrl+n"}}));
        r.host.keymap_path = path;
        r.mount_workshop();
        r.ready();
        r.extent(200, 60);
        REQUIRE(r.load(dp::kDesktopStem, WORKSHOP_SO_DESKTOP_PANE, kDesktopRole).valid());
        REQUIRE(r.load_refusals.empty());
        REQUIRE(r.load_presenter(WORKSHOP_SO_MENU_PRESENTER).valid());
        r.key(input::scan::kK, input::mod::kCtrl);
        const Written tall = author_pane_size(r.session().setup.active,
                                              PaneRef{kDesktopRole, dp::kHotkeysPane},
                                              PaneSize{pane_unit::kPixels, cells_px(120)},
                                              PaneSize{pane_unit::kPixels, cells_px(40)});
        REQUIRE_MESSAGE(tall.accepted, tall.refusal);
        r.extent(200, 59);
        r.extent(200, 60);
        const RuntimePane* row = r.session().panes.runtime.find(kDesktopRole, dp::kHotkeysPane);
        REQUIRE(row != nullptr);
        hotkeys = row->kind;
        REQUIRE(r.session().panes.keyboard == hotkeys);
    }

    std::vector<std::string> rows() {
        const ExternalPane* pane = r.session().panes.external_pane(hotkeys);
        REQUIRE(pane != nullptr);
        return held_row_texts(*pane);
    }
    std::string text() {
        std::string all;
        for (const std::string& line : rows()) all += line + "\n";
        return all;
    }
    std::string file() { return slurp(path); }
    /// THE ROW SHOWING `id`, walked into the window with the weaver's cursor keys.
    std::int64_t row_of(const std::string& id) {
        r.key(input::scan::kHome);
        for (int step = 0; step < 200; ++step) {
            const std::vector<std::string> shown = rows();
            for (std::size_t i = 0; i < shown.size(); ++i) {
                if (shown[i].find(id) != std::string::npos) return static_cast<std::int64_t>(i);
            }
            r.key(input::scan::kDown);
        }
        return -1;
    }
    /// The cell a press on body row `row` aims at, past the row's mark.
    std::pair<std::int64_t, std::int64_t> cell(std::int64_t row) {
        const ui::Rect body = external_body_rect(r.session(), hotkeys);
        return {body.x + 4, body.y + kExternalHeaderRows + row};
    }
    /// THE WEAVER'S RIGHT PRESS ON A ROW, which offers that binding's edits.
    void right(std::int64_t row) {
        const auto [x, y] = cell(row);
        for (const bool down : {true, false}) {
            r.publish(loom::to_value(input::PointerButton{3, down, x, y + surface::kTuiCanvasTopRow,
                                                          input::space::kCells, input::mod::kNone}));
        }
    }
    /// The weaver chooses the presented menu's line reading `label`, through the keyboard.
    void choose(const std::string& label) {
        REQUIRE(menu_shown(r.session()));
        const std::int64_t at = presented_line_of(r.session(), label);
        REQUIRE_MESSAGE(at >= 0, "no menu line reads " << label);
        for (std::int64_t i = 0; i < at; ++i) r.key(input::scan::kDown);
        r.key(input::scan::kReturn);
    }
};

} // namespace

// ============================================================================
// THE EDITOR AND THE TERMINAL: places only the weaver's hand reaches
// ============================================================================

TEST_CASE("on a weaver's host a guest's keys, text, save and presses toward the Editor are refused at the dispatch, and the file is unchanged") {
    // THE STORY'S OWN HAND, ADMITTED AS A GUEST; the weaver's hand is the platform's reader.
    TransferStory s("guest-editor");
    Weaver weaver{s.r, s.physical};
    const std::string path = s.write("notes.txt", "alpha\n");
    REQUIRE(s.open(path).accepted);
    // THE WEAVER'S OWN EDIT, UNSAVED: the buffer differs from the file, so a save would show.
    weaver.click(s.editor, editor_chrome(s) + 0, 5);
    REQUIRE_MESSAGE(s.r.session().panes.keyboard == s.editor, s.r.last_notice());
    weaver.text("!");
    const std::string weavers = editor_doc(s).text;
    REQUIRE(weavers == "alpha!\n");
    REQUIRE(slurp(path) == "alpha\n");
    admit_as_guest(s.r, s.hand_id, /*development=*/false);

    // ITS TEXT RESTS IN NO PANE...
    const std::string typed = said_for(s.r, [&] { s.text("zzz"); });
    CHECK_MESSAGE(holds_words(typed, kTextRefused), typed);
    CHECK(editor_doc(s).text == weavers);
    // ...ITS KEYS REACH NO ROW OF THE EDITOR'S, an erase included...
    const std::string keyed = said_for(s.r, [&] { s.key(input::scan::kBackspace); });
    CHECK_MESSAGE(holds_words(keyed, kEditorRefused), keyed);
    CHECK(editor_doc(s).text == weavers);
    // ...AND ITS ^s SAVES NOTHING, though the buffer is dirty.
    const std::string saving = said_for(s.r, [&] { s.key(input::scan::kS, input::mod::kCtrl); });
    CHECK_MESSAGE(holds_words(saving, kEditorRefused), saving);
    CHECK(editor_doc(s).saved_text == "alpha\n");
    CHECK(slurp(path) == "alpha\n");

    // ITS PRESS ON THE DOCUMENT, the keys elsewhere: it moves neither the keys nor the caret.
    weaver.press_room();
    REQUIRE(s.r.session().panes.keyboard != s.editor);
    const std::int64_t caret = editor_doc(s).caret_byte;
    const std::string pressed = said_for(s.r, [&] { s.click(s.editor, editor_chrome(s) + 0, 1); });
    CHECK_MESSAGE(holds_words(pressed, kEditorRefused), pressed);
    CHECK(s.r.session().panes.keyboard != s.editor);
    CHECK(s.r.session().panes.selected != s.editor);
    CHECK(editor_doc(s).caret_byte == caret);
    CHECK(editor_doc(s).text == weavers);
    CHECK(slurp(path) == "alpha\n");

    // THE WEAVER'S OWN ^s, after all of it, writes the weaver's buffer.
    weaver.click(s.editor, editor_chrome(s) + 0, 0);
    REQUIRE(s.r.session().panes.keyboard == s.editor);
    weaver.key(input::scan::kS, input::mod::kCtrl);
    CHECK(slurp(path) == weavers);
}

TEST_CASE("on a weaver's host a guest's keys and presses toward the Terminal are refused, and no line is sent and no editor switch asked") {
    TerminalDesk t;
    guest_hand::GuestHand g(t.r, {"input"});
    std::size_t acts = 0;
    std::size_t switches = 0;
    RemoveObserver watching{t.r.bus, t.r.bus.add_observer([&](const loom::BusEvent& seen) {
        if (seen.kind == loom::EventKind::Delivered && seen.schema_name == TerminalActRequested::zen_name) {
            ++acts;
        }
        if (seen.schema_name == EditorSwitchRequested::zen_name) ++switches;
    })};
    const ui::Rect body = external_body_rect(t.r.session(), t.kind);
    const auto [room_x, room_y] = free_room_cell(t.r);
    g.weaver_press_cell(room_x, room_y);
    REQUIRE(t.r.session().panes.keyboard != t.kind);

    // ITS PRESS DOES NOT TAKE THE KEYS FOR THE TERMINAL.
    const std::string pressed = said_for(t.r, [&] { g.press_cell(body.x, body.y); });
    CHECK_MESSAGE(holds_words(pressed, kTerminalRefused), pressed);
    CHECK(t.r.session().panes.keyboard != t.kind);

    // THE WEAVER PRESSES IN; the guest's line, typed whole, rests nowhere.
    g.weaver_press_cell(body.x, body.y);
    REQUIRE(t.r.session().panes.keyboard == t.kind);
    const std::string typed = said_for(t.r, [&] { g.text(switch_line("guest")); });
    CHECK_MESSAGE(holds_words(typed, kTextRefused), typed);
    CHECK_MESSAGE(t.shown().find("destination=guest") == std::string::npos, t.shown());

    // THE WEAVER TYPES A SWITCH LINE, AND THE GUEST'S RETURN WOULD SEND IT: refused, nothing goes.
    for (const char c : switch_line("twin")) g.weaver_text(std::string(1, c));
    const std::string returned = said_for(t.r, [&] { g.key(input::scan::kReturn); });
    CHECK_MESSAGE(holds_words(returned, kTerminalRefused), returned);
    CHECK(t.record().empty());
    CHECK(acts == 0);
    CHECK(switches == 0);
    CHECK(t.office->asked.empty());

    // THE WEAVER'S OWN RETURN SENDS THE SAME LINE, and the switch is asked.
    g.weaver_key(input::scan::kReturn);
    CHECK(acts == 1);
    CHECK_FALSE(t.record().empty());
    CHECK(t.office->asked == std::vector<std::string>{"twin"});
}

// ============================================================================
// TYPED TEXT: what the weaver commits carries none of the guest's
// ============================================================================

TEST_CASE("on a weaver's host a guest's text toward Info, the Composer, Layouts' naming line and a Flow dialog field is refused, and the weaver's commit carries none of it") {
    SUBCASE("Info's edit line") {
        InventoryStory s;
        Weaver weaver{s.r, s.physical};
        // THE ENTRY IS PLACED BY THE STORY'S HAND before it is a guest; Info holds the keys.
        s.acquire();
        s.place();
        admit_as_guest(s.r, s.hand_id, /*development=*/false);
        weaver.key(input::scan::kReturn);
        weaver.key(input::scan::kA, input::mod::kCtrl);
        weaver.text("42");
        const std::string typed = said_for(s.r, [&] { s.text("9"); });
        CHECK_MESSAGE(holds_words(typed, kTextRefused), typed);
        weaver.key(input::scan::kReturn);
        CHECK_MESSAGE(s.shown(s.info).find("count: 42") != std::string::npos, s.shown(s.info));
        weaver.key(input::scan::kS, input::mod::kCtrl);
        CHECK_MESSAGE(s.stored().item.get("count")->as_int() == 42, s.shown(s.info));
    }
    SUBCASE("a Composer field") {
        InventoryStory s(191, true);
        Weaver weaver{s.r, s.physical};
        NoteDesk* desk = mount_note_desk(s.r);
        const std::int64_t compose = compose_at_notes(s);
        admit_as_guest(s.r, s.hand_id, /*development=*/false);
        const std::int64_t message = s.row_of(compose, "GuestsNote v1");
        REQUIRE_MESSAGE(message >= 0, s.shown(compose));
        weaver.click(compose, message);
        const std::int64_t field = s.row_of(compose, "words:");
        REQUIRE_MESSAGE(field >= 0, s.shown(compose));
        weaver.click(compose, field);
        REQUIRE(s.r.session().panes.keyboard == compose);
        weaver.text("weaver");
        const std::string typed = said_for(s.r, [&] { s.text("-guest"); });
        CHECK_MESSAGE(holds_words(typed, kTextRefused), typed);
        const std::int64_t submit = s.row_of(compose, "[ Submit ]");
        REQUIRE_MESSAGE(submit >= 0, s.shown(compose));
        weaver.click(compose, submit);
        CHECK_MESSAGE(desk->heard == std::vector<std::string>{"weaver"}, s.shown(compose));
    }
    SUBCASE("Layouts' naming line") {
        InventoryStory s;
        Weaver weaver{s.r, s.physical};
        admit_as_guest(s.r, s.hand_id, /*development=*/false);
        open_rename_on_live_tab(s.r);
        REQUIRE(s.r.session().setup.naming.open);
        for (int guard = 0; guard < 64 && !s.r.session().setup.naming.line.empty(); ++guard) {
            weaver.key(input::scan::kBackspace);
        }
        REQUIRE(s.r.session().setup.naming.line.empty());
        weaver.text("Morning");
        const std::string typed = said_for(s.r, [&] { s.text("-guest"); });
        CHECK_MESSAGE(holds_words(typed, kTextRefused), typed);
        CHECK(s.r.session().setup.naming.line.text() == "Morning");
        weaver.key(input::scan::kReturn);
        CHECK_FALSE(s.r.session().setup.naming.open);
        CHECK(s.r.session().setup.active.name == "Morning");
    }
    SUBCASE("a Flow dialog field") {
        TempDir dir("guest-flow-field");
        const std::string path = (dir.path() / "weaver.flow-workspace").generic_string();
        InventoryStory s(191, false, false, /*with_flow=*/true);
        REQUIRE(s.flow != 0);
        Weaver weaver{s.r, s.physical};
        admit_as_guest(s.r, s.hand_id, /*development=*/false);
        weaver.click_on(s.flow, "[Graph]");
        REQUIRE(s.r.session().panes.keyboard == s.flow);
        weaver.key(input::scan::kS, input::mod::kCtrl);
        REQUIRE_MESSAGE(pictured(s.r, s.flow, "Save complete workspace"), picture_text(s.r, s.flow));
        weaver.key(input::scan::kA, input::mod::kCtrl);
        weaver.text(path);
        const std::string typed = said_for(s.r, [&] { s.text("-guest"); });
        CHECK_MESSAGE(holds_words(typed, kTextRefused), typed);
        // THE WEAVER CONFIRMS: the file named is the weaver's, nothing of the guest's after it.
        weaver.key(input::scan::kReturn);
        CHECK_MESSAGE(std::filesystem::exists(path), picture_text(s.r, s.flow));
        CHECK_FALSE(std::filesystem::exists(path + "-guest"));
    }
}

// ============================================================================
// CLASS `write`: the acts that write a file, by a guest's hand
// ============================================================================

TEST_CASE("on a weaver's host a guest's s, t and quit are refused, and nothing is written") {
    TempDir dir("guest-own-writes");
    PaneRig r;
    r.host.setup_path = dir.file("setup.json");
    r.host.prefs_path = dir.file("prefs.json");
    r.host.session_path = dir.file("session.json");
    r.mount_workshop();
    r.ready();
    r.extent(160, 48);
    guest_hand::GuestHand g(r, {"input"});
    // A SECOND PARTICIPANT OF THE SAME ADMITTED ROW asks the quit by message, as a session does.
    loom::WeaveId asker_id{};
    QuitAsker* asker = mount_quit_asker(r, asker_id);
    answer_as_guest_too(r, asker_id, g.row);
    REQUIRE_FALSE(std::filesystem::exists(r.host.setup_path));
    REQUIRE_FALSE(std::filesystem::exists(r.host.prefs_path));
    REQUIRE_FALSE(std::filesystem::exists(r.host.session_path));
    const bool titles = r.session().pane_titles;

    // `s` SAVES THE SETUP: the guest's writes none, and says why.
    const std::string saved = said_for(r, [&] { g.key(input::scan::kS); });
    CHECK_MESSAGE(holds_words(saved, "the setup was not saved"), saved);
    CHECK_MESSAGE(holds_words(saved, kWriteRefused), saved);
    CHECK_FALSE(std::filesystem::exists(r.host.setup_path));
    // `t` IS SAVED AS IT IS STATED: the guest's toggles nothing and writes no prefs.
    const std::string toggled = said_for(r, [&] { g.key(input::scan::kT); });
    CHECK_MESSAGE(holds_words(toggled, "pane titles were not toggled"), toggled);
    CHECK_MESSAGE(holds_words(toggled, kWriteRefused), toggled);
    CHECK(r.session().pane_titles == titles);
    CHECK_FALSE(std::filesystem::exists(r.host.prefs_path));
    // A QUIT WRITES THE LAST SESSION: `q` and ^c leave Workshop open and write none.
    const auto quit_by = [&](std::int64_t scan, std::int64_t mods) {
        const std::string kept = said_for(r, [&] { g.key(scan, mods); });
        CHECK_MESSAGE(holds_words(kept, "Workshop stays open"), kept);
        CHECK_MESSAGE(holds_words(kept, kWriteRefused), kept);
        CHECK_FALSE(r.host.quit);
        CHECK_FALSE(std::filesystem::exists(r.host.session_path));
    };
    quit_by(input::scan::kQ, input::mod::kNone);
    quit_by(input::scan::kC, input::mod::kCtrl);
    // ...AND ASKED BY MESSAGE, the answer is a refusal in the same words.
    (void)r.bus.send(asker_id, loom::Message(loom::to_value(SeatDo{}), loom::WeaveId{},
                                             loom::WeaveId{}, 0));
    r.bus.drain_until_idle();
    REQUIRE(asker->refusals.size() == 1);
    CHECK_MESSAGE(holds_words(asker->refusals[0], "Workshop stays open"), asker->refusals[0]);
    CHECK_MESSAGE(holds_words(asker->refusals[0], kWriteRefused), asker->refusals[0]);
    CHECK(asker->acks == 0);
    CHECK_FALSE(r.host.quit);
    CHECK_FALSE(std::filesystem::exists(r.host.session_path));

    // THE WEAVER'S OWN `s`, `t` AND `q` WRITE EACH FILE.
    g.weaver_key(input::scan::kS);
    CHECK_MESSAGE(std::filesystem::exists(r.host.setup_path), r.last_notice());
    g.weaver_key(input::scan::kT);
    CHECK(r.session().pane_titles != titles);
    CHECK_MESSAGE(std::filesystem::exists(r.host.prefs_path), r.last_notice());
    g.weaver_key(input::scan::kQ);
    CHECK(r.host.quit);
    CHECK(std::filesystem::exists(r.host.session_path));
}

TEST_CASE("on a weaver's host a guest's Flow generate and View Builder save leave no file") {
    SUBCASE("Flow's generate, confirmed by the guest") {
        TempDir dir("guest-flow-generate");
        const std::filesystem::path out = dir.path() / "generated";
        InventoryStory s(191, false, false, /*with_flow=*/true);
        REQUIRE(s.flow != 0);
        Weaver weaver{s.r, s.physical};
        OperationAnswers answers;
        RemoveObserver watching{s.r.bus, watch_answers(s.r, answers)};
        admit_as_guest(s.r, s.hand_id, /*development=*/false);
        // THE WEAVER OPENS THE DIALOG AND NAMES THE DIRECTORY.
        weaver.click_on(s.flow, "[Graph]");
        REQUIRE(s.r.session().panes.keyboard == s.flow);
        weaver.key(input::scan::kG, input::mod::kCtrl);
        REQUIRE_MESSAGE(pictured(s.r, s.flow, "Generate native C++"), picture_text(s.r, s.flow));
        weaver.key(input::scan::kA, input::mod::kCtrl);
        weaver.text(out.generic_string());
        // THE GUEST CONFIRMS BY KEY, then by the dialog's own control: each asked, each refused.
        const std::size_t before = answers.refused.size();
        s.key(input::scan::kReturn);
        REQUIRE(answers.refused.size() == before + 1);
        CHECK_MESSAGE(holds_words(answers.refused.back(), kWriteRefused), answers.refused.back());
        CHECK_MESSAGE(pictured(s.r, s.flow, "Generating was refused"), picture_text(s.r, s.flow));
        CHECK_FALSE(std::filesystem::exists(out));
        guest_click_on(s, s.flow, "[Confirm]");
        REQUIRE(answers.refused.size() == before + 2);
        CHECK_MESSAGE(holds_words(answers.refused.back(), kWriteRefused), answers.refused.back());
        CHECK_FALSE(std::filesystem::exists(out));
        // THE WEAVER'S OWN CONFIRM, on the dialog still open, generates there.
        weaver.key(input::scan::kReturn);
        CHECK_MESSAGE(std::filesystem::exists(out / "generated.cpp"), picture_text(s.r, s.flow));
    }
    SUBCASE("the View Builder's save, pressed by the guest") {
        TempDir dir("guest-builder-save");
        const std::string path = (dir.path() / "weaver.view").generic_string();
        InventoryStory s(191, false, false, false, false, /*with_builder=*/true);
        REQUIRE(s.builder != 0);
        Weaver weaver{s.r, s.physical};
        OperationAnswers answers;
        RemoveObserver watching{s.r.bus, watch_answers(s.r, answers)};
        admit_as_guest(s.r, s.hand_id, /*development=*/false);
        // THE WEAVER TYPES THE ABSOLUTE PATH INTO FILE, the box five columns past its word.
        weaver.click_on(s.builder, "File", 6);
        REQUIRE(s.r.session().panes.keyboard == s.builder);
        weaver.text(path);
        weaver.key(input::scan::kReturn);
        REQUIRE_MESSAGE(builder_file(s) == path, picture_text(s.r, s.builder));
        // THE GUEST'S ^s: asked, refused in words, and no file.
        const std::size_t before = answers.refused.size();
        s.key(input::scan::kS, input::mod::kCtrl);
        REQUIRE(answers.refused.size() == before + 1);
        CHECK_MESSAGE(holds_words(answers.refused.back(), kWriteRefused), answers.refused.back());
        CHECK_MESSAGE(pictured(s.r, s.builder, "Saving was refused"), picture_text(s.r, s.builder));
        CHECK_FALSE(std::filesystem::exists(path));
        // THE WEAVER'S OWN ^s writes it.
        weaver.key(input::scan::kS, input::mod::kCtrl);
        CHECK_MESSAGE(std::filesystem::exists(path), picture_text(s.r, s.builder));
    }
}

TEST_CASE("on a weaver's host a guest's toolbox save sent to the Inventory pane is refused through ActorScopeRequested, and no file is written") {
    TempDir files("guest-toolbox");
    const std::string path = files.file("guest.toolbox");
    InventoryStory s(191 | 512, true);
    std::size_t scoped = 0;
    RemoveObserver watching{s.r.bus, s.r.bus.add_observer([&](const loom::BusEvent& seen) {
        if (seen.kind == loom::EventKind::Delivered && seen.schema_name == ActorScopeRequested::zen_name) {
            ++scoped;
        }
    })};
    admit_as_guest(s.r, s.hand_id, /*development=*/false, {"input", "toolbox"});
    // THE SAVE CROSSES NO GESTURE: sent straight to the pane's office by the hand.
    const auto save = [&] {
        s.act([&](loom::Mail& m) { m.send_to_role(slots::kRole, slots::InventoryToolboxSave{path}); });
    };
    s.hand->expect_refusal = true;
    save();
    REQUIRE(s.hand->refusals.size() == 1);
    CHECK_MESSAGE(holds_words(s.hand->refusals[0], kWriteRefused), s.hand->refusals[0]);
    CHECK(scoped == 1);
    CHECK(s.hand->toolboxes.empty());
    CHECK_FALSE(std::filesystem::exists(path));
    // ON A DEVELOPMENT HOST THE SAME SAVE, asked the same way, writes the file.
    s.r.host.host_fact.development = true;
    s.hand->expect_refusal = false;
    save();
    REQUIRE_MESSAGE(s.hand->toolboxes.size() == 1, s.shown(s.source));
    CHECK(s.hand->toolboxes.back().operation == "save");
    CHECK(scoped == 2);
    CHECK(std::filesystem::exists(path));
}

TEST_CASE("on a weaver's host a guest's Submit of a toolbox save in the Composer is refused, and no file is written") {
    // A RELAY SENDS AS ITS OWN OFFICE: the Inventory pane would ask Workshop about the Composer, no
    // guest, so the send is judged at the Submit's approval, for the guest's hand, as the class its
    // shape is. The weaver types the path; the act that sends it is the guest's.
    TempDir files("guest-relay");
    const std::string path = files.file("relayed.toolbox");
    InventoryStory s(191 | 512, true);
    Weaver weaver{s.r, s.physical};
    const std::int64_t compose = compose_at_notes(s, slots::kRole);
    admit_as_guest(s.r, s.hand_id, /*development=*/false, {"input", "inventory", "toolbox"});
    // THE PANE ACCEPTS MORE MESSAGES THAN ITS ROOM SHOWS: the weaver's keys walk the list to the
    // save, from the heading, and Return chooses it.
    weaver.click(compose, 0);
    REQUIRE(s.r.session().panes.keyboard == compose);
    for (int guard = 0; guard < 64 && s.row_of(compose, "> InventoryToolboxSave v1") < 0; ++guard) {
        weaver.key(input::scan::kDown);
    }
    REQUIRE_MESSAGE(s.row_of(compose, "> InventoryToolboxSave v1") >= 0, s.shown(compose));
    weaver.key(input::scan::kReturn);
    const std::int64_t field = s.row_of(compose, "path:");
    REQUIRE_MESSAGE(field >= 0, s.shown(compose));
    weaver.click(compose, field);
    weaver.text(path);
    // THE WEAVER'S ARROWS PUT THE CURSOR ON SUBMIT; THE GUEST'S RETURN IS THE ACT THAT SENDS.
    for (int guard = 0; guard < 8 && s.row_of(compose, "> [ Submit ]") < 0; ++guard) {
        weaver.key(input::scan::kDown);
    }
    REQUIRE_MESSAGE(s.row_of(compose, "> [ Submit ]") >= 0, s.shown(compose));
    REQUIRE_FALSE(std::filesystem::exists(path));
    s.key(input::scan::kReturn);
    CHECK_MESSAGE(s.shown(compose).find(kWriteRefused) != std::string::npos, s.shown(compose));
    CHECK_FALSE(std::filesystem::exists(path));
    // THE WEAVER'S OWN RETURN ON THE SAME DRAFT WRITES IT.
    weaver.key(input::scan::kReturn);
    CHECK_MESSAGE(std::filesystem::exists(path), s.shown(compose));
}

TEST_CASE("on a weaver's host a guest's keys and presses toward the Hotkeys pane are refused, and the keymap file is unchanged") {
    HotkeysDesk k("guest-hotkeys");
    guest_hand::GuestHand g(k.r, {"input"});
    const std::string before = k.file();
    const std::int64_t row = k.row_of("desktop.terminal");
    REQUIRE(row >= 0);
    const std::pair<std::int64_t, std::int64_t> aimed = k.cell(row);
    const std::int64_t x = aimed.first;
    const std::int64_t y = aimed.second;

    // A RIGHT PRESS ON A BINDING'S ROW, which would offer its edits: refused, and no menu opens.
    const std::string offered = said_for(k.r, [&] { g.press_cell(x, y, 3); });
    CHECK_MESSAGE(holds_words(offered, kHotkeysRefused), offered);
    CHECK_FALSE(menu_shown(k.r.session()));
    CHECK_FALSE(k.r.session().context.open);
    // A PRESS ON IT, AND A KEY TOWARD THE PANE.
    const std::string pressed = said_for(k.r, [&] { g.press_cell(x, y); });
    CHECK_MESSAGE(holds_words(pressed, kHotkeysRefused), pressed);
    const std::string keyed = said_for(k.r, [&] { g.key(input::scan::kDown); });
    CHECK_MESSAGE(holds_words(keyed, kHotkeysRefused), keyed);

    // THE WEAVER ARMS A CAPTURE: the next key the pane hears becomes the binding, and is written.
    k.right(row);
    REQUIRE(menu_shown(k.r.session()));
    k.choose("Modify (press a key)");
    REQUIRE_MESSAGE(k.text().find("press the key for `desktop.terminal`") != std::string::npos, k.text());
    REQUIRE(k.r.session().panes.keyboard == k.hotkeys);
    const std::string captured = said_for(k.r, [&] { g.key(input::scan::kG, input::mod::kCtrl); });
    CHECK_MESSAGE(holds_words(captured, kHotkeysRefused), captured);
    const AppRow* kept = k.r.session().keymap.app_row_of_id("desktop.terminal");
    REQUIRE(kept != nullptr);
    CHECK(kept->gesture == Gesture{input::scan::kT, input::mod::kCtrl});
    CHECK(k.file() == before);

    // THE WEAVER'S OWN KEY IS THE ONE CAPTURED, and the file is written.
    k.r.key(input::scan::kG, input::mod::kCtrl);
    const AppRow* moved = k.r.session().keymap.app_row_of_id("desktop.terminal");
    REQUIRE(moved != nullptr);
    CHECK(moved->gesture == Gesture{input::scan::kG, input::mod::kCtrl});
    CHECK(k.file() != before);
    CHECK(k.file().find("ctrl+g") != std::string::npos);
}

// ============================================================================
// THE DEVELOPMENT HOST: the agent's own, where the same hand reaches all of it
// ============================================================================

TEST_CASE("on a development host the same guest types into the Editor and saves, types a Terminal line, and toggles titles") {
    SUBCASE("the Editor") {
        TransferStory s("guest-editor-development");
        const std::string path = s.write("notes.txt", "alpha\n");
        REQUIRE(s.open(path).accepted);
        admit_as_guest(s.r, s.hand_id, /*development=*/true);
        s.click(s.editor, editor_chrome(s) + 0, 5);
        REQUIRE_MESSAGE(s.r.session().panes.keyboard == s.editor, s.r.last_notice());
        s.text("!");
        CHECK(editor_doc(s).text == "alpha!\n");
        s.key(input::scan::kS, input::mod::kCtrl);
        CHECK(slurp(path) == "alpha!\n");
    }
    SUBCASE("the Terminal") {
        TerminalDesk t;
        guest_hand::GuestHand g(t.r, {"input"}, /*development=*/true);
        const ui::Rect body = external_body_rect(t.r.session(), t.kind);
        g.press_cell(body.x, body.y);
        REQUIRE(t.r.session().panes.keyboard == t.kind);
        g.text(switch_line("guest"));
        g.key(input::scan::kReturn);
        CHECK_FALSE(t.record().empty());
        CHECK(t.office->asked == std::vector<std::string>{"guest"});
    }
    SUBCASE("Workshop's titles") {
        TempDir dir("guest-titles-development");
        PaneRig r;
        r.host.prefs_path = dir.file("prefs.json");
        r.mount_workshop();
        r.ready();
        r.extent(160, 48);
        guest_hand::GuestHand g(r, {"input"}, /*development=*/true);
        const bool titles = r.session().pane_titles;
        g.key(input::scan::kT);
        CHECK(r.session().pane_titles != titles);
        CHECK_MESSAGE(std::filesystem::exists(r.host.prefs_path), r.last_notice());
    }
}
