// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_VIEW_BUILDER_VOCABULARY_HPP
#define ZENGINE_VIEW_BUILDER_VOCABULARY_HPP

// What another participant may ask the View Builder, and what it keeps across a reload of its own
// image. The edits are the ones its controls spend; `describe` names their grammar.

#include <zen/weave/shape.hpp>

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

/// The draft, its file and whether it is saved; and whether the view host runs it for this
/// office, which a reload in place cannot ask again. Never a dialog, a grant or a gesture.
struct BuilderState {
    loom::Bytes description;
    std::string path;
    bool dirty = false, running = false;
    ZEN_SHAPE(BuilderState, 1, ZEN_FIELD(description), ZEN_FIELD(path), ZEN_FIELD(dirty),
              ZEN_FIELD(running));
};

} // namespace zengine::view_builder
#endif
