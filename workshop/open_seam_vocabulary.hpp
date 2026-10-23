// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_OPEN_SEAM_VOCABULARY_HPP
#define ZENGINE_WORKSHOP_OPEN_SEAM_VOCABULARY_HPP

// The managed opening's conversation (WL-OPEN, agents/workshop/opening.md). A requester asks
// `zengine.opening`; the manager trials the seat with Workshop (which says the room the pane's area
// will have), has the Editor prepare the candidate and Workshop admit the seat, publishes both
// owners' claims in one protected step, and answers from the bus's record that both applied them.
// The Editor draws the candidate in its own area as it commits its claim; nothing moves before the
// commitment, nothing is rolled back after it, and a pending open stays inspectable.

#include "pane_vocabulary.hpp"

#include <zen/weave/shape.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace zengine::workshop {

/// The office that manages openings -- a role, so its holder can move.
inline constexpr const char* kOpeningRole = "zengine.opening";

// ---- The two jointly published claims ----------------------------------------------------

/// The Editor's latest claim: the small document facts the commitment publishes. The body
/// (bytes, history, caret, viewport) stays the Editor's, derived from this.
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

/// The presentation owner's latest claim about one managed pane, derived at the end of every
/// delivery and claimed only when it changed -- so its revision moves exactly when a fact a
/// prepared open depends on moved.
struct PanePresentation {
    std::string provider;
    std::string pane;
    bool member = false;             ///< named by the active setup
    bool seated = false;             ///< on the desk
    bool selected = false;
    bool keyboard = false;           ///< the keys point at it
    std::int64_t rows = 0;           ///< the rows and columns of its body as placed, or 0
    std::int64_t columns = 0;
    std::int64_t content_generation = 0; ///< doc epoch the opening that seated it admitted, or 0
    std::int64_t routed = 0;         ///< inputs routed to this pane, ever
    std::int64_t capacity = 0;       ///< overlay slots the screen has now
    std::int64_t setup_digest = 0;   ///< the active setup's membership, places, sizes and ranks, hashed
    std::int64_t shown_by = 0;       ///< the managed operation that seated it, or 0
    ZEN_SHAPE(PanePresentation, 1, ZEN_FIELD(provider), ZEN_FIELD(pane), ZEN_FIELD(member),
              ZEN_FIELD(seated), ZEN_FIELD(selected), ZEN_FIELD(keyboard), ZEN_FIELD(rows),
              ZEN_FIELD(columns), ZEN_FIELD(content_generation), ZEN_FIELD(routed),
              ZEN_FIELD(capacity), ZEN_FIELD(setup_digest), ZEN_FIELD(shown_by));
};

// ---- Manager -> presentation owner --------------------------------------------------------

/// Would this pane seat, and with what room? Judged on a copy of the setup through the launch
/// door's trial; nothing moves.
struct PresentationTrialRequested {
    std::int64_t op = 0;
    std::string provider;
    std::string pane;
    ZEN_SHAPE(PresentationTrialRequested, 1, ZEN_FIELD(op), ZEN_FIELD(provider), ZEN_FIELD(pane));
};

/// The room the pane's area would have once seated: its size in pixels, as Workshop would place
/// it, and how the window that shows the desk sets text there (`graphics::AreaRoom`'s measure) --
/// none where no window shows it, and the cell's where its unit is the cell.
struct OpeningRoom {
    std::int64_t width = 0, height = 0;
    std::int64_t cell_px = 0;
    std::int64_t text_advance_px = 0, text_line_px = 0;
    ZEN_SHAPE(OpeningRoom, 1, ZEN_FIELD(width), ZEN_FIELD(height), ZEN_FIELD(cell_px),
              ZEN_FIELD(text_advance_px), ZEN_FIELD(text_line_px));
};

namespace v3 {

/// The trial's answer: the seat would be made, and the room the pane's area would have; or why not.
struct PresentationTrial {
    std::int64_t op = 0;
    bool ok = false;
    std::string refusal;
    OpeningRoom room;
    ZEN_SHAPE(PresentationTrial, 3, ZEN_FIELD(op), ZEN_FIELD(ok), ZEN_FIELD(refusal),
              ZEN_FIELD(room));
};

/// Admit the trial's seat and offer the presentation claim for this operation: the candidate is
/// ready, at this generation. Nothing moves.
struct PresentationAdmitRequested {
    std::int64_t op = 0;
    std::string provider;
    std::string pane;
    std::int64_t generation = 0;
    ZEN_SHAPE(PresentationAdmitRequested, 3, ZEN_FIELD(op), ZEN_FIELD(provider), ZEN_FIELD(pane),
              ZEN_FIELD(generation));
};

} // namespace v3

struct PresentationAdmitted {
    std::int64_t op = 0;
    bool ok = false;
    std::string refusal;
    ZEN_SHAPE(PresentationAdmitted, 1, ZEN_FIELD(op), ZEN_FIELD(ok), ZEN_FIELD(refusal));
};

// ---- Manager -> document owner ------------------------------------------------------------

namespace v3 {

/// Prepare this source as a whole candidate beside the current document, composed for the room the
/// trial said, and offer the document claim. A dirty document or a paste still arriving refuses;
/// the current document stays untouched and editable. The candidate is drawn at the commitment.
struct PrepareSourceRequested {
    std::int64_t op = 0;
    std::string path;
    OpeningRoom room;
    ZEN_SHAPE(PrepareSourceRequested, 3, ZEN_FIELD(op), ZEN_FIELD(path), ZEN_FIELD(room));
};

/// The candidate prepared, or why not: its generation, nothing of what it shows.
struct SourcePrepared {
    std::int64_t op = 0;
    bool ok = false;
    std::string refusal;
    std::int64_t generation = 0; ///< the candidate's doc epoch
    ZEN_SHAPE(SourcePrepared, 3, ZEN_FIELD(op), ZEN_FIELD(ok), ZEN_FIELD(refusal),
              ZEN_FIELD(generation));
};

} // namespace v3

// ---- Manager -> both owners, afterwards ------------------------------------------------------

/// The operation ended: `committed` (published) and `applied` (every owner completed its showing,
/// before this arrived). Bookkeeping and a sentence, never the commitment.
struct ManagedOpenSettled {
    std::int64_t op = 0;
    bool committed = false;
    bool applied = false;
    std::string refusal;
    std::string path;
    ZEN_SHAPE(ManagedOpenSettled, 1, ZEN_FIELD(op), ZEN_FIELD(committed), ZEN_FIELD(applied),
              ZEN_FIELD(refusal), ZEN_FIELD(path));
};

/// What the manager is waiting on -- the stage and the office addressed -- so a weaver can read it;
/// a standing condition until it settles. At `apply` it is said to both owners: that delivery
/// is what shows each its published claim.
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
