// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_NEOVIM_EDITOR_VOCABULARY_HPP
#define ZENGINE_NEOVIM_EDITOR_VOCABULARY_HPP

// The Neovim-backed Editor's durable names: the office and pane it holds, the actions its keys
// answer to, what a probe reads of it, and the three asks a maker sends it directly. Its office
// and pane are the Editor's (`zengine.editor`), so whatever reaches the Editor reaches whichever
// implementation a load plan chose, and a switch keeps the maker's seat; they are spelled here,
// not borrowed from the standard Editor, so neither depends on the other. From a Loom with no
// Workshop, `NeovimStartRequested` starts Neovim headless and listening, for a second terminal to
// attach with `nvim --server <address> --remote-ui`; every answer to the three asks is
// `zen.Result` or `zen.Refused`.
// Reference: docs/workshop/neovim.md.

#include <zen/weave/shape.hpp>

#include <cstdint>
#include <string>

namespace zengine::neovim_editor {

/// THE EDITOR'S OFFICE AND PANE KEY -- the standard Editor's own spellings (see above).
inline constexpr const char* kEditorOffice = "zengine.editor";
inline constexpr const char* kEditorPane = "editor";

/// WHAT THE PANE MANAGER, INFO AND THE PANE HEADER SAY while this implementation holds the
/// office.
inline constexpr const char* kPaneName = "Neovim";
inline constexpr const char* kPaneSummary = "edit a source file in Neovim";

/// THE LIBRARY STEM a load plan names for this choice.
inline constexpr const char* kNeovimEditorStem = "zengine-neovim-editor";

// ---- THE ACTIONS THE PANE DECLARES ---------------------------------------------------------

/// Write the current buffer (`:write`), on the save chord -- naming Workshop's retired document
/// save as what it stands in for, exactly as the standard Editor's save does (a host from before
/// the retirement still declares that row; this one admits the name standing in for nothing).
inline constexpr const char* kActionWrite = "neovim.write";
/// Neovim's own `<C-o>` (jump back; one Normal command from Insert), on the chord Workshop's
/// retired `document.open` used -- named as what it stands in for, for the same reason.
inline constexpr const char* kActionJumpOlder = "neovim.jump-older";
/// Carry a copy of the Visual selection to a receiving pane by pick-and-place -- on `ctrl+r`,
/// declared only while Neovim is in Visual or Select mode, where Neovim gives it no meaning; in
/// every other mode `ctrl+r` is Neovim's own (redo, Insert's register).
inline constexpr const char* kActionExtract = "neovim.extract";
/// Carry this file's location (its path, and the cursor's line and byte) the same way. Declared
/// with NO default key: in Normal mode every plain ctrl+letter is Neovim's or the desktop's, so a
/// maker who wants one binds it in the keymap; the status row's drag and menu carry it meanwhile.
inline constexpr const char* kActionLocation = "neovim.location";

// ---- WHAT A PROBE READS (`zen.PokeRead`) ---------------------------------------------------

/// THE READ SURFACE, written at the end of every delivery from what this weave knows without
/// asking Neovim anything: whether it runs, which Neovim, the current buffer's facts as Neovim's
/// own notifications last said them, and the words for why it is not running. It is not a reload
/// shape in any useful sense -- a reload of this image is refused while Neovim runs, because the
/// process belongs to the incarnation that started it (`snapshot` in `pane.cpp`).
struct NeovimEditorState {
    bool running = false;      ///< a Neovim is started and has not ended
    bool ready = false;        ///< ...and answered Zengine's module install
    std::string version;       ///< "0.11.6", once it said so
    std::string profile;       ///< "clean", "user", or an init file
    std::string ui;            ///< "pane" (Workshop), "remote" (a second terminal), or empty
    std::string listen;        ///< the address a remote interface attaches to, in remote mode
    std::string path;          ///< the current buffer's file, normalized; empty = none
    bool modified = false;     ///< the current buffer is modified
    std::string mode;          ///< Neovim's mode, as `mode()` spells it
    std::int64_t tick = 0;     ///< the current buffer's `changedtick`
    std::int64_t doc_epoch = 0; ///< the generation the pane's rows carry
    std::string failure;       ///< why Neovim is not running, in words
    std::string notice;        ///< the pane's standing notice
    bool notice_bad = false;
    std::string project_dir;
    bool project_known = false;
    std::int64_t starts = 0;   ///< how many Neovims this incarnation has started
    std::int64_t screens = 0;  ///< how many times the pane was said
    ZEN_SHAPE(NeovimEditorState, 1, ZEN_FIELD(running), ZEN_FIELD(ready), ZEN_FIELD(version),
              ZEN_FIELD(profile), ZEN_FIELD(ui), ZEN_FIELD(listen), ZEN_FIELD(path),
              ZEN_FIELD(modified), ZEN_FIELD(mode), ZEN_FIELD(tick), ZEN_FIELD(doc_epoch),
              ZEN_FIELD(failure), ZEN_FIELD(notice), ZEN_FIELD(notice_bad),
              ZEN_FIELD(project_dir), ZEN_FIELD(project_known), ZEN_FIELD(starts),
              ZEN_FIELD(screens));
};

// ---- THE THREE ASKS ----------------------------------------------------------------------------

/// START NEOVIM FOR A SECOND TERMINAL'S INTERFACE: headless, listening at `listen` (empty: an
/// address this weave makes), and editing `path` when one is named. Answered with the address and
/// the exact line that attaches to it, or refused in words (already running, not available, a
/// configuration that stopped at a prompt, a file another Neovim holds).
struct NeovimStartRequested {
    std::string path;
    std::string listen;
    ZEN_SHAPE(NeovimStartRequested, 1, ZEN_FIELD(path), ZEN_FIELD(listen));
};

/// END THE NEOVIM THIS WEAVE RUNS. Refused while a buffer holds unsaved changes, naming them,
/// unless `discard` says to lose them.
struct NeovimStopRequested {
    bool discard = false;
    ZEN_SHAPE(NeovimStopRequested, 1, ZEN_FIELD(discard));
};

/// WHAT RUNS, AND WHAT IT HOLDS, in one sentence: which Neovim, how it is attached, the current
/// buffer and whether it is modified -- asked of Neovim at the moment of the question.
struct NeovimStatusRequested {
    ZEN_SHAPE(NeovimStatusRequested, 1);
};

} // namespace zengine::neovim_editor

#endif // ZENGINE_NEOVIM_EDITOR_VOCABULARY_HPP
