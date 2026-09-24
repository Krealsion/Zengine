// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_SOURCE_TRANSFER_VOCABULARY_HPP
#define ZENGINE_SOURCE_TRANSFER_VOCABULARY_HPP

// TEXT AND FILE LOCATIONS AS TYPED MATERIAL -- what an editor hands Inventory, and what a drop on
// an editor means. Each is the pure item of an Inventory pair (`inventory/codec.hpp`); what was
// observed while acquiring it rides beside it as metadata and never inside it. Nothing here is a
// live link to a buffer, a grant, or a promise that a file still exists or holds what it held.
// The public contract is docs/reference/source-transfer.md.

#include <zen/weave/shape.hpp>

#include <cstddef>
#include <cstdint>
#include <string>

namespace zengine::source_transfer {

/// TEXT, AS CHARACTERS: valid UTF-8, each line break one LF. Where it came from is metadata
/// (`SourceSelection`), never part of the text, so two copies of the same characters are equal.
struct SourceText {
    std::string text;
    ZEN_SHAPE(SourceText, 1, ZEN_FIELD(text));
};

/// HOW A SELECTION WAS TAKEN, as observed at the moment of acquisition. Descriptive only: the
/// document may since have changed, closed or gone, and nothing here reaches it again.
struct SourceSelection {
    std::string editor;       ///< which editor held the document, in its own words
    std::string path;         ///< the document's normalized path; empty for a buffer with no file
    std::string project_root; ///< where the run began, as the editor was told; empty when unknown
    std::string kind;         ///< `characters`, `lines` or `block` (`kCharacters`...)
    std::int64_t first_line = 0;   ///< 1-based line of the first selected byte
    std::int64_t first_column = 0; ///< 1-based byte of it in that line
    std::int64_t end_line = 0;     ///< 1-based line of the position just after the selection
    std::int64_t end_column = 0;   ///< 1-based byte of that position
    std::string line_ending;       ///< the document's own break: `LF` or `CRLF`
    bool unsaved = false;          ///< the document held edits its file did not
    std::int64_t captured_at_epoch_s = 0; ///< this process's clock at acquisition
    ZEN_SHAPE(SourceSelection, 1, ZEN_FIELD(editor), ZEN_FIELD(path), ZEN_FIELD(project_root),
              ZEN_FIELD(kind), ZEN_FIELD(first_line), ZEN_FIELD(first_column),
              ZEN_FIELD(end_line), ZEN_FIELD(end_column), ZEN_FIELD(line_ending),
              ZEN_FIELD(unsaved), ZEN_FIELD(captured_at_epoch_s));
};

inline constexpr const char* kCharacters = "characters";
inline constexpr const char* kLines = "lines";
inline constexpr const char* kBlock = "block";

/// A FILE LOCATION TO OPEN AGAIN: an absolute, lexically normal path, and optionally the caret's
/// line and byte column (1-based; 0 = not recorded). A bookmark, not an identity: the path means
/// that spelling on this machine, never a moved file, another worktree's same-named file or a
/// filesystem object. Opening it is a request the receiving editor makes under current authority.
struct SourceLocation {
    std::string path;
    std::int64_t line = 0;
    std::int64_t column = 0;
    ZEN_SHAPE(SourceLocation, 1, ZEN_FIELD(path), ZEN_FIELD(line), ZEN_FIELD(column));
};

/// WHAT WAS TRUE WHEN A LOCATION WAS SAVED -- context for reading or rebinding it, never used to
/// resolve it. `line_text` is the caret line as it read then, so an editor can decline to place a
/// caret on a line that has since changed: an observation shorter than `kMaxLineText` bytes is the
/// whole line, which must still read exactly so; a longer line was observed as its first
/// `kMaxLineText` bytes (and the rest of a character they cut), which the line must still begin
/// with -- proving that beginning, not the rest. An empty `line_text` observes nothing.
struct SourceLocationContext {
    std::string editor;
    std::string project_root; ///< the run's project root then; empty when unknown
    std::string relative;     ///< `path` relative to that root, when it was inside it
    std::string line_text;    ///< the caret line then, whole or its first `kMaxLineText` bytes
    bool unsaved = false;     ///< the buffer held edits its file did not
    std::int64_t captured_at_epoch_s = 0;
    ZEN_SHAPE(SourceLocationContext, 1, ZEN_FIELD(editor), ZEN_FIELD(project_root),
              ZEN_FIELD(relative), ZEN_FIELD(line_text), ZEN_FIELD(unsaved),
              ZEN_FIELD(captured_at_epoch_s));
};

inline constexpr std::size_t kMaxLineText = 240;

} // namespace zengine::source_transfer

#endif
