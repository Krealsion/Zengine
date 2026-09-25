// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_LOAD_EXECUTE_HPP
#define ZENGINE_WORKSHOP_LOAD_EXECUTE_HPP

// Performing an authored load plan (agents/realization.md). The owner walks the rows in authored
// order -- mount the provider, offer this host's operator resolution, load the weave -- and returns
// to the host while a load's answer is outstanding. It stops at the first row it cannot perform, a
// barrier and never a hole; rolls back only the refusing row's own mount; and reloads a live
// weave-only row in place from a copy the host staged. No loader, scheduler or solver lives here.

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
#include <string_view>
#include <utility>
#include <vector>

namespace zengine::workshop::load {

class PlanExecutor;

// ---- What the runtime made of one authored row --------------------------------

/// What one artifact's participation produced: resolved truth, kept only here and never written to
/// the plan -- the provider identity the artifact declared, what it contributed, the WeaveId minted
/// this run. Rollback needs it; `workshop/arrangement.hpp` reads it beside the authored plan.
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

    /// The file the running code was opened from: the plan's spelling of the stem, or after a
    /// reload the per-operation copy the host staged.
    std::string image;
    /// The image before the last reload, or empty: what `revert` reloads.
    std::string previous;
    /// The per-operation copy a promotion wrote into the plan's file, or empty.
    std::string promoted_from;
    /// Is `image` the file a restart loads? When false, a maker who quits runs the old code next
    /// launch, and the sentence they read says so.
    bool default_image = false;
    /// The authored choice this row's office moved to, or empty: such a row is not running, and
    /// keeps its images so switching back runs the code the maker last had.
    std::string switched_to;
};

/// What executing a whole plan produced, or where it stopped. A refusal names the artifact, the
/// step and the refusing layer's own sentence.
struct Executed {
    bool ok = false;
    std::string refusal;
    std::vector<ResolvedArtifact> resolved;
    /// The one row realization stopped at because the host said it waits on the maker, or empty --
    /// one name, because the walk stops at the first. While set, `ok` is false and `refusal` empty.
    std::string waiting_on;

    /// The optional rows that refused and were stepped over, each the refusing layer's sentence.
    /// They leave `ok` true -- the plan said they may be missing -- and are named, never silent.
    std::vector<std::string> unavailable;
    /// ...AND EACH ONE'S ARTIFACT, in the same order: the name a maker builds. A condition keyed
    /// and named by it says WHICH tool is missing on a row no tool paints (`unavailable_tool`).
    std::vector<std::string> unavailable_stems;

    explicit operator bool() const noexcept { return ok; }
};

// ---- Where realization is, and what it has made of one authored row ------------

/// How far the owner has got with the whole plan. `Advancing` is transient: nothing dispatches
/// inside it. `Complete` means every authored row settled; a plan stopped at a waiting row is
/// `Waiting` -- unfinished, nothing refused -- and that row is `RowState::Pending`.
enum class Realization : std::uint8_t {
    Unstarted, ///< `begin` has not been called; the plan has not been touched
    Advancing, ///< inside `advance`: performing what is knowable now (transient)
    Loading,   ///< a `zen.LoadWeave` conversation is outstanding for the current row
    Waiting,   ///< the frontier row is waiting on the maker; the walk stopped there
    Complete,  ///< every authored row settled: resolved, or authored optional and unavailable
    Failed,    ///< a row refused; progression stopped and earlier rows still stand
};

/// What realization has made of one authored row. `Pending`: reached, and the host said it waits
/// on the maker -- the barrier, held by at most the row `cursor_` is on. `Reloading`: resolved, and
/// a reload conversation is open, so it keeps its resolved fields. `Switched`: its office moved to
/// another authored choice. `Unavailable`: an optional row that refused and was stepped over.
enum class RowState : std::uint8_t {
    Authored,
    Pending,
    Loading,
    Resolved,
    Refused,
    Reloading,
    Switched,
    Unavailable
};

/// The kernel's reason for refusing a reload, in a maker's words: each sentence names the artifact
/// and what the maker can do, shape mismatches first. An unknown reason is quoted whole.
inline std::string reload_refusal_words(const std::string& stem, const std::string& loom_words) {
    const auto says = [&loom_words](const char* head) {
        return loom_words.rfind(head, 0) == 0;
    };
    std::string said;
    if (says("state schema version mismatch")) {
        said = "the rebuilt '" + stem +
               "' keeps a different STATE than the running one; a same-shape reload cannot "
               "carry the state across, so the running weave was left as it is. Replacing it "
               "is a prepared replacement with an authored migration";
    } else if (says("accepted schema contract mismatch")) {
        said = "the rebuilt '" + stem +
               "' answers to different MESSAGES than the running one; the doors it publishes "
               "would lie about what is loaded, so the running weave was left as it is. "
               "Replacing it is a prepared replacement, not a reload";
    } else if (says("not loaded:")) {
        said = "nothing named '" + stem +
               "' is loaded in this process: this would be an initial load, not a reload";
    } else if (says("open failed:")) {
        said = "the rebuilt '" + stem + "' did not open; the running weave is unchanged";
    } else if (says("library create() returned null")) {
        said = "the rebuilt '" + stem + "' produced no weave; the running weave is unchanged";
    } else if (says("new library refused:") &&
               loom_words.find("is already published with a different shape") !=
                   std::string::npos) {
        // A shape kept its name and version: the new library is refused before state is compared,
        // so say which edit did it rather than "refused to construct".
        said = "the rebuilt '" + stem +
               "' changed a shape but kept its name and version, and this process already holds "
               "the old meaning, so the running weave was left as it is. Put the shape back to "
               "reload in place; a changed shape needs a new version and a prepared replacement";
    } else if (says("new library refused:")) {
        said = "the rebuilt '" + stem +
               "' refused to construct; the running weave is unchanged";
    } else if (says("snapshot of the live weave failed:")) {
        said = "the running '" + stem +
               "' could not be snapshotted, so nothing was replaced";
    } else if (says("revive after swap was refused")) {
        said = "the new code of '" + stem +
               "' could not take the saved state; '" + stem +
               "' is unavailable until it is reloaded again";
    } else if (says("the reload ended a prepared replacement")) {
        said = "'" + stem +
               "' was a replacement candidate; reloading it ended that replacement and released "
               "the artifact";
    } else {
        said = "the kernel's control door refused the reload of '" + stem + "'";
    }
    return said + " (Loom: " + loom_words + ")";
}

/// Can a rebuilt image of this resolved row be reloaded in place at all? The rules about the row,
/// not the image, in the owner's words, or empty. `PlanExecutor::reloadable` spends it; a host
/// reads it to tell a maker before an edit. One rule, so the two sentences cannot disagree.
inline std::string reload_refusal(const ResolvedArtifact& done) {
    if (!done.switched_to.empty()) {
        return "artifact '" + done.stem + "' is not running: its office " + done.role +
               " is held by the authored choice '" + done.switched_to +
               "'; switch back to it before reloading it";
    }
    if (!done.weave_loaded) {
        return "artifact '" + done.stem +
               "' loaded no weave in this run: a provider's contribution is not reloaded in "
               "place (unmount-and-remount is not built), and there is no weave to reload";
    }
    if (done.provider_mounted) {
        return "artifact '" + done.stem +
               "' also supplies operators to the catalog; reloading its weave would leave the "
               "catalog on the old image, and unmount-and-remount is not built. The running "
               "weave is unchanged";
    }
    return std::string();
}

// ---- The weave that asks, and hears the answer --------------------------------

/// What the plan booter heard about the load it last asked for. An arrival settles the load only
/// if both its correlation and its bus-stamped sender match the one outstanding ask
/// (`loom::AskBook`): a correlation identifies, it does not authenticate (Loom ANS-05). What stays
/// here is payload semantics: which answer means success, and what a `zen.Result`'s text is.
struct BootAnswers {
    bool answered = false;
    bool refused = false;
    std::string reason;   ///< the Manager's own words, when it refused
    std::uint64_t weave = 0;

    /// Open a conversation with `respondent` and return the correlation the request must carry,
    /// clearing the previous answer. Zero means none was opened, and the caller must send nothing.
    std::uint64_t ask(loom::WeaveId respondent, const char* shape = loom::LoadWeave::zen_name,
                      std::uint32_t version = loom::LoadWeave::zen_version) {
        answered = false;
        refused = false;
        reason.clear();
        weave = 0;
        const loom::AskOpened opened = book_.open(respondent, shape, version);
        current_ = opened.id;
        return opened.correlation;
    }

    /// Is this arrival the answer to the conversation this record waits on? Read-only.
    bool settles(std::uint64_t correlation, loom::WeaveId from) const noexcept {
        return book_.is_settled_by(current_, correlation, from);
    }

    /// An answer closed the conversation: said explicitly, so a duplicate finds nothing to close.
    void settled() noexcept { close(); }

    /// This host stopped waiting, with no answer -- local only: the respondent is told nothing,
    /// and a late answer matches no record. Say it only when the wait will not be resumed.
    void stopped_waiting() noexcept { close(); }

    /// Is a load conversation still outstanding?
    bool awaiting() const noexcept { return book_.waiting_on(current_); }

    /// The correlation of the outstanding conversation, or 0 when none is.
    std::uint64_t asking() const noexcept {
        const loom::PendingAsk* p = book_.find(current_);
        return p == nullptr ? 0 : p->correlation;
    }

    /// What this record has open -- at most the one load currently in flight.
    const loom::AskBook& book() const noexcept { return book_; }

private:
    /// One conversation, leaving by one of two doors that say different facts.
    void close() noexcept {
        (void)book_.forget(current_);
        current_ = 0;
    }

    /// One conversation at a time: a second is refused, not shed. Its correlations are monotonic
    /// and never reused, so a late answer to a forgotten load cannot settle the next one.
    loom::AskBook book_{1};
    /// Which of the book's conversations is the load now in flight.
    std::uint64_t current_ = 0;
};

/// The plan booter's own state. It holds nothing: what it hears goes into the host's
/// `BootAnswers`, which the executor and the host both read.
struct BootState {
    std::int64_t asked = 0;
    ZEN_EXPOSE();
    ZEN_SHAPE(BootState, 1, ZEN_FIELD(asked));
};

/// The weave that asks the Weave Manager to load, reload, promote and revert a plan's artifacts.
/// It holds kernel reach, and the host writes that grant. It is the bridge, not the state machine:
/// it hears an answer, checks that it settles this booter's conversation, and hands it to the
/// owner. The bus outlives the owner, so the owner wires `wakes` and clears it. An offered
/// artifact's announced path is never used, and the owner decides whether a stem is eligible.
class PlanBooter
    : public loom::WeaveBase<PlanBooter, BootState,
                             loom::Accept<loom::Result, loom::Ack, loom::Refused,
                                          zengine::builder::OfferArtifact,
                                          zengine::builder::PromoteArtifact,
                                          zengine::builder::RevertArtifact>,
                             loom::Emit<loom::LoadWeave, loom::ReloadWeave,
                                        zengine::builder::ArtifactRealized,
                                        zengine::builder::ArtifactPromoted,
                                        zengine::builder::RealizationAsked>> {
public:
    explicit PlanBooter(BootAnswers& answers) : answers_(&answers) {}

    /// A build produced an artifact and the maker offered it: ask the owner, publish its answer.
    void on(const zengine::builder::OfferArtifact& offer, loom::Mail& mail);

    /// The maker asked for the running image to become the one a restart loads.
    void on(const zengine::builder::PromoteArtifact& ask, loom::Mail& mail);

    /// The maker asked for the image before the last reload to run again; a refusal is published
    /// now, an acceptance answered later, as a load is.
    void on(const zengine::builder::RevertArtifact& ask, loom::Mail& mail);

    /// Whose unfinished work an answer to this booter wakes, or nobody. Called only by
    /// `PlanExecutor`'s constructor and destructor.
    void wakes(PlanExecutor* owner) noexcept { owner_ = owner; }

    /// Which weave this is: the executor sends its load as this booter, so the answer comes back
    /// to something that can hear it.
    loom::WeaveId speaker() const noexcept { return self_; }

    void on(const loom::Result& r, loom::Mail& mail) {
        if (!settles(mail)) {
            return;
        }
        answers_->answered = true;
        answers_->refused = false;
        // `zen.Result` carries text; one this host cannot parse is still an answer, just not a
        // WeaveId anybody may quote.
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
    /// Hand the settled fact over, after `settled()`.
    void wake(loom::Mail& mail);

    /// The one door all three answer shapes pass through: a wall on two of three is no wall. An
    /// arrival that does not settle this load is left alone -- not refused, not recorded.
    bool settles(const loom::Mail& mail) const {
        return answers_->settles(mail.correlation(), mail.sender());
    }

    /// SAY WHAT BECAME OF ONE ASK before anything else about it (`RealizationAsked`): its number
    /// when taken, minted here and never reused in this booter's life; 0 and why when not.
    std::int64_t heard(const std::string& artifact, const char* act, bool taken,
                       const std::string& refusal, loom::Mail& mail) {
        const std::int64_t number = taken ? ++asks_ : 0;
        (void)mail.publish(zengine::builder::RealizationAsked{artifact, act, number, taken, refusal});
        return number;
    }

    /// The owner's answer to the ask in flight, which is then answered.
    void realized(const std::string& stem, bool done, const std::string& detail,
                  bool default_image, loom::Mail& mail) {
        (void)mail.publish(
            zengine::builder::ArtifactRealized{stem, done, detail, default_image, in_flight_});
        in_flight_ = 0;
    }

    BootAnswers* answers_;
    /// The host-side owner this booter speaks for, or null (a rig that wants only the payload).
    PlanExecutor* owner_ = nullptr;
    std::int64_t asks_ = 0;     ///< realization asks taken, ever
    std::int64_t in_flight_ = 0; ///< the taken ask whose answer is still owed; 0 when none is
};


// ---- The realization owner -----------------------------------------------------

/// Performs an authored plan against one host's runtime, across the host's own turns; everything
/// it uses is handed in. Host-side for lifetime: it holds an `op::OperatorOffer`, so it is a local
/// of the host's `main` declared after the Kernel, never a weave (agents/realization.md). The host
/// advances Loom; this decides what the facts mean.
class PlanExecutor {
public:
    /// How a host spells an artifact stem as a file. The host owns the rule, so one plan is legal
    /// on every platform.
    using ArtifactPath = std::function<std::string(const std::string& stem)>;

    /// Is this row waiting on the maker? A predicate the host owns -- the file rule and the recipes
    /// are its facts -- asked once, when the row is reached. A yes buys a wait, never a reorder:
    /// the walk stops there. Empty means nothing ever waits.
    using AwaitingBuild = std::function<bool(const std::string& stem)>;

    /// What the host made of staging one built product: where to open it, or why not.
    struct Staged {
        bool ok = false;
        std::string path;
        std::string refusal;
    };

    /// Put the product of `recipe` where `stem` can be opened from. `reload` false copies it to the
    /// plan's file; true copies it to a per-operation path off the loaded file, which is mapped.
    /// Empty: an initial realization opens the plan's file, and a reload is refused.
    using StageArtifact =
        std::function<Staged(const std::string& stem, const std::string& recipe, bool reload)>;

    /// What the host made of a promotion: done, or why not in the OS's words.
    struct Promoted {
        bool ok = false;
        std::string detail;
        std::string kept; ///< where the host kept the bytes it wrote over, or empty
    };

    /// Write the running image's bytes into the file the plan resolves `stem` to: the host's
    /// durable write. Empty means a promotion is refused.
    using PromoteImage =
        std::function<Promoted(const std::string& stem, const std::string& image)>;

    /// Realization came to rest -- every row settled, a `refusal`, or `waiting_on` -- called from
    /// the delivery that brought it there. Waiting is not terminal: it rests again once the maker
    /// builds and asks. A notice, not a policy: what a host does about it is the host's.
    using Settled = std::function<void(const Executed&)>;

    /// The booter as an object: this sends as its id and wires the answer path through it.
    PlanExecutor(loom::Switchboard& bus, op::Catalog& catalog,
                 const op::OperatorHostSurface& operators, PlanBooter& voice,
                 loom::WeaveId manager, BootAnswers& answers, ArtifactPath path_of,
                 Settled settled = Settled(), AwaitingBuild awaiting = AwaitingBuild(),
                 StageArtifact stage = StageArtifact(), PromoteImage promote = PromoteImage())
        : bus_(&bus), catalog_(&catalog), operators_(&operators), voice_(&voice),
          manager_(manager), answers_(&answers), path_of_(std::move(path_of)),
          settled_(std::move(settled)), awaiting_(std::move(awaiting)),
          stage_(std::move(stage)), promote_(std::move(promote)) {
        voice_->wakes(this);
    }

    PlanExecutor(const PlanExecutor&) = delete;
    PlanExecutor& operator=(const PlanExecutor&) = delete;
    PlanExecutor(PlanExecutor&&) = delete;
    PlanExecutor& operator=(PlanExecutor&&) = delete;

    /// Release what a row may still hold, in the safe order: the booter's pointer to this (the bus
    /// outlives this object), the operator offer (its image share goes before the Kernel unloads),
    /// then an ask that will never be resumed is forgotten, locally. The catalog is not unwound.
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

    /// Begin realizing `plan`, and return: it performs what it can now and stops at the first row
    /// that needs a fact not here yet. By value, because the owner outlives the expression that
    /// started it; this copy is the authored half the projection reads. A second call is refused.
    void begin(LoadPlan plan) {
        if (state_ != Realization::Unstarted) {
            return;
        }
        plan_ = std::move(plan);
        state_ = Realization::Advancing;
        advance();
    }

    /// The load this owner asked for has settled: called by `PlanBooter` from that delivery,
    /// never by a host. The booter already judged the arrival; with nothing outstanding, inert.
    void answered() {
        if (state_ != Realization::Loading) {
            return;
        }
        // A reload settles its own way: no row is judged, and the plan does not advance.
        if (reloading_.has_value()) {
            settle_reload();
            return;
        }
        // The offer's custody ends here: after the load, before the row is judged or the next
        // row begins.
        offer_.reset();
        if (answers_->refused) {
            fail("weave load refused: " + answers_->reason);
            // An optional row that refused leaves the owner advancing: resume the walk here.
            if (state_ == Realization::Advancing) {
                ++cursor_;
                advance();
            }
            return;
        }
        current_.weave_loaded = true;
        current_.weave = loom::WeaveId{answers_->weave};
        // A row loaded from the plan runs from the plan's own file, the one a restart loads.
        current_.image = path_of_(current_.stem);
        current_.default_image = true;
        // One settling path, whoever asked: the frontier moves on by one.
        settled_row();
        ++cursor_;
        state_ = Realization::Advancing;
        advance();
    }

    // ---- Realizing THE waiting row, because a maker asked --------------------------

    /// What asking for one row came to at once: `started` false means nothing moved and `refusal`
    /// says why; true means under way (a provider-only row is already over).
    struct Asked {
        bool started = false;
        std::string refusal;
    };

    /// Realize the row this run is waiting on. In order: the owner must be between rows; a resolved
    /// row is reloaded in place (`reload`); the plan must name the artifact; and it must be the
    /// frontier -- any other row is refused by the name of the row in front of it. An ineligible
    /// ask changes nothing and fails nothing. The recipe only reaches the host's staging rule.
    Asked realize(const std::string& stem, const std::string& recipe = std::string()) {
        if (state_ != Realization::Waiting && state_ != Realization::Complete) {
            return Asked{false, why_not_asked_now()};
        }
        for (std::size_t i = 0; i < resolved_.size(); ++i) {
            if (resolved_[i].stem == stem) {
                return reload(i, recipe);
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
        // The refusal names the row in front: that is what a maker can act on.
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
        // The host copies the built product into place first; if it cannot, nothing moves.
        if (stage_ && !recipe.empty()) {
            const Staged staged = stage_(stem, recipe, /*reload=*/false);
            if (!staged.ok) {
                return Asked{false, "artifact '" + stem + "': " + staged.refusal};
            }
        }
        on_demand_ = true;
        state_ = Realization::Advancing;
        perform_row(cursor_);
        // A row with no weave intent is already over: the walk resumes before this returns.
        if (state_ == Realization::Advancing) {
            ++cursor_;
            advance();
        }
        return Asked{true, std::string()};
    }

    // ---- Reloading a live row in place, promoting it, reverting it ----------------------------

    /// Run the image before the last reload again: a reload from `previous`, same shapes by
    /// construction. Refused in words with no previous image, or whenever a reload would be.
    Asked revert(const std::string& stem) {
        if (state_ != Realization::Waiting && state_ != Realization::Complete) {
            return Asked{false, why_not_asked_now()};
        }
        for (std::size_t i = 0; i < resolved_.size(); ++i) {
            if (resolved_[i].stem != stem) {
                continue;
            }
            const Asked live = reloadable(resolved_[i]);
            if (!live.started) {
                return live;
            }
            if (resolved_[i].previous.empty()) {
                return Asked{false, "artifact '" + stem +
                                        "' has no previous image to revert to: it has not "
                                        "been reloaded in this run, or a promotion wrote "
                                        "over the image before its last reload"};
            }
            return open_reload(i, resolved_[i].previous, /*revert=*/true);
        }
        return Asked{false, "artifact '" + stem + "' is not part of this running project"};
    }

    /// Make the running image the one a restart loads (synchronous). When the image before the last
    /// reload is the plan's file, the host keeps those bytes aside (`kept`), so a revert still runs
    /// the code the maker had.
    Promoted promote(const std::string& stem) {
        if (state_ != Realization::Waiting && state_ != Realization::Complete) {
            return Promoted{false, why_not_asked_now(), std::string()};
        }
        for (ResolvedArtifact& done : resolved_) {
            if (done.stem != stem) {
                continue;
            }
            if (!done.weave_loaded) {
                return Promoted{false, "artifact '" + stem +
                                           "' loaded no weave in this run; there is no "
                                           "running image to promote", std::string()};
            }
            if (done.default_image) {
                return Promoted{false, "artifact '" + stem +
                                           "' already runs from the file a restart loads; "
                                           "there is nothing to promote", std::string()};
            }
            if (!promote_) {
                return Promoted{false, "this host has no rule for writing a running image "
                                       "into the file a restart loads; nothing was promoted", std::string()};
            }
            Promoted wrote = promote_(stem, done.image);
            if (!wrote.ok) {
                return Promoted{false, "the running '" + stem +
                                           "' could not be promoted: " + wrote.detail +
                                           "; the file a restart loads is unchanged", std::string()};
            }
            done.default_image = true;
            done.promoted_from = done.image;
            if (done.previous == path_of_(stem)) {
                done.previous = wrote.kept;
            }
            return Promoted{true, "promoted: the next launch runs the image weave #" +
                                      std::to_string(done.weave.value) + " is running now" +
                                      (wrote.detail.empty() ? std::string() : " -- " + wrote.detail), std::string()};
        }
        return Promoted{false, "artifact '" + stem + "' is not part of this running project",
                        std::string()};
    }

    // ---- A row appended to the plan, because a maker asked ------------------------------------

    /// What appending one authored row came to at once: `accepted` false leaves the plan as it
    /// was; true says what happened to the row in `detail`.
    struct Appended {
        bool accepted = false;
        std::string refusal;
        std::string detail;
    };

    /// Append one authored row, checked by the plan's own law, and perform it when the walk has
    /// room: from `Complete` the walk resumes at it; in `Waiting` it is authored behind the
    /// frontier; any other state refuses. Nothing here writes a file (`workshop/authoring.hpp`).
    // WL-AUTH-03 -- agents/workshop/authoring.md
    Appended append(ArtifactIntent row) {
        if (state_ == Realization::Unstarted || state_ == Realization::Failed ||
            state_ == Realization::Loading || state_ == Realization::Advancing) {
            return Appended{false, why_not_asked_now(), std::string()};
        }
        LoadPlan candidate = plan_;
        candidate.artifacts.push_back(std::move(row));
        const Written legal = check_plan(candidate);
        if (!legal.accepted) {
            return Appended{false, legal.refusal, std::string()};
        }
        plan_ = std::move(candidate);
        const std::string& stem = plan_.artifacts.back().stem;
        if (state_ == Realization::Waiting) {
            return Appended{true, std::string(),
                            "authored behind '" + waiting_on() + "', which the project is waiting on"};
        }
        // COMPLETE: the cursor rests at the old end, which is exactly the new row.
        state_ = Realization::Advancing;
        advance();
        std::string said;
        switch (state_of(stem)) {
        case RowState::Resolved: said = "resolved"; break;
        case RowState::Loading: said = "loading"; break;
        case RowState::Pending: said = "pending -- the project is waiting on it"; break;
        case RowState::Refused: said = "refused: " + refusal_; break;
        default: said = "authored"; break;
        }
        return Appended{true, std::string(), said};
    }

    // ---- An office moved between authored choices (the editor switch) ----------------

    /// WHAT RECORDING A SWITCH CAME TO: nothing moved and `refusal` says why, or the rows say
    /// where the office is now.
    struct Recorded {
        bool accepted = false;
        std::string refusal;
    };

    /// The image a choice's artifact runs from when a switch loads it: its row's last image, so
    /// reloaded code survives a switch away and back, or the plan's own file.
    std::string image_of(const std::string& stem) const {
        for (const ResolvedArtifact& done : resolved_) {
            if (done.stem == stem && !done.image.empty()) {
                return done.image;
            }
        }
        return path_of_(stem);
    }

    /// An office with authored choices is now held by `weave`, loaded from `image` for the choice
    /// whose artifact is `stem`, recorded after Loom's admission moved the office. The former
    /// holder's row becomes `switched`; the choice's row is resolved with the admitted weave.
    /// Refused, changing nothing, for an unauthored choice or while a conversation is open.
    // WL-SWITCH-02 -- agents/workshop/editor-switch.md
    Recorded record_choice_holder(const std::string& role, const std::string& stem,
                                  loom::WeaveId weave, const std::string& image) {
        bool authored = false;
        for (const ChoiceIntent& c : plan_.choices) {
            authored = authored || (c.role == role && c.stem == stem);
        }
        if (!authored) {
            return Recorded{false, "this project authors no choice '" + stem + "' for " + role};
        }
        if (state_ == Realization::Loading || state_ == Realization::Advancing) {
            return Recorded{false, why_not_asked_now()};
        }
        // Every row not holding the office now names where it went, one an earlier switch left too.
        for (ResolvedArtifact& done : resolved_) {
            if (done.role == role && done.stem != stem) {
                done.weave_loaded = false;
                done.weave = loom::WeaveId{};
                done.switched_to = stem;
            }
        }
        for (ResolvedArtifact& done : resolved_) {
            if (done.stem != stem) {
                continue;
            }
            done.weave_loaded = true;
            done.weave = weave;
            done.role = role;
            done.switched_to.clear();
            if (done.image != image) {
                done.image = image;
                done.default_image = image == path_of_(stem) ||
                                     (!done.promoted_from.empty() && image == done.promoted_from);
            }
            return Recorded{true, std::string()};
        }
        ResolvedArtifact row;
        row.stem = stem;
        row.weave_loaded = true;
        row.weave = weave;
        row.role = role;
        row.image = image;
        row.default_image = image == path_of_(stem);
        resolved_.push_back(std::move(row));
        return Recorded{true, std::string()};
    }

    /// WHICH AUTHORED CHOICE FOR `role` IS RUNNING NOW, by its artifact: the resolved, not
    /// switched-away row loaded into `role`, or empty.
    std::string choice_holder(const std::string& role) const {
        for (const ResolvedArtifact& done : resolved_) {
            if (done.role == role && done.weave_loaded && done.switched_to.empty()) {
                return done.stem;
            }
        }
        return std::string();
    }

    /// IS THIS ROW'S RUNNING IMAGE THE FILE A RESTART LOADS? False for a stem this run
    /// did not resolve -- the answer a refusal about it carries.
    bool default_image_of(const std::string& stem) const noexcept {
        for (const ResolvedArtifact& done : resolved_) {
            if (done.stem == stem) {
                return done.default_image;
            }
        }
        return false;
    }

    /// What the last on-demand realization came to, taken away: it has one reader, and leaving it
    /// would announce it twice. `settled` false means nothing to say yet.
    struct Realized {
        bool settled = false;
        std::string stem;
        bool realized = false;
        std::string detail;
        bool default_image = false; ///< the image now running is the file a restart loads
    };

    Realized take_realization() {
        Realized out = realized_;
        realized_ = Realized{};
        return out;
    }

    /// The row realization is waiting on, or empty: derived from the cursor, never stored.
    const std::string& waiting_on() const noexcept {
        static const std::string kNone;
        return state_ == Realization::Waiting && cursor_ < plan_.artifacts.size()
                   ? plan_.artifacts[cursor_].stem
                   : kNone;
    }

    /// How many authored rows are behind the waiting row (0 when none), derived from the cursor so
    /// a presentation needs no copy of the plan.
    std::size_t behind() const noexcept {
        return state_ == Realization::Waiting && cursor_ < plan_.artifacts.size()
                   ? plan_.artifacts.size() - cursor_ - 1
                   : 0;
    }

    // ---- What is true right now --------------------------------------------------

    /// The authored intent this owner is realizing -- empty until `begin`.
    const LoadPlan& plan() const noexcept { return plan_; }

    /// Is `office` still to come -- a plan row loading a weave into it not yet settled? Pending is
    /// not a verdict: such a tool is never said to be unavailable.
    // WL-DESK-04 -- agents/workshop/desktop.md
    bool office_pending(std::string_view office) const {
        for (const ArtifactIntent& row : plan_.artifacts) {
            if (!row.weave.has_value() || row.weave->role != office) {
                continue;
            }
            const RowState now = state_of(row.stem);
            if (now == RowState::Authored || now == RowState::Loading ||
                now == RowState::Pending) {
                return true;
            }
        }
        return false;
    }

    /// How far realization has got with the plan as a whole.
    Realization state() const noexcept { return state_; }

    /// WHICH AUTHORED ROW REALIZATION IS AT: the index of the row in flight, or the
    /// row that refused, or `plan().artifacts.size()` once every row has resolved.
    std::size_t position() const noexcept { return cursor_; }

    /// What realization has made of the row named `stem` (a key: `check_plan` refuses a repeat),
    /// derived from the cursor, the resolved list and the row in flight -- never stored.
    RowState state_of(const std::string& stem) const noexcept {
        // Reloading first: the row is resolved and a conversation about it is open.
        if (reloading_.has_value() && resolved_[*reloading_].stem == stem &&
            state_ == Realization::Loading) {
            return RowState::Reloading;
        }
        // Loading next: a row a maker asked for is the row the cursor is still on.
        if (current_.stem == stem && state_ == Realization::Loading) {
            return RowState::Loading;
        }
        for (const ResolvedArtifact& done : resolved_) {
            if (done.stem == stem) {
                return done.switched_to.empty() ? RowState::Resolved : RowState::Switched;
            }
        }
        if (current_.stem == stem && state_ == Realization::Failed) {
            return RowState::Refused;
        }
        for (const SteppedOver& gone : stepped_over_) {
            if (gone.stem == stem) {
                return RowState::Unavailable;
            }
        }
        if (waiting_on() == stem) {
            return RowState::Pending;
        }
        return RowState::Authored;
    }

    /// WHY AN `Unavailable` OR `Refused` ROW IS NOT RUNNING: the refusing layer's own sentence,
    /// without the artifact prefix the banner writes in front of it. Empty for every other row.
    std::string reason_of(const std::string& stem) const {
        for (const SteppedOver& gone : stepped_over_) {
            if (gone.stem == stem) {
                return gone.why;
            }
        }
        if (state_ == Realization::Failed && current_.stem == stem) {
            return refused_why_;
        }
        return std::string();
    }

    /// The correlation of the load conversation currently outstanding, or 0.
    std::uint64_t asking() const noexcept { return answers_->asking(); }

    /// What this executor has actually put into the runtime, in the order it did.
    const std::vector<ResolvedArtifact>& resolved() const noexcept { return resolved_; }

    /// WHICH ARTIFACT STOPPED THE PLAN AND WHY, in the deepest layer's own words.
    /// Empty unless `state() == Realization::Failed`.
    const std::string& refusal() const noexcept { return refusal_; }

    /// What the whole plan has produced so far. `ok` is completion, not absence of failure: a
    /// loading plan and a waiting one both answer false, the waiting one with a named `waiting_on`.
    Executed outcome() const {
        Executed out;
        out.ok = state_ == Realization::Complete;
        out.refusal = refusal_;
        out.resolved = resolved_;
        out.waiting_on = waiting_on();
        out.unavailable = unavailable();
        for (const SteppedOver& gone : stepped_over_) {
            out.unavailable_stems.push_back(gone.stem);
        }
        return out;
    }

    /// The optional rows this run could not perform, in the order met, as the banner says them.
    /// Never cleared; the structured answer per row is `state_of` and `reason_of`.
    std::vector<std::string> unavailable() const {
        std::vector<std::string> out;
        out.reserve(stepped_over_.size());
        for (const SteppedOver& gone : stepped_over_) {
            out.push_back("artifact '" + gone.stem + "': " + gone.why);
        }
        return out;
    }

    /// Unmount one record's provider contribution, and only that; `Catalog::unmount` drops the
    /// contributions before the custody.
    bool unmount(const ResolvedArtifact& done) {
        if (!done.provider_mounted || done.provider.empty()) {
            return false;
        }
        return catalog_->unmount(done.provider);
    }

private:
    /// Perform everything knowable now, then return. A loop over authored rows, not a scheduler:
    /// each turn settles a row or opens exactly one conversation and leaves. Row N+1 does not
    /// begin until row N settled -- which is what makes an overlay's catalog order deterministic --
    /// and a waiting row stops the walk.
    void advance() {
        while (state_ == Realization::Advancing && cursor_ < plan_.artifacts.size()) {
            // Asked before anything is mounted or commanded. A yes stops the walk: the rows behind
            // stay `Authored`, and `realize` is the only door past this line.
            if (awaiting_ && awaiting_(plan_.artifacts[cursor_].stem)) {
                state_ = Realization::Waiting;
                // Told last, with the cursor and the state already settled.
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

    /// Perform one authored row as far as this process can take it now: mount, offer, load, in
    /// that order for both callers, because the order within one artifact is semantic law. On
    /// return `state_` is `Advancing` (settled), `Loading`, `Failed` or `Waiting` (an on-demand
    /// refusal, which leaves the frontier where the ask found it).
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
                    // The mount's own words: it already says what went wrong.
                    fail("provider mount refused: " + mounted.reason);
                    return;
                }
                current_.provider_mounted = true;
                current_.provider = mounted.provider;
                current_.contributed = mounted.contributed;
            }

            if (!artifact.weave.has_value()) {
                // A provider-only row is complete when its mount returns. The cursor stays on it:
                // moving the frontier is the walker's.
                settled_row();
                return;
            }
            current_.role = artifact.weave->role;

            // The offer goes up before the command is sent -- a consumer's first need is inside
            // `create()` -- and comes down in `answered()`.
            offer_.emplace(*operators_, path);
            current_.offer = offer_->outcome();
            if (current_.offer == op::OfferOutcome::VersionMismatch) {
                // A failed handoff refuses the artifact rather than loading it unoffered.
                const std::string why = "operator handoff refused: " + offer_->reason();
                offer_.reset();
                fail(why);
                return;
            }

            const std::uint64_t correlation = answers_->ask(manager_);
            if (correlation == 0) {
                // No conversation, so no command: an answer this host could not recognize would
                // leave the row `Loading` forever.
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
            // The boundary: the command is queued and nothing has delivered it. The host's loop
            // does, and the Manager's answer wakes `answered()`; turning the bus here is the defect
            // this owner exists to avoid.
            state_ = Realization::Loading;
            return;
        }
    }

    // ---- the reload arm -----------------------------------------------------------------------

    /// CAN THIS RESOLVED ROW BE RELOADED IN PLACE AT ALL? The two rules that are about
    /// the ROW rather than about the image: it loaded a weave, and it mounted no
    /// provider. `started` true means yes; false carries the words.
    Asked reloadable(const ResolvedArtifact& done) const {
        std::string refused = reload_refusal(done);
        return refused.empty() ? Asked{true, std::string()} : Asked{false, std::move(refused)};
    }

    /// RELOAD A LIVE ROW FROM ITS REBUILT PRODUCT. The host stages the product off the
    /// loaded path; a refusal there is the host's own sentence and nothing moves.
    Asked reload(std::size_t index, const std::string& recipe) {
        const ResolvedArtifact& done = resolved_[index];
        const Asked live = reloadable(done);
        if (!live.started) {
            return live;
        }
        if (!stage_) {
            return Asked{false, "artifact '" + done.stem +
                                    "' is live, and this host has no rule for staging a "
                                    "rebuilt image off the loaded path; nothing was reloaded"};
        }
        const Staged staged = stage_(done.stem, recipe, /*reload=*/true);
        if (!staged.ok) {
            return Asked{false, "the rebuilt '" + done.stem +
                                    "' could not be staged off the loaded path: " +
                                    staged.refusal + "; the running image is unchanged"};
        }
        return open_reload(index, staged.path, /*revert=*/false);
    }

    /// Open the one reload conversation over `path`: the three moves a load makes -- the offer,
    /// the conversation, the command sent as the booter -- then return; `answered()` settles it.
    Asked open_reload(std::size_t index, const std::string& path, bool revert) {
        const std::string& stem = resolved_[index].stem;
        offer_.emplace(*operators_, path);
        if (offer_->outcome() == op::OfferOutcome::VersionMismatch) {
            const std::string why = "artifact '" + stem +
                                    "': operator handoff refused: " + offer_->reason();
            offer_.reset();
            return Asked{false, why};
        }
        const std::uint64_t correlation =
            answers_->ask(manager_, loom::ReloadWeave::zen_name, loom::ReloadWeave::zen_version);
        if (correlation == 0) {
            offer_.reset();
            return Asked{false, "artifact '" + stem +
                                    "': no reload conversation could be opened with the weave "
                                    "this host was given as its Weave Manager; nothing was "
                                    "reloaded"};
        }
        const loom::WeaveId booter = voice_->speaker();
        bus_->send_as(booter, manager_,
                      loom::Message(loom::to_value(loom::ReloadWeave{stem, path}), booter, booter,
                                    correlation));
        reloading_ = index;
        reload_path_ = path;
        reverting_ = revert;
        resume_ = state_;
        state_ = Realization::Loading;
        return Asked{true, std::string()};
    }

    /// The reload conversation settled: the frontier is untouched and the owner goes back to the
    /// state the ask found it in; what differs is the row's image and the maker's sentence.
    void settle_reload() {
        ResolvedArtifact& row = resolved_[*reloading_];
        const op::OfferOutcome offer = offer_.has_value() ? offer_->outcome()
                                                          : op::OfferOutcome::NotAConsumer;
        offer_.reset();
        if (answers_->refused) {
            realized_ = Realized{true, row.stem, false,
                                 reload_refusal_words(row.stem, answers_->reason),
                                 row.default_image};
        } else {
            row.previous = row.image;
            row.image = reload_path_;
            row.offer = offer;
            row.default_image =
                row.image == path_of_(row.stem) ||
                (!row.promoted_from.empty() && row.image == row.promoted_from);
            const std::string id = std::to_string(row.weave.value);
            std::string said = reverting_
                                   ? "reverted: weave #" + id +
                                         " runs the image before the last reload again, and "
                                         "keeps its id and its state"
                                   : "reloaded in place -- weave #" + id +
                                         " keeps its id and its state";
            if (!row.default_image) {
                said += "; not the default yet -- the next launch runs the old code until "
                        "this image is promoted";
            }
            realized_ = Realized{true, row.stem, true, said, row.default_image};
        }
        reloading_.reset();
        reload_path_.clear();
        reverting_ = false;
        state_ = resume_;
    }

    /// The current row participated in full; kept in authored order. It does not move the cursor:
    /// that is the walker's. A row a maker asked for leaves its sentence before the walk resumes,
    /// so a later row's refusal or wait cannot overwrite the answer they are owed.
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
            realized_ = Realized{true, current_.stem, true, said, current_.default_image};
            on_demand_ = false;
        }
        resolved_.push_back(std::move(current_));
        current_ = ResolvedArtifact{};
    }

    /// Why an ask cannot be answered right now, for the three states not between rows.
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

    /// This row refused: roll back exactly what it introduced (its own mount; earlier rows stand),
    /// then stop -- a host carrying on would run a project nobody authored. `current_` stays, so
    /// `state_of` can answer `Refused`. A row a maker asked for does not fail the arrangement,
    /// which in Workshop would end the process: its refusal is left for publishing and the
    /// frontier goes back to `Waiting`, where a corrected build can reach it.
    void fail(const std::string& why) {
        (void)unmount(current_);
        const std::string said = "artifact '" + current_.stem + "': " + why;
        // An optional row that refused is an unavailable tool, not a refused project: record it
        // and step over it. The caller moves the frontier; moving it here too would skip the next
        // row. A row a maker asked for keeps its own answer below.
        if (!on_demand_ && cursor_ < plan_.artifacts.size() &&
            plan_.artifacts[cursor_].optional) {
            stepped_over_.push_back(SteppedOver{current_.stem, why});
            current_ = ResolvedArtifact{};
            state_ = Realization::Advancing;
            return;
        }
        if (on_demand_) {
            realized_ = Realized{true, current_.stem, false, said};
            on_demand_ = false;
            current_ = ResolvedArtifact{};
            state_ = Realization::Waiting;
            return;
        }
        refusal_ = said;
        refused_why_ = why;
        state_ = Realization::Failed;
        announce();
    }

    /// Every authored row settled -- performed, or optional and recorded `Unavailable` -- reached
    /// only by the walk running off the end. A fact about realization, not the process.
    void complete() {
        state_ = Realization::Complete;
        announce();
    }

    /// Tell the host, if it asked, that realization came to rest: every row settled, a refusal, or
    /// a row waiting on the maker -- the last may happen more than once in a run.
    void announce() {
        if (settled_) {
            settled_(outcome());
        }
    }

    loom::Switchboard* bus_;
    op::Catalog* catalog_;
    const op::OperatorHostSurface* operators_;
    /// The participant that speaks and hears for this owner, never null: the constructor is the
    /// only place the link is made.
    PlanBooter* voice_;
    loom::WeaveId manager_;
    BootAnswers* answers_;
    ArtifactPath path_of_;
    Settled settled_;
    /// The one question this owner cannot answer for itself. Empty means nothing waits.
    AwaitingBuild awaiting_;
    /// The two disk acts this owner cannot perform for itself; empty means the arms that need one
    /// refuse in words.
    StageArtifact stage_;
    PromoteImage promote_;

    // ---- the walk's own state ------------------------------------------------------------------

    /// The authored intent, owned.
    LoadPlan plan_;
    /// Which authored row is being realized.
    std::size_t cursor_ = 0;
    /// The optional rows that refused and were stepped over, with the refusing layer's sentence.
    struct SteppedOver {
        std::string stem;
        std::string why;
    };
    std::vector<SteppedOver> stepped_over_;
    /// The refusing layer's sentence for the row that stopped the plan; `refusal_` prefixes it.
    std::string refused_why_;
    /// The row being built.
    ResolvedArtifact current_;
    /// The offer around the current load: neither copyable nor movable, so `std::optional`
    /// constructs it in place and `reset()` withdraws it. Empty outside a load.
    std::optional<op::OperatorOffer> offer_;
    /// Where the whole plan is.
    Realization state_ = Realization::Unstarted;
    /// Which artifact stopped it, and why.
    std::string refusal_;
    /// What this executor has put into the runtime, in the order it did.
    std::vector<ResolvedArtifact> resolved_;

    // ---- a row a maker asked for ----------------------------------------------------------------

    /// Is the row in flight one a maker asked for? It decides what a refusal of it means (the
    /// arrangement stops, or the frontier goes back to waiting) and who is owed a sentence.
    bool on_demand_ = false;
    /// What the last on-demand realization came to, until somebody takes it.
    Realized realized_;

    // ---- a reload in place, while its one conversation is open ----------------------------------

    /// Which resolved row is being reloaded, set exactly while a `zen.ReloadWeave` is outstanding.
    std::optional<std::size_t> reloading_;
    /// The image the open reload is about: what `image` becomes on `Ack`.
    std::string reload_path_;
    /// Is the open reload a revert? It decides the sentence only.
    bool reverting_ = false;
    /// Where the owner was when the reload was asked for, and goes back to: a reload moves no
    /// frontier.
    Realization resume_ = Realization::Complete;
};

/// Defined here because the owner it hands the fact to is declared above.
inline void PlanBooter::wake(loom::Mail& mail) {
    if (owner_ == nullptr) {
        return;
    }
    owner_->answered();
    // A row a maker asked for is announced once, as the answer to the ask that is in flight: the
    // owner leaves the fact for one reader, and it holds one realization conversation at a time.
    const PlanExecutor::Realized settled = owner_->take_realization();
    if (settled.settled) {
        realized(settled.stem, settled.realized, settled.detail, settled.default_image, mail);
    }
}

/// Put the Builder's offer to the owner and publish what it made of it -- a refusal as loudly as
/// an acceptance, both as `ArtifactRealized`, after `RealizationAsked`. With no owner wired this
/// says nothing. A weave row is still loading here, and its answer is published later from the
/// load's settling path, naming this ask.
inline void PlanBooter::on(const zengine::builder::OfferArtifact& offer, loom::Mail& mail) {
    if (owner_ == nullptr) {
        return;
    }
    const PlanExecutor::Asked asked = owner_->realize(offer.artifact, offer.recipe);
    const std::int64_t number = heard(offer.artifact, zengine::builder::realization_act::kOffer,
                                      asked.started, asked.refusal, mail);
    if (!asked.started) {
        (void)mail.publish(zengine::builder::ArtifactRealized{
            offer.artifact, false, asked.refusal, owner_->default_image_of(offer.artifact), 0});
        return;
    }
    in_flight_ = number;
    const PlanExecutor::Realized settled = owner_->take_realization();
    if (settled.settled) {
        realized(settled.stem, settled.realized, settled.detail, settled.default_image, mail);
    }
}

/// Ask the owner, publish what it said: a promotion is synchronous, so its answer names its ask in
/// the same delivery.
inline void PlanBooter::on(const zengine::builder::PromoteArtifact& ask, loom::Mail& mail) {
    if (owner_ == nullptr) {
        return;
    }
    const PlanExecutor::Promoted done = owner_->promote(ask.artifact);
    const std::int64_t number =
        heard(ask.artifact, zengine::builder::realization_act::kPromote, true, std::string(), mail);
    (void)mail.publish(zengine::builder::ArtifactPromoted{ask.artifact, done.ok, done.detail, number});
}

/// A revert is a reload: an accepted one is answered later from the `Ack` path, naming its ask; a
/// refusal now.
inline void PlanBooter::on(const zengine::builder::RevertArtifact& ask, loom::Mail& mail) {
    if (owner_ == nullptr) {
        return;
    }
    const PlanExecutor::Asked asked = owner_->revert(ask.artifact);
    const std::int64_t number = heard(ask.artifact, zengine::builder::realization_act::kRevert,
                                      asked.started, asked.refusal, mail);
    if (!asked.started) {
        (void)mail.publish(zengine::builder::ArtifactRealized{
            ask.artifact, false, asked.refusal, owner_->default_image_of(ask.artifact), 0});
        return;
    }
    in_flight_ = number;
}

} // namespace zengine::workshop::load

#endif // ZENGINE_WORKSHOP_LOAD_EXECUTE_HPP
