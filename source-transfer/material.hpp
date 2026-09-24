// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_SOURCE_TRANSFER_MATERIAL_HPP
#define ZENGINE_SOURCE_TRANSFER_MATERIAL_HPP

// WHAT A DROPPED PAIR MEANS TO AN EDITOR, AND THE PAIRS AN EDITOR HANDS OUT.
//
// A drop on an editor carries an owned Inventory pair (`inventory/codec.hpp`): an item and its
// metadata, each admitted against the schema its own closure declares. An editor reads exactly
// four meanings from it and refuses everything else in words:
//
//   SourceText                       text to insert
//   an Info FieldValue of a Text     that field's text (Info's typed field pickup)
//   SourceLocation                   a file to open or reveal -- never inserted as text
//   any other message, or a preset   a command: its Terminal line (command_line.hpp), and in a
//                                     C++ document, by a separate choice, C++ (cpp.hpp)
//
// Metadata stays metadata. The one observation a command reads is where a Terminal capture was
// SENT (`TerminalCaptureFacts`, a submitted entry addressed to an office or published) -- written
// into the line as the address it was, never as permission -- and a location's saved context. The
// schema identities are compiled here and compared whole: a shape that merely shares a name is
// not the one this file knows.

#include "source-transfer/command_line.hpp"
#include "source-transfer/text.hpp"
#include "source-transfer/vocabulary.hpp"

#include "inventory/codec.hpp"
#include "message-draft/transfer.hpp"
#include "workshop/terminal_seam_vocabulary.hpp"

#include <zen/weave/shape.hpp>

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace zengine::source_transfer {

/// The carrier's own bound (`workshop/pane_carry.hpp`): an envelope larger than this is refused
/// before it is offered, never truncated.
inline constexpr std::size_t kMaxCarryBytes = 65536;

enum class MaterialKind { Text, Command, Location, Unsupported };

struct Material {
    MaterialKind kind = MaterialKind::Unsupported;
    std::string what;                   ///< how a notice names it: "SourceText", "EnsureTimer v1"
    std::string text;                   ///< Text: the text
    std::optional<loom::Value> command; ///< Command: the message, or a preset's draft of it
    bool preset = false;                ///< Command: from an unfinished preset (StoredDraft)
    std::string address;                ///< Command: `@office` or `*` the capture supplied, else ""
    std::string address_note;           ///< Command: why a supplied address was not used
    SourceLocation location;            ///< Location
    std::optional<SourceLocationContext> location_context;
    std::string refusal;                ///< Unsupported: why, in words
};

namespace detail {
template <class T>
bool is(const loom::Value& v) {
    return loom::same_identity(v.schema(), *loom::schema_of<T>());
}
} // namespace detail

/// READ A DROPPED PAIR. Never throws: a malformed or unknown envelope is Unsupported, in words.
inline Material read_material(std::string_view pair_bytes) {
    Material m;
    std::optional<inventory::DecodedPair> decoded;
    try {
        decoded = inventory::decode_pair(pair_bytes);
    } catch (const std::exception& e) {
        m.refusal = std::string("the dropped value could not be read: ") + e.what();
        return m;
    }
    const inventory::DecodedPair& pair = *decoded;
    const loom::Value& item = pair.item;
    m.what = item.schema().name() + " v" + std::to_string(item.schema().version());
    try {
        if (detail::is<SourceText>(item)) {
            m.kind = MaterialKind::Text;
            m.text = loom::from_value<SourceText>(item).text;
            return m;
        }
        if (detail::is<SourceLocation>(item)) {
            m.kind = MaterialKind::Location;
            m.location = loom::from_value<SourceLocation>(item);
            for (const loom::Value& meta : pair.metadata) {
                if (detail::is<SourceLocationContext>(meta)) {
                    m.location_context = loom::from_value<SourceLocationContext>(meta);
                }
            }
            return m;
        }
        if (message_draft::is_field_value(item)) {
            const message_draft::FieldValue field = message_draft::read_field(item);
            if (field.type.kind != loom::Kind::Text) {
                m.refusal = "the dropped field is " + std::string(loom::name_of(field.type.kind)) +
                            ", not text -- drop the whole command to insert its Terminal line";
                return m;
            }
            m.kind = MaterialKind::Text;
            m.what = "a text field";
            m.text = field.cell.as_text();
            return m;
        }
        if (message_draft::is_stored_draft(item)) {
            const message_draft::Preset preset = message_draft::read_draft(item);
            m.kind = MaterialKind::Command;
            m.preset = true;
            m.command = preset.draft.snapshot();
            m.what = "the preset " + preset.draft.schema()->name() + " v" +
                     std::to_string(preset.draft.schema()->version());
            return m;
        }
    } catch (const std::exception& e) {
        m.kind = MaterialKind::Unsupported;
        m.refusal = "the dropped " + m.what + " could not be read: " + e.what();
        return m;
    }
    m.kind = MaterialKind::Command;
    m.command = item;
    for (const loom::Value& meta : pair.metadata) {
        if (!detail::is<workshop::TerminalCaptureFacts>(meta)) {
            continue;
        }
        const auto facts = loom::from_value<workshop::TerminalCaptureFacts>(meta);
        if (facts.kind != "submitted") {
            continue; // a received or answered value was addressed to the terminal, not by it
        }
        if (facts.addressing == "role" && !facts.role.empty()) {
            m.address = "@" + facts.role;
        } else if (facts.addressing == "publish") {
            m.address = "*";
        } else if (facts.addressing == "weave") {
            m.address_note = "it was sent to weave #" + facts.target +
                             ", an identity of that run only, so the line names no address";
        }
    }
    return m;
}

inline std::int64_t clock_now() {
    return static_cast<std::int64_t>(std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
}

/// A CAPTURE'S PAIR: the item and its one observation, or why it cannot be carried.
struct Pair {
    bool ok = false;
    std::string bytes;
    std::string refusal;
};

namespace detail {
inline Pair seal(const loom::Value& item, const loom::Value& meta) {
    Pair p;
    try {
        p.bytes = inventory::encode_pair(item, {meta});
    } catch (const std::exception& e) {
        p.refusal = std::string("the copy could not be made: ") + e.what();
        return p;
    }
    if (p.bytes.size() > kMaxCarryBytes) {
        p.refusal = "the copy is " + std::to_string(p.bytes.size()) +
                    " bytes, more than the 64 KiB a carry holds -- select less";
        p.bytes.clear();
        return p;
    }
    p.ok = true;
    return p;
}
} // namespace detail

inline Pair text_pair(const std::string& text, const SourceSelection& selection) {
    if (!valid_utf8(text)) {
        return Pair{false, {}, "the selection is not valid UTF-8, which text cannot carry"};
    }
    if (text.find('\0') != std::string::npos) {
        return Pair{false, {}, "the selection holds a NUL byte, which no editor here inserts faithfully"};
    }
    return detail::seal(loom::to_value(SourceText{text}), loom::to_value(selection));
}

inline Pair location_pair(const SourceLocation& location, const SourceLocationContext& context) {
    return detail::seal(loom::to_value(location), loom::to_value(context));
}

/// `path` relative to `root` when it lies inside it (both normalized, forward slashes), else "".
inline std::string relative_to(const std::string& path, const std::string& root) {
    if (root.empty() || path.size() <= root.size() + 1 || path.compare(0, root.size(), root) != 0) {
        return std::string();
    }
    const char sep = path[root.size()];
    if (sep != '/' && root.back() != '/') {
        return std::string();
    }
    return path.substr(root.size() + (root.back() == '/' ? 0 : 1));
}

} // namespace zengine::source_transfer

#endif
