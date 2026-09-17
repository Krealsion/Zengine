// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_NEOVIM_EDITOR_VOCABULARY_HPP
#define ZENGINE_NEOVIM_EDITOR_VOCABULARY_HPP

// The Neovim-backed Editor's DURABLE NAMES -- the office and pane it holds, the actions its keys
// answer to, what a probe reads of it, and the three asks a maker sends it directly.
//
// ITS OFFICE AND ITS PANE ARE THE EDITOR'S, ON PURPOSE. This weave is another implementation of
// the one Editor a project has, authored beside the standard one as a load plan choice for
// `zengine.editor` (`workshop/load_plan.hpp`'s `ChoiceIntent`). Holding the same office is what
// lets Files, the Builder and Edit Code reach whichever editor holds it without knowing there is a
// choice; offering the same pane key is what keeps the maker's seat, its place and its keys when a
// switch moves the office. `kEditorOffice` and `kEditorPane` are spelled here rather than borrowed
// from `editor-pane/vocabulary.hpp`, so neither implementation depends on the other, and a case
// checks the two spellings agree.
//
// IT RUNS NEOVIM TWO WAYS, and a maker chooses by where they are:
//
//   IN WORKSHOP'S PANE   Neovim is started with this weave attached as its interface, and its
//                        screen is said to the pane in the protocol's rows, caret and one range
//                        (`neovim/projection.hpp`); keys, text, presses, drags and the wheel go
//                        back as Neovim input. Started when the pane is first given room, when an
//                        open asks for a file, or when a switch warms it.
//   FROM A BASELINE LOOM `NeovimStartRequested` starts Neovim headless and listening, and the maker
//                        attaches Neovim's own interface from a second terminal
//                        (`nvim --server <address> --remote-ui`); `loom-host` keeps its console.
//
// EVERY ANSWER TO THE THREE ASKS IS `zen.Result` OR `zen.Refused`, so any console reads them.

#include <zen/weave/shape.hpp>

#include <cstdint>
#include <string>

namespace zengine::neovim_editor {

/// THE EDITOR'S OFFICE AND PANE KEY -- the standard Editor's own spellings (see above).
inline constexpr const char* kEditorOffice = "zengine.editor";
inline constexpr const char* kEditorPane = "editor";

/// WHAT THE PICKER AND THE PANE HEADER SAY while this implementation holds the office.
inline constexpr const char* kPaneName = "Neovim";
inline constexpr const char* kPaneSummary = "edit a source file in Neovim";

/// THE LIBRARY STEM a load plan names for this choice.
inline constexpr const char* kNeovimEditorStem = "zengine-neovim-editor";

// ---- THE ACTIONS THE PANE DECLARES ---------------------------------------------------------

/// Write the current buffer (`:write`), on the save chord -- standing in for Workshop's own
/// document save while this pane holds the keys, exactly as the standard Editor's save does.
inline constexpr const char* kActionWrite = "neovim.write";
/// Neovim's own `<C-o>` (jump back; one Normal command from Insert), on the chord Workshop's
/// `document.open` uses elsewhere -- standing in for it while this pane holds the keys.
inline constexpr const char* kActionJumpOlder = "neovim.jump-older";

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
