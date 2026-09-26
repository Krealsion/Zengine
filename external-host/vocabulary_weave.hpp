// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_EXTERNAL_HOST_VOCABULARY_WEAVE_HPP
#define ZENGINE_EXTERNAL_HOST_VOCABULARY_WEAVE_HPP

// Workshop's guest vocabulary, declared on another Loom host. An external host's link re-admits
// a far answer, and encodes a Python tool's JSON request, against shapes its own registry
// resolves (Loom `docs/reference/bridge.md`); a Python tool is no participant there, so this
// weave declares them: the Input session, injection and closing shapes, the Skin's picture and
// chunk shapes, the guest door's inventory, the managed opening's ask and answer, the Builder's.
// Reference: docs/workshop/external-host.md.

// Declared through Loom's agreement wall (`Emit<...>`: a declared shape resolves while its
// declarer lives), compiled from the same published headers as Workshop, so a drifted definition
// is refused where the two meet. It does nothing: accepts only the substrate doors, sends
// nothing, holds no state and grants nothing -- unloaded, its shapes stop resolving, said so.

#include "builder/vocabulary.hpp"
#include "input/vocabulary.hpp"
#include "inventory/vocabulary.hpp"
#include "inventory-pane/vocabulary.hpp"
#include "surface/vocabulary.hpp"
#include "workshop/guest_seam_vocabulary.hpp"
#include "workshop/open_seam_vocabulary.hpp"
#include "workshop/pane_seam_vocabulary.hpp"
#include "workshop/pane_view.hpp"
#include "workshop/setup_control.hpp"
#include "demo-control/vocabulary.hpp"

#include <zen/weave.hpp>

#include <cstdint>

namespace zengine::external_host {

struct GuestVocabularyState {
    std::int64_t declared = 59; ///< top-level emitted shapes, excluding nested and substrate shapes
    ZEN_SHAPE(GuestVocabularyState, 1, ZEN_FIELD(declared));
};

class GuestVocabulary final
    : public loom::WeaveBase<
          GuestVocabulary, GuestVocabularyState, loom::Accept<>,
          loom::Emit<zengine::inventory_pane::InventoryViewEdit, zengine::inventory_pane::InventoryViewsRequested,
                     zengine::inventory_pane::InventoryToolboxSave, zengine::inventory_pane::InventoryToolboxRestore,
                     zengine::inventory_pane::InventoryToolboxFinished,
                     zengine::inventory_pane::InventoryViews, zengine::workshop::SetupApplyRequested, zengine::workshop::PaneResetRequested,
                     zengine::demo::DemoServiceOpened, zengine::demo::DemoServiceClosed,
                     zengine::demo::DemoWorkRequested, zengine::demo::DemoWork, zengine::demo::DemoWorkFinished,
                     zengine::demo::DemoResetRequested, zengine::demo::DemoStatusRequested, zengine::demo::DemoStatus,
                     zengine::demo::DemoReadyRequested,
                     zengine::workshop::PaneViewRequested, zengine::workshop::PaneView, zengine::workshop::PanePointRequested, zengine::workshop::PanePoint, zengine::input::InputSessionRequested, zengine::input::InputSessionOpened,
                     zengine::input::PointerMotionRequested, zengine::input::InjectInput, zengine::input::InputInjected,
                     zengine::input::InputSessionClosed,
                     zengine::surface::SurfaceCaptureRequested, zengine::surface::SurfaceCaptured,
                     zengine::surface::SurfaceCaptureChunkRequested,
                     zengine::surface::SurfaceCaptureChunk,
                     zengine::workshop::GuestConnectionsRequested,
                     zengine::workshop::GuestConnections,
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
