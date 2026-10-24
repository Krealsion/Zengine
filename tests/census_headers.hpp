// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The shapes a header publishes that no weave library's manifest declares -- a file's format, a
// value carried inside another, a sentence a host speaks -- or that only a library some builds
// leave out declares, for the census of shapes (test_shapes_census.cpp). A shape the census finds
// in neither place is named there.

#ifndef ZENGINE_TESTS_CENSUS_HEADERS_HPP
#define ZENGINE_TESTS_CENSUS_HEADERS_HPP

#include "app/host.hpp"
#include "app/vocabulary.hpp"
#include "builder/runner.hpp"
#include "builder/vocabulary.hpp"
#include "builder/weave.hpp"
#include "composer/command_context.hpp"
#include "files/marks_persist.hpp"
#include "flow/flow-host/runtime.hpp"
#include "flow/graph_edit.hpp"
#include "flow/workspace.hpp"
#include "inventory/inventory-pane/toolbox_file.hpp"
#include "inventory/vocabulary.hpp"
#include "manual/vocabulary.hpp"
#include "examples/snake/board.hpp"
#include "examples/snake/play_state.hpp"
#include "graphics/elements.hpp"
#include "graphics/ops.hpp"
#include "graphics/painting.hpp"
#include "editor/source-transfer/vocabulary.hpp"
#include "settings/theme_persist.hpp"
#include "settings/vocabulary.hpp"
#include "graphics/vocabulary.hpp"
#include "timer/vocabulary.hpp"
#include "ui/vocabulary.hpp"
#include "view/view-builder/vocabulary.hpp"
#include "view/host.hpp"
#include "view/view.hpp"
#include "workshop/arrangement.hpp"
#include "workshop/desktop_seam_vocabulary.hpp"
#include "workshop/editor_switch.hpp"
#include "workshop/editor_switch_vocabulary.hpp"
#include "workshop/guest_door.hpp"
#include "workshop/guide_vocabulary.hpp"
#include "workshop/keymap_persist.hpp"
#include "workshop/load_execute.hpp"
#include "workshop/load_persist.hpp"
#include "workshop/open_seam_vocabulary.hpp"
#include "workshop/opening.hpp"
#include "workshop/pane_carry.hpp"
#include "workshop/pane_document.hpp"
#include "workshop/pane_doors.hpp"
#include "workshop/pane_view.hpp"
#include "workshop/pane_vocabulary.hpp"
#include "workshop/powers_door.hpp"
#include "workshop/prefs_persist.hpp"
#include "workshop/quit_delivery.hpp"
#include "workshop/recipe_persist.hpp"
#include "workshop/sample_door.hpp"
#include "workshop/session_history.hpp"
#include "workshop/session_persist.hpp"
#include "workshop/setup_persist.hpp"
#include "workshop/terminal_seam_vocabulary.hpp"
#include "workshop/vocabulary.hpp"

#include <zen/weave/shape.hpp>

#include <memory>
#include <vector>

template <class S>
std::shared_ptr<const loom::Schema> shape() {
    return loom::schema_of<S>();
}

inline std::vector<std::shared_ptr<const loom::Schema>> census_header_shapes() {
    return {
shape<::zengine::builder::RunnerState>(), // RunnerState v2
        shape<::zengine::app::AppCloseState>(), // AppCloseState v1
        shape<::zengine::app::AppPlaceDoorState>(), // AppPlaceDoorState v1
        shape<::zengine::app::AppSeatState>(), // AppSeatState v1
        shape<::zengine::app::AppSeatTurn>(), // AppSeatTurn v1
        shape<::zengine::app::ZengineAppFile>(), // ZengineAppFile v1
        shape<::zengine::app::ZengineAppFlow>(), // ZengineAppFlow v1
        shape<::zengine::app::ZengineAppView>(), // ZengineAppView v1
        shape<::zengine::app::ZengineAppWindow>(), // ZengineAppWindow v1
        shape<::zengine::builder::ArtifactPromoted>(), // ArtifactPromoted v2
        shape<::zengine::builder::ArtifactRealized>(), // ArtifactRealized v3
        shape<::zengine::builder::BuildAsked>(), // BuildAsked v1
        shape<::zengine::builder::BuildFinished>(), // BuildFinished v2
        shape<::zengine::builder::BuildNotStarted>(), // BuildNotStarted v2
        shape<::zengine::builder::BuildOutput>(), // BuildOutput v3
        shape<::zengine::builder::BuildStarted>(), // BuildStarted v2
        shape<::zengine::builder::LookAtBuilds>(), // LookAtBuilds v1
        shape<::zengine::builder::OfferArtifact>(), // OfferArtifact v1
        shape<::zengine::builder::RealizationAsked>(), // RealizationAsked v1
        shape<::zengine::builder::RunBuild>(), // RunBuild v2
        shape<::zengine::builder::BuilderState>(), // BuilderState v4
        shape<::zengine::workshop::marks_persist::WorkshopMark>(), // WorkshopMark v1
        shape<::zengine::workshop::marks_persist::WorkshopMarks>(), // WorkshopMarks v1
        shape<::zengine::flow_host::detail::FlowHostClockStart>(), // FlowHostClockStart v1
        shape<::zengine::flow_host::detail::FlowHostClockState>(), // FlowHostClockState v1
        shape<::zengine::flow_host::detail::FlowHostPulse>(), // FlowHostPulse v1
        shape<::zengine::flow::NodePlace>(), // NodePlace v1
        shape<::zengine::flow::EmptyTrigger>(), // EmptyTrigger v1
        shape<::zengine::flow::StoredFormFile>(), // StoredFormFile v1
        shape<::zengine::flow::WorkspaceFile>(), // WorkspaceFile v1
        shape<::zengine::inventory_pane::InventoryToolbox>(), // InventoryToolbox v1
        shape<::zengine::inventory_pane::v2::InventoryToolbox>(), // InventoryToolbox v2
        shape<::zengine::inventory_pane::SavedBinding>(), // SavedBinding v1
        shape<::zengine::inventory_pane::SavedView>(), // SavedView v1
        shape<::zengine::inventory::CaptureAddRequest>(), // CaptureAddRequest v1
        shape<::zengine::inventory::CaptureContext>(), // CaptureContext v1
        shape<::zengine::inventory::CaptureRequest>(), // CaptureRequest v1
        shape<::zengine::source_transfer::SourceLocation>(), // SourceLocation v1
        shape<::zengine::source_transfer::SourceLocationContext>(), // SourceLocationContext v1
        shape<::zengine::source_transfer::SourceSelection>(), // SourceSelection v1
        shape<::zengine::source_transfer::SourceText>(), // SourceText v1
        shape<::zengine::timer::TimerHandoff>(), // TimerHandoff v1
        shape<::zengine::timer::TimerHandoffEntry>(), // TimerHandoffEntry v1
        shape<::zengine::ui::Element>(), // Element v2
        shape<::zengine::ui::Extent>(), // Extent v1
        shape<::zengine::view_builder::ViewBuilderRun>(), // ViewBuilderRun v1
        shape<::zengine::view::detail::ViewRetire>(), // ViewRetire v1
        shape<::zengine::view::detail::ViewHostState>(), // ViewHostState v1
        shape<::zengine::view::detail::ViewRedraw>(), // ViewRedraw v1
        shape<::zengine::view::detail::ViewStart>(), // ViewStart v2
        shape<::zengine::workshop::ArrangementDoorState>(), // ArrangementDoorState v2
        shape<::zengine::workshop::EditorSwitchState>(), // EditorSwitchState v1
        shape<::zengine::workshop::EditorSwitchAnswered>(), // EditorSwitchAnswered v1
        shape<::zengine::workshop::EditorSwitchCancelled>(), // EditorSwitchCancelled v1
        shape<::zengine::workshop::EditorSwitchConfirmed>(), // EditorSwitchConfirmed v1
        shape<::zengine::workshop::EditorSwitchProgress>(), // EditorSwitchProgress v1
        shape<::zengine::workshop::EditorSwitchRequested>(), // EditorSwitchRequested v1
        shape<::zengine::workshop::EditorSwitchStatusRequested>(), // EditorSwitchStatusRequested v1
        shape<::zengine::workshop::GuestDoorState>(), // GuestDoorState v1
        shape<::zengine::workshop::keymap_persist::v1::WorkshopKeymap>(), // WorkshopKeymap v1
        shape<::zengine::workshop::keymap_persist::WorkshopKeymap>(), // WorkshopKeymap v2
        shape<::zengine::workshop::keymap_persist::WorkshopKeymapRow>(), // WorkshopKeymapRow v1
        shape<::zengine::workshop::load::BootState>(), // BootState v1
        shape<::zengine::workshop::load_persist::WorkshopLoadAlternative>(), // WorkshopLoadAlternative v1
        shape<::zengine::workshop::load_persist::v1::WorkshopLoadArtifact>(), // WorkshopLoadArtifact v1
        shape<::zengine::workshop::load_persist::v3::WorkshopLoadArtifact>(), // WorkshopLoadArtifact v2
        shape<::zengine::workshop::load_persist::WorkshopLoadArtifact>(), // WorkshopLoadArtifact v3
        shape<::zengine::workshop::load_persist::WorkshopLoadChoice>(), // WorkshopLoadChoice v1
        shape<::zengine::workshop::load_persist::v1::WorkshopLoadFile>(), // WorkshopLoadFile v1
        shape<::zengine::workshop::load_persist::v2::WorkshopLoadFile>(), // WorkshopLoadFile v2
        shape<::zengine::workshop::load_persist::v3::WorkshopLoadFile>(), // WorkshopLoadFile v3
        shape<::zengine::workshop::load_persist::WorkshopLoadFile>(), // WorkshopLoadFile v4
        shape<::zengine::workshop::load_persist::WorkshopLoadProvider>(), // WorkshopLoadProvider v1
        shape<::zengine::workshop::load_persist::WorkshopLoadWeave>(), // WorkshopLoadWeave v1
        shape<::zengine::workshop::v2::PaneActions>(), // PaneActions v2
        shape<::zengine::workshop::PanePresentation>(), // PanePresentation v1
        shape<::zengine::workshop::v3::PresentationAdmitRequested>(), // PresentationAdmitRequested v3
        shape<::zengine::workshop::PresentationAdmitted>(), // PresentationAdmitted v1
        shape<::zengine::workshop::v3::PresentationTrial>(), // PresentationTrial v3
        shape<::zengine::workshop::PresentationTrialRequested>(), // PresentationTrialRequested v1
        shape<::zengine::workshop::OpeningState>(), // OpeningState v1
        shape<::zengine::workshop::ProjectDoorState>(), // ProjectDoorState v3
        shape<::zengine::workshop::PaneRevealAnswered>(), // PaneRevealAnswered v1
        shape<::zengine::workshop::PaneRevealRequested>(), // PaneRevealRequested v1
        shape<::zengine::workshop::PowersDoorState>(), // PowersDoorState v1
        shape<::zengine::workshop::prefs_persist::WorkshopPrefs>(), // WorkshopPrefs v3
        shape<::zengine::workshop::prefs_persist::v2::WorkshopPrefs>(), // WorkshopPrefs v2
        shape<::zengine::workshop::prefs_persist::v1::WorkshopPrefs>(), // WorkshopPrefs v1
        shape<::zengine::workshop::OfficeSettings>(), // OfficeSettings v1
        shape<::zengine::workshop::OfficeSettingsDeclared>(), // OfficeSettingsDeclared v1
        shape<::zengine::workshop::QuitDeliveryRefusalNoted>(), // QuitDeliveryRefusalNoted v1
        shape<::zengine::workshop::recipe_persist::v1::WorkshopCMakeTarget>(), // WorkshopCMakeTarget v1
        shape<::zengine::workshop::recipe_persist::WorkshopCMakeTarget>(), // WorkshopCMakeTarget v2
        shape<::zengine::workshop::recipe_persist::v1::WorkshopRecipe>(), // WorkshopRecipe v1
        shape<::zengine::workshop::recipe_persist::WorkshopRecipe>(), // WorkshopRecipe v2
        shape<::zengine::workshop::recipe_persist::v1::WorkshopRecipeFile>(), // WorkshopRecipeFile v1
        shape<::zengine::workshop::recipe_persist::WorkshopRecipeFile>(), // WorkshopRecipeFile v2
        shape<::zengine::workshop::recipe_persist::WorkshopSingleSource>(), // WorkshopSingleSource v1
        shape<::zengine::workshop::SampleDoorState>(), // SampleDoorState v1
        shape<::zengine::workshop::session_history::v6::WorkshopLayout>(), // WorkshopLayout v1
        shape<::zengine::workshop::session_history::v1::WorkshopSession>(), // WorkshopSession v1
        shape<::zengine::workshop::session_history::v2::WorkshopSession>(), // WorkshopSession v2
        shape<::zengine::workshop::session_history::v3::WorkshopSession>(), // WorkshopSession v3
        shape<::zengine::workshop::session_history::v4::WorkshopSession>(), // WorkshopSession v4
        shape<::zengine::workshop::session_history::v5::WorkshopSession>(), // WorkshopSession v5
        shape<::zengine::workshop::session_history::v6::WorkshopSession>(), // WorkshopSession v6
        shape<::zengine::workshop::session_history::v6::WorkshopSetupLink>(), // WorkshopSetupLink v1
        shape<::zengine::workshop::session_history::v6::WorkshopViewport>(), // WorkshopViewport v1
        shape<::zengine::workshop::session_history::v7::WorkshopLayout>(), // WorkshopLayout v2
        shape<::zengine::workshop::session_history::v7::WorkshopSession>(), // WorkshopSession v7
        shape<::zengine::workshop::session_history::v7::WorkshopSetupLink>(), // WorkshopSetupLink v2
        shape<::zengine::workshop::session_history::v8::WorkshopSession>(), // WorkshopSession v8
        shape<::zengine::workshop::session_history::v9::WorkshopSession>(), // WorkshopSession v9
        shape<::zengine::workshop::session_persist::WorkshopChoice>(), // WorkshopChoice v1
        shape<::zengine::workshop::session_persist::WorkshopLayout>(), // WorkshopLayout v3
        shape<::zengine::workshop::session_persist::WorkshopPlacement>(), // WorkshopPlacement v1
        shape<::zengine::workshop::session_persist::WorkshopSession>(), // WorkshopSession v10
        shape<::zengine::workshop::session_persist::WorkshopSetupLink>(), // WorkshopSetupLink v3
        shape<::zengine::workshop::session_persist::WorkshopViewport>(), // WorkshopViewport v2
        shape<::zengine::workshop::setup_persist::v2::WorkshopPanePlace>(), // WorkshopPanePlace v1
        shape<::zengine::workshop::setup_persist::v3::WorkshopPanePlace>(), // WorkshopPanePlace v2
        shape<::zengine::workshop::setup_persist::WorkshopPanePlace>(), // WorkshopPanePlace v3
        shape<::zengine::workshop::setup_persist::v2::WorkshopPaneSize>(), // WorkshopPaneSize v1
        shape<::zengine::workshop::setup_persist::v3::WorkshopPaneSize>(), // WorkshopPaneSize v2
        shape<::zengine::workshop::setup_persist::WorkshopPaneSize>(), // WorkshopPaneSize v3
        shape<::zengine::workshop::setup_persist::v2::WorkshopSetup>(), // WorkshopSetup v2
        shape<::zengine::workshop::setup_persist::v3::WorkshopSetup>(), // WorkshopSetup v3
        shape<::zengine::workshop::setup_persist::v4::WorkshopSetup>(), // WorkshopSetup v4
        shape<::zengine::workshop::setup_persist::WorkshopSetup>(), // WorkshopSetup v5
        shape<::zengine::workshop::setup_persist::v2::WorkshopSetupPane>(), // WorkshopSetupPane v2
        shape<::zengine::workshop::setup_persist::v3::WorkshopSetupPane>(), // WorkshopSetupPane v3
        shape<::zengine::workshop::setup_persist::v4::WorkshopSetupPane>(), // WorkshopSetupPane v4
        shape<::zengine::workshop::setup_persist::WorkshopSetupPane>(), // WorkshopSetupPane v5
        shape<::zengine::workshop::PaneSettingsDeclared>(), // PaneSettingsDeclared v1
        shape<::zengine::workshop::PaneDocumentRequested>(), // PaneDocumentRequested v1
        shape<::zengine::workshop::PaneDocumentShown>(), // PaneDocumentShown v2
        shape<::zengine::workshop::PaneDocumentSection>(), // PaneDocumentSection v2
        shape<::zengine::workshop::PaneDocumentKey>(), // PaneDocumentKey v1
        shape<::zengine::manual::ShapeDocumentRequested>(), // ShapeDocumentRequested v1
        shape<::zengine::manual::ShapeDocumentShown>(), // ShapeDocumentShown v1
        shape<::zengine::workshop::PaneSettings>(), // PaneSettings v1
        shape<::zengine::workshop::TerminalCaptureFacts>(), // TerminalCaptureFacts v1
        shape<::zengine::workshop::WithdrawalFence>(), // WithdrawalFence v1
        shape<::zengine::workshop::WorkshopState>(), // WorkshopState v1
        // ...and the declarations only Workshop still accepts, from a desktop built before causes.
        shape<::zengine::workshop::AppActionRow>(), // AppActionRow v1
        shape<::zengine::workshop::AppActions>(), // AppActions v1
        shape<::zengine::composer::ComposerCommandContext>(), // ComposerCommandContext v1
        shape<::zengine::snake::OperatorState>(), // OperatorState v1
        // What crosses the window's two seams, and the operands of the seven operators: bytes in
        // process, in no weave's manifest.
        shape<::zengine::graphics::PaintNode>(), // PaintNode v1
        shape<::zengine::graphics::PaintTask>(), // PaintTask v2
        shape<::zengine::graphics::NodePaint>(), // NodePaint v1
        shape<::zengine::graphics::PaintResult>(), // PaintResult v1
        shape<::zengine::graphics::ElementEvent>(), // ElementEvent v1
        shape<::zengine::graphics::ElementOutcome>(), // ElementOutcome v1
        shape<::zengine::graphics::ElementDescribed>(), // ElementDescribed v1
        shape<::zengine::graphics::Paint>(), // Paint v1
        shape<::zengine::graphics::GroupArgs>(), // GroupArgs v1
        shape<::zengine::graphics::RectArgs>(), // RectArgs v1
        shape<::zengine::graphics::EllipseArgs>(), // EllipseArgs v1
        shape<::zengine::graphics::LineArgs>(), // LineArgs v1
        shape<::zengine::graphics::PathArgs>(), // PathArgs v1
        shape<::zengine::graphics::TextArgs>(), // TextArgs v1
        shape<::zengine::graphics::ImageArgs>(), // ImageArgs v1
        // The faces the parts an owner draws record beside their nodes (graphics/vocabulary.hpp,
        // graphics/elements.hpp).
        shape<::zengine::graphics::PartFace>(), // PartFace v1
        shape<::zengine::graphics::ButtonFace>(), // ButtonFace v1
        shape<::zengine::graphics::CheckboxFace>(), // CheckboxFace v1
        shape<::zengine::graphics::ListRowFace>(), // ListRowFace v1
        shape<::zengine::graphics::GridCellFace>(), // GridCellFace v1
        shape<::zengine::graphics::IconFace>(), // IconFace v1
        shape<::zengine::graphics::ImageFace>(), // ImageFace v1
        shape<::zengine::graphics::ScrollBarFace>(), // ScrollBarFace v1
        shape<::zengine::graphics::TextBoxFace>(), // TextBoxFace v1
        // The snake's board: its data, changes and state, bytes its world and its code exchange.
        shape<::zengine::snake::SnakeBoardData>(), // SnakeBoardData v1
        shape<::zengine::snake::SnakeStep>(), // SnakeStep v1
        shape<::zengine::snake::SnakeNewGame>(), // SnakeNewGame v1
        shape<::zengine::snake::SnakeBoardState>(), // SnakeBoardState v1
        // The theme file and the settings office's doors, which no weave declares.
        shape<::zengine::settings::ThemeSetting>(), // ThemeSetting v1
        shape<::zengine::settings::ThemeKey>(), // ThemeKey v1
        shape<::zengine::settings::ThemeFile>(), // ThemeFile v1
        shape<::zengine::settings::ResolvedThemeSetting>(), // ResolvedThemeSetting v1
        shape<::zengine::settings::ResolvedThemeKey>(), // ResolvedThemeKey v1
        shape<::zengine::settings::ThemeResolved>(), // ThemeResolved v1
        shape<::zengine::settings::ThemeSummary>(), // ThemeSummary v1
        shape<::zengine::settings::ThemeApplyRequested>(), // ThemeApplyRequested v1
        shape<::zengine::settings::ThemeSkip>(), // ThemeSkip v1
        shape<::zengine::settings::ThemeActed>(), // ThemeActed v1
        shape<::zengine::settings::ThemeSaveRequested>(), // ThemeSaveRequested v1
        shape<::zengine::settings::ThemeSnapshotRequested>(), // ThemeSnapshotRequested v1
        shape<::zengine::settings::ThemePromoteRequested>(), // ThemePromoteRequested v1
        shape<::zengine::settings::ThemeSaved>(), // ThemeSaved v1
        shape<::zengine::settings::ThemesRequested>(), // ThemesRequested v1
        shape<::zengine::settings::ThemesShown>(), // ThemesShown v1
        shape<::zengine::settings::ThemeResolveRequested>(), // ThemeResolveRequested v1
        shape<::zengine::settings::ThemeProvenanceRequested>(), // ThemeProvenanceRequested v1
        shape<::zengine::settings::ThemeProvenanceEntry>(), // ThemeProvenanceEntry v1
        shape<::zengine::settings::ThemeProvenanceShown>(), // ThemeProvenanceShown v1
        // Said by the OS window, which a build without SDL has no manifest of.
        shape<zengine::graphics::SurfaceCloseRequested>(), // SurfaceCloseRequested v1
    };
}

#endif
