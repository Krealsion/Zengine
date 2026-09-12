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
// is a source request, a preparation, a quit answer, rows and a caret. What the host keeps is
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
#include "workshop/open_seam_vocabulary.hpp"
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

using ws::EditorBuffer;
using ws::EditorDocument;
using ws::EditorPos;
using ws::EditorState;
using ws::ManagedOpenProgress;
using ws::ManagedOpenSettled;
using ws::OpenSourceRequested;
using ws::PaneActionRequested;
using ws::PaneCatalogRequested;
using ws::PaneDragged;
using ws::PaneKey;
using ws::PaneOffered;
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
                       PaneDragged, PaneKey, PaneTextInput, PaneWheel, PaneActionRequested,
                       PaneQuitRequested, OpenSourceRequested, PrepareSourceRequested,
                       ManagedOpenProgress, ManagedOpenSettled, SourceOpened,
                       loom::DispatchRefused, ProjectRoot, surface::ClipboardCopy,
                       surface::ClipboardText>,
          loom::Emit<PaneOffered, ws::v2::PaneActions, ws::v2::PaneContent, ws::v2::PaneCaret,
                     PaneQuitAnswered, SourceOpened, SourcePrepared, OpenSourceRequested,
                     ProjectRootRequested, surface::ClipboardCopy,
                     surface::ClipboardTextRequested>,
          loom::Claims<EditorDocument>> {
    /// ONE PREPARED CANDIDATE: the whole document a managed opening would install, built
    /// beside the current one for one exact operation, and the identity this weave OFFERED
    /// for it. Not a document: nothing reads it, paints it or edits it. It becomes the
    /// document only in `on_claim_published`, when the bus has published that identity.
    ///
    /// ⚠ NOTHING IS HELD FOR IT. Input to the current document applies to the current
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
        ws::v2::PaneCaret caret;
        Viewport view;
    };

public:
    using Base = loom::WeaveBase<
        EditorPaneWeave, pane::EditorPaneState,
        loom::Accept<loom::Activated, PaneCatalogRequested, PaneRoom, PanePressed, PaneDragged,
                     PaneKey, PaneTextInput, PaneWheel, PaneActionRequested, PaneQuitRequested,
                     OpenSourceRequested, PrepareSourceRequested, ManagedOpenProgress,
                     ManagedOpenSettled, SourceOpened, loom::DispatchRefused, ProjectRoot,
                     surface::ClipboardCopy, surface::ClipboardText>,
        loom::Emit<PaneOffered, ws::v2::PaneActions, ws::v2::PaneContent, ws::v2::PaneCaret,
                   PaneQuitAnswered, SourceOpened, SourcePrepared, OpenSourceRequested,
                   ProjectRootRequested, surface::ClipboardCopy, surface::ClipboardTextRequested>,
        loom::Claims<EditorDocument>>;

    // ---- The state a reload carries, and the surface a poke reads ----------------------

    /// ⚠ THERE IS NO `snapshot()` HERE, AND ITS ABSENCE IS THE POINT (VD-26). This pane used
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
    /// ⚠ THE OFFER CAN BE REFUSED BY THE BUS -- the operation is not this weave's, or the
    /// document's claim already moved since the operation bound it -- and a refused offer
    /// is answered as one: no candidate is kept for an operation that cannot commit.
    void on(const PrepareSourceRequested& asked, loom::Mail& mail) {
        if (!mail.authored_from_role(ws::kOpeningRole)) {
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
    /// ⚠ AN IDENTITY THIS INCARNATION DID NOT PREPARE IS NOT INSTALLED, AND IS ANSWERED SO.
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
        if (resay_) {
            resay_ = false;
            say(mail);
        }
        mirror_state();
        claim_document(mail);
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
    /// ⚠ THE OUTSTANDING PASTE IS CLEARED THE MOMENT ITS AUTHENTICATED ANSWER IS CONSUMED --
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
            plan.outcome =
                Written::no(e_.path + " has unsaved changes -- save source or discard "
                                      "source edits in the Editor first; nothing was opened");
            return plan;
        }
        if (paste_.awaiting) {
            // A PASTE STILL ARRIVING IS THE OPEN DOCUMENT'S, and its answer could still land
            // in it: replacing the document under it would strand a maker's own paste. The
            // quit's rule (WL-EDIT-14), one operation over: refused in words, try again.
            plan.outcome = Written::no(e_.path + " is still waiting for a clipboard answer -- "
                                                 "try again once it has arrived; nothing was "
                                                 "opened");
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
            return path + " was not opened -- the document changed while opening; try again";
        default:
            return path + " was not prepared -- the opening was no longer arranged (" +
                   loom::name_of(why) + ")";
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
    /// ⚠ NO PASTE AND NO CANDIDATE COME BACK, deliberately: both were the old incarnation's
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
    /// ⚠ THE GRANTED ROOM, AND NOT THE ROWS THE DOCUMENT WAS LEFT (VD-27). A notice appearing
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
    /// never repaint the one that replaced it (`v2::PaneContent`).
    void say(loom::Mail& mail) {
        if (!granted_) {
            return; // no room has been sent: nothing this pane could truthfully fill
        }
        Composition c = compose(e_, viewport_of(e_), rows_, columns_, notice_, notice_bad_);
        apply_viewport(e_, c.view);
        chrome_rows_ = c.chrome_rows;
        doc_rows_ = c.doc_rows;
        (void)mail.as_role(pane::kEditorPaneRole)
            .send_to_role(kWorkshopRole,
                          ws::v2::PaneContent{pane::kEditorPane, std::move(c.rows),
                                              static_cast<std::int64_t>(e_.doc_epoch)});
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
        // ⚠ THE BYTES' OWN REVISION, NOT THE BUFFER'S (VD-27). `revision()` moves when the
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
};

} // namespace

ZEN_EXPORT_WEAVE(EditorPaneWeave)
