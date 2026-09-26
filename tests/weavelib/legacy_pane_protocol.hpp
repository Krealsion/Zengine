// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_TESTS_WEAVELIB_LEGACY_PANE_PROTOCOL_HPP
#define ZENGINE_TESTS_WEAVELIB_LEGACY_PANE_PROTOCOL_HPP

// The pane protocol as `84b6bc0` published it: the shapes a provider built before ownership
// existed derives, so an image can speak them without the current header and a suite can compare
// them with it (`loom::same_identity`). The spellings match the current header's because a
// shape's identity is its name, version and fields, the name taken from the type token
// (Loom GATE-04), so this file's version one is the host's. Only the shapes the legacy image
// speaks or hears; the protocol's law is agents/panes.md.

#include "surface/vocabulary.hpp"

#include <zen/weave/shape.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace legacy_protocol {

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
    std::vector<zengine::surface::SurfaceTextRow> rows;
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
