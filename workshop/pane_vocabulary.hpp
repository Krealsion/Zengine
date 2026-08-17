// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_PANE_VOCABULARY_HPP
#define ZENGINE_WORKSHOP_PANE_VOCABULARY_HPP

// THE WHOLE PROTOCOL BETWEEN WORKSHOP AND A WEAVE THAT OFFERS IT A PANE (WP-0).
// Four shapes, and there is not a fifth.
//
//     PaneCatalogRequested   Workshop  ->  everyone   "who has panes?"
//     PaneOffered            provider  ->  Workshop   "I have this one."
//     PaneRoom               Workshop  ->  provider   "here is how much prose it gets."
//     PaneContent            provider  ->  Workshop   "here is what it says."
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
// ---- THE SHAPES THAT ARE DELIBERATELY ABSENT --------------------------------
//
// No `PanePressed` and no input of any kind; no focus, capture, hotkey or
// gesture; no `PaneClosed`, `PaneUnavailable` or unload notification (Loom gives
// Workshop no participant-visible provider-unload event, and manufacturing one
// out of silence is the exact dishonesty WP-R0 was corrected for); no
// `PaneInstance`, because one `PaneRef` is one presentation; no `PaneConfig`,
// because no consumer has asked; no generic request/response envelope, because
// four named shapes are four readable sentences and an envelope is a framework.

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

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_PANE_VOCABULARY_HPP
