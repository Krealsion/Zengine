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
// (!) THE DOCUMENT CAME WITH IT, AND THAT IS THE DIFFERENCE FROM EVERY EARLIER MIGRATION.
// Files left the project root with the host, the Builder left the tool, the Terminal left the
// participant; each pane presents a subject somebody else owns and asks about it. The
// Editor's subject is the buffer a maker types into, and a pane that presented a buffer the
// host still owned would cross the seam twice per keystroke and hold a second mutable copy
// of the same bytes. So this weave is the one custodian: the buffer machinery
// (`editor.hpp`) came here whole, the file is read and written from here, and what crosses
// is a source request, a preparation, a quit answer, rows and a caret. What the host keeps is
// room, focus, membership and the exit DECISION -- which it now makes by asking.
//
// (!) A REPLACEABLE PANE IS THE CUSTODIAN OF UNSAVED WORK, DELIBERATELY, and three things make
// that a design rather than an accident. Presentation and custody are two lifetimes here
// exactly as they were: hiding, covering, moving, reordering or removing the PANE touches
// this weave not at all, because Workshop closes a presentation and sends no unload. A
// SAME-SHAPE RELOAD carries the document across (`EditorPaneState`; `mirror_state`/`revive`
// below), so the one lifecycle act a maker can perform on this image keeps their work. And
// an ORDERLY QUIT asks this weave before it stops the bus (`PaneQuitRequested`), so dirty
// source refuses the exit exactly as it did when the host could read it. What is NOT
// claimed is what was never claimed: process death still loses drafts.
//
// (!) WHAT MOVED ACROSS A MESSAGE BOUNDARY, AND WHAT DID NOT. Save stayed one synchronous
// call inside this weave: the write, then the saved comparison, with no delivery between
// them, so a success is about the bytes that were written and can never mark a later edit
// clean. What became a round trip is the PASTE (the Skin answers later; the answer is pinned
// to the document epoch and the buffer revision it was asked for, as the host pinned it),
// the QUIT (the host asks and waits; this weave answers about the instant of the answer, and
// refuses while a paste is still arriving, because a permission a queued message could
// falsify is not one) -- and the OPEN, which has TWO doors now:
//
//   THE OLD DOOR, `OpenSourceRequested` at `zengine.editor`, WITH ITS PROMISE KEPT (editor-
//   managed-open-slice-corrections): the document opened AND shown, or a truthful refusal --
//   what every requester was promised before an opening manager existed. This weave does
//   not arrange the desk, so it RELAYS: it keeps the requester's answer right, asks the
//   opening manager in a conversation of its own, and answers the requester with the
//   manager's outcome through the kept right (its own correlation and attempt match the
//   manager's answer or the bus's refusal of the attempt; the requester's right is spent
//   only by this weave). It never installs a document without the presentation, and a
//   relay that cannot be made is refused in words. The opening office may not ask it.
//
//   THE MANAGED DOOR (WL-OPEN-01): a requester asks the opening
//   manager (`zengine.opening`), which asks THIS weave to PREPARE the source for the room the
//   desk's trial would grant (`PrepareSourceRequested`). This weave judges and admits the
//   file, builds a WHOLE candidate document beside the current one, composes it for that
//   room, OFFERS the candidate's identity as the next value of its own latest claim
//   (`EditorDocument`) for that exact operation, and answers with the composition. The
//   current document stays current, readable and editable throughout: every edit to it moves
//   its claim, which aborts the operation at the bus before it could commit. When the manager
//   commits, the bus exchanges this weave's claim and the desk's in ONE protected step, and
//   shows this weave its published claim (`on_claim_published`) BEFORE its next delivery or
//   snapshot -- that is where the candidate becomes the document, and why a read queued
//   behind the commitment can never see the old one. A refusal at any step leaves the
//   document, its caret, its history, the desk and the keys exactly as they were, and the
//   candidate is dropped. Nothing is held, replayed or rolled back.

#include "editor-pane/vocabulary.hpp"

#include "editor-pane/editor.hpp"
#include "source-transfer/cpp.hpp"
#include "source-transfer/material.hpp"
#include "workshop/editor_handoff_vocabulary.hpp"
#include "workshop/editor_switch_vocabulary.hpp"
#include "workshop/open_seam_vocabulary.hpp"
#include "workshop/pane_carry.hpp"
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

#include <zen/kernel/export.hpp>
#include <zen/weave.hpp>
#include <zen/weave/lifecycle.hpp>
#include <zen/weave/standard_shapes.hpp>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

namespace component = zengine::component;
namespace input = zengine::input;
namespace surface = zengine::surface;
namespace ws = zengine::workshop;
namespace pane = zengine::editor_pane;
namespace st = zengine::source_transfer;

using ws::EditorAdopted;
using ws::EditorAdoptRequested;
using ws::EditorBuffer;
using ws::EditorDocument;
using ws::EditorHandoffEnded;
using ws::EditorHandoffJudged;
using ws::EditorHandoffJudgeRequested;
using ws::EditorHandoffOffered;
using ws::EditorHandoffRequested;
using ws::EditorLive;
using ws::EditorLiveRequested;
using ws::EditorPreparationTick;
using ws::EditorRetired;
using ws::EditorRetireRequested;
using ws::EditorTransfer;
using ws::EditorWarmed;
using ws::EditorWarmRequested;
using ws::EditorPos;
using ws::EditorState;
using ws::ManagedOpenProgress;
using ws::ManagedOpenSettled;
using ws::OpenSourceRequested;
using ws::PaneActionRequested;
using ws::PaneCatalogRequested;
using ws::PaneDragged;
using ws::PaneKey;
using ws::v2::PaneOffered;
using ws::PanePressed;
using ws::PaneQuitAnswered;
using ws::PaneQuitRequested;
using ws::PaneRoom;
using ws::PaneTextInput;
using ws::PaneWheel;
using ws::PrepareSourceRequested;
using ws::ProjectRoot;
using ws::ProjectRootRequested;
using ws::SourceOpened;
using ws::SourcePrepared;
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
                       ws::v3::PanePressed, PaneDragged, PaneKey, PaneTextInput, PaneWheel,
                       ws::PaneButton, PaneActionRequested, PaneQuitRequested, OpenSourceRequested,
                       PrepareSourceRequested, ManagedOpenProgress, ManagedOpenSettled,
                       SourceOpened, loom::DispatchRefused, ProjectRoot, surface::ClipboardCopy,
                       surface::ClipboardText, EditorHandoffJudgeRequested, EditorWarmRequested,
                       EditorPreparationTick, EditorHandoffRequested, EditorHandoffEnded,
                       EditorAdoptRequested, EditorLiveRequested, EditorRetireRequested,
                       ws::PaneValueDrop, ws::PaneMenuAnswered, ws::PaneOperationAnswered,
                       ws::PaneCarryAnswered>,
          loom::Emit<PaneOffered, ws::v2::PaneActions, ws::v3::PaneContent, ws::v2::PaneCaret,
                     PaneQuitAnswered, SourceOpened, SourcePrepared, OpenSourceRequested,
                     ProjectRootRequested, surface::ClipboardCopy,
                     surface::ClipboardTextRequested, EditorHandoffJudged, EditorWarmed,
                     EditorHandoffOffered, EditorAdopted, EditorLive, EditorRetired,
                     ws::PaneMenuRequested, ws::PaneOperationRequested,
                     ws::PaneValueCarryRequested>,
          loom::Claims<EditorDocument>> {
    /// ONE PREPARED CANDIDATE: the whole document a managed opening would install, built
    /// beside the current one for one exact operation, and the identity this weave OFFERED
    /// for it. Not a document: nothing reads it, paints it or edits it. It becomes the
    /// document only in `on_claim_published`, when the bus has published that identity.
    ///
    /// (!) NOTHING IS HELD FOR IT. Input to the current document applies to the current
    /// document, immediately and in order; an edit moves this weave's claim, and the bus
    /// aborts the operation whose offer was made against the previous revision. A refused,
    /// superseded or aborted operation drops the candidate and nothing else.
    struct Candidate {
        bool live = false;
        std::uint64_t op = 0;
        bool same_path = false; ///< the current document is the one asked for: only the desk moves
        std::string path;
        EditorState doc;        ///< the whole candidate (bytes, saved copy, convention, epoch)
        /// THE ROOM IT WAS COMPOSED FOR, AND THE GEOMETRY OF THAT COMPOSITION: the trial
        /// room the desk named, which is the room the pane has the instant the publication
        /// seats it -- before the desk's own room grant, which follows and agrees.
        std::int64_t rows = 0;
        std::int64_t columns = 0;
        std::int64_t chrome_rows = 0;
        std::int64_t doc_rows = 0;
    };

    /// THE SWEEP A PRESS BEGAN, and the picture it began against (WL-EDIT-16).
    struct Drag {
        bool armed = false;
        std::int64_t chrome_rows = 0;
        std::int64_t doc_rows = 0;
    };

    /// THE WINDOW THE DOCUMENT IS LOOKED AT THROUGH, as one value `reconcile` moves -- so a
    /// composition can be made for a room WITHOUT moving the live view (a candidate's, or a
    /// re-request's, which moves the pane and never the view: WL-EDIT-09).
    struct Viewport {
        std::size_t first_row = 0;
        std::int64_t first_col = 0;
        bool follow_caret = false;
        std::int64_t last_rows = 0;
        std::int64_t last_cols = 0;
    };

    /// WHAT COMPOSING A DOCUMENT FOR A ROOM PRODUCED: the rows, where the document begins in
    /// them, how many rows it got, the caret beside them, and the viewport it was made with.
    struct Composition {
        std::vector<surface::SurfaceTextRow> rows;
        std::int64_t chrome_rows = 0;
        std::int64_t doc_rows = 0;
        bool status_row = false; ///< row 0 is the status row (not a notice standing in for it)
        ws::v2::PaneCaret caret;
        Viewport view;
    };

    // ---- What a transfer holds, between the gestures and the answers (WL-EDIT-17..21) -------

    /// WHAT A HAND IS TAKING: the selection's text, or this file's location.
    enum class Take : std::uint8_t { Selection, Location };

    /// THE DOCUMENT AT ONE INSTANT (`mark_now`): equal marks mean nothing moved in between.
    struct Mark {
        std::uint64_t epoch = 0;
        std::uint64_t content = 0;
        std::uint64_t revision = 0;
        friend bool operator==(const Mark& a, const Mark& b) {
            return a.epoch == b.epoch && a.content == b.content && a.revision == b.revision;
        }
    };

    /// A PRESS ON THE HIGHLIGHT OR ON THE STATUS ROW, remembered until the hand moves to another
    /// cell (then it is a carry) or the next act comes (then it was only ever a press).
    struct Grab {
        bool armed = false;
        bool started = false;
        Take what = Take::Selection;
        std::int64_t row = 0;
        std::int64_t column = 0;
        std::uint64_t gesture = 0; ///< the press's own correlation, echoed on the approval
        Mark at;                   ///< the document when it was pressed
        EditorPos anchor;          ///< Selection: the selection the press landed on...
        EditorPos caret;
        Mark after;                ///< ...and the document once the press had placed the caret
    };

    /// ONE ACQUISITION IN FLIGHT: Workshop's approval for the gesture, then the carry itself.
    struct Pickup {
        enum class Stage : std::uint8_t { Idle, Permission, Carry };
        Stage stage = Stage::Idle;
        std::uint64_t ask = 0;
        std::uint64_t gesture = 0;
        bool drag = false;
        loom::Bytes bytes;
        std::string label;
        std::string what;
        loom::Ticket ticket{};
    };

    /// WHERE A DROP LANDED: the character under it, and whether that is on the highlight.
    struct Landing {
        EditorPos pos;
        bool inside = false;
    };

    /// A DROPPED COMMAND IN A C++ DOCUMENT, WAITING FOR THE MAKER'S CHOICE.
    struct Dropped {
        bool pending = false;
        st::Material material;
        Landing at;
        Mark mark;
        ws::pane_menu::Asked menu;
    };

    /// A DROPPED LOCATION: Workshop's approval for the gesture, then the managed open.
    struct Locate {
        enum class Stage : std::uint8_t { Idle, Permission, Opening };
        Stage stage = Stage::Idle;
        std::uint64_t ask = 0;
        loom::Ticket ticket{};
        st::SourceLocation loc;
        std::optional<st::SourceLocationContext> ctx;
        bool same_path = false;
        Mark mark;
    };

    /// THE PICTURE'S ROW-TO-MEANING MAP, as the facts it is made of (WL-EDIT-18).
    struct PictureKey {
        std::uint64_t epoch = 0;
        std::uint64_t content = 0;
        std::size_t first_row = 0;
        std::int64_t first_col = 0;
        std::int64_t chrome_rows = 0;
        std::int64_t rows = 0;
        std::int64_t columns = 0;
        bool selection = false;
        EditorPos from;
        EditorPos to;
        friend bool operator==(const PictureKey& a, const PictureKey& b) {
            return a.epoch == b.epoch && a.content == b.content && a.first_row == b.first_row &&
                   a.first_col == b.first_col && a.chrome_rows == b.chrome_rows && a.rows == b.rows &&
                   a.columns == b.columns && a.selection == b.selection && a.from == b.from && a.to == b.to;
        }
    };

    static constexpr const char* kDropSubject = "drop";
    static constexpr const char* kInsertLine = "editor.insert-line";
    static constexpr const char* kInsertCpp = "editor.insert-cpp";

public:
    using Base = loom::WeaveBase<
        EditorPaneWeave, pane::EditorPaneState,
        loom::Accept<loom::Activated, PaneCatalogRequested, PaneRoom, PanePressed,
                     ws::v3::PanePressed, PaneDragged, PaneKey, PaneTextInput, PaneWheel,
                     ws::PaneButton, PaneActionRequested, PaneQuitRequested, OpenSourceRequested,
                     PrepareSourceRequested, ManagedOpenProgress, ManagedOpenSettled,
                     SourceOpened, loom::DispatchRefused, ProjectRoot, surface::ClipboardCopy,
                     surface::ClipboardText, EditorHandoffJudgeRequested, EditorWarmRequested,
                     EditorPreparationTick, EditorHandoffRequested, EditorHandoffEnded,
                     EditorAdoptRequested, EditorLiveRequested, EditorRetireRequested,
                     ws::PaneValueDrop, ws::PaneMenuAnswered, ws::PaneOperationAnswered,
                     ws::PaneCarryAnswered>,
        loom::Emit<PaneOffered, ws::v2::PaneActions, ws::v3::PaneContent, ws::v2::PaneCaret,
                   PaneQuitAnswered, SourceOpened, SourcePrepared, OpenSourceRequested,
                   ProjectRootRequested, surface::ClipboardCopy, surface::ClipboardTextRequested,
                   EditorHandoffJudged, EditorWarmed, EditorHandoffOffered, EditorAdopted,
                   EditorLive, EditorRetired, ws::PaneMenuRequested, ws::PaneOperationRequested,
                   ws::PaneValueCarryRequested>,
        loom::Claims<EditorDocument>>;

    // ---- The state a reload carries, and the surface a poke reads ----------------------

    /// (!) THERE IS NO `snapshot()` HERE, AND ITS ABSENCE IS THE POINT (VD-26). This pane used
    /// to build the shape from the live buffer at the moment Loom asked and leave `state_`
    /// untouched -- two truths, of which Loom reads the WRONG one for `zen.PokeRead`: the
    /// poke doors are answered off `state_` before any handler runs, so a pane holding an
    /// unsaved document answered `path` and `text` with empty strings, and a reloaded one
    /// answered with the snapshot it revived from. `state_` is now written from the live
    /// document at the end of every delivery (`after_delivery` -> `mirror_state`) and again
    /// inside the publication hook, so Loom's own `snapshot()` is right by construction and
    /// the read surface cannot drift from what the maker is looking at.
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
        // (*) THE FACT A RELATIVE SPELLING TURNS ON (VD-26). An owner that has not answered
        // and an owner that authoritatively named no project are different, and only the
        // second one is permission to spell a relative path against the process. This flag
        // is what tells them apart; before it, an unanswered door left `project_dir_` empty
        // and a relative request went to the filesystem to mean whatever the process
        // directory happened to hold.
        project_known_ = true;
    }

    // ---- THE TWO DOORS: open a source directly, or prepare it for a managed opening ------

    /// THE OLD DOOR, AND ITS PROMISE KEPT (`OpenSourceRequested` at `zengine.editor`;
    /// WL-EDIT-05, WL-OPEN-07). This is the address every requester used
    /// before an opening manager existed, and what it promised was the document AND its
    /// presentation -- seated, selected, holding the keys -- or a truthful refusal. It
    /// still promises exactly that. This weave does not arrange the desk; the opening
    /// manager does; so the ask is RELAYED: the requester's answer right is kept here
    /// (Loom's deferred right, bound to this incarnation), the manager is asked in a
    /// conversation of THIS weave's own (its own correlation, its own attempt), and the
    /// manager's outcome is what the requester is answered with, through the kept right.
    /// The manager's answer authenticates the relay's conversation and nothing else: it is
    /// matched to this weave's own ask and spent into the requester's right, never treated
    /// as an answer to the requester. There is no document-only install behind this door
    /// and no fallback to one: a relay that cannot be made is a refusal, in words, now.
    ///
    /// AN OFFICE, AND ONLY AN OFFICE, the host doors' rule; and NOT THE OPENING OFFICE. The
    /// manager asks this weave to PREPARE a source (`PrepareSourceRequested`); a holder of
    /// that office asking this door would be asking this weave to ask it back, and the
    /// loop is refused by name rather than started.
    void on(const OpenSourceRequested& asked, loom::Mail& mail) {
        if (mail.authored_role().empty()) {
            return;
        }
        if (mail.authored_from_role(ws::kOpeningRole)) {
            (void)mail.answer(SourceOpened{
                false, "the opening office asks the Editor to prepare a source, not to open "
                       "one -- nothing was relayed for " +
                           asked.path});
            return;
        }
        if (holding_.active) {
            ++holding_.refused;
            (void)mail.answer(SourceOpened{false, "the Editor is being switched -- " + asked.path +
                                                      " was not opened; open it again once the "
                                                      "switch has settled"});
            return;
        }
        if (relays_.size() >= kMaxRelays) {
            (void)mail.answer(SourceOpened{
                false, "too many opens are already being relayed through the Editor -- " +
                           asked.path + " was not opened; try again"});
            return;
        }
        const std::uint64_t correlation = ++asked_;
        const loom::Ticket attempt =
            mail.as_role(pane::kEditorPaneRole)
                .send_to_role(ws::kOpeningRole, OpenSourceRequested{asked.path}, correlation);
        if (!attempt.valid()) {
            // NOTHING WAS QUEUED -- this weave could not author the ask as its own office.
            // Refused now, in words: never silence, and never a document-only success
            // standing in for the presentation the requester asked for.
            (void)mail.answer(SourceOpened{
                false, "nothing was queued: the Editor could not ask the opening office to show " +
                           asked.path});
            return;
        }
        Relay relay;
        relay.path = asked.path;
        relay.correlation = correlation;
        relay.attempt = attempt;
        relay.answer = mail.defer_answer();
        relays_.push_back(std::move(relay));
    }

    /// THE MANAGER'S ANSWER TO A RELAY: Loom's word that this answers an ask of THIS weave,
    /// matched to the relay by this weave's own correlation, and spent into the requester's
    /// kept right. An answer to nothing this weave asked is dropped; a right whose requester
    /// was replaced meanwhile spends into nobody, which the bus refuses and this weave
    /// does not pretend otherwise.
    void on(const SourceOpened& said, loom::Mail& mail) {
        if (!mail.answers_ask()) {
            return;
        }
        if (locate_.stage == Locate::Stage::Opening && mail.correlation() == locate_.ask) {
            settle_location(said);
            say(mail);
            return;
        }
        for (auto it = relays_.begin(); it != relays_.end(); ++it) {
            if (it->correlation == mail.correlation()) {
                (void)loom::answer_deferred(it->answer, mail,
                                            SourceOpened{said.accepted, said.refusal});
                relays_.erase(it);
                return;
            }
        }
    }

    /// THE BUS'S WORD THAT A RELAY'S ATTEMPT WAS REFUSED BEFORE ANY HANDLER RAN -- no
    /// opening office is held, its holder does not accept the ask, or it is held behind a
    /// claim it could not apply. Matched by EXACT ATTEMPT (the provenance is the fact,
    /// the shape alone is speech), and the requester is told, in words.
    void on(const loom::DispatchRefused& refused, loom::Mail& mail) {
        if (!mail.dispatch_refused()) {
            return;
        }
        const loom::Ticket attempt = refused.refused_attempt();
        if (!attempt.valid()) {
            return;
        }
        for (auto it = relays_.begin(); it != relays_.end(); ++it) {
            if (it->attempt.seq == attempt.seq) {
                (void)loom::answer_deferred(
                    it->answer, mail,
                    SourceOpened{false, std::string(ws::kOpeningRole) + " could not be reached (" +
                                            refused.reason + ") -- " + it->path +
                                            " was not opened"});
                relays_.erase(it);
                return;
            }
        }
        // A PICKUP OR A DROPPED LOCATION'S ASK THAT NEVER ARRIVED, named by its exact attempt: known
        // failure, said, and the record released. Delivered silence stays pending.
        if (pickup_.stage != Pickup::Stage::Idle && pickup_.ticket.valid() &&
            attempt.seq == pickup_.ticket.seq) {
            pickup_ = Pickup{};
            notice("nothing was carried -- Workshop could not be asked (" + refused.reason + ")", true);
            say(mail);
            return;
        }
        if (locate_.stage != Locate::Stage::Idle && locate_.ticket.valid() &&
            attempt.seq == locate_.ticket.seq) {
            const std::string path = locate_.loc.path;
            locate_ = Locate{};
            notice("nothing was opened -- " + path + " could not be asked for (" + refused.reason + ")", true);
            say(mail);
        }
    }

    /// THE `apply` WORD: the delivery in which the
    /// bus showed this weave its published claim, before this handler -- the hook below
    /// did the work, and the end of this delivery says the rows. Nothing to do here.
    void on(const ManagedOpenProgress& said, loom::Mail& mail) {
        (void)said;
        (void)mail;
    }

    /// THE MANAGED DOOR (WL-OPEN-01, WL-OPEN-03): PREPARE this source for the room the desk's trial
    /// would grant, and OFFER the document's identity for the exact operation. Judged with
    /// nothing moved, exactly as the direct door judges; the candidate is a whole document
    /// built beside the current one, composed for `rows` x `columns`, and the composition
    /// travels back so the desk can admit it as the trial's content. Nothing here is
    /// readable, paintable or editable, and the current document stays all three.
    ///
    /// (!) THE OFFER CAN BE REFUSED BY THE BUS -- the operation is not this weave's, or the
    /// document's claim already moved since the operation bound it -- and a refused offer
    /// is answered as one: no candidate is kept for an operation that cannot commit.
    void on(const PrepareSourceRequested& asked, loom::Mail& mail) {
        if (!mail.authored_from_role(ws::kOpeningRole)) {
            return;
        }
        if (holding_.active) {
            ++holding_.refused;
            (void)mail.answer(not_prepared(asked.op, "the Editor is being switched -- " + asked.path +
                                                         " was not prepared; open it again once the "
                                                         "switch has settled"));
            return;
        }
        candidate_ = Candidate{}; // a newer preparation supersedes an older candidate
        const std::uint64_t op = static_cast<std::uint64_t>(asked.op);
        Plan plan = judge_source(asked.path);
        if (!plan.outcome.accepted) {
            (void)mail.answer(not_prepared(asked.op, plan.outcome.refusal));
            return;
        }
        Candidate next;
        next.live = true;
        next.op = op;
        next.path = plan.path;
        next.same_path = plan.same_path;
        EditorDocument offered;
        Composition shown;
        if (plan.same_path) {
            // THE DOCUMENT IS THIS ONE; what the operation changes is the desk. Composed for
            // the trial room on a COPY of the view, because asking for the open source again
            // moves the pane and never the view (WL-EDIT-09).
            offered = identity_now();
            offered.opened_by = asked.op;
            shown = compose(e_, viewport_of(e_), asked.rows, asked.columns,
                            (e_.dirty() ? "UNSAVED edits stand -- editing " : "editing ") +
                                tail_of_path(e_.path, asked.columns > 24 ? asked.columns - 24
                                                                            : asked.columns),
                            false);
        } else {
            next.doc.path = plan.path;
            next.doc.saved_lines = plan.admitted.lines;
            next.doc.buffer.set_lines(std::move(plan.admitted.lines));
            next.doc.convention = plan.admitted.convention;
            next.doc.doc_epoch = e_.doc_epoch + 1;
            next.doc.follow_caret = true;
            offered.path = next.doc.path;
            offered.doc_epoch = static_cast<std::int64_t>(next.doc.doc_epoch);
            offered.convention = next.doc.convention;
            offered.content_revision =
                static_cast<std::int64_t>(next.doc.buffer.content_revision());
            offered.dirty = false;
            offered.opened_by = asked.op;
            shown = compose(next.doc, viewport_of(next.doc), asked.rows, asked.columns,
                            "editing " + tail_of_path(next.doc.path,
                                                      asked.columns > 24 ? asked.columns - 24
                                                                         : asked.columns),
                            false);
            // The candidate keeps the viewport it was composed with, so what it shows at
            // activation is what the desk admitted.
            apply_viewport(next.doc, shown.view);
        }
        next.rows = asked.rows;
        next.columns = asked.columns;
        next.chrome_rows = shown.chrome_rows;
        next.doc_rows = shown.doc_rows;
        const loom::JointResult offer = mail.offer(op, offered);
        if (!offer.ok) {
            (void)mail.answer(not_prepared(asked.op, offer_refusal(plan.path, offer.why)));
            return;
        }
        candidate_ = std::move(next);
        SourcePrepared prepared;
        prepared.op = asked.op;
        prepared.ok = true;
        prepared.generation = offered.doc_epoch;
        prepared.rows = std::move(shown.rows);
        prepared.caret_row = shown.caret.row;
        prepared.caret_col = shown.caret.column;
        prepared.sel_begin_row = shown.caret.sel_begin_row;
        prepared.sel_begin_col = shown.caret.sel_begin_col;
        prepared.sel_end_row = shown.caret.sel_end_row;
        prepared.sel_end_col = shown.caret.sel_end_col;
        (void)mail.answer(prepared);
    }

    /// THE OPERATION ENDED, said by the manager AFTER the fact. On a commitment this weave
    /// was already shown its published claim before this delivery (the hook below), so the
    /// candidate is already the document; on a refusal the candidate is dropped and the
    /// reason stands where the maker reads. Neither is the commitment.
    void on(const ManagedOpenSettled& said, loom::Mail& mail) {
        if (!mail.authored_from_role(ws::kOpeningRole)) {
            return;
        }
        if (!said.committed) {
            if (candidate_.live && candidate_.op == static_cast<std::uint64_t>(said.op)) {
                candidate_ = Candidate{};
            }
            if (!said.refusal.empty()) {
                notice(said.refusal, true);
            }
        } else if (!said.applied && !said.refusal.empty()) {
            // PUBLISHED, AND AN OWNER COULD NOT APPLY IT. This weave applied its half (a held
            // weave would not be receiving this); the other owner is named in the sentence.
            notice(said.refusal, true);
        }
        say(mail);
    }

    /// THE HOOK (WL-OPEN-02; Loom's `Weave::claim_published`): the bus published THIS
    /// weave's document claim by a joint operation, and this weave has not run since. Called
    /// before the next delivery and before the next snapshot, with no Mail: the candidate the
    /// published identity names becomes the document HERE, and the mirror is rebuilt HERE,
    /// so a poke, a snapshot or a message queued behind the commitment finds the new document
    /// and never the old one. Rows are said at the end of the next delivery.
    ///
    /// (!) AN IDENTITY THIS INCARNATION DID NOT PREPARE IS NOT INSTALLED, AND IS ANSWERED SO.
    /// The operation bound the incarnation that prepared the candidate; the candidate does
    /// not ride a reload; so a successor reloaded over a predecessor that could not apply
    /// the publication is shown it with no candidate to install. The honest answer is to
    /// keep what this weave holds, say so, re-claim that truth at the end of the next
    /// delivery -- never to fabricate a document from an identity -- and to ANSWER `false`
    ///: the bus records Declined against this
    /// participant and publication, holds nothing, and the manager records "not applied
    /// after repair". A successor that survives with A is a repaired owner and not an
    /// applied operation; returning normally from this branch used to say the opposite.
    bool on_claim_published(const EditorDocument& published) {
#ifdef ZENGINE_EDITOR_TEST_THROW_ON_B_CPP
        // TEST INSTRUMENTATION, COMPILED ONLY INTO `zengine-editor-throwing` (tests/CMakeLists.txt):
        // this exact source plus one deliberate throw
        // before anything is activated, for a published path ending in `/b.cpp` and for nothing
        // else -- a real owner whose image cannot complete a showing. The normal image never
        // defines this and compiles none of it.
        if (published.path.size() >= 6 &&
            published.path.compare(published.path.size() - 6, 6, "/b.cpp") == 0) {
            throw std::runtime_error("test instrumentation: the Editor could not apply " +
                                     published.path);
        }
#endif
        // WHAT THE BUS HOLDS UNDER THIS WEAVE'S KEY IS `published` NOW, whichever branch
        // follows: the identity the end of the next delivery compares the document against,
        // so an activated candidate (which derives exactly it) claims nothing again and a
        // kept document (which differs) claims its own truth.
        claimed_ = published;
        claimed_ever_ = true;
        if (candidate_.live && published.opened_by == static_cast<std::int64_t>(candidate_.op)) {
            activate();
            mirror_state();
            resay_ = true;
            return true;
        }
        // A PUBLICATION THIS INCARNATION DID NOT PREPARE -- a successor reloaded over a
        // predecessor that could not apply it. The
        // document this weave holds is kept and re-claimed at the end of the next delivery.
        // Its GENERATION moves past the published one first: the desk keeps the rows it
        // admitted for the publication and drops any projection of an older generation, so
        // rows said for this document under its old epoch would never repaint the desk.
        // A paste pinned to the old epoch retires with it, as at any install. And the
        // answer is DECLINED: not applied, not
        // broken.
        const std::uint64_t published_epoch =
            published.doc_epoch < 0 ? 0 : static_cast<std::uint64_t>(published.doc_epoch);
        e_.doc_epoch = std::max(e_.doc_epoch, published_epoch) + 1;
        paste_ = Paste{};
        notice("the desk published " + published.path +
                   " for an opening this Editor did not prepare -- " +
                   (e_.open_document() ? "keeping " + shown_path() : "no source is open"),
               true);
        mirror_state();
        resay_ = true;
        return false;
    }

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

    // ---- A switch: this Editor as the incumbent (WL-SWITCH) ---------------------------------

    /// WHAT WOULD A SWITCH AWAY COST? Nothing the transfer cannot carry -- the standard model IS
    /// the transfer's model -- so there are no losses to consent to; the undo history and the
    /// wheel's fraction are reset and said. A state no switch may begin in is refused in words.
    // WL-SWITCH-04 -- agents/workshop/editor-switch.md
    void on(const EditorHandoffJudgeRequested& asked, loom::Mail& mail) {
        if (!mail.authored_from_role(ws::kEditorSwitchRole)) {
            return;
        }
        EditorHandoffJudged judged;
        judged.op = asked.op;
        judged.refusal = handoff_refusal();
        judged.ok = judged.refusal.empty();
        if (judged.ok) {
            judged.resets = handoff_resets();
            judged.losses = handoff_losses();
            judged.digest = ws::handoff_digest(judged.losses);
            judged.rows = rows_;
            judged.columns = columns_;
            judged.project_dir = project_dir_;
            judged.project_known = project_known_;
        }
        (void)mail.answer(judged);
    }

    /// THE BOUNDARY: the exact document, and this Editor holds still until it hears the outcome.
    // WL-SWITCH-04 -- agents/workshop/editor-switch.md
    void on(const EditorHandoffRequested& asked, loom::Mail& mail) {
        if (!mail.authored_from_role(ws::kEditorSwitchRole)) {
            return;
        }
        EditorHandoffOffered offered;
        offered.op = asked.op;
        offered.refusal = handoff_refusal();
        if (!offered.refusal.empty()) {
            (void)mail.answer(offered);
            return;
        }
        holding_ = Holding{true, asked.op, 0};
        offered.ok = true;
        offered.losses = handoff_losses();
        offered.digest = ws::handoff_digest(offered.losses);
        offered.transfer = transfer_now();
        offered.resets = handoff_resets();
        (void)mail.answer(offered);
    }

    /// NOTHING MOVED: this Editor is still the Editor. It stops holding still and says what it
    /// refused meanwhile.
    void on(const EditorHandoffEnded& ended, loom::Mail& mail) {
        if (!mail.authored_from_role(ws::kEditorSwitchRole) || !holding_.active ||
            holding_.op != ended.op) {
            return;
        }
        const std::int64_t refused = holding_.refused;
        holding_ = Holding{};
        std::string said = "the editor switch did not happen";
        if (!ended.why.empty()) {
            said += " (" + ended.why + ")";
        }
        if (refused > 0) {
            said += " -- " + std::to_string(refused) +
                    (refused == 1 ? " input was" : " inputs were") +
                    " not applied while it was being prepared";
        }
        notice(said, refused > 0);
        say(mail);
    }

    /// RETIRED: say what was refused while holding still. The coordinator unloads this image next.
    void on(const EditorRetireRequested& asked, loom::Mail& mail) {
        if (!holding_.active || holding_.op != asked.op) {
            return;
        }
        (void)mail.answer(EditorRetired{asked.op, holding_.refused});
    }

    // ---- A switch: this Editor as the candidate, sealed --------------------------------------

    /// READY AT ONCE: the standard Editor needs nothing started. What it keeps is the room it
    /// will be seated in and where the project began.
    void on(const EditorWarmRequested& asked, loom::Mail& mail) {
        if (activation_.activated()) {
            return; // a live Editor is not a candidate
        }
#ifdef ZENGINE_EDITOR_TEST_SILENT_WARM
        // TEST INSTRUMENTATION, COMPILED ONLY INTO `zengine-editor-pane-silent`: a candidate that
        // keeps its answer right and never spends it, so a switch waits on it for as long as a
        // case likes. The normal image never defines this.
        silent_warm_ = mail.defer_answer();
        return;
#endif
        rows_ = asked.rows;
        columns_ = asked.columns;
        project_dir_ = asked.project_dir;
        project_known_ = asked.project_known;
        (void)mail.answer(EditorWarmed{asked.op, true, std::string(), "the standard Editor"});
    }

    void on(const EditorPreparationTick&, loom::Mail&) {}

    /// ADOPT THE DOCUMENT, OR SAY EXACTLY WHY NOT. The bytes meet the law a file meets
    /// (`source_in`), so a document this editor cannot carry truthfully is refused here, while
    /// nothing has moved, naming the line; the caret and anchor are put back clamped, and the
    /// generation goes past the incumbent's so no row it said can repaint this one.
    // WL-SWITCH-05 -- agents/workshop/editor-switch.md
    void on(const EditorAdoptRequested& asked, loom::Mail& mail) {
        if (activation_.activated()) {
            return;
        }
        EditorAdopted adopted;
        adopted.op = asked.op;
#ifdef ZENGINE_EDITOR_TEST_REFUSE_ADOPT
        // TEST INSTRUMENTATION, COMPILED ONLY INTO `zengine-editor-pane-refusing`: a candidate that
        // refuses every document, so a case sees a refusal at adoption with the incumbent holding
        // still. The normal image never defines this.
        adopted.refusal = "test instrumentation: this image refuses every adoption";
        (void)mail.answer(adopted);
        return;
#endif
        const EditorTransfer& t = asked.transfer;
        project_dir_ = t.project_dir;
        project_known_ = t.project_known;
        if (t.path.empty()) {
            e_ = EditorState{};
            e_.doc_epoch = t.doc_epoch < 0 ? 1 : static_cast<std::uint64_t>(t.doc_epoch) + 1;
            adopted_ = true;
            adopted.ready = true;
            (void)mail.answer(adopted);
            return;
        }
        ws::SourceIn text = ws::source_in(t.text);
        if (!text.outcome.accepted) {
            adopted.refusal = "the standard Editor cannot carry " + t.path + ": " + text.outcome.refusal;
            (void)mail.answer(adopted);
            return;
        }
        ws::SourceIn saved = ws::source_in(t.saved_text);
        if (!saved.outcome.accepted) {
            adopted.refusal = "the standard Editor cannot carry the saved copy of " + t.path + ": " +
                              saved.outcome.refusal;
            (void)mail.answer(adopted);
            return;
        }
        if (t.text.size() > ws::kMaxSourceBytes) {
            adopted.refusal = t.path + " is larger than the standard Editor opens";
            (void)mail.answer(adopted);
            return;
        }
        EditorState next;
        next.path = t.path;
        next.saved_lines = std::move(saved.lines);
        next.buffer.set_lines(std::move(text.lines));
        next.convention = text.convention;
        next.doc_epoch = t.doc_epoch < 0 ? 1 : static_cast<std::uint64_t>(t.doc_epoch) + 1;
        next.buffer.restore_selection(as_index(t.anchor_row), as_index(t.anchor_byte),
                                      as_index(t.caret_row), as_index(t.caret_byte));
        next.first_row = as_index(t.first_row);
        next.first_col = t.first_col < 0 ? 0 : t.first_col;
        next.follow_caret = true;
        if (next.buffer.caret_row() != as_index(t.caret_row) ||
            next.buffer.caret_byte() != as_index(t.caret_byte) ||
            next.buffer.anchor_row() != as_index(t.anchor_row) ||
            next.buffer.anchor_byte() != as_index(t.anchor_byte)) {
            adopted.notes.push_back("the caret or selection named a place the document does not "
                                    "have, and was clamped into it");
        }
        if (next.dirty() != t.modified) {
            adopted.notes.push_back(t.modified ? "the document was marked modified and matches its "
                                                 "saved copy, so it is shown as saved"
                                               : "the document differs from its saved copy, so it "
                                                 "is shown as unsaved");
        }
        e_ = std::move(next);
        ++saved_stamp_;
        adopted_ = true;
        adopted.ready = true;
        (void)mail.answer(adopted);
    }

    /// SERVING? True once this Editor's admission activated it.
    void on(const EditorLiveRequested& asked, loom::Mail& mail) {
        if (!mail.authored_from_role(ws::kEditorSwitchRole)) {
            return;
        }
#ifdef ZENGINE_EDITOR_TEST_NOT_LIVE
        // TEST INSTRUMENTATION, COMPILED ONLY INTO `zengine-editor-pane-not-live`: a successor that
        // holds the office and says it does not serve, so a case sees a failure after the
        // commitment. The normal image never defines this.
        (void)mail.answer(EditorLive{asked.op, false, "test instrumentation: this image never serves"});
        return;
#endif
        (void)mail.answer(EditorLive{asked.op, activation_.activated(),
                                     activation_.activated()
                                         ? "the standard Editor holds " +
                                               (e_.open_document() ? e_.path : std::string("no source"))
                                         : "the standard Editor was not activated"});
    }

    // ---- The pointer ---------------------------------------------------------------------

    /// A PRESS NAMES A ROW OF THIS PANE'S ROOM. The rows above the document -- the status
    /// row, a standing notice -- are consumed as a focus statement and move nothing; a row
    /// of the document places the caret through the same tab geometry the row was painted
    /// with (WL-EDIT-08), at `first_col + column` of the whole line, which is the one
    /// subtraction a horizontal viewport adds to a hit test.
    ///
    /// (!) AND IT DECIDES WHETHER A SWEEP IS UNDER WAY (VD-26, WL-EDIT-16). Workshop takes
    /// hold of this pane for the length of the button whenever a press named ANY row of the
    /// body -- it owns physical routing and does not read this pane's rows to learn what
    /// they mean -- so the motions of a focus-only press arrive here exactly as a real
    /// sweep's do. What tells them apart is this: a `PaneDragged` extends the gesture a
    /// press began, and a press this pane consumed as focus began none. Without it, a press
    /// on the status row followed by a drag into the document extended a selection from
    /// wherever the caret had been left.
    ///
    /// (!) AND IT LEAVES THE NOTICE ROW ALONE, WHICH IS GEOMETRY AND NOT MANNERS. Clearing a
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
        press_at(press.row, press.column, -1, mail); // an unnumbered press holds nothing
    }

    /// ...AND THE PRESS THAT NAMES ITS PICTURE (`v3::PanePressed`), which may mean one thing more
    /// once the hand moves: a press ON the painted highlight is an ordinary press that remembers
    /// the selection it landed on, and a press on the status row remembers this file's location
    /// (WL-EDIT-19). A press aimed at an older picture remembers nothing.
    void on(const ws::v3::PanePressed& press, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || press.pane != pane::kEditorPane) {
            return;
        }
        press_at(press.row, press.column, press.picture, mail);
    }

    // WL-EDIT-19 -- agents/workshop/editor-transfers.md
    void press_at(std::int64_t prow, std::int64_t pcol, std::int64_t picture, loom::Mail& mail) {
        if (held_still()) {
            return;
        }
        grab_ = Grab{};
        drag_ = Drag{};
        if (!e_.open_document()) {
            return;
        }
        if (prow < chrome_rows_) {
            // STILL A FOCUS STATEMENT THAT MOVES NOTHING: the status row names this file, so a
            // press there is remembered in case the hand moves on, and changes no row.
            if (prow == 0 && status_row_) {
                grab_ = Grab{true, false, Take::Location, prow, pcol, mail.correlation(), mark_now(), {}, {}, {}};
            }
            return;
        }
        const std::size_t row = e_.first_row + static_cast<std::size_t>(prow - chrome_rows_);
        const std::size_t target =
            row < e_.buffer.line_count() ? row : e_.buffer.line_count() - 1;
        const std::int64_t column = pcol < 0 ? 0 : pcol;
        const EditorPos at{target, ws::byte_of_visual_col(e_.buffer.line(target), e_.first_col + column)};
        if (picture >= 0 && picture == picture_ && row < e_.buffer.line_count() && on_highlight(at)) {
            grab_ = Grab{true, false, Take::Selection, prow, pcol, mail.correlation(), mark_now(),
                         EditorPos{e_.buffer.anchor_row(), e_.buffer.anchor_byte()},
                         EditorPos{e_.buffer.caret_row(), e_.buffer.caret_byte()}, {}};
        }
        drag_.armed = true;
        drag_.chrome_rows = chrome_rows_;
        drag_.doc_rows = doc_rows_;
        e_.buffer.place(at.row, at.byte);
        e_.follow_caret = true;
        grab_.after = mark_now();
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
        if (held_still()) {
            return;
        }
        // A REMEMBERED PRESS BECOMES A CARRY ON ITS FIRST MOTION TO ANOTHER CELL, and never a
        // sweep (WL-EDIT-19): the pressed selection is put back exactly as it stood -- nothing but
        // the press's own caret moved since, or nothing is carried -- and a copy of it is taken.
        if (grab_.armed) {
            if (!grab_.started && (drag.row != grab_.row || drag.column != grab_.column)) {
                grab_.started = true;
                if (grab_.what == Take::Location) {
                    acquire(Take::Location, true, grab_.gesture, grab_.at, mail);
                } else if (e_.open_document() && grab_.after == mark_now()) {
                    e_.buffer.restore_selection(grab_.anchor.row, grab_.anchor.byte, grab_.caret.row,
                                                grab_.caret.byte);
                    acquire(Take::Selection, true, grab_.gesture, mark_now(), mail);
                } else {
                    notice("nothing was carried -- the document changed after you pressed the highlight", true);
                }
                say(mail);
            }
            return;
        }
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
        if (held_still()) {
            return;
        }
        if (!e_.open_document()) {
            return;
        }
        end_grab();
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

    /// TEXT THE MAKER TYPED INTO THE SOURCE, gated by the source-byte law at this door
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

    /// THE FOUR ROWS THIS PANE DECLARED, acted on by NAME (WL-KEY-15): save, newline, tab,
    /// discard. A maker's override moved the key; the id is what arrives.
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

    // ---- Transfers: the highlight carried out, material dropped in, a location opened ----
    //
    // The pane's half of Workshop's value carry (`workshop/pane_carry.hpp`): Workshop carries owned
    // bytes and interprets none of them; this pane decides what a drop means for its document
    // (`source-transfer/material.hpp`), and asks Workshop to approve every acquisition and every
    // open for the gesture that caused it. Nothing here saves, builds, sends or runs.

    /// THE SECOND BUTTON, OFFERED ONLY FOR SELECTED MATERIAL: a right press on the painted
    /// highlight offers Extract; on the status row, this file's location. Anywhere else it is
    /// silence -- the body a right press met before this pane took the door (WL-EDIT-19).
    void on(const ws::PaneButton& b, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || b.pane != pane::kEditorPane || !b.pressed ||
            b.lost || b.button != 3) {
            return;
        }
        if (held_still()) {
            return;
        }
        end_grab();
        if (!e_.open_document()) {
            return;
        }
        if (b.row < chrome_rows_) {
            if (b.row == 0 && status_row_) {
                offer(Take::Location, b.row, b.column, mail);
            }
            return;
        }
        const std::size_t row = e_.first_row + static_cast<std::size_t>(b.row - chrome_rows_);
        if (b.picture == picture_ && row < e_.buffer.line_count() &&
            on_highlight(EditorPos{row, ws::byte_of_visual_col(e_.buffer.line(row),
                                                              e_.first_col + (b.column < 0 ? 0 : b.column))})) {
            offer(Take::Selection, b.row, b.column, mail);
        }
    }

    /// MATERIAL DROPPED ON THE DOCUMENT (WL-EDIT-17): text is inserted where it landed, or replaces
    /// the selection when it landed ON the painted highlight; a location opens; a command is its
    /// Terminal line, or -- in a C++ document, by a separate choice -- C++. One undo takes any
    /// insertion back; nothing is saved. Refused material leaves the document, its selection and
    /// its history exactly as they were.
    void on(const ws::PaneValueDrop& drop, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || drop.pane != pane::kEditorPane) {
            return;
        }
        if (held_still()) {
            return;
        }
        end_grab();
        notice_.clear();
        receive(drop, mail);
        say(mail);
    }

    void on(const ws::PaneMenuAnswered& answer, loom::Mail& mail) {
        if (drop_.pending && drop_.menu.pending() && answer.subject == kDropSubject) {
            const std::string chosen = drop_.menu.take(mail, answer);
            if (drop_.menu.pending()) {
                return; // not settled: not this ask's answer, or from nobody who may give it
            }
            Dropped d = std::move(drop_);
            drop_ = Dropped{};
            choose_drop(d, chosen);
            say(mail);
            return;
        }
        const std::string chosen = menu_.take(mail, answer);
        if (chosen == pane::kActionExtract || chosen == pane::kActionLocation) {
            acquire(chosen == pane::kActionExtract ? Take::Selection : Take::Location, false,
                    mail.correlation(), menu_mark_, mail);
            say(mail);
        }
    }

    /// WORKSHOP'S WORD ON WHETHER THIS GESTURE'S ACTOR MAY DO WHAT THIS PANE ASKED: carry a copy
    /// (a pickup), or open a dropped location. Loom's answer to this pane's own ask, or nothing.
    void on(const ws::PaneOperationAnswered& answer, loom::Mail& mail) {
        if (!mail.answers_ask()) {
            return;
        }
        if (pickup_.stage == Pickup::Stage::Permission && mail.correlation() == pickup_.ask) {
            if (!answer.allowed) {
                pickup_ = Pickup{};
                notice("nothing was carried -- " + answer.reason, true);
                say(mail);
                return;
            }
            pickup_.stage = Pickup::Stage::Carry;
            pickup_.ticket = mail.as_role(pane::kEditorPaneRole)
                                 .send_to_role(kWorkshopRole,
                                               ws::PaneValueCarryRequested{pane::kEditorPane, pickup_.label,
                                                                           pickup_.bytes, pickup_.drag},
                                               pickup_.gesture);
            if (!pickup_.ticket.valid()) {
                pickup_ = Pickup{};
                notice("nothing was carried -- the copy could not be handed to Workshop", true);
                say(mail);
            }
            return;
        }
        if (locate_.stage == Locate::Stage::Permission && mail.correlation() == locate_.ask) {
            if (!answer.allowed) {
                const std::string path = locate_.loc.path;
                locate_ = Locate{};
                notice("nothing was opened -- " + answer.reason, true);
                say(mail);
                return;
            }
            // THE OPEN IS THE MANAGED ONE, asked as this office (the relay's own route): the
            // unsaved-work floor, the desk and a refusal's attribution are the opening's, unchanged.
            locate_.stage = Locate::Stage::Opening;
            locate_.same_path = e_.open_document() && e_.path == locate_.loc.path;
            locate_.mark = mark_now();
            locate_.ask = ++asked_;
            locate_.ticket = mail.as_role(pane::kEditorPaneRole)
                                 .send_to_role(ws::kOpeningRole, OpenSourceRequested{locate_.loc.path},
                                               locate_.ask);
            if (!locate_.ticket.valid()) {
                const std::string path = locate_.loc.path;
                locate_ = Locate{};
                notice("nothing was opened -- the Editor could not ask the opening office for " + path, true);
                say(mail);
            }
        }
    }

    /// WORKSHOP TOOK THE COPY, OR SAID WHY NOT. The document was never touched either way.
    void on(const ws::PaneCarryAnswered& answer, loom::Mail& mail) {
        if (pickup_.stage != Pickup::Stage::Carry || !mail.answers_ask() ||
            mail.correlation() != pickup_.gesture) {
            return;
        }
        const Pickup done = std::move(pickup_);
        pickup_ = Pickup{};
        if (!answer.carried) {
            notice("nothing was carried -- " + answer.reason, true);
        } else if (!done.drag) {
            notice("carrying a copy of " + done.what +
                       " -- click a receiving pane, or Escape; this document is unchanged",
                   false);
        }
        say(mail);
    }

    // ---- The exit ------------------------------------------------------------------------

    /// MAY THE WORKSHOP END? Answered about THIS instant: dirty source refuses, naming the two
    /// ways out; a paste still arriving refuses too, because its answer could dirty the
    /// document after this one was given; an opening still being arranged refuses in words,
    /// because its commitment could replace the document this answer was about (the paste's
    /// rule, one operation over; the candidate is bounded -- it settles or is superseded);
    /// a clean document, or no document, permits. No input can reach this weave between this
    /// answer and the host's decision (the host holds every gesture while it waits), which is
    /// what makes "clean" a fact rather than a race.
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

    /// THE SKIN'S ANSWER TO A PASTE THIS PANE ASKED FOR (WL-EDIT-11, WL-TEXT-09). The
    /// correlation says this is the answer to an ask this incarnation made; it does not say
    /// the document that asked still stands, or stands where it stood. The settlement pins
    /// the whole position, as the host pinned it: a replaced or closed document strands the
    /// payload silently (the dead draft's own fate); a document that MOVED -- any edit, any
    /// caret or selection change between request and answer -- gets a sentence instead of a
    /// paste, because relocating the text to wherever the caret is now would be answering a
    /// question the maker no longer asked.
    ///
    /// (!) THE OUTSTANDING PASTE IS CLEARED THE MOMENT ITS AUTHENTICATED ANSWER IS CONSUMED --
    /// before the payload is judged -- so a stale answer (the source moved) leaves no paste
    /// in flight behind it, and a fresh opening is eligible right after it.
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
    // ---- The switch's helpers ----------------------------------------------------------------

    /// WHILE A SWITCH HOLDS THIS EDITOR STILL, an input that would change the document is counted
    /// and not applied. True when it was refused.
    bool held_still() {
        if (!holding_.active) {
            return false;
        }
        ++holding_.refused;
        return true;
    }

    /// A STATE NO SWITCH MAY BEGIN IN, in words; empty when a switch may begin.
    std::string handoff_refusal() const {
        if (holding_.active) {
            return "the Editor is already being switched";
        }
        if (candidate_.live) {
            return "the Editor is still opening " + candidate_.path + " -- switch once it has settled";
        }
        if (paste_.awaiting) {
            return "the Editor is still waiting for a clipboard answer -- switch once it has arrived";
        }
        if (!relays_.empty()) {
            return "the Editor is still relaying an open -- switch once it has settled";
        }
        if (locate_.stage != Locate::Stage::Idle) {
            return "the Editor is still opening a dropped location -- switch once it has settled";
        }
        if (drop_.pending) {
            return "a dropped command is waiting for your choice -- choose or dismiss it, then switch";
        }
        return std::string();
    }

    static std::vector<std::string> handoff_resets() {
        return {"the standard Editor's undo history", "the wheel's unspent fraction of a line"};
    }

    /// WHAT A SWITCH AWAY WOULD LOSE: nothing. The standard model IS the transfer's model, so the
    /// digest a switch away names is the digest of no losses, whatever the document holds.
    std::vector<std::string> handoff_losses() const {
#ifdef ZENGINE_EDITOR_TEST_LOSSES
        // TEST INSTRUMENTATION, COMPILED ONLY INTO `zengine-editor-pane-losing`: an editor whose
        // switch away loses something that moves with its document's line count, so a case can
        // ask for consent and make a given consent stale by typing a line. The normal image never
        // defines this.
        if (e_.open_document()) {
            return {"test instrumentation: " + std::to_string(e_.buffer.lines().size()) + " lines"};
        }
#endif
        return {};
    }

    EditorTransfer transfer_now() const {
        EditorTransfer t;
        t.doc_epoch = static_cast<std::int64_t>(e_.doc_epoch);
        t.project_dir = project_dir_;
        t.project_known = project_known_;
        t.source = "the standard Editor";
        if (!e_.open_document()) {
            return t;
        }
        t.path = e_.path;
        t.text = ws::source_text(e_.buffer.lines(), e_.convention);
        t.saved_text = ws::source_text(e_.saved_lines, e_.convention);
        t.convention = e_.convention;
        t.modified = e_.dirty();
        t.caret_row = static_cast<std::int64_t>(e_.buffer.caret_row());
        t.caret_byte = static_cast<std::int64_t>(e_.buffer.caret_byte());
        t.anchor_row = static_cast<std::int64_t>(e_.buffer.anchor_row());
        t.anchor_byte = static_cast<std::int64_t>(e_.buffer.anchor_byte());
        t.first_row = static_cast<std::int64_t>(e_.first_row);
        t.first_col = e_.first_col;
        return t;
    }

    // ---- Transfer helpers ------------------------------------------------------------------

    /// THE DOCUMENT AT THIS INSTANT: its generation, its bytes' revision, and the revision that
    /// moves with every caret or selection change -- so a mark taken at a press says whether
    /// anything at all moved since, and undo back to identical text still reads as moved.
    Mark mark_now() const {
        return Mark{e_.doc_epoch, e_.buffer.content_revision(), e_.buffer.revision()};
    }

    /// IS THIS POSITION ON THE HIGHLIGHT? The selection's characters, and the break of every line
    /// the selection runs past -- exactly the cells the pane paints as selected.
    bool on_highlight(EditorPos at) const {
        if (!e_.buffer.has_selection()) {
            return false;
        }
        const EditorPos from = e_.buffer.selection_begin();
        const EditorPos to = e_.buffer.selection_end();
        return !(at < from) && at < to;
    }

    /// A REMEMBERED PRESS ENDS at the next act that is not its own motion: it was only a press.
    void end_grab() { grab_ = Grab{}; }

    void offer(Take what, std::int64_t row, std::int64_t column, loom::Mail& mail) {
        menu_mark_ = mark_now();
        const bool selection = what == Take::Selection;
        menu_ = ws::pane_menu::Offer(pane::kEditorPane, selection ? "selection" : "location")
                    .at(row, column)
                    .row(selection ? pane::kActionExtract : pane::kActionLocation,
                         selection ? "Extract selection to Inventory" : "Carry this file's location")
                    .send(mail, pane::kEditorPaneRole);
    }

    static std::string file_name(const std::string& path) {
        const std::size_t slash = path.find_last_of('/');
        return slash == std::string::npos ? path : path.substr(slash + 1);
    }

    std::string where_words(EditorPos p) const {
        return "L" + std::to_string(p.row + 1) + ":C" +
               std::to_string(ws::visual_col_of(e_.buffer.line(p.row), p.byte) + 1);
    }

    std::string project_root() const {
        return project_known_ ? ws::persist::resolved_against(std::string(), project_dir_) : std::string();
    }

    /// ONE ACQUISITION, whichever hand asked -- the highlight's drag, the right-click menu, or the
    /// key (WL-EDIT-19): an owned copy of what the mark still names, or a refusal in words. The
    /// copy is the SELECTION'S OWN TEXT, unsaved edits included, never the file on disk and never
    /// an implicit line or word; or this file's location. Then Workshop is asked to approve the
    /// carry for the gesture that caused it; the carry itself follows the approval.
    // WL-EDIT-19 -- agents/workshop/editor-transfers.md
    void acquire(Take what, bool drag, std::uint64_t gesture, const Mark& at, loom::Mail& mail) {
        const bool selection = what == Take::Selection;
        if (!e_.open_document()) {
            notice("nothing was carried -- no source is open", true);
            return;
        }
        if (!(at == mark_now())) {
            notice(selection ? "nothing was carried -- the selection changed after you pointed at it"
                             : "nothing was carried -- the document changed after you pointed at it",
                   true);
            return;
        }
        if (pickup_.stage != Pickup::Stage::Idle) {
            notice("nothing was carried -- a copy is already on its way to Workshop", true);
            return;
        }
        st::Pair pair;
        std::string label;
        std::string what_words;
        if (selection) {
            if (!e_.buffer.has_selection()) {
                notice("nothing was carried -- select text first; nothing else is carried in its place", true);
                return;
            }
            const EditorPos from = e_.buffer.selection_begin();
            const EditorPos to = e_.buffer.selection_end();
            st::SourceSelection s;
            s.editor = "the standard Editor";
            s.path = e_.path;
            s.project_root = project_root();
            s.kind = st::kCharacters;
            s.first_line = static_cast<std::int64_t>(from.row) + 1;
            s.first_column = static_cast<std::int64_t>(from.byte) + 1;
            s.end_line = static_cast<std::int64_t>(to.row) + 1;
            s.end_column = static_cast<std::int64_t>(to.byte) + 1;
            s.line_ending = e_.convention == ws::line_ending::kCRLF ? "CRLF" : "LF";
            s.unsaved = e_.dirty();
            s.captured_at_epoch_s = st::clock_now();
            const std::string text = e_.buffer.selected_text();
            pair = st::text_pair(text, s);
            const st::Lines lines = st::standard_lines(text);
            what_words = (lines.ok ? st::amount_words(lines.lines) : std::string("the selection")) +
                         " of " + file_name(e_.path);
            label = file_name(e_.path) + " " + where_words(from) + "-" + where_words(to);
        } else {
            const std::size_t row = e_.buffer.caret_row();
            st::SourceLocation loc{e_.path, static_cast<std::int64_t>(row) + 1,
                                   static_cast<std::int64_t>(e_.buffer.caret_byte()) + 1};
            st::SourceLocationContext c;
            c.editor = "the standard Editor";
            c.project_root = project_root();
            c.relative = st::relative_to(e_.path, c.project_root);
            c.line_text = st::observe_line(e_.buffer.line(row));
            c.unsaved = e_.dirty();
            c.captured_at_epoch_s = st::clock_now();
            pair = st::location_pair(loc, c);
            what_words = "the location of " + file_name(e_.path) + " at line " + std::to_string(row + 1);
            label = file_name(e_.path) + ":" + std::to_string(row + 1) + " (location)";
        }
        if (!pair.ok) {
            notice("nothing was carried -- " + pair.refusal, true);
            return;
        }
        pickup_.stage = Pickup::Stage::Permission;
        pickup_.ask = ++asked_;
        pickup_.gesture = gesture;
        pickup_.drag = drag;
        pickup_.bytes.assign(pair.bytes.begin(), pair.bytes.end());
        pickup_.label = label.substr(0, 128);
        pickup_.what = what_words;
        pickup_.ticket = mail.as_role(pane::kEditorPaneRole)
                             .send_to_role(kWorkshopRole,
                                           ws::PaneOperationRequested{pane::kEditorPane, kWorkshopRole,
                                                                      ws::PaneValueCarryRequested::zen_name,
                                                                      ws::PaneValueCarryRequested::zen_version,
                                                                      static_cast<std::int64_t>(gesture)},
                                           pickup_.ask);
        if (!pickup_.ticket.valid()) {
            pickup_ = Pickup{};
            notice("nothing was carried -- Workshop could not be asked", true);
        }
    }

    /// WHY A DROP CANNOT INSERT INTO THIS DOCUMENT NOW, or empty.
    std::string insertion_refusal() const {
        if (!e_.open_document()) {
            return "open a source first: a drop inserts into the open document";
        }
        if (candidate_.live) {
            return "the Editor is opening " + candidate_.path + "; drop again once it has settled";
        }
        if (paste_.awaiting) {
            return "a paste is still arriving; drop again once it has";
        }
        if (drop_.pending) {
            return "a dropped command is still waiting for your choice";
        }
        return std::string();
    }

    /// WHERE A DROP LANDED, read through the picture it was aimed at (the caller checked the
    /// number): the character under it, or the end of the text below the last line -- and whether
    /// that character is on the highlight.
    Landing landing(std::int64_t prow, std::int64_t pcol) const {
        Landing l;
        const std::size_t row = e_.first_row + static_cast<std::size_t>(prow - chrome_rows_);
        if (row >= e_.buffer.line_count()) {
            const std::size_t last = e_.buffer.line_count() - 1;
            l.pos = EditorPos{last, e_.buffer.line(last).size()};
            return l;
        }
        l.pos = EditorPos{row, ws::byte_of_visual_col(e_.buffer.line(row), e_.first_col + (pcol < 0 ? 0 : pcol))};
        l.inside = on_highlight(l.pos);
        return l;
    }

    void receive(const ws::PaneValueDrop& drop, loom::Mail& mail) {
        const std::string bytes(drop.data.begin(), drop.data.end());
        st::Material m = st::read_material(bytes);
        if (m.kind == st::MaterialKind::Location) {
            open_location(m, mail);
            return;
        }
        if (m.kind == st::MaterialKind::Unsupported) {
            notice("nothing was inserted -- " + m.refusal, true);
            return;
        }
        const std::string busy = insertion_refusal();
        if (!busy.empty()) {
            notice("nothing was inserted -- " + busy, true);
            return;
        }
        if (drop.picture != picture_) {
            notice("nothing was inserted -- the text moved under the drop; drop it again", true);
            return;
        }
        if (drop.row < chrome_rows_) {
            notice("nothing was inserted -- drop onto the document's text, not its status row", true);
            return;
        }
        const Landing at = landing(drop.row, drop.column);
        if (m.kind == st::MaterialKind::Text) {
            (void)insert_lines(m.text, at, "", false);
            return;
        }
        const st::CppDocument cpp = st::cpp_document(e_.path);
        if (cpp == st::CppDocument::No) {
            insert_command(m, at);
            return;
        }
        // IN A C++ DOCUMENT THE MAKER CHOOSES, AND THE DROP ITSELF CHOOSES NOTHING (WL-EDIT-20): a
        // menu at the drop, continuing its gesture; nothing is inserted until a row is chosen.
        drop_.pending = true;
        drop_.material = std::move(m);
        drop_.at = at;
        drop_.mark = mark_now();
        drop_.menu = ws::pane_menu::Offer(pane::kEditorPane, kDropSubject)
                         .at(drop.row, drop.column)
                         .row(kInsertLine, "Insert its Terminal line")
                         .row(kInsertCpp, cpp == st::CppDocument::Yes ? "Generate C++ that builds it"
                                                                      : "Generate C++ (this .h is C++)")
                         .send(mail, pane::kEditorPaneRole);
        if (!drop_.menu.pending()) {
            drop_ = Dropped{};
            notice("nothing was inserted -- the choice for the dropped command could not be offered", true);
            return;
        }
        notice("choose how the dropped " + drop_.material.what + " goes in -- nothing is inserted until you do", false);
    }

    /// TEXT INTO THE DOCUMENT AS ONE UNDOABLE EDIT, or a refusal that leaves it untouched. `select`
    /// leaves the insertion selected (generated code, for review).
    bool insert_lines(const std::string& text, const Landing& at, const std::string& note, bool select) {
        const st::Lines lines = st::standard_lines(text);
        if (!lines.ok) {
            notice("nothing was inserted -- " + lines.refusal, true);
            return false;
        }
        std::size_t size = text.size();
        for (const std::string& l : e_.buffer.lines()) {
            size += l.size() + 2;
        }
        if (size > ws::kMaxSourceBytes) {
            notice("nothing was inserted -- the document would outgrow what the standard Editor holds", true);
            return false;
        }
        const bool replacing = at.inside && e_.buffer.has_selection();
        const EditorPos start = replacing ? e_.buffer.selection_begin() : at.pos;
        e_.buffer.insert_at(at.pos, lines.lines, at.inside);
        if (select) {
            e_.buffer.restore_selection(start.row, start.byte, e_.buffer.caret_row(), e_.buffer.caret_byte());
        }
        e_.follow_caret = true;
        const std::string what = note.empty() ? st::amount_words(lines.lines) : note;
        notice((replacing ? "replaced the highlighted selection with " : "inserted ") + what + " at " +
                   where_words(start) + " -- ctrl+z takes it back; nothing was saved",
               false);
        return true;
    }

    void insert_command(const st::Material& m, const Landing& at) {
        const st::TerminalLine t = st::terminal_line(*m.command, m.address);
        if (!t.ok) {
            notice("nothing was inserted -- " + m.what + ": " + t.refusal, true);
            return;
        }
        if (!insert_lines(t.line, at, "the Terminal line for " + m.what, false)) {
            return; // refused: its sentence stands
        }
        std::string said = notice_ + "; text only -- nothing was sent";
        if (!t.missing.empty()) {
            said += "; INCOMPLETE: ";
            for (std::size_t i = 0; i < t.missing.size(); ++i) {
                said += (i > 0 ? ", " : "") + t.missing[i];
            }
            said += t.missing.size() == 1 ? " is not set" : " are not set";
        }
        if (!t.address_supplied) {
            said += "; <address> marks a destination this value never named";
        }
        if (!m.address_note.empty()) {
            said += " (" + m.address_note + ")";
        }
        notice(said, !t.missing.empty());
    }

    void choose_drop(const Dropped& d, const std::string& chosen) {
        if (chosen.empty()) {
            notice("the dropped " + d.material.what + " was not inserted", false);
            return;
        }
        if (!(d.mark == mark_now())) {
            notice("nothing was inserted -- the document changed after the drop; drop it again", true);
            return;
        }
        if (chosen == kInsertLine) {
            insert_command(d.material, d.at);
        } else if (chosen == kInsertCpp) {
            insert_cpp(d.material, d.at);
        }
    }

    /// C++ THAT BUILDS THE DROPPED COMMAND (WL-EDIT-20), inserted as ONE undoable edit and left
    /// selected: the insertion is the preview. Its includes are named, never written in.
    // WL-EDIT-20 -- agents/workshop/editor-transfers.md
    void insert_cpp(const st::Material& m, const Landing& at) {
        const st::GeneratedCpp g = st::cpp_value_function(*m.command, e_.buffer.lines());
        if (!g.ok) {
            notice("no C++ was generated -- " + g.refusal, true);
            return;
        }
        // GENERATED CODE IS WHOLE LINES: it goes in before the line the drop landed on and ends in
        // a line break, so it never joins the text on either side of it.
        Landing whole = at;
        whole.pos.byte = 0;
        whole.inside = false;
        if (!insert_lines(st::join_lf(g.lines) + "\n", whole, "C++ for " + m.what, true)) {
            return; // refused: its sentence stands
        }
        std::string said = "generated " + g.function + "() for " + m.what + ", selected for review; ";
        if (g.missing_includes.empty()) {
            said += "its includes are already here";
        } else {
            said += "add #include";
            for (std::size_t i = 0; i < g.missing_includes.size(); ++i) {
                said += (i > 0 ? " and " : " ") + g.missing_includes[i];
            }
        }
        if (!g.holes.empty()) {
            said += "; INCOMPLETE until you fill " + std::to_string(g.holes.size()) + " required field" +
                    (g.holes.size() == 1 ? "" : "s");
        }
        notice(said + " -- ctrl+z removes it; nothing was sent, saved or built", !g.holes.empty());
    }

    /// A DROPPED LOCATION: ask Workshop whether this gesture's actor may open a file, then ask the
    /// opening office as this Editor's office (WL-EDIT-21). A locator is never inserted as text.
    // WL-EDIT-21 -- agents/workshop/editor-transfers.md
    void open_location(const st::Material& m, loom::Mail& mail) {
        const st::SourceLocation& loc = m.location;
        if (loc.path.empty() || !std::filesystem::path(loc.path).is_absolute()) {
            notice("nothing was opened -- this location names no absolute path; edit its path in Info", true);
            return;
        }
        if (locate_.stage != Locate::Stage::Idle) {
            notice("nothing was opened -- a dropped location is still being opened", true);
            return;
        }
        locate_.stage = Locate::Stage::Permission;
        locate_.loc = loc;
        locate_.loc.path = ws::persist::resolved_against(std::string(), loc.path);
        locate_.ctx = m.location_context;
        locate_.ask = ++asked_;
        locate_.ticket = mail.as_role(pane::kEditorPaneRole)
                             .send_to_role(kWorkshopRole,
                                           ws::PaneOperationRequested{pane::kEditorPane, ws::kOpeningRole,
                                                                      OpenSourceRequested::zen_name,
                                                                      OpenSourceRequested::zen_version,
                                                                      static_cast<std::int64_t>(mail.correlation())},
                                           locate_.ask);
        if (!locate_.ticket.valid()) {
            locate_ = Locate{};
            notice("nothing was opened -- Workshop could not be asked", true);
        }
    }

    /// THE OPEN CAME TO SOMETHING. A refusal is said with the location's own context; a success
    /// places the caret only where the location says, in that same document, with nothing moved
    /// since it was shown, and on a line that still reads as it did when saved.
    void settle_location(const SourceOpened& said) {
        Locate l = std::move(locate_);
        locate_ = Locate{};
        const std::string root = l.ctx ? l.ctx->project_root : std::string();
        const std::string here = project_root();
        const bool elsewhere = !root.empty() && !here.empty() && root != here;
        if (!said.accepted) {
            std::string why = "could not open " + l.loc.path + ": " + said.refusal;
            if (elsewhere) {
                why += " -- it was saved under " + root + ", and this run's project is " + here +
                       "; a location never follows its name to another root (edit its path in Info to rebind it)";
            }
            notice(why, true);
            return;
        }
        if (!e_.open_document() || e_.path != l.loc.path) {
            notice("the location's file was shown, but the Editor holds another document now -- the caret was not moved", false);
            return;
        }
        const std::string opened = "opened " + shown_path() +
                                   (elsewhere ? " (saved under another project root, " + root + ")" : std::string());
        if (l.loc.line <= 0) {
            notice(opened, false);
            return;
        }
        const bool untouched = l.same_path
                                   ? e_.doc_epoch == l.mark.epoch && e_.buffer.revision() == l.mark.revision
                                   : e_.doc_epoch == installed_epoch_ && e_.buffer.revision() == installed_revision_;
        if (!untouched) {
            notice(opened + " -- you moved in it before the location arrived, so the caret stays where you put it", false);
            return;
        }
        const std::size_t row = static_cast<std::size_t>(l.loc.line - 1);
        if (row >= e_.buffer.line_count()) {
            notice(opened + " -- it has no line " + std::to_string(l.loc.line) + " now, so the caret was not moved", false);
            return;
        }
        if (l.ctx && !st::still_reads(e_.buffer.line(row), l.ctx->line_text)) {
            notice(opened + " -- line " + std::to_string(l.loc.line) +
                       " no longer reads as it did when the location was saved, so the caret was not moved",
                   false);
            return;
        }
        e_.buffer.place(row, l.loc.column > 0 ? static_cast<std::size_t>(l.loc.column - 1) : 0);
        e_.follow_caret = true;
        notice(opened + " at line " + std::to_string(l.loc.line), false);
    }

    // ---- Offering and declaring ---------------------------------------------------------

    void announce(loom::Mail& mail) {
        (void)mail.as_role(pane::kEditorPaneRole)
            .send_to_role(kWorkshopRole, PaneOffered{pane::kEditorPane, pane::kEditorPaneName,
                                                     pane::kEditorPaneSummary, 20, 84});
        declare(mail);
    }

    /// WHAT THIS PANE ANSWERS TO -- four rows, the built-in's own four, and they never
    /// change. Everything else a maker presses reaches the buffer as an ordinary `PaneKey`,
    /// which is what lets Backspace erase and ctrl+z undo without either being anybody's row.
    void declare(loom::Mail& mail) {
        // (!) THE SECOND VERSION OF THE DECLARATION, because this pane owns one of Workshop's
        // actions and version one has no field to say so (VD-27). Every pane that owns
        // nothing keeps declaring version one, unchanged and unrebuilt.
        ws::v2::PaneActions actions;
        actions.pane = pane::kEditorPane;
        const auto row = [&actions](const char* id, const char* label, std::int64_t sc,
                                    std::int64_t mods, const char* stands_for = "") {
            actions.rows.push_back(ws::v2::PaneActionRow{id, label, sc, mods, stands_for});
        };
        // (*) AND THE SAVE ROW STILL NAMES `document.save` AS WHAT IT STANDS IN FOR (VD-26), though
        // that host row retired with the object document (WL-KEY-15 says how a pane stands in). A
        // host on this side of the retirement admits the name standing in for nothing; a host from
        // before it still declares the row, and there the name is what keeps `ctrl+s` this pane's
        // rather than a collision -- so one image of this pane loads in both.
        row(pane::kActionSave, "save source", input::scan::kS, input::mod::kCtrl,
            ws::kOwnableDocumentSave);
        row(pane::kActionNewline, "newline", input::scan::kReturn, input::mod::kNone);
        row(pane::kActionTab, "insert tab", input::scan::kTab, input::mod::kNone);
        row(pane::kActionDiscard, "discard source edits", input::scan::kD, input::mod::kCtrl);
        // Plain ctrl+letters, which every medium can say (a POSIX terminal cannot say alt or
        // ctrl+shift on a letter, and no terminal backend names a function key).
        row(pane::kActionExtract, "carry the selection", input::scan::kE, input::mod::kCtrl);
        row(pane::kActionLocation, "carry this file's location", input::scan::kL, input::mod::kCtrl);
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
            // THE REASON FIRST, THE FILE AFTER IT, in every refusal of an open: a requester's
            // row is cut at its width from the end, and the file is what a maker can lose.
            return Written::no("a relative path means nothing until this Editor is told where "
                               "this run began -- open " +
                               requested + " by its full path");
        }
        out = ws::persist::resolved_against(project_dir_, requested);
        return Written::ok();
    }

    /// WHAT AN OPEN WOULD COME TO, JUDGED WITH NOTHING MOVED (WL-EDIT-05). The candidate it
    /// carries is bytes and a name; it is not a document until it is installed.
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
            plan.outcome = Written::no("the Editor holds unsaved changes to " + e_.path +
                                       " -- save source or discard source edits in the "
                                       "Editor first; nothing was opened");
            return plan;
        }
        if (paste_.awaiting) {
            // A PASTE STILL ARRIVING IS THE OPEN DOCUMENT'S, and its answer could still land
            // in it: replacing the document under it would strand a maker's own paste. The
            // quit's rule (WL-EDIT-14), one operation over: refused in words, try again.
            plan.outcome = Written::no("the Editor is still waiting for a clipboard answer "
                                       "for " +
                                       e_.path +
                                       " -- try again once it has arrived; nothing was opened");
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
            plan.outcome =
                Written::no(plan.admitted.outcome.refusal + " -- " + plan.path + " was not opened");
        }
        return plan;
    }

    // THERE IS NO `install` HERE ANY MORE. The Step 1
    // slice kept a document-only install behind the old door; that door relays now, and a
    // document becomes this weave's only inside `activate`, in the showing hook, from a
    // candidate a managed operation published. A dead installer with the old meaning would
    // be the seam that meaning could creep back through, so it is gone rather than kept.

    /// THE MAKER'S WORDS FOR AN OFFER THE BUS REFUSED: the document this weave claims moved
    /// since the operation bound it (an edit, a paste), the operation was superseded, or a
    /// participant was replaced. The diagnostic name stays in parentheses.
    static std::string offer_refusal(const std::string& path, loom::JointRefusal why) {
        switch (why) {
        case loom::JointRefusal::StaleRevision:
        case loom::JointRefusal::ParticipantChanged:
        case loom::JointRefusal::WrongState:
        case loom::JointRefusal::Cancelled:
            return "the document changed while opening " + path + " -- try again";
        default:
            return "the opening was no longer arranged (" + std::string(loom::name_of(why)) +
                   ") -- " + path + " was not prepared";
        }
    }

    /// A REFUSED PREPARATION, as one value: the operation, no, and the sentence.
    static SourcePrepared not_prepared(std::int64_t op, std::string refusal) {
        SourcePrepared refused;
        refused.op = op;
        refused.ok = false;
        refused.refusal = std::move(refusal);
        return refused;
    }

    /// THE CANDIDATE BECOMES THE DOCUMENT -- inside the publication hook, and nowhere else.
    /// A same-path candidate changes nothing but the notice; any other replaces the whole
    /// document, retiring the old one's paste and sweep with it.
    void activate() {
        if (!candidate_.same_path) {
            e_ = std::move(candidate_.doc);
            ++saved_stamp_;
            drag_ = Drag{};
            paste_ = Paste{};
            grab_ = Grab{};
            installed_epoch_ = e_.doc_epoch;
            installed_revision_ = e_.buffer.revision();
        }
        // THE ROOM IS THE TRIAL'S, NOW: the desk seated this pane in the same step it
        // published, with the rows composed for exactly this room, so a gesture that arrives
        // before the desk's own room grant is judged against the picture that is showing.
        granted_ = true;
        rows_ = candidate_.rows;
        columns_ = candidate_.columns;
        chrome_rows_ = candidate_.chrome_rows;
        doc_rows_ = candidate_.doc_rows;
        opened_by_ = candidate_.op;
        candidate_ = Candidate{};
        notice((e_.dirty() ? "UNSAVED edits stand -- editing " : "editing ") + shown_path(),
               false);
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
    ///
    /// (!) NO PASTE AND NO CANDIDATE COME BACK, deliberately: both were the old incarnation's
    /// conversations, and their answers arrive to a pane that is no longer waiting. A fresh
    /// opening is eligible at once.
    void restore_from_state() {
        // WHERE THIS RUN BEGAN, AND THE LAST THING THIS PANE SAID: both are the picture the
        // maker was looking at, and both come back before the document does, because the
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
        e_.convention = state_.convention;
        e_.doc_epoch = static_cast<std::uint64_t>(state_.doc_epoch);
        e_.buffer.restore_selection(as_index(state_.anchor_row), as_index(state_.anchor_byte),
                                    as_index(state_.caret_row), as_index(state_.caret_byte));
        e_.first_row = as_index(state_.first_row);
        e_.first_col = state_.first_col < 0 ? 0 : state_.first_col;
        e_.wheel_accum = 0.0;
        e_.follow_caret = false;
        // (*) THE ROOM THE DOCUMENT WAS LAST LOOKED AT THROUGH, CARRIED (VD-26). Zeroing these
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

    static bool same_identity(const EditorDocument& a, const EditorDocument& b) {
        return a.path == b.path && a.doc_epoch == b.doc_epoch && a.convention == b.convention &&
               a.content_revision == b.content_revision && a.dirty == b.dirty &&
               a.opened_by == b.opened_by;
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

    static Viewport viewport_of(const EditorState& doc) {
        return Viewport{doc.first_row, doc.first_col, doc.follow_caret, doc.last_rows,
                        doc.last_cols};
    }

    static void apply_viewport(EditorState& doc, const Viewport& v) {
        doc.first_row = v.first_row;
        doc.first_col = v.first_col;
        doc.follow_caret = v.follow_caret;
        doc.last_rows = v.last_rows;
        doc.last_cols = v.last_cols;
    }

    /// KEEP THE VIEWPORT TRUE AGAINST THE ROOM AND THE DOCUMENT IT HAS NOW (WL-EDIT-09): clamp
    /// the offsets always, follow the caret when a gesture asked or THE GRANTED ROOM changed,
    /// and deliberately not after the wheel.
    ///
    /// (!) THE GRANTED ROOM, AND NOT THE ROWS THE DOCUMENT WAS LEFT (VD-27). A notice appearing
    /// or clearing changes the second and not the first, and a maker who scrolled somewhere
    /// to read did not ask to be taken back to the caret because this pane had something to
    /// say. A genuine resize still follows, because that is what these two numbers are.
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
        // Rule 1's horizontal half, measured on the caret's own line: no blank room at the
        // right while its text is hidden at the left, so erasing a long line back down
        // recovers the room it freed.
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

    /// THE STATUS ROW: the dirty word first, then `L:C/N`, then the path -- in the order the
    /// facts must survive a narrow room (WL-EDIT-12). The path is cut from its HEAD when the
    /// room is short of it: the end of a path is the part that says which file this is, and a
    /// temporary directory's spelling is long enough on every platform to have proved it. The
    /// pane's name and office are the host's header, one row above, so `Editor` is not said
    /// twice.
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

    /// COMPOSE A DOCUMENT FOR A ROOM: the status row, a standing notice where the room holds
    /// one, then the document through the viewport -- and the caret and selection beside the
    /// rows, on the same lattice a press names (WL-EDIT-12, WL-CARET-01). Pure over the
    /// document and the viewport it is handed, so the same composition serves the live
    /// document (whose viewport is then written back) and a candidate (whose is kept with it).
    ///
    /// THE ROW BUDGET IS SPENT IN THIS ORDER, because a pane can be granted any height a
    /// maker's arrangement gives it. The status row is first: it is where the dirty word
    /// lives, and the one thing a maker must be able to read before they build. A notice --
    /// a refusal, or what the last act came to -- gets a row of its own only where at least
    /// one document row survives under it; in a smaller room it stands in for the status row
    /// instead, so the document keeps its rows and the caret keeps its place. The document
    /// takes what is left.
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

    /// WHERE THE CARET IS, AND WHAT IS SELECTED -- beside the rows, never inside them, in the
    /// body lattice (WL-CARET-01). The caret is said only while its row is in the window; the
    /// selection is clamped into the window on both ends, and a range that runs past the last
    /// shown row ends at `(rows, 0)` -- the exclusive end one past the last row, which is the
    /// one position with no row that a reading-order range may name (WL-CARET-03). A selection
    /// may stand with no caret: the caret scrolled out of the window is `kNoCaret`, and the
    /// range it belongs to is still on screen.
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

    /// THE PANE, SAID: the live document composed for the granted room, its viewport moved
    /// as the composition moved it, and the rows and the caret published beside each other,
    /// each naming the document's generation so a projection of a document that is gone can
    /// never repaint the one that replaced it, numbered by the picture it is (`v3::PaneContent`).
    // WL-EDIT-18 -- agents/workshop/editor-transfers.md
    void say(loom::Mail& mail) {
        if (!granted_) {
            return; // no room has been sent: nothing this pane could truthfully fill
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
        (void)mail.as_role(pane::kEditorPaneRole)
            .send_to_role(kWorkshopRole,
                          ws::v3::PaneContent{pane::kEditorPane, std::move(c.rows),
                                              static_cast<std::int64_t>(e_.doc_epoch), picture_});
        (void)mail.as_role(pane::kEditorPaneRole).send_to_role(kWorkshopRole, c.caret);
    }

    /// THE LIVE DOCUMENT, WRITTEN INTO THE SHAPE LOOM ANSWERS READS FROM (VD-26). Called at
    /// the end of every delivery and inside the publication hook, which is one call per
    /// delivery and not one per field written.
    ///
    /// THE TWO EXPENSIVE FIELDS ARE GATED ON WHAT ACTUALLY MOVED: the buffer's own revision
    /// for `text`, a stamp bumped by the writers of `saved_lines` for `saved_text`. A
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
        // (!) THE BYTES' OWN REVISION, NOT THE BUFFER'S (VD-27). `revision()` moves when the
        // CARET moves, because a pending paste has to notice that; keying the mirror on it
        // rebuilt a four-megabyte string on an arrow key, a press and every motion of a
        // drag, with every byte identical. `content_revision()` moves when the lines do.
        // A document that was just activated is a different buffer at some revision of its
        // own, so the epoch is part of the key.
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

    /// AN OPEN RELAYED THROUGH THE OLD DOOR: the
    /// requester's kept answer right, and this weave's own conversation with the manager
    /// -- its correlation and its exact attempt -- so the manager's answer, or the bus's
    /// refusal of the attempt, finds the requester it was for. Bounded, and not in the
    /// state shape: the rights die with the incarnation that kept them (ANS-02), and a
    /// manager's answer to a predecessor's relay is refused by the bus rather than
    /// delivered to code that never asked.
    struct Relay {
        std::string path;
        std::uint64_t correlation = 0;
        loom::Ticket attempt{};
        loom::DeferredAnswer answer;
    };
    static constexpr std::size_t kMaxRelays = 4;
    std::vector<Relay> relays_;

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

    /// WHAT THE MIRRORED SHAPE WAS BUILT FROM: the buffer revision and epoch `text` was joined
    /// at, and the stamp `saved_text` was. Bumped by the writers of `saved_lines` (install,
    /// activation, save, revival), so a rebuild happens when the bytes moved and at no other
    /// time.
    std::uint64_t saved_stamp_ = 0;
    std::uint64_t mirrored_content_ = 0;
    std::uint64_t mirrored_epoch_ = 0;
    std::uint64_t mirrored_saved_ = 0;

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

    /// A SWITCH HOLDING THIS EDITOR STILL at its boundary: which operation, and how many inputs
    /// it refused meanwhile. Not in the state shape -- a reload is not a switch, and the hold
    /// belongs to the incarnation the coordinator is talking to.
    struct Holding {
        bool active = false;
        std::int64_t op = 0;
        std::int64_t refused = 0;
    };
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

ZEN_EXPORT_WEAVE(EditorPaneWeave)
