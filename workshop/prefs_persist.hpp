// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_PREFS_PERSIST_HPP
#define ZENGINE_WORKSHOP_PREFS_PERSIST_HPP

// THE WEAVER'S PRESENTATION PREFERENCES -- a seventh durable artifact, and the second file
// of the weaver-configuration kind.
// Workshop law: agents/workshop/focus.md, agents/workshop/key-spelling.md

#include "keymap.hpp"
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

/// The preferences format version this build writes. It reads this one and the one before it,
/// whose file held the titles alone.
inline constexpr std::int64_t kFormatVersion = 2;
inline constexpr std::int64_t kTitlesOnlyVersion = 1;

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

// WL-KEY-16 -- agents/workshop/key-spelling.md
inline constexpr const char* kKeysDefault = "default";
inline constexpr const char* kKeysShort = kSpelledShort;
inline constexpr const char* kKeysFull = kSpelledFull;

inline constexpr const char* kKeysWords = "default, short or full";

/// What the code answers when no spelling is authored: the band short, the Hotkeys view full.
inline constexpr KeySpelling kBandKeysDefaultValue = KeySpelling::kShort;
inline constexpr KeySpelling kHotkeysKeysDefaultValue = KeySpelling::kFull;

/// THE PREFERENCES AS VALUES: whether the arrangeable panes wear their titles, and how the band
/// and the Hotkeys view each spell a key.
struct Presentation {
    bool titles_shown = kTitlesDefaultValue;
    KeySpelling band_keys = kBandKeysDefaultValue;
    KeySpelling hotkeys_keys = kHotkeysKeysDefaultValue;
};

// ---- The file's own shape --------------------------------------------------------------

/// A whole saved preferences file: what it is, which version of that it is, and the
/// preferences as words.
struct WorkshopPrefs {
    std::string format;
    std::int64_t format_version = 0;
    std::string titles;
    std::string band_keys;
    std::string hotkeys_keys;

    ZEN_SHAPE(WorkshopPrefs, 2, ZEN_FIELD(format), ZEN_FIELD(format_version),
              ZEN_FIELD(titles), ZEN_FIELD(band_keys), ZEN_FIELD(hotkeys_keys));
};

/// THE FILE AS VERSION 1 WROTE IT, the titles alone, copied verbatim: read as today's with each
/// spelling at its default, and never written.
namespace v1 {
struct WorkshopPrefs {
    std::string format;
    std::int64_t format_version = 0;
    std::string titles;

    ZEN_SHAPE(WorkshopPrefs, 1, ZEN_FIELD(format), ZEN_FIELD(format_version),
              ZEN_FIELD(titles));
};
} // namespace v1

/// The envelope's shape version and the prefs format version are ONE NUMBER -- the
/// family's coupling, for the family's reason: a foreign version is refused by ITS number,
/// before its fields are judged against this shape.
static_assert(WorkshopPrefs::zen_version == static_cast<std::uint32_t>(kFormatVersion),
              "the prefs file's format version and its envelope's shape version are one "
              "number: a foreign version must be refused by ITS number, before its fields "
              "are judged against this shape");
static_assert(v1::WorkshopPrefs::zen_version == static_cast<std::uint32_t>(kTitlesOnlyVersion),
              "the titles-only file is read by its own number");

// ---- Writing ---------------------------------------------------------------------------

/// The written word for a titles preference. A toggle is a stated preference, so a save
/// writes the concrete word for what the weaver chose -- never `default`, which is the
/// hand-author's word for "whatever the code answers".
inline const char* titles_word(bool shown) { return shown ? kTitlesShown : kTitlesHidden; }

/// ...and the written word for a spelling, by the same rule.
inline const char* keys_word(KeySpelling spelling) { return spelling_word(spelling); }

inline WorkshopPrefs to_prefs(const Presentation& said) {
    WorkshopPrefs out;
    out.format = kFormat;
    out.format_version = kFormatVersion;
    out.titles = titles_word(said.titles_shown);
    out.band_keys = keys_word(said.band_keys);
    out.hotkeys_keys = keys_word(said.hotkeys_keys);
    return out;
}

inline std::string to_text(const Presentation& said) {
    return loom::compat::serialize(loom::to_value(to_prefs(said)));
}

// ---- Reading ---------------------------------------------------------------------------

/// What reading produced: whether it worked, and the preferences if it did. Each is the
/// EFFECTIVE value -- the authored word, or the code's answer where the word is `default`.
struct LoadedPrefs {
    Written outcome;
    Presentation prefs;

    static LoadedPrefs no(std::string why) {
        return LoadedPrefs{Written::no(std::move(why)), Presentation{}};
    }
};

/// One sentence for a version this build does not read, shared by the envelope preflight
/// and the field check so the two doors cannot word it differently.
inline std::string wrong_version(std::int64_t found) {
    return "prefs version " + std::to_string(found) + " -- this Workshop reads versions " +
           std::to_string(kTitlesOnlyVersion) + " and " + std::to_string(kFormatVersion);
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

/// The spelling a written word means, `fallback` for `default`. False for a word outside the
/// closed set.
inline bool keys_in(const std::string& word, KeySpelling fallback, KeySpelling& out) {
    if (word == kKeysDefault) {
        out = fallback;
        return true;
    }
    if (word == kKeysShort) {
        out = KeySpelling::kShort;
        return true;
    }
    if (word == kKeysFull) {
        out = KeySpelling::kFull;
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
    // VERSION 1 HELD THE TITLES ALONE: admitted against its own shape, and read as today's file
    // with each spelling `default`.
    const bool titles_only = claim.claimed_name() == std::string(WorkshopPrefs::zen_name) &&
                             claim.claimed_version() == v1::WorkshopPrefs::zen_version;
    if (claim.claimed_name() == std::string(WorkshopPrefs::zen_name) && !titles_only &&
        claim.claimed_version() != WorkshopPrefs::zen_version) {
        return LoadedPrefs::no(
            wrong_version(static_cast<std::int64_t>(claim.claimed_version())));
    }
    WorkshopPrefs file;
    if (titles_only) {
        const loom::Admission admitted =
            loom::admit(claim, loom::schema_of<v1::WorkshopPrefs>(), loom::Report::FirstError);
        if (!admitted.ok()) {
            return LoadedPrefs::no(admitted.first_error().message());
        }
        const v1::WorkshopPrefs old = loom::from_value<v1::WorkshopPrefs>(admitted.value());
        file.format = old.format;
        file.format_version = old.format_version;
        file.titles = old.titles;
        file.band_keys = kKeysDefault;
        file.hotkeys_keys = kKeysDefault;
    } else {
        const loom::Admission admitted =
            loom::admit(claim, loom::schema_of<WorkshopPrefs>(), loom::Report::FirstError);
        if (!admitted.ok()) {
            return LoadedPrefs::no(admitted.first_error().message());
        }
        file = loom::from_value<WorkshopPrefs>(admitted.value());
    }
    if (file.format != kFormat) {
        return LoadedPrefs::no("not a Workshop prefs file: it says it is `" + file.format +
                               "`");
    }
    if (file.format_version != (titles_only ? kTitlesOnlyVersion : kFormatVersion)) {
        return LoadedPrefs::no(wrong_version(file.format_version));
    }
    LoadedPrefs loaded;
    if (!titles_in(file.titles, loaded.prefs.titles_shown)) {
        return LoadedPrefs::no("`" + file.titles + "` is not a pane-titles preference (" +
                               kTitlesWords + ")");
    }
    if (!keys_in(file.band_keys, kBandKeysDefaultValue, loaded.prefs.band_keys)) {
        return LoadedPrefs::no("`" + file.band_keys + "` is not a spelling for the band's keys (" +
                               kKeysWords + ")");
    }
    if (!keys_in(file.hotkeys_keys, kHotkeysKeysDefaultValue, loaded.prefs.hotkeys_keys)) {
        return LoadedPrefs::no("`" + file.hotkeys_keys +
                               "` is not a spelling for the Hotkeys view's keys (" + kKeysWords +
                               ")");
    }
    loaded.outcome = Written::ok();
    return loaded;
}

// ---- The file itself -------------------------------------------------------------------

/// Save the preferences, through the family's one safe write: a complete candidate to a
/// sibling, then a rename over the destination. Through `write_file_making_room`, because
/// this file's ordinary home is the per-user configuration root, which is created on the
/// first write and never on a read.
inline Written save_file(const std::string& path, const Presentation& said) {
    return persist::write_file_making_room(path, to_text(said));
}

/// Read preferences from a file. A missing file is not a refusal -- it is the defaults,
/// silently -- so the caller that wants to distinguish "no file" from "a file this build
/// refused" asks `std::filesystem::exists` first, exactly as the keymap load does.
inline LoadedPrefs load_file(const std::string& path) {
    const persist::FileText read =
        persist::read_file(path, kMaxPrefsBytes, "a Workshop prefs file");
    if (!read.outcome.accepted) {
        return LoadedPrefs{read.outcome, Presentation{}};
    }
    return from_text(read.text);
}

} // namespace zengine::workshop::prefs_persist

#endif // ZENGINE_WORKSHOP_PREFS_PERSIST_HPP
