// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Editor pane: a loadable weave that offers Workshop one pane and holds the one source
// document a weaver edits -- path, bytes, saved comparison, line convention, caret, selection,
// history and viewport (WL-EDIT-01). It is the one custodian of the buffer (`editor.hpp`) and
// the file; what crosses is a source request, a preparation, a quit answer and its picture. The
// host keeps room, focus, membership and the exit decision, which it asks (WL-EDIT-03, WL-EDIT-14).
// Workshop law: agents/workshop/editor.md

// Save is one synchronous call (the write, then the saved comparison), so a success can never
// mark a later edit clean. Paste, quit and open are round trips: a paste answer is pinned to the
// document it was asked for (WL-EDIT-11); a quit is answered about the instant (WL-EDIT-14); an
// open comes through the old door, relayed to the opening manager with the requester's answer
// right kept, or the managed door, where a candidate built beside the document becomes it only
// when the bus publishes this weave's claim (WL-EDIT-05).

// This file is the weave: its lifecycle and revival, the room, the keys, the clipboard and the
// exit, save and discard, and the rows, caret and mirror it composes. It declares every member;
// `transfers.hpp` defines the pointer's and the transfers' (WL-EDIT-16..21) and
// `opening_switch.hpp` the open doors' and a switch's, both included after the class, over the
// records and pure helpers of `common.hpp`.

#include "editor/editor-pane/common.hpp"
#include "editor/editor-pane/vocabulary.hpp"

#include "editor/editor-pane/editor.hpp"
#include "workshop/editor_handoff_vocabulary.hpp"
#include "workshop/open_seam_vocabulary.hpp"
#include "workshop/pane_canvas_rows.hpp"
#include "workshop/pane_carry.hpp"
#include "workshop/pane_causes.hpp"
#include "workshop/pane_document.hpp"
#include "workshop/pane_menu.hpp"
#include "workshop/pane_operation.hpp"
#include "workshop/pane_seam_vocabulary.hpp"
#include "workshop/pane_text.hpp"
#include "workshop/pane_vocabulary.hpp"
#include "workshop/persist.hpp"

#include "activation/activation.hpp"
#include "component/text_box.hpp"
#include "input/vocabulary.hpp"
#include "surface/vocabulary.hpp"

#include "manuals/zengine.editor.hpp" // the Editor's manual, compiled in from its page

#include <zen/kernel/export.hpp>
#include <zen/weave.hpp>
#include <zen/weave/lifecycle.hpp>
#include <zen/weave/standard_shapes.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

namespace component = zengine::component;
namespace input = zengine::input;

// =============================================================================

class EditorPaneWeave
    : public zengine::With<ws::PaneManualFacet<
          pane::kEditorPane, zengine::manual::pages::zengine_editor::kManual>>::Weave<
          EditorPaneWeave, pane::EditorPaneState,
          loom::Accept<loom::Activated, PaneCatalogRequested, PaneRoom, ws::PaneCanvasRoom,
                       ws::PaneCanvasPointer, ws::PaneCanvasRejected, PaneKey, PaneTextInput,
                       PaneActionRequested, PaneQuitRequested, OpenSourceRequested,
                       PrepareSourceRequested, ws::v2::PrepareSourceRequested,
                       ManagedOpenProgress, ManagedOpenSettled,
                       SourceOpened, loom::DispatchRefused, ProjectRoot, surface::ClipboardCopy,
                       surface::ClipboardText, EditorHandoffJudgeRequested, EditorWarmRequested,
                       EditorPreparationTick, EditorHandoffRequested, EditorHandoffEnded,
                       EditorAdoptRequested, EditorLiveRequested, EditorRetireRequested,
                       ws::PaneCanvasValueDrop, ws::PaneMenuAnswered, ws::PaneOperationAnswered,
                       ws::PaneCarryAnswered>,
          loom::Emit<PaneOffered, ws::v3::PaneActions, ws::v5::PaneCanvasContent,
                     ws::v4::PaneContent, ws::v2::PaneCaret,
                     ws::PaneEscapeUnspent, // declared so Escape reaches it, which it keeps
                     PaneQuitAnswered, SourceOpened, SourcePrepared, ws::v2::SourcePrepared,
                     OpenSourceRequested,
                     ProjectRootRequested, surface::ClipboardCopy,
                     surface::ClipboardTextRequested, EditorHandoffJudged, EditorWarmed,
                     EditorHandoffOffered, EditorAdopted, EditorLive, EditorRetired,
                     ws::PaneMenuRequested, ws::PanePassRequested, ws::PaneOperationRequested,
                     ws::PaneValueCarryRequested>,
          loom::WeaveBase, loom::Claims<EditorDocument>> {
public:
    using Base = Weave;

    // ---- The state a reload carries, and the surface a poke reads ----------------------

    /// No `snapshot()` override: `state_` is written from the live document at the end of every
    /// delivery (`mirror_state`) and in the publication hook, so Loom's own snapshot and the
    /// `zen.PokeRead` doors, answered off `state_`, read what the weaver sees (WL-EDIT-15). A
    /// reload revives the document here, before the first delivery and before the activation in
    /// which the pane re-offers itself. The bytes meet the file's law: a snapshot `source_in`
    /// refuses leaves no document rather than one that cannot be edited truthfully.
    void revive(const loom::Value& v) override {
        Base::revive(v);
        restore_from_state();
    }

    // ---- Offering and the room -----------------------------------------------------------

    void on(const loom::Activated& a, loom::Mail& mail) {
        if (!activation_.accept(mail, a)) {
            return;
        }
        // A SWITCH'S SUCCESSOR IS ACTIVATED BY ITS ADMISSION, holding the document it adopted
        // while sealed: it is the Editor from this delivery on, so it offers the pane, and the
        // desk re-grants the room it seats (WL-SWITCH-05).
        if (adopted_) {
            notice((e_.dirty() ? "switched editors -- UNSAVED edits stand in " : "switched editors -- editing ") +
                       (e_.open_document() ? shown_path() : std::string("no source")),
                   false);
        }
        announce(mail);
        ask_project_root(mail);
    }

    void on(const PaneCatalogRequested&, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole)) {
            return;
        }
        announce(mail);
    }

    /// THE PROSE ROOM: the rows and columns this pane composes for where its host grants it no
    /// canvas. Once a canvas room is granted the canvas decides, and this room is not spent.
    void on(const PaneRoom& room, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || room.pane != pane::kEditorPane) {
            return;
        }
        if (canvas_.grant > 0) {
            return; // the canvas room, granted first, already drew
        }
        rows_ = room.rows;
        columns_ = room.columns;
        granted_ = true;
        if (!root_asked_) {
            ask_project_root(mail); // a host whose door was not there at activation
        }
        say(mail);
    }

    /// THE CANVAS ROOM: this pane draws its rows there as its own picture, the lattice's rows and
    /// columns its room. A new grant ends the sweep its predecessor held; a room with no extent
    /// draws nothing and moves nothing, so the view a hidden pane was left at is the view it
    /// shows again.
    void on(const ws::PaneCanvasRoom& room, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || room.pane != pane::kEditorPane) {
            return;
        }
        if (room.grant != canvas_.grant) {
            held_ = 0;
            drag_ = Drag{};
            if (!grab_.started) {
                end_grab();
            }
        }
        canvas_ = room;
        if (!on_canvas()) {
            return;
        }
        const ws::CanvasRows lattice = ws::canvas_rows(canvas_);
        rows_ = lattice.rows;
        columns_ = lattice.columns;
        granted_ = true;
        if (!root_asked_) {
            ask_project_root(mail);
        }
        say(mail);
    }

    /// A refused picture leaves the last good one showing, and the next saying draws again.
    void on(const ws::PaneCanvasRejected&, loom::Mail&) {}

    /// WHERE THIS RUN BEGAN, asked of `zengine.project` -- the one host fact a relative
    /// spelling needs, and the door Files asks for the same reason (WL-FILES-02).
    void on(const ProjectRoot& said, loom::Mail& mail) {
        if (!mail.answers_ask() || mail.correlation() != root_pending_) {
            return; // not Loom's answer to the question this pane asked
        }
        project_dir_ = said.project_dir;
        // An owner that has not answered and one that authoritatively named no project are
        // different facts, and only the second permits spelling a relative path against the
        // process (WL-EDIT-06).
        project_known_ = true;
    }

    // ---- THE TWO DOORS, defined in `opening_switch.hpp` -------------------------------------

    void on(const OpenSourceRequested& asked, loom::Mail& mail);
    void on(const SourceOpened& said, loom::Mail& mail);
    void on(const loom::DispatchRefused& refused, loom::Mail& mail);
    void on(const ManagedOpenProgress& said, loom::Mail& mail);
    void on(const PrepareSourceRequested& asked, loom::Mail& mail);
    void on(const ws::v2::PrepareSourceRequested& asked, loom::Mail& mail);
    Preparation prepare(std::int64_t asked_op, const std::string& asked_path, std::int64_t rows,
                        std::int64_t columns, const ws::PaneCanvasRoom& room, loom::Mail& mail);
    void on(const ManagedOpenSettled& said, loom::Mail& mail);
    bool on_claim_published(const EditorDocument& published);

    /// THE END OF EVERY DELIVERY (WL-OPEN-03; Loom's `after_delivery`): say what a
    /// publication owes, mirror the live document into the read surface, and claim the
    /// document's identity if it moved. One place, mechanically, so no handler can forget
    /// and no read can find the mirror stale.
    void after_delivery(loom::Mail& mail) {
        mirror_state();
        // A SEALED CANDIDATE HOLDS NO OFFICE AND IS NOBODY'S PANE YET: it may speak only to its
        // coordinator, so it says no rows and claims nothing until its admission activates it.
        if (!activation_.activated()) {
            return;
        }
        if (resay_) {
            resay_ = false;
            say(mail);
        }
        claim_document(mail);
    }

    // ---- A switch, as the incumbent and as the sealed candidate: `opening_switch.hpp` -------

    void on(const EditorHandoffJudgeRequested& asked, loom::Mail& mail);
    void on(const EditorHandoffRequested& asked, loom::Mail& mail);
    void on(const EditorHandoffEnded& ended, loom::Mail& mail);
    void on(const EditorRetireRequested& asked, loom::Mail& mail);
    void on(const EditorWarmRequested& asked, loom::Mail& mail);
    void on(const EditorPreparationTick&, loom::Mail&);
    void on(const EditorAdoptRequested& asked, loom::Mail& mail);
    void on(const EditorLiveRequested& asked, loom::Mail& mail);

    // ---- The pointer, defined in `transfers.hpp` --------------------------------------------

    void on(const ws::PaneCanvasPointer& event, loom::Mail& mail);
    void press_at(std::int64_t prow, std::int64_t pcol, bool current, loom::Mail& mail);
    void dragged(std::int64_t drow, std::int64_t dcol, loom::Mail& mail);
    void wheel(double dy, loom::Mail& mail);

    // ---- The keys ------------------------------------------------------------------------

    /// THE BUFFER'S OWN VOCABULARY: caret movement, selection, clipboard, word erases,
    /// history -- through the one `consume` every consumer of it makes (WL-EDIT-02). A
    /// gesture this pane declared a row for never reaches here; it arrives as
    /// `PaneActionRequested` instead.
    void on(const PaneKey& key, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || key.pane != pane::kEditorPane) {
            return;
        }
        if (held_still()) {
            return;
        }
        if (!e_.open_document()) {
            return; // an empty editor has no document for a key to mean anything to
        }
        end_grab();
        const std::uint64_t copied_before = clip_.writes;
        const std::uint64_t pastes_before = clip_.paste_requests;
        if (!e_.buffer.consume(key.scancode, key.modifiers, clip_)) {
            return;
        }
        notice_.clear();
        e_.follow_caret = true;
        if (clip_.writes != copied_before) {
            // A COPY IS SAID TO THE PROCESS ONCE: the Skin offers it to the platform's
            // clipboard and every other text-holding participant mirrors it.
            mail.publish(surface::ClipboardCopy{clip_.text});
        }
        if (clip_.paste_requests != pastes_before) {
            begin_paste(mail);
        }
        say(mail);
    }

    /// TEXT THE WEAVER TYPED INTO THE SOURCE, gated by the source-byte law at this door
    /// (WL-EDIT-07): a chunk with one byte outside plain ASCII is refused whole, with a
    /// sentence, and the keystroke costs nothing.
    void on(const PaneTextInput& typed, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || typed.pane != pane::kEditorPane) {
            return;
        }
        if (held_still()) {
            return;
        }
        if (!e_.open_document() || typed.text.empty()) {
            return;
        }
        end_grab();
        if (!ws::source_text_ok(typed.text)) {
            notice("nothing was inserted -- that text holds bytes outside plain ASCII, "
                   "which this editor cannot carry truthfully",
                   true);
            say(mail);
            return;
        }
        notice_.clear();
        e_.buffer.type(typed.text);
        e_.follow_caret = true;
        say(mail);
    }

    /// The rows this pane declared, acted on by name (WL-KEY-15): save, newline, tab, discard and
    /// the two carries. A weaver's override moved the key; the id is what arrives.
    void on(const PaneActionRequested& asked, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || asked.pane != pane::kEditorPane) {
            return;
        }
        if (held_still()) {
            return;
        }
        end_grab();
        if (asked.id == pane::kActionExtract || asked.id == pane::kActionLocation) {
            // THE KEYBOARD ROUTE (TUI's, and anyone's): pick-and-place through the same
            // acquisition the highlight's drag and the right-click menu reach (WL-EDIT-19).
            acquire(asked.id == pane::kActionExtract ? Take::Selection : Take::Location, false,
                    mail.correlation(), mark_now(), mail);
            say(mail);
            return;
        }
        if (asked.id == pane::kActionSave) {
            save_source();
        } else if (asked.id == pane::kActionNewline) {
            if (e_.open_document()) {
                notice_.clear();
                e_.buffer.newline();
                e_.follow_caret = true;
            }
        } else if (asked.id == pane::kActionTab) {
            // A TAB BYTE, PRESERVED AS ONE -- the byte policy's insertion half. It arrives
            // as a key rather than as text because no backend delivers a control byte as
            // entered text (input's own law).
            if (e_.open_document()) {
                notice_.clear();
                e_.buffer.type("\t");
                e_.follow_caret = true;
            }
        } else if (asked.id == pane::kActionDiscard) {
            discard_source_edits();
        }
        say(mail);
    }

    // ---- Transfers, defined in `transfers.hpp` ----------------------------------------------

    void right_press(const ws::RowCell& at, bool current, loom::Mail& mail);
    void on(const ws::PaneCanvasValueDrop& drop, loom::Mail& mail);
    void on(const ws::PaneMenuAnswered& answer, loom::Mail& mail);
    void on(const ws::PaneOperationAnswered& answer, loom::Mail& mail);
    void on(const ws::PaneCarryAnswered& answer, loom::Mail& mail);

    // ---- The exit ------------------------------------------------------------------------

    /// May the Workshop end? Answered about this instant (WL-EDIT-14): dirty source refuses,
    /// naming the two ways out; a paste still arriving or an opening still being arranged refuses
    /// too, since either could replace or dirty the document after the answer; a clean document,
    /// or none, permits. The host holds every gesture while it waits, so "clean" is a fact.
    void on(const PaneQuitRequested&, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole)) {
            return;
        }
        if (holding_.active) {
            (void)mail.answer(PaneQuitAnswered{pane::kEditorPane, false,
                                               "the Editor is being switched -- quit again once the "
                                               "switch has settled"});
            return;
        }
        if (candidate_.live) {
            (void)mail.answer(PaneQuitAnswered{pane::kEditorPane, false,
                                               "the Editor is still opening " + candidate_.path +
                                                   " -- quit again once it has settled"});
            return;
        }
        if (paste_.awaiting) {
            (void)mail.answer(PaneQuitAnswered{pane::kEditorPane, false, kPasteInFlight});
            return;
        }
        if (e_.dirty()) {
            (void)mail.answer(PaneQuitAnswered{
                pane::kEditorPane, false,
                "the Editor holds unsaved changes to " + e_.path +
                    " -- save source or discard source edits in the Editor first; Workshop "
                    "stays open"});
            return;
        }
        (void)mail.answer(PaneQuitAnswered{pane::kEditorPane, true, std::string()});
    }

    // ---- The clipboard -------------------------------------------------------------------

    void on(const surface::ClipboardCopy& said, loom::Mail&) {
        // WHAT THE PROCESS SAYS IT COPIED -- this pane's own copies included, which is why
        // it is a mirror rather than a second store: a paste answers with this.
        clip_.text = said.text;
    }

    /// The Skin's answer to a paste this pane asked for (WL-EDIT-11, WL-TEXT-09). The correlation
    /// says which ask, not that the document still stands where it asked: a replaced or closed
    /// document strands the payload silently, and one that moved gets a sentence instead of a
    /// paste. The outstanding paste is cleared the moment its authenticated answer is consumed,
    /// before the payload is judged, so a stale answer leaves no paste in flight behind it.
    void on(const surface::ClipboardText& a, loom::Mail& mail) {
        if (!mail.answers_ask() || !paste_.awaiting || mail.correlation() != paste_.pending) {
            return;
        }
        paste_.awaiting = false;
        if (!e_.open_document() || e_.doc_epoch != paste_.doc) {
            return; // the document that asked is gone; discarded, silently
        }
        if (e_.buffer.revision() != paste_.revision) {
            notice("nothing was pasted -- the answer arrived after the source moved; paste "
                   "again",
                   true);
            say(mail);
            return;
        }
        if (a.readable) {
            clip_.text = a.text; // the platform's current truth, asked for by THIS paste
        }
        if (clip_.text.empty()) {
            say(mail);
            return; // an empty clipboard pastes nothing, the component's own law
        }
        const ws::PasteableSource judged = ws::pasteable_source(clip_.text);
        if (!judged.representable) {
            notice("nothing was pasted -- the clipboard holds bytes outside plain ASCII, "
                   "which this editor cannot carry truthfully",
                   true);
            say(mail);
            return;
        }
        notice_.clear();
        e_.buffer.paste_lines(judged.lines);
        e_.follow_caret = true;
        say(mail);
    }

private:
    // ---- The switch's helpers, defined in `opening_switch.hpp` ------------------------------

    bool held_still();
    std::string handoff_refusal() const;
    std::vector<std::string> handoff_losses() const;
    EditorTransfer transfer_now() const;

    // ---- Transfer helpers, defined in `transfers.hpp` ---------------------------------------

    Mark mark_now() const;
    bool on_highlight(EditorPos at) const;
    void end_grab();
    void offer(Take what, std::int64_t row, std::int64_t column, loom::Mail& mail);
    std::string where_words(EditorPos p) const;
    std::string project_root() const;
    void acquire(Take what, bool drag, std::uint64_t gesture, const Mark& at, loom::Mail& mail);
    std::string insertion_refusal() const;
    Landing landing(std::int64_t prow, std::int64_t pcol) const;
    void receive(const loom::Bytes& data, const ws::RowCell& drop, bool current, loom::Mail& mail);
    bool insert_lines(const std::string& text, const Landing& at, const std::string& note,
                      bool select);
    void insert_command(const st::Material& m, const Landing& at);
    void choose_drop(const Dropped& d, const std::string& chosen);
    void insert_cpp(const st::Material& m, const Landing& at);
    void open_location(const st::Material& m, loom::Mail& mail);
    void settle_location(const SourceOpened& said);

    // ---- Offering and declaring ---------------------------------------------------------

    void announce(loom::Mail& mail) {
        (void)mail.as_role(pane::kEditorPaneRole)
            .send_to_role(kWorkshopRole, PaneOffered{pane::kEditorPane, pane::kEditorPaneName,
                                                     pane::kEditorPaneSummary, 20, 84});
        declare(mail);
    }

    /// What this pane answers to: the rows below, which never change. Everything else a weaver
    /// presses reaches the buffer as an ordinary `PaneKey`, so Backspace erases and ctrl+z undoes
    /// without being anybody's row.
    void declare(loom::Mail& mail) {
        // Each row says what it stands in for among Workshop's own actions, and what its press may
        // send beside the pane protocol: a carry, once Workshop allows the acquisition.
        ws::v3::PaneActions actions;
        actions.pane = pane::kEditorPane;
        const auto row = [&actions](const char* id, const char* label, std::int64_t sc,
                                    std::int64_t mods, const char* stands_for = "",
                                    std::vector<ws::ActionCause> causes = {}) {
            actions.rows.push_back(
                ws::v3::PaneActionRow{id, label, sc, mods, stands_for, std::move(causes)});
        };
        const std::vector<ws::ActionCause> carried{ws::cause_of<ws::PaneValueCarryRequested>()};
        // The save row still names `document.save` as what it stands in for (WL-KEY-15): a host
        // after that row retired admits a name standing in for nothing, and a host from before
        // still declares it, where the name keeps `ctrl+s` this pane's -- one image loads in both.
        row(pane::kActionSave, "save source", input::scan::kS, input::mod::kCtrl,
            ws::kOwnableDocumentSave);
        row(pane::kActionNewline, "newline", input::scan::kReturn, input::mod::kNone);
        row(pane::kActionTab, "insert tab", input::scan::kTab, input::mod::kNone);
        row(pane::kActionDiscard, "discard source edits", input::scan::kD, input::mod::kCtrl);
        // Plain ctrl+letters, which every medium can say (a POSIX terminal cannot say alt or
        // ctrl+shift on a letter, and no terminal backend names a function key).
        row(pane::kActionExtract, "carry the selection", input::scan::kE, input::mod::kCtrl, "",
            carried);
        row(pane::kActionLocation, "carry this file's location", input::scan::kL, input::mod::kCtrl,
            "", carried);
        (void)mail.as_role(pane::kEditorPaneRole).send_to_role(kWorkshopRole, actions);
    }

    void ask_project_root(loom::Mail& mail) {
        root_asked_ = true;
        root_pending_ = ++asked_;
        (void)mail.as_role(pane::kEditorPaneRole)
            .send_to_role(ws::kProjectRole, ProjectRootRequested{}, root_pending_);
    }

    // ---- The document doors --------------------------------------------------------------

    // `resolve`, `judge_source` and `activate` are defined in `opening_switch.hpp`.
    Written resolve(const std::string& requested, std::string& out) const;
    Plan judge_source(const std::string& requested);
    void activate();

    /// WRITE THE SOURCE TO ITS FILE -- the editor's save authority (WL-EDIT-01). Atomic
    /// through the family's safe writer; the saved comparison moves only after the write
    /// succeeded, in the same call, so a success can never be about bytes that were not
    /// written.
    void save_source() {
        if (!e_.open_document()) {
            notice("no source is open -- nothing was saved", true);
            return;
        }
        const Written written = ws::persist::write_file(
            e_.path, ws::source_text(e_.buffer.lines(), e_.convention));
        if (!written.accepted) {
            notice(written.refusal, true);
            return;
        }
        e_.saved_lines = e_.buffer.lines();
        ++saved_stamp_;
        notice("saved -- " + shown_path(), false);
    }

    /// THE ONE DELIBERATE DISCARD DOOR (WL-EDIT-03): the buffer back to the last saved state,
    /// as one ordinary structural edit, so one undo takes a slip back.
    void discard_source_edits() {
        if (!e_.open_document()) {
            notice("no source is open -- nothing to discard", true);
            return;
        }
        if (!e_.dirty()) {
            notice("the source matches its last saved state -- nothing to discard", false);
            return;
        }
        e_.buffer.revert_to(e_.saved_lines);
        e_.follow_caret = true;
        notice("discarded unsaved edits; undo takes them back -- " + shown_path() +
                   " is back to its last saved state",
               false);
    }

    /// The paste this document asks for, pinned to the document epoch and the buffer revision at
    /// the moment of the ask (WL-EDIT-11).
    void begin_paste(loom::Mail& mail) {
        paste_.pending = ++asked_;
        paste_.doc = e_.doc_epoch;
        paste_.revision = e_.buffer.revision();
        paste_.awaiting = true;
        (void)mail.as_role(pane::kEditorPaneRole)
            .send_to_role(surface::kSkinRole, surface::ClipboardTextRequested{},
                          paste_.pending);
    }

    /// The document a snapshot carried, put back (see `revive`). `restore_selection` clamps a pair
    /// that outran the bytes, and the next reconcile clamps the viewport. No paste and no
    /// candidate come back: both were the old incarnation's conversations.
    void restore_from_state() {
        // WHERE THIS RUN BEGAN, AND THE LAST THING THIS PANE SAID: both are the picture the
        // weaver was looking at, and both come back before the document does, because the
        // notice is a ROW and the room the document gets is what is left under it.
        project_dir_ = state_.project_dir;
        project_known_ = state_.project_known;
        notice_ = state_.notice;
        notice_bad_ = state_.notice_bad;
        opened_by_ = static_cast<std::uint64_t>(state_.opened_by < 0 ? 0 : state_.opened_by);
        candidate_ = Candidate{};
        paste_ = Paste{};
        relays_.clear(); // the old incarnation's conversations; their rights died with it
        if (state_.path.empty()) {
            e_ = EditorState{};
            return;
        }
        ws::SourceIn text = ws::source_in(state_.text);
        ws::SourceIn saved = ws::source_in(state_.saved_text);
        if (!text.outcome.accepted || !saved.outcome.accepted) {
            e_ = EditorState{};
            notice("the reload carried a document this editor cannot edit truthfully -- "
                   "no source is open",
                   true);
            return;
        }
        e_.path = state_.path;
        e_.saved_lines = std::move(saved.lines);
        ++saved_stamp_;
        e_.buffer.set_lines(std::move(text.lines));
        // THE UNDO HISTORY THE SNAPSHOT CARRIED, under the document it was taken from.
        std::vector<EditorBuffer::HistoryStep> history;
        history.reserve(state_.undo.size());
        for (const pane::EditorUndoStep& step : state_.undo) {
            history.push_back(EditorBuffer::HistoryStep{
                split_lines(step.text), EditorPos{as_index(step.caret_row), as_index(step.caret_byte)},
                EditorPos{as_index(step.anchor_row), as_index(step.anchor_byte)}});
        }
        e_.buffer.restore_history(std::move(history));
        e_.convention = state_.convention;
        e_.doc_epoch = static_cast<std::uint64_t>(state_.doc_epoch);
        e_.buffer.restore_selection(as_index(state_.anchor_row), as_index(state_.anchor_byte),
                                    as_index(state_.caret_row), as_index(state_.caret_byte));
        e_.first_row = as_index(state_.first_row);
        e_.first_col = state_.first_col < 0 ? 0 : state_.first_col;
        e_.wheel_accum = 0.0;
        e_.follow_caret = false;
        // The room the document was last looked at through, carried (WL-EDIT-15): zeroed, the
        // first grant after a revival would read as a resize and pull a scrolled view back to
        // the caret. A genuinely different room still resizes.
        e_.last_rows = state_.last_rows;
        e_.last_cols = state_.last_cols;
    }

    void notice(std::string text, bool bad) {
        notice_ = std::move(text);
        notice_bad_ = bad;
    }

    // ---- The document's latest claim (WL-OPEN-03) ------------------------------------------

    /// THE DOCUMENT'S IDENTITY AS THIS WEAVE CLAIMS IT: the durable facts a commitment is
    /// about, and the ones any edit moves. `dirty` is recomputed only when the bytes or the
    /// saved comparison moved since the last claim, because comparing two four-megabyte
    /// documents at the end of every delivery is not a cost a caret key should pay.
    EditorDocument identity_now() {
        EditorDocument d;
        d.path = e_.path;
        d.doc_epoch = static_cast<std::int64_t>(e_.doc_epoch);
        d.convention = e_.convention;
        d.content_revision = static_cast<std::int64_t>(e_.buffer.content_revision());
        d.opened_by = static_cast<std::int64_t>(opened_by_);
        if (!dirty_known_ || dirty_content_ != e_.buffer.content_revision() ||
            dirty_saved_ != saved_stamp_) {
            dirty_cached_ = e_.dirty();
            dirty_content_ = e_.buffer.content_revision();
            dirty_saved_ = saved_stamp_;
            dirty_known_ = true;
        }
        d.dirty = dirty_cached_;
        return d;
    }

    /// CLAIM THE DOCUMENT'S IDENTITY IF IT MOVED. The claim is what a managed opening binds
    /// and what its commitment exchanges; claiming a moved identity is what aborts an
    /// operation that offered against the previous one. Unchanged identity, no claim.
    void claim_document(loom::Mail& mail) {
        const EditorDocument now = identity_now();
        if (claimed_ever_ && same_identity(now, claimed_)) {
            return;
        }
        const loom::SenseClaimResult claimed = mail.claim(now);
        if (claimed.accepted) {
            claimed_ = now;
            claimed_ever_ = true;
        }
    }

    // ---- The viewport ----------------------------------------------------------------------

    /// Keep the viewport true against the room and the document it has now (WL-EDIT-09): clamp
    /// the offsets always, and follow the caret when a gesture asked or the granted room changed
    /// -- not the document's rows, which a notice moves -- and never after the wheel.
    static void reconcile(const EditorState& doc, Viewport& v, std::int64_t granted_rows,
                          std::int64_t granted_cols, std::int64_t rows_in,
                          std::int64_t text_cols) {
        const bool resized = granted_rows != v.last_rows || granted_cols != v.last_cols;
        v.last_rows = granted_rows;
        v.last_cols = granted_cols;
        const std::size_t rows = static_cast<std::size_t>(rows_in > 0 ? rows_in : 0);
        const std::size_t total = doc.buffer.line_count();
        const std::size_t furthest_row = total > rows ? total - rows : 0;
        if (v.first_row > furthest_row) {
            v.first_row = furthest_row;
        }
        if (v.first_col < 0) {
            v.first_col = 0;
        }
        if (!v.follow_caret && !resized) {
            return;
        }
        v.follow_caret = false;
        if (rows == 0) {
            return; // no row to follow into: the offsets keep their answer for a room to come
        }
        const std::size_t cr = doc.buffer.caret_row();
        if (cr < v.first_row) {
            v.first_row = cr;
        }
        if (cr >= v.first_row + rows) {
            v.first_row = cr + 1 - rows;
        }
        if (text_cols <= 0) {
            return;
        }
        const std::string& line = doc.buffer.line(cr);
        const std::int64_t vis = ws::visual_col_of(line, doc.buffer.caret_byte());
        // The horizontal half, measured on the caret's own line: no blank room at the right while
        // text is hidden at the left, so erasing a long line recovers the room it freed.
        const std::int64_t need = ws::visual_len(line) + kCaretCols;
        const std::int64_t furthest_col = need > text_cols ? need - text_cols : 0;
        if (v.first_col > furthest_col) {
            v.first_col = furthest_col;
        }
        if (vis < v.first_col) {
            v.first_col = vis;
        }
        if (vis - v.first_col > text_cols) {
            v.first_col = vis - text_cols;
        }
    }

    // ---- The rows, and the caret beside them ---------------------------------------------

    /// The status row: the dirty word, then `L:C/N`, then the path, in the order the facts must
    /// survive a narrow room (WL-EDIT-12). The path is cut from its head, since its end says which
    /// file this is; the pane's name is the host's header, one row above.
    static std::string status_text(const EditorState& doc, std::int64_t columns) {
        if (!doc.open_document()) {
            return "no source open -- Return on a file in Files, or e in the Builder";
        }
        std::string head = doc.dirty() ? "UNSAVED" : "saved";
        head += " L" + std::to_string(doc.buffer.caret_row() + 1) + ":C" +
                std::to_string(ws::visual_col_of(doc.buffer.line(doc.buffer.caret_row()),
                                                 doc.buffer.caret_byte()) +
                               1);
        head += "/" + std::to_string(doc.buffer.line_count());
        head += " -- ";
        return head + tail_of_path(doc.path, columns - static_cast<std::int64_t>(head.size()));
    }

    /// THE OPEN PATH AS A NOTICE SAYS IT: whole where the room holds it, its end otherwise.
    /// A notice is one row of this pane's room, and every sentence above puts its fact
    /// before the path so the fact survives whatever the path costs.
    std::string shown_path() const {
        return tail_of_path(e_.path, columns_ > 24 ? columns_ - 24 : columns_);
    }

    /// Compose a document for a room: the status row, a standing notice where the room holds one,
    /// then the document through the viewport, with the caret and selection beside the rows on
    /// the lattice a press names (WL-EDIT-12, WL-CARET-01). Pure over the document and viewport it
    /// is handed, so it serves the live document and a candidate alike. The status row comes
    /// first; a notice gets its own row only where a document row survives under it, and
    /// otherwise stands in for the status row, so the document keeps its rows.
    static Composition compose(const EditorState& doc, Viewport view, std::int64_t rows,
                               std::int64_t columns, const std::string& note, bool bad) {
        Composition out;
        const auto push = [&out, columns](std::string text, std::int64_t role) {
            out.rows.push_back(surface::SurfaceTextRow{drawable(fit(std::move(text), columns)), role});
        };
        if (rows >= 1) {
            const bool notice_row = !note.empty() && rows >= kNoticeNeedsRows;
            if (!note.empty() && !notice_row) {
                push(note, bad ? surface::role::kAlert : surface::role::kMuted);
            } else {
                push(status_text(doc, columns), surface::role::kAccent);
                out.status_row = true;
            }
            if (notice_row) {
                push(note, bad ? surface::role::kAlert : surface::role::kMuted);
            }
        }
        out.chrome_rows = static_cast<std::int64_t>(out.rows.size());
        out.doc_rows = rows > out.chrome_rows ? rows - out.chrome_rows : 0;
        const std::int64_t text_cols = columns - kCaretCols > 0 ? columns - kCaretCols : 0;
        std::size_t last = 0;
        if (doc.open_document()) {
            reconcile(doc, view, rows, columns, out.doc_rows, text_cols);
            const std::size_t total = doc.buffer.line_count();
            const std::size_t shown = static_cast<std::size_t>(out.doc_rows);
            last = view.first_row + shown < total ? view.first_row + shown : total;
            for (std::size_t r = view.first_row; r < last; ++r) {
                out.rows.push_back(surface::SurfaceTextRow{
                    ws::expanded_slice(doc.buffer.line(r), view.first_col, text_cols),
                    surface::role::kFill});
            }
        }
        out.view = view;
        out.caret = caret_of(doc, view, out.chrome_rows, out.doc_rows, last, text_cols);
        return out;
    }

    /// Where the caret is and what is selected, beside the rows in the body lattice
    /// (WL-CARET-01). The caret is said only while its row is in the window; the selection is
    /// clamped into the window, and a range past the last shown row ends at `(rows, 0)`
    /// (WL-CARET-03). A selection may stand with no caret (`kNoCaret`).
    static ws::v2::PaneCaret caret_of(const EditorState& doc, const Viewport& view,
                                      std::int64_t chrome_rows, std::int64_t doc_rows,
                                      std::size_t last, std::int64_t text_cols) {
        ws::v2::PaneCaret caret;
        caret.pane = pane::kEditorPane;
        caret.generation = static_cast<std::int64_t>(doc.doc_epoch);
        if (doc.open_document() && doc_rows > 0) {
            const std::size_t cr = doc.buffer.caret_row();
            if (cr >= view.first_row && cr < last) {
                const std::int64_t vis =
                    ws::visual_col_of(doc.buffer.line(cr), doc.buffer.caret_byte());
                const std::int64_t col = vis - view.first_col;
                if (col >= 0 && col <= text_cols) {
                    caret.row = chrome_rows + static_cast<std::int64_t>(cr - view.first_row);
                    caret.column = col;
                }
            }
            if (doc.buffer.has_selection()) {
                const EditorPos from = doc.buffer.selection_begin();
                const EditorPos to = doc.buffer.selection_end();
                if (from.row < last && to.row >= view.first_row) {
                    std::int64_t brow;
                    std::int64_t bcol;
                    if (from.row < view.first_row) {
                        brow = chrome_rows;
                        bcol = 0;
                    } else {
                        brow = chrome_rows + static_cast<std::int64_t>(from.row - view.first_row);
                        const std::int64_t v =
                            ws::visual_col_of(doc.buffer.line(from.row), from.byte) - view.first_col;
                        bcol = v < 0 ? 0 : (v > text_cols ? text_cols : v);
                    }
                    std::int64_t erow;
                    std::int64_t ecol;
                    if (to.row >= last) {
                        erow = chrome_rows + static_cast<std::int64_t>(last - view.first_row);
                        ecol = 0;
                    } else {
                        erow = chrome_rows + static_cast<std::int64_t>(to.row - view.first_row);
                        const std::int64_t v =
                            ws::visual_col_of(doc.buffer.line(to.row), to.byte) - view.first_col;
                        ecol = v < 0 ? 0 : (v > text_cols ? text_cols : v);
                    }
                    if (erow > brow || ecol > bcol) {
                        caret.sel_begin_row = brow;
                        caret.sel_begin_col = bcol;
                        caret.sel_end_row = erow;
                        caret.sel_end_col = ecol;
                    }
                }
            }
        }
        return caret;
    }

    /// THE PANE, SAID: the live document composed for the granted room, its viewport moved as
    /// the composition moved it, drawn as this pane's own picture in its canvas room -- the caret
    /// and selection standing in its rows, the picture numbered within the room by what its rows
    /// mean -- its first row named `status` (the status, or a notice in its place) and each
    /// document line by its number, `line:<n>`. To a host granting no canvas the rows and the
    /// caret are said beside each other as prose, each naming the document's generation.
    // WL-EDIT-18 -- agents/workshop/editor-transfers.md
    void say(loom::Mail& mail) {
        if (!granted_ || (canvas_.grant > 0 && !on_canvas())) {
            return; // no room, or a canvas room with none: nothing this pane could truthfully fill
        }
        Composition c = compose(e_, viewport_of(e_), rows_, columns_, notice_, notice_bad_);
        apply_viewport(e_, c.view);
        chrome_rows_ = c.chrome_rows;
        doc_rows_ = c.doc_rows;
        status_row_ = c.status_row;
        // THE PICTURE MOVES EXACTLY WHEN THE ROW-TO-MEANING MAP DOES (WL-EDIT-18): the document,
        // its bytes, the viewport, the rows above it, the room -- or the selection a drop may land
        // ON. A caret move alone, a notice's words, a held press's words: the same picture.
        PictureKey key;
        key.epoch = e_.doc_epoch;
        key.content = e_.buffer.content_revision();
        key.first_row = e_.first_row;
        key.first_col = e_.first_col;
        key.chrome_rows = c.chrome_rows;
        key.rows = rows_;
        key.columns = columns_;
        key.selection = e_.open_document() && e_.buffer.has_selection();
        if (key.selection) {
            key.from = e_.buffer.selection_begin();
            key.to = e_.buffer.selection_end();
        }
        if (picture_ == 0 || !(key == picture_key_)) {
            picture_key_ = key;
            ++picture_;
        }
        std::vector<ws::PaneRowPart> parts = parts_of(c, columns_);
        if (on_canvas()) {
            (void)mail.as_role(pane::kEditorPaneRole)
                .send_to_role(kWorkshopRole,
                              ws::rows_picture(canvas_, pictures_.next(canvas_, picture_), c.rows,
                                               parts, rows_caret(c.caret)));
            return;
        }
        (void)mail.as_role(pane::kEditorPaneRole)
            .send_to_role(kWorkshopRole,
                          ws::v4::PaneContent{pane::kEditorPane, std::move(c.rows),
                                              static_cast<std::int64_t>(e_.doc_epoch), picture_,
                                              std::move(parts)});
        (void)mail.as_role(pane::kEditorPaneRole).send_to_role(kWorkshopRole, c.caret);
    }

    /// The live document, written into the shape Loom answers reads from (WL-EDIT-15), at the end
    /// of every delivery and in the publication hook. The two expensive fields are gated on what
    /// moved -- the bytes' revision for `text`, a stamp its writers bump for `saved_text` -- so a
    /// press, a drag, the wheel or a room grant rebuilds neither (`vocabulary.hpp` has the cost).
    void mirror_state() {
        state_.notice = notice_;
        state_.notice_bad = notice_bad_;
        state_.project_dir = project_dir_;
        state_.project_known = project_known_;
        state_.last_rows = e_.last_rows;
        state_.last_cols = e_.last_cols;
        state_.opened_by = static_cast<std::int64_t>(opened_by_);
        if (!e_.open_document()) {
            state_.path.clear();
            state_.text.clear();
            state_.saved_text.clear();
            state_.convention = 0;
            state_.doc_epoch = static_cast<std::int64_t>(e_.doc_epoch);
            state_.caret_row = 0;
            state_.caret_byte = 0;
            state_.anchor_row = 0;
            state_.anchor_byte = 0;
            state_.first_row = 0;
            state_.first_col = 0;
            state_.undo.clear();
            mirrored_content_ = 0;
            mirrored_saved_ = 0;
            mirrored_history_.reset();
            return;
        }
        state_.path = e_.path;
        state_.convention = e_.convention;
        state_.doc_epoch = static_cast<std::int64_t>(e_.doc_epoch);
        state_.caret_row = static_cast<std::int64_t>(e_.buffer.caret_row());
        state_.caret_byte = static_cast<std::int64_t>(e_.buffer.caret_byte());
        state_.anchor_row = static_cast<std::int64_t>(e_.buffer.anchor_row());
        state_.anchor_byte = static_cast<std::int64_t>(e_.buffer.anchor_byte());
        state_.first_row = static_cast<std::int64_t>(e_.first_row);
        state_.first_col = e_.first_col;
        // The bytes' own revision, not the buffer's: `revision()` moves with the caret, which a
        // pending paste must notice, and would rebuild the whole text on an arrow key. The epoch
        // is part of the key, since an activated document is another buffer.
        if (mirrored_content_ != e_.buffer.content_revision() || mirrored_epoch_ != e_.doc_epoch) {
            state_.text = ws::source_text(e_.buffer.lines(), e_.convention);
            mirrored_content_ = e_.buffer.content_revision();
            mirrored_epoch_ = e_.doc_epoch;
            ++state_.text_builds; // the cost, counted where it is paid
        }
        if (mirrored_saved_ != saved_stamp_) {
            state_.saved_text = ws::source_text(e_.saved_lines, e_.convention);
            mirrored_saved_ = saved_stamp_;
        }
        // THE HISTORY, written when its steps move -- a new group, an undo, a redo, an eviction, a
        // new document -- and never while typing grows the group its newest step began.
        const std::pair<std::uint64_t, std::uint64_t> history{e_.buffer.history_revision(), e_.doc_epoch};
        if (mirrored_history_ != history) {
            state_.undo.clear();
            for (const EditorBuffer::HistoryStep& step : e_.buffer.history()) {
                state_.undo.push_back(pane::EditorUndoStep{
                    joined_lines(step.lines), static_cast<std::int64_t>(step.caret.row),
                    static_cast<std::int64_t>(step.caret.byte), static_cast<std::int64_t>(step.anchor.row),
                    static_cast<std::int64_t>(step.anchor.byte)});
            }
            mirrored_history_ = history;
        }
    }

    // ---- State not in the shape ----------------------------------------------------------

    zengine::ActivationCursor activation_;

    /// THE ONE SOURCE DOCUMENT (editor.hpp): the identity, the buffer with its caret,
    /// selection and history, the saved comparison the dirty answer derives from, the line
    /// convention, the epoch, and the viewport. Owned by nobody else in this process.
    EditorState e_;

    /// THE ONE PREPARED CANDIDATE of a managed opening, and the sweep in flight.
    Candidate candidate_;
    Drag drag_;

    /// The opens relayed through the old door (`Relay`), at most `kMaxRelays` at once.
    std::vector<Relay> relays_;

    /// WHERE THIS RUN BEGAN, as the host said it -- what a relative spelling means. Empty
    /// until answered, and empty for a run that began nowhere; `project_known_` is which of
    /// those two an empty string is (WL-EDIT-06).
    std::string project_dir_;
    bool project_known_ = false;
    bool root_asked_ = false;
    std::uint64_t root_pending_ = 0;

    component::Clipboard clip_;
    Paste paste_;

    /// THE LAST THING THIS PANE HAD TO SAY -- a refusal, or what an act came to -- standing
    /// until the weaver's next act, on the row the composition budgets for it.
    std::string notice_;
    bool notice_bad_ = false;

    /// WHICH COMPOSED ROWS ARE ABOVE THE DOCUMENT, and how many the document was given --
    /// written by `say` and read by a press and a drag, so a press and a picture cannot
    /// disagree about what is where.
    std::int64_t chrome_rows_ = 0;
    std::int64_t doc_rows_ = 0;

    /// ONE COUNTER FOR EVERY QUESTION THIS PANE ASKS, so a correlation is this incarnation's
    /// own and an answer to somebody else's question is not mistaken for one to ours.
    std::uint64_t asked_ = 0;

    /// What the mirrored shape was built from: the content revision and epoch `text` was joined
    /// at, and the stamp `saved_text` was (bumped by every writer of `saved_lines`), so a rebuild
    /// happens when the bytes moved and at no other time.
    std::uint64_t saved_stamp_ = 0;
    std::uint64_t mirrored_content_ = 0;
    std::uint64_t mirrored_epoch_ = 0;
    std::uint64_t mirrored_saved_ = 0;
    /// ...and the history revision and epoch `undo` was written at; none before the first.
    std::optional<std::pair<std::uint64_t, std::uint64_t>> mirrored_history_;

    /// THE MANAGED OPERATION THAT INSTALLED THE CURRENT DOCUMENT, or 0 -- carried in the
    /// claim so the publication hook can match the identity it is shown to the candidate.
    std::uint64_t opened_by_ = 0;

    /// THE DOCUMENT IDENTITY THIS WEAVE LAST CLAIMED, and the dirty answer cached beside it.
    EditorDocument claimed_;
    bool claimed_ever_ = false;
    bool dirty_known_ = false;
    bool dirty_cached_ = false;
    std::uint64_t dirty_content_ = 0;
    std::uint64_t dirty_saved_ = 0;

    /// ROWS OWED AT THE END OF THE NEXT DELIVERY -- set by the publication hook, which has no
    /// Mail to say them with.
    bool resay_ = false;

    std::int64_t rows_ = 0;
    std::int64_t columns_ = 0;
    bool granted_ = false;

    /// THE CANVAS THIS PANE DRAWS ON: the room Workshop granted it there, its pictures numbered in
    /// that room by what their rows mean, and the primary press whose motion sweeps. Not reload
    /// state: a reloaded image waits for its room.
    ws::PaneCanvasRoom canvas_;
    ws::CanvasPictures pictures_;
    std::int64_t held_ = 0;
    bool on_canvas() const { return canvas_.grant > 0 && canvas_.width > 0 && canvas_.height > 0; }

    /// THE SWITCH HOLDING THIS EDITOR STILL, while one does (`Holding`).
    Holding holding_;

    /// THIS INCARNATION BEGAN AS A SWITCH'S CANDIDATE AND ADOPTED A DOCUMENT while sealed.
    bool adopted_ = false;

    // ---- Transfers (WL-EDIT-17..21). None of it is reload state: every record below is this
    // incarnation's conversation, and a reloaded image begins with none -- a late answer to a
    // predecessor's ask matches nothing here.

    /// THE PICTURE LAST PUBLISHED, and the map it numbers (WL-EDIT-18).
    std::int64_t picture_ = 0;
    PictureKey picture_key_;
    /// WHETHER ROW 0 IS THE STATUS ROW -- the location's surface -- in the picture last said.
    bool status_row_ = false;
    Grab grab_;
    Pickup pickup_;
    ws::pane_menu::Asked menu_;
    Mark menu_mark_;
    Dropped drop_;
    Locate locate_;
    /// WHAT THE LAST ACTIVATION INSTALLED, so a dropped location's caret lands only on it untouched.
    std::uint64_t installed_epoch_ = 0;
    std::uint64_t installed_revision_ = 0;
#ifdef ZENGINE_EDITOR_TEST_SILENT_WARM
    loom::DeferredAnswer silent_warm_;
#endif
};

} // namespace

#include "editor/editor-pane/opening_switch.hpp"
#include "editor/editor-pane/transfers.hpp"

ZEN_EXPORT_WEAVE(EditorPaneWeave)
