// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_PREFS_PERSIST_HPP
#define ZENGINE_WORKSHOP_PREFS_PERSIST_HPP

// THE MAKER'S PRESENTATION PREFERENCES -- a seventh durable artifact, and the second file
// of the maker-configuration kind (WUX-3).
//
// The keymap file is the maker's HAND: which gesture requests which action. This file is
// the maker's EYES: how Workshop presents itself, starting with the one preference that
// earned durability by being toggled and lost across runs -- whether the arrangeable
// panes paint their title rows (WUX-1's `t`). They are two files because they are two
// promises with two custodies: the keymap is hand-edited and Workshop never writes it on
// its own; this file is written BY Workshop, at the moment the maker states a preference,
// and hand-editing it is legal but not its ordinary life. Parking presentation preferences
// in the keymap file instead would grow "the maker's hand" into a miscellaneous drawer and
// cost every existing keymap file a version bump for a fact that is not about keys.
//
// IT IS DELIBERATELY NOT A CONFIGURATION FRAMEWORK. One format, a handful of word fields,
// the family's standing persistence law (the Loom compat codec, the same gate the live bus
// uses, safe-write through `persist::write_file`, a format identity that refuses the other
// files by name, deterministic bytes). A future presentation preference is one field and
// one version here -- and anything that is not presentation preference belongs somewhere
// with its own name.
//
// ---- What version 1 promises -----------------------------------------------------------
//
//   PROMISED   Workshop reads and writes prefs format version 1: the pane-title
//              visibility, as a word. Written when the maker toggles the preference, so
//              the file exists exactly when a preference has been stated; an absent file
//              is the defaults, silently.
//   REFUSED    any other `format_version`, by number, from the envelope's claim BEFORE the
//              fields are judged (the family's preflight); a `format` that is not this
//              one; a field the shape does not declare; a titles word outside the closed
//              set, with what was found and what would have worked both named; a file
//              larger than preferences can be.
//   NOT DONE   pane colors, themes, per-pane preferences, appearance configuration,
//              migration, a version graph. One version exists.
//
// A FILE THAT EXISTS AND CANNOT BE ADMITTED IS REFUSED OUT LOUD AND NEVER OVERWRITTEN.
// The refusal stands on the notice line, the defaults stand on screen, and a later toggle
// changes the LIVE preference without writing -- Workshop does not rewrite, half-apply or
// delete a file it could not understand (the keymap's own law, KEY-0), and a toggle that
// silently replaced a maker's unreadable bytes with a fresh file would be exactly that.

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
//
// The legend's decision, for the legend's reason: the in-memory value is a bool today and
// is free to become anything tomorrow, and a renumbering or re-typing must not be able to
// change what a saved preference means. `default` is the one canonical spelling of "no
// authored difference" -- this build projects it as shown -- and a maker who authored
// `shown` has pinned that word against any future default.

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
