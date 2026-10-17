// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_NEOVIM_EDITOR_OPENING_SWITCH_HPP
#define ZENGINE_NEOVIM_EDITOR_OPENING_SWITCH_HPP

// The Neovim-backed Editor's doors for opening and switching: members of `NeovimEditorWeave`, which
// pane.cpp declares and includes this after. The managed opening's preparation and publication and
// the old door's relay (WL-NVIM-01); both halves of an editor switch, as the incumbent (WL-NVIM-04)
// and as the sealed candidate (WL-NVIM-03, WL-NVIM-05); and Neovim started headless from a Loom
// with no Workshop (WL-NVIM-08).
// Workshop law: agents/workshop/neovim.md

#include "editor/neovim-editor/common.hpp"
#include "editor/neovim-editor/vocabulary.hpp"

#include "editor/neovim/document.hpp"
#include "editor/neovim/host.hpp"
#include "editor/neovim/launch.hpp"
#include "editor/neovim/lua.hpp"
#include "editor/office_seam.hpp"
#include "workshop/editor_handoff_vocabulary.hpp"
#include "workshop/editor_switch_vocabulary.hpp"
#include "workshop/open_seam_vocabulary.hpp"
#include "workshop/pane_canvas_rows.hpp"
#include "workshop/persist.hpp"

#include <zen/weave.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace {

// ---- THE TWO OPEN DOORS (WL-OPEN) -----------------------------------------------------------

/// THE OLD DOOR, RELAYED to the opening manager with the requester's right kept -- the standard
/// Editor's promise, kept the same way (`office::relay_open_source`).
inline void NeovimEditorWeave::on(const OpenSourceRequested& asked, loom::Mail& mail) {
    office::relay_open_source(asked, mail, nve::kEditorOffice, holding_, relays_, asked_);
}

/// The manager's answer: to a dropped location's own open, settled here; otherwise to a relay,
/// spent into its requester's kept right (`office::settle_relay`).
inline void NeovimEditorWeave::on(const SourceOpened& said, loom::Mail& mail) {
    if (mail.answers_ask() && locate_.stage == Locate::Stage::Opening && mail.correlation() == locate_.ask) {
        settle_location(said);
        resay_ = true;
        return;
    }
    office::settle_relay(said, mail, relays_);
}

/// The bus's word that an ask never arrived, matched by exact attempt: a relay's requester is told
/// in words, and a pickup's or a dropped location's failure is said (`office::settle_dispatch_refused`).
inline void NeovimEditorWeave::on(const loom::DispatchRefused& refused, loom::Mail& mail) {
    if (office::settle_dispatch_refused(refused, mail, relays_, pickup_, locate_, noticing())) {
        resay_ = true;
    }
}

inline void NeovimEditorWeave::on(const ManagedOpenProgress&, loom::Mail&) {}

/// THE MANAGED DOOR: PREPARE the file in Neovim, hidden, and OFFER its identity for the exact
/// operation. Nothing is shown and the current buffer stays current, readable and editable; the
/// candidate becomes what the pane shows only in `on_claim_published`. A Neovim holding the
/// current buffer modified does not refuse: Neovim keeps it, loaded and modified, beside the new
/// one -- unless `hidden` is off, which the showing then refuses in Neovim's words.
inline void NeovimEditorWeave::on(const PrepareSourceRequested& asked, loom::Mail& mail) {
    if (!mail.authored_from_role(ws::kOpeningRole)) {
        return;
    }
    (void)mail.answer(prepare(asked.op, asked.path, asked.rows, asked.columns, ws::PaneCanvasRoom{}, mail));
}

/// ...AS THIS PANE'S OWN PICTURE in the canvas room the trial reserved, for the rows and columns
/// of its lattice, numbered as the first picture in that room.
inline void NeovimEditorWeave::on(const ws::v2::PrepareSourceRequested& asked, loom::Mail& mail) {
    if (!mail.authored_from_role(ws::kOpeningRole)) {
        return;
    }
    const ws::CanvasRows lattice = ws::canvas_rows(asked.room);
    const SourcePrepared p = prepare(asked.op, asked.path, lattice.rows, lattice.columns, asked.room, mail);
    ws::v2::SourcePrepared prepared;
    prepared.op = asked.op;
    prepared.ok = p.ok;
    prepared.refusal = p.refusal;
    if (p.ok) {
        prepared.generation = p.generation;
        prepared.picture = ws::rows_picture(
            asked.room, 1, p.rows, status_part(p.rows, lattice.columns),
            ws::RowsCaret{p.caret_row, p.caret_col, p.sel_begin_row, p.sel_begin_col, p.sel_end_row,
                          p.sel_end_col});
    }
    (void)mail.answer(prepared);
}

/// THE CANDIDATE PREPARED FOR `rows` BY `columns`, or refused in words: its rows and caret, and
/// the generation offered for it.
inline SourcePrepared NeovimEditorWeave::prepare(std::int64_t op, const std::string& asked_path,
                                                 std::int64_t rows, std::int64_t columns,
                                                 const ws::PaneCanvasRoom& room, loom::Mail& mail) {
    if (holding_.active) {
        ++holding_.refused;
        return not_prepared(op, "the Editor is being switched -- " + asked_path +
                                    " was not prepared; open it again once the switch has settled");
    }
    // A CHANGE NEOVIM HOLDS goes first (WL-NVIM-13): an open asked behind it would be held too,
    // and would change the buffer the change was aimed at before it runs.
    if (!settle_held()) {
        return not_prepared(op, held_->what + " is still waiting for Neovim -- " + asked_path +
                                    " was not opened; open it again once it has settled");
    }
    drop_candidate();
    std::string path;
    if (!asked_path.empty() && !std::filesystem::path(asked_path).is_absolute() && !project_known_) {
        return not_prepared(op, "a relative path means nothing until this Editor is told where this "
                                "run began -- open " + asked_path + " by its full path");
    }
    path = ws::persist::resolved_against(project_dir_, asked_path);
    std::string why;
    if (!ensure_running(why)) {
        return not_prepared(op, why + " -- " + path + " was not opened");
    }
    Candidate next;
    next.live = true;
    next.op = static_cast<std::uint64_t>(op);
    next.path = path;
    next.rows = rows;
    next.columns = columns;
    next.room = room;
    EditorDocument offered;
    SourcePrepared prepared;
    prepared.op = op;
    if (!doc_path().empty() && doc_path() == path) {
        next.same_path = true;
        offered = identity_now();
        offered.opened_by = op;
        compose_current(prepared, rows, columns);
    } else {
        const std::optional<mp::Value> r = lua_now(nv::lua::kPrepare,
                                                   nv::rpc::params(mp::Value::str(path),
                                                                   mp::Value::integer(rows > 0 ? rows : 1)),
                                                   kAskMs, why);
        if (!r.has_value()) {
            return not_prepared(op, "Neovim could not prepare " + path + ": " + why);
        }
        if (const mp::Value* refused = r->get("why"); refused != nullptr) {
            return not_prepared(op, "Neovim refused " + path + ": " + refused->as_str());
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
        offered.opened_by = op;
        if (const mp::Value* got = r->get("lines"); got != nullptr) {
            for (const mp::Value& l : got->as_array()) {
                next.lines.push_back(l.as_str());
            }
        }
        compose_preview(prepared, offered.path, next.lines, rows, columns);
    }
    const loom::JointResult offer = mail.offer(static_cast<std::uint64_t>(op), offered);
    if (!offer.ok) {
        discard_prepared(next);
        return not_prepared(op, offer_refusal(path, offer.why));
    }
    prepared.ok = true;
    prepared.generation = offered.doc_epoch;
    candidate_ = std::move(next);
    return prepared;
}

inline void NeovimEditorWeave::on(const ManagedOpenSettled& said, loom::Mail& mail) {
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
inline bool NeovimEditorWeave::on_claim_published(const EditorDocument& published) {
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
            // ITS PREVIEW STANDS UNTIL NEOVIM HAS DRAWN IT: Neovim answered the showing, and its
            // first flush after that answer is the first screen of the buffer now in front.
            preview_ = Preview{true, candidate_.lines, host_->flushes_at_answer()};
        }
        epoch_ = published.doc_epoch;
        opened_by_ = candidate_.op;
        granted_ = true;
        // A CANDIDATE COMPOSED AS A PICTURE brings the canvas room it was drawn for, whose
        // pictures this pane numbers afresh; one composed as rows for a pane on a canvas leaves
        // its room as it is, and the desk grants it another. Either way nothing aimed at the
        // document it replaced counts.
        if (candidate_.room.grant > 0) {
            canvas_ = candidate_.room;
            pictures_ = ws::CanvasPictures{};
        } else {
            pictures_.retire();
        }
        pressed_ = 0;
        if (candidate_.room.grant > 0 || canvas_.grant <= 0) {
            rows_ = candidate_.rows;
            columns_ = candidate_.columns;
        }
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

/// A PREPARED BUFFER NOBODY SHOWED goes, when it holds nothing and was not there before.
inline void NeovimEditorWeave::discard_prepared(const Candidate& c) {
    if (!c.live || c.same_path || c.buf == 0 || !running()) {
        return;
    }
    std::string why;
    (void)lua_now(nv::lua::kDiscard, nv::rpc::params(mp::Value::integer(c.buf), mp::Value::boolean(c.existed)),
                  kAskMs, why);
}

inline void NeovimEditorWeave::drop_candidate() {
    if (candidate_.live) {
        discard_prepared(candidate_);
    }
    candidate_ = Candidate{};
}

// ---- A switch: this editor as the incumbent (WL-SWITCH-04) ---------------------------------

/// WHAT WOULD A SWITCH AWAY COST? Asked of Neovim now: the other modified buffers and running
/// terminal jobs are losses a weaver must agree to; the undo history, registers, marks, extra
/// windows and clean buffers are resets; a state no switch may begin in is refused in words.
// WL-NVIM-04 -- agents/workshop/neovim.md
inline void NeovimEditorWeave::on(const EditorHandoffJudgeRequested& asked, loom::Mail& mail) {
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
inline void NeovimEditorWeave::on(const EditorHandoffRequested& asked, loom::Mail& mail) {
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

inline void NeovimEditorWeave::on(const EditorHandoffEnded& ended, loom::Mail& mail) {
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

inline void NeovimEditorWeave::on(const EditorRetireRequested& asked, loom::Mail& mail) {
    if (!holding_.active || holding_.op != asked.op) {
        return;
    }
    (void)mail.answer(EditorRetired{asked.op, holding_.refused});
}

// ---- A switch: this editor as the candidate, sealed (WL-SWITCH-05) -----------------------

/// START NEOVIM in the room the incumbent had, and answer when it is ready -- later, from a
/// relayed tick, because a start is the one step that waits on something outside this process.
// WL-NVIM-05 -- agents/workshop/neovim.md
inline void NeovimEditorWeave::on(const EditorWarmRequested& asked, loom::Mail& mail) {
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

inline void NeovimEditorWeave::on(const EditorPreparationTick&, loom::Mail& mail) {
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
inline void NeovimEditorWeave::on(const EditorAdoptRequested& asked, loom::Mail& mail) {
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
inline void NeovimEditorWeave::on(const EditorLiveRequested& asked, loom::Mail& mail) {
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

inline void NeovimEditorWeave::settle_warm(loom::Mail& mail) {
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

// ---- What a switch is judged from, asked of Neovim now --------------------------------------

/// UNTIL NEOVIM IS NO LONGER WAITING FOR INPUT, at most `ms`: its fast mode, asked every 10 ms.
inline bool NeovimEditorWeave::wait_unblocked(int ms) {
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

inline std::optional<Exported> NeovimEditorWeave::export_now(std::string& why) {
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

inline Judgement NeovimEditorWeave::judge_now() {
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
        // so does a prompt). A PROMPT is the weaver's to answer: refused. AN UNFINISHED COMMAND is
        // a count or a register name nobody finished: cancelled the way a weaver would, with
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

// ---- From a baseline Loom: a second terminal's Neovim --------------------------------------

// WL-NVIM-08 -- agents/workshop/neovim.md
inline void NeovimEditorWeave::on(const nve::NeovimStartRequested& asked, loom::Mail& mail) {
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

inline void NeovimEditorWeave::on(const nve::NeovimStopRequested& asked, loom::Mail& mail) {
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

inline void NeovimEditorWeave::on(const nve::NeovimStatusRequested&, loom::Mail& mail) {
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

} // namespace

#endif // ZENGINE_NEOVIM_EDITOR_OPENING_SWITCH_HPP
