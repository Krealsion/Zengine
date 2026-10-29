// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_INFO_PANE_VOCABULARY_HPP
#define ZENGINE_INFO_PANE_VOCABULARY_HPP

// The Info pane's durable names: the office it holds, the panes it offers, the action ids its
// keys answer to, and the state a same-shape reload keeps. It inspects one pane as a subject,
// named by an office, with property edits written by the pane's owner
// (`workshop/inspection_seam_vocabulary.hpp`). The office is not the host's `zengine.workshop`,
// whose holder admission refuses as a pane's offerer (WL-CAT-03); the key stays `info`, so a
// desk saved as `zengine.workshop/info` converts at load by moving the office only.
// Workshop law: agents/workshop/info-body.md

// `info.up`, `info.down` and `info.edit` keep the ids weavers' overrides name. The draft's
// commit and cancel are `info.commit` and `info.cancel`: `draft.commit` and `draft.cancel` are
// still the host's (the Pane Manager declares them), and `join_pane_rows` refuses a pane
// claiming them -- so an override of `draft.commit` moves the Pane Manager's and not this one.

#include <zen/weave/shape.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace zengine::info_pane {

/// THE OFFICE THIS PANE HOLDS -- the durable half of its `PaneRef`, and the only address
/// anything reaches it by. A ROLE, so the window and Workshop find whoever holds it rather
/// than the incarnation that offered first.
inline constexpr const char* kInfoPaneRole = "zengine.info";

/// THE PANE KEY, in this office's namespace. The built-in it replaces carried this same key
/// under `zengine.workshop`; the conversion moves the office and keeps the key.
inline constexpr const char* kInfoPane = "info";

/// THE TWO LINES A WEAVER READS ABOUT THIS PANE -- its name in the Pane Manager's list, its
/// summary in Info -- and what Workshop's pane header says after the office.
inline constexpr const char* kInfoPaneName = "Info";
inline constexpr const char* kInfoPaneSummary = "one pane's properties";

/// THE LIBRARY STEM A HOST BOOTS. Not part of the pane protocol and not durable the way the
/// office is -- it is a file name, here because the host's boot plan and the suite's loader
/// must agree on it.
inline constexpr const char* kInfoPaneStem = "zengine-info-pane";

// ---- THE ACTIONS THE PANE DECLARES (`PaneActions`, workshop/pane_vocabulary.hpp) ---------

inline constexpr const char* kActionUp = "info.up";     ///< the property cursor, up
inline constexpr const char* kActionDown = "info.down"; ///< ...and down
/// RETURN: open a draft on the cursor's property row.
inline constexpr const char* kActionEdit = "info.edit";

/// THE DRAFT'S TWO, DECLARED ONLY WHILE ONE IS OPEN (WL-FILES-16's rule): a pane is ONE
/// keyboard context, so while a weaver is typing, these two are the only rows this pane
/// declares and every other key reaches it as an ordinary `PaneKey` for the line to consume.
inline constexpr const char* kActionCommit = "info.commit";
inline constexpr const char* kActionCancel = "info.cancel";

// ---- Independent value views (`value_view.hpp`) --------------------------------------------
//
// The default pane `info` plus three fixed slots, `info.2`..`info.4`, each offered on first use
// and re-offered at every announce; offers have no withdrawal, so views are reused and the
// catalog never grows. Only `info` keeps the pane-property view. The value views' ids are
// `info.value.*`, so none is the Inventory pane's.

inline constexpr std::size_t kMaxInfoViews = 4;
inline constexpr std::size_t kMaxViewTitle = 24;
inline constexpr const char* kInfoViewSummary = "an independent typed value view";
inline std::string view_key(std::size_t slot) {
    return slot == 1 ? std::string(kInfoPane) : std::string(kInfoPane) + "." + std::to_string(slot);
}

inline constexpr const char* kActionInventory = "info.inventory"; ///< pane properties -> value
inline constexpr const char* kValueUp = "info.value.up";
inline constexpr const char* kValueDown = "info.value.down";
inline constexpr const char* kValueEdit = "info.value.edit";
/// THE VALUE'S LEVELS, a tree: open the level under the cursor or step into it, shut it or step
/// out to the level above, and shut every level with the cursor on the first field.
inline constexpr const char* kValueOpen = "info.value.open";
inline constexpr const char* kValueShut = "info.value.shut";
inline constexpr const char* kValueShutAll = "info.value.shut-all";
inline constexpr const char* kActionSave = "info.value.save";
inline constexpr const char* kActionSaveCopy = "info.value.save-copy";
inline constexpr const char* kActionRefresh = "info.value.fresh";
inline constexpr const char* kActionWatch = "info.value.watch";
inline constexpr const char* kActionSample = "info.value.sample";
inline constexpr const char* kActionPreset = "info.value.preset";
inline constexpr const char* kActionUnset = "info.value.unset";
inline constexpr const char* kActionGrab = "info.value.grab";
inline constexpr const char* kActionDiscard = "info.value.discard";
inline constexpr const char* kActionPanes = "info.value.panes";
inline constexpr const char* kActionFieldAccept = "info.value.field.accept";
inline constexpr const char* kActionFieldCancel = "info.value.field.cancel";
inline constexpr const char* kActionViewNew = "info.view.new";
inline constexpr const char* kActionViewFork = "info.view.fork";
inline constexpr const char* kActionViewRename = "info.view.rename";
inline constexpr const char* kActionViewClose = "info.view.close";
inline constexpr const char* kActionViewMenu = "info.view.menu";
inline constexpr const char* kActionViewUse = "info.view.use";
/// STOP WAITING for a Refresh, Link or Sample: declared only while one is pending.
inline constexpr const char* kActionStop = "info.view.stop";
inline constexpr const char* kActionRenameAccept = "info.view.rename.accept";
inline constexpr const char* kActionRenameCancel = "info.view.rename.cancel";

/// ONE VALUE VIEW A RELOAD KEEPS, by its slot: its title; whether the default view shows its value;
/// its draft, a stored-draft Inventory pair with its metadata, and whether it is a preset, titled;
/// the entry it read or last saved, by reference and revision, and its pair; its label and marks;
/// the selected and edited fields, by their place among its fields; whether a watch was on; and
/// its open levels, by path (`history[1]`, `meta[0].source`). Never a request in flight, a lease,
/// a sample or a pickup.
struct InfoViewState {
    std::int64_t slot = 0;
    std::string title;
    bool active = false;
    loom::Bytes draft; ///< empty while the view holds no value
    bool preset = false;
    std::string preset_title;
    std::string owner, entry; ///< the entry's reference, both empty for none
    std::int64_t revision = 0;
    loom::Bytes saved;
    std::string label;
    bool detached = false, dirty = false, sampled = false, stale = false;
    std::int64_t selected = -1;
    std::vector<std::int64_t> edited;
    bool watched = false;
    std::vector<std::string> open; ///< the open levels, read and written here while the view lives
    ZEN_SHAPE(InfoViewState, 2, ZEN_FIELD(slot), ZEN_FIELD(title), ZEN_FIELD(active), ZEN_FIELD(draft),
              ZEN_FIELD(preset), ZEN_FIELD(preset_title), ZEN_FIELD(owner), ZEN_FIELD(entry),
              ZEN_FIELD(revision), ZEN_FIELD(saved), ZEN_FIELD(label), ZEN_FIELD(detached),
              ZEN_FIELD(dirty), ZEN_FIELD(sampled), ZEN_FIELD(stale), ZEN_FIELD(selected),
              ZEN_FIELD(edited), ZEN_FIELD(watched), ZEN_FIELD(open));
};

/// The state a same-shape reload keeps: the weaver's position and work, one `views` row an
/// allocated view (at most `kMaxInfoViews`), which while the weave runs holds its slot and its open
/// levels, every fold reading and writing them there, the rest written as a reload takes the state.
/// What the default pane shows is the host's reading (`agents/decisions/a-presentation-owns-no-facts.md`).
/// The property cursor is an index over a stable list; a pane-property draft is work in flight a
/// reload may drop. Version 5: a live reload from version 4 is refused.
struct InfoPaneState {
    std::int64_t cursor = 0; ///< which property row the weaver is on
    std::vector<InfoViewState> views;
    ZEN_SHAPE(InfoPaneState, 5, ZEN_FIELD(cursor), ZEN_FIELD(views));
};

} // namespace zengine::info_pane

#endif // ZENGINE_INFO_PANE_VOCABULARY_HPP
