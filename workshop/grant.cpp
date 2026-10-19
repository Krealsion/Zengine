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
    speak.allow_to_any(surface::SurfaceCanvas::zen_name, surface::SurfaceCanvas::zen_version);
    speak.allow_to_any(surface::SurfaceText::zen_name, surface::SurfaceText::zen_version);
    speak.allow_to_any(surface::ClipboardCopy::zen_name, surface::ClipboardCopy::zen_version);
    speak.allow_to_role(surface::ClipboardTextRequested::zen_name, surface::ClipboardTextRequested::zen_version, surface::kSkinRole);
    speak.allow_to_role(surface::SurfacePlacementRemembered::zen_name, surface::SurfacePlacementRemembered::zen_version, surface::kSkinRole);
    speak.allow_to_any(PaneCatalogRequested::zen_name, PaneCatalogRequested::zen_version);
    speak.allow_to_any(PaneRoom::zen_name, PaneRoom::zen_version);
    speak.allow_to_any(PaneSettings::zen_name, PaneSettings::zen_version);
    speak.allow_to_any(v2::PaneSettings::zen_name, v2::PaneSettings::zen_version);
    // ...and one pane's settings read and written by message, answered to whoever asked.
    speak.allow_to_any(PaneSettingsShown::zen_name, PaneSettingsShown::zen_version);
    speak.allow_to_any(PaneSettingActed::zen_name, PaneSettingActed::zen_version);
    // ...and an office's office-wide settings, handed to its holder and answered to whoever asked,
    // and a pin act's answer.
    speak.allow_to_any(OfficeSettings::zen_name, OfficeSettings::zen_version);
    speak.allow_to_any(OfficeSettingsShown::zen_name, OfficeSettingsShown::zen_version);
    speak.allow_to_any(PaneSettingPinActed::zen_name, PaneSettingPinActed::zen_version);
    speak.allow_to_any(PanePressed::zen_name, PanePressed::zen_version);
    speak.allow_to_any(v2::PanePressed::zen_name, v2::PanePressed::zen_version);
    speak.allow_to_any(loom::Refused::zen_name, loom::Refused::zen_version);
    speak.allow_to_any(loom::Ack::zen_name, loom::Ack::zen_version);
    speak.allow_to_any(PaneView::zen_name, PaneView::zen_version);
    speak.allow_to_any(PanePoint::zen_name, PanePoint::zen_version);
    speak.allow_to_any(DeskView::zen_name, DeskView::zen_version);
    speak.allow_to_any(v2::PaneView::zen_name, v2::PaneView::zen_version);
    speak.allow_to_any(v2::PanePoint::zen_name, v2::PanePoint::zen_version);
    speak.allow_to_any(v3::PaneView::zen_name, v3::PaneView::zen_version);
    speak.allow_to_any(v2::DeskView::zen_name, v2::DeskView::zen_version);
    speak.allow_to_any(v4::PaneView::zen_name, v4::PaneView::zen_version);
    speak.allow_to_any(DeskRead::zen_name, DeskRead::zen_version);
    speak.allow_to_any(v5::PaneView::zen_name, v5::PaneView::zen_version);
    speak.allow_to_any(v2::DeskRead::zen_name, v2::DeskRead::zen_version);
    speak.allow_to_any(v6::PaneView::zen_name, v6::PaneView::zen_version);
    // ...and the notice that the desk moved, published to whoever follows it, a relay's listener
    // among them, which holds no office.
    speak.allow_to_any(DeskStamps::zen_name, DeskStamps::zen_version);
    speak.allow_to_any(v2::DeskStamps::zen_name, v2::DeskStamps::zen_version);
    speak.allow_to_any(PaneObservationAnswered::zen_name, PaneObservationAnswered::zen_version);
    speak.allow_to_any(PaneOperationAnswered::zen_name, PaneOperationAnswered::zen_version);
    speak.allow_to_any(ActorScopeJudged::zen_name, ActorScopeJudged::zen_version);
    // ...and the same question, asked of the guest door, which judges it from the rows it admitted.
    speak.allow_to_role(ActorScopeRequested::zen_name, ActorScopeRequested::zen_version, kGuestsRole);
    speak.allow_to_any(PaneCarryAnswered::zen_name, PaneCarryAnswered::zen_version);
    speak.allow_to_any(PaneDrop::zen_name, PaneDrop::zen_version);
    speak.allow_to_any(PaneValueDrop::zen_name, PaneValueDrop::zen_version);
    speak.allow_to_any(v2::PaneValueDrop::zen_name, v2::PaneValueDrop::zen_version);
    speak.allow_to_any(PaneCanvasValueDrop::zen_name, PaneCanvasValueDrop::zen_version);
    speak.allow_to_any(PaneCanvasDrop::zen_name, PaneCanvasDrop::zen_version);
    speak.allow_to_any(PaneCanvasRoom::zen_name, PaneCanvasRoom::zen_version);
    speak.allow_to_any(PaneCanvasPointer::zen_name, PaneCanvasPointer::zen_version);
    speak.allow_to_any(PaneCanvasHover::zen_name, PaneCanvasHover::zen_version);
    speak.allow_to_any(PaneCanvasRejected::zen_name, PaneCanvasRejected::zen_version);
    speak.allow_to_any(v1::PaneCanvasValueDrop::zen_name, v1::PaneCanvasValueDrop::zen_version);
    speak.allow_to_any(v2::PaneCanvasRoom::zen_name, v2::PaneCanvasRoom::zen_version);
    speak.allow_to_any(v1::PaneCanvasPointer::zen_name, v1::PaneCanvasPointer::zen_version);
    speak.allow_to_any(v1::PaneCanvasHover::zen_name, v1::PaneCanvasHover::zen_version);
    speak.allow_to_any(PaneKey::zen_name, PaneKey::zen_version);
    speak.allow_to_any(PaneTextInput::zen_name, PaneTextInput::zen_version);
    speak.allow_to_any(PaneWheel::zen_name, PaneWheel::zen_version);
    speak.allow_to_any(PaneActionRequested::zen_name, PaneActionRequested::zen_version);
    speak.allow_to_any(v3::PanePressed::zen_name, v3::PanePressed::zen_version);
    speak.allow_to_any(PaneButton::zen_name, PaneButton::zen_version);
    speak.allow_to_any(PaneMenuAnswered::zen_name, PaneMenuAnswered::zen_version);
    speak.allow_to_role(PictureFence::zen_name, PictureFence::zen_version, kWorkshopProvider);
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
    speak.allow_to_any(PaneDragged::zen_name, PaneDragged::zen_version);
    speak.allow_to_any(PaneQuitRequested::zen_name, PaneQuitRequested::zen_version);
    speak.allow_to_any(PaneRevealAnswered::zen_name, PaneRevealAnswered::zen_version);
    speak.allow_to_any(PresentationTrial::zen_name, PresentationTrial::zen_version);
    speak.allow_to_any(v2::PresentationTrial::zen_name, v2::PresentationTrial::zen_version);
    speak.allow_to_any(PresentationAdmitted::zen_name, PresentationAdmitted::zen_version);
    speak.allow_to_role(OpenSourceRequested::zen_name, OpenSourceRequested::zen_version, kOpeningRole);
    speak.allow_to_any(PaneSourceOpened::zen_name, PaneSourceOpened::zen_version);
    speak.allow_to_any(StandingConditions::zen_name, StandingConditions::zen_version);
    speak.allow_to_any(PaneSubjectShown::zen_name, PaneSubjectShown::zen_version);
    speak.allow_to_any(v2::PaneSubjectShown::zen_name, v2::PaneSubjectShown::zen_version);
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
