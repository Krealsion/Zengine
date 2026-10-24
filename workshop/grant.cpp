// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#include "workshop/grant.hpp"
#include "workshop/guest_seam_vocabulary.hpp" // the guest door's office
#include "workshop/weave.hpp"

namespace zengine::workshop {
// Shared by the native host and its fixtures. Answers target the actual requester;
// role-scoped requests retain their explicit destination. No manifest-derived grants.
loom::Grant workshop_grant() {
    loom::Grant speak;
    speak.allow_to_any(graphics::ClipboardCopy::zen_name, graphics::ClipboardCopy::zen_version);
    // A paste and the place remembered, to the window that holds the platform.
    for (const std::string& medium :
         {std::string(graphics::kWindowOfficePrefix) + graphics::kTerminalWindow,
          std::string(graphics::kWindowOfficePrefix) + graphics::kSdlWindow}) {
        speak.allow_to_role(graphics::ClipboardTextRequested::zen_name,
                            graphics::ClipboardTextRequested::zen_version, medium);
        speak.allow_to_role(graphics::SurfacePlacementRemembered::zen_name,
                            graphics::SurfacePlacementRemembered::zen_version, medium);
    }
    // ...and the OS window shaped to the room a restore brings back, as the office arranging there.
    speak.allow_to_role(graphics::WindowShaped::zen_name, graphics::WindowShaped::zen_version,
                        std::string(graphics::kWindowOfficePrefix) + graphics::kSdlWindow);
    speak.allow_to_any(PaneCatalogRequested::zen_name, PaneCatalogRequested::zen_version);
    speak.allow_to_any(PaneSettings::zen_name, PaneSettings::zen_version);
    speak.allow_to_any(v2::PaneSettings::zen_name, v2::PaneSettings::zen_version);
    // ...and one pane's settings read and written by message, answered to whoever asked.
    speak.allow_to_any(PaneSettingsShown::zen_name, PaneSettingsShown::zen_version);
    speak.allow_to_any(PaneSettingActed::zen_name, PaneSettingActed::zen_version);
    // ...and an office's office-wide settings, handed to its holder and answered to whoever asked,
    // and a pin act's, a lock act's, every layout's reading's and a batch's answers.
    speak.allow_to_any(OfficeSettings::zen_name, OfficeSettings::zen_version);
    speak.allow_to_any(OfficeSettingsShown::zen_name, OfficeSettingsShown::zen_version);
    speak.allow_to_any(PaneSettingPinActed::zen_name, PaneSettingPinActed::zen_version);
    speak.allow_to_any(PaneSettingLockActed::zen_name, PaneSettingLockActed::zen_version);
    speak.allow_to_any(LayoutSettingsShown::zen_name, LayoutSettingsShown::zen_version);
    speak.allow_to_any(SettingsWritten::zen_name, SettingsWritten::zen_version);
    speak.allow_to_any(loom::Refused::zen_name, loom::Refused::zen_version);
    speak.allow_to_any(loom::Ack::zen_name, loom::Ack::zen_version);
    speak.allow_to_any(PaneView::zen_name, PaneView::zen_version);
    speak.allow_to_any(PanePoint::zen_name, PanePoint::zen_version);
    speak.allow_to_any(DeskRead::zen_name, DeskRead::zen_version);
    // ...and the notice that the desk moved, published to whoever follows it, a relay's listener
    // among them, which holds no office.
    speak.allow_to_any(DeskStamps::zen_name, DeskStamps::zen_version);
    // ...and what each pane's area shows, asked of the window that shows the desk.
    for (const char* window : {graphics::kTerminalWindow, graphics::kSdlWindow}) {
        speak.allow_to_role(graphics::AreaShowAsked::zen_name, graphics::AreaShowAsked::zen_version,
                            std::string(graphics::kWindowOfficePrefix) + window);
    }
    speak.allow_to_any(PaneObservationAnswered::zen_name, PaneObservationAnswered::zen_version);
    speak.allow_to_any(PaneOperationAnswered::zen_name, PaneOperationAnswered::zen_version);
    speak.allow_to_any(ActorScopeJudged::zen_name, ActorScopeJudged::zen_version);
    // ...and the same question, asked of the guest door, which judges it from the rows it admitted.
    speak.allow_to_role(ActorScopeRequested::zen_name, ActorScopeRequested::zen_version, kGuestsRole);
    speak.allow_to_any(PaneCarryAnswered::zen_name, PaneCarryAnswered::zen_version);
    speak.allow_to_any(PaneCanvasValueDrop::zen_name, PaneCanvasValueDrop::zen_version);
    speak.allow_to_any(PaneCanvasDrop::zen_name, PaneCanvasDrop::zen_version);
    speak.allow_to_any(PaneCanvasHover::zen_name, PaneCanvasHover::zen_version);
    speak.allow_to_any(PaneKey::zen_name, PaneKey::zen_version);
    // ...and whether the keys reach a pane a graphics window shows in an area of its own.
    speak.allow_to_any(PaneKeys::zen_name, PaneKeys::zen_version);
    speak.allow_to_any(PaneTextInput::zen_name, PaneTextInput::zen_version);
    speak.allow_to_any(PaneActionRequested::zen_name, PaneActionRequested::zen_version);
    speak.allow_to_any(PaneMenuAnswered::zen_name, PaneMenuAnswered::zen_version);
    speak.allow_to_role(MenuGranted::zen_name, MenuGranted::zen_version, kPresenterRole);
    speak.allow_to_role(MenuInput::zen_name, MenuInput::zen_version, kPresenterRole);
    speak.allow_to_role(MenuWithdrawn::zen_name, MenuWithdrawn::zen_version, kPresenterRole);
    speak.allow_to_role(WithdrawalFence::zen_name, WithdrawalFence::zen_version, kWorkshopProvider);
    speak.allow_to_any(AppActionRequested::zen_name, AppActionRequested::zen_version);
    speak.allow_to_any(ActionsJudged::zen_name, ActionsJudged::zen_version);
    speak.allow_to_any(ActionsWithdrawn::zen_name, ActionsWithdrawn::zen_version);
    speak.allow_to_any(PaneLaunchAnswered::zen_name, PaneLaunchAnswered::zen_version);
    speak.allow_to_any(PaneCloseAnswered::zen_name, PaneCloseAnswered::zen_version);
    speak.allow_to_any(PaneToggleAnswered::zen_name, PaneToggleAnswered::zen_version);
    speak.allow_to_any(KeymapEditAnswered::zen_name, KeymapEditAnswered::zen_version);
    speak.allow_to_any(PaneInventory::zen_name, PaneInventory::zen_version);
    speak.allow_to_any(KeymapShown::zen_name, KeymapShown::zen_version);
    speak.allow_to_any(CommandRunAnswered::zen_name, CommandRunAnswered::zen_version);
    speak.allow_to_any(PaneQuitRequested::zen_name, PaneQuitRequested::zen_version);
    speak.allow_to_any(PaneRevealAnswered::zen_name, PaneRevealAnswered::zen_version);
    speak.allow_to_any(v3::PresentationTrial::zen_name, v3::PresentationTrial::zen_version);
    speak.allow_to_any(PresentationAdmitted::zen_name, PresentationAdmitted::zen_version);
    speak.allow_to_role(OpenSourceRequested::zen_name, OpenSourceRequested::zen_version, kOpeningRole);
    speak.allow_to_any(PaneSourceOpened::zen_name, PaneSourceOpened::zen_version);
    speak.allow_to_any(StandingConditions::zen_name, StandingConditions::zen_version);
    speak.allow_to_any(PaneSubjectShown::zen_name, PaneSubjectShown::zen_version);
    speak.allow_to_any(PaneSubjectActed::zen_name, PaneSubjectActed::zen_version);
    speak.allow_to_any(TranscriptShown::zen_name, TranscriptShown::zen_version);
    speak.allow_to_any(TerminalActed::zen_name, TerminalActed::zen_version);
    speak.allow_to_any(TerminalValueAnswered::zen_name, TerminalValueAnswered::zen_version);
    speak.allow_to_any(TerminalCompletionOffered::zen_name, TerminalCompletionOffered::zen_version);
    // A GUIDE MARK'S ANSWER, to the guest that asked for it, at the ask and when the mark ends.
    speak.allow_to_any(GuideAnswered::zen_name, GuideAnswered::zen_version);
    // WHAT A SHAPE THIS OFFICE ACCEPTS DOES, from its manual, to whoever asked.
    speak.allow_to_any(manual::ShapeDocumentShown::zen_name, manual::ShapeDocumentShown::zen_version);
    // THE EDITOR'S DOCUMENT, read to refuse a guest the words of the guests file.
    speak.allow_observe(EditorDocument::zen_name, EditorDocument::zen_version);
    // THE DESK IN A GRAPHICS WINDOW: where each area stands and which skin paints it, said to the
    // directory, and Workshop's own drawing, to the window that shows its area.
    speak.allow_to_role(graphics::AreaPlaced::zen_name, graphics::AreaPlaced::zen_version, graphics::kGraphicsRole);
    speak.allow_to_role(graphics::SkinOffered::zen_name, graphics::SkinOffered::zen_version, graphics::kGraphicsRole);
    speak.allow_to_role(graphics::SkinChosen::zen_name, graphics::SkinChosen::zen_version, graphics::kGraphicsRole);
    speak.allow_to_role(graphics::RoomAsked::zen_name, graphics::RoomAsked::zen_version, graphics::kGraphicsRole);
    speak.allow_to_any(graphics::Drawn::zen_name, graphics::Drawn::zen_version);
    return speak;
}
}
