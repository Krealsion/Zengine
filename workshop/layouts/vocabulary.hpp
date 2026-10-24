// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_LAYOUTS_VOCABULARY_HPP
#define ZENGINE_WORKSHOP_LAYOUTS_VOCABULARY_HPP

// The layouts' owner and its presenter: the live layout as the owner sends it to the one office
// that presents it, every change the presenter's gestures make asked of the owner, the session
// written at an orderly close, and the one question the owner asks the presenter -- whether a
// gesture is the weaver's current act.
// Workshop law: agents/workshop/layouts.md, agents/workshop/session-restore.md

#include "workshop/pane_settings.hpp"
#include "workshop/setup_persist.hpp"

#include <zen/weave/shape.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace zengine::workshop {

/// The office that holds every layout, the live one included, and every setting kept for them.
inline constexpr const char* kLayoutsRole = "zengine.layouts";

/// ONE LAYOUT AS A TAB: the id the owner minted for it, its name, and whether it is linked to a
/// setup file and how (`setup_link::kNone`, `kCurrent` or `kModified`).
struct LiveTab {
    std::int64_t id = 0;
    std::string name;
    std::int64_t link = 0;
    ZEN_SHAPE(LiveTab, 1, ZEN_FIELD(id), ZEN_FIELD(name), ZEN_FIELD(link));
    friend bool operator==(const LiveTab&, const LiveTab&) = default;
};

/// ONE LIVE ROW'S SETTINGS REVISION, as the owner minted it: the number a hand-off of the row's
/// settings is made at, and the number a commit of one of them is accepted at.
struct RowRevision {
    std::string provider;
    std::string pane;
    std::int64_t revision = 0;
    ZEN_SHAPE(RowRevision, 1, ZEN_FIELD(provider), ZEN_FIELD(pane), ZEN_FIELD(revision));
    friend bool operator==(const RowRevision&, const RowRevision&) = default;
};

/// THE LIVE LAYOUT AS ITS PRESENTER NEEDS IT, from the owner to the presenter's office: sent on
/// every change of the run or of the live layout, and before the answer to every edit. Whole
/// every time: the presenter replaces its copy with it and never edits that copy.
struct LiveLayout {
    std::int64_t revision = 0;   ///< moves on any change of the run or of the live layout
    std::int64_t put_live = 0;   ///< moves each time another desk is put live
    std::int64_t id = 0;         ///< the live layout's
    std::int64_t position = 0;   ///< ...and where it stands in the weaver's order
    setup_persist::WorkshopSetup desk;
    std::string link_path;       ///< the setup file the live layout is linked to, or empty
    std::vector<LiveTab> tabs; ///< every layout, the live one included, in the weaver's order
    std::string said;            ///< the owner's sentence about what moved, for the notice, or empty
    bool bad = false;            ///< ...and whether it is a refusal
    bool restored = false;       ///< `said` is the head of the last session's restore
    std::string session_refused; ///< the session file the owner would not read, why; held all run
    std::string settings_refused; ///< ...and the settings file
    std::vector<RowRevision> revisions; ///< each live row's settings revision
    ZEN_SHAPE(LiveLayout, 1, ZEN_FIELD(revision), ZEN_FIELD(put_live), ZEN_FIELD(id),
              ZEN_FIELD(position), ZEN_FIELD(desk), ZEN_FIELD(link_path), ZEN_FIELD(tabs),
              ZEN_FIELD(said), ZEN_FIELD(bad), ZEN_FIELD(restored), ZEN_FIELD(session_refused),
              ZEN_FIELD(settings_refused), ZEN_FIELD(revisions));
};

/// The words a `LayoutEdit` is said in: a row of the live layout seated or taken off, its window
/// written, its place in the front order moved; another layout put live, made, copied, removed,
/// moved or renamed; the live layout written to its setup file or read back from it.
namespace layout_op {
inline constexpr const char* kSeat = "seat";
inline constexpr const char* kUnseat = "unseat";
inline constexpr const char* kWindow = "window";
inline constexpr const char* kFront = "front";
inline constexpr const char* kBack = "back";
inline constexpr const char* kRaise = "raise";
inline constexpr const char* kLower = "lower";
inline constexpr const char* kResetFront = "reset-front";
inline constexpr const char* kSwitch = "switch";
inline constexpr const char* kStep = "step";
inline constexpr const char* kNew = "new";
inline constexpr const char* kDuplicate = "duplicate";
inline constexpr const char* kRemove = "remove";
inline constexpr const char* kMove = "move";
inline constexpr const char* kRename = "rename";
inline constexpr const char* kSave = "save";
inline constexpr const char* kRestore = "restore";
} // namespace layout_op

/// ONE CHANGE A PRESENTER'S GESTURE MAKES. `op` is one word of `layout_op`; the fields it reads
/// are its own: a row's ops read `provider` and `pane`, and `window` its `place`, `width` and
/// `height` whole; a layout's ops read `layout`, an id (0 the live one); `step` and `move` read
/// `by`, a direction and a place in the weaver's order; `rename` reads `name`.
struct LayoutEdit {
    std::string op;
    std::int64_t layout = 0;
    std::string provider;
    std::string pane;
    setup_persist::WorkshopPanePlace place;
    setup_persist::WorkshopPaneSize width;
    setup_persist::WorkshopPaneSize height;
    std::int64_t by = 0;
    std::string name;
    ZEN_SHAPE(LayoutEdit, 1, ZEN_FIELD(op), ZEN_FIELD(layout), ZEN_FIELD(provider),
              ZEN_FIELD(pane), ZEN_FIELD(place), ZEN_FIELD(width), ZEN_FIELD(height),
              ZEN_FIELD(by), ZEN_FIELD(name));
};

/// The most edits one request carries: a window and two order moves a row of the fullest desk.
inline constexpr std::size_t kMaxLayoutEdits = 3 * kMaxSetupPanes;

/// THE PRESENTER ASKS: these edits, judged from the `LiveLayout` at `revision` (0 for one judged
/// from no picture), written whole or not at all. None asks for the live layout as it stands, and
/// the presenter's first such ask is the start: the owner restores the last session first.
struct LayoutEditRequested {
    std::int64_t revision = 0;
    std::vector<LayoutEdit> edits;
    ZEN_SHAPE(LayoutEditRequested, 1, ZEN_FIELD(revision), ZEN_FIELD(edits));
};

/// WHAT THE EDITS CAME TO: stored, or refused in words with nothing written; the revision of the
/// `LiveLayout` sent before this answer; and, for an unseat, the settings and locks the row took.
struct LayoutEditAnswered {
    bool accepted = false;
    std::string refusal;
    std::int64_t revision = 0;
    std::vector<PaneSetting> discarded = {};
    std::vector<std::string> unlocked = {};
    ZEN_SHAPE(LayoutEditAnswered, 1, ZEN_FIELD(accepted), ZEN_FIELD(refusal), ZEN_FIELD(revision),
              ZEN_FIELD(discarded), ZEN_FIELD(unlocked));
};

/// THE PRESENTER'S ORDERLY CLOSE: write the session now. Asked once every pane holding unsaved
/// work has accepted the quit, and never on a quit that was refused.
struct SessionSaveRequested {
    ZEN_SHAPE(SessionSaveRequested, 1);
};

/// WHETHER IT WAS WRITTEN: `written`, or why not -- an isolated run, a session the owner would not
/// read, a write that failed.
struct SessionSaveAnswered {
    bool written = false;
    std::string refusal;
    ZEN_SHAPE(SessionSaveAnswered, 1, ZEN_FIELD(written), ZEN_FIELD(refusal));
};

/// THE OWNER ASKS THE PRESENTER, the judge of every hand: is `gesture` the weaver's current act,
/// sent to a pane of `office`?
struct WeaversActRequested {
    std::int64_t gesture = 0;
    std::string office;
    ZEN_SHAPE(WeaversActRequested, 1, ZEN_FIELD(gesture), ZEN_FIELD(office));
};

/// ...ANSWERED: whether it is.
struct WeaversActAnswered {
    bool current = false;
    ZEN_SHAPE(WeaversActAnswered, 1, ZEN_FIELD(current));
};

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_LAYOUTS_VOCABULARY_HPP
