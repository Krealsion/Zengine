// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_INFO_PANE_VOCABULARY_HPP
#define ZENGINE_INFO_PANE_VOCABULARY_HPP

// The Info pane's durable names: the office it holds, the panes it offers, the action ids its
// keys answer to, and the state a same-shape reload keeps. It inspects panes -- the host's one
// inventory, and one pane as a subject -- with property edits written by the pane's owner
// (`workshop/inspection_seam_vocabulary.hpp`). The office is not the host's `zengine.workshop`,
// whose holder admission refuses as a pane's offerer (WL-CAT-03); the key stays `info`, so a
// desk saved as `zengine.workshop/info` converts at load by moving the office only.
// Workshop law: agents/workshop/info-body.md

// `info.up`, `info.down` and `info.edit` keep the ids makers' overrides name. The draft's
// commit and cancel are `info.commit` and `info.cancel`: `draft.commit` and `draft.cancel` are
// still the host's (the Pane Manager declares them), and `join_pane_rows` refuses a pane
// claiming them -- so an override of `draft.commit` moves the Pane Manager's and not this one.

#include <zen/weave/shape.hpp>

#include <cstdint>
#include <string>

namespace zengine::info_pane {

/// THE OFFICE THIS PANE HOLDS -- the durable half of its `PaneRef`, and the only address
/// anything reaches it by. A ROLE, so Workshop's `PaneRoom` finds whoever holds it rather
/// than the incarnation that offered first.
inline constexpr const char* kInfoPaneRole = "zengine.info";

/// THE PANE KEY, in this office's namespace. The built-in it replaces carried this same key
/// under `zengine.workshop`; the conversion moves the office and keeps the key.
inline constexpr const char* kInfoPane = "info";

/// THE TWO LINES A MAKER READS ABOUT THIS PANE -- its name in the Pane Manager's list, its
/// summary in Info -- and what Workshop's pane header says after the office. They are the
/// built-in's own two lines, unchanged.
inline constexpr const char* kInfoPaneName = "Info";
inline constexpr const char* kInfoPaneSummary = "the panes, and one pane's properties";

/// THE LIBRARY STEM A HOST BOOTS. Not part of the pane protocol and not durable the way the
/// office is -- it is a file name, here because the host's boot plan and the suite's loader
/// must agree on it.
inline constexpr const char* kInfoPaneStem = "zengine-info-pane";

// ---- THE ACTIONS THE PANE DECLARES (`PaneActions`, workshop/pane_vocabulary.hpp) ---------

inline constexpr const char* kActionUp = "info.up";     ///< the cursor of the list with the keys, up
inline constexpr const char* kActionDown = "info.down"; ///< ...and down
/// RETURN: on the pane list, inspect the pane under the cursor; on the properties, open a
/// draft on the cursor's row. One id, labelled for the list it answers in.
inline constexpr const char* kActionEdit = "info.edit";
/// TAB: move the keys between the pane list and the subject's properties.
inline constexpr const char* kActionSwitch = "info.switch";

/// THE DRAFT'S TWO, DECLARED ONLY WHILE ONE IS OPEN (WL-FILES-16's rule): a pane is ONE
/// keyboard context, so while a maker is typing, these two are the only rows this pane
/// declares and every other key reaches it as an ordinary `PaneKey` for the line to consume.
inline constexpr const char* kActionCommit = "info.commit";
inline constexpr const char* kActionCancel = "info.cancel";

// ---- Independent value views (`value_view.hpp`) --------------------------------------------
//
// The default pane `info` plus three fixed slots, `info.2`..`info.4`, each offered on first use
// and re-offered at every announce; offers have no withdrawal, so views are reused and the
// catalog never grows. Only `info` keeps the pane-property view. The value-view ids keep the
// `inventory.*` spellings a maker's keymap overrides name.

inline constexpr std::size_t kMaxInfoViews = 4;
inline constexpr std::size_t kMaxViewTitle = 24;
inline constexpr const char* kInfoViewSummary = "an independent typed value view";
inline std::string view_key(std::size_t slot) {
    return slot == 1 ? std::string(kInfoPane) : std::string(kInfoPane) + "." + std::to_string(slot);
}

inline constexpr const char* kActionInventory = "info.inventory"; ///< pane properties -> value
inline constexpr const char* kValueUp = "inventory.up";
inline constexpr const char* kValueDown = "inventory.down";
inline constexpr const char* kValueEdit = "inventory.edit";
inline constexpr const char* kActionSave = "inventory.save";
inline constexpr const char* kActionSaveCopy = "inventory.save-copy";
inline constexpr const char* kActionRefresh = "inventory.fresh";
inline constexpr const char* kActionWatch = "inventory.watch";
inline constexpr const char* kActionSample = "inventory.sample";
inline constexpr const char* kActionPreset = "inventory.preset";
inline constexpr const char* kActionUnset = "inventory.unset";
inline constexpr const char* kActionGrab = "inventory.grab";
inline constexpr const char* kActionDiscard = "inventory.discard";
inline constexpr const char* kActionPanes = "inventory.panes";
inline constexpr const char* kActionFieldAccept = "inventory.field.accept";
inline constexpr const char* kActionFieldCancel = "inventory.field.cancel";
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

/// The state a same-shape reload keeps: the maker's position, and nothing shown -- what this
/// pane shows is the host's reading (`agents/decisions/a-presentation-owns-no-facts.md`), and the
/// host records the subject. The list cursor is an identity, and a chosen pane that left the list
/// keeps its keys, so a reloaded image still knows the choice is lost; the property cursor is an
/// index over a stable list. The draft is work in flight a reload may drop. Version 2: a live
/// reload from version 1 is refused.
struct InfoPaneState {
    std::int64_t cursor = 0; ///< which property row the maker is on
    std::string list_office; ///< which pane the list cursor holds, by identity -- kept when it
    std::string list_pane;   ///< leaves the list; both empty until a row is first held
    bool on_panes = true;    ///< the keys are in the pane list (true) or the properties
    ZEN_SHAPE(InfoPaneState, 2, ZEN_FIELD(cursor), ZEN_FIELD(list_office), ZEN_FIELD(list_pane),
              ZEN_FIELD(on_panes));
};

} // namespace zengine::info_pane

#endif // ZENGINE_INFO_PANE_VOCABULARY_HPP
