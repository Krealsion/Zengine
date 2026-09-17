// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_EDITOR_HANDOFF_VOCABULARY_HPP
#define ZENGINE_WORKSHOP_EDITOR_HANDOFF_VOCABULARY_HPP

// THE CONVERSATION AN EDITOR SWITCH HOLDS WITH TWO EDITORS (WL-SWITCH,
// agents/workshop/editor-switch.md).
//
// A switch replaces the weave holding `zengine.editor` with another authored choice for it, and
// carries the document across. Replacement is Loom's prepared replacement -- the successor is
// loaded SEALED, prepared in a private conversation, and admitted in one dispatch that is also its
// activation -- and the document is carried by an AUTHORED HANDOFF over it, the Loom's own
// pattern: an ordinary message at an exact FIFO position is the boundary, the incumbent authors
// its final value there and holds still, and that value is what the successor adopts before it is
// admitted.
//
//     coordinator -> incumbent   EditorHandoffJudgeRequested  -> EditorHandoffJudged
//                                   what a switch away would lose (consent), reset, and refuse;
//                                   nothing is held and nothing is loaded yet
//     coordinator -> candidate   EditorWarmRequested          -> EditorWarmed      (sealed)
//     coordinator -> candidate   EditorPreparationTick                              (sealed)
//                                   a sealed weave receives no Timer beat; the coordinator
//                                   relays one while the candidate starts
//     coordinator -> incumbent   EditorHandoffRequested       -> EditorHandoffOffered
//                                   THE BOUNDARY: the exact transfer, and the incumbent holds
//                                   still -- it applies no input until it is told the outcome
//     coordinator -> candidate   EditorAdoptRequested         -> EditorAdopted     (Loom's
//                                   preparation ask: the answer is what the transaction reads)
//     coordinator -> incumbent   EditorHandoffEnded                 (only when nothing moved:
//                                   the incumbent resumes, and says what it refused meanwhile)
//     (commit: the office moves and the successor is activated, in one dispatch)
//     coordinator -> successor   EditorLiveRequested          -> EditorLive
//     coordinator -> retired     EditorRetireRequested        -> EditorRetired     (sealed for
//                                   retirement, still the coordinator's to reach: what it
//                                   refused while it held still, before it is unloaded)
//
// ONE VOCABULARY FOR EVERY EDITOR. The standard Editor and the Neovim-backed one speak exactly
// these shapes, as incumbent and as candidate, so a switch is between any two authored choices
// and neither side knows what the other is.
//
// WHAT IS NOT HERE: no document shape of either implementation (the transfer is the standard
// model, which both can say), no timeout (a silent participant leaves the switch pending and
// inspectable), and no rollback after the commitment (a failure after it is reported as one).

#include <zen/weave/shape.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace zengine::workshop {

/// THE DOCUMENT, AS IT CROSSES A SWITCH -- in the standard Editor's model, which both editors
/// can say exactly: file bytes, a caret and an anchor as (line, byte), a selection [min, max)
/// with an exclusive end, and a viewport in lines and displayed columns.
///
/// `text` and `saved_text` are the exact bytes a save would write and the bytes the saved
/// comparison holds, each one Text so a four-megabyte document is two cells of Loom's decode
/// budget rather than a hundred thousand. An empty `path` is "no document open", and every other
/// field is then meaningless.
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

/// WHAT WOULD A SWITCH AWAY COST? Asked of the incumbent before anything is loaded. Nothing is
/// held and nothing changes.
struct EditorHandoffJudgeRequested {
    std::int64_t op = 0;
    ZEN_SHAPE(EditorHandoffJudgeRequested, 1, ZEN_FIELD(op));
};

/// ...ANSWERED. `losses` are what the transfer cannot carry and a maker must agree to lose (each
/// named); `resets` are what does not cross and is reported, never consented to; a refusal is a
/// state no switch may begin in. `digest` names exactly those losses -- what is lost, not the
/// document that crosses -- so a consent given for them is recognised at the boundary only while
/// the losses are still the ones agreed to, and typing into the document meanwhile moves nothing. `rows` and
/// `columns` are the room the incumbent's pane was granted, which the candidate starts in.
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

/// BECOME ABLE TO HOLD THE OFFICE -- start whatever the implementation needs, in the room the
/// incumbent had. Answered at once, or later: a candidate that must wait on something external
/// defers its answer and spends it from a later `EditorPreparationTick`.
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

/// A BEAT, RELAYED. A sealed candidate receives nothing but its coordinator's speech, so the
/// Timer's beat cannot reach it; the coordinator sends this on its own beat while a candidate is
/// warming or adopting. It asks nothing and is answered by nothing.
struct EditorPreparationTick {
    std::int64_t op = 0;
    ZEN_SHAPE(EditorPreparationTick, 1, ZEN_FIELD(op));
};

// ---- the boundary ----------------------------------------------------------------------------

/// THE BOUNDARY. The incumbent authors the exact transfer and HOLDS STILL: from this delivery
/// until it hears the outcome, it applies no input to the document and refuses what would change
/// it, counting what it refused.
struct EditorHandoffRequested {
    std::int64_t op = 0;
    ZEN_SHAPE(EditorHandoffRequested, 1, ZEN_FIELD(op));
};

/// ...ANSWERED with the exact document, the losses and their digest recomputed at this instant, and
/// what does not cross. A refusal means the incumbent did not hold still and nothing crossed.
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

/// THE HANDOFF ENDED WITHOUT A COMMITMENT: the incumbent is still the Editor. It stops holding
/// still, and says what it refused meanwhile.
struct EditorHandoffEnded {
    std::int64_t op = 0;
    std::string why;
    ZEN_SHAPE(EditorHandoffEnded, 1, ZEN_FIELD(op), ZEN_FIELD(why));
};

// ---- adoption: the transaction's preparation ask -------------------------------------------

/// ADOPT THIS DOCUMENT. Sent as Loom's preparation ask, so the candidate's answer is what the
/// transaction reads: ready means the candidate holds exactly this document and can be admitted.
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

/// ARE YOU SERVING? Asked of the successor once it holds the office. The answer is what decides
/// whether the retired incumbent is released or kept.
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

/// YOU ARE RETIRED. Asked of the incumbent after its successor proved it serves: the incumbent is
/// sealed for retirement to the coordinator, and says what it refused while it held still before
/// it is unloaded.
struct EditorRetireRequested {
    std::int64_t op = 0;
    ZEN_SHAPE(EditorRetireRequested, 1, ZEN_FIELD(op));
};

struct EditorRetired {
    std::int64_t op = 0;
    std::int64_t refused_inputs = 0;
    ZEN_SHAPE(EditorRetired, 1, ZEN_FIELD(op), ZEN_FIELD(refused_inputs));
};

/// THE CONSENT DIGEST: `c` and eight lowercase hex digits of FNV-1a over the parts, each ended by a
/// NUL. The leading letter keeps a typed consent Text in the Terminal's grammar.
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
