// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

// The Neovim-backed Editor: a loadable weave that holds the Editor's role with a Neovim it
// runs. It holds `zengine.editor`, offers the Editor's pane key and answers every door the
// standard Editor answers -- a managed opening's preparation and publication, the old door's
// relay, the orderly quit, the clipboard, the project root and both halves of an editor switch's
// handoff -- so Files, the Builder and Edit Code reach it by reaching the role (WL-NVIM-01).
// Workshop law: agents/workshop/neovim.md

// Neovim owns the buffers, their bytes, the modes, undo, registers and the screen. This weave
// owns the process (`neovim::Host`), the translation between the pane protocol and Neovim
// (`editor/neovim/projection.hpp`), and the Editor's facts as the role needs them: the identity it
// claims (path, changedtick, modified), the generation its rows carry, the transfer at a switch.

// Neovim is never waited on (WL-NVIM-05): keys and text go as notifications, a question is sent
// with Neovim's fast mode beside it (`Host::ask_later`), and what follows its answer runs in the
// delivery that hears it -- the next beat (the Timer's while this weave holds the role, the
// coordinator's tick while it warms) -- answering a bus ask then (`defer_answer`). A slow or stuck
// Neovim holds no handler: its question is outstanding at its bound, a start at `kStartWaitMs`.

// A change is this Editor's until Neovim answers it (WL-NVIM-13): a request Neovim holds is never
// withdrawn, so an unanswered change stays held, one at a time, said on the status row; its whole
// target travels with it to be checked when it runs, and its outcome is said once.

// Hiding the pane ends nothing; a switch away ends Neovim once the successor serves; `:qa` leaves
// the role held with no document; the orderly quit is refused while a buffer is unsaved; and a
// reload is refused while Neovim runs (`snapshot`), since the process is its incarnation's.

// The class is declared here with its lifecycle, its wait and pump, its keys, its quit, its
// clipboard and the pane it draws. opening_switch.hpp defines its opening and switching doors and
// transfers.hpp its pointer and transfers, both included after it; common.hpp holds what the
// three share: the bounds and names, the arithmetic and the records of its conversations.

#include "editor/neovim-editor/common.hpp"
#include "timer/source_wait.hpp"
#include "editor/neovim-editor/vocabulary.hpp"

#include "editor/neovim/host.hpp"
#include "editor/neovim/keys.hpp"
#include "editor/neovim/launch.hpp"
#include "editor/neovim/lua.hpp"
#include "editor/neovim/projection.hpp"

#include "editor/source-transfer/material.hpp"
#include "workshop/editor_handoff_vocabulary.hpp"
#include "workshop/open_seam_vocabulary.hpp"
#include "pane/rows.hpp"
#include "pane/carry.hpp"
#include "pane/causes.hpp"
#include "pane/document.hpp"
#include "pane/menu.hpp"
#include "pane/operation.hpp"
#include "workshop/pane_seam_vocabulary.hpp"
#include "workshop/project_seam_vocabulary.hpp"
#include "pane/text.hpp"
#include "pane/vocabulary.hpp"

#include "activation/activation.hpp"
#include "graphics/vocabulary.hpp"
#include "input/vocabulary.hpp"
#include "timer/vocabulary.hpp"

#include "pane/settings.hpp"
#include "pane/settings_taken.hpp"

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
using zengine::pane::PaneActionRequested;
using zengine::pane::PaneCatalogRequested;
using zengine::pane::PaneKey;
using zengine::pane::v2::PaneOffered;
using zengine::pane::PaneQuitAnswered;
using zengine::pane::PaneQuitRequested;
using zengine::pane::PaneTextInput;
using ws::ProjectRoot;
using ws::ProjectRootRequested;
using ws::SourceOpened;
using zengine::pane::pane_text::printable;
using zengine::pane::pane_text::fit;

using namespace zengine::neovim_editor::detail;

/// Its row's settings, asked of the layouts' owner when it arrives; no room is drawn before.
using Settings = zengine::pane::PaneSettingsFacet<nve::kEditorPane, nve::kEditorRole>;

using NeovimEditorBase = zengine::With<zengine::pane::PaneManualFacet<
    nve::kEditorPane, zengine::manual::pages::zengine_editor::kManual>, Settings>::Weave<
    class NeovimEditorWeave, nve::NeovimEditorState,
    loom::Accept<loom::Activated, PaneCatalogRequested, PaneKey, PaneTextInput, PaneActionRequested, PaneQuitRequested,
                 OpenSourceRequested, ws::v3::PrepareSourceRequested,
                 ManagedOpenProgress, ManagedOpenSettled,
                 SourceOpened, loom::AskEnded, ProjectRoot, graphics::ClipboardCopy,
                 graphics::ClipboardText, EditorHandoffJudgeRequested, EditorWarmRequested,
                 EditorPreparationTick, EditorHandoffRequested, EditorHandoffEnded,
                 EditorAdoptRequested, EditorLiveRequested, EditorRetireRequested,
                 nve::NeovimStartRequested, nve::NeovimStopRequested, nve::NeovimStatusRequested,
                 zengine::pane::PaneCanvasValueDrop, zengine::pane::PaneMenuAnswered, zengine::pane::PaneOperationAnswered,
                 zengine::pane::PaneCarryAnswered, graphics::AreaRoom, graphics::DrawnRefused,
                 graphics::ActionRequested, graphics::AreaButtonChanged, graphics::AreaPointerMoved,
                 graphics::AreaWheelTurned, graphics::AreaHoldEnded, loom::host::Waited>,
    loom::Emit<PaneOffered, zengine::pane::v3::PaneActions,
               zengine::pane::PaneEscapeUnspent, // declared so Escape reaches it, which it keeps
               PaneQuitAnswered, SourceOpened, ws::v3::SourcePrepared,
               OpenSourceRequested,
               ProjectRootRequested, graphics::ClipboardCopy, graphics::ClipboardTextRequested,
               EditorHandoffJudged, EditorWarmed, EditorHandoffOffered, EditorAdopted, EditorLive,
               EditorRetired, loom::Result, loom::Refused,
               zengine::pane::PaneMenuRequested, zengine::pane::PaneOperationRequested,
               zengine::pane::PaneValueCarryRequested,
               graphics::Drawn, graphics::RoomAsked, zengine::pane::v2::PaneSettingsDeclared>,
    loom::WeaveBase, loom::Claims<EditorDocument, loom::host::WaitSet>>;

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

    // ---- Being the role ----------------------------------------------------------------------

    void on(const loom::Activated& a, loom::Mail& mail) {
        if (!activation_.accept(mail, a)) {
            return;
        }
        if (adopted_) {
            notice((doc_.modified ? "switched editors -- UNSAVED edits stand in " : "switched editors -- editing ") +
                       (doc_path().empty() ? std::string("no source") : shown_path(doc_path())),
                   false);
        }
        announce(mail);
        ask_project_root(mail);
        // THE GRAPHICS WINDOW THAT SHOWS THE ROLE'S AREA, asked for its room once each life; a
        // room it said before this activation is taken now.
        window_.ask_room(mail);
        if (window_.in_window()) {
            size_to(window_.rows(), window_.columns(), mail);
        }
        resay_ = true;
    }

    /// What the host's one wait said about this role, with the time on its clock: Neovim is ready
    /// to be read, or the time the pane claimed came, so the pane pumps now on that time; or the
    /// wait holds the pane's claim, or no longer does (timer/source_wait.hpp).
    void on(const loom::host::Waited& w, loom::Mail& mail) {
        if (!activation_.activated()) {
            return;
        }
        if (wait_.hear(w, mail)) {
            pump(mail, wait_.now_ms());
        }
    }

    void on(const PaneCatalogRequested&, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole)) {
            return;
        }
        announce(mail);
    }

    /// THE SETTINGS THIS LAYOUT KEEPS FOR THE PANE, as the layouts' owner handed them and its
    /// facet took them whole. The profile is the next start's, never a running Neovim's; the status
    /// row moves Neovim's screen at once. Held as plain members, never reload state: a reloaded
    /// image asks for them again, and draws no room before it holds them.
    void on_settings(const zengine::pane::SettingsTaken& taken, loom::Mail& mail) {
        profile_ = taken.text(nve::kSettingProfile);
        status_shown_ = taken.flag(nve::kSettingStatus);
        say(mail);
    }

    /// THE SETTINGS THE PANE TAKES, each with its default, how a weaver finds it and what it does:
    /// the profile, pinned, one of the two words the variable's own default is among, and the
    /// status row.
    static std::vector<zengine::pane::v2::PaneSettingRow> setting_rows() {
        zengine::pane::v2::PaneSettingRow profile;
        profile.key = nve::kSettingProfile;
        profile.text = std::string(nv::LaunchChoice{}.profile);
        profile.choices = {"clean", "user"};
        profile.tags = {"editing"};
        profile.pinned = true;
        profile.summary = "Neovim profile name when ZENGINE_NEOVIM_PROFILE is unset";
        zengine::pane::v2::PaneSettingRow status;
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

    /// THE ROOM THE GRAPHICS WINDOW THAT SHOWS THE ROLE'S AREA GIVES, from that window's role:
    /// Neovim is sized to it, or started in it.
    void on(const graphics::AreaRoom& r, loom::Mail& mail) {
        if (!window_.room(r, mail) || !activation_.activated()) {
            return; // a room said before the activation is taken there
        }
        size_to(window_.rows(), window_.columns(), mail);
    }

    /// A drawing the window refused (`EditorWindow::refused`): a refused change is followed by the
    /// rows whole, a refused whole drawing by nothing until they change.
    void on(const graphics::DrawnRefused& refused, loom::Mail& mail) {
        (void)window_.refused(refused, mail);
    }

    /// THE POINTER IN THE WINDOW, given by the window that shows the area as its role and read
    /// onto the rows drawn there: Neovim's own mouse (`pointer`), its press, moves and release under
    /// the window's number for the press, a hold the window ended a loss.
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
        if (const std::optional<zengine::pane::RowCell> at = window_.part(asked, mail)) {
            press_at(at->row, at->column, false, mail);
            release_drag();
            flush(mail);
        }
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
        } else if (asked.id == nve::kActionCtrlSpace) {
            send_input(mail, "<C-Space>");
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
            const std::uint64_t gesture = mail.correlation();
            snapshot_later(mail, [this, gesture](Snapshot snap, loom::Mail& later) {
                if (!snap.ok) {
                    notice("nothing was carried -- " + snap.refusal, true);
                } else {
                    carry_snapshot(snap, false, gesture, later);
                }
                resay_ = true;
            });
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

    void on_window(const std::optional<zengine::pane::WindowPointer>& p, loom::Mail& mail);
    void pointer(std::int64_t phase, std::int64_t button, const zengine::pane::RowCell& at, bool aimed,
                 std::int64_t gesture, double dy, loom::Mail& mail);
    void press_at(std::int64_t row, std::int64_t column, bool aimed, loom::Mail& mail);
    void dragged(std::int64_t row, std::int64_t column, loom::Mail& mail);
    void right_press(std::int64_t row, std::int64_t column, bool aimed, loom::Mail& mail);
    void right_release(std::int64_t row, std::int64_t column, loom::Mail& mail);
    void wheel(double dy, loom::Mail& mail);

    // ---- Transfers, in transfers.hpp: the selection out, material in, a location opened ---------

    void on(const zengine::pane::PaneCanvasValueDrop& drop, loom::Mail& mail);
    void on(const zengine::pane::PaneMenuAnswered& answer, loom::Mail& mail);
    void on(const zengine::pane::PaneOperationAnswered& answer, loom::Mail& mail);
    void on(const zengine::pane::PaneCarryAnswered& answer, loom::Mail& mail);

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
        if (candidate_.live || preparing_) {
            answer(false, "Neovim is still opening " + (candidate_.live ? candidate_.path : std::string("a file")) +
                              " -- quit again once it has settled");
            return;
        }
        if (!running()) {
            answer(true, std::string());
            return;
        }
        // ASKED OF NEOVIM AND ANSWERED WHEN IT ANSWERS: Workshop waits for every pane's word.
        const auto due = std::make_shared<loom::DeferredAnswer>(mail.defer_answer());
        ask(nv::lua::kUnsaved, nv::rpc::params(), kAskMs, mail,
            [this, due](const Answer& a, loom::Mail& later) { answer_quit(*due, a, later); });
    }

    /// THE QUIT'S ANSWER, from what Neovim said it holds.
    void answer_quit(const loom::DeferredAnswer& due, const Answer& said, loom::Mail& mail) {
        const auto answer = [&mail, &due](bool permitted, std::string why) {
            (void)loom::answer_deferred(due, mail, PaneQuitAnswered{nve::kEditorPane, permitted, std::move(why)});
        };
        if (!said.result.has_value()) {
            answer(false, "Neovim could not be asked whether it holds unsaved work (" + said.why +
                              ") -- Workshop stays open");
            return;
        }
        const std::optional<mp::Value>& unsaved = said.result;
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

    void on(const graphics::ClipboardCopy& said, loom::Mail&) { clip_ = said.text; }

    /// THE SKIN'S ANSWER TO A PASTE NEOVIM ASKED FOR, handed to the request Neovim is waiting on.
    void on(const graphics::ClipboardText& a, loom::Mail& mail) {
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
    void on(const loom::AskEnded& ended, loom::Mail& mail);
    void on(const ManagedOpenProgress&, loom::Mail&);
    void on(const ws::v3::PrepareSourceRequested& asked, loom::Mail& mail);
    void prepare(std::int64_t op, const std::string& asked_path, std::int64_t rows,
                 std::int64_t columns, loom::Mail& mail, Prepared then);
    void prepare_in(const Candidate& next, loom::Mail& mail, Prepared then);
    void offer_prepared(Candidate next, EditorDocument offered, loom::Mail& mail, Prepared then);
    void on(const ManagedOpenSettled& said, loom::Mail& mail);
    bool on_claim_published(const EditorDocument& published);
    void settle_show(const Candidate& shown, const Answer& a);

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
        settle_wait(mail);
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
    std::string listening_words(const std::string& opened) const;
    void stop_neovim(const loom::DeferredAnswer& due, loom::Mail& mail);
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

    // What a switch is judged from, asked of Neovim and answered when it answers: in
    // opening_switch.hpp.
    void until_unblocked(std::int64_t until, loom::Mail& mail,
                         std::function<void(bool, loom::Mail&)> then);
    void export_later(int ms, loom::Mail& mail, Exported_then then);
    static std::optional<Exported> exported_of(const mp::Value& v);
    void judge(loom::Mail& mail, Judged then);
    Judgement judged_from(Judgement j, const Exported& e) const;
    void adopt_placed(std::int64_t op, EditorTransfer t, bool dos, std::shared_ptr<loom::DeferredAnswer> due,
                      loom::Mail& mail);

    // A change Neovim holds until it answers (WL-NVIM-13): in transfers.hpp.
    void change(const char* chunk, mp::Value args, std::string what, const std::string& waiting, Settle settle);
    Heard heard_of(const nv::rpc::Response& r) const;
    bool settle_held();

    // ---- Asking Neovim, never waiting (WL-NVIM-05) ------------------------------------------

    std::string not_running_words() const {
        return "Neovim is not running" + (failure_.empty() ? std::string() : ": " + failure_);
    }

    /// WHAT AN ANSWER SAYS, from the Host's outcome: the result, or why there is none.
    static Answer answer_of(nv::Host::Outcome o) {
        if (o.asked == nv::Host::Asked::Outstanding) {
            return Answer{std::nullopt, o.why, true, 0};
        }
        if (!o.got.has_value()) {
            return Answer{std::nullopt, "Neovim did not answer", false, 0};
        }
        if (!o.got->error.is_nil()) {
            return Answer{std::nullopt, nv::rpc::error_text(o.got->error), false, o.flushes};
        }
        return Answer{std::move(o.got->result), std::string(), false, o.flushes};
    }

    /// ONE QUESTION TO THE MODULE, never waited for: `then` runs exactly once -- in this delivery
    /// when it cannot be asked, else in the pump that hears what became of it, with that
    /// delivery's Mail. A Neovim holding `kMaxAsks` questions is asked no more until it answers.
    void ask(const char* chunk, mp::Value args, int ms, loom::Mail& mail, Then then) {
        ask_rpc("nvim_exec_lua", nv::rpc::params(mp::Value::str(chunk), std::move(args)), ms, mail,
                std::move(then));
    }

    /// ...and one asked where there is no Mail to answer with (a publication hook): `then` runs in
    /// a later pump either way.
    void ask_quietly(const char* chunk, mp::Value args, int ms, Then then) {
        const mp::Value params = nv::rpc::params(mp::Value::str(chunk), std::move(args));
        std::string why;
        const std::shared_ptr<std::vector<Arrival>> inbox = inbox_;
        ++asking_;
        if (!running()) {
            inbox->push_back(Arrival{Answer{std::nullopt, not_running_words(), false, 0}, std::move(then)});
            return;
        }
        if (!host_->ask_later("nvim_exec_lua", params, ms,
                              [inbox, then](nv::Host::Outcome o) {
                                  inbox->push_back(Arrival{answer_of(std::move(o)), then});
                              },
                              why)) {
            inbox->push_back(Arrival{Answer{std::nullopt, why, false, 0}, std::move(then)});
        }
    }

    void ask_rpc(const char* method, const mp::Value& params, int ms, loom::Mail& mail, Then then) {
        if (!running()) {
            then(Answer{std::nullopt, not_running_words(), false, 0}, mail);
            return;
        }
        if (asking_ >= kMaxAsks) {
            then(Answer{std::nullopt,
                        "Neovim has " + std::to_string(kMaxAsks) + " questions unanswered already",
                        false, 0},
                 mail);
            return;
        }
        std::string why;
        const std::shared_ptr<std::vector<Arrival>> inbox = inbox_;
        if (!host_->ask_later(method, params, ms,
                              [inbox, then](nv::Host::Outcome o) {
                                  inbox->push_back(Arrival{answer_of(std::move(o)), then});
                              },
                              why)) {
            then(Answer{std::nullopt, why, false, 0}, mail);
            return;
        }
        ++asking_;
    }

    /// EVERY ANSWER HEARD SINCE, its continuation run in the order heard, once.
    void hear(loom::Mail& mail) {
        if (hearing_) {
            return; // a continuation that pumps hears nothing new here; the loop below does
        }
        hearing_ = true;
        struct Done {
            bool* flag;
            ~Done() { *flag = false; }
        } done{&hearing_};
        while (!inbox_->empty()) {
            std::vector<Arrival> now = std::move(*inbox_);
            inbox_->clear();
            for (Arrival& a : now) {
                if (asking_ > 0) {
                    --asking_;
                }
                a.then(a.answer, mail);
            }
        }
    }

    /// A NEOVIM, RUNNING AND READY, for an answer that needs one: `then` runs once, now when one
    /// is ready or cannot start, else in the pump where it becomes ready or fails, or at most
    /// `kStartWaitMs` after the first wake that follows the first waiter.
    void when_ready(loom::Mail& mail, Ready then) {
        if (!running()) {
            then(false, failure_.empty() ? not_running_words() : failure_, mail);
            return;
        }
        if (host_->ready()) {
            then(true, std::string(), mail);
            return;
        }
        if (readies_.empty()) {
            ready_since_.reset();
        }
        readies_.push_back(std::move(then));
    }

    void ensure_running(loom::Mail& mail, Ready then) {
        if (!running() && !start_embedded(ui_rows(rows_ > 0 ? rows_ : kDefaultRows, chrome_rows()),
                                          ui_cols(columns_ > 0 ? columns_ : kDefaultCols))) {
            then(false, failure_, mail);
            return;
        }
        when_ready(mail, std::move(then));
    }

    void settle_readies(loom::Mail& mail, const std::optional<std::int64_t>& handed) {
        if (readies_.empty()) {
            return;
        }
        bool ok = false;
        std::string why;
        if (running() && host_->ready()) {
            ok = true;
        } else if (!running()) {
            why = failure_.empty() ? not_running_words() : failure_;
        } else if (!ready_since_) {
            ready_since_ = handed; // the wait has not woken the pane since the first waiter came
            return;
        } else if (!handed || *handed - *ready_since_ < kStartWaitMs) {
            return;
        } else {
            why = "Neovim did not become ready within " + std::to_string(kStartWaitMs / 1000) + " seconds";
        }
        std::vector<Ready> all = std::move(readies_);
        readies_.clear();
        for (Ready& r : all) {
            r(ok, why, mail);
        }
    }

    // ---- Transfer helpers, in transfers.hpp -----------------------------------------------------

    bool on_highlight(std::int64_t row, std::int64_t col) const;
    std::string grid_row(std::int64_t row) const;
    void snapshot_later(loom::Mail& mail, std::function<void(Snapshot, loom::Mail&)> then);
    static Snapshot snapshot_of(const Answer& a);
    std::string project_root() const;
    void begin_pickup(const st::Pair& pair, bool drag, std::uint64_t gesture, std::string label, std::string what,
                      loom::Mail& mail);
    void carry_snapshot(const Snapshot& snap, bool drag, std::uint64_t gesture, loom::Mail& mail);
    void acquire_location(bool drag, std::uint64_t gesture, loom::Mail& mail);
    void carry_location(bool drag, std::uint64_t gesture, const Answer& a, loom::Mail& mail);
    void receive_into(st::Material m, Aim aim, const zengine::pane::RowCell& drop, const mp::Value& lang,
                      std::uint64_t gesture, loom::Mail& mail);
    void receive(const loom::Bytes& data, const zengine::pane::RowCell& drop, bool aimed,
                 loom::Mail& mail);
    void insert_lines(const std::string& text, const Aim& aim, Insertion said, bool whole = false);
    void settle_insertion(const Insertion& said, const Heard& h);
    void insert_command(const st::Material& m, const Aim& aim);
    void insert_cpp(const st::Material& m, const Aim& aim, loom::Mail& mail);
    void insert_cpp_from(const st::Material& m, const Aim& aim, const Answer& lang);
    void open_location(const st::Material& m, loom::Mail& mail);
    void settle_location(const SourceOpened& said);

    // ---- The wait, the pump and what it observed --------------------------------------------

    /// MOVE NEOVIM'S BYTES, APPLY WHAT IT SAID, run what follows; on `handed`, the wake's time.
    void pump(loom::Mail& mail, const std::optional<std::int64_t>& handed = std::nullopt) {
        if (host_ != nullptr) {
            nv::Observed seen = std::move(carried_);
            carried_ = nv::Observed{};
            if (host_->phase() != nv::Host::Phase::Idle) {
                seen.merge(host_->pump(handed));
            }
            absorb(mail, std::move(seen));
        }
        settle_readies(mail, handed);
        hear(mail);
    }

    /// What Neovim's pipes wait on claimed as this role after every delivery, with the time to look
    /// by (`look_again`): its soonest bound, `Host::due_ms` or `ready_by`, said when it comes.
    void settle_wait(loom::Mail& mail) {
        bool now = false;
        const loom::host::WaitSet set = host_ != nullptr ? host_->wait_set(&now) : loom::host::WaitSet{};
        std::optional<std::int64_t> due = host_ != nullptr ? host_->due_ms() : std::nullopt;
        if (!readies_.empty()) {
            due = due && *due < ready_by(ready_since_) ? *due : ready_by(ready_since_);
        }
        const LookAgain look = look_again(due, wait_.now_ms(), now,
                                          host_ != nullptr && host_->on_time(), wait_.held());
        wait_.settle(mail, nve::kEditorRole, set, look.on_time, look.every_ms);
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
        if (seen.doc_changed && !preview_.awaiting) {
            doc_ = host_->doc(); // while a showing is unanswered, the facts are the candidate's
        }
        (void)settle_held(); // a change Neovim held, answered since -- or ended with it (WL-NVIM-13)
        for (const nv::ClipboardCopy& copy : seen.copies) {
            clip_ = join_lines(copy.lines, copy.regtype == "V" || copy.regtype == "line");
            (void)mail.publish(graphics::ClipboardCopy{clip_});
        }
        for (const std::uint32_t id : seen.paste_requests) {
            const std::uint64_t correlation = ++asked_;
            const loom::Ticket asked = mail.as_role(nve::kEditorRole)
                                           .ask_role(window_.clipboard_role(),
                                                     graphics::ClipboardTextRequested{},
                                                     correlation);
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
        (void)mail.as_role(nve::kEditorRole)
            .send_to_role(kWorkshopRole, PaneOffered{nve::kEditorPane, nve::kPaneName, nve::kPaneSummary, 20, 84});
        declare(mail);
        (void)mail.as_role(nve::kEditorRole)
            .send_to_role(zengine::pane::kLayoutsRole,
                          zengine::pane::v2::PaneSettingsDeclared{nve::kEditorPane,
                                                                  setting_rows()});
    }

    /// Two rows naming Workshop's retired document rows as what they stand in for, so an older
    /// host that still declares them lets these keep their chords: save writes the buffer, and
    /// open is Neovim's own jump back. Every other key reaches Neovim, except `ctrl+r` while a
    /// Visual or Select selection stands, where it carries the selection (WL-NVIM-10); this
    /// file's location has no default key, every plain ctrl+letter being Neovim's or the
    /// desktop's (WL-NVIM-12).
    void declare(loom::Mail& mail) {
        zengine::pane::v3::PaneActions actions;
        actions.pane = nve::kEditorPane;
        actions.rows.push_back(zengine::pane::v3::PaneActionRow{nve::kActionWrite, "write (:w)", zengine::input::scan::kS,
                                                     zengine::input::mod::kCtrl, zengine::pane::kOwnableDocumentSave});
        actions.rows.push_back(zengine::pane::v3::PaneActionRow{nve::kActionJumpOlder, "jump back (<C-o>)",
                                                     zengine::input::scan::kO, zengine::input::mod::kCtrl,
                                                     zengine::pane::kOwnableDocumentOpen});
        // CTRL+SPACE IS NEOVIM'S while it holds the keys (completion, `<C-@>`'s insert), so the row
        // stands in for the palette's key there; the palette is a Pane Manager row away.
        actions.rows.push_back(zengine::pane::v3::PaneActionRow{nve::kActionCtrlSpace, "<C-Space> to Neovim",
                                                     zengine::input::scan::kSpace,
                                                     zengine::input::mod::kCtrl, nve::kPaletteKey});
        // The two carries' rows say what their press may send: the carry, once Workshop allows it.
        const std::vector<zengine::pane::ActionCause> carried{
            zengine::pane::cause_of<zengine::pane::PaneValueCarryRequested>()};
        actions.rows.push_back(zengine::pane::v3::PaneActionRow{nve::kActionLocation, "carry this file's location",
                                                     zengine::input::scan::kUnknown, zengine::input::mod::kNone,
                                                     "", carried});
        declared_selection_ = carries_selection();
        if (declared_selection_) {
            actions.rows.push_back(zengine::pane::v3::PaneActionRow{nve::kActionExtract, "carry the selection",
                                                         zengine::input::scan::kR, zengine::input::mod::kCtrl, "",
                                                         carried});
        }
        (void)mail.as_role(nve::kEditorRole).send_to_role(kWorkshopRole, actions);
    }

    /// DOES `ctrl+r` CARRY THE SELECTION IN NEOVIM'S MODE NOW? Only while Visual or Select holds one.
    bool carries_selection() const { return running() && host_->ready() && visual_mode(host_->mode()); }

    void ask_project_root(loom::Mail& mail) {
        root_asked_ = true;
        root_pending_ = ++asked_;
        (void)mail.as_role(nve::kEditorRole).ask_role(ws::kProjectRole, ProjectRootRequested{}, root_pending_);
    }

    void notice(std::string text, bool bad) {
        notice_ = std::move(text);
        notice_bad_ = bad;
    }

    /// `notice`, as the role's shared steps say one (`editor/role_seam.hpp`).
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
        if (held_.has_value() && held_->said) {
            head += held_->what + " waits for Neovim -- "; // held until Neovim answers (WL-NVIM-13)
        }
        return head + (path.empty() ? std::string("no file") : tail_of_path(path, columns - static_cast<std::int64_t>(head.size())));
    }

    /// THE ONE PART THIS PANE NAMES, `status`: the row it draws above Neovim's screen, saying the
    /// status or a notice in its place. The screen beneath is Neovim's own, and named nothing.
    std::vector<zengine::pane::RowPart> status_part(const std::vector<zengine::pane::TextRow>& rows) const {
        if (chrome_rows() <= 0 || rows.empty()) {
            return {};
        }
        return {zengine::pane::RowPart{"status", 0, 0, columns_}};
    }

    /// THE PANE, DRAWN in the window that shows its area: the status row (or a standing notice in
    /// its place), then Neovim's screen, cropped to the room, with the caret and the one range
    /// standing in its rows. Their fingerprint is kept, so a press or a drop can tell the screen it
    /// was aimed at from one that moved since (WL-NVIM-11).
    void say(loom::Mail& mail) {
        if (!window_.in_window() || !activation_.activated() ||
            !this->template facet<Settings>().held()) {
            return;
        }
        // NEOVIM'S SCREEN FITS UNDER THE ROWS DRAWN ABOVE IT NOW: a notice standing or going, or a
        // layout's status row, moves it, and a size Neovim already has asks nothing.
        if (running() && rows_ > 0) {
            (void)host_->resize(screen_rows(), ui_cols(columns_));
        }
        std::vector<zengine::pane::TextRow> rows;
        zengine::pane::RowsCaret caret;
        compose(rows, caret, rows_, columns_);
        drawn_hash_ = picture_hash(rows, caret, epoch_);
        ++state_.screens;
        window_.draw(mail, rows, status_part(rows), caret, drawn_number());
    }

    /// THE NUMBER THE ROWS LAST DRAWN ARE SAID WITH (`graphics::Sketch::number`): their
    /// fingerprint, which a window says with each press that lands on them.
    std::int64_t drawn_number() const { return static_cast<std::int64_t>(drawn_hash_ >> 1); }

    /// DOES NEOVIM'S SCREEN STILL READ AS THE ROWS A HAND AIMED AT? `aimed` when the hand was on the
    /// rows last drawn in the window, and Neovim drew nothing since.
    bool fresh(bool aimed) const {
        if (!aimed || !window_.in_window()) {
            return false;
        }
        std::vector<zengine::pane::TextRow> rows;
        zengine::pane::RowsCaret caret;
        compose(rows, caret, rows_, columns_);
        return picture_hash(rows, caret, epoch_) == drawn_hash_;
    }

    // WL-NVIM-02 -- agents/workshop/neovim.md
    void compose(std::vector<zengine::pane::TextRow>& rows, zengine::pane::RowsCaret& caret, std::int64_t room_rows,
                 std::int64_t room_cols) const {
        if (room_rows <= 0 || room_cols <= 0) {
            return;
        }
        const std::int64_t chrome = chrome_rows();
        if (!notice_.empty()) {
            rows.push_back(zengine::pane::TextRow{printable(fit(notice_, room_cols)),
                                                   notice_bad_ ? graphics::colors::alert : graphics::colors::quiet});
        } else if (chrome > 0) {
            rows.push_back(zengine::pane::TextRow{printable(fit(status_text(room_cols), room_cols)),
                                                   graphics::colors::accent});
        }
        if (!running() || !host_->ready()) {
            return;
        }
        const std::int64_t text_cols = room_cols - kCaretCols > 0 ? room_cols - kCaretCols : 0;
        if (preview_.active && (preview_.awaiting || host_->grid().flushes() == preview_.flushes)) {
            // THE FILE A SHOWING PUT IN FRONT, BEFORE NEOVIM HAS DRAWN IT: its first lines as they
            // were prepared, so no screen of the file it replaced stands under its status.
            for (std::size_t i = 0;
                 i < preview_.lines.size() && static_cast<std::int64_t>(i) + chrome < room_rows; ++i) {
                rows.push_back(zengine::pane::TextRow{preview_line(preview_.lines[i], text_cols),
                                                       graphics::colors::text});
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
            graphics::Color ink = graphics::colors::text;
            switch (row.kind) {
            case nv::RowKind::Chrome: ink = graphics::colors::accent; break;
            case nv::RowKind::Muted: ink = graphics::colors::quiet; break;
            case nv::RowKind::Alert: ink = graphics::colors::alert; break;
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
            rows.push_back(zengine::pane::TextRow{printable(fit(std::move(text), text_cols)), ink});
        }
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
    /// THE ANSWERS NEOVIM GAVE, waiting for the pump to run what follows each. Shared with the
    /// Host's callbacks, which a Neovim ending may run as this weave goes.
    std::shared_ptr<std::vector<Arrival>> inbox_ = std::make_shared<std::vector<Arrival>>();
    std::size_t asking_ = 0;
    bool hearing_ = false;
    zengine::timer::SourceWait wait_; ///< what the host's wait last said, and the time it handed
    /// What waits for a starting Neovim, and since when on the host's wait's clock.
    std::vector<Ready> readies_;
    std::optional<std::int64_t> ready_since_;
    /// An opening is being prepared in Neovim now, before it is offered.
    bool preparing_ = false;
    /// What waits for Neovim's answer to a showing: a dropped location's cursor.
    std::function<void()> after_show_;
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
    /// The fingerprint of the rows last drawn in the window (`say`).
    std::uint64_t drawn_hash_ = 0;
    Grab grab_;
    Pickup pickup_;
    zengine::pane::pane_menu::Asked menu_;
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

    /// The presses on the highlight counted, so a selection Neovim answers for an earlier press arms
    /// nothing.
    std::uint64_t grabs_ = 0;
    /// The left press whose motion is Neovim's drag, by the window's number for it. Not reload
    /// state.
    std::int64_t pressed_ = 0;
    Preview preview_;

    /// THE GRAPHICS WINDOW THAT SHOWS THE ROLE'S AREA, and the rows drawn there. Not reload state.
    role::EditorWindow window_{nve::kEditorRole, nve::kEditorPane};
};

} // namespace

#include "editor/neovim-editor/opening_switch.hpp"
#include "editor/neovim-editor/transfers.hpp"

ZEN_EXPORT_WEAVE(NeovimEditorWeave)
