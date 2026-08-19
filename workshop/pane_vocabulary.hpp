// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_PANE_VOCABULARY_HPP
#define ZENGINE_WORKSHOP_PANE_VOCABULARY_HPP

// THE WHOLE PROTOCOL BETWEEN WORKSHOP AND A WEAVE THAT OFFERS IT A PANE (WP-0,
// widened once by SEL-0). Five shapes, and there is not a sixth.
//
//     PaneCatalogRequested   Workshop  ->  everyone   "who has panes?"
//     PaneOffered            provider  ->  Workshop   "I have this one."
//     PaneRoom               Workshop  ->  provider   "here is how much prose it gets."
//     PaneContent            provider  ->  Workshop   "here is what it says."
//     PanePressed            Workshop  ->  provider   "a maker pressed here, in that room."
//
// THE FIFTH IS THE FOURTH'S BUDGET READ BACKWARDS, which is what made it the one
// worth adding. `PaneRoom` grants a lattice of prose rows and columns; `PanePressed`
// names a place IN that lattice and says nothing else. Workshop already owns the
// geometry that answers WHICH pane a press landed on -- `bounds_of` -> `contains`,
// the same rectangle the painter used -- so the synchronous half of the question
// never crosses the wire and no `consumed` ever comes back (WP-R0). A press that
// reaches a provider was consumed at the Workshop boundary before it was sent.
//
// ---- THERE IS NO PROVIDER FIELD, AND THAT IS THE POINT ----------------------
//
// `PaneOffered` and `PaneContent` say WHICH PANE and never WHOSE. The provider
// half of the durable `PaneRef` (setup.hpp) is `mail.authored_role()` -- the
// office Loom verified at the moment the sentence was authored, carried as
// DELIVERY PROVENANCE that no payload can write and no sender can choose
// (MSG-07). A `provider` field here would be a second answer to a question that
// already has one, and the second answer is the forgeable one: any weave granted
// the shape could name somebody else's office in it and Workshop would have no
// way to tell.
//
// So the absence is enforcement rather than economy. There is nothing to compare
// against the stamp, because there is nothing to compare.
//
// WHAT A LOOM ROLE PROVES, EXACTLY. That the sender held this office at the
// moment it spoke, on this bus, in this process. It is a LIVE, REPLACEMENT-STABLE
// SERVICE ROUTE. It is NOT a package author, a signature, a publisher, a
// marketplace identity, or evidence that the same author came back after a
// restart. Nothing in WP-0 claims any of those and nothing here may grow to.
//
// ---- WHAT PANEROOM IS NOT ---------------------------------------------------
//
// A budget of PROSE ROWS AND COLUMNS, resolved by Workshop through the one
// `surface::fit_region` call every other bounded region in this application goes
// through. Not cells, not pixels, not an extent, not a font, not a rectangle, and
// not the identity of the medium that answered. A provider that knew any of those
// could compute a second layout, and two parties measuring one region is the
// defect `SurfaceTextRegion`'s own doc comment exists to forbid.
//
// ---- WHAT PANECONTENT IS NOT ------------------------------------------------
//
// `surface::SurfaceTextRow` values, reused directly rather than mirrored into a
// parallel row type -- so a provider's row carries the same semantic `role` and
// `background` every first-party row does, and the Skin's palette answers for it
// unchanged. A provider supplies no `SurfaceRect`, no `SurfaceLabel`, no
// `SurfaceTextRegion`, no coordinate, no z-order, no viewport and no caret.
// Workshop assembles ONE `SurfaceCanvas`; nothing here is a second publisher.
//
// ---- WHAT PANEPRESSED IS NOT ------------------------------------------------
//
// A PRIMARY PRESS, and the shape says so by having nowhere to say anything else.
// No button field, no pressed/released field, no modifier field, no timestamp and
// no gesture identity: SEL-0 earned exactly one gesture, so the shape's ARRIVAL is
// the gesture and a reader has nothing to switch on. A release, a second button, a
// wheel, a hover, a double press and a drag all remain unsayable here, which is
// what keeps "a provider gets input now" from meaning anything wider than the one
// sentence above.
//
// AND IT IS NOT FOCUS. Receiving a press makes a pane the target of nothing else:
// no keyboard reaches it, no key shape exists to reach it with, no pane is
// focused, nothing is captured, and Workshop keeps no record of which pane a maker
// touched last. The press is resolved, sent, and forgotten in the same statement.
//
// ---- THE SHAPES THAT ARE DELIBERATELY ABSENT --------------------------------
//
// No keyboard, focus, capture, hotkey, wheel or drag shape of any kind; no
// `PaneClosed`, `PaneUnavailable` or unload notification (Loom gives Workshop no
// participant-visible provider-unload event, and manufacturing one out of silence
// is the exact dishonesty WP-R0 was corrected for); no `PaneInstance`, because one
// `PaneRef` is one presentation; no `PaneConfig`, because no consumer has asked;
// no generic request/response envelope, because five named shapes are five
// readable sentences and an envelope is a framework.

#include "surface/vocabulary.hpp"

#include <zen/weave/shape.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace zengine::workshop {

/// WORKSHOP ASKING THE ROOM WHO HAS PANES. Office-published, because Workshop
/// does not know any provider's role yet -- discovering that is the whole job.
///
/// It carries nothing. A field on it would be a filter, and a filter is a policy
/// about which providers may answer that nothing has asked for.
struct PaneCatalogRequested {
    ZEN_SHAPE(PaneCatalogRequested, 1);
};

/// A PROVIDER NAMING ONE PANE IT CAN PRESENT -- its key in that provider's own
/// namespace, and the two lines a maker reads in the picker.
///
/// WHOSE it is, is `mail.authored_role()`. See the header comment.
struct PaneOffered {
    std::string pane;    ///< the durable pane key, in the AUTHORING office's namespace
    std::string name;    ///< what the picker lists
    std::string summary; ///< one line, so a maker can tell what they are opening
    ZEN_SHAPE(PaneOffered, 1, ZEN_FIELD(pane), ZEN_FIELD(name), ZEN_FIELD(summary));
};

/// HOW MUCH PROSE WORKSHOP IS GRANTING THIS PANE, and the whole of what the
/// provider is told about where it is.
///
/// `rows` and `columns` are exactly the `surface::RegionFit` Workshop resolved
/// for the pane's BODY on the current screen with the current medium's text
/// metric. Sent when the pane opens, when a valid re-offer refreshes it, and
/// when that resolution changes -- and at no other time, so a resize that leaves
/// prose capacity equal says nothing.
struct PaneRoom {
    std::string pane;
    std::int64_t rows = 0;
    std::int64_t columns = 0;
    ZEN_SHAPE(PaneRoom, 1, ZEN_FIELD(pane), ZEN_FIELD(rows), ZEN_FIELD(columns));
};

/// WHAT THE PANE SAYS, inside the budget it was granted.
///
/// The rows are the provider's semantic material and Workshop copies them only
/// after they have passed the CURRENT room's bounds. Over-budget content is
/// refused whole rather than truncated: a pane presenting an unmarked partial
/// sentence as a provider's complete answer is the failure `detail::fit` exists
/// to prevent one row at a time, and truncation here would commit it silently.
struct PaneContent {
    std::string pane;
    std::vector<surface::SurfaceTextRow> rows;
    ZEN_SHAPE(PaneContent, 1, ZEN_FIELD(pane), ZEN_FIELD(rows));
};

/// A MAKER PRESSED INSIDE THE ROOM THIS PANE WAS GRANTED -- which pane, and where.
///
/// `row` AND `column` ARE THE `PaneRoom` LATTICE, AND NOTHING ELSE IS. Row 0 is the
/// first prose row of the BODY this provider was granted -- under Workshop's header
/// row, which the provider never received and is never told about -- and the pair
/// is inside `[0, rows) x [0, columns)` of the room currently in force. Workshop
/// resolves it through the same `external_body_place` that granted that room and
/// the same `prose_at` every bounded region in this application locates a press
/// with, so one measurer answers where a row is drawn and where a hand meets it.
/// No pixel, no cell, no canvas coordinate, no window origin, no pane rectangle,
/// no chrome geometry and no medium identity: a provider that learned any of those
/// could place itself on a screen it has no business seeing.
///
/// A PRESS THAT NAMES NO ROW IS NOT SENT. The header row, the padding below the
/// last prose row of a graphical medium, and anything outside the granted lattice
/// are all still consumed by the pane -- a pane that owns visible room owns
/// pointer refusal for that room -- and simply produce no sentence. Workshop does
/// not round them to a nearest row: a strip too short to fit prose is not a row,
/// and inventing one would hand a provider a press at a place it never wrote to.
///
/// WORKSHOP SENDS IT AND ASKS NOTHING BACK. No reply, no acknowledgement, no
/// disposition and no `consumed`: what a press MEANS is the provider's vocabulary
/// and Workshop holds none of it. If the answer is a changed presentation it
/// arrives as an ordinary `PaneContent`, judged against the same room as any other.
struct PanePressed {
    std::string pane;
    std::int64_t row = 0;
    std::int64_t column = 0;
    ZEN_SHAPE(PanePressed, 1, ZEN_FIELD(pane), ZEN_FIELD(row), ZEN_FIELD(column));
};

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_PANE_VOCABULARY_HPP
