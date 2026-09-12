// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_OPEN_SEAM_VOCABULARY_HPP
#define ZENGINE_WORKSHOP_OPEN_SEAM_VOCABULARY_HPP

// EXPERIMENTAL (editor-managed-open-slice, 2026-09-11) — THE MANAGED OPENING'S CONVERSATION.
//
// Opening a source B while A is current has ONE observable commitment boundary. Before it,
// A is the current readable/editable document and B — with its trial presentation — is a
// candidate nobody can read; at it, B's document, its admitted content and caret, its room
// and the keyboard agree; the terminal answer reports that fact afterwards. A refused open
// preserves A and every legitimate intervening edit, and moves no setup and no focus.
//
// THREE OWNERS, ONE MANAGER, AND WHAT EACH ONE SAYS:
//
//     requester (Files, the Builder)  ->  zengine.opening   OpenSourceRequested{path}
//     manager   ->  zengine.workshop  PresentationTrialRequested   -> PresentationTrial
//     manager   ->  zengine.editor    PrepareSourceRequested       -> SourcePrepared
//     manager   ->  zengine.workshop  PresentationAdmitRequested   -> PresentationAdmitted
//     manager   ->  (the bus)         commit_joint                  (the commitment)
//     manager   ->  both owners       ManagedOpenProgress{apply}    (each is shown its claim)
//     (the bus) ->  manager           zen.JointApplied              (what the showings came to)
//     manager   ->  both owners       ManagedOpenSettled            (the fact, afterwards)
//     manager   ->  requester         SourceOpened                  (the answer, afterwards)
//     manager   ->  zengine.workshop  ManagedOpenProgress           (what a maker can read)
//
// The Editor owns source admission and the document; Workshop (the presentation owner
// today) owns the trial and admitted presentation, room, focus and input routing; the
// manager owns the intent, the exact participants, the stage, the outstanding attempt,
// supersession and the outcome it can establish. Workshop keeps application authority:
// it mounts the manager, mints its authority and grants its speech.
//
// THE TWO FACTS THAT CHANGE TOGETHER ARE LATEST CLAIMS (Loom Senses), published jointly:
// the Editor claims `EditorDocument` and Workshop claims `PanePresentation`. Each owner
// PREPARES its half natively — the Editor a whole candidate document, Workshop a trial
// seat with B's rows — and OFFERS a small identity value for the exact operation; the bus
// exchanges the two identities in one protected step and shows each owner its published
// value before that owner runs again. The heavy state never crosses and never doubles:
// the identity in the claim is what the native state is DERIVED FROM after commitment.
//
// PUBLICATION IS NOT APPLICATION (editor-managed-open-slice-corrections). The commitment
// publishes both identities; each owner is then SHOWN its published claim by the bus before
// it runs again, and what that showing came to is a second fact the bus records and tells
// the manager (`zen.JointApplied`). The manager answers the requester only from that fact:
// `SourceOpened{accepted}` means the publication committed AND both owners applied it. An
// owner whose showing failed is HELD by the bus (nothing is delivered to it until it is
// reloaded or removed), the answer says which owner and why, and nothing is rolled back --
// a published claim is not the manager's to unpublish. Silence stays pending, as before.
//
// THE SHOWING HAS THREE ANSWERS, AND THE RECORD OUTLIVES THE PUBLICATION (editor-managed-
// open-slice-corrections-2). An owner applies the published claim (Applied), or answers that
// it keeps state of its own (Declined: functioning, not held, re-claiming its own truth at its
// next delivery -- what a successor says when shown a value its predecessor prepared), or
// cannot complete the showing (Failed: held). A repaired owner is not proof that its old
// operation applied: the manager re-reads the record, which the bus KEEPS until the manager
// releases it -- so an unrelated coordination begun meanwhile takes nothing from it, and the
// bus's notice always finds what it names. The manager releases every record it consumed and
// retains at most one, a commitment an owner could not apply, for the late word about that
// owner's repair: "applied after repair", or "not applied after repair".
//
// THE OLD DOOR KEEPS ITS PROMISE. `OpenSourceRequested` at `zengine.editor` -- the address
// every requester used before the manager existed -- still coordinates the document AND
// its presentation, or refuses truthfully: the Editor keeps the requester's deferred answer
// right, asks `zengine.opening` in a conversation of its own (its own correlation, its own
// attempt), and answers the requester with the manager's outcome through that right. It is
// never a document-only act, and a relay that cannot be made (no opening office to ask, a
// refused attempt, a manager replaced while asked) is refused in words -- never silence and
// never a fallback install. The opening office itself may not ask that door: it asks the
// Editor to PREPARE, and a loop is refused by name.
//
// ⚠ WHAT IS DELIBERATELY NOT HERE: no reveal ask and no held-gesture queue (input applies
// to the current document immediately and invalidates preparation at the owners' doors);
// no rollback (nothing moves before commitment); no timeout, retry or recovery framework
// (a pending open is a truthful, bounded, inspectable state).

#include "pane_vocabulary.hpp"

#include <zen/weave/shape.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace zengine::workshop {

/// THE OFFICE THAT MANAGES OPENINGS. A ROLE, for `kEditorRole`'s reason. Held today by a
/// native weave the host mounts beside Workshop; movable.
inline constexpr const char* kOpeningRole = "zengine.opening";

// ---- The two jointly published claims ----------------------------------------------------

/// THE EDITOR'S DURABLE DOCUMENT FACTS -- its latest claim, and the whole of what the
/// commitment publishes on its side. Small on purpose: identity, generation, convention,
/// the content revision (so any edit moves it) and whether the bytes match their file.
/// The body (bytes, history, caret, viewport) stays the Editor's, derived from this.
struct EditorDocument {
    std::string path;                 ///< normalized identity; empty = no source open
    std::int64_t doc_epoch = 0;       ///< the generation every projection of it carries
    std::int64_t convention = 0;      ///< line ending
    std::int64_t content_revision = 0; ///< moves on every mutation of the bytes
    bool dirty = false;               ///< bytes differ from the saved comparison
    std::int64_t opened_by = 0;       ///< the managed operation that installed it, or 0
    ZEN_SHAPE(EditorDocument, 1, ZEN_FIELD(path), ZEN_FIELD(doc_epoch), ZEN_FIELD(convention),
              ZEN_FIELD(content_revision), ZEN_FIELD(dirty), ZEN_FIELD(opened_by));
};

/// THE PRESENTATION OWNER'S FACTS ABOUT ONE MANAGED PANE -- its latest claim. Derived from
/// the live session at the end of every delivery and claimed only when it changed, so its
/// revision moves exactly when a fact a prepared open depends on moved: membership, seat,
/// selection, keys, room, the admitted content's generation, the count of inputs routed to
/// the pane (admitted work not yet delivered), the stack capacity and the setup digest.
struct PanePresentation {
    std::string provider;
    std::string pane;
    bool member = false;             ///< named by the active setup
    bool seated = false;             ///< on the desk
    bool selected = false;
    bool keyboard = false;           ///< the keys point at it
    std::int64_t rows = 0;           ///< the granted room, or 0
    std::int64_t columns = 0;
    std::int64_t content_generation = 0; ///< doc epoch of the admitted rows, or 0
    std::int64_t routed = 0;         ///< inputs routed to this pane, ever
    std::int64_t capacity = 0;       ///< overlay slots the screen has now
    std::int64_t setup_digest = 0;   ///< the active setup's membership and places, hashed
    std::int64_t shown_by = 0;       ///< the managed operation that seated it, or 0
    ZEN_SHAPE(PanePresentation, 1, ZEN_FIELD(provider), ZEN_FIELD(pane), ZEN_FIELD(member),
              ZEN_FIELD(seated), ZEN_FIELD(selected), ZEN_FIELD(keyboard), ZEN_FIELD(rows),
              ZEN_FIELD(columns), ZEN_FIELD(content_generation), ZEN_FIELD(routed),
              ZEN_FIELD(capacity), ZEN_FIELD(setup_digest), ZEN_FIELD(shown_by));
};

// ---- Manager -> presentation owner --------------------------------------------------------

/// WOULD THIS PANE SEAT, AND WHAT ROOM WOULD IT GET? Judged on a copy of the setup through
/// the picker's own trial; nothing moves. The answer carries the room the pane's body
/// would have after seating, so the document owner can compose for it.
struct PresentationTrialRequested {
    std::int64_t op = 0;
    std::string provider;
    std::string pane;
    ZEN_SHAPE(PresentationTrialRequested, 1, ZEN_FIELD(op), ZEN_FIELD(provider), ZEN_FIELD(pane));
};

struct PresentationTrial {
    std::int64_t op = 0;
    bool ok = false;
    std::string refusal;
    std::int64_t rows = 0;
    std::int64_t columns = 0;
    ZEN_SHAPE(PresentationTrial, 1, ZEN_FIELD(op), ZEN_FIELD(ok), ZEN_FIELD(refusal),
              ZEN_FIELD(rows), ZEN_FIELD(columns));
};

/// ADMIT THIS CONTENT AS THE TRIAL'S, AND OFFER THE PRESENTATION. The rows are the
/// document owner's composition of the candidate for the trial room; the presentation
/// owner judges them against the room it would grant, keeps them as the trial's content,
/// and offers its prepared claim for the exact operation. Still nothing moves.
struct PresentationAdmitRequested {
    std::int64_t op = 0;
    std::string provider;
    std::string pane;
    std::int64_t generation = 0;
    std::vector<surface::SurfaceTextRow> rows;
    std::int64_t caret_row = -1;
    std::int64_t caret_col = 0;
    std::int64_t sel_begin_row = -1;
    std::int64_t sel_begin_col = 0;
    std::int64_t sel_end_row = -1;
    std::int64_t sel_end_col = 0;
    ZEN_SHAPE(PresentationAdmitRequested, 1, ZEN_FIELD(op), ZEN_FIELD(provider), ZEN_FIELD(pane),
              ZEN_FIELD(generation), ZEN_FIELD(rows), ZEN_FIELD(caret_row), ZEN_FIELD(caret_col),
              ZEN_FIELD(sel_begin_row), ZEN_FIELD(sel_begin_col), ZEN_FIELD(sel_end_row),
              ZEN_FIELD(sel_end_col));
};

struct PresentationAdmitted {
    std::int64_t op = 0;
    bool ok = false;
    std::string refusal;
    ZEN_SHAPE(PresentationAdmitted, 1, ZEN_FIELD(op), ZEN_FIELD(ok), ZEN_FIELD(refusal));
};

// ---- Manager -> document owner ------------------------------------------------------------

/// PREPARE THIS SOURCE AS A CANDIDATE FOR THE TRIAL ROOM, AND OFFER THE DOCUMENT. The
/// Editor resolves the path, judges eligibility (a dirty document refuses, a paste still
/// arriving refuses), reads and admits the file, builds a whole candidate document beside
/// the current one, composes it for `rows` x `columns`, and offers its prepared claim for
/// the exact operation. The current document is untouched and stays editable.
struct PrepareSourceRequested {
    std::int64_t op = 0;
    std::string path;
    std::int64_t rows = 0;
    std::int64_t columns = 0;
    ZEN_SHAPE(PrepareSourceRequested, 1, ZEN_FIELD(op), ZEN_FIELD(path), ZEN_FIELD(rows),
              ZEN_FIELD(columns));
};

struct SourcePrepared {
    std::int64_t op = 0;
    bool ok = false;
    std::string refusal;
    std::int64_t generation = 0; ///< the candidate's doc epoch
    std::vector<surface::SurfaceTextRow> rows;
    std::int64_t caret_row = -1;
    std::int64_t caret_col = 0;
    std::int64_t sel_begin_row = -1;
    std::int64_t sel_begin_col = 0;
    std::int64_t sel_end_row = -1;
    std::int64_t sel_end_col = 0;
    ZEN_SHAPE(SourcePrepared, 1, ZEN_FIELD(op), ZEN_FIELD(ok), ZEN_FIELD(refusal),
              ZEN_FIELD(generation), ZEN_FIELD(rows), ZEN_FIELD(caret_row), ZEN_FIELD(caret_col),
              ZEN_FIELD(sel_begin_row), ZEN_FIELD(sel_begin_col), ZEN_FIELD(sel_end_row),
              ZEN_FIELD(sel_end_col));
};

// ---- Manager -> both owners, afterwards ------------------------------------------------------

/// THE OPERATION ENDED. `committed` says the joint publication happened; `applied` says
/// every owner was shown its published claim and completed the showing -- both were shown
/// before this arrived, so this is bookkeeping and a sentence, never the commitment. A
/// refusal, and a commitment an owner could not apply, carry the reason a maker reads.
/// (`applied` was added by editor-managed-open-slice-corrections to this still-unpublished
/// experimental shape; nothing outside the experiment ever admitted version 1.)
struct ManagedOpenSettled {
    std::int64_t op = 0;
    bool committed = false;
    bool applied = false;
    std::string refusal;
    std::string path;
    ZEN_SHAPE(ManagedOpenSettled, 1, ZEN_FIELD(op), ZEN_FIELD(committed), ZEN_FIELD(applied),
              ZEN_FIELD(refusal), ZEN_FIELD(path));
};

/// WHAT THE MANAGER IS WAITING ON, said to the presentation owner so a maker can read it:
/// the stage, and the office the outstanding attempt was addressed to. A pending open is a
/// standing condition on the desk until it settles. At `apply` it is said to BOTH owners:
/// that delivery is what shows each its published claim (the bus shows before it delivers),
/// and the manager then waits on the bus's word of what the showings came to.
struct ManagedOpenProgress {
    std::int64_t op = 0;
    std::string path;
    std::string stage;    ///< trial | prepare | admit | apply
    std::string awaiting; ///< the office addressed -- at `apply`, both owners -- or empty
    bool pending = true;  ///< false retracts the condition
    ZEN_SHAPE(ManagedOpenProgress, 1, ZEN_FIELD(op), ZEN_FIELD(path), ZEN_FIELD(stage),
              ZEN_FIELD(awaiting), ZEN_FIELD(pending));
};

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_OPEN_SEAM_VOCABULARY_HPP
