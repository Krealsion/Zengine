// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#include "workshop/grant.hpp"
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
    speak.allow_to_any(PanePressed::zen_name, PanePressed::zen_version);
    speak.allow_to_any(v2::PanePressed::zen_name, v2::PanePressed::zen_version);
    speak.allow_to_any(loom::Refused::zen_name, loom::Refused::zen_version);
    speak.allow_to_any(loom::Ack::zen_name, loom::Ack::zen_version);
    speak.allow_to_any(PaneView::zen_name, PaneView::zen_version);
    speak.allow_to_any(PanePoint::zen_name, PanePoint::zen_version);
    speak.allow_to_any(PaneObservationAnswered::zen_name, PaneObservationAnswered::zen_version);
    speak.allow_to_any(PaneOperationAnswered::zen_name, PaneOperationAnswered::zen_version);
    speak.allow_to_any(PaneCarryAnswered::zen_name, PaneCarryAnswered::zen_version);
    speak.allow_to_any(PaneDrop::zen_name, PaneDrop::zen_version);
    speak.allow_to_any(PaneValueDrop::zen_name, PaneValueDrop::zen_version);
    speak.allow_to_any(v2::PaneValueDrop::zen_name, v2::PaneValueDrop::zen_version);
    speak.allow_to_any(PaneCanvasRoom::zen_name, PaneCanvasRoom::zen_version);
    speak.allow_to_any(PaneCanvasPointer::zen_name, PaneCanvasPointer::zen_version);
    speak.allow_to_any(PaneCanvasRejected::zen_name, PaneCanvasRejected::zen_version);
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
    speak.allow_to_any(MakerPaneAnswered::zen_name, MakerPaneAnswered::zen_version);
    speak.allow_to_any(PaneInventory::zen_name, PaneInventory::zen_version);
    speak.allow_to_any(KeymapShown::zen_name, KeymapShown::zen_version);
    speak.allow_to_any(PaneDragged::zen_name, PaneDragged::zen_version);
    speak.allow_to_any(PaneQuitRequested::zen_name, PaneQuitRequested::zen_version);
    speak.allow_to_any(PaneRevealAnswered::zen_name, PaneRevealAnswered::zen_version);
    speak.allow_to_any(PresentationTrial::zen_name, PresentationTrial::zen_version);
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
    return speak;
}
}
