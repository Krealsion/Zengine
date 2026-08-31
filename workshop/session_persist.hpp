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
// ---- ...and the window's desktop PLACEMENT, since WUX-3 --------------------
//
// The omission this paragraph used to record is closed: the Surface vocabulary now carries
// the placement pair (`surface::SurfacePlacement` / `SurfacePlacementRemembered`), so the
// window's desktop position and its maximized state are facts a medium tells Workshop and
// facts Workshop may truthfully hand back. What is persisted is EXACTLY what the last
// medium reported -- the normal window's position in the medium's own desktop units, and
// whether the window was maximized -- held opaque: Workshop cannot interpret a desktop
// coordinate, cannot validate one (its session law says so, and it is still true), and
// does not try. Restoring hands the remembered placement back to whichever medium holds
// the surface, and THE MEDIUM is the judge -- it can see the displays that exist now, so
// it restores a reachable position faithfully and adapts a stranded one (the law is
// `surface::placement_within`). A session written under a terminal run RETAINS the last
// placement a graphical medium ever reported, unchanged: the TUI has no desktop fact and
// makes no claim, and carrying the remembered value forward is remembering, not claiming.
//
// The placement rides the SESSION file -- machine-local state -- and that is correctness
// rather than convenience: a desktop coordinate describes THIS machine's monitors, which
// is the same kind of fact the viewport already was. It is emphatically NOT pane/canvas
// geometry: WUX-2's fine lattice is untouched, and no desktop unit enters authored intent.
//
// ---- ...and what an OLDER session file is now (MIG-0) ----------------------
//
// THIS READER KNOWS ONE SHAPE: the one it admits. It used to carry two more -- version 1
// and version 2, with a road each -- and it does not any more. What it knows about
// yesterday is exactly enough to recognise it: bytes claiming THIS durable shape at a
// version this build does not admit are a HISTORICAL CLAIM, and a historical claim is
// handed to one bounded question (`op::migrate`, `operator/migration.hpp`):
//
//     is there one currently-live conversion from that version to this one?
//
//   YES   it is spent -- the file's own bytes admitted at the old shape's own gate, the
//         answer gated at this shape -- and the candidate that comes back then goes
//         through THIS reader's ordinary current-shape law, every layer of it, as if it
//         had arrived that way. A conversion cannot skip a check; it can only produce
//         something for the checks to be run on.
//   NO    an ordinary refusal that names the version found and the conversion that is
//         missing. The file is not rewritten, nothing is half-installed, and no version
//         claim has caused anything to be loaded -- which is the whole of the authority
//         story here: a claim is a LOOKUP KEY, and a lookup key cannot mount code.
//
// WHERE THE OLD SHAPES WENT: `workshop/session_history.hpp`, which is the conversion
// artifact's material rather than this reader's. This file names none of it and must not
// -- the point of the move is that the current owner stops compiling in yesterday.
//
// ---- What version 3 promises (WUX-3, amended by MIG-0) ---------------------
//
//   PROMISED   Workshop reads and writes session format version 3 — the desk nested at
//              setup format 3, the viewport, and the desktop placement — and a second save
//              of a loaded session is byte-identical to the first. An OLDER version is read
//              exactly when a conversion to this one is currently live, and is rewritten
//              only by the ordinary close-time save (which writes v3).
//   REFUSED    a version this build does not admit and has no live conversion for, with the
//              number named; a `format` that is not this one; a field the shape does not
//              declare; a field of the wrong kind; a
//              placement mode or window word outside its closed set, with what was found
//              and what would have worked both named; an absent placement carrying
//              non-zero coordinates or a maximized window (absence has ONE spelling); a
//              nested desk that is not a legal saved setup, in `setup_persist`'s own
//              words; a file larger than a session can be.
//   ACCEPTED   a desk holding references this build cannot resolve, with all of its authored
//              window intent -- `setup_persist`'s rule, unchanged, because it is the same
//              value. And a viewport this Workshop will not honour: see below, because that
//              is deliberately NOT a refusal of the file. The placement's coordinates are
//              accepted UNJUDGED -- they are another machine's desktop truth, and the only
//              party that can judge one is the medium at restore time.
//   NOT DONE   an upgrade path framework, a dual writer, crash durability, fullscreen
//              state, monitor identity, and any notion of WHICH document or WHICH setup
//              file the session belonged to. Where things are is the host's business and
//              always was.
//
// ---- A viewport that cannot be honoured is not a broken file --------------
//
// The two are separate answers on purpose (§13). A file that cannot be read or understood
// costs a maker their desk, and they are told why. A file that is perfectly well formed and
// whose viewport this Workshop will not open at costs them nothing but the size: the desk is
// restored and the viewport is not, and the notice says which value was declined. Throwing
// away a good desk over a bad number would be the corrupt-save-makes-Workshop-useless
// failure this phase exists to avoid, committed by the code meant to avoid it.

#include "operator/catalog.hpp"
#include "operator/migration.hpp"
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

/// The ONE session format version this build writes and admits.
///
/// SINCE MIG-0 THERE IS NO SECOND NUMBER HERE. An older file is not read by a road this
/// file carries; it is read by a conversion this run happens to have, which names its own
/// two versions and is the only party that has to know them. So there is no list to keep
/// in step with anything, and adding a version to this format is changing this number --
/// not appending a rung to a ladder.
inline constexpr std::int64_t kFormatVersion = 3;

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

// ---- The placement's words, and why they are words (WUX-3) ---------------------------
//
// The setup format's decision, for its reason: the in-memory values are a bool and two
// integers, and what a saved fact MEANS must not be movable by a renumbering. Two closed
// sets: `mode` says whether there is a remembered placement at all, `window` says which
// state the window was in. `none` is the one canonical spelling of "no placement was ever
// reported" -- a session saved under a run whose medium never spoke one -- and its unused
// numbers must be zero and its window `normal`, because Loom's admission has no optional
// fields and a value nobody means must have exactly one spelling (WIND-2's law, verbatim).

inline constexpr const char* kPlacementNone = "none";
inline constexpr const char* kPlacementDesktop = "desktop";
inline constexpr const char* kPlacementModeWords = "none or desktop";

inline constexpr const char* kWindowNormal = "normal";
inline constexpr const char* kWindowMaximized = "maximized";
inline constexpr const char* kWindowWords = "normal or maximized";

/// WHERE THE WINDOW SAT ON ITS DESKTOP, as the last medium reported it (WUX-3).
///
/// `x`/`y` are the NORMAL window's top-left in the reporting medium's own desktop units --
/// opaque to Workshop, remembered and handed back, never interpreted (the vocabulary's
/// custody split, `surface::SurfacePlacement`). Signed, because a monitor left of the
/// primary lives at negative coordinates, legitimately.
struct WorkshopPlacement {
    std::string mode;   ///< none | desktop
    std::int64_t x = 0; ///< the normal window's top-left, in the medium's desktop units
    std::int64_t y = 0;
    std::string window; ///< normal | maximized

    ZEN_SHAPE(WorkshopPlacement, 1, ZEN_FIELD(mode), ZEN_FIELD(x), ZEN_FIELD(y),
              ZEN_FIELD(window));
};

/// A WHOLE SAVED SESSION: what it is, which version of that it is, the room it was in, the
/// desk that was in the room, and where the room's window sat on the desktop.
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
    WorkshopPlacement placement;

    /// Version 2 (WUX-2): the nested desk became setup format 3 — sub-cell
    /// geometry — and this format's version moved with it, exactly as the
    /// assertion below always demanded it would.
    /// Version 3 (WUX-3): the desktop placement, carried as the words above.
    ZEN_SHAPE(WorkshopSession, 3, ZEN_FIELD(format), ZEN_FIELD(format_version),
              ZEN_FIELD(viewport), ZEN_FIELD(desk), ZEN_FIELD(placement));
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

/// THE PLACEMENT HALF OF A SESSION, AS A VALUE -- what the weave remembers between a
/// medium's report and a save, and what a load hands back. `known == false` IS the
/// canonical absence: its numbers are zero and its window normal by construction, so the
/// written spelling cannot carry a coordinate nobody means.
struct Placement {
    bool known = false;     ///< a medium has reported one (this run or a restored one)
    std::int64_t x = 0;     ///< the normal window's top-left, opaque desktop units
    std::int64_t y = 0;
    bool maximized = false; ///< whether the window was maximized
};

/// The session, as the value that gets written.
///
/// NOTHING IS SORTED, NORMALISED, RESOLVED OR DROPPED ON THE WAY OUT -- `setup_persist`'s
/// own rule, inherited by using its own function for the desk. The viewport and the
/// placement are written exactly as the session held them, which is exactly what the last
/// medium said, which is the only reason writing either is worth anything.
inline WorkshopSession to_session(const Setup& desk, std::int64_t viewport_w,
                                  std::int64_t viewport_h, const Placement& place) {
    WorkshopSession out;
    out.format = kFormat;
    out.format_version = kFormatVersion;
    out.viewport = WorkshopViewport{viewport_w, viewport_h};
    out.desk = setup_persist::to_setup(desk);
    out.placement.mode = place.known ? kPlacementDesktop : kPlacementNone;
    out.placement.x = place.known ? place.x : 0;
    out.placement.y = place.known ? place.y : 0;
    out.placement.window =
        place.known && place.maximized ? kWindowMaximized : kWindowNormal;
    return out;
}

inline std::string to_text(const Setup& desk, std::int64_t viewport_w,
                           std::int64_t viewport_h, const Placement& place) {
    return loom::compat::serialize(
        loom::to_value(to_session(desk, viewport_w, viewport_h, place)));
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
    Placement placement;    ///< the remembered desktop placement; `known == false` = none
                            ///< was ever reported (and every legacy version reads as that)

    static LoadedSession no(std::string why) {
        LoadedSession bad;
        bad.outcome = Written::no(std::move(why));
        bad.present = true;
        return bad;
    }
};

/// WHAT TO SAY ABOUT A SESSION WHOSE OWN `format_version` FIELD IS NOT THE VERSION ITS
/// ENVELOPE CLAIMED.
///
/// THE TWO DOORS SAY DIFFERENT THINGS NOW, AND THAT IS THE REPAIR (MIG-0). A file whose
/// ENVELOPE claims another version is a historical claim, and what it is owed is a sentence
/// about the conversion it needs -- said by the seam that looked for one, in
/// `operator/migration.hpp`, because only that party knows whether one is live. A file
/// whose envelope claims THIS version over a body that says another is not old, it is
/// inconsistent with itself, and only a forgery produces one. Two facts, two sentences.
inline std::string forged_version(std::int64_t found) {
    return "this session claims version " + std::to_string(kFormatVersion) +
           " and its own format_version field says " + std::to_string(found);
}

/// WHAT TO SAY ABOUT AN OLDER SESSION THAT COULD NOT BE BROUGHT FORWARD -- whatever the
/// conversion seam answered, under the number this file was written at.
///
/// THE NUMBER IS NAMED BY THIS FILE and the reason is quoted from whoever produced it: a
/// missing conversion, a refused gate, a converter's own complaint about yesterday's
/// vocabulary. Re-wording any of them here would be a second answer to somebody else's
/// question.
inline std::string could_not_convert(std::uint32_t found, const std::string& why) {
    return "session version " + std::to_string(found) + " cannot be read: " + why;
}

/// THE PLACEMENT'S OWN ADMISSION: the two closed word sets, and the one-spelling law for
/// the absence. Judged here rather than inline in `from_text`, so the v3 road reads as the
/// v2 road plus exactly this.
inline Written placement_in(const WorkshopPlacement& file, Placement& out) {
    bool maximized = false;
    if (file.window == kWindowMaximized) {
        maximized = true;
    } else if (file.window != kWindowNormal) {
        return Written::no("`" + file.window + "` is not a window state (" + kWindowWords +
                           ")");
    }
    if (file.mode == kPlacementNone) {
        if (file.x != 0 || file.y != 0 || maximized) {
            return Written::no("a placement of `none` carries no coordinates and no "
                               "maximized state -- zero its numbers and set the window to `" +
                               std::string(kWindowNormal) + "`, or set the mode to `" +
                               std::string(kPlacementDesktop) + "`");
        }
        out = Placement{};
        return Written::ok();
    }
    if (file.mode != kPlacementDesktop) {
        return Written::no("`" + file.mode + "` is not a placement mode (" +
                           kPlacementModeWords + ")");
    }
    out.known = true;
    out.x = file.x;
    out.y = file.y;
    out.maximized = maximized;
    return Written::ok();
}

// ---- ...and NOTHING about versions 1 and 2 (MIG-0) ------------------------------------
//
// This is where two retained shapes and two retained roads used to be. They are not here
// any more, and the absence is the phase: what an older session file LOOKED LIKE, and what
// it MEANT, belongs to whoever converts it -- `workshop/session_history.hpp`, carried into
// a run by a provider artifact this host may mount, replace, or simply not have.
//
// ⚠ A HISTORICAL SHAPE MUST NOT COME BACK TO THIS FILE, and it would be easy to add one the
// next time this format moves. The whole value of the move is that the current reader stops
// growing a rung per vintage: the arm in `from_text` recognises a historical CLAIM (this
// shape's name, another version) and asks for a conversion -- one sentence that does not
// get longer as history does.

/// The shared tail of every read road: judge the desk with `setup_persist`'s own readers
/// and the viewport with this file's own band, into a loaded session. The desk translation
/// differs per road, so it arrives already done; everything after it is one law.
inline LoadedSession loaded_from(Setup desk, std::int64_t viewport_w,
                                 std::int64_t viewport_h, const Placement& place) {
    LoadedSession loaded;
    loaded.outcome = Written::ok();
    loaded.present = true;
    loaded.desk = std::move(desk);
    loaded.placement = place;
    if (viewport_honoured(viewport_w, viewport_h)) {
        loaded.viewport_w = viewport_w;
        loaded.viewport_h = viewport_h;
        loaded.honoured = true;
    } else {
        loaded.declined = declined_viewport(viewport_w, viewport_h);
    }
    return loaded;
}

/// A CURRENT-SHAPE SESSION VALUE AS A LOADED ONE -- this format's own law, all of it, on a
/// value that has already been admitted at this shape.
///
/// SEPARATE FROM `from_text` SINCE MIG-0, and the separation is what makes "a conversion
/// cannot skip a check" structural. A session value reaches this function from exactly two
/// places -- straight off the gate, or out of a conversion's answer -- and neither of them
/// carries a check of its own. So a converted file cannot be admitted on easier terms than
/// a native one, because there are no other terms: this is where the terms are.
inline LoadedSession current_in(const loom::Value& admitted) {
    const WorkshopSession file = loom::from_value<WorkshopSession>(admitted);
    if (file.format != kFormat) {
        return LoadedSession::no("not a Workshop session: it says it is `" + file.format +
                                 "`");
    }
    // AND THE FIELD IS STILL CHECKED, for `setup_in`'s reason exactly: the shape's own
    // version got the value this far, and a body that then says it is a different vintage
    // is a forgery -- whether it was forged on disk or produced by a conversion that
    // answered with something its declared target does not mean.
    if (file.format_version != kFormatVersion) {
        return LoadedSession::no(forged_version(file.format_version));
    }
    // THE DESK IS BUILT INTO A LOCAL and only handed over once every layer has passed, which
    // is `setup_persist`'s own structural guarantee spent here rather than restated.
    Setup desk;
    const Written understood = setup_persist::setup_in(file.desk, desk);
    if (!understood.accepted) {
        return LoadedSession::no(understood.refusal);
    }
    // THE PLACEMENT'S WORDS ARE JUDGED; ITS COORDINATES ARE NOT. A word outside its closed
    // set refuses the file (a mode this build cannot read is a file it cannot claim to have
    // understood); a coordinate is another machine's desktop truth, accepted unjudged,
    // because the only party that can judge one is the medium at restore time.
    Placement place;
    const Written placed = placement_in(file.placement, place);
    if (!placed.accepted) {
        return LoadedSession::no(placed.refusal);
    }
    // THE VIEWPORT IS JUDGED AND NOT REFUSED (inside `loaded_from`). A well-formed session
    // whose size this build will not open at is still a session, and the desk in it is
    // still the maker's.
    return loaded_from(std::move(desk), file.viewport.width, file.viewport.height, place);
}

/// Text to a session. Total: every input is either a session or a refusal with a reason, and
/// nothing here throws.
///
/// THE LAYERS, IN ORDER: the envelope must parse; its CLAIM decides which of two roads it
/// takes (the WIND-2 preflight, so an older session is answered by its NUMBER rather than by
/// whichever field this version added); and whichever road it took, the value that comes out
/// of it meets `current_in` -- this format's whole law, borrowed rather than repeated,
/// including `setup_persist`'s own readers for the desk, so a desk cannot be legal in one
/// file and illegal in the other.
///
/// `conversions` IS THE HOST'S OPERATOR CATALOG, or nothing.
///
/// A READING AND NOT A POWER, `frontier`'s seam one layer in: this reader may LOOK for a
/// live conversion and spend one, and there is nothing it can do with the catalog beyond
/// that -- `op::migrate` performs no mount, no load, no plan walk and no write, and this
/// file holds no catalog of its own between calls. `nullptr` is ordinary and is what every
/// fixture gets: it means this run has no conversions, which an older file is told in words.
inline LoadedSession from_text(std::string_view bytes,
                               const op::Catalog* conversions = nullptr) {
    const loom::Unverified claim = loom::compat::parse(bytes);
    if (!claim.well_formed()) {
        const loom::Admission refused =
            loom::admit(claim, loom::schema_of<WorkshopSession>(), loom::Report::FirstError);
        return LoadedSession::no("not a Workshop session: " + refused.first_error().message());
    }
    // ---- THE HISTORICAL ARM (MIG-0) -----------------------------------------
    //
    // THIS SHAPE'S NAME AT ANOTHER VERSION IS THE WHOLE TEST, and it is deliberately the
    // only door to a conversion. A file of another FORMAT falls through to the gate below
    // and is refused there, by identity, exactly as it always was; a file of THIS format at
    // this version never reaches here at all. So no corrupt session, no wrong file and no
    // hostile value can turn into a search for something willing to translate it -- which is
    // the difference between a version road and a fallback.
    if (claim.claimed_name() == std::string(WorkshopSession::zen_name) &&
        claim.claimed_version() != WorkshopSession::zen_version) {
        const op::Evaluation converted =
            op::migrate(conversions, claim, loom::schema_of<WorkshopSession>());
        if (!converted) {
            return LoadedSession::no(
                could_not_convert(claim.claimed_version(), converted.reason()));
        }
        return current_in(op::migrated(converted));
    }
    const loom::Admission admitted =
        loom::admit(claim, loom::schema_of<WorkshopSession>(), loom::Report::FirstError);
    if (!admitted.ok()) {
        return LoadedSession::no(admitted.first_error().message());
    }
    return current_in(admitted.value());
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
/// It writes THROUGH `persist::write_file_making_room` since WUX-3: the ordinary home of
/// this file is the per-user state root, which is created on first write and never on a
/// read -- a run that persists nothing leaves no trace.
inline Written save_file(const std::string& path, const Setup& desk, std::int64_t viewport_w,
                         std::int64_t viewport_h, const Placement& place) {
    return persist::write_file_making_room(path, to_text(desk, viewport_w, viewport_h, place));
}

/// Read the last session from a file.
///
/// IT ASKS WHETHER THE FILE EXISTS BEFORE IT TRIES TO READ IT, which is the one thing this
/// reader does that the other two do not, and it is the whole of §13's first distinction: to
/// `persist::read_file` a missing file and an unreadable one are both "cannot read", and a
/// FIRST LAUNCH reported as an error is the single most likely way this feature could become
/// noise. So absence is asked about separately, and answered with `present == false` rather
/// than with a refusal.
/// ...AND THE BYTES ARE THIS OWNER'S, WHOEVER TRANSLATES THEM (MIG-0). Finding the file,
/// bounding what may be read out of it, and deciding whether a conversion may be looked for
/// at this load attempt are all decided here, before any conversion is a possibility. A
/// conversion never opens a file, never sees a path, and never learns there was one.
inline LoadedSession load_file(const std::string& path,
                               const op::Catalog* conversions = nullptr) {
    std::error_code ec;
    if (!std::filesystem::exists(path, ec) || ec) {
        return LoadedSession{}; // no previous session: not an error, and nothing to say
    }
    const persist::FileText read =
        persist::read_file(path, kMaxSessionBytes, "a Workshop session");
    if (!read.outcome.accepted) {
        return LoadedSession::no(read.outcome.refusal);
    }
    return from_text(read.text, conversions);
}

} // namespace zengine::workshop::session_persist

#endif // ZENGINE_WORKSHOP_SESSION_PERSIST_HPP
