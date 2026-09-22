// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_WORKSHOP_ADMISSION_HPP
#define ZENGINE_WORKSHOP_ADMISSION_HPP
#include "inventory/grant.hpp"
#include <zen/kernel/admission.hpp>

namespace zengine::workshop {
// Hosting policy, independent of inventory's implementation or artifact filename.
// Keep the inventory office's bounded speech when its holder becomes replaceable.
inline loom::AdmissionPolicy artifact_admission() {
    auto existing = loom::trust_every_artifact(
        "Workshop admits its authored plan artifacts; general per-row authority awaits "
        "a maker-visible policy seat");
    return [existing](const loom::AdmissionRequest& request) {
        if (request.stage == loom::AdmissionStage::Speak &&
            request.role == inventory::kInventoryRole) {
            return loom::AdmissionVerdict::admit(inventory::inventory_grant(),
                "Workshop's inventory office may capture descriptions and answer its protocol");
        }
        return existing(request);
    };
}
} // namespace zengine::workshop
#endif
