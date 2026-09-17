// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_EDITOR_SWITCH_HPP
#define ZENGINE_WORKSHOP_EDITOR_SWITCH_HPP

// THE EDITOR SWITCH COORDINATOR: one owner for one switch of `zengine.editor` between the choices
// the load plan authors for it (WL-SWITCH, agents/workshop/editor-switch.md).
//
// IT OWNS THE OPERATION AND NOTHING ELSE. The document belongs to whichever editor holds the
// office; the office's holder, its sealing and its admission belong to Loom; which artifacts may
// hold the office belongs to the plan; which one holds it now is recorded by the realization
// owner. The coordinator holds one switch's intent, stage, consent and outcome, and it drives
// Loom's prepared replacement through the handoff conversation (`editor_handoff_vocabulary.hpp`):
//
//     request   judge the incumbent, before anything is loaded: losses need consent; a refusal
//               ends it; the destination already holding the office ends it harmlessly
//     load      the destination's artifact SEALED (`loom::PreparedReplacement::start`)
//     warm      the candidate starts in the room the incumbent had; the coordinator relays a
//               beat to it, because a sealed weave receives none
//     boundary  the incumbent authors the exact document and holds still; losses that changed
//               since the consent send the maker back to confirm, and nothing is kept
//     adopt     Loom's one preparation ask: the candidate adopts the document or says why not
//     commit    the admission, which is also the successor's activation; the office moves
//     prove     the successor is asked whether it serves; its answer releases the retired
//               incumbent, or keeps it and reports a failure after the commitment
//     retire    the retired incumbent says what it refused while it held still, and is unloaded
//
// EVERY ENDING IS AN ANSWER, to the ask that is waiting: `EditorSwitchAnswered`. Before the
// commitment an ending moves nothing -- the candidate is discarded by the transaction's abort
// and the incumbent is told to resume. After it, nothing is rolled back.
//
// ONE SWITCH AT A TIME, AND NO TIMEOUT. A second request while one is under way is refused in
// words; a request while one only awaits confirmation replaces it. A participant that never
// answers leaves the switch pending and published (`EditorSwitchProgress`), and a maker cancels
// it. Nothing here waits, sleeps or pumps the bus.
//
// HOST-TIER COMPOSITION. It holds the `Switchboard&` and `Kernel&` a prepared replacement needs,
// handed to it by the host that mounts it -- the Loom's own authoring pattern (a coordinator holds
// the host's handle) -- and four readings of the realization owner, as functions the host wires.

#include "editor_handoff_vocabulary.hpp"
#include "editor_switch_vocabulary.hpp"
#include "load_plan.hpp"

#include "timer/binding.hpp"

#include <zen/host/prepared_replacement.hpp>
#include <zen/kernel/kernel.hpp>
#include <zen/switchboard/switchboard.hpp>
#include <zen/terminal/vocabulary.hpp>
#include <zen/weave.hpp>
#include <zen/weave/dispatch_refusal.hpp>

#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace zengine::workshop {

/// WHAT THE HOST HANDS THE COORDINATOR.
struct EditorSwitchHost {
    loom::Switchboard* bus = nullptr;
    loom::Kernel* kernel = nullptr;
    /// THE OFFICE A SWITCH MOVES: the office of the one pane the host manages, spelled where the
    /// host spells it, and never here.
    std::string office;
    /// The plan's authored choices for `zengine.editor`, read at the moment of the ask.
    std::function<std::vector<load::ChoiceIntent>()> choices;
    /// The artifact holding the office now, as realization recorded it (empty: none).
    std::function<std::string()> holder;
    /// The image a choice's artifact runs from when a switch loads it.
    std::function<std::string(const std::string& stem)> image_of;
    /// Record that the office is held by `weave`, loaded from `image`, for choice `stem`. Empty
    /// on success, else the owner's refusal.
    std::function<std::string(const std::string& stem, loom::WeaveId weave, const std::string& image)>
        record;
};

/// THE COORDINATOR'S READ SURFACE: what it has done, and the switch under way.
struct EditorSwitchState {
    std::int64_t switched = 0;
    std::int64_t refused = 0;
    std::int64_t op = 0;       ///< the switch under way, or 0
    std::string stage;         ///< its stage, or empty
    std::string destination;   ///< the choice it is switching to
    ZEN_SHAPE(EditorSwitchState, 1, ZEN_FIELD(switched), ZEN_FIELD(refused), ZEN_FIELD(op),
              ZEN_FIELD(stage), ZEN_FIELD(destination));
};

/// WHAT THE COORDINATOR MAY SAY, as the host grants it: its answers and its progress to whoever
/// Loom routes them to; the incumbent's four questions to `office`, the office it switches, and
/// nowhere else; the candidate's three and the retired editor's one to an id only known at
/// runtime; a beat asked of the Timer; and poke answers for its read surface. Nothing loads,
/// unloads or admits by message -- those are the host-tier calls it was handed.
inline loom::Grant editor_switch_grant(const std::string& office) {
    loom::Grant g;
    g.allow_to_any(EditorSwitchAnswered::zen_name, EditorSwitchAnswered::zen_version);
    g.allow_to_any(EditorSwitchProgress::zen_name, EditorSwitchProgress::zen_version);
    g.allow_to_role(EditorHandoffJudgeRequested::zen_name, EditorHandoffJudgeRequested::zen_version,
                    office);
    g.allow_to_role(EditorHandoffRequested::zen_name, EditorHandoffRequested::zen_version,
                    office);
    g.allow_to_role(EditorHandoffEnded::zen_name, EditorHandoffEnded::zen_version, office);
    g.allow_to_role(EditorLiveRequested::zen_name, EditorLiveRequested::zen_version, office);
    g.allow_to_any(EditorWarmRequested::zen_name, EditorWarmRequested::zen_version);
    g.allow_to_any(EditorPreparationTick::zen_name, EditorPreparationTick::zen_version);
    g.allow_to_any(EditorAdoptRequested::zen_name, EditorAdoptRequested::zen_version);
    g.allow_to_any(EditorRetireRequested::zen_name, EditorRetireRequested::zen_version);
    g.allow_to_role(zengine::timer::EnsureTimer::zen_name, zengine::timer::EnsureTimer::zen_version,
                    zengine::timer::kTimerRole);
    g.allow_to_role(zengine::timer::CancelTimer::zen_name, zengine::timer::CancelTimer::zen_version,
                    zengine::timer::kTimerRole);
    loom::allow_poke_answers(g);
    return g;
}

/// WHAT A TERMINAL PARTICIPANT MAY ASK OF THE SWITCH, AND HEAR BACK: the four request shapes,
/// known and granted to the switch's office alone, and the answer accepted. The host widens the
/// terminal it mounts with exactly this, and a case that types the documented example widens its
/// terminal with the same call -- so what the example proves is what the host grants.
inline void let_terminal_switch_editors(loom::TerminalVocabulary& vocab, loom::Grant& grant) {
    vocab.knows(loom::schema_of<EditorSwitchRequested>())
        .knows(loom::schema_of<EditorSwitchConfirmed>())
        .knows(loom::schema_of<EditorSwitchCancelled>())
        .knows(loom::schema_of<EditorSwitchStatusRequested>())
        .accepts(loom::schema_of<EditorSwitchAnswered>());
    grant.allow_to_role(EditorSwitchRequested::zen_name, EditorSwitchRequested::zen_version,
                        kEditorSwitchRole);
    grant.allow_to_role(EditorSwitchConfirmed::zen_name, EditorSwitchConfirmed::zen_version,
                        kEditorSwitchRole);
    grant.allow_to_role(EditorSwitchCancelled::zen_name, EditorSwitchCancelled::zen_version,
                        kEditorSwitchRole);
    grant.allow_to_role(EditorSwitchStatusRequested::zen_name,
                        EditorSwitchStatusRequested::zen_version, kEditorSwitchRole);
}

class EditorSwitchCoordinator;

using EditorSwitchBase = zengine::timer::TimedWeave<
    EditorSwitchCoordinator, EditorSwitchState,
    loom::Accept<EditorSwitchRequested, EditorSwitchConfirmed, EditorSwitchCancelled,
                 EditorSwitchStatusRequested, EditorHandoffJudged, EditorWarmed,
                 EditorHandoffOffered, EditorAdopted, EditorLive, EditorRetired,
                 loom::DispatchRefused>,
    loom::Emit<EditorSwitchAnswered, EditorSwitchProgress, EditorHandoffJudgeRequested,
               EditorWarmRequested, EditorPreparationTick, EditorHandoffRequested,
               EditorHandoffEnded, EditorAdoptRequested, EditorLiveRequested,
               EditorRetireRequested>>;

/// How often a warming or adopting candidate is handed a beat (the Timer's own floor).
inline constexpr std::int64_t kSwitchBeatMs = 10;

// WL-SWITCH-03 -- agents/workshop/editor-switch.md
class EditorSwitchCoordinator : public EditorSwitchBase {
public:
    explicit EditorSwitchCoordinator(EditorSwitchHost host) : host_(std::move(host)) {
        beat_ = timers().repeat("zengine.editor-switch.beat", std::chrono::milliseconds(kSwitchBeatMs),
                                &EditorSwitchCoordinator::on_beat);
    }

    using EditorSwitchBase::on;

    /// THE HOST MOUNTS THIS WEAVE AND TELLS IT WHICH WEAVE IT IS -- and the coordinator needs its
    /// own id to begin a replacement as its operator and coordinator.
    void zen_set_self(loom::WeaveId id) {
        EditorSwitchBase::zen_set_self(id);
        self_id_ = id;
    }

    enum class Stage : std::uint8_t {
        Idle,
        Judging,
        AwaitingConsent,
        Warming,
        Boundary,
        Adopting,
        Proving,
        Retiring,
    };

    Stage stage() const noexcept { return op_.stage; }
    std::int64_t op() const noexcept { return op_.id; }

    // ---- the four doors a maker's asker uses -------------------------------------------------

    void on(const EditorSwitchRequested& asked, loom::Mail& mail) {
        if (host_.office.empty()) {
            answer_now(mail, refused_answer(0, asked.destination,
                                            "this host manages no editor office to switch"));
            return;
        }
        const std::vector<load::ChoiceIntent> choices = authored();
        const load::ChoiceIntent* destination = choice_named(choices, asked.destination);
        if (destination == nullptr) {
            answer_now(mail, refused_answer(0, asked.destination,
                                            asked.destination.empty()
                                                ? "name the choice to switch to"
                                                : "this project authors no editor choice named `" +
                                                      asked.destination + "`"));
            return;
        }
        const std::string holding = host_.holder ? host_.holder() : std::string();
        if (holding.empty()) {
            answer_now(mail, refused_answer(0, destination->name,
                                            "nobody holds " + host_.office +
                                                " -- there is no editor to switch from"));
            return;
        }
        if (holding == destination->stem) {
            EditorSwitchAnswered same = base_answer(0, switch_outcome::kAlreadyActive, destination->name);
            same.detail = "`" + destination->name + "` already holds " + host_.office +
                          "; nothing was loaded";
            answer_now(mail, same);
            return;
        }
        std::string superseded;
        std::int64_t replaced = 0;
        if (op_.stage == Stage::AwaitingConsent) {
            superseded = "switch " + std::to_string(op_.id) + " to `" + op_.destination +
                         "` was awaiting confirmation and was replaced by this one";
            replaced = op_.id;
            // ITS STANDING QUESTION IS OVER: retracted, and said, before the new switch begins.
            EditorSwitchAnswered over = base_answer(op_.id, switch_outcome::kSuperseded, op_.destination);
            over.detail = "switch " + std::to_string(op_.id) + " to `" + op_.destination +
                          "` was replaced by a newer request before it was confirmed";
            publish_said(mail, over);
            op_ = Op{};
        } else if (op_.stage != Stage::Idle) {
            answer_now(mail, refused_answer(0, destination->name,
                                            "switch " + std::to_string(op_.id) + " to `" +
                                                op_.destination + "` is under way (" +
                                                stage_word(op_.stage) + ") -- ask again once it has settled"));
            return;
        }
        begin_op(mail, *destination, holding);
        if (!superseded.empty()) {
            op_.notes.push_back(superseded);
            superseded_ = Superseded{replaced, op_.id};
        }
        op_.asker = mail.defer_answer();
        judge(mail);
    }

    void on(const EditorSwitchConfirmed& confirmed, loom::Mail& mail) {
        if (answer_superseded(mail, confirmed.op)) {
            return;
        }
        if (op_.stage != Stage::AwaitingConsent || confirmed.op != op_.id) {
            EditorSwitchAnswered no = base_answer(confirmed.op, switch_outcome::kRefused, std::string());
            no.detail = "switch " + std::to_string(confirmed.op) + " is not awaiting confirmation";
            answer_now(mail, no);
            return;
        }
        if (confirmed.consent != op_.consent) {
            EditorSwitchAnswered no = base_answer(op_.id, switch_outcome::kNeedsConfirmation, op_.destination);
            no.consent = op_.consent;
            no.losses = op_.losses;
            no.detail = "that consent is not the one this switch asked for -- confirm with `" +
                        op_.consent + "`";
            answer_now(mail, no);
            return;
        }
        op_.consented = op_.consent;
        op_.asker = mail.defer_answer();
        load_and_warm(mail);
    }

    void on(const EditorSwitchCancelled& cancelled, loom::Mail& mail) {
        if (answer_superseded(mail, cancelled.op)) {
            return;
        }
        if (op_.stage == Stage::Idle || cancelled.op != op_.id) {
            EditorSwitchAnswered no = base_answer(cancelled.op, switch_outcome::kRefused, std::string());
            no.detail = "switch " + std::to_string(cancelled.op) + " is not under way";
            answer_now(mail, no);
            return;
        }
        if (op_.stage == Stage::Proving || op_.stage == Stage::Retiring) {
            EditorSwitchAnswered no = base_answer(op_.id, switch_outcome::kRefused, op_.destination);
            no.detail = "switch " + std::to_string(op_.id) + " has already committed and cannot be cancelled";
            answer_now(mail, no);
            return;
        }
        const std::string said = "switch " + std::to_string(op_.id) + " to `" + op_.destination +
                                  "` was cancelled; nothing moved";
        EditorSwitchAnswered done = base_answer(op_.id, switch_outcome::kCancelled, op_.destination);
        done.detail = said;
        answer_now(mail, done);
        abandon(mail, switch_outcome::kCancelled, said);
    }

    void on(const EditorSwitchStatusRequested&, loom::Mail& mail) {
        EditorSwitchAnswered said = base_answer(op_.id, switch_outcome::kStatus, op_.destination);
        std::string authored;
        for (const std::string& name : said.choices) {
            authored += (authored.empty() ? "" : ", ") + name;
        }
        const std::string holds = (said.active.empty() ? std::string("no authored choice") : "`" + said.active + "`") +
                                  " holds " + host_.office + " (choices: " + authored + "); ";
        if (op_.stage != Stage::Idle) {
            said.detail = holds + "switch " + std::to_string(op_.id) + " to `" + op_.destination + "` is " +
                          stage_word(op_.stage);
            said.consent = op_.stage == Stage::AwaitingConsent ? op_.consent : std::string();
            said.losses = op_.stage == Stage::AwaitingConsent ? op_.losses : std::vector<std::string>{};
        } else {
            said.detail = holds + (retained_.empty() ? "no switch is under way"
                                                     : "no switch is under way; the retired `" + retained_ +
                                                           "` is kept after a failure");
        }
        answer_now(mail, said);
    }

    // ---- the incumbent's answers ----------------------------------------------------------------

    void on(const EditorHandoffJudged& judged, loom::Mail& mail) {
        if (!answers(mail, Stage::Judging, judged.op)) {
            return;
        }
        if (!judged.ok) {
            finish_refused(mail, "the editor in office refused: " + judged.refusal, false);
            return;
        }
        op_.rows = judged.rows;
        op_.columns = judged.columns;
        op_.project_dir = judged.project_dir;
        op_.project_known = judged.project_known;
        op_.resets = judged.resets;
        op_.losses = judged.losses;
        op_.consent = judged.digest;
        if (!judged.losses.empty()) {
            op_.stage = Stage::AwaitingConsent;
            progress(mail, "the maker's confirmation");
            EditorSwitchAnswered ask = base_answer(op_.id, switch_outcome::kNeedsConfirmation, op_.destination);
            ask.consent = op_.consent;
            ask.losses = op_.losses;
            ask.resets = op_.resets;
            ask.detail = "switching to `" + op_.destination + "` would lose " + losses_said(op_.losses) +
                         "; confirm with op " + std::to_string(op_.id) + " and consent " + op_.consent + ", or cancel";
            spend(mail, ask);
            return;
        }
        load_and_warm(mail);
    }

    void on(const EditorHandoffOffered& offered, loom::Mail& mail) {
        if (!answers(mail, Stage::Boundary, offered.op) || !(mail.sender() == op_.incumbent)) {
            return;
        }
        if (!offered.ok) {
            // It refused, so it did not hold still: nothing to end on its side.
            abort_transaction();
            finish_refused(mail, "the editor in office refused at the boundary: " + offered.refusal, false);
            return;
        }
        op_.holding = true;
        if (!offered.losses.empty() && offered.digest != op_.consented) {
            // THE LOSSES MOVED SINCE THE CONSENT (or were never consented to): nothing is kept.
            abort_transaction();
            end_handoff(mail, "its losses changed before the boundary");
            op_.stage = Stage::AwaitingConsent;
            op_.losses = offered.losses;
            op_.consent = offered.digest;
            op_.consented.clear();
            op_.candidate = loom::WeaveId{};
            progress(mail, "the maker's confirmation");
            EditorSwitchAnswered ask = base_answer(op_.id, switch_outcome::kNeedsConfirmation, op_.destination);
            ask.consent = op_.consent;
            ask.losses = op_.losses;
            ask.detail = "what switching would lose changed before it could begin -- now " +
                         losses_said(op_.losses) + "; confirm again with op " + std::to_string(op_.id) +
                         " and consent " + op_.consent + ", or cancel";
            spend(mail, ask);
            return;
        }
        for (const std::string& r : offered.resets) {
            add_unique(op_.resets, r);
        }
        for (const std::string& n : offered.notes) {
            op_.notes.push_back(n);
        }
        op_.stage = Stage::Adopting;
        progress(mail, op_.destination_stem);
        EditorAdoptRequested adopt;
        adopt.op = op_.id;
        adopt.transfer = offered.transfer;
        const loom::TxnResult asked = op_.txn->ask(adopt);
        if (!asked.ok) {
            abort_transaction();
            end_handoff(mail, "the candidate could not be asked to adopt the document");
            finish_refused(mail, std::string("the candidate could not be asked to adopt the document (Loom: ") +
                                     loom::name_of(asked.why) + ")",
                           false);
        }
    }

    // ---- the candidate's answers ----------------------------------------------------------------

    void on(const EditorWarmed& warmed, loom::Mail& mail) {
        if (!answers(mail, Stage::Warming, warmed.op) || !(mail.sender() == op_.candidate)) {
            return;
        }
        if (!warmed.ok) {
            abort_transaction();
            finish_refused(mail, "`" + op_.destination + "` could not start: " + warmed.refusal, false);
            return;
        }
        if (!warmed.detail.empty()) {
            op_.notes.push_back("started " + warmed.detail);
        }
        op_.stage = Stage::Boundary;
        progress(mail, host_.office);
        op_.awaiting = mail.as_role(kEditorSwitchRole)
                           .send_to_role(host_.office, EditorHandoffRequested{op_.id},
                                         correlation());
    }

    void on(const EditorAdopted& adopted, loom::Mail& mail) {
        if (op_.stage != Stage::Adopting || adopted.op != op_.id || !op_.txn.has_value()) {
            return;
        }
        // THE TRANSACTION IS THE JUDGE of whether this is the candidate's authentic answer to its
        // preparation ask; the coordinator only maps the candidate's word onto Ready or Refused.
        const loom::TxnResult offered = op_.txn->offer_current_answer(
            adopted.ready ? loom::PreparationAnswer::Ready : loom::PreparationAnswer::Refused);
        if (!offered.ok) {
            return; // not the answer this transaction is waiting for
        }
        for (const std::string& n : adopted.notes) {
            op_.notes.push_back(n);
        }
        if (!adopted.ready) {
            (void)op_.txn->take_outcome();
            op_.txn.reset();
            end_handoff(mail, "the destination could not adopt the document");
            finish_refused(mail, "`" + op_.destination + "` could not adopt the document: " + adopted.refusal,
                           false);
            return;
        }
        const loom::TxnResult committed = op_.txn->commit(++activations_);
        if (!committed.ok) {
            abort_transaction();
            end_handoff(mail, "the admission could not be scheduled");
            finish_refused(mail, std::string("the admission could not be scheduled (Loom: ") +
                                     loom::name_of(committed.why) + ")",
                           false);
            return;
        }
        // THE ADMISSION IS SCHEDULED AHEAD OF THE FIRST QUEUED TRAFFIC THAT COULD REACH THE
        // CANDIDATE, and this role-addressed question is such traffic: by the time it is
        // delivered the office has moved, or the admission was revoked and it reaches the
        // incumbent -- and the answer's sender tells the two apart.
        op_.stage = Stage::Proving;
        progress(mail, op_.destination_stem);
        op_.awaiting = mail.as_role(kEditorSwitchRole)
                           .send_to_role(host_.office, EditorLiveRequested{op_.id}, correlation());
    }

    // ---- the successor's answer ------------------------------------------------------------------

    void on(const EditorLive& live, loom::Mail& mail) {
        if (!answers(mail, Stage::Proving, live.op)) {
            return;
        }
        const std::optional<loom::TxnOutcome> outcome =
            op_.txn.has_value() ? op_.txn->take_outcome() : std::nullopt;
        const bool committed = outcome.has_value() && outcome->state == loom::TxnState::Committed;
        op_.txn.reset();
        if (!committed) {
            end_handoff(mail, "the admission did not commit");
            finish_refused(mail, std::string("the admission did not commit") +
                                     (outcome.has_value() ? std::string(" (Loom: ") +
                                                                loom::name_of(outcome->reason) + ")"
                                                          : std::string()),
                           false);
            return;
        }
        // THE OFFICE MOVED: realization records who holds it, whatever the successor says next.
        const std::string recorded =
            host_.record ? host_.record(op_.destination_stem, op_.candidate, op_.image) : std::string();
        if (!recorded.empty()) {
            op_.notes.push_back("the realization record was not updated: " + recorded);
        }
        if (!live.ok || !(mail.sender() == op_.candidate)) {
            retained_ = op_.from_stem;
            ++state_.refused;
            EditorSwitchAnswered failed = base_answer(op_.id, switch_outcome::kFailedAfterCommit, op_.destination);
            failed.detail = "`" + op_.destination + "` holds " + host_.office +
                            " and did not prove it serves: " +
                            (live.detail.empty() ? std::string("no word") : live.detail) +
                            " -- the retired `" + op_.from + "` was kept";
            failed.resets = op_.resets;
            failed.notes = op_.notes;
            spend(mail, failed);
            finish(mail);
            return;
        }
        // THE RETIRED INCUMBENT IS SEALED TO THIS COORDINATOR and still answers it: what it refused
        // while it held still belongs in the answer before it is unloaded.
        op_.stage = Stage::Retiring;
        progress(mail, op_.from_stem);
        op_.awaiting = mail.send(op_.incumbent, EditorRetireRequested{op_.id}, correlation());
    }

    void on(const EditorRetired& retired, loom::Mail& mail) {
        if (!answers(mail, Stage::Retiring, retired.op) || !(mail.sender() == op_.incumbent)) {
            return;
        }
        if (retired.refused_inputs > 0) {
            op_.notes.push_back(std::to_string(retired.refused_inputs) +
                                (retired.refused_inputs == 1 ? " input was" : " inputs were") +
                                " refused while `" + op_.from + "` held still for the switch");
        }
        complete_switch(mail);
    }

    // ---- refusals the bus reports --------------------------------------------------------------

    void on(const loom::DispatchRefused& refused, loom::Mail& mail) {
        if (!mail.dispatch_refused() || op_.stage == Stage::Idle || op_.stage == Stage::AwaitingConsent) {
            return;
        }
        const loom::Ticket attempt = refused.refused_attempt();
        if (!attempt.valid() || attempt.seq != op_.awaiting.seq) {
            return;
        }
        const std::string why = "a switch message could not be delivered (" + refused.reason + ")";
        if (op_.stage == Stage::Retiring) {
            complete_switch(mail); // the switch happened; only the retired editor's count is missing
            return;
        }
        if (op_.stage == Stage::Judging) {
            finish_refused(mail, "the editor in office does not take part in switches: " + why, false);
            return;
        }
        if (op_.stage == Stage::Proving) {
            // The office moved or did not; the outcome says which, and the successor never heard.
            const std::optional<loom::TxnOutcome> outcome =
                op_.txn.has_value() ? op_.txn->take_outcome() : std::nullopt;
            op_.txn.reset();
            if (outcome.has_value() && outcome->state == loom::TxnState::Committed) {
                if (host_.record) {
                    (void)host_.record(op_.destination_stem, op_.candidate, op_.image);
                }
                retained_ = op_.from_stem;
                EditorSwitchAnswered failed =
                    base_answer(op_.id, switch_outcome::kFailedAfterCommit, op_.destination);
                failed.detail = "`" + op_.destination + "` holds " + host_.office +
                                " and could not be asked whether it serves: " + why;
                spend(mail, failed);
                finish(mail);
                return;
            }
        }
        abort_transaction();
        end_handoff(mail, why);
        finish_refused(mail, why, false);
    }

private:
    struct Op {
        std::int64_t id = 0;
        Stage stage = Stage::Idle;
        std::string destination;      ///< the choice's name
        std::string destination_stem;
        std::string from;             ///< the incumbent choice's name (or its stem)
        std::string from_stem;
        loom::DeferredAnswer asker;
        loom::Ticket awaiting{};
        std::uint64_t correlation = 0;
        std::int64_t rows = 0;
        std::int64_t columns = 0;
        std::string project_dir;
        bool project_known = false;
        std::vector<std::string> losses;
        std::vector<std::string> resets;
        std::vector<std::string> notes;
        std::string consent;   ///< the digest this switch asked the maker to confirm
        std::string consented; ///< the digest the maker confirmed
        std::optional<loom::PreparedReplacement> txn;
        loom::WeaveId incumbent{};
        loom::WeaveId candidate{};
        std::string image;
        bool holding = false;  ///< the incumbent answered the boundary and holds still
    };

    // ---- the operation's steps ---------------------------------------------------------------

    void complete_switch(loom::Mail& mail) {
        release_retired();
        ++state_.switched;
        EditorSwitchAnswered done = base_answer(op_.id, switch_outcome::kSwitched, op_.destination);
        done.detail = "switched " + host_.office + " from `" + op_.from + "` to `" +
                      op_.destination + "`";
        done.resets = op_.resets;
        done.notes = op_.notes;
        spend(mail, done);
        finish(mail);
    }

    void begin_op(loom::Mail& mail, const load::ChoiceIntent& destination, const std::string& holding) {
        (void)mail;
        op_ = Op{};
        op_.id = ++next_op_;
        op_.destination = destination.name;
        op_.destination_stem = destination.stem;
        op_.from_stem = holding;
        op_.from = holding;
        for (const load::ChoiceIntent& c : authored()) {
            if (c.stem == holding) {
                op_.from = c.name;
            }
        }
    }

    void judge(loom::Mail& mail) {
        op_.stage = Stage::Judging;
        progress(mail, host_.office);
        op_.awaiting = mail.as_role(kEditorSwitchRole)
                           .send_to_role(host_.office, EditorHandoffJudgeRequested{op_.id},
                                         correlation());
    }

    /// LOAD THE DESTINATION SEALED AND ASK IT TO START. A load the Kernel refuses ends the switch
    /// in the loader's own words, with nothing moved.
    void load_and_warm(loom::Mail& mail) {
        if (host_.bus == nullptr || host_.kernel == nullptr) {
            finish_refused(mail, "this host gave the switch no Loom to replace an office with", false);
            return;
        }
        // A RETIRED EDITOR KEPT AFTER A FAILURE is released first: the office's holder carries the
        // document now, and the retained weave would hold the name the destination loads under.
        release_retained();
        op_.image = host_.image_of ? host_.image_of(op_.destination_stem) : std::string();
        op_.txn.emplace(*host_.bus, *host_.kernel);
        loom::PreparedReplacement::Start start;
        start.operator_id = self();
        start.coordinator = self();
        start.role = host_.office;
        start.candidate_name = op_.destination_stem;
        start.candidate_path = op_.image;
        start.budget = 1;
        const loom::PreparedReplacement::StartResult started = op_.txn->start(start);
        if (!started.ok) {
            std::string why;
            switch (started.stage) {
            case loom::PreparedReplacement::StartStage::CandidateLoad:
                why = "`" + op_.destination + "` did not load: " + started.error;
                break;
            case loom::PreparedReplacement::StartStage::NoRoleHolder:
                why = std::string("nobody holds ") + host_.office;
                break;
            case loom::PreparedReplacement::StartStage::BeginTransaction:
                why = std::string("the replacement could not begin (Loom: ") +
                      loom::name_of(started.begin_reason) + ")";
                break;
            default:
                why = "the replacement could not begin";
                break;
            }
            op_.txn.reset();
            finish_refused(mail, why, false);
            return;
        }
        op_.incumbent = op_.txn->incumbent();
        op_.candidate = op_.txn->candidate();
        op_.stage = Stage::Warming;
        progress(mail, op_.destination_stem);
        if (!beat_.waiting()) {
            beat_.restart(mail);
        }
        op_.awaiting = mail.send(op_.candidate,
                                 EditorWarmRequested{op_.id, op_.rows, op_.columns, op_.project_dir,
                                                     op_.project_known},
                                 correlation());
    }

    /// THE BEAT, RELAYED to a sealed candidate while it starts or adopts; idle, it stops itself.
    void on_beat(const zengine::timer::TimerFired&, loom::Mail& mail) {
        if ((op_.stage == Stage::Warming || op_.stage == Stage::Adopting) && op_.candidate.valid()) {
            (void)mail.send(op_.candidate, EditorPreparationTick{op_.id});
            return;
        }
        if (op_.stage == Stage::Idle && !beat_.canceled()) {
            beat_.cancel(mail);
        }
    }

    /// Is THIS delivery the answer this switch is waiting for, at this stage?
    bool answers(const loom::Mail& mail, Stage stage, std::int64_t op) const {
        return op_.stage == stage && op == op_.id && mail.answers_ask() &&
               mail.correlation() == op_.correlation;
    }

    std::uint64_t correlation() {
        op_.correlation = ++correlations_;
        return op_.correlation;
    }

    void abort_transaction() {
        if (op_.txn.has_value()) {
            (void)op_.txn->abort();
            (void)op_.txn->take_outcome();
            op_.txn.reset();
        }
    }

    /// TELL A HOLDING INCUMBENT IT IS STILL THE EDITOR.
    void end_handoff(loom::Mail& mail, const std::string& why) {
        if (!op_.holding) {
            return;
        }
        op_.holding = false;
        (void)mail.as_role(kEditorSwitchRole)
            .send_to_role(host_.office, EditorHandoffEnded{op_.id, why});
    }

    /// A PENDING SWITCH ABANDONED BY THE MAKER (cancel): the transaction, the hold, the beat.
    void abandon(loom::Mail& mail, const char* outcome, const std::string& why) {
        abort_transaction();
        end_handoff(mail, why);
        if (op_.asker.valid()) {
            EditorSwitchAnswered done = base_answer(op_.id, outcome, op_.destination);
            done.detail = why;
            spend(mail, done);
        }
        finish(mail);
    }

    void finish_refused(loom::Mail& mail, const std::string& why, bool keep_transaction) {
        if (!keep_transaction) {
            abort_transaction();
        }
        ++state_.refused;
        EditorSwitchAnswered no = refused_answer(op_.id, op_.destination, why);
        no.notes = op_.notes;
        spend(mail, no);
        finish(mail);
    }

    /// THE OPERATION IS OVER: nothing stands, and what it came to was already said with its answer.
    void finish(loom::Mail& mail) {
        (void)mail;
        op_ = Op{};
        mirror();
    }

    /// UNLOAD THE RETIRED INCUMBENT once its successor proved it serves.
    void release_retired() {
        if (host_.kernel != nullptr && !op_.from_stem.empty() && op_.from_stem != op_.destination_stem) {
            (void)host_.kernel->unload(op_.from_stem);
        }
    }

    void release_retained() {
        if (retained_.empty() || host_.kernel == nullptr) {
            return;
        }
        const std::string holding = host_.holder ? host_.holder() : std::string();
        if (retained_ != holding) {
            (void)host_.kernel->unload(retained_);
        }
        retained_.clear();
    }

    // ---- answers and publications --------------------------------------------------------------

    /// A CONFIRMATION OR CANCEL FOR THE SWITCH A NEWER REQUEST REPLACED, answered in those words.
    bool answer_superseded(loom::Mail& mail, std::int64_t op) {
        if (op == 0 || op != superseded_.op) {
            return false;
        }
        EditorSwitchAnswered said = base_answer(op, switch_outcome::kSuperseded, std::string());
        said.detail = "switch " + std::to_string(op) + " was replaced by switch " +
                      std::to_string(superseded_.by) + " before it was confirmed";
        answer_now(mail, said);
        return true;
    }

    void spend(loom::Mail& mail, const EditorSwitchAnswered& said) {
        if (!op_.asker.valid()) {
            return;
        }
        (void)loom::answer_deferred(op_.asker, mail, said);
        op_.asker = loom::DeferredAnswer{};
        publish_said(mail, said);
    }

    void answer_now(loom::Mail& mail, const EditorSwitchAnswered& said) {
        if (said.outcome == switch_outcome::kRefused) {
            ++state_.refused;
        }
        (void)mail.answer(said);
        publish_said(mail, said);
        mirror();
    }

    /// WHAT AN ANSWER SAID, PUBLISHED AS THE OFFICE: a switch that is over retracts its condition
    /// and its outcome is said where the maker reads; a question awaiting confirmation stands with
    /// the consent it asks for; a status answer is said and retracts nothing.
    void publish_said(loom::Mail& mail, const EditorSwitchAnswered& said) {
        EditorSwitchProgress now;
        now.op = said.op;
        now.destination = said.destination;
        now.outcome = said.outcome;
        now.detail = said.detail;
        if (said.outcome == switch_outcome::kNeedsConfirmation && op_.stage == Stage::AwaitingConsent &&
            said.op == op_.id) {
            now.stage = stage_word(op_.stage);
            now.awaiting = "the maker's confirmation";
            now.consent = said.consent;
            now.pending = true;
        }
        (void)mail.as_role(kEditorSwitchRole).publish(now);
    }

    /// WHAT THE SWITCH WAITS ON, SAID AS THE OFFICE: a desk keeps it as a standing condition only
    /// when the switch's office said it.
    void progress(loom::Mail& mail, const std::string& awaiting) {
        EditorSwitchProgress now;
        now.op = op_.id;
        now.destination = op_.destination;
        now.stage = stage_word(op_.stage);
        now.awaiting = awaiting;
        now.pending = true;
        now.consent = op_.stage == Stage::AwaitingConsent ? op_.consent : std::string();
        (void)mail.as_role(kEditorSwitchRole).publish(now);
        mirror();
    }

    void mirror() {
        state_.op = op_.id;
        state_.stage = op_.stage == Stage::Idle ? std::string() : stage_word(op_.stage);
        state_.destination = op_.destination;
    }

    EditorSwitchAnswered base_answer(std::int64_t op, const char* outcome, const std::string& destination) {
        EditorSwitchAnswered a;
        a.op = op;
        a.outcome = outcome;
        a.destination = destination;
        const std::vector<load::ChoiceIntent> choices = authored();
        const std::string holding = host_.holder ? host_.holder() : std::string();
        for (const load::ChoiceIntent& c : choices) {
            a.choices.push_back(c.name);
            if (c.stem == holding) {
                a.active = c.name;
            }
        }
        return a;
    }

    EditorSwitchAnswered refused_answer(std::int64_t op, const std::string& destination, const std::string& why) {
        EditorSwitchAnswered a = base_answer(op, switch_outcome::kRefused, destination);
        a.detail = why;
        return a;
    }

    std::vector<load::ChoiceIntent> authored() const {
        std::vector<load::ChoiceIntent> out;
        if (!host_.choices) {
            return out;
        }
        for (const load::ChoiceIntent& c : host_.choices()) {
            if (c.role == host_.office) {
                out.push_back(c);
            }
        }
        return out;
    }

    static const load::ChoiceIntent* choice_named(const std::vector<load::ChoiceIntent>& choices,
                                                  const std::string& name) {
        for (const load::ChoiceIntent& c : choices) {
            if (c.name == name) {
                return &c;
            }
        }
        return nullptr;
    }

    static void add_unique(std::vector<std::string>& into, const std::string& one) {
        for (const std::string& e : into) {
            if (e == one) {
                return;
            }
        }
        into.push_back(one);
    }

    /// THE LOSSES, IN ONE SENTENCE: an answer's words are all a Terminal's maker reads of it.
    static std::string losses_said(const std::vector<std::string>& losses) {
        std::string out;
        for (const std::string& loss : losses) {
            out += (out.empty() ? "" : "; ") + loss;
        }
        return out;
    }

    static const char* stage_word(Stage s) {
        switch (s) {
        case Stage::Judging: return "judging";
        case Stage::AwaitingConsent: return "awaiting-confirmation";
        case Stage::Warming: return "warming";
        case Stage::Boundary: return "boundary";
        case Stage::Adopting: return "adopting";
        case Stage::Proving: return "proving";
        case Stage::Retiring: return "retiring";
        case Stage::Idle: break;
        }
        return "idle";
    }

    loom::WeaveId self() const { return self_id_; }

    EditorSwitchHost host_;
    loom::WeaveId self_id_{};
    Handle beat_;
    Op op_;
    std::int64_t next_op_ = 0;
    std::uint64_t correlations_ = 0;
    std::int64_t activations_ = 0;
    /// THE RETIRED EDITOR KEPT AFTER A FAILURE AFTER A COMMITMENT, by artifact, or empty.
    std::string retained_;
    /// THE LATEST SWITCH A NEWER REQUEST REPLACED WHILE IT AWAITED CONFIRMATION, and by which.
    struct Superseded {
        std::int64_t op = 0;
        std::int64_t by = 0;
    };
    Superseded superseded_;
};

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_EDITOR_SWITCH_HPP
