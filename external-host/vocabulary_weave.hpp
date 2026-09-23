// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_EXTERNAL_HOST_VOCABULARY_WEAVE_HPP
#define ZENGINE_EXTERNAL_HOST_VOCABULARY_WEAVE_HPP

// WORKSHOP'S GUEST VOCABULARY, DECLARED ON ANOTHER LOOM HOST.
//
// An external host that asks a Workshop anything through a link can only use shapes IT knows: a
// far answer is re-admitted through the external host's own gate, against the shape its own
// registry resolves, and a request a Python tool writes as Zen's JSON envelope is encoded by the
// external host's link against the same (Loom `docs/reference/bridge.md`). A compiled asker like
// the workshop probe declares what it accepts and so makes those shapes resolvable by existing.
// A Python tool is no participant of the external host's bus, so something else must: this.
//
// WHAT IT IS: one loadable weave, booted by the external host (`zengine-guest-vocabulary`), that
// DECLARES the guest-facing vocabulary -- the Input session, injection and closing shapes, the
// Skin's picture and chunk shapes, and the guest door's inventory -- through Loom's one
// agreement wall (`Emit<...>`: since Loom's ABI v9 a declared shape is claimed by definition and
// resolves while its declarer lives). It is compiled from the same published headers Workshop
// is, so a definition that drifted from this Workshop's is refused where the two meet: at the
// external host's wall against another declarer, or at the far gate by content id.
//
// WHAT IT IS NOT: a participant that does anything. It accepts nothing but the substrate doors,
// sends nothing, holds no state and needs no rule: declaring a shape is never authority to send
// it (the external host's operator still grants whatever actually speaks). Unload it and the
// shapes stop resolving, and a tool that needs them is refused by the link in words.

#include "input/vocabulary.hpp"
#include "inventory/vocabulary.hpp"
#include "inventory-pane/vocabulary.hpp"
#include "surface/vocabulary.hpp"
#include "workshop/guest_seam_vocabulary.hpp"
#include "workshop/pane_view.hpp"
#include "workshop/setup_control.hpp"
#include "demo-control/vocabulary.hpp"

#include <zen/weave.hpp>

#include <cstdint>

namespace zengine::external_host {

struct GuestVocabularyState {
    std::int64_t declared = 43; ///< top-level emitted shapes, excluding nested and substrate shapes
    ZEN_SHAPE(GuestVocabularyState, 1, ZEN_FIELD(declared));
};

class GuestVocabulary final
    : public loom::WeaveBase<
          GuestVocabulary, GuestVocabularyState, loom::Accept<>,
          loom::Emit<zengine::inventory_pane::InventoryViewEdit, zengine::inventory_pane::InventoryViewsRequested,
                     zengine::inventory_pane::InventoryViews, zengine::workshop::SetupApplyRequested, zengine::workshop::PaneResetRequested,
                     zengine::demo::DemoServiceOpened, zengine::demo::DemoServiceClosed,
                     zengine::demo::DemoWorkRequested, zengine::demo::DemoWork, zengine::demo::DemoWorkFinished,
                     zengine::demo::DemoResetRequested, zengine::demo::DemoStatusRequested, zengine::demo::DemoStatus,
                     zengine::demo::DemoReadyRequested,
                     zengine::workshop::PaneViewRequested, zengine::workshop::PaneView, zengine::input::InputSessionRequested, zengine::input::InputSessionOpened,
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
                     zengine::inventory::InventoryRemove, zengine::inventory::InventoryCaptureAdd>> {};

} // namespace zengine::external_host

#endif // ZENGINE_EXTERNAL_HOST_VOCABULARY_WEAVE_HPP
