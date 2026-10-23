// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_NEOVIM_EDITOR_COMMON_HPP
#define ZENGINE_NEOVIM_EDITOR_COMMON_HPP

// What the Neovim-backed Editor's weave and the two files that complete it share: the bounds and
// names it works by, the arithmetic of its paths, rows, pictures and clipboard, and the records of
// its own conversations -- a candidate, a transfer's snapshot and aim, a change Neovim holds --
// beside what both Editors keep alike (`editor/office_seam.hpp`). Private to
// `zengine-neovim-editor`, whose pane.cpp reads it whole (`using namespace`).
// Workshop law: agents/workshop/neovim.md

#include "editor/neovim/document.hpp"
#include "editor/neovim/host.hpp"
#include "editor/office_seam.hpp"
#include "editor/source-transfer/material.hpp"
#include "workshop/editor_handoff_vocabulary.hpp"
#include "workshop/open_seam_vocabulary.hpp"
#include "workshop/pane_rows.hpp"
#include "workshop/pane_menu.hpp"
#include "workshop/pane_text.hpp"
#include "workshop/pane_vocabulary.hpp"
#include "workshop/persist.hpp"

#include "surface/vocabulary.hpp"

#include <zen/weave.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#if defined(_WIN32)
#include <process.h>
#else
#include <unistd.h>
#endif

namespace zengine::neovim_editor::detail {

namespace nv = zengine::neovim;
namespace mp = zengine::neovim::msgpack;
namespace st = zengine::source_transfer;
namespace surface = zengine::surface;
namespace ws = zengine::workshop;

using ws::EditorDocument;
using ws::EditorTransfer;
using ws::pane_text::drawable;
using ws::pane_text::fit;

namespace office = zengine::editor_office;

// What both Editors keep and say alike (`editor/office_seam.hpp`), named as this weave's own.
// `kCaretCols` is why Neovim is given one column less than the room: an Insert caret after the
// last cell still has a cell to be drawn in.
using office::file_name;
using office::Holding;
using office::kCaretCols;
using office::kDropSubject;
using office::kWorkshopRole;
using office::not_prepared;
using office::offer_refusal;
using office::Pickup;
using office::Relay;
using office::same_identity;
using office::tail_of_path;
using office::Take;

/// THE ROW ABOVE NEOVIM'S SCREEN: what the Editor says about itself -- the mode, whether the buffer
/// is saved, the file -- or a standing notice in its place. A layout keeping `status` off gives
/// that row to Neovim while nothing stands in it.
inline constexpr std::int64_t kChromeRows = 1;

/// The smallest screen a Neovim is asked for, whatever the room: Neovim's own floor is one row and
/// one column, and a screen smaller than this says nothing a weaver can use. A smaller room shows
/// the top-left of it.
inline constexpr std::int64_t kMinUiRows = 2;
inline constexpr std::int64_t kMinUiCols = 12;

/// The screen a Neovim starts with before any room was granted.
inline constexpr std::int64_t kDefaultRows = 24;
inline constexpr std::int64_t kDefaultCols = 80;

/// THE BEAT this weave asks of the Timer while it holds the office.
inline constexpr const char* kBeatId = "zengine.neovim-editor.beat";
inline constexpr std::int64_t kBeatMs = 10;

/// HOW LONG A QUESTION MAY TAKE: long enough for any Neovim that is not waiting, short enough that a
/// Neovim that is stuck costs a weaver a moment rather than a Workshop.
inline constexpr int kAskMs = 2000;
/// ...and an adoption, which carries a whole document (up to the transfer's bound) both ways.
inline constexpr int kAdoptMs = 10000;
/// ...and a start an answer waits for (an open with no Neovim running, a baseline start).
inline constexpr int kStartWaitMs = 10000;

/// THE ROWS A TRANSFER'S MENUS OFFER, told apart by name: a drop's, and a press's.
inline constexpr const char* kInsertLine = "neovim.insert-line";
inline constexpr const char* kInsertCpp = "neovim.insert-cpp";
inline constexpr const char* kExtractRow = "neovim.extract";
inline constexpr const char* kLocationRow = "neovim.location";
inline constexpr const char* kNeovimMenuRow = "neovim.menu";

/// A PATH AS THE STANDARD EDITOR SPELLS ONE: absolute, lexically normal, forward slashes -- so the
/// office's two implementations name one file one way and a same-path open is recognised.
inline std::string spelled(const std::string& name) {
    return ws::persist::resolved_against(std::string(), name);
}

/// ONE FIELD OF A MODULE ANSWER, or nil where the answer has none -- so an answer that lacks a
/// field reads as absent rather than as a crash.
inline const mp::Value& field(const mp::Value& v, const char* key) {
    static const mp::Value absent;
    const mp::Value* f = v.get(key);
    return f != nullptr ? *f : absent;
}

/// THE FINGERPRINT OF THE ROWS DRAWN: every row's text and role, the caret, the one range and the
/// generation -- what a weaver saw, so a press or a drop aimed at it can be told from one aimed at
/// a screen that moved since (FNV-1a).
inline std::uint64_t picture_hash(const std::vector<ws::TextRow>& rows, const ws::RowsCaret& caret,
                                  std::int64_t generation) {
    std::uint64_t h = 1469598103934665603ull;
    const auto mix = [&h](const void* p, std::size_t n) {
        const auto* b = static_cast<const unsigned char*>(p);
        for (std::size_t i = 0; i < n; ++i) {
            h = (h ^ b[i]) * 1099511628211ull;
        }
    };
    const auto num = [&mix](std::int64_t v) { mix(&v, sizeof v); };
    for (const ws::TextRow& r : rows) {
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
    num(generation);
    return h;
}

/// NEOVIM'S MODE, as the status row says it.
inline std::string mode_word(const std::string& mode) {
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
inline std::string join_lines(const std::vector<std::string>& lines, bool linewise) {
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

inline PasteLines paste_lines(const std::string& text) {
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
inline std::string preview_line(const std::string& line, std::int64_t columns) {
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

inline long long process_id() {
#if defined(_WIN32)
    return static_cast<long long>(_getpid());
#else
    return static_cast<long long>(::getpid());
#endif
}

/// AN ADDRESS A REMOTE INTERFACE CAN ATTACH TO, unique to this process and start.
inline std::string default_listen(std::int64_t n) {
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

inline bool visual_mode(const std::string& mode) {
    if (mode.empty()) {
        return false;
    }
    const char k = mode[0];
    return k == 'v' || k == 'V' || k == '\x16' || k == 's' || k == 'S' || k == '\x13';
}

/// A CHANGE NEOVIM ENDED WITH, as the Editor says it: the buffer it went to is gone, and a
/// change writes no file.
inline std::string ended_words(const std::string& what, const std::string& why) {
    return "Neovim ended while " + what + " was held (" + why +
           ") -- nothing was written, and the Editor holds no document; open a source to start Neovim again";
}

inline std::int64_t ui_rows(std::int64_t rows, std::int64_t chrome = kChromeRows) {
    return std::max(rows - chrome, kMinUiRows);
}
inline std::int64_t ui_cols(std::int64_t cols) { return std::max(cols - kCaretCols, kMinUiCols); }

inline std::string listed(const std::vector<std::string>& names) {
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
    /// The prepared file's first lines, shown as its preview until Neovim draws it.
    std::vector<std::string> lines;
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

/// A DROP AIMED AT A CELL: the buffer, its tick and the screen row the hand saw, so Neovim can
/// refuse a drop whose target moved. A command in a C++ buffer waits here for the weaver's choice.
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

/// A DROPPED LOCATION, judged against the changedtick of the buffer Neovim held when the open was
/// asked.
using Locate = office::LocateOf<std::int64_t>;

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

/// WHAT A SWITCH AWAY WOULD CARRY, LOSE, RESET, OR REFUSE -- asked of Neovim at this instant.
struct Judgement {
    std::string refusal;
    std::vector<std::string> losses;
    std::vector<std::string> resets;
    std::vector<std::string> notes;
    EditorTransfer transfer;
};

/// THE FILE A SHOWING PUT IN FRONT, BEFORE NEOVIM HAS DRAWN IT: its first lines, and how many
/// flushes this Neovim had made when it answered the showing -- its preview stands until the next.
struct Preview {
    bool active = false;
    std::vector<std::string> lines;
    std::uint64_t flushes = 0;
};

} // namespace zengine::neovim_editor::detail

#endif // ZENGINE_NEOVIM_EDITOR_COMMON_HPP
