// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_VIEW_CREATOR_PANE_HPP
#define ZENGINE_VIEW_CREATOR_PANE_HPP

// A pane the Pane Creator saved, read as a view: the shapes its file was written in, kept with the
// wire identity those bytes claim, the names a project and a saved desk used for it, and the one
// rule that turns its name into a view's. The reader is `view::read_description`; a saved desk is
// met by Workshop's `pane_migration::convert_retired_panes`. Law: agents/view.md.

#include <zen/weave/shape.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace zengine::view {

/// The provider namespace a saved desk names a Pane Creator pane under, beside the pane's name.
inline constexpr const char* kCreatorPaneProvider = "zengine.workshop.maker";

/// The file the Pane Creator saved its pane to, in the project directory.
inline constexpr const char* kCreatorPaneFileName = "workshop-pane.json";

/// What the Pane Creator's file says it is, and the one version it was written at.
inline constexpr const char* kCreatorPaneFormat = "zengine-workshop-pane";
inline constexpr std::int64_t kCreatorPaneFormatVersion = 1;

/// The file's places and sizes are sub-units, this many to a canvas pixel.
inline constexpr std::int64_t kCreatorSubsPerPixel = 4;

/// The one kind of region the file holds: a line of text.
inline constexpr const char* kCreatorRegionText = "text";

/// ONE REGION AS THE FILE WROTE IT: an id, a kind word, a place and a size in sub-units relative
/// to the pane's interior, and a line of text.
struct WorkshopPaneRegion {
    std::int64_t id = 0;
    std::string kind;
    std::int64_t x = 0;
    std::int64_t y = 0;
    std::int64_t width = 0;
    std::int64_t height = 0;
    std::string text;

    ZEN_SHAPE(WorkshopPaneRegion, 1, ZEN_FIELD(id), ZEN_FIELD(kind), ZEN_FIELD(x), ZEN_FIELD(y),
              ZEN_FIELD(width), ZEN_FIELD(height), ZEN_FIELD(text));
};

/// THE WHOLE FILE: what it is, its version, the pane's name, the next region id it would have
/// minted, and its regions in order.
struct WorkshopPaneDefinition {
    std::string format;
    std::int64_t format_version = 0;
    std::string name;
    std::int64_t next_id = 0;
    std::vector<WorkshopPaneRegion> regions;

    ZEN_SHAPE(WorkshopPaneDefinition, 1, ZEN_FIELD(format), ZEN_FIELD(format_version),
              ZEN_FIELD(name), ZEN_FIELD(next_id), ZEN_FIELD(regions));
};

/// A PANE CREATOR PANE'S NAME AS A VIEW'S NAME: each byte a view's name cannot hold, and a
/// leading or trailing `.`, becomes `_`. The file and a desk naming the pane are read through
/// this one rule, so the view the file becomes is the pane the desk names.
inline std::string view_name_of_creator_pane(const std::string& pane) {
    std::string out = pane;
    for (std::size_t i = 0; i < out.size(); ++i) {
        const auto byte = static_cast<unsigned char>(out[i]);
        const bool kept = (byte >= '0' && byte <= '9') || (byte >= 'A' && byte <= 'Z') ||
                          (byte >= 'a' && byte <= 'z') || byte == '_' || byte == '-' ||
                          (byte == '.' && i != 0 && i + 1 != out.size());
        if (!kept) {
            out[i] = '_';
        }
    }
    return out;
}

} // namespace zengine::view
#endif
