// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_FLOW_PANE_VOCABULARY_HPP
#define ZENGINE_FLOW_PANE_VOCABULARY_HPP
#include <zen/weave/shape.hpp>
#include <string>
#include <vector>
namespace zengine::flow_pane {
inline constexpr const char* kRole = "zengine.flow";
inline constexpr const char* kPane = "flow";
// The same semantic edits the pane's controls spend. Arguments are separate
// strings, not shell text. Describe names their grammar; targets stay local.
struct FlowEdit {
    std::string action;
    std::vector<std::string> arguments;
    ZEN_SHAPE(FlowEdit, 1, ZEN_FIELD(action), ZEN_FIELD(arguments));
};
struct FlowEdited {
    bool ok = false;
    std::string reason;
    loom::Bytes workspace;
    std::vector<std::string> problems;
    ZEN_SHAPE(FlowEdited, 1, ZEN_FIELD(ok), ZEN_FIELD(reason), ZEN_FIELD(workspace), ZEN_FIELD(problems));
};
struct FlowDialogEntry {
    std::string label, text;
    std::int64_t caret = 0, anchor = 0;
    ZEN_SHAPE(FlowDialogEntry, 1, ZEN_FIELD(label), ZEN_FIELD(text), ZEN_FIELD(caret), ZEN_FIELD(anchor));
};
struct FlowPaneState {
    loom::Bytes workspace;
    std::string path;
    bool dirty = false, state_edited = false;
    std::int64_t page = 0, dialog_selected = 0;
    std::string dialog_title, dialog_action;
    std::vector<FlowDialogEntry> dialog_entries;
    ZEN_SHAPE(FlowPaneState, 1, ZEN_FIELD(workspace), ZEN_FIELD(path), ZEN_FIELD(dirty),
              ZEN_FIELD(state_edited), ZEN_FIELD(page), ZEN_FIELD(dialog_selected),
              ZEN_FIELD(dialog_title), ZEN_FIELD(dialog_action), ZEN_FIELD(dialog_entries));
};
} // namespace zengine::flow_pane
#endif
