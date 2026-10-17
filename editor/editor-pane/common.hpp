// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_EDITOR_PANE_COMMON_HPP
#define ZENGINE_EDITOR_PANE_COMMON_HPP

// What the Editor pane's weave and its two aspect headers share: the names they speak in, the
// constants the rows are budgeted by, the records of a candidate, a view, a composition, a hold
// and a transfer in flight, and the pure helpers over them. Private to `zengine-editor-pane`:
// only `pane.cpp` includes it, and everything here stands in the unnamed namespace that file's
// weave does, so each name keeps the internal linkage it has in that one translation unit.
// Workshop law: agents/workshop/editor.md

#include "editor/editor-pane/vocabulary.hpp"

#include "editor/editor-pane/editor.hpp"
#include "editor/source-transfer/material.hpp"
#include "workshop/editor_handoff_vocabulary.hpp"
#include "workshop/open_seam_vocabulary.hpp"
#include "workshop/pane_canvas_rows.hpp"
#include "workshop/pane_menu.hpp"
#include "workshop/pane_seam_vocabulary.hpp"
#include "workshop/pane_text.hpp"
#include "workshop/pane_vocabulary.hpp"
#include "workshop/persist.hpp"

#include "surface/vocabulary.hpp"

#include <zen/weave.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

namespace surface = zengine::surface;
namespace ws = zengine::workshop;
namespace pane = zengine::editor_pane;
namespace st = zengine::source_transfer;

using ws::EditorAdopted;
using ws::EditorAdoptRequested;
using ws::EditorBuffer;
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
using ws::EditorPos;
using ws::EditorState;
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
using ws::Written;
using zengine::workshop::pane_text::drawable;
using zengine::workshop::pane_text::fit;

/// The office Workshop holds, named as a string rather than through `workshop/panes.hpp`: a
/// provider is a stranger to Workshop's internals.
constexpr const char* kWorkshopRole = "zengine.workshop";

/// One column of every document row the text may not use: the caret's own, so a caret at the
/// end of a full row stays inside the room (WL-EDIT-08).
constexpr std::int64_t kCaretCols = 1;

/// The fewest rows in which a refusal gets a row of its own: status, refusal, and one document
/// row. In a smaller room the refusal replaces the status row, so the document keeps its rows
/// (WL-EDIT-12).
constexpr std::int64_t kNoticeNeedsRows = 3;

/// The answer to a quit ask while a paste is arriving: a refusal, not a wait, since the host
/// counts answers and a silent pane would hold every other pane's exit hostage.
constexpr const char* kPasteInFlight =
    "the Editor is still waiting for a clipboard answer -- quit again";

// ---- The documents, the view, the hold ------------------------------------------------------

/// One prepared candidate: the whole document a managed opening would install, built beside
/// the current one for one exact operation, and the identity offered for it. Nothing reads,
/// paints or edits it; it becomes the document only in `on_claim_published`. Nothing is held
/// for it: input applies to the current document, an edit moves the claim and the bus aborts
/// the operation, and a refused, superseded or aborted operation drops the candidate alone.
struct Candidate {
    bool live = false;
    std::uint64_t op = 0;
    bool same_path = false; ///< the current document is the one asked for: only the desk moves
    std::string path;
    EditorState doc;        ///< the whole candidate (bytes, saved copy, convention, epoch)
    /// The trial room the desk named and the geometry composed for it: the room the pane has
    /// the instant the publication seats it, before the desk's own grant, which agrees.
    std::int64_t rows = 0;
    std::int64_t columns = 0;
    std::int64_t chrome_rows = 0;
    std::int64_t doc_rows = 0;
    /// The canvas room the trial reserved, where the candidate was composed as this pane's
    /// picture; no grant where it was composed as rows.
    ws::PaneCanvasRoom room;
};

/// THE SWEEP A PRESS BEGAN, and the picture it began against (WL-EDIT-16).
struct Drag {
    bool armed = false;
    std::int64_t chrome_rows = 0;
    std::int64_t doc_rows = 0;
};

/// THE WINDOW THE DOCUMENT IS LOOKED AT THROUGH, as one value `reconcile` moves -- so a
/// composition can be made for a room WITHOUT moving the live view (a candidate's, or a
/// re-request's, which moves the pane and never the view: WL-EDIT-09).
struct Viewport {
    std::size_t first_row = 0;
    std::int64_t first_col = 0;
    bool follow_caret = false;
    std::int64_t last_rows = 0;
    std::int64_t last_cols = 0;
};

/// WHAT COMPOSING A DOCUMENT FOR A ROOM PRODUCED: the rows, where the document begins in
/// them, how many rows it got, the caret beside them, and the viewport it was made with.
struct Composition {
    std::vector<surface::SurfaceTextRow> rows;
    std::int64_t chrome_rows = 0;
    std::int64_t doc_rows = 0;
    bool status_row = false; ///< row 0 is the status row (not a notice standing in for it)
    ws::v2::PaneCaret caret;
    Viewport view;
};

/// WHAT A PREPARATION CAME TO: refused in words, or the generation offered and the
/// candidate's composition for the room asked.
struct Preparation {
    std::string refusal;
    std::int64_t generation = 0;
    Composition shown;
};

/// WHAT AN OPEN WOULD COME TO, JUDGED WITH NOTHING MOVED (WL-EDIT-05). The candidate it
/// carries is bytes and a name; it is not a document until it is installed.
struct Plan {
    Written outcome = Written::ok();
    bool same_path = false;
    std::string path;
    ws::SourceIn admitted;
};

/// An open relayed through the old door: the requester's kept answer right, and this weave's
/// own conversation with the manager (its correlation and exact attempt), so the answer or
/// the bus's refusal finds its requester. Bounded, and not reload state: the rights die with
/// the incarnation that kept them (ANS-02), and an answer to a predecessor's relay is refused.
struct Relay {
    std::string path;
    std::uint64_t correlation = 0;
    loom::Ticket attempt{};
    loom::DeferredAnswer answer;
};
constexpr std::size_t kMaxRelays = 4;

/// THE PASTE THIS DOCUMENT ASKED FOR, pinned to where it stood (`begin_paste`).
struct Paste {
    bool awaiting = false;
    std::uint64_t pending = 0;
    std::uint64_t doc = 0;      ///< the document epoch that asked (WL-EDIT-11)
    std::uint64_t revision = 0; ///< ...and exactly where it stood
};

/// A SWITCH HOLDING THIS EDITOR STILL at its boundary: which operation, and how many inputs
/// it refused meanwhile. Not in the state shape -- a reload is not a switch, and the hold
/// belongs to the incarnation the coordinator is talking to.
struct Holding {
    bool active = false;
    std::int64_t op = 0;
    std::int64_t refused = 0;
};

// ---- What a transfer holds, between the gestures and the answers (WL-EDIT-17..21) -------

/// WHAT A HAND IS TAKING: the selection's text, or this file's location.
enum class Take : std::uint8_t { Selection, Location };

/// THE DOCUMENT AT ONE INSTANT (`mark_now`): equal marks mean nothing moved in between.
struct Mark {
    std::uint64_t epoch = 0;
    std::uint64_t content = 0;
    std::uint64_t revision = 0;
    friend bool operator==(const Mark& a, const Mark& b) {
        return a.epoch == b.epoch && a.content == b.content && a.revision == b.revision;
    }
};

/// A PRESS ON THE HIGHLIGHT OR ON THE STATUS ROW, remembered until the hand moves to another
/// cell (then it is a carry) or the next act comes (then it was only ever a press).
struct Grab {
    bool armed = false;
    bool started = false;
    Take what = Take::Selection;
    std::int64_t row = 0;
    std::int64_t column = 0;
    std::uint64_t gesture = 0; ///< the press's own correlation, echoed on the approval
    Mark at;                   ///< the document when it was pressed
    EditorPos anchor;          ///< Selection: the selection the press landed on...
    EditorPos caret;
    Mark after;                ///< ...and the document once the press had placed the caret
};

/// ONE ACQUISITION IN FLIGHT: Workshop's approval for the gesture, then the carry itself.
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

/// WHERE A DROP LANDED: the character under it, and whether that is on the highlight.
struct Landing {
    EditorPos pos;
    bool inside = false;
};

/// A DROPPED COMMAND IN A C++ DOCUMENT, WAITING FOR THE WEAVER'S CHOICE.
struct Dropped {
    bool pending = false;
    st::Material material;
    Landing at;
    Mark mark;
    ws::pane_menu::Asked menu;
};

/// A DROPPED LOCATION: Workshop's approval for the gesture, then the managed open.
struct Locate {
    enum class Stage : std::uint8_t { Idle, Permission, Opening };
    Stage stage = Stage::Idle;
    std::uint64_t ask = 0;
    loom::Ticket ticket{};
    st::SourceLocation loc;
    std::optional<st::SourceLocationContext> ctx;
    bool same_path = false;
    Mark mark;
};

/// THE PICTURE'S ROW-TO-MEANING MAP, as the facts it is made of (WL-EDIT-18).
struct PictureKey {
    std::uint64_t epoch = 0;
    std::uint64_t content = 0;
    std::size_t first_row = 0;
    std::int64_t first_col = 0;
    std::int64_t chrome_rows = 0;
    std::int64_t rows = 0;
    std::int64_t columns = 0;
    bool selection = false;
    EditorPos from;
    EditorPos to;
    friend bool operator==(const PictureKey& a, const PictureKey& b) {
        return a.epoch == b.epoch && a.content == b.content && a.first_row == b.first_row &&
               a.first_col == b.first_col && a.chrome_rows == b.chrome_rows && a.rows == b.rows &&
               a.columns == b.columns && a.selection == b.selection && a.from == b.from && a.to == b.to;
    }
};

constexpr const char* kDropSubject = "drop";
constexpr const char* kInsertLine = "editor.insert-line";
constexpr const char* kInsertCpp = "editor.insert-cpp";

// ---- Pure helpers ---------------------------------------------------------------------------

/// A carried row, byte or offset as an index: a negative one is the first.
inline std::size_t as_index(std::int64_t n) {
    return n < 0 ? 0 : static_cast<std::size_t>(n);
}

/// A PATH THAT KEEPS ITS END. Whole when it fits; otherwise `...` and the last `width - 3`
/// bytes, so the file's own name survives before its directories do; a width too small
/// for even that keeps the last bytes alone.
inline std::string tail_of_path(const std::string& path, std::int64_t width) {
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

/// A history step's lines as one Text, a newline byte between them, and back: no line holds
/// one, so the two are exact inverses whatever the file's convention.
inline std::string joined_lines(const std::vector<std::string>& lines) {
    std::string out;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        if (i > 0) {
            out += '\n';
        }
        out += lines[i];
    }
    return out;
}
inline std::vector<std::string> split_lines(const std::string& text) {
    std::vector<std::string> out(1);
    for (const char c : text) {
        if (c == '\n') {
            out.emplace_back();
        } else {
            out.back() += c;
        }
    }
    return out;
}

inline Viewport viewport_of(const EditorState& doc) {
    return Viewport{doc.first_row, doc.first_col, doc.follow_caret, doc.last_rows,
                    doc.last_cols};
}

inline void apply_viewport(EditorState& doc, const Viewport& v) {
    doc.first_row = v.first_row;
    doc.first_col = v.first_col;
    doc.follow_caret = v.follow_caret;
    doc.last_rows = v.last_rows;
    doc.last_cols = v.last_cols;
}

/// THE PARTS A COMPOSITION NAMES: its first row `status` where the room has chrome, and each
/// document line by its number, `line:<n>`, across the room's columns.
inline std::vector<ws::PaneRowPart> parts_of(const Composition& c, std::int64_t columns) {
    std::vector<ws::PaneRowPart> parts;
    if (c.chrome_rows > 0) {
        parts.push_back(ws::PaneRowPart{"status", 0, 0, columns});
    }
    for (std::int64_t row = c.chrome_rows; row < static_cast<std::int64_t>(c.rows.size()); ++row) {
        const std::size_t line = c.view.first_row + static_cast<std::size_t>(row - c.chrome_rows);
        parts.push_back(ws::PaneRowPart{"line:" + std::to_string(line + 1), row, 0, columns});
    }
    return parts;
}

/// The caret and selection a composition says beside its rows, as its picture stands them in
/// them.
inline ws::RowsCaret rows_caret(const ws::v2::PaneCaret& c) {
    return ws::RowsCaret{c.row, c.column, c.sel_begin_row, c.sel_begin_col, c.sel_end_row,
                         c.sel_end_col};
}

inline bool same_identity(const EditorDocument& a, const EditorDocument& b) {
    return a.path == b.path && a.doc_epoch == b.doc_epoch && a.convention == b.convention &&
           a.content_revision == b.content_revision && a.dirty == b.dirty &&
           a.opened_by == b.opened_by;
}

/// THE WEAVER'S WORDS FOR AN OFFER THE BUS REFUSED: the document this weave claims moved
/// since the operation bound it (an edit, a paste), the operation was superseded, or a
/// participant was replaced. The diagnostic name stays in parentheses.
inline std::string offer_refusal(const std::string& path, loom::JointRefusal why) {
    switch (why) {
    case loom::JointRefusal::StaleRevision:
    case loom::JointRefusal::ParticipantChanged:
    case loom::JointRefusal::WrongState:
    case loom::JointRefusal::Cancelled:
        return "the document changed while opening " + path + " -- try again";
    default:
        return "the opening was no longer arranged (" + std::string(loom::name_of(why)) +
               ") -- " + path + " was not prepared";
    }
}

/// A REFUSED PREPARATION, as one value: the operation, no, and the sentence.
inline SourcePrepared not_prepared(std::int64_t op, std::string refusal) {
    SourcePrepared refused;
    refused.op = op;
    refused.ok = false;
    refused.refusal = std::move(refusal);
    return refused;
}

/// WHAT A SWITCH AWAY RESETS: the transfer carries neither, so a judgement and an offer say both.
inline std::vector<std::string> handoff_resets() {
    return {"the standard Editor's undo history", "the wheel's unspent fraction of a line"};
}

/// A PATH'S LAST PART, the name a carried copy goes by.
inline std::string file_name(const std::string& path) {
    const std::size_t slash = path.find_last_of('/');
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

} // namespace

#endif // ZENGINE_EDITOR_PANE_COMMON_HPP
