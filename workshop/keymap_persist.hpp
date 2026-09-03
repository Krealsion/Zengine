// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_KEYMAP_PERSIST_HPP
#define ZENGINE_WORKSHOP_KEYMAP_PERSIST_HPP

// THE MAKER'S KEYMAP FILE -- a sixth durable artifact, and a sixth KIND of durable fact.
// Workshop law: agents/workshop/keyboard.md (+1 registers; agents/workshop.md routes)

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
#include <vector>

namespace zengine::workshop::keymap_persist {

/// What a Workshop keymap file says it is -- its own word, beside the family's others, so
/// handing Workshop the wrong one of its own files is named rather than half-read.
inline constexpr const char* kFormat = "zengine-workshop-keymap";

/// The only keymap format version this build reads or writes.
inline constexpr std::int64_t kFormatVersion = 1;

/// A keymap is the smallest of the six files: a handful of two-string rows and one word.
/// The ceiling is the read side of the decoder's own materialisation law -- a hostile file
/// does not get to choose the cost of refusing it.
inline constexpr std::uintmax_t kMaxKeymapBytes = 1u << 16;

/// The file's suggested name, beside the family's other defaults.
inline constexpr const char* kDefaultKeymapFileName = "workshop-keymap.json";

// ---- The legend's words, and why they are words -----------------------------------------
// WL-KEY-09 -- agents/workshop/keyboard.md

inline constexpr const char* kLegendDefault = "default";
inline constexpr const char* kLegendFull = "full";
inline constexpr const char* kLegendCompact = "compact";
inline constexpr const char* kLegendHidden = "hidden";

inline constexpr const char* kLegendWords = "default, full, compact or hidden";

// ---- The file's own shapes --------------------------------------------------------------

/// One override row AS WRITTEN: which action, and the gesture that now requests it. Two
/// strings, deliberately -- the action id must be able to name an action this build has
/// never heard of, and the gesture must survive that row unjudged.
struct WorkshopKeymapRow {
    std::string action;
    std::string gesture;

    ZEN_SHAPE(WorkshopKeymapRow, 1, ZEN_FIELD(action), ZEN_FIELD(gesture));
};

/// A whole saved keymap: what it is, which version of that it is, the legend preference,
/// and the override rows IN AUTHORED ORDER.
struct WorkshopKeymap {
    std::string format;
    std::int64_t format_version = 0;
    std::string legend;
    std::vector<WorkshopKeymapRow> overrides;

    ZEN_SHAPE(WorkshopKeymap, 1, ZEN_FIELD(format), ZEN_FIELD(format_version),
              ZEN_FIELD(legend), ZEN_FIELD(overrides));
};

/// The envelope's shape version and the keymap format version are ONE NUMBER --
/// setup_persist's coupling, for its reason: it is what lets `from_text` refuse a future
/// file by its NUMBER, before its rows are judged against this build's shape.
static_assert(WorkshopKeymap::zen_version == static_cast<std::uint32_t>(kFormatVersion),
              "the keymap file's format version and its envelope's shape version are one "
              "number: a foreign version must be refused by ITS number, before its rows "
              "are judged against this shape");

// ---- Writing ----------------------------------------------------------------------------

/// The written word for a legend value. Total, and the fall-through is `default` -- the
/// one answer that cannot invent a preference (`unit_word`'s argument next door).
inline const char* legend_word(std::int64_t legend) {
    if (legend == legend_mode::kFull) {
        return kLegendFull;
    }
    if (legend == legend_mode::kCompact) {
        return kLegendCompact;
    }
    if (legend == legend_mode::kHidden) {
        return kLegendHidden;
    }
    return kLegendDefault;
}

/// The keymap, as the value that gets written.
// WL-KEY-07 -- agents/workshop/keyboard.md
inline WorkshopKeymap to_keymap(const Keymap& k) {
    WorkshopKeymap out;
    out.format = kFormat;
    out.format_version = kFormatVersion;
    out.legend = legend_word(k.legend);
    out.overrides.reserve(k.authored.size());
    for (const AuthoredOverride& row : k.authored) {
        out.overrides.push_back(WorkshopKeymapRow{row.action, row.gesture});
    }
    return out;
}

inline std::string to_text(const Keymap& k) {
    return loom::compat::serialize(loom::to_value(to_keymap(k)));
}

// ---- Reading ----------------------------------------------------------------------------

/// What reading produced: whether it worked, and the keymap if it did. Returned rather
/// than written through a reference, so a refused file cannot leave a live keymap halfway
/// restored.
struct LoadedKeymap {
    Written outcome;
    Keymap keymap;

    static LoadedKeymap no(std::string why) {
        return LoadedKeymap{Written::no(std::move(why)), {}};
    }
};

/// One sentence for a version this build does not read, shared by the envelope preflight
/// and the field check so the two doors cannot word it differently.
inline std::string wrong_version(std::int64_t found) {
    return "keymap version " + std::to_string(found) + " -- this Workshop reads version " +
           std::to_string(kFormatVersion);
}

/// The legend value a written word means. False for a word outside the closed set.
inline bool legend_in(const std::string& word, std::int64_t& out) {
    if (word == kLegendDefault) {
        out = legend_mode::kDefault;
        return true;
    }
    if (word == kLegendFull) {
        out = legend_mode::kFull;
        return true;
    }
    if (word == kLegendCompact) {
        out = legend_mode::kCompact;
        return true;
    }
    if (word == kLegendHidden) {
        out = legend_mode::kHidden;
        return true;
    }
    return false;
}

/// A WRITTEN KEYMAP, AS A LIVE ONE -- its format word, its version, its legend word, and
/// then the override law (`apply_overrides`: grammar, the global walls, twice-authored,
/// and the same-context collision refusal, each in words naming what a maker can fix).
inline Written keymap_in(const WorkshopKeymap& file, Keymap& out) {
    if (file.format != kFormat) {
        return Written::no("not a Workshop keymap: it says it is `" + file.format + "`");
    }
    if (file.format_version != kFormatVersion) {
        return Written::no(wrong_version(file.format_version));
    }
    std::int64_t legend = legend_mode::kDefault;
    if (!legend_in(file.legend, legend)) {
        return Written::no("`" + file.legend + "` is not a legend mode (" + kLegendWords +
                           ")");
    }
    std::vector<std::pair<std::string, std::string>> rows;
    rows.reserve(file.overrides.size());
    for (const WorkshopKeymapRow& row : file.overrides) {
        rows.emplace_back(row.action, row.gesture);
    }
    Keymap candidate;
    const Written applied = apply_overrides(rows, legend, candidate);
    if (!applied.accepted) {
        return applied;
    }
    out = std::move(candidate);
    return Written::ok();
}

/// Text to a keymap. Total: every input is either a keymap or a refusal with a reason,
/// and nothing here throws. The version preflight reads the envelope's CLAIM first, so a
/// future file is refused by its number rather than by the first field it renamed.
inline LoadedKeymap from_text(std::string_view bytes) {
    const loom::Unverified claim = loom::compat::parse(bytes);
    if (!claim.well_formed()) {
        const loom::Admission refused =
            loom::admit(claim, loom::schema_of<WorkshopKeymap>(), loom::Report::FirstError);
        return LoadedKeymap::no("not a Workshop keymap: " + refused.first_error().message());
    }
    if (claim.claimed_name() == std::string(WorkshopKeymap::zen_name) &&
        claim.claimed_version() != WorkshopKeymap::zen_version) {
        return LoadedKeymap::no(
            wrong_version(static_cast<std::int64_t>(claim.claimed_version())));
    }
    const loom::Admission admitted =
        loom::admit(claim, loom::schema_of<WorkshopKeymap>(), loom::Report::FirstError);
    if (!admitted.ok()) {
        return LoadedKeymap::no(admitted.first_error().message());
    }

    Keymap candidate;
    const Written understood =
        keymap_in(loom::from_value<WorkshopKeymap>(admitted.value()), candidate);
    if (!understood.accepted) {
        return LoadedKeymap::no(understood.refusal);
    }

    LoadedKeymap loaded;
    loaded.outcome = Written::ok();
    loaded.keymap = std::move(candidate);
    return loaded;
}

// ---- The file itself --------------------------------------------------------------------

/// Save a keymap, through the family's one safe write: a complete candidate to a sibling,
/// then a rename over the destination. The promise is `persist::write_file`'s and is not
/// restated as though it were a second mechanism.
inline Written save_file(const std::string& path, const Keymap& k) {
    return persist::write_file(path, to_text(k));
}

/// Read a keymap from a file. The composition of every layer: the file, the format, and
/// the override law.
// WL-KEY-07 -- agents/workshop/keyboard.md
inline LoadedKeymap load_file(const std::string& path) {
    const persist::FileText read =
        persist::read_file(path, kMaxKeymapBytes, "a Workshop keymap");
    if (!read.outcome.accepted) {
        return LoadedKeymap{read.outcome, {}};
    }
    return from_text(read.text);
}

} // namespace zengine::workshop::keymap_persist

#endif // ZENGINE_WORKSHOP_KEYMAP_PERSIST_HPP
