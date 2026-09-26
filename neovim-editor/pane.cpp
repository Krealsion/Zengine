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
// (`neovim/projection.hpp`), and the Editor's facts as the office needs them: the identity it
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

#include "neovim-editor/vocabulary.hpp"

#include "neovim/document.hpp"
#include "neovim/host.hpp"
#include "neovim/keys.hpp"
#include "neovim/launch.hpp"
#include "neovim/lua.hpp"
#include "neovim/projection.hpp"

#include "source-transfer/cpp.hpp"
#include "source-transfer/material.hpp"
#include "workshop/editor_handoff_vocabulary.hpp"
#include "workshop/editor_switch_vocabulary.hpp"
#include "workshop/open_seam_vocabulary.hpp"
#include "workshop/pane_carry.hpp"
#include "workshop/pane_menu.hpp"
#include "workshop/pane_operation.hpp"
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
#include <functional>
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
using ws::PaneButton;
using ws::PaneDragged;
using ws::PaneKey;
using ws::v2::PaneOffered;
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

/// ONE FIELD OF A MODULE ANSWER, or nil where the answer has none -- so an answer that lacks a
/// field reads as absent rather than as a crash.
const mp::Value& field(const mp::Value& v, const char* key) {
    static const mp::Value absent;
    const mp::Value* f = v.get(key);
    return f != nullptr ? *f : absent;
}

/// A PICTURE'S FINGERPRINT: every row's text and role, the caret, the one range and the
/// generation -- what a maker saw, so a press or a drop aimed at it can be told from one aimed at
/// a screen that moved since (FNV-1a).
std::uint64_t picture_hash(const std::vector<surface::SurfaceTextRow>& rows, const ws::v2::PaneCaret& caret) {
    std::uint64_t h = 1469598103934665603ull;
    const auto mix = [&h](const void* p, std::size_t n) {
        const auto* b = static_cast<const unsigned char*>(p);
        for (std::size_t i = 0; i < n; ++i) {
            h = (h ^ b[i]) * 1099511628211ull;
        }
    };
    const auto num = [&mix](std::int64_t v) { mix(&v, sizeof v); };
    for (const surface::SurfaceTextRow& r : rows) {
        num(static_cast<std::int64_t>(r.text.size()));
        mix(r.text.data(), r.text.size());
        num(r.role);
        num(r.background);
    }
    num(caret.row);
    num(caret.column);
    num(caret.sel_begin_row);
    num(caret.sel_begin_col);
    num(caret.sel_end_row);
    num(caret.sel_end_col);
    num(caret.generation);
    return h;
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
                 PaneCatalogRequested, PaneRoom, PanePressed, ws::v3::PanePressed, PaneDragged, PaneKey,
                 PaneTextInput, PaneWheel, PaneButton, PaneActionRequested, PaneQuitRequested,
                 OpenSourceRequested, PrepareSourceRequested, ManagedOpenProgress, ManagedOpenSettled,
                 SourceOpened, loom::DispatchRefused, ProjectRoot, surface::ClipboardCopy,
                 surface::ClipboardText, EditorHandoffJudgeRequested, EditorWarmRequested,
                 EditorPreparationTick, EditorHandoffRequested, EditorHandoffEnded,
                 EditorAdoptRequested, EditorLiveRequested, EditorRetireRequested,
                 nve::NeovimStartRequested, nve::NeovimStopRequested, nve::NeovimStatusRequested,
                 ws::PaneValueDrop, ws::PaneMenuAnswered, ws::PaneOperationAnswered,
                 ws::PaneCarryAnswered>,
    loom::Emit<PaneOffered, ws::v2::PaneActions, ws::v3::PaneContent, ws::v2::PaneCaret,
               PaneQuitAnswered, SourceOpened, SourcePrepared, OpenSourceRequested,
               ProjectRootRequested, surface::ClipboardCopy, surface::ClipboardTextRequested,
               EditorHandoffJudged, EditorWarmed, EditorHandoffOffered, EditorAdopted, EditorLive,
               EditorRetired, timer::EnsureTimer, timer::CancelTimer, loom::Result, loom::Refused,
               ws::PaneMenuRequested, ws::PaneOperationRequested, ws::PaneValueCarryRequested>,
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

    // ---- What a transfer holds, between the gestures and the answers (WL-NVIM-10..12) --------

    enum class Take : std::uint8_t { Selection, Location };

    /// THE SELECTION AS NEOVIM HELD IT at one instant (`zengine_neovim.selection`): its text as
    /// Neovim's own yank takes it, and the identity a later step compares against.
    struct Snapshot {
        bool ok = false;
        std::string refusal;
        std::string text;
        std::string kind; ///< `characters`, `lines` or `block`
        std::string mode;
        std::int64_t buf = 0;
        std::int64_t tick = 0;
        std::int64_t first_line = 0;
        std::int64_t first_col = 0;
        std::int64_t last_line = 0;
        std::int64_t last_col = 0;
        std::string name;
        std::string buftype;
        std::string fileformat;
        bool modified = false;
        friend bool operator==(const Snapshot& a, const Snapshot& b) {
            return a.ok == b.ok && a.buf == b.buf && a.tick == b.tick && a.mode == b.mode &&
                   a.first_line == b.first_line && a.first_col == b.first_col &&
                   a.last_line == b.last_line && a.last_col == b.last_col;
        }
    };

    /// A PRESS ON THE HIGHLIGHT OR THE STATUS ROW, remembered until the hand moves to another cell.
    struct Grab {
        bool armed = false;
        bool started = false;
        Take what = Take::Selection;
        std::int64_t row = 0;
        std::int64_t column = 0;
        std::uint64_t gesture = 0;
        Snapshot snap;
    };

    struct Pickup {
        enum class Stage : std::uint8_t { Idle, Permission, Carry };
        Stage stage = Stage::Idle;
        std::uint64_t ask = 0;
        std::uint64_t gesture = 0;
        bool drag = false;
        loom::Bytes bytes;
        std::string label;
        std::string what;
        loom::Ticket ticket{};
    };

    /// A DROP AIMED AT A CELL: the buffer, its tick and the screen row the hand saw, so Neovim can
    /// refuse a drop whose target moved. A command in a C++ buffer waits here for the maker's choice.
    struct Aim {
        std::int64_t row = 0;
        std::int64_t column = 0;
        std::int64_t buf = 0;
        std::int64_t tick = 0;
        std::string row_text;
    };
    struct Dropped {
        bool pending = false;
        st::Material material;
        Aim aim;
        ws::pane_menu::Asked menu;
    };

    struct Locate {
        enum class Stage : std::uint8_t { Idle, Permission, Opening };
        Stage stage = Stage::Idle;
        std::uint64_t ask = 0;
        loom::Ticket ticket{};
        st::SourceLocation loc;
        std::optional<st::SourceLocationContext> ctx;
        bool same_path = false;
        std::int64_t tick = 0;
    };

    /// WHAT NEOVIM SAID OF A CHANGE (WL-NVIM-13): the module's answer, which may carry its own
    /// refusal (`why`); or no answer -- never sent (`why`), answered with an error (`why`), or
    /// Neovim ended before it answered (`ended`).
    struct Heard {
        std::optional<mp::Value> result;
        std::string why;
        bool ended = false;
    };
    using Settle = std::function<void(const Heard&)>;

    /// A CHANGE NEOVIM HOLDS, sent once with its whole target, which Neovim checks again when it
    /// runs it. The Editor keeps it until the answer (or Neovim's end) fills `answer` -- inside a
    /// pump, by the only callback that can -- and then says `settle`'s outcome, exactly once.
    struct Held {
        std::string what; ///< "a drop", "a location's cursor": what the status row says still waits
        std::shared_ptr<std::optional<nv::rpc::Response>> answer;
        Settle settle;
    };

    /// WHAT AN INSERTION SAYS ONCE NEOVIM HAS TAKEN IT, however late.
    struct Insertion {
        std::string amount;  ///< what went in: "2 lines", "the Terminal line for EnsureTimer v1"
        std::string after;   ///< what follows its place: a command's "nothing was sent", its holes
        std::string instead; ///< said instead of its place: generated C++'s own sentence
        bool alert = false;  ///< a missing field or a hole
    };

    static constexpr const char* kDropSubject = "drop";
    static constexpr const char* kInsertLine = "neovim.insert-line";
    static constexpr const char* kInsertCpp = "neovim.insert-cpp";
    static constexpr const char* kExtractRow = "neovim.extract";
    static constexpr const char* kLocationRow = "neovim.location";
    static constexpr const char* kNeovimMenuRow = "neovim.menu";

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
        } else if (asked.id == nve::kActionExtract) {
            // THE KEYBOARD ROUTE (WL-NVIM-10), declared only while a selection stands. The maker may
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
            // A ROW WITH NO DEFAULT KEY: reached only by a chord the maker bound (WL-NVIM-12).
            grab_ = Grab{};
            flush(mail);
            acquire_location(false, mail.correlation(), mail);
            resay_ = true;
        }
    }

    /// A PRESS ON NEOVIM'S SCREEN IS A MOUSE PRESS THERE; on the status row it is a focus statement
    /// and moves nothing. The release is said before the next input that is not the same drag.
    void on(const PanePressed& press, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || press.pane != nve::kEditorPane) {
            return;
        }
        press_at(press.row, press.column, -1, mail);
    }

    /// ...AND THE PRESS THAT NAMES ITS PICTURE: still Neovim's own press, and one more meaning once
    /// the hand moves -- a press ON the painted Visual highlight remembers the selection Neovim held
    /// (its text taken now, as its yank takes it), and a press on the status row remembers this
    /// file's location (WL-NVIM-10). A press aimed at an older picture remembers nothing.
    void on(const ws::v3::PanePressed& press, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || press.pane != nve::kEditorPane) {
            return;
        }
        press_at(press.row, press.column, press.picture, mail);
    }

    // WL-NVIM-10 -- agents/workshop/neovim-transfers.md
    void press_at(std::int64_t row, std::int64_t column, std::int64_t picture, loom::Mail& mail) {
        if (held_still()) {
            return;
        }
        release_drag();
        grab_ = Grab{};
        if (running()) {
            pump(mail); // what Neovim drew since the last picture, before the press is judged
        }
        const bool current = fresh(picture);
        if (!running() || row < kChromeRows) {
            if (row == 0 && current && host_->ready() && !doc_path().empty()) {
                grab_ = Grab{true, false, Take::Location, row, column, mail.correlation(), Snapshot{}};
            }
            return;
        }
        notice_.clear();
        const std::int64_t r = row - kChromeRows;
        const std::int64_t c = column < 0 ? 0 : column;
        if (current && on_highlight(r, c)) {
            Snapshot snap = snapshot_now();
            if (snap.ok) {
                grab_ = Grab{true, false, Take::Selection, row, column, mail.correlation(), std::move(snap)};
            }
        }
        drag_ = Drag{true, r, c};
        mouse("left", "press", drag_.row, drag_.col);
        flush(mail);
    }

    void on(const PaneDragged& drag, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || drag.pane != nve::kEditorPane) {
            return;
        }
        if (held_still()) {
            return;
        }
        // A REMEMBERED PRESS BECOMES A CARRY ON ITS FIRST MOTION TO ANOTHER CELL, never a sweep
        // (WL-NVIM-10): Neovim's own press ends as the click it was, the Visual selection is put
        // back (`gv`) when the buffer has not moved since, and the copy is the one taken at the press.
        if (grab_.armed) {
            if (!grab_.started && (drag.row != grab_.row || drag.column != grab_.column)) {
                grab_.started = true;
                if (grab_.what == Take::Location) {
                    acquire_location(true, grab_.gesture, mail);
                } else {
                    release_drag();
                    std::string why;
                    const std::optional<mp::Value> now = lua_now(nv::lua::kDocFacts, nv::rpc::params(), kAskMs, why);
                    const bool same = now.has_value() && field(*now, "buf").as_int(-1) == grab_.snap.buf &&
                                      field(*now, "tick").as_int(-1) == grab_.snap.tick;
                    if (!same) {
                        notice("nothing was carried -- the buffer changed after you pressed the highlight", true);
                    } else {
                        (void)host_->input("gv");
                        carry_snapshot(grab_.snap, true, grab_.gesture, mail);
                    }
                }
                flush(mail);
                resay_ = true;
            }
            return;
        }
        if (!drag_.down || !running()) {
            return;
        }
        drag_.row = std::clamp<std::int64_t>(drag.row - kChromeRows, 0, ui_rows(rows_) - 1);
        drag_.col = std::clamp<std::int64_t>(drag.column, 0, ui_cols(columns_) - 1);
        mouse("left", "drag", drag_.row, drag_.col);
        flush(mail);
    }

    /// THE SECOND BUTTON IS NEOVIM'S: a right press and its release cross as Neovim's own mouse
    /// events (`nvim_input_mouse`), consumed whole -- nothing is handed back and no host menu
    /// opens over Neovim's screen. A right DRAG does not cross (the seam carries no secondary
    /// motion), the middle button is left to Neovim's default of nothing, and a `lost` release
    /// is delivered as a release so Neovim's own state agrees with the hand. This is press and
    /// release, not Neovim's complete mouse.
    void on(const PaneButton& b, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || b.pane != nve::kEditorPane ||
            b.button != 3) {
            return;
        }
        if (held_still() || !running()) {
            return;
        }
        if (b.pressed) {
            grab_ = Grab{};
            if (b.row < kChromeRows) {
                // THE STATUS ROW NAMES THIS FILE: its menu carries the location (WL-NVIM-10).
                if (b.row == 0 && host_->ready() && !doc_path().empty()) {
                    menu_take_ = Take::Location;
                    menu_ = ws::pane_menu::Offer(nve::kEditorPane, "location")
                                .at(b.row, b.column)
                                .row(kLocationRow, "Carry this file's location")
                                .send(mail, nve::kEditorOffice);
                }
                return; // otherwise a focus statement that moves nothing
            }
            // ON THE PAINTED VISUAL HIGHLIGHT THE RIGHT PRESS IS NOT NEOVIM'S YET: a menu offers the
            // copy and Neovim's own menu, so neither a destructive click nor a replaced popup comes first.
            const std::int64_t r = b.row - kChromeRows;
            const std::int64_t c = b.column < 0 ? 0 : b.column;
            if (running()) {
                pump(mail);
            }
            if (fresh(b.picture) && on_highlight(r, c)) {
                Snapshot snap = snapshot_now();
                if (snap.ok) {
                    release_drag();
                    menu_take_ = Take::Selection;
                    menu_snap_ = std::move(snap);
                    menu_cell_ = Drag{true, r, c};
                    menu_ = ws::pane_menu::Offer(nve::kEditorPane, "selection")
                                .at(b.row, b.column)
                                .row(kExtractRow, "Extract selection to Inventory")
                                .row(kNeovimMenuRow, "Neovim's own menu")
                                .send(mail, nve::kEditorOffice);
                    return;
                }
            }
            release_drag();
            right_ = Drag{true, b.row - kChromeRows, b.column < 0 ? 0 : b.column};
            mouse("right", "press", right_.row, right_.col);
            flush(mail);
            return;
        }
        if (!right_.down) {
            return;
        }
        const std::int64_t row = std::clamp<std::int64_t>(b.row - kChromeRows, 0, ui_rows(rows_) - 1);
        const std::int64_t col = std::clamp<std::int64_t>(b.column, 0, ui_cols(columns_) - 1);
        right_ = Drag{};
        mouse("right", "release", row, col);
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

    // ---- Transfers: the selection carried out, material dropped in as data, a location opened ---
    //
    // The same Workshop carry the standard Editor answers, through Neovim's own owners: the copy is
    // what Neovim's yank takes, the insertion is `nvim_buf_set_text` in one undo block, and every
    // check of "still the same" is Neovim's buffer and changedtick, never the screen alone
    // (`neovim/lua.hpp`). Nothing here writes, builds, sends, or feeds a payload byte as a key.

    /// MATERIAL DROPPED ON NEOVIM'S SCREEN (WL-NVIM-11).
    void on(const ws::PaneValueDrop& drop, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || drop.pane != nve::kEditorPane) {
            return;
        }
        if (held_still()) {
            return;
        }
        grab_ = Grab{};
        receive(drop, mail); // every outcome says its own notice; the standing one is part of the picture
        flush(mail);
        resay_ = true;
    }

    void on(const ws::PaneMenuAnswered& answer, loom::Mail& mail) {
        if (drop_.pending && drop_.menu.pending() && answer.subject == kDropSubject) {
            const std::string chosen = drop_.menu.take(mail, answer);
            if (drop_.menu.pending()) {
                return;
            }
            Dropped d = std::move(drop_);
            drop_ = Dropped{};
            if (chosen.empty()) {
                notice("the dropped " + d.material.what + " was not inserted", false);
            } else if (chosen == kInsertLine) {
                insert_command(d.material, d.aim);
            } else if (chosen == kInsertCpp) {
                insert_cpp(d.material, d.aim);
            }
            flush(mail);
            resay_ = true;
            return;
        }
        const std::string chosen = menu_.take(mail, answer);
        if (chosen == kExtractRow) {
            const Snapshot now = snapshot_now();
            if (!now.ok || !(now == menu_snap_)) {
                notice(now.ok ? std::string("nothing was carried -- the selection changed after the menu opened")
                              : "nothing was carried -- " + now.refusal,
                       true);
            } else {
                carry_snapshot(now, false, mail.correlation(), mail);
            }
        } else if (chosen == kLocationRow) {
            acquire_location(false, mail.correlation(), mail);
        } else if (chosen == kNeovimMenuRow && running()) {
            // NEOVIM'S OWN POPUP, as the right press it would have been.
            mouse("right", "press", menu_cell_.row, menu_cell_.col);
            mouse("right", "release", menu_cell_.row, menu_cell_.col);
        }
        flush(mail);
        resay_ = true;
    }

    void on(const ws::PaneOperationAnswered& answer, loom::Mail& mail) {
        if (!mail.answers_ask()) {
            return;
        }
        if (pickup_.stage == Pickup::Stage::Permission && mail.correlation() == pickup_.ask) {
            if (!answer.allowed) {
                pickup_ = Pickup{};
                notice("nothing was carried -- " + answer.reason, true);
                resay_ = true;
                return;
            }
            pickup_.stage = Pickup::Stage::Carry;
            pickup_.ticket = mail.as_role(nve::kEditorOffice)
                                 .send_to_role(kWorkshopRole,
                                               ws::PaneValueCarryRequested{nve::kEditorPane, pickup_.label,
                                                                           pickup_.bytes, pickup_.drag},
                                               pickup_.gesture);
            if (!pickup_.ticket.valid()) {
                pickup_ = Pickup{};
                notice("nothing was carried -- the copy could not be handed to Workshop", true);
                resay_ = true;
            }
            return;
        }
        if (locate_.stage == Locate::Stage::Permission && mail.correlation() == locate_.ask) {
            if (!answer.allowed) {
                locate_ = Locate{};
                notice("nothing was opened -- " + answer.reason, true);
                resay_ = true;
                return;
            }
            locate_.stage = Locate::Stage::Opening;
            locate_.same_path = doc_path() == locate_.loc.path;
            locate_.tick = doc_.tick;
            locate_.ask = ++asked_;
            locate_.ticket = mail.as_role(nve::kEditorOffice)
                                 .send_to_role(ws::kOpeningRole, OpenSourceRequested{locate_.loc.path}, locate_.ask);
            if (!locate_.ticket.valid()) {
                const std::string path = locate_.loc.path;
                locate_ = Locate{};
                notice("nothing was opened -- the Editor could not ask the opening office for " + path, true);
                resay_ = true;
            }
        }
    }

    void on(const ws::PaneCarryAnswered& answer, loom::Mail& mail) {
        if (pickup_.stage != Pickup::Stage::Carry || !mail.answers_ask() || mail.correlation() != pickup_.gesture) {
            return;
        }
        const Pickup done = std::move(pickup_);
        pickup_ = Pickup{};
        if (!answer.carried) {
            notice("nothing was carried -- " + answer.reason, true);
        } else if (!done.drag) {
            notice("carrying a copy of " + done.what + " -- click a receiving pane, or Escape; Neovim's buffer is unchanged",
                   false);
        }
        resay_ = true;
    }

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
        if (locate_.stage == Locate::Stage::Opening && mail.correlation() == locate_.ask) {
            settle_location(said);
            resay_ = true;
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
        if (pickup_.stage != Pickup::Stage::Idle && pickup_.ticket.valid() && attempt.seq == pickup_.ticket.seq) {
            pickup_ = Pickup{};
            notice("nothing was carried -- Workshop could not be asked (" + refused.reason + ")", true);
            resay_ = true;
            return;
        }
        if (locate_.stage != Locate::Stage::Idle && locate_.ticket.valid() && attempt.seq == locate_.ticket.seq) {
            const std::string path = locate_.loc.path;
            locate_ = Locate{};
            notice("nothing was opened -- " + path + " could not be asked for (" + refused.reason + ")", true);
            resay_ = true;
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
        // A CHANGE NEOVIM HOLDS goes first (WL-NVIM-13): an open asked behind it would be held too,
        // and would change the buffer the change was aimed at before it runs.
        if (!settle_held()) {
            (void)mail.answer(not_prepared(asked.op, held_->what + " is still waiting for Neovim -- " + asked.path +
                                                         " was not opened; open it again once it has settled"));
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
                shown_tick_ = candidate_.doc.tick;
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

    /// ONE CHANGE ASKED OF THE MODULE, OWNED UNTIL NEOVIM ANSWERS IT (WL-NVIM-13). `settle` runs
    /// exactly once with what Neovim said: now, when it answered within the bound or the change
    /// never reached it; or later, from `pump`, when Neovim holds the request -- which the Editor
    /// then says (`waiting`, and why) and keeps as `held_`. Nothing is sent while another change is
    /// held: that one is answered first, and this one is refused.
    // WL-NVIM-13 -- agents/workshop/neovim-transfers.md
    void change(const char* chunk, mp::Value args, std::string what, const std::string& waiting, Settle settle) {
        if (!settle_held()) {
            settle(Heard{std::nullopt, held_->what + " is still waiting for Neovim; try again once it has settled", false});
            return;
        }
        if (!running()) {
            settle(Heard{std::nullopt, "Neovim is not running" + (failure_.empty() ? std::string() : ": " + failure_), false});
            return;
        }
        auto answer = std::make_shared<std::optional<nv::rpc::Response>>();
        std::optional<nv::rpc::Response> got;
        std::string why;
        switch (host_->ask("nvim_exec_lua", nv::rpc::params(mp::Value::str(chunk), std::move(args)), kAskMs, got, why,
                           [answer](const nv::rpc::Response& r) { *answer = r; })) {
        case nv::Host::Asked::Answered:
            settle(heard_of(*got));
            return;
        case nv::Host::Asked::Unsent:
            settle(Heard{std::nullopt, "Neovim could not be asked (" + why + ")", false});
            return;
        case nv::Host::Asked::Outstanding:
            held_ = Held{std::move(what), std::move(answer), std::move(settle)};
            notice(waiting + " -- " + why, false);
            return;
        }
    }

    /// WHAT AN ANSWER TO A CHANGE SAYS: the module's result or its error -- and when Neovim has
    /// ended since, only that it ended, because whatever the change did was in a buffer now gone.
    Heard heard_of(const nv::rpc::Response& r) const {
        if (!running()) {
            return Heard{std::nullopt, host_ != nullptr && !host_->failure().empty() ? host_->failure() : failure_, true};
        }
        if (!r.error.is_nil()) {
            return Heard{std::nullopt, "Neovim answered with an error (" + nv::rpc::error_text(r.error) + ")", false};
        }
        return Heard{r.result, std::string(), false};
    }

    /// A HELD CHANGE NEOVIM HAS ANSWERED SINCE IS SAID NOW, once (WL-NVIM-13). True when no change
    /// is held any longer.
    bool settle_held() {
        if (!held_.has_value()) {
            return true;
        }
        if (!held_->answer->has_value()) {
            return false;
        }
        Held done = std::move(*held_);
        held_.reset();
        done.settle(heard_of(**done.answer));
        resay_ = true;
        return true;
    }

    /// A CHANGE NEOVIM ENDED WITH, as the Editor says it: the buffer it went to is gone, and a
    /// change writes no file.
    static std::string ended_words(const std::string& what, const std::string& why) {
        return "Neovim ended while " + what + " was held (" + why +
               ") -- nothing was written, and the Editor holds no document; open a source to start Neovim again";
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
        if (locate_.stage != Locate::Stage::Idle) {
            j.refusal = "a dropped location is still being opened -- switch once it has settled";
            return j;
        }
        if (drop_.pending) {
            j.refusal = "a dropped command is still waiting for your choice -- choose or dismiss it, then switch";
            return j;
        }
        if (!settle_held()) {
            j.refusal = held_->what + " is still waiting for Neovim -- answer what Neovim is waiting for (Escape "
                                      "ends an unfinished command), then switch";
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

    // ---- Transfer helpers ---------------------------------------------------------------------

    static bool visual_mode(const std::string& mode) {
        if (mode.empty()) {
            return false;
        }
        const char k = mode[0];
        return k == 'v' || k == 'V' || k == '\x16' || k == 's' || k == 'S' || k == '\x13';
    }

    /// IS THIS CELL OF NEOVIM'S SCREEN ON THE PAINTED VISUAL HIGHLIGHT? The cells Neovim marks
    /// Visual, and the cursor's own cell while a selection stands (Neovim does not mark it).
    bool on_highlight(std::int64_t row, std::int64_t col) const {
        if (!running() || !host_->ready() || !visual_mode(host_->mode())) {
            return false;
        }
        const nv::Grid& g = host_->grid();
        if (row < 0 || row >= g.rows() || col < 0 || col >= g.columns()) {
            return false;
        }
        return (g.groups_at(row, col) & nv::ui_group::kVisual) != 0 ||
               (row == g.cursor_row() && col == g.cursor_column());
    }

    /// ONE SCREEN ROW, CELL BY CELL, as this pane's grid holds it -- what a drop tells Neovim it
    /// was aimed at, so Neovim can refuse one whose row moved.
    std::string grid_row(std::int64_t row) const {
        std::string out;
        const nv::Grid& g = host_->grid();
        for (std::int64_t c = 0; row >= 0 && row < g.rows() && c < g.columns(); ++c) {
            out += g.at(row, c).text;
        }
        return out;
    }

    /// THE SELECTION NEOVIM HOLDS NOW, as its own yank would take it -- or why there is none.
    Snapshot snapshot_now() {
        Snapshot s;
        std::string why;
        const std::optional<mp::Value> v = lua_now(nv::lua::kSelection, nv::rpc::params(), kAskMs, why);
        if (!v.has_value()) {
            s.refusal = "Neovim could not be asked for its selection (" + why + ")";
            return s;
        }
        if (const mp::Value* w = v->get("why"); w != nullptr) {
            s.refusal = w->as_str();
            return s;
        }
        const mp::Value::Array& lines = field(*v, "lines").as_array();
        for (std::size_t i = 0; i < lines.size(); ++i) {
            s.text += (i > 0 ? "\n" : "") + lines[i].as_str();
        }
        if (field(*v, "newline").as_bool(false)) {
            s.text += '\n';
        }
        const std::string kind = field(*v, "kind").as_str();
        s.kind = kind == "V" ? st::kLines : kind == "\x16" ? st::kBlock : st::kCharacters;
        s.mode = field(*v, "mode").as_str();
        s.buf = field(*v, "buf").as_int();
        s.tick = field(*v, "tick").as_int();
        s.first_line = field(*v, "first").at(0).as_int();
        s.first_col = field(*v, "first").at(1).as_int();
        s.last_line = field(*v, "after").at(0).as_int();
        s.last_col = field(*v, "after").at(1).as_int();
        s.name = field(*v, "name").as_str();
        s.buftype = field(*v, "buftype").as_str();
        s.fileformat = field(*v, "fileformat").as_str();
        s.modified = field(*v, "modified").as_bool(false);
        s.ok = true;
        return s;
    }

    std::string project_root() const {
        return project_known_ ? ws::persist::resolved_against(std::string(), project_dir_) : std::string();
    }

    static std::string file_name(const std::string& path) {
        const std::size_t slash = path.find_last_of('/');
        return slash == std::string::npos ? path : path.substr(slash + 1);
    }

    /// ASK WORKSHOP TO APPROVE A CARRY OF THIS PAIR FOR THE GESTURE THAT CAUSED IT; the carry
    /// follows the approval (`on(PaneOperationAnswered)`).
    void begin_pickup(const st::Pair& pair, bool drag, std::uint64_t gesture, std::string label, std::string what,
                      loom::Mail& mail) {
        if (!pair.ok) {
            notice("nothing was carried -- " + pair.refusal, true);
            return;
        }
        if (pickup_.stage != Pickup::Stage::Idle) {
            notice("nothing was carried -- a copy is already on its way to Workshop", true);
            return;
        }
        pickup_.stage = Pickup::Stage::Permission;
        pickup_.ask = ++asked_;
        pickup_.gesture = gesture;
        pickup_.drag = drag;
        pickup_.bytes.assign(pair.bytes.begin(), pair.bytes.end());
        pickup_.label = label.substr(0, 128);
        pickup_.what = std::move(what);
        pickup_.ticket = mail.as_role(nve::kEditorOffice)
                             .send_to_role(kWorkshopRole,
                                           ws::PaneOperationRequested{nve::kEditorPane, kWorkshopRole,
                                                                      ws::PaneValueCarryRequested::zen_name,
                                                                      ws::PaneValueCarryRequested::zen_version,
                                                                      static_cast<std::int64_t>(gesture)},
                                           pickup_.ask);
        if (!pickup_.ticket.valid()) {
            pickup_ = Pickup{};
            notice("nothing was carried -- Workshop could not be asked", true);
        }
    }

    /// A COPY OF THE SELECTION NEOVIM HELD (WL-NVIM-10): its text, with where it came from beside it.
    // WL-NVIM-10 -- agents/workshop/neovim-transfers.md
    void carry_snapshot(const Snapshot& snap, bool drag, std::uint64_t gesture, loom::Mail& mail) {
        st::SourceSelection s;
        s.editor = started_words();
        s.path = snap.buftype.empty() && !snap.name.empty() ? spelled(snap.name) : std::string();
        s.project_root = project_root();
        s.kind = snap.kind;
        s.first_line = snap.first_line;
        s.first_column = snap.first_col;
        s.end_line = snap.last_line;
        s.end_column = snap.last_col;
        s.line_ending = snap.fileformat == "dos" ? "CRLF" : "LF";
        s.unsaved = snap.modified;
        s.captured_at_epoch_s = st::clock_now();
        const st::Lines lines = st::neovim_lines(snap.text);
        const std::string name = s.path.empty() ? std::string("[No Name]") : file_name(s.path);
        begin_pickup(st::text_pair(snap.text, s), drag, gesture,
                     name + " " + std::to_string(snap.first_line) + ":" + std::to_string(snap.first_col) + " (" + snap.kind + ")",
                     (lines.ok ? st::amount_words(lines.lines) : std::string("the selection")) + " of " + name, mail);
    }

    /// THIS FILE'S LOCATION, AT NEOVIM'S CURSOR (WL-NVIM-12).
    void acquire_location(bool drag, std::uint64_t gesture, loom::Mail& mail) {
        std::string why;
        const std::optional<mp::Value> v = lua_now(nv::lua::kLocation, nv::rpc::params(), kAskMs, why);
        if (!v.has_value()) {
            notice("nothing was carried -- Neovim could not say where its cursor is (" + why + ")", true);
            return;
        }
        const std::string name = field(*v, "name").as_str();
        if (!field(*v, "buftype").as_str().empty() || name.empty()) {
            notice("nothing was carried -- Neovim's current buffer names no file", true);
            return;
        }
        st::SourceLocation loc{spelled(name), field(*v, "line").as_int(), field(*v, "col").as_int()};
        st::SourceLocationContext c;
        c.editor = started_words();
        c.project_root = project_root();
        c.relative = st::relative_to(loc.path, c.project_root);
        c.line_text = st::observe_line(field(*v, "text").as_str());
        c.unsaved = field(*v, "modified").as_bool(false);
        c.captured_at_epoch_s = st::clock_now();
        begin_pickup(st::location_pair(loc, c), drag, gesture,
                     file_name(loc.path) + ":" + std::to_string(loc.line) + " (location)",
                     "the location of " + file_name(loc.path) + " at line " + std::to_string(loc.line), mail);
    }

    void receive(const ws::PaneValueDrop& drop, loom::Mail& mail) {
        const std::string bytes(drop.data.begin(), drop.data.end());
        st::Material m = st::read_material(bytes);
        if (m.kind == st::MaterialKind::Location) {
            open_location(m, mail);
            return;
        }
        if (m.kind == st::MaterialKind::Unsupported) {
            notice("nothing was inserted -- " + m.refusal, true);
            return;
        }
        if (!running() || !host_->ready()) {
            notice("nothing was inserted -- Neovim is not running; open a source to start it", true);
            return;
        }
        if (candidate_.live || !pastes_.empty() || drop_.pending || !settle_held()) {
            notice(std::string("nothing was inserted -- ") +
                       (candidate_.live ? "Neovim is opening " + candidate_.path
                        : drop_.pending ? std::string("a dropped command is still waiting for your choice")
                        : held_.has_value() ? held_->what + " is still waiting for Neovim"
                                            : std::string("a paste is still arriving")) +
                       "; drop again once it has settled",
                   true);
            return;
        }
        pump(mail);
        if (!fresh(drop.picture)) {
            notice("nothing was inserted -- Neovim's screen moved under the drop; drop it again", true);
            return;
        }
        if (drop.row < kChromeRows) {
            notice("nothing was inserted -- drop onto Neovim's text, not the status row", true);
            return;
        }
        std::string why;
        const std::optional<mp::Value> lang =
            lua_now(nv::lua::kLanguage, nv::rpc::params(mp::Value::integer(400)), kAskMs, why);
        if (!lang.has_value()) {
            notice("nothing was inserted -- Neovim could not be asked about its buffer (" + why + ")", true);
            return;
        }
        Aim aim;
        aim.row = drop.row - kChromeRows;
        aim.column = drop.column < 0 ? 0 : drop.column;
        aim.buf = field(*lang, "buf").as_int();
        aim.tick = field(*lang, "tick").as_int();
        aim.row_text = grid_row(aim.row);
        if (m.kind == st::MaterialKind::Text) {
            insert_lines(m.text, aim, Insertion{});
            return;
        }
        // A COMMAND: its Terminal line, or -- in a C++ buffer, by a separate choice -- C++. Neovim's
        // own filetype is the language owner; an extension decides only where Neovim named none.
        const std::string ft = field(*lang, "filetype").as_str();
        const st::CppDocument cpp = ft == "cpp" ? st::CppDocument::Yes
                                    : !ft.empty() ? st::CppDocument::No
                                                  : st::cpp_document(field(*lang, "name").as_str());
        if (cpp == st::CppDocument::No) {
            insert_command(m, aim);
            return;
        }
        drop_.pending = true;
        drop_.material = std::move(m);
        drop_.aim = aim;
        drop_.menu = ws::pane_menu::Offer(nve::kEditorPane, kDropSubject)
                         .at(drop.row, drop.column)
                         .row(kInsertLine, "Insert its Terminal line")
                         .row(kInsertCpp, cpp == st::CppDocument::Yes ? "Generate C++ that builds it"
                                                                      : "Generate C++ (this .h is C++)")
                         .send(mail, nve::kEditorOffice);
        if (!drop_.menu.pending()) {
            drop_ = Dropped{};
            notice("nothing was inserted -- the choice for the dropped command could not be offered", true);
            return;
        }
        notice("choose how the dropped " + drop_.material.what + " goes in -- nothing is inserted until you do", false);
    }

    /// TEXT INTO NEOVIM AS DATA (WL-NVIM-11): one undo block where the hand aimed, or Neovim's own
    /// refusal with nothing changed -- now, or once Neovim answers a drop it held (WL-NVIM-13).
    // WL-NVIM-11, WL-NVIM-13 -- agents/workshop/neovim-transfers.md
    void insert_lines(const std::string& text, const Aim& aim, Insertion said, bool whole = false) {
        const st::Lines lines = st::neovim_lines(text);
        if (!lines.ok) {
            notice("nothing was inserted -- " + lines.refusal, true);
            return;
        }
        if (said.amount.empty()) {
            said.amount = st::amount_words(lines.lines);
        }
        mp::Value::Array arr;
        for (const std::string& l : lines.lines) {
            arr.push_back(mp::Value::str(l));
        }
        change(nv::lua::kDrop,
               nv::rpc::params(mp::Value::integer(aim.buf), mp::Value::integer(aim.tick), mp::Value::integer(aim.row),
                               mp::Value::integer(aim.column), mp::Value::array(std::move(arr)), mp::Value::str(aim.row_text),
                               mp::Value::boolean(whole)),
               "a drop", "the drop waits for Neovim to take it where you aimed, unless that spot changes first",
               [this, said = std::move(said)](const Heard& h) { settle_insertion(said, h); });
    }

    /// WHAT NEOVIM DID WITH A DROP, said once, whenever it answered.
    void settle_insertion(const Insertion& said, const Heard& h) {
        if (h.ended) {
            notice(ended_words("a drop", h.why), true);
            return;
        }
        if (!h.result.has_value()) {
            notice("nothing was inserted -- " + h.why, true);
            return;
        }
        if (const mp::Value* refused = h.result->get("why"); refused != nullptr) {
            notice("nothing was inserted -- " + refused->as_str(), true);
            return;
        }
        if (!said.instead.empty()) {
            notice(said.instead, said.alert);
            return;
        }
        const bool replaced = field(*h.result, "replaced").as_bool(false);
        notice(std::string(replaced ? "replaced the Visual selection with " : "inserted ") + said.amount + " at line " +
                   std::to_string(field(*h.result, "line").as_int()) + ", byte " +
                   std::to_string(field(*h.result, "col").as_int()) + " -- u takes it back; nothing was written" +
                   said.after,
               said.alert);
    }

    void insert_command(const st::Material& m, const Aim& aim) {
        const st::TerminalLine t = st::terminal_line(*m.command, m.address);
        if (!t.ok) {
            notice("nothing was inserted -- " + m.what + ": " + t.refusal, true);
            return;
        }
        Insertion said;
        said.amount = "the Terminal line for " + m.what;
        said.after = "; text only -- nothing was sent";
        if (!t.missing.empty()) {
            said.after += "; INCOMPLETE: ";
            for (std::size_t i = 0; i < t.missing.size(); ++i) {
                said.after += (i > 0 ? ", " : "") + t.missing[i];
            }
            said.after += t.missing.size() == 1 ? " is not set" : " are not set";
        }
        if (!t.address_supplied) {
            said.after += "; <address> marks a destination this value never named";
        }
        if (!m.address_note.empty()) {
            said.after += " (" + m.address_note + ")";
        }
        said.alert = !t.missing.empty();
        insert_lines(t.line, aim, std::move(said));
    }

    void insert_cpp(const st::Material& m, const Aim& aim) {
        std::string why;
        const std::optional<mp::Value> lang =
            lua_now(nv::lua::kLanguage, nv::rpc::params(mp::Value::integer(400)), kAskMs, why);
        std::vector<std::string> document;
        if (lang.has_value()) {
            for (const mp::Value& l : field(*lang, "lines").as_array()) {
                document.push_back(l.as_str());
            }
        }
        const st::GeneratedCpp g = st::cpp_value_function(*m.command, document);
        if (!g.ok) {
            notice("no C++ was generated -- " + g.refusal, true);
            return;
        }
        Insertion said;
        said.amount = "C++ for " + m.what;
        said.instead = "generated " + g.function + "() for " + m.what + "; ";
        if (g.missing_includes.empty()) {
            said.instead += "its includes are already here";
        } else {
            said.instead += "add #include";
            for (std::size_t i = 0; i < g.missing_includes.size(); ++i) {
                said.instead += (i > 0 ? " and " : " ") + g.missing_includes[i];
            }
        }
        if (!g.holes.empty()) {
            said.instead += "; INCOMPLETE until you fill " + std::to_string(g.holes.size()) + " required field" +
                            (g.holes.size() == 1 ? "" : "s");
        }
        said.instead += " -- u removes it; nothing was sent, written or built";
        said.alert = !g.holes.empty();
        // GENERATED CODE IS WHOLE LINES, before the line the drop landed on, joining no text.
        insert_lines(st::join_lf(g.lines), aim, std::move(said), true);
    }

    /// A DROPPED LOCATION (WL-NVIM-12): Workshop approves the gesture, then the managed open.
    // WL-NVIM-12 -- agents/workshop/neovim-transfers.md
    void open_location(const st::Material& m, loom::Mail& mail) {
        const st::SourceLocation& loc = m.location;
        if (loc.path.empty() || !std::filesystem::path(loc.path).is_absolute()) {
            notice("nothing was opened -- this location names no absolute path; edit its path in Info", true);
            return;
        }
        if (locate_.stage != Locate::Stage::Idle) {
            notice("nothing was opened -- a dropped location is still being opened", true);
            return;
        }
        if (!settle_held()) {
            notice("nothing was opened -- " + held_->what + " is still waiting for Neovim; drop the location again once it "
                   "has settled",
                   true);
            return;
        }
        // A LOCATION REOPENS A FILE; IT NEVER CREATES ONE. Neovim edits a path that is not there
        // as a new buffer, so a location whose file is gone is refused here, before anything is
        // asked -- with where it was saved, when that was another root.
        std::error_code ec;
        if (!std::filesystem::is_regular_file(std::filesystem::path(spelled(loc.path)), ec)) {
            const std::string root = m.location_context ? m.location_context->project_root : std::string();
            const std::string here = project_root();
            std::string why = "nothing was opened -- " + spelled(loc.path) +
                              " is not there, and a location reopens a file rather than creating one";
            if (!root.empty() && !here.empty() && root != here) {
                why += " (it was saved under " + root + ", and this run's project is " + here +
                       "; edit its path in Info to rebind it)";
            }
            notice(why, true);
            return;
        }
        locate_.stage = Locate::Stage::Permission;
        locate_.loc = loc;
        locate_.loc.path = spelled(loc.path);
        locate_.ctx = m.location_context;
        locate_.ask = ++asked_;
        locate_.ticket = mail.as_role(nve::kEditorOffice)
                             .send_to_role(kWorkshopRole,
                                           ws::PaneOperationRequested{nve::kEditorPane, ws::kOpeningRole,
                                                                      OpenSourceRequested::zen_name,
                                                                      OpenSourceRequested::zen_version,
                                                                      static_cast<std::int64_t>(mail.correlation())},
                                           locate_.ask);
        if (!locate_.ticket.valid()) {
            locate_ = Locate{};
            notice("nothing was opened -- Workshop could not be asked", true);
        }
    }

    void settle_location(const SourceOpened& said) {
        Locate l = std::move(locate_);
        locate_ = Locate{};
        const std::string root = l.ctx ? l.ctx->project_root : std::string();
        const std::string here = project_root();
        const bool elsewhere = !root.empty() && !here.empty() && root != here;
        if (!said.accepted) {
            std::string why = "could not open " + l.loc.path + ": " + said.refusal;
            if (elsewhere) {
                why += " -- it was saved under " + root + ", and this run's project is " + here +
                       "; a location never follows its name to another root (edit its path in Info to rebind it)";
            }
            notice(why, true);
            return;
        }
        if (doc_path() != l.loc.path) {
            notice("the location's file was shown, but Neovim holds another buffer now -- the cursor was not moved", false);
            return;
        }
        const std::string opened = "opened " + shown_path(l.loc.path) +
                                   (elsewhere ? " (saved under another project root, " + root + ")" : std::string());
        if (l.loc.line <= 0) {
            notice(opened, false);
            return;
        }
        // THE CURSOR IS A CHANGE LIKE A DROP (WL-NVIM-13): asked with its buffer, its changedtick and
        // the saved line -- whole or cut, as the observation says (`whole_line`) -- which Neovim
        // checks again when it runs it, so a placement it holds lands only on that line unchanged.
        const std::string line = std::to_string(l.loc.line);
        const bool text = l.ctx.has_value() && !l.ctx->line_text.empty();
        change(nv::lua::kLocate,
               nv::rpc::params(mp::Value::integer(doc_.buf), mp::Value::integer(l.same_path ? l.tick : shown_tick_),
                               mp::Value::integer(l.loc.line), mp::Value::integer(l.loc.column),
                               text ? mp::Value::str(l.ctx->line_text) : mp::Value::nil(),
                               mp::Value::boolean(text && st::whole_line(l.ctx->line_text))),
               "a location's cursor", opened + " -- the cursor waits for Neovim to put it on line " + line +
                                          ", unless the file changes first",
               [this, opened, line](const Heard& h) {
                   if (h.ended) {
                       notice(ended_words("a location's cursor", h.why), true);
                   } else if (!h.result.has_value()) {
                       notice(opened + " -- the cursor was not moved: " + h.why, false);
                   } else if (!field(*h.result, "placed").as_bool(false)) {
                       const mp::Value& why = field(*h.result, "why");
                       notice(opened + " -- " +
                                  (why.is_nil() ? std::string("the cursor was not moved")
                                                : why.as_str() + ", so the cursor was not moved"),
                              false);
                   } else {
                       notice(opened + " at line " + line, false);
                   }
               });
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
        (void)settle_held(); // a change Neovim held, answered since -- or ended with it (WL-NVIM-13)
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
        if (activation_.activated() && carries_selection() != declared_selection_) {
            declare(mail); // `ctrl+r` follows Neovim's mode (WL-NVIM-10)
        }
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
            .send_to_role(kWorkshopRole, PaneOffered{nve::kEditorPane, nve::kPaneName, nve::kPaneSummary, 20, 84});
        declare(mail);
    }

    /// Two rows naming Workshop's retired document rows as what they stand in for, so an older
    /// host that still declares them lets these keep their chords: save writes the buffer, and
    /// open is Neovim's own jump back. Every other key reaches Neovim, except `ctrl+r` while a
    /// Visual or Select selection stands, where it carries the selection (WL-NVIM-10); this
    /// file's location has no default key, every plain ctrl+letter being Neovim's or the
    /// desktop's (WL-NVIM-12).
    void declare(loom::Mail& mail) {
        ws::v2::PaneActions actions;
        actions.pane = nve::kEditorPane;
        actions.rows.push_back(ws::v2::PaneActionRow{nve::kActionWrite, "write (:w)", zengine::input::scan::kS,
                                                     zengine::input::mod::kCtrl, ws::kOwnableDocumentSave});
        actions.rows.push_back(ws::v2::PaneActionRow{nve::kActionJumpOlder, "jump back (<C-o>)",
                                                     zengine::input::scan::kO, zengine::input::mod::kCtrl,
                                                     ws::kOwnableDocumentOpen});
        actions.rows.push_back(ws::v2::PaneActionRow{nve::kActionLocation, "carry this file's location",
                                                     zengine::input::scan::kUnknown, zengine::input::mod::kNone,
                                                     ""});
        declared_selection_ = carries_selection();
        if (declared_selection_) {
            actions.rows.push_back(ws::v2::PaneActionRow{nve::kActionExtract, "carry the selection",
                                                         zengine::input::scan::kR, zengine::input::mod::kCtrl, ""});
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
        if (held_.has_value()) {
            head += held_->what + " waits for Neovim -- "; // held until Neovim answers (WL-NVIM-13)
        }
        return head + (path.empty() ? std::string("no file") : tail_of_path(path, columns - static_cast<std::int64_t>(head.size())));
    }

    /// THE PANE, SAID: the status row (or a standing notice in its place), then Neovim's screen,
    /// cropped to the room; the caret and the one range beside the rows, in the same lattice.
    /// NUMBERED (WL-NVIM-11): a picture whose rows, caret, range or generation differ from the
    /// last one said is a new picture, so a press or a drop names which screen it was aimed at.
    void say(loom::Mail& mail) {
        if (!granted_) {
            return;
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
        (void)mail.as_role(nve::kEditorOffice)
            .send_to_role(kWorkshopRole, ws::v3::PaneContent{nve::kEditorPane, std::move(rows), epoch_, picture_});
        (void)mail.as_role(nve::kEditorOffice).send_to_role(kWorkshopRole, caret);
    }

    /// IS THIS THE PICTURE THE PANE SHOWS, AND DOES NEOVIM'S SCREEN STILL READ AS IT? Neovim may
    /// have drawn since the last picture was said; anything it drew makes the picture stale.
    bool fresh(std::int64_t picture) const {
        if (!granted_ || picture <= 0 || picture != picture_) {
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
};

} // namespace

ZEN_EXPORT_WEAVE(NeovimEditorWeave)
