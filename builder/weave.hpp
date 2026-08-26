// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_BUILDER_WEAVE_HPP
#define ZENGINE_BUILDER_WEAVE_HPP

// The Builder tool: an ORDINARY weave that knows which recipes this project has,
// which ARTIFACT each of them produces, follows how the build of one is going,
// and says so.
//
// ORDINARY IS THE CLAIM. Its grant is three rules -- it may order the build runner,
// it may publish what it knows, and (BLD-1) it may say that an artifact a maker
// asked to have realized is now on disk. It holds nothing else: no process, no
// command, no build tree, no source path, no kernel reach, no manager, no surface,
// no timer. Everything a maker sees of a build passes through those rules, so "what
// did giving Zengine a Build button actually cost in authority" has a short and
// checkable answer.
//
// IT IS NOT A PANEL AND IT DOES NOT KNOW ABOUT ONE. Nothing here mentions
// Workshop, a screen, a canvas or a row. It publishes `BuildStatus` and whoever
// wants to present it may; the Builder PANEL in workshop/panel.hpp is one such
// presentation, and closing that panel does not reach this weave at all. The
// visible proof is `builds`: the tool counts its own asks, so a panel that was
// closed and reopened shows the tool's running total rather than starting again
// from zero -- a panel that owned the state could not do that, and that is the
// difference this phase exists to keep.
//
// ---------------------------------------------------------------------------
// IT REMEMBERS A BUILD THAT IS STILL HAPPENING (ASYNC-1), which was BLD-0's gap.
// The runner reports four kinds of observation and this weave folds them into one
// current picture:
//
//     BuildStarted    -> running, and this is the operation and the command
//     BuildOutput     -> ...and this is the newest thing it said
//     BuildFinished   -> the process exited; ...and then, see below
//     BuildNotStarted -> nothing ran, and this is why
//
// FACTS AND STATE ARE DIFFERENT THINGS AND ARE KEPT APART. What arrives is an
// observation about a moment; what this weave holds is a picture of where things
// stand, recomputed from each arrival and published whole. That split is not
// bookkeeping: BLD-0's first live run announced a build that had finished
// minutes earlier because a presentation could not tell a fact it LEARNED from
// an event it WATCHED, and only a design where the two are separate lets a
// panel opened mid-build be told "running" without being told "it just started".
//
// ---------------------------------------------------------------------------
// A PROCESS EXITING ZERO IS NOT AN ARTIFACT (BLD-1), and this weave is where the
// difference is spent.
//
// The runner owns process custody and only process custody: it can honestly say a
// child exited and with what status, and nothing more. Whether the FILE a recipe
// said it would produce is now on disk is a different question about a different
// subject, and this weave is the one that holds recipe-to-artifact knowledge --
// so it is the one that looks:
//
//     BuildFinished, status 0, artifact present   -> succeeded
//     BuildFinished, status 0, artifact ABSENT    -> kNoArtifact, and it says so
//     BuildFinished, status non-zero              -> FAILED, and the artifact is
//                                                    never consulted
//
// THE STALE-ARTIFACT TRAP IS CLOSED BY THE ORDER OF THOSE THREE, not by a
// timestamp heuristic. A failed build leaves whatever was at the destination
// before -- possibly a perfectly good artifact from an earlier build -- and the
// exit status is checked FIRST, so a previous success can never make the current
// operation look successful. The stamp this weave takes when a build STARTS is
// therefore not a correctness mechanism; it is how a maker is told whether their
// build actually relinked anything, which an incremental build often does not.
//
// ---------------------------------------------------------------------------
// ONE BUILD AT A TIME, AND IT IS A POLICY OF THIS TOOL. A `BuildRequested` that
// arrives while a build is still going is refused with a sentence naming the
// operation already running, and nothing is ordered. That is a product decision
// -- it is emphatically NOT a limitation of the mechanism underneath: the runner
// holds a vector, mints an identity per operation, and observes each of them
// independently, so the day a Builder wants two builds at once it changes this
// file and not that one. Written down here because a refusal whose reason lives
// in the wrong layer is a refusal nobody can lift.
//
// WHAT IT DELIBERATELY DOES NOT DO:
//
//   - it does not choose the recipes. The host does, at construction, out of an
//     authored file. A tool that could be told a new recipe over the wire would
//     be a tool whose reach is whatever somebody types, which is the thing this
//     package is arranged not to build.
//   - it does not hold a command, and cannot describe one until the runner has
//     told it what is actually running. Before the first build the panel says so.
//   - it does not hold a build tree, a source path, a package prefix or a link
//     list. It holds an identity, an artifact stem and the one file that stem
//     means -- which is exactly what its two questions need and no more.
//   - it does not keep the build log. It keeps a bounded tail of what the current
//     operation has said -- enough for the rows a panel has -- and the whole
//     stream stays on the bus, where a later presentation phase can pick it up
//     without this weave having become a log server.
//   - it does not poll, ask "is it done yet?", or hold a timer. It hears. The
//     one participant that polls anything is the runner, on its own handles.
//   - it does not load, unload, replace or reload anything, and it holds no
//     realization state of its own. When a maker asked for BUILD & REALIZE and
//     the artifact arrived, it makes ONE offer -- `OfferArtifact` -- and the
//     realization owner decides, in its own words, what that is worth. What comes
//     back (`ArtifactRealized`) is folded in for the panel and changes nothing
//     about the build. ⚠ An OFFER and not an order: this weave holds no
//     realization authority and every eligibility rule is the owner's.
//   - it does not cancel, and cannot: there is no shape for it, and inventing
//     one would mean deciding what a maker's "stop" claims about a process that
//     may already have finished.

#include "builder/recipe.hpp"
#include "builder/vocabulary.hpp"

#include <zen/weave.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace zengine::builder {

/// The most of one operation's output this tool remembers.
///
/// It is a WORKING TAIL and not a log: what a presentation is given is the last
/// few lines of it, and what anybody who wants the whole stream should read is
/// the `BuildOutput` facts themselves, which is where all of it is. The OLDEST
/// characters are dropped, for the reason that rule is always chosen here -- the
/// end of a build's output is the part that says what went wrong.
inline constexpr std::size_t kMaxRemembered = 8u * 1024u;

/// How many of those lines a published status carries.
inline constexpr std::size_t kDetailLines = 3;

/// WHAT A FILE LOOKED LIKE AT ONE MOMENT.
///
/// PRESENCE, SIZE AND WHEN IT LAST CHANGED -- taken before a build begins and again
/// after it ends, so that "your build produced a new artifact" and "your build was
/// already up to date" can be told apart in the sentence a maker reads. It is
/// DELIBERATELY NOT the artifact-success test: that is `status == 0 && present`, in
/// that order, because a build system that reported success is the only party
/// entitled to say a file is current and a timestamp comparison would call an
/// honest incremental no-op a failure.
struct ArtifactStamp {
    bool present = false;
    std::uintmax_t size = 0;
    std::int64_t changed = 0; ///< an opaque, comparable modification time

    friend bool operator==(const ArtifactStamp&, const ArtifactStamp&) = default;
};

/// LOOK AT ONE FILE, NOW. Total: a path that is not there, cannot be read, or is
/// not a regular file all answer `present = false`, and nothing throws.
inline ArtifactStamp stamp_of(const std::string& path) {
    ArtifactStamp out;
    if (path.empty()) {
        return out;
    }
    std::error_code ec;
    const std::filesystem::path file(path);
    if (!std::filesystem::is_regular_file(file, ec) || ec) {
        return out;
    }
    out.present = true;
    out.size = std::filesystem::file_size(file, ec);
    if (ec) {
        out.size = 0;
    }
    const std::filesystem::file_time_type when = std::filesystem::last_write_time(file, ec);
    out.changed = ec ? 0 : static_cast<std::int64_t>(when.time_since_epoch().count());
    return out;
}

/// The tool's memory. All of it is what a presentation is shown, plus two
/// tallies that only an operator would ask for.
///
/// v3 (BLD-1): `target` became `recipe`, `recipe` became `command`, and the
/// artifact and realization fields joined -- the tool now answers two questions
/// about one ask, and neither is derivable from the other.
struct BuilderState {
    std::string recipe;               ///< the recipe this picture is about
    std::string artifact;             ///< the artifact stem that recipe produces
    std::int64_t outcome = outcome::kNeverBuilt;
    std::int64_t status = 0;          ///< the last process's exit status
    std::string command;              ///< what the runner said it ran, once it has run
    std::string detail;               ///< the last thing said about this recipe
    std::int64_t builds = 0;          ///< asks this tool has taken, ever
    std::int64_t stray = 0;           ///< observations that were about somebody else's work
    std::int64_t op = 0;              ///< the operation this picture is about
    std::int64_t chunks = 0;          ///< output observations folded in for it
    bool realize = false;             ///< the current ask was BUILD & REALIZE
    std::int64_t realization = realization::kNotAsked;
    std::string realized_detail;      ///< realization's own words, when it has said any
    std::int64_t offered = 0;         ///< artifacts this tool has offered for realization

    ZEN_EXPOSE();
    ZEN_SHAPE(BuilderState, 3, ZEN_FIELD(recipe), ZEN_FIELD(artifact), ZEN_FIELD(outcome),
              ZEN_FIELD(status), ZEN_FIELD(command), ZEN_FIELD(detail), ZEN_FIELD(builds),
              ZEN_FIELD(stray), ZEN_FIELD(op), ZEN_FIELD(chunks), ZEN_FIELD(realize),
              ZEN_FIELD(realization), ZEN_FIELD(realized_detail), ZEN_FIELD(offered));
};

class BuilderWeave
    : public loom::WeaveBase<BuilderWeave, BuilderState,
                             loom::Accept<BuildRequested, StatusRequested, BuildStarted,
                                          BuildOutput, BuildFinished, BuildNotStarted,
                                          ArtifactRealized>,
                             loom::Emit<RunBuild, BuildStatus, RecipeCatalog, OfferArtifact>> {
public:
    /// THE RECIPE VIEWS ARRIVE AT CONSTRUCTION, FROM THE HOST, and they are a plain
    /// member rather than part of the weave's state -- the runner's reason, one layer
    /// out: `ZEN_SHAPE` state is poke-writable by design, and a poke that could write
    /// a new artifact path in here would be a poke that could make this tool announce
    /// somebody else's file as a build product.
    explicit BuilderWeave(std::vector<RecipeView> recipes) : recipes_(std::move(recipes)) {}

    /// SAY WHAT YOU ARE. The message a presentation sends when it opens, so a
    /// fresh panel shows a LIVE tool rather than an empty one -- including what
    /// this project can build at all, which is the first thing a maker needs and
    /// the one fact that exists before any build has happened, and including a
    /// build that is running right now.
    ///
    /// TWO SHAPES, BECAUSE THEY ANSWER TWO QUESTIONS THAT CHANGE AT DIFFERENT
    /// RATES. The catalog is fixed for the life of the process; the status moves
    /// on every line a compiler says. Publishing the catalog on every status would
    /// put the whole thing on the bus hundreds of times per build.
    void on(const StatusRequested&, loom::Mail& mail) {
        RecipeCatalog said;
        said.recipes.reserve(recipes_.size());
        for (const RecipeView& r : recipes_) {
            said.recipes.push_back(RecipeSummary{r.id, r.artifact});
        }
        (void)mail.publish(std::move(said));
        say(mail);
    }

    /// BUILD THE RECIPE YOU KNOW BY THIS NAME.
    ///
    /// The name is checked against the catalog this tool holds, and a name it does
    /// not hold is a refusal that says how many it does. That check is not
    /// politeness: it is the reason a `BuildRequested` arriving from anywhere at all
    /// -- a panel, a terminal participant, a test -- cannot widen what this program
    /// will build.
    void on(const BuildRequested& ask, loom::Mail& mail) {
        const RecipeView* chosen = view_named(recipes_, ask.recipe);
        if (chosen == nullptr) {
            state_.outcome = outcome::kUnknownRecipe;
            state_.detail = recipes_.empty()
                                ? std::string("this Builder holds no recipes at all")
                                : "this Builder holds no recipe called `" + ask.recipe +
                                      "` (it holds " + std::to_string(recipes_.size()) + ")";
            say(mail);
            return;
        }
        if (still_going(state_.outcome)) {
            // THE ONE-AT-A-TIME POLICY, and it refuses in the tool's own voice
            // rather than by dropping the ask. `builds` does NOT move, because
            // nothing was ordered -- the counter is asks this tool TOOK, and a
            // number that also counted the ones it turned down would answer two
            // questions at once and get one of them wrong.
            state_.detail = state_.op == 0
                                ? std::string("a build was already asked for and has not "
                                              "started yet")
                                : "a build is already running: operation #" +
                                      std::to_string(state_.op);
            say(mail);
            return;
        }
        ++state_.builds;
        state_.recipe = chosen->id;
        state_.artifact = chosen->artifact;
        state_.outcome = outcome::kAsked;
        state_.status = 0;
        state_.op = 0;
        state_.chunks = 0;
        state_.detail.clear();
        state_.realize = ask.realize;
        state_.realization = ask.realize ? realization::kAsked : realization::kNotAsked;
        state_.realized_detail.clear();
        remembered_.clear();
        // WHAT WAS THERE BEFORE, TAKEN BEFORE ANYTHING IS ORDERED. It is what lets
        // the ending say `produced` or `unchanged`; it is not, and must not become,
        // the test for whether this build succeeded (see this file's header).
        before_ = stamp_of(chosen->path);
        // THE PREVIOUS COMMAND IS NOT CLEARED WHEN THE RECIPE IS THE SAME, and that
        // is the smaller lie of the two available: it describes how THIS recipe is
        // built, so it is about to be replaced by the same sentence. When the recipe
        // CHANGED it is cleared, because it then describes something else entirely.
        if (built_ != chosen->id) {
            state_.command.clear();
        }
        //
        // SAID BEFORE THE ORDER IS GIVEN, so the record reads in the order the
        // facts became true.
        say(mail);
        (void)mail.send_to_role(kBuildRunnerRole, RunBuild{state_.recipe});
    }

    /// A PROCESS BEGAN. This is the moment BLD-0 had no way to observe.
    void on(const BuildStarted& began, loom::Mail& mail) {
        if (!mine(began.recipe)) {
            return;
        }
        state_.op = began.op;
        state_.command = began.command;
        state_.outcome = outcome::kRunning;
        state_.status = 0;
        state_.chunks = 0;
        state_.detail.clear();
        remembered_.clear();
        say(mail);
    }

    /// IT SAID SOMETHING. Folded in and republished, which is what makes a
    /// running build visibly alive rather than merely believed to be.
    void on(const BuildOutput& said, loom::Mail& mail) {
        if (!mine(said.recipe) || !about_current(said.op)) {
            return;
        }
        ++state_.chunks;
        remember(said.text);
        state_.detail = tail_lines(remembered_, kDetailLines);
        say(mail);
    }

    /// IT EXITED -- AND ONLY NOW IS THERE AN ARTIFACT QUESTION TO ASK.
    ///
    /// THE EXIT STATUS IS CONSULTED FIRST AND THE FILE SECOND, and the order is the
    /// whole staleness guarantee: a failed build is `FAILED` whatever is sitting at
    /// the destination, so an artifact left there by an earlier success can never be
    /// mistaken for this build's product.
    void on(const BuildFinished& done, loom::Mail& mail) {
        if (!mine(done.recipe) || !about_current(done.op)) {
            return;
        }
        state_.status = done.status;
        const std::string said = tail_lines(remembered_, kDetailLines);
        if (done.status != 0) {
            state_.outcome = outcome::kFailed;
            state_.detail = said;
            if (state_.realize) {
                // A FAILED BUILD OFFERS NOTHING. Said in the status rather than left
                // as an absence, because a maker who pressed BUILD & REALIZE is owed
                // the reason nothing was realized.
                state_.realization = realization::kRefused;
                state_.realized_detail = "the build failed, so nothing was offered to the "
                                         "project";
            }
            say(mail);
            return;
        }
        const RecipeView* chosen = view_named(recipes_, done.recipe);
        const ArtifactStamp after = chosen == nullptr ? ArtifactStamp{} : stamp_of(chosen->path);
        if (!after.present) {
            // A GREEN BUILD WITH NO PRODUCT. It is neither success nor failure and is
            // reported as neither: the process is fine and the project is not.
            state_.outcome = outcome::kNoArtifact;
            state_.detail = "the build succeeded and `" + state_.artifact +
                            "` is not at " +
                            (chosen == nullptr ? std::string("(no recipe)") : chosen->path) +
                            (said.empty() ? std::string() : " | " + said);
            if (state_.realize) {
                state_.realization = realization::kRefused;
                state_.realized_detail = "the expected artifact was not produced, so nothing "
                                         "was offered to the project";
            }
            say(mail);
            return;
        }
        state_.outcome = outcome::kSucceeded;
        built_ = done.recipe;
        const bool moved = !(after == before_);
        state_.detail = std::string(moved ? "built " : "already up to date: ") +
                        state_.artifact + (said.empty() ? std::string() : " | " + said);
        if (!state_.realize) {
            say(mail);
            return;
        }
        // ONE FACT, AND THE DECISION IS SOMEBODY ELSE'S. This tool does not know
        // whether the project participates in this artifact, whether it is already
        // loaded, or what a load would mean; it knows a maker asked, a build worked,
        // and the file is there. Everything after this line belongs to the
        // realization owner, which refuses in its own words when it must.
        ++state_.offered;
        state_.realization = realization::kOffered;
        state_.realized_detail = "offered to the project";
        say(mail);
        (void)mail.publish(
            OfferArtifact{state_.op, state_.recipe, state_.artifact, chosen->path});
    }

    /// NOTHING RAN, and this is why.
    ///
    /// It may name the operation (a child that never became its program) or name
    /// none at all (a recipe nobody holds, a build tree that is not there); both are
    /// answers to an ask this tool made, and `about_current` accepts either because
    /// the tool's own `op` is 0 for exactly as long as no operation has been
    /// announced.
    void on(const BuildNotStarted& never, loom::Mail& mail) {
        if (!mine(never.recipe) || !about_current(never.op)) {
            return;
        }
        state_.outcome = outcome::kNotStarted;
        state_.status = 0;
        if (!never.command.empty()) {
            state_.command = never.command;
        }
        state_.detail = never.trouble;
        if (state_.realize) {
            state_.realization = realization::kRefused;
            state_.realized_detail = "no build ran, so nothing was offered to the project";
        }
        say(mail);
    }

    /// THE PROJECT ANSWERED. Folded in and republished; it changes nothing about the
    /// build, which is the point of it being a second field.
    ///
    /// IT IS MATCHED BY ARTIFACT AND NOT BY OPERATION, because realization has no
    /// operation: what it answers about is an artifact stem, and an answer about an
    /// artifact this tool did not just offer is somebody else's conversation.
    void on(const ArtifactRealized& answer, loom::Mail& mail) {
        if (state_.realization != realization::kOffered || answer.artifact != state_.artifact) {
            ++state_.stray;
            return;
        }
        state_.realization = answer.realized ? realization::kRealized : realization::kRefused;
        state_.realized_detail = answer.detail;
        say(mail);
    }

    /// What this tool knows, for a suite that wants to check where a message
    /// left it. Read-only, and it is deliberately MORE than the published
    /// status: `stray` is a fact an operator or a test may want and a
    /// presentation has no business showing.
    const BuilderState& known() const { return state_; }

    /// The recipes this tool holds -- the host's own list coming back. Read-only, and
    /// no weave learns it this way.
    const std::vector<RecipeView>& recipes() const { return recipes_; }

    /// What the artifact looked like before the current build began.
    const ArtifactStamp& before() const { return before_; }

private:
    /// IS THIS ABOUT THE RECIPE THIS TOOL IS FOLLOWING?
    ///
    /// NOT THIS TOOL'S FACT is counted rather than dropped in silence: a tally is
    /// what makes "this never happens" something an operator can check instead of
    /// something a comment asserts. Recording it would make this tool's own
    /// history a mixture of two recipes' outcomes, which is exactly the confusion
    /// `recipe` exists to prevent.
    bool mine(const std::string& recipe) {
        if (recipe == state_.recipe) {
            return true;
        }
        ++state_.stray;
        return false;
    }

    /// IS THIS ABOUT THE OPERATION THIS TOOL IS FOLLOWING?
    ///
    /// The second half of the same question, and the half that only exists once
    /// operations do. An observation about a different operation of the same
    /// recipe is somebody else's conversation -- this tool orders one build at a
    /// time, so it can only ever be following one -- and folding it in would
    /// mix two builds' output into one picture.
    bool about_current(std::int64_t op) {
        if (op == state_.op) {
            return true;
        }
        ++state_.stray;
        return false;
    }

    void remember(const std::string& text) {
        remembered_ += text;
        remembered_ += '\n';
        if (remembered_.size() > kMaxRemembered) {
            remembered_.erase(0, remembered_.size() - kMaxRemembered);
        }
    }

    void say(loom::Mail& mail) {
        (void)mail.publish(BuildStatus{state_.recipe, state_.artifact, state_.outcome,
                                       state_.status, state_.command, state_.detail,
                                       state_.builds, state_.op, state_.chunks, state_.realize,
                                       state_.realization, state_.realized_detail});
    }

    /// The recipes this tool may be asked for -- identity, artifact, and the one file
    /// that artifact means. Never state; see the constructor.
    std::vector<RecipeView> recipes_;

    /// The current operation's output so far, bounded.
    ///
    /// A PLAIN MEMBER AND NOT STATE, because it is a working buffer rather than
    /// something anybody should read from outside: what this tool SAYS about it
    /// is `detail`, which is published, bounded to the rows a panel has, and the
    /// same for every reader. A second, larger copy reachable by a poke would be
    /// a second answer to "what did the build say".
    std::string remembered_;

    /// What the expected artifact looked like when the current build was ordered.
    ArtifactStamp before_;

    /// The last recipe this tool saw succeed -- which is how it knows whether the
    /// command it is still showing is about the recipe now being asked for.
    std::string built_;
};

} // namespace zengine::builder

#endif // ZENGINE_BUILDER_WEAVE_HPP
