// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_EXTERNAL_HOST_VOCABULARY_WEAVE_HPP
#define ZENGINE_EXTERNAL_HOST_VOCABULARY_WEAVE_HPP

// Workshop's guest vocabulary, declared on another Loom host. An external host's link re-admits a
// far answer, and encodes a Python tool's JSON request, against shapes its own registry resolves
// (the Loom's bridge reference page); a Python tool is no participant there, so this weave declares
// them: Input's session shapes, the Skin's picture, the desk read whole, the pane inventory, the
// keymap, the door's inventory and the asker's own row, the managed opening and the Builder's read.
// Reference: external-host/docs/external-host.md.

// Declared through Loom's agreement wall (`Emit<...>`: a declared shape resolves while its
// declarer lives), compiled from the same published headers as Workshop, so a drifted definition
// is refused where the two meet. It does nothing: accepts only the substrate doors, sends
// nothing, holds no state and grants nothing -- unloaded, its shapes stop resolving, said so.

#include "builder/vocabulary.hpp"
#include "input/vocabulary.hpp"
#include "inventory/vocabulary.hpp"
#include "inventory/inventory-pane/vocabulary.hpp"
#include "surface/vocabulary.hpp"
#include "workshop/desktop_seam_vocabulary.hpp"
#include "workshop/guest_seam_vocabulary.hpp"
#include "workshop/open_seam_vocabulary.hpp"
#include "workshop/pane_seam_vocabulary.hpp"
#include "workshop/pane_view.hpp"
#include "workshop/setup_control.hpp"
#include "external-host/demo-control/vocabulary.hpp"

#include <zen/weave.hpp>

#include <cstdint>

namespace zengine::external_host {

struct GuestVocabularyState {
    std::int64_t declared = 90; ///< top-level emitted shapes, excluding nested and substrate shapes
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
                     zengine::workshop::PaneViewRequested, zengine::workshop::PaneView, zengine::workshop::PanePointRequested, zengine::workshop::PanePoint,
                     zengine::workshop::DeskViewRequested, zengine::workshop::DeskView,
                     zengine::workshop::v2::PaneViewRequested, zengine::workshop::v2::PaneView,
                     zengine::workshop::v2::PanePointRequested, zengine::workshop::v2::PanePoint,
                     zengine::workshop::v3::PaneViewRequested, zengine::workshop::v3::PaneView,
                     zengine::workshop::v3::PanePointRequested,
                     zengine::workshop::v2::DeskViewRequested, zengine::workshop::v2::DeskView,
                     // The desk read whole, a pane's reading paged under one stamp, and what a
                     // `capture` guest reads beside them: the inventory of panes and the keymap.
                     zengine::workshop::DeskReadRequested, zengine::workshop::DeskRead,
                     zengine::workshop::v4::PaneViewRequested, zengine::workshop::v4::PaneView,
                     // ...the same with each stamp naming its picture by fingerprint, and the
                     // notice that the desk moved, which a `capture` row may observe.
                     zengine::workshop::v2::DeskReadRequested, zengine::workshop::v2::DeskRead,
                     zengine::workshop::v5::PaneViewRequested, zengine::workshop::v5::PaneView,
                     zengine::workshop::DeskStamps,
                     zengine::workshop::PaneInventoryRequested, zengine::workshop::PaneInventory,
                     zengine::workshop::KeymapRequested, zengine::workshop::KeymapShown,
                     zengine::input::InputSessionRequested, zengine::input::InputSessionOpened,
                     zengine::input::PointerMotionRequested, zengine::input::InjectInput, zengine::input::InputInjected,
                     zengine::input::InputSessionClosed,
                     zengine::surface::SurfaceCaptureRequested, zengine::surface::SurfaceCaptured,
                     zengine::surface::SurfaceCaptureChunkRequested,
                     zengine::surface::SurfaceCaptureChunk,
                     zengine::workshop::GuestConnectionsRequested,
                     zengine::workshop::GuestConnections,
                     // The door's inventory with each row's powers, version and losses.
                     zengine::workshop::v2::GuestConnectionsRequested,
                     zengine::workshop::v2::GuestConnections,
                     // The asker's own row, which any admitted session may ask for.
                     zengine::workshop::GuestRowDescribedRequested,
                     zengine::workshop::GuestRowDescribed,
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
                     zengine::builder::BuildStatusRequested, zengine::builder::BuildStatus>> {};

} // namespace zengine::external_host

#endif // ZENGINE_EXTERNAL_HOST_VOCABULARY_WEAVE_HPP
