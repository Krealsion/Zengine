// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_BUILDER_WEAVE_HPP
#define ZENGINE_BUILDER_WEAVE_HPP

// The Builder tool: an ORDINARY weave that knows the name of one buildable
// thing, follows how its build is going, and says so.
//
// ORDINARY IS THE CLAIM. Its grant is two rules -- it may order the build runner,
// and it may publish what it knows -- and it holds nothing else: no process, no
// command, no file, no kernel reach, no manager, no surface, no timer. Everything
// a maker sees of a build passes through those two rules, so "what did giving
// Zengine a Build button actually cost in authority" has a short and checkable
// answer.
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
// IT REMEMBERS A BUILD THAT IS STILL HAPPENING (ASYNC-1), which is the whole of
// what this file gained. BLD-0's tool had no word for "running", because a build
// that held the pump could not be asked about while it held it -- the one moment
// a maker most wants an answer was the one moment nothing could answer. Now the
// runner reports four kinds of observation and this weave folds them into one
// current picture:
//
//     BuildStarted    -> running, and this is the operation and the command
//     BuildOutput     -> ...and this is the newest thing it said
//     BuildFinished   -> succeeded or FAILED, with the exit status
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
// ONE BUILD AT A TIME, AND IT IS A POLICY OF THIS TOOL. A `BuildRequested` that
// arrives while a build is still going is refused with a sentence naming the
// operation already running, and nothing is ordered. That is a product decision
// about one target and one button -- it is emphatically NOT a limitation of the
// mechanism underneath: the runner holds a vector, mints an identity per
// operation, and observes each of them independently, so the day a Builder wants
// two targets it changes this file and not that one. Written down here because a
// refusal whose reason lives in the wrong layer is a refusal nobody can lift.
//
// WHAT IT DELIBERATELY DOES NOT DO:
//
//   - it does not choose the target. The host does, at construction, and the
//     tool carries that one name for its whole life. A tool that could be told a
//     new target over the wire would be a tool whose reach is whatever somebody
//     types, which is the thing BLD-0 is arranged not to build.
//   - it does not hold a command, and cannot describe one until the runner has
//     told it what is actually running. Before the first build the panel says so.
//   - it does not keep the build log. It keeps a bounded tail of what the current
//     operation has said -- enough for the rows a panel has -- and the whole
//     stream stays on the bus, where a later presentation phase can pick it up
//     without this weave having become a log server.
//   - it does not poll, ask "is it done yet?", or hold a timer. It hears. The
//     one participant that polls anything is the runner, on its own handles.
//   - it does not cancel, and cannot: there is no shape for it, and inventing
//     one would mean deciding what a maker's "stop" claims about a process that
//     may already have finished.

#include "builder/vocabulary.hpp"

#include <zen/weave.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

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

/// The tool's memory. All of it is what a presentation is shown, plus two
/// tallies that only an operator would ask for.
///
/// v2 (ASYNC-1): `op` and `chunks` joined, and they are the two fields that make
/// an unfinished build describable. `op` is which operation this picture is
/// about -- 0 while none is held -- and `chunks` is how many times the runner has
/// been heard about it, which is the number a maker can watch climb.
struct BuilderState {
    std::string target;               ///< the ONE name this tool knows
    std::int64_t outcome = outcome::kNeverBuilt;
    std::int64_t status = 0;          ///< the last process's exit status
    std::string recipe;               ///< what the runner said it ran, once it has run
    std::string detail;               ///< the last thing said about this target
    std::int64_t builds = 0;          ///< asks this tool has taken, ever
    std::int64_t stray = 0;           ///< observations that were about somebody else's work
    std::int64_t op = 0;              ///< the operation this picture is about
    std::int64_t chunks = 0;          ///< output observations folded in for it

    ZEN_EXPOSE();
    ZEN_SHAPE(BuilderState, 2, ZEN_FIELD(target), ZEN_FIELD(outcome), ZEN_FIELD(status),
              ZEN_FIELD(recipe), ZEN_FIELD(detail), ZEN_FIELD(builds), ZEN_FIELD(stray),
              ZEN_FIELD(op), ZEN_FIELD(chunks));
};

class BuilderWeave
    : public loom::WeaveBase<BuilderWeave, BuilderState,
                             loom::Accept<BuildRequested, StatusRequested, BuildStarted,
                                          BuildOutput, BuildFinished, BuildNotStarted>,
                             loom::Emit<RunBuild, BuildStatus>> {
public:
    explicit BuilderWeave(std::string target) { state_.target = std::move(target); }

    /// SAY WHAT YOU ARE. The message a presentation sends when it opens, so a
    /// fresh panel shows a LIVE tool rather than an empty one -- including the
    /// target's name, which is the first thing a maker needs and the one fact
    /// that exists before any build has happened, and including a build that is
    /// running right now, which BLD-0's tool could never be asked about.
    void on(const StatusRequested&, loom::Mail& mail) { say(mail); }

    /// BUILD THE THING YOU KNOW BY THIS NAME.
    ///
    /// The name is checked against the one this tool holds, and a mismatch is a
    /// refusal that says what it does know. That check is not politeness: it is
    /// the reason a `BuildRequested` arriving from anywhere at all -- a panel, a
    /// terminal participant, a test -- cannot widen what this program will build.
    void on(const BuildRequested& ask, loom::Mail& mail) {
        if (ask.target != state_.target) {
            state_.outcome = outcome::kUnknownTarget;
            state_.detail = "this Builder knows `" + state_.target + "` and nothing else";
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
        state_.outcome = outcome::kAsked;
        state_.status = 0;
        state_.op = 0;
        state_.chunks = 0;
        state_.detail.clear();
        remembered_.clear();
        // THE PREVIOUS RECIPE IS NOT CLEARED, and that is the smaller lie of the
        // two available: it describes how THIS target is built, so it is about
        // to be replaced by the same sentence, whereas blanking it would put
        // "nothing has run yet" on a panel about a target that has run before.
        // What IS cleared is `detail`, which is the previous build's output and
        // is genuinely no longer about anything.
        //
        // SAID BEFORE THE ORDER IS GIVEN, so the record reads in the order the
        // facts became true.
        say(mail);
        (void)mail.send_to_role(kBuildRunnerRole, RunBuild{state_.target});
    }

    /// A PROCESS BEGAN. This is the moment BLD-0 had no way to observe.
    void on(const BuildStarted& began, loom::Mail& mail) {
        if (!mine(began.target)) {
            return;
        }
        state_.op = began.op;
        state_.recipe = began.recipe;
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
        if (!mine(said.target) || !about_current(said.op)) {
            return;
        }
        ++state_.chunks;
        remember(said.text);
        state_.detail = tail_lines(remembered_, kDetailLines);
        say(mail);
    }

    /// IT EXITED.
    ///
    /// `status` decides which of two conditions this is, and there is no
    /// `started` flag to consult: something that never started arrives as
    /// `BuildNotStarted` instead, which is the same distinction BLD-0 drew with
    /// two fields, drawn now with two shapes because the two facts no longer
    /// arrive at the same moment.
    void on(const BuildFinished& done, loom::Mail& mail) {
        if (!mine(done.target) || !about_current(done.op)) {
            return;
        }
        state_.status = done.status;
        state_.outcome = done.status == 0 ? outcome::kSucceeded : outcome::kFailed;
        state_.detail = tail_lines(remembered_, kDetailLines);
        say(mail);
    }

    /// NOTHING RAN, and this is why.
    ///
    /// It may name the operation (a child that never became its program) or name
    /// none at all (a target nobody holds a recipe for); both are answers to an
    /// ask this tool made, and `about_current` accepts either because the tool's
    /// own `op` is 0 for exactly as long as no operation has been announced.
    void on(const BuildNotStarted& never, loom::Mail& mail) {
        if (!mine(never.target) || !about_current(never.op)) {
            return;
        }
        state_.outcome = outcome::kNotStarted;
        state_.status = 0;
        if (!never.recipe.empty()) {
            state_.recipe = never.recipe;
        }
        state_.detail = never.trouble;
        say(mail);
    }

    /// What this tool knows, for a suite that wants to check where a message
    /// left it. Read-only, and it is deliberately MORE than the published
    /// status: `stray` is a fact an operator or a test may want and a
    /// presentation has no business showing.
    const BuilderState& known() const { return state_; }

private:
    /// IS THIS ABOUT THE TARGET THIS TOOL HOLDS?
    ///
    /// NOT THIS TOOL'S FACT is counted rather than dropped in silence: a tally is
    /// what makes "this never happens" something an operator can check instead of
    /// something a comment asserts. Recording it would make this tool's own
    /// history a mixture of two targets' outcomes, which is exactly the confusion
    /// `target` exists to prevent.
    bool mine(const std::string& target) {
        if (target == state_.target) {
            return true;
        }
        ++state_.stray;
        return false;
    }

    /// IS THIS ABOUT THE OPERATION THIS TOOL IS FOLLOWING?
    ///
    /// The second half of the same question, and the half that only exists once
    /// operations do. An observation about a different operation of the same
    /// target is somebody else's conversation -- this tool orders one build at a
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
        (void)mail.publish(BuildStatus{state_.target, state_.outcome, state_.status,
                                       state_.recipe, state_.detail, state_.builds, state_.op,
                                       state_.chunks});
    }

    /// The current operation's output so far, bounded.
    ///
    /// A PLAIN MEMBER AND NOT STATE, because it is a working buffer rather than
    /// something anybody should read from outside: what this tool SAYS about it
    /// is `detail`, which is published, bounded to the rows a panel has, and the
    /// same for every reader. A second, larger copy reachable by a poke would be
    /// a second answer to "what did the build say".
    std::string remembered_;
};

} // namespace zengine::builder

#endif // ZENGINE_BUILDER_WEAVE_HPP
