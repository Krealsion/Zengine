// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Workshop panes suite — THE EDITOR, AS A LOADED WEAVE.
//
// THIS FILE OWNS the one source document a maker edits: opening it through the one door,
// editing it, saving it, discarding a slip, losing it to nothing a maker did not do, and
// leaving the process only when it is safe to. Everything the built-in Editor did that a maker
// can see is driven here through the REAL `zengine-editor-pane` image, over the REAL pane
// protocol, against the REAL host doors -- and read back off the pane's own rows, the caret
// Workshop admitted for it, and the bytes on disk. Nothing in this file constructs the weave,
// reaches into its state, or calls one of its functions.
//
// ---- WHY THIS ONE IS DIFFERENT FROM THE OTHER FIVE -------------------------------
//
// ⚠ THE DOCUMENT MIGRATED, AND THAT IS MEASURED. Files left the project root behind, the
// Builder left the tool, the Terminal left the participant; each of those panes presents a
// subject somebody else holds. The Editor's subject is the buffer a maker types into, and it
// is the weave's now: the host holds no path, no bytes, no saved copy and no dirty answer, and
// what it can know about the document it learns by asking (`PaneQuitRequested`) or by being
// told (`PaneRevealRequested`). So the cases that used to read `session().editor` read the
// pane's status row instead, and the ones about the exit read what the host DECIDED after the
// pane ANSWERED -- which is the shape of the one operation that became a conversation.
//
// ⚠ AND IT IS THE FIRST PANE WHOSE CUSTODY IS THE CLAIM. A replaceable pane holds a maker's
// unsaved work, deliberately, and three cases below are the three things that make that a
// design: presentation and custody are two lifetimes (remove the pane, get the document back
// whole); a same-shape reload carries the document (its SHAPE is pinned here, the reload is
// driven end to end in `test_workshop_load.cpp`); and an orderly quit is asked of the pane
// before the bus stops, with a maker's gestures held and replayed if the answer is no.
//
// ⚠ THE FOUR THINGS THAT CROSS A TURN BOUNDARY are staged with `enqueue_*` and `settle`,
// never with a sleep: the paste (a Skin answers later), the open (a reveal follows), the quit
// (an answer decides) and the sweep (one motion at a time). The bus is FIFO, a batch is
// delivered the way one poll delivers it, and an answer asked for during the batch lands
// behind the gestures already queued.

// main() and the framework live in doctest_main.cpp -- the shared one that
// refuses a run selecting zero cases (POP-01).
#include "workshop_support.hpp"

#include "editor-pane/editor.hpp"
#include "editor-pane/vocabulary.hpp"
#include "workshop/builder_seam_vocabulary.hpp"
#include "workshop/pane_doors.hpp"
#include "workshop/pane_migration.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace {

namespace pane = zengine::editor_pane;

/// The office and pane a saved setup names, spelled through the package's own header -- the
/// durable names, not literals, so a case cannot agree with a typo.
inline PaneRef editor_ref() { return PaneRef{pane::kEditorPaneRole, pane::kEditorPane}; }

inline void put_bytes(const std::filesystem::path& at, const std::string& bytes) {
    std::ofstream out(at, std::ios::binary | std::ios::trunc);
    REQUIRE(out.good());
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    out.close();
    REQUIRE(out.good());
}

/// A SOURCE WITH ITS COMMENTS REMOVED: `/* */` first, then every `//` to its line's end.
inline std::string code_only(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    std::size_t i = 0;
    while (i < text.size()) {
        if (text.compare(i, 2, "/*") == 0) {
            const std::size_t end = text.find("*/", i + 2);
            i = end == std::string::npos ? text.size() : end + 2;
            continue;
        }
        if (text.compare(i, 2, "//") == 0) {
            const std::size_t end = text.find('\n', i);
            i = end == std::string::npos ? text.size() : end;
            continue;
        }
        out.push_back(text[i]);
        ++i;
    }
    return out;
}

inline std::string bytes_of(const std::filesystem::path& at) {
    std::ifstream in(at, std::ios::binary);
    REQUIRE(in.good());
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

/// THE DOOR'S OWN SPELLING OF A PATH. The Editor resolves every entrant through
/// `persist::resolved_against` -- absolute, lexically normal, one separator convention -- so
/// a case spells its expectation through the same function rather than a platform's
/// punctuation.
inline std::string spelled(const std::filesystem::path& at) {
    return persist::resolved_against(std::string(), at.generic_string());
}

/// A SKIN THAT TAKES THE CLIPBOARD ANSWER AWAY WITH IT -- `defer_answer()` moves the answer
/// out of the queue and into a capability this seat holds, so a case can put real turns
/// between a maker's paste and the platform's answer. `AnswerNow` is the unrelated delivery
/// that lets the held answer be spent; the correlation the ask was delivered with is restored
/// by the bus, never chosen here.
struct AnswerNow {
    ZEN_SHAPE(AnswerNow, 1);
};

class SlowSkin : public loom::WeaveBase<SlowSkin, SeenState,
                                        loom::Accept<surface::ClipboardTextRequested, AnswerNow>,
                                        loom::Emit<surface::ClipboardText>> {
public:
    std::string text;
    bool held = false;
    int asked = 0;
    loom::DeferredAnswer answer;

    void on(const surface::ClipboardTextRequested&, loom::Mail& mail) {
        ++asked;
        answer = mail.defer_answer();
        held = answer.valid();
    }
    void on(const AnswerNow&, loom::Mail& mail) {
        if (!held) {
            return;
        }
        held = false;
        (void)loom::answer_deferred(answer, mail, surface::ClipboardText{true, text});
    }
};

/// A LIVE WORKSHOP WITH THE REAL EDITOR PANE LOADED INTO IT.
///
/// The order is the host's, and it is the whole arrangement under test: the project door is
/// mounted BEFORE the plan runs, because the pane asks `zengine.project` where this run began
/// on the very beat it is activated -- a door mounted afterwards would be absent exactly when
/// the only ask that matters is made.
struct EditorRig {
    TempDir dir;
    std::filesystem::path root;
    std::string marks_path;
    PaneRig r;
    std::int64_t kind = 0;
    DoorAsker* asker = nullptr;
    SkinSeat* skin = nullptr;
    SlowSkin* slow = nullptr;
    loom::WeaveId slow_id{};
    /// What the host's recipe seam answers -- the fact the read-only project door spends.
    HostContext::RecipeSource next_source{};
    /// WHICH COMPOSED ROWS ARE ABOVE THE DOCUMENT, as the pane will have composed them at
    /// the next gesture: the status row always, and a notice row while one stands. Kept by
    /// the helpers that know which acts leave one, so a case aims at document row `n` and
    /// hits document row `n`.
    std::int64_t chrome = 1;

    explicit EditorRig(const char* tag) : dir(tag) {
        root = dir.path();
        r.host.project_dir = root.generic_string();
        marks_path = (root / "workshop-marks.json").generic_string();
        r.host.recipe_source = [this](const std::string&) { return next_source; };
    }

    /// LOAD THE IMAGE AND, ORDINARILY, PICK THE PANE. A case about the reveal loads it and
    /// leaves the picking to the document's own arrival.
    void open(std::int64_t width = 160, std::int64_t height = 48, bool pick_it = true,
              bool slow_skin = false) {
        r.mount_workshop();
        mount_project_door();
        if (slow_skin) {
            mount_slow_skin();
        } else {
            skin = r.mount_skin_seat();
        }
        load::LoadPlan plan;
        load::ArtifactIntent seat;
        seat.stem = pane::kEditorPaneStem;
        seat.weave = load::WeaveIntent{pane::kEditorPaneRole};
        plan.artifacts.push_back(seat);
        const load::Executed done = r.run_plan(plan);
        REQUIRE_MESSAGE(done.ok, done.refusal);
        r.ready();
        r.extent(width, height);
        REQUIRE_MESSAGE(row() != nullptr, "the loaded image offered no `editor` pane");
        kind = row()->kind;
        if (pick_it) {
            r.pick(editor_ref());
            REQUIRE(r.session().panels.has(kind));
        }
        mount_asker();
    }

    const RuntimePane* row() {
        return r.session().panels.runtime.find(pane::kEditorPaneRole, pane::kEditorPane);
    }

    void mount_project_door() {
        auto door = std::make_unique<ProjectDoor>(r.host.project_dir, marks_path,
                                                  ProjectDoor::Frontier{}, ProjectDoor::Names{},
                                                  r.host.recipe_source);
        ProjectDoor* raw = door.get();
        loom::Grant say;
        say.allow_to_any(ProjectRoot::zen_name, ProjectRoot::zen_version);
        say.allow_to_any(RecipeSourceSaid::zen_name, RecipeSourceSaid::zen_version);
        const loom::WeaveId id =
            r.bus.register_weave(std::move(door), std::move(say), std::string(kProjectRole));
        raw->zen_set_self(id);
    }

    void mount_slow_skin() {
        auto seat = std::make_unique<SlowSkin>();
        slow = seat.get();
        loom::Grant grant;
        grant.allow_to_any(surface::ClipboardText::zen_name, surface::ClipboardText::zen_version);
        slow_id = r.bus.register_weave(std::move(seat), std::move(grant), surface::kSkinRole);
        slow->zen_set_self(slow_id);
    }

    void answer_now() {
        REQUIRE(slow != nullptr);
        (void)r.bus.send(slow_id, loom::Message(loom::to_value(AnswerNow{}), loom::WeaveId{},
                                                loom::WeaveId{}, 0));
        r.bus.drain_until_idle();
    }

    /// The party that stands in for Files and the Builder: an office that asks the two doors
    /// and reads their answers.
    void mount_asker() {
        auto held = std::make_unique<DoorAsker>(std::string(kDoorAskerOffice));
        asker = held.get();
        loom::Grant grant;
        grant.allow_to_any(OpenSourceRequested::zen_name, OpenSourceRequested::zen_version);
        grant.allow_to_any(RecipeSourceRequested::zen_name, RecipeSourceRequested::zen_version);
        grant.allow_to_any(ProjectRootRequested::zen_name, ProjectRootRequested::zen_version);
        // ...AND THE TWO SENTENCES A STRANGER MIGHT FORGE, granted so the forgery cases below
        // prove the HOST's refusal and not the bus's: a stray reveal and a stray quit answer
        // must reach Workshop to be dropped by it. (The first matrix found both cases vacuous:
        // the asker could not say them, so nothing was ever judged.)
        grant.allow_to_any(PaneRevealRequested::zen_name, PaneRevealRequested::zen_version);
        grant.allow_to_any(PaneQuitAnswered::zen_name, PaneQuitAnswered::zen_version);
        const loom::WeaveId id = r.bus.register_weave(std::move(held), std::move(grant),
                                                      std::string(kDoorAskerOffice));
        asker->zen_set_self(id);
        asker->id = id;
    }

    void asker_says(std::function<void(DoorAsker&, loom::Mail&)> what) {
        asker->next = std::move(what);
        (void)r.bus.send(asker->id, loom::Message(loom::to_value(SeatDo{}), loom::WeaveId{},
                                                  loom::WeaveId{}, 0));
        r.bus.drain_until_idle();
    }

    /// ASK THE EDITOR'S DOOR TO OPEN ONE PATH -- what a Return on a source row in Files
    /// crosses as -- and hand back what it answered.
    SourceOpened ask_open(const std::string& path) {
        const std::size_t before = asker->opens.size();
        asker_says([path](DoorAsker& a, loom::Mail& mail) {
            a.ask(mail, kEditorRole, OpenSourceRequested{path});
        });
        REQUIRE(asker->opens.size() == before + 1);
        chrome = 2; // "editing ..." stands on the notice row until the maker's next act
        return asker->opens.back();
    }

    /// OPEN A FILE AND REQUIRE THAT IT TOOK: the ordinary beginning of a case.
    std::string open_file(const char* name, const std::string& bytes) {
        put_bytes(root / name, bytes);
        const std::string path = spelled(root / name);
        const SourceOpened said = ask_open(path);
        REQUIRE_MESSAGE(said.accepted, said.refusal);
        REQUIRE(r.session().panels.has(kind));
        return path;
    }

    /// The pane's rows, as Workshop admitted and painted them.
    std::vector<std::string> shown() { return pane_rows(r, kind); }

    std::string text() {
        std::string all;
        for (const std::string& one : shown()) {
            all += one;
            all += '\n';
        }
        return all;
    }

    /// THE STATUS ROW: the first row the pane composes.
    std::string status() {
        const std::vector<std::string> rows = shown();
        REQUIRE_FALSE(rows.empty());
        return rows[0];
    }

    /// Document row `n` of the window, as shown.
    std::string doc_row(std::int64_t n) {
        const std::vector<std::string> rows = shown();
        const std::size_t at = static_cast<std::size_t>(chrome + n);
        REQUIRE(at < rows.size());
        return rows[at];
    }

    bool says(const std::string& piece) { return text().find(piece) != std::string::npos; }
    bool dirty() { return status().rfind("UNSAVED", 0) == 0; }
    bool clean() { return status().rfind("saved", 0) == 0; }
    bool no_source() { return status().rfind("no source open", 0) == 0; }

    /// PRESS INTO THE PANE ON ITS STATUS ROW -- a row that means nothing but focus.
    void focus() {
        press_pane(r, kind, 0, 0);
        REQUIRE(r.session().panels.keyboard == kind);
    }

    /// Hand the keys back the way a maker does -- a press on the bare workspace.
    void unfocus() {
        r.press_cell(0, screen_of(r.session()).h - 1);
        REQUIRE(r.session().panels.keyboard != kind);
    }

    /// A press on document row `row`, column `col` of the window.
    void press_doc(std::int64_t row, std::int64_t col) {
        press_pane(r, kind, chrome + row, col);
        chrome = 1; // a press on the document clears a standing notice
    }
    void motion_doc(std::int64_t row, std::int64_t col) {
        const ui::Rect body = external_body_rect(r.session(), kind);
        r.motion_cell(body.x + col, body.y + kExternalHeaderRows + chrome + row);
    }
    void release_doc(std::int64_t row, std::int64_t col) {
        const ui::Rect body = external_body_rect(r.session(), kind);
        r.release_cell(body.x + col, body.y + kExternalHeaderRows + chrome + row);
    }
    void wheel(double dy) {
        const ui::Rect body = external_body_rect(r.session(), kind);
        r.wheel_cell(dy, body.x + 2, body.y + kExternalHeaderRows + chrome);
    }

    void key(std::int64_t sc, std::int64_t mods = input::mod::kNone) {
        r.key(sc, mods);
        chrome = 1;
    }
    void type(const std::string& s) {
        for (const char c : s) {
            r.text(std::string(1, c));
        }
        chrome = 1;
    }
    /// A save, a discard: acts that leave a notice standing.
    void save() {
        r.key(input::scan::kS, input::mod::kCtrl);
        chrome = 2;
    }
    void discard() {
        r.key(input::scan::kD, input::mod::kCtrl);
        chrome = 2;
    }

    // ---- STAGING AN ORDER ON THE REAL BUS ------------------------------------------
    void enqueue_key(std::int64_t sc, std::int64_t mods = input::mod::kNone) {
        (void)r.bus.publish(loom::Message(loom::to_value(input::KeyPressed{sc, "", mods}),
                                          loom::WeaveId{}, loom::WeaveId{}, 0));
    }
    void enqueue_text(const std::string& s) {
        (void)r.bus.publish(loom::Message(loom::to_value(input::TextEntered{s}), loom::WeaveId{},
                                          loom::WeaveId{}, 0));
    }
    void settle() {
        r.bus.drain_until_idle();
        chrome = 1;
    }

    /// THE CARET WORKSHOP IS HOLDING FOR THIS PANE, read off the host's own record -- so a
    /// case asks what was ADMITTED rather than what was sent.
    const ExternalPane* seat() { return r.session().panels.external_pane(kind); }

    /// GIVE THIS PANE EXACTLY `rows` ROWS OF ITS OWN, by authoring the height a maker would
    /// drag -- the pane's own chrome is three cells of the authored box, measured -- and
    /// repaint, because a room arrives on the next repaint.
    void give_rows(std::int64_t rows) {
        const Written wrote =
            author_pane_size(r.session().setup.active, editor_ref(), PaneSize{},
                             PaneSize{pane_unit::kSubcells, subs(rows + 3)});
        REQUIRE_MESSAGE(wrote.accepted, wrote.refusal);
        focus();
        REQUIRE(seat() != nullptr);
        REQUIRE_MESSAGE(seat()->rows == rows, "asked for ", rows, " rows and was granted ",
                        seat()->rows);
    }

    /// The ids this pane declares RIGHT NOW, sorted.
    std::vector<std::string> declared() {
        const RuntimePane* one = row();
        REQUIRE(one != nullptr);
        std::vector<std::string> ids;
        for (const PaneActionRow& a : one->actions) {
            ids.push_back(a.id);
        }
        std::sort(ids.begin(), ids.end());
        return ids;
    }

    /// Ask to quit the way a maker does from command mode, and hand back whether the host
    /// decided to end.
    bool quit_by_key() {
        unfocus();
        r.key(input::scan::kQ);
        return r.host.quit;
    }
};

} // namespace

// ============================================================================
// THE OFFER, THE ROWS, AND THE KEYS
// ============================================================================

TEST_CASE("EDIT-W1: the Editor is an ordinary arranged pane, offered by an office") {
    // ⭐ THE BUILT-IN'S ROW IS GONE, and what replaces it arrives the way Files, the Builder
    // and the Terminal did: an offer from an office, admitted into the runtime catalog, picked
    // from the picker, seated in the stack.
    EditorRig e("edit-offer");
    e.open();
    REQUIRE(e.row() != nullptr);
    CHECK(is_runtime_kind(e.kind));
    CHECK(e.row()->provider == "zengine.editor");
    CHECK(e.row()->pane == "editor");
    CHECK(e.row()->name == "Editor");
    CHECK(e.row()->summary == "edit a source file");
    // ...AND THE HOST COMPILES NO ROW FOR IT: no `panel::k*`, no catalog entry, no key.
    for (std::size_t i = 0; i < kPanelKinds; ++i) {
        CHECK(std::string(kPanelCatalog[i].pane) != "editor");
    }
    CHECK(row_of_id("editor.save") == nullptr);
    CHECK(row_of_id("editor.discard") == nullptr);
    // THE EMPTY PANE SAYS HOW TO FILL ITSELF, in its own first row.
    CHECK(e.no_source());
    CHECK(e.says("Return on a file in Files"));
}

TEST_CASE("EDIT-W2: the four keys are the pane's rows, on the built-in's own spellings") {
    // ⭐ THE FOUR `Act` VALUES BECAME FOUR DECLARED ROWS (VD-22, VD-25). The ids did not
    // change, because a maker's keymap file names them; what changed is who answers.
    EditorRig e("edit-rows");
    e.open();
    CHECK(e.declared() == std::vector<std::string>{"editor.discard", "editor.newline",
                                                   "editor.save", "editor.tab"});
    // ...on the keys the built-in bound them to.
    const PaneRow* save = e.r.session().keymap.pane_action_for(e.kind, input::scan::kS,
                                                                input::mod::kCtrl);
    REQUIRE(save != nullptr);
    CHECK(save->id == "editor.save");
    const PaneRow* discard = e.r.session().keymap.pane_action_for(e.kind, input::scan::kD,
                                                                   input::mod::kCtrl);
    REQUIRE(discard != nullptr);
    CHECK(discard->id == "editor.discard");
    CHECK(e.r.session().keymap.pane_action_for(e.kind, input::scan::kReturn, input::mod::kNone) !=
          nullptr);
    CHECK(e.r.session().keymap.pane_action_for(e.kind, input::scan::kTab, input::mod::kNone) !=
          nullptr);
}

TEST_CASE("EDIT-W3: one physical ^s is the document's save or the source's, by who holds the keys") {
    // ⭐ `document.save` IS A `kNoText` ROW NOW. It used to be `kNoEditor` -- "everywhere but
    // the source editor" -- because the editor was the one text-taking place that owned ^s.
    // A pane that declares `editor.save` on ctrl+s is a pane, so the host row yields wherever
    // a pane holds the keys, and the collision law is what says the two never both fire.
    const Keymap k;
    CHECK(k.action_for(KeyContext::kCommand, input::scan::kS, input::mod::kCtrl) ==
          Act::kSaveDocument);
    // ⚠ A NAMED CHANGE: the layout-name line TAKES TEXT, so the document's save is not
    // active while a maker types a name -- it was, while the class was "everywhere but the
    // editor". The class is now the one `workshop.quit` already had, and the two rows agree.
    CHECK(k.action_for(KeyContext::kNaming, input::scan::kS, input::mod::kCtrl) == Act::kNone);
    CHECK(k.above_mode_action(KeyContext::kCommand, input::scan::kS, input::mod::kCtrl) ==
          Act::kSaveDocument);
    CHECK(k.above_mode_action(KeyContext::kPane, input::scan::kS, input::mod::kCtrl) ==
          Act::kNone);
    // `^o` stays global -- a pane holding the keys included.
    CHECK(k.above_mode_action(KeyContext::kPane, input::scan::kO, input::mod::kCtrl) ==
          Act::kOpenDocument);
    // ...and the class algebra no longer has an editor-shaped hole in it.
    CHECK(active_in(KeyContext::kNoText, KeyContext::kCommand));
    CHECK_FALSE(active_in(KeyContext::kNoText, KeyContext::kPane));
    CHECK_FALSE(contexts_intersect(KeyContext::kNoText, KeyContext::kPane));

    // LIVE: ^s with the Editor holding the keys saves the SOURCE and not the document.
    EditorRig e("edit-ctrl-s");
    e.open();
    const std::string path = e.open_file("a.cpp", "one\n");
    e.press_doc(0, 3);
    e.type("x");
    REQUIRE(e.dirty());
    const std::string doc_notice = e.r.session().notice;
    e.save();
    CHECK(e.clean());
    CHECK(bytes_of(e.root / "a.cpp") == "onex\n");
    CHECK(e.r.session().notice == doc_notice); // the host's save never ran
}

TEST_CASE("EDIT-W4: a saved setup naming the built-in Editor opens as the loaded pane") {
    // ⭐ `zengine.workshop/editor` NAMES NO BUILT-IN NOW, and every desk written while it did
    // is converted at read (`pane_migration.hpp`) rather than left pointing at nothing.
    Setup old;
    old.name = "Vintage";
    REQUIRE(add_pane(old, PaneRef{pane_migration::kRetiredEditorProvider,
                                  pane_migration::kEditorPane}));
    const setup_persist::LoadedSetup read = setup_persist::from_text(setup_persist::to_text(old));
    REQUIRE(read.outcome.accepted);
    CHECK(read.converted.total() == 1);
    REQUIRE(read.setup.panes.size() == 1);
    CHECK(read.setup.panes[0].ref == editor_ref());
    CHECK(pane_migration::names_the_retired_editor(
        PaneRef{pane_migration::kRetiredEditorProvider, pane_migration::kEditorPane}));
    CHECK_FALSE(pane_migration::names_the_retired_editor(editor_ref()));
    // ...and the converted row resolves to the loaded image's own handle.
    EditorRig e("edit-mig");
    e.open(160, 48, /*pick_it=*/false);
    CHECK(resolve_pane(read.setup.panes[0].ref, e.r.session().panels).value_or(-1) == e.kind);
}

// ============================================================================
// THE DOOR: open, refuse, reveal
// ============================================================================

TEST_CASE("EDIT-W5: opening a source installs it, answers the asker, and asks to be shown") {
    // ⭐ THE ONE DOOR, AT ITS NEW OFFICE. The pane is not even seated: the document arrives,
    // the asker hears `accepted`, and the pane asks Workshop to reveal it -- which seats it,
    // selects it, and points the keys at it, in that order.
    EditorRig e("edit-open");
    e.open(160, 48, /*pick_it=*/false);
    REQUIRE_FALSE(e.r.session().panels.has(e.kind));
    put_bytes(e.root / "hello.cpp", "one\ntwo\nthree\n");
    const SourceOpened said = e.ask_open(spelled(e.root / "hello.cpp"));
    CHECK(said.accepted);
    CHECK(said.refusal.empty());
    // SEATED, SELECTED, KEYED -- the reveal's three facts.
    REQUIRE(e.r.session().panels.has(e.kind));
    CHECK(e.r.session().panels.selected == e.kind);
    CHECK(e.r.session().panels.keyboard == e.kind);
    CHECK(e.r.session().notice.find("showing Editor") != std::string::npos);
    // AND THE ROWS ARE THE DOCUMENT'S, under a status row that says where the caret is.
    CHECK(e.status().rfind("saved L1:C1/4", 0) == 0);
    CHECK(e.status().find(" -- ") != std::string::npos);
    CHECK(e.says("editing "));
    CHECK(e.doc_row(0) == "one");
    CHECK(e.doc_row(1) == "two");
    CHECK(e.doc_row(2) == "three");
}

TEST_CASE("EDIT-W6: a relative path is the PROJECT's file, resolved through the project door") {
    // The pane asked `zengine.project` where this run began; a relative spelling means that.
    EditorRig e("edit-relative");
    e.open();
    std::filesystem::create_directories(e.root / "src");
    put_bytes(e.root / "src" / "a.cpp", "project bytes\n");
    const SourceOpened said = e.ask_open("src/a.cpp");
    REQUIRE_MESSAGE(said.accepted, said.refusal);
    CHECK(e.doc_row(0) == "project bytes");
    CHECK(e.status().find("src/a.cpp") != std::string::npos);
}

TEST_CASE("EDIT-W7: a missing file, an oversized one and refused bytes cost the asker the refusal and nothing else") {
    EditorRig e("edit-refuse");
    e.open();
    const std::string first = e.open_file("one.cpp", "int one;\n");
    // A FILE THAT IS NOT THERE.
    const SourceOpened missing = e.ask_open(spelled(e.root / "absent.cpp"));
    CHECK_FALSE(missing.accepted);
    CHECK_FALSE(missing.refusal.empty());
    CHECK(e.status().find("one.cpp") != std::string::npos);
    CHECK(e.doc_row(0) == "int one;");
    // BYTES THE LAW REFUSES: mixed endings.
    put_bytes(e.root / "mixed.cpp", "a\r\nb\n");
    const SourceOpened mixed = e.ask_open(spelled(e.root / "mixed.cpp"));
    CHECK_FALSE(mixed.accepted);
    CHECK(mixed.refusal.find("mixed.cpp") != std::string::npos);
    CHECK(e.status().find("one.cpp") != std::string::npos);
    // ...AND ONE PAST THE BOUND, refused by the reader before a byte is judged.
    put_bytes(e.root / "huge.cpp", std::string(kMaxSourceBytes + 1, 'x'));
    const SourceOpened huge = e.ask_open(spelled(e.root / "huge.cpp"));
    CHECK_FALSE(huge.accepted);
    CHECK_FALSE(huge.refusal.empty());
    CHECK(e.status().find("one.cpp") != std::string::npos);
    (void)first;
    // ...WHILE ONE AT THE BOUND IS ADMITTED WHOLE.
    put_bytes(e.root / "max.cpp", std::string(kMaxSourceBytes - 1, 'y') + "\n");
    const SourceOpened max = e.ask_open(spelled(e.root / "max.cpp"));
    CHECK_MESSAGE(max.accepted, max.refusal);
    CHECK(e.status().find("max.cpp") != std::string::npos);
}

TEST_CASE("EDIT-W8: the door refuses speech with no author, and answers nobody") {
    EditorRig e("edit-anon");
    e.open();
    put_bytes(e.root / "a.cpp", "a\n");
    const std::size_t heard = e.asker->opens.size();
    e.asker->personally = true;
    e.asker_says([&e](DoorAsker& a, loom::Mail& mail) {
        a.ask(mail, kEditorRole, OpenSourceRequested{spelled(e.root / "a.cpp")});
    });
    CHECK(e.asker->opens.size() == heard);
    CHECK(e.no_source());
}

TEST_CASE("EDIT-W9: with no room to seat it, the document is open and the reveal is refused in the picker's words") {
    // ⭐ THE ORDER IS THE HONESTY. The document is installed and the asker answered FIRST;
    // the reveal is a second ask the host may refuse, and a refused reveal changes nothing
    // in the pane -- the source is open in a pane a maker can bring back once there is room.
    EditorRig e("edit-noroom");
    e.open(160, kMinScreen.h, /*pick_it=*/false);
    e.r.pick(ref_of(panel::kPaneEditor)); // the one stack slot the minimum screen has
    REQUIRE(e.r.session().panels.has(panel::kPaneEditor));
    put_bytes(e.root / "a.cpp", "held\n");
    const SourceOpened said = e.ask_open(spelled(e.root / "a.cpp"));
    CHECK(said.accepted);
    CHECK_FALSE(e.r.session().panels.has(e.kind));
    CHECK(e.r.session().notice == "no room for Editor on this screen -- make the window "
                                  "taller, then p again");
    CHECK_FALSE(has_pane(e.r.session().setup.active, editor_ref())); // nothing authored
    // A TALLER WINDOW, AND THE PICK SHOWS THE DOCUMENT THAT WAS THERE ALL ALONG.
    e.r.extent(160, 48);
    e.r.pick(editor_ref());
    REQUIRE(e.r.session().panels.has(e.kind));
    CHECK(e.doc_row(0) == "held");
}

TEST_CASE("EDIT-W10: a reveal from an office that offered no such pane is dropped") {
    // ⚔ MUTATION: `on(PaneRevealRequested)` seating by NAME rather than by the offer's row.
    EditorRig e("edit-stray-reveal");
    e.open(160, 48, /*pick_it=*/false);
    const std::string notice = e.r.session().notice;
    e.asker_says([](DoorAsker&, loom::Mail& mail) {
        (void)mail.as_role(kDoorAskerOffice)
            .send_to_role(kWorkshopProvider, PaneRevealRequested{pane::kEditorPane});
    });
    CHECK_FALSE(e.r.session().panels.has(e.kind));
    CHECK(e.r.session().notice == notice);
    // ...and personal speech is dropped the same way.
    e.asker->personally = true;
    e.asker_says([](DoorAsker&, loom::Mail& mail) {
        (void)mail.send_to_role(kWorkshopProvider, PaneRevealRequested{pane::kEditorPane});
    });
    CHECK_FALSE(e.r.session().panels.has(e.kind));
}

TEST_CASE("EDIT-W11: re-requesting the open source reveals it and destroys nothing") {
    EditorRig e("edit-again");
    e.open();
    const std::string path = e.open_file("a.cpp", "one\ntwo\n");
    e.press_doc(1, 2);
    e.type("X");
    REQUIRE(e.dirty());
    // Remove the presentation, then ask for the same source again.
    e.unfocus();
    e.r.pick(editor_ref());
    REQUIRE_FALSE(e.r.session().panels.has(e.kind));
    const SourceOpened again = e.ask_open(path);
    CHECK(again.accepted);
    REQUIRE(e.r.session().panels.has(e.kind));
    CHECK(e.dirty());
    CHECK(e.says("UNSAVED edits stand"));
    CHECK(e.doc_row(1) == "twXo");
    CHECK(e.status().rfind("UNSAVED L2:C4/3", 0) == 0); // the caret stood where it was
}

TEST_CASE("EDIT-W12: a dirty buffer refuses a different source, and a save opens the way") {
    // THE NO-SILENT-LOSS FLOOR, said to whoever asked.
    EditorRig e("edit-dirty");
    e.open();
    const std::string first = e.open_file("one.cpp", "int one;\n");
    put_bytes(e.root / "two.cpp", "int two;\n");
    e.press_doc(0, 0);
    e.type("x");
    REQUIRE(e.dirty());
    const SourceOpened refused = e.ask_open(spelled(e.root / "two.cpp"));
    CHECK_FALSE(refused.accepted);
    CHECK(refused.refusal.find("unsaved changes") != std::string::npos);
    CHECK(refused.refusal.find("one.cpp") != std::string::npos);
    CHECK(refused.refusal.find("save source or discard source edits") != std::string::npos);
    CHECK(e.status().find("one.cpp") != std::string::npos);
    CHECK(e.dirty());
    e.save();
    REQUIRE(e.clean());
    const SourceOpened opened = e.ask_open(spelled(e.root / "two.cpp"));
    CHECK(opened.accepted);
    CHECK(e.doc_row(0) == "int two;");
}

// ============================================================================
// CUSTODY: save, discard, and what a presentation cannot lose
// ============================================================================

TEST_CASE("EDIT-W13: dirty derives by comparison, a save writes the bytes, and an unchanged save says so plainly") {
    EditorRig e("edit-save");
    e.open();
    e.open_file("a.cpp", "ab\n");
    e.press_doc(0, 2);
    e.type("c");
    CHECK(e.dirty());
    e.key(input::scan::kBackspace);
    CHECK(e.clean()); // back to the saved text is clean, by comparison and not by counting
    e.type("c");
    e.save();
    CHECK(e.clean());
    CHECK(e.says("saved "));
    CHECK(bytes_of(e.root / "a.cpp") == "abc\n");
    e.save();
    CHECK(e.clean());
    CHECK(bytes_of(e.root / "a.cpp") == "abc\n");
}

TEST_CASE("EDIT-W14: a failed save keeps the buffer, keeps dirty, and speaks the writer's refusal") {
    EditorRig e("edit-savefail");
    e.open();
    e.open_file("a.cpp", "ab\n");
    e.press_doc(0, 2);
    e.type("c");
    // THE FILE'S PLACE BECOMES A DIRECTORY, so the atomic writer cannot rename over it.
    std::filesystem::remove(e.root / "a.cpp");
    std::filesystem::create_directories(e.root / "a.cpp");
    e.save();
    CHECK(e.dirty());
    CHECK(e.doc_row(0) == "abc");
    CHECK_FALSE(e.says("saved "));
    CHECK(std::filesystem::is_directory(e.root / "a.cpp"));
}

TEST_CASE("EDIT-W15: CRLF and the final-newline state round-trip; tabs stay tabs and Tab inserts one") {
    EditorRig e("edit-bytes");
    e.open();
    e.open_file("crlf.txt", "a\r\nb\r\n");
    e.press_doc(1, 1);
    e.type("c");
    e.save();
    CHECK(bytes_of(e.root / "crlf.txt") == "a\r\nbc\r\n");
    // No final newline stays no final newline.
    put_bytes(e.root / "bare.txt", "x");
    REQUIRE(e.ask_open(spelled(e.root / "bare.txt")).accepted);
    e.press_doc(0, 1);
    e.type("y");
    e.save();
    CHECK(bytes_of(e.root / "bare.txt") == "xy");
    // Tabs.
    put_bytes(e.root / "tabs.txt", "\tone\n");
    REQUIRE(e.ask_open(spelled(e.root / "tabs.txt")).accepted);
    CHECK(e.doc_row(0).rfind("    one", 0) == 0); // shown as spaces...
    e.press_doc(0, 7);
    e.key(input::scan::kTab);
    e.save();
    CHECK(bytes_of(e.root / "tabs.txt") == "\tone\t\n"); // ...saved as tabs
}

TEST_CASE("EDIT-W16: discard is deliberate, scoped, undoable, and honest about nothing to do") {
    EditorRig e("edit-discard");
    e.open();
    e.open_file("a.cpp", "one\n");
    e.discard();
    CHECK(e.says("nothing to discard"));
    e.press_doc(0, 3);
    e.type("!");
    REQUIRE(e.dirty());
    e.discard();
    CHECK(e.clean());
    CHECK(e.doc_row(0) == "one");
    CHECK(e.says("discarded unsaved edits"));
    CHECK(bytes_of(e.root / "a.cpp") == "one\n"); // the file was never touched
    e.key(input::scan::kZ, input::mod::kCtrl);
    CHECK(e.doc_row(0) == "one!"); // undo takes the discard back
    CHECK(e.dirty());
}

TEST_CASE("EDIT-W17: removing and reopening the pane cannot lose a byte, a caret, or a step of history") {
    // ⭐ PRESENTATION AND CUSTODY ARE TWO LIFETIMES. Workshop closes a presentation; the
    // document is the weave's and the weave is not unloaded. What comes back is not a copy
    // of rows but the buffer with its history -- the undo below is the measurement.
    EditorRig e("edit-lifetime");
    e.open();
    e.open_file("a.cpp", "one\ntwo\n");
    e.press_doc(1, 3);
    e.type("!");
    e.key(input::scan::kReturn);
    e.type("three");
    REQUIRE(e.dirty());
    REQUIRE(e.status().rfind("UNSAVED L3:C6/4", 0) == 0);
    e.unfocus();
    e.r.pick(editor_ref());
    REQUIRE_FALSE(e.r.session().panels.has(e.kind));
    e.r.pick(editor_ref());
    REQUIRE(e.r.session().panels.has(e.kind));
    e.chrome = 1;
    CHECK(e.dirty());
    CHECK(e.status().rfind("UNSAVED L3:C6/4", 0) == 0);
    CHECK(e.doc_row(1) == "two!");
    CHECK(e.doc_row(2) == "three");
    e.focus();
    e.key(input::scan::kZ, input::mod::kCtrl);
    CHECK(e.doc_row(2).empty());
    CHECK(e.doc_row(1) == "two!");
    e.key(input::scan::kZ, input::mod::kCtrl);
    e.key(input::scan::kZ, input::mod::kCtrl);
    CHECK(e.clean());
    CHECK(e.doc_row(1) == "two");
}

TEST_CASE("EDIT-W18: arranging the Editor pane moves its window and not one byte of its source") {
    EditorRig e("edit-arrange");
    e.open();
    e.open_file("a.cpp", "one\ntwo\n");
    e.press_doc(0, 1);
    e.type("Z");
    const std::string before = e.text();
    const Written moved = author_pane_place(e.r.session().setup.active, editor_ref(), subs(6),
                                            subs(4));
    REQUIRE_MESSAGE(moved.accepted, moved.refusal);
    e.focus();
    CHECK(e.text() == before);
    CHECK(e.dirty());
    CHECK(bytes_of(e.root / "a.cpp") == "one\ntwo\n");
}

TEST_CASE("EDIT-W19: the state a same-shape reload keeps is the DOCUMENT, and the shape says what it is not") {
    // ⭐ THE DECISION, PINNED AS A SHAPE. The document rides across a reload whole -- path,
    // bytes, the saved comparison, the convention, the epoch, the caret, the anchor, the
    // window -- as two Texts and nine Ints, so a four-megabyte source is eleven decoded cells
    // against Loom's budget of 65,536 rather than a list of a hundred thousand lines. What is
    // NOT in it is said by its absence: no undo history (a reload is a new incarnation, and
    // the history is the old one's), no paste in flight (its answer is correlated to an
    // incarnation that is gone), no wheel fraction, no follow flag.
    //
    // ⚠ THE RELOAD ITSELF IS WITNESSED IN `test_workshop_load.cpp`, over a real Kernel, a
    // real Manager and a staged image, document and all. What is pinned here is the shape,
    // because the shape is the decision.
    const std::shared_ptr<const loom::Schema> shape = loom::schema_of<pane::EditorPaneState>();
    REQUIRE(shape != nullptr);
    REQUIRE(shape->fields().size() == 11);
    CHECK(shape->fields()[0].name == "path");
    CHECK(shape->fields()[0].type.kind == loom::Kind::Text);
    CHECK(shape->fields()[1].name == "text");
    CHECK(shape->fields()[1].type.kind == loom::Kind::Text);
    CHECK(shape->fields()[2].name == "saved_text");
    CHECK(shape->fields()[2].type.kind == loom::Kind::Text);
    for (std::size_t i = 3; i < 11; ++i) {
        CAPTURE(i);
        CHECK(shape->fields()[i].type.kind == loom::Kind::Int);
    }
    for (const auto& f : shape->fields()) {
        CHECK(f.type.kind != loom::Kind::List);
    }
}

// ============================================================================
// THE EXIT: asked of the room, decided by the answer
// ============================================================================

TEST_CASE("EDIT-W20: an orderly quit refuses while source is unsaved, and proceeds once it is not") {
    // ⭐ THE ONE SYNCHRONOUS READ THE HOST MADE OF THE DOCUMENT BECAME AN ASK. `q` publishes
    // `PaneQuitRequested`; the pane answers about THIS instant; the host decides on the answer
    // and says the pane's own refusal on its notice line. There is no confirmation surface
    // and no armed second press.
    EditorRig e("edit-quit");
    e.open();
    e.open_file("a.cpp", "one\n");
    e.press_doc(0, 3);
    e.type("!");
    REQUIRE(e.dirty());
    CHECK_FALSE(e.quit_by_key());
    CHECK(e.r.session().notice.find("unsaved changes") != std::string::npos);
    CHECK(e.r.session().notice.find("a.cpp") != std::string::npos);
    CHECK(e.r.session().notice.find("Workshop stays open") != std::string::npos);
    // THE SAME DOOR FROM THE NATIVE CLOSE BOX.
    e.r.publish(loom::to_value(surface::SurfaceCloseRequested{}));
    CHECK_FALSE(e.r.host.quit);
    // SAVE, AND THE NEXT `q` ENDS THE PROCESS WITHOUT CEREMONY.
    e.focus();
    e.save();
    REQUIRE(e.clean());
    CHECK(e.quit_by_key());
}

TEST_CASE("EDIT-W21: an authoritative `no source open` permits the quit, and so does a clean one") {
    EditorRig e("edit-quit-clean");
    e.open();
    CHECK(e.no_source());
    CHECK(e.quit_by_key());
}

TEST_CASE("EDIT-W22: a Workshop with no custodian in the room quits at once") {
    // Nobody accepts the ask: zero answers are owed, and zero is the authoritative "nobody
    // holds anything". The count comes from Loom's own publication, not from a field.
    PaneRig r;
    r.mount_workshop();
    r.ready();
    r.extent(160, 48);
    r.key(input::scan::kQ);
    CHECK(r.host.quit);
}

TEST_CASE("EDIT-W23: a forged quit answer moves nothing -- only Loom's answer to the host's ask decides") {
    // ⚔ MUTATION: `on(PaneQuitAnswered)` reading `permitted` without `answers_ask()` and the
    // correlation. A stranger who could say "permitted" could end a process holding a maker's
    // work.
    EditorRig e("edit-quit-forged");
    e.open();
    e.open_file("a.cpp", "one\n");
    e.press_doc(0, 0);
    e.type("x");
    REQUIRE(e.dirty());
    e.asker_says([](DoorAsker&, loom::Mail& mail) {
        (void)mail.as_role(kDoorAskerOffice)
            .send_to_role(kWorkshopProvider, PaneQuitAnswered{pane::kEditorPane, true, ""});
    });
    CHECK_FALSE(e.r.host.quit);
    CHECK_FALSE(e.quit_by_key());
    CHECK_FALSE(e.r.host.quit);
    // ...AND A FORGERY THAT ARRIVES WHILE THE ROOM IS BEING ASKED decides nothing either: it
    // is not Loom's answer to the host's ask, so it is not counted, and the pane's own refusal
    // is what the host acts on. Staged in one poll: `q`, then the asker's forged permission,
    // which lands at the host BEFORE the pane's real answer does.
    e.unfocus();
    e.enqueue_key(input::scan::kQ);
    e.asker->next = [](DoorAsker&, loom::Mail& mail) {
        (void)mail.as_role(kDoorAskerOffice)
            .send_to_role(kWorkshopProvider, PaneQuitAnswered{pane::kEditorPane, true, ""});
    };
    (void)e.r.bus.send(e.asker->id, loom::Message(loom::to_value(SeatDo{}), loom::WeaveId{},
                                                  loom::WeaveId{}, 0));
    e.settle();
    CHECK_FALSE(e.r.host.quit);
    CHECK(e.r.session().notice.find("unsaved changes") != std::string::npos);
}

TEST_CASE("EDIT-W24: an edit racing the exit check is judged at the answer, and a refused quit costs no keystroke") {
    // ⭐ THE RACE, STAGED. `q` and a typed character arrive in ONE poll. While the room is
    // being asked nothing is routed: the character is HELD, the pane answers about the document
    // as it stands, and the host either ends the process (the character never dirtied anything)
    // or replays the character into the pane (a refused quit loses nothing a maker typed).
    EditorRig e("edit-quit-race");
    e.open();
    e.open_file("a.cpp", "one\n");
    e.press_doc(0, 3);
    e.type("!");
    REQUIRE(e.dirty());
    e.unfocus();
    // DIRTY: refused, and the character typed through the refusal lands where it was aimed.
    // (The keys are the workspace's when `q` fires, so the replayed text is COMMAND text --
    // consumed as nothing -- which is exactly what the maker's hands did; the point is that it
    // was replayed at all.) A press held through the ask is replayed too, and points the keys.
    e.enqueue_key(input::scan::kQ);
    const ui::Rect body = external_body_rect(e.r.session(), e.kind);
    (void)e.r.bus.publish(loom::Message(
        loom::to_value(input::PointerButton{1, true, body.x + 2,
                                            body.y + kExternalHeaderRows + 1 +
                                                surface::kTuiCanvasTopRow,
                                            input::space::kCells, input::mod::kNone}),
        loom::WeaveId{}, loom::WeaveId{}, 0));
    e.enqueue_text("?");
    e.enqueue_key(input::scan::kP); // a KEY held too: routed at once it would open the picker
    e.settle();
    CHECK_FALSE(e.r.host.quit);
    CHECK(e.r.session().notice.find("unsaved changes") != std::string::npos);
    CHECK(e.r.session().panels.keyboard == e.kind); // the held press was replayed...
    CHECK(e.doc_row(0) == "on?e!");                  // ...and the held text landed
    CHECK_FALSE(e.r.session().panels.picker.open);   // ...and the held key went to the pane
    // CLEAN, WITH AN EDIT RACING: the quit is answered on the clean document and the process
    // ends; the character never reached a pane, so nothing was dirtied and nothing was lost.
    e.save();
    REQUIRE(e.clean());
    e.unfocus();
    e.enqueue_key(input::scan::kQ);
    e.enqueue_text("z");
    e.settle();
    CHECK(e.r.host.quit);
}

TEST_CASE("EDIT-W25: a paste still arriving refuses the quit, because its answer could dirty the document") {
    EditorRig e("edit-quit-paste");
    e.open(160, 48, /*pick_it=*/true, /*slow_skin=*/true);
    e.open_file("a.cpp", "one\n");
    e.press_doc(0, 3);
    e.slow->text = "PASTED";
    e.key(input::scan::kV, input::mod::kCtrl);
    REQUIRE(e.slow->held);
    CHECK(e.clean());
    CHECK_FALSE(e.quit_by_key());
    CHECK(e.r.session().notice.find("clipboard answer") != std::string::npos);
    // The answer lands, dirties the document, and the next quit says THAT.
    e.answer_now();
    e.chrome = 1;
    CHECK(e.doc_row(0) == "onePASTED");
    CHECK(e.dirty());
    CHECK_FALSE(e.quit_by_key());
    CHECK(e.r.session().notice.find("unsaved changes") != std::string::npos);
}

// ============================================================================
// EDITING: keys, text, and the clipboard
// ============================================================================

TEST_CASE("EDIT-W26: printable text edits the source, and command letters stop being commands") {
    EditorRig e("edit-text");
    e.open();
    e.open_file("a.cpp", "one\n");
    e.press_doc(0, 3);
    e.key(input::scan::kQ);
    e.type("q");
    e.key(input::scan::kP);
    e.type("p");
    CHECK(e.doc_row(0) == "oneqp");
    CHECK_FALSE(e.r.host.quit);
    CHECK_FALSE(e.r.session().panels.picker.open);
}

TEST_CASE("EDIT-W27: ^c copies, does not quit, and the copy reaches the platform clipboard") {
    EditorRig e("edit-copy");
    e.open();
    e.open_file("a.cpp", "one two\n");
    e.press_doc(0, 0);
    e.key(input::scan::kRight, input::mod::kShift);
    e.key(input::scan::kRight, input::mod::kShift);
    e.key(input::scan::kRight, input::mod::kShift);
    e.key(input::scan::kC, input::mod::kCtrl);
    CHECK_FALSE(e.r.host.quit);
    CHECK(e.skin->platform == "one");
    CHECK(e.doc_row(0) == "one two");
}

TEST_CASE("EDIT-W28: ^o keeps its global object-document meaning while the Editor has the keys") {
    EditorRig e("edit-ctrl-o");
    e.open();
    e.open_file("a.cpp", "one\n");
    e.press_doc(0, 0);
    const std::string before = e.r.session().notice;
    e.key(input::scan::kO, input::mod::kCtrl);
    // The host's open ran (this rig names no document file, so it refused in its own
    // words), and the keys never left the pane; the chord was not a keystroke to it.
    CHECK(e.r.session().notice != before);
    CHECK(e.r.session().notice.find("document") != std::string::npos);
    CHECK(e.r.session().panels.keyboard == e.kind);
    CHECK(e.doc_row(0) == "one");
}

TEST_CASE("EDIT-W29: Escape means nothing in the Editor -- no mode closes, no text moves") {
    EditorRig e("edit-escape");
    e.open();
    e.open_file("a.cpp", "one\n");
    e.press_doc(0, 1);
    e.key(input::scan::kEscape);
    CHECK(e.r.session().panels.keyboard == e.kind);
    CHECK(e.doc_row(0) == "one");
    e.type("d");
    CHECK(e.doc_row(0) == "odne"); // a habitual Esc did not hand `d` to command mode
}

TEST_CASE("EDIT-W30: an empty Editor pane takes the keys and does nothing with them") {
    // ⚠ A NAMED CHANGE. The built-in left the keys to command mode while it held no document;
    // a runtime pane holds the keys from the press that pointed at it, whatever it shows.
    EditorRig e("edit-empty-keys");
    e.open();
    e.focus();
    e.key(input::scan::kQ);
    e.type("q");
    CHECK_FALSE(e.r.host.quit);
    CHECK(e.no_source());
    e.unfocus();
    e.r.key(input::scan::kQ);
    CHECK(e.r.host.quit);
}

TEST_CASE("EDIT-W31: copy here, paste there -- multiline, through the medium's own answer") {
    EditorRig e("edit-paste");
    e.open();
    e.open_file("a.cpp", "one\ntwo\n");
    e.press_doc(0, 0);
    e.key(input::scan::kA, input::mod::kCtrl);
    e.key(input::scan::kC, input::mod::kCtrl);
    CHECK(e.skin->platform == "one\ntwo\n");
    e.skin->platform = "alpha\nbeta";
    e.press_doc(1, 3);
    e.key(input::scan::kV, input::mod::kCtrl);
    CHECK(e.skin->clipboard_reads == 1);
    CHECK(e.doc_row(1) == "twoalpha");
    CHECK(e.doc_row(2) == "beta");
    CHECK(e.dirty());
    // ONE GESTURE: undo takes the whole paste back.
    e.key(input::scan::kZ, input::mod::kCtrl);
    CHECK(e.doc_row(1) == "two");
}

TEST_CASE("EDIT-W32: a late paste answer may not land at a caret that has since moved") {
    // ⭐ THE SETTLEMENT PINS THE WHOLE POSITION, as the host pinned it: a document that MOVED
    // between the ask and the answer gets a sentence instead of a paste.
    EditorRig e("edit-paste-late");
    e.open(160, 48, /*pick_it=*/true, /*slow_skin=*/true);
    e.open_file("a.cpp", "one\ntwo\n");
    e.press_doc(0, 3);
    e.slow->text = "LATE";
    e.key(input::scan::kV, input::mod::kCtrl);
    REQUIRE(e.slow->held);
    e.press_doc(1, 0); // the caret moves while the answer is on its way
    e.answer_now();
    e.chrome = 2;
    CHECK(e.says("after the source moved"));
    CHECK(e.doc_row(0) == "one");
    CHECK(e.doc_row(1) == "two");
    CHECK(e.clean());
    // ...AND A PASTE WHOSE DOCUMENT STOOD STILL LANDS.
    e.key(input::scan::kV, input::mod::kCtrl);
    REQUIRE(e.slow->held);
    e.answer_now();
    e.chrome = 1;
    CHECK(e.doc_row(1) == "LATEtwo");
}

TEST_CASE("EDIT-W33: a late answer for a replaced document is discarded whole") {
    EditorRig e("edit-paste-replaced");
    e.open(160, 48, /*pick_it=*/true, /*slow_skin=*/true);
    e.open_file("a.cpp", "one\n");
    put_bytes(e.root / "b.cpp", "bee\n");
    e.press_doc(0, 3);
    e.slow->text = "STRAY";
    e.key(input::scan::kV, input::mod::kCtrl);
    REQUIRE(e.slow->held);
    REQUIRE(e.ask_open(spelled(e.root / "b.cpp")).accepted); // the document that asked is gone
    e.answer_now();
    CHECK(e.doc_row(0) == "bee");
    CHECK(e.clean());
    CHECK_FALSE(e.says("STRAY"));
    CHECK_FALSE(e.says("after the source moved")); // silence: the dead draft's own fate
}

TEST_CASE("EDIT-W34: a clipboard holding non-ASCII refuses the paste, and typed non-ASCII is refused with a sentence") {
    EditorRig e("edit-ascii");
    e.open();
    e.open_file("a.cpp", "one\n");
    e.press_doc(0, 3);
    e.skin->platform = "caf\xc3\xa9";
    e.key(input::scan::kV, input::mod::kCtrl);
    e.chrome = 2;
    CHECK(e.says("outside plain ASCII"));
    CHECK(e.doc_row(0) == "one");
    CHECK(e.clean());
    e.r.text("\xc3\xa9");
    e.chrome = 2;
    CHECK(e.says("nothing was inserted"));
    CHECK(e.doc_row(0) == "one");
    CHECK(e.clean());
    // ...and a clipboard with breaks and tabs is flattened rather than refused.
    e.skin->platform = "a\tb\r\nc";
    e.press_doc(0, 3);
    e.key(input::scan::kV, input::mod::kCtrl);
    CHECK(e.doc_row(0) == "onea    b"); // the tab byte, shown at its stop
    CHECK(e.doc_row(1) == "c");
}

// ============================================================================
// THE POINTER AND THE VIEWPORT
// ============================================================================

TEST_CASE("EDIT-W35: a press places the caret through the same tab geometry the paint used, and the caret is published beside the rows") {
    EditorRig e("edit-press");
    e.open();
    e.open_file("a.cpp", "\tab\ncd\n");
    e.press_doc(0, 5); // inside `b`, past the four-column tab
    REQUIRE(e.seat() != nullptr);
    CHECK(e.seat()->caret_row == 1); // body lattice: the status row is 0
    CHECK(e.seat()->caret_col == 5);
    e.type("X");
    CHECK(e.doc_row(0) == "    aXb");
    // A PRESS ON THE STATUS ROW FOCUSES WITHOUT MOVING THE CARET.
    e.unfocus();
    press_pane(e.r, e.kind, 0, 0);
    CHECK(e.r.session().panels.keyboard == e.kind);
    CHECK(e.seat()->caret_row == 1);
    CHECK(e.seat()->caret_col == 6);
}

TEST_CASE("EDIT-W36: a drag sweeps a multiline selection, and the selection survives release") {
    // ⭐ THE ONE MOTION THAT CROSSES THE SEAM. The press records which pane the hand is in;
    // each motion resolves against that pane's body and crosses as `PaneDragged`; the release
    // ends the record and sends nothing, and the range it swept is still on screen.
    EditorRig e("edit-drag");
    e.open();
    e.open_file("a.cpp", "one\ntwo\nthree\n");
    e.press_doc(0, 1);
    e.motion_doc(1, 2);
    REQUIRE(e.seat() != nullptr);
    CHECK(e.seat()->sel_begin_row == 1);
    CHECK(e.seat()->sel_begin_col == 1);
    CHECK(e.seat()->sel_end_row == 2);
    CHECK(e.seat()->sel_end_col == 2);
    e.release_doc(1, 2);
    CHECK(e.seat()->sel_end_row == 2);
    CHECK(e.seat()->sel_end_col == 2);
    // The selection is real: typing replaces it.
    e.type("_");
    CHECK(e.doc_row(0) == "o_o");
    CHECK(e.doc_row(1) == "three");
    // ...AND A MOTION WITH THE BUTTON UP SWEEPS NOTHING.
    e.motion_doc(1, 3);
    CHECK(e.seat()->sel_begin_row == surface::kNoSelection);
}

TEST_CASE("EDIT-W37: a drag past the body's bottom edge steps the window, one row per motion") {
    EditorRig e("edit-drag-edge");
    e.open();
    std::string lines;
    for (int i = 1; i <= 30; ++i) {
        lines += "line " + std::to_string(i) + "\n";
    }
    e.open_file("a.cpp", lines);
    e.give_rows(6);
    e.press_doc(0, 0);
    // Five document rows fit under the status row; a hand below them steps the caret past
    // the window and the follow pulls the window after it.
    e.motion_doc(5, 0);
    CHECK(e.doc_row(0) == "line 2");
    e.motion_doc(5, 0);
    CHECK(e.doc_row(0) == "line 3");
    REQUIRE(e.seat() != nullptr);
    CHECK(e.seat()->sel_begin_row == 1); // the anchor is above the window: clipped to its top
    CHECK(e.seat()->sel_begin_col == 0);
    CHECK(e.seat()->sel_end_row == 5);   // the caret's own row, the last one shown
    CHECK(e.seat()->sel_end_col == 0);
    CHECK(e.seat()->caret_row == 5);
    // ...AND SCROLLED BACK UP, THE RANGE RUNS PAST THE WINDOW: it ends at the exclusive row
    // one past the last shown, `(rows, 0)`, which is the one position with no row a
    // reading-order range may honestly name (WL-CARET-03) -- and the caret, off screen, is
    // `kNoCaret` beside a selection that is still said.
    e.wheel(1.0);
    CHECK(e.doc_row(0) == "line 1");
    CHECK(e.seat()->sel_begin_row == 1);
    CHECK(e.seat()->sel_begin_col == 0);
    CHECK(e.seat()->sel_end_row == 6);
    CHECK(e.seat()->sel_end_col == 0);
    CHECK(e.seat()->caret_row == surface::kNoCaret);
}

TEST_CASE("EDIT-W38: a selection that runs above the window is clipped, and one wholly out of it is not said") {
    EditorRig e("edit-sel-clip");
    e.open();
    std::string lines;
    for (int i = 1; i <= 30; ++i) {
        lines += "line " + std::to_string(i) + "\n";
    }
    e.open_file("a.cpp", lines);
    e.give_rows(6);
    e.press_doc(0, 2);
    for (int i = 0; i < 12; ++i) {
        e.key(input::scan::kDown, input::mod::kShift);
    }
    // The caret is on line 13 and the window followed it; the anchor is above the window.
    CHECK(e.status().rfind("saved L13:C3/31", 0) == 0);
    REQUIRE(e.seat() != nullptr);
    CHECK(e.seat()->sel_begin_row == 1);
    CHECK(e.seat()->sel_begin_col == 0);
    CHECK(e.seat()->sel_end_row == 5);
    CHECK(e.seat()->sel_end_col == 2);
    CHECK(e.seat()->caret_row == 5);
    // SCROLL THE WHOLE RANGE OUT OF THE WINDOW: nothing to draw, so nothing is said.
    e.wheel(-1.0);
    e.wheel(-1.0);
    e.wheel(-1.0);
    e.wheel(-1.0);
    e.wheel(-1.0);
    CHECK(e.seat()->caret_row == surface::kNoCaret);
    CHECK(e.seat()->sel_begin_row == surface::kNoSelection);
    // ...and scrolling back up brings the range back with the caret out of the window: a
    // selection may stand with no caret (WL-CARET-03).
    for (int i = 0; i < 9; ++i) {
        e.wheel(1.0);
    }
    CHECK(e.seat()->caret_row == surface::kNoCaret);
    CHECK(e.seat()->sel_begin_row != surface::kNoSelection);
}

TEST_CASE("EDIT-W39: the wheel scrolls the body, moves no caret, and elsewhere reaches nothing") {
    EditorRig e("edit-wheel");
    e.open();
    std::string lines;
    for (int i = 1; i <= 30; ++i) {
        lines += "line " + std::to_string(i) + "\n";
    }
    e.open_file("a.cpp", lines);
    e.give_rows(6);
    e.press_doc(0, 0);
    e.wheel(-1.0); // away from the maker: the document scrolls up by kEditorWheelLines
    CHECK(e.doc_row(0) == "line " + std::to_string(1 + kEditorWheelLines));
    CHECK(e.seat()->caret_row == surface::kNoCaret); // the caret stayed on line 1, off screen
    CHECK(e.status().rfind("saved L1:C1", 0) == 0);
    e.wheel(1.0);
    CHECK(e.doc_row(0) == "line 1");
    // A WHEEL ON THE BARE WORKSPACE SCROLLS NOTHING.
    e.r.wheel_cell(-1.0, 0, screen_of(e.r.session()).h - 1);
    CHECK(e.doc_row(0) == "line 1");
    // ...AND THE NEXT CARET GESTURE BRINGS THE VIEW BACK.
    e.wheel(-1.0);
    e.wheel(-1.0);
    CHECK(e.doc_row(0) != "line 1");
    e.key(input::scan::kRight);
    CHECK(e.doc_row(0) == "line 1");
}

TEST_CASE("EDIT-W40: keyboard navigation scrolls the window and the caret never leaves it") {
    EditorRig e("edit-nav");
    e.open();
    std::string lines;
    for (int i = 1; i <= 30; ++i) {
        lines += "line " + std::to_string(i) + "\n";
    }
    e.open_file("a.cpp", lines);
    e.give_rows(6);
    e.press_doc(0, 0);
    for (int i = 0; i < 10; ++i) {
        e.key(input::scan::kDown);
    }
    CHECK(e.status().rfind("saved L11:C1", 0) == 0);
    CHECK(e.doc_row(4) == "line 11");
    CHECK(e.seat()->caret_row == 5);
    for (int i = 0; i < 20; ++i) {
        e.key(input::scan::kUp);
    }
    CHECK(e.doc_row(0) == "line 1");
    CHECK(e.seat()->caret_row == 1);
}

TEST_CASE("EDIT-W41: a horizontal window follows the caret and recovers the room an erase frees") {
    EditorRig e("edit-horizontal");
    e.open();
    e.open_file("a.cpp", std::string(60, 'a') + "\nshort\n");
    // A narrow pane: forty columns of body.
    const Written wrote = author_pane_size(e.r.session().setup.active, editor_ref(),
                                           PaneSize{pane_unit::kSubcells, subs(42)}, PaneSize{});
    REQUIRE_MESSAGE(wrote.accepted, wrote.refusal);
    e.focus();
    e.press_doc(0, 0);
    e.key(input::scan::kEnd);
    CHECK(e.doc_row(0).find('a') != std::string::npos);
    CHECK(e.doc_row(0).size() < 60);
    CHECK(e.seat()->caret_col > 0);
    CHECK(e.doc_row(1).empty()); // `short` is entirely left of the window
    // Erase back down, and the window comes back to the left edge.
    for (int i = 0; i < 40; ++i) {
        e.key(input::scan::kBackspace);
    }
    CHECK(e.doc_row(1) == "short");
    CHECK(e.doc_row(0) == std::string(20, 'a'));
}

TEST_CASE("EDIT-W42: a resize reconciles the viewport and does not strand the caret") {
    EditorRig e("edit-resize");
    e.open();
    std::string lines;
    for (int i = 1; i <= 30; ++i) {
        lines += "line " + std::to_string(i) + "\n";
    }
    e.open_file("a.cpp", lines);
    e.press_doc(0, 0);
    for (int i = 0; i < 20; ++i) {
        e.key(input::scan::kDown);
    }
    REQUIRE(e.status().rfind("saved L21:C1", 0) == 0);
    e.give_rows(4);
    // Three document rows, and the caret's line is one of them.
    REQUIRE(e.seat()->rows == 4);
    CHECK(e.seat()->caret_row != surface::kNoCaret);
    CHECK(e.doc_row(e.seat()->caret_row - 1) == "line 21");
}

TEST_CASE("EDIT-W43: in a room too small for both, the document keeps its rows and a notice stands in for the status row") {
    EditorRig e("edit-tiny");
    e.open();
    e.open_file("a.cpp", "one\ntwo\n");
    e.press_doc(0, 0); // clears the "editing ..." notice, so the two rows are status and text
    e.give_rows(2);
    CHECK(e.shown().size() == 2);
    CHECK(e.status().rfind("saved", 0) == 0);
    CHECK(e.doc_row(0) == "one");
    // A refusal in two rows: it takes the status row's place, and the document keeps its row.
    e.r.text("\xc3\xa9");
    CHECK(e.shown()[0].find("nothing was inserted") != std::string::npos);
    CHECK(e.shown()[1] == "one");
    // ONE ROW: the standing notice, and nothing else -- the caret has no row to be on.
    e.give_rows(1);
    CHECK(e.shown().size() == 1);
    CHECK(e.seat()->caret_row == surface::kNoCaret);
    e.type("z"); // ...and typing into it still edits the document, and clears the notice
    CHECK(e.shown().size() == 1);
    CHECK(e.shown()[0].rfind("UNSAVED", 0) == 0);
    e.give_rows(6);
    e.chrome = 1;
    CHECK(e.doc_row(0) == "zone");
}

TEST_CASE("EDIT-W44: long and tabbed lines are windowed by displayed columns, exactly") {
    EditorRig e("edit-long");
    e.open();
    e.open_file("a.cpp", "\t\tx" + std::string(200, 'y') + "\n");
    const Written wrote = author_pane_size(e.r.session().setup.active, editor_ref(),
                                           PaneSize{pane_unit::kSubcells, subs(22)}, PaneSize{});
    REQUIRE_MESSAGE(wrote.accepted, wrote.refusal);
    e.focus();
    CHECK(e.doc_row(0).rfind("        x", 0) == 0);
    CHECK(e.doc_row(0).size() <= 20);
    e.press_doc(0, 9); // the first y
    e.type("Q");
    CHECK(e.doc_row(0).rfind("        xQ", 0) == 0);
    e.key(input::scan::kEnd);
    CHECK(e.doc_row(0).find('x') == std::string::npos); // scrolled off to the left
    CHECK(e.seat()->caret_col >= 0);
}

TEST_CASE("EDIT-W45: a sweep in a pane that lost its seat ends, and sends nothing") {
    EditorRig e("edit-drag-gone");
    e.open();
    e.open_file("a.cpp", "one\ntwo\n");
    e.press_doc(0, 1);
    REQUIRE(e.r.session().text_drag.active);
    e.r.session().panels.keyboard = kNoPaneKind; // the keys put down with no gesture
    e.r.pick(editor_ref()); // the pane is removed mid-sweep (the picker's own keys)
    REQUIRE_FALSE(e.r.session().panels.has(e.kind));
    const ui::Rect body = external_body_rect(e.r.session(), e.kind);
    e.r.motion_cell(body.x + 2, body.y + 3);
    CHECK_FALSE(e.r.session().text_drag.active);
    // Back, and nothing was selected by a motion the pane never saw.
    e.r.pick(editor_ref());
    e.chrome = 1;
    CHECK(e.seat()->sel_begin_row == surface::kNoSelection);
}

TEST_CASE("EDIT-W46: a press begins a sweep only where it named a row of the body") {
    // The header and the padding are pressed for focus; they begin no sweep, so a motion after
    // them extends nothing.
    EditorRig e("edit-press-header");
    e.open();
    e.open_file("a.cpp", "one\ntwo\n");
    const ui::Rect body = external_body_rect(e.r.session(), e.kind);
    e.r.press_cell(body.x, body.y); // the host's own header row
    CHECK(e.r.session().panels.keyboard == e.kind);
    CHECK_FALSE(e.r.session().text_drag.active);
    e.r.release_cell(body.x, body.y);
    e.press_doc(0, 0);
    CHECK(e.r.session().text_drag.active);
    CHECK(e.r.session().text_drag.place == text_drag_place::kExternalPane);
    CHECK(e.r.session().text_drag.kind == e.kind);
    e.release_doc(0, 0);
    CHECK_FALSE(e.r.session().text_drag.active);
}

// ============================================================================
// THE HOST KNOWS NO EDITOR, AND THE IMAGE KNOWS NO HOST
// ============================================================================

TEST_CASE("EDIT-W47: the image that holds a document cannot reach the host") {
    // ⚠ A SOURCE READ, because it is the only instrument that can keep this claim: the
    // translation unit that holds a maker's source names nothing of the host's session, its
    // screen or its weave, and the build file links nothing that would bring them in.
    std::ifstream in(EDITOR_PANE_SOURCE);
    REQUIRE(in.good());
    std::stringstream buffer;
    buffer << in.rdbuf();
    const std::string source = buffer.str();
    REQUIRE_FALSE(source.empty());
    for (const char* forbidden : {"#include \"workshop/weave.hpp\"", "#include \"workshop/screen.hpp\"",
                                  "#include \"workshop/panel.hpp\"", "#include \"workshop/setup.hpp\"",
                                  "Session&", "HostContext", "WorkshopWeave", "session_."}) {
        CAPTURE(forbidden);
        CHECK(source.find(forbidden) == std::string::npos);
    }
    std::ifstream cmake(EDITOR_PANE_CMAKE);
    REQUIRE(cmake.good());
    std::stringstream cbuf;
    cbuf << cmake.rdbuf();
    const std::string build = cbuf.str();
    CHECK(build.find("zengine-workshop-logic") == std::string::npos);
    CHECK(build.find("zengine-workshop-vocabulary") != std::string::npos);
}

TEST_CASE("EDIT-W48: the editor this host used to compile is named by no presentation source") {
    // ⭐ INTR-1's tripwire, one pane later, and the hardest direction to keep: the Editor WAS
    // this host's -- its kind, its document, its keys, its context, its painter, its paste
    // owner, its drag place and the one synchronous read `quit()` made. Every one of those
    // identifiers is a coupling somebody could restore because a helper was convenient.
    std::vector<std::string> sources = presentation_sources();
    sources.push_back(WORKSHOP_HOST_CPP);
    for (const std::string& path : sources) {
        std::ifstream in(path);
        REQUIRE(in.good());
        std::stringstream buffer;
        buffer << in.rdbuf();
        // THE CODE, WITH ITS COMMENTS STRIPPED -- the register checker's own reading: a
        // retirement note that names what left is prose, and a tripwire that could not tell
        // prose from a branch would forbid explaining the code.
        const std::string source = code_only(buffer.str());
        for (const char* forbidden : {"panel::kEditor", "pane_key::kEditor", "KeyContext::kEditor",
                                      "kNoEditor", "Act::kEditorSave", "Act::kEditorDiscard",
                                      "Act::kEditorNewline", "Act::kEditorTab", "EditorState",
                                      "EditorBuffer", "session_.editor", "::editor_key(",
                                      "::editor_text(", "::editor_press(", "paint_editor(",
                                      "refresh_editor(", "PasteOwner::kEditor",
                                      "kEditorBody", "open_source(", "save_source(",
                                      "discard_source_edits(", "\"zengine.editor\"",
                                      "editor-pane/"}) {
            CHECK_MESSAGE(source.find(forbidden) == std::string::npos, path, " names '",
                          forbidden, "'");
        }
    }
    // ...AND THE HOST'S ONE MENTION OF THE OFFICE IS THE CONVERSION TABLE, where a retired
    // spelling is turned into the weave's own -- a historical fact about files already
    // written, and not a route.
    CHECK(std::string(pane_migration::kEditorProvider) == pane::kEditorPaneRole);
    CHECK(std::string(pane_migration::kEditorPane) == pane::kEditorPane);
}

// ============================================================================
// THE PROJECT READS THE FILE, NEVER THE BUFFER
// ============================================================================

TEST_CASE("EDIT-W49: recipes come from the saved file, never from an unsaved Editor buffer") {
    // ⭐ THE DURABLE-AUTHORSHIP CLAIM, from the pane's side now. The same path can be open in
    // the Editor and chosen as the recipe catalog, and the two are answering different
    // questions: what a maker is WRITING, and what this session currently MEANS. Joining them
    // -- consuming the buffer, or saving it first -- would make an unsaved draft into build
    // procedure. The host cannot even reach the buffer any more, which is the strongest form of
    // the claim; what is measured here is that the install reads the bytes on disk while the
    // Editor holds different ones.
    EditorRig e("edit-catalog");
    e.open();
    zengine::builder::SingleSourceRecipe one;
    one.source = "src/beta.cpp";
    one.links.push_back("loom::kernel");
    zengine::builder::Recipe beta;
    beta.id = "beta";
    beta.artifact = "beta";
    beta.single_source = one;
    put_bytes(e.root / "b.json", recipe_persist::to_text({beta}));
    const std::string durable = bytes_of(e.root / "b.json");

    // OPEN THE CATALOG IN THE EDITOR AND MAKE THE BUFFER DIFFER. One character at the caret is
    // enough, and it is deliberately one that makes the BUFFER unreadable as a catalog.
    const std::string path = e.open_file("b.json", durable);
    e.press_doc(0, 0);
    e.type("x");
    REQUIRE(e.dirty());

    // THE DURABLE FILE IS WHAT IS READ.
    CurrentRecipes owner;
    const Written installed = install_recipes(owner, path, e.root.generic_string(),
                                              e.r.host.project_dir, &HostContext::so_in);
    REQUIRE_MESSAGE(installed.accepted, installed.refusal);
    REQUIRE(owner.all().size() == 1);
    CHECK(owner.all()[0].id == "beta");
    // NOTHING WAS SAVED ON THE MAKER'S BEHALF, and the draft is still theirs.
    CHECK(bytes_of(e.root / "b.json") == durable);
    CHECK(e.dirty());

    // AND WHEN THEY DO SAVE, THE RELOAD READS WHAT THEY SAVED. The saved bytes are the buffer's,
    // which is not a catalog -- so the refusal here IS the proof that the file on disk is the
    // input, and that a same-path reload is a real read rather than a cached answer.
    e.save();
    REQUIRE(e.clean());
    CHECK(bytes_of(e.root / "b.json") != durable);
    const Written reread = install_recipes(owner, path, e.root.generic_string(),
                                           e.r.host.project_dir, &HostContext::so_in);
    CHECK_FALSE(reread.accepted);
    CHECK(owner.all().size() == 1); // a refused candidate installs nothing
}
