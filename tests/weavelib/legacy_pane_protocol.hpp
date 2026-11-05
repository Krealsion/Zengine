// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_TESTS_WEAVELIB_LEGACY_PANE_PROTOCOL_HPP
#define ZENGINE_TESTS_WEAVELIB_LEGACY_PANE_PROTOCOL_HPP

// The pane protocol as `5e96e05` published it: the shapes a provider built before ownership
// existed derives, so an image can speak them without the current header and a suite can compare
// them with it (`loom::same_identity`). The spellings match the current header's because a
// shape's identity is its name, version and fields, the name taken from the type token
// (Loom GATE-04), so this file's version one is the host's. Only the shapes the legacy image
// speaks or hears; the protocol's law is agents/panes.md.


#include <zen/weave/shape.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace legacy_protocol {

/// The numbers that protocol drew text by: ordinary, quiet, and no ground at all.
inline constexpr std::int64_t kOrdinaryText = 0;
inline constexpr std::int64_t kQuietText = 2;
inline constexpr std::int64_t kNoGround = -1;

/// One row of a pane's text as that protocol carried it: its words, the number it was drawn by,
/// and the number of a ground beneath them.
struct SurfaceTextRow {
    std::string text;
    std::int64_t role = kOrdinaryText;
    std::int64_t background = kNoGround;
    ZEN_SHAPE(SurfaceTextRow, 2, ZEN_FIELD(text), ZEN_FIELD(role), ZEN_FIELD(background));
};

struct PaneCatalogRequested {
    ZEN_SHAPE(PaneCatalogRequested, 1);
};

struct PaneOffered {
    std::string pane;
    std::string name;
    std::string summary;
    ZEN_SHAPE(PaneOffered, 1, ZEN_FIELD(pane), ZEN_FIELD(name), ZEN_FIELD(summary));
};

struct PaneRoom {
    std::string pane;
    std::int64_t rows = 0;
    std::int64_t columns = 0;
    ZEN_SHAPE(PaneRoom, 1, ZEN_FIELD(pane), ZEN_FIELD(rows), ZEN_FIELD(columns));
};

struct PaneContent {
    std::string pane;
    std::vector<SurfaceTextRow> rows;
    ZEN_SHAPE(PaneContent, 1, ZEN_FIELD(pane), ZEN_FIELD(rows));
};

struct PaneActionRow {
    std::string id;
    std::string label;
    std::int64_t scancode = 0;
    std::int64_t modifiers = 0;
    ZEN_SHAPE(PaneActionRow, 1, ZEN_FIELD(id), ZEN_FIELD(label), ZEN_FIELD(scancode),
              ZEN_FIELD(modifiers));
};

struct PaneActions {
    std::string pane;
    std::vector<PaneActionRow> rows;
    ZEN_SHAPE(PaneActions, 1, ZEN_FIELD(pane), ZEN_FIELD(rows));
};

struct PaneActionRequested {
    std::string pane;
    std::string id;
    ZEN_SHAPE(PaneActionRequested, 1, ZEN_FIELD(pane), ZEN_FIELD(id));
};

} // namespace legacy_protocol

#endif // ZENGINE_TESTS_WEAVELIB_LEGACY_PANE_PROTOCOL_HPP
