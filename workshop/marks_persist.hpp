// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_MARKS_PERSIST_HPP
#define ZENGINE_WORKSHOP_MARKS_PERSIST_HPP

// THE PLACES A MAKER SAID THEY WANT BACK -- an eighth durable artifact, and the third file
// of the maker's-own-facts kind.
// Workshop law: agents/workshop/files.md

#include "marks.hpp"
#include "path_admission.hpp"
#include "persist.hpp"

#include <zen/admission.hpp>
#include <zen/schema.hpp>
#include <zen/serialize.hpp>
#include <zen/value.hpp>
#include <zen/weave/shape.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace zengine::workshop::marks_persist {

/// What a Workshop marks file says it is -- its own word, beside the family's others, so
/// handing Workshop the wrong one of its own files is named rather than half-read.
inline constexpr const char* kFormat = "zengine-workshop-marks";

/// The only marks format version this build reads or writes.
inline constexpr std::int64_t kFormatVersion = 1;

/// A mark list is a handful of paths. The ceiling is the read side of the decoder's own
/// materialisation law -- a hostile file does not get to choose the cost of refusing it --
/// and 64 KiB is the same order of magnitude every other file in this family reasons from.
inline constexpr std::uintmax_t kMaxMarksBytes = 1u << 16;

/// The file's suggested name, beside the family's other defaults.
inline constexpr const char* kDefaultMarksFileName = "workshop-marks.json";

// ---- The file's own shapes ---------------------------------------------------------------

/// ONE MARK AS WRITTEN: the location, and nothing else.
// WL-FILES-08 -- agents/workshop/files.md
struct WorkshopMark {
    std::string path;

    ZEN_SHAPE(WorkshopMark, 1, ZEN_FIELD(path));
};

/// A whole saved mark list: what it is, which version of that it is, and the places.
struct WorkshopMarks {
    std::string format;
    std::int64_t format_version = 0;
    std::vector<WorkshopMark> marks;

    ZEN_SHAPE(WorkshopMarks, 1, ZEN_FIELD(format), ZEN_FIELD(format_version),
              ZEN_FIELD(marks));
};

/// The envelope's shape version and the marks format version are ONE NUMBER -- the
/// family's coupling, for the family's reason: a foreign version is refused by ITS number,
/// before its rows are judged against this shape.
static_assert(WorkshopMarks::zen_version == static_cast<std::uint32_t>(kFormatVersion),
              "the marks file's format version and its envelope's shape version are one "
              "number: a file of another version must be refused by ITS number, before its "
              "rows are judged against this shape");

// ---- Writing -----------------------------------------------------------------------------

/// The marks, as the value that gets written. The owner already holds them normalized,
/// unique and in one order, so writing is observation and sorts nothing on the way out.
inline WorkshopMarks to_marks(const std::vector<std::string>& maker) {
    WorkshopMarks out;
    out.format = kFormat;
    out.format_version = kFormatVersion;
    out.marks.reserve(maker.size());
    for (const std::string& path : maker) {
        out.marks.push_back(WorkshopMark{path});
    }
    return out;
}

inline std::string to_text(const std::vector<std::string>& maker) {
    return loom::compat::serialize(loom::to_value(to_marks(maker)));
}

// ---- Reading -----------------------------------------------------------------------------

/// What reading produced: whether the FILE was understood, the places that came out of it,
/// and the sentence about any row that did not.
struct LoadedMarks {
    Written outcome;
    std::vector<std::string> maker; ///< normalized, unique, sorted -- the owner's own order
    std::string skipped;            ///< empty when every row was usable

    static LoadedMarks no(std::string why) {
        return LoadedMarks{Written::no(std::move(why)), {}, {}};
    }
};

/// One sentence for a version this build does not read, shared by the envelope preflight
/// and the field check so the two doors cannot word it differently.
inline std::string wrong_version(std::int64_t found) {
    return "marks version " + std::to_string(found) + " -- this Workshop reads version " +
           std::to_string(kFormatVersion);
}

/// A WRITTEN MARK LIST, AS LIVE PLACES. The format word and the version are the file's
/// claims about itself and refuse it whole; each row is then admitted on its own.
inline Written marks_in(const WorkshopMarks& file, std::vector<std::string>& out,
                        std::string& skipped) {
    if (file.format != kFormat) {
        return Written::no("not a Workshop marks file: it says it is `" + file.format + "`");
    }
    if (file.format_version != kFormatVersion) {
        return Written::no(wrong_version(file.format_version));
    }
    LocationMarks candidate;
    std::size_t refused = 0;
    std::string first;
    for (const WorkshopMark& row : file.marks) {
        // ⚠ THE ROW IS ADMITTED, NOT TRUSTED. These bytes were written by this application
        // and may have been edited by a person since, and turning a stored narrow spelling
        // back into a path is a conversion that REFUSES on some platforms (measured
        // throw, in its other direction). A relative spelling is refused here too, and not
        // resolved against anything: a place is where it is, and re-basing a remembered one
        // against wherever this process happens to be standing would be the two-bases defect
        // `persist::resolved_against` exists to end.
        const std::string located = admit_location(row.path);
        if (located.empty()) {
            ++refused;
            if (first.empty()) {
                first = row.path.empty() ? std::string("(an empty location)") : row.path;
            }
            continue;
        }
        candidate.remember(located);
    }
    if (refused > 0) {
        skipped = std::to_string(refused) + (refused == 1 ? " mark was" : " marks were") +
                  " skipped -- a mark must be an absolute location this Workshop can carry"
                  ", and `" + first + "` is not one";
    }
    out = std::move(candidate.maker);
    return Written::ok();
}

/// Text to marks. Total: every input is either a mark list or a refusal with a reason, and
/// nothing here throws. The version preflight reads the envelope's CLAIM first, so a future
/// file is refused by its number rather than by the first field it renamed.
inline LoadedMarks from_text(std::string_view bytes) {
    const loom::Unverified claim = loom::compat::parse(bytes);
    if (!claim.well_formed()) {
        const loom::Admission refused =
            loom::admit(claim, loom::schema_of<WorkshopMarks>(), loom::Report::FirstError);
        return LoadedMarks::no("not a Workshop marks file: " +
                               refused.first_error().message());
    }
    if (claim.claimed_name() == std::string(WorkshopMarks::zen_name) &&
        claim.claimed_version() != WorkshopMarks::zen_version) {
        return LoadedMarks::no(
            wrong_version(static_cast<std::int64_t>(claim.claimed_version())));
    }
    const loom::Admission admitted =
        loom::admit(claim, loom::schema_of<WorkshopMarks>(), loom::Report::FirstError);
    if (!admitted.ok()) {
        return LoadedMarks::no(admitted.first_error().message());
    }

    LoadedMarks loaded;
    const Written understood = marks_in(loom::from_value<WorkshopMarks>(admitted.value()),
                                        loaded.maker, loaded.skipped);
    if (!understood.accepted) {
        return LoadedMarks::no(understood.refusal);
    }
    loaded.outcome = Written::ok();
    return loaded;
}

// ---- The file itself ---------------------------------------------------------------------

/// Save the marks, through the family's one safe write: a complete candidate to a sibling,
/// then a rename over the destination. Through `write_file_making_room`, because this
/// file's ordinary home is the per-user state root, which is created on the first write and
/// never on a read.
inline Written save_file(const std::string& path, const std::vector<std::string>& maker) {
    return persist::write_file_making_room(path, to_text(maker));
}

/// Read the marks from a file. A missing file is not a refusal -- it is no marks, silently
/// -- so the caller that wants to distinguish "no file" from "a file this build refused"
/// asks `std::filesystem::exists` first, exactly as the keymap load does.
inline LoadedMarks load_file(const std::string& path) {
    const persist::FileText read =
        persist::read_file(path, kMaxMarksBytes, "a Workshop marks file");
    if (!read.outcome.accepted) {
        return LoadedMarks{read.outcome, {}, {}};
    }
    return from_text(read.text);
}

} // namespace zengine::workshop::marks_persist

#endif // ZENGINE_WORKSHOP_MARKS_PERSIST_HPP
