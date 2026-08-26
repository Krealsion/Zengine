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
//         if the host says this row is WAITING ON THE MAKER:  STOP HERE
//         if provider intent:  mount it
//         if weave intent:     offer this host's operator resolution
//                              load the weave
//                              withdraw the offer
//
// ---- ...AND ONE ROW MAY BE PERFORMED LATER, BECAUSE A MAKER ASKED (BLD-1) --------
//
// The first line above is the whole of what BLD-1 added to the law, and the second
// half of it is `realize(stem)`: a door that performs THE ROW REALIZATION IS WAITING
// ON, with the same three steps in the same order, at a moment a maker chose.
//
//   WHY A ROW MAY BE WAITING AT ALL. An artifact a project intends to run may not be
//   on this disk yet, because this project is where it gets built. Refusing the plan
//   over it -- which is what "stops rather than skips" would do -- makes the one
//   Workshop a maker could have built it in refuse to start. So the HOST is asked, per
//   row, whether that row is waiting on the maker (`AwaitingBuild`), and this owner
//   stops there. It never learns why, and it never asks twice.
//
//   WHAT IT IS NOT. Not build-on-missing: nothing here starts, requests or knows about
//   a build. Not a retry: a waiting row waits forever unless it is asked for. Not a
//   scheduler: `realize` is refused outright while anything else is in flight. And not
//   hot reload -- an artifact already resolved is refused in words, because BLD-1 does
//   not unload, replace or migrate anything and a second load of a live artifact would
//   be pretending otherwise.
//
// ---- ...AND A WAITING ROW IS A BARRIER, NOT A HOLE (BLD-1a) ---------------------
//
// BLD-1 shipped that first line as `record it, carry on`, and carrying on is the one
// thing it may not do. AUTHORED PLAN ORDER IS REALIZATION ORDER -- that is LOAD-0's
// whole dependency model, the reason the file has no `after:` field and no solver --
// and a walk that steps over a row it could not perform has quietly replaced it with
// ELIGIBILITY ORDER: whatever happened to be on disk goes first.
//
// That is not a smaller promise, it is a different one, and it is wrong in a way the
// plan cannot express. An overlay row authored BEFORE the ordinary provider it covers
// is a bad plan, and the catalog says so -- the overlay installs over nothing and the
// ordinary mount then collides with it. Skip that row because its artifact is not
// built yet and the ordinary mount goes first, so the overlay arrives LAST and is
// VALID. The absence of a file repaired an authored order, and a maker who then builds
// the artifact gets a working arrangement their plan does not describe.
//
// So realization STOPS at the first row it cannot perform:
//
//     artifact absent, and this project can produce it
//         -> the row is PENDING, the owner is `Waiting`, and it RETURNS TO THE HOST
//         -> the row after it is NOT reached, NOT mounted, NOT loaded, NOT asked about
//
//     the maker builds it and asks
//         -> `realize(stem)` performs THAT row, by the ordinary three steps
//         -> when it settles, the frontier moves ON BY ONE and the walk resumes
//
//   BUILDABILITY IS PERMISSION TO WAIT, NOT PERMISSION TO REORDER. What the host's
//   predicate buys is that a missing artifact this project builds does not REFUSE the
//   Workshop a maker would have built it in. It buys nothing about order, because
//   order was never realization's to decide.
//
//   THE OWNER MAY WAIT ACROSS TIME; AUTHORED ORDER MUST NOT MOVE AROUND WHAT IT IS
//   WAITING FOR. BOOT-0 made the owner outlive its stack frame precisely so an
//   unfinished row could stay unfinished without anything having to be scheduled. A
//   row waiting on a maker is that same shape with a slower answer, and it wants the
//   same treatment: hold the frontier, return to the host, resume when the answer
//   comes. There is no queue of eligible rows here, no readiness scan, no dependency
//   graph and no second pass -- ONE frontier, and it is `cursor_`.
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

#include "builder/vocabulary.hpp"

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
    /// THE ONE AUTHORED ROW REALIZATION STOPPED AT, or empty when it stopped at none
    /// (BLD-1, corrected by BLD-1a) -- because the host said that row is waiting on the
    /// maker.
    ///
    /// ⚠ IT IS ONE NAME AND NOT A LIST, AND THE SHAPE IS THE LAW. BLD-1 kept a vector
    /// here because a waiting row was stepped over, so a plan could accumulate several.
    /// A waiting row is a BARRIER now: the walk stops at the FIRST row it cannot
    /// perform, so at most one row can be waiting at any instant and a second slot
    /// could only ever hold a claim that later rows had leapfrogged an earlier one.
    ///
    /// IT IS NOT A FAILURE AND IT IS NOT COMPLETION EITHER. `ok` is false while this is
    /// set and `refusal` is empty -- nothing refused anything, and the arrangement the
    /// plan describes is not standing yet.
    std::string waiting_on;

    explicit operator bool() const noexcept { return ok; }
};

// ---- Where realization is, and what it has made of one authored row ------------

/// HOW FAR THE OWNER HAS GOT WITH THE WHOLE PLAN.
///
/// SIX STATES, AND FIVE OF THEM ARE OBSERVABLE. `Advancing` is the inside of
/// `advance()` -- provider mounts and operator offers are synchronous and nothing
/// dispatches while one is running, so no participant can be looking when it holds.
/// It is spelled anyway because it is the loop's own condition, and a state a
/// function tests should have a name rather than be a bool nobody named.
///
/// `Waiting` IS BLD-1a's, AND IT IS WHAT `Complete` USED TO BE LYING ABOUT. BLD-1 let
/// a plan whose rows included some that were never performed answer `Complete`, so
/// `outcome().ok` could be read as *the whole arrangement is live* while an authored
/// artifact had not been mounted, loaded or even looked at. Two facts were one token.
/// Now: `Complete` means EVERY authored row settled, and a plan stopped at a row the
/// host says is waiting on the maker is `Waiting` -- unfinished, and nothing refused.
///
/// THE TWO SUBJECTS ARE DIFFERENT, AND SO ARE THE TWO WORDS. The OWNER is `Waiting`;
/// the ROW it is waiting on is `pending` (`RowState::Pending`, and the maker-facing
/// token in the Project pane). One is about realization, the other about one artifact,
/// and collapsing them would leave no way to say *which* row.
///
/// THERE IS NO `Stopped`, NO `TimedOut` AND NO `Cancelled`. This owner never stops
/// waiting on a clock (BOOT-0 deleted the fuse that made it), and nothing in this
/// process can cancel a load it has already commanded. `Failed` is a refusal
/// somebody actually stated.
enum class Realization : std::uint8_t {
    Unstarted, ///< `begin` has not been called; the plan has not been touched
    Advancing, ///< inside `advance`: performing what is knowable now (transient)
    Loading,   ///< a `zen.LoadWeave` conversation is outstanding for the current row
    Waiting,   ///< the frontier row is waiting on the maker; the walk stopped there
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
///   `Pending`   THIS OWNER's too, and BLD-1's: "I reached this row, and the host told
///               me it is waiting on the maker." It is not `Authored` (that is a row
///               nothing has looked at) and it is not `Refused` (nothing refused
///               anything). See `AwaitingBuild` below for what the host is answering
///               and why realization cannot answer it itself.
///
///               ⚠ SINCE BLD-1a IT IS ALSO A BARRIER, and at most ONE row can hold
///               it: it is exactly the row `cursor_` is on while the owner is
///               `Waiting`. Every row after it is `Authored` -- not skipped, not
///               eligible, not looked at -- until this one settles.
///
/// AND THREE THAT WERE ASKED FOR AND REFUSED. `waiting` is the cursor's business, not
/// a row's; `available` has no preflight owner (an image is discovered to be
/// unopenable by trying); and `mounting` is not a state at all, because a provider
/// mount is synchronous and there is no instant at which anything could observe it. A
/// token with no owner is a field that goes stale in its first week.
///
/// ⚠ `building` IS STILL NOT ONE OF THESE, and BLD-1 did not make it one. BOOT-0
/// refused it because nothing mapped a build to an artifact; that map exists now, and
/// the token is still refused -- because it would be a claim about a BUILD, which this
/// owner cannot see, cannot start and cannot be told the end of. `Pending` says the
/// only thing realization actually knows: this row is not realized and this owner is
/// not going to do anything about it unless it is asked.
enum class RowState : std::uint8_t { Authored, Pending, Loading, Resolved, Refused };

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
/// ---- ...AND SINCE BLD-1 IT HAS A SECOND EAR (and exactly one new sentence) -----
///
/// A maker who asked for BUILD & REALIZE has, when the build works, produced a file
/// that the project may already have authored participation for. The OFFER reaches this
/// bus as `builder::OfferArtifact`, said by the Builder tool; the DECISION about what
/// it is worth is the realization owner's, and this weave is the owner's ear.
///
/// ⚠ THE ANNOUNCED PATH IS NOT USED, and that is the whole safety of this door. The
/// owner resolves a stem to a file with the HOST's rule, exactly as it does for every
/// startup row, so a message naming a path cannot redirect a load. What the message
/// contributes is a STEM and an occasion; everything else is looked up.
///
/// ⚠ AND THE OWNER DECIDES ELIGIBILITY, NOT THE ANNOUNCER. A stem the authored plan
/// does not name, a row already resolved, a row the plan authors BEHIND the one
/// realization is waiting on, an owner in the middle of something -- each is refused by
/// `PlanExecutor::realize`, in its own words, and this weave publishes the refusal
/// rather than inventing one. So the widest thing the Builder tool can cause is that
/// THE ONE ARTIFACT this project's realization is currently stopped at is realized now
/// -- never a later one, and never out of authored order (BLD-1a).
///
/// ITS GRANT GAINS ONE RULE: it may say `ArtifactRealized` to anyone who accepts it.
/// That is an observation and not a power -- nothing acts on it, the Builder panel
/// shows it, and the Builder tool folds it into the picture it publishes.
class PlanBooter
    : public loom::WeaveBase<PlanBooter, BootState,
                             loom::Accept<loom::Result, loom::Ack, loom::Refused,
                                          zengine::builder::OfferArtifact>,
                             loom::Emit<loom::LoadWeave, zengine::builder::ArtifactRealized>> {
public:
    explicit PlanBooter(BootAnswers& answers) : answers_(&answers) {}

    /// A BUILD PRODUCED AN ARTIFACT AND THE MAKER OFFERED IT TO THE PROJECT.
    ///
    /// Defined out of line at the bottom of this file, because the owner it asks is not
    /// declared yet. Two lines of its own: ask the owner, publish what the owner said.
    void on(const zengine::builder::OfferArtifact& offer, loom::Mail& mail);

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
        wake(mail);
    }

    void on(const loom::Ack&, loom::Mail& mail) {
        if (!settles(mail)) {
            return;
        }
        answers_->answered = true;
        answers_->refused = false;
        answers_->settled();
        wake(mail);
    }

    void on(const loom::Refused& r, loom::Mail& mail) {
        if (!settles(mail)) {
            return;
        }
        answers_->answered = true;
        answers_->refused = true;
        answers_->reason = r.reason;
        answers_->settled();
        wake(mail);
    }

private:
    /// HAND THE SETTLED FACT OVER -- after `settled()`, so the owner it wakes finds a
    /// conversation that is closed and a payload that is this answer's. Defined out of
    /// line at the bottom of this file, because the owner is not declared yet.
    void wake(loom::Mail& mail);

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

    /// IS THIS ROW WAITING ON THE MAKER? -- the one seam by which realization learns
    /// that an absent artifact may be a build state rather than a broken deployment
    /// (BLD-1).
    ///
    /// IT IS A PREDICATE AND NOT A RECIPE, and the shape is the containment. This file
    /// cannot start a process, name a compiler, read a recipe catalog or look at a
    /// disk, and it never learns WHY the answer is yes. What it does with a yes is the
    /// smallest possible thing: it does not perform the row, and it STOPS THERE.
    ///
    /// ⚠ A YES BUYS A WAIT AND NOT A REORDER (BLD-1a). It says this row cannot be
    /// performed yet; it says nothing whatever about the rows behind it, and BLD-1
    /// reading it as permission to realize them first is the defect BLD-1a repaired.
    ///
    /// WHY THE HOST OWNS IT. Answering needs two facts that live in two other places --
    /// whether the artifact file is there (the host owns the rule that spells a stem as
    /// a file) and whether this project can produce it (the Builder's authored recipes
    /// name their artifacts). Neither is realization's, and a realization owner that
    /// went and got them would be the build system's second half growing here.
    ///
    /// ⚠ IT IS ASKED ONCE PER ROW, WHEN THE ROW IS REACHED, and never again. This owner
    /// does not re-check, watch, poll or notice; a row that was waiting stays waiting
    /// until somebody asks for it (`realize`). Empty means nothing ever waits, which is
    /// what every caller that does not pass one gets and is exactly LOAD-0's behaviour.
    using AwaitingBuild = std::function<bool(const std::string& stem)>;

    /// REALIZATION CAME TO REST -- called from inside whatever delivery brought it
    /// there, with what the plan has produced so far.
    ///
    /// THREE RESTING POINTS, AND A RUN MAY PASS THROUGH MORE THAN ONE (BLD-1a):
    ///
    ///   `ok`                       every authored row settled
    ///   `refusal` non-empty        a row refused and the plan stopped there
    ///   `waiting_on` non-empty     the walk stopped at a row waiting on the maker,
    ///                              and nothing has refused anything
    ///
    /// ⚠ THE THIRD IS NOT TERMINAL, and that is the point of telling a host about it.
    /// A maker whose project is short an artifact needs to know which one and that
    /// nothing went wrong; when they build it and ask, realization resumes and rests
    /// again. BLD-1 could describe this as "called once" because a waiting row did not
    /// stop anything -- which is exactly the defect BLD-1a repaired.
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
                 Settled settled = Settled(), AwaitingBuild awaiting = AwaitingBuild())
        : bus_(&bus), catalog_(&catalog), operators_(&operators), voice_(&voice),
          manager_(manager), answers_(&answers), path_of_(std::move(path_of)),
          settled_(std::move(settled)), awaiting_(std::move(awaiting)) {
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
        // ONE SETTLING PATH, WHOEVER ASKED (BLD-1a). A row a maker asked for is the row
        // the walk stopped at, so it settles the way every other row settles: the
        // frontier moves ON BY ONE and the plan carries on from there. The only thing
        // `on_demand_` still decides is who is owed a sentence about it, and what a
        // REFUSAL of it means -- see `settled_row` and `fail`.
        settled_row();
        ++cursor_;
        state_ = Realization::Advancing;
        advance();
    }

    // ---- Realizing THE waiting row, because a maker asked (BLD-1) ------------------

    /// WHAT ASKING FOR ONE ROW CAME TO, IMMEDIATELY.
    ///
    /// `started` false means nothing was mounted, nothing was offered and nothing was
    /// commanded, NOTHING MOVED AT ALL, and `refusal` is why. `started` true means the
    /// row is under way -- which for a provider-only row is already OVER
    /// (`take_realization()` has the answer, and the walk has already resumed past it)
    /// and for a weave row means a conversation is outstanding and the answer arrives
    /// later.
    struct Asked {
        bool started = false;
        std::string refusal;
    };

    /// REALIZE THE AUTHORED ROW THIS RUN IS WAITING ON.
    ///
    /// THE ELIGIBILITY RULES ARE ALL HERE, AND THEY ARE ALL ABOUT THE AUTHORED PLAN --
    /// which is what makes this door narrow enough to be reachable from a build. In
    /// order:
    ///
    ///   this owner is between rows      a realization already in flight is not
    ///                                   interruptible, and queueing one would make
    ///                                   this a scheduler
    ///   it is not already resolved      ⚠ AND THIS IS WHERE HOT RELOAD IS REFUSED.
    ///                                   BLD-1 does not unload, reload, replace or
    ///                                   migrate anything, so an artifact that is
    ///                                   already live is told so in words rather than
    ///                                   quietly loaded a second time
    ///   the plan NAMES this artifact    a stem the project never authored cannot be
    ///                                   realized by asking; there is no participation
    ///                                   intent to perform
    ///   it IS THE FRONTIER              ⚠ BLD-1a's, and the one that keeps this door
    ///                                   from being random access: the only row that can
    ///                                   be performed by asking is the row the walk
    ///                                   STOPPED at. Any other authored row is refused
    ///                                   BY THE NAME OF THE ROW IT IS BEHIND
    ///
    /// ⚠ AN INELIGIBLE ASK CHANGES NOTHING AT ALL. No state moves, no row is touched,
    /// and the project is NOT failed -- a maker may build any recipe this project
    /// exposes, including one for an artifact whose authored row is nowhere near the
    /// frontier, and being told "not yet, and here is what we are waiting on" is the
    /// truthful answer to that. `Realization::Failed` is what a host reads to end the
    /// process; a maker who asked too early has lost nothing.
    ///
    /// NOTHING HERE CONSULTS A BUILD, A RECIPE, A FILE OR A TIMESTAMP. This owner does
    /// not know that a build happened and does not need to: what it is being asked is
    /// "perform the participation this project already authored for X", and if X is not
    /// on disk the load refuses in the loader's own words exactly as it always would.
    Asked realize(const std::string& stem) {
        if (state_ != Realization::Waiting && state_ != Realization::Complete) {
            return Asked{false, why_not_asked_now()};
        }
        for (const ResolvedArtifact& done : resolved_) {
            if (done.stem == stem) {
                return Asked{false, "artifact '" + stem +
                                        "' is already part of this running project. BLD-1 "
                                        "does not unload, reload or replace a live artifact, "
                                        "so a rebuilt file has NOT changed the image that is "
                                        "running -- restart to pick it up."};
            }
        }
        std::size_t index = plan_.artifacts.size();
        for (std::size_t i = 0; i < plan_.artifacts.size(); ++i) {
            if (plan_.artifacts[i].stem == stem) {
                index = i;
                break;
            }
        }
        if (index == plan_.artifacts.size()) {
            return Asked{false, "this project does not name artifact '" + stem +
                                    "': a build can produce a file, and only the project's "
                                    "own plan can say how it participates"};
        }
        // ---- IS IT THE ROW REALIZATION IS ACTUALLY WAITING ON? (BLD-1a) --------
        //
        // THE REFUSAL NAMES THE ROW IN FRONT, because that is the only thing a maker
        // can act on. "It is not waiting" is true and useless: what they need to know
        // is that this project is stopped somewhere EARLIER, and where.
        if (state_ != Realization::Waiting) {
            return Asked{false, "artifact '" + stem +
                                    "' is not waiting to be realized in this run"};
        }
        if (index != cursor_) {
            return Asked{false, "this project's realization is waiting on artifact '" +
                                    plan_.artifacts[cursor_].stem +
                                    "', which its plan authors " +
                                    (index > cursor_ ? "before '" : "after '") + stem +
                                    "'. Authored order is realization order, so '" + stem +
                                    "' may be BUILT now and participates when the rows in "
                                    "front of it have."};
        }
        on_demand_ = true;
        state_ = Realization::Advancing;
        perform_row(cursor_);
        // A ROW WITH NO WEAVE INTENT IS ALREADY OVER, so the frontier moves and the
        // walk resumes before this returns -- which is what makes an on-demand row
        // indistinguishable from a startup row in everything except who asked.
        if (state_ == Realization::Advancing) {
            ++cursor_;
            advance();
        }
        return Asked{true, std::string()};
    }

    /// WHAT THE LAST ON-DEMAND REALIZATION CAME TO, TAKEN AWAY.
    ///
    /// TAKEN RATHER THAN READ, because it is a settled fact with exactly one reader --
    /// the participant that will publish it -- and leaving it in place would let the
    /// same realization be announced twice. `settled` false means there is nothing to
    /// say, which is the ordinary answer while a row is still loading.
    struct Realized {
        bool settled = false;
        std::string stem;
        bool realized = false;
        std::string detail;
    };

    Realized take_realization() {
        Realized out = realized_;
        realized_ = Realized{};
        return out;
    }

    /// THE AUTHORED ROW REALIZATION IS WAITING ON, or empty when it is waiting on none.
    ///
    /// DERIVED FROM THE CURSOR, NOT STORED (BLD-1a). A waiting row is a barrier, so the
    /// row that is waiting is by construction the row the walk stopped at -- and a
    /// second record of that would be the mirror that goes stale, which is the same
    /// argument `state_of` already makes for every other row state.
    const std::string& waiting_on() const noexcept {
        static const std::string kNone;
        return state_ == Realization::Waiting && cursor_ < plan_.artifacts.size()
                   ? plan_.artifacts[cursor_].stem
                   : kNone;
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
    /// all five states between them, and a second record of the same fact is the
    /// mirror that goes stale.
    RowState state_of(const std::string& stem) const noexcept {
        // LOADING FIRST, because the row a maker asked for is the row the walk stopped
        // at -- the cursor is still on it, so the two answers below would both apply --
        // and what it is right now is in flight.
        if (current_.stem == stem && state_ == Realization::Loading) {
            return RowState::Loading;
        }
        for (const ResolvedArtifact& done : resolved_) {
            if (done.stem == stem) {
                return RowState::Resolved;
            }
        }
        if (current_.stem == stem && state_ == Realization::Failed) {
            return RowState::Refused;
        }
        if (waiting_on() == stem) {
            return RowState::Pending;
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
    ///
    /// ⚠ AND SINCE BLD-1a A WAITING PLAN IS ONE OF THOSE. `Complete` means every
    /// authored row settled; a plan stopped at a row waiting on the maker answers
    /// `ok == false` with an EMPTY `refusal` and a named `waiting_on`, which is three
    /// fields saying one true thing rather than one token saying two.
    Executed outcome() const {
        Executed out;
        out.ok = state_ == Realization::Complete;
        out.refusal = refusal_;
        out.resolved = resolved_;
        out.waiting_on = waiting_on();
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
    ///
    /// ⚠ AND `SETTLED` INCLUDES `WAITING` (BLD-1a). A row this walk cannot perform yet
    /// is where the walk STOPS, not something it steps over. The loop below has exactly
    /// one exit that is not the end of the plan and one that is, and neither of them
    /// leaves an unperformed row behind it.
    void advance() {
        while (state_ == Realization::Advancing && cursor_ < plan_.artifacts.size()) {
            // IS THIS ROW WAITING ON THE MAKER? Asked before anything is mounted,
            // opened or commanded, so a waiting row costs exactly one predicate and
            // touches no runtime state at all.
            //
            // ⚠ A YES STOPS THE WALK (BLD-1a). BLD-1 recorded the answer and carried
            // on, which turned authored order into eligibility order the moment one
            // artifact was not built yet -- see the barrier section in the header. A
            // waiting row is not a refusal and is not a hole either: it is where this
            // project has got to, and the rows behind it stay `Authored` until it
            // settles. `realize(stem)` is the only door past this line.
            if (awaiting_ && awaiting_(plan_.artifacts[cursor_].stem)) {
                state_ = Realization::Waiting;
                // TOLD LAST, with the cursor and the state already what they will be:
                // a host reading this notice is reading a settled owner, and one that
                // answers `realize` on this very row if it decides to.
                announce();
                return;
            }
            perform_row(cursor_);
            if (state_ == Realization::Advancing) {
                ++cursor_;
            }
        }
        if (state_ == Realization::Advancing) {
            complete();
        }
    }

    /// PERFORM ONE AUTHORED ROW, as far as this process can take it right now.
    ///
    /// IT WAS THE BODY OF `advance`'s LOOP and is a function because BLD-1 gave it a
    /// second caller: a maker asking for one waiting row. Both callers get exactly the
    /// same three steps in exactly the same order -- mount, offer, load -- because the
    /// WITHIN-one-artifact order is semantic law (CAT-0) and a second copy of it is
    /// how two orders come to disagree.
    ///
    /// On return, `state_` is one of: `Advancing` (the row settled and the caller
    /// decides what is next), `Loading` (a conversation is outstanding), `Failed` (a
    /// startup row refused), or `Waiting` (an on-demand row refused, which does not end
    /// the arrangement -- the frontier simply goes back to where the ask found it).
    void perform_row(std::size_t index) {
        {
            const ArtifactIntent& artifact = plan_.artifacts[index];
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
                // the host having turned anything. It leaves `state_` at `Advancing`
                // with the cursor STILL ON THIS ROW: moving the frontier belongs to
                // whoever is walking, which is `advance` or `realize`.
                settled_row();
                return;
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
    }

    /// THE CURRENT ROW PARTICIPATED IN FULL. Kept in authored order, which is what
    /// makes a later reversal walkable backwards (LOAD-0, BOOT-R0 §23).
    ///
    /// IT DOES NOT MOVE THE CURSOR, and the subtraction is BLD-1's: the cursor is the
    /// WALK's business and this is the row's. Its caller advances it.
    ///
    /// ---- ...AND IF A MAKER ASKED, THEY ARE OWED A SENTENCE (BLD-1) ---------------
    ///
    /// THE SENTENCE IS THE RESOLVED ROW'S OWN, said the way the host's startup banner
    /// says one: what was mounted, what was supplied, which weave was minted and under
    /// which role. A maker who asked for BUILD & REALIZE is owed the same account of
    /// what participated as a maker who read the banner at startup.
    ///
    /// ⚠ IT IS LEFT BEFORE THE WALK RESUMES, which is why this is one function and not
    /// two. The caller advances the frontier straight afterwards, and the rows that
    /// follow may refuse or wait; neither is a fact about the artifact the maker asked
    /// for, and neither may overwrite the answer they are owed about it.
    void settled_row() {
        if (on_demand_) {
            std::string said;
            if (current_.provider_mounted) {
                said += "provider '" + current_.provider + "' supplied " +
                        std::to_string(current_.contributed);
            }
            if (current_.weave_loaded) {
                if (!said.empty()) {
                    said += " | ";
                }
                said += "weave #" + std::to_string(current_.weave.value) + " as " + current_.role;
            }
            realized_ = Realized{true, current_.stem, true, said};
            on_demand_ = false;
        }
        resolved_.push_back(std::move(current_));
        current_ = ResolvedArtifact{};
    }

    /// WHY AN ASK CANNOT BE ANSWERED AT ALL RIGHT NOW -- for the three states in which
    /// this owner is not between rows, said apart because they are not one situation:
    /// a project that has not begun, one that refused, and one mid-row.
    std::string why_not_asked_now() const {
        if (state_ == Realization::Unstarted) {
            return "realization has not begun";
        }
        if (state_ == Realization::Failed) {
            return "this project's realization stopped at a refusal and is not "
                   "performing any further rows";
        }
        return "realization is not between rows: it is still performing the authored plan";
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
    /// ⚠ AN ON-DEMAND ROW THAT REFUSES DOES NOT FAIL THE ARRANGEMENT (BLD-1), and
    /// that distinction is load-bearing rather than a nicety. `Realization::Failed`
    /// is what the HOST'S settle notice reads to decide that a project could not be
    /// realized -- in Workshop, that ends the process with exit 4. A maker whose
    /// hand-asked realization of one waiting artifact was refused has not lost the
    /// arrangement they are working in, and must not lose the Workshop they are
    /// working in either. So the row's own mount is rolled back, the refusal is left
    /// for the participant that will publish it, and THE FRONTIER GOES BACK TO WHERE
    /// IT WAS: the row is `pending` again, the cursor has not moved, and a corrected
    /// build can reach it in the same run.
    ///
    /// ⚠ AND `Waiting` IS WHERE IT GOES BACK TO, not `Complete` (BLD-1a). The rows
    /// behind it were never performed, so answering `Complete` here would be this
    /// owner reporting an arrangement that is missing everything from this row on. A
    /// failed build followed by an asked realization leaves the project exactly where
    /// the build found it, which is what makes a retry a retry.
    void fail(const std::string& why) {
        (void)unmount(current_);
        const std::string said = "artifact '" + current_.stem + "': " + why;
        if (on_demand_) {
            realized_ = Realized{true, current_.stem, false, said};
            on_demand_ = false;
            current_ = ResolvedArtifact{};
            state_ = Realization::Waiting;
            return;
        }
        refusal_ = said;
        state_ = Realization::Failed;
        announce();
    }

    /// EVERY AUTHORED ROW RESOLVED, AND THAT IS WHAT THIS WORD MEANS (BLD-1a). It is
    /// reachable from exactly one place -- the walk running off the END of the plan --
    /// and the walk cannot reach the end past a row it did not perform, so `Complete`
    /// and an unresolved authored row cannot coexist.
    ///
    /// It is a fact about realization and about nothing else: it does not stop the bus,
    /// end the host or claim the process is done.
    void complete() {
        state_ = Realization::Complete;
        announce();
    }

    /// TELL THE HOST, IF IT ASKED TO BE TOLD, THAT REALIZATION HAS COME TO REST.
    ///
    /// THREE RESTING POINTS AND NOT ONE (BLD-1a): every row resolved, a row refused, or
    /// the walk stopped at a row waiting on the maker. A host is owed all three, because
    /// all three are moments at which realization will not move again on its own -- and
    /// the third one is the one a maker has to act on.
    ///
    /// SO IT IS NOT "ONCE PER RUN" ANY MORE. A project with a waiting row rests when it
    /// reaches that row and rests again when the maker's realization lets it finish; a
    /// host that prints a banner prints it at each. What did not change is that this is
    /// a NOTICE and not a policy: what a Workshop does about a project that finished,
    /// stopped or is waiting is written in the Workshop.
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
    /// THE ONE QUESTION THIS OWNER CANNOT ANSWER FOR ITSELF (BLD-1). Empty in every
    /// caller that does not pass one, which is LOAD-0's behaviour exactly.
    AwaitingBuild awaiting_;

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

    // ---- what BLD-1 added, and BLD-1a left at two members ---------------------------

    /// IS THE ROW IN FLIGHT ONE A MAKER ASKED FOR? Since BLD-1a it decides ONE thing:
    /// what a REFUSAL of that row means -- the arrangement stops (`Failed`, which ends
    /// a Workshop), or the frontier goes back to waiting where a corrected build can
    /// reach it. It also says who is owed a sentence about the row either way.
    ///
    /// ⚠ IT NO LONGER DECIDES WHETHER THE WALK ADVANCES. A row a maker asked for IS
    /// the row the walk stopped at, so it settles the way every row settles; BLD-1's
    /// second job for this flag existed only because a waiting row had been stepped
    /// over and the cursor was already past it.
    ///
    /// THERE IS NO LIST OF WAITING ROWS. A waiting row is a barrier, so which row is
    /// waiting is `cursor_` and whether one is is `state_` -- see `waiting_on()`.
    bool on_demand_ = false;
    /// WHAT THE LAST ON-DEMAND REALIZATION CAME TO, until somebody takes it.
    Realized realized_;
};

/// DEFINED HERE because the owner it hands the fact to is declared above. One call,
/// no branch of its own: whether this answer means anything to the plan is the
/// owner's question, and it already knows whether it has a row in flight.
inline void PlanBooter::wake(loom::Mail& mail) {
    if (owner_ == nullptr) {
        return;
    }
    owner_->answered();
    // ...AND IF WHAT JUST SETTLED WAS A ROW A MAKER ASKED FOR, SAY SO (BLD-1). The
    // owner leaves the fact where exactly one reader can take it, so this cannot
    // announce a realization twice and cannot announce a startup row at all -- the
    // ordinary case takes nothing and publishes nothing.
    const PlanExecutor::Realized settled = owner_->take_realization();
    if (settled.settled) {
        (void)mail.publish(zengine::builder::ArtifactRealized{settled.stem, settled.realized,
                                                              settled.detail});
    }
}

/// DEFINED HERE for `wake()`'s reason. Two acts and no judgement of its own: put the
/// Builder's offer to the owner, and publish whatever the owner made of it.
///
/// ⚠ A REFUSAL IS PUBLISHED AS LOUDLY AS AN ACCEPTANCE, and both go out as
/// `ArtifactRealized`. A maker who pressed BUILD & REALIZE and got a green build is
/// owed a sentence either way, and the sentence is the owner's -- "already part of this
/// running project", "this project does not name artifact X", the loader's own words
/// about a file it could not open.
///
/// ⚠ WITH NO OWNER WIRED THIS SAYS NOTHING AT ALL. A rig that mounts this weave for its
/// payload semantics and never builds an executor has nobody to ask, and inventing a
/// refusal on its behalf would be this bridge answering for a participant that does not
/// exist.
///
/// A ROW THAT SETTLES SYNCHRONOUSLY -- a provider-only artifact, whose mount returns
/// before this line does -- is already answered by the time `realize` returns, so the
/// take below finds it. A weave row is still loading, `take_realization()` finds
/// nothing, and the answer is published later from `on(loom::Result)`'s path instead.
inline void PlanBooter::on(const zengine::builder::OfferArtifact& offer, loom::Mail& mail) {
    if (owner_ == nullptr) {
        return;
    }
    const PlanExecutor::Asked asked = owner_->realize(offer.artifact);
    if (!asked.started) {
        (void)mail.publish(
            zengine::builder::ArtifactRealized{offer.artifact, false, asked.refusal});
        return;
    }
    const PlanExecutor::Realized settled = owner_->take_realization();
    if (settled.settled) {
        (void)mail.publish(zengine::builder::ArtifactRealized{settled.stem, settled.realized,
                                                              settled.detail});
    }
}

} // namespace zengine::workshop::load

#endif // ZENGINE_WORKSHOP_LOAD_EXECUTE_HPP
