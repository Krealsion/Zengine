// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_EDITOR_PANE_OPENING_SWITCH_HPP
#define ZENGINE_EDITOR_PANE_OPENING_SWITCH_HPP

// The Editor pane's two open doors and its half of a switch: an open relayed to the opening
// office or prepared for a managed opening, the candidate that becomes the document when the
// bus publishes this weave's claim (WL-EDIT-05), and the incumbent's and the sealed candidate's
// doors (WL-SWITCH-04, WL-SWITCH-05). These are `EditorPaneWeave`'s members, declared in
// `pane.cpp`, which includes this file after the class; it compiles in no other place.
// Workshop law: agents/workshop/editor.md, agents/workshop/editor-switch.md, agents/workshop/opening.md

#include "editor/editor-pane/common.hpp"

#include "editor/editor-pane/editor.hpp"
#include "editor/office_seam.hpp"
#include "workshop/editor_handoff_vocabulary.hpp"
#include "workshop/editor_switch_vocabulary.hpp"
#include "workshop/open_seam_vocabulary.hpp"
#include "workshop/pane_canvas_rows.hpp"
#include "workshop/pane_seam_vocabulary.hpp"
#include "workshop/persist.hpp"

#include <zen/weave.hpp>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

// ---- THE TWO DOORS: open a source directly, or prepare it for a managed opening ------

/// The old door (WL-EDIT-05, WL-OPEN-07): the document and its presentation -- seated,
/// selected, holding the keys -- or a truthful refusal. This weave does not arrange the desk,
/// so it relays (`office::relay_open_source`): the requester's answer right is kept here, the
/// opening manager is asked in this weave's own conversation, and its outcome is spent into the
/// kept right. There is no document-only install behind this door.
inline void EditorPaneWeave::on(const OpenSourceRequested& asked, loom::Mail& mail) {
    office::relay_open_source(asked, mail, pane::kEditorPaneRole, holding_, relays_, asked_);
}

/// The manager's answer: to a dropped location's own open, settled here; otherwise to a relay,
/// spent into its requester's kept right (`office::settle_relay`).
inline void EditorPaneWeave::on(const SourceOpened& said, loom::Mail& mail) {
    if (mail.answers_ask() && locate_.stage == Locate::Stage::Opening &&
        mail.correlation() == locate_.ask) {
        settle_location(said);
        say(mail);
        return;
    }
    office::settle_relay(said, mail, relays_);
}

/// The bus's word that an ask was refused before any handler ran -- no opening office held, its
/// holder does not accept, or it holds behind a claim it could not apply -- matched by exact
/// attempt: a relay's requester is told in words, and a pickup's or a dropped location's failure
/// is said (`office::settle_dispatch_refused`).
inline void EditorPaneWeave::on(const loom::DispatchRefused& refused, loom::Mail& mail) {
    if (office::settle_dispatch_refused(refused, mail, relays_, pickup_, locate_, noticing())) {
        say(mail);
    }
}

/// The `apply` word: the bus showed this weave its published claim before this handler; the
/// hook did the work, and the end of the delivery says the rows.
inline void EditorPaneWeave::on(const ManagedOpenProgress& said, loom::Mail& mail) {
    (void)said;
    (void)mail;
}

/// The managed door (WL-OPEN-01, WL-OPEN-03): prepare this source for the room the desk's
/// trial would grant, and offer the document's identity for the exact operation. Judged with
/// nothing moved; the candidate is a whole document built beside the current one, and its
/// composition travels back as the trial's content: as rows and a caret here...
inline void EditorPaneWeave::on(const PrepareSourceRequested& asked, loom::Mail& mail) {
    if (!mail.authored_from_role(ws::kOpeningRole)) {
        return;
    }
    Preparation p = prepare(asked.op, asked.path, asked.rows, asked.columns, ws::PaneCanvasRoom{},
                            mail);
    if (!p.refusal.empty()) {
        (void)mail.answer(not_prepared(asked.op, std::move(p.refusal)));
        return;
    }
    SourcePrepared prepared;
    prepared.op = asked.op;
    prepared.ok = true;
    prepared.generation = p.generation;
    prepared.rows = std::move(p.shown.rows);
    prepared.caret_row = p.shown.caret.row;
    prepared.caret_col = p.shown.caret.column;
    prepared.sel_begin_row = p.shown.caret.sel_begin_row;
    prepared.sel_begin_col = p.shown.caret.sel_begin_col;
    prepared.sel_end_row = p.shown.caret.sel_end_row;
    prepared.sel_end_col = p.shown.caret.sel_end_col;
    (void)mail.answer(prepared);
}

/// ...or as this pane's own picture in the canvas room the trial reserved, composed for the
/// rows and columns of its lattice and numbered as the first picture in that room.
inline void EditorPaneWeave::on(const ws::v2::PrepareSourceRequested& asked, loom::Mail& mail) {
    if (!mail.authored_from_role(ws::kOpeningRole)) {
        return;
    }
    const ws::CanvasRows lattice = ws::canvas_rows(asked.room);
    Preparation p = prepare(asked.op, asked.path, lattice.rows, lattice.columns, asked.room, mail);
    ws::v2::SourcePrepared prepared;
    prepared.op = asked.op;
    prepared.ok = p.refusal.empty();
    prepared.refusal = std::move(p.refusal);
    if (prepared.ok) {
        prepared.generation = p.generation;
        prepared.picture = ws::rows_picture(asked.room, 1, p.shown.rows,
                                            parts_of(p.shown, lattice.columns),
                                            rows_caret(p.shown.caret));
    }
    (void)mail.answer(prepared);
}

/// PREPARE THE CANDIDATE: judged with nothing moved, built beside the current document,
/// composed for `rows` by `columns`, and its identity offered for the exact operation. The bus
/// can refuse the offer (not this weave's operation, or the claim already moved); then no
/// candidate is kept.
inline Preparation EditorPaneWeave::prepare(std::int64_t asked_op, const std::string& asked_path,
                                            std::int64_t rows, std::int64_t columns,
                                            const ws::PaneCanvasRoom& room, loom::Mail& mail) {
    Preparation out;
    if (holding_.active) {
        ++holding_.refused;
        out.refusal = "the Editor is being switched -- " + asked_path +
                      " was not prepared; open it again once the switch has settled";
        return out;
    }
    candidate_ = Candidate{}; // a newer preparation supersedes an older candidate
    const std::uint64_t op = static_cast<std::uint64_t>(asked_op);
    Plan plan = judge_source(asked_path);
    if (!plan.outcome.accepted) {
        out.refusal = plan.outcome.refusal;
        return out;
    }
    Candidate next;
    next.live = true;
    next.op = op;
    next.path = plan.path;
    next.same_path = plan.same_path;
    EditorDocument offered;
    if (plan.same_path) {
        // THE DOCUMENT IS THIS ONE; what the operation changes is the desk. Composed for
        // the trial room on a COPY of the view, because asking for the open source again
        // moves the pane and never the view (WL-EDIT-09).
        offered = identity_now();
        offered.opened_by = asked_op;
        out.shown = compose(e_, viewport_of(e_), rows, columns,
                            (e_.dirty() ? "UNSAVED edits stand -- editing " : "editing ") +
                                tail_of_path(e_.path, columns > 24 ? columns - 24 : columns),
                            false, status_shown_);
    } else {
        next.doc.path = plan.path;
        next.doc.saved_lines = plan.admitted.lines;
        next.doc.buffer.set_lines(std::move(plan.admitted.lines));
        next.doc.convention = plan.admitted.convention;
        next.doc.doc_epoch = e_.doc_epoch + 1;
        next.doc.follow_caret = true;
        wear(next.doc);
        offered.path = next.doc.path;
        offered.doc_epoch = static_cast<std::int64_t>(next.doc.doc_epoch);
        offered.convention = next.doc.convention;
        offered.content_revision =
            static_cast<std::int64_t>(next.doc.buffer.content_revision());
        offered.dirty = false;
        offered.opened_by = asked_op;
        out.shown = compose(next.doc, viewport_of(next.doc), rows, columns,
                            "editing " + tail_of_path(next.doc.path,
                                                      columns > 24 ? columns - 24 : columns),
                            false, status_shown_);
        // The candidate keeps the viewport it was composed with, so what it shows at
        // activation is what the desk admitted.
        apply_viewport(next.doc, out.shown.view);
    }
    next.rows = rows;
    next.columns = columns;
    next.chrome_rows = out.shown.chrome_rows;
    next.doc_rows = out.shown.doc_rows;
    next.room = room;
    const loom::JointResult offer = mail.offer(op, offered);
    if (!offer.ok) {
        out.refusal = offer_refusal(plan.path, offer.why);
        return out;
    }
    candidate_ = std::move(next);
    out.generation = offered.doc_epoch;
    return out;
}

/// THE OPERATION ENDED, said by the manager AFTER the fact. On a commitment this weave
/// was already shown its published claim before this delivery (the hook below), so the
/// candidate is already the document; on a refusal the candidate is dropped and the
/// reason stands where the weaver reads. Neither is the commitment.
inline void EditorPaneWeave::on(const ManagedOpenSettled& said, loom::Mail& mail) {
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

/// The hook (WL-OPEN-02; Loom's `Weave::claim_published`): the bus published this weave's
/// document claim by a joint operation, and this weave has not run since. Called before the
/// next delivery and snapshot, with no Mail: the candidate the published identity names
/// becomes the document here and the mirror is rebuilt here, so nothing queued behind the
/// commitment finds the old document. An identity this incarnation did not prepare (a
/// reloaded successor's) is not installed: it keeps its document and answers `false`.
inline bool EditorPaneWeave::on_claim_published(const EditorDocument& published) {
#ifdef ZENGINE_EDITOR_TEST_THROW_ON_B_CPP
    // Test instrumentation, compiled only into `zengine-editor-throwing`
    // (tests/CMakeLists.txt): one deliberate throw before anything is activated, for a
    // published path ending in `/b.cpp`: a real owner whose image cannot complete a showing.
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
    // A publication this incarnation did not prepare: keep the document and re-claim it at
    // the end of the next delivery. Its generation moves past the published one first, since
    // the desk drops projections of an older generation; a paste pinned to the old epoch
    // retires with it. The answer is Declined -- not applied, not broken -- and the manager
    // records it as not applied after repair.
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

// ---- A switch: this Editor as the incumbent (WL-SWITCH) ---------------------------------

/// WHAT WOULD A SWITCH AWAY COST? Nothing the transfer cannot carry -- the standard model IS
/// the transfer's model -- so there are no losses to consent to; the undo history and the
/// wheel's fraction are reset and said. A state no switch may begin in is refused in words.
// WL-SWITCH-04 -- agents/workshop/editor-switch.md
inline void EditorPaneWeave::on(const EditorHandoffJudgeRequested& asked, loom::Mail& mail) {
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
inline void EditorPaneWeave::on(const EditorHandoffRequested& asked, loom::Mail& mail) {
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
inline void EditorPaneWeave::on(const EditorHandoffEnded& ended, loom::Mail& mail) {
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
inline void EditorPaneWeave::on(const EditorRetireRequested& asked, loom::Mail& mail) {
    if (!holding_.active || holding_.op != asked.op) {
        return;
    }
    (void)mail.answer(EditorRetired{asked.op, holding_.refused});
}

// ---- A switch: this Editor as the candidate, sealed --------------------------------------

/// READY AT ONCE: the standard Editor needs nothing started. What it keeps is the room it
/// will be seated in and where the project began.
inline void EditorPaneWeave::on(const EditorWarmRequested& asked, loom::Mail& mail) {
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

inline void EditorPaneWeave::on(const EditorPreparationTick&, loom::Mail&) {}

/// ADOPT THE DOCUMENT, OR SAY EXACTLY WHY NOT. The bytes meet the law a file meets
/// (`source_in`), so a document this editor cannot carry truthfully is refused here, while
/// nothing has moved, naming the line; the caret and anchor are put back clamped, and the
/// generation goes past the incumbent's so no row it said can repaint this one.
// WL-SWITCH-05 -- agents/workshop/editor-switch.md
inline void EditorPaneWeave::on(const EditorAdoptRequested& asked, loom::Mail& mail) {
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
inline void EditorPaneWeave::on(const EditorLiveRequested& asked, loom::Mail& mail) {
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

// ---- The switch's helpers ----------------------------------------------------------------

/// WHILE A SWITCH HOLDS THIS EDITOR STILL, an input that would change the document is counted
/// and not applied. True when it was refused.
inline bool EditorPaneWeave::held_still() {
    if (!holding_.active) {
        return false;
    }
    ++holding_.refused;
    return true;
}

/// A STATE NO SWITCH MAY BEGIN IN, in words; empty when a switch may begin.
inline std::string EditorPaneWeave::handoff_refusal() const {
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

/// WHAT A SWITCH AWAY WOULD LOSE: nothing. The standard model IS the transfer's model, so the
/// digest a switch away names is the digest of no losses, whatever the document holds.
inline std::vector<std::string> EditorPaneWeave::handoff_losses() const {
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

inline EditorTransfer EditorPaneWeave::transfer_now() const {
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

// ---- The document doors --------------------------------------------------------------

/// What this spelling means here, or why it means nothing (WL-EDIT-06): an absolute path is
/// itself; a relative one is the project's file, refused until the owner has answered, since
/// resolving it against the process directory would silently open, and save to, another
/// file. An owner that named no project root keeps its policy: the spelling is spent as is.
inline Written EditorPaneWeave::resolve(const std::string& requested, std::string& out) const {
    if (!requested.empty() && !std::filesystem::path(requested).is_absolute() &&
        !project_known_) {
        // THE REASON FIRST, THE FILE AFTER IT, in every refusal of an open: a requester's
        // row is cut at its width from the end, and the file is what a weaver can lose.
        return Written::no("a relative path means nothing until this Editor is told where "
                           "this run began -- open " +
                           requested + " by its full path");
    }
    out = ws::persist::resolved_against(project_dir_, requested);
    return Written::ok();
}

inline Plan EditorPaneWeave::judge_source(const std::string& requested) {
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
        // in it: replacing the document under it would strand a weaver's own paste. The
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

/// THE CANDIDATE BECOMES THE DOCUMENT -- inside the publication hook, and nowhere else.
/// A same-path candidate changes nothing but the notice; any other replaces the whole
/// document, retiring the old one's paste and sweep with it.
inline void EditorPaneWeave::activate() {
    if (!candidate_.same_path) {
        e_ = std::move(candidate_.doc);
        ++saved_stamp_;
        drag_ = Drag{};
        paste_ = Paste{};
        grab_ = Grab{};
        installed_epoch_ = e_.doc_epoch;
        installed_revision_ = e_.buffer.revision();
    }
    // THE ROOM IS THE TRIAL'S, NOW: the desk seated this pane as it published, so a gesture
    // before the desk's own room grant is judged against the picture showing. A candidate
    // composed as a picture brings the canvas room it was drawn for, numbered afresh; one
    // composed as rows for a pane drawing on a canvas leaves its room, and the desk grants it
    // another. Either way nothing aimed at the document it replaced counts.
    granted_ = true;
    if (candidate_.room.grant > 0) {
        canvas_ = candidate_.room;
        pictures_ = ws::CanvasPictures{};
    } else {
        pictures_.retire();
    }
    held_ = 0;
    if (candidate_.room.grant > 0 || canvas_.grant <= 0) {
        rows_ = candidate_.rows;
        columns_ = candidate_.columns;
    }
    chrome_rows_ = candidate_.chrome_rows;
    doc_rows_ = candidate_.doc_rows;
    opened_by_ = candidate_.op;
    candidate_ = Candidate{};
    notice((e_.dirty() ? "UNSAVED edits stand -- editing " : "editing ") + shown_path(),
           false);
}

} // namespace

#endif // ZENGINE_EDITOR_PANE_OPENING_SWITCH_HPP
