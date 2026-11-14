// SPDX-License-Identifier: AGPL-3.0-only
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
#include "editor/role_seam.hpp"
#include "workshop/editor_handoff_vocabulary.hpp"
#include "workshop/editor_switch_vocabulary.hpp"
#include "workshop/open_seam_vocabulary.hpp"
#include "pane/rows.hpp"
#include "persist/file.hpp"
#include "persist/written.hpp" // `Written`

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
/// Editor's promise, kept the same way (`role::relay_open_source`).
inline void NeovimEditorWeave::on(const OpenSourceRequested& asked, loom::Mail& mail) {
    role::relay_open_source(asked, mail, nve::kEditorRole, holding_, relays_, asked_);
}

/// The manager's answer: to a dropped location's own open, settled here; otherwise to a relay,
/// spent into its requester's kept right (`role::settle_relay`).
inline void NeovimEditorWeave::on(const SourceOpened& said, loom::Mail& mail) {
    if (mail.answers_ask() && locate_.stage == Locate::Stage::Opening && mail.correlation() == locate_.ask) {
        settle_location(said);
        resay_ = true;
        return;
    }
    role::settle_relay(said, mail, relays_);
}

/// Loom's word that an ask ended with no answer, matched by exact attempt: a relay's requester is
/// told in words, and a pickup's or a dropped location's failure is said (`role::settle_ask_ended`).
/// The project's root ends by the ask's correlation, and a paste Neovim waits on is answered from
/// the mirror, as an unsent one is.
inline void NeovimEditorWeave::on(const loom::AskEnded& ended, loom::Mail& mail) {
    if (mail.ask_ended() && root_pending_ != 0 && mail.correlation() == root_pending_) {
        root_pending_ = 0;
    }
    if (const auto it = pastes_.find(mail.correlation()); mail.ask_ended() && it != pastes_.end()) {
        const PasteLines paste = paste_lines(clip_);
        (void)host_->answer_paste(it->second, paste.lines, paste.regtype);
        pastes_.erase(it);
    }
    if (role::settle_ask_ended(ended, mail, relays_, pickup_, locate_, noticing())) {
        resay_ = true;
    }
}

inline void NeovimEditorWeave::on(const ManagedOpenProgress&, loom::Mail&) {}

/// THE MANAGED DOOR: PREPARE the file in Neovim, hidden, and OFFER its identity for the exact
/// operation, answered once Neovim has prepared it. Nothing is shown and the current buffer stays
/// current, readable and editable; the candidate becomes what the pane shows only in
/// `on_claim_published`. A Neovim holding the current buffer modified does not refuse: Neovim keeps
/// it, loaded and modified, beside the new one.
inline void NeovimEditorWeave::on(const ws::v3::PrepareSourceRequested& asked, loom::Mail& mail) {
    if (!mail.authored_from_role(ws::kOpeningRole)) {
        return;
    }
    const zengine::pane::RowsRoom room = role::EditorWindow::room_of(asked.room);
    const auto due = std::make_shared<loom::DeferredAnswer>(mail.defer_answer());
    prepare(asked.op, asked.path, room.rows(), room.columns(), mail,
            [due](ws::v3::SourcePrepared prepared, loom::Mail& later) {
                (void)loom::answer_deferred(*due, later, prepared);
            });
}

/// THE CANDIDATE PREPARED FOR `rows` BY `columns`, or refused in words: `then` hears the generation
/// offered for it, once Neovim has prepared it -- or why not, at once when it cannot be asked.
inline void NeovimEditorWeave::prepare(std::int64_t op, const std::string& asked_path, std::int64_t rows,
                                       std::int64_t columns, loom::Mail& mail, Prepared then) {
    if (holding_.active) {
        ++holding_.refused;
        then(not_prepared(op, "the Editor is being switched -- " + asked_path +
                                  " was not prepared; open it again once the switch has settled"),
             mail);
        return;
    }
    // A CHANGE NEOVIM HOLDS goes first (WL-NVIM-13): an open asked behind it would be held too,
    // and would change the buffer the change was aimed at before it runs.
    if (!settle_held()) {
        then(not_prepared(op, held_->what + " is still waiting for Neovim -- " + asked_path +
                                  " was not opened; open it again once it has settled"),
             mail);
        return;
    }
    if (preparing_) {
        then(not_prepared(op, "Neovim is still preparing another file -- " + asked_path +
                                  " was not opened; open it again once it has settled"),
             mail);
        return;
    }
    drop_candidate();
    if (!asked_path.empty() && !std::filesystem::path(asked_path).is_absolute() && !project_known_) {
        then(not_prepared(op, "a relative path means nothing until this Editor is told where this "
                              "run began -- open " + asked_path + " by its full path"),
             mail);
        return;
    }
    Candidate next;
    next.live = true;
    next.op = static_cast<std::uint64_t>(op);
    next.path = zengine::persist::resolved_against(project_dir_, asked_path);
    next.rows = rows;
    next.columns = columns;
    preparing_ = true;
    ensure_running(mail, [this, next, then](bool ok, const std::string& why, loom::Mail& later) {
        if (!ok) {
            preparing_ = false;
            then(not_prepared(static_cast<std::int64_t>(next.op), why + " -- " + next.path + " was not opened"),
                 later);
            return;
        }
        prepare_in(next, later, then);
    });
}

/// THE FILE PREPARED IN A RUNNING NEOVIM: the buffer it holds already, or a hidden one Neovim loads.
inline void NeovimEditorWeave::prepare_in(const Candidate& asked, loom::Mail& mail, Prepared then) {
    const std::int64_t op = static_cast<std::int64_t>(asked.op);
    if (!doc_path().empty() && doc_path() == asked.path) {
        Candidate next = asked;
        next.same_path = true;
        EditorDocument offered = identity_now();
        offered.opened_by = op;
        offer_prepared(std::move(next), offered, mail, then);
        return;
    }
    ask(nv::lua::kPrepare,
        nv::rpc::params(mp::Value::str(asked.path), mp::Value::integer(asked.rows > 0 ? asked.rows : 1)),
        kAskMs, mail, [this, asked, op, then](const Answer& a, loom::Mail& later) {
            if (!a.result.has_value()) {
                preparing_ = false;
                then(not_prepared(op, "Neovim could not prepare " + asked.path + ": " + a.why), later);
                return;
            }
            const mp::Value& r = *a.result;
            if (const mp::Value* refused = r.get("why"); refused != nullptr) {
                preparing_ = false;
                then(not_prepared(op, "Neovim refused " + asked.path + ": " + refused->as_str()), later);
                return;
            }
            Candidate next = asked;
            next.buf = r.get("buf") != nullptr ? r.get("buf")->as_int() : 0;
            next.existed = r.get("existed") != nullptr && r.get("existed")->as_bool(false);
            next.doc.known = true;
            next.doc.buf = next.buf;
            next.doc.name = r.get("name") != nullptr ? r.get("name")->as_str() : asked.path;
            next.doc.modified = r.get("modified") != nullptr && r.get("modified")->as_bool(false);
            next.doc.tick = r.get("tick") != nullptr ? r.get("tick")->as_int() : 0;
            next.convention = r.get("fileformat") != nullptr && r.get("fileformat")->as_str() == "dos" ? 1 : 0;
            if (const mp::Value* got = r.get("lines"); got != nullptr) {
                for (const mp::Value& l : got->as_array()) {
                    next.lines.push_back(l.as_str());
                }
            }
            EditorDocument offered;
            offered.path = spelled(next.doc.name);
            offered.doc_epoch = epoch_ + 1;
            offered.convention = next.convention;
            offered.content_revision = next.doc.tick;
            offered.dirty = next.doc.modified;
            offered.opened_by = op;
            offer_prepared(std::move(next), offered, later, then);
        });
}

/// THE PREPARED IDENTITY OFFERED for the exact operation, and the candidate kept until it is
/// published or dropped.
inline void NeovimEditorWeave::offer_prepared(Candidate next, EditorDocument offered, loom::Mail& mail,
                                              Prepared then) {
    preparing_ = false;
    const std::int64_t op = static_cast<std::int64_t>(next.op);
    const loom::JointResult offer = mail.offer(next.op, offered);
    if (!offer.ok) {
        discard_prepared(next);
        then(not_prepared(op, offer_refusal(next.path, offer.why)), mail);
        return;
    }
    ws::v3::SourcePrepared prepared;
    prepared.op = op;
    prepared.ok = true;
    prepared.generation = offered.doc_epoch;
    candidate_ = std::move(next);
    then(prepared, mail);
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

/// THE PUBLICATION HOOK: the bus published this weave's document claim by a joint operation. The
/// prepared buffer becomes the current one HERE: the showing is sent to Neovim, ahead of every key
/// that follows, and the pane shows the candidate's preview from now. Neovim's refusal of the
/// showing (its `hidden` is off and the current buffer is modified; the buffer went away), when it
/// answers, is said, and the Editor claims again what Neovim shows.
inline bool NeovimEditorWeave::on_claim_published(const EditorDocument& published) {
    claimed_ = published;
    claimed_ever_ = true;
    if (candidate_.live && published.opened_by == static_cast<std::int64_t>(candidate_.op)) {
        if (!candidate_.same_path) {
            const Candidate shown = candidate_;
            doc_ = candidate_.doc;
            convention_ = candidate_.convention;
            shown_tick_ = candidate_.doc.tick;
            // ITS PREVIEW STANDS UNTIL NEOVIM HAS DRAWN IT: until Neovim answers the showing, and
            // then until its first flush after that answer.
            preview_ = Preview{true, candidate_.lines, 0, true};
            ask_quietly(nv::lua::kShow, nv::rpc::params(mp::Value::integer(shown.buf)), kAskMs,
                        [this, shown](const Answer& a, loom::Mail&) { settle_show(shown, a); });
        }
        epoch_ = published.doc_epoch;
        opened_by_ = candidate_.op;
        // THE CANDIDATE IS COMPOSED FOR THE ROOM THE TRIAL SAID, and drawn at the end of this
        // delivery; nothing aimed at the document it replaced counts.
        pressed_ = 0;
        rows_ = candidate_.rows;
        columns_ = candidate_.columns;
        if (running()) {
            (void)host_->resize(screen_rows(), ui_cols(columns_));
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

/// WHAT NEOVIM DID WITH A SHOWING: shown, and its preview stands until Neovim's next flush; or
/// refused, said, and the document claimed again as Neovim holds it, under the next generation.
inline void NeovimEditorWeave::settle_show(const Candidate& shown, const Answer& a) {
    const struct After {
        NeovimEditorWeave* self;
        ~After() {
            if (self->after_show_) {
                const std::function<void()> next = std::move(self->after_show_);
                self->after_show_ = nullptr;
                next();
            }
        }
    } after{this};
    const mp::Value* refused = a.result.has_value() ? a.result->get("why") : nullptr;
    if (a.result.has_value() && refused == nullptr) {
        if (preview_.active && preview_.awaiting) {
            preview_.awaiting = false;
            preview_.flushes = a.flushes;
        }
        if (running() && host_->doc().known && host_->doc().buf == shown.buf) {
            doc_ = host_->doc();
        }
        resay_ = true;
        return;
    }
    const std::string words = refused != nullptr ? refused->as_str() : a.why;
    preview_ = Preview{};
    doc_ = running() ? host_->doc() : nv::DocFacts{};
    ++epoch_;
    notice("Neovim did not show " + shown.path + ": " + words, true);
    mirror_state();
    resay_ = true;
}

/// A PREPARED BUFFER NOBODY SHOWED goes, when it holds nothing and was not there before: asked of
/// Neovim, and its answer heard by nobody.
inline void NeovimEditorWeave::discard_prepared(const Candidate& c) {
    if (!c.live || c.same_path || c.buf == 0 || !running()) {
        return;
    }
    (void)host_->call("nvim_exec_lua",
                      nv::rpc::params(mp::Value::str(nv::lua::kDiscard),
                                      nv::rpc::params(mp::Value::integer(c.buf), mp::Value::boolean(c.existed))),
                      nv::Host::Callback{});
}

inline void NeovimEditorWeave::drop_candidate() {
    if (candidate_.live) {
        discard_prepared(candidate_);
    }
    candidate_ = Candidate{};
}

// ---- A switch: this editor as the incumbent (WL-SWITCH-04) ---------------------------------

/// WHAT WOULD A SWITCH AWAY COST? Asked of Neovim, and answered when it answers: the other
/// modified buffers and running terminal jobs are losses a weaver must agree to; the undo history,
/// registers, marks, extra windows and clean buffers are resets; a state no switch may begin in is
/// refused in words.
// WL-NVIM-04 -- agents/workshop/neovim.md
inline void NeovimEditorWeave::on(const EditorHandoffJudgeRequested& asked, loom::Mail& mail) {
    if (!mail.authored_from_role(ws::kEditorSwitchRole)) {
        return;
    }
    const auto due = std::make_shared<loom::DeferredAnswer>(mail.defer_answer());
    const std::int64_t op = asked.op;
    judge(mail, [this, due, op](Judgement j, loom::Mail& later) {
        EditorHandoffJudged judged;
        judged.op = op;
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
        (void)loom::answer_deferred(*due, later, judged);
    });
}

/// THE BOUNDARY: this editor holds still from the ask -- no input reaches Neovim until the outcome
/// is told -- and Neovim's exact document is asked and answered when Neovim answers.
// WL-NVIM-04 -- agents/workshop/neovim.md
inline void NeovimEditorWeave::on(const EditorHandoffRequested& asked, loom::Mail& mail) {
    if (!mail.authored_from_role(ws::kEditorSwitchRole)) {
        return;
    }
    release_drag();
    const std::int64_t op = asked.op;
    const auto due = std::make_shared<loom::DeferredAnswer>(mail.defer_answer());
    holding_ = Holding{true, op, 0};
    judge(mail, [this, due, op](Judgement j, loom::Mail& later) {
        EditorHandoffOffered offered;
        offered.op = op;
        if (!j.refusal.empty()) {
            if (holding_.active && holding_.op == op) {
                holding_ = Holding{};
            }
            offered.refusal = j.refusal;
            (void)loom::answer_deferred(*due, later, offered);
            return;
        }
        offered.ok = true;
        offered.losses = j.losses;
        offered.digest = ws::handoff_digest(j.losses);
        offered.transfer = std::move(j.transfer);
        offered.resets = j.resets;
        offered.notes = j.notes;
        (void)loom::answer_deferred(*due, later, offered);
    });
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
    if (!running() && !start_embedded(ui_rows(rows_ > 0 ? rows_ : kDefaultRows, chrome_rows()),
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

/// ADOPT THE DOCUMENT, OR SAY EXACTLY WHY NOT, answered when Neovim has: the bytes into a buffer
/// loaded from the same file (no undo step, the line format and final newline set, the modified
/// flag carried), the caret and selection placed by keys and READ BACK, and the viewport restored.
/// What did not land exactly is a note, never a silence.
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
        adopted.refusal = not_running_words();
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
    const auto due = std::make_shared<loom::DeferredAnswer>(mail.defer_answer());
    const std::int64_t op = asked.op;
    const EditorTransfer kept = t;
    ask(nv::lua::kAdopt,
        nv::rpc::params(mp::Value::str(t.path), mp::Value::array(std::move(lines)), mp::Value::boolean(dos),
                        mp::Value::boolean(text.final_newline), mp::Value::boolean(t.modified)),
        kAdoptMs, mail, [this, op, kept, dos, due](const Answer& a, loom::Mail& later) {
            EditorAdopted refused;
            refused.op = op;
            if (!a.result.has_value()) {
                refused.refusal = "Neovim could not adopt " + kept.path + ": " + a.why;
                (void)loom::answer_deferred(*due, later, refused);
                return;
            }
            if (const mp::Value* why = a.result->get("why"); why != nullptr) {
                refused.refusal = "Neovim would not adopt " + kept.path + ": " + why->as_str();
                (void)loom::answer_deferred(*due, later, refused);
                return;
            }
            adopt_placed(op, kept, dos, due, later);
        });
}

/// THE ADOPTED BUFFER'S CARET AND SELECTION placed by keys, then read back, and the viewport asked
/// back to where it stood; answered once Neovim says where its caret is.
inline void NeovimEditorWeave::adopt_placed(std::int64_t op, EditorTransfer t, bool dos,
                                            std::shared_ptr<loom::DeferredAnswer> due, loom::Mail& mail) {
    const nv::NeovimText text = nv::neovim_text(t.text);
    const nv::Pos anchor{t.anchor_row, t.anchor_byte};
    const nv::Pos caret{t.caret_row, t.caret_byte};
    const nv::Placement placed = nv::place(text.lines, text.final_newline, anchor, caret);
    (void)host_->input(nv::placement_keys(placed));
    export_later(kAdoptMs, mail,
                 [this, op, t, dos, due, placed, anchor, caret](std::optional<Exported> back, const Answer& a,
                                                               loom::Mail& later) {
                     EditorAdopted adopted;
                     adopted.op = op;
                     if (placed.adjusted) {
                         adopted.notes.push_back(placed.note);
                     }
                     // THE CARET, READ BACK: what Neovim holds now is the fact, and a place that
                     // differs from the one the transfer named -- beyond the adjustment `place`
                     // already said -- is said too.
                     if (!back.has_value()) {
                         adopted.refusal =
                             "Neovim adopted " + t.path + " and could not say where its caret is: " + a.why;
                         (void)loom::answer_deferred(*due, later, adopted);
                         return;
                     }
                     if (!placed.adjusted) {
                         const nv::Carried carried = nv::carry(back->lines, back->final_newline, back->at);
                         if (!(carried.caret == caret) || !(carried.anchor == anchor)) {
                             adopted.notes.push_back(
                                 "the caret landed at line " + std::to_string(carried.caret.row + 1) + ", byte " +
                                 std::to_string(carried.caret.byte + 1) + " rather than line " +
                                 std::to_string(caret.row + 1) + ", byte " + std::to_string(caret.byte + 1));
                         }
                     }
                     if (running()) {
                         (void)host_->call("nvim_exec_lua",
                                           nv::rpc::params(mp::Value::str(nv::lua::kView),
                                                           nv::rpc::params(mp::Value::integer(t.first_row + 1),
                                                                           mp::Value::integer(t.first_col))),
                                           nv::Host::Callback{});
                     }
                     doc_ = nv::DocFacts{true, back->buf, back->name, back->modified, back->tick, back->buftype};
                     convention_ = dos ? 1 : 0;
                     adopted_ = true;
                     kept_ = t; // carried back as it was handed if this Neovim ends before it proves it serves
                     adopted.ready = true;
                     (void)loom::answer_deferred(*due, later, adopted);
                 });
}

/// SERVING? A round trip to Neovim that names what it holds, answered when Neovim answers.
inline void NeovimEditorWeave::on(const EditorLiveRequested& asked, loom::Mail& mail) {
    if (!mail.authored_from_role(ws::kEditorSwitchRole)) {
        return;
    }
    if (!activation_.activated() || !running()) {
        (void)mail.answer(EditorLive{asked.op, false, not_running_words()});
        return;
    }
    const auto due = std::make_shared<loom::DeferredAnswer>(mail.defer_answer());
    const std::int64_t op = asked.op;
    ask(nv::lua::kDocFacts, nv::rpc::params(), kAskMs, mail, [this, op, due](const Answer& a, loom::Mail& later) {
        if (!a.result.has_value()) {
            (void)loom::answer_deferred(*due, later, EditorLive{op, false, "Neovim did not answer: " + a.why});
            return;
        }
        kept_.reset();
        (void)loom::answer_deferred(
            *due, later,
            EditorLive{op, true,
                       started_words() + " holds " + (doc_path().empty() ? std::string("no source") : doc_path())});
    });
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

// ---- What a switch is judged from, asked of Neovim ------------------------------------------

/// UNTIL NEOVIM IS NO LONGER WAITING FOR INPUT, or `until` on the host's wait's clock: its fast
/// mode, asked again after each answer.
inline void NeovimEditorWeave::until_unblocked(std::int64_t until, loom::Mail& mail,
                                               std::function<void(bool, loom::Mail&)> then) {
    ask_rpc("nvim_get_mode", nv::rpc::params(), kAskMs, mail, [this, until, then](const Answer& a, loom::Mail& later) {
        const mp::Value* blocking = a.result.has_value() ? a.result->get("blocking") : nullptr;
        if (blocking != nullptr && !blocking->as_bool(false)) {
            then(true, later);
            return;
        }
        const std::optional<std::int64_t> now = wait_.now_ms();
        if (!running() || (now && *now >= until)) {
            then(false, later);
            return;
        }
        until_unblocked(until, later, then);
    });
}

/// EVERYTHING A TRANSFER AND A JUDGMENT ARE DECIDED FROM, asked of Neovim: `then` hears it, or no
/// answer and why.
inline void NeovimEditorWeave::export_later(int ms, loom::Mail& mail, Exported_then then) {
    ask(nv::lua::kExport, nv::rpc::params(), ms, mail, [then](const Answer& a, loom::Mail& later) {
        then(a.result.has_value() ? exported_of(*a.result) : std::nullopt, a, later);
    });
}

inline std::optional<Exported> NeovimEditorWeave::exported_of(const mp::Value& v) {
    const auto str = [&v](const char* k) {
        const mp::Value* f = v.get(k);
        return f != nullptr ? f->as_str() : std::string();
    };
    const auto num = [&v](const char* k, std::int64_t d) {
        const mp::Value* f = v.get(k);
        return f != nullptr ? f->as_int(d) : d;
    };
    const auto flag = [&v](const char* k, bool d) {
        const mp::Value* f = v.get(k);
        return f != nullptr ? f->as_bool(d) : d;
    };
    Exported e;
    e.mode = str("mode");
    e.buf = num("buf", 0);
    e.name = str("name");
    e.buftype = str("buftype");
    e.modified = flag("modified", false);
    e.tick = num("tick", 0);
    if (const mp::Value* lines = v.get("lines"); lines != nullptr) {
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
    if (const mp::Value* c = v.get("cursor"); c != nullptr) {
        e.at.cursor = nv::Pos{c->at(0).as_int(), c->at(1).as_int()};
    }
    if (const mp::Value* s = v.get("vstart"); s != nullptr) {
        e.at.vstart = nv::Pos{s->at(0).as_int(), s->at(1).as_int()};
    }
    e.topline = num("topline", 1);
    e.leftcol = num("leftcol", 0);
    e.wrap = flag("wrap", true);
    if (const mp::Value* others = v.get("others"); others != nullptr) {
        for (const mp::Value& o : others->as_array()) {
            const mp::Value* n = o.get("name");
            e.others.push_back(n == nullptr || n->as_str().empty() ? std::string("[No Name]") : spelled(n->as_str()));
        }
    }
    if (const mp::Value* jobs = v.get("jobs"); jobs != nullptr) {
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

/// THE JUDGMENT A SWITCH IS DECIDED FROM: refused at once for a state no switch may begin in,
/// else from what Neovim says it holds -- when Neovim waits at a prompt, refused; in an unfinished
/// command, that command ended with Escape, said among the resets, and Neovim asked again.
inline void NeovimEditorWeave::judge(loom::Mail& mail, Judged then) {
    Judgement j;
    j.transfer.doc_epoch = epoch_;
    j.transfer.project_dir = project_dir_;
    j.transfer.project_known = project_known_;
    j.transfer.source = started_words();
    const auto refuse = [&](std::string why) {
        j.refusal = std::move(why);
        then(std::move(j), mail);
    };
    if (candidate_.live || preparing_) {
        refuse("Neovim is still opening " + (candidate_.live ? candidate_.path : std::string("a file")) +
               " -- switch once it has settled");
        return;
    }
    if (!pastes_.empty()) {
        refuse("Neovim is still waiting for a clipboard answer -- switch once it has arrived");
        return;
    }
    if (!relays_.empty()) {
        refuse("the Editor is still relaying an open -- switch once it has settled");
        return;
    }
    if (locate_.stage != Locate::Stage::Idle) {
        refuse("a dropped location is still being opened -- switch once it has settled");
        return;
    }
    if (drop_.pending) {
        refuse("a dropped command is still waiting for your choice -- choose or dismiss it, then switch");
        return;
    }
    if (!settle_held()) {
        refuse(held_->what + " is still waiting for Neovim -- answer what Neovim is waiting for (Escape "
                             "ends an unfinished command), then switch");
        return;
    }
    if (!running()) {
        // NO NEOVIM, NO DOCUMENT -- unless one was handed to this editor and Neovim ended before
        // it proved it serves: that document is carried back exactly as it was handed.
        if (kept_.has_value()) {
            j.transfer = *kept_;
            j.transfer.doc_epoch = epoch_;
            j.notes.push_back("Neovim ended before it served; the document it was handed is carried back as it was handed");
        }
        then(std::move(j), mail);
        return;
    }
    export_later(kAdoptMs, mail, [this, j, then](std::optional<Exported> e, const Answer& a, loom::Mail& later) {
        if (e.has_value()) {
            then(judged_from(j, *e), later);
            return;
        }
        if (!running() || !a.outstanding) {
            Judgement no = j;
            no.refusal = "Neovim could not be asked what it holds (" + a.why + ")";
            then(std::move(no), later);
            return;
        }
        // NEOVIM IS WAITING FOR INPUT (measured: a typed count holds every non-fast request, and
        // so does a prompt). A PROMPT is the weaver's to answer: refused. AN UNFINISHED COMMAND is
        // a count or a register name nobody finished: cancelled the way a weaver would, with
        // Escape, and said among the resets -- then Neovim is asked again.
        const std::string first_why = a.why;
        ask_rpc("nvim_get_mode", nv::rpc::params(), kAskMs, later,
                [this, j, then, first_why](const Answer& m, loom::Mail& l2) {
                    const mp::Value* mode = m.result.has_value() ? m.result->get("mode") : nullptr;
                    const mp::Value* blocking = m.result.has_value() ? m.result->get("blocking") : nullptr;
                    Judgement next = j;
                    if (mode == nullptr || blocking == nullptr || !blocking->as_bool(false)) {
                        next.refusal = "Neovim could not be asked what it holds (" + first_why + ")";
                        then(std::move(next), l2);
                        return;
                    }
                    const std::string waiting = mode->as_str();
                    if (!waiting.empty() && waiting[0] == 'r') {
                        next.refusal = "Neovim is waiting at a prompt -- answer it in Neovim and switch again";
                        then(std::move(next), l2);
                        return;
                    }
                    (void)host_->input("<Esc>");
                    // THE ESCAPE IS TYPEAHEAD, and the fast mode is answered ahead of typeahead:
                    // asked at once it would still say waiting. So Neovim is asked until it has
                    // taken the key.
                    until_unblocked(wait_.now_ms().value_or(0) + kAskMs, l2,
                                    [this, next, then, waiting](bool, loom::Mail& l3) {
                                        export_later(kAdoptMs, l3,
                                                     [this, next, then, waiting](std::optional<Exported> again,
                                                                                 const Answer& a2, loom::Mail& l4) {
                                                         Judgement last = next;
                                                         if (!again.has_value()) {
                                                             last.refusal = "Neovim could not be asked what it holds (" +
                                                                            a2.why + ")";
                                                             then(std::move(last), l4);
                                                             return;
                                                         }
                                                         last.resets.push_back(
                                                             "an unfinished command in Neovim (it was waiting in mode `" +
                                                             waiting + "`)");
                                                         then(judged_from(std::move(last), *again), l4);
                                                     });
                                    });
                });
    });
}

/// WHAT NEOVIM'S EXPORT MAKES OF A JUDGMENT: what a transfer cannot carry is refused, what a
/// switch would lose and reset is named, and the document is carried.
inline Judgement NeovimEditorWeave::judged_from(Judgement j, const Exported& e0) const {
    const Exported* e = &e0;
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
        const zengine::persist::FileText saved = zengine::persist::read_file(t.path, 1u << 28, "a source file");
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

/// THE LINE A STARTED NEOVIM IS ANSWERED WITH: where it listens, what it opened, how to attach.
inline std::string NeovimEditorWeave::listening_words(const std::string& opened) const {
    return started_words() + " is listening at " + listen_ + opened +
           " -- attach its interface with: nvim --server " + listen_ + " --remote-ui";
}

/// START A NEOVIM FOR A SECOND TERMINAL, answered once it is ready and has opened the file asked.
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
    const auto due = std::make_shared<loom::DeferredAnswer>(mail.defer_answer());
    const std::string asked_path = asked.path;
    when_ready(mail, [this, due, asked_path](bool ok, const std::string& why, loom::Mail& later) {
        if (!ok) {
            (void)loom::answer_deferred(*due, later, loom::Refused{why});
            return;
        }
        if (asked_path.empty()) {
            (void)loom::answer_deferred(*due, later, loom::Result{listening_words(std::string())});
            return;
        }
        const std::string path = zengine::persist::resolved_against(
            project_known_ ? project_dir_ : std::filesystem::current_path().generic_string(), asked_path);
        ask(nv::lua::kPrepare, nv::rpc::params(mp::Value::str(path), mp::Value::integer(1)), kAskMs, later,
            [this, due, path](const Answer& a, loom::Mail& l2) {
                const mp::Value* refused = a.result.has_value() ? a.result->get("why") : nullptr;
                if (!a.result.has_value() || refused != nullptr) {
                    const std::string words = refused != nullptr ? refused->as_str() : a.why;
                    if (host_ != nullptr) {
                        (void)host_->finish(nv::kQuitGraceMs);
                    }
                    (void)loom::answer_deferred(*due, l2, loom::Refused{"Neovim could not open " + path + ": " + words});
                    return;
                }
                const std::int64_t buf = field(*a.result, "buf").as_int();
                ask(nv::lua::kShow, nv::rpc::params(mp::Value::integer(buf)), kAskMs, l2,
                    [this, due, path](const Answer&, loom::Mail& l3) {
                        (void)loom::answer_deferred(*due, l3, loom::Result{listening_words(" editing " + path)});
                    });
            });
    });
}

inline void NeovimEditorWeave::on(const nve::NeovimStopRequested& asked, loom::Mail& mail) {
    if (!running()) {
        (void)mail.answer(loom::Result{not_running_words()});
        return;
    }
    if (holding_.active) {
        (void)mail.answer(loom::Refused{"the Editor is being switched -- stop Neovim once the switch has settled"});
        return;
    }
    const auto due = std::make_shared<loom::DeferredAnswer>(mail.defer_answer());
    if (asked.discard) {
        stop_neovim(*due, mail);
        return;
    }
    ask(nv::lua::kUnsaved, nv::rpc::params(), kAskMs, mail, [this, due](const Answer& a, loom::Mail& later) {
        if (!a.result.has_value()) {
            (void)loom::answer_deferred(
                *due, later, loom::Refused{"Neovim could not be asked whether it holds unsaved work (" + a.why + ")"});
            return;
        }
        std::vector<std::string> named;
        if (const mp::Value* buffers = a.result->get("buffers"); buffers != nullptr) {
            for (const mp::Value& b : buffers->as_array()) {
                const mp::Value* n = b.get("name");
                named.push_back(n == nullptr || n->as_str().empty() ? std::string("[No Name]") : spelled(n->as_str()));
            }
        }
        if (!named.empty()) {
            (void)loom::answer_deferred(*due, later,
                                        loom::Refused{"Neovim holds unsaved changes to " + listed(named) +
                                                      " -- write them first, or stop with discard=true to lose them"});
            return;
        }
        stop_neovim(*due, later);
    });
}

/// NEOVIM ENDED BY THIS EDITOR, and the asker told how.
inline void NeovimEditorWeave::stop_neovim(const loom::DeferredAnswer& due, loom::Mail& mail) {
    if (!running()) {
        (void)loom::answer_deferred(due, mail, loom::Result{not_running_words()});
        return;
    }
    const nv::Child::Ended ended = host_->finish(nv::kQuitGraceMs);
    failure_ = "Neovim was stopped";
    doc_ = nv::DocFacts{};
    notice("Neovim was stopped -- open a source to start it again", false);
    resay_ = true;
    (void)loom::answer_deferred(due, mail,
                                loom::Result{"Neovim ended (status " + std::to_string(ended.status) +
                                             (ended.forced ? ", forced" : "") + ")"});
}

inline void NeovimEditorWeave::on(const nve::NeovimStatusRequested&, loom::Mail& mail) {
    if (!running()) {
        (void)mail.answer(loom::Result{not_running_words()});
        return;
    }
    const auto due = std::make_shared<loom::DeferredAnswer>(mail.defer_answer());
    ask(nv::lua::kDocFacts, nv::rpc::params(), kAskMs, mail, [this, due](const Answer& a, loom::Mail& later) {
        if (!a.result.has_value()) {
            (void)loom::answer_deferred(
                *due, later,
                loom::Result{started_words() + " (" + attached_words() + ") could not say what it holds: " + a.why});
            return;
        }
        const mp::Value* name = a.result->get("name");
        const mp::Value* modified = a.result->get("modified");
        const std::string path =
            name == nullptr || name->as_str().empty() ? std::string("no file") : spelled(name->as_str());
        (void)loom::answer_deferred(
            *due, later,
            loom::Result{started_words() + " (" + attached_words() + ") is editing " + path + ", " +
                         (modified != nullptr && modified->as_bool(false) ? "modified" : "saved") + ", in mode " +
                         (running() ? host_->mode() : std::string())});
    });
}

} // namespace

#endif // ZENGINE_NEOVIM_EDITOR_OPENING_SWITCH_HPP
