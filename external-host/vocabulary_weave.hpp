// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_EXTERNAL_HOST_VOCABULARY_WEAVE_HPP
#define ZENGINE_EXTERNAL_HOST_VOCABULARY_WEAVE_HPP

// Workshop's guest vocabulary, declared on another Loom host. An external host's link re-admits a
// far answer, and encodes a Python tool's JSON request, against shapes its own registry resolves
// (the Loom's bridge reference page); a Python tool is no participant there, so this weave declares
// them: Input's session, the window's picture, the desk read whole, the pane inventory, the keymap,
// the door's inventory, the asker's row, the plan, the opening, the Builder's read, the settings.
// Reference: external-host/docs/external-host.md.

// Declared through Loom's agreement wall (`Emit<...>`: a declared shape resolves while its
// declarer lives), compiled from the same published headers as Workshop, so a drifted definition
// is refused where the two meet. It does nothing: accepts only the substrate doors, sends
// nothing, holds no state and grants nothing -- unloaded, its shapes stop resolving, said so.

#include "builder/vocabulary.hpp"
#include "input/vocabulary.hpp"
#include "inventory/vocabulary.hpp"
#include "inventory/inventory-pane/vocabulary.hpp"
#include "graphics/vocabulary.hpp"
#include "workshop/desktop_seam_vocabulary.hpp"
#include "workshop/guest_seam_vocabulary.hpp"
#include "workshop/open_seam_vocabulary.hpp"
#include "workshop/pane_seam_vocabulary.hpp"
#include "workshop/pane_view.hpp"
#include "workshop/guide_vocabulary.hpp"
#include "workshop/pane_document.hpp"
#include "workshop/setup_control.hpp"
#include "workshop/settings_seam_vocabulary.hpp"
#include "external-host/demo-control/vocabulary.hpp"

#include <zen/weave.hpp>

#include <cstdint>

namespace zengine::external_host {

struct GuestVocabularyState {
    std::int64_t declared = 99; ///< top-level emitted shapes, excluding nested and substrate shapes
    ZEN_SHAPE(GuestVocabularyState, 1, ZEN_FIELD(declared));
};

class GuestVocabulary final
    : public loom::WeaveBase<
          GuestVocabulary, GuestVocabularyState, loom::Accept<>,
          loom::Emit<zengine::inventory_pane::InventoryViewEdit, zengine::inventory_pane::InventoryViewsRequested,
                     zengine::inventory_pane::InventoryToolboxSave, zengine::inventory_pane::InventoryToolboxRestore,
                     zengine::inventory_pane::InventoryToolboxFinished,
                     zengine::inventory_pane::InventoryViews, zengine::workshop::SetupApplyRequested, zengine::workshop::PaneResetRequested, zengine::workshop::WorkshopQuitRequested,
                     zengine::demo::DemoServiceOpened, zengine::demo::DemoServiceClosed,
                     zengine::demo::DemoWorkRequested, zengine::demo::DemoWork, zengine::demo::DemoWorkFinished,
                     zengine::demo::DemoResetRequested, zengine::demo::DemoStatusRequested, zengine::demo::DemoStatus,
                     zengine::demo::DemoReadyRequested,
                     // The desk read whole, a pane's reading paged under one stamp and a point in
                     // it, and the notice that the desk moved, which a `capture` row may observe;
                     // a guide mark on a control and its answer, and a pane's manual, asked of the
                     // pane's own office.
                     zengine::workshop::DeskReadRequested, zengine::workshop::DeskRead,
                     zengine::workshop::PaneViewRequested, zengine::workshop::PaneView,
                     zengine::workshop::PanePointRequested, zengine::workshop::PanePoint,
                     zengine::workshop::DeskStamps,
                     zengine::workshop::GuideRequested, zengine::workshop::GuideAnswered,
                     zengine::workshop::PaneDocumentRequested, zengine::workshop::PaneDocumentShown,
                     zengine::workshop::PaneInventoryRequested, zengine::workshop::PaneInventory,
                     zengine::workshop::KeymapRequested, zengine::workshop::KeymapShown,
                     zengine::input::InputSessionRequested, zengine::input::InputSessionOpened,
                     zengine::input::PointerMotionRequested, zengine::input::InjectInput, zengine::input::InputInjected,
                     zengine::input::InputSessionClosed,
                     zengine::graphics::SurfaceCaptureRequested, zengine::graphics::SurfaceCaptured,
                     zengine::graphics::SurfaceCaptureChunkRequested,
                     zengine::graphics::SurfaceCaptureChunk,
                     zengine::workshop::GuestConnectionsRequested,
                     zengine::workshop::GuestConnections,
                     // The door's inventory with each row's powers, version and losses.
                     zengine::workshop::v2::GuestConnectionsRequested,
                     zengine::workshop::v2::GuestConnections,
                     // ...and at version 3, with each row's role and scopes.
                     zengine::workshop::v3::GuestConnectionsRequested,
                     zengine::workshop::v3::GuestConnections,
                     // The asker's own row, which any admitted session may ask for.
                     zengine::workshop::GuestRowDescribedRequested,
                     zengine::workshop::GuestRowDescribed,
                     zengine::workshop::v2::GuestRowDescribedRequested,
                     zengine::workshop::v2::GuestRowDescribed,
                     // Where Workshop's plan is, which any admitted session may ask.
                     zengine::workshop::GuestPlanRequested, zengine::workshop::GuestPlanSaid,
                     zengine::inventory::InventorySet, zengine::inventory::InventoryGet,
                     zengine::inventory::InventoryState, zengine::inventory::InventoryCaptured,
                     zengine::inventory::InventoryCaptureDescribe,
                     zengine::inventory::InventoryLocate, zengine::inventory::InventoryRead,
                     zengine::inventory::InventoryWrite, zengine::inventory::InventoryEntry,
                     zengine::inventory::InventoryAdd, zengine::inventory::InventoryList,
                     zengine::inventory::InventoryListed, zengine::inventory::InventoryRename,
                     zengine::inventory::InventoryRemove, zengine::inventory::InventoryCaptureAdd,
                     zengine::inventory::InventoryFile, zengine::inventory::InventoryFolderCreate,
                     zengine::inventory::InventoryFolderRename, zengine::inventory::InventoryFolderMove,
                     zengine::inventory::InventoryFolderRemove, zengine::inventory::InventoryFolderState,
                     zengine::inventory::v2::InventoryAdd, zengine::inventory::v2::InventoryList,
                     zengine::inventory::v2::InventoryListed,
                     // The managed opening a guest's `open` power reaches (workshop/guests.hpp).
                     zengine::workshop::OpenSourceRequested, zengine::workshop::SourceOpened,
                     // Where the Builder stands, answered to one observer (builder/vocabulary.hpp).
                     zengine::builder::BuildStatusRequested, zengine::builder::BuildStatus,
                     // One pane's settings on the live layout and one office's, a pane's pin list,
                     // a lock, and every layout's rows read and written in one batch, asked of the
                     // layouts' owner, which the `settings` power reaches.
                     zengine::workshop::PaneSettingsRequested, zengine::workshop::PaneSettingsAnswered,
                     zengine::workshop::PaneSettingCommitRequested,
                     zengine::workshop::PaneSettingCommitAnswered,
                     zengine::workshop::OfficeSettingsRequested,
                     zengine::workshop::OfficeSettingsAnswered,
                     zengine::workshop::OfficeSettingCommitRequested,
                     zengine::workshop::OfficeSettingCommitAnswered,
                     zengine::workshop::PaneSettingPinRequested,
                     zengine::workshop::PaneSettingPinAnswered,
                     zengine::workshop::PaneSettingLockRequested,
                     zengine::workshop::PaneSettingLockAnswered,
                     zengine::workshop::LayoutSettingsRequested,
                     zengine::workshop::LayoutSettingsAnswered,
                     zengine::workshop::SettingsWriteRequested,
                     zengine::workshop::SettingsWriteAnswered>> {};

} // namespace zengine::external_host

#endif // ZENGINE_EXTERNAL_HOST_VOCABULARY_WEAVE_HPP
