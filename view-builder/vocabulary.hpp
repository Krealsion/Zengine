// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_VIEW_BUILDER_VOCABULARY_HPP
#define ZENGINE_VIEW_BUILDER_VOCABULARY_HPP

// What another participant may ask the View Builder, what it keeps across a reload of its own
// image, and the project file it keeps across a launch. The edits are the ones its controls spend;
// `describe` names their grammar.

#include <zen/weave/shape.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace zengine::view_builder {

inline constexpr const char* kRole = "zengine.view.builder";
inline constexpr const char* kPane = "view-builder";

struct ViewEdit {
    std::string action;
    std::vector<std::string> arguments;
    ZEN_SHAPE(ViewEdit, 1, ZEN_FIELD(action), ZEN_FIELD(arguments));
};

/// The answer to an edit: whether it was taken, the builder's notice, and the draft's bytes.
struct ViewEdited {
    bool ok = false;
    std::string reason;
    loom::Bytes description;
    ZEN_SHAPE(ViewEdited, 1, ZEN_FIELD(ok), ZEN_FIELD(reason), ZEN_FIELD(description));
};

/// The draft; the name in its File box, which a weaver may have typed and not saved, and the file
/// it was saved to or opened from; whether it is saved; whether the view host runs it for this
/// office, which a reload in place cannot ask again; and whether the project file is kept as a
/// launch found it, naming a view file the builder could not open. Never a dialog, a grant or a
/// gesture.
struct BuilderState {
    loom::Bytes description;
    std::string path, file;
    bool dirty = false, running = false, kept = false;
    ZEN_SHAPE(BuilderState, 3, ZEN_FIELD(description), ZEN_FIELD(path), ZEN_FIELD(file),
              ZEN_FIELD(dirty), ZEN_FIELD(running), ZEN_FIELD(kept));
};

/// THE VIEW BUILDER'S OWN PROJECT FILE, beside the views it saves: the view file it has open -- a
/// file inside the project named relative to it -- and whether the view host runs it for this
/// office, in Zen's JSON text as Workshop's project files are. A launch reads it and runs that
/// view again, its pane seated by the desk the weaver left.
inline constexpr const char* kRunFileName = "view-builder.json";
inline constexpr const char* kRunFormat = "zengine-view-builder";
inline constexpr std::int64_t kRunFormatVersion = 1;

struct ViewBuilderRun {
    std::string format;
    std::int64_t format_version = 0;
    std::string path;
    bool running = false;
    ZEN_SHAPE(ViewBuilderRun, 1, ZEN_FIELD(format), ZEN_FIELD(format_version), ZEN_FIELD(path),
              ZEN_FIELD(running));
};

} // namespace zengine::view_builder
#endif
