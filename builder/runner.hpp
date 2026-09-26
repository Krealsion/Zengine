// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_BUILDER_RUNNER_HPP
#define ZENGINE_BUILDER_RUNNER_HPP

// The build runner: the one weave that holds a command and starts a process, and holds the
// processes it started, saying what it sees of them. A weave rather than a helper, so its
// authority is a grant a reader can see: report to `zengine.builder`, ask the Timer for a beat.
// A `RunBuild` for a name outside the host's catalog is refused and nothing runs; the one
// program it starts is the host's CMake, which no recipe, message or poke can reach.
// Builder law: agents/realization.md

// The participant that possesses the process capability owns the unfinished work: it starts a
// child and returns, looks on an ordinary beat held only while it holds a process, and says only
// newly observed facts -- nothing upstream asks "is it done yet?". `op` means something within
// this incarnation: the runner is mounted natively, so no successor can inherit a number (a
// loadable one would need a surviving high-water mark). Destroying it terminates and reaps every
// child and publishes nothing. Reference: docs/reference/builder.md.

#include "builder/generate.hpp"
#include "builder/recipe.hpp"
#include "builder/run.hpp"
#include "builder/vocabulary.hpp"

#include "timer/binding.hpp"

#include <zen/weave.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace zengine::builder {

/// Everything in `buffer` that is now a complete line, taken out of it, so a maker never sees
/// half a path. `ending` releases the remainder (a last line without a newline is still said),
/// and so does a remainder past `kMaxLookBytes` with no newline at all.
inline std::string take_complete_lines(std::string& buffer, bool ending) {
    const std::size_t nl = buffer.rfind('\n');
    if (ending || (nl == std::string::npos && buffer.size() > kMaxLookBytes)) {
        std::string all;
        all.swap(buffer);
        return all;
    }
    if (nl == std::string::npos) {
        return {};
    }
    std::string done = buffer.substr(0, nl + 1);
    buffer.erase(0, nl + 1);
    return done;
}

/// HOW MANY BYTES OF `ready`, FROM `at`, THE NEXT `BuildOutput` CARRIES: all that is left when
/// it fits in `kMaxOutputChars`, else up to the last line break inside that bound, else the
/// bound itself (a line longer than a message continues in the next). A message ends where a
/// line does whenever one can, and no byte is left out to make one fit.
// WL-OUT-01 -- agents/workshop/build-output.md
inline std::size_t output_piece(const std::string& ready, std::size_t at) {
    const std::size_t left = ready.size() - at;
    if (left <= kMaxOutputChars) {
        return left;
    }
    const std::size_t nl = ready.rfind('\n', at + kMaxOutputChars - 1);
    if (nl != std::string::npos && nl >= at) {
        return nl - at + 1;
    }
    return kMaxOutputChars;
}

/// The runner's own books; nothing here is a capability (the catalog is, and it is not state).
/// `live` makes custody visible to an operator; `looks` tells a running beat that finds nothing
/// from a beat that is not running.
struct RunnerState {
    std::int64_t ran = 0;     ///< recipes this runner has tried to start
    std::int64_t refused = 0; ///< names this runner does not hold a recipe for
    std::int64_t live = 0;    ///< operations held right now
    std::int64_t looks = 0;   ///< observation passes performed
    ZEN_EXPOSE();
    ZEN_SHAPE(RunnerState, 2, ZEN_FIELD(ran), ZEN_FIELD(refused), ZEN_FIELD(live),
              ZEN_FIELD(looks));
};

class BuildRunnerWeave;

/// Named once, because three spellings of it is what `using ...::on` would
/// otherwise cost.
using BuildRunnerBase =
    zengine::timer::TimedWeave<BuildRunnerWeave, RunnerState,
                               loom::Accept<RunBuild, LookAtBuilds>,
                               loom::Emit<BuildStarted, BuildOutput, BuildFinished,
                                          BuildNotStarted>>;

class BuildRunnerWeave : public BuildRunnerBase {
public:
    /// The catalog is read from its owner, the host: never copied (a replaced catalog is what
    /// this runner then builds) and never state, since `ZEN_SHAPE` state is poke-writable and a
    /// poke that could write a program path would be arbitrary execution. It is `const`: a
    /// runner writing somebody else's catalog would be a procedure with no author. The owner
    /// must outlive this weave. The live handles and `cmake_` stay out of state for that reason.
    BuildRunnerWeave(const std::vector<Recipe>& catalog, std::string cmake)
        : catalog_(catalog), cmake_(std::move(cmake)) {
        look_ = timers().repeat(std::string(kLookTimerId), std::chrono::milliseconds(kLookBeatMs),
                                &BuildRunnerWeave::on_look_beat);
    }

    /// A temporary catalog would dangle; only the compiler can catch that before a maker does.
    BuildRunnerWeave(std::vector<Recipe>&&, std::string) = delete;

    /// The one line of ceremony the binding layer cannot remove: this weave has
    /// its own `on` handlers, which would otherwise HIDE the binding layer's.
    using BuildRunnerBase::on;

    /// Start a build and go home: the handler returns as soon as the child exists, leaving an
    /// owned record a later beat finds it by.
    void on(const RunBuild& order, loom::Mail& mail) {
        const Recipe* recipe = recipe_named(catalog_, order.recipe);
        if (recipe == nullptr) {
            ++state_.refused;
            // A refusal names no operation (`op` 0): none was created, and one shape answers
            // "did a process run for my ask?" either way.
            (void)mail.send_to_role(kBuilderRole,
                                    BuildNotStarted{0, order.recipe, std::string(),
                                                    "no recipe here is called `" + order.recipe +
                                                        "`"});
            return;
        }
        // The recipe becomes a command here (and a single-source recipe's project is written);
        // an ordinary failure a maker can fix is "nothing ran", with the reason.
        const PreparedBuild prepared = prepare(*recipe, cmake_);
        if (!prepared.ok) {
            ++state_.refused;
            (void)mail.send_to_role(
                kBuilderRole,
                BuildNotStarted{0, recipe->id, std::string(), prepared.trouble});
            return;
        }
        ++state_.ran;
        RecipeStart begun = start_recipe(prepared.command);
        if (!begun.started) {
            (void)mail.send_to_role(
                kBuilderRole,
                BuildNotStarted{0, recipe->id, prepared.command.as_line(), begun.trouble});
            return;
        }
        Held held;
        held.op = ++next_op_;
        held.recipe = recipe->id;
        held.command = prepared.command.as_line();
        held.process = std::move(begun.process);
        held_.push_back(std::move(held));
        state_.live = static_cast<std::int64_t>(held_.size());
        (void)mail.send_to_role(kBuilderRole, BuildStarted{held_.back().op, held_.back().recipe,
                                                           held_.back().command});
        // ...AND ONLY NOW ASK FOR THE BEAT. Held first, announced second, armed
        // third: each step is only true because the one before it is.
        if (!look_.waiting()) {
            look_.restart(mail);
        }
    }

    /// The direct door: the same hands the beat opens, on request.
    void on(const LookAtBuilds&, loom::Mail& mail) { look_at_held(mail); }

    /// What this runner can build, for a host that wants to say so in its banner.
    /// Read-only, and it is the host's own list coming back -- no weave learns it
    /// this way, and this weave never held a second one to hand back.
    const std::vector<Recipe>& catalog() const { return catalog_; }

    /// How many processes this runner has tried to start, how many names it has
    /// turned down, how many operations it is holding, and how many times it has
    /// looked. The numbers a suite needs in order to assert that something did
    /// NOT run -- "the outcome was a refusal" is a weaker claim than "no process
    /// began", and only the second one is the guarantee.
    std::int64_t ran() const { return state_.ran; }
    std::int64_t refused() const { return state_.refused; }
    std::int64_t live() const { return state_.live; }
    std::int64_t looks() const { return state_.looks; }

private:
    /// One unfinished operation: the smallest record a later beat needs. No `phase` field: an
    /// operation is either in `held_` or not.
    struct Held {
        std::int64_t op = 0;
        std::string recipe;  ///< the authored recipe this operation is carrying out
        std::string command; ///< what is actually running, as one line
        std::string pending; ///< drained bytes that are not yet a complete line
        RunningRecipe process;
    };

    void on_look_beat(const zengine::timer::TimerFired&, loom::Mail& mail) { look_at_held(mail); }

    /// One bounded pass over everything held: each look is non-blocking and capped, so a slow
    /// child is not slow to check and a second operation does not wait behind the first.
    void look_at_held(loom::Mail& mail) {
        ++state_.looks;
        for (std::size_t i = 0; i < held_.size();) {
            Held& held = held_[i];
            const RunLook seen = held.process.look();
            held.pending += seen.fresh;
            const std::string ready = take_complete_lines(held.pending, seen.ended);
            // Every byte the look made ready, in order, in as many messages as it takes.
            for (std::size_t at = 0; at < ready.size();) {
                const std::size_t piece = output_piece(ready, at);
                (void)mail.send_to_role(kBuilderRole,
                                        BuildOutput{held.op, held.recipe, ready.substr(at, piece)});
                at += piece;
            }
            if (!seen.ended) {
                ++i;
                continue;
            }
            if (seen.never_ran) {
                // It ended because it never began: on POSIX this is the only moment that can
                // be known (builder/run.hpp), under the number it was already announced with.
                (void)mail.send_to_role(
                    kBuilderRole, BuildNotStarted{held.op, held.recipe, held.command,
                                                  seen.trouble});
            } else {
                (void)mail.send_to_role(kBuilderRole,
                                        BuildFinished{held.op, held.recipe, seen.status});
            }
            held_.erase(held_.begin() + static_cast<std::ptrdiff_t>(i));
        }
        state_.live = static_cast<std::int64_t>(held_.size());
        // NOTHING LEFT TO WATCH, SO STOP WATCHING. The cancel is what keeps
        // "polling is contained" from meaning "polling is constant".
        if (held_.empty() && !look_.canceled()) {
            look_.cancel(mail);
        }
    }

    /// The owner's catalog, bound once to the vector the host holds, so replacing its contents
    /// replaces what this runner builds.
    const std::vector<Recipe>& catalog_;
    /// The one program this weave starts, out of state for the catalog's reason.
    std::string cmake_;
    std::vector<Held> held_;
    /// Never state: a poke that rewound it would re-issue a number a published fact has used.
    std::int64_t next_op_ = 0;
    Handle look_;
};

} // namespace zengine::builder

#endif // ZENGINE_BUILDER_RUNNER_HPP
