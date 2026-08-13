// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_BUILDER_WEAVE_HPP
#define ZENGINE_BUILDER_WEAVE_HPP

// The Builder tool: an ORDINARY weave that knows the name of one buildable
// thing, remembers what happened to it, and says so.
//
// ORDINARY IS THE CLAIM. Its grant is two rules — it may order the build runner,
// and it may publish what it knows — and it holds nothing else: no process, no
// command, no file, no kernel reach, no manager, no surface. Everything a maker
// sees of a build passes through those two rules, so "what did giving Zengine a
// Build button actually cost in authority" has a short and checkable answer.
//
// IT IS NOT A PANEL AND IT DOES NOT KNOW ABOUT ONE. Nothing here mentions
// Workshop, a screen, a canvas or a row. It publishes `BuildStatus` and whoever
// wants to present it may; the Builder PANEL in workshop/panel.hpp is one such
// presentation, and closing that panel does not reach this weave at all. The
// visible proof is `builds`: the tool counts its own asks, so a panel that was
// closed and reopened shows the tool's running total rather than starting again
// from zero — a panel that owned the state could not do that, and that is the
// difference this phase exists to keep.
//
// WHY IT PUBLISHES RATHER THAN BEING READ. A presentation that reached in and
// read this weave's members would be the second owner of its facts, and the two
// would drift the first time one of them cached something. Publishing keeps one
// owner: the tool states its condition after every change, and every reader
// hears the same sentence at the same moment.
//
// WHAT IT DELIBERATELY DOES NOT DO:
//
//   - it does not choose the target. The host does, at construction, and the
//     tool carries that one name for its whole life. A tool that could be told a
//     new target over the wire would be a tool whose reach is whatever somebody
//     types, which is the thing BLD-0 is arranged not to build.
//   - it does not hold a command, and cannot describe one until the runner has
//     told it what was actually run. Before the first build the panel says so.
//   - it does not queue. A second `BuildRequested` while one is outstanding is
//     ordered too; BLD-0's runner builds synchronously, so this cannot arise
//     from a maker's hand, and inventing a queue for a case that cannot happen
//     would be machinery with no witness.

#include "builder/vocabulary.hpp"

#include <zen/weave.hpp>

#include <cstdint>
#include <string>
#include <utility>

namespace zengine::builder {

/// The tool's memory. All of it is what a presentation is shown, plus two
/// tallies that only an operator would ask for.
struct BuilderState {
    std::string target;               ///< the ONE name this tool knows
    std::int64_t outcome = outcome::kNeverBuilt;
    std::int64_t status = 0;          ///< the last process's exit status
    std::string recipe;               ///< what the runner said it ran, once it has run
    std::string detail;               ///< the last thing said about this target
    std::int64_t builds = 0;          ///< asks this tool has taken, ever
    std::int64_t stray = 0;           ///< answers that were about somebody else's target

    ZEN_EXPOSE();
    ZEN_SHAPE(BuilderState, 1, ZEN_FIELD(target), ZEN_FIELD(outcome), ZEN_FIELD(status),
              ZEN_FIELD(recipe), ZEN_FIELD(detail), ZEN_FIELD(builds), ZEN_FIELD(stray));
};

class BuilderWeave
    : public loom::WeaveBase<BuilderWeave, BuilderState,
                             loom::Accept<BuildRequested, StatusRequested, BuildOutcome>,
                             loom::Emit<RunBuild, BuildStatus>> {
public:
    explicit BuilderWeave(std::string target) { state_.target = std::move(target); }

    /// SAY WHAT YOU ARE. The message a presentation sends when it opens, so a
    /// fresh panel shows a LIVE tool rather than an empty one — including the
    /// target's name, which is the first thing a maker needs and the one fact
    /// that exists before any build has happened.
    void on(const StatusRequested&, loom::Mail& mail) { say(mail); }

    /// BUILD THE THING YOU KNOW BY THIS NAME.
    ///
    /// The name is checked against the one this tool holds, and a mismatch is a
    /// refusal that says what it does know. That check is not politeness: it is
    /// the reason a `BuildRequested` arriving from anywhere at all — a panel, a
    /// terminal participant, a test — cannot widen what this program will build.
    void on(const BuildRequested& ask, loom::Mail& mail) {
        if (ask.target != state_.target) {
            state_.outcome = outcome::kUnknownTarget;
            state_.detail = "this Builder knows `" + state_.target + "` and nothing else";
            say(mail);
            return;
        }
        ++state_.builds;
        state_.outcome = outcome::kAsked;
        state_.status = 0;
        state_.detail.clear();
        // SAID BEFORE THE ORDER IS GIVEN, so the record reads in the order the
        // facts became true. It changes nothing about what a maker SEES while a
        // synchronous build runs -- the runner blocks the same pump that would
        // have to deliver a repaint -- and that limit is BLD-0's, stated in the
        // report rather than disguised by shuffling these two lines.
        say(mail);
        (void)mail.send_to_role(kBuildRunnerRole, RunBuild{state_.target});
    }

    /// The runner answered.
    ///
    /// `started` and `status` are kept apart, because a build that FAILED and a
    /// build that never began are different things to tell a maker and only the
    /// first of them has an exit status worth reading.
    void on(const BuildOutcome& done, loom::Mail& mail) {
        if (done.target != state_.target) {
            // NOT THIS TOOL'S FACT. Recording it would make this tool's own
            // history a mixture of two targets' outcomes, which is exactly the
            // confusion `target` exists to prevent. It is counted rather than
            // dropped in silence: a tally is what makes "this never happens"
            // something an operator can check instead of something a comment
            // asserts.
            ++state_.stray;
            return;
        }
        state_.status = done.status;
        state_.recipe = done.recipe;
        state_.detail = done.detail;
        if (!done.started) {
            state_.outcome = outcome::kNotStarted;
        } else {
            state_.outcome = done.status == 0 ? outcome::kSucceeded : outcome::kFailed;
        }
        say(mail);
    }

    /// What this tool knows, for a suite that wants to check where a message
    /// left it. Read-only, and it is deliberately MORE than the published
    /// status: `stray` is a fact an operator or a test may want and a
    /// presentation has no business showing.
    const BuilderState& known() const { return state_; }

private:
    void say(loom::Mail& mail) {
        (void)mail.publish(BuildStatus{state_.target, state_.outcome, state_.status,
                                       state_.recipe, state_.detail, state_.builds});
    }
};

} // namespace zengine::builder

#endif // ZENGINE_BUILDER_WEAVE_HPP
