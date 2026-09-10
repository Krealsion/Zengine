// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Editor pane -- a loadable weave that offers Workshop one pane and HOLDS the one source
// document a maker edits: its path, its bytes, its saved comparison, its line convention,
// its caret and selection, its history, and the viewport it is looked at through.
//
// IT USED TO BE A BUILT-IN INSIDE THE HOST (`panel::kEditor`, `Session::editor`,
// `screen_editor.cpp`'s painter, `weave_editor.cpp`'s key/text/press bodies, `weave_pane_
// editor.cpp`'s `open_source`/`save_source`/`discard_source_edits`, `KeyContext::kEditor`
// and `kNoEditor`, four `Act` values, a `PasteOwner`, a text-drag place, a wheel arm, a
// motion arm, and the one synchronous read `quit()` made of the dirty state). Now it is a
// weave beside the Skin, the Timer, Files, the Builder, Attention, Info and the Terminal --
// the last of the built-ins this arc set out to move.
//
// ⚠ THE DOCUMENT CAME WITH IT, AND THAT IS THE DIFFERENCE FROM EVERY EARLIER MIGRATION.
// Files left the project root with the host, the Builder left the tool, the Terminal left the
// participant; each pane presents a subject somebody else owns and asks about it. The
// Editor's subject is the buffer a maker types into, and a pane that presented a buffer the
// host still owned would cross the seam twice per keystroke and hold a second mutable copy
// of the same bytes. So this weave is the one custodian: the buffer machinery
// (`editor.hpp`) came here whole, the file is read and written from here, and what crosses
// is a source request, a reveal, a quit answer, rows and a caret. What the host keeps is
// room, focus, membership and the exit DECISION -- which it now makes by asking.
//
// ⚠ A REPLACEABLE PANE IS THE CUSTODIAN OF UNSAVED WORK, DELIBERATELY, and three things make
// that a design rather than an accident. Presentation and custody are two lifetimes here
// exactly as they were: hiding, covering, moving, reordering or removing the PANE touches
// this weave not at all, because Workshop closes a presentation and sends no unload. A
// SAME-SHAPE RELOAD carries the document across (`EditorPaneState`; `snapshot`/`revive`
// below), so the one lifecycle act a maker can perform on this image keeps their work. And
// an ORDERLY QUIT asks this weave before it stops the bus (`PaneQuitRequested`), so dirty
// source refuses the exit exactly as it did when the host could read it. What is NOT
// claimed is what was never claimed: process death still loses drafts.
//
// ⚠ WHAT MOVED ACROSS A MESSAGE BOUNDARY, AND WHAT DID NOT. Save stayed one synchronous
// call inside this weave: the write, then the saved comparison, with no delivery between
// them, so a success is about the bytes that were written and can never mark a later edit
// clean. Open stayed one call too: read and judge before anything moves, then install. What
// became a round trip is the PASTE (the Skin answers later; the answer is pinned to the
// document epoch and the buffer revision it was asked for, as the host pinned it), the
// REVEAL (the host seats the pane after this weave has installed the document; a refused
// reveal leaves the document open here, as a removed pane would) and the QUIT (the host asks
// and waits; this weave answers about the instant of the answer, and refuses while a paste
// is still arriving, because a permission a queued message could falsify is not one).

#include "editor-pane/vocabulary.hpp"

#include "editor-pane/editor.hpp"
#include "workshop/pane_seam_vocabulary.hpp"
#include "workshop/pane_text.hpp"
#include "workshop/pane_vocabulary.hpp"
#include "workshop/persist.hpp"

#include "activation/activation.hpp"
#include "component/text_box.hpp"
#include "input/vocabulary.hpp"
#include "surface/vocabulary.hpp"

#include <zen/kernel/export.hpp>
#include <zen/weave.hpp>
#include <zen/weave/lifecycle.hpp>
#include <zen/weave/standard_shapes.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace {

namespace component = zengine::component;
namespace input = zengine::input;
namespace surface = zengine::surface;
namespace ws = zengine::workshop;
namespace pane = zengine::editor_pane;

using ws::EditorBuffer;
using ws::EditorPos;
using ws::EditorState;
using ws::OpenSourceRequested;
using ws::PaneActionRequested;
using ws::PaneActionRow;
using ws::PaneActions;
using ws::PaneCaret;
using ws::PaneCatalogRequested;
using ws::PaneContent;
using ws::PaneDragged;
using ws::PaneKey;
using ws::PaneOffered;
using ws::PanePressed;
using ws::PaneQuitAnswered;
using ws::PaneQuitRequested;
using ws::PaneRevealRequested;
using ws::PaneRoom;
using ws::PaneTextInput;
using ws::PaneWheel;
using ws::ProjectRoot;
using ws::ProjectRootRequested;
using ws::SourceOpened;
using ws::Written;
using zengine::workshop::pane_text::drawable;
using zengine::workshop::pane_text::fit;

/// The office Workshop holds, named as a STRING rather than reached through
/// `workshop/panel.hpp`: a provider is a stranger to Workshop's internals and says who it
/// is talking to the way a third party would.
constexpr const char* kWorkshopRole = "zengine.workshop";

/// ONE COLUMN OF EVERY DOCUMENT ROW THE TEXT MAY NOT USE -- the caret's own. The built-in's
/// `kEditorCaretCols`, carried: a caret at the end of a full row would otherwise sit one past
/// the room, and in a cell projection the mark is a character that needs a cell of its own.
constexpr std::int64_t kCaretCols = 1;

/// THE FEWEST ROWS IN WHICH A REFUSAL GETS A ROW OF ITS OWN: the status row, the refusal,
/// and at least one row of the document under them. In a smaller room the refusal takes the
/// status row's place instead, so the document keeps every row it had (the Terminal's rule,
/// one pane over: what a maker is typing into is the last thing a small room gives up).
constexpr std::int64_t kNoticeNeedsRows = 3;

/// THE ANSWER A QUIT ASK GETS WHILE A PASTE IS STILL ARRIVING. A refusal and not a wait,
/// because the host counts answers and a pane that answered nothing would hold every other
/// pane's exit hostage to one clipboard read.
constexpr const char* kPasteInFlight =
    "the Editor is still waiting for a clipboard answer -- quit again";

// =============================================================================

class EditorPaneWeave
    : public loom::WeaveBase<
          EditorPaneWeave, pane::EditorPaneState,
          loom::Accept<loom::Activated, PaneCatalogRequested, PaneRoom, PanePressed,
                       PaneDragged, PaneKey, PaneTextInput, PaneWheel, PaneActionRequested,
                       PaneQuitRequested, OpenSourceRequested, ProjectRoot,
                       surface::ClipboardCopy, surface::ClipboardText>,
          loom::Emit<PaneOffered, PaneActions, PaneContent, PaneCaret, PaneRevealRequested,
                     PaneQuitAnswered, SourceOpened, ProjectRootRequested,
                     surface::ClipboardCopy, surface::ClipboardTextRequested>> {
public:
    using Base = loom::WeaveBase<
        EditorPaneWeave, pane::EditorPaneState,
        loom::Accept<loom::Activated, PaneCatalogRequested, PaneRoom, PanePressed, PaneDragged,
                     PaneKey, PaneTextInput, PaneWheel, PaneActionRequested, PaneQuitRequested,
                     OpenSourceRequested, ProjectRoot, surface::ClipboardCopy,
                     surface::ClipboardText>,
        loom::Emit<PaneOffered, PaneActions, PaneContent, PaneCaret, PaneRevealRequested,
                   PaneQuitAnswered, SourceOpened, ProjectRootRequested, surface::ClipboardCopy,
                   surface::ClipboardTextRequested>>;

    // ---- The state a reload carries, materialized on demand ----------------------------

    /// THE SNAPSHOT IS BUILT WHEN LOOM ASKS FOR IT, from the live buffer, and at no other
    /// moment. `state_` is deliberately not written on every keystroke: the document can be
    /// four megabytes, and a copy of it per gesture would be the cost of a reload paid on
    /// every keystroke that never leads to one. The shape's own comment says what crosses
    /// and what does not.
    loom::Value snapshot() const override {
        pane::EditorPaneState s;
        if (e_.open_document()) {
            s.path = e_.path;
            s.text = ws::source_text(e_.buffer.lines(), e_.convention);
            s.saved_text = ws::source_text(e_.saved_lines, e_.convention);
            s.convention = e_.convention;
            s.doc_epoch = static_cast<std::int64_t>(e_.doc_epoch);
            s.caret_row = static_cast<std::int64_t>(e_.buffer.caret_row());
            s.caret_byte = static_cast<std::int64_t>(e_.buffer.caret_byte());
            s.anchor_row = static_cast<std::int64_t>(e_.buffer.anchor_row());
            s.anchor_byte = static_cast<std::int64_t>(e_.buffer.anchor_byte());
            s.first_row = static_cast<std::int64_t>(e_.first_row);
            s.first_col = e_.first_col;
        }
        return loom::to_value(s);
    }

    /// THE DOCUMENT COMES BACK IN `revive`, WHICH IS THE CALL A RELOAD ACTUALLY MAKES.
    /// `swap_state` revives the new incarnation from the host-owned snapshot before anything
    /// else happens to it, so the buffer is whole before the first delivery -- and before
    /// the activation the control door announces afterwards, which is where this pane
    /// re-offers itself so the host re-grants its room (`on(Activated)`).
    ///
    /// THE BYTES MEET THE SAME LAW A FILE MEETS. `text` is `source_text`'s output, so
    /// `source_in` admits it exactly; a snapshot that somehow held bytes the law refuses
    /// (there is no such writer, but a snapshot is a value and a value can be anything)
    /// leaves the pane with no document rather than with a document it could not edit
    /// truthfully.
    void revive(const loom::Value& v) override {
        Base::revive(v);
        restore_from_state();
    }

    // ---- Offering and the room -----------------------------------------------------------

    void on(const loom::Activated& a, loom::Mail& mail) {
        if (!activation_.accept(mail, a)) {
            return;
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

    void on(const PaneRoom& room, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || room.pane != pane::kEditorPane) {
            return;
        }
        rows_ = room.rows;
        columns_ = room.columns;
        granted_ = true;
        if (!root_asked_) {
            ask_project_root(mail); // a host whose door was not there at activation
        }
        say(mail);
    }

    /// WHERE THIS RUN BEGAN, asked of `zengine.project` -- the one host fact a relative
    /// spelling needs, and the door Files asks for the same reason (WL-FILES-02).
    void on(const ProjectRoot& said, loom::Mail& mail) {
        if (!mail.answers_ask() || mail.correlation() != root_pending_) {
            return; // not Loom's answer to the question this pane asked
        }
        project_dir_ = said.project_dir;
    }

    // ---- THE ONE DOOR: open a source ----------------------------------------------------

    /// SOMEBODY ASKS THIS WEAVE TO OPEN A SOURCE. Files hands it a row's absolute path; the
    /// Builder hands it the one path the host resolved a recipe to. Both arrive here, through
    /// the one `open_source` the built-in had (WL-EDIT-05), and the outcome travels back as
    /// the answer.
    ///
    /// AN OFFICE, AND ONLY AN OFFICE, the host doors' rule: opening a maker's source for
    /// anonymous speech would be this weave acting on a sentence with no author.
    ///
    /// THEN THE REVEAL, AND THE ORDER IS THE HONESTY. The document is installed and the asker
    /// is answered FIRST; the ask to be shown goes out afterwards, and the host may refuse it
    /// (no slot on this screen). A refused reveal is said by the host on its notice line and
    /// changes nothing here: the document is open in a pane the maker can bring back from
    /// the picker, exactly as a removed pane's document is.
    void on(const OpenSourceRequested& asked, loom::Mail& mail) {
        if (mail.authored_role().empty()) {
            return;
        }
        const Written done = open_source(asked.path);
        (void)mail.answer(SourceOpened{done.accepted, done.refusal});
        if (done.accepted) {
            (void)mail.as_role(pane::kEditorPaneRole)
                .send_to_role(kWorkshopRole, PaneRevealRequested{pane::kEditorPane});
        }
        say(mail);
    }

    // ---- The pointer ---------------------------------------------------------------------

    /// A PRESS NAMES A ROW OF THIS PANE'S ROOM. The rows above the document -- the status
    /// row, a standing notice -- are consumed as a focus statement and move nothing; a row
    /// of the document places the caret through the same tab geometry the row was painted
    /// with (WL-EDIT-08), at `first_col + column` of the whole line, which is the one
    /// subtraction a horizontal viewport adds to a hit test.
    void on(const PanePressed& press, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || press.pane != pane::kEditorPane) {
            return;
        }
        if (!e_.open_document() || press.row < chrome_rows_) {
            return;
        }
        notice_.clear();
        const std::size_t row = e_.first_row + static_cast<std::size_t>(press.row - chrome_rows_);
        const std::size_t target =
            row < e_.buffer.line_count() ? row : e_.buffer.line_count() - 1;
        const std::int64_t column = press.column < 0 ? 0 : press.column;
        e_.buffer.place(target, ws::byte_of_visual_col(e_.buffer.line(target),
                                                        e_.first_col + column));
        e_.follow_caret = true;
        say(mail);
    }

    /// THE HAND MOVED WITH THE BUTTON DOWN -- the built-in's selection sweep, restored through
    /// the one drag shape the seam gained for it. The row is UNCLAMPED on purpose: a hand past
    /// the body's top or bottom edge steps the caret one row further per motion (the
    /// component's leftward-step law, turned vertical), and the follow flag then pulls the
    /// viewport after it -- deterministic, minimal, and enough to sweep a selection out of the
    /// window a motion at a time. A negative column steps one position leftward per motion for
    /// the same reason (`EditorBuffer::drag_to`).
    void on(const PaneDragged& drag, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || drag.pane != pane::kEditorPane) {
            return;
        }
        if (!e_.open_document()) {
            return;
        }
        const std::int64_t brow = drag.row - chrome_rows_;
        std::size_t target;
        if (brow < 0) {
            target = e_.first_row > 0 ? e_.first_row - 1 : 0;
        } else if (brow >= doc_rows_) {
            target = e_.first_row + static_cast<std::size_t>(doc_rows_);
        } else {
            target = e_.first_row + static_cast<std::size_t>(brow);
        }
        e_.buffer.drag_to(target, drag.column < 0 ? std::int64_t{-1} : e_.first_col + drag.column);
        e_.follow_caret = true;
        say(mail);
    }

    /// THE WHEEL SCROLLS THE DOCUMENT AND MOVES NO CARET (WL-EDIT-10): the notches accumulate
    /// until they are worth whole lines, the window slides inside the document, and the
    /// follow flag is deliberately NOT set -- the wheel's whole meaning is to look elsewhere
    /// while the caret stays put. The next caret gesture brings the view back.
    void on(const PaneWheel& wheel, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || wheel.pane != pane::kEditorPane) {
            return;
        }
        if (!e_.open_document()) {
            return;
        }
        e_.wheel_accum += wheel.dy * static_cast<double>(ws::kEditorWheelLines);
        const std::int64_t lines = static_cast<std::int64_t>(e_.wheel_accum);
        e_.wheel_accum -= static_cast<double>(lines);
        if (lines == 0) {
            return;
        }
        const std::size_t rows = static_cast<std::size_t>(doc_rows_ > 0 ? doc_rows_ : 0);
        const std::size_t total = e_.buffer.line_count();
        const std::size_t furthest = total > rows ? total - rows : 0;
        std::size_t first = e_.first_row;
        if (lines > 0) {
            const std::size_t up = static_cast<std::size_t>(lines);
            first = first > up ? first - up : 0;
        } else {
            first += static_cast<std::size_t>(-lines);
        }
        if (first > furthest) {
            first = furthest;
        }
        if (first == e_.first_row) {
            return; // already at the edge: nothing moved, nothing to say again
        }
        e_.first_row = first;
        say(mail);
    }

    // ---- The keys ------------------------------------------------------------------------

    /// THE BUFFER'S OWN VOCABULARY: caret movement, selection, clipboard, word erases,
    /// history -- through the one `consume` every consumer of it makes (WL-EDIT-02). A
    /// gesture this pane declared a row for never reaches here; it arrives as
    /// `PaneActionRequested` instead.
    void on(const PaneKey& key, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || key.pane != pane::kEditorPane) {
            return;
        }
        if (!e_.open_document()) {
            return; // an empty editor has no document for a key to mean anything to
        }
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

    /// TEXT THE MAKER TYPED INTO THE SOURCE, gated by the source-byte law at this door
    /// (WL-EDIT-07): a chunk with one byte outside plain ASCII is refused whole, with a
    /// sentence, and the keystroke costs nothing.
    void on(const PaneTextInput& typed, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || typed.pane != pane::kEditorPane) {
            return;
        }
        if (!e_.open_document() || typed.text.empty()) {
            return;
        }
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

    /// THE FOUR ROWS THIS PANE DECLARED, acted on by NAME (WL-KEY-15): save, newline, tab,
    /// discard. A maker's override moved the key; the id is what arrives.
    void on(const PaneActionRequested& asked, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || asked.pane != pane::kEditorPane) {
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

    // ---- The exit ------------------------------------------------------------------------

    /// MAY THE WORKSHOP END? Answered about THIS instant: dirty source refuses, naming the two
    /// ways out; a paste still arriving refuses too, because its answer could dirty the
    /// document after this one was given; a clean document, or no document, permits. No
    /// input can reach this weave between this answer and the host's decision (the host holds
    /// every gesture while it waits), which is what makes "clean" a fact rather than a race.
    void on(const PaneQuitRequested&, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole)) {
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

    /// THE SKIN'S ANSWER TO A PASTE THIS PANE ASKED FOR (WL-EDIT-11, WL-TEXT-09). The
    /// correlation says this is the answer to an ask this incarnation made; it does not say
    /// the document that asked still stands, or stands where it stood. The settlement pins
    /// the whole position, as the host pinned it: a replaced or closed document strands the
    /// payload silently (the dead draft's own fate); a document that MOVED -- any edit, any
    /// caret or selection change between request and answer -- gets a sentence instead of a
    /// paste, because relocating the text to wherever the caret is now would be answering a
    /// question the maker no longer asked.
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
    // ---- Offering and declaring ---------------------------------------------------------

    void announce(loom::Mail& mail) {
        (void)mail.as_role(pane::kEditorPaneRole)
            .send_to_role(kWorkshopRole, PaneOffered{pane::kEditorPane, pane::kEditorPaneName,
                                                     pane::kEditorPaneSummary});
        declare(mail);
    }

    /// WHAT THIS PANE ANSWERS TO -- four rows, the built-in's own four, and they never
    /// change. Everything else a maker presses reaches the buffer as an ordinary `PaneKey`,
    /// which is what lets Backspace erase and ctrl+z undo without either being anybody's row.
    void declare(loom::Mail& mail) {
        PaneActions actions;
        actions.pane = pane::kEditorPane;
        const auto row = [&actions](const char* id, const char* label, std::int64_t sc,
                                    std::int64_t mods) {
            actions.rows.push_back(PaneActionRow{id, label, sc, mods});
        };
        row(pane::kActionSave, "save source", input::scan::kS, input::mod::kCtrl);
        row(pane::kActionNewline, "newline", input::scan::kReturn, input::mod::kNone);
        row(pane::kActionTab, "insert tab", input::scan::kTab, input::mod::kNone);
        row(pane::kActionDiscard, "discard source edits", input::scan::kD, input::mod::kCtrl);
        (void)mail.as_role(pane::kEditorPaneRole).send_to_role(kWorkshopRole, actions);
    }

    void ask_project_root(loom::Mail& mail) {
        root_asked_ = true;
        root_pending_ = ++asked_;
        (void)mail.as_role(pane::kEditorPaneRole)
            .send_to_role(ws::kProjectRole, ProjectRootRequested{}, root_pending_);
    }

    // ---- The document doors --------------------------------------------------------------

    /// THE ONE DOOR (WL-EDIT-05, WL-EDIT-06, WL-EDIT-13's remedy moved to the host): normalize
    /// against the project, same-path reveal, dirty refusal, bounded read, `source_in`, install
    /// with `doc_epoch++` and a viewport reset. Every referrer arrives through it.
    Written open_source(const std::string& requested) {
        const std::string path = ws::persist::resolved_against(project_dir_, requested);
        if (e_.open_document() && e_.path == path) {
            // RE-REQUESTING THE OPEN SOURCE REVEALS IT AND DESTROYS NOTHING: the buffer,
            // its caret, its selection, its history and its viewport all stand; what
            // moves is presence and the keyboard, and those are the host's to move.
            e_.follow_caret = true;
            notice(e_.dirty() ? "UNSAVED edits stand -- editing " + shown_path()
                              : "editing " + shown_path(),
                   false);
            return Written::ok();
        }
        if (e_.dirty()) {
            // THE UNSAVED-LOSS FLOOR: a different source must not silently replace a dirty
            // buffer. The two ways out are this pane's own save and its one deliberate
            // discard, named by their actions; the band spells their keys while the pane
            // holds the keyboard.
            return Written::no(e_.path + " has unsaved changes -- save source or discard "
                                         "source edits in the Editor first; nothing was opened");
        }
        // READ AND JUDGE BEFORE ANYTHING MOVES: a refused file costs the asker its refusal
        // and nothing else -- the current document (if any) and the file itself are exactly
        // as they were.
        const ws::persist::FileText read =
            ws::persist::read_file(path, ws::kMaxSourceBytes, "a source file");
        if (!read.outcome.accepted) {
            return read.outcome;
        }
        ws::SourceIn admitted = ws::source_in(read.text);
        if (!admitted.outcome.accepted) {
            return Written::no(path + ": " + admitted.outcome.refusal);
        }
        install(path, std::move(admitted));
        notice("editing " + shown_path(), false);
        return Written::ok();
    }

    /// PUT AN ADMITTED DOCUMENT IN PLACE: identity, bytes, saved copy, convention, a new
    /// epoch, and a fresh viewport. The one writer of `e_.path`.
    void install(const std::string& path, ws::SourceIn admitted) {
        e_.path = path;
        e_.saved_lines = admitted.lines;
        e_.buffer.set_lines(std::move(admitted.lines));
        e_.convention = admitted.convention;
        ++e_.doc_epoch;
        e_.first_row = 0;
        e_.first_col = 0;
        e_.wheel_accum = 0.0;
        e_.follow_caret = true;
    }

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

    /// ...AND THE DRAFT THAT ASKED, pinned as the host pinned it: the document epoch and the
    /// buffer revision at the moment of the ask (WL-EDIT-11).
    void begin_paste(loom::Mail& mail) {
        paste_.pending = ++asked_;
        paste_.doc = e_.doc_epoch;
        paste_.revision = e_.buffer.revision();
        paste_.awaiting = true;
        (void)mail.as_role(pane::kEditorPaneRole)
            .send_to_role(surface::kSkinRole, surface::ClipboardTextRequested{},
                          paste_.pending);
    }

    /// THE DOCUMENT A SNAPSHOT CARRIED, PUT BACK (see `revive`). `restore_selection` clamps a
    /// pair that outran the bytes; the viewport offsets are clamped by the next reconcile.
    void restore_from_state() {
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
        e_.buffer.set_lines(std::move(text.lines));
        e_.convention = state_.convention;
        e_.doc_epoch = static_cast<std::uint64_t>(state_.doc_epoch);
        e_.buffer.restore_selection(as_index(state_.anchor_row), as_index(state_.anchor_byte),
                                    as_index(state_.caret_row), as_index(state_.caret_byte));
        e_.first_row = as_index(state_.first_row);
        e_.first_col = state_.first_col < 0 ? 0 : state_.first_col;
        e_.wheel_accum = 0.0;
        e_.follow_caret = false;
        e_.last_rows = 0;
        e_.last_cols = 0;
    }

    static std::size_t as_index(std::int64_t n) {
        return n < 0 ? 0 : static_cast<std::size_t>(n);
    }

    void notice(std::string text, bool bad) {
        notice_ = std::move(text);
        notice_bad_ = bad;
    }

    // ---- The viewport ----------------------------------------------------------------------

    /// KEEP THE VIEWPORT TRUE AGAINST THE ROOM AND THE DOCUMENT IT HAS NOW (WL-EDIT-09): clamp
    /// the offsets always, follow the caret when a gesture asked or the body's room changed,
    /// and deliberately not after the wheel.
    void reconcile(std::int64_t rows_in, std::int64_t text_cols) {
        const bool resized = rows_in != e_.last_rows || text_cols != e_.last_cols;
        e_.last_rows = rows_in;
        e_.last_cols = text_cols;
        const std::size_t rows = static_cast<std::size_t>(rows_in > 0 ? rows_in : 0);
        const std::size_t total = e_.buffer.line_count();
        const std::size_t furthest_row = total > rows ? total - rows : 0;
        if (e_.first_row > furthest_row) {
            e_.first_row = furthest_row;
        }
        if (e_.first_col < 0) {
            e_.first_col = 0;
        }
        if (!e_.follow_caret && !resized) {
            return;
        }
        e_.follow_caret = false;
        if (rows == 0) {
            return; // no row to follow into: the offsets keep their answer for a room to come
        }
        const std::size_t cr = e_.buffer.caret_row();
        if (cr < e_.first_row) {
            e_.first_row = cr;
        }
        if (cr >= e_.first_row + rows) {
            e_.first_row = cr + 1 - rows;
        }
        if (text_cols <= 0) {
            return;
        }
        const std::string& line = e_.buffer.line(cr);
        const std::int64_t vis = ws::visual_col_of(line, e_.buffer.caret_byte());
        // Rule 1's horizontal half, measured on the caret's own line: no blank room at the
        // right while its text is hidden at the left, so erasing a long line back down
        // recovers the room it freed.
        const std::int64_t need = ws::visual_len(line) + kCaretCols;
        const std::int64_t furthest_col = need > text_cols ? need - text_cols : 0;
        if (e_.first_col > furthest_col) {
            e_.first_col = furthest_col;
        }
        if (vis < e_.first_col) {
            e_.first_col = vis;
        }
        if (vis - e_.first_col > text_cols) {
            e_.first_col = vis - text_cols;
        }
    }

    // ---- The rows, and the caret beside them ---------------------------------------------

    /// THE STATUS ROW: the dirty word first, then `L:C/N`, then the path -- in the order the
    /// facts must survive a narrow room (WL-EDIT-12). The path is cut from its HEAD when the
    /// room is short of it: the end of a path is the part that says which file this is, and a
    /// temporary directory's spelling is long enough on every platform to have proved it. The
    /// pane's name and office are the host's header, one row above, so `Editor` is not said
    /// twice.
    std::string status_text() const {
        if (!e_.open_document()) {
            return "no source open -- Return on a file in Files, or e in the Builder";
        }
        std::string head = e_.dirty() ? "UNSAVED" : "saved";
        head += " L" + std::to_string(e_.buffer.caret_row() + 1) + ":C" +
                std::to_string(ws::visual_col_of(e_.buffer.line(e_.buffer.caret_row()),
                                                 e_.buffer.caret_byte()) +
                               1);
        head += "/" + std::to_string(e_.buffer.line_count());
        head += " -- ";
        return head + tail_of_path(e_.path, columns_ - static_cast<std::int64_t>(head.size()));
    }

    /// THE OPEN PATH AS A NOTICE SAYS IT: whole where the room holds it, its end otherwise.
    /// A notice is one row of this pane's room, and every sentence above puts its fact
    /// before the path so the fact survives whatever the path costs.
    std::string shown_path() const {
        return tail_of_path(e_.path, columns_ > 24 ? columns_ - 24 : columns_);
    }

    /// A PATH THAT KEEPS ITS END. Whole when it fits; otherwise `...` and the last `width - 3`
    /// bytes, so the file's own name survives before its directories do; a width too small
    /// for even that keeps the last bytes alone.
    static std::string tail_of_path(const std::string& path, std::int64_t width) {
        if (width <= 0) {
            return std::string();
        }
        const std::size_t room = static_cast<std::size_t>(width);
        if (path.size() <= room) {
            return path;
        }
        if (room <= 4) {
            return path.substr(path.size() - room);
        }
        return "..." + path.substr(path.size() - (room - 3));
    }

    /// THE PANE, COMPOSED: the status row, a standing notice where the room holds one, then
    /// the document through the viewport -- and the caret and selection published beside the
    /// rows, on the same lattice a press names.
    ///
    /// THE ROW BUDGET IS SPENT IN THIS ORDER, because a pane can be granted any height a
    /// maker's arrangement gives it. The status row is first: it is where the dirty word
    /// lives, and the one thing a maker must be able to read before they build. A notice --
    /// a refusal, or what the last act came to -- gets a row of its own only where at least
    /// one document row survives under it; in a smaller room it stands in for the status row
    /// instead, so the document keeps its rows and the caret keeps its place. The document
    /// takes what is left.
    void say(loom::Mail& mail) {
        if (!granted_) {
            return; // no room has been sent: there is nothing this pane could truthfully fill
        }
        std::vector<surface::SurfaceTextRow> out;
        const auto push = [&out, this](std::string text, std::int64_t role) {
            out.push_back(surface::SurfaceTextRow{drawable(fit(std::move(text), columns_)), role});
        };
        chrome_rows_ = 0;
        doc_rows_ = 0;
        if (rows_ >= 1) {
            const bool notice_row = !notice_.empty() && rows_ >= kNoticeNeedsRows;
            if (!notice_.empty() && !notice_row) {
                push(notice_, notice_bad_ ? surface::role::kAlert : surface::role::kMuted);
            } else {
                push(status_text(), surface::role::kAccent);
            }
            if (notice_row) {
                push(notice_, notice_bad_ ? surface::role::kAlert : surface::role::kMuted);
            }
        }
        chrome_rows_ = static_cast<std::int64_t>(out.size());
        doc_rows_ = rows_ > chrome_rows_ ? rows_ - chrome_rows_ : 0;
        const std::int64_t text_cols = columns_ - kCaretCols > 0 ? columns_ - kCaretCols : 0;
        std::size_t last = 0;
        if (e_.open_document()) {
            reconcile(doc_rows_, text_cols);
            const std::size_t total = e_.buffer.line_count();
            const std::size_t rows = static_cast<std::size_t>(doc_rows_);
            last = e_.first_row + rows < total ? e_.first_row + rows : total;
            for (std::size_t r = e_.first_row; r < last; ++r) {
                out.push_back(surface::SurfaceTextRow{
                    ws::expanded_slice(e_.buffer.line(r), e_.first_col, text_cols),
                    surface::role::kFill});
            }
        }
        (void)mail.as_role(pane::kEditorPaneRole)
            .send_to_role(kWorkshopRole, PaneContent{pane::kEditorPane, std::move(out)});
        say_caret(mail, last, text_cols);
    }

    /// WHERE THE CARET IS, AND WHAT IS SELECTED -- beside the rows, never inside them, in the
    /// body lattice (WL-CARET-01). The caret is said only while its row is in the window; the
    /// selection is clamped into the window on both ends, and a range that runs past the last
    /// shown row ends at `(rows, 0)` -- the exclusive end one past the last row, which is the
    /// one position with no row that a reading-order range may name (WL-CARET-03). A selection
    /// may stand with no caret: the caret scrolled out of the window is `kNoCaret`, and the
    /// range it belongs to is still on screen.
    void say_caret(loom::Mail& mail, std::size_t last, std::int64_t text_cols) {
        PaneCaret caret;
        caret.pane = pane::kEditorPane;
        if (e_.open_document() && doc_rows_ > 0) {
            const std::size_t cr = e_.buffer.caret_row();
            if (cr >= e_.first_row && cr < last) {
                const std::int64_t vis =
                    ws::visual_col_of(e_.buffer.line(cr), e_.buffer.caret_byte());
                const std::int64_t col = vis - e_.first_col;
                if (col >= 0 && col <= text_cols) {
                    caret.row = chrome_rows_ + static_cast<std::int64_t>(cr - e_.first_row);
                    caret.column = col;
                }
            }
            if (e_.buffer.has_selection()) {
                const EditorPos from = e_.buffer.selection_begin();
                const EditorPos to = e_.buffer.selection_end();
                if (from.row < last && to.row >= e_.first_row) {
                    std::int64_t brow;
                    std::int64_t bcol;
                    if (from.row < e_.first_row) {
                        brow = chrome_rows_;
                        bcol = 0;
                    } else {
                        brow = chrome_rows_ + static_cast<std::int64_t>(from.row - e_.first_row);
                        const std::int64_t v =
                            ws::visual_col_of(e_.buffer.line(from.row), from.byte) - e_.first_col;
                        bcol = v < 0 ? 0 : (v > text_cols ? text_cols : v);
                    }
                    std::int64_t erow;
                    std::int64_t ecol;
                    if (to.row >= last) {
                        erow = chrome_rows_ + static_cast<std::int64_t>(last - e_.first_row);
                        ecol = 0;
                    } else {
                        erow = chrome_rows_ + static_cast<std::int64_t>(to.row - e_.first_row);
                        const std::int64_t v =
                            ws::visual_col_of(e_.buffer.line(to.row), to.byte) - e_.first_col;
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
        (void)mail.as_role(pane::kEditorPaneRole).send_to_role(kWorkshopRole, caret);
    }

    // ---- State not in the shape ----------------------------------------------------------

    zengine::ActivationCursor activation_;

    /// THE ONE SOURCE DOCUMENT (editor.hpp): the identity, the buffer with its caret,
    /// selection and history, the saved comparison the dirty answer derives from, the line
    /// convention, the epoch, and the viewport. Owned by nobody else in this process.
    EditorState e_;

    /// WHERE THIS RUN BEGAN, as the host said it -- what a relative spelling means. Empty
    /// until answered, and empty for a run that began nowhere.
    std::string project_dir_;
    bool root_asked_ = false;
    std::uint64_t root_pending_ = 0;

    component::Clipboard clip_;
    struct Paste {
        bool awaiting = false;
        std::uint64_t pending = 0;
        std::uint64_t doc = 0;      ///< the document epoch that asked (WL-EDIT-11)
        std::uint64_t revision = 0; ///< ...and exactly where it stood
    };
    Paste paste_;

    /// THE LAST THING THIS PANE HAD TO SAY -- a refusal, or what an act came to -- standing
    /// until the maker's next act, on the row the composition budgets for it.
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

    std::int64_t rows_ = 0;
    std::int64_t columns_ = 0;
    bool granted_ = false;
};

} // namespace

ZEN_EXPORT_WEAVE(EditorPaneWeave)
