// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_TESTS_WORKSHOP_SUPPORT_HPP
#define ZENGINE_TESTS_WORKSHOP_SUPPORT_HPP

// Shared support for the Workshop suites: what more than one of them needs, and nothing a single
// suite uses (VM-POP-13). Fixture placement, actor grants and delayed replies:
// docs/contributing/testing-workshop-panes.md. Its functions and its rigs' members are declared
// here and defined once, in tests/workshop_support.cpp, the zengine-workshop-support library
// every Workshop suite links, so no suite compiles their bodies; a template or a constexpr
// function is defined here. They stand at namespace scope, since a namespace would change
// unqualified lookup inside each of them. It includes doctest.h because the fixtures assert.

#include "role_set.hpp"
#include "doctest.h"
#include "control_parts.hpp"
#include "rig_windows.hpp"

#include "info/settings_rows.hpp"
#include "persist/file.hpp"
#include "property/property.hpp"
#include "workshop/screen.hpp"
#include "workshop/setup.hpp"
#include "workshop/setup_persist.hpp"
#include "workshop/keymap.hpp"
#include "workshop/keymap_persist.hpp"
#include "workshop/prefs_persist.hpp"
#include "workshop/user_paths.hpp"
#include "workshop/layouts/session_history.hpp"
#include "workshop/window_persist.hpp"
#include "workshop/weave.hpp"
#include "workshop/grant.hpp"
#include "workshop/opening.hpp" // the opening manager the host mounts
#include "pane/drawing.hpp"
#include "pane/menu.hpp"
#include "pane/parts.hpp"
#include "pane/rows.hpp"
#include "graphics/implied.hpp"
#include "graphics/ops.hpp"
#include "graphics/window/store.hpp"
#include "workshop/vocabulary.hpp"
// The process id a temporary root is named by, and on Windows the attribute the sweep reads.
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <unistd.h> // getpid
#endif

#include "composer/vocabulary.hpp"
#include "introspection/loaded.hpp"
#include "introspection/powers.hpp"
#include "introspection/resolved.hpp"
#include "introspection/vocabulary.hpp"
#include "operator/catalog.hpp"
#include "operator/host_surface.hpp"
#include "operator/provider_host.hpp"
#include "workshop/arrangement.hpp"
#include "workshop/arrangement_vocabulary.hpp"
#include "workshop/host_sources.hpp"
#include "workshop/powers_door.hpp"
#include "realization/owner.hpp"
#include "realization/load_plan.hpp"
#include "realization/vocabulary.hpp"
#include "realization_claims.hpp"
#include "workshop/provenance.hpp" // what stands behind a role's code, wired as the host wires it
#include "workshop/recipes.hpp"
#include "workshop/sample_door.hpp"
#include "workshop/sample_presentation.hpp"
#include "workshop/sample_vocabulary.hpp"
#include "workshop/screen_planes.hpp"
#include "timer/vocabulary.hpp"
#include "graphics/vocabulary.hpp"
#include "screen_cells.hpp"
#include "ui/layout.hpp"
#include "ui/vocabulary.hpp"
#include "layouts_stand_in.hpp"
#include "workshop/project_seam_vocabulary.hpp"

#include <zen/host/reactor.hpp>
#include <zen/kernel/control.hpp>
#include <zen/kernel/admission.hpp>
#include <zen/kernel/kernel.hpp>
#include <zen/kernel/manager.hpp>

#include <zen/history/logger.hpp>   // the durable record attention must not reach
#include <zen/history/recorder.hpp> // ...and the working memory it must not reach either

#include <zen/host/terminal_wiring.hpp>
#include <zen/schema.hpp>
#include <zen/serialize.hpp>
#include <zen/terminal/input_lex.hpp>
#include <zen/terminal/session.hpp>
#include <zen/terminal/transcript.hpp>
#include <zen/terminal/vocabulary.hpp>
#include <zen/weave.hpp>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib> // std::system, for the junction `make_linked_directory` makes on Windows
#include <filesystem>
#include <fstream>
#include <functional>
#include <ios>
#include <iterator>
#include <memory>
#include <limits>
#include <map>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

namespace zengine::testing {

/// The entry name CMake gave this suite, `ZENGINE_WORKSHOP_SUITE`, answered by the suite's own unit
/// (tests/workshop_suite.cpp), which is compiled with the suite's definitions.
const char* workshop_suite();

/// The image file `stem` names when only some suites are given one and this suite is, else null;
/// answered by the suite's own unit as `workshop_suite` is.
const char* workshop_suite_artifact(const std::string& stem);

} // namespace zengine::testing

using namespace zengine::workshop;

using namespace zengine::pane;
using loom::schema_of;
namespace component = zengine::component;
namespace input = zengine::input;
namespace op = zengine::op;
namespace gfx = zengine::graphics;
namespace ui = zengine::ui;
namespace load = zengine::realization;
/// WHAT THE WINDOW SHOWING THE DESK SHOWS, as it answers `WindowShowAsked` (rig_windows.hpp).
using DeskShown = zengine::testing::rig_windows::Shown;

/// THE DRAWINGS THE WINDOWS ON A RIG'S BUS TOOK, mirrored off its tap: each `Drawn` delivered to a
/// `zengine.window.*` role applied to its area as the window's store applies one
/// (graphics/window/store.hpp), and the room the window last gave each area. What a case reads of
/// a pane's rows, parts, caret and selection is read from these -- what the pane drew, in the room
/// the window gave it -- never from Workshop, which holds none of it.
class DrawnMirror {
public:
    explicit DrawnMirror(loom::Switchboard& bus);
    ~DrawnMirror();
    DrawnMirror(const DrawnMirror&) = delete;
    DrawnMirror& operator=(const DrawnMirror&) = delete;

    /// The area as the windows hold it, or none before anything was drawn or given there.
    const zengine::graphics::window::WinArea* area(const std::string& name) const;
    /// How the window that gave the area its room sets text there.
    gfx::TextMeasure measure(const std::string& name) const;

private:
    void take(const loom::BusEvent& ev);

    loom::Switchboard& bus_;
    loom::ObserverId tap_{};
    std::map<std::string, zengine::graphics::window::WinArea> areas_;
    std::map<std::string, gfx::TextMeasure> measures_;
};

/// THE RIGS ALIVE NOW, the innermost last: the mirror of what their windows took and their
/// Workshop's session, which a held reading reads. Each rig stands in it while it lives.
struct HeldDesk {
    const DrawnMirror* mirror = nullptr;
    std::function<const Session&()> session;
};
std::vector<HeldDesk>& held_desks();
class HeldDeskEntry {
public:
    HeldDeskEntry(const DrawnMirror& mirror, std::function<const Session&()> session);
    ~HeldDeskEntry();
    HeldDeskEntry(const HeldDeskEntry&) = delete;
    HeldDeskEntry& operator=(const HeldDeskEntry&) = delete;

private:
    const DrawnMirror* mirror_ = nullptr;
};



/// A `component::TextBox` holding a text of this LENGTH, with its caret and its window where a
/// case wants them. The window is moved through the real door, never written:
/// `keep_caret_visible(caret - first)` lands exactly on `first` for every value a case can ask for
/// (its rule 1 caps the window at `size - room`, which is `first` here, and rule 3 pulls it up to
/// the same place). A window past the end of the text (`first > length`) collapses to the end.
component::TextBox box_of(std::size_t length, std::size_t caret, std::size_t first);

/// A canvas is a list of planes. These concatenate one KIND across them, in the order a medium
/// walks them -- the right shape for "was this drawn" and "which of two same-kind primitives is
/// later" -- and none is a visibility oracle: what is SEEN at a cell is `cell_text_of` below
/// (text) and `rasterized` (the whole picture), which run the planes' own two-level order.
std::vector<ScreenRect> all_rects(const ScreenCanvas& c);

std::vector<ScreenLabel> all_labels(const ScreenCanvas& c);

std::vector<ScreenRegion> all_texts(const ScreenCanvas& c);

/// THE SAME CANVAS WITHOUT THE WORKSPACE'S OWN PLANE -- what the screen's panes, panes and
/// overlays published, and nothing the DOCUMENT published. The workspace plane carries a text
/// region per placed object (the weaver's authored name, `kGroundBeneath`), so a case counting the
/// chrome's regions or indexing a pane's projected rows asks about the planes after it. A plane
/// and not a predicate, because `paint` writes the workspace whole into `layers.front()` before
/// any pane exists; cases about the names themselves read `object_names`.
ScreenCanvas without_workspace(const ScreenCanvas& c);

/// THE AUTHORED OBJECTS' NAMES, as the workspace plane published them.
std::vector<ScreenRegion> object_names(const ScreenCanvas& c);

/// WHAT WORKSHOP'S PLANES SHOW AS CELLS, no ink: each row of the canvas's cells, ended as a
/// terminal's lines are (`screen_cells::rows_of`).
std::string plain_cells(const ScreenCanvas& c);

/// A CELL COUNT IN CANVAS PIXELS -- the one multiply a case that thinks in whole cells performs
/// to author or compare pane geometry, which is in pixels. Short because it appears wherever a
/// case says "40 cells".
inline constexpr std::int64_t cells_px(std::int64_t cells) { return cells * gfx::kCellPx; }

/// WHERE THE ROOM STARTS, IN CELLS, ON A SCREEN WHERE TEXT IS A CELL: under the top band's rows.
/// The cases that think in workspace cells stand on such a screen; a case on a face that sets type
/// reads `Screen::room_y`.
inline constexpr std::int64_t kRoomCellX = 0;
inline constexpr std::int64_t kRoomCellY = kTopRows;
static_assert(kMinScreen.room_y == cells_px(kRoomCellY),
              "the cell-medium room starts where the screen says it does");

/// THE SCREEN'S BOUNDS IN CELLS, for the cases that sweep screens a cell at a time.
inline constexpr std::int64_t kScreenMinCols = kScreenMinW / gfx::kCellPx;
inline constexpr std::int64_t kScreenMinRows = kScreenMinH / gfx::kCellPx;
inline constexpr std::int64_t kScreenMaxCols = kScreenMaxW / gfx::kCellPx;
inline constexpr std::int64_t kScreenMaxRows = kScreenMaxH / gfx::kCellPx;

/// A SCREEN'S FURNITURE IN CELLS, each edge floored as a terminal shows it: what a case that
/// thinks in cells compares its cell coordinates against. On a screen where text is a cell every
/// one of these is exact.
/// THE STRIP AT THE ROOM'S RIGHT EDGE a case stands a pane in: `top-right` at no offset, this many
/// cells wide and the room's whole height (`open_at_top_right`).
inline constexpr std::int64_t kStripCols = 28;

struct ScreenCells {
    std::int64_t w = 0, h = 0, strip_x = 0, room_y = 0, room_w = 0, room_h = 0, band_y = 0;
};
ScreenCells cells_of(const Screen& sc);

/// THE BOTTOM BAND'S FIRST LEGEND ROW, in cells: the band's first row.
std::int64_t help_row(const Screen& sc);

/// THE PLANE A CASE THAT BUILDS ITS OWN CANVAS BY HAND WORKS ON, created on first use.
/// A case exercising ONE painter is asking about one presentation, which is one plane.
ScreenPlane& plane(ScreenCanvas& c);

/// ONE REGION ON A CANVAS OF ITS OWN, at the same extent -- the plan for exactly this
/// presentation and nothing else on the screen. A fresh canvas, not the published one with a
/// plane overwritten: a Workshop canvas carries a plane per presentation, so replacing one
/// plane's regions would leave every other plane's on it.
ScreenCanvas canvas_of_region(const ScreenCanvas& like,
                                     const ScreenRegion& r);

/// EVERY REGION'S ROWS AS CELLS, across the planes in their order (`screen_cells::region_rows`).
std::vector<zengine::testing::screen_cells::CellRow> projected_of(const ScreenCanvas& c);

/// EVERYTHING ON A CANVAS SHOWN AS TEXT IN CELLS, in painter's order: per plane, its labels and
/// then its regions' rows as cells, so the LAST entry at a cell is the topmost text there. Text
/// only: a later plane's rect over a label is a question for `rasterized`.
std::vector<ScreenLabel> cell_text_of(const ScreenCanvas& c);

/// Find a label's text at a canvas cell, or "" -- how the screen tier asks what
/// a weaver would see at a place: a label's pixel anchor lands on the cell a terminal floors it to.
std::string label_at(const ScreenCanvas& c, std::int64_t x, std::int64_t y);

/// The byte of the row at a canvas cell that a region's caret stands on, or -1, read through the
/// cell projection as `label_at` reads the row's text.
std::int64_t caret_at(const ScreenCanvas& c, std::int64_t x, std::int64_t y);

/// ONE PROSE ROW OF THE INSPECTOR'S PROPERTY BODY, as a weaver reads it. The body is a bounded
/// region, which owns what is inside its bounds, so its cell projection pads every row to the
/// region's full width; that padding erases what was underneath and is noise in an assertion
/// about what a row SAYS, so this trims it. Through `label_at` and the real cell projection, so a
/// row cut at the body's width is still cut.
std::string inspector_row(const ScreenCanvas& c, std::int64_t x, std::int64_t y);


/// What is actually SEEN at a cell where several labels landed: the LAST one
/// written, because painter's order is list order and Workshop's planes draw it that
/// way. `label_at` above answers with the first, which is the bottom of the
/// stack -- fine everywhere nothing overlaps, and exactly wrong for asking
/// whether an overlay covered what is under it.
std::string topmost_at(const ScreenCanvas& c, std::int64_t x, std::int64_t y);

// ---- The same questions, asked of the window that shows the desk -------------------------------
// What a weaver sees is what the window shows: its rows, front areas over the ones behind, read at
// a canvas cell (12 canvas pixels a cell, wherever the window's face sets a character). A row says
// what stands from that cell to its end, its trailing blanks cut; the window draws no label apart
// from the row it stands on, so "the label at a cell" is the row from there.

/// What the window shows at a canvas cell, to the end of its row.
std::string label_at(const DeskShown& s, std::int64_t x, std::int64_t y);

/// ...the same, read as one row of the Inspector's body is read.
std::string inspector_row(const DeskShown& s, std::int64_t x, std::int64_t y);

/// What is seen at a cell: the window shows only the front-most, so the row is the answer.
std::string topmost_at(const DeskShown& s, std::int64_t x, std::int64_t y);

/// The window's rows, one string a row.
std::vector<std::string> rasterized(const DeskShown& s);

/// Every row the window shows, the rows joined as a terminal's lines are.
std::string plain_cells(const DeskShown& s);

/// THE SETUP A CASE THAT BUILT ITS `Panes` BY HAND IS IMPLICITLY WORKING UNDER: one authored row
/// per open pane, in open order, with no override on any axis and the identity front ranks --
/// exactly what `reconcile` would have produced from that setup -- so such a case still asks the
/// ONE resolver the question the application asks, not a second one with a defaulted argument.
Setup setup_for(const Panes& panes);

/// ONE DESK AS A WHOLE LAYOUT RUN -- what a case means when it says something about a session and
/// nothing about the plural. The session format carries the weaver's run and the position that was
/// live, so a case that means "one desk" says so once, here, not `{desk}, 0` at sixty call sites.
/// Its association is `none`: the layout is durable and related to no standalone Setup file.
std::vector<Layout> one_layout(Setup desk);

/// A RUN OF PLAIN DESKS, EACH WITH NO ASSOCIATION -- the same convenience for the cases
/// that mean several layouts and nothing about their Setup relationships.
std::vector<Layout> plain_run(std::vector<Setup> desks);

/// ONE LAYOUT ASSOCIATED WITH AN ARTIFACT WHOSE KNOWN VALUE IS `known`.
Layout linked_layout(Setup desk, std::string path, Setup known);

/// THE DESKS OF A RUN, in order -- for the cases that compare arrangements and mean nothing
/// about associations.
std::vector<Setup> desks_of(const std::vector<Layout>& run);

/// A RUN AS A SESSION KEEPS IT: every layout a positive id of its own -- the one it has, else its
/// place in the run counted from 1.
std::vector<Layout> numbered(std::vector<Layout> run);

/// ...AND THE ID THE NEXT NEW LAYOUT OF IT TAKES: one above every id it holds.
std::int64_t next_id_of(const std::vector<Layout>& run);

/// THE SESSION A RUN IS SAVED AS, as its value and as its file's text.
session_persist::WorkshopSession
session_value(const std::vector<Layout>& run, std::size_t active,
              const std::vector<session_persist::WorkshopChoice>& choices = {});
std::string session_text(const std::vector<Layout>& run, std::size_t active,
                                const std::vector<session_persist::WorkshopChoice>& choices = {});

/// THE LAST RUN AS A QUIT LEAVES IT: the session at `path`, and the window file beside it holding
/// the room and the placement.
Written save_last_run(const std::string& path, const std::vector<Layout>& run,
                             std::size_t active, std::int64_t w, std::int64_t h,
                             const window_persist::Placement& place = {},
                             const std::vector<session_persist::WorkshopChoice>& choices = {});

/// THE LAYOUT A LOADED SESSION WAS STANDING ON. `at()` rather than `[]` deliberately: a
/// case that reads this off a session that was NOT admitted has asked the wrong question,
/// and an exception says so where a default-constructed desk would quietly pass.
const Setup& live_layout(const session_persist::LoadedSession& loaded);

/// ...AND THE SETUP ASSOCIATION IT WAS STANDING ON, by the same rule.
const SetupLink& live_link(const session_persist::LoadedSession& loaded);

/// THE LIVE LAYOUT'S SETUP-ASSOCIATION VERDICT -- `none`, `current` or `modified` -- asked of the
/// layout that owns it: a fresh desk is `none`, related to no artifact, and two layouts can give
/// two different answers.
std::int64_t live_status(const SetupState& s);

/// ...AS THE LIVE COPY A SESSION PRESENTS SAYS IT.
std::int64_t live_status(const LiveDesk& d);

/// THE COPY A SESSION PRESENTS OF `run`, as the layouts' store would send it: for the cases that
/// paint a session of several layouts and stage no store.
LiveDesk live_copy_of(const SetupState& run);

/// THE TAB RUN AND ONE TAB OF A RUN, as the copy a session presents of it paints them.
LayoutTabRun layout_tab_run(const SetupState& run, std::int64_t columns);
std::string layout_tab_text(const SetupState& run, std::size_t at);

/// WHICH ROLE A SHAPE IS ASKED OF: the layouts' owner for every door it keeps -- a declaration,
/// a pane's row asked and what it cannot use, a settings door, a desk put live -- and Workshop for
/// every other.
template <class Shape>
inline constexpr bool kLayoutsDoor =
    std::is_same_v<Shape, PaneSettingsDeclared> || std::is_same_v<Shape, zengine::pane::v2::PaneSettingsDeclared> ||
    std::is_same_v<Shape, PaneSettingsAsked> || std::is_same_v<Shape, PaneSettingUnused> ||
    std::is_same_v<Shape, RoleSettingsDeclared> || std::is_same_v<Shape, PaneSettingsRequested> ||
    std::is_same_v<Shape, PaneSettingCommitRequested> ||
    std::is_same_v<Shape, RoleSettingsRequested> ||
    std::is_same_v<Shape, RoleSettingCommitRequested> ||
    std::is_same_v<Shape, PaneSettingPinRequested> || std::is_same_v<Shape, PaneSettingLockRequested> ||
    std::is_same_v<Shape, LayoutSettingsRequested> || std::is_same_v<Shape, SettingsWriteRequested> ||
    std::is_same_v<Shape, SetupApplyRequested> || std::is_same_v<Shape, OfficeSettingsDeclared> ||
    std::is_same_v<Shape, OfficeSettingsRequested> ||
    std::is_same_v<Shape, OfficeSettingCommitRequested>;
template <class Shape> inline const char* door_of() {
    return kLayoutsDoor<Shape> ? kLayoutsRole : kWorkshopProvider;
}

/// THE ROLE OF THE RIG'S STAGE HAND, and what the rig tells it: ask the layouts' owner something.
inline constexpr const char* kStageRole = "zengine.test.stage";
struct StageNudge {
    ZEN_SHAPE(StageNudge, 1);
};
struct StageHandState {
    ZEN_SHAPE(StageHandState, 1);
};

/// THE RIG'S STAGE HAND: a run staged in the owner's store past the doors reaches Workshop as the
/// owner's next picture, which the owner sends when it next answers anyone -- and this hand asks.
class StageHand
    : public loom::WeaveBase<StageHand, StageHandState, loom::Accept<StageNudge, LayoutSettingsAnswered>,
                             loom::Emit<LayoutSettingsRequested>> {
public:
    void on(const StageNudge&, loom::Mail& mail);
    void on(const LayoutSettingsAnswered&, loom::Mail&);
};

/// WHO IS ON THE RIG'S BUS NOW, as Loom's participants claim says it, read by the host: what a rig
/// hands the layouts' owner when it reads the owner's store between messages.
inline loom::host::Participants bus_participants(const loom::Switchboard& bus) {
    return loom::from_value<loom::host::Participants>(loom::host::participants_now(bus));
}

/// STAGE THE RUN THE LAYOUTS' OWNER HOLDS, changed by `f`: Workshop's copy is the store's at once,
/// as a write past the doors showed it, and the owner's picture of it is presented at Workshop's
/// next delivery -- nothing is reconciled before then.
template <class Rig, class F>
void stage_run(Rig& r, F&& f) {
    r.owner->read_host_now(bus_participants(r.bus));
    SetupState run = r.owner->layouts().run();
    f(run);
    r.owner->store().put_run(std::move(run));
    LiveDesk& d = const_cast<Session&>(r.w->session()).setup;
    LayoutNaming naming = std::move(d.naming);
    const LiveLayout live = r.owner->store().live_layout();
    d = live_copy_of(r.owner->layouts().run());
    d.naming = std::move(naming);
    d.revision = live.revision;
    d.unused = live.unused;
    r.nudge_owner();
}

/// ...ONLY ITS LIVE DESK, for the cases that write one desk past the doors.
template <class Rig, class F>
void stage_desk(Rig& r, F&& f) {
    stage_run(r, [&](SetupState& run) { f(run.active); });
}

/// ...AND ONE ROW OF IT, as a case authors past the doors: the setter's own answer.
template <class Rig>
Written staged_author_pane_size(Rig& r, const PaneRef& ref, const PaneSize& width,
                                const PaneSize& height) {
    Written out = Written::ok();
    stage_desk(r, [&](Setup& desk) { out = author_pane_size(desk, ref, width, height); });
    return out;
}
template <class Rig>
Written staged_author_pane_place(Rig& r, const PaneRef& ref, std::int64_t x, std::int64_t y) {
    Written out = Written::ok();
    stage_desk(r, [&](Setup& desk) { out = author_pane_place(desk, ref, x, y); });
    return out;
}
template <class Rig> bool staged_add_pane(Rig& r, const PaneRef& ref) {
    bool out = false;
    stage_desk(r, [&](Setup& desk) { out = add_pane(desk, ref); });
    return out;
}
template <class Rig> bool staged_remove_pane(Rig& r, const PaneRef& ref) {
    bool out = false;
    stage_desk(r, [&](Setup& desk) { out = remove_pane(desk, ref); });
    return out;
}
template <class Rig> bool staged_reset_pane_place(Rig& r, const PaneRef& ref) {
    bool out = false;
    stage_desk(r, [&](Setup& desk) { out = reset_pane_place(desk, ref); });
    return out;
}

/// RELATE THE LIVE LAYOUT TO `path`, WITH ITS CURRENT DESK AS THE KNOWN VALUE -- the state a
/// successful `s` leaves behind, staged directly for the cases that are about something
/// else.
void link_live_setup(SetupState& s, std::string path);

/// THE CELLS A PUBLISHED RECTANGLE COVERS, as a terminal floors it -- what a case that thinks in
/// cells compares a picture's rectangles by.
CellRect covered_cells(const ScreenRect& r);
CellRect covered_cells(const ScreenRegion& r);

/// IS THERE A RECTANGLE IN THIS COLOUR COVERING EXACTLY THESE CELLS?
bool has_rect(const ScreenCanvas& c, std::int64_t x, std::int64_t y, std::int64_t w,
                     std::int64_t h, gfx::Color ink);

/// One canvas as cells, one string per screen row: the glyphs Workshop's planes put on the cells
/// they cover, a rectangle's ink told by its glyph.
std::vector<std::string> rasterized(const ScreenCanvas& canvas);

struct SeenState {
    std::int64_t frames = 0;
    ZEN_SHAPE(SeenState, 1, ZEN_FIELD(frames));
};

/// The rig's ears on Workshop's publications, kept as values: the conditions, transcripts and pane
/// subjects it said. The publication is `to_any` and a load plan names who presents it, so a
/// host-side case can only ask "did Workshop say anything, and what" by having somebody on the bus
/// listening. Every utterance is kept in order, because the claim under test is as much about
/// SILENCE as about content. The desk is read from the window (`DeskShown`).
class Painter : public loom::WeaveBase<Painter, SeenState,
                                       loom::Accept<StandingConditions, TranscriptShown,
                                                    PaneSubjectShown>,
                                       loom::Emit<>> {
public:
    Painter(std::vector<StandingConditions>& conditions,
            std::vector<TranscriptShown>& transcripts,
            std::vector<PaneSubjectShown>& subjects);
    void on(const StandingConditions& c, loom::Mail&);
    void on(const TranscriptShown& t, loom::Mail&);
    void on(const PaneSubjectShown& s, loom::Mail&);

private:
    std::vector<StandingConditions>* conditions_;
    std::vector<TranscriptShown>* transcripts_;
    std::vector<PaneSubjectShown>* subjects_;
};

/// A SHAPE THE RIG'S TERMINAL SPEAKS, to whoever holds `kRigBoard`: a slot and a line of text, a
/// sample of ordinary speech for the cases about the Terminal's sending, completion and refusals.
struct RigNote {
    std::string slot;
    std::string text;
    ZEN_SHAPE(RigNote, 1, ZEN_FIELD(slot), ZEN_FIELD(text));
};
inline constexpr const char* kRigBoard = "zengine.test.board";

/// THE ROOMS ASKED OF THE DIRECTORY on `bus`, by area and by the role that asked, counted from
/// each ask's fate on the bus: delivered to the rig's own directory, or refused while none holds
/// its role yet.
struct RoomAsks {
    explicit RoomAsks(loom::Switchboard& bus);
    ~RoomAsks();
    RoomAsks(const RoomAsks&) = delete;
    RoomAsks& operator=(const RoomAsks&) = delete;
    /// How many times `role` asked the room of `area`.
    std::size_t times(const std::string& area, const std::string& role) const;

private:
    loom::Switchboard& bus_;
    loom::ObserverId tap_{};
    std::vector<std::pair<std::string, std::string>> asked_;
};

/// HOW MANY PASTES WERE ASKED of the role `window` names, counted on `bus` from the delivery of
/// each to its holder: the TUI window reads no clipboard and keeps no count of its own.
loom::ObserverId count_paste_asks(loom::Switchboard& bus, std::string window,
                                         std::int64_t& asks);

/// HOW MANY DRAWINGS OF ITS DESK WORKSHOP SENT the window that shows it, counted on `bus` from
/// the delivery of each: a repaint that moved nothing on the desk sends none.
loom::ObserverId count_desk_drawings(loom::Switchboard& bus, const loom::WeaveId& workshop,
                                            std::size_t& drawings);


/// One participant's own record, filtered -- the transcript is a MODEL, so a test asks it
/// structured questions rather than grepping rendered prose.
std::vector<loom::TranscriptEntry> of_kind(const loom::TerminalSession& me,
                                                  loom::TranscriptKind kind);


/// ONE CONDITION OUT OF A PROJECTION, BY KEY -- or nothing, which is the answer a resolved
/// condition gives. It takes the vector rather than a fixture so the two rigs share
/// one spelling, and it deliberately reads the PROJECTION rather than `Session::conditions`:
/// a derived condition is never in the held set, and a case that looked there would be
/// asserting about the wrong half of the model.
const Condition* condition_by_key(const std::vector<Condition>& all, const std::string& key);

/// THE MONOTONIC READING A CASE OWNS -- what the windows count the weaver's clicks by, and what
/// a rig keeping Workshop's own times runs its wait on (`HubTime`), so "how long ago was the last
/// press" is a thing a case STATES. It advances on every reading, by default past the longest
/// double-click time input takes, so two presses in a case are two deliberate aims; a case that
/// wants a DOUBLE-click says so (`together`), and the pace is the only thing it has to say.
struct InteractionClock {
    std::int64_t now = 0;
    std::int64_t step = input::kMostDoubleClickMs + 1;
    std::int64_t read();
    /// PRESSES FROM HERE ON ARE CLOSE ENOUGH TO BE ONE GESTURE...
    void together();
    /// ...and far enough apart to be two again.
    void apart();
    /// ...or exactly this far apart, for the two cases that pin the boundary itself.
    void spaced(std::int64_t ms);
};

/// THE HOST'S OWN WORD TO A SOURCE OF ITS WAIT, as a rig gives it: what the source claimed is
/// ready and its time came, on the machine's clock -- a source over a real child process keeps
/// real time. It has no sender, as only the host's has.
loom::Message host_wake();

/// THE HOST'S ONE WAIT FOR WORKSHOP'S OWN TIMES, AS A RIG KEEPS IT: a `loom::host::Reactor` on the
/// case's reading, trusting Workshop's role, so Workshop is handed the time as its host hands it --
/// Shift's taps, the desk reads' rate and a desk wait's bound are timed by it, and a rig without one
/// keeps none. The clock moves only when told: by the case (`clock.now`), by each turn of `settle`
/// (`clock.step`, the case's pace), and by nothing it reads.
struct HubTime {
    struct OnReading final : loom::host::Clock {
        InteractionClock* clock = nullptr;
        std::int64_t now_ms() const override;
        bool moves_on_its_own() const noexcept override;
        void arrive(std::int64_t ms) override;
    };
    loom::Switchboard& bus;
    OnReading reading;
    loom::host::Reactor wait;
    HubTime(loom::Switchboard& b, InteractionClock& clock);
    static OnReading on(InteractionClock& clock);
    /// THE BUS TURNED UNTIL IDLE, the case's pace of time passing at each turn and the wait looking
    /// after it: what was due by then is handed to Workshop before the case's next step.
    void settle();
};

// ---- The stack's stand-in: a runtime pane every `Live` admits first --------------------------
// An ordinary overlay-stack pane to seat, arrange, occlude, persist and remove: offered under a
// test role and admitted through the seam handler's own `admit_pane_offer`, with no provider
// behind it. `Live` admits it before anything else, so its handle is `kFirstRuntimeKind` by the
// mint's law; a case building its own `Panes` admits it first too (`admit_stock`). `PaneRig`
// does not: the seam suites mint their own handles and count on the order.
namespace stock {
// Nobody holds this role: the room Workshop grants the pane is refused on the tap, and its body
// shows the host's own waiting sentence.
inline constexpr const char* kRole = "zengine.test.stack";
inline constexpr const char* kPane = "stack";
inline constexpr const char* kName = "Stack";
inline constexpr const char* kSummary = "a stack pane with nobody behind it";
inline constexpr std::int64_t kKind = kFirstRuntimeKind;
} // namespace stock

PaneRef stock_ref();

/// THE STAND-INS' ROLE HELD, AND DEAD: a weave in `stock::kRole` that takes an area's room, as a
/// pane drawing in a window does, marked dead at once, so Loom's participants claim names a holder
/// the launch door seats the stand-ins for, while everything sent to the role is refused on the
/// tap and its bodies show the host's own waiting sentence.
loom::WeaveId mount_stock_holder(loom::Switchboard& bus);

/// THE SECOND STAND-IN, admitted right after the first under the same role: the other ordinary
/// stack pane a case seats, overlaps, orders and persists.
namespace second {
inline constexpr const char* kPane = "second";
inline constexpr const char* kName = "Second";
inline constexpr const char* kSummary = "a second stack pane with nobody behind it";
inline constexpr std::int64_t kKind = kFirstRuntimeKind + 1;
} // namespace second

PaneRef second_ref();

/// ADMIT THE STAND-IN INTO A CATALOG, first -- and prove it took the handle every case
/// spells. Idempotent: a refreshed offer keeps the handle it already had.
void admit_stock(Panes& panes);

/// ...AND THE SECOND, right after it, proving it took the handle every case spells.
void admit_second(Panes& panes);

class DoorHand;

/// A live Workshop: the real weave on a real bus, driven only by published input messages.
/// Nothing here reaches past the message boundary except to READ the result.
struct Live {
    loom::Switchboard bus;
    /// THE KERNEL AND THE WEAVE MANAGER the window's directory loads through, as a host loads it.
    loom::Kernel kernel{bus, loom::trust_every_artifact(
                                 "Zengine's test harness: it loads only this build tree's output")};
    loom::WeaveId control = loom::mount_control(kernel, bus);
    loom::WeaveId manager = loom::mount_manager(control, bus);
    /// THE WINDOW THE DESK IS SHOWN IN, and the directory beside it (rig_windows.hpp): brought up
    /// when a case starts the desk (`ready`, a room), or at its first gesture or reading; Workshop
    /// starts on its offer. Declared after the kernel, so it goes before the directory does.
    zengine::testing::rig_windows::Windows windows{bus, manager, GRAPHICS_SO};
    /// WHAT THE WINDOWS ON THIS BUS TOOK, mirrored, which a held reading reads while this rig lives.
    DrawnMirror drawn{bus};
    HeldDeskEntry held_entry{drawn, [this]() -> const Session& { return session(); }};
    /// False: this case brings its own directory or window, and the rig's never comes up.
    bool in_window = true;
    InteractionClock clock;
    HostContext host;
    /// HOW MANY DRAWINGS OF ITS DESK WORKSHOP SENT the window (`count_desk_drawings`) -- a
    /// repaint that moved the desk's count, never a desk to read.
    std::size_t pictures = 0;
    /// EVERY `StandingConditions` THIS WORKSHOP HAS SAID, in order -- so a case can ask how
    /// MANY times it spoke and not only what it last said.
    std::vector<StandingConditions> said_conditions;
    /// ...and every `TranscriptShown` this host published, for the same reason.
    std::vector<TranscriptShown> said_transcripts;
    /// ...and every `PaneSubjectShown` -- an inspector's pane subject (WL-INFO-14) -- in order.
    std::vector<PaneSubjectShown> said_subjects;
    WorkshopWeave* w = nullptr;
    loom::WeaveId workshop_id{};
    /// Loom's participants claimant, which Workshop reads who is on the bus from.
    loom::WeaveId participants_id{};
    /// The host's session door, which Workshop asks to end the host's lifetime once its quit is
    /// settled; here it stops nothing, and says whether it was asked.
    HostSessionDoor* session_door = nullptr;
    loom::WeaveId terminal_id{};
    /// THE HAND THAT OPENS AND CLOSES PANES THROUGH THE HOST'S DOORS, mounted when a case first
    /// asks (`door_hand`), so a case that never opens a pane has no extra party on its bus.
    DoorHand* hand = nullptr;
    loom::WeaveId hand_id{};
    /// THE HOST'S WATCH OVER THE QUIT'S DELIVERIES, mounted beside Workshop as `workshop.cpp`
    /// mounts it (WL-SESSION-19). Declared after the bus and the `HostContext` whose book it
    /// writes, so it is removed first.
    std::unique_ptr<QuitDeliveryWatch> quit_watch;

    /// THE LAYOUTS' OWNER, mounted in its own role before Workshop, as the host mounts it, and the
    /// rig's stage hand, mounted when a case first stages a run past the doors.
    LayoutsOwner* owner = nullptr;
    loom::WeaveId owner_id{};
    loom::WeaveId stage_id{};
    void mount_owner();
    /// ...AND THE OWNER ASKED SOMETHING, so the picture it owes is sent: delivered at the next drain.
    void nudge_owner();

    Live();

    /// MOUNT THE PARTICIPANT THE WAY THE HOST DOES -- on this bus, beside Workshop's own weave,
    /// handed over as `HostContext::terminal` -- with this rig's one rule, `RigNote` to whoever holds
    /// `kRigBoard`. `widen` is the canary lever, Workshop's own `to_any` rule, without which "a
    /// pane's publication reaches nobody" measures nothing. `shapes` mounts extra declared shapes,
    /// so a completion list can outgrow its room.
    loom::TerminalSession* mount_terminal(bool widen = false, int shapes = 0);

    /// THE DESK STARTED: the window brought up at the room Workshop holds, so it offers itself
    /// and Workshop starts on that offer, as a run's first showing starts it. A case that seated a
    /// window of its own (`own_medium`) is started by that window's offer.
    void ready();

    /// A ROOM OF `width` BY `height` CANVAS CELLS, the window sized to it as a weaver sizes it: a
    /// cell medium (the TUI window) unless a face is given, `adv` by `line` canvas pixels a
    /// character (the GUI window). The window says its device unit itself, so `cell` is kept only
    /// for the spelling of a medium the rig no longer reports.
    void extent(std::int64_t width, std::int64_t height, std::int64_t adv = 0,
                std::int64_t line = 0, std::int64_t cell = 0);
    /// ...and a room in canvas pixels, which a cell medium holds in whole cells.
    void extent_px(std::int64_t width, std::int64_t height, std::int64_t adv = 0,
                   std::int64_t line = 0, std::int64_t cell = 0);
    /// THE SHIPPED GRAPHICAL FACE, at a room in canvas cells: the GUI window with its face.
    void extent_on_window(std::int64_t width, std::int64_t height);

    /// WHAT THE WINDOW SHOWING THE DESK SHOWS NOW.
    DeskShown shown();

    /// THE WEAVER'S OWN BUTTON THROUGH THE WINDOW SHOWING THE DESK, at a canvas cell: landed by the
    /// window in whatever area stands there -- a pane's menu its presenter draws among them --
    /// where a bare moment goes to Workshop alone. `pressed` false is its release.
    void window_button(std::int64_t cx, std::int64_t cy, bool pressed, std::int64_t button = 1);
    /// ...a press and its release there.
    void window_click(std::int64_t cx, std::int64_t cy, std::int64_t button = 1);
    /// ...AT A POINT IN THE MEDIUM'S OWN SPACE, as a reading names one: the TUI window's cell,
    /// its rows above the desk counted, or the GUI window's pixel.
    void hand_button(std::int64_t x, std::int64_t y, std::int64_t space, bool pressed,
                     std::int64_t button = 1);
    /// ...OR QUEUED ON THE WINDOW'S PLATFORM, a button or a key, and read together on its next
    /// beat (`window_beat`), as one batch of the weaver's.
    void window_queue_button(std::int64_t cx, std::int64_t cy, bool pressed,
                             std::int64_t button = 1);
    void window_queue_key(std::int64_t scan, std::int64_t mods = input::mod::kNone);
    void window_beat();

    /// THE GUI WINDOW FROM HERE ON, with the face it ships with, for every room after -- one given
    /// in cells too: the medium whose platform clipboard a paste can read (`windows.set_clipboard`).
    /// The TUI window reads none. A window up already is moved to it now.
    void on_gui_window();
    bool gui_window = false;
    /// The face a room is shown with: the one asked, or the GUI window's own once this rig is on it.
    std::pair<std::int64_t, std::int64_t> face_for(std::int64_t adv, std::int64_t line) const;

    /// WHETHER THIS CASE SEATED A MEDIUM OF ITS OWN -- a directory or a window in its role --
    /// before the rig's came up: then the rig's never does, and the case's own window offers the
    /// room and starts the desk.
    bool own_medium();


    /// The window comes up, if it is not up, at the room and face Workshop holds now.
    void come_up();

    /// Type a whole line into whatever is taking text, then press Return.
    void type_line(const std::string& line);

    /// A GESTURE OR A WORD, published as a medium publishes it -- the window up first, so the
    /// desk it lands on is the one a weaver sees.
    void publish(const loom::Value& v);

    void key(std::int64_t sc, std::int64_t mods = input::mod::kNone);
    void text(const std::string& s);

    /// A pointer event AT A WORKSPACE CELL: `wx` cells from the room's left and `wy` cells below
    /// its top, which is where this screen's top band ends. The translation to the cell a window
    /// reports -- its own cell, with no console row above the canvas -- is the inverse of the
    /// weave's own, done here so every case below reads in the coordinates a weaver thinks in.
    std::int64_t term_x(std::int64_t wx) const;
    std::int64_t term_y(std::int64_t wy);

    /// The same workspace cell, as the WINDOW would report it: the pixel at the
    /// cell's top-left corner. Deliberately the inverse of the GUI window's
    /// own layout and not of the terminal's -- the two media report different
    /// numbers for one place (workshop/docs/pointer-spaces.md).
    std::int64_t px_x(std::int64_t wx) const;
    std::int64_t px_y(std::int64_t wy) const;

    /// THE WEAVER'S OWN POINTER MOMENT, through the window showing the desk, landed in whatever
    /// area stands there as a weaver's hand is.
    template <class Moment>
    void hand_moment(const Moment& moment) {
        come_up();
        REQUIRE_MESSAGE(windows.up(), "no window shows this desk");
        windows.hand({moment});
        bus.drain_until_idle();
    }

    /// A PRESS AT AN EXACT POSITION IN THE MEDIUM'S OWN NUMBERS -- a window pixel or a terminal
    /// cell, untranslated. Every other helper speaks the WORKSPACE cells a weaver thinks in for
    /// the document; the Terminal's interior is finer than a cell, so its cases must say a pixel.
    void press_at(std::int64_t x, std::int64_t y, std::int64_t space,
                  std::int64_t mods = input::mod::kNone);
    /// A MOTION at a raw position -- the one gesture hover reveal is about.
    void motion_at(std::int64_t x, std::int64_t y, std::int64_t space);

    void press(std::int64_t wx, std::int64_t wy, std::int64_t mods = input::mod::kNone);
    void release(std::int64_t wx, std::int64_t wy);
    /// A SECOND-BUTTON press at a workspace cell -- the same translation `press` uses, for the
    /// button that asks a question instead of taking hold.
    void right_press(std::int64_t wx, std::int64_t wy);
    /// The same two gestures at a CANVAS cell -- for presses on Workshop's own furniture
    /// (a pane's slot, the contextual surface), which is placed on the canvas and never
    /// in the document's room.
    void press_canvas(std::int64_t cx, std::int64_t cy);
    void release_canvas(std::int64_t cx, std::int64_t cy);
    void motion_canvas(std::int64_t cx, std::int64_t cy);
    void right_press_canvas(std::int64_t cx, std::int64_t cy);
    /// The contextual-action surface's state, as every contextual-menu case reads it.
    const ContextMenu& menu() const;
    void motion(std::int64_t wx, std::int64_t wy);
    /// THE WHEEL, AT A CANVAS CELL -- `press_canvas`'s translation for the one event the
    /// editor's viewport consumes. `dy` is notches, +1 away from the weaver (the wire's
    /// own convention), fractional exactly as a high-resolution wheel reports.
    void wheel_canvas(double dy, std::int64_t cx, std::int64_t cy);

    /// The same three gestures, arriving from the GUI window. The
    /// `+ cell/2` puts the event in the MIDDLE of the cell rather than on its
    /// corner, which is where a weaver's pointer actually is and is what makes a
    /// truncating (rather than flooring) projection visible.
    static std::int64_t mid(std::int64_t p);
    void press_px(std::int64_t wx, std::int64_t wy);
    void release_px(std::int64_t wx, std::int64_t wy);
    void motion_px(std::int64_t wx, std::int64_t wy);

    /// THE WINDOW'S BEATS UNTIL ITS ROOM STANDS: the GUI window grows to the reach of the areas it
    /// shows on one beat and offers its new room on the next.
    void window_settles();
    /// THE WEAVER CLOSED THE WINDOW THAT SHOWS THE DESK: its close, said as that window's role.
    void close_requested();

    const Session& session() const;
    /// EVERY LAYOUT OF THE RUN, as the layouts' store keeps it.
    const SetupState& run() const;
    std::string notice() const;

    /// THE GLANCE A PRESENTER OF WHAT IS TRUE LEADS WITH, from what this Workshop last said
    /// (`attention_glance`). Empty is the honest answer: nothing is true.
    std::string glance() const;
    /// WHAT IS CURRENTLY TRUE OF THIS WORKSHOP, through the one projection the
    /// screen and the view both spend.
    std::vector<Condition> conditions() const;
};

/// A long value that cannot fit an Inspector row at any extent this composition has.
inline const std::string kLongValue = "the quick brown fox jumps over the lazy dog";

/// THIS PROCESS, AS A NAME: the one thing two processes of one suite running at once are
/// guaranteed not to share.
std::string this_process_id();

/// THE TEMPORARY ROOT THIS SUITE OWNS IN THIS PROCESS, and nothing else does (VM-POP-14): the
/// suites run at once, each `TempDir` counter starts at zero, and `TempDir` removes what it finds
/// before it creates, so a shared name would be one case deleting another's files. The suite is
/// `zengine::testing::workshop_suite()`, the entry name CMake gave this binary, and the process id is the
/// rest, so two runs of one suite cannot collide either. A process sweeps only the root it made:
/// a crashed run's root stays, since a global sweep would be the deleting this name prevents.
std::filesystem::path workshop_temp_root();

/// DOES THIS ENTRY LEAVE THE TREE? The sweep's own predicate: the host's reparse attribute on
/// Windows, `symlink_status` elsewhere. A failure marks the row, which is what makes a dangling
/// junction removable BY NAME.
bool sweep_leaves_the_tree(const std::filesystem::directory_entry& entry);

/// REMOVE `p` AND EVERYTHING BENEATH IT, entering only what is really a directory (VM-POP-15):
/// libstdc++ on Windows walks a directory junction as one, so `std::filesystem::remove_all` would
/// empty a directory outside the tree through a junction. An entry that leaves the tree is
/// removed BY NAME and never entered -- `std::filesystem::remove` takes a junction, live or
/// dangling, and unlinks a symbolic link. Failures are swallowed: a destructor cannot report.
void remove_tree(const std::filesystem::path& p);

/// WHICH KIND OF LINK A CASE MADE, so it can say what a lane exercised.
enum class LinkArm { none, symbolic_link, junction };

const char* link_arm_name(LinkArm arm);

/// THE LINK THIS PLATFORM'S CASES MAKE: a junction on Windows, a directory symlink elsewhere.
inline constexpr LinkArm kPlatformLinkArm =
#if defined(_WIN32)
    LinkArm::junction;
#else
    LinkArm::symbolic_link;
#endif

/// A DIRECTORY THAT LEAVES THE TREE. On Windows a JUNCTION, by `mklink /J`, which needs no
/// privilege: a junction answers `is_symlink()` FALSE while still leaving the tree, so it is the
/// entry that tells the host's attribute from `is_symlink()`, and a symbolic link, which a
/// privileged runner could make, is not. Elsewhere a directory symlink. `none` when nothing was.
LinkArm make_linked_directory(const std::filesystem::path& link,
                                     const std::filesystem::path& target);

/// A directory of this run's own, removed when the case ends. Tests never write
/// into the source tree and never share a path with each other.
class TempDir {
public:
    explicit TempDir(const char* tag);
    ~TempDir();
    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;

    std::string file(const char* name) const;
    std::string document() const;
    const std::filesystem::path& path() const;

private:
    std::filesystem::path path_;
};

/// Read a whole file as bytes, for a case that wants to look at what was
/// actually written rather than at what the writer said it wrote.
std::string slurp(const std::string& path);

/// Put bytes at a path -- how a case forges a file that Workshop then meets as
/// an ordinary weaver would.
void spillout(const std::string& path, const std::string& text);

/// A RETIRED OBJECT DOCUMENT, BYTE FOR BYTE -- what the object document's own writer
/// (`persist::save_file`) wrote for the boot document, two objects called `panel`, before the
/// prototype canvas retired. Produced by that commit's code and kept here as a weaver's old
/// `workshop.json` is kept on their disk: every reader this host has must refuse it as what it
/// is not, and no door reads it as what it was (WL-DOC-22).
inline constexpr const char* kRetiredObjectDocument =
    R"({"zen":1,"schema":"WorkshopDocument","version":2,"content_id":"0xbfde7d02abe39177","fields":{"format":"zengine-workshop","format_version":"1","next_id":"3","objects":[{"id":"1","name":"panel","context":"0","x":"3","y":"2","width":{"mode":"percent","amount":"60"},"height":{"mode":"cells","amount":"6"}},{"id":"2","name":"panel","context":"0","x":"6","y":"10","width":{"mode":"cells","amount":"14"},"height":{"mode":"cells","amount":"4"}}]}})";

/// A stand-in for the Builder tool: it holds the role, records what it was
/// asked, and answers with whatever status the case has set up.
///
/// It is not a mock of the real tool's LOGIC (that has its own suite); it is the
/// other end of the conversation, so that "Workshop asked" and "Workshop showed
/// what it was told" are two facts a case can separate.
class ToolSeat
    : public loom::WeaveBase<ToolSeat, SeenState,
                             loom::Accept<zengine::builder::StatusRequested,
                                          zengine::builder::BuildRequested>,
                             loom::Emit<zengine::builder::BuildStatus,
                                        zengine::builder::RecipeCatalog>> {
public:
    /// TWO SHAPES ON ONE ASK, exactly as the real tool answers: what can be built here, and where
    /// the one it last built stands. The catalog first, because a pane that heard a status about
    /// a recipe it had never been told existed would be showing a choice nobody offered it.
    void on(const zengine::builder::StatusRequested&, loom::Mail& mail);
    void on(const zengine::builder::BuildRequested& ask, loom::Mail& mail);

    /// What this tool will say next time it is asked anything.
    zengine::builder::BuildStatus next{};
    /// ...and what it says this project can build at all.
    zengine::builder::RecipeCatalog catalog{};
    /// WHETHER EACH ASK WAS A BUILD OR A BUILD-AND-REALIZE. Recorded rather than
    /// asserted from the pane, because "the weaver's second intention crossed the
    /// role boundary" is a fact about what was SAID and not about what was shown.
    std::vector<bool> realize_asked;
    /// ...and whether it answers a build at all. A real build takes seconds and
    /// answers when the process exits; a stand-in that always answers instantly
    /// would make the pane's `waiting` state unreachable from any case.
    bool answers_builds = true;
    std::int64_t described = 0;
    std::vector<std::string> asked;
};

/// Mount the stand-in into the Builder role on this Workshop's bus.
ToolSeat* mount_tool(Live& t, const std::string& recipe);

/// EVERYTHING AT A PANE'S BOUNDS, top to bottom -- one question about a rectangle of canvas cells,
/// for every kind, asked of the window: each line of its face inside the rectangle, cut to the
/// rectangle's width, a line that shows nothing there left out. The window composes front areas
/// over the ones behind, so what a row says is what a weaver reads there.
std::string rect_text(const DeskShown& s, const CellRect& b);

/// THE CELLS INSIDE A SURFACE'S OWN CHROME -- where every row a pane, popup or overlay draws
/// lands: `pane_interior` at the cell grain, so "what does this pane SAY" and the painter that
/// said it are one rectangle. A case about the pane's PLACE (occupancy, a press on its edge,
/// coverage) wants the outer rectangle `bounds_of` answers. It asks the character medium's
/// question, in canvas CELLS, subtracting the chrome a cell-projected interior leaves
/// (`kChromePx`); a case about a FACE's own boundary takes the overload below with its screen.
CellRect pane_body_cells(const PixelRect& outer);
CellRect pane_body_cells(const CellRect& outer);
CellRect pane_body_cells(const PixelRect& outer, const Screen& sc);

/// What the overlay stack's first slot is showing, whatever is in it. The stack is anchored to
/// the canvas's top-left and its ROWS are the same on every screen -- only its width follows the
/// room -- and `rect_text` reads one column and a run of rows, so the minimum screen's rectangle
/// names the right rows on any of them. It reads the slot's INTERIOR, where the rows are.
std::string stack_text(const DeskShown& s);

/// Where a kind sits in the combined population, so a case names a KIND rather than a row
/// number that a later catalog entry would silently invalidate. The population is the
/// combined one -- built-ins, then the weaver's pane, then the admitted runtime panes --
/// because that is the one list every consumer walks (WL-CAT-05).
std::size_t catalog_at(const Panes& panes, std::int64_t kind);

/// OPEN A CLOSED KIND, OR CLOSE AN OPEN ONE -- through the host's launch and close doors, the two
/// the Pane Manager spends (defined below, once the hand that asks exists), for the cases that
/// only need a pane on or off the desk.
void pick(Live& t, std::int64_t kind);

/// Open the stack's stand-in pane the way a weaver does (see `stock`).
void open_stock_pane(Live& t);

/// TAKE THE KEYS BACK FROM WHATEVER PANE HOLDS THEM, without a gesture. A runtime pane holds the
/// keyboard from the press that pointed at it until a press elsewhere, so a case
/// that pressed into the stand-in and then wants COMMAND letters answered would otherwise be
/// typing into a pane nobody is behind.
void release_keys(Live& t);

/// THE STAND-IN IS IN THE STACK'S FIRST SLOT, asked through the placement path and read off
/// the window at the answer -- so the case cannot agree with the screen by both of them
/// holding the same constant. What the slot shows is the host's own header for a runtime
/// pane, which names the pane.
bool first_slot_shows_stock(Live& t);

/// EVERYTHING A PANE IS SHOWING, top to bottom, ASKED BY KIND: the placement path says where the
/// pane IS, as it told the painter, so no case can agree with the screen by both of them holding
/// the same constant.
std::string pane_shown(const DeskShown& shown, const Session& s, std::int64_t kind);

/// The reference a built-in kind is spelled with, as a case says it -- through
/// the catalog, never as a literal, so a case cannot agree with a typo.
PaneRef ref_of(std::int64_t kind);

/// A reference to a pane no build of this Workshop has ever had. The
/// third-party entry every unresolved case is built on.
PaneRef stranger();

/// NOBODY HAS OFFERED ANYTHING -- said out loud, because the resolution door takes the whole
/// `Panes` rather than something a caller could forget. A default-constructed one has an empty
/// runtime catalog and a closed weaver pane, so a case passing it asks what the BUILT-IN half
/// answers with no provider in the process.
const Panes& no_providers();

/// A setup, spelled the way a case reads: a name and the kinds it means.
Setup setup_of(const std::string& name, const std::vector<std::int64_t>& kinds);

/// The kinds a session currently has open, in open order -- what the authored
/// order is supposed to have produced.
std::vector<std::int64_t> open_kinds(const Panes& panes);

/// A desk as a version-3 Workshop wrote it: every pixel amount in sub-units, four to a pixel,
/// and a place measured from the canvas, whose room began `kCanvasRoomTopPx` down.
/// The place word a version-3 to 5 Workshop wrote for a place as written today: those versions had
/// three words, and the room's top-right at no offset was their `right-column`.
std::string old_place_word(const setup_persist::WorkshopPanePlace& p);

setup_persist::v3::WorkshopSetup v3_desk(const Setup& s);

/// A version-3 desk read as its own reader would: landed on pixels as version 4, then on the
/// current shape, then the setup law.
Written v3_setup_in(const setup_persist::v3::WorkshopSetup& old, Setup& out);

/// A desk as a version-4 Workshop wrote it: today's rows without their settings.
setup_persist::v4::WorkshopSetup v4_desk(const Setup& s);

/// A desk as a version-5 Workshop wrote it: every coordinate and amount written, zero where its
/// word reads none.
setup_persist::v5::WorkshopSetup v5_desk(const Setup& s);

/// A RUN AS WORKSHOP KEPT IT AT VERSION 11: each layout's desk and remembered value at setup
/// version 5, with no id.
std::vector<session_history::v11::WorkshopLayout> v11_run(const std::vector<Layout>& run);

/// A SESSION AS WORKSHOP WROTE ITS FILE AT VERSION 11: the header's line, then a layout a line.
std::string v11_lines(const session_history::v11::WorkshopSession& s);

/// A version-11 session of `run`, standing on `active`, in the room `w` by `h` placed by `place`.
session_history::v11::WorkshopSession
v11_session(const std::vector<Layout>& run, std::size_t active, std::int64_t w, std::int64_t h,
            const window_persist::Placement& place = {},
            const std::vector<session_persist::WorkshopChoice>& choices = {});

/// A DESK AS WORKSHOP WROTE IT AT SETUP VERSION 6: every field of version 7's but each row's fold
/// and dock.
setup_persist::v6::WorkshopSetup v6_desk(const Setup& s);

/// A version-12 session of `run`, standing on `active`: each layout's id, its desk and remembered
/// value at setup version 6, and the next id after them.
session_history::v12::WorkshopSession
v12_session(const std::vector<Layout>& run, std::size_t active,
            const std::vector<session_persist::WorkshopChoice>& choices = {});

/// ...AS WORKSHOP WROTE ITS FILE AT VERSION 12: the header's line, then a layout a line.
/// A setup of version 7 of `s`: its rows without their theme, its themes' bottom as its baseline.
setup_persist::v7::WorkshopSetup v7_desk(const Setup& s);

/// A version-13 session of `run`, standing on `active`: each layout's id, its desk and remembered
/// value at setup version 7, and the next id after them.
session_history::v13::WorkshopSession v13_session(const std::vector<Layout>& run,
                                                         std::size_t active);

/// ...as its file's lines.
std::string v13_lines(const session_history::v13::WorkshopSession& s);

std::string v12_lines(const session_history::v12::WorkshopSession& s);

/// A DESK AT EVERY BOUND A FILE CAN MEET: `kMaxSetupPanes` rows whose keys are
/// `kMaxPaneKeyLen` bytes a writer escapes, every place and extent at `kMaxPanePixels`, and
/// `kMaxPaneSettingsPerDesk` settings with keys at their bound and texts escaped whole -- the
/// largest file the setup law lets a weaver write.
void fill_to_every_bound(Setup& s);

/// PRESS THE KEY THE KEYMAP BINDS TO A LAYOUT ACT, as a weaver does -- read from the keymap, so a
/// remap re-measures the case.
template <class Rig> inline void layout_key(Rig& r, Act act) {
    const Gesture g = r.session().keymap.gesture_of(act);
    r.key(g.scancode, g.modifiers);
}

/// A setup file's text with one substring replaced -- how the refusal cases
/// forge a file the honest writer could never produce. The document tier's own
/// `forged`, asked about the other artifact.
std::string forged_setup(const Setup& s, const std::string& from, const std::string& to);

/// The keys the shipped catalog binds to the four layout gestures, read from the keymap rather
/// than spelled here: a case that hard-coded `.` would keep passing after a remap, measuring its
/// own literal. Cases about the layout run's GEOMETRY belong to the screen suite and those about
/// its gestures to the panes -- two suites, one reading of the keymap.
struct LayoutKeys {
    Gesture next;
    Gesture previous;
    Gesture make;
    Gesture drop;
};

LayoutKeys layout_keys(const Live& t);

void press_gesture(Live& t, const Gesture& g);

/// WHERE A PAINTED TAB'S FIRST CELL IS, out of the SAME composition the painter wrote -- never a
/// column a case computed.
std::int64_t tab_column(Live& t, std::size_t at);

/// PRESS A PAINTED LAYOUT TAB. The band is the canvas's first row.
void press_tab(Live& t, std::size_t at);

/// ...AND ASK IT WHAT CAN BE DONE WITH IT.
void right_press_tab(Live& t, std::size_t at);

/// CHOOSE A CONTEXTUAL ROW BY ITS ACTION ID, driving the open surface with its own keys --
/// so a case names the operation it means and never a cursor index. A row inside a
/// presentation group is reached by descending into that group, exactly as a weaver reaches
/// it, and which group that is comes from the declaration table rather than from a guess.
bool choose_context_action(Live& t, const char* id);

/// OPEN THE RENAME EDITOR ON A TAB THE WAY A WEAVER DOES: two presses, close enough together to be
/// one gesture.
void open_rename_on_tab(Live& t, std::size_t at);

/// THE SAME GESTURE ON THE LIVE LAYOUT'S TAB, FOR ANY RIG that publishes raw input --
/// written against `publish` rather than against a workspace translation, because the pane
/// rig has no document room to speak in.
template <typename Rig>
inline void open_rename_on_live_tab(Rig& r) {
    const BandStatus band = band_status(r.session(), screen_of(r.session()));
    std::int64_t column = -1;
    for (const LayoutTab& tab : band.tabs) {
        if (tab.at == r.session().setup.active_at) {
            column = tab.column;
        }
    }
    REQUIRE(column >= 0);
    r.clock.together();
    // The band's row, the canvas's first, on the console row the medium puts it on; in a medium
    // that sets type, the middle of the tab's first character, in pixels.
    std::int64_t x = column, y = 0;
    std::int64_t space = input::space::kCells;
    const Screen sc = screen_of(r.session());
    if (sc.text_advance_px > 0) {
        const RegionFit fit = layouts_body(r.session(), sc).fit;
        x = fit.view.x + fit.origin_x + column * fit.advance_px + fit.advance_px / 2;
        y = fit.view.y + fit.origin_y + band_tab_row(r.session(), sc) * fit.line_px +
            fit.line_px / 2;
        space = input::space::kPixels;
    }
    for (int press = 0; press < 2; ++press) {
        r.hand_moment(input::PointerButton{1, true, x, y, space, input::mod::kNone});
        r.hand_moment(input::PointerButton{1, false, x, y, space, input::mod::kNone});
    }
    r.clock.apart();
}

/// RUN A LAYOUT ACT FROM THE CONTEXTUAL MENU ON THE LIVE LAYOUT'S OWN TAB -- the shipped route for
/// the acts no key answers -- entering `group` on the way when the act stands in one.
template <typename Rig>
inline void tab_menu_act(Rig& r, const std::string& act, const std::string& group = std::string()) {
    const BandStatus band = band_status(r.session(), screen_of(r.session()));
    std::int64_t column = -1;
    for (const LayoutTab& tab : band.tabs) {
        if (tab.at == r.session().setup.active_at) {
            column = tab.column;
        }
    }
    REQUIRE(column >= 0);
    r.hand_moment(input::PointerButton{3, true, column, 0,
                                                  input::space::kCells, input::mod::kNone});
    REQUIRE(r.session().context.open);
    REQUIRE(r.session().context.subject == context_subject::kLayout);
    for (int guard = 0; guard < 32 && r.session().context.open; ++guard) {
        const std::vector<ContextEntry> rows =
            context_population(r.session().context.subject, r.session().context.group);
        REQUIRE(r.session().context.cursor < rows.size());
        const ContextEntry& here = rows[r.session().context.cursor];
        if (here.is_group && !group.empty() && std::string(here.group) == group &&
            r.session().context.group != group) {
            r.key(input::scan::kReturn);
            continue;
        }
        if (!here.is_group && here.row != nullptr &&
            std::string(here.row->id) == act) {
            r.key(input::scan::kReturn);
            return;
        }
        r.key(input::scan::kDown);
    }
    REQUIRE_FALSE(r.session().context.open);
}
/// ...DUPLICATE IT, for the rigs whose cases mean "two layouts holding the same desk".
template <typename Rig> inline void duplicate_live_layout(Rig& r) {
    tab_menu_act(r, "layout.duplicate");
}

/// TYPE A WHOLE NAME INTO THE OPEN EDITOR AND COMMIT IT.
void type_name(Live& t, const std::string& name);

/// RENAME THE LIVE LAYOUT, THROUGH THE GESTURE A WEAVER USES -- a double-click on its own tab, then
/// the name. NO FILE IS WRITTEN by any part of this.
void rename_live_layout(Live& t, const std::string& name);

/// SAVE THE LIVE LAYOUT'S DESK TO ITS SETUP ARTIFACT -- `s`, and only that.
void save_setup(Live& t);

/// NAME THE LIVE LAYOUT AND WRITE IT TO THE SETUP FILE -- two gestures, because renaming is a
/// layout operation and saving a file operation: a case that means "a named desk that matches its
/// file" says both, and one that means only one of them says only that one.
void name_setup(Live& t, const std::string& name);

/// The identity row as a weaver reads it, off the canvas at the place the painter put it -- never
/// rebuilt here, so a case cannot pass while the screen says something else. That place is the
/// Layouts pane's first interior row, which at the developer default is Workshop's first row.
std::string setup_row(const DeskShown& c, const Screen& sc);

/// The workspace-extent fact as a weaver reads it -- the Layouts pane's SECOND row where the medium
/// fits one, folded into the identity row where it does not. It takes the SESSION, because how
/// many rows that surface has is a fact about a pane a weaver can resize, not about the screen.
std::string workspace_row(const DeskShown& c, const Session& s, const Screen& sc);

/// THE LAYOUTS PANE'S PUBLISHED REGION on a canvas, found the way the painter published it --
/// `layouts_body`'s own answer, so a case cannot pass while reading a region the painter did not
/// write. Not the top band's whole rectangle: the pane's interior is inset by one device unit on
/// a face that can draw one.
const ScreenRegion* layouts_region_on(const ScreenCanvas& c,
                                             const Session& s,
                                             const Screen& sc);

/// THE FIRST SHOWING OF THIS WORKSHOP: the window brought up, so Workshop starts on its offer and
/// draws its desk there -- a weave runs only on message, so a desk no window offered itself to
/// shows nothing at all.
DeskShown first_frame(Live& t);

/// The Hello fixture's identity, spelled as a STRANGER would have to spell it --
/// by string. Nothing here includes the fixture's source, because a provider is a
/// stranger to Workshop and the whole claim is that two strings are enough.
inline constexpr const char* kHelloRole = "zengine.test.workshop-hello";
inline constexpr const char* kHelloPane = "hello";

PaneRef hello_ref();

/// A SECOND ROLE, for the cases about two providers and one pane key.
inline constexpr const char* kOtherRole = "zengine.test.other-provider";

/// The nudge that makes a native seat speak at an instant a case chooses. It is
/// the seat's own private shape and no part of the pane protocol -- a case must
/// be able to make a real weave author a real sentence without that mechanism
/// being reachable by anything else.
struct SeatDo {
    ZEN_SHAPE(SeatDo, 1);
};

/// A TOOL THAT ASKS THIS HOST'S DOORS, in a role of its own: what the Files weave is, reduced
/// to the sentences a case needs. It asks `zengine.project` where this run began and to install or
/// author a catalog, and asks `zengine.workshop` to open one source in the Editor, recording every
/// answer -- the party each door was built for. It authors AS A ROLE, because both doors
/// refuse anonymous speech; a rig that asked personally would be proving the refusal rather than
/// the answer.
struct DoorAskerState {
    std::int64_t asks = 0;
    std::int64_t answers = 0;
    ZEN_EXPOSE();
    ZEN_SHAPE(DoorAskerState, 1, ZEN_FIELD(asks), ZEN_FIELD(answers));
};

class DoorAsker
    : public loom::WeaveBase<DoorAsker, DoorAskerState,
                             loom::Accept<ProjectRoot, ProjectFrontierSaid, PlanNames,
                                          RecipeOutcome, PlanRowWritten, SourceOpened,
                                          RecipeSourceSaid, SeatDo>,
                             loom::Emit<ProjectRootRequested, ProjectFrontierRequested,
                                        PlanNamesRequested, RecipeUseRequested,
                                        RecipeAuthorRequested, PlanRowRequested,
                                        OpenSourceRequested, RecipeSourceRequested,
                                        PaneRevealRequested, PaneQuitAnswered,
                                        // ...and the four
                                        // sentences a stranger might forge at the managed
                                        // opening's three parties, so the forgery cases prove
                                        // the parties' refusals and not the bus's.
                                        ManagedOpenSettled, zengine::workshop::v3::SourcePrepared, PresentationAdmitted,
                                        loom::DispatchRefused>> {
public:
    explicit DoorAsker(std::string role);

    /// What this asker should say next, run INSIDE its own delivery so `as_role` has a real
    /// authorship moment for Loom to verify.
    std::function<void(DoorAsker&, loom::Mail&)> next;

    /// The answers, as they arrived.
    std::vector<ProjectRoot> roots;
    std::vector<ProjectFrontierSaid> frontiers;
    std::vector<PlanNames> names;
    std::vector<RecipeOutcome> outcomes;
    std::vector<PlanRowWritten> rows;
    std::vector<SourceOpened> opens;
    std::vector<bool> opens_authentic; ///< beside `opens`: Loom's word that it answered our ask
    std::vector<RecipeSourceSaid> sources;
    /// Whether the last ask was authored as a role at all -- the canary lever for the
    /// rule every door keeps.
    bool personally = false;
    /// This asker's own id on the bus, so a case can nudge it the way `drive` nudges a seat.
    loom::WeaveId id{};

    void on(const SeatDo&, loom::Mail& mail);
    /// ONE MORE THING TO SAY, ON THE BEAT THE PROJECT DOOR ANSWERS: a statement TWO deliveries
    /// behind an ask made in the same handler. The bus is FIFO, so a case that queues an ask and a
    /// nudge together cannot land the nudge inside the ask's own conversation, and this is the
    /// shortest honest way to reach that instant with real messages (VM-FIX-24, one hop further).
    std::function<void(DoorAsker&, loom::Mail&)> then_root;

    void on(const ProjectRoot& said, loom::Mail& mail);
    void on(const ProjectFrontierSaid& said, loom::Mail&);
    void on(const PlanNames& said, loom::Mail&);
    void on(const RecipeOutcome& said, loom::Mail&);
    void on(const PlanRowWritten& said, loom::Mail&);
    void on(const SourceOpened& said, loom::Mail& mail);
    void on(const RecipeSourceSaid& said, loom::Mail&);

    template <class Shape>
    void ask(loom::Mail& mail, const char* role, const Shape& shape) {
        ++state_.asks;
        if (personally) {
            (void)mail.send_to_role(role, shape);
            return;
        }
        (void)mail.as_role(role_).send_to_role(role, shape);
    }

private:
    std::string role_;
};

/// The role an asker holds unless a case says otherwise. It is a TEST role and nothing
/// in the host names it: what the doors require is that an asker holds SOME role, so the
/// spelling is the case's to choose and the door's answer must not depend on it.
inline constexpr const char* kDoorAskerRole = "zengine.test.door-asker";

/// Seat an asker on this bus, granted exactly the eight asks and nothing else. It hears the
/// three answers the way any weave does -- through Loom's own answer route -- so nothing
/// here has to be granted for it to be told.
DoorAsker* mount_door_asker(Live& t, const char* role = kDoorAskerRole);

/// Make an asker say one sentence INSIDE ITS OWN DELIVERY -- `drive`'s reason exactly, and
/// the only way `mail.as_role(...)` has an authorship moment Loom can verify.
void asker_do(Live& t, DoorAsker* asker,
                     std::function<void(DoorAsker&, loom::Mail&)> what);

/// ASK THE EDITOR'S DOOR TO OPEN ONE PATH, and hand back what it answered: the whole of what a
/// Return on a source row in the Files pane crosses as (WL-EDIT-05's asker half). The door is at
/// `zengine.editor`, so a rig that holds no Editor weave gets no answer -- the ask is refused on
/// the tap, and this helper fails loudly rather than inventing one; a case about the document
/// goes to `test_workshop_panes_editor.cpp`, over the real image.
SourceOpened open_through_door(Live& t, DoorAsker* asker, const std::string& path);

/// ASK THE PROJECT DOOR WHICH FILE A RECIPE NAMES, and hand back what it answered. This is
/// the first of the two doors the Builder pane's `builder.edit-source` walks: the pane holds a
/// recipe name and never a path, the host's read-only role resolves it against the catalog
/// IT owns (`RecipeSourceSaid`), and the pane then asks the Editor's own door to open it.
RecipeSourceSaid edit_source_through_door(Live& t, DoorAsker* asker,
                                                 const std::string& recipe);

struct SeatState {
    std::int64_t said = 0;
    ZEN_SHAPE(SeatState, 1, ZEN_FIELD(said));
};

/// THE GUEST DOOR'S STAND-IN, for a case that writes a guest's row with no door listening: it holds
/// `zengine.guests` and answers `GuestRowOfRequested` as the door does -- to this host's own roles
/// alone -- with the row `rows` says, read at each ask.
class GuestRowsStandIn
    : public loom::WeaveBase<GuestRowsStandIn, SeatState, loom::Accept<GuestRowOfRequested>,
                             loom::Emit<GuestRowOf, loom::Refused>> {
public:
    explicit GuestRowsStandIn(std::function<scope::GuestRowFacts(loom::WeaveId)> rows)
        : rows_(std::move(rows)) {}
    void on(const GuestRowOfRequested& asked, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopProvider) && !mail.authored_from_role(kLayoutsRole)) {
            (void)mail.answer(loom::Refused{"a row is said to this host's own roles alone"});
            return;
        }
        const loom::WeaveId who{static_cast<std::uint64_t>(asked.session < 0 ? 0 : asked.session)};
        (void)mail.answer(scope::said_row(asked.session, rows_ ? rows_(who) : scope::GuestRowFacts{}));
    }

private:
    std::function<scope::GuestRowFacts(loom::WeaveId)> rows_;
};

/// Mount the stand-in on `bus`, answering from `rows`.
inline loom::WeaveId mount_guest_rows(loom::Switchboard& bus,
                                      std::function<scope::GuestRowFacts(loom::WeaveId)> rows) {
    auto door = std::make_unique<GuestRowsStandIn>(std::move(rows));
    GuestRowsStandIn* raw = door.get();
    loom::Grant answers;
    answers.allow_to_any(GuestRowOf::zen_name, GuestRowOf::zen_version);
    answers.allow_to_any(loom::Refused::zen_name, loom::Refused::zen_version);
    const loom::WeaveId id =
        bus.register_weave(std::move(door), std::move(answers), std::string(kGuestsRole));
    raw->zen_set_self(id);
    return id;
}

/// A PARTY THAT PUTS A PANE ON THE DESK, OR TAKES ONE OFF, THROUGH THE TWO DOORS -- the host's
/// `PaneLaunchRequested` and the layouts' owner's `PaneCloseRequested` -- what the desktop's Pane
/// Manager does, for the many cases that need a pane open or closed and are not about who asked or
/// with which key. It holds a role of its own, because the doors answer a role and only a
/// role; the cases about the Pane Manager itself drive the shipped desktop image.
class DoorHand
    : public loom::WeaveBase<DoorHand, SeatState,
                             loom::Accept<PaneLaunchAnswered, PaneCloseAnswered,
                                          PaneToggleAnswered, PaneSubjectActed, SeatDo>,
                             loom::Emit<PaneLaunchRequested, PaneCloseRequested,
                                        PaneToggleRequested, InspectPaneRequested,
                                        PaneCommitRequested>> {
public:
    void on(const PaneLaunchAnswered& a, loom::Mail&);
    void on(const PaneCloseAnswered& a, loom::Mail&);
    void on(const PaneToggleAnswered& a, loom::Mail&);
    /// ...AND, ASKED AS AN INSPECTOR (Info's path), what naming a subject or writing a row came to.
    void on(const PaneSubjectActed& a, loom::Mail&);
    void on(const SeatDo&, loom::Mail& mail);
    std::vector<PaneLaunchAnswered> launched;
    std::vector<PaneCloseAnswered> closed;
    std::vector<PaneToggleAnswered> toggled;
    std::vector<PaneSubjectActed> acted;
    std::function<void(DoorHand&, loom::Mail&)> next;
    static constexpr const char* kRole = "zengine.test.hand";
};

/// THE RIG'S HAND, mounted on first use in its own role.
template <class Rig>
inline DoorHand& door_hand(Rig& t) {
    if (t.hand == nullptr) {
        auto seat = std::make_unique<DoorHand>();
        DoorHand* raw = seat.get();
        loom::Grant grant;
        grant.allow_to_role(PaneLaunchRequested::zen_name, PaneLaunchRequested::zen_version,
                            kWorkshopProvider);
        grant.allow_to_role(PaneCloseRequested::zen_name, PaneCloseRequested::zen_version,
                            kLayoutsRole);
        grant.allow_to_role(PaneToggleRequested::zen_name, PaneToggleRequested::zen_version,
                            kWorkshopProvider);
        grant.allow_to_role(InspectPaneRequested::zen_name, InspectPaneRequested::zen_version,
                            kWorkshopProvider);
        grant.allow_to_role(InspectPaneRequested::zen_name, InspectPaneRequested::zen_version,
                            kInfoPaneProvider);
        grant.allow_to_role(PaneCommitRequested::zen_name, PaneCommitRequested::zen_version,
                            kWorkshopProvider);
        // ...AND A SETTING'S COMMIT, as Info sends one, to the layouts' owner.
        grant.allow_to_role(PaneSettingCommitRequested::zen_name,
                            PaneSettingCommitRequested::zen_version, kLayoutsRole);
        t.hand_id =
            t.bus.register_weave(std::move(seat), std::move(grant), std::string(DoorHand::kRole));
        raw->zen_set_self(t.hand_id);
        t.hand = raw;
    }
    return *t.hand;
}

/// A PANE A CASE OPENS, STOOD IN THE COLUMN: an open with the keys places a pane the layout never
/// had at the room's centre, so a case that is not about placement has the layout
/// hold a hidden row for it at the code's place and size first, past the doors, and the open
/// shows it there.
template <class Rig>
inline void left_in_column(Rig& t, const PaneRef& ref) {
    stage_run(t, [&ref](SetupState& run) {
        if (!holds_pane(run.active, ref)) {
            (void)add_pane(run.active, ref);
            pane_of(run.active, ref)->fold = pane_fold::kHidden;
        }
    });
}

/// ASK THE HOST TO OPEN OR FOCUS THIS PANE, as a role, inside the hand's own delivery.
template <class Rig>
inline PaneLaunchAnswered hand_launch(Rig& t, const PaneRef& ref) {
    DoorHand& h = door_hand(t);
    const std::size_t before = h.launched.size();
    h.next = [ref](DoorHand&, loom::Mail& m) {
        (void)m.as_role(DoorHand::kRole)
            .send_to_role(kWorkshopProvider, PaneLaunchRequested{ref.provider, ref.pane});
    };
    (void)t.bus.send(t.hand_id, loom::Message(loom::to_value(SeatDo{}), loom::WeaveId{},
                                              loom::WeaveId{}, 0));
    t.bus.drain_until_idle();
    REQUIRE_MESSAGE(h.launched.size() == before + 1, "the launch door did not answer");
    return h.launched.back();
}

/// ...AND TO TAKE IT OFF THE DESK, UNLOADING NOTHING: asked of the layouts' owner.
template <class Rig>
inline PaneCloseAnswered hand_close(Rig& t, const PaneRef& ref) {
    DoorHand& h = door_hand(t);
    const std::size_t before = h.closed.size();
    h.next = [ref](DoorHand&, loom::Mail& m) {
        (void)m.as_role(DoorHand::kRole)
            .send_to_role(kLayoutsRole, PaneCloseRequested{ref.provider, ref.pane});
    };
    (void)t.bus.send(t.hand_id, loom::Message(loom::to_value(SeatDo{}), loom::WeaveId{},
                                              loom::WeaveId{}, 0));
    t.bus.drain_until_idle();
    REQUIRE_MESSAGE(h.closed.size() == before + 1, "the close door did not answer");
    return h.closed.back();
}

/// ...OR TOGGLE IT, as the desktop's Pane Manager does: hidden if the desk names it, shown if not.
template <class Rig>
inline PaneToggleAnswered hand_toggle(Rig& t, const PaneRef& ref) {
    DoorHand& h = door_hand(t);
    const std::size_t before = h.toggled.size();
    h.next = [ref](DoorHand&, loom::Mail& m) {
        (void)m.as_role(DoorHand::kRole)
            .send_to_role(kWorkshopProvider, PaneToggleRequested{ref.provider, ref.pane});
    };
    (void)t.bus.send(t.hand_id, loom::Message(loom::to_value(SeatDo{}), loom::WeaveId{},
                                              loom::WeaveId{}, 0));
    t.bus.drain_until_idle();
    REQUIRE_MESSAGE(h.toggled.size() == before + 1, "the toggle door did not answer");
    return h.toggled.back();
}

/// NAME A PANE AS THE INSPECTED SUBJECT THROUGH THE HOST'S DOOR, as an inspector role asks it
/// (Info's own path, WL-INFO-14), and answer what the door said.
template <class Rig>
inline PaneSubjectActed hand_inspect(Rig& t, const PaneRef& ref) {
    DoorHand& h = door_hand(t);
    const std::size_t before = h.acted.size();
    h.next = [ref](DoorHand&, loom::Mail& m) {
        (void)m.as_role(DoorHand::kRole)
            .send_to_role(kWorkshopProvider, InspectPaneRequested{ref.provider, ref.pane});
    };
    (void)t.bus.send(t.hand_id, loom::Message(loom::to_value(SeatDo{}), loom::WeaveId{},
                                              loom::WeaveId{}, 0));
    t.bus.drain_until_idle();
    REQUIRE_MESSAGE(h.acted.size() == before + 1, "the inspection door did not answer");
    return h.acted.back();
}

/// ...OR ASK INFO TO INSPECT IT, as the Pane Manager's `inspect in Info` row does: Info's own
/// inspect, which a live draft there holds back, answered with what came of it.
template <class Rig>
inline PaneSubjectActed hand_inspect_in_info(Rig& t, const PaneRef& ref) {
    DoorHand& h = door_hand(t);
    const std::size_t before = h.acted.size();
    h.next = [ref](DoorHand&, loom::Mail& m) {
        (void)m.as_role(DoorHand::kRole)
            .send_to_role(kInfoPaneProvider, InspectPaneRequested{ref.provider, ref.pane});
    };
    (void)t.bus.send(t.hand_id, loom::Message(loom::to_value(SeatDo{}), loom::WeaveId{},
                                              loom::WeaveId{}, 0));
    t.bus.drain_until_idle();
    REQUIRE_MESSAGE(h.acted.size() == before + 1, "Info did not answer the inspect it was asked");
    return h.acted.back();
}

/// WHERE THE INSPECTED PANE'S ROW WITH THIS LABEL IS -- the first such after the section named
/// `after` (the start when empty), so a weaver pane's region `X` is told from its AUTHORED `X`.
std::size_t subject_row_index(const Session& s, const std::string& label,
                                     const std::string& after = std::string());

/// ...ITS ROW, AND ITS VALUE READ NOW.
const Row* subject_row(const Session& s, const std::string& label,
                              const std::string& after = std::string());
std::string subject_value(const Session& s, const std::string& label,
                                 const std::string& after = std::string());

/// WRITE THE INSPECTED PANE'S ROW WITH THIS LABEL THROUGH THE COMMIT DOOR -- the subject's name as
/// the host gave it, the row's index and the text, exactly as Info sends a finished draft -- and
/// answer what the door said.
template <class Rig>
inline PaneSubjectActed hand_commit(Rig& t, const std::string& label, const std::string& text,
                                    const std::string& after = std::string()) {
    const std::size_t at = subject_row_index(t.session(), label, after);
    REQUIRE_MESSAGE(at < t.session().inspected.rows.size(),
                    "the inspected pane has no row labelled ", label);
    const std::int64_t subject = t.session().inspected.name;
    DoorHand& h = door_hand(t);
    const std::size_t before = h.acted.size();
    h.next = [subject, at, text](DoorHand&, loom::Mail& m) {
        (void)m.as_role(DoorHand::kRole)
            .send_to_role(kWorkshopProvider,
                          PaneCommitRequested{subject, static_cast<std::int64_t>(at), text});
    };
    (void)t.bus.send(t.hand_id, loom::Message(loom::to_value(SeatDo{}), loom::WeaveId{},
                                              loom::WeaveId{}, 0));
    t.bus.drain_until_idle();
    REQUIRE_MESSAGE(h.acted.size() == before + 1, "the commit door did not answer");
    return h.acted.back();
}

/// THE ROW OF `ref` AS THE LAYOUTS' OWNER READS IT FOR INFO (`PaneSettingsRequested`), and Info's
/// SETTINGS rows over it, composed as Info composes them (`info/settings_rows.hpp`).
template <class Rig>
inline PaneSettingsAnswered info_read(Rig& t, const PaneRef& ref) {
    t.owner->read_host_now(bus_participants(t.bus));
    return t.owner->store().read_pane(std::string(), PaneSettingsRequested{ref.provider, ref.pane});
}
template <class Rig>
inline std::vector<::zengine::info_pane::SettingsRow> info_rows(Rig& t, const PaneRef& ref) {
    return ::zengine::info_pane::settings_rows::rows_of(info_read(t, ref));
}
/// ...ITS KEYS IN INFO'S ORDER, WHETHER ONE IS SHOWN, AND ONE'S VALUE AS INFO SAYS IT.
template <class Rig>
inline std::vector<std::string> info_labels(Rig& t, const PaneRef& ref) {
    std::vector<std::string> out;
    for (const ::zengine::info_pane::SettingsRow& row : info_rows(t, ref)) {
        out.push_back(row.key);
    }
    return out;
}
template <class Rig> inline bool info_has(Rig& t, const PaneRef& ref, const std::string& key) {
    const std::vector<std::string> keys = info_labels(t, ref);
    return std::find(keys.begin(), keys.end(), key) != keys.end();
}
template <class Rig>
inline std::string info_value(Rig& t, const PaneRef& ref, const std::string& key) {
    for (const ::zengine::info_pane::SettingsRow& row : info_rows(t, ref)) {
        if (row.key == key) {
            return row.value;
        }
    }
    FAIL("Info shows no SETTINGS row " << key);
    return std::string();
}
/// TEXT TYPED ON ONE OF INFO'S SETTINGS ROWS: refused in Info's words, `<key>: <why>`, or the
/// commit Info sends, judged at the owner's door, its picture presented to Workshop after it.
template <class Rig>
inline PaneSubjectActed info_commit(Rig& t, const PaneRef& ref, const std::string& key,
                                    const std::string& text) {
    const PaneSettingsAnswered read = info_read(t, ref);
    for (const ::zengine::info_pane::SettingsRow& row : ::zengine::info_pane::settings_rows::rows_of(read)) {
        if (row.key != key) {
            continue;
        }
        const ::zengine::info_pane::settings_rows::Typed typed =
            ::zengine::info_pane::settings_rows::commit_of(read, row, text);
        if (!typed.commit) {
            return PaneSubjectActed{false, key + ": " + typed.refusal};
        }
        const PaneSettingCommitAnswered said =
            t.owner->store().commit_pane(std::string(), *typed.commit);
        t.nudge_owner();
        t.bus.drain_until_idle();
        return PaneSubjectActed{said.accepted, said.accepted ? std::string() : key + ": " + said.refusal};
    }
    FAIL("Info shows no SETTINGS row " << key);
    return PaneSubjectActed{};
}

/// KEEP A PANE'S SETTINGS IN THE LIVE LAYOUT'S ROW AS A HAND-WRITTEN DESK KEEPS THEM -- in key
/// order and judged by form alone (WL-SETTING-02), so a value its setting does not take and a key
/// no declaration names stand as written -- and hand them to the pane's holder at the frame a key
/// nothing answers redraws (WL-SETTING-04).
template <class Rig>
inline void keep_settings(Rig& t, const PaneRef& ref, std::vector<PaneSetting> settings) {
    std::sort(settings.begin(), settings.end(),
              [](const PaneSetting& a, const PaneSetting& b) { return a.key < b.key; });
    const Written legal = check_pane_settings(ref, settings);
    REQUIRE_MESSAGE(legal.accepted, legal.refusal);
    stage_desk(t, [&](Setup& desk) {
        SetupPane* row = pane_of(desk, ref);
        REQUIRE_MESSAGE(row != nullptr, "the live layout keeps no row for ", ref_text(ref));
        row->settings = std::move(settings);
    });
    t.key(input::scan::kUnknown);
}

/// THE LAYOUTS' OWNER'S STORE AS A DELIVERY WOULD READ IT NOW: its host's readings, and who is on
/// the bus as Loom's participants claim says it off the rig's bus, which a rig reading the store
/// between messages hands it.
template <class Rig>
inline const LayoutStore& layouts_now(Rig& r) {
    r.owner->read_host_now(bus_participants(r.bus));
    return r.owner->layouts();
}

/// WHAT A PANE'S COUNTED DECLARATION TAKES, as the layouts' store counts it; none for none.
template <class Rig>
inline std::vector<zengine::pane::v2::PaneSettingRow> declared_settings(Rig& t, const PaneRef& ref) {
    const RuntimePane* row = t.session().panes.runtime.find(ref.provider, ref.pane);
    REQUIRE_MESSAGE(row != nullptr, "nothing offers ", ref_text(ref));
    const std::vector<zengine::pane::v2::PaneSettingRow>* counted = layouts_now(t).counted(ref);
    return counted != nullptr ? *counted : std::vector<zengine::pane::v2::PaneSettingRow>{};
}

/// ...AND WHAT ITS HOLDER SAID IT CANNOT USE, each `<key>: <why>`, as the layouts' owner keeps it,
/// only against the revision the holder was last handed (WL-SETTING-08).
template <class Rig>
inline std::vector<std::string> unused_settings(Rig& t, const PaneRef& ref) {
    const RuntimePane* row = t.session().panes.runtime.find(ref.provider, ref.pane);
    REQUIRE_MESSAGE(row != nullptr, "nothing offers ", ref_text(ref));
    std::vector<std::string> out;
    for (const RowSettingUnused& u : layouts_now(t).unused_of(ref)) {
        out.push_back(u.key + ": " + u.why);
    }
    return out;
}

/// ...OR QUEUE THE CLOSE WITHOUT DELIVERING ANYTHING -- role-authored as the hand, behind
/// whatever the bus already holds -- for the cases about what a removal in the same poll does to
/// a flight already under way. The next pump delivers it in order.
template <class Rig>
inline void enqueue_close(Rig& t, const PaneRef& ref) {
    (void)door_hand(t);
    // ROLE-ADDRESSED, as the hand's grant is: a send to the weave's id would be refused at
    // delivery, silently to this case.
    const loom::Ticket queued = t.bus.role_send_to_role_as(
        t.hand_id, DoorHand::kRole, kLayoutsRole,
        loom::Message(loom::to_value(PaneCloseRequested{ref.provider, ref.pane}), t.hand_id,
                      t.hand_id, 0));
    REQUIRE(queued.valid());
}

/// PUT A PANE ON THE DESK AS SCAFFOLDING: the launch door, then the selection and the keys put
/// back where they were, with a fresh frame. A launch selects and focuses what it opens
/// (WL-DESK-03); the many cases that open a pane only to arrange, persist or paint it do not mean
/// that, and the cases that do drive the desktop's Pane Manager or the door directly. The frame
/// is redrawn by a key nothing answers (`kUnknown`), so a case reads the desk as it now stands.
template <class Rig>
inline PaneLaunchAnswered seat_pane(Rig& t, const PaneRef& ref) {
    Session& s = const_cast<Session&>(t.session());
    const std::int64_t selected = s.panes.selected;
    const std::int64_t keyboard = s.panes.keyboard;
    if (!has_pane(t.session().setup.active, ref)) {
        left_in_column(t, ref);
    }
    const PaneLaunchAnswered said = hand_launch(t, ref);
    s.panes.selected = selected;
    s.panes.keyboard = keyboard;
    t.key(input::scan::kUnknown);
    return said;
}

/// ONE TOGGLE OVER THE TWO DOORS: close the pane if the live desk names it, seat it if not.
template <class Rig>
inline void toggle_pane(Rig& t, const PaneRef& ref) {
    if (has_pane(t.session().setup.active, ref)) {
        (void)hand_close(t, ref);
    } else {
        (void)seat_pane(t, ref);
    }
}

void pick(Live& t, std::int64_t kind);

void open_stock_pane(Live& t);

/// THE ROWS A SEAT DRAWS for one of its panes, and the parts of them it names.
struct SeatRows {
    std::string pane;
    std::vector<TextRow> rows;
    std::vector<RowPart> parts;
    SeatRows() = default;
    SeatRows(std::string p, std::vector<TextRow> r, std::vector<RowPart> named = {});
};

/// A ROOM A WINDOW GAVE ONE OF A SEAT'S AREAS: the pane it draws there and the rows and columns of
/// the window's text the room holds.
struct SeatRoom {
    std::string pane;
    std::string area;
    std::int64_t rows = 0, columns = 0;
};

/// A PRESS THE WINDOW GAVE A SEAT'S AREA, read onto the rows last drawn there: the pane, the button,
/// the cell, and whether the keys were already the pane's as Workshop last said (`PaneKeys`).
struct SeatPress {
    std::string pane;
    std::int64_t button = 1;
    std::int64_t row = 0, column = 0;
    bool shown = true;
    bool keys_went_here = false;
};

/// THE WHEEL THE WINDOW GAVE A SEAT'S AREA.
struct SeatWheel {
    std::string pane;
    double dy = 0;
};

/// A NATIVE PROVIDER SEAT: a weave that holds a role, can be made to say anything, deliberately
/// or personally, and draws its rows in each pane's area in the window that shows it,
/// `<role>.<pane>`. The `.so` fixture proves
/// the real ABI and load path; this proves the AUTHORITY cases, which need the exact wrong sentence
/// at the exact wrong moment, as no shipped fixture could be talked into.
class ProviderSeat
    : public loom::WeaveBase<ProviderSeat, SeatState,
                             loom::Accept<PaneCatalogRequested, PaneKey, PaneTextInput,
                                          PaneActionRequested, PaneKeys, gfx::AreaRoom,
                                          gfx::DrawnRefused, gfx::AreaButtonChanged,
                                          gfx::AreaWheelTurned, PaneCanvasValueDrop, SeatDo>,
                             loom::Emit<PaneOffered, PaneActions, zengine::pane::v2::PaneActions,
                                        zengine::pane::v3::PaneActions,
                                        zengine::pane::v4::PaneActions, PaneEscapeUnspent, PanePassRequested,
                                        gfx::Drawn,
                                        gfx::AreaButtonChanged>> {
public:
    explicit ProviderSeat(std::string role);

    /// A RESOLVED ACTION THIS SEAT WAS ASKED FOR (WL-KEY-15), recorded and never
    /// interpreted, for the key's reason exactly: a case asserts what WORKSHOP resolved.
    void on(const PaneActionRequested& a, loom::Mail& mail);

    void on(const PaneCatalogRequested&, loom::Mail& mail);
    /// THE ROOM A WINDOW GIVES ONE OF ITS AREAS, recorded with its author, and the rows last said
    /// for that pane drawn there whole. A room another role gives draws nothing.
    void on(const gfx::AreaRoom& r, loom::Mail& mail);
    void on(const gfx::DrawnRefused& refused, loom::Mail& mail);
    /// A PRESS THE WINDOW GAVE ONE OF ITS AREAS, and its author beside it, read onto the rows last
    /// drawn there. The seat interprets nothing -- it is a recorder, so a case can assert exactly
    /// what the window said -- except that a third-button press is handed back, as a pane hands
    /// back a press that means nothing to it, so Workshop's own pane menu opens where it was made.
    void on(const gfx::AreaButtonChanged& b, loom::Mail& mail);
    void on(const gfx::AreaWheelTurned& w, loom::Mail& mail);
    /// A VALUE PLACED ON ONE OF ITS AREAS, kept as Workshop said it.
    void on(const PaneCanvasValueDrop& d, loom::Mail&);
    std::vector<PaneCanvasValueDrop> value_drops;
    /// Whether the keys reach the pane now, as Workshop last said.
    void on(const PaneKeys& k, loom::Mail& mail);
    /// A KEY AND THE TEXT IT PRODUCED, recorded and never interpreted, for the press's reason
    /// exactly: a case asserts what WORKSHOP said, and a seat that made something of one would be
    /// asserting a provider's opinion instead.
    void on(const PaneKey& k, loom::Mail& mail);
    void on(const PaneTextInput& t, loom::Mail& mail);
    void on(const SeatDo&, loom::Mail& mail);

    /// Offer, deliberately AS this role -- the ordinary, correct spelling.
    void offer(loom::Mail& mail, const PaneOffered& o);
    void offer(loom::Mail& mail, const zengine::pane::v2::PaneOffered& o);
    /// Offer PERSONALLY, from the very weave that holds the role. Holding is not
    /// speaking-for (MSG-07), and this is the sentence that says so.
    void offer_personally(loom::Mail& mail, const PaneOffered& o);
    /// THE ROWS OF ONE OF ITS PANES, drawn in that pane's area while a window shows it, and kept,
    /// so a room given later draws them there.
    void say(loom::Mail& mail, const SeatRows& c);
    /// ...and where its caret and selection stand in them.
    void caret(loom::Mail& mail, const std::string& pane, const RowsCaret& c);
    /// THE ESCAPE THIS SEAT WAS SENT WAS UNSPENT HERE -- said as the role, and personally for
    /// the case about authorship rather than about the gesture. Echoed under the number the
    /// LATEST Escape arrived on, which is what a pane answering the key it was just handed
    /// does; `unspent_answering` is for a case that answers an older one on purpose, and
    /// `unspent_anonymously` for one that echoes nothing at all.
    void unspent(loom::Mail& mail, const std::string& pane);
    void unspent_answering(loom::Mail& mail, const std::string& pane, std::uint64_t answering);
    void unspent_anonymously(loom::Mail& mail, const std::string& pane);
    void unspent_personally(loom::Mail& mail, const std::string& pane);
    /// THE NUMBER THE LAST ESCAPE THIS SEAT WAS SENT CAME UNDER, or zero if it was sent none.
    std::uint64_t latest_escape_ask() const;
    /// Declare a pane's actions, deliberately AS this role -- and personally, from the
    /// very weave that holds it, which Workshop must drop for the offer's reason.
    void declare(loom::Mail& mail, const PaneActions& a);
    void declare_personally(loom::Mail& mail, const PaneActions& a);
    /// The SECOND published version of the declaration -- the one that can name an action this
    /// pane owns. A provider built before it existed sends the shape above.
    void declare_v2(loom::Mail& mail, const zengine::pane::v2::PaneActions& a);
    /// ...and the THIRD, which says what each press may cause and the modes.
    void declare_v3(loom::Mail& mail, const zengine::pane::v3::PaneActions& a);
    /// ...and the FOURTH, whose rows may name a key for a POSIX terminal.
    void declare_v4(loom::Mail& mail, const zengine::pane::v4::PaneActions& a);
    /// FORGE A PRESS AT SOMEBODY ELSE'S AREA -- deliberately authored, and deliberately by a
    /// role that is no window's: the sentence a pane must refuse, a stranger telling it a weaver
    /// clicked in its area.
    void press_at(loom::Mail& mail, const char* role, const gfx::AreaButtonChanged& b);

    /// The area one of its panes draws in.
    std::string area_for(const std::string& pane) const;
    /// The face the window last set that pane's area in, a cell before one.
    gfx::TextMeasure room_text(const std::string& pane) const;
    /// The rows and columns the room last given that pane's area holds, 0 by 0 before one.
    std::int64_t room_rows(const std::string& pane) const;
    std::int64_t room_columns(const std::string& pane) const;

    /// EVERYTHING THIS SEAT WAS EVER TOLD, counted. It is what lets a case say
    /// "closing a presentation reached the provider not at all" as a number rather
    /// than as an absence somebody has to trust.
    std::int64_t said = 0;
    std::vector<SeatRoom> rooms;
    std::vector<std::string> room_authors;
    std::vector<std::string> refusals;
    std::vector<std::string> asks;
    /// Each primary press on a row the pane shows, as a pane reads a press on its rows; and every
    /// press the window gave the seat's areas, any button, anywhere in them.
    std::vector<SeatPress> presses;
    std::vector<SeatPress> any_presses;
    std::vector<std::string> press_authors;
    std::vector<PaneKey> keys;
    std::vector<std::string> key_authors;
    /// THE CORRELATION EACH KEY ARRIVED UNDER, beside `keys` index for index. Workshop stamps
    /// one on an Escape and nothing else, and an answer that does not echo it is about no
    /// Escape at all.
    std::vector<std::uint64_t> key_asks;
    std::vector<PaneTextInput> typed;
    std::vector<std::string> text_authors;
    std::vector<SeatWheel> wheels;
    std::vector<std::string> wheel_authors;
    std::vector<PaneActionRequested> actions;
    std::vector<std::string> action_authors;
    /// Whether a third-button press is handed back for Workshop's own pane menu.
    bool pass_right = true;
    std::function<void(ProviderSeat&, loom::Mail&)> next;

private:
    void offered(const std::string& pane);
    /// The pane an area of its own shows: `<role>.<pane>`.
    std::string pane_of(const std::string& area) const;
    WindowDrawing& drawing_of(const std::string& area);
    std::int64_t columns_of(const std::string& area) const;
    /// The rows last said for `pane`, drawn in its area as many as its room holds, the caret in
    /// them: what changed there, or the whole after a new room.
    void draw(loom::Mail& mail, const std::string& pane);

    std::string role_;
    std::vector<std::string> offered_;
    std::map<std::string, WindowDrawing> drawings_;
    std::map<std::string, SeatRows> said_rows_;
    std::map<std::string, RowsCaret> carets_;
    std::map<std::string, std::int64_t> drawn_rows_;
    bool keys_here_ = false;
};

/// A NATIVE SEAT THAT TAKES ITS SETTINGS: it holds a role, offers and declares as it is told,
/// asks the layouts' owner for its row whenever it offers, as an image that arrives does, and
/// records in order every room a window gives its area and every hand-off sent it, with the role
/// each was authored as. Apart from `ProviderSeat`, whose cases count every delivery and are owed
/// no hand-off.
class SettingsSeat
    : public loom::WeaveBase<SettingsSeat, SeatState,
                             loom::Accept<PaneCatalogRequested, gfx::AreaRoom, PaneSettings, SeatDo>,
                             loom::Emit<PaneOffered, PaneSettingsDeclared, PaneSettingsAsked,
                                        PaneSettingUnused, gfx::RoomAsked>> {
public:
    explicit SettingsSeat(std::string role);

    void on(const PaneCatalogRequested&, loom::Mail&);
    void on(const gfx::AreaRoom& r, loom::Mail& mail);
    void on(const PaneSettings& s, loom::Mail& mail);
    void on(const SeatDo&, loom::Mail& mail);

    /// THE OFFER, its row asked of the layouts' owner, and the room of an area it was given before
    /// asked again, as an image that arrives holding nothing asks them.
    void offer(loom::Mail& mail, const PaneOffered& o);
    void declare(loom::Mail& mail, const PaneSettingsDeclared& d);
    /// A SETTING IT WAS HANDED AND CANNOT USE, said to the layouts' owner as its role.
    void unused(loom::Mail& mail, const PaneSettingUnused& u);
    /// ...and PERSONALLY, from the weave that holds the role, which declares nothing.
    void declare_personally(loom::Mail& mail, const PaneSettingsDeclared& d);

    std::function<void(SettingsSeat&, loom::Mail&)> next;
    std::vector<std::string> heard;    ///< "room" or "settings", in the order they arrived
    std::vector<SeatRoom> rooms;
    std::vector<PaneSettings> handed;
    std::vector<std::string> authors;  ///< the role each was authored as, in the same order
    int asks = 0;

private:
    std::string role_;
};

/// A WEAVE THAT HOLDS `zengine.workshop` AND IS NOT WORKSHOP -- the instrument for a PROVIDER's
/// own checks on what it offers and on who asks for its catalog, whose refusals are invisible from
/// Workshop's side. Holding the role lets this ask for the catalog as Workshop, and also
/// PERSONALLY -- the sentence a weave that merely held the role would produce with
/// `send_to_role`. Two spellings, one holder, opposite outcomes.
class PaneWatcher
    : public loom::WeaveBase<PaneWatcher, SeatState,
                             loom::Accept<PaneOffered, zengine::pane::v2::PaneOffered, SeatDo>,
                             loom::Emit<PaneCatalogRequested, PaneKeys, gfx::AreaButtonChanged>> {
public:
    void on(const zengine::pane::v2::PaneOffered& o, loom::Mail& mail);
    void on(const PaneOffered& o, loom::Mail& mail);
    void on(const SeatDo&, loom::Mail& mail);
    void ask(loom::Mail& mail);
    void ask_personally(loom::Mail& mail);

    std::vector<PaneOffered> offers;
    std::vector<std::string> offer_authors;
    std::function<void(PaneWatcher&, loom::Mail&)> next;
};

/// The rig's booter's state: a name of its own, so it loads beside a realization owner's
/// `BootState`.
struct RigBootState {
    std::int64_t n = 0;
    ZEN_SHAPE(RigBootState, 1, ZEN_FIELD(n));
};

/// The weave that commands the Weave Manager and HEARS ITS ANSWERS -- the host's own boot shape,
/// because a load whose refusal is addressed to nobody looks exactly like a load that worked. It
/// hears `zen.Ack` too: the control door answers an unload with an Ack rather than a Result, and
/// an answer nobody accepts is that same silence.
class Booter : public loom::WeaveBase<Booter, RigBootState,
                                      loom::Accept<loom::Result, loom::Ack, loom::Refused>,
                                      loom::Emit<loom::LoadWeave, loom::UnloadLibrary>> {
public:
    Booter(std::vector<std::string>& ok, std::vector<std::string>& no);
    void on(const loom::Result& r, loom::Mail&);
    void on(const loom::Ack&, loom::Mail&);
    void on(const loom::Refused& r, loom::Mail&);

    std::int64_t acks = 0;

private:
    std::vector<std::string>* ok_;
    std::vector<std::string>* no_;
};

/// A SEAT AT THE CONTROL DOOR FOR A RELOAD OR AN
/// UNLOAD, with a state shape of its own (see `PaneRig::enqueue_reload`).
struct ControlSeatState {
    std::int64_t n = 0;
    ZEN_SHAPE(ControlSeatState, 1, ZEN_FIELD(n));
};

class ControlSeat
    : public loom::WeaveBase<ControlSeat, ControlSeatState,
                             loom::Accept<loom::Result, loom::Ack, loom::Refused>,
                             loom::Emit<loom::ReloadLibrary, loom::UnloadLibrary>> {
public:
    ControlSeat(std::vector<std::string>& ok, std::vector<std::string>& no);
    void on(const loom::Result& r, loom::Mail&);
    void on(const loom::Ack&, loom::Mail&);
    void on(const loom::Refused& r, loom::Mail&);

private:
    std::vector<std::string>* ok_;
    std::vector<std::string>* no_;
};

/// A live Workshop that can be handed providers -- native ones always, and the real dynamic Hello
/// when a case asks for it. The mount order is the case's to choose, which is why this is its own
/// rig and not a flag on `Live`: the discovery claim is that neither load order loses an offer,
/// and a rig that always mounted Workshop first could prove only one of the two.
struct PaneRig {
    /// The declaration order is the host's lifetime claim, destruction running in reverse: the
    /// owners the host's own Sources read come first, because each Source's native body closes
    /// over a reference to one and reads it at the sample (`workshop.cpp` declares them above its
    /// own `op::Catalog`); the catalog precedes the Kernel, so every artifact goes down before the
    /// store its contributions live in. The owners are ordinary and mutable: a case moving one
    /// between two samples proves a sample is an evaluation, not a cached answer.
    std::string project_anchor = "/zen/pane-rig";
    CurrentRecipes host_recipes;

    loom::Switchboard bus;
    op::Catalog catalog;
    op::OperatorHostSurface operator_host{catalog};
    loom::Kernel kernel{bus, loom::trust_every_artifact(
                                 "Zengine's test harness: it loads only this build tree's output")};
    loom::WeaveId control = loom::mount_control(kernel, bus);
    loom::WeaveId manager = loom::mount_manager(control, bus);
    /// THE WINDOW THE DESK IS SHOWN IN, and the directory beside it (rig_windows.hpp), as `Live`'s:
    /// brought up when a case starts the desk or gives it a room, or at its first gesture or reading
    /// once Workshop is mounted; Workshop starts on its offer.
    zengine::testing::rig_windows::Windows windows{bus, manager, GRAPHICS_SO};
    /// WHAT THE WINDOWS ON THIS BUS TOOK, mirrored, which a held reading reads while this rig lives.
    DrawnMirror drawn{bus};
    HeldDeskEntry held_entry{drawn, [this]() -> const Session& { return session(); }};
    /// False: this case brings its own directory or window, and the rig's never comes up.
    bool in_window = true;
    InteractionClock clock;
    HostContext host;
    /// HOW MANY DRAWINGS OF ITS DESK WORKSHOP SENT the window (`count_desk_drawings`) -- a
    /// repaint that moved the desk's count, never a desk to read.
    std::size_t pictures = 0;
    /// EVERY `StandingConditions` THIS WORKSHOP HAS SAID, in order -- so a case can ask how
    /// MANY times it spoke and not only what it last said.
    std::vector<StandingConditions> said_conditions;
    /// ...and every `TranscriptShown` this host published, for the same reason.
    std::vector<TranscriptShown> said_transcripts;
    /// ...and every `PaneSubjectShown` -- an inspector's pane subject (WL-INFO-14) -- in order.
    std::vector<PaneSubjectShown> said_subjects;
    /// ...and the hand that opens and closes panes through the host's doors (`door_hand`).
    DoorHand* hand = nullptr;
    loom::WeaveId hand_id{};
    WorkshopWeave* w = nullptr;
    loom::WeaveId workshop_id{};
    /// Loom's participants claimant, which Workshop reads who is on the bus from.
    loom::WeaveId participants_id{};
    /// The host's session door, which Workshop asks to end the host's lifetime once its quit is
    /// settled; here it stops nothing, and says whether it was asked.
    HostSessionDoor* session_door = nullptr;
    /// THE HOST'S WATCH OVER THE QUIT'S DELIVERIES, mounted with Workshop exactly as
    /// `workshop.cpp` mounts it (WL-SESSION-19) -- a rig without it would be a Workshop whose
    /// quit waits on questions Loom refused to deliver, which no shipped host is. Declared after
    /// the bus and the `HostContext` whose book it writes, so it is removed first.
    std::unique_ptr<QuitDeliveryWatch> quit_watch;
    std::vector<std::string> loaded;
    std::vector<std::string> load_refusals;

    /// The rig's ears on what Workshop publishes (`Painter`).
    loom::WeaveId painter_id{};

    /// THE LAYOUTS' OWNER, mounted in its own role before Workshop, as the host mounts it, and the
    /// rig's stage hand, mounted when a case first stages a run past the doors.
    LayoutsOwner* owner = nullptr;
    loom::WeaveId owner_id{};
    loom::WeaveId stage_id{};
    void mount_owner();
    /// ...AND THE OWNER ASKED SOMETHING, so the picture it owes is sent: delivered at the next drain.
    void nudge_owner();

    PaneRig();


    /// MOUNT WORKSHOP THE WAY THE HOST DOES: in the `zengine.workshop` role, with the host's own
    /// compiled grant, not one minted from the Emit set -- `emit_default_grant` gives `to_any` for
    /// everything declared, wider than the host writes, and the two Builder sentences are
    /// ROLE-SCOPED. Independent denial witnesses check the policy acquires no unintended authority.
    WorkshopWeave* mount_workshop();

    /// Use the native host's policy; policy denials are also exercised independently.
    static loom::Grant workshop_grant();

    /// REGISTER WORKSHOP'S WEAVE IN ITS ROLE, with the production grant and the quit's watch.
    void register_workshop(std::unique_ptr<loom::Weave> weave);

    /// TAKE WORKSHOP'S WEAVE OFF THIS BUS FOR AN INTERVAL, AND PUT IT BACK. The weave itself --
    /// its document, its session, the panes it seated -- is the same object throughout; what leaves
    /// with its record is its role and the accept-set that declared its doors, so for the interval
    /// a shape only Workshop accepts is one this bus has never heard of. Put back, it holds the
    /// role again under a new id (`workshop_id`), with the grant it was mounted with. Host root
    /// authority, for an interval no gesture can make.
    std::unique_ptr<loom::Weave> take_workshop_off();
    void put_workshop_back(std::unique_ptr<loom::Weave> weave);

    /// THE OPENING MANAGER, MOUNTED THE WAY THE HOST MOUNTS IT -- its production grant spelled by
    /// hand, since one minted from the Emit set could not notice the host widening it, and the
    /// authority to commit a joint publication minted by this rig's bus for the manager's own id,
    /// over exactly the two roles the host names. A case that wants the managed door mounts
    /// this beside Workshop; one that wants the direct door alone does not.
    OpeningManager* opening = nullptr;
    loom::WeaveId opening_id{};

    OpeningManager* mount_opening();

    /// THE TERMINAL PARTICIPANT, MOUNTED THE WAY THE HOST MOUNTS IT -- owned by the bus, handed to
    /// Workshop as a non-owning pointer, and out of the pane's reach, so every case drives the pane
    /// and asks THIS object what it heard. Its grant is this rig's, `RigNote` to whoever holds
    /// `kRigBoard`; `widen` lets it say that to any target, so a case can address a weave by id
    /// and measure what the BUS says about the target rather than what the grant does.
    loom::TerminalSession* mount_terminal(int shapes = 0, bool widen = false);
    loom::WeaveId terminal_id{};
    bool terminal_edits_settings = false;

    /// A native provider in a role of its own, granted the pane sentences a provider says --
    /// offer, both declarations, unspent Escape, a press handed back -- its drawing to the window
    /// that shows its area, and a press to forge. The role is a view, so a case can seat a weave
    /// in a role longer than Workshop's own key bound.
    ProviderSeat* mount_provider(std::string_view role);

    /// A SEAT THAT TAKES ITS SETTINGS in `role`, granted only what it says.
    SettingsSeat* mount_settings_seat(std::string_view role);
    /// ...AND TAKE IT OFF THE BUS, role and all, so another weave may hold that role.
    void unmount_settings_seat(SettingsSeat* seat);
    loom::WeaveId settings_seat_id(const SettingsSeat* seat) const;
    /// Make a seat perform one sentence INSIDE ITS OWN DELIVERY, which is what gives
    /// `mail.as_role(...)` a real authorship moment for Loom to verify.
    void drive(SettingsSeat* seat, std::function<void(SettingsSeat&, loom::Mail&)> what);

    void drive(ProviderSeat* seat, std::function<void(ProviderSeat&, loom::Mail&)> what);

    /// LOAD A REAL SHARED LIBRARY THROUGH THE REAL KERNEL AND MANAGER, exactly as a
    /// host does: an ordinary `LoadWeave` command sent as a weave that can hear the
    /// answer. There is no direct `Kernel::load` here and no test-only door -- the
    /// artifact goes through the same path `zengine-workshop` sends its window through,
    /// and the activation the loaded weave receives is Loom's own, attested.
#ifdef WORKSHOP_SO_MENU_PRESENTER
    /// LOAD A PRESENTER INTO `zengine.presenter` THROUGH THE REAL MANAGER -- the shipped one by
    /// default, the replacement example when a case asks -- under the plan's own stem, so a later
    /// reload through the control door names the library the plan names.
    loom::WeaveId load_presenter(const char* path = WORKSHOP_SO_MENU_PRESENTER);
#endif

    loom::WeaveId load(const char* name, const char* path, const char* role);
    loom::WeaveId load(const char* name, const char* path, std::vector<std::string> roles);

    /// UNLOAD A REAL LIBRARY THROUGH THE REAL CONTROL DOOR. The Weave Manager has no unload op --
    /// its four are load, swap, reload and list -- so this addresses `zen.UnloadLibrary` to the
    /// door itself, under Loom's own `load_capability` grant rather than a hand-written subset: a
    /// case that quietly narrowed the dangerous grant would be testing its own idea of it.
    bool unload(const char* name);

    /// RELOAD A REAL LIBRARY IN PLACE THROUGH THE REAL CONTROL DOOR -- `zen.ReloadLibrary`, the op
    /// the Weave Manager spends when a weaver's rebuilt product is offered: the Kernel snapshots
    /// the live weave, opens the new image and revives it at the same id. QUEUED, NOT DRAINED, so a
    /// reload lands at an exact interval of an operation in flight; the case pumps, and
    /// `load_refusals` says whether it was refused.
    void enqueue_reload(const char* name, const std::string& path);
    /// ...and the same, queued: the unload lands at its place in the burst.
    void enqueue_unload(const char* name);


    /// A watcher in the `zengine.workshop` role, INSTEAD of Workshop. Only one weave
    /// may hold a role, so a case uses this OR `mount_workshop`, never both.
    PaneWatcher* mount_watcher();

    void drive_watcher(PaneWatcher* watch, std::function<void(PaneWatcher&, loom::Mail&)> what);

    loom::WeaveId watcher_id{};

    /// WHAT BECAME OF ONE ID SAID AS WORKSHOP'S ROLE (`workshop_action`), read off the tap.
    struct RoleAction {
        bool authored = false;  ///< Loom took the authorship: Workshop held the role named
        bool delivered = false; ///< ...and the role's holder ran its handler on the delivery
        std::string author;     ///< the role that delivery carried
    };

    /// A RESOLVED ACTION ID SAID AS WORKSHOP'S OWN ROLE, WITH NO KEY BEHIND IT -- through the
    /// host's verified role door (`role_send_to_role_as`): Workshop's weave is the stamped
    /// sender, `zengine.workshop` is verified at authorship, and Workshop's grant gates it. It
    /// hands a pane an id the keymap would never resolve for it, under provenance the pane
    /// accepts; a case pairs it with a declared id through the same door to show provenance was
    /// never the reason. Drained.
    RoleAction workshop_action(std::string_view role, const std::string& pane,
                                 const std::string& id);

    /// A GESTURE OR A WORD, published as a medium publishes it -- the window up first, once
    /// Workshop is mounted, so the desk it lands on is the one a weaver sees.
    void publish(const loom::Value& v);

    /// THE DESK STARTED: the window brought up at the room Workshop holds, so it offers itself
    /// and Workshop starts on that offer -- where it asks the room who has panes. A case that
    /// seated a window of its own (`own_medium`) is started by that window's offer.
    void ready();

    /// A ROOM OF `width` BY `height` CANVAS CELLS, the window sized to it as a weaver sizes it: a
    /// cell medium -- the TUI window, its device unit the cell -- unless a face is given, `adv`
    /// by `line` canvas pixels a character, which the GUI window sets text in, every pixel shown.
    /// The window says its device unit itself, so `cell` is kept only for the spelling of a
    /// medium the rig no longer reports.
    void extent(std::int64_t width, std::int64_t height, std::int64_t adv = 0,
                std::int64_t line = 0, std::int64_t cell = 0);

    /// ...and a room in canvas pixels, for the cases about a window whose size is not a whole
    /// number of cells; a cell medium holds it in whole cells.
    void extent_px(std::int64_t width, std::int64_t height, std::int64_t adv = 0,
                   std::int64_t line = 0, std::int64_t cell = 0);

    /// THE SHIPPED GRAPHICAL FACE, at whatever room a case wants -- the GUI window with the face
    /// it measures. Named, so a case reads as "the weaver is on the window" rather than as numbers.
    void extent_on_window(std::int64_t width, std::int64_t height);

    /// WHAT THE WINDOW SHOWING THE DESK SHOWS NOW.
    DeskShown shown();

    /// THE WEAVER'S OWN BUTTON THROUGH THE WINDOW SHOWING THE DESK, at a canvas cell: landed by the
    /// window in whatever area stands there -- a pane's menu its presenter draws among them --
    /// where a bare moment goes to Workshop alone. `pressed` false is its release.
    void window_button(std::int64_t cx, std::int64_t cy, bool pressed, std::int64_t button = 1);
    /// ...a press and its release there.
    void window_click(std::int64_t cx, std::int64_t cy, std::int64_t button = 1);
    /// ...AT A POINT IN THE MEDIUM'S OWN SPACE, as a reading names one: the TUI window's cell,
    /// its rows above the desk counted, or the GUI window's pixel.
    void hand_button(std::int64_t x, std::int64_t y, std::int64_t space, bool pressed,
                     std::int64_t button = 1);
    /// ...OR QUEUED ON THE WINDOW'S PLATFORM, a button or a key, and read together on its next
    /// beat (`window_beat`), as one batch of the weaver's.
    void window_queue_button(std::int64_t cx, std::int64_t cy, bool pressed,
                             std::int64_t button = 1);
    void window_queue_key(std::int64_t scan, std::int64_t mods = input::mod::kNone);
    void window_beat();

    /// THE GUI WINDOW FROM HERE ON, with the face it ships with, for every room after -- one given
    /// in cells too: the medium whose platform clipboard a paste can read (`windows.set_clipboard`).
    /// The TUI window reads none. A window up already is moved to it now.
    void on_gui_window();
    bool gui_window = false;
    /// The face a room is shown with: the one asked, or the GUI window's own once this rig is on it.
    std::pair<std::int64_t, std::int64_t> face_for(std::int64_t adv, std::int64_t line) const;

    /// WHETHER THIS CASE SEATED A MEDIUM OF ITS OWN -- a directory or a window in its role --
    /// before the rig's came up: then the rig's never does, and the case's own window offers the
    /// room and starts the desk.
    bool own_medium();


    /// The window comes up, if it is not up and Workshop is mounted, at the room and face
    /// Workshop holds now.
    void come_up();

    void key(std::int64_t sc, std::int64_t mods = input::mod::kNone);

    /// THE CHARACTER A PRINTABLE TRIGGER ALSO PRODUCES -- `Live`'s own door, here because a
    /// case that enters pane management has to pay the `swallow_text_` rule the way a real
    /// backend makes it pay: the key transition AND the text, in the order they arrive.
    void text(const std::string& s);

    /// THE WEAVER'S OWN POINTER MOMENT, through the window showing the desk, landed in whatever
    /// area stands there as a weaver's hand is -- or, where a case seated a window of its own, a
    /// button through that window's hand (`own_hand`).
    template <class Moment>
    void hand_moment(const Moment& moment) {
        if constexpr (std::is_same_v<Moment, input::PointerButton>) {
            if (own_medium() && own_hand) {
                own_hand(moment);
                bus.drain_until_idle();
                return;
            }
        }
        come_up();
        REQUIRE_MESSAGE(windows.up(), "no window shows this desk");
        windows.hand({moment});
        bus.drain_until_idle();
    }
    /// A CASE'S OWN WINDOW'S HAND for a button, set by the case that seated it.
    std::function<void(const input::PointerButton&)> own_hand;
    /// THE WINDOW'S BEATS UNTIL ITS ROOM STANDS: the GUI window grows to the reach of the areas it
    /// shows on one beat and offers its new room on the next.
    void window_settles();
    /// THE WEAVER CLOSED THE WINDOW THAT SHOWS THE DESK: its close, said as that window's role.
    void close_requested();
    /// A PRIMARY PRESS AT A CANVAS CELL, as the TUI window reports it. These cases speak
    /// canvas cells, not workspace cells, because a pane is placed on the canvas and never in the
    /// document's room -- `Live::term_x/y` is the other conversation's inverse.
    void press_cell(std::int64_t cx, std::int64_t cy);
    /// THE WHEEL AT A CANVAS CELL, as the TUI window reports it. `dy` is notches, +1 away
    /// from the weaver, fractional as a precise wheel reports.
    void wheel_cell(double dy, std::int64_t cx, std::int64_t cy);
    /// The same gesture as the WINDOW reports it: an exact device pixel, untranslated.
    /// A graphical press is finer than a cell, so its cases have to be able to say one.
    void press_pixel(std::int64_t px, std::int64_t py);
    /// A SECOND-BUTTON press at a canvas cell, `press_cell`'s own translation.
    void right_press_cell(std::int64_t cx, std::int64_t cy);
    void release_cell(std::int64_t cx, std::int64_t cy);
    /// THE HAND MOVING AT A CANVAS CELL, as the TUI window reports it -- what a sweep is
    /// made of between its press and its release.
    void motion_cell(std::int64_t cx, std::int64_t cy);

    /// OPEN THIS PANE, OR CLOSE IT IF THE DESK NAMES IT -- through the host's two doors, the ones
    /// the Pane Manager spends (`toggle_pane`).
    void pick(const PaneRef& ref);

    // ---- A real authored arrangement, and the door that answers for it ----------------------
    // NO TIMER IN THIS RIG, EVER: `PaneRig` pumps to EMPTY and a live Timer service re-arms its
    // own beat inside its own handler, so a plan naming `zengine-timer` would not return; the
    // arrangement facts that need one are proved in `test_workshop_load.cpp`, whose rigs drain in
    // bounded turns. The artifacts resolve from the suite's own `_SO` paths, not a staging
    // directory: where a host finds a file is the load suite's question.
    static std::string artifact_path(const std::string& stem);

    /// REALIZE AN AUTHORED PLAN ON THIS RIG'S BUS, the way the host does: the host writes the
    /// owner's grant and mounts it, the owner realizes the rows, and the owner RETAINS the plan
    /// because the projection pairs authored intent with resolved state. The turns are the caller's: `begin`
    /// issues what it can and returns with a row in flight, and a rig with no host loop turns the
    /// crank itself -- `Owner` never does (`test_workshop_load.cpp` owns that claim).
    load::Executed run_plan(load::LoadPlan plan);

    /// WIRE WHAT STANDS BEHIND A ROLE'S CODE the way `workshop.cpp` wires it: the bus's holder,
    /// this rig's realization owner's rows (none before `run_plan`), and the catalog it holds --
    /// each read at the moment a pane's Edit Code asks.
    void wire_code_source();

    /// MOUNT THE HOST'S OBSERVATION DOOR, with the production grant spelled out: a rig that minted
    /// the grant from the Emit set could not notice the host quietly widening it.
    loom::WeaveId mount_arrangement(std::string plan_path = std::string());

    /// MOUNT THE HOST'S DISCOVERY DOOR over this rig's catalog, with the production grant spelled
    /// out for `mount_arrangement`'s reason: its two answers and its page's sections, to any.
    loom::WeaveId mount_powers();

    /// EXPOSE THE HOST'S OWN TWO SOURCES, through the one door the host uses.
    ///
    /// ⚠ BEFORE THE PLAN RUNS, deliberately, exactly as `workshop.cpp` does it: the
    /// ordinary collision law then answers a provider that would supply one of these
    /// identities, in words, rather than letting load order decide.
    void expose_host_sources();

    /// MOUNT THE HOST'S SAMPLE DOOR, with the production grant spelled out, for
    /// `mount_arrangement`'s reason.
    loom::WeaveId mount_sampler();

    Session& session();
    /// EVERY LAYOUT OF THE RUN, as the layouts' store keeps it.
    const SetupState& run() const;
    /// THE LAST THING WORKSHOP SAID, as the session holds it (`Session::notice`).
    const std::string& last_notice() const;

    /// WHAT IS CURRENTLY TRUE OF THIS WORKSHOP, through the same projection the
    /// screen and the view both spend -- never a second walk of the owners.
    std::vector<Condition> conditions() const;
    /// THE GLANCE A PRESENTER OF WHAT IS TRUE LEADS WITH, from what this Workshop last said.
    std::string glance() const;

    std::vector<loom::WeaveId> seat_ids;
    std::vector<ProviderSeat*> seats_;
    std::vector<std::pair<SettingsSeat*, loom::WeaveId>> settings_seats_;
    /// The realization owner a case's plan runs on, once `run_plan` mounted it.
    load::Owner* plan_ = nullptr;
    /// HELD BY POINTER SO IT IS NOT CONSTRUCTED UNTIL A CASE ASKS, and declared LAST
    /// so it is destroyed FIRST: the owner retains the provider identity of every mount it
    /// made, and must not outlive the artifacts holding them.
    std::unique_ptr<load::Mounted> mounted_plan_;
};

/// Every row the window shows in an external pane's body, Workshop's header row included as row
/// 0: each line of the window's face inside the body's cells, cut to its width, "" where a line
/// shows nothing, as far as the last line that shows something. The window composes front areas
/// over the ones behind, so a row is what a weaver reads there, whatever covers the pane.
std::vector<std::string> external_region_rows(const DeskShown& s, const CellRect& body);

/// The rows an external pane's PROVIDER is currently showing, read off the window at the pane's
/// body -- never off the session, so a case reads what a weaver sees. Workshop's own header, row 0
/// of the body, is dropped: it is Workshop's sentence, and every case here asks what the PROVIDER
/// said.
std::vector<std::string> external_rows(const DeskShown& s, const CellRect& body);

PaneOffered good_offer();

std::string bytes(std::size_t n, char c);

/// The body bounds an open external pane resolves to, in the cells they cover -- read through
/// the very functions the painter and the pointer use, so a case cannot measure a rectangle
/// nothing draws in.
CellRect external_body_rect(const Session& s, std::int64_t kind);

/// THE DURABLE REFERENCE THE INFO PANE IS OFFERED UNDER, as a case spells it: a weave's name
/// (`info/vocabulary.hpp`), not a built-in kind's. A case in the panes suite checks this
/// literal against the weave's own constant, which is where a divergence would be caught.
PaneRef info_ref();
/// ...and the Notices pane's, the shipped desk's other loaded pane, spelled as a stranger would.
PaneRef notices_pane_ref();

/// The setup a two-pane ordering case starts from: two overlay panes, at a screen with room for
/// two stack slots, built through the doors so the ranks are the identity permutation `add_pane`
/// assigns. The two are the stack's stand-ins (`stock` and `second`, both admitted by every
/// `Live`), so no case built on it is about an unresolved row.
Setup two_overlays();

/// A `Live`'s session, mutably -- the same door `PaneRig::session` already opens, so a case
/// that has to arrange a setup DIRECTLY (rather than through the keys it is measuring) can.
Session& live(Live& t);

/// AUTHOR A PLACE AT A CANVAS POSITION: what a case measured on the canvas, said in the room an
/// authored place is measured from.
Written place_at_canvas(Session& s, const PaneRef& ref, std::int64_t x, std::int64_t y);

/// ...ON A LIVE WORKSHOP'S STORE, staged as a case writes past the doors.
template <class Rig>
inline Written place_at_canvas(Rig& r, const PaneRef& ref, std::int64_t x, std::int64_t y) {
    const std::int64_t room_y = screen_of(r.session()).room_y;
    Written out = Written::ok();
    stage_desk(r, [&](Setup& desk) { out = author_pane_place(desk, ref, x, y - room_y); });
    return out;
}

/// The kinds a setup AUTHORS, in the order the file holds them: the BASE, not what a weaver sees
/// -- `painted_order` below is the effective one.
std::vector<std::int64_t> authored_order(const Session& s);

/// WHERE A KIND SITS IN AN ORDER, or -1 -- so a case can state a RELATION between two panes
/// ("this one is ahead of that one") rather than a position ("this one is last"). A position
/// is a claim about every other pane on the desk as well, which is how a case about two
/// panes comes to go red when a third arrives that has nothing to do with it.
std::int64_t index_of(const std::vector<std::int64_t>& order, std::int64_t kind);

/// The kinds a setup resolves to, in the order they would be PAINTED -- the authored order with
/// the selected pane lifted. The same call paint, hit testing and coverage spend, so a case
/// reading it is reading the picture.
std::vector<std::int64_t> painted_order(const Session& s);

/// The front ranks of a setup, in list order -- what every ordering case reads.
std::vector<std::int64_t> ranks_of(const Setup& s);

/// Is this set of ranks exactly `{0 .. n-1}`? The invariant, spelled once so every ordering
/// case asks it the same way and none of them can weaken it by accident.
bool is_permutation(const Setup& s);

/// THE POPUP UNDER THE HAND, in canvas cells on a CELL medium: a pane's menu while its presenter
/// shows one, the host's own otherwise (at most one is open) -- through the same `context_bounds`
/// the painter and the press resolver spend. `context_entry_cell_y` is population row `index`'s
/// canvas row while the window shows the population from its top with no `earlier` marker, which
/// every case using it arranges; the surface reserves no heading rows, so row `index` is the
/// `index`'th row of the interior.
PixelRect menu_bounds(const Session& s);
std::int64_t context_cell_x(const Session& s);
std::int64_t context_entry_cell_y(const Session& s, std::size_t index);

/// IS A PANE'S MENU OPEN AND SHOWN -- granted to the presenter, which said how much of its area
/// the menu takes?
bool menu_shown(const Session& s);

/// The contextual surface's rows, as the window shows them at exactly its region: each line of
/// the window's face there, cut to the region, its trailing blanks left off, as far as the last
/// line that shows something. The surface paints over every pane, so the window shows it whole.
std::vector<std::string> context_rows_on(const DeskShown& shown, const Session& s);

/// THE LINES OF THE OPEN PANE'S MENU, AS THE WINDOW SHOWS THEM: its presenter draws them in an
/// area of its own inside the popup's frame (empty when none is shown).
template <class Rig>
std::vector<std::string> presented_texts(Rig& r) {
    if (!menu_shown(r.session())) return {};
    return context_rows_on(r.shown(), r.session());
}

/// THE LINE OF THE PRESENTED MENU THAT READS `label` (after whatever marks the presenter puts in
/// front of a row, and whatever a window's reading shows past its words, or the row's tail), or -1.
template <class Rig>
std::int64_t presented_line_of(Rig& r, const std::string& label) {
    const std::vector<std::string> lines = presented_texts(r);
    for (std::size_t i = 0; i < lines.size(); ++i) {
        const std::string& t = lines[i];
        // The marks are a cursor and a digit, each followed by a blank: the words begin after them.
        std::size_t at = 0;
        while (at < t.size() && at < 4 && (t[at] == ' ' || t[at] == '>' ||
                                           (t[at] >= '0' && t[at] <= '9'))) {
            ++at;
        }
        // ...or the line ends with the words asked for, the tail of a longer row.
        const bool ends = t.size() >= label.size() &&
                          t.compare(t.size() - label.size(), label.size(), label) == 0;
        if (t.compare(at, label.size(), label) == 0 || ends) {
            return static_cast<std::int64_t>(i);
        }
    }
    return -1;
}

/// OPEN A PANE THROUGH THE LAUNCH DOOR, AS SCAFFOLDING (`seat_pane`), and require that it opened.
void open_pane(Live& t, const PaneRef& ref);

/// OPEN A PANE AND STAND IT IN THE STRIP AT THE ROOM'S RIGHT EDGE: `top-right` at no offset,
/// `kStripCols` cells wide and the room's height (`fill`), written as a desk row says it, so a case
/// can have a pane there without a role behind it. A key or a hand could reach the same row;
/// writing it says the words exactly.
void open_at_top_right(Live& t, std::int64_t kind);

/// Put the desk's keyboard address on a setup-named pane, by the key a weaver presses.
/// Bounded for `open_pane`'s reason.
void select_pane(Live& t, const PaneRef& ref);

/// A `Live` driven into the desk arrangement scope. Every keyboard case begins here, through the
/// REAL input path -- the `w` transition and the character the platform's layout made of it,
/// both, in the order the backends report them.
void enter_arrange_desk(Live& t);

namespace intro = zengine::introspection;

inline constexpr const char* kIntroRole = intro::kIntrospectionRole;
inline constexpr const char* kIntroPane = intro::kLoadedPane;

PaneRef intro_ref();

/// HOW MANY PANES THIS ONE ROLE OFFERS. Cases say the number rather than assume it: "one
/// provider is not one pane" is what `PaneOffered` was shaped for, and a case that indexed
/// `entries[0]` would quietly assert the opposite.
inline constexpr std::size_t kIntroPaneCount = 3;

/// THE RUNTIME HANDLE WORKSHOP MINTED FOR ONE OF THIS ROLE'S PANES -- BY `PaneRef`
/// AND NEVER BY INDEX. Catalog order is first-accepted-offer order, which is a fact
/// about a boot sequence and not an identity; a case that meant `loaded` says so.
const RuntimePane* intro_row(PaneRig& r, const char* pane);

/// Does any row of this projection contain `needle`?
bool any_row(const std::vector<TextRow>& rows, const std::string& needle);

bool any_row(const std::vector<std::string>& rows, const std::string& needle);

/// A population of `n` distinct weaves, each with a role -- the input the density
/// sweep varies, so that what changes between budgets is only the budget.
std::vector<intro::LoadedWeave> loaded_population(std::size_t n);

/// Every region on a canvas whose upper-left corner is in this cell.
std::vector<ScreenRegion> regions_at(const ScreenCanvas& c,
                                            std::int64_t x, std::int64_t y);

/// A session with a screen extent and a text metric.
Session screen_session(std::int64_t w, std::int64_t h, std::int64_t advance,
                              std::int64_t line, std::int64_t cell_px = 0);

/// A PRESS AT A CANVAS CELL AS THE GRAPHICAL MEDIUM REPORTS IT -- the pixel at the
/// middle of that cell, which is where a weaver's pointer actually is.
///
/// It is the inverse of `plan_canvas`'s layout and NOT of the terminal's, because the
/// two media report different numbers for one place (workshop/docs/pointer-spaces.md).
std::int64_t cell_mid_px(std::int64_t cell);

/// THE PANE RECTANGLE AN EXTERNAL PANE OCCUPIES -- the painter's own, through the one
/// `bounds_of` path, so a case never spells a placement of its own.
PixelRect external_pane_rect(const Session& s, std::int64_t kind);

/// WHETHER A PANE DRAWS IN AN AREA OF ITS OWN: a participant holds its role, and draws in a
/// graphics window.
bool shows_canvas(const ExternalPane& pane);

/// THE LATTICE A PANE'S AREA SETS ITS ROWS ON, in the desk's pixels: where the area was last
/// placed, the window's text there, and the rows and columns its room holds. Empty while the area
/// holds no room.
struct CanvasRows {
    std::int64_t x = 0, y = 0;
    std::int64_t advance = gfx::kCellPx, line = gfx::kCellPx;
    std::int64_t columns = 0, rows = 0;
    std::int64_t width = 0, height = 0; ///< the room's

    bool empty() const noexcept;
    /// Where row `row` stands.
    std::int64_t row_y(std::int64_t row) const noexcept;
    /// Where column `column` stands.
    std::int64_t column_x(std::int64_t column) const noexcept;
};

/// The area a pane draws in, while the rig's Workshop knows the pane; else "".
std::string held_area_of(std::int64_t kind);

CanvasRows held_canvas_rows(const ExternalPane& pane);

/// ONE ROW OF A PANE'S AREA AS ITS DRAWING SAYS IT, blanks and all: the runs of text
/// `rows_drawing` sets on that row, in order across it.
std::string held_row_raw(std::int64_t kind, std::int64_t row);

/// ONE ROW OF TEXT A PANE DREW IN ITS AREA: what it says, the blanks after its last character
/// left off, and the colour its first run is set in.
struct HeldRun {
    std::int64_t row = 0;
    std::string text;
    gfx::Color ink = gfx::no_color();
};

/// EVERY ROW OF A PANE'S AREA ITS DRAWING SETS TEXT ON, top to bottom.
/// The rows a pane drew, read as the window reads them (`held_reading`).
std::vector<std::string> held_read_rows(std::int64_t kind);

std::vector<HeldRun> held_runs(std::int64_t kind);

/// ONE RUN OF TEXT, OR ONE RECTANGLE, A PANE DREW IN ITS AREA, in the area's pixels: its place,
/// what it says, and the colour it is drawn in -- what the drawing shows, its parts' places drawn
/// again and its marks left out.
struct HeldText {
    std::int64_t x = 0, y = 0;
    std::string text;
    gfx::Color ink = gfx::no_color();
};
struct HeldRect {
    std::int64_t x = 0, y = 0, w = 0, h = 0;
    gfx::Color ink = gfx::no_color();
};

const zengine::graphics::window::WinArea* held_area(std::int64_t kind);

bool held_shown_node(const std::string& path);

std::vector<HeldText> held_texts(std::int64_t kind);

std::vector<HeldRect> held_rects(std::int64_t kind);

/// WHETHER THE WINDOW HOLDS A GROUND UNDER ALL OF A PANE'S AREA: a rectangle its drawing lays
/// first, over the whole room, so nothing behind the area shows through its rows.
bool wears_ground(const ExternalPane& pane);

/// ONE LIT ROW THE WINDOW HOLDS FOR A PANE'S AREA, as `rows_drawing` draws one: its row, the colour
/// of the ground laid under it, and the colour its words are set in.
struct HeldLitRow {
    std::int64_t row = 0;
    gfx::Color ground = gfx::no_color();
    gfx::Color ink = gfx::no_color();
};

std::vector<HeldLitRow> held_lit_rows(std::int64_t kind);

/// A PRESS AND ITS RELEASE AT A ROW AND COLUMN OF A PANE'S AREA, told its role as the window
/// showing the desk tells one, queued and not drained: the press a weaver's hand makes there once
/// the bus is idle.
template <class Rig>
inline void enqueue_window_press(Rig& r, std::int64_t kind, std::int64_t row, std::int64_t column) {
    const ExternalPane* pane = r.session().panes.external_pane(kind);
    REQUIRE(pane != nullptr);
    const RuntimePane* entry = r.session().panes.runtime.of_kind(kind);
    REQUIRE(entry != nullptr);
    const CanvasRows lattice = held_canvas_rows(*pane);
    REQUIRE_FALSE(lattice.empty());
    const std::string area = pane_window_area(r.session().panes, *entry);
    const std::int64_t x = column * lattice.advance + lattice.advance / 2;
    const std::int64_t y = row * lattice.line + lattice.line / 2;
    for (const bool down : {true, false}) {
        REQUIRE(r.bus.role_send_to_role_as(r.windows.id(), r.windows.role(), entry->provider,
                                             loom::Message(loom::to_value(gfx::AreaButtonChanged{
                                                 area, 1, down, x, y, 0}),
                                                           r.windows.id(), r.windows.id(), 0))
                    .valid());
    }
}

/// A POINTER MOMENT FOR A PANE'S AREA, said to its role as the window showing the desk says
/// one under the window's number for its press, its point in the area's pixels, and the bus
/// settled.
template <class Rig, class Moment>
inline void as_window(Rig& r, std::int64_t kind, const Moment& moment, std::uint64_t press = 0) {
    const RuntimePane* entry = r.session().panes.runtime.of_kind(kind);
    REQUIRE(entry != nullptr);
    REQUIRE(r.bus.role_send_to_role_as(r.windows.id(), r.windows.role(), entry->provider,
                                         loom::Message(loom::to_value(moment), r.windows.id(),
                                                       r.windows.id(), press))
                .valid());
    r.bus.drain_until_idle();
}

/// The cell of the lattice a point of the desk falls on.
RowCell row_cell_at(const CanvasRows& rows, std::int64_t x, std::int64_t y);

/// WHAT A PANE DREW IN ITS AREA, as the window holds it: its rows, each its text with the blanks
/// after the last character left off, "" where a row shows none; the parts its records name, each
/// on the row and columns its text covers, saying whether it applies now; and its caret and
/// selection, where its marks stand.
struct HeldReading {
    std::vector<std::string> rows;
    std::vector<gfx::Color> inks; ///< each row's colour: that of the first text drawn on it
    std::map<std::string, RowPart> parts;
    RowsCaret caret;
};

HeldReading held_reading(std::int64_t kind);

std::vector<std::string> held_read_rows(std::int64_t kind);

/// WHERE A PANE'S CARET AND SELECTION STAND in the rows it drew.
RowsCaret held_caret(const ExternalPane& pane);

/// THE ROWS A PANE DREW, each its text, "" where a row has none.
std::vector<std::string> held_canvas_text(const ExternalPane& pane);
std::vector<std::string> held_row_texts(const ExternalPane& pane);

/// THE ADDRESS LAW AGAINST WHAT A PANE DREW (`control_parts_problem`, tests/control_parts.hpp):
/// the parts its drawing names, and the rows its keymap has in force for it.
std::string held_control_parts_problem(const Session& s, std::int64_t kind);

/// THE PARTS A PANE NAMED in what it drew, by name, each on the row and columns of its rows it
/// covers, saying whether its operation applies now. Every read holds the drawing to the address
/// law (`held_control_parts_problem`).
std::map<std::string, RowPart> held_parts(const Session& s, std::int64_t kind);

/// ...and what the named part's columns of its row say, as the pane drew them.
std::string held_part_text(const Session& s, std::int64_t kind, const RowPart& part);

/// The place Workshop resolved for that pane's body, read from the same function that places its
/// area.
ExternalBodyPlace external_body_of(const Session& s, std::int64_t kind);

/// WHAT AN INDEPENDENT LISTENER HEARD -- kept outside the weave so a case can outlive
/// the bus that delivered it, which is the same shape `Painter` uses for canvases.
struct Ears {
    std::vector<intro::LoadedSelected> heard;
    std::vector<std::string> authors;
};

/// AN INDEPENDENT LISTENER, and the point is that it is a STRANGER: it compiles
/// `introspection/vocabulary.hpp` and nothing else of the tool, registers no callback, is handed
/// no pointer, and is not Workshop, a provider or in any role. What it hears, it hears because
/// a weave published an ordinary Loom message -- the whole claim `LoadedSelected` makes good on.
class SelectionListener
    : public loom::WeaveBase<SelectionListener, SeenState, loom::Accept<intro::LoadedSelected>,
                             loom::Emit<>> {
public:
    explicit SelectionListener(Ears& ears);
    void on(const intro::LoadedSelected& s, loom::Mail& mail);

private:
    Ears* ears_;
};

/// THE ROWS A CANVAS PANE SHOWS, read off the window as `external_rows` reads a text pane's: each
/// line of the window's face inside the pane's picture body, cut to the body's width, its blanks
/// after the last character left off, "" where a line shows none, as far as the last line that
/// shows something.
std::vector<std::string> canvas_rows_shown(const Session& s, const DeskShown& shown,
                                                  std::int64_t kind);

/// The prose rows the Loaded pane is currently showing, with Workshop's header dropped
/// -- said once here so a case below reads as a gesture and an assertion. Off its picture when
/// it draws one.
/// LOADED'S HEADING with no search typed: the count, then the search's empty box.
std::string loaded_heading(const std::string& count);

std::vector<std::string> loaded_rows(PaneRig& r, std::int64_t kind);

/// The library name an entry row shows, read off the canvas: `  name @role` without
/// its two-character mark. A case must never assume WHICH weave a row holds -- the
/// kernel's map decides that -- so it reads the row a weaver would have aimed at.
std::string named_by(const std::string& row);

bool is_entry_row(const std::string& row);

inline constexpr const char* kComposerRole = zengine::composer::kComposerRole;
inline constexpr const char* kComposePane = zengine::composer::kComposePane;
inline constexpr const char* kTimerRole = zengine::timer::kTimerRole;

PaneRef composer_ref();

// ---- A stand-in desktop: the participating owner of the application's defaults ------------
// Escape-to-deselect is a DECLARED application row (WL-DESK-02), so a Workshop with no desktop
// has no such row and Escape does nothing -- "a weaver can disable an application default" -- and
// a case asserting the deselect must supply the party that owns it. A stand-in, not the shipped
// weave: it declares the shipped ids on the shipped gestures and answers the deselect row the
// shipped way, for the many cases that need the behaviour without loading `workshop/desktop-pane/`.
class DesktopSeat
    : public loom::WeaveBase<DesktopSeat, SeatState,
                             loom::Accept<AppActionRequested, ActionsJudged, ActionsWithdrawn,
                                          PaneLaunchAnswered, PaneCloseAnswered,
                                          zengine::workshop::v1::PaneLaunchAnswered,
                                          zengine::workshop::v2::PaneCloseAnswered,
                                          zengine::workshop::v1::PaneToggleAnswered, SeatDo>,
                             loom::Emit<AppActions, DeselectRequested, PaneLaunchRequested,
                                        PaneCloseRequested, zengine::workshop::v1::PaneLaunchRequested,
                                        zengine::workshop::v2::PaneLaunchRequested,
                                        zengine::workshop::v1::PaneCloseRequested,
                                        zengine::workshop::v1::PaneToggleRequested>> {
public:
    void on(const AppActionRequested& asked, loom::Mail& mail);
    /// WORKSHOP'S VERDICT ON ONE OF THIS PARTY'S DECLARATIONS, with the number Loom says it
    /// answers and whether Loom says it IS an answer. Kept, never acted on: a declarer's recovery
    /// is its own, and this one's is to remember.
    void on(const ActionsJudged& said, loom::Mail& mail);
    /// ...AND A DECLARATION IT HAD IN FORCE LEAVING THE KEYMAP LATER.
    void on(const ActionsWithdrawn& said, loom::Mail&);
    /// WHAT ITS LAUNCHES AND CLOSES CAME TO, kept in arrival order.
    void on(const PaneLaunchAnswered& said, loom::Mail&);
    void on(const PaneCloseAnswered& said, loom::Mail&);
    /// ...AND AS A PANE BUILT ON AN EARLIER VOCABULARY HEARS THEM, the role named `office`.
    void on(const zengine::workshop::v1::PaneLaunchAnswered& said, loom::Mail&);
    void on(const zengine::workshop::v2::PaneCloseAnswered& said, loom::Mail&);
    void on(const zengine::workshop::v1::PaneToggleAnswered& said, loom::Mail&);

    void on(const SeatDo&, loom::Mail& mail);

    void declare(loom::Mail& mail, const AppActions& a, std::uint64_t attempt = 0);
    /// Declared PERSONALLY, from the very weave holding the role. Holding is not
    /// speaking-for (MSG-07), and Workshop drops it for the offer's reason.
    void declare_personally(loom::Mail& mail, const AppActions& a);
    void launch(loom::Mail& mail, const std::string& role, const std::string& pane);
    void close(loom::Mail& mail, const std::string& role, const std::string& pane);
    /// The same asks in an earlier vocabulary's versions.
    void launch_v1(loom::Mail& mail, const std::string& office, const std::string& pane);
    void launch_v2(loom::Mail& mail, const std::string& office, const std::string& pane, bool keys);
    void close_v1(loom::Mail& mail, const std::string& office, const std::string& pane);
    void toggle_v1(loom::Mail& mail, const std::string& office, const std::string& pane);
    /// A DESELECT ANSWER THAT ECHOES A NUMBER OF ITS OWN CHOOSING -- for the cases about an
    /// answer to an ask that is over, and about one that echoes nothing at all.
    void deselect_answering(loom::Mail& mail, std::uint64_t answering);

    const std::vector<std::string>& asked() const;
    const std::vector<std::string>& refusals() const;
    /// ONE VERDICT AS IT ARRIVED: the correlation it echoes, Loom's word that it answers an ask,
    /// and what it said.
    struct Verdict {
        std::uint64_t correlation = 0;
        bool answer = false;
        ActionsJudged said;
    };
    const std::vector<Verdict>& verdicts() const;
    const std::vector<ActionsWithdrawn>& withdrawals() const;
    const std::vector<PaneLaunchAnswered>& launched() const;
    const std::vector<PaneCloseAnswered>& closed() const;
    const std::vector<zengine::workshop::v1::PaneLaunchAnswered>& launched_v1() const;
    const std::vector<zengine::workshop::v2::PaneCloseAnswered>& closed_v2() const;
    const std::vector<zengine::workshop::v1::PaneToggleAnswered>& toggled_v1() const;
    /// THE NUMBER THE LAST ASK ARRIVED UNDER -- what an honest answer echoes, and what a case
    /// about a DISHONEST one has to be able to miss on purpose.
    std::uint64_t last_ask() const;

    /// WHETHER THIS STAND-IN ANSWERS ITS DESELECT ROW BY ITSELF. Off for the case that has to
    /// leave an ask outstanding at a known gesture and then answer it wrongly: with the
    /// automatic answer in the way, the host has already reset its record and the wrong answer
    /// is refused by the gesture test rather than by the one under examination.
    bool autoanswer = true;

    std::function<void(DesktopSeat&, loom::Mail&)> next;

    static constexpr const char* kDeselectId = "desktop.deselect";
    static constexpr const char* kTerminalId = "desktop.terminal";
    static constexpr const char* kPanesId = "desktop.panes";
    static constexpr const char* kHotkeysId = "desktop.hotkeys";

private:
    std::vector<std::string> asked_;
    std::vector<std::string> refusals_;
    std::vector<Verdict> verdicts_;
    std::vector<ActionsWithdrawn> withdrawals_;
    std::vector<PaneLaunchAnswered> launched_;
    std::vector<PaneCloseAnswered> closed_;
    std::vector<zengine::workshop::v1::PaneLaunchAnswered> launched_v1_;
    std::vector<zengine::workshop::v2::PaneCloseAnswered> closed_v2_;
    std::vector<zengine::workshop::v1::PaneToggleAnswered> toggled_v1_;
    std::uint64_t last_ask_ = 0;
};

/// THE FOUR ROWS THE SHIPPED DESKTOP DECLARES, spelled once so a case and the product cannot
/// come to disagree about what the defaults are.
AppActions shipped_app_actions();

/// THE EFFECTIVE KEYMAP AS TEXT, one binding per line -- `group | gesture | label | id`, with
/// ` *` where the weaver's file moved it -- the value a presenter of keys is given
/// (`keymap_shown`), read by a case the way the Hotkeys pane reads it.
std::string keymap_text(const Session& s);

/// SEAT A STAND-IN DESKTOP AND LET IT DECLARE. Returns the seat, so a case can read what it
/// was asked for and what Workshop refused it.
template <class Rig>
inline DesktopSeat* mount_desktop(Rig& t, AppActions rows = shipped_app_actions()) {
    auto seat = std::make_unique<DesktopSeat>();
    DesktopSeat* raw = seat.get();
    loom::Grant grant;
    grant.allow_to_role(AppActions::zen_name, AppActions::zen_version, kWorkshopProvider);
    grant.allow_to_role(DeselectRequested::zen_name, DeselectRequested::zen_version,
                        kWorkshopProvider);
    grant.allow_to_role(PaneLaunchRequested::zen_name, PaneLaunchRequested::zen_version,
                        kWorkshopProvider);
    grant.allow_to_role(PaneCloseRequested::zen_name, PaneCloseRequested::zen_version,
                        kLayoutsRole);
    // ...AND THE SAME ASKS IN AN EARLIER VOCABULARY'S VERSIONS, as a pane built on one says them.
    grant.allow_to_role(zengine::workshop::v1::PaneLaunchRequested::zen_name,
                        zengine::workshop::v1::PaneLaunchRequested::zen_version,
                        kWorkshopProvider);
    grant.allow_to_role(zengine::workshop::v2::PaneLaunchRequested::zen_name,
                        zengine::workshop::v2::PaneLaunchRequested::zen_version,
                        kWorkshopProvider);
    grant.allow_to_role(zengine::workshop::v1::PaneToggleRequested::zen_name,
                        zengine::workshop::v1::PaneToggleRequested::zen_version,
                        kWorkshopProvider);
    grant.allow_to_role(zengine::workshop::v1::PaneCloseRequested::zen_name,
                        zengine::workshop::v1::PaneCloseRequested::zen_version,
                        kLayoutsRole);
    const loom::WeaveId id =
        t.bus.register_weave(std::move(seat), std::move(grant), std::string(kDesktopRole));
    raw->zen_set_self(id);
    raw->next = [rows](DesktopSeat& d, loom::Mail& m) { d.declare(m, rows); };
    (void)t.bus.send(id, loom::Message(loom::to_value(SeatDo{}), loom::WeaveId{},
                                       loom::WeaveId{}, 0));
    t.bus.drain_until_idle();
    return raw;
}

/// RUN ONE SENTENCE FROM THE STAND-IN DESKTOP, inside its own delivery, and drain.
template <class Rig>
inline void desktop_does(Rig& t, DesktopSeat* seat,
                         std::function<void(DesktopSeat&, loom::Mail&)> what) {
    REQUIRE(seat != nullptr);
    seat->next = std::move(what);
    t.publish(loom::to_value(SeatDo{}));
}


/// SEAT A RECORDING PROVIDER ON A `Live` RIG, and make it offer one pane -- answering with the
/// kind Workshop minted, or `kNoPaneKind`. `PaneRig` has its own for the seam's cases; a `Live`
/// case uses this when it needs another entry in a pane population, which comes from where every
/// pane comes from: a role that offered one.
std::int64_t live_offer_pane(Live& t, const char* role, const char* pane,
                                    const char* name);

/// Open an external pane belonging to a seat in `role`, and answer with its kind: the seat
/// offers, Workshop admits, the launch door opens it and the room is granted -- four beats a case
/// would otherwise spell every time.
std::int64_t seat_pane_open(PaneRig& r, ProviderSeat* seat, const char* role,
                                   const char* pane);

/// Press the first prose row of an external pane's body, in cells.
void press_body(PaneRig& r, std::int64_t kind);
/// ...AND LET GO THERE: the press ended, so the area a held press kept where it was aimed
/// is placed under the title rows the keys brought.
void click_body(PaneRig& r, std::int64_t kind);

/// Press somewhere this pane is NOT -- the gesture that hands the keyboard back to Workshop,
/// which a case must perform before it can use a command key. The cell is derived from the
/// pane's own rectangle: an overlay slot starts at the canvas's top-left, so a literal `(1, 1)`
/// is inside the pane it was meant to be outside of, a mistake that reads as a routing defect.
void press_outside(PaneRig& r, std::int64_t kind);

namespace ws = zengine::workshop;

/// AN ARRANGEMENT BUILT AS A VALUE, so a projection can be asked what it means without
/// a bus, a Kernel or an artifact anywhere near it.
ws::ArtifactParticipation participation(const char* stem, const char* mode, const char* role);

ws::ArtifactParticipation resolved_provider(ws::ArtifactParticipation a, const char* identity,
                                                   std::int64_t powers);

ws::ArtifactParticipation resolved_weave(ws::ArtifactParticipation a, std::int64_t id,
                                                const char* offer);


/// The production-shaped arrangement, as a value: a provider-only artifact, two
/// weave-only artifacts, and one artifact that is BOTH.
ws::ResolvedArrangement shaped_arrangement();

/// ---- Door rows, built as values ---------------------------------------------------------
/// One row as the discovery door would say it of a real definition: `kind` is the door's
/// derivation (`op::is_source`, `op::declares_migration`) and the output identity the definition's
/// own output schema. The provider, the construction and the contract are separate arguments
/// because the claim under test is that they are separate facts.
ws::PowerRow row_from(const char* identity, const char* kind, const char* provider,
                             bool composite = false, const char* yields = "zengine.Fixture");

ws::PowerRow source_from(const char* identity, const char* provider,
                                bool composite = false, const char* yields = "zengine.Fixture");

ws::PowerRow operator_from(const char* identity, const char* provider,
                                  bool composite = false, const char* yields = "zengine.Fixture");

/// An answer carrying these rows, whole: no page beyond them, and the census given.
ws::PowersFound found_of(std::vector<ws::PowerRow> rows, std::int64_t providers);

/// ALL FOUR `Source x Composite` CELLS, WITH DELIBERATELY MISLEADING NAMES.
///
/// `source.looks.like.one` takes arguments and `math.max` does not, which is the
/// falsifier for any implementation tempted to read a view membership off an
/// identity's spelling: a pane that classified by name would put both in the wrong
/// list and every other case here would stay green.
ws::PowersFound four_cells();

/// A population big enough that a pane has to window it, in catalog (name) order.
ws::PowersFound many_sources(std::size_t n);

/// What the door would say of one row's identity: the row, and its stack bottom to top.
ws::PowerDescribed described_as(const ws::PowerRow& row,
                                       std::vector<ws::PowerLayer> stack);

ws::PowerLayer layer_from(const char* provider, bool composite = false,
                                 const char* about = "");

/// A Powers pane state holding one answer, with nothing else authored.
intro::PowersUi showing(ws::PowersFound said);

/// The text of each row, a row of Workshop's own or one a pane draws.
template <class Row>
inline std::vector<std::string> texts_of(const std::vector<Row>& rows) {
    std::vector<std::string> out;
    for (const Row& r : rows) {
        out.push_back(r.text);
    }
    return out;
}

/// Which row of a projection carries `needle`, or -1.
template <class Row>
inline std::int64_t row_with(const std::vector<Row>& rows, const std::string& needle) {
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (rows[i].text.find(needle) != std::string::npos) {
            return static_cast<std::int64_t>(i);
        }
    }
    return -1;
}

/// The same question of rows already read off a published canvas.
std::int64_t row_with_text(const std::vector<std::string>& rows, const std::string& needle);

/// The arrangement this tier performs, and it is deliberately Timer-free (see the
/// section header). It still carries a PROVIDER-ONLY artifact and two weave-only ones,
/// which is what the pane claims must be distinguishable.
load::LoadPlan pane_plan();

/// Stand a live Workshop up over that arrangement, with the host's observation door
/// mounted, and open one of the tool's panes.
std::int64_t open_intro_pane(PaneRig& r, const char* pane);

/// THE SAME LIVE WORKSHOP, WITH THE POWERS PANE OPEN AND THE HOST'S DOORS MOUNTED -- and with
/// the host's own two Sources really in the catalog, so the Sources view has the population a
/// weaver meets. The order is the host's: sources exposed before the plan runs, the observation,
/// discovery and sample doors mounted before realization can grant a pane room.
std::int64_t open_powers(PaneRig& r);

/// THE ROWS A PANE SHOWS, read off the window as `external_rows` and `canvas_rows_shown` read them;
/// on a rig whose case seated a medium of its own (`own_medium`), which shows no desk here, the
/// rows Workshop holds for the pane. A TUI window draws the caret as a bar over the cell it
/// stands before, so a row read for its text reads that one cell as the pane holds it, and a bar
/// past the row's end as nothing.
std::vector<std::string> pane_rows(PaneRig& r, std::int64_t kind);

/// The band's legend rows, padding trimmed, read in one place.
std::vector<std::string> band_lines(PaneRig& r);

/// THE CELL SHOWING COLUMN `column` OF BODY ROW `row`, counted from the body's first cell: the
/// column's own, since a caret moves no character from its cell.
std::int64_t pane_cell_of(const Session&, std::int64_t, std::int64_t, std::int64_t column);

/// PRESS A PLACE IN AN EXTERNAL PANE'S OWN ROOM -- the provider's row and column,
/// which is exactly the pair `PanePressed` carries. Cases speak the provider's lattice
/// so the arithmetic that turns it into a canvas cell lives in one place. On a face, the
/// window's pixel at the middle of that glyph of the body's fit, where the window draws it.
void press_pane(PaneRig& r, std::int64_t kind, std::int64_t row, std::int64_t column);

/// THE PART THE WINDOW SHOWS A PANE DRAWING AT A CELL of its rows ("" for none): what a press there
/// lands on, by the name the pane gave it -- the narrowest that holds the cell, as a control inside
/// a row answers before the row.
std::string part_drawn_at(const std::map<std::string, RowPart>& parts, std::int64_t row,
                                 std::int64_t column);
std::string part_drawn_at(std::int64_t kind, std::int64_t row, std::int64_t column);

/// THE NUMBER EACH AREA'S LATEST DRAWING WAS SENT WITH (`Drawn::number`), heard as the bus delivers
/// it: what the window holds of an area's drawing, by its drawer's number.
struct DrawnNumbers {
    loom::Switchboard& bus;
    loom::ObserverId id{};
    std::map<std::string, std::int64_t> number;
    explicit DrawnNumbers(loom::Switchboard& b);
    ~DrawnNumbers();
    DrawnNumbers(const DrawnNumbers&) = delete;
    DrawnNumbers& operator=(const DrawnNumbers&) = delete;
    /// The number of the drawing the window holds of a pane's area.
    std::int64_t of(const Session& s, std::int64_t kind) const;
};

/// A PRESS AND ITS RELEASE THE WINDOW LANDED ON AN EARLIER DRAWING of a pane's area, told the
/// pane's role as the window that shows the desk tells it: at the cell `row`, `column` of the
/// pane's rows in a window of cells, `part` the part drawn under the hand then and `drawing` that
/// drawing's number. What a case says here is what the window said when the press landed.
void press_as_drawn(PaneRig& r, std::int64_t kind, std::int64_t row, std::int64_t column,
                           const std::string& part, std::int64_t drawing = 0,
                           std::int64_t button = 1);

/// POINT THE KEYBOARD AT A PANE WITHOUT ALSO AUTHORING A GESTURE. The keyboard goes to the pane a
/// weaver last pressed into and there is no shape for asking, so this spends a press
/// on a row whose provider-side meaning is NOTHING -- the bound sentence where there is one, the
/// last row otherwise -- possible only because every target in this pane means exactly one thing.
void focus_pane(PaneRig& r, std::int64_t kind);

/// Which column of the Powers chrome row carries a given word, or -1 -- how a case
/// aims at the `Operators` control without knowing the composition's arithmetic.
std::int64_t chrome_column(PaneRig& r, std::int64_t kind, const std::string& word);

/// MAKE ONE PANE TALLER, the way a weaver can: an authored height in canvas cells, written into
/// the setup the screen resolves its bounds from. Not a test door: `kStackRows` (nine -- eight
/// prose rows under one header) is the developer's default, not a law, and a projection whose
/// entries are several rows tall does not fit six artifacts in eight rows; it counts what it
/// could not show and gives the weaver a pane that grows.
void make_taller(PaneRig& r, const char* pane, std::int64_t cells);

std::string file_source(const char* path);

/// THE PRESENTATION, AS A POPULATION OF SOURCE FILES, for the tripwires that read Workshop as
/// text: every `.hpp` and `.cpp` directly in `WORKSHOP_SOURCE_DIR` but the host's side of the seam,
/// whose whole business is what the presentation must not reach -- a tripwire that read those
/// would forbid a door from naming what it opens. WALKED, NEVER ENUMERATED: a list fails open when
/// a body moves into a new file, and a walk holds the file the moment it exists. A walk that finds
/// fewer than the floor is a red, never a smaller claim; it goes down only with the files moved.
inline constexpr std::size_t kPresentationSourceFloor = 62;

std::vector<std::string> presentation_sources();

/// A keymap file's bytes, composed through the format's own serializer -- which is also what
/// makes the round-trip cases byte-exact rather than approximately so.
std::string keymap_file_text(const std::string& legend,
                                    const std::vector<std::pair<std::string, std::string>>& rows);

void write_keymap_file(const std::string& path, const std::string& text);

/// A Live Workshop that read the given keymap file on its first surface.
struct Keyed : Live {
    explicit Keyed(const std::string& path);
};

// Author an explicit TUI body budget for a behavior fixture. Preferences remain the
// default-size witnesses' subject; a larger window alone need not change a preferred pane.
template<class Rig>
void author_test_pane_room(Rig& r, std::int64_t kind, std::int64_t rows, std::int64_t columns) {
    bool found = false;
    stage_desk(r, [&](Setup& staged) {
        for (auto& row : staged.panes) {
            if (resolve_pane(row.ref, r.session().panes) != kind) continue;
            row.width = {pane_unit::kPixels, cells_px(columns + 2)};
            row.height = {pane_unit::kPixels, cells_px(rows + 3)};
            found = true;
        }
    });
    REQUIRE(found);
}

#endif // ZENGINE_TESTS_WORKSHOP_SUPPORT_HPP
