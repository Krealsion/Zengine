// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_TERMINAL_SPEECH_HPP
#define ZENGINE_WORKSHOP_TERMINAL_SPEECH_HPP

// What the Terminal participant this host mounts knows and may say: one call, made by the host
// and by the case that reads its grant, so the two cannot differ.

#include "editor_switch.hpp"
#include "powers_door.hpp"
#include "weave.hpp"

#include "graphics/vocabulary.hpp"

#include <zen/switchboard/grant.hpp>
#include <zen/terminal/vocabulary.hpp>
#include <zen/weave.hpp>

namespace zengine::workshop {

/// THE TERMINAL'S WHOLE SPEECH: the discovery door's two questions, its ordinary verb; the editor
/// switch's four; a pane's settings; each known and granted to its own office alone, and Loom's
/// three answers accepted. `Drawn` is known, so the completer describes what drawing takes, and
/// granted to no one; its lists cannot be typed on a line, so a line naming it is never sent.
/// Being a terminal confers nothing else.
// WL-TERM-20 -- agents/workshop/terminal.md
inline void let_terminal_speak(loom::TerminalVocabulary& vocab, loom::Grant& grant) {
    let_terminal_find_powers(vocab, grant);
    let_terminal_switch_editors(vocab, grant);
    let_terminal_edit_settings(vocab, grant);
    vocab.knows(loom::schema_of<zengine::graphics::Drawn>())
        .accepts(loom::schema_of<loom::Ack>())
        .accepts(loom::schema_of<loom::Result>())
        .accepts(loom::schema_of<loom::Refused>());
}

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_TERMINAL_SPEECH_HPP
