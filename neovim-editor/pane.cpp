// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Neovim-backed Editor -- a loadable weave that holds the Editor's office with a Neovim it runs.
//
// IT IS THE EDITOR, IMPLEMENTED BY NEOVIM, AND NOTHING ELSE KNOWS. It holds `zengine.editor`, offers
// the Editor's pane key and answers every door the standard Editor answers: a managed opening's
// preparation and publication (WL-OPEN), the old door's relay, the orderly quit, the clipboard, the
// project root, and both halves of an editor switch's handoff (WL-SWITCH). A load plan authors it as
// a choice for the office; Files, the Builder and Edit Code reach it because they reach the office.
//
// ---- WHAT NEOVIM OWNS, AND WHAT THIS WEAVE OWNS ------------------------------------------------
//
// Neovim owns the buffers, their bytes, the modes, the undo history, the registers and the screen.
// This weave owns the process (`neovim::Host`: spawned, pumped, asked within bounds, ended), the
// translation of the pane protocol into Neovim input and of Neovim's screen into the protocol's
// rows, caret and one range (`neovim/projection.hpp`), and the Editor's facts as the office's
// conversations need them: the document's identity it CLAIMS (path, changedtick, modified), the
// generation its rows carry, and the transfer it authors or adopts at a switch.
//
// ---- HOW IT WAITS ------------------------------------------------------------------------------
//
// THE FLOW IS NEVER WAITED ON. Keys and text go to Neovim as notifications and return nothing; the
// screen comes back as redraw events and is said to the pane on the next beat. The beat is the
// Timer's (every 10 ms, the Timer's own floor) while this weave holds the office, and the switch
// coordinator's relayed tick while it is a sealed candidate warming.
//
// A QUESTION IS ASKED WITHIN A BOUND, in the one delivery that needs its answer: what a switch away
// would lose and the exact document at the boundary, the adoption and where it put the caret, an
// open's preparation and its showing, whether a quit may proceed. Each is `Host::call_now`, which
// asks Neovim's fast mode beside the question -- so a Neovim waiting at a prompt or in an unfinished
// command is said to be waiting, in words, instead of being waited on.
//
// STARTING IS NEVER ASKED WITHIN A HANDLER'S BOUND EXCEPT WHERE AN ANSWER NEEDS IT: a warm-up answers
// later (the coordinator holds the switch pending and cancellable), and the pane shows `starting`
// until Neovim answers; an open or a baseline start that finds no Neovim starts one and waits for it
// at most `kStartWaitMs`.
//
// ---- WHAT ENDS NEOVIM ----------------------------------------------------------------------------
//
// Hiding the pane ends nothing. A switch away ends it when the successor has proved it serves and
// this weave is unloaded. `:qa` inside Neovim ends it, and the office stays held with no document,
// said so. Workshop's orderly quit asks first and is refused while a buffer holds unsaved changes.
// A RELOAD OF THIS IMAGE IS REFUSED WHILE NEOVIM RUNS (`snapshot`): the process belongs to the
// incarnation that started it, and a reload would end it with whatever it holds.

#include "neovim-editor/vocabulary.hpp"

#include "neovim/document.hpp"
#include "neovim/host.hpp"
#include "neovim/keys.hpp"
#include "neovim/launch.hpp"
#include "neovim/lua.hpp"
#include "neovim/projection.hpp"

#include "workshop/editor_handoff_vocabulary.hpp"
#include "workshop/editor_switch_vocabulary.hpp"
#include "workshop/open_seam_vocabulary.hpp"
#include "workshop/pane_seam_vocabulary.hpp"
#include "workshop/pane_text.hpp"
#include "workshop/pane_vocabulary.hpp"
#include "workshop/persist.hpp"

#include "activation/activation.hpp"
#include "input/vocabulary.hpp"
#include "surface/vocabulary.hpp"
#include "timer/vocabulary.hpp"

#include <zen/kernel/export.hpp>
#include <zen/weave.hpp>
#include <zen/weave/standard_shapes.hpp>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#if defined(_WIN32)
#include <process.h>
#else
#include <unistd.h>
#endif

namespace {

namespace nv = zengine::neovim;
namespace mp = zengine::neovim::msgpack;
namespace nve = zengine::neovim_editor;
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
using ws::PaneDragged;
using ws::PaneKey;
using ws::PaneOffered;
using ws::PanePressed;
using ws::PaneQuitAnswered;
using ws::PaneQuitRequested;
using ws::PaneRoom;
using ws::PaneTextInput;
using ws::PaneWheel;
using ws::PrepareSourceRequested;
using ws::ProjectRoot;
using ws::ProjectRootRequested;
using ws::SourceOpened;
using ws::SourcePrepared;
using zengine::workshop::pane_text::drawable;
using zengine::workshop::pane_text::fit;

/// Workshop's office, as a stranger names it.
constexpr const char* kWorkshopRole = "zengine.workshop";

/// THE CARET'S COLUMN, reserved as the standard Editor reserves it: Neovim is given one column less
/// than the room, so an Insert caret after the last cell still has a cell to be drawn in.
constexpr std::int64_t kCaretCols = 1;

/// THE ROW ABOVE NEOVIM'S SCREEN: what the Editor says about itself -- the mode, whether the buffer
/// is saved, the file -- or a standing notice in its place.
constexpr std::int64_t kChromeRows = 1;

/// The smallest screen a Neovim is asked for, whatever the room: Neovim's own floor is one row and
/// one column, and a screen smaller than this says nothing a maker can use. A smaller room shows
/// the top-left of it.
constexpr std::int64_t kMinUiRows = 2;
constexpr std::int64_t kMinUiCols = 12;

/// The screen a Neovim starts with before any room was granted.
constexpr std::int64_t kDefaultRows = 24;
constexpr std::int64_t kDefaultCols = 80;

/// THE BEAT this weave asks of the Timer while it holds the office.
constexpr const char* kBeatId = "zengine.neovim-editor.beat";
constexpr std::int64_t kBeatMs = 10;

/// HOW LONG A QUESTION MAY TAKE: long enough for any Neovim that is not waiting, short enough that a
/// Neovim that is stuck costs a maker a moment rather than a Workshop.
constexpr int kAskMs = 2000;
/// ...and an adoption, which carries a whole document (up to the transfer's bound) both ways.
constexpr int kAdoptMs = 10000;
/// ...and a start an answer waits for (an open with no Neovim running, a baseline start).
constexpr int kStartWaitMs = 10000;

/// How many opens may be relayed through the old door at once (the standard Editor's bound).
constexpr std::size_t kMaxRelays = 4;

/// A PATH AS THE STANDARD EDITOR SPELLS ONE: absolute, lexically normal, forward slashes -- so the
/// office's two implementations name one file one way and a same-path open is recognised.
std::string spelled(const std::string& name) {
    return ws::persist::resolved_against(std::string(), name);
}

/// A PATH THAT KEEPS ITS END (the standard Editor's rule).
std::string tail_of_path(const std::string& path, std::int64_t width) {
    if (width <= 0) {
        return std::string();
    }
    const std::size_t room = static_cast<std::size_t>(width);
    if (path.size() <= room) {
        return path;
    }
    if (room <= 4) {
        return path.substr(path.size() - room);
    }
    return "..." + path.substr(path.size() - (room - 3));
}

/// NEOVIM'S MODE, as the status row says it.
std::string mode_word(const std::string& mode) {
    if (mode.empty()) {
        return "NORMAL";
    }
    switch (mode[0]) {
    case 'n': return mode.size() > 1 && mode[1] == 'o' ? "PENDING" : "NORMAL";
    case 'i': return "INSERT";
    case 'R': return "REPLACE";
    case 'v': return "VISUAL";
    case 'V': return "V-LINE";
    case '\x16': return "V-BLOCK";
    case 's': case 'S': case '\x13': return "SELECT";
    case 'c': return "COMMAND";
    case 'r': return "PROMPT";
    case 't': return "TERMINAL";
    default: return "NORMAL";
    }
}

/// WHAT NEOVIM COPIED, AS CLIPBOARD TEXT. A linewise copy arrives with its final empty line already
/// in `lines` (measured: `"+yy` of `copy me` is `{"copy me", ""}`, `V`), so its newline is that
/// empty line; one is added only for a linewise copy that lacks it.
std::string join_lines(const std::vector<std::string>& lines, bool linewise) {
    std::string out;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        if (i > 0) {
            out += '\n';
        }
        out += lines[i];
    }
    if (linewise && (lines.empty() || !lines.back().empty())) {
        out += '\n';
    }
    return out;
}

/// CLIPBOARD TEXT AS A PASTE NEOVIM ASKED FOR: its lines and their register type. Text that ends in
/// a newline is a whole-line paste (`V`, the lines without that final newline), as Neovim reads its
/// own linewise registers; anything else is charwise (`v`).
struct PasteLines {
    std::vector<std::string> lines;
    const char* regtype = "v";
};

PasteLines paste_lines(const std::string& text) {
    PasteLines out;
    std::size_t start = 0;
    for (;;) {
        const std::size_t at = text.find('\n', start);
        if (at == std::string::npos) {
            out.lines.push_back(text.substr(start));
            break;
        }
        std::string line = text.substr(start, at - start);
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        out.lines.push_back(std::move(line));
        start = at + 1;
    }
    if (out.lines.size() > 1 && out.lines.back().empty()) {
        out.lines.pop_back();
        out.regtype = "V";
    }
    return out;
}

/// ONE LINE OF A FILE AS THE PANE CAN DRAW IT, before Neovim draws it: tabs to the next eighth
/// column, and every byte the pane cannot draw as `?`.
std::string preview_line(const std::string& line, std::int64_t columns) {
    std::string out;
    for (const char c : line) {
        if (c == '\t') {
            do {
                out += ' ';
            } while (out.size() % 8 != 0);
        } else {
            out += c;
        }
        if (static_cast<std::int64_t>(out.size()) >= columns) {
            break;
        }
    }
    return drawable(fit(std::move(out), columns));
}

long long process_id() {
#if defined(_WIN32)
    return static_cast<long long>(_getpid());
#else
    return static_cast<long long>(::getpid());
#endif
}

/// AN ADDRESS A REMOTE INTERFACE CAN ATTACH TO, unique to this process and start.
std::string default_listen(std::int64_t n) {
    const std::string name = "zengine-neovim-" + std::to_string(process_id()) + "-" + std::to_string(n);
#if defined(_WIN32)
    return "\\\\.\\pipe\\" + name;
#else
    std::error_code ec;
    std::filesystem::path dir = std::filesystem::temp_directory_path(ec);
    if (ec) {
        dir = "/tmp";
    }
    return (dir / (name + ".sock")).string();
#endif
}

// =============================================================================

using NeovimEditorBase = loom::WeaveBase<
    class NeovimEditorWeave, nve::NeovimEditorState,
    loom::Accept<loom::Activated, timer::TimerReady, timer::TimerFired, timer::TimerResolution,
                 PaneCatalogRequested, PaneRoom, PanePressed, PaneDragged, PaneKey, PaneTextInput,
                 PaneWheel, PaneActionRequested, PaneQuitRequested, OpenSourceRequested,
                 PrepareSourceRequested, ManagedOpenProgress, ManagedOpenSettled, SourceOpened,
                 loom::DispatchRefused, ProjectRoot, surface::ClipboardCopy, surface::ClipboardText,
                 EditorHandoffJudgeRequested, EditorWarmRequested, EditorPreparationTick,
                 EditorHandoffRequested, EditorHandoffEnded, EditorAdoptRequested,
                 EditorLiveRequested, EditorRetireRequested, nve::NeovimStartRequested,
                 nve::NeovimStopRequested, nve::NeovimStatusRequested>,
    loom::Emit<PaneOffered, ws::v2::PaneActions, ws::v2::PaneContent, ws::v2::PaneCaret,
               PaneQuitAnswered, SourceOpened, SourcePrepared, OpenSourceRequested,
               ProjectRootRequested, surface::ClipboardCopy, surface::ClipboardTextRequested,
               EditorHandoffJudged, EditorWarmed, EditorHandoffOffered, EditorAdopted, EditorLive,
               EditorRetired, timer::EnsureTimer, timer::CancelTimer, loom::Result, loom::Refused>,
    loom::Claims<EditorDocument>>;

// WL-NVIM-01 -- agents/workshop/neovim.md
class NeovimEditorWeave : public NeovimEditorBase {
private:
    struct Candidate {
        bool live = false;
        std::uint64_t op = 0;
        bool same_path = false;
        std::string path;
        std::int64_t buf = 0;
        bool existed = false;
        nv::DocFacts doc;
        std::int64_t convention = 0;
        std::int64_t rows = 0;
        std::int64_t columns = 0;
    };

    struct Relay {
        std::string path;
        std::uint64_t correlation = 0;
        loom::Ticket attempt{};
        loom::DeferredAnswer answer;
    };

    struct Holding {
        bool active = false;
        std::int64_t op = 0;
        std::int64_t refused = 0;
    };

    struct Warm {
        std::int64_t op = 0;
        loom::DeferredAnswer answer;
    };

    struct Drag {
        bool down = false;
        std::int64_t row = 0;
        std::int64_t col = 0;
    };

public:
public:
    // ---- A reload, refused while Neovim runs ---------------------------------------------------

    /// THE SNAPSHOT A RELOAD CARRIES -- refused while a Neovim runs. The process, its buffers, its
    /// undo history and its unsaved changes belong to THIS incarnation, and a reload ends it: the
    /// honest answer is no, in words, until the maker ends Neovim or switches editors. The words
    /// also stand on the pane, because the kernel's own sentence about a refused snapshot is about
    /// a library, not about a maker's work.
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

    void on(const PaneRoom& room, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || room.pane != nve::kEditorPane) {
            return;
        }
        rows_ = room.rows;
        columns_ = room.columns;
        granted_ = true;
        if (!root_asked_) {
            ask_project_root(mail);
        }
        if (running()) {
            (void)host_->resize(ui_rows(rows_), ui_cols(columns_));
        } else if (!start_tried_) {
            // THE PANE WAS GIVEN ROOM AND NO NEOVIM RUNS YET: start one now, in this room. Once --
            // a start that failed is said on the pane and retried by the maker's next open, never by
            // the next room grant.
            (void)start_embedded(ui_rows(rows_), ui_cols(columns_));
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
        }
    }

    /// A PRESS ON NEOVIM'S SCREEN IS A MOUSE PRESS THERE; on the status row it is a focus statement
    /// and moves nothing. The release is said before the next input that is not the same drag.
    void on(const PanePressed& press, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || press.pane != nve::kEditorPane) {
            return;
        }
        if (held_still()) {
            return;
        }
        release_drag();
        if (!running() || press.row < kChromeRows) {
            return;
        }
        notice_.clear();
        drag_ = Drag{true, press.row - kChromeRows, press.column < 0 ? 0 : press.column};
        mouse("left", "press", drag_.row, drag_.col);
        flush(mail);
    }

    void on(const PaneDragged& drag, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || drag.pane != nve::kEditorPane) {
            return;
        }
        if (held_still() || !drag_.down || !running()) {
            return;
        }
        drag_.row = std::clamp<std::int64_t>(drag.row - kChromeRows, 0, ui_rows(rows_) - 1);
        drag_.col = std::clamp<std::int64_t>(drag.column, 0, ui_cols(columns_) - 1);
        mouse("left", "drag", drag_.row, drag_.col);
        flush(mail);
    }

    void on(const PaneWheel& wheel, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || wheel.pane != nve::kEditorPane) {
            return;
        }
        if (held_still() || !running()) {
            return;
        }
        release_drag();
        wheel_ += wheel.dy;
        const std::int64_t notches = static_cast<std::int64_t>(wheel_);
        wheel_ -= static_cast<double>(notches);
        const std::int64_t row = ui_rows(rows_) / 2;
        for (std::int64_t i = 0; i < (notches < 0 ? -notches : notches); ++i) {
            mouse("wheel", notches > 0 ? "up" : "down", row, 0);
        }
        flush(mail);
    }

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

    // ---- THE TWO OPEN DOORS (WL-OPEN) -----------------------------------------------------------

    /// THE OLD DOOR, RELAYED to the opening manager with the requester's right kept -- the standard
    /// Editor's promise, kept the same way.
    void on(const OpenSourceRequested& asked, loom::Mail& mail) {
        if (mail.authored_role().empty()) {
            return;
        }
        if (mail.authored_from_role(ws::kOpeningRole)) {
            (void)mail.answer(SourceOpened{false, "the opening office asks the Editor to prepare a source, not "
                                                  "to open one -- nothing was relayed for " +
                                                      asked.path});
            return;
        }
        if (holding_.active) {
            ++holding_.refused;
            (void)mail.answer(SourceOpened{false, "the Editor is being switched -- " + asked.path +
                                                      " was not opened; open it again once the switch has settled"});
            return;
        }
        if (relays_.size() >= kMaxRelays) {
            (void)mail.answer(SourceOpened{false, "too many opens are already being relayed through the Editor -- " +
                                                      asked.path + " was not opened; try again"});
            return;
        }
        const std::uint64_t correlation = ++asked_;
        const loom::Ticket attempt = mail.as_role(nve::kEditorOffice)
                                         .send_to_role(ws::kOpeningRole, OpenSourceRequested{asked.path}, correlation);
        if (!attempt.valid()) {
            (void)mail.answer(SourceOpened{false, "nothing was queued: the Editor could not ask the opening office to show " +
                                                      asked.path});
            return;
        }
        Relay relay;
        relay.path = asked.path;
        relay.correlation = correlation;
        relay.attempt = attempt;
        relay.answer = mail.defer_answer();
        relays_.push_back(std::move(relay));
    }

    void on(const SourceOpened& said, loom::Mail& mail) {
        if (!mail.answers_ask()) {
            return;
        }
        for (auto it = relays_.begin(); it != relays_.end(); ++it) {
            if (it->correlation == mail.correlation()) {
                (void)loom::answer_deferred(it->answer, mail, SourceOpened{said.accepted, said.refusal});
                relays_.erase(it);
                return;
            }
        }
    }

    void on(const loom::DispatchRefused& refused, loom::Mail& mail) {
        if (!mail.dispatch_refused()) {
            return;
        }
        const loom::Ticket attempt = refused.refused_attempt();
        if (!attempt.valid()) {
            return;
        }
        for (auto it = relays_.begin(); it != relays_.end(); ++it) {
            if (it->attempt.seq == attempt.seq) {
                (void)loom::answer_deferred(
                    it->answer, mail,
                    SourceOpened{false, std::string(ws::kOpeningRole) + " could not be reached (" + refused.reason +
                                            ") -- " + it->path + " was not opened"});
                relays_.erase(it);
                return;
            }
        }
    }

    void on(const ManagedOpenProgress&, loom::Mail&) {}

    /// THE MANAGED DOOR: PREPARE the file in Neovim, hidden, and OFFER its identity for the exact
    /// operation. Nothing is shown and the current buffer stays current, readable and editable; the
    /// candidate becomes what the pane shows only in `on_claim_published`. A Neovim holding the
    /// current buffer modified does not refuse: Neovim keeps it, loaded and modified, beside the new
    /// one -- unless `hidden` is off, which the showing then refuses in Neovim's words.
    void on(const PrepareSourceRequested& asked, loom::Mail& mail) {
        if (!mail.authored_from_role(ws::kOpeningRole)) {
            return;
        }
        if (holding_.active) {
            ++holding_.refused;
            (void)mail.answer(not_prepared(asked.op, "the Editor is being switched -- " + asked.path +
                                                         " was not prepared; open it again once the switch has settled"));
            return;
        }
        drop_candidate();
        std::string path;
        if (!asked.path.empty() && !std::filesystem::path(asked.path).is_absolute() && !project_known_) {
            (void)mail.answer(not_prepared(asked.op, "a relative path means nothing until this Editor is told where this "
                                                     "run began -- open " + asked.path + " by its full path"));
            return;
        }
        path = ws::persist::resolved_against(project_dir_, asked.path);
        std::string why;
        if (!ensure_running(why)) {
            (void)mail.answer(not_prepared(asked.op, why + " -- " + path + " was not opened"));
            return;
        }
        Candidate next;
        next.live = true;
        next.op = static_cast<std::uint64_t>(asked.op);
        next.path = path;
        next.rows = asked.rows;
        next.columns = asked.columns;
        EditorDocument offered;
        SourcePrepared prepared;
        prepared.op = asked.op;
        if (!doc_path().empty() && doc_path() == path) {
            next.same_path = true;
            offered = identity_now();
            offered.opened_by = asked.op;
            compose_current(prepared, asked.rows, asked.columns);
        } else {
            const std::optional<mp::Value> r = lua_now(nv::lua::kPrepare,
                                                       nv::rpc::params(mp::Value::str(path),
                                                                       mp::Value::integer(asked.rows > 0 ? asked.rows : 1)),
                                                       kAskMs, why);
            if (!r.has_value()) {
                (void)mail.answer(not_prepared(asked.op, "Neovim could not prepare " + path + ": " + why));
                return;
            }
            if (const mp::Value* refused = r->get("why"); refused != nullptr) {
                (void)mail.answer(not_prepared(asked.op, "Neovim refused " + path + ": " + refused->as_str()));
                return;
            }
            next.buf = r->get("buf") != nullptr ? r->get("buf")->as_int() : 0;
            next.existed = r->get("existed") != nullptr && r->get("existed")->as_bool(false);
            next.doc.known = true;
            next.doc.buf = next.buf;
            next.doc.name = r->get("name") != nullptr ? r->get("name")->as_str() : path;
            next.doc.modified = r->get("modified") != nullptr && r->get("modified")->as_bool(false);
            next.doc.tick = r->get("tick") != nullptr ? r->get("tick")->as_int() : 0;
            next.convention = r->get("fileformat") != nullptr && r->get("fileformat")->as_str() == "dos" ? 1 : 0;
            offered.path = spelled(next.doc.name);
            offered.doc_epoch = epoch_ + 1;
            offered.convention = next.convention;
            offered.content_revision = next.doc.tick;
            offered.dirty = next.doc.modified;
            offered.opened_by = asked.op;
            std::vector<std::string> lines;
            if (const mp::Value* got = r->get("lines"); got != nullptr) {
                for (const mp::Value& l : got->as_array()) {
                    lines.push_back(l.as_str());
                }
            }
            compose_preview(prepared, offered.path, lines, asked.rows, asked.columns);
        }
        const loom::JointResult offer = mail.offer(static_cast<std::uint64_t>(asked.op), offered);
        if (!offer.ok) {
            discard_prepared(next);
            (void)mail.answer(not_prepared(asked.op, offer_refusal(path, offer.why)));
            return;
        }
        prepared.ok = true;
        prepared.generation = offered.doc_epoch;
        candidate_ = std::move(next);
        (void)mail.answer(prepared);
    }

    void on(const ManagedOpenSettled& said, loom::Mail& mail) {
        if (!mail.authored_from_role(ws::kOpeningRole)) {
            return;
        }
        if (!said.committed) {
            if (candidate_.live && candidate_.op == static_cast<std::uint64_t>(said.op)) {
                drop_candidate();
            }
            if (!said.refusal.empty()) {
                notice(said.refusal, true);
            }
        } else if (!said.applied && !said.refusal.empty()) {
            notice(said.refusal, true);
        }
        say(mail);
    }

    /// THE PUBLICATION HOOK: the bus published this weave's document claim by a joint operation.
    /// The prepared buffer becomes the current one HERE -- asked of Neovim within a bound -- so a
    /// delivery queued behind the commitment finds it current. A showing Neovim refuses (its
    /// `hidden` is off and the current buffer is modified; the buffer went away) or cannot answer is
    /// DECLINED, in words, and the pane keeps what it shows.
    bool on_claim_published(const EditorDocument& published) {
        claimed_ = published;
        claimed_ever_ = true;
        if (candidate_.live && published.opened_by == static_cast<std::int64_t>(candidate_.op)) {
            if (!candidate_.same_path) {
                std::string why;
                const std::optional<mp::Value> shown =
                    lua_now(nv::lua::kShow, nv::rpc::params(mp::Value::integer(candidate_.buf)), kAskMs, why);
                const mp::Value* refused = shown.has_value() ? shown->get("why") : nullptr;
                if (!shown.has_value() || refused != nullptr) {
                    const std::string words = shown.has_value() ? refused->as_str() : why;
                    epoch_ = std::max(epoch_, published.doc_epoch) + 1;
                    notice("Neovim did not show " + candidate_.path + ": " + words, true);
                    candidate_ = Candidate{};
                    mirror_state();
                    resay_ = true;
                    return false;
                }
                doc_ = candidate_.doc;
                convention_ = candidate_.convention;
            }
            epoch_ = published.doc_epoch;
            opened_by_ = candidate_.op;
            granted_ = true;
            rows_ = candidate_.rows;
            columns_ = candidate_.columns;
            if (running()) {
                (void)host_->resize(ui_rows(rows_), ui_cols(columns_));
            }
            notice((doc_.modified ? "UNSAVED edits stand -- editing " : "editing ") + shown_path(doc_path()), false);
            candidate_ = Candidate{};
            mirror_state();
            resay_ = true;
            return true;
        }
        epoch_ = std::max(epoch_, published.doc_epoch) + 1;
        notice("the desk published " + published.path + " for an opening this Editor did not prepare -- " +
                   (doc_path().empty() ? std::string("no source is open") : "keeping " + shown_path(doc_path())),
               true);
        mirror_state();
        resay_ = true;
        return false;
    }

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

    // ---- A switch: this editor as the incumbent (WL-SWITCH-04) ---------------------------------

    /// WHAT WOULD A SWITCH AWAY COST? Asked of Neovim now: the other modified buffers and running
    /// terminal jobs are losses a maker must agree to; the undo history, registers, marks, extra
    /// windows and clean buffers are resets; a state no switch may begin in is refused in words.
    // WL-NVIM-04 -- agents/workshop/neovim.md
    void on(const EditorHandoffJudgeRequested& asked, loom::Mail& mail) {
        if (!mail.authored_from_role(ws::kEditorSwitchRole)) {
            return;
        }
        EditorHandoffJudged judged;
        judged.op = asked.op;
        Judgement j = judge_now();
        judged.refusal = j.refusal;
        judged.ok = j.refusal.empty();
        if (judged.ok) {
            judged.losses = j.losses;
            judged.resets = j.resets;
            judged.digest = ws::handoff_digest(j.losses);
            judged.rows = rows_;
            judged.columns = columns_;
            judged.project_dir = project_dir_;
            judged.project_known = project_known_;
        }
        (void)mail.answer(judged);
    }

    /// THE BOUNDARY: Neovim's exact document, asked now, and this editor holds still -- no input
    /// reaches Neovim until the outcome is told.
    // WL-NVIM-04 -- agents/workshop/neovim.md
    void on(const EditorHandoffRequested& asked, loom::Mail& mail) {
        if (!mail.authored_from_role(ws::kEditorSwitchRole)) {
            return;
        }
        EditorHandoffOffered offered;
        offered.op = asked.op;
        release_drag();
        Judgement j = judge_now();
        if (!j.refusal.empty()) {
            offered.refusal = j.refusal;
            (void)mail.answer(offered);
            return;
        }
        holding_ = Holding{true, asked.op, 0};
        offered.ok = true;
        offered.losses = j.losses;
        offered.digest = ws::handoff_digest(j.losses);
        offered.transfer = std::move(j.transfer);
        offered.resets = j.resets;
        offered.notes = j.notes;
        (void)mail.answer(offered);
    }

    void on(const EditorHandoffEnded& ended, loom::Mail& mail) {
        if (!mail.authored_from_role(ws::kEditorSwitchRole) || !holding_.active || holding_.op != ended.op) {
            return;
        }
        const std::int64_t refused = holding_.refused;
        holding_ = Holding{};
        std::string said = "the editor switch did not happen";
        if (!ended.why.empty()) {
            said += " (" + ended.why + ")";
        }
        if (refused > 0) {
            said += " -- " + std::to_string(refused) + (refused == 1 ? " input was" : " inputs were") +
                    " not applied while it was being prepared";
        }
        notice(said, refused > 0);
        say(mail);
    }

    void on(const EditorRetireRequested& asked, loom::Mail& mail) {
        if (!holding_.active || holding_.op != asked.op) {
            return;
        }
        (void)mail.answer(EditorRetired{asked.op, holding_.refused});
    }

    // ---- A switch: this editor as the candidate, sealed (WL-SWITCH-05) -----------------------

    /// START NEOVIM in the room the incumbent had, and answer when it is ready -- later, from a
    /// relayed tick, because a start is the one step that waits on something outside this process.
    // WL-NVIM-05 -- agents/workshop/neovim.md
    void on(const EditorWarmRequested& asked, loom::Mail& mail) {
        if (activation_.activated()) {
            return;
        }
        rows_ = asked.rows;
        columns_ = asked.columns;
        project_dir_ = asked.project_dir;
        project_known_ = asked.project_known;
        if (running() && host_->ready()) {
            (void)mail.answer(EditorWarmed{asked.op, true, std::string(), started_words()});
            return;
        }
        if (!running() && !start_embedded(ui_rows(rows_ > 0 ? rows_ : kDefaultRows),
                                          ui_cols(columns_ > 0 ? columns_ : kDefaultCols))) {
            (void)mail.answer(EditorWarmed{asked.op, false, failure_, std::string()});
            return;
        }
        warm_.op = asked.op;
        warm_.answer = mail.defer_answer();
        settle_warm(mail);
    }

    void on(const EditorPreparationTick&, loom::Mail& mail) {
        if (activation_.activated()) {
            return;
        }
        pump(mail);
    }

    /// ADOPT THE DOCUMENT, OR SAY EXACTLY WHY NOT: the bytes into a buffer loaded from the same file
    /// (no undo step, the line format and final newline set, the modified flag carried), the caret
    /// and selection placed by keys and READ BACK, and the viewport restored. What did not land
    /// exactly is a note, never a silence.
    // WL-NVIM-03 -- agents/workshop/neovim.md
    void on(const EditorAdoptRequested& asked, loom::Mail& mail) {
        if (activation_.activated()) {
            return;
        }
        EditorAdopted adopted;
        adopted.op = asked.op;
        const EditorTransfer& t = asked.transfer;
        project_dir_ = t.project_dir;
        project_known_ = t.project_known;
        if (!running() || !host_->ready()) {
            adopted.refusal = "Neovim is not running" + (failure_.empty() ? std::string() : ": " + failure_);
            (void)mail.answer(adopted);
            return;
        }
        epoch_ = t.doc_epoch < 0 ? 1 : t.doc_epoch + 1;
        if (t.path.empty()) {
            adopted_ = true;
            adopted.ready = true;
            (void)mail.answer(adopted);
            return;
        }
        const nv::NeovimText text = nv::neovim_text(t.text);
        const bool dos = text.dos || (t.convention == 1 && t.text.find('\n') == std::string::npos);
        mp::Value::Array lines;
        for (const std::string& l : text.lines) {
            lines.push_back(mp::Value::str(l));
        }
        std::string why;
        const std::optional<mp::Value> r = lua_now(
            nv::lua::kAdopt,
            nv::rpc::params(mp::Value::str(t.path), mp::Value::array(std::move(lines)), mp::Value::boolean(dos),
                            mp::Value::boolean(text.final_newline), mp::Value::boolean(t.modified)),
            kAdoptMs, why);
        if (!r.has_value()) {
            adopted.refusal = "Neovim could not adopt " + t.path + ": " + why;
            (void)mail.answer(adopted);
            return;
        }
        if (const mp::Value* refused = r->get("why"); refused != nullptr) {
            adopted.refusal = "Neovim would not adopt " + t.path + ": " + refused->as_str();
            (void)mail.answer(adopted);
            return;
        }
        const nv::Pos anchor{t.anchor_row, t.anchor_byte};
        const nv::Pos caret{t.caret_row, t.caret_byte};
        const nv::Placement placed = nv::place(text.lines, text.final_newline, anchor, caret);
        (void)host_->input(nv::placement_keys(placed));
        if (placed.adjusted) {
            adopted.notes.push_back(placed.note);
        }
        // THE CARET, READ BACK: what Neovim holds now is the fact, and a place that differs from the
        // one the transfer named -- beyond the adjustment `place` already said -- is said too.
        const std::optional<Exported> back = export_now(why);
        if (!back.has_value()) {
            adopted.refusal = "Neovim adopted " + t.path + " and could not say where its caret is: " + why;
            (void)mail.answer(adopted);
            return;
        }
        if (!placed.adjusted) {
            const nv::Carried carried = nv::carry(back->lines, back->final_newline, back->at);
            if (!(carried.caret == caret) || !(carried.anchor == anchor)) {
                adopted.notes.push_back("the caret landed at line " + std::to_string(carried.caret.row + 1) +
                                        ", byte " + std::to_string(carried.caret.byte + 1) + " rather than line " +
                                        std::to_string(caret.row + 1) + ", byte " + std::to_string(caret.byte + 1));
            }
        }
        (void)lua_now(nv::lua::kView,
                      nv::rpc::params(mp::Value::integer(t.first_row + 1), mp::Value::integer(t.first_col)),
                      kAskMs, why);
        doc_ = nv::DocFacts{true, back->buf, back->name, back->modified, back->tick, back->buftype};
        convention_ = dos ? 1 : 0;
        adopted_ = true;
        kept_ = t; // carried back as it was handed if this Neovim ends before it proves it serves
        adopted.ready = true;
        (void)mail.answer(adopted);
    }

    /// SERVING? A round trip to Neovim that names what it holds.
    void on(const EditorLiveRequested& asked, loom::Mail& mail) {
        if (!mail.authored_from_role(ws::kEditorSwitchRole)) {
            return;
        }
        if (!activation_.activated() || !running()) {
            (void)mail.answer(EditorLive{asked.op, false,
                                         "Neovim is not running" + (failure_.empty() ? std::string() : ": " + failure_)});
            return;
        }
        std::string why;
        const std::optional<mp::Value> facts = lua_now(nv::lua::kDocFacts, nv::rpc::params(), kAskMs, why);
        if (!facts.has_value()) {
            (void)mail.answer(EditorLive{asked.op, false, "Neovim did not answer: " + why});
            return;
        }
        kept_.reset();
        (void)mail.answer(EditorLive{asked.op, true,
                                     started_words() + " holds " + (doc_path().empty() ? std::string("no source") : doc_path())});
    }

    // ---- From a baseline Loom: a second terminal's Neovim --------------------------------------

    // WL-NVIM-08 -- agents/workshop/neovim.md
    void on(const nve::NeovimStartRequested& asked, loom::Mail& mail) {
        if (running()) {
            (void)mail.answer(loom::Refused{"Neovim is already running (" + attached_words() +
                                            ") -- stop it first with NeovimStopRequested"});
            return;
        }
        listen_ = asked.listen.empty() ? default_listen(state_.starts + 1) : asked.listen;
        if (!start(nv::UiMode::Remote, kDefaultRows, kDefaultCols)) {
            (void)mail.answer(loom::Refused{failure_});
            return;
        }
        std::string why;
        if (!wait_ready(why)) {
            (void)mail.answer(loom::Refused{why});
            return;
        }
        std::string opened;
        if (!asked.path.empty()) {
            const std::string path = ws::persist::resolved_against(
                project_known_ ? project_dir_ : std::filesystem::current_path().generic_string(), asked.path);
            const std::optional<mp::Value> r = lua_now(nv::lua::kPrepare,
                                                       nv::rpc::params(mp::Value::str(path), mp::Value::integer(1)),
                                                       kAskMs, why);
            if (!r.has_value() || r->get("why") != nullptr) {
                const std::string words = r.has_value() ? r->get("why")->as_str() : why;
                (void)host_->finish(nv::kQuitGraceMs);
                (void)mail.answer(loom::Refused{"Neovim could not open " + path + ": " + words});
                return;
            }
            (void)lua_now(nv::lua::kShow, nv::rpc::params(mp::Value::integer(r->get("buf")->as_int())), kAskMs, why);
            opened = " editing " + path;
        }
        (void)mail.answer(loom::Result{started_words() + " is listening at " + listen_ + opened +
                                       " -- attach its interface with: nvim --server " + listen_ + " --remote-ui"});
    }

    void on(const nve::NeovimStopRequested& asked, loom::Mail& mail) {
        if (!running()) {
            (void)mail.answer(loom::Result{"Neovim is not running" + (failure_.empty() ? std::string() : ": " + failure_)});
            return;
        }
        if (holding_.active) {
            (void)mail.answer(loom::Refused{"the Editor is being switched -- stop Neovim once the switch has settled"});
            return;
        }
        if (!asked.discard) {
            std::string why;
            const std::optional<mp::Value> unsaved = lua_now(nv::lua::kUnsaved, nv::rpc::params(), kAskMs, why);
            if (!unsaved.has_value()) {
                (void)mail.answer(loom::Refused{"Neovim could not be asked whether it holds unsaved work (" + why + ")"});
                return;
            }
            std::vector<std::string> named;
            if (const mp::Value* buffers = unsaved->get("buffers"); buffers != nullptr) {
                for (const mp::Value& b : buffers->as_array()) {
                    const mp::Value* n = b.get("name");
                    named.push_back(n == nullptr || n->as_str().empty() ? std::string("[No Name]") : spelled(n->as_str()));
                }
            }
            if (!named.empty()) {
                (void)mail.answer(loom::Refused{"Neovim holds unsaved changes to " + listed(named) +
                                                " -- write them first, or stop with discard=true to lose them"});
                return;
            }
        }
        const nv::Child::Ended ended = host_->finish(nv::kQuitGraceMs);
        failure_ = "Neovim was stopped";
        doc_ = nv::DocFacts{};
        notice("Neovim was stopped -- open a source to start it again", false);
        resay_ = true;
        (void)mail.answer(loom::Result{"Neovim ended (status " + std::to_string(ended.status) +
                                       (ended.forced ? ", forced" : "") + ")"});
    }

    void on(const nve::NeovimStatusRequested&, loom::Mail& mail) {
        if (!running()) {
            (void)mail.answer(loom::Result{"Neovim is not running" + (failure_.empty() ? std::string() : ": " + failure_)});
            return;
        }
        std::string why;
        const std::optional<mp::Value> facts = lua_now(nv::lua::kDocFacts, nv::rpc::params(), kAskMs, why);
        if (!facts.has_value()) {
            (void)mail.answer(loom::Result{started_words() + " (" + attached_words() + ") could not say what it holds: " + why});
            return;
        }
        const mp::Value* name = facts->get("name");
        const mp::Value* modified = facts->get("modified");
        const std::string path = name == nullptr || name->as_str().empty() ? std::string("no file") : spelled(name->as_str());
        (void)mail.answer(loom::Result{started_words() + " (" + attached_words() + ") is editing " + path + ", " +
                                       (modified != nullptr && modified->as_bool(false) ? "modified" : "saved") +
                                       ", in mode " + host_->mode()});
    }

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
        choice_ = nv::choice_from_environment();
        // WHICH CONFIGURATION, JUDGED BEFORE A NEOVIM EXISTS. A profile that is neither spelling
        // and names no file is the maker's own typo, and saying so here is the difference between
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
        if (!running() && !start_embedded(ui_rows(rows_ > 0 ? rows_ : kDefaultRows),
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

    /// UNTIL NEOVIM IS NO LONGER WAITING FOR INPUT, at most `ms`: its fast mode, asked every 10 ms.
    bool wait_unblocked(int ms) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
        while (running()) {
            std::string why;
            const std::optional<nv::rpc::Response> m = host_->call_now("nvim_get_mode", nv::rpc::params(), ms, why);
            const mp::Value* blocking = m.has_value() ? m->result.get("blocking") : nullptr;
            if (blocking != nullptr && !blocking->as_bool(false)) {
                return true;
            }
            if (std::chrono::steady_clock::now() >= deadline) {
                return false;
            }
            (void)host_->wait(10);
        }
        return false;
    }

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

    /// EVERYTHING A TRANSFER AND A JUDGMENT ARE DECIDED FROM, asked now.
    struct Exported {
        std::string mode;
        std::int64_t buf = 0;
        std::string name;
        std::string buftype;
        bool modified = false;
        std::int64_t tick = 0;
        std::vector<std::string> lines;
        std::string fileformat;
        bool final_newline = false;
        bool bomb = false;
        bool binary = false;
        nv::NeovimPosition at;
        std::int64_t topline = 1;
        std::int64_t leftcol = 0;
        bool wrap = true;
        std::vector<std::string> others;
        std::vector<std::string> jobs;
        std::string recording;
        std::int64_t windows = 1;
        std::int64_t tabs = 1;
        std::int64_t listed = 1;
    };

    std::optional<Exported> export_now(std::string& why) {
        const std::optional<mp::Value> v = lua_now(nv::lua::kExport, nv::rpc::params(), kAdoptMs, why);
        if (!v.has_value()) {
            return std::nullopt;
        }
        const auto str = [&v](const char* k) {
            const mp::Value* f = v->get(k);
            return f != nullptr ? f->as_str() : std::string();
        };
        const auto num = [&v](const char* k, std::int64_t d) {
            const mp::Value* f = v->get(k);
            return f != nullptr ? f->as_int(d) : d;
        };
        const auto flag = [&v](const char* k, bool d) {
            const mp::Value* f = v->get(k);
            return f != nullptr ? f->as_bool(d) : d;
        };
        Exported e;
        e.mode = str("mode");
        e.buf = num("buf", 0);
        e.name = str("name");
        e.buftype = str("buftype");
        e.modified = flag("modified", false);
        e.tick = num("tick", 0);
        if (const mp::Value* lines = v->get("lines"); lines != nullptr) {
            for (const mp::Value& l : lines->as_array()) {
                e.lines.push_back(l.as_str());
            }
        }
        if (e.lines.empty()) {
            e.lines.emplace_back();
        }
        e.fileformat = str("fileformat");
        e.final_newline = nv::writes_final_newline(e.lines, flag("eol", true), flag("fixeol", true),
                                                   flag("binary", false), num("bytes", 0));
        e.bomb = flag("bomb", false);
        e.binary = flag("binary", false);
        e.at.mode = e.mode;
        if (const mp::Value* c = v->get("cursor"); c != nullptr) {
            e.at.cursor = nv::Pos{c->at(0).as_int(), c->at(1).as_int()};
        }
        if (const mp::Value* s = v->get("vstart"); s != nullptr) {
            e.at.vstart = nv::Pos{s->at(0).as_int(), s->at(1).as_int()};
        }
        e.topline = num("topline", 1);
        e.leftcol = num("leftcol", 0);
        e.wrap = flag("wrap", true);
        if (const mp::Value* others = v->get("others"); others != nullptr) {
            for (const mp::Value& o : others->as_array()) {
                const mp::Value* n = o.get("name");
                e.others.push_back(n == nullptr || n->as_str().empty() ? std::string("[No Name]") : spelled(n->as_str()));
            }
        }
        if (const mp::Value* jobs = v->get("jobs"); jobs != nullptr) {
            for (const mp::Value& j : jobs->as_array()) {
                const mp::Value* n = j.get("name");
                e.jobs.push_back(n == nullptr ? std::string("a terminal") : n->as_str());
            }
        }
        e.recording = str("recording");
        e.windows = num("windows", 1);
        e.tabs = num("tabs", 1);
        e.listed = num("listed", 1);
        return e;
    }

    /// WHAT A SWITCH AWAY WOULD CARRY, LOSE, RESET, OR REFUSE -- asked of Neovim at this instant.
    struct Judgement {
        std::string refusal;
        std::vector<std::string> losses;
        std::vector<std::string> resets;
        std::vector<std::string> notes;
        EditorTransfer transfer;
    };

    Judgement judge_now() {
        Judgement j;
        j.transfer.doc_epoch = epoch_;
        j.transfer.project_dir = project_dir_;
        j.transfer.project_known = project_known_;
        j.transfer.source = started_words();
        if (candidate_.live) {
            j.refusal = "Neovim is still opening " + candidate_.path + " -- switch once it has settled";
            return j;
        }
        if (!pastes_.empty()) {
            j.refusal = "Neovim is still waiting for a clipboard answer -- switch once it has arrived";
            return j;
        }
        if (!relays_.empty()) {
            j.refusal = "the Editor is still relaying an open -- switch once it has settled";
            return j;
        }
        if (!running()) {
            // NO NEOVIM, NO DOCUMENT -- unless one was handed to this editor and Neovim ended before
            // it proved it serves: that document is carried back exactly as it was handed.
            if (kept_.has_value()) {
                j.transfer = *kept_;
                j.transfer.doc_epoch = epoch_;
                j.notes.push_back("Neovim ended before it served; the document it was handed is carried back as it was handed");
            }
            return j;
        }
        std::string why;
        std::optional<Exported> e = export_now(why);
        if (!e.has_value() && running()) {
            // NEOVIM IS WAITING FOR INPUT (measured: a typed count holds every non-fast request, and
            // so does a prompt). A PROMPT is the maker's to answer: refused. AN UNFINISHED COMMAND is
            // a count or a register name nobody finished: cancelled the way a maker would, with
            // Escape, and said among the resets -- then Neovim is asked again.
            std::string mode_why;
            const std::optional<nv::rpc::Response> m =
                host_->call_now("nvim_get_mode", nv::rpc::params(), kAskMs, mode_why);
            const mp::Value* mode = m.has_value() ? m->result.get("mode") : nullptr;
            const mp::Value* blocking = m.has_value() ? m->result.get("blocking") : nullptr;
            if (mode != nullptr && blocking != nullptr && blocking->as_bool(false)) {
                const std::string waiting = mode->as_str();
                if (!waiting.empty() && waiting[0] == 'r') {
                    j.refusal = "Neovim is waiting at a prompt -- answer it in Neovim and switch again";
                    return j;
                }
                (void)host_->input("<Esc>");
                // THE ESCAPE IS TYPEAHEAD, and the fast mode is answered ahead of typeahead: asked at
                // once it would still say waiting. So Neovim is asked until it has taken the key.
                (void)wait_unblocked(kAskMs);
                e = export_now(why);
                if (e.has_value()) {
                    j.resets.push_back("an unfinished command in Neovim (it was waiting in mode `" + waiting + "`)");
                }
            }
        }
        if (!e.has_value()) {
            j.refusal = "Neovim could not be asked what it holds (" + why + ")";
            return j;
        }
        if (!e->buftype.empty()) {
            j.refusal = "Neovim's current buffer is not a file (buftype " + e->buftype +
                        ") -- go to a file's buffer and switch again";
            return j;
        }
        const bool empty = e->lines.size() == 1 && e->lines.front().empty();
        if (e->name.empty() && !(empty && !e->modified)) {
            j.refusal = "Neovim's current buffer has no file name -- write it to a file (:w <path>) and switch again";
            return j;
        }
        if (e->fileformat == "mac") {
            j.refusal = "Neovim's current buffer uses 'mac' line endings, which a transfer cannot carry";
            return j;
        }
        if (e->bomb) {
            j.refusal = "Neovim's current buffer writes a byte-order mark ('bomb'), which a transfer cannot carry";
            return j;
        }
        if (e->binary) {
            j.refusal = "Neovim's current buffer is in 'binary' mode, which a transfer cannot carry";
            return j;
        }
        for (const std::string& other : e->others) {
            j.losses.push_back("unsaved changes to " + other + " in another Neovim buffer");
        }
        for (const std::string& job : e->jobs) {
            j.losses.push_back("the terminal job " + job + " running in Neovim");
        }
        j.resets.push_back("Neovim's undo history");
        j.resets.push_back("Neovim's registers, marks and jumplist");
        if (e->windows > 1) {
            j.resets.push_back(std::to_string(e->windows) + " Neovim windows (the current one's buffer is carried)");
        }
        if (e->tabs > 1) {
            j.resets.push_back(std::to_string(e->tabs) + " Neovim tab pages");
        }
        if (e->listed > 1) {
            j.resets.push_back(std::to_string(e->listed - 1) + " other listed Neovim buffers");
        }
        if (!e->recording.empty()) {
            j.resets.push_back("the macro being recorded into register " + e->recording);
        }
        if (e->name.empty()) {
            return j; // nothing open, nothing lost with it
        }
        const bool dos = e->fileformat == "dos";
        EditorTransfer& t = j.transfer;
        t.path = spelled(e->name);
        t.text = nv::file_bytes(e->lines, dos, e->final_newline);
        t.convention = dos ? 1 : 0;
        t.modified = e->modified;
        if (!e->modified) {
            t.saved_text = t.text;
        } else {
            const ws::persist::FileText saved = ws::persist::read_file(t.path, 1u << 28, "a source file");
            std::error_code ec;
            if (saved.outcome.accepted) {
                t.saved_text = saved.text;
            } else if (!std::filesystem::exists(t.path, ec)) {
                t.saved_text.clear();
                j.notes.push_back(t.path + " does not exist on disk yet, so nothing of it counts as saved");
            } else {
                j.refusal = "the saved copy of " + t.path + " could not be read to compare against (" +
                            saved.outcome.refusal + ")";
                return j;
            }
        }
        const nv::Carried carried = nv::carry(e->lines, e->final_newline, e->at);
        t.anchor_row = carried.anchor.row;
        t.anchor_byte = carried.anchor.byte;
        t.caret_row = carried.caret.row;
        t.caret_byte = carried.caret.byte;
        t.first_row = nv::first_row_of(e->topline);
        t.first_col = nv::first_col_of(e->leftcol, e->wrap);
        if (!carried.note.empty()) {
            j.notes.push_back(carried.note);
        }
        return j;
    }

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
        for (const nv::ClipboardCopy& copy : seen.copies) {
            clip_ = join_lines(copy.lines, copy.regtype == "V" || copy.regtype == "line");
            (void)mail.publish(surface::ClipboardCopy{clip_});
        }
        for (const std::uint32_t id : seen.paste_requests) {
            const std::uint64_t correlation = ++asked_;
            const loom::Ticket asked = mail.as_role(nve::kEditorOffice)
                                           .send_to_role(surface::kSkinRole, surface::ClipboardTextRequested{}, correlation);
            if (asked.valid()) {
                pastes_.emplace(correlation, id);
            } else {
                const PasteLines paste = paste_lines(clip_); // no Skin: the mirror is the clipboard
                (void)host_->answer_paste(id, paste.lines, paste.regtype);
            }
        }
        settle_warm(mail);
        if (seen.flushed || seen.mode_changed || seen.doc_changed || seen.became_ready || seen.failed || seen.ended) {
            resay_ = true;
        }
    }

    void settle_warm(loom::Mail& mail) {
        if (!warm_.answer.valid() || host_ == nullptr) {
            return;
        }
        if (host_->ready()) {
            (void)loom::answer_deferred(warm_.answer, mail, EditorWarmed{warm_.op, true, std::string(), started_words()});
            warm_ = Warm{};
        } else if (!host_->alive()) {
            failure_ = host_->failure();
            (void)loom::answer_deferred(warm_.answer, mail, EditorWarmed{warm_.op, false, failure_, std::string()});
            warm_ = Warm{};
        }
    }

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

    static std::int64_t ui_rows(std::int64_t rows) { return std::max(rows - kChromeRows, kMinUiRows); }
    static std::int64_t ui_cols(std::int64_t cols) { return std::max(cols - kCaretCols, kMinUiCols); }

    std::string shown_path(const std::string& path) const {
        return tail_of_path(path, columns_ > 24 ? columns_ - 24 : columns_);
    }

    void announce(loom::Mail& mail) {
        (void)mail.as_role(nve::kEditorOffice)
            .send_to_role(kWorkshopRole, PaneOffered{nve::kEditorPane, nve::kPaneName, nve::kPaneSummary});
        declare(mail);
    }

    /// TWO ROWS, EACH STANDING IN FOR ONE OF WORKSHOP'S OWN while this pane holds the keys: the save
    /// chord writes the buffer, and the open chord is Neovim's own jump back. Every other key reaches
    /// Neovim as a key.
    void declare(loom::Mail& mail) {
        ws::v2::PaneActions actions;
        actions.pane = nve::kEditorPane;
        actions.rows.push_back(ws::v2::PaneActionRow{nve::kActionWrite, "write (:w)", zengine::input::scan::kS,
                                                     zengine::input::mod::kCtrl, ws::kOwnableDocumentSave});
        actions.rows.push_back(ws::v2::PaneActionRow{nve::kActionJumpOlder, "jump back (<C-o>)",
                                                     zengine::input::scan::kO, zengine::input::mod::kCtrl,
                                                     ws::kOwnableDocumentOpen});
        (void)mail.as_role(nve::kEditorOffice).send_to_role(kWorkshopRole, actions);
    }

    void ask_project_root(loom::Mail& mail) {
        root_asked_ = true;
        root_pending_ = ++asked_;
        (void)mail.as_role(nve::kEditorOffice).send_to_role(ws::kProjectRole, ProjectRootRequested{}, root_pending_);
    }

    void notice(std::string text, bool bad) {
        notice_ = std::move(text);
        notice_bad_ = bad;
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
        // ...AND WHICH CONFIGURATION IT IS RUNNING, because a maker whose plugins are missing has
        // no other way to tell `clean` from `user` once Neovim is up (the full words are in the
        // start and status answers).
        std::string head = std::string(doc_.modified ? "UNSAVED " : "saved ") +
                           mode_word(host_->mode()) + " " + nv::profile_tag(choice_) + " -- ";
        return head + (path.empty() ? std::string("no file") : tail_of_path(path, columns - static_cast<std::int64_t>(head.size())));
    }

    /// THE PANE, SAID: the status row (or a standing notice in its place), then Neovim's screen,
    /// cropped to the room; the caret and the one range beside the rows, in the same lattice.
    void say(loom::Mail& mail) {
        if (!granted_) {
            return;
        }
        std::vector<surface::SurfaceTextRow> rows;
        ws::v2::PaneCaret caret;
        caret.pane = nve::kEditorPane;
        caret.generation = epoch_;
        compose(rows, caret, rows_, columns_);
        ++state_.screens;
        (void)mail.as_role(nve::kEditorOffice)
            .send_to_role(kWorkshopRole, ws::v2::PaneContent{nve::kEditorPane, std::move(rows), epoch_});
        (void)mail.as_role(nve::kEditorOffice).send_to_role(kWorkshopRole, caret);
    }

    // WL-NVIM-02 -- agents/workshop/neovim.md
    void compose(std::vector<surface::SurfaceTextRow>& rows, ws::v2::PaneCaret& caret, std::int64_t room_rows,
                 std::int64_t room_cols) const {
        if (room_rows <= 0 || room_cols <= 0) {
            return;
        }
        if (!notice_.empty()) {
            rows.push_back(surface::SurfaceTextRow{drawable(fit(notice_, room_cols)),
                                                   notice_bad_ ? surface::role::kAlert : surface::role::kMuted});
        } else {
            rows.push_back(surface::SurfaceTextRow{drawable(fit(status_text(room_cols), room_cols)), surface::role::kAccent});
        }
        if (!running() || !host_->ready()) {
            return;
        }
        const nv::Screen screen = nv::project(host_->grid(), host_->visual_kind());
        const std::int64_t shown_rows = std::min<std::int64_t>(static_cast<std::int64_t>(screen.rows.size()),
                                                               room_rows - kChromeRows);
        const std::int64_t text_cols = room_cols - kCaretCols > 0 ? room_cols - kCaretCols : 0;
        for (std::int64_t r = 0; r < shown_rows; ++r) {
            const nv::Screen::Row& row = screen.rows[static_cast<std::size_t>(r)];
            std::int64_t role = surface::role::kFill;
            switch (row.kind) {
            case nv::RowKind::Chrome: role = surface::role::kAccent; break;
            case nv::RowKind::Muted: role = surface::role::kMuted; break;
            case nv::RowKind::Alert: role = surface::role::kAlert; break;
            case nv::RowKind::Text: break;
            }
            std::string text = row.text;
            while (!text.empty() && text.back() == ' ') {
                text.pop_back();
            }
            rows.push_back(surface::SurfaceTextRow{drawable(fit(std::move(text), text_cols)), role});
        }
        if (screen.caret_row >= 0 && screen.caret_row < shown_rows && screen.caret_col <= text_cols) {
            caret.row = screen.caret_row + kChromeRows;
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
                caret.sel_begin_row = screen.sel_begin_row + kChromeRows;
                caret.sel_begin_col = bcol;
                caret.sel_end_row = erow + kChromeRows;
                caret.sel_end_col = ecol;
            }
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

    static bool same_identity(const EditorDocument& a, const EditorDocument& b) {
        return a.path == b.path && a.doc_epoch == b.doc_epoch && a.convention == b.convention &&
               a.content_revision == b.content_revision && a.dirty == b.dirty && a.opened_by == b.opened_by;
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

    static std::string offer_refusal(const std::string& path, loom::JointRefusal why) {
        switch (why) {
        case loom::JointRefusal::StaleRevision:
        case loom::JointRefusal::ParticipantChanged:
        case loom::JointRefusal::WrongState:
        case loom::JointRefusal::Cancelled:
            return "the document changed while opening " + path + " -- try again";
        default:
            return "the opening was no longer arranged (" + std::string(loom::name_of(why)) + ") -- " + path +
                   " was not prepared";
        }
    }

    static SourcePrepared not_prepared(std::int64_t op, std::string refusal) {
        SourcePrepared refused;
        refused.op = op;
        refused.ok = false;
        refused.refusal = std::move(refusal);
        return refused;
    }

    /// A PREPARED BUFFER NOBODY SHOWED goes, when it holds nothing and was not there before.
    void discard_prepared(const Candidate& c) {
        if (!c.live || c.same_path || c.buf == 0 || !running()) {
            return;
        }
        std::string why;
        (void)lua_now(nv::lua::kDiscard, nv::rpc::params(mp::Value::integer(c.buf), mp::Value::boolean(c.existed)),
                      kAskMs, why);
    }

    void drop_candidate() {
        if (candidate_.live) {
            discard_prepared(candidate_);
        }
        candidate_ = Candidate{};
    }

    static std::string listed(const std::vector<std::string>& names) {
        std::string out;
        const std::size_t shown = std::min<std::size_t>(names.size(), 3);
        for (std::size_t i = 0; i < shown; ++i) {
            if (i > 0) {
                out += ", ";
            }
            out += names[i];
        }
        if (names.size() > shown) {
            out += " and " + std::to_string(names.size() - shown) + " more";
        }
        return out;
    }

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
    double wheel_ = 0.0;
    bool adopted_ = false;
    std::optional<EditorTransfer> kept_;

    std::string clip_;
    std::map<std::uint64_t, std::uint32_t> pastes_;

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
};

} // namespace

ZEN_EXPORT_WEAVE(NeovimEditorWeave)
