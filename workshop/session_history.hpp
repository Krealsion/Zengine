// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_SESSION_HISTORY_HPP
#define ZENGINE_WORKSHOP_SESSION_HISTORY_HPP

// WHAT A WORKSHOP SESSION USED TO LOOK LIKE, AND HOW IT BECOMES WHAT ONE LOOKS LIKE NOW
// (MIG-0) — the whole of yesterday, in one place that is not the reader.
//
// ---- WHY THIS FILE EXISTS -------------------------------------------------
//
// `session_persist.hpp` used to carry three shapes and three roads: the current one, and
// version 1 and version 2 kept beside it so that an older file could still be read. Every
// one of those old shapes was compiled into every Workshop, into every suite that asks what
// a screen looks like, and into the reader itself — where it sat next to the CURRENT shape
// with nothing but a namespace between them.
//
// It moved here because the current reader should know exactly one thing: the shape it
// admits. What yesterday looked like, and what yesterday MEANT, is the business of whoever
// converts it — and that party is a provider artifact this host may mount, replace or not
// have at all (`workshop/session_migration_provider.cpp`, and `operator/migration.hpp` for
// the seam it arrives through).
//
// ---- WHAT MOVED, AND WHAT DELIBERATELY DID NOT ----------------------------
//
//     MOVED HERE     the version-1, version-2 and version-3 session shapes, and the exact
//                    translation of each into the current one. Version 3 arrived at
//                    WUX-10, when the layout run made the current version 4 — and it
//                    arrived by the ordinary route this file exists to be: the reader
//                    changed one number, and its retired shape was copied here verbatim
//     STAYED PUT     `setup_persist::v2` -- the whole-cell DESK. The standalone setup
//                    file still reads one, in its own owner, on its own terms
//                    (`setup_persist::setup_in_v2`), so that shape has a live consumer
//                    that has nothing to do with sessions. Copying it here would be two
//                    definitions of one historical truth, which is worse than either
//                    place could be wrong on its own. A version-1 SESSION nests a
//                    version-2 DESK, so this file names the setup owner's shape and
//                    converts it; it does not own it.
//     STAYED PUT     the CURRENT session shape, for the same reason from the other end:
//                    a conversion must name its target rather than describe one.
//
// ---- THE TRANSLATIONS ARE WIRE-SHAPE TO WIRE-SHAPE ------------------------
//
// Each function below takes an old file's shape and answers with the shape the current
// reader admits — and it stops there. It does not build a `Setup`, does not run
// `check_setup`, does not decide whether a viewport is honourable and does not touch a
// file. Everything a current session file goes through, a converted one goes through
// afterwards, unchanged and in the reader: that is what makes "the same file means the
// same thing" a structural fact rather than a promise two roads keep separately.
//
// WHICH FIELDS ARE JUDGED HERE AND WHICH ARE PASSED THROUGH is decided by one question:
// would the current reader ask the same question and give the same answer? A `format`
// word crosses untouched, because `setup_in` and the session reader both check it and
// their sentences are the ones a maker already knows. A `format_version` FIELD cannot
// cross untouched -- the target's reader demands the current number -- so it is judged
// here, against the version this edge converts, and a disagreement refuses. That is the
// forgery an old file's reader has always had to answer: an envelope claiming one version
// over a body that says another.
//
// ---- WHY THE EDGES ARE DIRECT AND STILL COMPOSE INTERNALLY ----------------
//
// Three edges leave this file — v1, v2 and v3, each straight to the current shape — and
// none of them is a route anybody searched for: `op::migrate` computes ONE identity and
// calls `find` once, so a mounted `v1 -> v3` and a mounted `v3 -> v4` would satisfy nothing
// asking for `v1 -> v4` (MIG-0's law, deliberately). What the BODY of an edge does is the
// author's business, and these bodies compose two named translations in C++ — yesterday's
// meaning said at version 3, then the one field version 4 added. That is authorship, not
// discovery: the composition is written here, by the party that supplies the edge, and it
// is what keeps v1's and v2's meaning from being transcribed a second time and drifting.
//
// ---- HOW A REFUSAL TRAVELS ------------------------------------------------
//
// By throwing. A conversion is spent through `op::Catalog`, whose evaluator contains a
// native body's throw and turns it into that evaluation's reason -- so yesterday's own
// vocabulary (`default or cells`, and the rest) reaches a maker through the ordinary
// refusal path, with the identity of the conversion that refused in front of it. Nothing
// here writes to anything, so a refusal costs exactly the sentence.

#include "operator/migration.hpp"
#include "operator/operator.hpp"
#include "session_persist.hpp"
#include "setup_persist.hpp"
#include "surface/region.hpp"

#include <zen/schema.hpp>
#include <zen/value.hpp>
#include <zen/weave/shape.hpp>

#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace zengine::workshop::session_history {

// ---- The shapes older Workshops wrote -----------------------------------------------
//
// The same trick `setup_persist::v2` documents: the C++ names live in nested namespaces,
// and `ZEN_SHAPE` stamps the BARE struct name -- so `v1::WorkshopSession` claims
// `WorkshopSession` v1 on the wire exactly as the build that wrote it did. That is what
// lets a legacy file meet ITS OWN gate at full strength -- the version, the content id and
// every field -- before one byte of it is translated.

namespace v1 {

/// WUX-0's session: the room, and a whole-cell desk. No placement existed to write.
struct WorkshopSession {
    std::string format;
    std::int64_t format_version = 0;
    session_persist::WorkshopViewport viewport;
    setup_persist::v2::WorkshopSetup desk;

    ZEN_SHAPE(WorkshopSession, 1, ZEN_FIELD(format), ZEN_FIELD(format_version),
              ZEN_FIELD(viewport), ZEN_FIELD(desk));
};

} // namespace v1

namespace v2 {

/// WUX-2's session: the desk moved to the fine lattice and this format's version moved
/// with it. Still no placement.
struct WorkshopSession {
    std::string format;
    std::int64_t format_version = 0;
    session_persist::WorkshopViewport viewport;
    setup_persist::WorkshopSetup desk;

    ZEN_SHAPE(WorkshopSession, 2, ZEN_FIELD(format), ZEN_FIELD(format_version),
              ZEN_FIELD(viewport), ZEN_FIELD(desk));
};

} // namespace v2

namespace v3 {

/// WUX-3's session, retired by WUX-10: the room, the current desk shape, and the desktop
/// placement. ONE desk — the plural arrived at version 4, and this is the shape that had
/// singular in it.
///
/// ⚠ EVERY FIELD, NAME, TYPE AND ORDER IS THE RETIRED ONE, VERBATIM. A historical shape is
/// not a description of what an old file meant; it IS the door those bytes claim, and its
/// content id is computed from all of that. The nested `WorkshopViewport`,
/// `WorkshopSetup` and `WorkshopPlacement` are named rather than copied for the same
/// reason `setup_persist::v2` was not moved here: they are unchanged, they have live
/// owners, and two definitions of one truth is worse than either place could be alone.
struct WorkshopSession {
    std::string format;
    std::int64_t format_version = 0;
    session_persist::WorkshopViewport viewport;
    setup_persist::WorkshopSetup desk;
    session_persist::WorkshopPlacement placement;

    ZEN_SHAPE(WorkshopSession, 3, ZEN_FIELD(format), ZEN_FIELD(format_version),
              ZEN_FIELD(viewport), ZEN_FIELD(desk), ZEN_FIELD(placement));
};

} // namespace v3

/// THE VERSION NUMBERS THIS FILE CONVERTS FROM. They are the edges' own, said once so the
/// shapes above and the definitions below cannot come to disagree about which vintage each
/// is.
inline constexpr std::int64_t kV1FormatVersion = 1;
inline constexpr std::int64_t kV2FormatVersion = 2;
inline constexpr std::int64_t kV3FormatVersion = 3;

static_assert(v1::WorkshopSession::zen_version == static_cast<std::uint32_t>(kV1FormatVersion),
              "a historical session shape's envelope version and the format version it "
              "converts are one number, exactly as they are for the current one");
static_assert(v2::WorkshopSession::zen_version == static_cast<std::uint32_t>(kV2FormatVersion),
              "a historical session shape's envelope version and the format version it "
              "converts are one number, exactly as they are for the current one");
static_assert(v3::WorkshopSession::zen_version == static_cast<std::uint32_t>(kV3FormatVersion),
              "a historical session shape's envelope version and the format version it "
              "converts are one number, exactly as they are for the current one");

/// ...AND THE VERSION THEY ALL CONVERT TO IS THE READER'S, NEVER A NUMBER TYPED HERE.
static_assert(session_persist::kFormatVersion > kV3FormatVersion,
              "every shape in this file is RETIRED: the current reader's version must be "
              "ahead of all of them, or one of these is not history");

// ---- Translating ---------------------------------------------------------------------

/// What to say about an old file whose own `format_version` field is not the version its
/// envelope claimed. One sentence, one place, because both edges can meet it.
///
/// ONLY A FORGERY PRODUCES ONE, which is exactly why it is checked: the envelope's claim
/// selected this conversion, and a body that then says it is a different vintage is a file
/// asking to be translated by a road it does not belong to.
inline std::string mismatched_version(const char* what, std::int64_t claimed,
                                      std::int64_t found) {
    return std::string("this ") + what + " claims version " + std::to_string(claimed) +
           " and its own format_version field says " + std::to_string(found);
}

/// THE PLACEMENT EVERY HISTORICAL SESSION HAS: none, in the one spelling that means it.
///
/// Loom's admission has no optional fields, so "no placement was ever reported" has to be
/// WRITTEN, and it has exactly one legal spelling: mode `none`, both numbers zero, window
/// `normal`. `session_persist::placement_in` refuses every other way of saying it, so this
/// is not a convenience -- it is the only value the reader on the far side will accept for
/// a file that never had one.
inline session_persist::WorkshopPlacement absent_placement() {
    session_persist::WorkshopPlacement none;
    none.mode = session_persist::kPlacementNone;
    none.x = 0;
    none.y = 0;
    none.window = session_persist::kWindowNormal;
    return none;
}

/// ONE AUTHORED PLACE, SAID IN THE FINE LATTICE'S WORDS.
///
/// Version 2 knew two place words and this is both of them. An unknown one refuses in
/// VERSION 2'S OWN VOCABULARY, because the file being read is a version-2 file and telling
/// its author about `subcells` would be answering a question they did not ask.
inline setup_persist::WorkshopPanePlace
place_v2_to_v3(const setup_persist::v2::WorkshopPanePlace& w) {
    setup_persist::WorkshopPanePlace out;
    if (w.mode == setup_persist::kUnitDefault) {
        out.mode = setup_persist::kUnitDefault;
        out.x = w.x;
        out.y = w.y;
        return out;
    }
    if (w.mode == setup_persist::v2::kUnitCells) {
        out.mode = setup_persist::kUnitSubcells;
        out.x = surface::subs_of_cells(w.x);
        out.y = surface::subs_of_cells(w.y);
        return out;
    }
    throw std::invalid_argument(
        setup_persist::unknown_unit(w.mode, "place", setup_persist::v2::kPlaceWords));
}

/// ONE AUTHORED SIZE, the same way — with `pixels`, which both versions spell identically
/// and which crosses unscaled because a device pixel did not change size when the lattice
/// got finer.
inline setup_persist::WorkshopPaneSize
size_v2_to_v3(const setup_persist::v2::WorkshopPaneSize& w, const char* which) {
    setup_persist::WorkshopPaneSize out;
    if (w.mode == setup_persist::kUnitDefault) {
        out.mode = setup_persist::kUnitDefault;
        out.amount = w.amount;
        return out;
    }
    if (w.mode == setup_persist::v2::kUnitCells) {
        out.mode = setup_persist::kUnitSubcells;
        out.amount = surface::subs_of_cells(w.amount);
        return out;
    }
    if (w.mode == setup_persist::kUnitPixels) {
        out.mode = setup_persist::kUnitPixels;
        out.amount = w.amount;
        return out;
    }
    throw std::invalid_argument(
        setup_persist::unknown_unit(w.mode, which, setup_persist::v2::kSizeWords));
}

/// A WHOLE-CELL DESK AS A FINE-LATTICE ONE — the exact multiply WUX-2 shipped, moved
/// beside the session history that is now its only session-side consumer.
///
/// `cells` amounts become `subcells` amounts times `surface::kCellSubs`, saturating, so
/// every whole-cell value lands on the fine lattice's exact cell boundary and a migrated
/// desk resolves to the identical pixels and the identical characters. `pixels` amounts
/// are device pixels in both versions and cross unscaled; `default` carries its numbers
/// through untouched, because both versions read `default` the same way.
///
/// THE FORMAT WORD IS NOT JUDGED HERE. `setup_persist::setup_in` asks whether a desk says
/// it is a Workshop setup, in a sentence a maker has been reading since WS-0, and it will
/// ask it of this desk a moment from now -- so asking it twice would be two answers waiting
/// for one of them to be edited.
inline setup_persist::WorkshopSetup desk_v2_to_v3(const setup_persist::v2::WorkshopSetup& old) {
    if (old.format_version != setup_persist::kLegacyFormatVersion) {
        throw std::invalid_argument(
            mismatched_version("desk", setup_persist::kLegacyFormatVersion,
                               old.format_version));
    }
    setup_persist::WorkshopSetup out;
    out.format = old.format;
    out.format_version = setup_persist::kFormatVersion;
    out.name = old.name;
    out.panes.reserve(old.panes.size());
    for (const setup_persist::v2::WorkshopSetupPane& p : old.panes) {
        setup_persist::WorkshopSetupPane row;
        row.provider = p.provider;
        row.pane = p.pane;
        row.front = p.front;
        row.place = place_v2_to_v3(p.place);
        row.width = size_v2_to_v3(p.width, "width");
        row.height = size_v2_to_v3(p.height, "height");
        out.panes.push_back(std::move(row));
    }
    return out;
}

/// A VERSION-1 SESSION AS A VERSION-3 ONE — yesterday's meaning, said in the last shape
/// that had a single desk in it.
///
/// The room crosses unchanged; the desk goes through the multiply above; and the placement
/// reads as THE CANONICAL ABSENCE, because nothing in a version-1 file could have said one
/// and a value nobody means must have exactly one spelling (WIND-2's law, which
/// `session_persist::placement_in` enforces at the other end).
///
/// IT ANSWERS AT VERSION 3 AND NOT AT THE READER'S VERSION, since WUX-10. This is the half
/// of the translation that is about WHAT A VERSION-1 FILE MEANT, and that meaning did not
/// change when the current format grew a layout run — so it is written once, here, and the
/// growth is applied to it by `session_v3_to_v4`.
inline v3::WorkshopSession session_v1_to_v3(const v1::WorkshopSession& old) {
    if (old.format_version != kV1FormatVersion) {
        throw std::invalid_argument(
            mismatched_version("session", kV1FormatVersion, old.format_version));
    }
    v3::WorkshopSession out;
    out.format = old.format;
    out.format_version = kV3FormatVersion;
    out.viewport = old.viewport;
    out.desk = desk_v2_to_v3(old.desk);
    out.placement = absent_placement();
    return out;
}

/// A VERSION-2 SESSION AS A VERSION-3 ONE — that format's version, plus the placement it
/// never had.
///
/// THE DESK CROSSES WHOLE AND UNJUDGED, and that is the accurate translation rather than a
/// shortcut: a version-2 session already nests the CURRENT desk shape, so its rows are
/// exactly the rows `setup_in` is about to read, and any question about them is that
/// reader's to ask in its own words.
inline v3::WorkshopSession session_v2_to_v3(const v2::WorkshopSession& old) {
    if (old.format_version != kV2FormatVersion) {
        throw std::invalid_argument(
            mismatched_version("session", kV2FormatVersion, old.format_version));
    }
    v3::WorkshopSession out;
    out.format = old.format;
    out.format_version = kV3FormatVersion;
    out.viewport = old.viewport;
    out.desk = old.desk;
    out.placement = absent_placement();
    return out;
}

/// A VERSION-3 SESSION AS A CURRENT ONE (WUX-10) — the one desk it had, as a layout run
/// holding exactly that desk, live.
///
/// ⚠ THIS IS FIELD DEFAULTING AND NOT INFERRED INTENT, and the distinction is the whole of
/// what makes it honest. A version-3 session could not say how many layouts a maker had,
/// because a version-3 Workshop restored one; so the truthful reading of those bytes is
/// *one layout, the desk that was live, standing at position zero* — the same thing
/// `absent_placement` does one field over. Inventing a second layout, or an empty run, or
/// an active position other than zero would be this file deciding something the maker never
/// wrote down.
///
/// EVERY NON-LAYOUT FACT CROSSES UNCHANGED: the room, the desk's own bytes, and the
/// placement — including a REAL one, because version 3 is the vintage that had it.
inline session_persist::WorkshopSession session_v3_to_v4(const v3::WorkshopSession& old) {
    if (old.format_version != kV3FormatVersion) {
        throw std::invalid_argument(
            mismatched_version("session", kV3FormatVersion, old.format_version));
    }
    session_persist::WorkshopSession out;
    out.format = old.format;
    out.format_version = session_persist::kFormatVersion;
    out.viewport = old.viewport;
    out.layouts.push_back(old.desk);
    out.active = 0;
    out.placement = old.placement;
    return out;
}

/// A VERSION-1 SESSION AS A CURRENT ONE — one authored edge, whose body composes the two
/// translations above.
///
/// ⚠ NOT A ROUTE, AND THE DIFFERENCE IS WHO WROTE IT. Nothing at the catalog layer knows
/// these functions exist or that one of them lands where the other starts; `op::migrate`
/// resolves `zengine.migrate.WorkshopSession.v1-to-v4` and spends exactly it. What this
/// composition buys is that version 1's MEANING is defined once — a second transcription of
/// the whole-cell multiply, the placement absence and the desk's vintage check is exactly
/// the drift this file exists to prevent.
inline session_persist::WorkshopSession session_v1_to_v4(const v1::WorkshopSession& old) {
    return session_v3_to_v4(session_v1_to_v3(old));
}

/// A VERSION-2 SESSION AS A CURRENT ONE, the same way.
inline session_persist::WorkshopSession session_v2_to_v4(const v2::WorkshopSession& old) {
    return session_v3_to_v4(session_v2_to_v3(old));
}

// ---- ...and the two of them as ordinary contributions ---------------------------------

/// EVERY EDGE THIS HISTORY SUPPLIES, as ordinary operator definitions.
///
/// THREE DIRECT EDGES AND NOT A LADDER. `v1 -> v4` is one authored conversion, not `v1 ->
/// v3` followed by `v3 -> v4`: it happens to reuse the same two translations, in C++,
/// inside one body -- which is what "authored" means. Nothing at the catalog layer knows
/// these three are related, and nothing may compose them: a run holding only `v1 -> v3` and
/// `v3 -> v4` satisfies no `v1 -> v4` lookup, because there is no lookup that walks.
///
/// ⚠ WHAT WUX-10 CHANGED HERE IS WHICH TARGET THEY NAME, and that is exactly what MIG-0
/// predicted a format move would cost: `current` is read off the reader's own shape, so the
/// identities re-derived themselves from v*-to-v3 to v*-to-v4 with no string edited, and the
/// shipped load plans needed no new row -- the same provider carries one more edge.
///
/// `op::make_migration` derives both the identity and the answer wrapper from the two
/// schemas, so this function cannot ship a contribution whose name and signature disagree.
inline std::vector<op::OperatorDef> conversions() {
    auto current = loom::schema_of<session_persist::WorkshopSession>();
    std::vector<op::OperatorDef> edges;
    edges.push_back(op::make_migration(
        loom::schema_of<v1::WorkshopSession>(), current, [](const loom::Value& old) {
            return loom::Cell::message(
                loom::to_value(session_v1_to_v4(loom::from_value<v1::WorkshopSession>(old))));
        }));
    edges.push_back(op::make_migration(
        loom::schema_of<v2::WorkshopSession>(), current, [](const loom::Value& old) {
            return loom::Cell::message(
                loom::to_value(session_v2_to_v4(loom::from_value<v2::WorkshopSession>(old))));
        }));
    edges.push_back(op::make_migration(
        loom::schema_of<v3::WorkshopSession>(), current, [](const loom::Value& old) {
            return loom::Cell::message(
                loom::to_value(session_v3_to_v4(loom::from_value<v3::WorkshopSession>(old))));
        }));
    return edges;
}

} // namespace zengine::workshop::session_history

#endif // ZENGINE_WORKSHOP_SESSION_HISTORY_HPP
