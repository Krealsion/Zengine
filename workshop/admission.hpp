// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_WORKSHOP_ADMISSION_HPP
#define ZENGINE_WORKSHOP_ADMISSION_HPP
#include "desktop_seam_vocabulary.hpp"
#include "editor_switch_vocabulary.hpp"
#include "first-run/setup.hpp"
#include "panes.hpp"
#include "pane_vocabulary.hpp"
#include "pane_operation.hpp"
#include "settings_seam_vocabulary.hpp"

#include "graphics/vocabulary.hpp"
#include "inventory/grant.hpp"
#include "launcher/vocabulary.hpp"
#include "manual/vocabulary.hpp"
#include "view/vocabulary.hpp"

#include <cstdint>
#include <string>
#include <utility>
#include <zen/kernel/admission.hpp>

namespace zengine::workshop {
// The inventory office's grant here: its bounded speech, and what a shape it accepts does, from
// its own page, to the one who asked.
inline loom::Grant inventory_office_grant() {
    loom::Grant g = inventory::inventory_grant();
    g.allow_to_any(manual::ShapeDocumentShown::zen_name, manual::ShapeDocumentShown::zen_version);
    return g;
}

// THE QUICK SETUP'S SPEECH, and nothing more: its two panes, its two settings, a launch of a pane
// to Workshop -- either version -- and its close to the layouts' owner; the room asked of the
// graphics directory and its drawing to the two windows; the editor switch's four asks, to the
// switch's office alone; Workshop asked to approve a start for the hand, and the launcher asked
// for its listing and a start; and every running view asked what its wiring has done, said to any
// since a view's office is its own name.
// WL-FIRST-03, WL-FIRST-04 -- agents/workshop/first-run.md
inline loom::Grant first_run_office_grant() {
    loom::Grant g;
    const auto to = [&g](std::string name, std::uint32_t version, const std::string& office) {
        g.allow_to_role(std::move(name), version, office);
    };
    const std::string workshop = kWorkshopProvider;
    to(v2::PaneOffered::zen_name, v2::PaneOffered::zen_version, workshop);
    to(v3::PaneActions::zen_name, v3::PaneActions::zen_version, workshop);
    to(v4::PaneActions::zen_name, v4::PaneActions::zen_version, workshop);
    const std::string layouts = kLayoutsRole;
    to(OfficeSettingsDeclared::zen_name, OfficeSettingsDeclared::zen_version, layouts);
    to(OfficeSettingsRequested::zen_name, OfficeSettingsRequested::zen_version, layouts);
    to(OfficeSettingCommitRequested::zen_name, OfficeSettingCommitRequested::zen_version, layouts);
    to(PaneLaunchRequested::zen_name, PaneLaunchRequested::zen_version, workshop);
    to(v2::PaneLaunchRequested::zen_name, v2::PaneLaunchRequested::zen_version, workshop);
    to(v2::PaneOperationRequested::zen_name, v2::PaneOperationRequested::zen_version, workshop);
    to(PaneCloseRequested::zen_name, PaneCloseRequested::zen_version, layouts);
    to(PaneInventoryRequested::zen_name, PaneInventoryRequested::zen_version, workshop);
    to(graphics::RoomAsked::zen_name, graphics::RoomAsked::zen_version, graphics::kGraphicsRole);
    for (const char* window : {graphics::kSdlWindow, graphics::kTerminalWindow}) {
        to(graphics::Drawn::zen_name, graphics::Drawn::zen_version,
           std::string(graphics::kWindowOfficePrefix) + window);
    }
    const std::string switcher = kEditorSwitchRole;
    to(EditorSwitchRequested::zen_name, EditorSwitchRequested::zen_version, switcher);
    to(EditorSwitchConfirmed::zen_name, EditorSwitchConfirmed::zen_version, switcher);
    to(EditorSwitchCancelled::zen_name, EditorSwitchCancelled::zen_version, switcher);
    to(EditorSwitchStatusRequested::zen_name, EditorSwitchStatusRequested::zen_version, switcher);
    const std::string starts = launcher::kLauncherRole;
    to(launcher::LaunchCatalogRequested::zen_name, launcher::LaunchCatalogRequested::zen_version,
       starts);
    to(launcher::Launch::zen_name, launcher::Launch::zen_version, starts);
    g.allow_to_any(view::ViewWiringRequested::zen_name, view::ViewWiringRequested::zen_version);
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
        if (request.stage == loom::AdmissionStage::Speak &&
            request.role == first_run::kFirstRunRole) {
            return loom::AdmissionVerdict::admit(first_run_office_grant(),
                "the quick setup may keep its settings, show its panes, ask the editor switch and the launcher, "
                "and ask the running views what their wiring has done");
        }
        return existing(request);
    };
}
} // namespace zengine::workshop
#endif
