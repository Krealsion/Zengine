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
#include <string>
#include <utility>
#include <vector>

namespace zengine::workshop::load {

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

// ---- The weave that asks, and hears the answer --------------------------------

/// WHAT THE PLAN BOOTER HEARD about the load it last asked for.
///
/// "Failures are values": the Manager answers its ASKER with `zen.Result` or
/// `zen.Refused`. A root send carries no asker, so every one of those answers would
/// be addressed to nobody -- which is the defect the original boot weave was written
/// to end, and this is that weave with somewhere to put the answer.
struct BootAnswers {
    bool answered = false;
    bool refused = false;
    std::string reason;   ///< the Manager's own words, when it refused
    std::uint64_t weave = 0;

    void clear() { *this = BootAnswers{}; }
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
class PlanBooter : public loom::WeaveBase<PlanBooter, BootState,
                                          loom::Accept<loom::Result, loom::Ack, loom::Refused>,
                                          loom::Emit<loom::LoadWeave>> {
public:
    explicit PlanBooter(BootAnswers& answers) : answers_(&answers) {}

    void on(const loom::Result& r, loom::Mail&) {
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
    }

    void on(const loom::Ack&, loom::Mail&) {
        answers_->answered = true;
        answers_->refused = false;
    }

    void on(const loom::Refused& r, loom::Mail&) {
        answers_->answered = true;
        answers_->refused = true;
        answers_->reason = r.reason;
    }

private:
    BootAnswers* answers_;
};

// ---- The executor --------------------------------------------------------------

/// How many bus turns one artifact's load may take before the executor stops waiting.
///
/// A HANG GUARD, NOT A SCHEDULE. `Kernel::load` is several deliveries below the
/// command -- `zen.LoadWeave` to the Manager, the Manager to the control door, the
/// door into the Kernel -- and the answer comes back the same way; the drain stops
/// the instant an answer arrives or the queue empties, so the ceiling is reached only
/// by a bus that somehow never quiesces. It is generous for the same reason the boot
/// drain it replaces was: an artifact that goes live and immediately starts working
/// (a Timer re-arms its own beat inside its own handler) makes a drain-to-empty never
/// return, so this counts GENERATIONS of `pump_pending()` rather than deliveries.
inline constexpr int kLoadDrainTurns = 64;

/// PERFORM AN AUTHORED PLAN AGAINST ONE HOST'S RUNTIME.
///
/// Everything it needs is handed to it and nothing is global: the catalog it mounts
/// into, the operator surface it offers, the bus it sends on, the booter that speaks
/// for it, the Manager it speaks to, the answers that come back, and the one rule
/// that spells an artifact stem as a file. A second Zengine host in this process
/// would own a second executor, correctly, and neither would be "the" executor.
class PlanExecutor {
public:
    /// How a host spells an artifact stem as a file on this platform. THE HOST OWNS
    /// THIS RULE and the plan does not: a directory, a separator and `.so`/`.dll` are
    /// deployment facts, and keeping them here is what makes ONE authored plan legal
    /// on Linux and on Windows with no platform field, no suffix and no locator.
    using ArtifactPath = std::function<std::string(const std::string& stem)>;

    PlanExecutor(loom::Switchboard& bus, op::Catalog& catalog,
                 const op::OperatorHostSurface& operators, loom::WeaveId booter,
                 loom::WeaveId manager, BootAnswers& answers, ArtifactPath path_of)
        : bus_(&bus), catalog_(&catalog), operators_(&operators), booter_(booter),
          manager_(manager), answers_(&answers), path_of_(std::move(path_of)) {}

    /// Execute every row, in authored order, stopping at the first refusal.
    ///
    /// STOPS RATHER THAN SKIPS. A host that carried on past a failed artifact would be
    /// running a project nobody authored while reporting the one that was asked for --
    /// which is the shape of failure durable intent exists to end.
    Executed run(const LoadPlan& plan) {
        Executed out;
        for (const ArtifactIntent& artifact : plan.artifacts) {
            ResolvedArtifact done;
            const std::string why = perform(artifact, done);
            if (!why.empty()) {
                // ROLLED BACK BEFORE IT IS REPORTED, so the sentence a host prints
                // describes a runtime that no longer holds anything this row put there.
                unmount(done);
                out.refusal = "artifact '" + artifact.stem + "': " + why;
                // COPIED, NOT MOVED, on both paths -- and the failure path is the one
                // that had to change (INTR-1). A move here emptied `resolved_`, so an
                // executor that had mounted three artifacts and refused the fourth
                // answered `resolved()` with nothing: the accessor below promises what
                // this executor has PUT INTO THE RUNTIME, and after a move that promise
                // was false exactly when somebody most needed it. Nothing in production
                // reached it -- a refused plan exits the host -- but a projection now
                // reads this accessor, and a view whose emptiness depends on which
                // return statement ran is not a view of anything.
                out.resolved = resolved_;
                return out;
            }
            resolved_.push_back(std::move(done));
        }
        out.ok = true;
        out.resolved = resolved_;
        return out;
    }

    /// What this executor has actually put into the runtime, in the order it did.
    const std::vector<ResolvedArtifact>& resolved() const noexcept { return resolved_; }

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
    /// ONE ARTIFACT, WHOLE. Returns the empty string on success, or the sentence
    /// describing which participation step refused and why.
    std::string perform(const ArtifactIntent& artifact, ResolvedArtifact& done) {
        done.stem = artifact.stem;
        const std::string path = path_of_(artifact.stem);

        if (artifact.provider.has_value()) {
            const op::MountResult mounted =
                op::mount_provider(*catalog_, path, artifact.provider->mode);
            if (!mounted.ok) {
                // THE MOUNT'S OWN WORDS. `mount_provider` already says whether the
                // file was there, whether it exports a provider surface, which
                // version it speaks, and who already supplies a colliding power; a
                // second sentence written here would be a second copy of a judgement
                // this file does not own.
                return "provider mount refused: " + mounted.reason;
            }
            done.provider_mounted = true;
            done.provider = mounted.provider;
            done.contributed = mounted.contributed;
        }

        if (!artifact.weave.has_value()) {
            return std::string();
        }
        done.role = artifact.weave->role;

        // THE OFFER BRACKETS THE LOAD AND IS WITHDRAWN BY THE CLOSING BRACE, which is
        // OPH-0's law and is not relaxed here. The offer goes up BEFORE the command is
        // sent, because a consumer's first legitimate need is inside `create()` and no
        // host can get between the Kernel and a constructor.
        {
            op::OperatorOffer offering(*operators_, path);
            done.offer = offering.outcome();
            if (done.offer == op::OfferOutcome::VersionMismatch) {
                // A BROKEN HANDOFF IS NOT "NO HOST INTENDED". See the header.
                return "operator handoff refused: " + offering.reason();
            }
            const std::string refused = load_weave(artifact.stem, path, artifact.weave->role);
            if (!refused.empty()) {
                return "weave load refused: " + refused;
            }
        }
        done.weave_loaded = true;
        done.weave = loom::WeaveId{answers_->weave};
        return std::string();
    }

    /// ASK THE WEAVE MANAGER, AND WAIT FOR THE ANSWER.
    ///
    /// An ordinary `zen.LoadWeave`, sent AS the booter so the Manager's answer comes
    /// back to something that can hear it. This adds no loading mechanism: it is the
    /// send Workshop's `main()` already made, with a drain that stops on the answer
    /// instead of leaving it to arrive whenever.
    ///
    /// ANSWERED, NOT MERELY LOADED. `Kernel::is_loaded` turns true the instant
    /// `Kernel::load` returns, while the `zen.Result` naming the new WeaveId is still
    /// queued -- so this waits for the ANSWER, which is also what makes a refused load
    /// stop the drain instead of spending its whole budget.
    std::string load_weave(const std::string& stem, const std::string& path,
                           const std::string& role) {
        answers_->clear();
        bus_->send_as(booter_, manager_,
                      loom::Message(loom::to_value(loom::LoadWeave{stem, path, role}), booter_,
                                    booter_));
        for (int turn = 0; turn < kLoadDrainTurns && !answers_->answered; ++turn) {
            if (bus_->pump_pending() == 0) {
                break;
            }
        }
        if (!answers_->answered) {
            // NEITHER CONFIRMED NOR REFUSED. Said as exactly that rather than as a
            // refusal somebody else wrote: the Manager has not spoken, and inventing
            // its sentence would be this layer claiming to know why.
            return "the Weave Manager neither confirmed nor refused it within " +
                   std::to_string(kLoadDrainTurns) + " bus turns";
        }
        return answers_->refused ? answers_->reason : std::string();
    }

    loom::Switchboard* bus_;
    op::Catalog* catalog_;
    const op::OperatorHostSurface* operators_;
    loom::WeaveId booter_;
    loom::WeaveId manager_;
    BootAnswers* answers_;
    ArtifactPath path_of_;
    std::vector<ResolvedArtifact> resolved_;
};

} // namespace zengine::workshop::load

#endif // ZENGINE_WORKSHOP_LOAD_EXECUTE_HPP
