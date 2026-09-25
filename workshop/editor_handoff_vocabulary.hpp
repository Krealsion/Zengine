// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_EDITOR_HANDOFF_VOCABULARY_HPP
#define ZENGINE_WORKSHOP_EDITOR_HANDOFF_VOCABULARY_HPP

// The conversation an editor switch holds with two editors (WL-SWITCH,
// agents/workshop/editor-switch.md). The successor is loaded sealed, warmed and handed the
// document through Loom's prepared replacement; the incumbent authors the exact transfer at the
// boundary and holds still until told the outcome. Every Editor speaks these shapes as incumbent
// and as candidate. No timeout (a silent participant leaves the switch pending), no rollback.

#include <zen/weave/shape.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace zengine::workshop {

/// The document as it crosses a switch, in the standard Editor's model: file bytes, caret and
/// anchor as (line, byte), and a viewport. The byte fields are Text, so a large document is two
/// cells of Loom's decode budget. An empty `path` is no document, and the rest then means nothing.
struct EditorTransfer {
    std::string path;
    std::string text;
    std::string saved_text;
    std::int64_t convention = 0; ///< the standard Editor's line_ending: 0 LF, 1 CRLF
    bool modified = false;
    std::int64_t caret_row = 0;
    std::int64_t caret_byte = 0;
    std::int64_t anchor_row = 0;
    std::int64_t anchor_byte = 0;
    std::int64_t first_row = 0;
    std::int64_t first_col = 0;
    std::int64_t doc_epoch = 0; ///< the incumbent's document generation; a successor goes past it
    std::string project_dir;
    bool project_known = false;
    std::string source; ///< which editor authored this, in words ("Editor", "Neovim 0.11.6")
    ZEN_SHAPE(EditorTransfer, 1, ZEN_FIELD(path), ZEN_FIELD(text), ZEN_FIELD(saved_text),
              ZEN_FIELD(convention), ZEN_FIELD(modified), ZEN_FIELD(caret_row),
              ZEN_FIELD(caret_byte), ZEN_FIELD(anchor_row), ZEN_FIELD(anchor_byte),
              ZEN_FIELD(first_row), ZEN_FIELD(first_col), ZEN_FIELD(doc_epoch),
              ZEN_FIELD(project_dir), ZEN_FIELD(project_known), ZEN_FIELD(source));
};

// ---- judging, before anything is loaded ----------------------------------------------------

/// What would a switch away cost? Asked of the incumbent before anything is loaded; nothing moves.
struct EditorHandoffJudgeRequested {
    std::int64_t op = 0;
    ZEN_SHAPE(EditorHandoffJudgeRequested, 1, ZEN_FIELD(op));
};

/// `losses` are what the transfer cannot carry and a maker must agree to lose; `resets` are
/// reported, never consented to. `digest` names exactly those losses, so a consent is honoured at
/// the boundary only while they are still the ones agreed to. `rows`/`columns` are the room the
/// candidate starts in.
struct EditorHandoffJudged {
    std::int64_t op = 0;
    bool ok = false;
    std::string refusal;
    std::vector<std::string> losses;
    std::vector<std::string> resets;
    std::string digest;
    std::int64_t rows = 0;
    std::int64_t columns = 0;
    std::string project_dir;
    bool project_known = false;
    ZEN_SHAPE(EditorHandoffJudged, 1, ZEN_FIELD(op), ZEN_FIELD(ok), ZEN_FIELD(refusal),
              ZEN_FIELD(losses), ZEN_FIELD(resets), ZEN_FIELD(digest), ZEN_FIELD(rows),
              ZEN_FIELD(columns), ZEN_FIELD(project_dir), ZEN_FIELD(project_known));
};

// ---- the candidate, sealed -----------------------------------------------------------------

/// Become able to hold the office, in the room the incumbent had. A candidate that waits on
/// something external defers its answer and spends it from a later `EditorPreparationTick`.
struct EditorWarmRequested {
    std::int64_t op = 0;
    std::int64_t rows = 0;
    std::int64_t columns = 0;
    std::string project_dir;
    bool project_known = false;
    ZEN_SHAPE(EditorWarmRequested, 1, ZEN_FIELD(op), ZEN_FIELD(rows), ZEN_FIELD(columns),
              ZEN_FIELD(project_dir), ZEN_FIELD(project_known));
};

/// ...ANSWERED: ready to adopt, or why not, in the candidate's own words.
struct EditorWarmed {
    std::int64_t op = 0;
    bool ok = false;
    std::string refusal;
    std::string detail; ///< what the candidate is, when it is ready ("Neovim 0.11.6, clean")
    ZEN_SHAPE(EditorWarmed, 1, ZEN_FIELD(op), ZEN_FIELD(ok), ZEN_FIELD(refusal), ZEN_FIELD(detail));
};

/// A beat relayed by the coordinator: a sealed candidate hears only its coordinator, so the
/// Timer's beat cannot reach it. It asks nothing.
struct EditorPreparationTick {
    std::int64_t op = 0;
    ZEN_SHAPE(EditorPreparationTick, 1, ZEN_FIELD(op));
};

// ---- the boundary ----------------------------------------------------------------------------

/// The boundary: the incumbent authors the exact transfer and holds still -- applying no input to
/// the document, and counting what it refused -- until it hears the outcome.
struct EditorHandoffRequested {
    std::int64_t op = 0;
    ZEN_SHAPE(EditorHandoffRequested, 1, ZEN_FIELD(op));
};

/// The exact document, with the losses and digest recomputed at this instant. A refusal means the
/// incumbent did not hold still and nothing crossed.
struct EditorHandoffOffered {
    std::int64_t op = 0;
    bool ok = false;
    std::string refusal;
    std::vector<std::string> losses;
    std::string digest;
    EditorTransfer transfer;
    std::vector<std::string> resets;
    std::vector<std::string> notes; ///< what crossed approximately (a position adjusted, a mode reset)
    ZEN_SHAPE(EditorHandoffOffered, 1, ZEN_FIELD(op), ZEN_FIELD(ok), ZEN_FIELD(refusal),
              ZEN_FIELD(losses), ZEN_FIELD(digest), ZEN_FIELD(transfer), ZEN_FIELD(resets),
              ZEN_FIELD(notes));
};

/// The handoff ended without a commitment: the incumbent is still the Editor, stops holding still
/// and says what it refused meanwhile.
struct EditorHandoffEnded {
    std::int64_t op = 0;
    std::string why;
    ZEN_SHAPE(EditorHandoffEnded, 1, ZEN_FIELD(op), ZEN_FIELD(why));
};

// ---- adoption: the transaction's preparation ask -------------------------------------------

/// Adopt this document -- Loom's preparation ask, so `ready` is what the transaction reads.
struct EditorAdoptRequested {
    std::int64_t op = 0;
    EditorTransfer transfer;
    ZEN_SHAPE(EditorAdoptRequested, 1, ZEN_FIELD(op), ZEN_FIELD(transfer));
};

struct EditorAdopted {
    std::int64_t op = 0;
    bool ready = false;
    std::string refusal;
    std::vector<std::string> notes;
    ZEN_SHAPE(EditorAdopted, 1, ZEN_FIELD(op), ZEN_FIELD(ready), ZEN_FIELD(refusal),
              ZEN_FIELD(notes));
};

// ---- after the commitment ------------------------------------------------------------------

/// Are you serving? Asked of the successor; the answer decides whether the retired incumbent is
/// released or kept.
struct EditorLiveRequested {
    std::int64_t op = 0;
    ZEN_SHAPE(EditorLiveRequested, 1, ZEN_FIELD(op));
};

struct EditorLive {
    std::int64_t op = 0;
    bool ok = false;
    std::string detail;
    ZEN_SHAPE(EditorLive, 1, ZEN_FIELD(op), ZEN_FIELD(ok), ZEN_FIELD(detail));
};

/// You are retired: the incumbent, sealed for retirement, says what it refused while it held
/// still before it is unloaded.
struct EditorRetireRequested {
    std::int64_t op = 0;
    ZEN_SHAPE(EditorRetireRequested, 1, ZEN_FIELD(op));
};

struct EditorRetired {
    std::int64_t op = 0;
    std::int64_t refused_inputs = 0;
    ZEN_SHAPE(EditorRetired, 1, ZEN_FIELD(op), ZEN_FIELD(refused_inputs));
};

/// The consent digest: `c` and eight lowercase hex digits of FNV-1a over the parts, each ended by
/// a NUL. The leading letter keeps a typed consent Text in the Terminal's grammar.
inline std::string handoff_digest(const std::vector<std::string>& parts) {
    std::uint32_t h = 2166136261u;
    for (const std::string& part : parts) {
        for (const char c : part) {
            h ^= static_cast<unsigned char>(c);
            h *= 16777619u;
        }
        h ^= 0u;
        h *= 16777619u;
    }
    const char* hex = "0123456789abcdef";
    std::string out = "c";
    for (int shift = 28; shift >= 0; shift -= 4) {
        out.push_back(hex[(h >> static_cast<unsigned>(shift)) & 0xFu]);
    }
    return out;
}

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_EDITOR_HANDOFF_VOCABULARY_HPP
