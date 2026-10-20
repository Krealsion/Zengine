// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Neovim-backed Editor: a loadable weave that holds the Editor's office with a Neovim it
// runs. It holds `zengine.editor`, offers the Editor's pane key and answers every door the
// standard Editor answers -- a managed opening's preparation and publication, the old door's
// relay, the orderly quit, the clipboard, the project root and both halves of an editor switch's
// handoff -- so Files, the Builder and Edit Code reach it by reaching the office (WL-NVIM-01).
// Workshop law: agents/workshop/neovim.md

// Neovim owns the buffers, their bytes, the modes, undo, registers and the screen. This weave
// owns the process (`neovim::Host`), the translation between the pane protocol and Neovim
// (`editor/neovim/projection.hpp`), and the Editor's facts as the office needs them: the identity it
// claims (path, changedtick, modified), the generation its rows carry, the transfer at a switch.

// The flow is never waited on (WL-NVIM-05): keys and text go as notifications, and the screen is
// said on the next beat (the Timer's while this weave holds the office, the coordinator's tick
// while it warms). A question is asked within a bound, in the one delivery that needs it, with
// Neovim's fast mode asked beside it (`Host::call_now`); a start answers later, except where an
// answer needs it, and then waits at most `kStartWaitMs`.

// A change is this Editor's until Neovim answers it (WL-NVIM-13): a request Neovim holds is never
// withdrawn, so an unanswered change stays held, one at a time, said on the status row; its whole
// target travels with it to be checked when it runs, and its outcome is said once.

// Hiding the pane ends nothing; a switch away ends Neovim once the successor serves; `:qa` leaves
// the office held with no document; the orderly quit is refused while a buffer is unsaved; and a
// reload is refused while Neovim runs (`snapshot`), since the process is its incarnation's.

// The class is declared here with its lifecycle, its beat and pump, its keys, its quit, its
// clipboard and the pane it draws. opening_switch.hpp defines its opening and switching doors and
// transfers.hpp its pointer and transfers, both included after it; common.hpp holds what the
// three share: the bounds and names, the arithmetic and the records of its conversations.

#include "editor/neovim-editor/common.hpp"
#include "editor/neovim-editor/vocabulary.hpp"

#include "editor/neovim/host.hpp"
#include "editor/neovim/keys.hpp"
#include "editor/neovim/launch.hpp"
#include "editor/neovim/lua.hpp"
#include "editor/neovim/projection.hpp"

#include "editor/source-transfer/material.hpp"
#include "workshop/editor_handoff_vocabulary.hpp"
#include "workshop/open_seam_vocabulary.hpp"
#include "workshop/pane_canvas_rows.hpp"
#include "workshop/pane_carry.hpp"
#include "workshop/pane_causes.hpp"
#include "workshop/pane_document.hpp"
#include "workshop/pane_menu.hpp"
#include "workshop/pane_operation.hpp"
#include "workshop/pane_seam_vocabulary.hpp"
#include "workshop/pane_text.hpp"
#include "workshop/pane_vocabulary.hpp"

#include "activation/activation.hpp"
#include "graphics/vocabulary.hpp"
#include "input/vocabulary.hpp"
#include "surface/vocabulary.hpp"
#include "timer/vocabulary.hpp"

#include "workshop/pane_settings.hpp"
#include "workshop/pane_settings_taken.hpp"

#include "manuals/zengine.editor.hpp" // the Editor's manual, compiled in from its page

#include <zen/kernel/export.hpp>
#include <zen/weave.hpp>
#include <zen/weave/standard_shapes.hpp>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

namespace graphics = zengine::graphics;
namespace nv = zengine::neovim;
namespace mp = zengine::neovim::msgpack;
namespace nve = zengine::neovim_editor;
namespace st = zengine::source_transfer;
namespace surface = zengine::surface;
namespace timer = zengine::timer;
namespace ws = zengine::workshop;

using ws::EditorAdopted;
using ws::EditorAdoptRequested;
using ws::EditorDocument;
using ws::EditorHandoffEnded;
using ws::EditorHandoffJudged;
using ws::EditorHandoffJudgeRequested;
using ws::EditorHandoffOffered;
using ws::EditorHandoffRequested;
using ws::EditorLive;
using ws::EditorLiveRequested;
using ws::EditorPreparationTick;
using ws::EditorRetired;
using ws::EditorRetireRequested;
using ws::EditorTransfer;
using ws::EditorWarmed;
using ws::EditorWarmRequested;
using ws::ManagedOpenProgress;
using ws::ManagedOpenSettled;
using ws::OpenSourceRequested;
using ws::PaneActionRequested;
using ws::PaneCatalogRequested;
using ws::PaneKey;
using ws::v2::PaneOffered;
using ws::PaneQuitAnswered;
using ws::PaneQuitRequested;
using ws::PaneRoom;
using ws::PaneTextInput;
using ws::PrepareSourceRequested;
using ws::ProjectRoot;
using ws::ProjectRootRequested;
using ws::SourceOpened;
using ws::SourcePrepared;
using zengine::workshop::pane_text::drawable;
using zengine::workshop::pane_text::fit;

using namespace zengine::neovim_editor::detail;

using NeovimEditorBase = zengine::With<ws::PaneManualFacet<
    nve::kEditorPane, zengine::manual::pages::zengine_editor::kManual>>::Weave<
    class NeovimEditorWeave, nve::NeovimEditorState,
    loom::Accept<loom::Activated, timer::TimerReady, timer::TimerFired, timer::TimerResolution,
                 PaneCatalogRequested, PaneRoom, ws::PaneCanvasRoom, ws::PaneCanvasPointer,
                 ws::PaneCanvasRejected, PaneKey, PaneTextInput, PaneActionRequested, PaneQuitRequested,
                 OpenSourceRequested, PrepareSourceRequested, ws::v2::PrepareSourceRequested,
                 ManagedOpenProgress, ManagedOpenSettled,
                 SourceOpened, loom::DispatchRefused, ProjectRoot, surface::ClipboardCopy,
                 surface::ClipboardText, EditorHandoffJudgeRequested, EditorWarmRequested,
                 EditorPreparationTick, EditorHandoffRequested, EditorHandoffEnded,
                 EditorAdoptRequested, EditorLiveRequested, EditorRetireRequested,
                 nve::NeovimStartRequested, nve::NeovimStopRequested, nve::NeovimStatusRequested,
                 ws::PaneCanvasValueDrop, ws::PaneMenuAnswered, ws::PaneOperationAnswered,
                 ws::PaneCarryAnswered, graphics::AreaRoom, graphics::DrawnRefused,
                 graphics::ActionRequested, graphics::AreaButtonChanged, graphics::AreaPointerMoved,
                 graphics::AreaWheelTurned, graphics::AreaHoldEnded, ws::v2::PaneSettings>,
    loom::Emit<PaneOffered, ws::v3::PaneActions, ws::v5::PaneCanvasContent, ws::v4::PaneContent,
               ws::v2::PaneCaret,
               ws::PaneEscapeUnspent, // declared so Escape reaches it, which it keeps
               PaneQuitAnswered, SourceOpened, SourcePrepared, ws::v2::SourcePrepared,
               OpenSourceRequested,
               ProjectRootRequested, surface::ClipboardCopy, surface::ClipboardTextRequested,
               EditorHandoffJudged, EditorWarmed, EditorHandoffOffered, EditorAdopted, EditorLive,
               EditorRetired, timer::EnsureTimer, timer::CancelTimer, loom::Result, loom::Refused,
               ws::PaneMenuRequested, ws::PaneOperationRequested, ws::PaneValueCarryRequested,
               graphics::Drawn, graphics::RoomAsked, ws::v2::PaneSettingsDeclared,
               ws::PaneSettingUnused>,
    loom::WeaveBase, loom::Claims<EditorDocument>>;

// WL-NVIM-01 -- agents/workshop/neovim.md
class NeovimEditorWeave : public NeovimEditorBase {
public:
    // ---- A reload, refused while Neovim runs ---------------------------------------------------

    /// THE SNAPSHOT A RELOAD CARRIES -- refused while a Neovim runs. The process, its buffers, its
    /// undo history and its unsaved changes belong to THIS incarnation, and a reload ends it: the
    /// honest answer is no, in words, until the weaver ends Neovim or switches editors. The words
    /// also stand on the pane, because the kernel's own sentence about a refused snapshot is about
    /// a library, not about a weaver's work.
    // WL-NVIM-06 -- agents/workshop/neovim.md
    loom::Value snapshot() const override {
        if (host_ != nullptr && host_->alive()) {
            refused_reload_ = true;
            throw std::runtime_error("the Neovim editor runs a Neovim, and a reload would end it -- "
                                     "quit Neovim (:qa) or switch editors, then reload");
        }
        return NeovimEditorBase::snapshot();
    }

    void revive(const loom::Value& v) override {
        NeovimEditorBase::revive(v);
        project_dir_ = state_.project_dir;
        project_known_ = state_.project_known;
        notice_ = state_.notice;
        notice_bad_ = state_.notice_bad;
        epoch_ = state_.doc_epoch;
    }

    // ---- Being the office ----------------------------------------------------------------------

    void on(const loom::Activated& a, loom::Mail& mail) {
        if (!activation_.accept(mail, a)) {
            return;
        }
        ensure_beat(mail);
        if (adopted_) {
            notice((doc_.modified ? "switched editors -- UNSAVED edits stand in " : "switched editors -- editing ") +
                       (doc_path().empty() ? std::string("no source") : shown_path(doc_path())),
                   false);
        }
        announce(mail);
        ask_project_root(mail);
        // THE GRAPHICS WINDOW THAT SHOWS THE OFFICE'S AREA, asked for its room once each life; a
        // room it said before this activation is taken now.
        window_.ask_room(mail);
        if (window_.in_window() && !granted_) {
            size_to(window_.rows(), window_.columns(), mail);
        }
        resay_ = true;
    }

    void on(const timer::TimerReady&, loom::Mail& mail) {
        if (activation_.activated()) {
            ensure_beat(mail);
        }
    }

    void on(const timer::TimerFired& fired, loom::Mail& mail) {
        if (fired.id != kBeatId || !activation_.activated()) {
            return;
        }
        pump(mail);
    }

    /// WHAT THE TIMER DID ABOUT THE BEAT. A refusal is said on the pane: without the beat, Neovim's
    /// screen reaches the pane only when a key does.
    void on(const timer::TimerResolution& r, loom::Mail& mail) {
        if (r.id != kBeatId || r.resolved != timer::kResolutionRefused) {
            return;
        }
        notice("the Timer refused this editor's beat (" + r.reason +
                   ") -- Neovim's screen is redrawn here only when a key reaches it",
               true);
        say(mail);
    }

    void on(const PaneCatalogRequested&, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole)) {
            return;
        }
        announce(mail);
    }

    /// THE PROSE ROOM: the rows and columns this pane and Neovim's screen are sized to where its
    /// host grants it no canvas. Once a canvas room is granted the canvas decides, and this is not
    /// spent.
    void on(const PaneRoom& room, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || room.pane != nve::kEditorPane) {
            return;
        }
        if (canvas_.grant > 0) {
            return; // the canvas room, granted first, already sized Neovim
        }
        take_room(room.rows, room.columns, mail);
    }

    /// THE CANVAS ROOM: this pane draws its status row and Neovim's screen there as its own
    /// picture, Neovim sized to the lattice. A new grant ends the presses its predecessor held; a
    /// room with no extent draws nothing and resizes nothing, so hiding the pane ends nothing.
    void on(const ws::PaneCanvasRoom& room, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || room.pane != nve::kEditorPane) {
            return;
        }
        if (room.grant != canvas_.grant) {
            pressed_ = 0;
            release_drag();
            if (right_.down && running()) {
                mouse("right", "release", right_.row, right_.col);
            }
            right_ = Drag{};
            if (!grab_.started) {
                grab_ = Grab{};
            }
        }
        canvas_ = room;
        if (!on_canvas()) {
            return;
        }
        const ws::CanvasRows lattice = ws::canvas_rows(canvas_);
        take_room(lattice.rows, lattice.columns, mail);
    }

    /// A refused picture leaves the last good one showing; the next change draws again.
    void on(const ws::PaneCanvasRejected&, loom::Mail&) {}

    /// THE SETTINGS THIS LAYOUT KEEPS FOR THE PANE, taken whole; what it cannot use is said back
    /// against the revision it was handed at. The profile is the next start's, never a running
    /// Neovim's; the status row moves Neovim's screen at once. Held as plain members, never reload
    /// state: a reloaded image is handed them again before its room.
    void on(const ws::v2::PaneSettings& handed, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || handed.pane != nve::kEditorPane) {
            return;
        }
        const ws::SettingsTaken taken = ws::take_pane_settings(setting_rows(), handed);
        ws::say_settings_unused(mail, nve::kEditorOffice, taken);
        profile_ = taken.text(nve::kSettingProfile);
        status_shown_ = taken.flag(nve::kSettingStatus);
        say(mail); // draws nothing until a room is granted, so the first picture has them
    }

    /// THE SETTINGS THE PANE TAKES, each with its default, how a weaver finds it and what it does:
    /// the profile, pinned, one of the two words the variable's own default is among, and the
    /// status row.
    static std::vector<ws::v2::PaneSettingRow> setting_rows() {
        ws::v2::PaneSettingRow profile;
        profile.key = nve::kSettingProfile;
        profile.text = std::string(nv::LaunchChoice{}.profile);
        profile.choices = {"clean", "user"};
        profile.tags = {"editing"};
        profile.pinned = true;
        profile.summary = "Neovim profile name when ZENGINE_NEOVIM_PROFILE is unset";
        ws::v2::PaneSettingRow status;
        status.key = nve::kSettingStatus;
        status.flag = true;
        status.tags = {"chrome"};
        status.summary = "show the status row above Neovim";
        return {profile, status};
    }

    /// THE ROWS DRAWN ABOVE NEOVIM'S SCREEN: the status row, or a notice in its place -- none while a
    /// layout keeps `status` off and nothing stands there, once a Neovim is up to show the room.
    std::int64_t chrome_rows() const {
        return status_shown_ || !notice_.empty() || !running() || !host_->ready() ? kChromeRows : 0;
    }

    /// NEOVIM'S SCREEN IN THE ROOM COMPOSED FOR, under those rows.
    std::int64_t screen_rows() const { return ui_rows(rows_, chrome_rows()); }

    /// THE ROOM THE GRAPHICS WINDOW THAT SHOWS THE OFFICE'S AREA GIVES, from that window's office.
    /// An Editor Workshop seats keeps Neovim sized to Workshop's room, and the window is sent those
    /// rows; one Workshop seats nowhere sizes Neovim to the window's room, or starts it there.
    void on(const graphics::AreaRoom& r, loom::Mail& mail) {
        if (!window_.room(r, mail) || !activation_.activated()) {
            return; // a room said before the activation is taken there
        }
        if (granted_) {
            say(mail);
            return;
        }
        size_to(window_.rows(), window_.columns(), mail);
    }

    /// A drawing the window refused (`EditorWindow::refused`): a refused change is followed by the
    /// rows whole, a refused whole drawing by nothing until they change.
    void on(const graphics::DrawnRefused& refused, loom::Mail& mail) {
        (void)window_.refused(refused, mail);
    }

    /// THE POINTER IN THE WINDOW, given by the window that shows the area as its office and read
    /// onto the rows drawn there: Neovim's own mouse, as on Workshop's canvas (`pointer`), its
    /// press, moves and release under the window's number for the press, a hold the window ended a
    /// loss.
    void on(const graphics::AreaButtonChanged& b, loom::Mail& mail) { on_window(window_.pointer(b, mail), mail); }
    void on(const graphics::AreaPointerMoved& m, loom::Mail& mail) { on_window(window_.pointer(m, mail), mail); }
    void on(const graphics::AreaWheelTurned& w, loom::Mail& mail) { on_window(window_.pointer(w, mail), mail); }
    void on(const graphics::AreaHoldEnded& e, loom::Mail& mail) {
        on_window(window_.pointer(e, mail), mail);
    }

    /// A PRESS ASKED AS A PART'S ACTION, by a window that shows the area placed deaf to the pointer:
    /// Neovim's own press at the first column of that part, on no picture a carry could begin from,
    /// and released at once, since the window says the press and its release as one act.
    void on(const graphics::ActionRequested& asked, loom::Mail& mail) {
        if (const std::optional<ws::RowCell> at = window_.part(asked, mail)) {
            press_at(at->row, at->column, 0, 0, mail);
            release_drag();
            flush(mail);
        }
    }

    /// THE ROOM TAKEN from Workshop: the rows and columns composed for (`size_to`).
    void take_room(std::int64_t rows, std::int64_t columns, loom::Mail& mail) {
        granted_ = true;
        size_to(rows, columns, mail);
    }

    /// THE ROWS AND COLUMNS COMPOSED FOR, and Neovim sized to them -- or started in them, the first
    /// time this pane is given room.
    void size_to(std::int64_t rows, std::int64_t columns, loom::Mail& mail) {
        rows_ = rows;
        columns_ = columns;
        if (!root_asked_) {
            ask_project_root(mail);
        }
        if (running()) {
            (void)host_->resize(screen_rows(), ui_cols(columns_));
        } else if (!start_tried_) {
            // THE PANE WAS GIVEN ROOM AND NO NEOVIM RUNS YET: start one now, in this room. Once --
            // a start that failed is said on the pane and retried by the weaver's next open, never by
            // the next room grant.
            (void)start_embedded(screen_rows(), ui_cols(columns_));
        }
        say(mail);
    }

    void on(const ProjectRoot& said, loom::Mail& mail) {
        if (!mail.answers_ask() || mail.correlation() != root_pending_) {
            return;
        }
        project_dir_ = said.project_dir;
        project_known_ = true;
    }

    // ---- The keys, the text, the pointer -------------------------------------------------------

    void on(const PaneKey& key, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || key.pane != nve::kEditorPane) {
            return;
        }
        if (held_still()) {
            return;
        }
        const std::string keys = nv::key_input(key.scancode, key.modifiers);
        if (keys.empty()) {
            return;
        }
        send_input(mail, keys);
    }

    void on(const PaneTextInput& typed, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || typed.pane != nve::kEditorPane) {
            return;
        }
        if (held_still() || typed.text.empty()) {
            return;
        }
        send_input(mail, nv::text_input(typed.text));
    }

    /// THE TWO ROWS THIS PANE DECLARED, by name: write, and Neovim's own jump back.
    void on(const PaneActionRequested& asked, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || asked.pane != nve::kEditorPane) {
            return;
        }
        if (held_still()) {
            return;
        }
        if (asked.id == nve::kActionWrite) {
            send_input(mail, "<Cmd>write<CR>");
        } else if (asked.id == nve::kActionJumpOlder) {
            send_input(mail, "<C-o>");
        } else if (asked.id == nve::kActionExtract) {
            // THE KEYBOARD ROUTE (WL-NVIM-10), declared only while a selection stands. The weaver may
            // have left Visual mode after the row was declared: then `ctrl+r` was Neovim's own key
            // after all (redo, or Insert's register), and goes to Neovim as one.
            grab_ = Grab{};
            flush(mail);
            if (!carries_selection()) {
                send_input(mail, "<C-r>");
                return;
            }
            const Snapshot snap = snapshot_now();
            if (!snap.ok) {
                notice("nothing was carried -- " + snap.refusal, true);
            } else {
                carry_snapshot(snap, false, mail.correlation(), mail);
            }
            resay_ = true;
        } else if (asked.id == nve::kActionLocation) {
            // A ROW WITH NO DEFAULT KEY: reached only by a chord the weaver bound (WL-NVIM-12).
            grab_ = Grab{};
            flush(mail);
            acquire_location(false, mail.correlation(), mail);
            resay_ = true;
        }
    }

    // ---- The pointer, in transfers.hpp ----------------------------------------------------------

    void on(const ws::PaneCanvasPointer& event, loom::Mail& mail);
    void on_window(const std::optional<ws::WindowPointer>& p, loom::Mail& mail);
    void pointer(std::int64_t phase, std::int64_t button, const ws::RowCell& at,
                 std::int64_t gesture, bool same_room, std::int64_t grant, std::int64_t picture,
                 double dy, loom::Mail& mail);
    void press_at(std::int64_t row, std::int64_t column, std::int64_t grant, std::int64_t picture,
                  loom::Mail& mail);
    void dragged(std::int64_t row, std::int64_t column, loom::Mail& mail);
    void right_press(std::int64_t row, std::int64_t column, std::int64_t grant, std::int64_t picture,
                     loom::Mail& mail);
    void right_release(std::int64_t row, std::int64_t column, loom::Mail& mail);
    void wheel(double dy, loom::Mail& mail);

    // ---- Transfers, in transfers.hpp: the selection out, material in, a location opened ---------

    void on(const ws::PaneCanvasValueDrop& drop, loom::Mail& mail);
    void on(const ws::PaneMenuAnswered& answer, loom::Mail& mail);
    void on(const ws::PaneOperationAnswered& answer, loom::Mail& mail);
    void on(const ws::PaneCarryAnswered& answer, loom::Mail& mail);

    // ---- The exit --------------------------------------------------------------------------------

    /// MAY THE WORKSHOP END? Asked of Neovim, now: refused while a buffer holds unsaved changes or a
    /// terminal job runs (both are named), while Neovim waits at a prompt or in an unfinished command
    /// (it could not be asked), and while a switch holds this editor still or an open is in flight.
    void on(const PaneQuitRequested&, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole)) {
            return;
        }
        const auto answer = [&mail](bool permitted, std::string why) {
            (void)mail.answer(PaneQuitAnswered{nve::kEditorPane, permitted, std::move(why)});
        };
        if (holding_.active) {
            answer(false, "the Editor is being switched -- quit again once the switch has settled");
            return;
        }
        if (candidate_.live) {
            answer(false, "Neovim is still opening " + candidate_.path + " -- quit again once it has settled");
            return;
        }
        if (!running()) {
            answer(true, std::string());
            return;
        }
        std::string why;
        const std::optional<mp::Value> unsaved = lua_now(nv::lua::kUnsaved, nv::rpc::params(), kAskMs, why);
        if (!unsaved.has_value()) {
            answer(false, "Neovim could not be asked whether it holds unsaved work (" + why +
                              ") -- Workshop stays open");
            return;
        }
        std::vector<std::string> named;
        if (const mp::Value* buffers = unsaved->get("buffers"); buffers != nullptr) {
            for (const mp::Value& b : buffers->as_array()) {
                const mp::Value* n = b.get("name");
                named.push_back(n == nullptr || n->as_str().empty() ? std::string("[No Name]") : spelled(n->as_str()));
            }
        }
        std::vector<std::string> jobs;
        if (const mp::Value* running_jobs = unsaved->get("jobs"); running_jobs != nullptr) {
            for (const mp::Value& j : running_jobs->as_array()) {
                const mp::Value* n = j.get("name");
                jobs.push_back(n == nullptr ? std::string("a terminal") : n->as_str());
            }
        }
        if (!named.empty()) {
            answer(false, "Neovim holds unsaved changes to " + listed(named) +
                              " -- write them (:w) or discard them in Neovim first; Workshop stays open");
            return;
        }
        if (!jobs.empty()) {
            answer(false, "a terminal job is running in Neovim (" + listed(jobs) +
                              ") -- end it first; Workshop stays open");
            return;
        }
        answer(true, std::string());
    }

    // ---- The clipboard ---------------------------------------------------------------------------

    void on(const surface::ClipboardCopy& said, loom::Mail&) { clip_ = said.text; }

    /// THE SKIN'S ANSWER TO A PASTE NEOVIM ASKED FOR, handed to the request Neovim is waiting on.
    void on(const surface::ClipboardText& a, loom::Mail& mail) {
        if (!mail.answers_ask()) {
            return;
        }
        const auto it = pastes_.find(mail.correlation());
        if (it == pastes_.end()) {
            return;
        }
        const std::uint32_t id = it->second;
        pastes_.erase(it);
        if (a.readable) {
            clip_ = a.text;
        }
        if (running()) {
            const PasteLines paste = paste_lines(clip_);
            (void)host_->answer_paste(id, paste.lines, paste.regtype);
        }
    }

    // ---- THE TWO OPEN DOORS (WL-OPEN), in opening_switch.hpp ------------------------------------

    void on(const OpenSourceRequested& asked, loom::Mail& mail);
    void on(const SourceOpened& said, loom::Mail& mail);
    void on(const loom::DispatchRefused& refused, loom::Mail& mail);
    void on(const ManagedOpenProgress&, loom::Mail&);
    void on(const PrepareSourceRequested& asked, loom::Mail& mail);
    void on(const ws::v2::PrepareSourceRequested& asked, loom::Mail& mail);
    SourcePrepared prepare(std::int64_t op, const std::string& asked_path, std::int64_t rows,
                           std::int64_t columns, const ws::PaneCanvasRoom& room, loom::Mail& mail);
    void on(const ManagedOpenSettled& said, loom::Mail& mail);
    bool on_claim_published(const EditorDocument& published);

    void after_delivery(loom::Mail& mail) {
        if (refused_reload_) {
            refused_reload_ = false;
            notice("a reload of the Neovim editor was refused: it would end the running Neovim -- quit Neovim "
                   "(:qa) or switch editors first",
                   true);
            resay_ = true;
        }
        mirror_state();
        if (!activation_.activated()) {
            return;
        }
        if (resay_) {
            resay_ = false;
            say(mail);
        }
        claim_document(mail);
    }

    // ---- A switch: this editor as the incumbent (WL-SWITCH-04), in opening_switch.hpp -----------

    void on(const EditorHandoffJudgeRequested& asked, loom::Mail& mail);
    void on(const EditorHandoffRequested& asked, loom::Mail& mail);
    void on(const EditorHandoffEnded& ended, loom::Mail& mail);
    void on(const EditorRetireRequested& asked, loom::Mail& mail);

    // ---- A switch: this editor as the candidate, sealed (WL-SWITCH-05), in opening_switch.hpp ---

    void on(const EditorWarmRequested& asked, loom::Mail& mail);
    void on(const EditorPreparationTick&, loom::Mail& mail);
    void on(const EditorAdoptRequested& asked, loom::Mail& mail);
    void on(const EditorLiveRequested& asked, loom::Mail& mail);

    // ---- From a baseline Loom: a second terminal's Neovim, in opening_switch.hpp ----------------

    void on(const nve::NeovimStartRequested& asked, loom::Mail& mail);
    void on(const nve::NeovimStopRequested& asked, loom::Mail& mail);
    void on(const nve::NeovimStatusRequested&, loom::Mail& mail);

private:
    // ---- The Neovim --------------------------------------------------------------------------

    bool running() const noexcept { return host_ != nullptr && host_->alive(); }

    std::string doc_path() const {
        return doc_.known && doc_.buftype.empty() && !doc_.name.empty() ? spelled(doc_.name) : std::string();
    }

    /// START ONE NEOVIM, attached as this pane's interface.
    bool start_embedded(std::int64_t rows, std::int64_t cols) {
        listen_.clear();
        return start(nv::UiMode::Embedded, rows, cols);
    }

    bool start(nv::UiMode mode, std::int64_t rows, std::int64_t cols) {
        start_tried_ = true;
        choice_ = nv::choice_from_environment(profile_);
        // WHICH CONFIGURATION, JUDGED BEFORE A NEOVIM EXISTS. A profile that is neither spelling
        // and names no file is the weaver's own typo, and saying so here is the difference between
        // a sentence they can act on and Neovim starting bare behind an `E282` prompt.
        const nv::ProfileChoice judged = nv::check_profile(choice_);
        if (!judged.ok) {
            failure_ = judged.refusal;
            notice(failure_, true);
            resay_ = true;
            return false;
        }
        if (!judged.resolved.empty()) {
            choice_.profile = judged.resolved; // said in full wherever the profile is said
        }
        host_ = std::make_unique<nv::Host>();
        preview_ = Preview{}; // a preview counts one Neovim's flushes, and this is another
        doc_ = nv::DocFacts{};
        nv::Host::Options o;
        const std::string cwd = project_known_ && !project_dir_.empty() ? project_dir_ : std::string();
        o.launch = nv::launch_spec(choice_, mode, listen_, cwd);
        o.attach_ui = mode == nv::UiMode::Embedded;
        o.rows = rows;
        o.columns = cols;
        ui_ = mode == nv::UiMode::Embedded ? "pane" : "remote";
        ++state_.starts;
        failure_.clear();
        if (!host_->start(o)) {
            failure_ = host_->failure();
            notice(failure_, true);
            resay_ = true;
            return false;
        }
        return true;
    }

    /// WAIT FOR A STARTING NEOVIM TO ANSWER, at most `kStartWaitMs`.
    bool wait_ready(std::string& why) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(kStartWaitMs);
        while (running() && !host_->ready()) {
            carried_.merge(host_->pump());
            if (std::chrono::steady_clock::now() >= deadline) {
                why = "Neovim did not become ready within " + std::to_string(kStartWaitMs / 1000) + " seconds";
                return false;
            }
            (void)host_->wait(10);
        }
        if (!running()) {
            failure_ = host_ != nullptr ? host_->failure() : std::string("Neovim is not running");
            why = failure_;
            return false;
        }
        return true;
    }

    /// A NEOVIM, RUNNING AND READY, for an answer that needs one now.
    bool ensure_running(std::string& why) {
        if (!running() && !start_embedded(ui_rows(rows_ > 0 ? rows_ : kDefaultRows, chrome_rows()),
                                          ui_cols(columns_ > 0 ? columns_ : kDefaultCols))) {
            why = failure_;
            return false;
        }
        return wait_ready(why);
    }

    std::string started_words() const {
        if (host_ == nullptr) {
            return "Neovim";
        }
        std::string said = "Neovim " + host_->version().text() + " (" + nv::profile_words(choice_) + ")";
        if (host_->ready() && !host_->version().tested()) {
            said += " -- an untested version";
        }
        return said;
    }

    std::string attached_words() const {
        return ui_ == "remote" ? "listening at " + listen_ + " for a remote interface" : std::string("in the Editor pane");
    }

    // What a switch is judged from, asked of Neovim now: in opening_switch.hpp.
    bool wait_unblocked(int ms);
    std::optional<Exported> export_now(std::string& why);
    Judgement judge_now();

    // A change Neovim holds until it answers (WL-NVIM-13): in transfers.hpp.
    void change(const char* chunk, mp::Value args, std::string what, const std::string& waiting, Settle settle);
    Heard heard_of(const nv::rpc::Response& r) const;
    bool settle_held();

    /// ONE QUESTION TO THE MODULE, answered within `ms` or refused in words.
    std::optional<mp::Value> lua_now(const char* chunk, mp::Value args, int ms, std::string& why) {
        if (!running()) {
            why = "Neovim is not running" + (failure_.empty() ? std::string() : ": " + failure_);
            return std::nullopt;
        }
        const std::optional<nv::rpc::Response> r =
            host_->call_now("nvim_exec_lua", nv::rpc::params(mp::Value::str(chunk), std::move(args)), ms, why);
        if (!r.has_value()) {
            return std::nullopt;
        }
        if (!r->error.is_nil()) {
            why = nv::rpc::error_text(r->error);
            return std::nullopt;
        }
        return r->result;
    }

    // ---- Transfer helpers, in transfers.hpp -----------------------------------------------------

    bool on_highlight(std::int64_t row, std::int64_t col) const;
    std::string grid_row(std::int64_t row) const;
    Snapshot snapshot_now();
    std::string project_root() const;
    void begin_pickup(const st::Pair& pair, bool drag, std::uint64_t gesture, std::string label, std::string what,
                      loom::Mail& mail);
    void carry_snapshot(const Snapshot& snap, bool drag, std::uint64_t gesture, loom::Mail& mail);
    void acquire_location(bool drag, std::uint64_t gesture, loom::Mail& mail);
    void receive(const loom::Bytes& data, const ws::RowCell& drop, std::int64_t grant, std::int64_t picture,
                 loom::Mail& mail);
    void insert_lines(const std::string& text, const Aim& aim, Insertion said, bool whole = false);
    void settle_insertion(const Insertion& said, const Heard& h);
    void insert_command(const st::Material& m, const Aim& aim);
    void insert_cpp(const st::Material& m, const Aim& aim);
    void open_location(const st::Material& m, loom::Mail& mail);
    void settle_location(const SourceOpened& said);

    // ---- The beat, the pump and what it observed --------------------------------------------

    /// THE BEAT, ORDERED IN THE TIMER'S OWN WORDS: the binding layer's default continuity -- keep
    /// the remaining time of a standing schedule, accept a restart -- spelled by the Timer
    /// vocabulary, because the Timer refuses an order whose continuity it cannot read.
    void ensure_beat(loom::Mail& mail) {
        const timer::ContinuityOrder order;
        (void)mail.send_to_role(timer::kTimerRole,
                                timer::EnsureTimer{kBeatId, kBeatMs, true, timer::spelling_of(order.preferred),
                                                   timer::fallback_spelling(order)});
    }

    void pump(loom::Mail& mail) {
        if (host_ == nullptr) {
            return;
        }
        nv::Observed seen = std::move(carried_);
        carried_ = nv::Observed{};
        if (host_->phase() != nv::Host::Phase::Idle) {
            seen.merge(host_->pump());
        }
        absorb(mail, std::move(seen));
    }

    // WL-NVIM-07 -- agents/workshop/neovim.md
    void absorb(loom::Mail& mail, nv::Observed seen) {
        if (seen.became_ready) {
            notice(started_words() + " is ready", false);
        }
        if (seen.failed || seen.ended) {
            failure_ = host_->failure();
            doc_ = nv::DocFacts{};
            notice(failure_ + " -- the Editor holds no document; open a source to start Neovim again", !host_->leaving());
            // A PASTE NEOVIM WAS WAITING ON can no longer be answered.
            pastes_.clear();
        }
        if (seen.doc_changed) {
            doc_ = host_->doc();
        }
        (void)settle_held(); // a change Neovim held, answered since -- or ended with it (WL-NVIM-13)
        for (const nv::ClipboardCopy& copy : seen.copies) {
            clip_ = join_lines(copy.lines, copy.regtype == "V" || copy.regtype == "line");
            (void)mail.publish(surface::ClipboardCopy{clip_});
        }
        for (const std::uint32_t id : seen.paste_requests) {
            const std::uint64_t correlation = ++asked_;
            const loom::Ticket asked = mail.as_role(nve::kEditorOffice)
                                           .send_to_role(window_.clipboard_office(), surface::ClipboardTextRequested{}, correlation);
            if (asked.valid()) {
                pastes_.emplace(correlation, id);
            } else {
                const PasteLines paste = paste_lines(clip_); // unsent: the mirror is the clipboard
                (void)host_->answer_paste(id, paste.lines, paste.regtype);
            }
        }
        settle_warm(mail);
        if (activation_.activated() && carries_selection() != declared_selection_) {
            declare(mail); // `ctrl+r` follows Neovim's mode (WL-NVIM-10)
        }
        if (seen.flushed || seen.mode_changed || seen.doc_changed || seen.became_ready || seen.failed || seen.ended) {
            resay_ = true;
        }
    }

    // The warm-up's answer, once Neovim is ready or has ended: in opening_switch.hpp.
    void settle_warm(loom::Mail& mail);

    // ---- Input -------------------------------------------------------------------------------

    bool held_still() {
        if (!holding_.active) {
            return false;
        }
        ++holding_.refused;
        return true;
    }

    void send_input(loom::Mail& mail, const std::string& keys) {
        if (!running()) {
            notice("Neovim is not running" + (failure_.empty() ? std::string() : " (" + failure_ + ")") +
                       " -- open a source to start it",
                   true);
            say(mail);
            return;
        }
        release_drag();
        notice_.clear();
        (void)host_->input(keys);
        flush(mail);
    }

    /// WRITE WHAT WAS SAID NOW, rather than on the next beat, and apply whatever already came back.
    void flush(loom::Mail& mail) { pump(mail); }

    void mouse(const char* button, const char* action, std::int64_t row, std::int64_t col) {
        (void)host_->notify("nvim_input_mouse",
                            nv::rpc::params(mp::Value::str(button), mp::Value::str(action), mp::Value::str(""),
                                            mp::Value::integer(0), mp::Value::integer(row), mp::Value::integer(col)));
    }

    void release_drag() {
        if (drag_.down && running()) {
            mouse("left", "release", drag_.row, drag_.col);
        }
        drag_ = Drag{};
    }

    // ---- The pane ----------------------------------------------------------------------------

    std::string shown_path(const std::string& path) const {
        return tail_of_path(path, columns_ > 24 ? columns_ - 24 : columns_);
    }

    void announce(loom::Mail& mail) {
        (void)mail.as_role(nve::kEditorOffice)
            .send_to_role(kWorkshopRole, PaneOffered{nve::kEditorPane, nve::kPaneName, nve::kPaneSummary, 20, 84});
        declare(mail);
        (void)mail.as_role(nve::kEditorOffice)
            .send_to_role(kWorkshopRole, ws::v2::PaneSettingsDeclared{nve::kEditorPane, setting_rows()});
    }

    /// Two rows naming Workshop's retired document rows as what they stand in for, so an older
    /// host that still declares them lets these keep their chords: save writes the buffer, and
    /// open is Neovim's own jump back. Every other key reaches Neovim, except `ctrl+r` while a
    /// Visual or Select selection stands, where it carries the selection (WL-NVIM-10); this
    /// file's location has no default key, every plain ctrl+letter being Neovim's or the
    /// desktop's (WL-NVIM-12).
    void declare(loom::Mail& mail) {
        ws::v3::PaneActions actions;
        actions.pane = nve::kEditorPane;
        actions.rows.push_back(ws::v3::PaneActionRow{nve::kActionWrite, "write (:w)", zengine::input::scan::kS,
                                                     zengine::input::mod::kCtrl, ws::kOwnableDocumentSave});
        actions.rows.push_back(ws::v3::PaneActionRow{nve::kActionJumpOlder, "jump back (<C-o>)",
                                                     zengine::input::scan::kO, zengine::input::mod::kCtrl,
                                                     ws::kOwnableDocumentOpen});
        // The two carries' rows say what their press may send: the carry, once Workshop allows it.
        const std::vector<ws::ActionCause> carried{ws::cause_of<ws::PaneValueCarryRequested>()};
        actions.rows.push_back(ws::v3::PaneActionRow{nve::kActionLocation, "carry this file's location",
                                                     zengine::input::scan::kUnknown, zengine::input::mod::kNone,
                                                     "", carried});
        declared_selection_ = carries_selection();
        if (declared_selection_) {
            actions.rows.push_back(ws::v3::PaneActionRow{nve::kActionExtract, "carry the selection",
                                                         zengine::input::scan::kR, zengine::input::mod::kCtrl, "",
                                                         carried});
        }
        (void)mail.as_role(nve::kEditorOffice).send_to_role(kWorkshopRole, actions);
    }

    /// DOES `ctrl+r` CARRY THE SELECTION IN NEOVIM'S MODE NOW? Only while Visual or Select holds one.
    bool carries_selection() const { return running() && host_->ready() && visual_mode(host_->mode()); }

    void ask_project_root(loom::Mail& mail) {
        root_asked_ = true;
        root_pending_ = ++asked_;
        (void)mail.as_role(nve::kEditorOffice).send_to_role(ws::kProjectRole, ProjectRootRequested{}, root_pending_);
    }

    void notice(std::string text, bool bad) {
        notice_ = std::move(text);
        notice_bad_ = bad;
    }

    /// `notice`, as the office's shared steps say one (`editor/office_seam.hpp`).
    auto noticing() {
        return [this](std::string text, bool bad) { notice(std::move(text), bad); };
    }

    std::string status_text(std::int64_t columns) const {
        if (!running()) {
            return "Neovim is not running" +
                   (failure_.empty() ? std::string(" -- open a source to start it") : " -- " + failure_);
        }
        if (!host_->ready()) {
            return "Neovim is starting (" + nv::profile_words(choice_) + ")";
        }
        const std::string path = doc_path();
        // ...AND WHICH CONFIGURATION IT IS RUNNING, because a weaver whose plugins are missing has
        // no other way to tell `clean` from `user` once Neovim is up (the full words are in the
        // start and status answers).
        std::string head = std::string(doc_.modified ? "UNSAVED " : "saved ") +
                           mode_word(host_->mode()) + " " + nv::profile_tag(choice_) + " -- ";
        if (held_.has_value()) {
            head += held_->what + " waits for Neovim -- "; // held until Neovim answers (WL-NVIM-13)
        }
        return head + (path.empty() ? std::string("no file") : tail_of_path(path, columns - static_cast<std::int64_t>(head.size())));
    }

    /// THE ONE PART THIS PANE NAMES, `status`: the row it draws above Neovim's screen, saying the
    /// status or a notice in its place. The screen beneath is Neovim's own, and named nothing.
    std::vector<ws::PaneRowPart> status_part(const std::vector<surface::SurfaceTextRow>& rows) const {
        return chrome_rows() > 0 ? status_part(rows, columns_) : std::vector<ws::PaneRowPart>{};
    }
    static std::vector<ws::PaneRowPart> status_part(const std::vector<surface::SurfaceTextRow>& rows,
                                                    std::int64_t columns) {
        if (rows.empty()) {
            return {};
        }
        return {ws::PaneRowPart{"status", 0, 0, columns}};
    }

    /// THE PANE, SAID: the status row (or a standing notice in its place), then Neovim's screen,
    /// cropped to the room, drawn as this pane's own picture with the caret and the one range
    /// standing in its rows -- or, to a host granting no canvas, said as prose with them beside.
    /// NUMBERED (WL-NVIM-11): a picture whose rows, caret, range or generation differ from the last
    /// one said is a new picture, so a press or a drop names which screen it was aimed at.
    void say(loom::Mail& mail) {
        // Workshop is told while it grants a room the pane can fill: none, or a canvas room with
        // no extent, is nothing its rows could truthfully fill.
        const bool to_workshop = granted_ && !(canvas_.grant > 0 && !on_canvas());
        const bool to_window = window_.in_window() && activation_.activated();
        if (!to_workshop && !to_window) {
            return;
        }
        // NEOVIM'S SCREEN FITS UNDER THE ROWS DRAWN ABOVE IT NOW: a notice standing or going, or a
        // layout's status row, moves it, and a size Neovim already has asks nothing.
        if (running() && rows_ > 0) {
            (void)host_->resize(screen_rows(), ui_cols(columns_));
        }
        std::vector<surface::SurfaceTextRow> rows;
        ws::v2::PaneCaret caret;
        caret.pane = nve::kEditorPane;
        caret.generation = epoch_;
        compose(rows, caret, rows_, columns_);
        const std::uint64_t hash = picture_hash(rows, caret);
        if (picture_ == 0 || hash != picture_hash_) {
            picture_hash_ = hash;
            ++picture_;
        }
        ++state_.screens;
        std::vector<ws::PaneRowPart> parts = status_part(rows);
        if (to_window) { // the same rows, part and caret, as a drawing in the graphics window
            window_.draw(mail, rows, parts, rows_caret(caret));
        }
        if (!to_workshop) {
            return;
        }
        if (on_canvas()) {
            (void)mail.as_role(nve::kEditorOffice)
                .send_to_role(kWorkshopRole,
                              ws::rows_picture(canvas_, pictures_.next(canvas_, picture_), rows, parts,
                                               rows_caret(caret)));
            return;
        }
        (void)mail.as_role(nve::kEditorOffice)
            .send_to_role(kWorkshopRole, ws::v4::PaneContent{nve::kEditorPane, std::move(rows), epoch_, picture_,
                                                             std::move(parts)});
        (void)mail.as_role(nve::kEditorOffice).send_to_role(kWorkshopRole, caret);
    }

    /// IS THIS A PICTURE OF THE SCREEN THE PANE SHOWS, AND DOES NEOVIM'S SCREEN STILL READ AS IT?
    /// Drawn in this room under what the pane shows now, and Neovim drew nothing since.
    /// Whether the picture a hand aimed at, `picture` of `grant`, still shows what Neovim holds;
    /// `kInWindow` as the grant names the rows the window showing the area was last given.
    bool fresh(std::int64_t grant, std::int64_t picture) const {
        const bool canvas = on_canvas() && grant == canvas_.grant && pictures_.current(grant, picture);
        if (!granted_ || (grant != kInWindow && !canvas)) {
            return false;
        }
        std::vector<surface::SurfaceTextRow> rows;
        ws::v2::PaneCaret caret;
        caret.pane = nve::kEditorPane;
        caret.generation = epoch_;
        compose(rows, caret, rows_, columns_);
        return picture_hash(rows, caret) == picture_hash_;
    }

    // WL-NVIM-02 -- agents/workshop/neovim.md
    void compose(std::vector<surface::SurfaceTextRow>& rows, ws::v2::PaneCaret& caret, std::int64_t room_rows,
                 std::int64_t room_cols) const {
        if (room_rows <= 0 || room_cols <= 0) {
            return;
        }
        const std::int64_t chrome = chrome_rows();
        if (!notice_.empty()) {
            rows.push_back(surface::SurfaceTextRow{drawable(fit(notice_, room_cols)),
                                                   notice_bad_ ? surface::role::kAlert : surface::role::kMuted});
        } else if (chrome > 0) {
            rows.push_back(surface::SurfaceTextRow{drawable(fit(status_text(room_cols), room_cols)), surface::role::kAccent});
        }
        if (!running() || !host_->ready()) {
            return;
        }
        const std::int64_t text_cols = room_cols - kCaretCols > 0 ? room_cols - kCaretCols : 0;
        if (preview_.active && host_->grid().flushes() == preview_.flushes) {
            // THE FILE A SHOWING PUT IN FRONT, BEFORE NEOVIM HAS DRAWN IT: its first lines as they
            // were prepared, so no screen of the file it replaced stands under its status.
            for (std::size_t i = 0;
                 i < preview_.lines.size() && static_cast<std::int64_t>(i) + chrome < room_rows; ++i) {
                rows.push_back(surface::SurfaceTextRow{preview_line(preview_.lines[i], text_cols),
                                                       surface::role::kFill});
            }
            return;
        }
        const nv::Screen screen = nv::project(host_->grid(), host_->visual_kind());
        const std::int64_t shown_rows = std::min<std::int64_t>(static_cast<std::int64_t>(screen.rows.size()),
                                                               room_rows - chrome);
        if (screen.caret_row >= 0 && screen.caret_row < shown_rows && screen.caret_col <= text_cols) {
            caret.row = screen.caret_row + chrome;
            caret.column = screen.caret_col;
        }
        if (screen.sel_begin_row >= 0 && screen.sel_begin_row < shown_rows) {
            std::int64_t erow = screen.sel_end_row;
            std::int64_t ecol = std::min(screen.sel_end_col, text_cols);
            if (erow >= shown_rows) {
                erow = shown_rows;
                ecol = 0;
            }
            const std::int64_t bcol = std::min(screen.sel_begin_col, text_cols);
            if (erow > screen.sel_begin_row || ecol > bcol) {
                caret.sel_begin_row = screen.sel_begin_row + chrome;
                caret.sel_begin_col = bcol;
                caret.sel_end_row = erow + chrome;
                caret.sel_end_col = ecol;
            }
        }
        for (std::int64_t r = 0; r < shown_rows; ++r) {
            const nv::Screen::Row& row = screen.rows[static_cast<std::size_t>(r)];
            std::int64_t role = surface::role::kFill;
            switch (row.kind) {
            case nv::RowKind::Chrome: role = surface::role::kAccent; break;
            case nv::RowKind::Muted: role = surface::role::kMuted; break;
            case nv::RowKind::Alert: role = surface::role::kAlert; break;
            case nv::RowKind::Text: break;
            }
            // A ROW'S BLANKS GO, BUT NOT THE ONES THE CARET OR THE RANGE STANDS ON: Neovim's cursor on
            // an empty line, or after blanks it typed, is a cell of the row, drawn as every caret is.
            const std::int64_t at = r + chrome;
            std::int64_t keep = 0;
            if (caret.row == at) keep = std::max(keep, caret.column);
            if (caret.sel_end_row == at) keep = std::max(keep, caret.sel_end_col);
            if (caret.sel_begin_row == at && caret.sel_end_row != at) keep = std::max(keep, caret.sel_begin_col);
            std::string text = row.text;
            while (static_cast<std::int64_t>(text.size()) > keep && !text.empty() && text.back() == ' ') {
                text.pop_back();
            }
            rows.push_back(surface::SurfaceTextRow{drawable(fit(std::move(text), text_cols)), role});
        }
    }

    /// THE CURRENT SCREEN FOR A TRIAL ROOM (a same-path open).
    void compose_current(SourcePrepared& prepared, std::int64_t room_rows, std::int64_t room_cols) const {
        ws::v2::PaneCaret caret;
        compose(prepared.rows, caret, room_rows, room_cols);
        prepared.caret_row = caret.row;
        prepared.caret_col = caret.column;
        prepared.sel_begin_row = caret.sel_begin_row;
        prepared.sel_begin_col = caret.sel_begin_col;
        prepared.sel_end_row = caret.sel_end_row;
        prepared.sel_end_col = caret.sel_end_col;
    }

    /// A PREPARED FILE FOR A TRIAL ROOM, before Neovim draws it: the status row and the first lines.
    static void compose_preview(SourcePrepared& prepared, const std::string& path, const std::vector<std::string>& lines,
                                std::int64_t room_rows, std::int64_t room_cols) {
        if (room_rows <= 0 || room_cols <= 0) {
            return;
        }
        prepared.rows.push_back(surface::SurfaceTextRow{
            drawable(fit("saved NORMAL -- " + tail_of_path(path, room_cols - 16), room_cols)), surface::role::kAccent});
        const std::int64_t text_cols = room_cols - kCaretCols > 0 ? room_cols - kCaretCols : 0;
        for (std::size_t i = 0; i < lines.size() && static_cast<std::int64_t>(i) + kChromeRows < room_rows; ++i) {
            prepared.rows.push_back(surface::SurfaceTextRow{preview_line(lines[i], text_cols), surface::role::kFill});
        }
        prepared.caret_row = surface::kNoCaret;
    }

    // ---- The document's claim ----------------------------------------------------------------

    EditorDocument identity_now() const {
        EditorDocument d;
        d.path = doc_path();
        d.doc_epoch = epoch_;
        d.convention = convention_;
        d.content_revision = d.path.empty() ? 0 : doc_.tick;
        d.dirty = !d.path.empty() && doc_.modified;
        d.opened_by = static_cast<std::int64_t>(opened_by_);
        return d;
    }

    void claim_document(loom::Mail& mail) {
        const EditorDocument now = identity_now();
        if (claimed_ever_ && same_identity(now, claimed_)) {
            return;
        }
        if (mail.claim(now).accepted) {
            claimed_ = now;
            claimed_ever_ = true;
        }
    }

    // A prepared buffer nobody showed: in opening_switch.hpp.
    void discard_prepared(const Candidate& c);
    void drop_candidate();

    void mirror_state() {
        state_.running = running();
        state_.ready = running() && host_->ready();
        state_.version = host_ != nullptr && host_->version().api_level > 0 ? host_->version().text() : std::string();
        state_.profile = host_ != nullptr ? choice_.profile : std::string();
        state_.ui = running() ? ui_ : std::string();
        state_.listen = running() ? listen_ : std::string();
        state_.path = doc_path();
        state_.modified = !state_.path.empty() && doc_.modified;
        state_.mode = running() ? host_->mode() : std::string();
        state_.tick = doc_.tick;
        state_.doc_epoch = epoch_;
        state_.failure = failure_;
        state_.notice = notice_;
        state_.notice_bad = notice_bad_;
        state_.project_dir = project_dir_;
        state_.project_known = project_known_;
    }

    // ---- State -------------------------------------------------------------------------------

    zengine::ActivationCursor activation_;
    std::unique_ptr<nv::Host> host_;
    nv::LaunchChoice choice_;
    nv::Observed carried_;
    /// THE LAYOUT'S SETTINGS, handed again after a reload, so never reload state: the profile a
    /// start uses where the variable names none, and whether the status row shows.
    std::string profile_ = nv::LaunchChoice{}.profile;
    bool status_shown_ = true;
    std::string ui_;
    std::string listen_;
    std::string failure_;
    bool start_tried_ = false;
    mutable bool refused_reload_ = false;

    nv::DocFacts doc_;
    std::int64_t convention_ = 0;
    std::int64_t epoch_ = 0;
    std::uint64_t opened_by_ = 0;
    EditorDocument claimed_;
    bool claimed_ever_ = false;
    bool resay_ = false;

    Candidate candidate_;
    std::vector<Relay> relays_;
    Holding holding_;
    Warm warm_;
    Drag drag_;
    /// THE RIGHT BUTTON'S OWN RECORD: down, and where it went down, so its release is said once.
    Drag right_;
    double wheel_ = 0.0;
    bool adopted_ = false;
    std::optional<EditorTransfer> kept_;

    std::string clip_;
    std::map<std::uint64_t, std::uint32_t> pastes_;

    // ---- Transfers (WL-NVIM-10..12): this incarnation's conversations, none of them reload state.
    std::int64_t picture_ = 0;
    std::uint64_t picture_hash_ = 0;
    Grab grab_;
    Pickup pickup_;
    ws::pane_menu::Asked menu_;
    Take menu_take_ = Take::Selection;
    Snapshot menu_snap_;
    Drag menu_cell_;
    Dropped drop_;
    Locate locate_;
    /// THE ONE CHANGE NEOVIM HOLDS UNANSWERED, if any (WL-NVIM-13).
    std::optional<Held> held_;
    /// WHETHER `ctrl+r` IS DECLARED NOW (`carries_selection`), re-declared when Neovim's mode moves it.
    bool declared_selection_ = false;
    /// THE CHANGEDTICK OF THE BUFFER THE LAST OPEN SHOWED, so a dropped location's caret lands only
    /// on that buffer as it was shown.
    std::int64_t shown_tick_ = 0;

    std::string notice_;
    bool notice_bad_ = false;
    std::string project_dir_;
    bool project_known_ = false;
    bool root_asked_ = false;
    std::uint64_t root_pending_ = 0;
    std::uint64_t asked_ = 0;

    std::int64_t rows_ = 0;
    std::int64_t columns_ = 0;
    bool granted_ = false;

    /// THE CANVAS THIS PANE DRAWS ON: the room Workshop granted it there, its pictures numbered in
    /// that room by what Neovim's screen shows, and the left press whose motion is Neovim's drag.
    /// Not reload state.
    ws::PaneCanvasRoom canvas_;
    ws::CanvasPictures pictures_;
    std::int64_t pressed_ = 0;
    /// The grant a hand in the window names: no canvas grant is below one.
    static constexpr std::int64_t kInWindow = -1;
    bool on_canvas() const { return canvas_.grant > 0 && canvas_.width > 0 && canvas_.height > 0; }
    Preview preview_;

    /// THE GRAPHICS WINDOW THAT SHOWS THE OFFICE'S AREA, and the rows drawn there. Not reload state.
    office::EditorWindow window_{nve::kEditorOffice};
};

} // namespace

#include "editor/neovim-editor/opening_switch.hpp"
#include "editor/neovim-editor/transfers.hpp"

ZEN_EXPORT_WEAVE(NeovimEditorWeave)
