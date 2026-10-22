// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_WORKSHOP_COMMAND_VOCABULARY_HPP
#define ZENGINE_WORKSHOP_COMMAND_VOCABULARY_HPP
// A command run by name: what a hand chose from a list of what it can do, asked of Workshop, which
// performs it by the path the command's own key or press takes and judges it for that hand.
// Reference: workshop/docs/zengine.workshop.md, `CommandRunRequested`.
#include <zen/weave/shape.hpp>
#include <string>
namespace zengine::workshop {

/// THE ONE OFFICE WHOSE PANE MAY ASK A COMMAND RUN, and that pane: the palette.
inline constexpr const char* kPaletteRole = "zengine.palette";
inline constexpr const char* kPalettePane = "palette";

/// What a command is: an action by its row's id, a pane by its address, or putting the palette
/// down with nothing run.
inline constexpr const char* kCommandAction = "action";
inline constexpr const char* kCommandPane = "pane";
inline constexpr const char* kCommandClose = "close";

/// Palette -> Workshop, under the correlation of the palette's own row Workshop sent it: run the
/// command. An action names its declarer's office, its pane for a pane's row, and its id; a pane
/// its office and pane. Answered on the asking delivery.
struct CommandRunRequested {
    std::string kind;
    std::string office;
    std::string pane;
    std::string id;
    ZEN_SHAPE(CommandRunRequested, 1, ZEN_FIELD(kind), ZEN_FIELD(office), ZEN_FIELD(pane),
              ZEN_FIELD(id));
};

/// Workshop -> the asker: `ran` when Workshop performed the step the command's path names -- a
/// key's request sent, a pane shown -- not that the pane did what its row says; otherwise the
/// refusal, in words.
struct CommandRunAnswered {
    bool ran = false;
    std::string refusal;
    ZEN_SHAPE(CommandRunAnswered, 1, ZEN_FIELD(ran), ZEN_FIELD(refusal));
};

} // namespace zengine::workshop
#endif
