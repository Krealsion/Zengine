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

/// A PROJECT OWNER THAT TAKES THE ANSWER AWAY WITH IT -- the same `defer_answer()` the slow
/// Skin spends, one door over, so a case can put a real open between the pane's question and
/// the project's answer. A run whose owner never answers is the same shape with `AnswerNow`
/// never sent.
class SlowProject : public loom::WeaveBase<SlowProject, SeenState,
                                           loom::Accept<ProjectRootRequested, AnswerNow>,
                                           loom::Emit<ProjectRoot>> {
public:
    std::string root;
    bool held = false;
    int asked = 0;
    loom::DeferredAnswer answer;

    void on(const ProjectRootRequested&, loom::Mail& mail) {
        ++asked;
        answer = mail.defer_answer();
        held = answer.valid();
    }
    void on(const AnswerNow&, loom::Mail& mail) {
        if (!held) {
            return;
        }
        held = false;
        (void)loom::answer_deferred(answer, mail, ProjectRoot{root, std::string()});
    }
};

/// A PARTY THAT READS THE PANE'S DECLARED SURFACE -- `zen.PokeRead`, the door every woven
/// weave answers, asked of the REAL loaded image and answered from whatever it is holding
/// right now. It is the rig's own instrument as well as a subject: the row a notice occupies
/// is read off the pane rather than guessed at by the case, so a case aims at document row
/// `n` because the pane says where the document starts.
class PokeSeat : public loom::WeaveBase<PokeSeat, SeenState,
                                        loom::Accept<loom::Result, loom::Refused,
                                                     loom::PokeStructure>,
                                        loom::Emit<>> {
public:
    std::vector<std::pair<std::uint64_t, std::string>> answers;
    std::vector<std::pair<std::uint64_t, std::string>> refusals;
    std::vector<loom::PokeStructure> structures;

    void on(const loom::PokeStructure& d, loom::Mail&) { structures.push_back(d); }

    void on(const loom::Result& r, loom::Mail& mail) {
        answers.emplace_back(mail.correlation(), r.value);
    }
    void on(const loom::Refused& r, loom::Mail& mail) {
        refusals.emplace_back(mail.correlation(), r.reason);
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
    /// AUTHORED KEYMAP ROWS, written to a file this rig's host reads on its first surface --
    /// how a case moves a binding the way a maker does.
    std::vector<std::pair<std::string, std::string>> overrides;
    /// THE LOADED IMAGE, and the party that reads its declared fields.
    loom::WeaveId image{};
    PokeSeat* poke = nullptr;
    loom::WeaveId poke_id{};
    std::uint64_t poke_corr = 0;

    explicit EditorRig(const char* tag) : dir(tag) {
        root = dir.path();
        r.host.project_dir = root.generic_string();
        marks_path = (root / "workshop-marks.json").generic_string();
        r.host.recipe_source = [this](const std::string&) { return next_source; };
    }

    /// LOAD THE IMAGE AND, ORDINARILY, PICK THE PANE. A case about the reveal loads it and
    /// leaves the picking to the document's own arrival.
    /// WHICH PROJECT OWNER THIS RUN HAS: the read-only door that answers at once, one that
    /// holds its answer until a case releases it, or none at all.
    enum class Project { kDoor, kSlow, kNone };

    void open(std::int64_t width = 160, std::int64_t height = 48, bool pick_it = true,
              bool slow_skin = false, Project project = Project::kDoor) {
        if (!overrides.empty()) {
            const std::string path = (root / "keymap.json").generic_string();
            write_keymap_file(path, keymap_file_text("full", overrides));
            r.host.keymap_path = path;
        }
        r.mount_workshop();
        if (project == Project::kDoor) {
            mount_project_door();
        } else if (project == Project::kSlow) {
            mount_slow_project();
        }
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
        image = r.kernel.weave_id(pane::kEditorPaneStem);
        REQUIRE(image.value != 0);
        mount_poke_seat();
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

    void mount_poke_seat() {
        auto seat = std::make_unique<PokeSeat>();
        poke = seat.get();
        poke_id = r.bus.register_weave(std::move(seat), loom::Grant{}, std::string());
        poke->zen_set_self(poke_id);
    }

    /// ONE DECLARED FIELD OF THE LIVE PANE, as `zen.PokeRead` answers it.
    std::string read(const char* field) {
        const std::uint64_t corr = ++poke_corr;
        (void)r.bus.send(image, loom::Message(loom::to_value(loom::PokeRead{field}),
                                              loom::WeaveId{}, poke_id, corr));
        r.bus.drain_until_idle();
        for (const std::pair<std::uint64_t, std::string>& one : poke->answers) {
            if (one.first == corr) {
                return one.second;
            }
        }
        for (const std::pair<std::uint64_t, std::string>& one : poke->refusals) {
            if (one.first == corr) {
                FAIL_CHECK("reading `", field, "` was refused: ", one.second);
                return std::string();
            }
        }
        FAIL_CHECK("reading `", field, "` was never answered");
        return std::string();
    }

    /// WHICH COMPOSED ROWS ARE ABOVE THE DOCUMENT -- read off the pane, not tracked here:
    /// the status row always, plus a notice row wherever the room holds one under it. The
    /// pane's own composition rule, spent against the pane's own standing notice.
    std::int64_t chrome() {
        const ExternalPane* seated = seat();
        const std::int64_t rows = seated != nullptr ? seated->rows : 0;
        return (!read("notice").empty() && rows >= 3) ? 2 : 1;
    }

    SlowProject* slow_project = nullptr;
    loom::WeaveId slow_project_id{};

    void mount_slow_project() {
        auto door = std::make_unique<SlowProject>();
        slow_project = door.get();
        slow_project->root = root.generic_string();
        loom::Grant say;
        say.allow_to_any(ProjectRoot::zen_name, ProjectRoot::zen_version);
        slow_project_id = r.bus.register_weave(std::move(door), std::move(say),
                                               std::string(kProjectRole));
        slow_project->zen_set_self(slow_project_id);
    }

    void project_answers() {
        REQUIRE(slow_project != nullptr);
        (void)r.bus.send(slow_project_id, loom::Message(loom::to_value(AnswerNow{}),
                                                        loom::WeaveId{}, loom::WeaveId{}, 0));
        r.bus.drain_until_idle();
    }

    /// THE PANE'S DECLARED FIELDS, as `zen.PokeDescribe` lists them.
    std::vector<std::string> described() {
        const std::size_t before = poke->structures.size();
        (void)r.bus.send(image, loom::Message(loom::to_value(loom::PokeDescribe{}),
                                              loom::WeaveId{}, poke_id, ++poke_corr));
        r.bus.drain_until_idle();
        REQUIRE(poke->structures.size() == before + 1);
        std::vector<std::string> names;
        for (const loom::PokeFieldInfo& f : poke->structures.back().fields) {
            names.push_back(f.name);
        }
        return names;
    }

    /// WHERE DOCUMENT ROW `row` IS ON THE CANVAS, resolved ONCE, before a batch begins.
    ///
    /// ⚠ AND NOTHING IN HERE ADVANCES THE SCHEDULE (VD-27, VM-FIX-24). The first writing of
    /// these helpers called `chrome()` per gesture, and `chrome()` reads the pane's declared
    /// notice -- which drains the bus. So the "batch" delivered its own press before the
    /// motion was even queued, and the case that was meant to prove one-poll ordering proved
    /// two polls. An observation helper that moves the thing it observes is not an
    /// observation. `aim()` is called by the case BEFORE it enqueues anything, and the
    /// enqueue helpers spend the numbers it answered.
    struct Aim {
        std::int64_t x = 0;
        std::int64_t y = 0;
        std::int64_t above = 0;
    };
    Aim aim() {
        const ui::Rect body = external_body_rect(r.session(), kind);
        return Aim{body.x, body.y + kExternalHeaderRows + surface::kTuiCanvasTopRow, chrome()};
    }
    void enqueue_press_doc(const Aim& at, std::int64_t row, std::int64_t col) {
        (void)r.bus.publish(loom::Message(
            loom::to_value(input::PointerButton{1, true, at.x + col, at.y + at.above + row,
                                                input::space::kCells, input::mod::kNone}),
            loom::WeaveId{}, loom::WeaveId{}, 0));
    }
    void enqueue_motion_doc(const Aim& at, std::int64_t row, std::int64_t col) {
        (void)r.bus.publish(loom::Message(
            loom::to_value(input::PointerMoved{at.x + col, at.y + at.above + row, 0, 0,
                                               input::space::kCells, input::mod::kNone}),
            loom::WeaveId{}, loom::WeaveId{}, 0));
    }
    /// THE CARET WORKSHOP IS HOLDING, read WITHOUT draining -- what a case checks between
    /// enqueueing a batch and settling it, to prove nothing was delivered in between. Both
    /// numbers, because a press inside one row moves the column and not the row, and an
    /// oracle that watched only the row let a draining helper through (VD-27).
    std::pair<std::int64_t, std::int64_t> admitted_caret() {
        const ExternalPane* seated = seat();
        return seated == nullptr ? std::pair<std::int64_t, std::int64_t>{surface::kNoCaret, 0}
                                 : std::pair<std::int64_t, std::int64_t>{seated->caret_row,
                                                                         seated->caret_col};
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
        grant.allow_to_any(PaneRevealSettled::zen_name, PaneRevealSettled::zen_version);
        grant.allow_to_any(surface::SurfaceExtent::zen_name, surface::SurfaceExtent::zen_version);
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
        const std::int64_t above = chrome();
        const std::vector<std::string> rows = shown();
        const std::size_t at = static_cast<std::size_t>(above + n);
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

    /// A press on document row `row`, column `col` of the window. The row is resolved
    /// against the picture standing NOW, which is the one the maker would be looking at.
    void press_doc(std::int64_t row, std::int64_t col) { press_pane(r, kind, chrome() + row, col); }
    void motion_doc(std::int64_t row, std::int64_t col) {
        const ui::Rect body = external_body_rect(r.session(), kind);
        r.motion_cell(body.x + col, body.y + kExternalHeaderRows + chrome() + row);
    }
    void release_doc(std::int64_t row, std::int64_t col) {
        const ui::Rect body = external_body_rect(r.session(), kind);
        r.release_cell(body.x + col, body.y + kExternalHeaderRows + chrome() + row);
    }
    void wheel(double dy) {
        const ui::Rect body = external_body_rect(r.session(), kind);
        r.wheel_cell(dy, body.x + 2, body.y + kExternalHeaderRows + chrome());
    }

    void key(std::int64_t sc, std::int64_t mods = input::mod::kNone) { r.key(sc, mods); }
    void type(const std::string& s) {
        for (const char c : s) {
            r.text(std::string(1, c));
        }
    }
    void save() { r.key(input::scan::kS, input::mod::kCtrl); }
    void discard() { r.key(input::scan::kD, input::mod::kCtrl); }

    // ---- STAGING AN ORDER ON THE REAL BUS ------------------------------------------
    void enqueue_key(std::int64_t sc, std::int64_t mods = input::mod::kNone) {
        (void)r.bus.publish(loom::Message(loom::to_value(input::KeyPressed{sc, "", mods}),
                                          loom::WeaveId{}, loom::WeaveId{}, 0));
    }
    void enqueue_text(const std::string& s) {
        (void)r.bus.publish(loom::Message(loom::to_value(input::TextEntered{s}), loom::WeaveId{},
                                          loom::WeaveId{}, 0));
    }
    void settle() { r.bus.drain_until_idle(); }

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
        for (const v2::PaneActionRow& a : one->actions) {
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
    // ⭐ `document.save` IS `kUnlessOwned`: everywhere, unless the pane holding the keyboard
    // DECLARED that it stands in for it. That is the sentence `kNoEditor` used to spell by
    // naming one built-in, with the exception moved to where the exception lives -- so the
    // object document's save is still the maker's key in a layout name, a draft, and every
    // pane that holds no document of its own, and it is the Editor's inside the Editor.
    const Keymap k;
    CHECK(k.action_for(KeyContext::kCommand, input::scan::kS, input::mod::kCtrl) ==
          Act::kSaveDocument);
    CHECK(k.action_for(KeyContext::kNaming, input::scan::kS, input::mod::kCtrl) ==
          Act::kSaveDocument);
    CHECK(k.action_for(KeyContext::kDraft, input::scan::kS, input::mod::kCtrl) ==
          Act::kSaveDocument);
    CHECK(k.above_mode_action(KeyContext::kCommand, input::scan::kS, input::mod::kCtrl) ==
          Act::kSaveDocument);
    // A PANE THAT DECLARED NOTHING KEEPS IT; the Editor's own handle is what takes it away,
    // and that handle is not in this bare keymap.
    CHECK(k.above_mode_action(KeyContext::kPane, input::scan::kS, input::mod::kCtrl) ==
          Act::kSaveDocument);
    // `^o` stays global -- a pane holding the keys included -- and no pane may own it.
    CHECK(k.above_mode_action(KeyContext::kPane, input::scan::kO, input::mod::kCtrl) ==
          Act::kOpenDocument);
    // ...and the class is a superset of every mode, because what removes it is not a mode.
    CHECK(active_in(KeyContext::kUnlessOwned, KeyContext::kCommand));
    CHECK(active_in(KeyContext::kUnlessOwned, KeyContext::kPane));
    CHECK(contexts_intersect(KeyContext::kUnlessOwned, KeyContext::kPane));
    // THE PANE PROTOCOL'S SPELLING OF THE ID AND THE HOST'S CATALOG AGREE.
    REQUIRE(row_of_id(kOwnableDocumentSave) != nullptr);
    CHECK(row_of_id(kOwnableDocumentSave)->act == Act::kSaveDocument);

    // LIVE: ^s with the Editor holding the keys saves the SOURCE and not the document.
    EditorRig e("edit-ctrl-s");
    e.open();
    const std::string path = e.open_file("a.cpp", "one\n");
    CHECK(e.r.session().keymap.pane_supersedes(e.kind, kOwnableDocumentSave));
    e.press_doc(0, 3);
    e.type("x");
    REQUIRE(e.dirty());
    const std::string doc_notice = e.r.session().notice;
    e.save();
    CHECK(e.clean());
    CHECK(bytes_of(e.root / "a.cpp") == "onex\n");
    CHECK(e.r.session().notice == doc_notice); // the host's save never ran
    // ...AND THE HOST'S SAVE IS THE MAKER'S KEY AGAIN THE MOMENT THE KEYS ARE NOT THE PANE'S.
    e.unfocus();
    e.r.key(input::scan::kS, input::mod::kCtrl);
    CHECK(e.r.session().notice != doc_notice);
}

TEST_CASE("EDIT-W50: supersession is by name, so moving either key moves neither meaning") {
    // ⚔ THE TWO WAYS A GESTURE-MATCHING RULE WOULD BREAK, driven over the real pane: with
    // `editor.save` moved off ^s, ^s must still not save the object document while the
    // Editor holds the keys; with `document.save` moved onto another chord, that chord must
    // not save the object document there either. Neither pane row nor host row is where the
    // relationship lives -- the declaration is (WL-KEY-15).
    SUBCASE("the pane's own row moved, and the host's row is still stood down") {
        EditorRig e("edit-remap-pane");
        e.overrides.push_back({"editor.save", "ctrl+e"});
        e.open();
        e.open_file("a.cpp", "one\n");
        e.press_doc(0, 3);
        e.type("x");
        REQUIRE(e.dirty());
        const std::string before = e.r.session().notice;
        e.r.key(input::scan::kS, input::mod::kCtrl); // the object document's default chord
        CHECK(e.dirty());                            // no source save
        CHECK(e.r.session().notice == before);       // and no document save either
        CHECK(bytes_of(e.root / "a.cpp") == "one\n");
        e.r.key(input::scan::kE, input::mod::kCtrl); // where the maker put the source save
        CHECK(e.clean());
        CHECK(bytes_of(e.root / "a.cpp") == "onex\n");
    }
    SUBCASE("the host's row moved, and the pane still owns it") {
        EditorRig e("edit-remap-host");
        e.overrides.push_back({"document.save", "ctrl+y"});
        e.open();
        e.open_file("a.cpp", "one\n");
        e.press_doc(0, 3);
        e.type("x");
        const std::string before = e.r.session().notice;
        e.r.key(input::scan::kY, input::mod::kCtrl);
        CHECK(e.r.session().notice == before); // the object document was not saved here
        CHECK(e.dirty());
        e.unfocus();
        e.r.key(input::scan::kY, input::mod::kCtrl);
        CHECK(e.r.session().notice != before); // ...and it is that key everywhere else
    }
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

TEST_CASE("EDIT-W9: an opening that cannot be shown opens nothing, and the requester is told why") {
    // ⭐ ONE TRANSACTION (VD-26). An acquisition that ends in a pane nobody can see is not
    // an acquisition: the pane reads and judges the file, asks the desk to show it, and
    // installs only if the desk says yes. A screen with no slot therefore leaves the prior
    // document, the authored setup and the file itself exactly as they were, and the
    // requester -- Files, the Builder -- is answered with the picker's own refusal instead
    // of a success it would have to discover was hollow.
    EditorRig e("edit-noroom");
    e.open(160, kMinScreen.h, /*pick_it=*/false);
    // A DOCUMENT ALREADY OPEN, so the refusal has something to preserve.
    put_bytes(e.root / "first.cpp", "first\n");
    REQUIRE(e.ask_open(spelled(e.root / "first.cpp")).accepted);
    REQUIRE(e.r.session().panels.has(e.kind));
    e.press_doc(0, 2);
    const std::int64_t caret = e.seat()->caret_col;
    // ...AND THEN THE ONE STACK SLOT THE MINIMUM SCREEN HAS, TAKEN BY SOMETHING ELSE.
    e.unfocus(); // the picker is command mode's row, and the Editor is holding the keys
    e.r.pick(editor_ref()); // the picker's other direction: it removes an open pane
    e.r.pick(ref_of(panel::kPaneEditor));
    REQUIRE(e.r.session().panels.has(panel::kPaneEditor));
    REQUIRE_FALSE(e.r.session().panels.has(e.kind));
    put_bytes(e.root / "a.cpp", "held\n");
    const SourceOpened said = e.ask_open(spelled(e.root / "a.cpp"));
    CHECK_FALSE(said.accepted);
    CHECK(said.refusal == "no room for Editor on this screen -- make the window taller, "
                          "then p again");
    CHECK(e.r.session().notice == said.refusal);
    CHECK_FALSE(has_pane(e.r.session().setup.active, editor_ref())); // nothing authored
    // NOTHING MOVED IN THE PANE: the first document, its caret and its bytes stand.
    e.r.extent(160, 48);
    e.r.pick(editor_ref());
    REQUIRE(e.r.session().panels.has(e.kind));
    CHECK(e.doc_row(0) == "first");
    CHECK(e.status().find("first.cpp") != std::string::npos);
    CHECK(e.seat()->caret_col == caret);
    CHECK(e.read("path").find("first.cpp") != std::string::npos);
    // ...AND WITH ROOM, THE SAME REQUEST TAKES.
    const SourceOpened again = e.ask_open(spelled(e.root / "a.cpp"));
    CHECK_MESSAGE(again.accepted, again.refusal);
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
    REQUIRE(shape->fields().size() == 18);
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
    CHECK(e.says("after the source moved"));
    CHECK(e.doc_row(0) == "one");
    CHECK(e.doc_row(1) == "two");
    CHECK(e.clean());
    // ...AND A PASTE WHOSE DOCUMENT STOOD STILL LANDS.
    e.key(input::scan::kV, input::mod::kCtrl);
    REQUIRE(e.slow->held);
    e.answer_now();
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
    CHECK(e.says("outside plain ASCII"));
    CHECK(e.doc_row(0) == "one");
    CHECK(e.clean());
    e.r.text("\xc3\xa9");
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
    const std::int64_t above = e.chrome(); // the status row, and the standing notice
    e.press_doc(0, 5);                     // inside `b`, past the four-column tab
    REQUIRE(e.seat() != nullptr);
    CHECK(e.seat()->caret_row == above); // body lattice: the document starts under the chrome
    CHECK(e.seat()->caret_col == 5);
    e.type("X");
    CHECK(e.doc_row(0) == "    aXb");
    // A PRESS ON THE STATUS ROW FOCUSES WITHOUT MOVING THE CARET.
    e.unfocus();
    press_pane(e.r, e.kind, 0, 0);
    CHECK(e.r.session().panels.keyboard == e.kind);
    CHECK(e.seat()->caret_row == e.chrome());
    CHECK(e.seat()->caret_col == 6);
}

TEST_CASE("EDIT-W36: a drag sweeps a multiline selection, and the selection survives release") {
    // ⭐ THE ONE MOTION THAT CROSSES THE SEAM. The press records which pane the hand is in;
    // each motion resolves against that pane's body and crosses as `PaneDragged`; the release
    // ends the record and sends nothing, and the range it swept is still on screen.
    EditorRig e("edit-drag");
    e.open();
    e.open_file("a.cpp", "one\ntwo\nthree\n");
    const std::int64_t above = e.chrome();
    e.press_doc(0, 1);
    e.motion_doc(1, 2);
    REQUIRE(e.seat() != nullptr);
    CHECK(e.seat()->sel_begin_row == above);
    CHECK(e.seat()->sel_begin_col == 1);
    CHECK(e.seat()->sel_end_row == above + 1);
    CHECK(e.seat()->sel_end_col == 2);
    e.release_doc(1, 2);
    CHECK(e.seat()->sel_end_row == above + 1);
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
    const std::int64_t above = e.chrome();
    const std::int64_t below = 6 - above; // the first row under the last document row
    e.press_doc(0, 0);
    // A hand below the document rows steps the caret past the window, and the follow pulls
    // the window after it.
    e.motion_doc(below, 0);
    CHECK(e.doc_row(0) == "line 2");
    e.motion_doc(below, 0);
    CHECK(e.doc_row(0) == "line 3");
    REQUIRE(e.seat() != nullptr);
    CHECK(e.seat()->sel_begin_row == above); // the anchor is above the window: clipped
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
    CHECK(e.seat()->sel_begin_row == above);
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
    e.give_rows(2);
    // TWO ROWS: the notice cannot have one of its own -- it would leave the document none --
    // so it stands in for the status row, and the document keeps its row.
    CHECK(e.shown().size() == 2);
    CHECK(e.shown()[0].find("editing") != std::string::npos);
    CHECK(e.shown()[1] == "one");
    // ...AND WITH THE NOTICE SPENT BY AN ACT, THE STATUS ROW IS BACK.
    e.press_doc(0, 0);
    e.type("z");
    CHECK(e.status().rfind("UNSAVED", 0) == 0);
    CHECK(e.doc_row(0) == "zone");
    // A refusal in two rows: it takes the status row's place, and the document keeps its row.
    e.r.text("\xc3\xa9");
    CHECK(e.shown()[0].find("nothing was inserted") != std::string::npos);
    CHECK(e.shown()[1] == "zone");
    // ONE ROW: the standing notice, and nothing else -- the caret has no row to be on.
    e.give_rows(1);
    CHECK(e.shown().size() == 1);
    CHECK(e.seat()->caret_row == surface::kNoCaret);
    e.type("z"); // ...and typing into it still edits the document, and clears the notice
    CHECK(e.shown().size() == 1);
    CHECK(e.shown()[0].rfind("UNSAVED", 0) == 0);
    e.give_rows(6);
    CHECK(e.doc_row(0) == "zzone");
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

// ============================================================================
// THE CORRECTIONS (VD-26): what each defect cost, and what now holds instead
// ============================================================================

TEST_CASE("EDIT-W51: a relative path is the project's file, and means nothing until the project has said") {
    // ⚔ THE DEFECT: an unanswered project owner left `project_dir_` empty and the relative
    // spelling went to the filesystem unchanged -- so `relative.cpp` opened whatever the
    // PROCESS directory happened to hold, and a save then wrote to it. An owner that has not
    // answered and an owner that authoritatively named no project are different facts.
    const std::filesystem::path here = std::filesystem::current_path();
    const std::string name = "zen-edit-w51-probe.cpp";
    put_bytes(here / name, "process\n");

    SUBCASE("no owner has answered: the spelling is refused, and the process directory is not read") {
        EditorRig e("edit-rel-none");
        e.open(160, 48, true, false, EditorRig::Project::kNone);
        put_bytes(e.root / name, "project\n");
        const SourceOpened said = e.ask_open(name);
        CHECK_FALSE(said.accepted);
        CHECK(said.refusal.find("relative path") != std::string::npos);
        CHECK(said.refusal.find("full path") != std::string::npos);
        CHECK(e.no_source());
        CHECK(e.read("path").empty());
        CHECK(e.read("project_known") == "false");
        // ...AND AN ABSOLUTE PATH IS ITSELF UNDER EVERY CONDITION.
        const SourceOpened whole = e.ask_open(spelled(e.root / name));
        CHECK_MESSAGE(whole.accepted, whole.refusal);
        CHECK(e.doc_row(0) == "project");
    }

    SUBCASE("the owner answers late: what was opened by full path is not retargeted, and the project's file is what a relative spelling means") {
        EditorRig e("edit-rel-late");
        e.open(160, 48, true, false, EditorRig::Project::kSlow);
        REQUIRE(e.slow_project != nullptr);
        REQUIRE(e.slow_project->held); // the pane asked at activation; the answer is held
        put_bytes(e.root / name, "project\n");
        // BEFORE THE ANSWER: the relative spelling means nothing, the full one means itself.
        CHECK_FALSE(e.ask_open(name).accepted);
        const std::string full = spelled(here / name);
        REQUIRE(e.ask_open(full).accepted);
        CHECK(e.doc_row(0) == "process");
        CHECK(e.read("path") == full);
        // THE ANSWER ARRIVES, and it does not move a document already open under another
        // identity: the path it was opened under is the path it keeps.
        e.project_answers();
        CHECK(e.read("project_known") == "true");
        CHECK(e.read("path") == full);
        CHECK(e.doc_row(0) == "process");
        // ...AND NOW THE RELATIVE SPELLING IS THE PROJECT'S FILE, and a save writes THERE.
        REQUIRE(e.ask_open(name).accepted);
        CHECK(e.read("path") == spelled(e.root / name));
        CHECK(e.doc_row(0) == "project");
        e.press_doc(0, 7);
        e.type("!");
        e.save();
        CHECK(bytes_of(e.root / name) == "project!\n");
        CHECK(bytes_of(here / name) == "process\n"); // untouched
    }

    SUBCASE("an owner that names no project keeps the policy it always had") {
        EditorRig e("edit-rel-noroot");
        e.open(160, 48, true, false, EditorRig::Project::kSlow);
        REQUIRE(e.slow_project != nullptr);
        e.slow_project->root.clear(); // a run that began nowhere
        e.project_answers();
        CHECK(e.read("project_known") == "true");
        CHECK(e.read("project_dir").empty());
        // The spelling is spent as the maker wrote it -- the existing law, unchanged.
        const SourceOpened said = e.ask_open(name);
        CHECK_MESSAGE(said.accepted, said.refusal);
        CHECK(e.doc_row(0) == "process");
    }

    std::error_code ec;
    std::filesystem::remove(here / name, ec);
}

TEST_CASE("EDIT-W52: every field the pane advertises reports what it is holding now") {
    // ⚔ THE DEFECT: `snapshot()` was overridden to build the shape from the live buffer, and
    // Loom answers `zen.PokeRead` from `state_` -- which nothing wrote. A pane holding an
    // unsaved document answered `path` and `text` with empty strings, and a reloaded one
    // answered with the snapshot it revived from. One truth now, read two ways.
    EditorRig e("edit-poke");
    e.open();
    // NO SECRET STATE: the describe door lists every field, and every one of them reads.
    const std::vector<std::string> fields = e.described();
    CHECK(fields.size() == 18);
    for (const std::string& f : fields) {
        (void)e.read(f.c_str());
    }
    CHECK(std::find(fields.begin(), fields.end(), "path") != fields.end());
    CHECK(std::find(fields.begin(), fields.end(), "saved_text") != fields.end());
    // EMPTY IS EMPTY, and it is the truth here.
    CHECK(e.read("path").empty());
    CHECK(e.read("text").empty());

    const std::string path = e.open_file("a.cpp", "one\ntwo\n");
    CHECK(e.read("path") == path);
    CHECK(e.read("text") == "one\ntwo\n");
    CHECK(e.read("saved_text") == "one\ntwo\n");
    CHECK(e.read("first_row") == "0");
    // AN EDIT: `text` moves, the saved comparison does not, and dirty is the difference.
    e.press_doc(0, 3);
    e.type("X");
    CHECK(e.read("text") == "oneX\ntwo\n");
    CHECK(e.read("saved_text") == "one\ntwo\n");
    CHECK(e.read("caret_row") == "0");
    CHECK(e.read("caret_byte") == "4");
    CHECK(e.dirty());
    // A SAVE MOVES THE COMPARISON; A DISCARD MOVES THE TEXT BACK.
    e.save();
    CHECK(e.read("saved_text") == "oneX\ntwo\n");
    e.type("Y");
    CHECK(e.read("text") == "oneXY\ntwo\n");
    e.discard();
    CHECK(e.read("text") == "oneX\ntwo\n");
    CHECK(e.read("saved_text") == "oneX\ntwo\n");
    // ...AND A GESTURE THAT MOVES ONLY THE VIEW MOVES ONLY THE VIEW.
    const std::string epoch = e.read("doc_epoch");
    e.press_doc(1, 1);
    CHECK(e.read("caret_row") == "1");
    CHECK(e.read("doc_epoch") == epoch);
    CHECK(e.read("text") == "oneX\ntwo\n");
    // THE NOTICE AND THE PROJECT ARE DECLARED TOO, and they are what the pane is showing.
    CHECK(e.read("project_dir") == e.r.host.project_dir);
    CHECK(e.read("project_known") == "true");
    e.r.text("\xc3\xa9");
    CHECK(e.read("notice").find("nothing was inserted") != std::string::npos);
    CHECK(e.read("notice_bad") == "true");
}

TEST_CASE("EDIT-W53: a press that only focuses begins no sweep, and a gesture keeps the geometry it was made against") {
    // ⚔ TWO DEFECTS, BOTH ABOUT WHAT A POINTER GESTURE MEANT. Workshop takes hold of a pane
    // whenever a press names any row of its body -- it does not read the pane's rows -- so the
    // motions after a focus-only press arrived exactly as a real sweep's did and extended a
    // selection from wherever the caret had been left. And a press used to clear the standing
    // notice, moving the document up one row BETWEEN the press and a motion the same poll had
    // already queued, so a sweep to line two selected to line three.
    SUBCASE("a press on the status row focuses, and the motions after it sweep nothing") {
        EditorRig e("edit-focus-drag");
        e.open();
        e.open_file("a.cpp", "one\ntwo\nthree\n");
        e.press_doc(1, 1); // a real caret, somewhere to sweep from
        REQUIRE(e.seat() != nullptr);
        CHECK(e.seat()->sel_begin_row == surface::kNoSelection);
        e.release_doc(1, 1);
        // ...AND NOW A PRESS THAT MEANS FOCUS AND NOTHING ELSE.
        press_pane(e.r, e.kind, 0, 0);
        CHECK(e.r.session().panels.keyboard == e.kind);
        e.motion_doc(2, 4);
        e.motion_doc(2, 5);
        CHECK(e.seat()->sel_begin_row == surface::kNoSelection);
        CHECK(e.seat()->caret_row == e.chrome() + 1); // the caret did not move either
        e.release_doc(2, 5);
        CHECK(e.seat()->sel_begin_row == surface::kNoSelection);
        // A REAL SWEEP STILL SWEEPS, from a press that named a document row.
        e.press_doc(0, 0);
        e.motion_doc(1, 3);
        CHECK(e.seat()->sel_begin_row == e.chrome());
        CHECK(e.seat()->sel_end_row == e.chrome() + 1);
    }

    SUBCASE("a press and the motion behind it in one poll mean the picture the maker saw") {
        EditorRig e("edit-one-poll");
        e.open();
        e.open_file("a.cpp", "one\ntwo\nthree\n");
        REQUIRE(e.chrome() == 2); // "editing ..." is standing, and it is a row
        // ONE POLL: the press, then the motion, both measured against the rows on screen --
        // and the geometry is resolved BEFORE either is queued, because resolving it in
        // between would drain the bus and deliver the press (VD-27).
        const EditorRig::Aim at = e.aim();
        const std::pair<std::int64_t, std::int64_t> caret_before = e.admitted_caret();
        e.enqueue_press_doc(at, 0, 1);
        e.enqueue_motion_doc(at, 1, 2);
        // NOTHING HAS BEEN DELIVERED YET: the condition this case exists to arrange.
        CHECK(e.admitted_caret() == caret_before);
        e.settle();
        REQUIRE(e.seat() != nullptr);
        CHECK(e.seat()->sel_begin_col == 1);
        CHECK(e.seat()->sel_end_col == 2);
        // THE RANGE IS LINE ONE TO LINE TWO, and typing over it proves which lines they were.
        e.release_doc(1, 2);
        e.type("_");
        CHECK(e.doc_row(0) == "o_o");
        CHECK(e.doc_row(1) == "three");
    }
}

TEST_CASE("EDIT-W54: a paste retires with the document it was asked for") {
    // ⚔ THE DEFECT: installing another document advanced the epoch and left `awaiting` set.
    // The answer could never have landed -- and never did -- but the quit handler reads that
    // flag, so a CLEAN new document refused every exit for the rest of the session.
    EditorRig e("edit-paste-retire");
    e.open(160, 48, true, /*slow_skin=*/true);
    e.open_file("a.cpp", "one\n");
    e.press_doc(0, 3);
    e.key(input::scan::kV, input::mod::kCtrl);
    REQUIRE(e.slow->held); // the answer is on its way to a document that is about to go
    CHECK_FALSE(e.quit_by_key()); // while THAT document stands, the quit is refused
    CHECK(e.r.session().notice.find("clipboard answer") != std::string::npos);
    // ANOTHER DOCUMENT, CLEAN.
    put_bytes(e.root / "b.cpp", "two\n");
    REQUIRE(e.ask_open(spelled(e.root / "b.cpp")).accepted);
    CHECK(e.clean());
    CHECK(e.quit_by_key()); // ...and it may end
    // ...AND THE OLD ANSWER, ARRIVING NOW, INSERTS NOTHING AND SETTLES NOTHING.
    e.answer_now();
    CHECK(e.doc_row(0) == "two");
    CHECK(e.clean());
    CHECK(e.read("text") == "two\n");
}

TEST_CASE("EDIT-W55: a dirty document with no paste in flight still refuses the exit") {
    // THE GUARD THE RETIREMENT MUST NOT HAVE WEAKENED, asked the other way round.
    EditorRig e("edit-paste-retire-dirty");
    e.open(160, 48, true, /*slow_skin=*/true);
    e.open_file("a.cpp", "one\n");
    e.press_doc(0, 3);
    e.key(input::scan::kV, input::mod::kCtrl);
    REQUIRE(e.slow->held);
    put_bytes(e.root / "b.cpp", "two\n");
    REQUIRE(e.ask_open(spelled(e.root / "b.cpp")).accepted);
    e.press_doc(0, 3);
    e.type("Z");
    REQUIRE(e.dirty());
    CHECK_FALSE(e.quit_by_key());
    CHECK(e.r.session().notice.find("unsaved changes") != std::string::npos);
    // ...AND A PASTE ASKED FOR BY THE DOCUMENT THAT IS STILL HERE REFUSES IT TOO. (The quit
    // above handed the keys to the desk; `^s` is the object document's there.)
    e.focus();
    e.save();
    e.key(input::scan::kV, input::mod::kCtrl);
    REQUIRE(e.slow->held);
    CHECK_FALSE(e.quit_by_key());
    CHECK(e.r.session().notice.find("clipboard answer") != std::string::npos);
}

TEST_CASE("EDIT-W56: an opening in flight is a candidate and never a second document") {
    // THE COMMITMENT IS RE-JUDGED. The desk answers on a later delivery, and a maker can type
    // into the document while it decides: a room that was free when the question was asked is
    // not permission to replace a document that is dirty now.
    SUBCASE("an edit that races the answer keeps its document, and the requester is told") {
        EditorRig e("edit-open-race");
        e.open();
        e.open_file("a.cpp", "one\n");
        put_bytes(e.root / "b.cpp", "two\n");
        // ONE POLL: the request, then a keystroke behind it. The pane judges `b.cpp`, asks the
        // desk, and the text is delivered before the desk's answer.
        e.press_doc(0, 3);
        e.asker->next = [&e](DoorAsker& a, loom::Mail& mail) {
            a.ask(mail, kEditorRole, OpenSourceRequested{spelled(e.root / "b.cpp")});
        };
        (void)e.r.bus.send(e.asker->id, loom::Message(loom::to_value(SeatDo{}), loom::WeaveId{},
                                                      loom::WeaveId{}, 0));
        e.enqueue_text("Z");
        e.settle();
        REQUIRE_FALSE(e.asker->opens.empty());
        const SourceOpened said = e.asker->opens.back();
        CHECK_FALSE(said.accepted);
        CHECK(said.refusal.find("unsaved changes") != std::string::npos);
        CHECK(e.read("path").find("a.cpp") != std::string::npos);
        CHECK(e.doc_row(0) == "oneZ");
    }

    SUBCASE("a second request while one is in flight is refused in words a maker can act on") {
        EditorRig e("edit-open-twice");
        e.open();
        e.open_file("a.cpp", "one\n");
        put_bytes(e.root / "b.cpp", "two\n");
        put_bytes(e.root / "c.cpp", "three\n");
        e.asker->next = [&e](DoorAsker& a, loom::Mail& mail) {
            a.ask(mail, kEditorRole, OpenSourceRequested{spelled(e.root / "b.cpp")});
            a.ask(mail, kEditorRole, OpenSourceRequested{spelled(e.root / "c.cpp")});
        };
        const std::size_t before = e.asker->opens.size();
        (void)e.r.bus.send(e.asker->id, loom::Message(loom::to_value(SeatDo{}), loom::WeaveId{},
                                                      loom::WeaveId{}, 0));
        e.settle();
        // TWO ANSWERS, AND THE ORDER IS THE PROTOCOL'S: the refusal is immediate, the
        // acquisition's own answer waits for the desk. They are told apart by what they say.
        REQUIRE(e.asker->opens.size() == before + 2);
        int accepted = 0;
        int still_opening = 0;
        for (std::size_t i = before; i < e.asker->opens.size(); ++i) {
            accepted += e.asker->opens[i].accepted ? 1 : 0;
            still_opening +=
                e.asker->opens[i].refusal.find("still opening") != std::string::npos ? 1 : 0;
        }
        CHECK(accepted == 1);
        CHECK(still_opening == 1);
        CHECK(e.doc_row(0) == "two");
        // ...AND THE PANE IS NOT WEDGED: the next request takes.
        const SourceOpened again = e.ask_open(spelled(e.root / "c.cpp"));
        CHECK_MESSAGE(again.accepted, again.refusal);
        CHECK(e.doc_row(0) == "three");
    }

    SUBCASE("a reveal answer for a flight that already ended decides nothing") {
        EditorRig e("edit-open-stale");
        e.open();
        e.open_file("a.cpp", "one\n");
        // A FORGED ANSWER, with no ask behind it: the pane is waiting for nothing.
        e.asker_says([](DoorAsker&, loom::Mail& mail) {
            (void)mail.as_role(kDoorAskerOffice)
                .send_to_role(pane::kEditorPaneRole,
                              PaneRevealAnswered{pane::kEditorPane, true, std::string()});
        });
        CHECK(e.doc_row(0) == "one");
        CHECK(e.read("path").find("a.cpp") != std::string::npos);
    }
}

TEST_CASE("EDIT-W57: both acquisition routes end in one transaction") {
    // FILES' RETURN AND THE BUILDER'S `e` REACH THE SAME DOOR, and both learn the same thing
    // about a screen that cannot show the result: the answer they get is the refusal.
    EditorRig e("edit-routes");
    e.open(160, 48, /*pick_it=*/false);
    put_bytes(e.root / "r.cpp", "recipe\n");
    // THE BUILDER'S ROUTE: the project door resolves the recipe, then the Editor's door opens.
    e.next_source = HostContext::RecipeSource{true, "single_source",
                                              (e.root / "r.cpp").generic_string()};
    RecipeSourceSaid said;
    e.asker_says([&said](DoorAsker& a, loom::Mail& mail) {
        a.ask(mail, kProjectRole, RecipeSourceRequested{"demo"});
    });
    REQUIRE_FALSE(e.asker->sources.empty());
    said = e.asker->sources.back();
    REQUIRE_MESSAGE(said.accepted, said.refusal);
    const SourceOpened opened = e.ask_open(said.source);
    CHECK_MESSAGE(opened.accepted, opened.refusal);
    CHECK(e.doc_row(0) == "recipe");
    CHECK(e.r.session().panels.has(e.kind)); // and the desk shows it
}

// ============================================================================
// PART TWO'S CORRECTIONS (VD-27): the transaction's two owners, input ownership,
// the mirror's real cost, and the viewport a reveal must not move
// ============================================================================

TEST_CASE("EDIT-W58: a refused commitment leaves the desk exactly as it was") {
    // ⚔ THE DEFECT, REPRODUCED WITH REAL MESSAGES. Workshop used to author the pane, select
    // it and take the keyboard when it ANSWERED the reveal -- before the Editor had re-judged
    // and committed. So an acquisition that then refused (a paste answer landed on the open
    // document and dirtied it while the desk was deciding) left the desk holding a
    // presentation change belonging to an operation that never happened.
    EditorRig e("edit-race-refuse");
    e.open(160, 48, /*pick_it=*/true, /*slow_skin=*/true);
    const std::string a_path = e.open_file("a.cpp", "one\n");
    e.slow->text = "PASTED"; // what the platform answers with, when it is let go
    e.press_doc(0, 3);
    e.key(input::scan::kV, input::mod::kCtrl); // a paste, held by the slow Skin
    REQUIRE(e.slow->held);
    // THE PANE IS TAKEN OFF THE DESK: the document is the weave's and stays.
    e.unfocus();
    e.r.pick(editor_ref());
    REQUIRE_FALSE(e.r.session().panels.has(e.kind));
    REQUIRE_FALSE(has_pane(e.r.session().setup.active, editor_ref()));
    const std::int64_t selected_before = e.r.session().panels.selected;

    // ONE POLL, TWO STATEMENTS: ask for b.cpp, and release the clipboard answer behind it.
    // The bus is FIFO, so the answer lands while the reveal is in flight -- which is exactly
    // the window the defect lived in.
    put_bytes(e.root / "b.cpp", "two\n");
    const std::string b_path = spelled(e.root / "b.cpp");
    e.asker->next = [b_path](DoorAsker& a, loom::Mail& mail) {
        a.ask(mail, kEditorRole, OpenSourceRequested{b_path});
    };
    const std::size_t before = e.asker->opens.size();
    (void)e.r.bus.send(e.asker->id, loom::Message(loom::to_value(SeatDo{}), loom::WeaveId{},
                                                  loom::WeaveId{}, 0));
    (void)e.r.bus.send(e.slow_id, loom::Message(loom::to_value(AnswerNow{}), loom::WeaveId{},
                                                loom::WeaveId{}, 0));
    e.settle();

    REQUIRE(e.asker->opens.size() == before + 1);
    const SourceOpened said = e.asker->opens.back();
    CHECK_FALSE(said.accepted);
    CHECK(said.refusal.find("unsaved changes") != std::string::npos);
    // THE DOCUMENT THAT WAS THERE IS STILL THERE, with the pasted bytes in it.
    CHECK(e.read("path") == a_path);
    CHECK(e.read("text") == "onePASTED\n");
    // ...AND THE DESK DID NOT MOVE: no authored row, no selection, no keyboard.
    CHECK_FALSE(has_pane(e.r.session().setup.active, editor_ref()));
    CHECK_FALSE(e.r.session().panels.has(e.kind));
    CHECK(e.r.session().panels.selected == selected_before);
    CHECK(e.r.session().panels.keyboard != e.kind);
}

TEST_CASE("EDIT-W59: a desk that changed while the asker was committing seats what it can, and says so") {
    // ⚔ THE OTHER HALF OF THE SAME DEFECT: the capacity answer is a fact about an instant,
    // and the desk can change before the asker settles. Here the screen shrinks between the
    // Editor's commitment and Workshop's seating, driven by a real `SurfaceExtent` landing two
    // deliveries behind the request. The commitment stands -- the document IS open, because
    // nothing about the document failed -- the pane is on the desk, and Workshop says plainly
    // that this screen cannot show it.
    EditorRig e("edit-race-shrink");
    e.open(160, 48, /*pick_it=*/false);
    e.r.pick(ref_of(panel::kPaneEditor)); // the Pane Manager takes the stack ahead of it
    REQUIRE(e.r.session().panels.has(panel::kPaneEditor));
    put_bytes(e.root / "a.cpp", "held\n");
    const std::string a_path = spelled(e.root / "a.cpp");
    e.asker->next = [a_path](DoorAsker& a, loom::Mail& mail) {
        a.ask(mail, kEditorRole, OpenSourceRequested{a_path});
        a.ask(mail, kProjectRole, ProjectRootRequested{}); // the pacing hop, and a real ask
    };
    e.asker->then_root = [](DoorAsker&, loom::Mail& mail) {
        mail.publish(surface::SurfaceExtent{160, kMinScreen.h, 0, 0});
    };
    const std::size_t before = e.asker->opens.size();
    (void)e.r.bus.send(e.asker->id, loom::Message(loom::to_value(SeatDo{}), loom::WeaveId{},
                                                  loom::WeaveId{}, 0));
    e.settle();

    REQUIRE(e.asker->opens.size() == before + 1);
    const SourceOpened said = e.asker->opens.back();
    CHECK_MESSAGE(said.accepted, said.refusal); // the document opened: nothing about it failed
    CHECK(e.read("path") == a_path);
    CHECK(e.read("text") == "held\n");
    // THE DESK AGREES WITH ITSELF: the pane is authored, the screen has no room, and the
    // sentence says which -- the keys are NOT pointed at a pane nobody can see.
    CHECK(has_pane(e.r.session().setup.active, editor_ref()));
    CHECK_FALSE(e.r.session().panels.has(e.kind));
    CHECK(e.r.session().panels.keyboard != e.kind);
    CHECK(e.r.session().notice.find("no room to show it") != std::string::npos);
    // ...AND A WINDOW BIG ENOUGH SHOWS IT, WITH THE DOCUMENT IN IT, WITH NO SECOND REQUEST.
    e.r.extent(160, 48);
    REQUIRE(e.r.session().panels.has(e.kind));
    CHECK(e.doc_row(0) == "held");
}

TEST_CASE("EDIT-W60: a pane that is not on the desk acquires a source and is shown, with nothing else disturbed") {
    // THE CONTROL FOR THE TWO RACES: the ordinary hidden-pane acquisition, uninterfered with.
    EditorRig e("edit-hidden-open");
    e.open(160, 48, /*pick_it=*/false);
    REQUIRE_FALSE(e.r.session().panels.has(e.kind));
    const std::int64_t others = static_cast<std::int64_t>(e.r.session().setup.active.panes.size());
    put_bytes(e.root / "a.cpp", "held\n");
    const SourceOpened said = e.ask_open(spelled(e.root / "a.cpp"));
    CHECK_MESSAGE(said.accepted, said.refusal);
    CHECK(has_pane(e.r.session().setup.active, editor_ref()));
    REQUIRE(e.r.session().panels.has(e.kind));
    CHECK(e.r.session().panels.selected == e.kind);
    CHECK(e.r.session().panels.keyboard == e.kind);
    CHECK(e.doc_row(0) == "held");
    CHECK(static_cast<std::int64_t>(e.r.session().setup.active.panes.size()) == others + 1);
}

TEST_CASE("EDIT-W61: an acquisition outstanding across a removal still settles, and a late answer decides nothing") {
    EditorRig e("edit-flight-removal");
    e.open();
    e.open_file("a.cpp", "one\n");
    put_bytes(e.root / "b.cpp", "two\n");
    const std::string b_path = spelled(e.root / "b.cpp");
    // THE PANE LEAVES THE DESK IN THE SAME POLL THE REQUEST IS MADE IN: the request is
    // queued first, so the flight is outstanding when the picker's Return is delivered.
    e.asker->next = [b_path](DoorAsker& a, loom::Mail& mail) {
        a.ask(mail, kEditorRole, OpenSourceRequested{b_path});
    };
    const std::size_t before = e.asker->opens.size();
    (void)e.r.bus.send(e.asker->id, loom::Message(loom::to_value(SeatDo{}), loom::WeaveId{},
                                                  loom::WeaveId{}, 0));
    e.settle();
    REQUIRE(e.asker->opens.size() == before + 1);
    CHECK(e.asker->opens.back().accepted);
    CHECK(e.read("path") == b_path);

    // ...AND AN ANSWER TO A FLIGHT THAT ALREADY ENDED MOVES NOTHING. The office may forge
    // one; the correlation says it belongs to nothing outstanding.
    const std::string text_before = e.read("text");
    e.asker_says([](DoorAsker&, loom::Mail& mail) {
        (void)mail.as_role(kDoorAskerOffice)
            .send_to_role(pane::kEditorPaneRole,
                          PaneRevealAnswered{pane::kEditorPane, true, std::string()});
    });
    CHECK(e.read("text") == text_before);
    CHECK(e.read("path") == b_path);
}

TEST_CASE("EDIT-W62: the object document's save belongs to every context that is not the pane's own") {
    // ⚔ THE DEFECT: supersession consulted the REMEMBERED keyboard pane, and that memory
    // outlives the mode. A maker with the Editor focused who opened the contextual menu was
    // typing into the MENU -- and `^s` there wrote nothing at all, because the Editor's
    // handle still suppressed the host's save. Ownership is the resolved context and the
    // remembered pane together.
    const Keymap k;
    CHECK(k.row_active(*row_of_id("document.save"), KeyContext::kContext, 7));
    CHECK(k.row_active(*row_of_id("document.save"), KeyContext::kNaming, 7));
    CHECK(k.row_active(*row_of_id("document.save"), KeyContext::kPicker, 7));
    CHECK(k.row_active(*row_of_id("document.save"), KeyContext::kDraft, 7));
    CHECK(Keymap::owner_of(KeyContext::kPane, 7) == 7);
    CHECK(Keymap::owner_of(KeyContext::kContext, 7) == kNoPaneKind);

    // LIVE, THROUGH THE REAL PANE AND THE REAL MENU.
    EditorRig e("edit-context-save");
    e.open();
    e.open_file("a.cpp", "one\n");
    e.press_doc(0, 3);
    REQUIRE(e.r.session().panels.keyboard == e.kind);
    const std::string doc_path = (e.root / "doc.json").generic_string();
    e.r.host.document_path = doc_path;
    // THE CONTEXTUAL MENU, OPENED OVER THE PANE THE KEYS BELONG TO.
    e.r.right_press_cell(2, 2);
    REQUIRE(e.r.session().context.open);
    e.r.key(input::scan::kS, input::mod::kCtrl);
    CHECK(std::filesystem::exists(doc_path)); // the object document was written
    CHECK(e.clean());                         // ...and the SOURCE was not touched
    CHECK(e.read("text") == "one\n");
}

TEST_CASE("EDIT-W63: a pane that owns one action may put its other rows on that action's key") {
    constexpr std::int64_t kSomePane = kFirstRuntimeKind;
    // ⚔ THE DEFECT: the collision law exempted only the superseding ROW, though dispatch
    // suppresses the host action throughout the pane. So `editor.save` on `ctrl+e` beside
    // `editor.newline` on `ctrl+s` -- which the keymap before all this accepted -- was
    // refused, and a rejoin then dropped the pane's whole action set.
    Keymap k;
    const Gesture save = k.gesture_of(Act::kSaveDocument);
    std::vector<v2::PaneActionRow> rows;
    rows.push_back(v2::PaneActionRow{"x.save", "save source", input::scan::kE, input::mod::kCtrl,
                                     std::string(kOwnableDocumentSave)});
    rows.push_back(
        v2::PaneActionRow{"x.newline", "newline", save.scancode, save.modifiers, std::string()});
    const Written joined = join_pane_rows(k, kSomePane, rows);
    CHECK_MESSAGE(joined.accepted, joined.refusal);
    REQUIRE(k.pane_rows(kSomePane) != nullptr);
    CHECK(k.pane_rows(kSomePane)->rows.size() == 2);
    // BOTH ROWS RESOLVE, and the host's row is stood down for the whole pane.
    REQUIRE(k.pane_action_for(kSomePane, save.scancode, save.modifiers) != nullptr);
    CHECK(k.pane_action_for(kSomePane, save.scancode, save.modifiers)->id == "x.newline");
    CHECK(k.above_mode_action(KeyContext::kPane, save.scancode, save.modifiers, kSomePane) ==
          Act::kNone);
    // A GENUINE COLLISION IS STILL REFUSED: `^k` is a global row, and no pane may own it.
    Keymap other;
    const Gesture keys = other.gesture_of(Act::kHotkeys);
    std::vector<v2::PaneActionRow> clash;
    clash.push_back(v2::PaneActionRow{"x.save", "save source", input::scan::kE, input::mod::kCtrl,
                                      std::string(kOwnableDocumentSave)});
    clash.push_back(
        v2::PaneActionRow{"x.keys", "keys", keys.scancode, keys.modifiers, std::string()});
    const Written refused = join_pane_rows(other, kSomePane, clash);
    CHECK_FALSE(refused.accepted);
    CHECK(refused.refusal == collision_sentence(keys, "workshop.hotkeys", "x.keys"));
    CHECK(other.pane_rows(kSomePane) == nullptr); // and nothing was written
}

TEST_CASE("EDIT-W64: the mirror is rebuilt when the bytes move and at no other time") {
    // ⚔ THE DEFECT: the mirror was invalidated by the buffer's revision, which moves when the
    // CARET moves -- a pending paste has to notice that. So a press, a drag and an arrow key
    // each rebuilt the whole four-megabyte string with every byte identical. The count below
    // is the pane's own, declared and readable, so the cost is measured rather than claimed.
    EditorRig e("edit-mirror-work");
    e.open();
    std::string big;
    for (int i = 0; i < 4000; ++i) {
        big += "line " + std::to_string(i) + " with some ordinary source on it\n";
    }
    e.open_file("a.cpp", big);
    const std::int64_t after_open = std::stoll(e.read("text_builds"));
    CHECK(after_open >= 1); // opening one materializes it once

    // NAVIGATION, POINTING, SWEEPING, SCROLLING, RESIZING, FOCUSING: no bytes move.
    e.press_doc(1, 2);
    e.motion_doc(2, 4);
    e.motion_doc(3, 6);
    e.release_doc(3, 6);
    e.key(input::scan::kRight);
    e.key(input::scan::kDown);
    e.key(input::scan::kEnd, input::mod::kShift);
    e.wheel(-1.0);
    e.wheel(1.0);
    e.give_rows(12);
    e.unfocus();
    e.focus();
    CHECK(std::stoll(e.read("text_builds")) == after_open);
    CHECK(e.read("text") == big); // ...and it still answers the truth

    // AN EDIT MOVES THE BYTES, AND PAYS FOR IT ONCE.
    const std::string before_edit = e.read("text");
    e.type("Z");
    CHECK(std::stoll(e.read("text_builds")) == after_open + 1);
    CHECK(e.read("text") != before_edit);
    // (the shift+End above left a selection standing, so the typed byte REPLACES it -- what
    //  matters here is that the bytes moved once and the mirror was rebuilt once.)
    // SO DO NEWLINE, UNDO, REDO, DISCARD AND SAVE'S COMPARISON.
    const std::int64_t before_more = std::stoll(e.read("text_builds"));
    e.key(input::scan::kReturn);
    e.key(input::scan::kZ, input::mod::kCtrl);
    e.key(input::scan::kY, input::mod::kCtrl);
    CHECK(std::stoll(e.read("text_builds")) == before_more + 3);
}

TEST_CASE("EDIT-W65: a paste that arrives after the caret moved is still refused, bytes unchanged") {
    // THE GUARD THE CHEAPER MIRROR MUST NOT HAVE WEAKENED. The paste pins the buffer's own
    // revision, which MOVES ON MOVEMENT -- that is why the mirror needed a different question
    // rather than that one made cheaper.
    EditorRig e("edit-mirror-paste");
    e.open(160, 48, true, /*slow_skin=*/true);
    e.open_file("a.cpp", "one\ntwo\n");
    e.press_doc(0, 3);
    e.key(input::scan::kV, input::mod::kCtrl);
    REQUIRE(e.slow->held);
    const std::int64_t builds = std::stoll(e.read("text_builds"));
    e.press_doc(1, 0); // the caret moves; not one byte does
    CHECK(std::stoll(e.read("text_builds")) == builds);
    e.answer_now();
    CHECK(e.says("after the source moved"));
    CHECK(e.read("text") == "one\ntwo\n");
    CHECK(std::stoll(e.read("text_builds")) == builds);
}

TEST_CASE("EDIT-W66: asking for the open source again moves the pane, never the view") {
    // ⚔ THE DEFECT: a same-path acquisition set the follow flag, so re-opening the file a
    // maker had scrolled away from yanked the window back to the caret -- and clearing the
    // flag alone would not have been enough, because the notice it sets changes the rows the
    // document is given, which `reconcile` also called a resize.
    EditorRig e("edit-samepath-view");
    e.open();
    std::string many;
    for (int i = 1; i <= 40; ++i) {
        many += "line " + std::to_string(i) + "\n";
    }
    const std::string path = e.open_file("a.cpp", many);
    e.wheel(-2.0);
    const std::string scrolled = e.read("first_row");
    CHECK(scrolled != "0");
    CHECK(e.read("caret_row") == "0"); // the wheel moved no caret

    SUBCASE("the pane is on the desk") {
        const SourceOpened again = e.ask_open(path);
        CHECK_MESSAGE(again.accepted, again.refusal);
        CHECK(e.read("first_row") == scrolled);
        CHECK(e.read("caret_row") == "0");
        CHECK(e.doc_row(0) == "line " + std::to_string(std::stoll(scrolled) + 1));
    }
    SUBCASE("the pane was taken off the desk and comes back with it") {
        e.unfocus();
        e.r.pick(editor_ref());
        REQUIRE_FALSE(e.r.session().panels.has(e.kind));
        const SourceOpened again = e.ask_open(path);
        CHECK_MESSAGE(again.accepted, again.refusal);
        REQUIRE(e.r.session().panels.has(e.kind));
        CHECK(e.read("first_row") == scrolled);
    }
    SUBCASE("a horizontal offset is kept too, and a genuine room change still reconciles") {
        e.press_doc(0, 0);
        e.key(input::scan::kEnd);
        const std::string col = e.read("first_col");
        (void)col;
        e.wheel(-2.0);
        const std::string where = e.read("first_row");
        REQUIRE(e.ask_open(path).accepted);
        CHECK(e.read("first_row") == where);
        // A REAL RESIZE STILL PULLS THE CARET'S LINE INTO VIEW -- and this rig's Editor is
        // already six rows tall, so the room has to actually change to be a resize.
        REQUIRE(e.read("last_rows") == "6");
        e.give_rows(12);
        CHECK(e.read("last_rows") == "12");
        CHECK(std::stoll(e.read("first_row")) <= std::stoll(e.read("caret_row")));
    }
}
