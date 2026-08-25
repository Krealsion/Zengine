// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_LOAD_EXECUTE_HPP
#define ZENGINE_WORKSHOP_LOAD_EXECUTE_HPP

// PERFORMING AN AUTHORED LOAD PLAN (LOAD-0).
//
// The plan DECLARES; this file PERFORMS. It owns ORCHESTRATION and adds no loader:
// a provider is mounted with PROV-0's `op::mount_provider`, a host is offered with
// OPH-0's `op::OperatorOffer`, and a weave is loaded by sending the Weave Manager
// an ordinary `zen.LoadWeave` -- the same three mechanisms Workshop's `main()`
// called by hand, in the same order, now driven from a file.
//
// ---- The execution law ---------------------------------------------------------
//
//     for artifact in AUTHORED ORDER:
//         if provider intent:  mount it
//         if weave intent:     offer this host's operator resolution
//                              load the weave
//                              withdraw the offer
//
// ---- ...AND THE OWNER OF THAT LAW OUTLIVES ONE STACK FRAME (BOOT-0) -------------
//
// THE LOOP ABOVE IS NOT A FUNCTION ANY MORE. Three of its four steps are
// synchronous host-native acts and the fourth is a CONVERSATION -- an ordinary
// `zen.LoadWeave` whose answer comes back several deliveries later -- so an executor
// written as a straight line had to turn the bus itself to hear its own answer. That
// is the one thing a semantic consumer must not do (FRIC-R2), and it was done here
// for exactly one reason: the continuation was a stack frame, and a stack frame
// cannot be put down and picked up again.
//
// So it is not one any more. `begin()` performs every transition it can know the
// answer to, issues the one request it cannot, and RETURNS TO THE HOST. The host
// turns the crank it already turns; the load's own correlated answer wakes
// `answered()`; and realization continues from where it stopped:
//
//     begin(plan)                       host loop                answered(...)
//         mount row 0's provider            drain/pump               withdraw the offer
//         mount row 1's provider            ...                      record the row
//         offer + send row 2's load         ...                      advance()
//         RETURN                            ...                      RETURN
//
// WHAT THAT COST, EXACTLY: the plan cursor, the row being built and the
// `op::OperatorOffer` became MEMBERS. The offer is the one that mattered -- OPH-0
// requires it to bracket the load, and its bracket used to be a `{ }` in `perform`.
// It is now a `std::optional` whose `reset()` is that same closing brace, said in
// the handler that learns the load settled.
//
// WHAT IT BOUGHT: the 64-turn dispatch fuse is GONE, not renamed. An owner that
// returns to its host has nothing to count. There is no `pump_pending`, no
// `drain_until_idle`, no `wait`, no sleep and no turn budget anywhere in this file,
// and a host that never called one would still realize its whole project.
//
// THIS IS NOT A SCHEDULER AND MUST NOT BECOME ONE. It holds ONE conversation and
// starts the next row only when that one has settled (see the serialization law
// below); it has no queue, no task, no future, no continuation object and no
// eligibility rule. What it has is a cursor into a list a person wrote.
//
// TWO ORDERINGS, AND THEY ARE DIFFERENT KINDS OF FACT.
//
// BETWEEN artifacts the order is AUTHORED POLICY. Nothing here infers that
// `zengine-timer`'s composition spends `zengine-operators-basic`'s primitives; a
// person wrote the rows down in the order they must happen. That is the whole V0
// dependency model and it is deliberately not a solver.
//
// WITHIN one artifact the order is SEMANTIC LAW and is not the plan's to state. A
// provider+consumer artifact -- `zengine-timer` is the only one that ships --
// VALIDATES the rule it is about to spend inside its own `create()` (CAT-0), and
// `create()` is several deliveries below the command that starts the load. So the
// contribution must be in the catalog before the artifact that needs it is built,
// and no authored order can be allowed to say otherwise. It is pinned here rather
// than in the file: a file that could say `weave, then provider` would be a file
// that could author a Timer whose semantics depend on which load was in flight.
//
// ---- One artifact is the atomic unit -------------------------------------------
//
// A record that mounted a provider and then failed to load its weave leaves a
// contribution in the host's catalog that no authored participant asked for -- and
// the artifact it came from is one nobody is running. So the record rolls back its
// OWN mount and stops.
//
// It rolls back exactly what THIS RECORD introduced, by the provider identity the
// artifact declared, which is what `Catalog::unmount` takes and what a `MountResult`
// carries. Earlier artifacts are NOT rolled back: a transaction across the whole
// plan is a bigger promise than this phase measured a need for, and the host is told
// which artifact stopped it and what still stands.
//
// ---- Three ways an operator offer can end, and they are not two ----------------
//
//   NotAConsumer     an ordinary weave. Not a fault, not a diagnostic; four of the
//                    five weaves this host loads are this and always were.
//   Offered          the artifact took the host's resolution for this one load.
//   a failed handoff the image DOES export a consumer surface and the handoff did
//                    not complete -- a version this host does not speak, a table it
//                    does not fill in, a refusal from the artifact's own `offer`.
//
// The third REFUSES THE ARTIFACT, and that is CAT-0's correction carried into the
// executor. A host-sensitive artifact loaded under a failed handoff is not the same
// as one that was never offered anything: `zengine-timer` falls back to a LOCAL
// catalog when nothing was offered, so loading it anyway would silently swap the
// process's semantic authority for the image's own copy -- a downgrade the host
// intended the opposite of, invisible in every answer until the two disagree.
//
//   NotOpened        the offer could not open the image at all. It is NOT refused
//                    here: the LOAD owns that sentence, and letting it run produces
//                    the loader's own words about a missing file rather than a
//                    second wording of them (which is also what makes a plan naming
//                    an artifact that is not on this disk refuse the way it should).
//
// ---- What this file does NOT do -------------------------------------------------
//
// It does not scan a directory, enumerate artifacts, resolve a dependency, choose an
// order, consult a version, reach a network, cache a resolution, or rewrite the plan
// it was handed. It does not infer a provider mount from a weave declaration or a
// weave load from a provider one -- an artifact that exports both surfaces and is
// asked for one gets one. And it does not unload or reload: LOAD-0 is initial and
// restart load intent, and the provider/reload interaction PROV-0 exposed is still
// open.

#include "load_plan.hpp"

#include "operator/catalog.hpp"
#include "operator/host_surface.hpp"
#include "operator/provider_host.hpp"

#include <zen/kernel/manager.hpp>
#include <zen/switchboard.hpp>
#include <zen/weave.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace zengine::workshop::load {

class PlanExecutor;

// ---- What the runtime made of one authored row --------------------------------

/// WHAT ONE ARTIFACT'S PARTICIPATION ACTUALLY PRODUCED.
///
/// THIS IS RESOLVED TRUTH AND IT LIVES ONLY HERE. The durable plan holds a stem, a
/// mode and a role; none of the fields below is in the file and none is ever written
/// to one. A provider identity is what the ARTIFACT declared about itself, a
/// contribution count is what it supplied, and a WeaveId is what this Kernel minted
/// this run -- three facts that would be a lie tomorrow.
///
/// It is kept rather than discarded because teardown and rollback both need it: an
/// unmount takes the provider's declared identity, which is knowable only after the
/// mount. Nothing here is a maker-facing surface.
///
/// SINCE INTR-1 IT IS ALSO PROJECTED, and the direction is worth reading: this stayed
/// exactly as it was and `workshop/arrangement.hpp` READS it. It gained no field, no
/// accessor and no maker-facing word, because a projection that needed its subject to
/// change shape would be a projection that had become an owner. What the projection
/// could NOT get from here is the authored mount MODE -- a resolved row does not know
/// whether its mount was an overlay -- so it pairs these rows with the authored plan
/// rather than asking this struct to start carrying intent (INTR-1's central law:
/// authored intent and resolved state are different truths).
struct ResolvedArtifact {
    std::string stem;

    bool provider_mounted = false;
    std::string provider;      ///< the identity the ARTIFACT declared, empty if not mounted
    std::size_t contributed = 0;

    /// How the operator offer around this artifact's load ended. `NotAConsumer` for
    /// every artifact with no weave intent, because none was made.
    op::OfferOutcome offer = op::OfferOutcome::NotAConsumer;

    bool weave_loaded = false;
    loom::WeaveId weave{};
    std::string role;
};

/// WHAT EXECUTING A WHOLE PLAN PRODUCED, or precisely where it stopped.
///
/// A refusal names WHICH ARTIFACT, WHICH PARTICIPATION STEP and WHY, and the `why`
/// is the deepest layer's own sentence -- the catalog's collision prose, the
/// loader's missing-file prose, the artifact's own refusal of an offer. There is no
/// error framework here and no error code: what a host prints is what the layer that
/// refused actually said, with the artifact and the step written in front of it.
struct Executed {
    bool ok = false;
    std::string refusal;
    std::vector<ResolvedArtifact> resolved;

    explicit operator bool() const noexcept { return ok; }
};

// ---- Where realization is, and what it has made of one authored row ------------

/// HOW FAR THE OWNER HAS GOT WITH THE WHOLE PLAN.
///
/// FIVE STATES, AND FOUR OF THEM ARE OBSERVABLE. `Advancing` is the inside of
/// `advance()` -- provider mounts and operator offers are synchronous and nothing
/// dispatches while one is running, so no participant can be looking when it holds.
/// It is spelled anyway because it is the loop's own condition, and a state a
/// function tests should have a name rather than be a bool nobody named.
///
/// THERE IS NO `Stopped`, NO `TimedOut` AND NO `Cancelled`. This owner never stops
/// waiting on a clock (BOOT-0 deleted the fuse that made it), and nothing in this
/// process can cancel a load it has already commanded. `Failed` is a refusal
/// somebody actually stated.
enum class Realization : std::uint8_t {
    Unstarted, ///< `begin` has not been called; the plan has not been touched
    Advancing, ///< inside `advance`: performing what is knowable now (transient)
    Loading,   ///< a `zen.LoadWeave` conversation is outstanding for the current row
    Complete,  ///< every authored row resolved
    Failed,    ///< a row refused; progression stopped and earlier rows still stand
};

/// WHAT REALIZATION HAS MADE OF ONE AUTHORED ROW.
///
/// FOUR STATES WITH FOUR OWNERS, and that is why there are four (BOOT-R0 §20):
///
///   `Authored`  the PLAN's -- a row a person wrote that this run has not reached.
///   `Loading`   THIS OWNER's, and only this owner's: it is "I opened a conversation
///               about this artifact and it has not settled". Nothing else in the
///               process can know it, which is exactly why it had no spelling before
///               realization survived its stack frame.
///   `Resolved`  this owner's record that every surface the row authored participated.
///   `Refused`   this owner's record of a refusal some layer below actually stated.
///
/// AND FOUR THAT WERE ASKED FOR AND REFUSED. `waiting` is the cursor's business, not
/// a row's; `building` has nothing that maps a build target to an artifact; `available`
/// has no preflight owner (an image is discovered to be unopenable by trying); and
/// `mounting` is not a state at all, because a provider mount is synchronous and there
/// is no instant at which anything could observe it. A token with no owner is a field
/// that goes stale in its first week.
enum class RowState : std::uint8_t { Authored, Loading, Resolved, Refused };

// ---- The weave that asks, and hears the answer --------------------------------

/// WHAT THE PLAN BOOTER HEARD about the load it last asked for, AND WHICH LOAD THAT
/// WAS.
///
/// "Failures are values": the Manager answers its ASKER with `zen.Result` or
/// `zen.Refused`. A root send carries no asker, so every one of those answers would
/// be addressed to nobody -- which is the defect the original boot weave was written
/// to end, and this is that weave with somewhere to put the answer.
///
/// ---- WHICH CONVERSATION (QR-9), AND WHOSE BOOKKEEPING THAT IS (FRIC-2) ---------
///
/// IT KNOWS WHICH ASK IT IS WAITING ON, and that is not a refinement. The three answer
/// shapes are a UNIVERSAL vocabulary: `zen.Result`, `zen.Ack` and `zen.Refused` derive
/// one schema everywhere, so any participant a host grants them may send one to any
/// weave that accepts them. Without an outstanding-ask record this struct answered the
/// mechanical question "did an answer-shaped message arrive?" and was read by everything
/// above it as if it had answered "did MY load settle?". The two differ by exactly one
/// unrelated sender -- and measured, that sender could report a WeaveId no Kernel ever
/// minted, refuse a load that had succeeded, and turn a missing artifact's refusal into
/// a success.
///
/// THE RECORD ITSELF IS NO LONGER THIS FILE'S (FRIC-2). It is `loom::AskBook`
/// (`zen/weave/ask_book.hpp`) -- the asker-side conversation record Loom now ships,
/// because the correlation-and-expected-respondent pair below was being rewritten
/// independently by every asker in this tree, this one included. What stays here is
/// what is genuinely about LOADING: `answered`, `refused`, `reason` and `weave` are
/// PAYLOAD SEMANTICS, owned by `PlanBooter`, and the book has never heard of a
/// `zen.Result`.
///
/// LOOM STATES THE OBLIGATION AS A STANDING RULE (`zen/weave/standard_shapes.hpp`): a
/// weave that accepts a standard reply shape matches each arrival against its own
/// outstanding requests BY CORRELATION AND BY BUS-STAMPED SENDER. `loom::relay` is that
/// wall for a weave relaying somebody else's answer; `loom::AskBook` is the same wall
/// for the participant that asked, and this struct is the small adapter that spends it.
///
/// ONE CONVERSATION, AND THE BOOK HOLDS EXACTLY THAT (QR-10). The executor asks for one
/// artifact at a time and waits for it, so this record tracks ONE current load --
/// `current_` -- and every question below is about that one. The book underneath can
/// hold several at once and is proved to, in Loom's own suite; that is not permission
/// for this file to start loading concurrently, which LOAD-0's authored order forbids.
///
/// IT USED TO ASK FOR ROOM IT COULD NOT SPEND. FRIC-2 gave this book four slots because
/// an expired fuse left its conversation open forever, so a later load had to be able to
/// open a fresh one beside the abandoned one -- and four of those refused the fifth load
/// by name. Nothing ever wanted those records: the wait they belonged to had returned,
/// its caller had stopped the plan, and no code in this host could resume, inspect or
/// settle one of them again. What the room bought was the accumulation, not a reader.
/// So the fuse now says so -- it FORGETS the ask it stopped waiting for -- and the book
/// is back to the one conversation this adapter genuinely has.
///
/// FORGETTING IS LOCAL AND CLAIMS NOTHING OF THE FAR END. Loom has no cancellation
/// vocabulary; nothing is sent, no `DeferredAnswer` is revoked, and the respondent still
/// holds whatever answer right it held. A late answer to a forgotten load is a true fact
/// about the world that this host is simply no longer a party to: it matches no record,
/// settles nothing, and cannot put one back.
///
/// CORRELATION IDENTIFIES; IT DOES NOT AUTHENTICATE (Loom's ANS-05). A correlation is a
/// number a sender chooses, so matching one proves the arrival NAMES the conversation
/// and never that it had any business answering it. What is load-bearing is the pair:
/// the sender is stamped by the BUS, and no participant can claim another's.
/// Authenticity itself stays exactly where it already lived -- the grant deciding who
/// may say `zen.Result` in this host at all, and Loom's own answer provenance -- and
/// nothing here is a substitute for either.
struct BootAnswers {
    bool answered = false;
    bool refused = false;
    std::string reason;   ///< the Manager's own words, when it refused
    std::uint64_t weave = 0;

    /// OPEN A CONVERSATION with `respondent`, and return the correlation the request
    /// must carry -- or 0 when no conversation could be opened. It CLEARS the previous
    /// answer -- the payload fields, not a book entry -- which is the point: everything
    /// read afterwards is about THIS ask.
    ///
    /// ZERO MEANS NO CONVERSATION WAS OPENED, and the caller must not send anything: a
    /// request this record cannot recognize the answer to is a load nobody could ever
    /// report on. Since QR-10 the book is empty at every call -- an answer closes the
    /// conversation and an expired fuse forgets it -- so what is left to fail is the
    /// respondent, which `loom::AskBook` refuses to record when it is not a valid weave.
    std::uint64_t ask(loom::WeaveId respondent) {
        answered = false;
        refused = false;
        reason.clear();
        weave = 0;
        const loom::AskOpened opened =
            book_.open(respondent, loom::LoadWeave::zen_name, loom::LoadWeave::zen_version);
        current_ = opened.id;
        return opened.correlation;
    }

    /// IS THIS ARRIVAL THE ANSWER TO THE CONVERSATION THIS RECORD IS WAITING ON? Both
    /// halves, and neither is sufficient alone: the correlation says WHICH conversation,
    /// the bus-stamped sender says the answer came from the weave that was asked.
    ///
    /// READ-ONLY. Asking is not closing, so a handler may look before it decides.
    bool settles(std::uint64_t correlation, loom::WeaveId from) const noexcept {
        return book_.is_settled_by(current_, correlation, from);
    }

    /// AN ANSWER CLOSED THE CONVERSATION. Said explicitly rather than inferred from
    /// `answered`, because the two are different facts: `answered` is WHAT the answer
    /// was, and this is that there is no longer one outstanding. It also makes a
    /// duplicate of the same answer inert, because the record it would have to close is
    /// gone.
    void settled() noexcept { close(); }

    /// THIS HOST STOPPED WAITING, WITH NO ANSWER AT ALL (QR-10) -- the other way a
    /// conversation leaves this record, and deliberately a different word from
    /// `settled()`, because a reader at the fuse must not see one that says an answer
    /// came.
    ///
    /// It is `loom::AskBook::forget` and only that: local, and local is all of it. The
    /// respondent was told nothing, its answer right is untouched, and if that answer
    /// ever arrives it will match no record here. Say this exactly when the owner has
    /// decided not to resume the wait -- stopping a wait and abandoning it are not the
    /// same act, and a caller that means to look again later must keep its record.
    void stopped_waiting() noexcept { close(); }

    /// IS A LOAD CONVERSATION STILL OUTSTANDING? This is the question a waiting caller
    /// asks -- never "was anything delivered this turn". It is about THIS load, which
    /// since QR-10 is the only thing the book can be holding.
    bool awaiting() const noexcept { return book_.waiting_on(current_); }

    /// The correlation of the outstanding conversation, or 0 when none is.
    std::uint64_t asking() const noexcept {
        const loom::PendingAsk* p = book_.find(current_);
        return p == nullptr ? 0 : p->correlation;
    }

    /// What this record has open -- at most the one load currently in flight.
    const loom::AskBook& book() const noexcept { return book_; }

private:
    /// ONE CONVERSATION, LEAVING BY ONE OF TWO DOORS. Written once because the local
    /// bookkeeping is identical and the FACT is not: `settled()` and `stopped_waiting()`
    /// are the two things that can be true, and a reader should have to pick one.
    void close() noexcept {
        (void)book_.forget(current_);
        current_ = 0;
    }

    /// The asker-side record itself, holding the ONE conversation this adapter has at a
    /// time -- the bound `loom::AskBook` requires its owner to state, and the honest
    /// number for an executor that asks for one artifact and waits for it. It refuses a
    /// second rather than shedding the first, so were this file ever to open one while
    /// another is outstanding it would be told, not quietly obliged.
    ///
    /// ITS CORRELATIONS ARE MONOTONIC AND LOCAL TO IT. They collide with nobody else's
    /// numbering, and forgetting a conversation does not hand its number back -- which
    /// is what keeps a late answer to a forgotten load from settling the next one.
    loom::AskBook book_{1};
    /// WHICH of the book's conversations is the load currently in flight. Not a second
    /// copy of the conversation -- the book owns membership, this owns "which one is
    /// mine right now".
    std::uint64_t current_ = 0;
};

/// The plan booter's own state. It holds nothing: what it hears goes into the
/// `BootAnswers` the host owns, because the executor and the host both need to read
/// it and a weave's state is not a shared surface.
struct BootState {
    std::int64_t asked = 0;
    ZEN_EXPOSE();
    ZEN_SHAPE(BootState, 1, ZEN_FIELD(asked));
};

/// THE WEAVE THAT ASKS THE WEAVE MANAGER TO LOAD A PLAN'S ARTIFACTS.
///
/// It holds the reach to the Manager -- which is kernel reach, transitively, and is
/// the dangerous grant in a Zengine host. THE HOST WRITES THAT GRANT, not this file:
/// mounting the booter is the host's act of saying "this weave, and only this weave,
/// may command lifecycle here", and the executor is handed the resulting WeaveId. An
/// executor that mounted its own weave with its own grant would be an orchestration
/// layer minting authority for itself.
///
/// TWO RESPONSIBILITIES, TWO GRANTS, unchanged from the boot weave this replaces:
/// this one OPERATES and Workshop's own weave AUTHORS, and neither holds the other's
/// reach.
///
/// ---- IT IS THE BRIDGE, NOT THE STATE MACHINE (BOOT-0) --------------------------
///
/// Since realization stopped being a stack frame, an arriving answer has somewhere to
/// go: the host-side owner whose unfinished row it settles. This weave's whole added
/// responsibility is the last line of each handler -- hear the answer, check that it
/// settles THIS booter's conversation, put the payload where the owner reads it, and
/// hand the fact over.
///
/// IT OWNS NOTHING IT HANDS OVER. No catalog, no operator offer, no cursor, no plan,
/// no order and no decision about what happens next; it does not turn the bus and
/// could not (it is inside a delivery). Everything below `wakes()` is the owner's.
/// The split is the one this file already had -- `PlanBooter` speaks and hears,
/// `PlanExecutor` decides -- and BOOT-0 changed only WHEN the second half runs.
///
/// ⚠ THE BUS OUTLIVES THE OWNER, so the pointer is nullable and the OWNER clears it.
/// A registered weave lives as long as the `Switchboard`, and the `Switchboard` in a
/// Zengine host is declared before the catalog and the Kernel -- while the realization
/// owner must be destroyed BEFORE them, because it holds an `op::OperatorOffer` into
/// an artifact image. So the two are wired in `PlanExecutor`'s constructor and unwired
/// in its destructor; nothing else may call `wakes`.
class PlanBooter : public loom::WeaveBase<PlanBooter, BootState,
                                          loom::Accept<loom::Result, loom::Ack, loom::Refused>,
                                          loom::Emit<loom::LoadWeave>> {
public:
    explicit PlanBooter(BootAnswers& answers) : answers_(&answers) {}

    /// WHOSE UNFINISHED WORK AN ANSWER TO THIS BOOTER WAKES -- or nobody. Called
    /// exactly twice, both times by `PlanExecutor` (its constructor and its
    /// destructor), so a host never writes this and cannot forget the second call.
    void wakes(PlanExecutor* owner) noexcept { owner_ = owner; }

    /// WHICH WEAVE THIS IS. The executor sends its `zen.LoadWeave` AS this booter --
    /// that is what makes the Manager's answer come back to something that can hear
    /// it -- so it needs the id the host minted, and this is where it reads it rather
    /// than being handed a second copy that could name a different weave.
    loom::WeaveId speaker() const noexcept { return self_; }

    void on(const loom::Result& r, loom::Mail& mail) {
        if (!settles(mail)) {
            return;
        }
        answers_->answered = true;
        answers_->refused = false;
        // THE WEAVE ID AS TEXT, because `zen.Result` carries text: the protocols that
        // use it move values as exact, locale-free text. Parsed defensively -- a
        // Result this host cannot read is still an ANSWER, and the load still
        // happened; what it is not is a WeaveId anybody may quote.
        answers_->weave = 0;
        try {
            answers_->weave = static_cast<std::uint64_t>(std::stoull(r.value));
        } catch (...) {
            answers_->weave = 0;
        }
        answers_->settled();
        wake();
    }

    void on(const loom::Ack&, loom::Mail& mail) {
        if (!settles(mail)) {
            return;
        }
        answers_->answered = true;
        answers_->refused = false;
        answers_->settled();
        wake();
    }

    void on(const loom::Refused& r, loom::Mail& mail) {
        if (!settles(mail)) {
            return;
        }
        answers_->answered = true;
        answers_->refused = true;
        answers_->reason = r.reason;
        answers_->settled();
        wake();
    }

private:
    /// HAND THE SETTLED FACT OVER -- after `settled()`, so the owner it wakes finds a
    /// conversation that is closed and a payload that is this answer's. Defined out of
    /// line at the bottom of this file, because the owner is not declared yet.
    void wake();

    /// THE ONE DOOR ALL THREE ANSWER SHAPES PASS THROUGH (QR-9). Written once because
    /// the three are one conversation's three possible endings, and a wall applied to
    /// two of them is not a wall: the measured defect had an unrelated `zen.Refused`
    /// killing a load that had in fact succeeded, which is the arm that an eye kept on
    /// `zen.Result` alone would have left wide open.
    ///
    /// AN ARRIVAL THAT DOES NOT SETTLE THIS LOAD IS LEFT ALONE -- not refused, not
    /// recorded, not complained about. The bus admitted it and it may be a perfectly
    /// legitimate answer to somebody; all this weave can truthfully say is that it is
    /// not the answer to the conversation this booter opened, and saying more would be
    /// an adapter judging traffic it was never part of.
    ///
    /// TWO FACTS, ONE OF THEM LOOM'S TO KEEP. `mail.correlation()` and `mail.sender()`
    /// are what this weave hands over; the record they are checked against is
    /// `loom::AskBook`'s, through `BootAnswers`. What is left in this file is the part
    /// that is about loading: which shape means success, which means refusal, and what
    /// a `zen.Result`'s text is.
    bool settles(const loom::Mail& mail) const {
        return answers_->settles(mail.correlation(), mail.sender());
    }

    BootAnswers* answers_;
    /// THE HOST-SIDE OWNER THIS BOOTER SPEAKS FOR, or null when none is wired.
    /// Null is an ordinary state and not a fault: a rig that only wants the payload
    /// semantics mounts this weave and never builds an executor at all.
    PlanExecutor* owner_ = nullptr;
};


// ---- The realization owner -----------------------------------------------------

/// PERFORM AN AUTHORED PLAN AGAINST ONE HOST'S RUNTIME, AND KEEP PERFORMING IT ACROSS
/// THE HOST'S OWN TURNS.
///
/// Everything it needs is handed to it and nothing is global: the catalog it mounts
/// into, the operator surface it offers, the bus it sends on, the booter that speaks
/// for it, the Manager it speaks to, the answers that come back, and the one rule
/// that spells an artifact stem as a file. A second Zengine host in this process
/// would own a second executor, correctly, and neither would be "the" executor.
///
/// ---- IT IS HOST-SIDE, AND THAT IS A LIFETIME CLAIM BEFORE IT IS ANYTHING ELSE ---
///
/// The obvious way to make an object event-driven in this system is to make it a
/// weave. This one must not be, and the reason is not authority -- an in-process weave
/// would hold the same `op::Catalog&` this does. It is that a registered weave is
/// owned by the `Switchboard`, and a Zengine host declares its bus BEFORE its catalog
/// and its Kernel so that reverse-order destruction takes the Kernel and its artifacts
/// down first. This object holds an `op::OperatorOffer` -- a share of an artifact
/// image -- and a cursor into a plan whose provider identities index that same
/// catalog. Moving it into the bus would move its destruction to AFTER the catalog it
/// unmounts from and AFTER the images it holds shares of. So it stays a local of the
/// host's `main`, declared after the Kernel, exactly as it always was.
///
/// The second reason is the one BOOT-R0 measured: two of the four steps in a plan row
/// have no message form and cannot cheaply acquire one. `op::mount_provider` needs a
/// `Catalog&`, and an `op::OperatorOffer` IS A LIFETIME rather than an operation --
/// there is no way to say a C++ object's lifetime in a message without inventing a
/// handle, an owner for the handle and a rule for a holder that dies mid-offer. That
/// is a generic host-action service, and PROV-0 kept "which powers are in force here"
/// as the host's own decision on purpose.
///
/// ---- WHAT IT DOES NOT CONTAIN ---------------------------------------------------
///
/// No `pump_pending`, no `drain_until_idle`, no `wait`, no `sleep`, no predicate loop,
/// no turn counter and no dispatch budget -- directly or behind a helper. THE HOST
/// ADVANCES LOOM; THIS DECIDES WHAT THE FACTS MEAN. If it ever needs one of those
/// words again, the thing that actually happened is that a fact lost its owner.
class PlanExecutor {
public:
    /// How a host spells an artifact stem as a file on this platform. THE HOST OWNS
    /// THIS RULE and the plan does not: a directory, a separator and `.so`/`.dll` are
    /// deployment facts, and keeping them here is what makes ONE authored plan legal
    /// on Linux and on Windows with no platform field, no suffix and no locator.
    using ArtifactPath = std::function<std::string(const std::string& stem)>;

    /// REALIZATION REACHED A TERMINAL STATE -- called ONCE, from inside whatever
    /// delivery settled the last row, with what the whole plan produced.
    ///
    /// IT IS A NOTICE AND NOT A POLICY. What a host does about a refused project --
    /// print it, end the process, carry on with a partial arrangement -- is the
    /// HOST's decision and is written in the host, which is the whole reason this is
    /// a hook rather than a `quit` this object sets. This owner has no opinion about
    /// process lifetime and never will: completion is a fact about realization.
    ///
    /// EMPTY IS ORDINARY. A caller that watches the state itself -- a test driving
    /// the host explicitly -- passes nothing and reads `outcome()` when it likes.
    using Settled = std::function<void(const Executed&)>;

    /// The booter is taken as an OBJECT rather than as an id, because this owner needs
    /// both halves of it: the WeaveId to send as (`speaker()`), and the participant
    /// itself, to wire the answer path in and out again.
    PlanExecutor(loom::Switchboard& bus, op::Catalog& catalog,
                 const op::OperatorHostSurface& operators, PlanBooter& voice,
                 loom::WeaveId manager, BootAnswers& answers, ArtifactPath path_of,
                 Settled settled = Settled())
        : bus_(&bus), catalog_(&catalog), operators_(&operators), voice_(&voice),
          manager_(manager), answers_(&answers), path_of_(std::move(path_of)),
          settled_(std::move(settled)) {
        voice_->wakes(this);
    }

    PlanExecutor(const PlanExecutor&) = delete;
    PlanExecutor& operator=(const PlanExecutor&) = delete;
    PlanExecutor(PlanExecutor&&) = delete;
    PlanExecutor& operator=(PlanExecutor&&) = delete;

    /// RELEASE WHAT IS STILL OUTSTANDING, IN THE ORDER THAT IS SAFE (BOOT-0).
    ///
    /// A persistent owner can be destroyed mid-row, which a stack frame could not be,
    /// so the three things a row can be holding are put down here explicitly rather
    /// than left to member order:
    ///
    ///   THE BOOTER'S POINTER TO THIS. First, because the `Switchboard` outlives this
    ///     object and a weave holding a dead owner is the one hazard this wiring adds.
    ///   THE OPERATOR OFFER. `~OperatorOffer` withdraws unconditionally and releases
    ///     its image share; that share must go before the Kernel unloads the artifact,
    ///     which the host's declaration order guarantees by putting this object after
    ///     the Kernel. Written out rather than left implicit, because the member order
    ///     that would do it anyway is not where a reader looks for a lifetime claim.
    ///   THE ASK. A conversation this host will never resume is FORGOTTEN (QR-10) --
    ///     genuinely stopping to care, which is the only thing that legitimately closes
    ///     an unanswered ask. Local and only local: nothing is sent, the Manager's
    ///     answer right is untouched, and a late answer matches no record here.
    ///
    /// The catalog is deliberately NOT unwound. What this executor mounted stays
    /// mounted; the host's own destruction order takes the Kernel and its artifacts
    /// down first and the catalog last, and an owner that unmounted here would be
    /// racing that order for no reader's benefit.
    ~PlanExecutor() {
        if (voice_ != nullptr) {
            voice_->wakes(nullptr);
        }
        offer_.reset();
        if (answers_->awaiting()) {
            answers_->stopped_waiting();
        }
    }

    // ---- Beginning, and continuing ---------------------------------------------

    /// BEGIN REALIZING `plan`, AND RETURN.
    ///
    /// It performs every transition it can already know the answer to -- a run of
    /// provider-only rows is mounted before this returns, synchronously, because
    /// nothing is owed by anybody -- and stops at the first row that needs a fact
    /// this process does not have yet. Then it returns to its caller, who is expected
    /// to go and be a host.
    ///
    /// THE PLAN IS TAKEN BY VALUE. A persistent owner outlives the expression that
    /// started it, so a reference would be a dangling one the first time a caller
    /// wrote `begin(plan_of({...}))`. This copy is also the AUTHORED half of what the
    /// arrangement projection reads (`plan()`), which is what keeps authored intent
    /// and resolved state one owner's two answers rather than two owners that must
    /// agree.
    ///
    /// ONE PLAN PER OWNER. Calling this twice would abandon a cursor and a possibly
    /// outstanding conversation, so the second call is refused and says nothing about
    /// the first: an owner is begun once, and a host that wants a second arrangement
    /// builds a second owner.
    void begin(LoadPlan plan) {
        if (state_ != Realization::Unstarted) {
            return;
        }
        plan_ = std::move(plan);
        state_ = Realization::Advancing;
        advance();
    }

    /// THE LOAD CONVERSATION THIS OWNER OPENED HAS SETTLED -- called by `PlanBooter`
    /// from inside the delivery that settled it, never by a host.
    ///
    /// IT READS THE PAYLOAD THE BOOTER JUST WROTE and nothing else: whether the
    /// Manager answered or refused, its own words when it refused, and the WeaveId it
    /// minted. The wall that decided this arrival was the answer to THIS conversation
    /// -- correlation AND bus-stamped respondent -- was spent one frame up, in the
    /// weave that is party to it (QR-9, FRIC-2). Nothing here re-decides it, and
    /// nothing here would be able to.
    ///
    /// AN ARRIVAL WITH NOTHING OUTSTANDING IS INERT. A duplicate of an answer already
    /// settled, or an answer that outlived the row it belonged to, finds no `Loading`
    /// state and advances nothing.
    void answered() {
        if (state_ != Realization::Loading) {
            return;
        }
        // THE OFFER'S CUSTODY ENDS AT THE SAME SEMANTIC POINT IT ALWAYS DID (OPH-0):
        // after the load has happened and before anything else does. It used to be the
        // closing brace of `perform`'s inner scope, reached on every path; it is this
        // line now, reached on every path, and the withdrawal is still unconditional
        // and still `~OperatorOffer`'s.
        //
        // BEFORE THE ROW IS JUDGED, AND BEFORE THE NEXT ROW BEGINS. A row that starts
        // while the previous row's temporary offer is still standing would be a row
        // loaded under a handoff nobody authored for it.
        offer_.reset();
        if (answers_->refused) {
            fail("weave load refused: " + answers_->reason);
            return;
        }
        current_.weave_loaded = true;
        current_.weave = loom::WeaveId{answers_->weave};
        settle_row();
        state_ = Realization::Advancing;
        advance();
    }

    // ---- What is true right now --------------------------------------------------

    /// The authored intent this owner is realizing -- empty until `begin`.
    const LoadPlan& plan() const noexcept { return plan_; }

    /// How far realization has got with the plan as a whole.
    Realization state() const noexcept { return state_; }

    /// WHICH AUTHORED ROW REALIZATION IS AT: the index of the row in flight, or the
    /// row that refused, or `plan().artifacts.size()` once every row has resolved.
    std::size_t position() const noexcept { return cursor_; }

    /// WHAT REALIZATION HAS MADE OF THE AUTHORED ROW NAMED `stem`.
    ///
    /// KEYED BY STEM BECAUSE THE PLAN'S OWN LAW MADE THE STEM A KEY -- `check_plan`
    /// refuses a file naming one artifact twice, so this is exact rather than
    /// best-effort, and a projection pairing authored rows with this needs no index
    /// agreement with anything.
    ///
    /// DERIVED, NOT STORED. There is no per-row status table here and there must not
    /// be one: the cursor, the resolved list and the one in-flight row already say
    /// all four states between them, and a second record of the same fact is the
    /// mirror that goes stale.
    RowState state_of(const std::string& stem) const noexcept {
        for (const ResolvedArtifact& done : resolved_) {
            if (done.stem == stem) {
                return RowState::Resolved;
            }
        }
        if (current_.stem == stem) {
            if (state_ == Realization::Loading) {
                return RowState::Loading;
            }
            if (state_ == Realization::Failed) {
                return RowState::Refused;
            }
        }
        return RowState::Authored;
    }

    /// The correlation of the load conversation currently outstanding, or 0.
    std::uint64_t asking() const noexcept { return answers_->asking(); }

    /// What this executor has actually put into the runtime, in the order it did.
    const std::vector<ResolvedArtifact>& resolved() const noexcept { return resolved_; }

    /// WHICH ARTIFACT STOPPED THE PLAN AND WHY, in the deepest layer's own words.
    /// Empty unless `state() == Realization::Failed`.
    const std::string& refusal() const noexcept { return refusal_; }

    /// WHAT THE WHOLE PLAN HAS PRODUCED SO FAR, as one value.
    ///
    /// `ok` IS COMPLETION AND NOT ABSENCE OF FAILURE: a plan still loading its third
    /// row has refused nothing and has not finished either, and answering `true`
    /// there would be this owner claiming an arrangement it has not got.
    Executed outcome() const {
        Executed out;
        out.ok = state_ == Realization::Complete;
        out.refusal = refusal_;
        out.resolved = resolved_;
        return out;
    }

    /// UNMOUNT ONE RECORD'S PROVIDER CONTRIBUTION, and only that record's.
    ///
    /// `Catalog::unmount` drops the contributions and only then the custody, so
    /// nothing that can call into the artifact's image outlives it -- by refcount
    /// rather than by any statement ordering here (PROV-0).
    bool unmount(const ResolvedArtifact& done) {
        if (!done.provider_mounted || done.provider.empty()) {
            return false;
        }
        return catalog_->unmount(done.provider);
    }

private:
    /// PERFORM EVERYTHING KNOWABLE NOW, THEN RETURN.
    ///
    /// The loop is over AUTHORED ROWS and not over anything else. It is not a
    /// scheduler and there is nothing to schedule: every iteration either finishes a
    /// row with no external fact outstanding, or opens exactly one conversation and
    /// leaves. There is no eligibility rule, no dependency graph, no readiness
    /// predicate, no retry and no parallelism -- LOAD-0's authored order is the whole
    /// policy, and persistence is deliberately NOT read as permission for concurrency.
    ///
    /// STRICT SERIALIZATION IS PRESERVED, and it is what the tests assert: row N+1's
    /// first step does not begin until row N has fully settled. The one measured
    /// inter-row constraint -- an overlay must be authored after the row it covers --
    /// is a CATALOG-STATE constraint, and only serialization makes it deterministic.
    void advance() {
        while (state_ == Realization::Advancing && cursor_ < plan_.artifacts.size()) {
            const ArtifactIntent& artifact = plan_.artifacts[cursor_];
            current_ = ResolvedArtifact{};
            current_.stem = artifact.stem;
            const std::string path = path_of_(artifact.stem);

            if (artifact.provider.has_value()) {
                const op::MountResult mounted =
                    op::mount_provider(*catalog_, path, artifact.provider->mode);
                if (!mounted.ok) {
                    // THE MOUNT'S OWN WORDS. `mount_provider` already says whether the
                    // file was there, whether it exports a provider surface, which
                    // version it speaks, and who already supplies a colliding power; a
                    // second sentence written here would be a second copy of a
                    // judgement this file does not own.
                    fail("provider mount refused: " + mounted.reason);
                    return;
                }
                current_.provider_mounted = true;
                current_.provider = mounted.provider;
                current_.contributed = mounted.contributed;
            }

            if (!artifact.weave.has_value()) {
                // A PROVIDER-ONLY ROW IS COMPLETE THE INSTANT ITS MOUNT RETURNS.
                // Nothing is owed, nobody was asked, and the row settles here without
                // the host having turned anything.
                settle_row();
                continue;
            }
            current_.role = artifact.weave->role;

            // THE OFFER GOES UP BEFORE THE COMMAND IS SENT, which is OPH-0's law and
            // is not relaxed here: a consumer's first legitimate need is inside
            // `create()` and no host can get between the Kernel and a constructor. It
            // comes down in `answered()`, several host turns later.
            offer_.emplace(*operators_, path);
            current_.offer = offer_->outcome();
            if (current_.offer == op::OfferOutcome::VersionMismatch) {
                // A BROKEN HANDOFF IS NOT "NO HOST INTENDED". See the header.
                const std::string why = "operator handoff refused: " + offer_->reason();
                offer_.reset();
                fail(why);
                return;
            }

            const std::uint64_t correlation = answers_->ask(manager_);
            if (correlation == 0) {
                // NO CONVERSATION, SO NO COMMAND -- and this is the guard, not a
                // leftover. Sending anyway would put a `zen.LoadWeave` on the bus
                // whose answer this host has no record to recognize, and this owner
                // would then sit in `Loading` forever waiting to be woken by a fact it
                // could not identify.
                offer_.reset();
                fail("weave load refused: no load conversation could be opened with the "
                     "weave this host was given as its Weave Manager; no load was "
                     "commanded");
                return;
            }
            const loom::WeaveId booter = voice_->speaker();
            bus_->send_as(booter, manager_,
                          loom::Message(loom::to_value(loom::LoadWeave{artifact.stem, path,
                                                                       artifact.weave->role}),
                                        booter, booter, correlation));
            // ---- THE BOUNDARY ---------------------------------------------------
            //
            // The command is on the queue and nothing in this process has delivered it
            // yet. What happens next is the HOST's turn, not this object's: it returns,
            // its caller goes back to being a host, and the Manager's own answer --
            // through the control door, which is where a loaded weave is ACTIVATED --
            // arrives at `PlanBooter` and wakes `answered()`.
            //
            // Turning the crank here instead is the whole defect BOOT-0 removed, and
            // it would be just as wrong spelled `drain`, `wait`, `settle` or `until`.
            state_ = Realization::Loading;
            return;
        }
        if (state_ == Realization::Advancing) {
            complete();
        }
    }

    /// THE CURRENT ROW PARTICIPATED IN FULL. Kept in authored order, which is what
    /// makes a later reversal walkable backwards (LOAD-0, BOOT-R0 §23).
    void settle_row() {
        resolved_.push_back(std::move(current_));
        current_ = ResolvedArtifact{};
        ++cursor_;
    }

    /// THIS ROW REFUSED, SO THE PLAN STOPS -- and the runtime is put back the way the
    /// row found it before anybody is told.
    ///
    /// ROLLS BACK EXACTLY WHAT THIS ROW INTRODUCED, by the provider identity the
    /// artifact declared. Earlier artifacts are NOT rolled back: a transaction across
    /// the whole plan is a bigger promise than this phase measured a need for, and the
    /// host is told which artifact stopped it and what still stands.
    ///
    /// STOPS RATHER THAN SKIPS. A host that carried on past a failed artifact would be
    /// running a project nobody authored while reporting the one that was asked for --
    /// which is the shape of failure durable intent exists to end.
    ///
    /// `current_` IS DELIBERATELY LEFT STANDING. It is how `state_of` answers
    /// `Refused` for the one row that did, and it names no runtime resource any more:
    /// the mount is unwound above and the offer was withdrawn by the caller.
    void fail(const std::string& why) {
        (void)unmount(current_);
        refusal_ = "artifact '" + current_.stem + "': " + why;
        state_ = Realization::Failed;
        announce();
    }

    /// EVERY AUTHORED ROW RESOLVED. That is a fact about realization and about nothing
    /// else -- it does not stop the bus, end the host or claim the process is done.
    void complete() {
        state_ = Realization::Complete;
        announce();
    }

    /// TELL THE HOST ONCE, IF IT ASKED TO BE TOLD.
    void announce() {
        if (settled_) {
            settled_(outcome());
        }
    }

    loom::Switchboard* bus_;
    op::Catalog* catalog_;
    const op::OperatorHostSurface* operators_;
    /// THE PARTICIPANT THAT SPEAKS AND HEARS FOR THIS OWNER. Never null: it is wired
    /// in the constructor and unwired in the destructor, and the host cannot get a
    /// half-built pair because the constructor is the only place the link is made.
    PlanBooter* voice_;
    loom::WeaveId manager_;
    BootAnswers* answers_;
    ArtifactPath path_of_;
    Settled settled_;

    // ---- what used to be a stack frame -------------------------------------------

    /// THE AUTHORED INTENT, OWNED. The durable half of what this owner answers for.
    LoadPlan plan_;
    /// WHICH AUTHORED ROW IS BEING REALIZED. It was `run()`'s loop index.
    std::size_t cursor_ = 0;
    /// THE ROW BEING BUILT. It was `perform()`'s `ResolvedArtifact& done`.
    ResolvedArtifact current_;
    /// THE OFFER AROUND THE CURRENT LOAD -- the one thing here whose LIFETIME, rather
    /// than whose value, was the stack frame.
    ///
    /// `op::OperatorOffer` is neither copyable nor movable, deliberately and for
    /// OPH-0's reasons, and it is not made either here: `std::optional` constructs it
    /// in place and `reset()` runs the same destructor the closing brace used to run.
    /// Empty outside a load, exactly as the host held nothing outside the old scope.
    std::optional<op::OperatorOffer> offer_;
    /// WHERE THE WHOLE PLAN IS.
    Realization state_ = Realization::Unstarted;
    /// WHICH ARTIFACT STOPPED IT AND WHY.
    std::string refusal_;
    /// What this executor has put into the runtime, in the order it did.
    std::vector<ResolvedArtifact> resolved_;
};

/// DEFINED HERE because the owner it hands the fact to is declared above. One call,
/// no branch of its own: whether this answer means anything to the plan is the
/// owner's question, and it already knows whether it has a row in flight.
inline void PlanBooter::wake() {
    if (owner_ != nullptr) {
        owner_->answered();
    }
}

} // namespace zengine::workshop::load

#endif // ZENGINE_WORKSHOP_LOAD_EXECUTE_HPP
