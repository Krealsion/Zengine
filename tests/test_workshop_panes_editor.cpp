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

#include <zen/admission.hpp>
#include <zen/serialize.hpp>

#include <algorithm>
#include <chrono>
#include <fstream>
#include <optional>
#include <sstream>
#include <stdexcept>

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
/// STAND-INS FOR A BROKEN EDITOR (test
/// instrumentation, labeled as such). Each holds `zengine.editor` in place of the real image,
/// offers the pane so the desk has a row, and claims an (empty) document identity so an
/// operation can bind it -- the least a participant must do to be ASKED -- and then fails to
/// prepare in one of three ways: silently (never answers), abnormally (the handler throws),
/// or deafly (does not accept the ask at all). Nothing here publishes a document, routes an
/// input or seats a pane; what these prove is what the manager and the desk do when a real
/// participant does not do its part.
class BrokenEditor
    : public loom::WeaveBase<BrokenEditor, SeenState,
                             loom::Accept<PaneCatalogRequested, PrepareSourceRequested,
                                          ManagedOpenSettled, SeatDo>,
                             loom::Emit<PaneOffered>, loom::Claims<EditorDocument>> {
public:
    bool throws = false;
    int asked = 0;
    int settled = 0;
    void on(const PaneCatalogRequested&, loom::Mail& mail) {
        (void)mail.as_role(pane::kEditorPaneRole)
            .send_to_role(kWorkshopProvider,
                          PaneOffered{pane::kEditorPane, "Editor", "a stand-in that cannot prepare"});
    }
    void on(const PrepareSourceRequested&, loom::Mail&) {
        ++asked;
        if (throws) {
            throw std::runtime_error("the stand-in's preparation failed abnormally");
        }
    }
    void on(const ManagedOpenSettled&, loom::Mail&) { ++settled; }
    void on(const SeatDo&, loom::Mail& mail) { (void)mail.claim(EditorDocument{}); }
};

/// ...and one that does not even accept the ask: the bus refuses the manager's attempt at
/// dispatch, and says so to the manager alone (an authenticated `zen.DispatchRefused`).
class DeafEditor : public loom::WeaveBase<DeafEditor, SeenState,
                                          loom::Accept<PaneCatalogRequested, SeatDo>,
                                          loom::Emit<PaneOffered>, loom::Claims<EditorDocument>> {
public:
    void on(const PaneCatalogRequested&, loom::Mail& mail) {
        (void)mail.as_role(pane::kEditorPaneRole)
            .send_to_role(kWorkshopProvider,
                          PaneOffered{pane::kEditorPane, "Editor", "a stand-in that hears nothing"});
    }
    void on(const SeatDo&, loom::Mail& mail) { (void)mail.claim(EditorDocument{}); }
};

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
              bool slow_skin = false, Project project = Project::kDoor,
              bool with_manager = true, const char* stem = pane::kEditorPaneStem) {
        if (!overrides.empty()) {
            const std::string path = (root / "keymap.json").generic_string();
            write_keymap_file(path, keymap_file_text("full", overrides));
            r.host.keymap_path = path;
        }
        // The host names the managed pane before
        // Workshop is mounted, and mounts the opening manager beside it.
        r.host.managed_pane = editor_ref();
        r.mount_workshop();
        if (with_manager) {
            r.mount_opening();
        }
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
        seat.stem = stem;
        seat.weave = load::WeaveIntent{pane::kEditorPaneRole};
        plan.artifacts.push_back(seat);
        const load::Executed done = r.run_plan(plan);
        REQUIRE_MESSAGE(done.ok, done.refusal);
        image = r.kernel.weave_id(stem);
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
        grant.allow_to_any(surface::SurfaceExtent::zen_name, surface::SurfaceExtent::zen_version);
        grant.allow_to_any(PaneQuitAnswered::zen_name, PaneQuitAnswered::zen_version);
        // ...and the managed opening's own
        // sentences, for the same reason -- a forged settlement, a forged preparation, a
        // forged admission and a forged dispatch refusal must reach their parties to be
        // dropped by them.
        grant.allow_to_any(ManagedOpenSettled::zen_name, ManagedOpenSettled::zen_version);
        grant.allow_to_any(SourcePrepared::zen_name, SourcePrepared::zen_version);
        grant.allow_to_any(PresentationAdmitted::zen_name, PresentationAdmitted::zen_version);
        grant.allow_to_any(loom::DispatchRefused::zen_name, loom::DispatchRefused::zen_version);
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

    /// ASK THE MANAGED DOOR TO OPEN ONE PATH -- what a Return on a source row in Files
    /// crosses as -- and hand back what it answered. (the door is the opening
    /// manager's office; the Editor's own office is the direct, presentation-less door.)
    SourceOpened ask_open(const std::string& path) {
        const std::size_t before = asker->opens.size();
        asker_says([path](DoorAsker& a, loom::Mail& mail) {
            a.ask(mail, kOpeningRole, OpenSourceRequested{path});
        });
        REQUIRE(asker->opens.size() == before + 1);
        return asker->opens.back();
    }

    /// ...and the DIRECT door, for the cases about the document act alone.
    SourceOpened ask_open_direct(const std::string& path) {
        const std::size_t before = asker->opens.size();
        asker_says([path](DoorAsker& a, loom::Mail& mail) {
            a.ask(mail, kEditorRole, OpenSourceRequested{path});
        });
        REQUIRE(asker->opens.size() == before + 1);
        return asker->opens.back();
    }

    /// The manager's own readable record -- which open is pending, its stage and who it
    /// waits on -- as a case reads it off the weave the rig mounted.
    const OpeningState& opening() { return r.opening->state(); }

    // ---- STAGING A MANAGED OPEN TURN BY TURN ----------------------------------------------
    //
    // A managed open is a conversation of nine deliveries (request; trial asked; trial
    // answered; prepare asked; prepared; admit asked; admitted; THE COMMITMENT; the owners
    // and the requester told). `pump_pending` is one turn -- exactly the backlog that was
    // waiting -- so a case can put a real message at an exact interval of the flight and
    // watch what each party does with it. Nothing here performs a party's duty.

    /// QUEUE A MANAGED OPEN WITHOUT DRAINING: the asker's nudge is the next delivery, and
    /// the request is queued by it; the case pumps.
    void enqueue_open(const std::string& path) {
        asker->next = [path](DoorAsker& a, loom::Mail& mail) {
            a.ask(mail, kOpeningRole, OpenSourceRequested{path});
        };
        (void)r.bus.send(asker->id, loom::Message(loom::to_value(SeatDo{}), loom::WeaveId{},
                                                  loom::WeaveId{}, 0));
    }
    /// ...and the same at the OLD door.
    void enqueue_open_direct(const std::string& path) {
        asker->next = [path](DoorAsker& a, loom::Mail& mail) {
            a.ask(mail, kEditorRole, OpenSourceRequested{path});
        };
        (void)r.bus.send(asker->id, loom::Message(loom::to_value(SeatDo{}), loom::WeaveId{},
                                                  loom::WeaveId{}, 0));
    }

    /// PUMP ONE TURN AT A TIME UNTIL THE MANAGER SAYS IT STANDS AT `stage` -- its own record,
    /// which is what `zen.PokeRead` of it answers. `stage` is the ask the manager has just
    /// QUEUED: at "prepare" the Editor has not yet heard; at "admit" it has prepared and the
    /// desk has not yet admitted. Fails visibly if the stage never comes.
    void pump_until_stage(const char* stage, int turns = 16) {
        for (int i = 0; i < turns; ++i) {
            if (opening().stage == stage) {
                return;
            }
            (void)r.bus.pump_pending();
        }
        REQUIRE_MESSAGE(opening().stage == stage, "the opening never reached stage `", stage,
                        "`; it stands at `", opening().stage, "` awaiting `",
                        opening().awaiting, "` (", opening().last_outcome, ")");
    }

    /// A POKE THAT DOES NOT DRAIN: queued, so it is answered in the turn a case chooses.
    std::uint64_t enqueue_read(const char* field, loom::WeaveId of) {
        const std::uint64_t corr = ++poke_corr;
        (void)r.bus.send(of, loom::Message(loom::to_value(loom::PokeRead{field}), loom::WeaveId{},
                                           poke_id, corr));
        return corr;
    }
    std::optional<std::string> answered(std::uint64_t corr) {
        for (const std::pair<std::uint64_t, std::string>& one : poke->answers) {
            if (one.first == corr) {
                return one.second;
            }
        }
        return std::nullopt;
    }
    /// The manager's declared record, read the way a maker's probe would: `zen.PokeRead`.
    std::string read_opening(const char* field) {
        const std::uint64_t corr = enqueue_read(field, r.opening_id);
        r.bus.drain_until_idle();
        const std::optional<std::string> got = answered(corr);
        REQUIRE_MESSAGE(got.has_value(), "the manager never answered `", field, "`");
        return *got;
    }

    /// THE TWO PUBLISHED CLAIMS, read off the bus's latest-claim store -- what any weave
    /// observing the Editor or the desk would be answered with at this instant.
    std::optional<EditorDocument> document_claim() {
        const loom::SenseReading got =
            r.bus.observe(image, EditorDocument::zen_name, EditorDocument::zen_version);
        if (!got.value) {
            return std::nullopt;
        }
        return loom::from_value<EditorDocument>(*got.value);
    }
    std::optional<PanePresentation> presentation_claim() {
        const loom::SenseReading got = r.bus.observe(r.workshop_id, PanePresentation::zen_name,
                                                     PanePresentation::zen_version);
        if (!got.value) {
            return std::nullopt;
        }
        return loom::from_value<PanePresentation>(*got.value);
    }

    /// THE EDITOR'S SNAPSHOT, admitted against its declared shape -- the bytes a reload
    /// carries, read the way Loom reads them (and shown any publication of
    /// its own first).
    loom::Value snapshot() {
        const loom::Unverified claim = loom::parse(r.bus.snapshot_bytes(image));
        const loom::Admission admitted =
            loom::admit(claim, loom::schema_of<pane::EditorPaneState>());
        REQUIRE_MESSAGE(admitted.ok(), "the Editor's snapshot did not admit as EditorPaneState: ",
                        admitted.first_error().message());
        return admitted.value();
    }

    /// A BROKEN EDITOR IN PLACE OF THE IMAGE: the desk, the manager, the
    /// project door and the asker are the real ones; the Editor's role is held by a stand-in
    /// that offers, claims and then fails to prepare (see `BrokenEditor`, `DeafEditor`).
    BrokenEditor* broken = nullptr;
    loom::WeaveId broken_id{};
    void open_with_stand_in(bool throws, bool deaf) {
        r.host.managed_pane = editor_ref();
        r.mount_workshop();
        r.mount_opening();
        mount_project_door();
        skin = r.mount_skin_seat();
        loom::Grant offer;
        offer.allow_to_any(PaneOffered::zen_name, PaneOffered::zen_version);
        if (deaf) {
            auto seat = std::make_unique<DeafEditor>();
            DeafEditor* raw = seat.get();
            broken_id = r.bus.register_weave(std::move(seat), std::move(offer),
                                             std::string(pane::kEditorPaneRole));
            raw->zen_set_self(broken_id);
        } else {
            auto seat = std::make_unique<BrokenEditor>();
            broken = seat.get();
            broken->throws = throws;
            broken_id = r.bus.register_weave(std::move(seat), std::move(offer),
                                             std::string(pane::kEditorPaneRole));
            broken->zen_set_self(broken_id);
        }
        mount_poke_seat();
        r.ready();
        r.extent(160, 48);
        REQUIRE_MESSAGE(row() != nullptr, "the stand-in offered no `editor` pane");
        kind = row()->kind;
        // THE STAND-IN CLAIMS ITS (EMPTY) DOCUMENT IDENTITY, so an operation can bind it.
        (void)r.bus.send(broken_id, loom::Message(loom::to_value(SeatDo{}), loom::WeaveId{},
                                                  loom::WeaveId{}, 0));
        r.bus.drain_until_idle();
        mount_asker();
    }

    /// THE IMAGE LOADED UNDER A ROLE THAT IS NOT ITS OWN: everything the Editor says AS `zengine.editor` is then refused at the
    /// authorship -- its offer, its project ask, and the relay it would make for an open
    /// asked of it directly. No pane is offered, so nothing here requires a row.
    void open_under_role(const char* role) {
        r.host.managed_pane = editor_ref();
        r.mount_workshop();
        r.mount_opening();
        mount_project_door();
        skin = r.mount_skin_seat();
        load::LoadPlan plan;
        load::ArtifactIntent seat;
        seat.stem = pane::kEditorPaneStem;
        seat.weave = load::WeaveIntent{role};
        plan.artifacts.push_back(seat);
        const load::Executed done = r.run_plan(plan);
        REQUIRE_MESSAGE(done.ok, done.refusal);
        image = r.kernel.weave_id(pane::kEditorPaneStem);
        REQUIRE(image.value != 0);
        mount_poke_seat();
        r.ready();
        r.extent(160, 48);
        mount_asker();
    }

    /// "THE MAKER REBUILT THE EDITOR": a copy of the same image, reloaded in place through the
    /// real control door at its place in the queue (see `PaneRig::enqueue_reload`).
    int reloads = 0;
    void enqueue_reload(const char* stem = pane::kEditorPaneStem) {
        const std::filesystem::path copy =
            root / ("editor-rebuilt-" + std::to_string(++reloads) + ".so");
        std::error_code ec;
        std::filesystem::copy_file(PaneRig::artifact_path(stem), copy,
                                   std::filesystem::copy_options::overwrite_existing, ec);
        REQUIRE_MESSAGE(!ec, "cannot copy the image: ", ec.message());
        r.enqueue_reload(stem, copy.generic_string());
    }

    /// ...AND A REBUILD THAT CHANGES THE CODE: the
    /// record loaded as `record` is reloaded in place from a copy of ANOTHER image, `image`,
    /// through the same door -- the repair of an owner whose image could not apply a claim,
    /// by the maker's corrected build of it.
    void enqueue_reload_into(const char* record, const char* image_stem) {
        const std::filesystem::path copy =
            root / ("editor-rebuilt-" + std::to_string(++reloads) + ".so");
        std::error_code ec;
        std::filesystem::copy_file(PaneRig::artifact_path(image_stem), copy,
                                   std::filesystem::copy_options::overwrite_existing, ec);
        REQUIRE_MESSAGE(!ec, "cannot copy the image: ", ec.message());
        r.enqueue_reload(record, copy.generic_string());
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
    // an acquisition: the pane reads and judges the file, asks the desk to seat it, and
    // installs on the desk's word that it did. A screen with no slot therefore leaves the prior
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
    // incarnation that is gone), no candidate (a preparation is the old
    // incarnation's conversation), no wheel fraction, no follow flag. `opened_by`
    // (one Int) names the managed operation that installed the document.
    //
    // ⚠ THE RELOAD ITSELF IS WITNESSED IN `test_workshop_load.cpp`, over a real Kernel, a
    // real Manager and a staged image, document and all. What is pinned here is the shape,
    // because the shape is the decision.
    const std::shared_ptr<const loom::Schema> shape = loom::schema_of<pane::EditorPaneState>();
    REQUIRE(shape != nullptr);
    REQUIRE(shape->fields().size() == 19);
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

TEST_CASE("EDIT-W33: a paste still arriving refuses another source, and its answer lands where it was asked") {
    // RETARGETED for the managed opening. This case pinned "a late answer
    // for a replaced document is discarded whole": the open replaced A under the maker's own
    // paste, and the answer was stranded, silently. The founder's guarantee names admitted
    // input that must not be dropped, and the paste is the maker's -- so the open now waits
    // for it, in words (the quit's rule, WL-EDIT-14, one operation over), and the answer
    // lands in the document that asked. The stranding law still holds for the one way a
    // document can be replaced under a paste: a reload (`test_workshop_load.cpp`, RELOAD-4).
    EditorRig e("edit-paste-replaced");
    e.open(160, 48, /*pick_it=*/true, /*slow_skin=*/true);
    const std::string a_path = e.open_file("a.cpp", "one\n");
    put_bytes(e.root / "b.cpp", "bee\n");
    e.press_doc(0, 3);
    e.slow->text = "STRAY";
    e.key(input::scan::kV, input::mod::kCtrl);
    REQUIRE(e.slow->held);
    const SourceOpened waited = e.ask_open(spelled(e.root / "b.cpp"));
    CHECK_FALSE(waited.accepted);
    CHECK(waited.refusal.find("clipboard answer") != std::string::npos);
    CHECK(e.r.session().notice == waited.refusal); // the desk says why
    CHECK(e.read("path") == a_path);              // the document that asked is still here
    e.answer_now();
    CHECK(e.doc_row(0) == "oneSTRAY"); // ...and its paste landed in it
    CHECK(e.dirty());
    // THE OPEN IS ELIGIBLE AGAIN THE MOMENT THE ANSWER IS CONSUMED: refused now for the
    // unsaved paste, which is the floor and not the flight; a discard opens the way.
    const SourceOpened dirty = e.ask_open(spelled(e.root / "b.cpp"));
    CHECK_FALSE(dirty.accepted);
    CHECK(dirty.refusal.find("unsaved changes") != std::string::npos);
    e.focus();
    e.discard();
    REQUIRE(e.clean());
    const SourceOpened opened = e.ask_open(spelled(e.root / "b.cpp"));
    CHECK_MESSAGE(opened.accepted, opened.refusal);
    CHECK(e.doc_row(0) == "bee");
    CHECK_FALSE(e.says("STRAY"));
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
    CHECK(fields.size() == 19);
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
    //
    // RESTAGED for the managed opening: an open no longer replaces a
    // document under its own paste (WL-EDIT-05) -- it is refused in words until the answer
    // is consumed -- so the flight retires the way it is spent, by its answer, cleared before
    // the payload is judged; `install` still clears it as the belt under that.
    EditorRig e("edit-paste-retire");
    e.open(160, 48, true, /*slow_skin=*/true);
    e.open_file("a.cpp", "one\n");
    e.press_doc(0, 3);
    e.key(input::scan::kV, input::mod::kCtrl);
    REQUIRE(e.slow->held); // the answer is on its way
    CHECK_FALSE(e.quit_by_key()); // while THAT paste is outstanding, the quit is refused...
    CHECK(e.r.session().notice.find("clipboard answer") != std::string::npos);
    put_bytes(e.root / "b.cpp", "two\n");
    const SourceOpened waited = e.ask_open(spelled(e.root / "b.cpp"));
    CHECK_FALSE(waited.accepted); // ...and so is another document
    CHECK(waited.refusal.find("clipboard answer") != std::string::npos);
    // THE ANSWER ARRIVES -- an empty clipboard, which pastes nothing -- and retires the flight.
    e.answer_now();
    CHECK(e.doc_row(0) == "one");
    CHECK(e.clean());
    // A CLEAN DOCUMENT WITH NO PASTE IN FLIGHT: the open takes, and the exit may end.
    REQUIRE(e.ask_open(spelled(e.root / "b.cpp")).accepted);
    CHECK(e.doc_row(0) == "two");
    CHECK(e.clean());
    CHECK(e.quit_by_key());
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
    // The open waits for the paste (WL-OPEN-03), which pastes nothing here.
    CHECK_FALSE(e.ask_open(spelled(e.root / "b.cpp")).accepted);
    e.answer_now();
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
    SUBCASE("a keystroke queued behind a request at the OLD door lands in the current document, and the open is refused for it") {
        // THE OLD DOOR KEEPS THE MANAGED MEANING:
        // `OpenSourceRequested` at `zengine.editor` is relayed to the opening manager, so a
        // keystroke queued behind the request is admitted A work exactly as it is behind a
        // managed request -- A is dirty when the Editor is asked to prepare B, the open is
        // refused in the floor's own words, nothing is held, and the desk did not move. (The
        // Step 1 slice made this door a document-only install that put the keystroke into
        // B; that weaker meaning is gone.)
        EditorRig e("edit-open-race");
        e.open();
        const std::string a_path = e.open_file("a.cpp", "one\n");
        put_bytes(e.root / "b.cpp", "two\n");
        e.press_doc(0, 3);
        const std::size_t before = e.asker->opens.size();
        e.asker->next = [&e](DoorAsker& a, loom::Mail& mail) {
            a.ask(mail, kEditorRole, OpenSourceRequested{spelled(e.root / "b.cpp")});
        };
        (void)e.r.bus.send(e.asker->id, loom::Message(loom::to_value(SeatDo{}), loom::WeaveId{},
                                                      loom::WeaveId{}, 0));
        e.enqueue_text("Z");
        e.settle();
        REQUIRE(e.asker->opens.size() == before + 1);
        const SourceOpened said = e.asker->opens.back();
        CHECK_FALSE(said.accepted);
        CHECK(said.refusal.find("unsaved changes") != std::string::npos);
        CHECK(e.read("path") == a_path);
        CHECK(e.read("text") == "oneZ\n");
        CHECK(e.dirty());
        CHECK(bytes_of(e.root / "a.cpp") == "one\n"); // ...and A's file was never touched
        CHECK(e.r.session().panels.keyboard == e.kind);
        CHECK(e.opening().last_outcome == "refused");
        CHECK(e.opening().stage == "idle");
    }

    SUBCASE("a keystroke ahead of the request dirties the document, and the request is refused at the judge") {
        // THE OTHER SIDE OF THE SAME BOUNDARY: a keystroke the pane applied BEFORE it judged
        // made the document dirty, so the judge refuses when the Editor is asked to prepare
        // -- no seat, no keys, no candidate; the desk's trial moved nothing and the desk says
        // the refusal, as it does for the managed door the old door now relays to.
        EditorRig e("edit-open-race-before");
        e.open();
        e.open_file("a.cpp", "one\n");
        put_bytes(e.root / "b.cpp", "two\n");
        e.press_doc(0, 3);
        const std::string notice = e.r.session().notice;
        e.enqueue_text("Z");
        e.asker->next = [&e](DoorAsker& a, loom::Mail& mail) {
            a.ask(mail, kEditorRole, OpenSourceRequested{spelled(e.root / "b.cpp")});
        };
        (void)e.r.bus.send(e.asker->id, loom::Message(loom::to_value(SeatDo{}), loom::WeaveId{},
                                                      loom::WeaveId{}, 0));
        e.settle();
        REQUIRE_FALSE(e.asker->opens.empty());
        const SourceOpened said = e.asker->opens.back();
        CHECK_FALSE(said.accepted);
        CHECK(said.refusal.find("unsaved changes") != std::string::npos);
        CHECK(e.read("path").find("a.cpp") != std::string::npos);
        CHECK(e.doc_row(0) == "oneZ");
        // THE DESK SAYS WHY, as it does for the managed door (the old door relays; the desk was asked for a trial, which moves
        // nothing, and the judge's refusal reached it through the manager's settlement).
        CHECK(e.r.session().notice == said.refusal);
        CHECK(e.r.session().notice != notice);
        CHECK(e.r.session().panels.keyboard == e.kind);
    }

    SUBCASE("a keystroke queued behind a MANAGED request lands in the current document, and the open is refused for it") {
        // THE MANAGED DOOR HOLDS NOTHING EITHER (WL-OPEN-03): input applies
        // to the current document at once, and what it changed is what the open is judged
        // against. The Z is admitted A work, so A is dirty when the Editor is asked to
        // prepare B; the open is refused in the floor's own words, and the desk did not move.
        EditorRig e("edit-open-race-managed");
        e.open();
        const std::string a_path = e.open_file("a.cpp", "one\n");
        put_bytes(e.root / "b.cpp", "two\n");
        e.press_doc(0, 3);
        const std::size_t before = e.asker->opens.size();
        e.enqueue_open(spelled(e.root / "b.cpp"));
        e.enqueue_text("Z");
        e.settle();
        REQUIRE(e.asker->opens.size() == before + 1);
        const SourceOpened said = e.asker->opens.back();
        CHECK_FALSE(said.accepted);
        CHECK(said.refusal.find("unsaved changes") != std::string::npos);
        CHECK(e.read("path") == a_path);
        CHECK(e.read("text") == "oneZ\n");
        CHECK(e.dirty());
        CHECK(e.r.session().panels.keyboard == e.kind);
        CHECK(bytes_of(e.root / "a.cpp") == "one\n");
        CHECK(e.opening().last_outcome == "refused");
        CHECK(e.opening().stage == "idle");
    }

    SUBCASE("a second request while one is in flight supersedes it, and both requesters are told") {
        // RETARGETED: the manager owns supersession. The newer intent ends the
        // older one -- the bus releases its offers, both owners hear it ended -- the first
        // requester is told so in words naming the newer request, and the newer one takes.
        // There is no queue of intents and no retry.
        EditorRig e("edit-open-twice");
        e.open();
        e.open_file("a.cpp", "one\n");
        put_bytes(e.root / "b.cpp", "two\n");
        put_bytes(e.root / "c.cpp", "three\n");
        e.asker->next = [&e](DoorAsker& a, loom::Mail& mail) {
            a.ask(mail, kOpeningRole, OpenSourceRequested{spelled(e.root / "b.cpp")});
            a.ask(mail, kOpeningRole, OpenSourceRequested{spelled(e.root / "c.cpp")});
        };
        const std::size_t before = e.asker->opens.size();
        const std::int64_t committed = e.opening().committed;
        (void)e.r.bus.send(e.asker->id, loom::Message(loom::to_value(SeatDo{}), loom::WeaveId{},
                                                      loom::WeaveId{}, 0));
        e.settle();
        // TWO ANSWERS, TOLD APART BY WHAT THEY SAY: the superseded one names its successor.
        REQUIRE(e.asker->opens.size() == before + 2);
        int accepted = 0;
        int superseded = 0;
        for (std::size_t i = before; i < e.asker->opens.size(); ++i) {
            accepted += e.asker->opens[i].accepted ? 1 : 0;
            superseded += (e.asker->opens[i].refusal.find("superseded") != std::string::npos &&
                           e.asker->opens[i].refusal.find("c.cpp") != std::string::npos)
                              ? 1
                              : 0;
        }
        CHECK(accepted == 1);
        CHECK(superseded == 1);
        CHECK(e.doc_row(0) == "three");
        CHECK(e.opening().committed == committed + 1);
        CHECK(e.opening().stage == "idle");
        CHECK(e.r.bus.joint_pending() == 0); // the superseded operation's offers are released
        // ...AND THE PANE IS NOT WEDGED: the next request takes.
        const SourceOpened again = e.ask_open(spelled(e.root / "b.cpp"));
        CHECK_MESSAGE(again.accepted, again.refusal);
        CHECK(e.doc_row(0) == "two");
    }

    SUBCASE("a settlement forged by a stranger, or one for a flight that already ended, decides nothing") {
        // RETARGETED: the reveal answer is gone; what a stranger might forge
        // now is the manager's `ManagedOpenSettled` -- which both owners take only from the
        // manager's office -- or an answer to the manager, which Loom's provenance says
        // answers nothing it asked.
        EditorRig e("edit-open-stale");
        e.open();
        const std::string a_path = e.open_file("a.cpp", "one\n");
        const std::int64_t op = std::stoll(e.read("opened_by")); // the operation that installed A
        REQUIRE(op != 0);
        const std::string notice = e.r.session().notice;
        put_bytes(e.root / "b.cpp", "two\n");
        const std::string b_path = spelled(e.root / "b.cpp");
        e.asker_says([op, b_path](DoorAsker&, loom::Mail& mail) {
            (void)mail.as_role(kDoorAskerOffice)
                .send_to_role(pane::kEditorPaneRole,
                              ManagedOpenSettled{op + 1, true, true, std::string(), b_path});
            (void)mail.as_role(kDoorAskerOffice)
                .send_to_role(kWorkshopProvider,
                              ManagedOpenSettled{op + 1, false, false, "forged refusal", b_path});
            SourcePrepared forged;
            forged.op = op + 1;
            forged.ok = true;
            (void)mail.as_role(kDoorAskerOffice).send_to_role(kOpeningRole, forged);
        });
        CHECK(e.doc_row(0) == "one");
        CHECK(e.read("path") == a_path);
        CHECK(e.r.session().notice == notice);
        CHECK(e.opening().stage == "idle");
        CHECK(e.opening().refused == 0);
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

TEST_CASE("EDIT-W58: a clipboard answer refuses the open wherever it lands, and A keeps its paste") {
    // ⚔ THE DEFECT PART TWO REPRODUCED WITH REAL MESSAGES: Workshop authored the pane, selected
    // it and took the keyboard when it ANSWERED the reveal, before the Editor had re-judged --
    // so an acquisition that then refused left the desk holding a presentation change for an
    // operation that never happened.
    //
    // RESTAGED for the managed opening: the commitment is the bus's joint
    // publication, and nothing is held. A clipboard answer for A lands in A the moment it is
    // delivered, wherever that falls in B's arrangement; what changes with WHEN is only which
    // party refuses -- the Editor's judge (A is dirty, or its paste is still arriving) or the
    // bus (A's claim moved after the operation bound it). In every interleaving the paste is
    // in A, B is not opened, and the desk did not move for the operation.
    SUBCASE("before the request: the document is dirty at the judge, the open is refused, the desk never moves") {
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
        // ONE POLL, TWO STATEMENTS, THE ANSWER FIRST: the clipboard answer lands on the open
        // document and dirties it, THEN the request for b.cpp reaches the manager.
        put_bytes(e.root / "b.cpp", "two\n");
        const std::string b_path = spelled(e.root / "b.cpp");
        const std::size_t before = e.asker->opens.size();
        (void)e.r.bus.send(e.slow_id, loom::Message(loom::to_value(AnswerNow{}), loom::WeaveId{},
                                                    loom::WeaveId{}, 0));
        e.enqueue_open(b_path);
        e.settle();
        REQUIRE(e.asker->opens.size() == before + 1);
        const SourceOpened said = e.asker->opens.back();
        CHECK_FALSE(said.accepted);
        CHECK(said.refusal.find("unsaved changes") != std::string::npos);
        // THE DOCUMENT THAT WAS THERE IS STILL THERE, with the pasted bytes in it.
        CHECK(e.read("path") == a_path);
        CHECK(e.read("text") == "onePASTED\n");
        // ...AND THE DESK DID NOT MOVE: no authored row, no seat, no selection, no keyboard --
        // the refusal is said, and that is all that is said.
        CHECK_FALSE(has_pane(e.r.session().setup.active, editor_ref()));
        CHECK_FALSE(e.r.session().panels.has(e.kind));
        CHECK(e.r.session().panels.selected == selected_before);
        CHECK(e.r.session().panels.keyboard != e.kind);
        CHECK(e.r.session().notice == said.refusal);
    }
    SUBCASE("behind the request: the answer lands in A while B is being arranged, and B is refused for it") {
        // THE ORDER PART TWO STAGED: the request first, the clipboard answer behind it. The
        // answer is delivered while the desk is answering the trial -- into A, at once, as
        // any input is -- so the Editor, asked to prepare B one turn later, finds A dirty and
        // refuses. The paste is the maker's and it is where the maker asked for it.
        EditorRig e("edit-race-held");
        e.open(160, 48, /*pick_it=*/true, /*slow_skin=*/true);
        const std::string a_path = e.open_file("a.cpp", "one\n");
        e.slow->text = "PASTED";
        e.press_doc(0, 3);
        e.key(input::scan::kV, input::mod::kCtrl);
        REQUIRE(e.slow->held);
        e.unfocus();
        e.r.pick(editor_ref());
        REQUIRE_FALSE(e.r.session().panels.has(e.kind));
        const std::int64_t selected_before = e.r.session().panels.selected;
        put_bytes(e.root / "b.cpp", "two\n");
        const std::string b_path = spelled(e.root / "b.cpp");
        const std::size_t before = e.asker->opens.size();
        e.enqueue_open(b_path);
        (void)e.r.bus.send(e.slow_id, loom::Message(loom::to_value(AnswerNow{}), loom::WeaveId{},
                                                    loom::WeaveId{}, 0));
        e.settle();
        REQUIRE(e.asker->opens.size() == before + 1);
        const SourceOpened said = e.asker->opens.back();
        CHECK_FALSE(said.accepted);
        CHECK(said.refusal.find("unsaved changes") != std::string::npos);
        CHECK(e.read("path") == a_path);
        CHECK(e.read("text") == "onePASTED\n");      // the paste landed where it was asked
        CHECK(bytes_of(e.root / "a.cpp") == "one\n"); // ...and A's bytes were never touched
        CHECK(e.read("text") != e.read("saved_text")); // dirty, read off the pane it is not showing on
        CHECK_FALSE(has_pane(e.r.session().setup.active, editor_ref()));
        CHECK_FALSE(e.r.session().panels.has(e.kind));
        CHECK(e.r.session().panels.selected == selected_before);
        CHECK(e.r.session().panels.keyboard != e.kind);
        CHECK(e.opening().last_outcome == "refused");
    }
    SUBCASE("still arriving when the Editor is asked: refused in words, and eligible once it lands") {
        // THE THIRD PLACE THE ANSWER CAN BE: not yet delivered when the Editor is asked to
        // prepare. The judge refuses for the paste itself (WL-EDIT-05), the answer then lands
        // in A, and the same request is eligible again -- refused now for the unsaved paste,
        // which is the floor.
        EditorRig e("edit-race-arriving");
        e.open(160, 48, /*pick_it=*/true, /*slow_skin=*/true);
        const std::string a_path = e.open_file("a.cpp", "one\n");
        e.slow->text = "PASTED";
        e.press_doc(0, 3);
        e.key(input::scan::kV, input::mod::kCtrl);
        REQUIRE(e.slow->held);
        put_bytes(e.root / "b.cpp", "two\n");
        const SourceOpened waited = e.ask_open(spelled(e.root / "b.cpp"));
        CHECK_FALSE(waited.accepted);
        CHECK(waited.refusal.find("clipboard answer") != std::string::npos);
        CHECK(e.read("text") == "one\n");
        e.answer_now();
        CHECK(e.read("text") == "onePASTED\n");
        CHECK(e.read("path") == a_path);
        const SourceOpened dirty = e.ask_open(spelled(e.root / "b.cpp"));
        CHECK_FALSE(dirty.accepted);
        CHECK(dirty.refusal.find("unsaved changes") != std::string::npos);
    }
}

// ============================================================================
// PART THREE'S CORRECTIONS: the commitment point is the desk's delivery of the ask
// ============================================================================
//
// EDIT-W59 stood here. It pinned the choice the founder rejected -- a pane authored and
// waiting for room when the screen shrank between the capacity answer and the settle -- and
// was retired rather than retargeted. EDIT-W67 and EDIT-W68 replace it at the corrected
// commitment point: room lost BEFORE the desk's delivery of the ask refuses the open with
// nothing moved; a resize AFTER it is an ordinary presentation change to an open that stands.

TEST_CASE("EDIT-W67: room lost before the commitment refuses the open, and nothing is authored or moved") {
    // ⚔ THE FINDING, RE-ESTABLISHED AT THE NEW CONVERSATION'S MILESTONES. A real
    // `SurfaceExtent` delivered to the desk BEFORE the pane's ask leaves the Editor waiting for
    // room, so the trial seat has nothing to commit to and refuses. The document that was open
    // stands, the requester is told the picker's words, and the desk did not move for this
    // operation -- the seat the Editor lost, it lost to the maker's own shrink.
    //
    // RESTAGED for the managed opening, at the managed door: the shrink lands
    // between the manager's binding of the desk and the desk's trial, so the trial finds no
    // seat and refuses with the picker's words; nothing was offered and nothing published.
    EditorRig e("edit-shrink-before");
    e.open(160, 48, /*pick_it=*/false);
    e.r.pick(ref_of(panel::kPaneEditor)); // the Pane Manager takes the stack ahead of it
    REQUIRE(e.r.session().panels.has(panel::kPaneEditor));
    const std::string a_path = e.open_file("a.cpp", "one\n"); // seated behind the Manager
    REQUIRE(e.r.session().panels.has(e.kind));
    put_bytes(e.root / "b.cpp", "two\n");
    const std::string b_path = spelled(e.root / "b.cpp");
    const std::size_t before = e.asker->opens.size();
    e.enqueue_open(b_path);
    // MILESTONE 1: the asker spoke; the request is queued and nothing has been judged.
    REQUIRE(e.r.bus.pump_pending() == 1);
    // THE SHRINK, queued behind the request -- and therefore AHEAD of the ask the pane will
    // send when it handles that request.
    (void)e.r.bus.publish(loom::Message(
        loom::to_value(surface::SurfaceExtent{160, kMinScreen.h, 0, 0}), loom::WeaveId{},
        loom::WeaveId{}, 0));
    // MILESTONE 2: one turn delivers the request (the manager binds the Editor and the desk as
    // they are, and asks the desk for a trial) and then the shrink (the Editor, authored
    // behind the Manager, loses its seat). The trial is queued behind both, so this is the
    // desk it will be judged on.
    (void)e.r.bus.pump_pending();
    REQUIRE(e.r.session().panels.has(panel::kPaneEditor));
    REQUIRE_FALSE(e.r.session().panels.has(e.kind));           // room lost...
    CHECK(has_pane(e.r.session().setup.active, editor_ref())); // ...by the shrink; the row stands
    CHECK(e.asker->opens.size() == before);                    // ...with the open still in flight
    CHECK(e.opening().stage == "trial");
    // MILESTONE 3: the trial finds no seat; the operation ends with nothing published.
    e.settle();
    CHECK(e.opening().stage == "idle");
    CHECK(e.opening().last_outcome == "refused");
    REQUIRE(e.asker->opens.size() == before + 1);
    const SourceOpened said = e.asker->opens.back();
    CHECK_FALSE(said.accepted);
    CHECK(said.refusal == "no room for Editor on this screen -- make the window taller, then p "
                          "again");
    CHECK(e.r.session().notice == said.refusal);
    CHECK(e.read("path") == a_path);
    CHECK(e.read("text") == "one\n");
    CHECK_FALSE(e.r.session().panels.has(e.kind));
    CHECK(keyboard_pane(e.r.session().panels) != e.kind); // no keys resolve to a hidden pane
    // ...AND A WINDOW BIG ENOUGH SHOWS THE DOCUMENT THAT WAS THERE, not the one refused.
    e.r.extent(160, 48);
    REQUIRE(e.r.session().panels.has(e.kind));
    CHECK(e.doc_row(0) == "one");
}

TEST_CASE("EDIT-W68: a resize after the commitment is an ordinary presentation change") {
    // THE CONTROL THE FOUNDER'S GUARANTEE NAMES: legitimate maker actions after a successful
    // commitment may change presentation. RESTAGED for the managed opening:
    // the commitment is the bus's joint publication, made inside the manager's delivery of the
    // desk's admission; each owner is shown its published claim before it runs again, and the
    // desk applies the presentation whole in that showing. A real `SurfaceExtent` delivered
    // after the commitment unseats the Editor the way a shrink unseats any pane, and the open
    // still completes with B in a pane that is on the desk and waiting for room. Two
    // interleavings, because the reviewer's ran the shrink between the two owners' showings.
    EditorRig e("edit-shrink-after");
    e.open(160, 48, /*pick_it=*/false);
    e.r.pick(ref_of(panel::kPaneEditor));
    REQUIRE(e.r.session().panels.has(panel::kPaneEditor));
    const std::string a_path = e.open_file("a.cpp", "one\n");
    REQUIRE(e.r.session().panels.has(e.kind));
    put_bytes(e.root / "b.cpp", "two\n");
    const std::string b_path = spelled(e.root / "b.cpp");
    const std::size_t before = e.asker->opens.size();
    const std::int64_t committed = e.opening().committed;
    const auto shrink = [&e] {
        (void)e.r.bus.publish(loom::Message(
            loom::to_value(surface::SurfaceExtent{160, kMinScreen.h, 0, 0}), loom::WeaveId{},
            loom::WeaveId{}, 0));
    };
    const auto after = [&e, &before, &b_path, &a_path, &committed] {
        REQUIRE(e.asker->opens.size() == before + 1);
        const SourceOpened said = e.asker->opens.back();
        CHECK_MESSAGE(said.accepted, said.refusal);
        CHECK(e.opening().committed == committed + 1); // established once both owners applied
        CHECK(e.opening().stage == "idle");
        CHECK(e.read("path") == b_path);
        CHECK(e.read("text") == "two\n");
        CHECK(has_pane(e.r.session().setup.active, editor_ref()));
        CHECK_FALSE(e.r.session().panels.has(e.kind));      // the shrink's doing, after the commitment
        CHECK(keyboard_pane(e.r.session().panels) != e.kind); // no keys resolve to a hidden pane
        // ...AND A WINDOW BIG ENOUGH SHOWS THE OPENED DOCUMENT, WITH NO SECOND REQUEST.
        e.r.extent(160, 48);
        REQUIRE(e.r.session().panels.has(e.kind));
        CHECK(e.doc_row(0) == "two");
        CHECK(e.doc_row(0) != a_path);
    };
    e.enqueue_open(b_path);
    e.pump_until_stage("admit");          // the Editor holds B as a candidate; the desk is asked
    REQUIRE(e.r.bus.pump_pending() >= 1); // ...and admits: its offer stands, its answer is queued
    const std::int64_t op = e.opening().op;
    REQUIRE(op != 0);

    SUBCASE("the shrink lands after the commitment and before either owner is shown it") {
        shrink(); // queued behind the desk's admission answer
        e.r.session().notice.clear();
        // ONE TURN: the manager commits (THE COMMITMENT), then the shrink reaches the desk --
        // which is shown its published presentation first (seat, selection, keys, rows) and
        // then loses the seat to the maker's shrink, as any pane would.
        (void)e.r.bus.pump_pending();
        CHECK(e.opening().stage == "apply"); // published; the owners' applications are owed
        CHECK(e.r.bus.joint_status(static_cast<std::uint64_t>(op)).state ==
              loom::JointState::Committed);
        CHECK(e.r.session().notice.find("showing Editor") != std::string::npos);
        CHECK(has_pane(e.r.session().setup.active, editor_ref()));
        CHECK_FALSE(e.r.session().panels.has(e.kind));
        CHECK(e.asker->opens.size() == before); // the terminal answer follows the application
        e.settle();
        after();
    }
    SUBCASE("the commitment is the milestone, observed on its own, and the shrink lands after the owners heard") {
        e.r.session().notice.clear(); // the host's own record of what it last said, blanked
        REQUIRE(e.r.bus.pump_pending() >= 1); // the manager's delivery alone: THE COMMITMENT
        CHECK(e.opening().committed == committed); // published, not yet applied: nothing established
        CHECK(e.opening().stage == "apply");
        CHECK(e.r.bus.joint_status(static_cast<std::uint64_t>(op)).state ==
              loom::JointState::Committed);
        CHECK(e.r.bus.joint_status(static_cast<std::uint64_t>(op)).application ==
              loom::JointApplication::Pending);
        // AT THE COMMITMENT, BEFORE EITHER OWNER RAN AGAIN, BOTH PUBLISHED CLAIMS AGREE: the
        // Editor's says B, opened by this operation; the desk's says seated, selected, keyed,
        // shown by the same operation, with B's rows admitted.
        const std::optional<EditorDocument> doc = e.document_claim();
        REQUIRE(doc.has_value());
        CHECK(doc->path == b_path);
        CHECK(doc->opened_by == op);
        const std::optional<PanePresentation> desk = e.presentation_claim();
        REQUIRE(desk.has_value());
        CHECK(desk->seated);
        CHECK(desk->selected);
        CHECK(desk->keyboard);
        CHECK(desk->shown_by == op);
        CHECK(desk->content_generation == doc->doc_epoch);
        CHECK(e.asker->opens.size() == before); // said and written before anyone is told
        // ...AND EACH OWNER'S OWN PICTURE IS THE PUBLISHED ONE AT ITS NEXT OBSERVATION.
        (void)e.r.bus.pump_pending();
        CHECK(e.r.session().notice == "showing Editor -- it opened b.cpp, and it has the keys");
        CHECK(e.r.session().panels.has(e.kind));
        CHECK(e.r.session().panels.selected == e.kind);
        CHECK(keyboard_pane(e.r.session().panels) == e.kind);
        shrink();
        e.settle();
        after();
    }
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

TEST_CASE("EDIT-W69: a quit asked while an open is being seated is refused in words, and the open then takes") {
    // A PERMISSION GIVEN WHILE AN OPEN IS BETWEEN ITS PREPARATION AND ITS COMMITMENT is one
    // the commitment could falsify -- the paste's rule, one operation over. RESTAGED
    // for the managed opening: the quit ask lands after the Editor has
    // prepared B and before the manager commits; the Editor refuses it naming the source it
    // is still opening, the host stays, and the open completes. (Before the preparation the
    // Editor knows nothing of the flight and answers about its document alone; after the
    // commitment, about B.)
    EditorRig e("edit-quit-while-opening");
    e.open();
    e.open_file("a.cpp", "one\n");
    put_bytes(e.root / "b.cpp", "two\n");
    const std::string b_path = spelled(e.root / "b.cpp");
    e.unfocus(); // `q` is command mode's
    const std::size_t before = e.asker->opens.size();
    e.enqueue_open(b_path);
    e.pump_until_stage("prepare"); // the Editor is about to be asked to prepare B
    e.enqueue_key(input::scan::kQ); // ...and the quit is queued behind that ask
    // ONE TURN: the Editor prepares B (a candidate, offered); the host asks the room whether
    // it may end. THE NEXT: the Editor, holding a candidate, refuses in words. THE NEXT: the
    // host hears the refusal, says it, and stays -- three turns, before the commitment.
    int turns = 0;
    while (e.r.session().notice.find("still opening") == std::string::npos) {
        REQUIRE(++turns <= 3);
        REQUIRE(e.r.bus.pump_pending() > 0);
    }
    CHECK_FALSE(e.r.host.quit);
    CHECK(e.r.session().notice.find("still opening") != std::string::npos);
    CHECK(e.r.session().notice.find("b.cpp") != std::string::npos);
    CHECK(e.asker->opens.size() == before);
    e.settle();
    CHECK_FALSE(e.r.host.quit);
    REQUIRE(e.asker->opens.size() == before + 1);
    CHECK_MESSAGE(e.asker->opens.back().accepted, e.asker->opens.back().refusal);
    CHECK(e.read("path") == b_path);
    CHECK(e.doc_row(0) == "two");
    // ...AND WITH THE OPEN DONE, THE SAME QUIT TAKES: a clean document permits.
    e.unfocus();
    e.r.key(input::scan::kQ);
    CHECK(e.r.host.quit);
}

TEST_CASE("EDIT-W61: an acquisition outstanding across the pane's removal still settles, and a forged answer decides nothing") {
    // THE PANE LEAVES THE DESK WHILE ITS OPEN IS IN FLIGHT. RESTAGED for the managed
    // opening: the picker's removal is queued behind the request, so one
    // turn has the manager bind the desk as it is and then the picker take the Editor off
    // it. The desk the operation bound is not the desk any more: its offer is refused by the
    // bus, the operation ends with nothing published, the requester is told, and the maker's
    // removal stands -- a stale preparation cannot undo newer intent. Asked again, the same
    // request seats the pane with B.
    EditorRig e("edit-flight-removal");
    e.open();
    const std::string a_path = e.open_file("a.cpp", "one\n");
    put_bytes(e.root / "b.cpp", "two\n");
    const std::string b_path = spelled(e.root / "b.cpp");
    e.unfocus(); // the picker is command mode's row
    const std::vector<CatalogRow> rows = combined_catalog(e.r.session().panels);
    std::size_t want = 0;
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (rows[i].ref == editor_ref()) {
            want = i;
        }
    }
    const std::size_t before = e.asker->opens.size();
    e.enqueue_open(b_path);
    REQUIRE(e.r.bus.pump_pending() == 1); // the request is queued
    // THE REMOVAL, QUEUED BEHIND THE REQUEST: the picker opens, walks to the Editor's row, and
    // Return removes it. None of it drains; it is one poll's burst.
    e.enqueue_key(input::scan::kP);
    for (std::size_t i = 0; i < want; ++i) {
        e.enqueue_key(input::scan::kDown);
    }
    e.enqueue_key(input::scan::kReturn);
    // ONE TURN: the manager binds the Editor and the desk and asks for a trial; then the
    // picker removes the Editor. The flight is outstanding across the removal.
    (void)e.r.bus.pump_pending();
    REQUIRE_FALSE(has_pane(e.r.session().setup.active, editor_ref()));
    REQUIRE_FALSE(e.r.session().panels.has(e.kind));
    CHECK(e.asker->opens.size() == before);
    CHECK(e.opening().op != 0);
    // THE END OF THE FLIGHT: refused, in words, with A standing and the removal standing.
    e.settle();
    REQUIRE(e.asker->opens.size() == before + 1);
    const SourceOpened said = e.asker->opens.back();
    CHECK_FALSE(said.accepted);
    CHECK(said.refusal.find("changed while opening") != std::string::npos);
    CHECK(e.read("path") == a_path);
    CHECK(e.read("text") == "one\n");
    CHECK_FALSE(has_pane(e.r.session().setup.active, editor_ref()));
    CHECK_FALSE(e.r.session().panels.has(e.kind));
    CHECK(e.r.bus.joint_pending() == 0);
    // ...AND THE SAME REQUEST, MADE AGAIN, SEATS THE PANE WITH B.
    const SourceOpened again = e.ask_open(b_path);
    CHECK_MESSAGE(again.accepted, again.refusal);
    CHECK(has_pane(e.r.session().setup.active, editor_ref()));
    REQUIRE(e.r.session().panels.has(e.kind));
    CHECK(e.r.session().panels.keyboard == e.kind);
    CHECK(e.doc_row(0) == "two");

    // ...AND A SETTLEMENT FOR A FLIGHT THAT ALREADY ENDED MOVES NOTHING. The office may forge
    // one; both owners take it from the manager's office alone.
    const std::string text_before = e.read("text");
    const std::int64_t op = std::stoll(e.read("opened_by"));
    e.asker_says([op, a_path](DoorAsker&, loom::Mail& mail) {
        (void)mail.as_role(kDoorAskerOffice)
            .send_to_role(pane::kEditorPaneRole, ManagedOpenSettled{op, false, false, "forged", a_path});
        (void)mail.as_role(kDoorAskerOffice)
            .send_to_role(kWorkshopProvider, ManagedOpenSettled{op, false, false, "forged", a_path});
    });
    CHECK(e.read("text") == text_before);
    CHECK(e.read("path") == b_path);
    CHECK(e.r.session().notice.find("forged") == std::string::npos);
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
    // LINE 1 IS LONGER THAN ANY ROOM THIS RIG GRANTS, so a caret at its end makes the window
    // slide horizontally -- the precondition the third subcase needs and asserts.
    std::string many = "line 1" + std::string(200, 'x') + "\n";
    for (int i = 2; i <= 40; ++i) {
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
        // THE WINDOW HAS SLID: the caret at the end of line 1 is past the room's right edge,
        // so `first_col` is not zero -- REQUIRED, because a case that cannot establish its
        // precondition must fail visibly rather than assert that a zero equals a zero.
        e.key(input::scan::kHome, input::mod::kCtrl);
        e.key(input::scan::kEnd);
        const std::string col = e.read("first_col");
        REQUIRE(std::stoll(col) > 0);
        REQUIRE(e.read("first_row") == "0");
        const std::string slice = e.doc_row(0); // line 1 as the maker sees it: its tail...
        REQUIRE(slice.rfind("line 1", 0) != 0); // ...which is not its head
        REQUIRE(e.ask_open(path).accepted);
        CHECK(e.read("first_col") == col);
        CHECK(e.read("first_row") == "0");
        CHECK(e.doc_row(0) == slice);
        // A REAL RESIZE STILL PULLS THE CARET'S LINE INTO VIEW -- and this rig's Editor is
        // already six rows tall, so the room has to actually change to be a resize.
        REQUIRE(e.read("last_rows") == "6");
        e.give_rows(12);
        CHECK(e.read("last_rows") == "12");
        CHECK(std::stoll(e.read("first_row")) <= std::stoll(e.read("caret_row")));
    }
}

// ============================================================================
// THE MANAGED OPENING'S OWN WITNESSES (WL-OPEN, agents/workshop/opening.md)
// ============================================================================
//
// Seven observations the founder's guarantees name, each staged with real messages at exact
// intervals of the nine-delivery conversation (see `EditorRig::pump_until_stage`), over the
// real loaded Editor image, the real Workshop and the real bus. Test code here controls
// scheduling and observes; it performs no party's publication, routing or lifecycle duty.

TEST_CASE("EDIT-W70: a managed open has one commitment -- the published claims, the pane's reads, its snapshot and the desk agree at it, and A is current before it") {
    EditorRig e("edit-commitment");
    e.open();
    const std::string a_path = e.open_file("a.cpp", "one\n");
    put_bytes(e.root / "b.cpp", "two\nthree\n");
    const std::string b_path = spelled(e.root / "b.cpp");
    const std::size_t before = e.asker->opens.size();
    const std::int64_t committed = e.opening().committed;
    const std::int64_t a_epoch = std::stoll(e.read("doc_epoch"));
    e.enqueue_open(b_path);
    e.pump_until_stage("admit");
    const std::int64_t op = e.opening().op;
    REQUIRE(op != 0);
    // BEFORE THE COMMITMENT: A is the document every reader is answered with, and B is a
    // candidate nobody can read -- the Editor's claim, its snapshot, a poke, the desk's rows.
    {
        const std::optional<EditorDocument> doc = e.document_claim();
        REQUIRE(doc.has_value());
        CHECK(doc->path == a_path);
        CHECK(doc->opened_by != op);
        const loom::Value snap = e.snapshot();
        CHECK(snap.get("path")->as_text() == a_path);
        CHECK(snap.get("text")->as_text() == "one\n");
        const std::vector<std::string> rows = e.shown();
        CHECK(std::find(rows.begin(), rows.end(), "one") != rows.end());
        CHECK(std::find(rows.begin(), rows.end(), "two") == rows.end());
        CHECK(e.r.bus.joint_status(static_cast<std::uint64_t>(op)).state ==
              loom::JointState::Preparing);
        CHECK(e.r.bus.joint_pending() == 1);
        CHECK(e.opening().stage == "admit");
        CHECK(e.opening().awaiting == kWorkshopProvider);
        // ...AND A MAKER CAN READ THAT ON THE DESK: the standing condition names the wait
        // (the desk's picture is the last progress it was told -- one delivery behind the
        // manager's own record, which is what "afterwards" means).
        const Condition* pending =
            e.r.session().conditions.find("opening:" + std::to_string(op));
        REQUIRE(pending != nullptr);
        CHECK(pending->compact.find("opening b.cpp") != std::string::npos);
        CHECK(pending->detail.find("waiting for zengine.") != std::string::npos);
    }
    // A POKE QUEUED NOW IS ANSWERED BEHIND THE DESK'S ADMISSION, before the commitment.
    const std::uint64_t poked_before = e.enqueue_read("path", e.image);
    REQUIRE(e.r.bus.pump_pending() >= 1); // the desk admits B's rows and offers
    CHECK(e.r.bus.joint_retained_bytes() > 0); // two offers stand, bounded
    // THE COMMITMENT: one delivery of the manager's, and nothing else runs in it.
    e.r.session().notice.clear();
    REQUIRE(e.r.bus.pump_pending() >= 1);
    CHECK(e.answered(poked_before) == a_path); // answered before it: A
    // PUBLISHED, NOT YET APPLIED: the manager holds
    // the flight at `apply` until the bus says what the owners' showings came to, and has
    // established nothing yet -- the claims say B, the application is owed.
    CHECK(e.opening().committed == committed);
    CHECK(e.opening().stage == "apply");
    CHECK(e.r.bus.joint_status(static_cast<std::uint64_t>(op)).state ==
          loom::JointState::Committed);
    CHECK(e.r.bus.joint_status(static_cast<std::uint64_t>(op)).application ==
          loom::JointApplication::Pending);
    CHECK(e.r.bus.joint_pending() == 0);
    CHECK(e.r.bus.joint_retained_bytes() == 0); // the offers were released at the commitment
    // BOTH PUBLISHED CLAIMS SAY B, TOGETHER, AND NEITHER OWNER HAS RUN SINCE.
    const std::optional<EditorDocument> doc = e.document_claim();
    REQUIRE(doc.has_value());
    CHECK(doc->path == b_path);
    CHECK(doc->opened_by == op);
    CHECK(doc->doc_epoch == a_epoch + 1);
    CHECK_FALSE(doc->dirty);
    const std::optional<PanePresentation> desk = e.presentation_claim();
    REQUIRE(desk.has_value());
    CHECK(desk->seated);
    CHECK(desk->selected);
    CHECK(desk->keyboard);
    CHECK(desk->shown_by == op);
    CHECK(desk->content_generation == doc->doc_epoch);
    CHECK(e.r.bus.has_unobserved_publication(e.image));
    CHECK(e.r.bus.has_unobserved_publication(e.r.workshop_id));
    CHECK(e.asker->opens.size() == before); // the terminal answer FOLLOWS the commitment
    CHECK(e.r.session().notice.empty());    // ...and so does every sentence about it
    // THE SNAPSHOT, TAKEN NOW, IS B: the bytes a reload would carry agree with what the bus
    // published, because the showing happens before the snapshot.
    const loom::Value snap = e.snapshot();
    CHECK(snap.get("path")->as_text() == b_path);
    CHECK(snap.get("text")->as_text() == "two\nthree\n");
    CHECK(snap.get("opened_by")->as_int() == op);
    CHECK_FALSE(e.r.bus.has_unobserved_publication(e.image)); // shown, once
    // THE NEXT TURN: each owner's own picture is the published one -- the manager's `apply`
    // word is the delivery that shows each its claim before it runs -- and the bus records
    // both applications. The requester is answered from that record, afterwards.
    (void)e.r.bus.pump_pending();
    CHECK(e.r.session().notice == "showing Editor -- it opened b.cpp, and it has the keys");
    REQUIRE(e.r.session().panels.has(e.kind));
    CHECK(e.r.session().panels.selected == e.kind);
    CHECK(keyboard_pane(e.r.session().panels) == e.kind);
    REQUIRE(e.seat() != nullptr);
    CHECK(e.seat()->content_generation == doc->doc_epoch);
    CHECK(e.seat()->rows == desk->rows);
    {
        // THE ADMITTED ROWS ARE B'S NOW; the painted canvas follows on the next turn, which
        // is physical display timing and not admitted presentation state.
        std::vector<std::string> admitted;
        for (const surface::SurfaceTextRow& row : e.seat()->shown) {
            admitted.push_back(row.text);
        }
        CHECK(std::find(admitted.begin(), admitted.end(), "two") != admitted.end());
        CHECK(std::find(admitted.begin(), admitted.end(), "one") == admitted.end());
    }
    CHECK(e.r.bus.joint_status(static_cast<std::uint64_t>(op)).application ==
          loom::JointApplication::Applied);
    CHECK_FALSE(e.r.bus.has_unobserved_publication(e.r.workshop_id));
    CHECK(e.asker->opens.size() == before); // not yet: the answer follows the application
    // THE TURN AFTER: the bus's word (`zen.JointApplied`) reaches the manager, which settles
    // from its own re-read of the record; the turn after that, the requester hears.
    (void)e.r.bus.pump_pending();
    CHECK(e.opening().committed == committed + 1);
    CHECK(e.opening().stage == "idle");
    CHECK(e.opening().last_outcome == "committed");
    (void)e.r.bus.pump_pending();
    REQUIRE(e.asker->opens.size() == before + 1);
    CHECK_MESSAGE(e.asker->opens.back().accepted, e.asker->opens.back().refusal);
    CHECK(e.r.session().conditions.find("opening:" + std::to_string(op)) == nullptr);
    // ...AND A POKE AGREES WITH ALL OF IT.
    CHECK(e.read("path") == b_path);
    CHECK(e.read("opened_by") == std::to_string(op));
    CHECK(e.read("text") == "two\nthree\n");
    CHECK(e.doc_row(0) == "two");
    CHECK(e.status().rfind("saved L1:C1/3", 0) == 0);
}

TEST_CASE("EDIT-W71: legitimate A input while B is being arranged is admitted to A, and B is refused without moving the desk") {
    SUBCASE("a caret key routed while the desk is admitting B aborts the open at the desk's offer") {
        // THE ADMISSION GUARD: an input the desk routed to the Editor is admitted A work
        // whether or not it has been delivered yet, so the desk's claim moves for it and its
        // offer for the operation is refused -- conservatively, for a key that changed no
        // byte. The key itself is applied to A, and the desk does not move.
        EditorRig e("edit-key-while-preparing");
        e.open();
        const std::string a_path = e.open_file("a.cpp", "one\n");
        put_bytes(e.root / "b.cpp", "two\n");
        const std::string b_path = spelled(e.root / "b.cpp");
        e.press_doc(0, 0);
        const std::size_t before = e.asker->opens.size();
        e.enqueue_open(b_path);
        e.pump_until_stage("prepare");
        e.enqueue_key(input::scan::kRight); // routed to the Editor behind its preparation
        e.settle();
        REQUIRE(e.asker->opens.size() == before + 1);
        const SourceOpened said = e.asker->opens.back();
        CHECK_FALSE(said.accepted);
        CHECK(said.refusal.find("the desk changed while opening Editor") != std::string::npos);
        CHECK(e.read("path") == a_path);
        CHECK(e.read("caret_byte") == "1"); // the key was applied to A
        CHECK(e.clean());
        CHECK(has_pane(e.r.session().setup.active, editor_ref()));
        REQUIRE(e.r.session().panels.has(e.kind));
        CHECK(e.r.session().panels.selected == e.kind);
        CHECK(e.r.session().panels.keyboard == e.kind);
        CHECK(e.r.bus.joint_pending() == 0);
        CHECK(e.r.session().notice == said.refusal);
    }
    SUBCASE("a paste asked while the desk is admitting B lands in A afterwards, and B is refused") {
        // THE FOUNDER'S NAMED WITNESS: a legitimate clipboard reply for A while B prepares.
        // The paste is asked by a routed key (the guard above), the platform answers later,
        // the answer lands in A -- never held, never retargeted -- and B was refused.
        EditorRig e("edit-paste-while-preparing");
        e.open(160, 48, /*pick_it=*/true, /*slow_skin=*/true);
        const std::string a_path = e.open_file("a.cpp", "one\n");
        put_bytes(e.root / "b.cpp", "two\n");
        const std::string b_path = spelled(e.root / "b.cpp");
        e.press_doc(0, 3);
        e.slow->text = "PASTED";
        const std::size_t before = e.asker->opens.size();
        e.enqueue_open(b_path);
        e.pump_until_stage("prepare");
        e.enqueue_key(input::scan::kV, input::mod::kCtrl); // the maker's paste, behind the preparation
        int turns = 0;
        while (!e.slow->held) { // turn by turn, until the platform holds the pane's ask
            REQUIRE(e.r.bus.pump_pending() > 0);
            REQUIRE(++turns < 20);
        }
        (void)e.r.bus.send(e.slow_id, loom::Message(loom::to_value(AnswerNow{}), loom::WeaveId{},
                                                    loom::WeaveId{}, 0));
        e.settle();
        REQUIRE(e.asker->opens.size() == before + 1);
        const SourceOpened said = e.asker->opens.back();
        CHECK_FALSE(said.accepted);
        CHECK(said.refusal.find("the desk changed while opening Editor") != std::string::npos);
        CHECK(e.read("path") == a_path);
        CHECK(e.read("text") == "onePASTED\n"); // the legitimate edit remains in A
        CHECK(bytes_of(e.root / "a.cpp") == "one\n");
        CHECK(e.dirty());
        CHECK(has_pane(e.r.session().setup.active, editor_ref()));
        REQUIRE(e.r.session().panels.has(e.kind));
        CHECK(e.r.session().panels.selected == e.kind);
        CHECK(e.r.session().panels.keyboard == e.kind);
        // ...AND ASKED AGAIN, B IS REFUSED FOR THE PASTE, WHICH IS THE FLOOR.
        const SourceOpened again = e.ask_open(b_path);
        CHECK_FALSE(again.accepted);
        CHECK(again.refusal.find("unsaved changes") != std::string::npos);
    }
    SUBCASE("an edit that leaves A clean still moves its claim, and the Editor's own offer is refused for it") {
        // THE EDITOR'S SIDE OF THE SAME LAW: a typed byte and its undo leave A exactly as
        // judged, but the document's revision moved after the operation bound it, so the
        // Editor's offer is refused by the bus and the Editor answers in words.
        EditorRig e("edit-clean-edit-while-preparing");
        e.open();
        const std::string a_path = e.open_file("a.cpp", "one\n");
        put_bytes(e.root / "b.cpp", "two\n");
        const std::string b_path = spelled(e.root / "b.cpp");
        e.press_doc(0, 3);
        const std::size_t before = e.asker->opens.size();
        e.enqueue_open(b_path);
        REQUIRE(e.r.bus.pump_pending() == 1); // the request is queued
        (void)e.r.bus.pump_pending();          // the manager binds A's claim as it is, asks the desk
        e.enqueue_text("Z");
        e.enqueue_key(input::scan::kZ, input::mod::kCtrl);
        e.settle();
        REQUIRE(e.asker->opens.size() == before + 1);
        const SourceOpened said = e.asker->opens.back();
        CHECK_FALSE(said.accepted);
        CHECK(said.refusal.find("the document changed while opening") != std::string::npos);
        CHECK(e.read("path") == a_path);
        CHECK(e.read("text") == "one\n");
        CHECK(e.clean());
        CHECK(e.r.bus.joint_pending() == 0);
    }
}

TEST_CASE("EDIT-W72: more than 256 ordinary events across an opening, from three producers, are admitted in order with nothing held and nothing dropped") {
    // THE DEFECT THE INVESTIGATION FOUND: the built-in held input while an open was in flight,
    // and its hold had a cap (`kMaxHeldInput`, 256 -- the quit's, borrowed), past which the
    // 257th event was dropped. The managed open holds nothing: every event is admitted to A
    // as it arrives, in order, and the open is refused for the work -- no larger cap is a
    // repair, and none is here.
    EditorRig e("edit-many-events");
    e.open(160, 48, /*pick_it=*/true, /*slow_skin=*/true);
    const std::string a_path = e.open_file("a.cpp", "\n");
    put_bytes(e.root / "b.cpp", "two\n");
    const std::string b_path = spelled(e.root / "b.cpp");
    e.slow->text = "|P|";
    const EditorRig::Aim at = e.aim(); // resolved before anything is queued (VD-27)
    const std::size_t before = e.asker->opens.size();
    e.enqueue_open(b_path);
    e.pump_until_stage("prepare");
    // THREE PRODUCERS, ONE BURST, QUEUED BEHIND THE EDITOR'S PREPARATION: the pointer (a press
    // that places the caret), the keyboard (257 typed characters), and the platform (the
    // answer to the paste the last key asks for).
    e.enqueue_press_doc(at, 0, 0);
    std::string typed;
    for (int i = 0; i < 257; ++i) {
        const char c = static_cast<char>('a' + (i % 26));
        typed.push_back(c);
        e.enqueue_text(std::string(1, c));
    }
    e.enqueue_key(input::scan::kV, input::mod::kCtrl);
    // DELAYED DELIVERY: one turn at a time, the answer let go the turn after the platform
    // holds the pane's ask.
    int turns = 0;
    bool answered = false;
    for (;;) {
        const std::size_t delivered = e.r.bus.pump_pending();
        ++turns;
        REQUIRE(turns < 4000);
        if (!answered && e.slow->held) {
            answered = true;
            (void)e.r.bus.send(e.slow_id, loom::Message(loom::to_value(AnswerNow{}),
                                                        loom::WeaveId{}, loom::WeaveId{}, 0));
            continue;
        }
        if (delivered == 0) {
            break;
        }
    }
    CHECK(answered);
    CHECK(turns >= 6);
    // EVERY EVENT LANDED, IN A, IN ORDER: the press placed the caret, the run was typed, the
    // paste followed it. 257 typed bytes, one more than the old hold could carry.
    CHECK(e.read("path") == a_path);
    CHECK(e.read("text") == typed + "|P|\n");
    CHECK(typed.size() == 257);
    // THE OPEN WAS REFUSED, in words, and the desk did not move for it.
    REQUIRE(e.asker->opens.size() == before + 1);
    CHECK_FALSE(e.asker->opens.back().accepted);
    CHECK(e.asker->opens.back().refusal.find("changed while opening") != std::string::npos);
    REQUIRE(e.r.session().panels.has(e.kind));
    CHECK(e.r.session().panels.selected == e.kind);
    CHECK(e.r.session().panels.keyboard == e.kind);
    CHECK(e.r.bus.joint_pending() == 0);
    // ...AND UNRELATED PANE INTERACTION PROGRESSES: the picker seats the Pane Manager.
    e.unfocus();
    e.r.pick(ref_of(panel::kPaneEditor));
    CHECK(e.r.session().panels.has(panel::kPaneEditor));
    CHECK(e.read("text") == typed + "|P|\n"); // ...and A is untouched by it
}

TEST_CASE("EDIT-W73: a competing open through the OLD door while B is being arranged supersedes it, the stale preparation cannot commit, and a later setup change survives") {
    // RETARGETED: the old door relays to the same
    // manager, so a competing request there is a newer intent at the manager -- it supersedes
    // B (the bus releases B's offers, B's requester is told which request did it) and C
    // opens through the one commitment. The Step 1 slice's document-only install, which won
    // by racing the manager, is gone.
    EditorRig e("edit-competing-open");
    e.open();
    const std::string a_path = e.open_file("a.cpp", "one\n");
    put_bytes(e.root / "b.cpp", "two\n");
    put_bytes(e.root / "c.cpp", "three\n");
    const std::string b_path = spelled(e.root / "b.cpp");
    const std::string c_path = spelled(e.root / "c.cpp");
    const std::size_t before = e.asker->opens.size();
    const std::int64_t committed = e.opening().committed;
    e.enqueue_open(b_path);
    e.pump_until_stage("prepare"); // the Editor is about to prepare B
    // THE COMPETING INTENT: the old door, for C, queued behind the preparation.
    e.asker->next = [c_path](DoorAsker& a, loom::Mail& mail) {
        a.ask(mail, kEditorRole, OpenSourceRequested{c_path});
    };
    (void)e.r.bus.send(e.asker->id, loom::Message(loom::to_value(SeatDo{}), loom::WeaveId{},
                                                  loom::WeaveId{}, 0));
    e.settle();
    // TWO ANSWERS: C's open took; B's was superseded by it, naming C.
    REQUIRE(e.asker->opens.size() == before + 2);
    int accepted = 0;
    std::string refusal;
    for (std::size_t i = before; i < e.asker->opens.size(); ++i) {
        if (e.asker->opens[i].accepted) {
            ++accepted;
        } else {
            refusal = e.asker->opens[i].refusal;
        }
    }
    CHECK(accepted == 1);
    CHECK(refusal.find("superseded") != std::string::npos);
    CHECK(refusal.find("c.cpp") != std::string::npos);
    CHECK(e.read("path") == c_path);
    CHECK(e.doc_row(0) == "three");
    CHECK(e.r.bus.joint_pending() == 0);
    CHECK(e.opening().committed == committed + 1);
    CHECK(e.opening().last_outcome == "committed");
    // A LATER, INDEPENDENT SETUP CHANGE SURVIVES: the Pane Manager is seated; B, asked again,
    // opens beside it and undoes nothing.
    e.unfocus();
    e.r.pick(ref_of(panel::kPaneEditor));
    REQUIRE(e.r.session().panels.has(panel::kPaneEditor));
    const std::size_t panes = e.r.session().setup.active.panes.size();
    const SourceOpened again = e.ask_open(b_path);
    CHECK_MESSAGE(again.accepted, again.refusal);
    CHECK(e.r.session().panels.has(panel::kPaneEditor));
    CHECK(e.r.session().setup.active.panes.size() == panes);
    CHECK(e.doc_row(0) == "two");
}

TEST_CASE("EDIT-W74: a real reload or removal of the Editor at queued intervals of an open cannot commit a stale preparation, keeps custody, and reclaims what the operation held") {
    SUBCASE("reload after the preparation: the bus ends the operation, the new incarnation holds A, and a fresh open takes") {
        EditorRig e("edit-reload-prepared");
        e.open();
        const std::string a_path = e.open_file("a.cpp", "one\n");
        put_bytes(e.root / "b.cpp", "two\n");
        const std::string b_path = spelled(e.root / "b.cpp");
        const std::size_t before = e.asker->opens.size();
        e.enqueue_open(b_path);
        e.pump_until_stage("prepare");
        const std::int64_t op = e.opening().op;
        REQUIRE(op != 0);
        e.enqueue_reload(); // queued behind the Editor's preparation
        // ONE TURN: the Editor prepares B and offers; then the maker's rebuilt image replaces
        // it in place -- same id, new incarnation, the document carried, no candidate.
        (void)e.r.bus.pump_pending();
        REQUIRE_MESSAGE(e.r.load_refusals.empty(),
                        "the reload was refused: ", e.r.load_refusals.back());
        CHECK(e.r.kernel.weave_id(pane::kEditorPaneStem) == e.image);
        const loom::JointStatus status = e.r.bus.joint_status(static_cast<std::uint64_t>(op));
        CHECK(status.state == loom::JointState::Aborted);
        CHECK(status.reason == loom::JointRefusal::ParticipantChanged);
        CHECK(e.r.bus.joint_retained_bytes() == 0); // the offer went with the incarnation
        e.settle();
        REQUIRE(e.asker->opens.size() == before + 1);
        const SourceOpened said = e.asker->opens.back();
        CHECK_FALSE(said.accepted);
        CHECK(said.refusal.find("replaced while opening") != std::string::npos);
        CHECK(e.opening().stage == "idle");
        // THE NEW INCARNATION HOLDS A -- no candidate rode -- and the desk is as it was.
        REQUIRE(e.row() != nullptr);
        e.kind = e.row()->kind;
        CHECK(e.read("path") == a_path);
        CHECK(e.read("text") == "one\n");
        CHECK(e.r.session().panels.has(e.kind));
        // ...AND A FRESH OPEN BINDS THE NEW INCARNATION AND TAKES.
        const SourceOpened again = e.ask_open(b_path);
        CHECK_MESSAGE(again.accepted, again.refusal);
        CHECK(e.doc_row(0) == "two");
        CHECK(e.read("opened_by") != "0");
    }
    SUBCASE("reload between the commitment and the owners' showing: the snapshot carries the published document") {
        EditorRig e("edit-reload-committed");
        e.open();
        e.open_file("a.cpp", "one\n");
        put_bytes(e.root / "b.cpp", "two\n");
        const std::string b_path = spelled(e.root / "b.cpp");
        const std::size_t before = e.asker->opens.size();
        e.enqueue_open(b_path);
        e.pump_until_stage("admit");
        (void)e.r.bus.pump_pending(); // the desk admits and offers
        const std::int64_t op = e.opening().op;
        e.enqueue_reload(); // queued behind the desk's admission answer
        // ONE TURN: the manager commits; then the reload snapshots the Editor -- which is
        // shown its published claim FIRST, so the snapshot carries B -- and revives the new
        // incarnation from it.
        (void)e.r.bus.pump_pending();
        REQUIRE_MESSAGE(e.r.load_refusals.empty(),
                        "the reload was refused: ", e.r.load_refusals.back());
        CHECK(e.r.bus.joint_status(static_cast<std::uint64_t>(op)).state ==
              loom::JointState::Committed);
        e.settle();
        REQUIRE(e.asker->opens.size() == before + 1);
        CHECK_MESSAGE(e.asker->opens.back().accepted, e.asker->opens.back().refusal);
        REQUIRE(e.row() != nullptr);
        e.kind = e.row()->kind;
        CHECK(e.read("path") == b_path);
        CHECK(e.read("text") == "two\n");
        CHECK(e.read("opened_by") == std::to_string(op));
        REQUIRE(e.r.session().panels.has(e.kind));
        CHECK(e.r.session().panels.selected == e.kind);
        CHECK(keyboard_pane(e.r.session().panels) == e.kind);
        CHECK(e.doc_row(0) == "two");
    }
    SUBCASE("removal after the preparation: the operation ends, the requester is told, the room's claims are reclaimed, and a later open is refused in words until an Editor is loaded again") {
        EditorRig e("edit-unload-prepared");
        e.open();
        const std::string a_path = e.open_file("a.cpp", "one\n");
        put_bytes(e.root / "b.cpp", "two\n");
        const std::string b_path = spelled(e.root / "b.cpp");
        const std::size_t before = e.asker->opens.size();
        e.enqueue_open(b_path);
        e.pump_until_stage("prepare");
        const std::int64_t op = e.opening().op;
        const std::size_t claims_before = e.r.bus.retained_claim_count();
        e.r.enqueue_unload(pane::kEditorPaneStem); // queued behind the Editor's preparation
        (void)e.r.bus.pump_pending();               // prepared, offered, then gone
        REQUIRE_MESSAGE(e.r.load_refusals.empty(),
                        "the unload was refused: ", e.r.load_refusals.back());
        CHECK_FALSE(e.r.kernel.is_loaded(pane::kEditorPaneStem));
        const loom::JointStatus status = e.r.bus.joint_status(static_cast<std::uint64_t>(op));
        CHECK(status.state == loom::JointState::Aborted);
        CHECK(status.reason == loom::JointRefusal::ParticipantChanged);
        CHECK(e.r.bus.joint_retained_bytes() == 0);
        CHECK(e.r.bus.retained_claim_count() < claims_before); // the removed Editor's claim is gone
        e.settle();
        REQUIRE(e.asker->opens.size() == before + 1);
        CHECK_FALSE(e.asker->opens.back().accepted);
        CHECK(e.asker->opens.back().refusal.find("replaced while opening") != std::string::npos);
        CHECK(e.opening().stage == "idle");
        // NO EDITOR: the next request is refused at once, in words; nothing is pending.
        const SourceOpened nobody = e.ask_open(b_path);
        CHECK_FALSE(nobody.accepted);
        CHECK(nobody.refusal.find("no Editor") != std::string::npos);
        CHECK(e.opening().stage == "idle");
        CHECK(e.r.bus.joint_pending() == 0);
        // ...AND WITH THE IMAGE LOADED AGAIN, THE SAME REQUEST TAKES.
        load::LoadPlan plan;
        load::ArtifactIntent seat;
        seat.stem = pane::kEditorPaneStem;
        seat.weave = load::WeaveIntent{pane::kEditorPaneRole};
        plan.artifacts.push_back(seat);
        const load::Executed done = e.r.run_plan(plan);
        REQUIRE_MESSAGE(done.ok, done.refusal);
        e.image = e.r.kernel.weave_id(pane::kEditorPaneStem);
        REQUIRE(e.image.value != 0);
        e.settle();
        REQUIRE(e.row() != nullptr);
        e.kind = e.row()->kind;
        const SourceOpened again = e.ask_open(b_path);
        CHECK_MESSAGE(again.accepted, again.refusal);
        CHECK(e.read("path") == b_path);
        CHECK(e.doc_row(0) == "two");
        (void)a_path;
    }
}

TEST_CASE("EDIT-W75: a silent or failed preparation stays pending and inspectable, a lost terminal answer undoes nothing, and no forged authority or attempt decides anything") {
    SUBCASE("a silent Editor leaves the open pending -- bounded, attributable, superseded by the next request -- and nothing else is blocked") {
        EditorRig e("edit-silent-prep");
        e.open_with_stand_in(/*throws=*/false, /*deaf=*/false);
        put_bytes(e.root / "b.cpp", "two\n");
        put_bytes(e.root / "c.cpp", "three\n");
        const std::string b_path = spelled(e.root / "b.cpp");
        const std::string c_path = spelled(e.root / "c.cpp");
        const std::size_t before = e.asker->opens.size();
        e.enqueue_open(b_path);
        e.settle();
        CHECK(e.asker->opens.size() == before); // no answer: not a refusal, not a success
        CHECK(e.broken->asked == 1);
        // PENDING, AND IT SAYS SO -- the manager's own record, read the way a probe reads it.
        CHECK(e.read_opening("stage") == "prepare");
        CHECK(e.read_opening("awaiting") == kEditorRole);
        CHECK(e.read_opening("path") == b_path);
        CHECK(e.read_opening("last_outcome").empty());
        CHECK(e.read_opening("attempt") != "0");
        const std::string op = e.read_opening("op");
        CHECK(op != "0");
        CHECK(e.r.bus.joint_pending() == 1);
        // ...AND ON THE DESK: a standing condition names the office it waits on.
        const Condition* pending = e.r.session().conditions.find("opening:" + op);
        REQUIRE(pending != nullptr);
        CHECK(pending->detail.find(kEditorRole) != std::string::npos);
        CHECK(pending->detail.find("prepare") != std::string::npos);
        // UNRELATED WORK IS NOT BLOCKED: the picker seats the Pane Manager.
        e.r.pick(ref_of(panel::kPaneEditor));
        CHECK(e.r.session().panels.has(panel::kPaneEditor));
        // A NEW REQUEST SUPERSEDES THE PENDING ONE: the first requester is told; the second is
        // pending in turn; nothing was ever fabricated.
        e.enqueue_open(c_path);
        e.settle();
        REQUIRE(e.asker->opens.size() == before + 1);
        CHECK_FALSE(e.asker->opens.back().accepted);
        CHECK(e.asker->opens.back().refusal.find("superseded") != std::string::npos);
        CHECK(e.read_opening("path") == c_path);
        CHECK(e.read_opening("stage") == "prepare");
        CHECK(e.r.bus.joint_pending() == 1);
        CHECK(e.read_opening("committed") == "0");
        CHECK(e.broken->settled == 1); // the superseded one was ended for its owner too
    }
    SUBCASE("a preparation that fails abnormally leaves the open pending the same way, and the failure is Loom's to surface") {
        EditorRig e("edit-failing-prep");
        e.open_with_stand_in(/*throws=*/true, /*deaf=*/false);
        put_bytes(e.root / "b.cpp", "two\n");
        const std::string b_path = spelled(e.root / "b.cpp");
        const std::size_t before = e.asker->opens.size();
        e.enqueue_open(b_path);
        // THE THROW SURFACES AT THE HOST'S LOOP -- caught, made a fact, rethrown (MSG-10) --
        // and the bus stays consistent: the turns after it deliver.
        bool threw = false;
        for (int i = 0; i < 12; ++i) {
            try {
                if (e.r.bus.pump_pending() == 0) {
                    break;
                }
            } catch (const std::exception&) {
                threw = true;
            }
        }
        CHECK(threw);
        CHECK(e.broken->asked == 1);
        CHECK(e.asker->opens.size() == before);
        CHECK(e.read_opening("stage") == "prepare");
        CHECK(e.read_opening("awaiting") == kEditorRole);
        CHECK(e.r.bus.joint_pending() == 1);
        CHECK(e.read_opening("committed") == "0");
    }
    SUBCASE("an Editor that does not accept the ask is refused at dispatch, and the refusal is attributed to the attempt") {
        EditorRig e("edit-deaf-prep");
        e.open_with_stand_in(/*throws=*/false, /*deaf=*/true);
        put_bytes(e.root / "b.cpp", "two\n");
        const std::string b_path = spelled(e.root / "b.cpp");
        const std::size_t before = e.asker->opens.size();
        e.enqueue_open(b_path);
        e.settle();
        REQUIRE(e.asker->opens.size() == before + 1);
        const SourceOpened said = e.asker->opens.back();
        CHECK_FALSE(said.accepted);
        CHECK(said.refusal.find("could not be") != std::string::npos);
        MESSAGE("attribution: ", said.refusal);
        CHECK(e.read_opening("stage") == "idle");
        CHECK(e.read_opening("last_outcome") == "refused");
        CHECK(e.r.bus.joint_pending() == 0);
    }
    SUBCASE("a requester replaced between its ask and the outcome loses only its answer: the open stands, counted as heard by nobody") {
        EditorRig e("edit-lost-answer");
        e.open();
        e.open_file("a.cpp", "one\n");
        put_bytes(e.root / "b.cpp", "two\n");
        put_bytes(e.root / "c.cpp", "three\n");
        const std::string b_path = spelled(e.root / "b.cpp");
        const std::int64_t committed = e.opening().committed;
        e.enqueue_open(b_path);
        e.pump_until_stage("admit");
        // THE REQUESTER LEAVES: its weave is removed from the bus (a reload of Files, an unload).
        (void)e.r.bus.unregister_weave(e.asker->id);
        e.asker = nullptr;
        e.settle();
        CHECK(e.opening().committed == committed + 1);
        CHECK(e.opening().answers_lost == 1);
        CHECK(e.opening().last_outcome == "committed, answer lost");
        CHECK(e.read("path") == b_path);
        REQUIRE(e.r.session().panels.has(e.kind));
        CHECK(e.r.session().panels.keyboard == e.kind);
        CHECK(e.doc_row(0) == "two");
        // ...AND THE NEXT REQUESTER IS ANSWERED AS EVER.
        e.mount_asker();
        const SourceOpened next = e.ask_open(spelled(e.root / "c.cpp"));
        CHECK_MESSAGE(next.accepted, next.refusal);
        CHECK(e.doc_row(0) == "three");
    }
    SUBCASE("a forged authority begins, commits and cancels nothing; a forged answer or refusal decides nothing") {
        EditorRig e("edit-forged-authority");
        e.open();
        e.open_file("a.cpp", "one\n");
        put_bytes(e.root / "b.cpp", "two\n");
        const std::string b_path = spelled(e.root / "b.cpp");
        const std::size_t before = e.asker->opens.size();
        const std::int64_t committed = e.opening().committed;
        e.enqueue_open(b_path);
        e.pump_until_stage("admit");
        const std::uint64_t op = static_cast<std::uint64_t>(e.opening().op);
        const std::int64_t attempt = e.opening().attempt;
        REQUIRE(op != 0);
        // A STRANGER WITH A DEFAULT-CONSTRUCTED AUTHORITY: every verb is refused by name...
        loom::JointBegin begun;
        loom::JointResult committed_by_stranger;
        loom::JointResult cancelled_by_stranger;
        e.asker->next = [&](DoorAsker&, loom::Mail& mail) {
            begun = mail.begin_joint(
                loom::JointAuthority{},
                {loom::claim_key<EditorDocument>(std::string_view(kEditorRole)),
                 loom::claim_key<PanePresentation>(std::string_view(kWorkshopProvider))});
            committed_by_stranger = mail.commit_joint(loom::JointAuthority{}, op);
            cancelled_by_stranger = mail.cancel_joint(loom::JointAuthority{}, op);
            // ...AND FORGED ANSWERS: an admission with the right operation number but no ask
            // behind it, and a dispatch refusal with the right attempt spelled as speech.
            (void)mail.as_role(kDoorAskerOffice)
                .send_to_role(kOpeningRole, PresentationAdmitted{static_cast<std::int64_t>(op),
                                                                 false, "forged"});
            loom::DispatchRefused forged;
            forged.attempt = std::to_string(attempt);
            forged.role = kWorkshopProvider;
            forged.shape = PresentationAdmitRequested::zen_name;
            forged.version = PresentationAdmitRequested::zen_version;
            forged.reason = "NoSuchTarget";
            (void)mail.as_role(kDoorAskerOffice).send_to_role(kOpeningRole, forged);
        };
        (void)e.r.bus.send(e.asker->id, loom::Message(loom::to_value(SeatDo{}), loom::WeaveId{},
                                                      loom::WeaveId{}, 0));
        (void)e.r.bus.pump_pending(); // the desk admits; the stranger speaks behind it
        CHECK_FALSE(begun.ok);
        CHECK(begun.why == loom::JointRefusal::ForeignAuthority);
        CHECK_FALSE(committed_by_stranger.ok);
        CHECK(committed_by_stranger.why == loom::JointRefusal::ForeignAuthority);
        CHECK_FALSE(cancelled_by_stranger.ok);
        CHECK(e.r.bus.joint_status(op).state == loom::JointState::Preparing);
        // ...AND THE FLIGHT COMPLETES AS IF NONE OF IT WAS SAID.
        e.settle();
        REQUIRE(e.asker->opens.size() == before + 1);
        CHECK_MESSAGE(e.asker->opens.back().accepted, e.asker->opens.back().refusal);
        CHECK(e.opening().committed == committed + 1);
        CHECK(e.opening().refused == 0);
        CHECK(e.read("path") == b_path);
        CHECK(e.doc_row(0) == "two");
    }
}

TEST_CASE("EDIT-W76: a stale clipboard answer clears its bookkeeping, a reload carries none, and a fresh open is eligible after each") {
    SUBCASE("after an edit: the answer is refused in words, the flight is cleared, and the open takes") {
        EditorRig e("edit-stale-paste");
        e.open(160, 48, /*pick_it=*/true, /*slow_skin=*/true);
        e.open_file("a.cpp", "one\n");
        put_bytes(e.root / "b.cpp", "two\n");
        const std::string b_path = spelled(e.root / "b.cpp");
        e.press_doc(0, 3);
        e.slow->text = "PASTED";
        e.key(input::scan::kV, input::mod::kCtrl);
        REQUIRE(e.slow->held);
        e.key(input::scan::kLeft); // the source moved under the ask
        CHECK_FALSE(e.ask_open(b_path).accepted); // still arriving: refused in words
        e.answer_now();
        CHECK(e.read("text") == "one\n");
        CHECK(e.says("after the source moved")); // the answer's own fate, said
        CHECK(e.clean());
        // THE BOOKKEEPING IS CLEARED WITH THE ANSWER, before its payload was judged: the same
        // request is eligible at once, and the exit is not refused for a paste.
        const SourceOpened opened = e.ask_open(b_path);
        CHECK_MESSAGE(opened.accepted, opened.refusal);
        CHECK(e.doc_row(0) == "two");
        CHECK(e.quit_by_key());
    }
    SUBCASE("across a reload: the new incarnation carries no paste in flight, the late answer reaches no incarnation that asked, and the open takes") {
        EditorRig e("edit-paste-reload");
        e.open(160, 48, /*pick_it=*/true, /*slow_skin=*/true);
        const std::string a_path = e.open_file("a.cpp", "one\n");
        put_bytes(e.root / "b.cpp", "two\n");
        const std::string b_path = spelled(e.root / "b.cpp");
        e.press_doc(0, 3);
        e.slow->text = "PASTED";
        e.key(input::scan::kV, input::mod::kCtrl);
        REQUIRE(e.slow->held);
        CHECK_FALSE(e.ask_open(b_path).accepted); // waits for the maker's paste
        e.enqueue_reload();
        e.settle();
        REQUIRE_MESSAGE(e.r.load_refusals.empty(),
                        "the reload was refused: ", e.r.load_refusals.back());
        REQUIRE(e.row() != nullptr);
        e.kind = e.row()->kind;
        CHECK(e.read("path") == a_path);
        CHECK(e.read("text") == "one\n");
        // THE LATE ANSWER REACHES NO INCARNATION THAT ASKED: nothing is pasted, nothing said.
        e.answer_now();
        CHECK_FALSE(e.slow->held);
        CHECK(e.read("text") == "one\n");
        CHECK_FALSE(e.says("pasted"));
        // ...AND THE OPEN IS ELIGIBLE: no orphan flag refuses it.
        const SourceOpened opened = e.ask_open(b_path);
        CHECK_MESSAGE(opened.accepted, opened.refusal);
        CHECK(e.doc_row(0) == "two");
    }
}

// ============================================================================
// MEASUREMENT: the cost of the changed paths
// ============================================================================
// THE CORRECTIONS: the old door's promise, and a
// loaded owner that cannot apply what was published
// ============================================================================

TEST_CASE("EDIT-W77: the old door still opens and shows, or refuses truthfully, by a kept answer right") {
    SUBCASE("a successful open: seated, selected, keyed, and answered by Loom's own word to the asker's ask") {
        EditorRig e("edit-old-door-opens");
        e.open(160, 48, /*pick_it=*/false);
        REQUIRE_FALSE(e.r.session().panels.has(e.kind));
        put_bytes(e.root / "a.cpp", "one\n");
        const std::string a_path = spelled(e.root / "a.cpp");
        const SourceOpened said = e.ask_open_direct(a_path);
        CHECK_MESSAGE(said.accepted, said.refusal);
        // THE ANSWER'S PROVENANCE IS THE REQUESTER'S OWN: the Editor spent the requester's
        // kept right, so Loom says this answers the asker's ask -- not the Editor's relay.
        REQUIRE_FALSE(e.asker->opens_authentic.empty());
        CHECK(e.asker->opens_authentic.back());
        // THE WHOLE PROMISE: the document, and the presentation, and the keys.
        REQUIRE(e.r.session().panels.has(e.kind));
        CHECK(e.r.session().panels.selected == e.kind);
        CHECK(e.r.session().panels.keyboard == e.kind);
        CHECK(e.r.session().notice.find("showing Editor") != std::string::npos);
        CHECK(e.doc_row(0) == "one");
        CHECK(e.read("path") == a_path);
        CHECK(e.read("opened_by") != "0"); // installed by the managed operation, not alone
        // THE MANAGER'S CONVERSATION WAS THE EDITOR'S, NOT THE ASKER'S: its requester of
        // record is the Editor's weave, and the flight settled applied.
        CHECK(e.opening().requester == static_cast<std::int64_t>(e.image.value));
        CHECK(e.opening().committed == 1);
        CHECK(e.opening().last_outcome == "committed");
    }
    SUBCASE("no room refuses the open in the picker's words, and nothing is authored or moved") {
        EditorRig e("edit-old-door-noroom");
        e.open(160, kMinScreen.h, /*pick_it=*/false);
        put_bytes(e.root / "first.cpp", "first\n");
        REQUIRE(e.ask_open_direct(spelled(e.root / "first.cpp")).accepted);
        REQUIRE(e.r.session().panels.has(e.kind));
        e.unfocus();
        e.r.pick(editor_ref()); // the picker's other direction: it removes the open pane
        e.r.pick(ref_of(panel::kPaneEditor));
        REQUIRE(e.r.session().panels.has(panel::kPaneEditor));
        REQUIRE_FALSE(e.r.session().panels.has(e.kind));
        put_bytes(e.root / "a.cpp", "held\n");
        const SourceOpened said = e.ask_open_direct(spelled(e.root / "a.cpp"));
        CHECK_FALSE(said.accepted);
        CHECK(said.refusal == "no room for Editor on this screen -- make the window taller, "
                              "then p again");
        CHECK(e.asker->opens_authentic.back());
        CHECK(e.r.session().notice == said.refusal);
        CHECK_FALSE(has_pane(e.r.session().setup.active, editor_ref()));
        CHECK(e.read("path").find("first.cpp") != std::string::npos);
        CHECK(e.read("text") == "first\n");
    }
    SUBCASE("a dirty document refuses a different source, in the judge's words") {
        EditorRig e("edit-old-door-dirty");
        e.open();
        put_bytes(e.root / "a.cpp", "one\n");
        const std::string a_path = spelled(e.root / "a.cpp");
        REQUIRE(e.ask_open_direct(a_path).accepted);
        e.press_doc(0, 3);
        e.type("Z");
        REQUIRE(e.dirty());
        put_bytes(e.root / "b.cpp", "two\n");
        const SourceOpened said = e.ask_open_direct(spelled(e.root / "b.cpp"));
        CHECK_FALSE(said.accepted);
        CHECK(said.refusal.find("unsaved changes") != std::string::npos);
        CHECK(e.read("path") == a_path);
        CHECK(e.read("text") == "oneZ\n");
        CHECK(e.r.session().panels.keyboard == e.kind);
    }
    SUBCASE("a paste still arriving refuses the open, and the answer lands in A") {
        EditorRig e("edit-old-door-paste");
        e.open(160, 48, /*pick_it=*/true, /*slow_skin=*/true);
        put_bytes(e.root / "a.cpp", "one\n");
        const std::string a_path = spelled(e.root / "a.cpp");
        REQUIRE(e.ask_open_direct(a_path).accepted);
        put_bytes(e.root / "b.cpp", "bee\n");
        e.press_doc(0, 3);
        e.slow->text = "STRAY";
        e.key(input::scan::kV, input::mod::kCtrl);
        REQUIRE(e.slow->held);
        const SourceOpened waited = e.ask_open_direct(spelled(e.root / "b.cpp"));
        CHECK_FALSE(waited.accepted);
        CHECK(waited.refusal.find("clipboard answer") != std::string::npos);
        CHECK(e.read("path") == a_path);
        e.answer_now();
        CHECK(e.doc_row(0) == "oneSTRAY");
        CHECK(e.dirty());
    }
    SUBCASE("no opening office is held: the relay's attempt is refused at dispatch, and the requester is told by that attempt -- never a document-only success") {
        EditorRig e("edit-old-door-no-manager");
        e.open(160, 48, /*pick_it=*/true, /*slow_skin=*/false, EditorRig::Project::kDoor,
               /*with_manager=*/false);
        put_bytes(e.root / "a.cpp", "one\n");
        const std::string notice = e.r.session().notice;
        const SourceOpened said = e.ask_open_direct(spelled(e.root / "a.cpp"));
        CHECK_FALSE(said.accepted);
        CHECK(said.refusal.find("could not be reached") != std::string::npos);
        CHECK(said.refusal.find("NoSuchTarget") != std::string::npos);
        CHECK(e.asker->opens_authentic.back());
        CHECK(e.no_source()); // nothing was installed in the presentation's absence
        CHECK(e.r.session().notice == notice);
    }
    SUBCASE("a relay that cannot be authored is refused at once, in words: nothing was queued") {
        // THE ONE IMMEDIATE ENQUEUE FAILURE THE RELAY CAN MEET: the Editor speaks to the
        // opening office AS its own office, and an image loaded under a different role cannot
        // author that -- Loom refuses the authorship at the enqueue (RoleAuthorshipDenied),
        // no attempt exists, and the Editor answers the requester now rather than installing
        // a document in the presentation's place.
        EditorRig e("edit-old-door-unauthored");
        e.open_under_role("zengine.editor-elsewhere");
        put_bytes(e.root / "a.cpp", "one\n");
        const std::string a_path = spelled(e.root / "a.cpp");
        const std::size_t before = e.asker->opens.size();
        e.asker_says([&e, a_path](DoorAsker&, loom::Mail& mail) {
            (void)mail.as_role(kDoorAskerOffice).send(e.image, OpenSourceRequested{a_path});
        });
        REQUIRE(e.asker->opens.size() == before + 1);
        const SourceOpened said = e.asker->opens.back();
        CHECK_FALSE(said.accepted);
        CHECK(said.refusal.find("nothing was queued") != std::string::npos);
        CHECK(e.asker->opens_authentic.back());
        CHECK(e.read("path").empty());
        CHECK(e.opening().stage == "idle"); // the manager never heard of it
        CHECK(e.opening().refused == 0);
    }
    SUBCASE("the opening office may not ask the old door: the loop is refused by name") {
        EditorRig e("edit-old-door-loop");
        e.open();
        put_bytes(e.root / "a.cpp", "one\n");
        const std::string a_path = spelled(e.root / "a.cpp");
        // A stranger holding the opening office's spelling cannot author as it; the real
        // manager can, and the Editor refuses exactly that authorship at its public door.
        // Said through the manager's own office by a seat that holds it in the manager's
        // place: the manager is removed, and the asker takes the role.
        REQUIRE(e.r.bus.unregister_weave(e.r.opening_id) != nullptr);
        e.r.opening = nullptr;
        auto seat = std::make_unique<DoorAsker>(std::string(kOpeningRole));
        DoorAsker* opening = seat.get();
        loom::Grant grant;
        grant.allow_to_any(OpenSourceRequested::zen_name, OpenSourceRequested::zen_version);
        const loom::WeaveId id =
            e.r.bus.register_weave(std::move(seat), std::move(grant), std::string(kOpeningRole));
        opening->zen_set_self(id);
        opening->id = id;
        opening->next = [a_path](DoorAsker& a, loom::Mail& mail) {
            a.ask(mail, kEditorRole, OpenSourceRequested{a_path});
        };
        (void)e.r.bus.send(id, loom::Message(loom::to_value(SeatDo{}), loom::WeaveId{},
                                             loom::WeaveId{}, 0));
        e.settle();
        REQUIRE(opening->opens.size() == 1);
        CHECK_FALSE(opening->opens.back().accepted);
        CHECK(opening->opens.back().refusal.find("nothing was relayed") != std::string::npos);
        CHECK(e.read("path").empty());
    }
    SUBCASE("the manager is removed while a relay is outstanding: no answer is invented, and a new manager serves the next ask") {
        EditorRig e("edit-old-door-manager-gone");
        e.open();
        put_bytes(e.root / "a.cpp", "one\n");
        const std::string a_path = spelled(e.root / "a.cpp");
        REQUIRE(e.ask_open_direct(a_path).accepted);
        put_bytes(e.root / "b.cpp", "two\n");
        const std::string b_path = spelled(e.root / "b.cpp");
        const std::size_t before = e.asker->opens.size();
        e.enqueue_open_direct(b_path);
        e.pump_until_stage("prepare");
        REQUIRE(e.r.bus.unregister_weave(e.r.opening_id) != nullptr);
        e.r.opening = nullptr;
        e.settle();
        // SILENCE, TRUTHFULLY: the party that owed the relay's answer is gone, its right went
        // with it, and the Editor fabricates nothing for the requester.
        CHECK(e.asker->opens.size() == before);
        CHECK(e.read("path") == a_path);
        CHECK(e.doc_row(0) == "one");
        // A NEW MANAGER, AND THE NEXT ASK AT THE OLD DOOR TAKES.
        e.r.mount_opening();
        const SourceOpened again = e.ask_open_direct(b_path);
        CHECK_MESSAGE(again.accepted, again.refusal);
        CHECK(e.doc_row(0) == "two");
        CHECK(e.asker->opens_authentic.back());
    }
    SUBCASE("the Editor is reloaded while relaying: the old incarnation's rights die with it, the manager counts the lost answer, and the successor serves the next ask") {
        EditorRig e("edit-old-door-editor-reloaded");
        e.open();
        put_bytes(e.root / "a.cpp", "one\n");
        const std::string a_path = spelled(e.root / "a.cpp");
        REQUIRE(e.ask_open_direct(a_path).accepted);
        put_bytes(e.root / "b.cpp", "two\n");
        const std::string b_path = spelled(e.root / "b.cpp");
        const std::size_t before = e.asker->opens.size();
        e.enqueue_open_direct(b_path);
        e.pump_until_stage("prepare");
        e.enqueue_reload(); // queued behind the Editor's preparation
        (void)e.r.bus.pump_pending();
        REQUIRE_MESSAGE(e.r.load_refusals.empty(),
                        "the reload was refused: ", e.r.load_refusals.back());
        e.settle();
        // THE MANAGER'S ANSWER TO THE RELAY REACHED NO INCARNATION THAT ASKED: counted as a
        // lost answer at the manager, and the requester heard nothing false.
        CHECK(e.asker->opens.size() == before);
        CHECK(e.opening().answers_lost == 1);
        CHECK(e.opening().last_outcome.find("answer lost") != std::string::npos);
        REQUIRE(e.row() != nullptr);
        e.kind = e.row()->kind;
        CHECK(e.read("path") == a_path);
        CHECK(e.read("text") == "one\n");
        const SourceOpened again = e.ask_open_direct(b_path);
        CHECK_MESSAGE(again.accepted, again.refusal);
        CHECK(e.doc_row(0) == "two");
    }
}

TEST_CASE("EDIT-W78: a loaded owner that cannot apply the published claim is held, named, and reloaded") {
    EditorRig e("edit-loaded-failing-owner");
    e.open(160, 48, /*pick_it=*/false, /*slow_skin=*/false, EditorRig::Project::kDoor,
           /*with_manager=*/true, "zengine-failing-editor");
    REQUIRE_FALSE(e.r.session().panels.has(e.kind));
    // ARMED THROUGH THE SUBSTRATE'S OWN WRITE DOOR: the stand-in's next showing will fail.
    (void)e.r.bus.send(e.image, loom::Message(loom::to_value(loom::PokeWrite{"fail_next", "1"}),
                                              loom::WeaveId{}, e.poke_id, ++e.poke_corr));
    e.settle();
    REQUIRE(e.read("fail_next") == "1");
    put_bytes(e.root / "b.cpp", "two\n");
    const std::string b_path = spelled(e.root / "b.cpp");
    const std::size_t before = e.asker->opens.size();
    e.enqueue_open(b_path);
    e.pump_until_stage("apply");
    const std::int64_t op = e.opening().op;
    REQUIRE(op != 0);
    CHECK(e.r.bus.joint_status(static_cast<std::uint64_t>(op)).state == loom::JointState::Committed);
    CHECK(e.r.bus.joint_status(static_cast<std::uint64_t>(op)).application ==
          loom::JointApplication::Pending);
    e.settle();
    // THE TERMINAL ANSWER IS TRUTHFUL: published, not applied, naming the owner that failed.
    REQUIRE(e.asker->opens.size() == before + 1);
    const SourceOpened said = e.asker->opens.back();
    CHECK_FALSE(said.accepted);
    CHECK(said.refusal.find("could not apply") != std::string::npos);
    CHECK(said.refusal.find(kEditorRole) != std::string::npos);
    CHECK(said.refusal.find("published") != std::string::npos);
    CHECK(e.opening().last_outcome == "committed, application failed");
    CHECK(e.opening().unapplied == 1);
    CHECK(e.opening().committed == 0);
    CHECK(e.opening().stage == "idle");
    CHECK(e.opening().last_refusal == said.refusal);
    // THE BUS'S RECORD: this participant, this operation, held.
    {
        const loom::JointStatus status = e.r.bus.joint_status(static_cast<std::uint64_t>(op));
        CHECK(status.state == loom::JointState::Committed);
        CHECK(status.application == loom::JointApplication::Failed);
        CHECK(status.failed == e.image);
        CHECK(status.failed_role == kEditorRole);
    }
    CHECK(e.r.bus.has_failed_application(e.image));
    // THE CLAIMS SAY B -- the commitment stands -- and the desk applied its own half: seated,
    // keyed, with the words a maker needs.
    {
        const std::optional<EditorDocument> doc = e.document_claim();
        REQUIRE(doc.has_value());
        CHECK(doc->path == b_path);
        CHECK(doc->opened_by == op);
    }
    REQUIRE(e.r.session().panels.has(e.kind));
    CHECK(e.r.session().panels.keyboard == e.kind);
    CHECK(e.r.session().notice.find("could not apply") != std::string::npos);
    CHECK(e.r.session().conditions.find("opening:" + std::to_string(op)) == nullptr);
    // THE HELD OWNER: a delivery to it is refused by exact attempt, and nothing is retried
    // -- the diagnostic read, which runs nothing, shows the one attempt and no application.
    const loom::Ticket poke = e.r.bus.send(
        e.image, loom::Message(loom::to_value(loom::PokeRead{"applied"}), loom::WeaveId{},
                               e.poke_id, ++e.poke_corr));
    e.settle();
    CHECK(e.r.bus.outcome(poke).disposition == loom::Disposition::Refused);
    CHECK(e.r.bus.outcome(poke).refusal.reason == loom::RefusalReason::ApplicationFailed);
    {
        const std::shared_ptr<const loom::Schema> shape =
            e.r.bus.resolve_schema("FailingEditorState", 1);
        REQUIRE(shape != nullptr);
        const loom::Admission a = loom::admit(
            loom::parse(e.r.bus.snapshot_bytes(e.image, loom::Switchboard::SnapshotAccess::Diagnostic)),
            shape);
        REQUIRE(a.ok());
        CHECK(a.value().get("published_seen")->as_int() == 1);
        CHECK(a.value().get("applied")->as_int() == 0);
        CHECK(a.value().get("fail_next")->as_int() == 0);
        CHECK(e.r.bus.has_failed_application(e.image)); // the read changed nothing
    }
    // UNRELATED WORK GOES ON: the picker seats the Pane Manager.
    e.unfocus();
    e.r.pick(ref_of(panel::kPaneEditor));
    CHECK(e.r.session().panels.has(panel::kPaneEditor));
    // THE REPAIR: a real reload through the control door. The successor is shown the
    // published value at its first delivery (its own activation), applies it, and the bus
    // tells the manager -- which re-reads the record it RETAINED for exactly this late word
    //, records it, and releases the record.
    CHECK(e.opening().retained == op);
    e.enqueue_reload("zengine-failing-editor");
    e.settle();
    REQUIRE_MESSAGE(e.r.load_refusals.empty(), "the reload was refused: ", e.r.load_refusals.back());
    CHECK(e.r.kernel.weave_id("zengine-failing-editor") == e.image);
    CHECK_FALSE(e.r.bus.has_failed_application(e.image));
    CHECK(e.read("applied") == "1");
    CHECK(e.read("published_seen") == "2"); // the predecessor's attempt rode; the successor's applied
    CHECK(e.read("path") == b_path);
    CHECK(e.opening().last_outcome == "committed, applied after repair");
    CHECK(e.opening().retained == 0);
    CHECK(e.r.bus.joint_status(static_cast<std::uint64_t>(op)).state ==
          loom::JointState::Missing); // consumed for the late word, then released
    CHECK(e.r.bus.joint_records() == 0);
}

// ============================================================================
//
// TEST-LOCAL COORDINATORS. Two instruments for two
// questions the real manager cannot be made to ask: what the REAL desk answers when it is
// shown a presentation it holds no trial for, and whether the real manager's outcome survives
// an UNRELATED coordination begun on the same bus before its notice was consumed. Neither is
// a product; both hold real authorities minted by the rig's bus and speak through real doors.

namespace {

struct CoordinatorState {
    std::int64_t n = 0;
    ZEN_SHAPE(CoordinatorState, 1, ZEN_FIELD(n));
};

/// AN OPERATOR IN THE OPENING OFFICE THAT ARRANGES THE DESK'S HALF ALONE -- a trial, an
/// admission with one row -- then tells the desk the operation did NOT commit (the desk drops
/// its trial, as it must), and only THEN commits the publication the desk had offered. The
/// real desk is shown a presentation it no longer holds a trial for: the fallback branch of
/// its hook, reachable by no mechanism but this instrumentation.
class DeskOnlyOperator
    : public loom::WeaveBase<DeskOnlyOperator, CoordinatorState,
                             loom::Accept<SeatDo, PresentationTrial, PresentationAdmitted,
                                          loom::JointApplied>,
                             loom::Emit<PresentationTrialRequested, PresentationAdmitRequested,
                                        ManagedOpenSettled>> {
public:
    void on(const SeatDo&, loom::Mail& mail) {
        if (step == 0) {
            begun = mail.begin_joint(
                authority, {loom::claim_key<PanePresentation>(std::string_view(kWorkshopProvider))});
            if (begun.ok) {
                op = begun.op;
                (void)mail.as_role(kOpeningRole)
                    .send_to_role(kWorkshopProvider,
                                  PresentationTrialRequested{static_cast<std::int64_t>(op),
                                                             pane.provider, pane.pane},
                                  op);
            }
            step = 1;
        } else if (step == 2) {
            committed = mail.commit_joint(authority, op);
            step = 3;
        } else if (step == 3) {
            released = mail.release_joint(authority, op);
        }
    }
    void on(const PresentationTrial& said, loom::Mail& mail) {
        if (!mail.answers_ask()) {
            return;
        }
        trial = said;
        if (!said.ok) {
            return;
        }
        PresentationAdmitRequested admit;
        admit.op = static_cast<std::int64_t>(op);
        admit.provider = pane.provider;
        admit.pane = pane.pane;
        admit.generation = 1;
        surface::SurfaceTextRow row;
        row.text = "x";
        admit.rows.push_back(row);
        admit.caret_row = 0;
        admit.caret_col = 0;
        (void)mail.as_role(kOpeningRole).send_to_role(kWorkshopProvider, admit, op);
    }
    void on(const PresentationAdmitted& said, loom::Mail& mail) {
        if (!mail.answers_ask()) {
            return;
        }
        admitted = said;
        if (!said.ok) {
            return;
        }
        // THE INSTRUMENT: the desk is told the operation did not commit -- it drops its
        // trial -- while the offer it made still stands at the bus, unrevoked.
        (void)mail.as_role(kOpeningRole)
            .send_to_role(kWorkshopProvider,
                          ManagedOpenSettled{static_cast<std::int64_t>(op), false, false,
                                             std::string(), std::string()});
        step = 2;
    }
    void on(const loom::JointApplied& said, loom::Mail& mail) {
        applied.push_back(said);
        applied_status.push_back(mail.joint_status(authority, said.applied_op()));
    }
    loom::JointAuthority authority;
    PaneRef pane;
    int step = 0;
    std::uint64_t op = 0;
    loom::JointBegin begun{};
    loom::JointResult committed{};
    loom::JointResult released{};
    PresentationTrial trial{};
    PresentationAdmitted admitted{};
    std::vector<loom::JointApplied> applied;
    std::vector<loom::JointStatus> applied_status;
};

/// A FACT NOBODY IN THE OPENING KNOWS, and two owners of it in offices of their own.
struct TestFact {
    std::string v;
    ZEN_SHAPE(TestFact, 1, ZEN_FIELD(v));
};
struct FactState {
    std::string v = "a";
    ZEN_SHAPE(FactState, 1, ZEN_FIELD(v));
};
class FactOwner : public loom::WeaveBase<FactOwner, FactState, loom::Accept<SeatDo>, loom::Emit<>,
                                         loom::Claims<TestFact>> {
public:
    void on(const SeatDo&, loom::Mail& mail) { claimed = mail.claim(TestFact{state_.v}); }
    loom::SenseClaimResult claimed{};
};

/// AN UNRELATED COORDINATOR: begins one operation over the two facts and nothing else. On
/// the START slice its begin took the slot of the manager's committed operation.
class UnrelatedCoordinator
    : public loom::WeaveBase<UnrelatedCoordinator, CoordinatorState, loom::Accept<SeatDo>,
                             loom::Emit<>> {
public:
    void on(const SeatDo&, loom::Mail& mail) {
        begun = mail.begin_joint(authority, {loom::claim_key<TestFact>(a), loom::claim_key<TestFact>(b)});
    }
    loom::JointAuthority authority;
    loom::WeaveId a{};
    loom::WeaveId b{};
    loom::JointBegin begun{};
};

/// The unrelated coordination, mounted on a rig's bus: two owners that have claimed, and the
/// coordinator ready to begin when nudged.
struct Unrelated {
    loom::WeaveId a{};
    loom::WeaveId b{};
    loom::WeaveId coordinator_id{};
    UnrelatedCoordinator* coordinator = nullptr;

    explicit Unrelated(PaneRig& r) {
        auto owner_a = std::make_unique<FactOwner>();
        FactOwner* raw_a = owner_a.get();
        a = r.bus.register_weave(std::move(owner_a), loom::Grant{}, std::string("test.fact.a"));
        raw_a->zen_set_self(a);
        auto owner_b = std::make_unique<FactOwner>();
        FactOwner* raw_b = owner_b.get();
        b = r.bus.register_weave(std::move(owner_b), loom::Grant{}, std::string("test.fact.b"));
        raw_b->zen_set_self(b);
        auto c = std::make_unique<UnrelatedCoordinator>();
        coordinator = c.get();
        coordinator_id =
            r.bus.register_weave(std::move(c), loom::Grant{}, std::string("test.coordinator"));
        coordinator->zen_set_self(coordinator_id);
        coordinator->authority =
            r.bus.mint_joint_authority(coordinator_id, {"test.fact.a", "test.fact.b"});
        coordinator->a = a;
        coordinator->b = b;
        (void)r.bus.send(a, loom::Message(loom::to_value(SeatDo{}), loom::WeaveId{}, loom::WeaveId{}, 0));
        (void)r.bus.send(b, loom::Message(loom::to_value(SeatDo{}), loom::WeaveId{}, loom::WeaveId{}, 0));
        r.bus.drain_until_idle();
        REQUIRE(raw_a->claimed.accepted);
        REQUIRE(raw_b->claimed.accepted);
    }
    /// QUEUE THE BEGIN, undrained: the case decides the turn it lands in.
    void enqueue_begin(PaneRig& r) {
        (void)r.bus.send(coordinator_id, loom::Message(loom::to_value(SeatDo{}), loom::WeaveId{},
                                                       loom::WeaveId{}, 0));
    }
};

} // namespace

TEST_CASE("EDIT-W80: the real desk, shown a presentation it holds no trial for, answers that it did not apply it -- Declined, not held, named, and re-claiming its own truth") {
    EditorRig e("edit-desk-declines");
    e.open(160, 48, /*pick_it=*/false, /*slow_skin=*/false, EditorRig::Project::kDoor,
           /*with_manager=*/false);
    REQUIRE_FALSE(e.r.session().panels.has(e.kind));
    // THE INSTRUMENT, IN THE OPENING OFFICE, with the grants the host gives the manager for
    // exactly this conversation and an authority over the desk's office alone.
    auto seat = std::make_unique<DeskOnlyOperator>();
    DeskOnlyOperator* op = seat.get();
    op->pane = editor_ref();
    loom::Grant arrange;
    arrange.allow_to_role(PresentationTrialRequested::zen_name, PresentationTrialRequested::zen_version,
                          kWorkshopProvider);
    arrange.allow_to_role(PresentationAdmitRequested::zen_name, PresentationAdmitRequested::zen_version,
                          kWorkshopProvider);
    arrange.allow_to_role(ManagedOpenSettled::zen_name, ManagedOpenSettled::zen_version,
                          kWorkshopProvider);
    const loom::WeaveId op_id =
        e.r.bus.register_weave(std::move(seat), std::move(arrange), std::string(kOpeningRole));
    op->zen_set_self(op_id);
    op->authority = e.r.bus.mint_joint_authority(op_id, {std::string(kWorkshopProvider)});
    const auto nudge = [&] {
        (void)e.r.bus.send(op_id, loom::Message(loom::to_value(SeatDo{}), loom::WeaveId{},
                                                loom::WeaveId{}, 0));
        e.settle();
    };
    // THE DESK'S HALF: bound, tried, admitted, offered -- then told the operation did not
    // commit, so it dropped its trial.
    nudge();
    REQUIRE_MESSAGE(op->begun.ok, loom::name_of(op->begun.why));
    REQUIRE_MESSAGE(op->trial.ok, op->trial.refusal);
    REQUIRE_MESSAGE(op->admitted.ok, op->admitted.refusal);
    CHECK(e.r.bus.joint_status(op->op).state == loom::JointState::Preparing);
    CHECK(e.r.bus.joint_retained_bytes() > 0); // the desk's offer still stands
    // THE COMMITMENT, on an offer whose trial is gone: the claim says seated, selected, keyed.
    nudge();
    REQUIRE_MESSAGE(op->committed.ok, loom::name_of(op->committed.why));
    CHECK(e.r.bus.has_unobserved_publication(e.r.workshop_id));
    {
        const std::optional<PanePresentation> claim = e.presentation_claim();
        REQUIRE(claim.has_value());
        CHECK(claim->seated);
        CHECK(claim->shown_by == static_cast<std::int64_t>(op->op));
    }
    // THE SHOWING, through the ordinary snapshot: the real desk's hook finds no trial, keeps
    // the desk as it is, says so -- and answers Declined. The snapshot is SERVED; nothing is
    // held; the pane is not on the desk.
    CHECK_NOTHROW((void)e.r.bus.snapshot_bytes(e.r.workshop_id));
    CHECK_FALSE(e.r.bus.has_failed_application(e.r.workshop_id));
    CHECK_FALSE(e.r.bus.has_unobserved_publication(e.r.workshop_id));
    CHECK_FALSE(e.r.session().panels.has(e.kind));
    CHECK(e.r.session().notice.find("did not prepare") != std::string::npos);
    {
        const loom::JointStatus status = e.r.bus.joint_status(op->op);
        CHECK(status.state == loom::JointState::Committed);
        CHECK(status.application == loom::JointApplication::Declined);
        CHECK(status.failed == e.r.workshop_id);
        CHECK(status.failed_role == kWorkshopProvider);
    }
    // THE OPERATOR IS TOLD ONCE, naming the desk, with the reason Declined.
    e.settle();
    REQUIRE(op->applied.size() == 1);
    CHECK_FALSE(op->applied[0].applied);
    CHECK(op->applied[0].reason == std::string(loom::name_of(loom::JointApplication::Declined)));
    CHECK(op->applied[0].failed_claimant() == e.r.workshop_id);
    CHECK(op->applied[0].role == kWorkshopProvider);
    CHECK(op->applied_status[0].application == loom::JointApplication::Declined);
    // THE DESK WORKS ON, and at the end of its next delivery re-claims its own truth: not
    // seated, shown by nobody; the claim record owes nothing; the operation keeps its word.
    e.unfocus();
    {
        const std::optional<PanePresentation> claim = e.presentation_claim();
        REQUIRE(claim.has_value());
        CHECK_FALSE(claim->seated);
        CHECK(claim->shown_by == 0);
    }
    CHECK(e.r.bus.application_of(e.r.workshop_id) == loom::JointApplication::None);
    CHECK(e.r.bus.joint_status(op->op).application == loom::JointApplication::Declined);
    CHECK(op->applied.size() == 1);
    // ...AND THE OPERATOR RELEASES THE RECORD IT CONSUMED.
    nudge();
    CHECK(op->released.ok);
    CHECK(e.r.bus.joint_status(op->op).state == loom::JointState::Missing);
    // The ordinary door still works afterwards: a real open through the old door seats the pane.
    put_bytes(e.root / "a.cpp", "one\n");
    const SourceOpened said = e.ask_open_direct(spelled(e.root / "a.cpp"));
    CHECK_FALSE(said.accepted); // no manager is mounted in this rig: refused in words, not silence
    CHECK(said.refusal.find("could not be reached") != std::string::npos);
}

TEST_CASE("EDIT-W81: the real manager's outcome survives an unrelated coordination begun on the same bus before its application notice was consumed") {
    SUBCASE("begun before the owners are shown") {
        EditorRig e("edit-unrelated-before-showing");
        e.open();
        e.open_file("a.cpp", "one\n");
        put_bytes(e.root / "b.cpp", "two\n");
        const std::string b_path = spelled(e.root / "b.cpp");
        Unrelated other(e.r);
        const std::size_t before = e.asker->opens.size();
        const std::int64_t committed = e.opening().committed;
        e.enqueue_open(b_path);
        e.pump_until_stage("admit");
        (void)e.r.bus.pump_pending(); // the desk admits and offers; its answer is queued
        const std::int64_t op = e.opening().op;
        REQUIRE(op != 0);
        // THE UNRELATED BEGIN, queued behind that answer: the next turn commits the open and
        // then begins the unrelated operation -- before either owner has been shown.
        other.enqueue_begin(e.r);
        (void)e.r.bus.pump_pending();
        REQUIRE_MESSAGE(other.coordinator->begun.ok, loom::name_of(other.coordinator->begun.why));
        CHECK(e.opening().stage == "apply");
        CHECK(e.r.bus.joint_status(static_cast<std::uint64_t>(op)).state == loom::JointState::Committed);
        CHECK(e.r.bus.joint_status(static_cast<std::uint64_t>(op)).application ==
              loom::JointApplication::Pending);
        CHECK(e.r.bus.joint_records() == 2);
        // THE REST: the owners are shown and apply, the bus tells the manager, the manager
        // re-reads the record -- still there -- and answers the requester: opened.
        e.settle();
        REQUIRE(e.asker->opens.size() == before + 1);
        CHECK_MESSAGE(e.asker->opens.back().accepted, e.asker->opens.back().refusal);
        CHECK(e.opening().last_outcome == "committed");
        CHECK(e.opening().committed == committed + 1);
        CHECK(e.opening().stage == "idle");
        CHECK(e.read("path") == b_path);
        CHECK(e.doc_row(0) == "two");
        // CONSUMED AND RELEASED: the manager's record is Missing; the unrelated one untouched.
        CHECK(e.r.bus.joint_status(static_cast<std::uint64_t>(op)).state == loom::JointState::Missing);
        CHECK(e.r.bus.joint_status(other.coordinator->begun.op).state == loom::JointState::Preparing);
        CHECK(e.r.bus.joint_records() == 1);
    }
    SUBCASE("begun while the application notice is queued") {
        EditorRig e("edit-unrelated-notice-queued");
        e.open();
        e.open_file("a.cpp", "one\n");
        put_bytes(e.root / "b.cpp", "two\n");
        const std::string b_path = spelled(e.root / "b.cpp");
        Unrelated other(e.r);
        const std::size_t before = e.asker->opens.size();
        const std::int64_t committed = e.opening().committed;
        e.enqueue_open(b_path);
        e.pump_until_stage("apply"); // committed; the `apply` words to both owners are queued
        const std::int64_t op = e.opening().op;
        REQUIRE(op != 0);
        // THE UNRELATED BEGIN, queued behind the apply words: the next turn shows both owners
        // (the application settles and the manager's notice is queued), then begins the
        // unrelated operation -- before the notice is consumed.
        other.enqueue_begin(e.r);
        (void)e.r.bus.pump_pending();
        REQUIRE_MESSAGE(other.coordinator->begun.ok, loom::name_of(other.coordinator->begun.why));
        CHECK(e.r.bus.joint_status(static_cast<std::uint64_t>(op)).application ==
              loom::JointApplication::Applied);
        CHECK(e.opening().stage == "apply"); // settled at the bus, not yet consumed
        CHECK(e.r.bus.joint_records() == 2);
        e.settle();
        REQUIRE(e.asker->opens.size() == before + 1);
        CHECK_MESSAGE(e.asker->opens.back().accepted, e.asker->opens.back().refusal);
        CHECK(e.opening().last_outcome == "committed");
        CHECK(e.opening().committed == committed + 1);
        CHECK(e.read("path") == b_path);
        CHECK(e.r.bus.joint_status(static_cast<std::uint64_t>(op)).state == loom::JointState::Missing);
        CHECK(e.r.bus.joint_status(other.coordinator->begun.op).state == loom::JointState::Preparing);
        CHECK(e.r.bus.joint_records() == 1);
    }
}

TEST_CASE("EDIT-W79: the real Editor, held behind a publication its image could not apply, is reloaded into the normal image -- the successor keeps A, and the record says B was never applied") {
    // THE START REPRODUCTION. The image is the exact
    // real Editor source built once more with one deliberate throw at the start of its showing
    // hook, for a published path ending in `/b.cpp` and nothing else (`zengine-editor-throwing`,
    // tests/CMakeLists.txt). Opening a.cpp through it is an ordinary open; publishing b.cpp
    // reaches the real hook and fails there. The repair is a real reload through the real
    // control door into the UNCHANGED normal image -- the maker's corrected build -- whose
    // successor holds a.cpp, because the candidate deliberately does not ride a reload.
    EditorRig e("edit-real-repair");
    e.open(160, 48, /*pick_it=*/false, /*slow_skin=*/false, EditorRig::Project::kDoor,
           /*with_manager=*/true, "zengine-editor-throwing");
    REQUIRE_FALSE(e.r.session().panels.has(e.kind));
    const std::string a_path = e.open_file("a.cpp", "one\n");
    CHECK(e.read("text") == "one\n");
    put_bytes(e.root / "b.cpp", "two\n");
    const std::string b_path = spelled(e.root / "b.cpp");
    const std::size_t before = e.asker->opens.size();
    const std::int64_t a_epoch = std::stoll(e.read("doc_epoch"));
    e.enqueue_open(b_path);
    e.pump_until_stage("apply");
    const std::int64_t op = e.opening().op;
    REQUIRE(op != 0);
    e.settle();
    // PUBLISHED, NOT APPLIED: the real hook threw, contained at the seam; the answer is
    // truthful and names the owner; the owner is held.
    REQUIRE(e.asker->opens.size() == before + 1);
    CHECK_FALSE(e.asker->opens.back().accepted);
    CHECK(e.asker->opens.back().refusal.find("could not apply") != std::string::npos);
    CHECK(e.asker->opens.back().refusal.find(kEditorRole) != std::string::npos);
    CHECK(e.opening().last_outcome == "committed, application failed");
    CHECK(e.opening().unapplied == 1);
    CHECK(e.opening().committed == 1); // a.cpp
    {
        const loom::JointStatus status = e.r.bus.joint_status(static_cast<std::uint64_t>(op));
        CHECK(status.state == loom::JointState::Committed);
        CHECK(status.application == loom::JointApplication::Failed);
        CHECK(status.failed == e.image);
        CHECK(status.failed_role == kEditorRole);
    }
    CHECK(e.r.bus.has_failed_application(e.image));
    {
        const std::optional<EditorDocument> doc = e.document_claim();
        REQUIRE(doc.has_value());
        CHECK(doc->path == b_path); // the commitment stands
        CHECK(doc->opened_by == op);
    }
    // THE HOLD: a poke is refused by exact attempt; the diagnostic read shows a.cpp as it is.
    const loom::Ticket poke = e.r.bus.send(
        e.image, loom::Message(loom::to_value(loom::PokeRead{"path"}), loom::WeaveId{},
                               e.poke_id, ++e.poke_corr));
    e.settle();
    CHECK(e.r.bus.outcome(poke).disposition == loom::Disposition::Refused);
    CHECK(e.r.bus.outcome(poke).refusal.reason == loom::RefusalReason::ApplicationFailed);
    {
        const loom::Unverified claim = loom::parse(e.r.bus.snapshot_bytes(
            e.image, loom::Switchboard::SnapshotAccess::Diagnostic));
        const loom::Admission held = loom::admit(claim, loom::schema_of<pane::EditorPaneState>());
        REQUIRE(held.ok());
        CHECK(held.value().get("path")->as_text() == a_path);
        CHECK(held.value().get("text")->as_text() == "one\n");
        CHECK(e.r.bus.has_failed_application(e.image)); // the read changed nothing
    }
    // THE DESK APPLIED ITS OWN HALF, and says which owner is held.
    REQUIRE(e.r.session().panels.has(e.kind));
    CHECK(e.r.session().panels.keyboard == e.kind);
    CHECK(e.r.session().notice.find("could not apply") != std::string::npos);
    // THE REPAIR: the record loaded from the throwing image is reloaded in place from a copy
    // of the normal image, through the real control door. The manager RETAINS the record for
    // the late word about this repair.
    CHECK(e.opening().retained == op);
    e.enqueue_reload_into("zengine-editor-throwing", pane::kEditorPaneStem);
    // TURN BY TURN, until the successor has been shown: its first delivery (the activation
    // the control door announces) shows it B, which it did not prepare -- and it ANSWERS SO.
    for (int i = 0; i < 8 && e.r.bus.joint_status(static_cast<std::uint64_t>(op)).application !=
                                 loom::JointApplication::Declined;
         ++i) {
        (void)e.r.bus.pump_pending();
    }
    REQUIRE_MESSAGE(e.r.load_refusals.empty(), "the reload was refused: ", e.r.load_refusals.back());
    CHECK(e.r.kernel.weave_id("zengine-editor-throwing") == e.image);
    CHECK_FALSE(e.r.bus.has_failed_application(e.image));
    {
        // THE BUS'S RECORD: this publication was DECLINED by exactly this owner -- not applied,
        // not failed, nothing held -- and the record is still there for the manager to read.
        const loom::JointStatus status = e.r.bus.joint_status(static_cast<std::uint64_t>(op));
        CHECK(status.state == loom::JointState::Committed);
        CHECK(status.application == loom::JointApplication::Declined);
        CHECK(status.failed == e.image);
        CHECK(status.failed_role == kEditorRole);
    }
    CHECK(e.opening().retained == op); // the late word is not consumed yet
    CHECK(e.opening().last_outcome == "committed, application failed");
    // THE SUCCESSOR HOLDS A -- the candidate did not ride -- and ordinary access resumed on it.
    e.settle();
    CHECK(e.read("path") == a_path);
    CHECK(e.read("text") == "one\n");
    CHECK(std::stoll(e.read("doc_epoch")) > a_epoch); // the generation moved past the publication
    REQUIRE(e.row() != nullptr);
    e.kind = e.row()->kind;
    // ...SO THE HISTORICAL PUBLICATION OF B WAS NOT APPLIED, and every record says so: the
    // manager's outcome in words, its counts, and the bus's record -- consumed for the late
    // word, then released. A repaired owner is not proof that its old operation applied.
    CHECK(e.opening().last_outcome == std::string("committed, not applied after repair -- ") +
                                          kEditorRole + " kept its own state");
    CHECK(e.opening().last_refusal.find("did not apply") != std::string::npos);
    CHECK(e.opening().retained == 0);
    CHECK(e.r.bus.joint_status(static_cast<std::uint64_t>(op)).state == loom::JointState::Missing);
    CHECK(e.opening().committed == 1);
    CHECK(e.opening().unapplied == 1);
    CHECK(e.asker->opens.size() == before + 1); // the requester's earlier answer stands
    // THE EDITOR'S OWN CLAIM IS a.cpp AGAIN -- the owner spoke, and its claim record owes
    // nothing -- and the desk shows a.cpp.
    {
        const std::optional<EditorDocument> doc = e.document_claim();
        REQUIRE(doc.has_value());
        CHECK(doc->path == a_path);
    }
    CHECK(e.r.bus.application_of(e.image) == loom::JointApplication::None);
    CHECK(e.doc_row(0) == "one");
    // ...AND THE REPAIRED EDITOR OPENS b.cpp WHEN ASKED AGAIN: a new fact, distinct from the
    // historical publication that was not applied.
    const SourceOpened again = e.ask_open(b_path);
    CHECK_MESSAGE(again.accepted, again.refusal);
    CHECK(e.read("path") == b_path);
    CHECK(e.doc_row(0) == "two");
    CHECK(e.opening().committed == 2);
    CHECK(e.opening().last_outcome == "committed");
}

// ============================================================================
//
// NOT A TEST OF SPEED: no timing is asserted. What is asserted is only that each measured
// step did what it says; the numbers go to the log as MESSAGEs, to be read against the same
// case compiled on the baseline tree. Wall time per step, in microseconds, over the whole
// round trip a maker's gesture takes on this rig (publish, route, edit, compose, admit, paint).

namespace {

struct Stopwatch {
    std::chrono::steady_clock::time_point at = std::chrono::steady_clock::now();
    double lap_us() {
        const auto now = std::chrono::steady_clock::now();
        const double us = std::chrono::duration<double, std::micro>(now - at).count();
        at = now;
        return us;
    }
};

struct Stats {
    double min = 0, mean = 0, max = 0;
};
inline Stats stats_of(const std::vector<double>& v) {
    Stats s;
    if (v.empty()) {
        return s;
    }
    s.min = v[0];
    s.max = v[0];
    double sum = 0;
    for (const double x : v) {
        s.min = std::min(s.min, x);
        s.max = std::max(s.max, x);
        sum += x;
    }
    s.mean = sum / static_cast<double>(v.size());
    return s;
}

/// A source of about `bytes` bytes: many short lines, the way a real file is shaped (one
/// four-megabyte line would measure line arithmetic, not the mechanism).
inline std::string lines_of(std::size_t bytes) {
    static const char* kLine = "int value_of_the_line_here = 1234567;\n"; // 38 bytes
    std::string out;
    out.reserve(bytes + 64);
    while (out.size() + 38 <= bytes) {
        out += kLine;
    }
    return out;
}

inline void measure_document(const char* label, std::size_t bytes) {
    EditorRig e(label);
    e.open();
    put_bytes(e.root / "a.cpp", lines_of(bytes));
    put_bytes(e.root / "b.cpp", lines_of(bytes));
    const std::string a_path = spelled(e.root / "a.cpp");
    const std::string b_path = spelled(e.root / "b.cpp");
    Stopwatch w;
    const SourceOpened first = e.ask_open(a_path);
    const double open_us = w.lap_us();
    REQUIRE_MESSAGE(first.accepted, first.refusal);
    e.press_doc(0, 0);
    w.lap_us();
    std::vector<double> keys;
    for (int i = 0; i < 40; ++i) {
        e.type("x");
        keys.push_back(w.lap_us());
    }
    REQUIRE(e.read("text").rfind(std::string(40, 'x'), 0) == 0);
    std::vector<double> undos;
    for (int i = 0; i < 10; ++i) {
        e.key(input::scan::kZ, input::mod::kCtrl);
        undos.push_back(w.lap_us());
    }
    // A CARET MOVE: the cheapest gesture, for what the mirror does NOT rebuild.
    std::vector<double> moves;
    for (int i = 0; i < 10; ++i) {
        e.key(input::scan::kRight);
        moves.push_back(w.lap_us());
    }
    // THE OPEN PATH WITH A DOCUMENT ALREADY THERE (clean): B replaces A.
    e.key(input::scan::kZ, input::mod::kCtrl); // ...after taking the rest of the typing back
    for (int i = 0; i < 40; ++i) {
        if (e.clean()) {
            break;
        }
        e.key(input::scan::kZ, input::mod::kCtrl);
    }
    REQUIRE(e.clean());
    w.lap_us();
    const SourceOpened second = e.ask_open(b_path);
    const double reopen_us = w.lap_us();
    REQUIRE_MESSAGE(second.accepted, second.refusal);
    REQUIRE(e.read("path") == b_path);
    const Stats k = stats_of(keys);
    const Stats u = stats_of(undos);
    const Stats m = stats_of(moves);
    MESSAGE("MEASURE ", label, " bytes=", bytes, " open_us=", open_us, " reopen_us=", reopen_us,
            " key_us(min/mean/max)=", k.min, "/", k.mean, "/", k.max,
            " undo_us(min/mean/max)=", u.min, "/", u.mean, "/", u.max,
            " move_us(min/mean/max)=", m.min, "/", m.mean, "/", m.max);
}

} // namespace

TEST_CASE("EDIT-M1: measurement -- the edit and open paths on a small and a near-bound document (no timing assertions)") {
    measure_document("measure-small", 256);
    measure_document("measure-near-bound",
                     static_cast<std::size_t>(zengine::workshop::kMaxSourceBytes) - 4096);
}

TEST_CASE("EDIT-M2: measurement -- what the managed open retains and how many turns it takes (no timing assertions)") {
    // THE MANAGED OPENING'S OWN NUMBERS: the size of the two published identities, the bytes
    // the bus retains while an open is prepared, the number of latest claims the room holds,
    // and the number of bus turns from a requester's ask to the terminal answer.
    for (const std::size_t bytes : {std::size_t{256},
                                    static_cast<std::size_t>(zengine::workshop::kMaxSourceBytes) - 4096}) {
        EditorRig e(bytes < 1024 ? "measure-managed-small" : "measure-managed-near-bound");
        e.open();
        put_bytes(e.root / "a.cpp", lines_of(bytes));
        put_bytes(e.root / "b.cpp", lines_of(bytes));
        REQUIRE(e.ask_open(spelled(e.root / "a.cpp")).accepted);
        const std::size_t claims_idle = e.r.bus.retained_claim_count();
        const std::size_t before = e.asker->opens.size();
        e.enqueue_open(spelled(e.root / "b.cpp"));
        e.pump_until_stage("admit");
        (void)e.r.bus.pump_pending(); // the desk admits and offers: both offers stand
        const std::size_t retained = e.r.bus.joint_retained_bytes();
        int turns = 2; // the request and the turns pump_until_stage took are counted below
        turns = 0;
        Stopwatch w;
        while (e.asker->opens.size() == before) {
            REQUIRE(e.r.bus.pump_pending() > 0);
            ++turns;
        }
        const double commit_to_answer_us = w.lap_us();
        REQUIRE(e.asker->opens.back().accepted);
        const std::optional<EditorDocument> doc = e.document_claim();
        const std::optional<PanePresentation> desk = e.presentation_claim();
        REQUIRE(doc.has_value());
        REQUIRE(desk.has_value());
        const std::size_t doc_bytes = loom::serialize(loom::to_value(*doc)).size();
        const std::size_t desk_bytes = loom::serialize(loom::to_value(*desk)).size();
        MESSAGE("MEASURE managed bytes=", bytes, " document_claim_bytes=", doc_bytes,
                " presentation_claim_bytes=", desk_bytes,
                " joint_retained_bytes_while_prepared=", retained,
                " retained_claims_idle=", claims_idle,
                " retained_claims_after=", e.r.bus.retained_claim_count(),
                " joint_retained_bytes_after=", e.r.bus.joint_retained_bytes(),
                " turns_from_admission_to_answer=", turns,
                " admission_to_answer_us=", commit_to_answer_us);
        CHECK(e.r.bus.joint_retained_bytes() == 0);
    }
}
