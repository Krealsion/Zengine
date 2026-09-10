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
// SAME-SHAPE RELOAD carries the document across (`EditorPaneState`; `mirror_state`/`revive`
// below), so the one lifecycle act a maker can perform on this image keeps their work. And
// an ORDERLY QUIT asks this weave before it stops the bus (`PaneQuitRequested`), so dirty
// source refuses the exit exactly as it did when the host could read it. What is NOT
// claimed is what was never claimed: process death still loses drafts.
//
// ⚠ WHAT MOVED ACROSS A MESSAGE BOUNDARY, AND WHAT DID NOT. Save stayed one synchronous
// call inside this weave: the write, then the saved comparison, with no delivery between
// them, so a success is about the bytes that were written and can never mark a later edit
// clean. What became a round trip is the PASTE (the Skin answers later; the answer is pinned
// to the document epoch and the buffer revision it was asked for, as the host pinned it),
// the QUIT (the host asks and waits; this weave answers about the instant of the answer, and
// refuses while a paste is still arriving, because a permission a queued message could
// falsify is not one) -- and the OPEN, which is one transaction across a boundary rather
// than one call: read and judge, hold every gesture that could change what was judged, ask
// the desk to seat this pane, and install on the desk's word that it did. The desk's delivery
// of that ask is the commitment point (`pane_vocabulary.hpp` says why): a refusal at any
// step -- a missing file, refused bytes, a dirty document, a screen with no seat -- leaves the
// document that was open, its caret, its history and the desk exactly as they were, and
// travels back to whoever asked as the answer to their request.

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
#include <filesystem>
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
using ws::PaneCaret;
using ws::PaneCatalogRequested;
using ws::PaneContent;
using ws::PaneDragged;
using ws::PaneKey;
using ws::PaneOffered;
using ws::PanePressed;
using ws::PaneQuitAnswered;
using ws::PaneQuitRequested;
using ws::PaneRevealAnswered;
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

/// THE ANSWER A QUIT ASK GETS WHILE AN OPEN IS STILL BEING SEATED. The gestures held for that
/// open may carry edits nobody has applied yet, and a permission a queued message could
/// falsify is not one -- the paste's rule, one operation over.
constexpr const char* kOpenInFlight = "the Editor is still opening a source -- quit again";

// =============================================================================

class EditorPaneWeave
    : public loom::WeaveBase<
          EditorPaneWeave, pane::EditorPaneState,
          loom::Accept<loom::Activated, PaneCatalogRequested, PaneRoom, PanePressed,
                       PaneDragged, PaneKey, PaneTextInput, PaneWheel, PaneActionRequested,
                       PaneQuitRequested, PaneRevealAnswered, OpenSourceRequested, ProjectRoot,
                       surface::ClipboardCopy, surface::ClipboardText>,
          loom::Emit<PaneOffered, ws::v2::PaneActions, PaneContent, PaneCaret,
                     PaneRevealRequested, PaneQuitAnswered, SourceOpened,
                     ProjectRootRequested, surface::ClipboardCopy,
                     surface::ClipboardTextRequested>> {
    /// ONE GESTURE HELD WHILE THE DESK IS DECIDING -- a press, a drag, the wheel, a key, text,
    /// a declared action, or the Skin's clipboard answer, whichever arrived, kept whole so it
    /// can be replayed exactly, in order, into whichever document the flight leaves open.
    /// Workshop's `HeldInput`, one seam over, for the same reason.
    struct Held {
        enum class Kind : std::uint8_t {
            kPressed,
            kDragged,
            kWheel,
            kKey,
            kText,
            kAction,
            kClipboard
        };
        Kind kind = Kind::kKey;
        PanePressed pressed;
        PaneDragged dragged;
        PaneWheel wheel;
        PaneKey key;
        PaneTextInput text;
        PaneActionRequested action;
        surface::ClipboardText clipboard;
    };

    /// ONE ACQUISITION IN FLIGHT: the candidate bytes, the ask the desk is answering, the
    /// requester's answer taken away from the delivery that asked, and the gestures held
    /// meanwhile (WL-EDIT-05). Not a document: nothing reads it, paints it or edits it.
    ///
    /// ⚠ THE HELD GESTURES ARE WHAT MAKES THE DESK'S DELIVERY THE COMMITMENT POINT. From the
    /// moment the ask is sent until the answer arrives, nothing that could change this weave's
    /// eligibility is applied -- so what was judged when the ask was sent is still true when
    /// the desk seats the pane, and the seat needs no second judgement. Bounded like
    /// Workshop's own hold: a burst past the bound is dropped and counted, and the count is
    /// said with the outcome rather than swallowed.
    struct Pending {
        bool live = false;
        bool same_path = false;
        /// Whether a `zengine.workshop` holder took the ask -- and therefore whether the
        /// desk's answer is what ends this transaction.
        bool asked_desk = false;
        std::uint64_t reveal = 0;
        std::string path;
        ws::SourceIn admitted;
        loom::DeferredAnswer answer;
        std::vector<Held> held;
        std::size_t dropped = 0;
    };

    /// HOW MANY GESTURES ONE OPEN WILL HOLD -- Workshop's own bound for its quit ask, for its
    /// reason: the exchange is one drain of the bus, so what arrives during it is one poll's
    /// burst at most.
    static constexpr std::size_t kMaxHeldGestures = 256;

    /// THE SWEEP A PRESS BEGAN, and the picture it began against (WL-EDIT-16).
    struct Drag {
        bool armed = false;
        std::int64_t chrome_rows = 0;
        std::int64_t doc_rows = 0;
    };

public:
    using Base = loom::WeaveBase<
        EditorPaneWeave, pane::EditorPaneState,
        loom::Accept<loom::Activated, PaneCatalogRequested, PaneRoom, PanePressed, PaneDragged,
                     PaneKey, PaneTextInput, PaneWheel, PaneActionRequested, PaneQuitRequested,
                     PaneRevealAnswered, OpenSourceRequested, ProjectRoot,
                     surface::ClipboardCopy, surface::ClipboardText>,
        loom::Emit<PaneOffered, ws::v2::PaneActions, PaneContent, PaneCaret,
                   PaneRevealRequested, PaneQuitAnswered, SourceOpened,
                   ProjectRootRequested, surface::ClipboardCopy,
                   surface::ClipboardTextRequested>>;

    // ---- The state a reload carries, and the surface a poke reads ----------------------

    /// ⚠ THERE IS NO `snapshot()` HERE, AND ITS ABSENCE IS THE POINT (VD-26). This pane used
    /// to build the shape from the live buffer at the moment Loom asked and leave `state_`
    /// untouched -- two truths, of which Loom reads the WRONG one for `zen.PokeRead`: the
    /// poke doors are answered off `state_` before any handler runs, so a pane holding an
    /// unsaved document answered `path` and `text` with empty strings, and a reloaded one
    /// answered with the snapshot it revived from. `state_` is now written from the live
    /// document at every composition (`mirror_state`), so Loom's own `snapshot()` is right
    /// by construction and the read surface cannot drift from what the maker is looking at.
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
        // ⭐ THE FACT A RELATIVE SPELLING TURNS ON (VD-26). An owner that has not answered
        // and an owner that authoritatively named no project are different, and only the
        // second one is permission to spell a relative path against the process. This flag
        // is what tells them apart; before it, an unanswered door left `project_dir_` empty
        // and a relative request went to the filesystem to mean whatever the process
        // directory happened to hold.
        project_known_ = true;
        mirror_state();
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
    /// ⚠ AND THE PRESENTATION IS PART OF THE TRANSACTION. An open that ends in a pane nobody
    /// can see is not an open, so this weave READS AND JUDGES the file, ASKS the desk to seat
    /// it, and INSTALLS on the desk's word that it did. The desk's delivery of that ask is the
    /// commitment point (`pane_vocabulary.hpp`, the reveal): from the ask's send to the answer
    /// this weave HOLDS every gesture and clipboard answer that could change what it judged,
    /// so nothing has to be judged again at the answer, and a refusal at any step -- a missing
    /// file, bytes the law refuses, a dirty document, a screen with no seat -- leaves the
    /// prior document, its caret, its history and the authored setup exactly as they were,
    /// and travels back to whoever asked as the answer to their own request.
    ///
    /// THE ANSWER IS THEREFORE DEFERRED, because the desk answers on a later delivery. What
    /// is held in the meantime is a CANDIDATE and never a second document: nothing about it
    /// is readable, paintable or editable.
    void on(const OpenSourceRequested& asked, loom::Mail& mail) {
        if (mail.authored_role().empty()) {
            return;
        }
        if (open_.live) {
            // ONE ACQUISITION AT A TIME. Two in flight would be two candidates racing for
            // one document with one deferred answer each; the second is refused in words a
            // maker can act on, and the first is still on its way to the desk.
            (void)mail.answer(SourceOpened{
                false, "the Editor is still opening " + open_.path + " -- try again once it "
                                                                     "has"});
            return;
        }
        Plan plan = judge_source(asked.path);
        if (!plan.outcome.accepted) {
            (void)mail.answer(SourceOpened{false, plan.outcome.refusal});
            say(mail);
            return;
        }
        Pending flight;
        flight.same_path = plan.same_path;
        flight.path = std::move(plan.path);
        flight.admitted = std::move(plan.admitted);
        flight.answer = mail.defer_answer();
        flight.reveal = ++asked_;
        const loom::Ticket queued =
            mail.as_role(pane::kEditorPaneRole)
                .send_to_role(kWorkshopRole, PaneRevealRequested{pane::kEditorPane},
                              flight.reveal);
        if (!queued.valid()) {
            // NOTHING TOOK THE ASK -- no `zengine.workshop` holder is there to answer, so
            // there is no presentation to be part of this transaction and the document is
            // installed now. The pane's own room, if it ever gets one, paints it.
            //
            // ⚠ AND A VALID TICKET IS NOT THE OTHER HALF OF THAT. It says the send was
            // authorized and queued, never that a recipient resolved or that delivery
            // happened; a host that accepts the shape and answers nothing leaves the flight
            // outstanding, and the next request is refused in words rather than lost. That
            // residue is named in WL-EDIT-05 and belongs to sender fate.
            settle(std::move(flight), true, std::string(), mail);
            return;
        }
        flight.asked_desk = true;
        flight.live = true;
        open_ = std::move(flight);
    }

    /// THE DESK'S WORD ON THE ASK: seated -- selected, with the keys, in the delivery that
    /// answered -- or refused with nothing moved. Nothing is judged again here: what this
    /// weave judged before it asked is still true, because everything that could have changed
    /// it has been held since (`Pending::held`).
    void on(const PaneRevealAnswered& said, loom::Mail& mail) {
        if (!mail.answers_ask() || !open_.live || mail.correlation() != open_.reveal) {
            return; // somebody else's answer, or one to a flight this pane already settled
        }
        Pending flight = std::move(open_);
        open_ = Pending{};
        settle(std::move(flight), said.seated, said.refusal, mail);
    }

    // ---- The pointer ---------------------------------------------------------------------

    /// A PRESS NAMES A ROW OF THIS PANE'S ROOM. The rows above the document -- the status
    /// row, a standing notice -- are consumed as a focus statement and move nothing; a row
    /// of the document places the caret through the same tab geometry the row was painted
    /// with (WL-EDIT-08), at `first_col + column` of the whole line, which is the one
    /// subtraction a horizontal viewport adds to a hit test.
    ///
    /// ⚠ AND IT DECIDES WHETHER A SWEEP IS UNDER WAY (VD-26, WL-EDIT-16). Workshop takes
    /// hold of this pane for the length of the button whenever a press named ANY row of the
    /// body -- it owns physical routing and does not read this pane's rows to learn what
    /// they mean -- so the motions of a focus-only press arrive here exactly as a real
    /// sweep's do. What tells them apart is this: a `PaneDragged` extends the gesture a
    /// press began, and a press this pane consumed as focus began none. Without it, a press
    /// on the status row followed by a drag into the document extended a selection from
    /// wherever the caret had been left.
    ///
    /// ⚠ AND IT LEAVES THE NOTICE ROW ALONE, WHICH IS GEOMETRY AND NOT MANNERS. Clearing a
    /// standing notice moves the document up one row; a press and the motions that follow it
    /// were measured by the maker's hand against ONE picture, and the poll that delivers
    /// them can deliver the motion after the press. A pointer gesture therefore changes no
    /// row this pane composes -- the notice stands until the maker's next ACT -- and the
    /// gesture also remembers the chrome it began against, so nothing else that reflows can
    /// move its meaning either.
    void on(const PanePressed& press, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || press.pane != pane::kEditorPane) {
            return;
        }
        if (holding()) {
            Held h;
            h.kind = Held::Kind::kPressed;
            h.pressed = press;
            hold(std::move(h));
            return;
        }
        apply_press(press, mail);
    }

    void apply_press(const PanePressed& press, loom::Mail& mail) {
        drag_ = Drag{};
        if (!e_.open_document() || press.row < chrome_rows_) {
            return;
        }
        drag_.armed = true;
        drag_.chrome_rows = chrome_rows_;
        drag_.doc_rows = doc_rows_;
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
        if (holding()) {
            Held h;
            h.kind = Held::Kind::kDragged;
            h.dragged = drag;
            hold(std::move(h));
            return;
        }
        apply_drag(drag, mail);
    }

    void apply_drag(const PaneDragged& drag, loom::Mail& mail) {
        if (!e_.open_document() || !drag_.armed) {
            return; // no gesture to extend: this hand took hold of nothing that selects
        }
        // THE PICTURE THE GESTURE BEGAN AGAINST, not the one composed since: the maker's
        // hand measured this motion against the rows they could see when they pressed.
        const std::int64_t brow = drag.row - drag_.chrome_rows;
        std::size_t target;
        if (brow < 0) {
            target = e_.first_row > 0 ? e_.first_row - 1 : 0;
        } else if (brow >= drag_.doc_rows) {
            target = e_.first_row + static_cast<std::size_t>(drag_.doc_rows);
        } else {
            target = e_.first_row + static_cast<std::size_t>(brow);
        }
        e_.buffer.drag_to(target, drag.column < 0 ? std::int64_t{-1} : e_.first_col + drag.column);
        e_.follow_caret = true;
        say(mail);
    }

    /// THE WHEEL SCROLLS THE DOCUMENT AND MOVES NO CARET (WL-PTR-10): the notches accumulate
    /// until they are worth whole lines, the window slides inside the document, and the
    /// follow flag is deliberately NOT set -- the wheel's whole meaning is to look elsewhere
    /// while the caret stays put. The next caret gesture brings the view back.
    void on(const PaneWheel& wheel, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || wheel.pane != pane::kEditorPane) {
            return;
        }
        if (holding()) {
            Held h;
            h.kind = Held::Kind::kWheel;
            h.wheel = wheel;
            hold(std::move(h));
            return;
        }
        apply_wheel(wheel, mail);
    }

    void apply_wheel(const PaneWheel& wheel, loom::Mail& mail) {
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
        if (holding()) {
            Held h;
            h.kind = Held::Kind::kKey;
            h.key = key;
            hold(std::move(h));
            return;
        }
        apply_key(key, mail);
    }

    void apply_key(const PaneKey& key, loom::Mail& mail) {
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
        if (holding()) {
            Held h;
            h.kind = Held::Kind::kText;
            h.text = typed;
            hold(std::move(h));
            return;
        }
        apply_text(typed, mail);
    }

    void apply_text(const PaneTextInput& typed, loom::Mail& mail) {
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
        if (holding()) {
            Held h;
            h.kind = Held::Kind::kAction;
            h.action = asked;
            hold(std::move(h));
            return;
        }
        apply_action(asked, mail);
    }

    void apply_action(const PaneActionRequested& asked, loom::Mail& mail) {
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
    /// document after this one was given, and so does an open still being seated, because the
    /// gestures it holds may carry edits; a clean document, or no document, permits. No input
    /// can reach this weave between this answer and the host's decision (the host holds every
    /// gesture while it waits), which is what makes "clean" a fact rather than a race.
    void on(const PaneQuitRequested&, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole)) {
            return;
        }
        if (open_.live) {
            (void)mail.answer(PaneQuitAnswered{pane::kEditorPane, false, kOpenInFlight});
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
        // THE ANSWER IS THIS PANE'S (Loom said so, and the correlation says which ask), so
        // it is no longer awaited -- but while an open is being seated it is HELD like any
        // other gesture, because it could dirty the document that was judged. Replayed after
        // the outcome, it meets the epoch it pinned: gone if the open took, there if it did not.
        if (holding()) {
            Held h;
            h.kind = Held::Kind::kClipboard;
            h.clipboard = a;
            hold(std::move(h));
            return;
        }
        apply_clipboard(a, mail);
    }

    void apply_clipboard(const surface::ClipboardText& a, loom::Mail& mail) {
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
        // ⚠ THE SECOND VERSION OF THE DECLARATION, because this pane owns one of Workshop's
        // actions and version one has no field to say so (VD-27). Every pane that owns
        // nothing keeps declaring version one, unchanged and unrebuilt.
        ws::v2::PaneActions actions;
        actions.pane = pane::kEditorPane;
        const auto row = [&actions](const char* id, const char* label, std::int64_t sc,
                                    std::int64_t mods, const char* stands_for = "") {
            actions.rows.push_back(ws::v2::PaneActionRow{id, label, sc, mods, stands_for});
        };
        // ⭐ AND THE SAVE ROW SAYS WHAT IT STANDS IN FOR (VD-26, WL-KEY-15). This pane holds a
        // document of its own, so while its keys are the maker's, `document.save` is not the
        // operation they are asking for -- and saying so by NAME is what keeps that true when
        // a maker moves either row's key. It is also what lets this row keep `ctrl+s`: the
        // two are one meaning in two scopes, not two meanings on one gesture.
        row(pane::kActionSave, "save source", input::scan::kS, input::mod::kCtrl,
            ws::kOwnableDocumentSave);
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

    /// WHAT THIS SPELLING MEANS HERE, or why it means nothing (WL-EDIT-06). An absolute
    /// path is itself under every condition. A relative one is the PROJECT'S file, and the
    /// project is a fact this pane is told: until the owner has answered, a relative
    /// spelling has no meaning here and is refused with the reason, because resolving it
    /// against the process directory would open a different file that happens to share a
    /// name -- silently, and then save to it. An owner that authoritatively named NO project
    /// root is a different answer, and its policy is the one it always was: the spelling is
    /// spent as the maker wrote it.
    Written resolve(const std::string& requested, std::string& out) const {
        if (!requested.empty() && !std::filesystem::path(requested).is_absolute() &&
            !project_known_) {
            return Written::no(requested +
                               " is a relative path and this Editor has not been told where "
                               "this run began -- open it by its full path");
        }
        out = ws::persist::resolved_against(project_dir_, requested);
        return Written::ok();
    }

    /// WHAT AN OPEN WOULD COME TO, JUDGED WITH NOTHING MOVED (WL-EDIT-05). The candidate it
    /// carries is bytes and a name; it is not a document until `commit_source` says so.
    struct Plan {
        Written outcome = Written::ok();
        bool same_path = false;
        std::string path;
        ws::SourceIn admitted;
    };

    Plan judge_source(const std::string& requested) {
        Plan plan;
        const Written meaning = resolve(requested, plan.path);
        if (!meaning.accepted) {
            plan.outcome = meaning;
            return plan;
        }
        if (e_.open_document() && e_.path == plan.path) {
            // RE-REQUESTING THE OPEN SOURCE REVEALS IT AND DESTROYS NOTHING: the buffer,
            // its caret, its selection, its history and its viewport all stand; what
            // moves is presence and the keyboard, and those are the host's to move.
            plan.same_path = true;
            return plan;
        }
        if (e_.dirty()) {
            // THE UNSAVED-LOSS FLOOR: a different source must not silently replace a dirty
            // buffer. The two ways out are this pane's own save and its one deliberate
            // discard, named by their actions; the band spells their keys while the pane
            // holds the keyboard.
            plan.outcome =
                Written::no(e_.path + " has unsaved changes -- save source or discard "
                                      "source edits in the Editor first; nothing was opened");
            return plan;
        }
        // READ AND JUDGE BEFORE ANYTHING MOVES: a refused file costs the asker its refusal
        // and nothing else -- the current document (if any) and the file itself are exactly
        // as they were.
        const ws::persist::FileText read =
            ws::persist::read_file(plan.path, ws::kMaxSourceBytes, "a source file");
        if (!read.outcome.accepted) {
            plan.outcome = read.outcome;
            return plan;
        }
        plan.admitted = ws::source_in(read.text);
        if (!plan.admitted.outcome.accepted) {
            plan.outcome = Written::no(plan.path + ": " + plan.admitted.outcome.refusal);
        }
        return plan;
    }

    /// THE INSTALL, ON THE DESK'S WORD (WL-EDIT-05). The desk answered on a later delivery,
    /// and nothing that could have changed what was judged was applied in between -- every
    /// gesture was held -- so the dirty question below is a belt and not a judgement: a `no`
    /// here would mean the hold leaked, not that the maker typed.
    Written commit_source(Pending& flight) {
        if (flight.same_path) {
            if (!e_.open_document() || e_.path != flight.path) {
                return Written::no(flight.path +
                                   " is no longer the open source -- ask for it again");
            }
            // ⚠ AND NOTHING ABOUT THE VIEW MOVES (VD-27). Re-requesting the open source is a
            // REVEAL: the buffer, its caret, its selection, its history AND the place the
            // maker had scrolled to all stand, because none of them is what the request was
            // about. Following the caret here threw away a scrolled viewport (first row six
            // back to zero with the caret on row zero); so does letting the notice this sets
            // count as a resize, which is why `reconcile` measures the granted room.
            notice(e_.dirty() ? "UNSAVED edits stand -- editing " + shown_path()
                              : "editing " + shown_path(),
                   false);
            return Written::ok();
        }
        if (e_.open_document() && e_.path == flight.path) {
            notice("editing " + shown_path(), false);
            return Written::ok();
        }
        if (e_.dirty()) {
            return Written::no(e_.path + " has unsaved changes -- save source or discard "
                                         "source edits in the Editor first; nothing was opened");
        }
        install(flight.path, std::move(flight.admitted));
        notice("editing " + shown_path(), false);
        return Written::ok();
    }

    /// END ONE ACQUISITION on the desk's word: install (or reveal) if it seated the pane,
    /// refuse if it did not, answer whoever asked for the source, repaint, and then give the
    /// maker's hands back what they did meanwhile -- in order, into whichever document is open
    /// now. The one place a deferred `SourceOpened` is spent, so a flight cannot end twice or
    /// not at all.
    ///
    /// ⚠ NOTHING IS JUDGED AGAIN HERE. The desk seated, selected and focused the pane in the
    /// delivery that answered -- that delivery was the commitment -- and this weave's own
    /// eligibility could not have changed since the ask, because every gesture that could
    /// change it was held. A seat the desk reported and a document this weave then refused
    /// would be a defect in the hold, and is said as one where the maker reads.
    void settle(Pending flight, bool seated, const std::string& refusal, loom::Mail& mail) {
        const Written done =
            seated ? commit_source(flight)
                   : Written::no(refusal.empty() ? std::string("the Editor could not be "
                                                               "shown; nothing was opened")
                                                 : refusal);
        if (seated && !done.accepted) {
            notice(done.refusal, true);
        }
        if (flight.answer.valid()) {
            (void)loom::answer_deferred(flight.answer, mail,
                                        SourceOpened{done.accepted, done.refusal});
        }
        say(mail);
        replay_held(std::move(flight.held), flight.dropped, mail);
    }

    /// IS A DESK DECIDING ABOUT THIS PANE RIGHT NOW? While it is, the gestures below are held.
    bool holding() const noexcept { return open_.live && open_.asked_desk; }

    /// HOLD ONE GESTURE FOR THE FLIGHT'S END -- or, past the bound, drop it and count it.
    void hold(Held h) {
        if (open_.held.size() >= kMaxHeldGestures) {
            ++open_.dropped;
            return;
        }
        open_.held.push_back(std::move(h));
    }

    /// PLAY THE HELD GESTURES BACK, IN ORDER, through the same bodies they would have reached,
    /// into whichever document the flight left open -- the new one on a seat, the old one on a
    /// refusal. A burst past the bound was dropped and counted, and the count is said where
    /// the maker reads, beside what the open came to.
    void replay_held(std::vector<Held> held, std::size_t dropped, loom::Mail& mail) {
        for (const Held& h : held) {
            switch (h.kind) {
            case Held::Kind::kPressed: apply_press(h.pressed, mail); break;
            case Held::Kind::kDragged: apply_drag(h.dragged, mail); break;
            case Held::Kind::kWheel: apply_wheel(h.wheel, mail); break;
            case Held::Kind::kKey: apply_key(h.key, mail); break;
            case Held::Kind::kText: apply_text(h.text, mail); break;
            case Held::Kind::kAction: apply_action(h.action, mail); break;
            case Held::Kind::kClipboard: apply_clipboard(h.clipboard, mail); break;
            }
        }
        if (dropped > 0) {
            notice(notice_ + " (" + std::to_string(dropped) +
                       " gesture(s) that arrived while the source was opening were dropped)",
                   true);
            say(mail);
        }
    }

    /// PUT AN ADMITTED DOCUMENT IN PLACE: identity, bytes, saved copy, convention, a new
    /// epoch, and a fresh viewport. The one writer of `e_.path`.
    void install(const std::string& path, ws::SourceIn admitted) {
        e_.path = path;
        e_.saved_lines = admitted.lines;
        ++saved_stamp_;
        e_.buffer.set_lines(std::move(admitted.lines));
        e_.convention = admitted.convention;
        ++e_.doc_epoch;
        e_.first_row = 0;
        e_.first_col = 0;
        e_.wheel_accum = 0.0;
        e_.follow_caret = true;
        // ⭐ AND THE PASTE THE OLD DOCUMENT ASKED FOR IS RETIRED WITH IT (VD-26). Its answer
        // could never have landed -- the epoch it pinned is gone, and `on(ClipboardText)`
        // discards it -- but the flag outlived the document it was about, and the quit
        // answer reads that flag: a clean new document refused every exit for the rest of
        // the session because a paste nobody could still spend had been asked for. Pending
        // context retires with its subject.
        drag_ = Drag{};
        paste_ = Paste{};
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
        // WHERE THIS RUN BEGAN, AND THE LAST THING THIS PANE SAID: both are the picture the
        // maker was looking at, and both come back before the document does, because the
        // notice is a ROW and the room the document gets is what is left under it.
        project_dir_ = state_.project_dir;
        project_known_ = state_.project_known;
        notice_ = state_.notice;
        notice_bad_ = state_.notice_bad;
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
        e_.convention = state_.convention;
        e_.doc_epoch = static_cast<std::uint64_t>(state_.doc_epoch);
        e_.buffer.restore_selection(as_index(state_.anchor_row), as_index(state_.anchor_byte),
                                    as_index(state_.caret_row), as_index(state_.caret_byte));
        e_.first_row = as_index(state_.first_row);
        e_.first_col = state_.first_col < 0 ? 0 : state_.first_col;
        e_.wheel_accum = 0.0;
        e_.follow_caret = false;
        // ⭐ THE ROOM THE DOCUMENT WAS LAST LOOKED AT THROUGH, CARRIED (VD-26). Zeroing these
        // made the first grant after a revival differ from the last room before it, which is
        // exactly what `reconcile` calls a resize -- so an unchanged room pulled the viewport
        // back to the caret and a maker who had scrolled somewhere lost the place they were
        // reading. A genuinely different room still resizes, because these are the numbers it
        // is compared against.
        e_.last_rows = state_.last_rows;
        e_.last_cols = state_.last_cols;
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
    /// the offsets always, follow the caret when a gesture asked or THE GRANTED ROOM changed,
    /// and deliberately not after the wheel.
    ///
    /// ⚠ THE GRANTED ROOM, AND NOT THE ROWS THE DOCUMENT WAS LEFT (VD-27). A notice appearing
    /// or clearing changes the second and not the first, and a maker who scrolled somewhere
    /// to read did not ask to be taken back to the caret because this pane had something to
    /// say. A genuine resize still follows, because that is what these two numbers are.
    void reconcile(std::int64_t rows_in, std::int64_t text_cols) {
        const bool resized = rows_ != e_.last_rows || columns_ != e_.last_cols;
        e_.last_rows = rows_;
        e_.last_cols = columns_;
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
            // NO ROOM HAS BEEN SENT: there is nothing this pane could truthfully fill -- but
            // the document is real whether or not anybody is showing it, and the read
            // surface says so.
            mirror_state();
            return;
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
        mirror_state();
    }

    /// THE LIVE DOCUMENT, WRITTEN INTO THE SHAPE LOOM ANSWERS READS FROM (VD-26). Called
    /// wherever this pane finishes an act, which is one call per delivery and not one per
    /// field written.
    ///
    /// THE TWO EXPENSIVE FIELDS ARE GATED ON WHAT ACTUALLY MOVED: the buffer's own revision
    /// for `text`, a stamp bumped by the three writers of `saved_lines` for `saved_text`. A
    /// press, a drag, the wheel, a resize, a focus change and a room grant therefore rebuild
    /// neither. `vocabulary.hpp` carries the cost this leaves and why there is no cheaper
    /// shape of it.
    void mirror_state() {
        state_.notice = notice_;
        state_.notice_bad = notice_bad_;
        state_.project_dir = project_dir_;
        state_.project_known = project_known_;
        state_.last_rows = e_.last_rows;
        state_.last_cols = e_.last_cols;
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
            mirrored_content_ = 0;
            mirrored_saved_ = 0;
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
        // ⚠ THE BYTES' OWN REVISION, NOT THE BUFFER'S (VD-27). `revision()` moves when the
        // CARET moves, because a pending paste has to notice that; keying the mirror on it
        // rebuilt a four-megabyte string on an arrow key, a press and every motion of a
        // drag, with every byte identical. `content_revision()` moves when the lines do.
        if (mirrored_content_ != e_.buffer.content_revision()) {
            state_.text = ws::source_text(e_.buffer.lines(), e_.convention);
            mirrored_content_ = e_.buffer.content_revision();
            ++state_.text_builds; // the cost, counted where it is paid
        }
        if (mirrored_saved_ != saved_stamp_) {
            state_.saved_text = ws::source_text(e_.saved_lines, e_.convention);
            mirrored_saved_ = saved_stamp_;
        }
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

    Pending open_;
    Drag drag_;

    /// WHERE THIS RUN BEGAN, as the host said it -- what a relative spelling means. Empty
    /// until answered, and empty for a run that began nowhere; `project_known_` is which of
    /// those two an empty string is (WL-EDIT-06).
    std::string project_dir_;
    bool project_known_ = false;
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

    /// WHAT THE MIRRORED SHAPE WAS BUILT FROM: the buffer revision `text` was joined at, and
    /// the stamp `saved_text` was. Bumped by the three writers of `saved_lines` (install,
    /// save, revival), so a rebuild happens when the bytes moved and at no other time.
    std::uint64_t saved_stamp_ = 0;
    std::uint64_t mirrored_content_ = 0;
    std::uint64_t mirrored_saved_ = 0;

    std::int64_t rows_ = 0;
    std::int64_t columns_ = 0;
    bool granted_ = false;
};

} // namespace

ZEN_EXPORT_WEAVE(EditorPaneWeave)
