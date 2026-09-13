// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_TESTS_WEAVELIB_LEGACY_PANE_PROTOCOL_HPP
#define ZENGINE_TESTS_WEAVELIB_LEGACY_PANE_PROTOCOL_HPP

// THE PANE PROTOCOL EXACTLY AS `84b6bc0` PUBLISHED IT -- the shapes a provider built before
// ownership existed derives, written out here so an image can speak them WITHOUT including the
// current `workshop/pane_vocabulary.hpp`, and so a suite can compare them against what that
// header publishes today (`loom::same_identity`).
//
// WHY THE SPELLINGS ARE THE SAME AS THE CURRENT HEADER'S. Loom's identity across a `.so` seam
// is the content-id derived from the shape -- the (name, version) and the fields in order --
// and `ZEN_SHAPE` takes the name from the C++ type token, so a struct of the same name, version
// and fields in a different namespace IS the same published shape (Loom GATE-04). That is what
// lets `tests/weavelib/legacy_pane.cpp` register with a host built against the current header:
// the host's version one and this file's version one have one identity.
//
// ONLY THE SHAPES THE LEGACY IMAGE SPEAKS OR HEARS. Nothing here is a second copy of the
// protocol's law; the law is `agents/panes.md`, and a shape's meaning is the current header's.

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
