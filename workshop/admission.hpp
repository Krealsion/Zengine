// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_WORKSHOP_ADMISSION_HPP
#define ZENGINE_WORKSHOP_ADMISSION_HPP
#include "inventory/grant.hpp"
#include "manual/vocabulary.hpp"
#include <zen/kernel/admission.hpp>

namespace zengine::workshop {
// The inventory office's grant here: its bounded speech, and what a shape it accepts does, from
// its own page, to the one who asked.
inline loom::Grant inventory_office_grant() {
    loom::Grant g = inventory::inventory_grant();
    g.allow_to_any(manual::ShapeDocumentShown::zen_name, manual::ShapeDocumentShown::zen_version);
    return g;
}

// Hosting policy, independent of inventory's implementation or artifact filename.
// Keep the inventory office's bounded speech when its holder becomes replaceable.
inline loom::AdmissionPolicy artifact_admission() {
    auto existing = loom::trust_every_artifact(
        "Workshop admits its authored plan artifacts; general per-row authority awaits "
        "a weaver-visible policy seat");
    return [existing](const loom::AdmissionRequest& request) {
        if (request.stage == loom::AdmissionStage::Speak &&
            request.role == inventory::kInventoryRole) {
            return loom::AdmissionVerdict::admit(inventory_office_grant(),
                "Workshop's inventory office may capture descriptions and answer its protocol");
        }
        return existing(request);
    };
}
} // namespace zengine::workshop
#endif
