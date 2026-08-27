// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_SESSION_PERSIST_HPP
#define ZENGINE_WORKSHOP_SESSION_PERSIST_HPP

// THE LAST SESSION -- a third artifact, beside the document's file and the setup's, and
// beside them on purpose (WUX-0).
//
// ---- What it is FOR -------------------------------------------------------
//
// One sentence: **close Workshop after arranging it into a useful desk, reopen it, and get
// that desk back** -- with no gesture, no picker and no command in between. Everything
// underneath already existed; what did not exist was anything that read it without being
// asked, and anything at all that remembered how big the window was.
//
// ---- Why it is not the setup's file ---------------------------------------
//
//   NAMED SETUP    explicit, a maker's own act, a maker's own word for it:
//                  "save this desk as Debugging". `s` writes it, `r` reads it,
//                  and Workshop touches that file at no other moment.
//   LAST SESSION   automatic, nobody's act, no name of its own:
//                  "give me back what I was just using". Written when Workshop
//                  leaves, read when it arrives, and never by a gesture.
//
// They are two PROMISES, so they are two files. Folding them together would mean that
// quitting silently rewrote whatever a maker had deliberately saved under a name -- the one
// thing an automatic save must never do to an explicit one. What they are NOT is two
// formats: a desk is a `Setup`, written by `setup_persist`'s own shapes and admitted by
// `setup_persist`'s own layers, and this file nests exactly that value rather than
// paraphrasing it. There is one durable representation of a desk in this program.
//
// ---- ...and what this file adds that a desk cannot hold -------------------
//
// The VIEWPORT: how much room the surface Workshop was being looked at through had, in
// canvas cells. It is one level above a desk because it describes the APPLICATION's window
// rather than the arrangement inside it -- the same desk is worth having in a big window
// and in a small one, and a maker who saves "Debugging" is saying nothing about how large
// they want it. That is exactly the distinction §11 of this phase asked to be preserved,
// and nesting is how it is preserved: `viewport` and `desk` are siblings, and only one of
// them is a setup.
//
// ---- Why a viewport is CELLS and not pixels -------------------------------
//
// Because cells are what Workshop knows. The window belongs to whichever Skin holds
// `zengine.skin`, which is a separately loaded artifact behind a C ABI; the only thing it
// ever says about its room is `surface::SurfaceExtent`, and the only thing Workshop ever
// says back is how big a picture it would like to paint. So the durable number is the one
// that actually crosses that seam. A pixel count written here would be a number Workshop
// had never been told and could not check.
//
// THE FIDELITY THAT COSTS, SAID PLAINLY. A graphical medium floors its drawable to whole
// cells (`surface::extent_of_drawable`) and creates its window from the picture it is asked
// for (`surface::canvas_window_size`), so a restored window is the maker's chosen size
// FLOORED TO WHOLE CELLS -- at most `kCanvasCellPx - 1` pixels short on each axis. That is
// a bound, not a hope, and it is the whole of what the cell round trip loses.
//
// WHAT IS NOT HERE, and is not silently missing: the window's screen POSITION, and whether
// it was maximized. Neither is a fact Workshop has ever been told. There is no message in
// the Surface vocabulary that carries either, in either direction, so persisting them would
// mean a new publisher-to-medium protocol -- which is the widening this phase was told not
// to absorb. Reported as a deliberate omission rather than attempted badly.
//
// ---- What version 2 promises (WUX-2) ---------------------------------------
//
//   PROMISED   Workshop reads and writes session format version 2 — the desk nested at
//              setup format 3, sub-cell geometry — and a second save of a loaded session
//              is byte-identical to the first. It also READS version 1, whose nested desk
//              is WIND-2's whole-cell format, through `setup_persist`'s own legacy reader:
//              the desk migrates exactly, the viewport crosses unchanged, and the file on
//              disk is rewritten only by the ordinary close-time save (which writes v2).
//   REFUSED    any OTHER `format_version`, with the number named; a `format` that is not
//              this one; a field the shape does not declare; a field of the wrong kind; a
//              nested desk that is not a legal saved setup, in `setup_persist`'s own words;
//              a file larger than a session can be.
//   ACCEPTED   a desk holding references this build cannot resolve, with all of its authored
//              window intent -- `setup_persist`'s rule, unchanged, because it is the same
//              value. And a viewport this Workshop will not honour: see below, because that
//              is deliberately NOT a refusal of the file.
//   NOT DONE   a version graph past v1, an upgrade path framework, a dual writer,
//              crash durability, and any notion of WHICH document or WHICH setup file the
//              session belonged to. Where things are is the host's business and always was.
//
// ---- A viewport that cannot be honoured is not a broken file --------------
//
// The two are separate answers on purpose (§13). A file that cannot be read or understood
// costs a maker their desk, and they are told why. A file that is perfectly well formed and
// whose viewport this Workshop will not open at costs them nothing but the size: the desk is
// restored and the viewport is not, and the notice says which value was declined. Throwing
// away a good desk over a bad number would be the corrupt-save-makes-Workshop-useless
// failure this phase exists to avoid, committed by the code meant to avoid it.

#include "persist.hpp"
#include "screen.hpp"
#include "setup.hpp"
#include "setup_persist.hpp"

#include <zen/admission.hpp>
#include <zen/schema.hpp>
#include <zen/serialize.hpp>
#include <zen/value.hpp>
#include <zen/weave/shape.hpp>

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace zengine::workshop::session_persist {

/// What a Workshop session file says it is. Its own word, beside and not equal to the
/// document's `zengine-workshop` or the setup's `zengine-workshop-setup`, so that handing
/// Workshop the wrong one of its own three files is named rather than half-read.
inline constexpr const char* kFormat = "zengine-workshop-session";

/// The session format version this build WRITES, and the newest it reads.
inline constexpr std::int64_t kFormatVersion = 2;

/// The one older version this build still reads (WUX-2's contract): the session
/// that nested WIND-2's whole-cell desk, translated on load — never rewritten in
/// place. The viewport is identical in both; only the nested desk moved.
inline constexpr std::int64_t kLegacyFormatVersion = 1;

/// Where the last session lives when the host does not say otherwise. Beside the document's
/// default (`persist::kDefaultDocumentName`) and the setup's
/// (`kDefaultSetupFileName`), and a third name for the third promise.
inline constexpr const char* kDefaultSessionFileName = "workshop-session.json";

/// A session is a setup plus two integers, so its ceiling is the setup's. Sixty-four
/// kibibytes is the same order-of-magnitude headroom `setup_persist` reasons its own ceiling
/// from, and it is the read side of the same law: a hostile file does not get to choose the
/// cost of refusing it.
inline constexpr std::uintmax_t kMaxSessionBytes = 1u << 16;

// ---- The file's own shapes ----------------------------------------------------

/// HOW MUCH ROOM THE SURFACE HAD, in canvas cells.
///
/// Its own shape rather than `surface::SurfaceExtent`, and the reason is the one
/// `setup_persist` gives for every shape in it: that struct is a MESSAGE this build happens
/// to send today and is free to grow a field whenever a medium has something new to say;
/// this is what a saved session IS. The text metric in particular has no business here --
/// how big one character of a face is belongs to whichever medium opens the face, is
/// republished on every run, and would be a stale claim about a font the moment it was
/// written down.
struct WorkshopViewport {
    std::int64_t width = 0;
    std::int64_t height = 0;

    ZEN_SHAPE(WorkshopViewport, 1, ZEN_FIELD(width), ZEN_FIELD(height));
};

/// A WHOLE SAVED SESSION: what it is, which version of that it is, the room it was in, and
/// the desk that was in the room.
///
/// `desk` IS `setup_persist::WorkshopSetup`, not a copy of its fields. That is the one
/// structural claim this file makes: a desk saved automatically and a desk saved under a
/// name are the same bytes in the same shape, so they cannot drift, and the four layers that
/// judge one judge the other (`setup_persist::setup_in`).
struct WorkshopSession {
    std::string format;
    std::int64_t format_version = 0;
    WorkshopViewport viewport;
    setup_persist::WorkshopSetup desk;

    /// Version 2 (WUX-2): the nested desk became setup format 3 — sub-cell
    /// geometry — and this format's version moved with it, exactly as the
    /// assertion below always demanded it would.
    ZEN_SHAPE(WorkshopSession, 2, ZEN_FIELD(format), ZEN_FIELD(format_version),
              ZEN_FIELD(viewport), ZEN_FIELD(desk));
};

/// THE ENVELOPE'S SHAPE VERSION AND THE SESSION FORMAT VERSION ARE ONE NUMBER, for the
/// reason `setup_persist` states about its own pair: it is what lets a session file of
/// another version be refused BY ITS NUMBER, on the claim, before a single field is judged.
static_assert(WorkshopSession::zen_version == static_cast<std::uint32_t>(kFormatVersion),
              "the session file's format version and its envelope's shape version are one "
              "number: a file of another version must be refused by ITS number, before its "
              "fields are judged against this version's shape");

/// AND THE DESK'S VERSION IS PINNED HERE ON PURPOSE.
///
/// A nested shape's version is part of the parent's wire identity, so the day
/// `WorkshopSetup` moves again every session file ever written stops admitting -- and what a
/// maker would be told is whatever the gate says about a nested field, not "this session was
/// written by an older Workshop". This assertion is the compile error that makes that a
/// DECISION: move the desk's version, and somebody has to come here, move this number and
/// this format's version together, and word the refusal. WUX-2 was the first to trip it:
/// the desk became v3, this format became v2, and the v1 session — nesting the v2 desk —
/// is read through the legacy road below rather than refused.
static_assert(setup_persist::WorkshopSetup::zen_version == 3,
              "the session file nests the setup's own shape: when the desk's version moves, "
              "this format's version moves with it, and the refusal is worded here rather "
              "than left to the gate");

// ---- Would this viewport be honoured? ------------------------------------------

/// IS THIS A VIEWPORT THIS WORKSHOP WOULD ACTUALLY OPEN AT?
///
/// The band is `screen_of`'s own -- the extents this composition is honest at -- and asking
/// the question here rather than letting `adopt_screen` clamp is the whole of the
/// sanitisation. The two are different answers to a hostile number:
///
///   clamp    a stored width of 100000 becomes `kScreenMaxW` and Workshop asks a medium for
///            a window nobody chose, on a display it cannot see
///   decline  Workshop opens at its floor, exactly as a first launch does, and says which
///            value it would not honour
///
/// The second is the one that cannot produce an unusable Workshop, so it is the one this
/// file implements. Zero and negative fall out of the same test rather than being special
/// cases of their own: a viewport is a pair of extents or it is not honoured.
///
/// WHAT IT CANNOT ASK. Whether a viewport fits the CURRENT DISPLAY is not a question
/// Workshop can put to anybody: the display belongs to whichever Skin holds the surface and
/// nothing in the Surface vocabulary reports it. So this is a plausibility bound and is
/// deliberately named as one -- what makes an implausible saved size harmless in practice is
/// that a medium answers with the room it actually has (`surface::SurfaceExtent`) and
/// Workshop takes that answer over anything it remembered.
inline constexpr bool viewport_honoured(std::int64_t width, std::int64_t height) noexcept {
    return width >= kScreenMinW && width <= kScreenMaxW && height >= kScreenMinH &&
           height <= kScreenMaxH;
}

/// What to say about a viewport this build will not open at. It names the value found,
/// because a maker looking at their own file can act on that.
inline std::string declined_viewport(std::int64_t width, std::int64_t height) {
    return "the saved window size " + std::to_string(width) + "x" + std::to_string(height) +
           " cells is not one this Workshop opens at (" + std::to_string(kScreenMinW) + "x" +
           std::to_string(kScreenMinH) + " to " + std::to_string(kScreenMaxW) + "x" +
           std::to_string(kScreenMaxH) + ") -- opening at the default size";
}

// ---- Writing -------------------------------------------------------------------

/// The session, as the value that gets written.
///
/// NOTHING IS SORTED, NORMALISED, RESOLVED OR DROPPED ON THE WAY OUT -- `setup_persist`'s
/// own rule, inherited by using its own function for the desk. The viewport is written
/// exactly as the session held it, which is exactly what the last medium said, which is the
/// only reason writing it is worth anything.
inline WorkshopSession to_session(const Setup& desk, std::int64_t viewport_w,
                                  std::int64_t viewport_h) {
    WorkshopSession out;
    out.format = kFormat;
    out.format_version = kFormatVersion;
    out.viewport = WorkshopViewport{viewport_w, viewport_h};
    out.desk = setup_persist::to_setup(desk);
    return out;
}

inline std::string to_text(const Setup& desk, std::int64_t viewport_w,
                           std::int64_t viewport_h) {
    return loom::compat::serialize(loom::to_value(to_session(desk, viewport_w, viewport_h)));
}

// ---- Reading -------------------------------------------------------------------

/// What reading a session produced.
///
/// FOUR ANSWERS AND NOT ONE BOOLEAN, because a startup has four genuinely different things
/// to do about a session file and telling them apart is most of what makes this legible
/// (§13):
///
///   `present == false`                 there is no previous session. NOT an error, and a
///                                      first launch must never be reported as one.
///   `outcome.accepted == false`        there is one and it cannot be read or understood.
///                                      Say why; use the defaults.
///   `honoured == false` with a desk    it was read; its viewport is not one this Workshop
///                                      opens at. Restore the desk, keep the default size,
///                                      say which value was declined.
///   everything accepted                restore both.
struct LoadedSession {
    Written outcome;        ///< whether a file that EXISTS was read and understood
    bool present = false;   ///< whether there was a previous session at all
    Setup desk;             ///< the arrangement, when `outcome.accepted`
    std::int64_t viewport_w = 0; ///< the room, when `honoured`
    std::int64_t viewport_h = 0;
    bool honoured = false;  ///< whether that room is one this Workshop will open at
    std::string declined;   ///< why not, when it is not -- empty otherwise

    static LoadedSession no(std::string why) {
        LoadedSession bad;
        bad.outcome = Written::no(std::move(why));
        bad.present = true;
        return bad;
    }
};

/// WHAT TO SAY ABOUT A SESSION VERSION THIS BUILD DOES NOT READ. One sentence, one place, so
/// the two doors that can meet a wrong version -- the envelope's claim and the file's own
/// `format_version` field -- cannot come to word it differently.
inline std::string wrong_version(std::int64_t found) {
    return "session version " + std::to_string(found) + " -- this Workshop reads versions " +
           std::to_string(kLegacyFormatVersion) + " and " + std::to_string(kFormatVersion);
}

// ---- VERSION 1, RETAINED FOR READING (WUX-2) -----------------------------------------
//
// The envelope that nested WIND-2's whole-cell desk, exactly as written: the same trick
// `setup_persist::v2` documents — the C++ names are namespaced, the WIRE names are the
// bare tokens, so `v1::WorkshopSession` claims `WorkshopSession` v1 over a `WorkshopSetup`
// v2 exactly as the old build did — and the translation is the desk's own legacy reader.
// The viewport crossed unchanged: it was cells then and it is cells now.
namespace v1 {

struct WorkshopSession {
    std::string format;
    std::int64_t format_version = 0;
    WorkshopViewport viewport;
    setup_persist::v2::WorkshopSetup desk;

    ZEN_SHAPE(WorkshopSession, 1, ZEN_FIELD(format), ZEN_FIELD(format_version),
              ZEN_FIELD(viewport), ZEN_FIELD(desk));
};

} // namespace v1

/// Text to a session. Total: every input is either a session or a refusal with a reason, and
/// nothing here throws.
///
/// THE LAYERS, IN ORDER, and the last two are borrowed rather than repeated: the envelope
/// must parse; its CLAIM must be a version this build reads (the WIND-2 preflight, so an
/// older session is refused by its number rather than by whichever field this version
/// added — and a version-1 claim takes the legacy road, WUX-2); it must admit against that
/// version's shape; it must say it is this format at that version; and its desk must be a
/// legal saved setup, judged by `setup_persist`'s own readers -- the same functions a setup
/// FILE goes through, so a desk cannot be legal in one file and illegal in the other.
inline LoadedSession from_text(std::string_view bytes) {
    const loom::Unverified claim = loom::compat::parse(bytes);
    if (!claim.well_formed()) {
        const loom::Admission refused =
            loom::admit(claim, loom::schema_of<WorkshopSession>(), loom::Report::FirstError);
        return LoadedSession::no("not a Workshop session: " + refused.first_error().message());
    }
    if (claim.claimed_name() == std::string(WorkshopSession::zen_name) &&
        claim.claimed_version() == v1::WorkshopSession::zen_version) {
        const loom::Admission old = loom::admit(claim, loom::schema_of<v1::WorkshopSession>(),
                                                loom::Report::FirstError);
        if (!old.ok()) {
            return LoadedSession::no(old.first_error().message());
        }
        const v1::WorkshopSession file = loom::from_value<v1::WorkshopSession>(old.value());
        if (file.format != kFormat) {
            return LoadedSession::no("not a Workshop session: it says it is `" + file.format +
                                     "`");
        }
        if (file.format_version != kLegacyFormatVersion) {
            return LoadedSession::no(wrong_version(file.format_version));
        }
        Setup desk;
        const Written understood = setup_persist::setup_in_v2(file.desk, desk);
        if (!understood.accepted) {
            return LoadedSession::no(understood.refusal);
        }
        LoadedSession loaded;
        loaded.outcome = Written::ok();
        loaded.present = true;
        loaded.desk = std::move(desk);
        if (viewport_honoured(file.viewport.width, file.viewport.height)) {
            loaded.viewport_w = file.viewport.width;
            loaded.viewport_h = file.viewport.height;
            loaded.honoured = true;
        } else {
            loaded.declined = declined_viewport(file.viewport.width, file.viewport.height);
        }
        return loaded;
    }
    if (claim.claimed_name() == std::string(WorkshopSession::zen_name) &&
        claim.claimed_version() != WorkshopSession::zen_version) {
        return LoadedSession::no(
            wrong_version(static_cast<std::int64_t>(claim.claimed_version())));
    }
    const loom::Admission admitted =
        loom::admit(claim, loom::schema_of<WorkshopSession>(), loom::Report::FirstError);
    if (!admitted.ok()) {
        return LoadedSession::no(admitted.first_error().message());
    }

    const WorkshopSession file = loom::from_value<WorkshopSession>(admitted.value());
    if (file.format != kFormat) {
        return LoadedSession::no("not a Workshop session: it says it is `" + file.format +
                                 "`");
    }
    if (file.format_version != kFormatVersion) {
        return LoadedSession::no(wrong_version(file.format_version));
    }

    // THE DESK IS BUILT INTO A LOCAL and only handed over once every layer has passed, which
    // is `setup_persist`'s own structural guarantee spent here rather than restated.
    Setup desk;
    const Written understood = setup_persist::setup_in(file.desk, desk);
    if (!understood.accepted) {
        return LoadedSession::no(understood.refusal);
    }

    LoadedSession loaded;
    loaded.outcome = Written::ok();
    loaded.present = true;
    loaded.desk = std::move(desk);
    // THE VIEWPORT IS JUDGED AND NOT REFUSED. A well-formed session whose size this build
    // will not open at is still a session, and the desk in it is still the maker's.
    if (viewport_honoured(file.viewport.width, file.viewport.height)) {
        loaded.viewport_w = file.viewport.width;
        loaded.viewport_h = file.viewport.height;
        loaded.honoured = true;
    } else {
        loaded.declined = declined_viewport(file.viewport.width, file.viewport.height);
    }
    return loaded;
}

// ---- The file itself -------------------------------------------------------------

/// Save the last session, through the document's own safe write: a complete candidate to a
/// sibling, then a rename over the destination.
///
/// THE PROMISE IS THE ONE `persist::write_file` MAKES and it is not restated here as though
/// it were a second mechanism: an ordinary detected write failure does not destroy the
/// previously valid session file, because nothing touches the destination until a complete
/// file exists beside it. CRASH DURABILITY IS NOT CLAIMED, here or anywhere else in this
/// program -- a Workshop that is killed loses the session it was in, and this phase does not
/// pretend otherwise.
inline Written save_file(const std::string& path, const Setup& desk, std::int64_t viewport_w,
                         std::int64_t viewport_h) {
    return persist::write_file(path, to_text(desk, viewport_w, viewport_h));
}

/// Read the last session from a file.
///
/// IT ASKS WHETHER THE FILE EXISTS BEFORE IT TRIES TO READ IT, which is the one thing this
/// reader does that the other two do not, and it is the whole of §13's first distinction: to
/// `persist::read_file` a missing file and an unreadable one are both "cannot read", and a
/// FIRST LAUNCH reported as an error is the single most likely way this feature could become
/// noise. So absence is asked about separately, and answered with `present == false` rather
/// than with a refusal.
inline LoadedSession load_file(const std::string& path) {
    std::error_code ec;
    if (!std::filesystem::exists(path, ec) || ec) {
        return LoadedSession{}; // no previous session: not an error, and nothing to say
    }
    const persist::FileText read =
        persist::read_file(path, kMaxSessionBytes, "a Workshop session");
    if (!read.outcome.accepted) {
        return LoadedSession::no(read.outcome.refusal);
    }
    return from_text(read.text);
}

} // namespace zengine::workshop::session_persist

#endif // ZENGINE_WORKSHOP_SESSION_PERSIST_HPP
