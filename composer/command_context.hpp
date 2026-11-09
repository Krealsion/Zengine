// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_COMPOSER_COMMAND_CONTEXT_HPP
#define ZENGINE_COMPOSER_COMMAND_CONTEXT_HPP

// The context the Message Composer hands a command it composes: the role the message is for.
// Pane law: agents/panes.md

#include <zen/weave/shape.hpp>

#include <string>

namespace zengine::composer {

struct ComposerCommandContext {
    std::string target_role;
    ZEN_SHAPE(ComposerCommandContext, 1, ZEN_FIELD(target_role));
};

} // namespace zengine::composer

#endif // ZENGINE_COMPOSER_COMMAND_CONTEXT_HPP
