// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_PREFS_PERSIST_HPP
#define ZENGINE_WORKSHOP_PREFS_PERSIST_HPP

// THE MAKER'S PRESENTATION PREFERENCES -- a seventh durable artifact, and the second file
// of the maker-configuration kind.
// Workshop law: agents/workshop/focus.md

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

namespace zengine::workshop::prefs_persist {

/// What a Workshop preferences file says it is -- its own word, beside the family's
/// others, so handing Workshop the wrong one of its own files is named rather than
/// half-read.
inline constexpr const char* kFormat = "zengine-workshop-prefs";

/// The only preferences format version this build reads or writes.
inline constexpr std::int64_t kFormatVersion = 1;

/// Preferences are the smallest file of the family: a couple of words. The ceiling is the
/// read side of the decoder's own materialisation law -- a hostile file does not get to
/// choose the cost of refusing it.
inline constexpr std::uintmax_t kMaxPrefsBytes = 1u << 16;

/// The file's suggested name, beside the family's other defaults.
inline constexpr const char* kDefaultPrefsFileName = "workshop-prefs.json";

// ---- The preference's words, and why they are words ------------------------------------
// WL-FOCUS-11 -- agents/workshop/focus.md

inline constexpr const char* kTitlesDefault = "default";
inline constexpr const char* kTitlesShown = "shown";
inline constexpr const char* kTitlesHidden = "hidden";

inline constexpr const char* kTitlesWords = "default, shown or hidden";

/// What the code answers when no preference is authored: titles are shown.
inline constexpr bool kTitlesDefaultValue = true;

// ---- The file's own shape --------------------------------------------------------------

/// A whole saved preferences file: what it is, which version of that it is, and the
/// preferences as words.
struct WorkshopPrefs {
    std::string format;
    std::int64_t format_version = 0;
    std::string titles;

    ZEN_SHAPE(WorkshopPrefs, 1, ZEN_FIELD(format), ZEN_FIELD(format_version),
              ZEN_FIELD(titles));
};

/// The envelope's shape version and the prefs format version are ONE NUMBER -- the
/// family's coupling, for the family's reason: a foreign version is refused by ITS number,
/// before its fields are judged against this shape.
static_assert(WorkshopPrefs::zen_version == static_cast<std::uint32_t>(kFormatVersion),
              "the prefs file's format version and its envelope's shape version are one "
              "number: a foreign version must be refused by ITS number, before its fields "
              "are judged against this shape");

// ---- Writing ---------------------------------------------------------------------------

/// The written word for a titles preference. A toggle is a stated preference, so a save
/// writes the concrete word for what the maker chose -- never `default`, which is the
/// hand-author's word for "whatever the code answers".
inline const char* titles_word(bool shown) { return shown ? kTitlesShown : kTitlesHidden; }

inline WorkshopPrefs to_prefs(bool titles_shown) {
    WorkshopPrefs out;
    out.format = kFormat;
    out.format_version = kFormatVersion;
    out.titles = titles_word(titles_shown);
    return out;
}

inline std::string to_text(bool titles_shown) {
    return loom::compat::serialize(loom::to_value(to_prefs(titles_shown)));
}

// ---- Reading ---------------------------------------------------------------------------

/// What reading produced: whether it worked, and the preferences if it did. `titles_shown`
/// is the EFFECTIVE value -- the authored word, or the code's answer where the word is
/// `default`.
struct LoadedPrefs {
    Written outcome;
    bool titles_shown = kTitlesDefaultValue;

    static LoadedPrefs no(std::string why) {
        return LoadedPrefs{Written::no(std::move(why)), kTitlesDefaultValue};
    }
};

/// One sentence for a version this build does not read, shared by the envelope preflight
/// and the field check so the two doors cannot word it differently.
inline std::string wrong_version(std::int64_t found) {
    return "prefs version " + std::to_string(found) + " -- this Workshop reads version " +
           std::to_string(kFormatVersion);
}

/// The titles value a written word means. False for a word outside the closed set.
inline bool titles_in(const std::string& word, bool& out) {
    if (word == kTitlesDefault) {
        out = kTitlesDefaultValue;
        return true;
    }
    if (word == kTitlesShown) {
        out = true;
        return true;
    }
    if (word == kTitlesHidden) {
        out = false;
        return true;
    }
    return false;
}

/// Text to preferences. Total: every input is either a preferences file or a refusal with
/// a reason, and nothing here throws. The version preflight reads the envelope's CLAIM
/// first, so a future file is refused by its number rather than by the first field it
/// renamed.
inline LoadedPrefs from_text(std::string_view bytes) {
    const loom::Unverified claim = loom::compat::parse(bytes);
    if (!claim.well_formed()) {
        const loom::Admission refused =
            loom::admit(claim, loom::schema_of<WorkshopPrefs>(), loom::Report::FirstError);
        return LoadedPrefs::no("not a Workshop prefs file: " +
                               refused.first_error().message());
    }
    if (claim.claimed_name() == std::string(WorkshopPrefs::zen_name) &&
        claim.claimed_version() != WorkshopPrefs::zen_version) {
        return LoadedPrefs::no(
            wrong_version(static_cast<std::int64_t>(claim.claimed_version())));
    }
    const loom::Admission admitted =
        loom::admit(claim, loom::schema_of<WorkshopPrefs>(), loom::Report::FirstError);
    if (!admitted.ok()) {
        return LoadedPrefs::no(admitted.first_error().message());
    }

    const WorkshopPrefs file = loom::from_value<WorkshopPrefs>(admitted.value());
    if (file.format != kFormat) {
        return LoadedPrefs::no("not a Workshop prefs file: it says it is `" + file.format +
                               "`");
    }
    if (file.format_version != kFormatVersion) {
        return LoadedPrefs::no(wrong_version(file.format_version));
    }
    LoadedPrefs loaded;
    if (!titles_in(file.titles, loaded.titles_shown)) {
        return LoadedPrefs::no("`" + file.titles + "` is not a pane-titles preference (" +
                               kTitlesWords + ")");
    }
    loaded.outcome = Written::ok();
    return loaded;
}

// ---- The file itself -------------------------------------------------------------------

/// Save the preferences, through the family's one safe write: a complete candidate to a
/// sibling, then a rename over the destination. Through `write_file_making_room`, because
/// this file's ordinary home is the per-user configuration root, which is created on the
/// first write and never on a read.
inline Written save_file(const std::string& path, bool titles_shown) {
    return persist::write_file_making_room(path, to_text(titles_shown));
}

/// Read preferences from a file. A missing file is not a refusal -- it is the defaults,
/// silently -- so the caller that wants to distinguish "no file" from "a file this build
/// refused" asks `std::filesystem::exists` first, exactly as the keymap load does.
inline LoadedPrefs load_file(const std::string& path) {
    const persist::FileText read =
        persist::read_file(path, kMaxPrefsBytes, "a Workshop prefs file");
    if (!read.outcome.accepted) {
        return LoadedPrefs{read.outcome, kTitlesDefaultValue};
    }
    return from_text(read.text);
}

} // namespace zengine::workshop::prefs_persist

#endif // ZENGINE_WORKSHOP_PREFS_PERSIST_HPP
