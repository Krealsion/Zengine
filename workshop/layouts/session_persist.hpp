// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_SESSION_PERSIST_HPP
#define ZENGINE_WORKSHOP_SESSION_PERSIST_HPP

// The last session: the run of layouts, the id each was minted, which one was live and which
// authored choice held each switchable office. Its file is a header line and a line per layout,
// each its own value; an older file of lines is read whole and brought forward.
// Workshop law: agents/workshop/session-restore.md (+4 registers; agents/workshop.md routes)

#include "operator/catalog.hpp"
#include "operator/migration.hpp"
#include "workshop/layouts/run.hpp"
#include "workshop/persist.hpp" // and the members a file of lines carries, read as text
#include "workshop/setup.hpp"
#include "workshop/setup_persist.hpp"

#include <zen/admission.hpp>
#include <zen/schema.hpp>
#include <zen/serialize.hpp>
#include <zen/value.hpp>
#include <zen/weave/shape.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace zengine::workshop::session_persist {

/// What a Workshop session file says it is: its own word, so handing Workshop the wrong file is
/// named rather than half-read.
inline constexpr const char* kFormat = "zengine-workshop-session";

/// The ONE session format version this build writes and admits.
// WL-MIG-01, WL-MIG-04, WL-MIG-06 -- agents/workshop/migration.md
// WL-SESSION-05 -- agents/workshop/session-restore.md
inline constexpr std::int64_t kFormatVersion = 12;

/// Where the last session lives when the host does not say otherwise.
inline constexpr const char* kDefaultSessionFileName = "workshop-session.json";

/// HOW LARGE A SESSION MAY BE, DERIVED FROM WHAT ONE MAY HOLD: a header line, and a line for
/// each layout holding two desks and a path.
// WL-SESSION-06 -- agents/workshop/session-restore.md
inline constexpr std::uintmax_t kMaxLinkPathBytes = 4096;

inline constexpr std::uintmax_t kMaxLayoutLineBytes =
    2u * setup_persist::kMaxSetupBytes + kMaxLinkPathBytes;

/// The header's line: room for every remembered choice with its office's name and its own at
/// their bounds, spelled at the most bytes JSON spells them, and the numbers and words beside.
inline constexpr std::uintmax_t kMaxHeaderLineBytes = 4u << 10;

inline constexpr std::uintmax_t kMaxSessionBytes =
    kMaxHeaderLineBytes + 1u +
    static_cast<std::uintmax_t>(kMaxLayouts) * (kMaxLayoutLineBytes + 1u);

// ---- The file's own shapes ----------------------------------------------------

/// A LAYOUT'S SETUP ASSOCIATION AS WRITTEN: which standalone artifact it refers to, and the last
/// value Workshop successfully knew that artifact to hold.
// WL-SESSION-05 -- agents/workshop/session-restore.md
struct WorkshopSetupLink {
    std::string path;
    setup_persist::WorkshopSetup known;

    ZEN_SHAPE(WorkshopSetupLink, 4, ZEN_FIELD(path), ZEN_FIELD(known));
};

/// ONE SAVED LAYOUT: the id the owner minted for it, the desk, and the artifact it is associated
/// with.
// WL-LAYOUT-09 -- agents/workshop/layouts.md
// WL-SESSION-05 -- agents/workshop/session-restore.md
struct WorkshopLayout {
    std::int64_t id = 0;
    setup_persist::WorkshopSetup desk;
    WorkshopSetupLink link;

    ZEN_SHAPE(WorkshopLayout, 4, ZEN_FIELD(id), ZEN_FIELD(desk), ZEN_FIELD(link));
};

/// WHICH AUTHORED CHOICE HELD AN OFFICE AT QUIT, by the office and the choice's name: what the
/// next start loads in place of the plan's row while the plan still authors it.
// WL-SWITCH-08 -- agents/workshop/editor-switch.md
struct WorkshopChoice {
    std::string role;
    std::string choice;

    ZEN_SHAPE(WorkshopChoice, 1, ZEN_FIELD(role), ZEN_FIELD(choice));
};

/// How many offices' choices a session may remember: one an office the plan authors choices for.
inline constexpr std::size_t kMaxRememberedChoices = 16;

/// A WHOLE SAVED SESSION: what it is, which version of that it is, the LAYOUTS of the run with
/// their ids and which one the weaver was standing on, the id the next layout takes, and which
/// authored choice held each switchable office. It is one value in memory alone: what the
/// reader's law judges and what a conversion of an older file answers with. Its file holds it a
/// line at a time.
// WL-SESSION-04 -- agents/workshop/session.md; WL-SESSION-05 -- agents/workshop/session-restore.md
struct WorkshopSession {
    std::string format;
    std::int64_t format_version = 0;
    std::vector<WorkshopLayout> layouts;
    std::int64_t active = 0;
    std::int64_t next_id = 0;

    // WL-SWITCH-08 -- agents/workshop/editor-switch.md
    std::vector<WorkshopChoice> choices;

    ZEN_SHAPE(WorkshopSession, 12, ZEN_FIELD(format), ZEN_FIELD(format_version),
              ZEN_FIELD(layouts), ZEN_FIELD(active), ZEN_FIELD(next_id), ZEN_FIELD(choices));
};

/// THE SESSION FILE'S FIRST LINE: the session's every field but its layouts, and how many
/// layouts follow it, one `WorkshopLayout` a line.
// WL-SESSION-05, WL-SESSION-06 -- agents/workshop/session-restore.md
struct WorkshopSessionHeader {
    std::string format;
    std::int64_t format_version = 0;
    std::int64_t layout_count = 0;
    std::int64_t active = 0;
    std::int64_t next_id = 0;
    std::vector<WorkshopChoice> choices;

    ZEN_SHAPE(WorkshopSessionHeader, 12, ZEN_FIELD(format), ZEN_FIELD(format_version),
              ZEN_FIELD(layout_count), ZEN_FIELD(active), ZEN_FIELD(next_id), ZEN_FIELD(choices));
};

/// THE ENVELOPE'S SHAPE VERSION AND THE SESSION FORMAT VERSION ARE ONE NUMBER: a session file of
/// another version is refused by ITS number, on the claim, before a single field is judged.
static_assert(WorkshopSession::zen_version == static_cast<std::uint32_t>(kFormatVersion) &&
                  WorkshopSessionHeader::zen_version == static_cast<std::uint32_t>(kFormatVersion),
              "the session file's format version and its envelope's shape version are one "
              "number: a file of another version must be refused by ITS number, before its "
              "fields are judged against this version's shape");

/// AND THE DESK'S VERSION IS PINNED HERE ON PURPOSE.
static_assert(setup_persist::WorkshopSetup::zen_version == 6,
              "the session file nests the setup's own shape: when the desk's version moves, "
              "this format's version moves with it, and the refusal is worded here rather "
              "than left to the gate");

// ---- Writing -------------------------------------------------------------------

/// One association, as the value that gets written: the path, and the desk the file was
/// known to hold -- an absence written as an empty path beside a default desk.
// WL-SESSION-05 -- agents/workshop/session-restore.md
inline WorkshopSetupLink to_link(const SetupLink& link) {
    // THE ABSENCE IS WRITTEN, NOT OMITTED, and it is written the one way `link_in` accepts: an
    // empty path beside the desk `setup_persist::to_setup` makes of a default `Setup` -- which has
    // an empty name, and is therefore a value no weaver's desk can equal.
    return WorkshopSetupLink{link.path, setup_persist::to_setup(link.known)};
}

/// The session, as the value that gets written. NOTHING IS SORTED, NORMALISED, RESOLVED OR
/// DROPPED ON THE WAY OUT: the order is the run's, each layout keeps its id, and `active` is where
/// the caller says the live one stands.
inline WorkshopSession to_session(const std::vector<Layout>& run, std::size_t active,
                                  std::int64_t next_id,
                                  const std::vector<WorkshopChoice>& choices = {}) {
    WorkshopSession out;
    out.format = kFormat;
    out.format_version = kFormatVersion;
    out.layouts.reserve(run.size());
    for (const Layout& layout : run) {
        out.layouts.push_back(WorkshopLayout{layout.id, setup_persist::to_setup(layout.desk),
                                             to_link(layout.link)});
    }
    out.active = static_cast<std::int64_t>(active);
    out.next_id = next_id;
    out.choices = choices;
    return out;
}

/// The header a session's file opens with: every field but the layouts, and their count.
inline WorkshopSessionHeader header_of(const WorkshopSession& session) {
    WorkshopSessionHeader out;
    out.format = session.format;
    out.format_version = session.format_version;
    out.layout_count = static_cast<std::int64_t>(session.layouts.size());
    out.active = session.active;
    out.next_id = session.next_id;
    out.choices = session.choices;
    return out;
}

/// The session as its file's text: the header's line, then each layout's in the run's order,
/// each line one value ended by a newline. A value's JSON escapes every newline it holds.
inline std::string to_text(const WorkshopSession& session) {
    std::string out = loom::compat::serialize(loom::to_value(header_of(session)));
    out += '\n';
    for (const WorkshopLayout& layout : session.layouts) {
        out += loom::compat::serialize(loom::to_value(layout));
        out += '\n';
    }
    return out;
}

inline std::string to_text(const std::vector<Layout>& run, std::size_t active,
                           std::int64_t next_id,
                           const std::vector<WorkshopChoice>& choices = {}) {
    return to_text(to_session(run, active, next_id, choices));
}

// ---- Reading -------------------------------------------------------------------

/// What reading a session produced.
// WL-SESSION-14 -- agents/workshop/session-restore.md
struct LoadedSession {
    Written outcome;        ///< whether a file that EXISTS was read and understood
    bool present = false;   ///< whether there was a previous session at all
    std::vector<Layout> layouts; ///< the weaver's ordered run, each with its id and association
    std::size_t active = 0; ///< which position of `layouts` was the live desk
    std::int64_t next_id = 0; ///< the id the next new or copied layout takes
    /// The version the file was written at: an older one was brought forward on the way in.
    std::int64_t read_version = 0;
    /// HOW MANY REFERENCES IN THIS SESSION NAMED A PANE THAT HAS SINCE CHANGED HANDS
    /// (`workshop/pane_migration.hpp`), over every desk and every remembered Setup value.
    pane_migration::Converted converted;
    /// Which authored choice held each switchable office at quit.
    std::vector<WorkshopChoice> choices;

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

/// WHAT TO SAY ABOUT AN OLDER SESSION THAT COULD NOT BE BROUGHT FORWARD.
// WL-MIG-06 -- agents/workshop/migration.md
inline std::string could_not_convert(std::uint32_t found, const std::string& why) {
    return "session version " + std::to_string(found) + " cannot be read: " + why;
}

// ---- THE LAYOUT RUN'S OWN LAW ----------------------------------------------------
// WL-SESSION-06 -- agents/workshop/session-restore.md

/// A RUN WITH NO LAYOUT IN IT: a file that says so cannot be installed.
inline std::string no_layouts() {
    return "this session holds no layout at all, and a Workshop always has at least one desk";
}

/// MORE LAYOUTS THAN THIS WORKSHOP KEEPS.
inline std::string too_many_layouts(std::size_t found) {
    return "this session holds " + std::to_string(found) +
           " layouts, and this Workshop keeps at most " + std::to_string(kMaxLayouts);
}

/// AN ACTIVE POSITION THAT IS NOT ONE OF THE LAYOUTS SAVED, in the file's own numbers.
inline std::string active_out_of_range(std::int64_t at, std::size_t held) {
    return "this session's active layout is position " + std::to_string(at) +
           ", and its layouts run from 0 to " + std::to_string(held - 1);
}

/// AN ID THAT NAMES NO LAYOUT, OR TWO.
// WL-SESSION-05 -- agents/workshop/session-restore.md
inline std::string bad_id(std::size_t at, std::int64_t id) {
    return "layout at position " + std::to_string(at) + " has id " + std::to_string(id) +
           " -- a layout's id is positive and no other layout holds it";
}

/// THE NEXT ID BELOW ONE THE RUN HOLDS: a new layout would take an id a layout has.
inline std::string next_id_held(std::int64_t next_id, std::int64_t top) {
    return "this session's next id is " + std::to_string(next_id) +
           ", and a layout of it holds " + std::to_string(top) + " -- the next is above every id";
}

/// WHICH LAYOUT REFUSED, in front of the setup owner's own sentence.
// WL-SESSION-05 -- agents/workshop/session-restore.md
inline std::string in_layout(std::size_t at, const std::string& why) {
    return "layout at position " + std::to_string(at) + ": " + why;
}

/// AN ASSOCIATION THAT SAYS TWO THINGS AT ONCE.
// WL-SESSION-05 -- agents/workshop/session-restore.md
inline std::string half_a_link(std::size_t at) {
    return in_layout(at,
                     "this layout names no Setup file and carries a remembered Setup value "
                     "anyway -- a layout with no association remembers nothing");
}

/// A LAYOUT'S ASSOCIATION, AS THE LIVE VALUE IT NAMES -- or why it is not one.
// WL-SESSION-05, WL-SESSION-06 -- agents/workshop/session-restore.md
inline Written link_in(const WorkshopSetupLink& file, std::size_t at, SetupLink& out,
                       pane_migration::Converted* converted = nullptr) {
    if (file.path.empty()) {
        Setup nothing;
        // NOTHING IS COUNTED ON THIS ROAD: the question is whether a link with no path carries a
        // value anyway, and the answer is thrown away.
        if (setup_persist::setup_in(file.known, nothing).accepted) {
            return Written::no(half_a_link(at));
        }
        out = SetupLink{};
        return Written::ok();
    }
    Setup known;
    const Written understood = setup_persist::setup_in(file.known, known, converted);
    if (!understood.accepted) {
        return Written::no(in_layout(at, "its remembered Setup value is not one this "
                                         "Workshop can read -- " +
                                             understood.refusal));
    }
    out = SetupLink{file.path, std::move(known)};
    return Written::ok();
}

/// THE RUN, AS THE LIVE VALUES IT NAMES -- or the first reason it is not a run at all: no layout,
/// too many, an active position out of range, an id held twice or not positive, a next id at or
/// below one held, then each layout's desk and link.
// WL-SESSION-05, WL-SESSION-06 -- agents/workshop/session-restore.md
inline Written layouts_in(const WorkshopSession& file, std::vector<Layout>& run,
                          std::size_t& active, pane_migration::Converted* converted = nullptr) {
    if (file.layouts.empty()) {
        return Written::no(no_layouts());
    }
    if (file.layouts.size() > kMaxLayouts) {
        return Written::no(too_many_layouts(file.layouts.size()));
    }
    if (file.active < 0 || static_cast<std::size_t>(file.active) >= file.layouts.size()) {
        return Written::no(active_out_of_range(file.active, file.layouts.size()));
    }
    std::int64_t top = 0;
    for (std::size_t at = 0; at < file.layouts.size(); ++at) {
        const std::int64_t id = file.layouts[at].id;
        bool twice = false;
        for (std::size_t j = 0; j < at; ++j) {
            twice = twice || file.layouts[j].id == id;
        }
        if (id <= 0 || twice) {
            return Written::no(bad_id(at, id));
        }
        top = id > top ? id : top;
    }
    if (file.next_id <= top) {
        return Written::no(next_id_held(file.next_id, top));
    }
    std::vector<Layout> candidate;
    candidate.reserve(file.layouts.size());
    for (std::size_t at = 0; at < file.layouts.size(); ++at) {
        // EVERY LAYOUT MEETS THE SETUP OWNER'S WHOLE LAW, in the setup owner's own words.
        Setup desk;
        const Written understood =
            setup_persist::setup_in(file.layouts[at].desk, desk, converted);
        if (!understood.accepted) {
            return Written::no(in_layout(at, understood.refusal));
        }
        SetupLink link;
        const Written related = link_in(file.layouts[at].link, at, link, converted);
        if (!related.accepted) {
            return Written::no(related.refusal);
        }
        candidate.push_back(Layout{std::move(desk), std::move(link), file.layouts[at].id});
    }
    run = std::move(candidate);
    active = static_cast<std::size_t>(file.active);
    return Written::ok();
}

// WL-MIG-01 -- agents/workshop/migration.md

/// The format word and the version a session's own fields say, judged: empty when they are this
/// format's, else why not.
inline std::string format_refused(const std::string& format, std::int64_t format_version) {
    if (format != kFormat) {
        return "not a Workshop session: it says it is `" + format + "`";
    }
    // AND THE FIELD IS STILL CHECKED: a body that says it is a different vintage than its shape is
    // a forgery, on disk or from a conversion.
    if (format_version != kFormatVersion) {
        return forged_version(format_version);
    }
    return {};
}

/// A CURRENT SESSION AS A LOADED ONE -- this format's own law, all of it, on a session read from
/// its file's lines or answered by a conversion.
// WL-MIG-07 -- agents/workshop/migration.md
inline LoadedSession current_in(const WorkshopSession& file, std::int64_t read_version) {
    const std::string wrong = format_refused(file.format, file.format_version);
    if (!wrong.empty()) {
        return LoadedSession::no(wrong);
    }
    std::vector<Layout> run;
    std::size_t active = 0;
    pane_migration::Converted converted;
    const Written understood = layouts_in(file, run, active, &converted);
    if (!understood.accepted) {
        return LoadedSession::no(understood.refusal);
    }
    // THE CHOICES ARE BOUNDED; whether the plan still authors each is the load's to judge.
    if (file.choices.size() > kMaxRememberedChoices) {
        return LoadedSession::no("a session remembers at most " +
                                 std::to_string(kMaxRememberedChoices) + " offices' choices");
    }
    LoadedSession loaded;
    loaded.outcome = Written::ok();
    loaded.present = true;
    loaded.layouts = std::move(run);
    loaded.active = active;
    loaded.next_id = file.next_id;
    loaded.read_version = read_version;
    loaded.converted = converted;
    loaded.choices = file.choices;
    return loaded;
}

/// A file's lines: each ended by a newline, the last one whether or not a newline ends it.
inline std::vector<std::string_view> lines_of(std::string_view bytes) {
    std::vector<std::string_view> lines;
    while (!bytes.empty()) {
        const std::size_t end = bytes.find('\n');
        lines.push_back(bytes.substr(0, end));
        bytes = end == std::string_view::npos ? std::string_view() : bytes.substr(end + 1);
    }
    return lines;
}

/// HEADER AND LINES THAT DISAGREE: said in both numbers, since either may be the wrong one.
// WL-SESSION-06 -- agents/workshop/session-restore.md
inline std::string count_disagrees(std::int64_t counted, std::size_t lines) {
    return "this session's header counts " + std::to_string(counted) + " layouts, and " +
           std::to_string(lines) + " lines follow it";
}

/// A file that is the whole session as one value at this version: the form only memory holds.
// WL-SESSION-06 -- agents/workshop/session-restore.md
inline std::string one_value() {
    return "this session is one value; a session of version " + std::to_string(kFormatVersion) +
           " is a header line and a line per layout";
}

/// A SESSION'S FILE READ A LINE AT A TIME, each line decoded alone at its own shape, and refused
/// whole where any line is refused or the lines are not the header's count.
// WL-SESSION-06 -- agents/workshop/session-restore.md
inline LoadedSession lines_in(const loom::Unverified& first,
                              const std::vector<std::string_view>& lines) {
    const loom::Admission head =
        loom::admit(first, loom::schema_of<WorkshopSessionHeader>(), loom::Report::FirstError);
    if (!head.ok()) {
        return LoadedSession::no(head.first_error().message());
    }
    const auto header = loom::from_value<WorkshopSessionHeader>(head.value());
    const std::string wrong = format_refused(header.format, header.format_version);
    if (!wrong.empty()) {
        return LoadedSession::no(wrong);
    }
    const std::size_t follow = lines.size() - 1;
    if (header.layout_count < 0 || static_cast<std::uint64_t>(header.layout_count) != follow) {
        return LoadedSession::no(count_disagrees(header.layout_count, follow));
    }
    // A RUN PAST THE CEILING IS REFUSED BEFORE ONE OF ITS LINES IS DECODED.
    if (follow > kMaxLayouts) {
        return LoadedSession::no(too_many_layouts(follow));
    }
    WorkshopSession file;
    file.format = header.format;
    file.format_version = header.format_version;
    file.active = header.active;
    file.next_id = header.next_id;
    file.choices = header.choices;
    file.layouts.reserve(follow);
    for (std::size_t at = 0; at < follow; ++at) {
        const loom::Admission layout =
            loom::admit(loom::compat::parse(lines[at + 1]), loom::schema_of<WorkshopLayout>(),
                        loom::Report::FirstError);
        if (!layout.ok()) {
            return LoadedSession::no(in_layout(at, layout.first_error().message()));
        }
        file.layouts.push_back(loom::from_value<WorkshopLayout>(layout.value()));
    }
    return current_in(file, kFormatVersion);
}

/// AN OLDER FILE OF LINES AS THE ONE VALUE ITS VERSION'S HISTORY CONVERTS: the header's fields but
/// its count, and each layout line's fields as the run, claiming the session's own name at the
/// header's version -- assembled as text by the convention every file of lines keeps, so no
/// retired shape is read here; the conversion admits it whole at that version's own shape. Empty
/// when the lines are not a header and its count of layouts.
// WL-MIG-11 -- agents/workshop/migration.md
inline std::string whole_of_lines(const std::vector<std::string_view>& lines,
                                  std::uint32_t version) {
    namespace json = persist::json;
    const std::optional<std::string_view> fields = json::member(lines.front(), "fields");
    const std::optional<std::int64_t> counted =
        fields ? json::integer(json::member(*fields, "layout_count")) : std::nullopt;
    if (!fields || !counted || *counted < 0 ||
        static_cast<std::uint64_t>(*counted) != lines.size() - 1 ||
        lines.size() - 1 > kMaxLayouts) {
        return std::string();
    }
    std::string out = "{\"zen\":1,\"schema\":\"" + std::string(WorkshopSession::zen_name) +
                      "\",\"version\":" + std::to_string(version) + ",\"fields\":{";
    const auto header = json::members(*fields);
    if (!header) {
        return std::string();
    }
    for (const auto& [name, value] : *header) {
        if (name == "layout_count") {
            continue;
        }
        out += "\"" + std::string(name) + "\":" + std::string(value) + ",";
    }
    out += "\"layouts\":[";
    for (std::size_t at = 1; at < lines.size(); ++at) {
        const std::optional<std::string_view> layout = json::member(lines[at], "fields");
        if (!layout) {
            return std::string();
        }
        out += (at > 1 ? "," : "") + std::string(*layout);
    }
    out += "]}}";
    return out;
}

/// AN OLDER SESSION, ONE VALUE, BROUGHT FORWARD by this run's conversions, or refused in words.
// WL-MIG-06, WL-MIG-08 -- agents/workshop/migration.md
inline LoadedSession converted_in(const loom::Unverified& claim, const op::Catalog* conversions) {
    const op::Evaluation converted =
        op::migrate(conversions, claim, loom::schema_of<WorkshopSession>());
    if (!converted) {
        return LoadedSession::no(could_not_convert(claim.claimed_version(), converted.reason()));
    }
    return current_in(loom::from_value<WorkshopSession>(op::migrated(converted)),
                      static_cast<std::int64_t>(claim.claimed_version()));
}

/// Text to a session. Total: every input is either a session or a refusal with a reason, and
/// nothing here throws.
// WL-MIG-06, WL-MIG-08 -- agents/workshop/migration.md
inline LoadedSession from_text(std::string_view bytes,
                               const op::Catalog* conversions = nullptr) {
    // A FIRST LINE CLAIMING THE HEADER IS A FILE OF LINES: this format's, read a line at a time,
    // or an older one's, read whole and brought forward. Anything else is read as one value, as
    // every session before version 11 was written.
    const std::vector<std::string_view> lines = lines_of(bytes);
    if (!lines.empty()) {
        const loom::Unverified first = loom::compat::parse(lines.front());
        if (first.well_formed() &&
            first.claimed_name() == std::string(WorkshopSessionHeader::zen_name)) {
            if (first.claimed_version() == WorkshopSessionHeader::zen_version) {
                return lines_in(first, lines);
            }
            if (first.claimed_version() > WorkshopSessionHeader::zen_version) {
                return LoadedSession::no(could_not_convert(
                    first.claimed_version(), "this Workshop reads a header line of version " +
                                                 std::to_string(kFormatVersion) + " and older"));
            }
            const std::string whole = whole_of_lines(lines, first.claimed_version());
            if (whole.empty()) {
                return LoadedSession::no(could_not_convert(
                    first.claimed_version(),
                    "its header and the lines after it are not a header and its count of layouts"));
            }
            return converted_in(loom::compat::parse(whole), conversions);
        }
    }
    const loom::Unverified claim = loom::compat::parse(bytes);
    if (!claim.well_formed()) {
        const loom::Admission refused =
            loom::admit(claim, loom::schema_of<WorkshopSession>(), loom::Report::FirstError);
        return LoadedSession::no("not a Workshop session: " + refused.first_error().message());
    }
    // ---- The historical arm -------------------------------------------------
    // This shape's name at another version is the only door to a conversion: a file of another
    // format is refused by the gate below, so no corrupt, wrong or hostile file can turn into a
    // search for something willing to translate it.
    if (claim.claimed_name() == std::string(WorkshopSession::zen_name) &&
        claim.claimed_version() != WorkshopSession::zen_version) {
        return converted_in(claim, conversions);
    }
    if (claim.claimed_name() == std::string(WorkshopSession::zen_name)) {
        return LoadedSession::no(one_value());
    }
    // Anything else is no session, and the gate says why in its own words.
    const loom::Admission admitted =
        loom::admit(claim, loom::schema_of<WorkshopSession>(), loom::Report::FirstError);
    return LoadedSession::no(admitted.ok() ? one_value() : admitted.first_error().message());
}

// ---- The file itself -------------------------------------------------------------

/// Save the last session through `persist`'s safe write: a complete candidate to a sibling, then
/// a rename over the destination.
// WL-SESSION-13 -- agents/workshop/session.md
inline Written save_file(const std::string& path, const std::vector<Layout>& run,
                         std::size_t active, std::int64_t next_id,
                         const std::vector<WorkshopChoice>& choices = {}) {
    return persist::write_file_making_room(path, to_text(run, active, next_id, choices));
}

/// Read the last session from a file.
// WL-MIG-08 -- agents/workshop/migration.md; WL-SESSION-14 -- agents/workshop/session-restore.md
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
