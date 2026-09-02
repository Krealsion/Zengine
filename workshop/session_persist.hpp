// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_SESSION_PERSIST_HPP
#define ZENGINE_WORKSHOP_SESSION_PERSIST_HPP

// THE LAST SESSION -- a third artifact, beside the document's file and the setup's, and
// beside them on purpose.
// Workshop law: agents/workshop/session.md (+3 registers; agents/workshop.md routes)

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

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace zengine::workshop::session_persist {

/// What a Workshop session file says it is. Its own word, beside and not equal to the
/// document's `zengine-workshop` or the setup's `zengine-workshop-setup`, so that handing
/// Workshop the wrong one of its own three files is named rather than half-read.
inline constexpr const char* kFormat = "zengine-workshop-session";

/// The ONE session format version this build writes and admits.
// WL-MIG-01, WL-MIG-04, WL-MIG-06 -- agents/workshop/migration.md
// WL-GEO-11 -- agents/workshop/geometry.md
// WL-SESSION-05 -- agents/workshop/session.md
inline constexpr std::int64_t kFormatVersion = 6;

/// Where the last session lives when the host does not say otherwise. Beside the document's
/// default (`persist::kDefaultDocumentName`) and the setup's
/// (`kDefaultSetupFileName`), and a third name for the third promise.
inline constexpr const char* kDefaultSessionFileName = "workshop-session.json";

/// HOW LARGE A SESSION MAY BE, DERIVED FROM WHAT ONE MAY HOLD.
// WL-SESSION-06 -- agents/workshop/session.md
inline constexpr std::uintmax_t kMaxLinkPathBytes = 4096;

inline constexpr std::uintmax_t kMaxSessionBytes =
    static_cast<std::uintmax_t>(kMaxLayouts) *
    (2u * setup_persist::kMaxSetupBytes + kMaxLinkPathBytes);

// ---- The file's own shapes ----------------------------------------------------

/// HOW MUCH ROOM THE SURFACE HAD, in canvas cells.
// WL-SESSION-07 -- agents/workshop/session.md
struct WorkshopViewport {
    std::int64_t width = 0;
    std::int64_t height = 0;

    ZEN_SHAPE(WorkshopViewport, 1, ZEN_FIELD(width), ZEN_FIELD(height));
};

// ---- The placement's words, and why they are words -----------------------------------
// WL-SESSION-08 -- agents/workshop/session.md

inline constexpr const char* kPlacementNone = "none";
inline constexpr const char* kPlacementDesktop = "desktop";
inline constexpr const char* kPlacementModeWords = "none or desktop";

inline constexpr const char* kWindowNormal = "normal";
inline constexpr const char* kWindowMaximized = "maximized";
inline constexpr const char* kWindowWords = "normal or maximized";

/// WHERE THE WINDOW SAT ON ITS DESKTOP, as the last medium reported it.
// WL-SESSION-08 -- agents/workshop/session.md
struct WorkshopPlacement {
    std::string mode;   ///< none | desktop
    std::int64_t x = 0; ///< the normal window's top-left, in the medium's desktop units
    std::int64_t y = 0;
    std::string window; ///< normal | maximized

    ZEN_SHAPE(WorkshopPlacement, 1, ZEN_FIELD(mode), ZEN_FIELD(x), ZEN_FIELD(y),
              ZEN_FIELD(window));
};

/// A LAYOUT'S SETUP ASSOCIATION AS WRITTEN: which standalone artifact it refers
/// to, and the last value Workshop successfully knew that artifact to hold.
// WL-SESSION-05 -- agents/workshop/session.md
struct WorkshopSetupLink {
    std::string path;
    setup_persist::WorkshopSetup known;

    ZEN_SHAPE(WorkshopSetupLink, 1, ZEN_FIELD(path), ZEN_FIELD(known));
};

/// ONE SAVED LAYOUT: the desk, and the artifact it is associated with.
// WL-LAYOUT-09, WL-LAYOUT-12 -- agents/workshop/layouts.md
// WL-SESSION-05 -- agents/workshop/session.md
struct WorkshopLayout {
    setup_persist::WorkshopSetup desk;
    WorkshopSetupLink link;

    ZEN_SHAPE(WorkshopLayout, 1, ZEN_FIELD(desk), ZEN_FIELD(link));
};

/// A WHOLE SAVED SESSION: what it is, which version of that it is, the room it was in, the
/// LAYOUTS that were in the room and which one the maker was standing on, and where the
/// room's window sat on the desktop.
// WL-SESSION-04, WL-SESSION-05 -- agents/workshop/session.md
struct WorkshopSession {
    std::string format;
    std::int64_t format_version = 0;
    WorkshopViewport viewport;
    std::vector<WorkshopLayout> layouts;
    std::int64_t active = 0;
    WorkshopPlacement placement;

    // WL-MIG-03 -- agents/workshop/migration.md; WL-SESSION-05 -- agents/workshop/session.md
    ZEN_SHAPE(WorkshopSession, 6, ZEN_FIELD(format), ZEN_FIELD(format_version),
              ZEN_FIELD(viewport), ZEN_FIELD(layouts), ZEN_FIELD(active),
              ZEN_FIELD(placement));
};

/// THE ENVELOPE'S SHAPE VERSION AND THE SESSION FORMAT VERSION ARE ONE NUMBER, for the
/// reason `setup_persist` states about its own pair: it is what lets a session file of
/// another version be refused BY ITS NUMBER, on the claim, before a single field is judged.
static_assert(WorkshopSession::zen_version == static_cast<std::uint32_t>(kFormatVersion),
              "the session file's format version and its envelope's shape version are one "
              "number: a file of another version must be refused by ITS number, before its "
              "fields are judged against this version's shape");

/// AND THE DESK'S VERSION IS PINNED HERE ON PURPOSE.
static_assert(setup_persist::WorkshopSetup::zen_version == 3,
              "the session file nests the setup's own shape: when the desk's version moves, "
              "this format's version moves with it, and the refusal is worded here rather "
              "than left to the gate");

// ---- Would this viewport be honoured? ------------------------------------------

/// IS THIS A VIEWPORT THIS WORKSHOP WOULD ACTUALLY OPEN AT?
// WL-SESSION-07 -- agents/workshop/session.md
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
/// medium's report and a save, and what a load hands back.
// WL-SESSION-08 -- agents/workshop/session.md
struct Placement {
    bool known = false;     ///< a medium has reported one (this run or a restored one)
    std::int64_t x = 0;     ///< the normal window's top-left, opaque desktop units
    std::int64_t y = 0;
    bool maximized = false; ///< whether the window was maximized
};

/// The session, as the value that gets written.
// WL-LAYOUT-12 -- agents/workshop/layouts.md; WL-SESSION-05 -- agents/workshop/session.md
inline WorkshopSetupLink to_link(const SetupLink& link) {
    // THE ABSENCE IS WRITTEN, NOT OMITTED, and it is written the one way `link_in` accepts:
    // an empty path beside the desk `setup_persist::to_setup` makes of a default `Setup` --
    // which has an empty name, and is therefore a value no maker's desk can equal.
    return WorkshopSetupLink{link.path, setup_persist::to_setup(link.known)};
}

inline WorkshopSession to_session(const std::vector<Layout>& run, std::size_t active,
                                  std::int64_t viewport_w, std::int64_t viewport_h,
                                  const Placement& place) {
    WorkshopSession out;
    out.format = kFormat;
    out.format_version = kFormatVersion;
    out.viewport = WorkshopViewport{viewport_w, viewport_h};
    out.layouts.reserve(run.size());
    for (const Layout& layout : run) {
        out.layouts.push_back(
            WorkshopLayout{setup_persist::to_setup(layout.desk), to_link(layout.link)});
    }
    out.active = static_cast<std::int64_t>(active);
    out.placement.mode = place.known ? kPlacementDesktop : kPlacementNone;
    out.placement.x = place.known ? place.x : 0;
    out.placement.y = place.known ? place.y : 0;
    out.placement.window =
        place.known && place.maximized ? kWindowMaximized : kWindowNormal;
    return out;
}

inline std::string to_text(const std::vector<Layout>& run, std::size_t active,
                           std::int64_t viewport_w, std::int64_t viewport_h,
                           const Placement& place) {
    return loom::compat::serialize(
        loom::to_value(to_session(run, active, viewport_w, viewport_h, place)));
}

// ---- Reading -------------------------------------------------------------------

/// What reading a session produced.
// WL-SESSION-07, WL-SESSION-14 -- agents/workshop/session.md
struct LoadedSession {
    Written outcome;        ///< whether a file that EXISTS was read and understood
    bool present = false;   ///< whether there was a previous session at all
    std::vector<Layout> layouts; ///< the maker's ordered run -- each desk WITH its Setup
                                 ///< association -- when `outcome.accepted`; a session that
                                 ///< was admitted holds at least one
    std::size_t active = 0; ///< which position of `layouts` was the live desk
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
// WL-MIG-07 -- agents/workshop/migration.md
inline std::string forged_version(std::int64_t found) {
    return "this session claims version " + std::to_string(kFormatVersion) +
           " and its own format_version field says " + std::to_string(found);
}

/// WHAT TO SAY ABOUT AN OLDER SESSION THAT COULD NOT BE BROUGHT FORWARD -- whatever the
/// conversion seam answered, under the number this file was written at.
// WL-MIG-06 -- agents/workshop/migration.md
inline std::string could_not_convert(std::uint32_t found, const std::string& why) {
    return "session version " + std::to_string(found) + " cannot be read: " + why;
}

// ---- THE LAYOUT RUN'S OWN LAW ----------------------------------------------------
// WL-SESSION-06 -- agents/workshop/session.md

/// A RUN WITH NO LAYOUT IN IT. Structurally impossible in runtime -- `SetupState::active`
/// is a value, so there is always at least one desk -- which is exactly why a file that
/// says otherwise cannot be installed.
inline std::string no_layouts() {
    return "this session holds no layout at all, and a Workshop always has at least one desk";
}

/// MORE LAYOUTS THAN THIS WORKSHOP KEEPS. The bound is `kMaxLayouts` -- the SAME number the
/// `=` gesture refuses a ninth layout with -- so a file cannot install a run the maker could
/// not have made, and raising the ceiling raises both at once.
inline std::string too_many_layouts(std::size_t found) {
    return "this session holds " + std::to_string(found) +
           " layouts, and this Workshop keeps at most " + std::to_string(kMaxLayouts);
}

/// AN ACTIVE POSITION THAT IS NOT ONE OF THE LAYOUTS SAVED. Said in the file's own numbers,
/// which are positions from zero, because the maker acting on it is reading the file.
inline std::string active_out_of_range(std::int64_t at, std::size_t held) {
    return "this session's active layout is position " + std::to_string(at) +
           ", and its layouts run from 0 to " + std::to_string(held - 1);
}

/// WHICH LAYOUT REFUSED, in front of the setup owner's own sentence.
// WL-SESSION-05 -- agents/workshop/session.md
inline std::string in_layout(std::size_t at, const std::string& why) {
    return "layout at position " + std::to_string(at) + ": " + why;
}

/// AN ASSOCIATION THAT SAYS TWO THINGS AT ONCE.
// WL-SESSION-05 -- agents/workshop/session.md
inline std::string half_a_link(std::size_t at) {
    return in_layout(at,
                     "this layout names no Setup file and carries a remembered Setup value "
                     "anyway -- a layout with no association remembers nothing");
}

/// A LAYOUT'S ASSOCIATION, AS THE LIVE VALUE IT NAMES -- or why it is not one.
// WL-SESSION-05, WL-SESSION-06 -- agents/workshop/session.md
inline Written link_in(const WorkshopSetupLink& file, std::size_t at, SetupLink& out) {
    if (file.path.empty()) {
        Setup nothing;
        if (setup_persist::setup_in(file.known, nothing).accepted) {
            return Written::no(half_a_link(at));
        }
        out = SetupLink{};
        return Written::ok();
    }
    Setup known;
    const Written understood = setup_persist::setup_in(file.known, known);
    if (!understood.accepted) {
        return Written::no(in_layout(at, "its remembered Setup value is not one this "
                                         "Workshop can read -- " +
                                             understood.refusal));
    }
    out = SetupLink{file.path, std::move(known)};
    return Written::ok();
}

/// THE RUN, AS THE LIVE VALUES IT NAMES -- or the first reason it is not a run at all.
// WL-SESSION-05, WL-SESSION-06 -- agents/workshop/session.md
inline Written layouts_in(const WorkshopSession& file, std::vector<Layout>& run,
                          std::size_t& active) {
    if (file.layouts.empty()) {
        return Written::no(no_layouts());
    }
    if (file.layouts.size() > kMaxLayouts) {
        return Written::no(too_many_layouts(file.layouts.size()));
    }
    if (file.active < 0 || static_cast<std::size_t>(file.active) >= file.layouts.size()) {
        return Written::no(active_out_of_range(file.active, file.layouts.size()));
    }
    std::vector<Layout> candidate;
    candidate.reserve(file.layouts.size());
    for (std::size_t at = 0; at < file.layouts.size(); ++at) {
        // EVERY LAYOUT MEETS THE SETUP OWNER'S WHOLE LAW, in the setup owner's own words.
        // There is one durable representation of a desk in this program and one function
        // that admits one; a session holding several of them holds several desks, not a
        // different kind of thing.
        Setup desk;
        const Written understood = setup_persist::setup_in(file.layouts[at].desk, desk);
        if (!understood.accepted) {
            return Written::no(in_layout(at, understood.refusal));
        }
        SetupLink link;
        const Written related = link_in(file.layouts[at].link, at, link);
        if (!related.accepted) {
            return Written::no(related.refusal);
        }
        candidate.push_back(Layout{std::move(desk), std::move(link)});
    }
    run = std::move(candidate);
    active = static_cast<std::size_t>(file.active);
    return Written::ok();
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

// WL-MIG-01, WL-MIG-04, WL-MIG-06 -- agents/workshop/migration.md

/// The shared tail of every read road: an admitted layout run, and the viewport judged
/// against this file's own band, into a loaded session.
inline LoadedSession loaded_from(std::vector<Layout> run, std::size_t active,
                                 std::int64_t viewport_w, std::int64_t viewport_h,
                                 const Placement& place) {
    LoadedSession loaded;
    loaded.outcome = Written::ok();
    loaded.present = true;
    loaded.layouts = std::move(run);
    loaded.active = active;
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
// WL-MIG-07 -- agents/workshop/migration.md
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
    // THE RUN IS JUDGED WHOLE AND BUILT INTO LOCALS, and only handed over once every layer
    // has passed -- `setup_persist`'s own structural guarantee, spent over a run rather than
    // restated. ⚠ AND THIS IS ORDINARY CURRENT-VERSION ADMISSION: a v4 file with no layout,
    // an active position out of range or a ninth layout is WRONG, not old, and is refused
    // here in current-data words. Nothing about an admission failure sends a file looking
    // for a conversion; only a historical CLAIM does that, in `from_text`.
    std::vector<Layout> run;
    std::size_t active = 0;
    const Written understood = layouts_in(file, run, active);
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
    // whose size this build will not open at is still a session, and the desks in it are
    // still the maker's.
    return loaded_from(std::move(run), active, file.viewport.width, file.viewport.height,
                       place);
}

/// Text to a session. Total: every input is either a session or a refusal with a reason, and
/// nothing here throws.
// WL-MIG-06, WL-MIG-08 -- agents/workshop/migration.md
inline LoadedSession from_text(std::string_view bytes,
                               const op::Catalog* conversions = nullptr) {
    const loom::Unverified claim = loom::compat::parse(bytes);
    if (!claim.well_formed()) {
        const loom::Admission refused =
            loom::admit(claim, loom::schema_of<WorkshopSession>(), loom::Report::FirstError);
        return LoadedSession::no("not a Workshop session: " + refused.first_error().message());
    }
    // ---- THE HISTORICAL ARM -------------------------------------------------
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
// WL-SESSION-13 -- agents/workshop/session.md
inline Written save_file(const std::string& path, const std::vector<Layout>& run,
                         std::size_t active, std::int64_t viewport_w,
                         std::int64_t viewport_h, const Placement& place) {
    return persist::write_file_making_room(
        path, to_text(run, active, viewport_w, viewport_h, place));
}

/// Read the last session from a file.
// WL-MIG-08 -- agents/workshop/migration.md; WL-SESSION-14 -- agents/workshop/session.md
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
