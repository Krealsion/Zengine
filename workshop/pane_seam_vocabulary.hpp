// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_PANE_SEAM_VOCABULARY_HPP
#define ZENGINE_WORKSHOP_PANE_SEAM_VOCABULARY_HPP

// The names a pane and Workshop both spell that no shape carries: the places file's suggested name,
// and the roles and panes Workshop's judge of a guest's hand names (actor_scope.hpp), each a literal
// a case checks against the weave that declares it. What a pane asks of Workshop's doors is the
// opening's (open_seam_vocabulary.hpp) and the project door's (project_seam_vocabulary.hpp).

#include <zen/weave/shape.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace zengine::workshop {

/// The places file's suggested name, spelled here because both sides say it. The host resolves
/// where it lives and answers it through `ProjectRoot`; the pane owns the file.
inline constexpr const char* kDefaultMarksFileName = "workshop-marks.json";

/// The role that holds the one source document. A literal rather than
/// `editor/editor-pane/vocabulary.hpp`'s constant: neither asker links the weave, and a case checks the
/// two spellings agree.
inline constexpr const char* kEditorRole = "zengine.editor";

/// The role that holds the Terminal's line, and the desktop's pane that edits the keymap: literals
/// for the same reason, each checked by a case against the weave that declares it.
inline constexpr const char* kTerminalRole = "zengine.terminal";
inline constexpr const char* kHotkeysPane = "hotkeys";
inline constexpr const char* kManualPane = "manual";
/// The role that shows who is connected, a literal for the same reason: only the weaver's hand
/// reads or points there.
inline constexpr const char* kConnectionsRole = "zengine.connections";

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_PANE_SEAM_VOCABULARY_HPP
