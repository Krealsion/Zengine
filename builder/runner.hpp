// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_BUILDER_RUNNER_HPP
#define ZENGINE_BUILDER_RUNNER_HPP

// The build runner: the one weave in this program that holds a command, the one
// that starts a process -- and, since ASYNC-1, the one that HOLDS the processes
// it started and says what it sees of them.
//
// IT IS A WEAVE AND NOT A HELPER, and that is the whole design. A free function
// the Workshop called would be authority handed over by a pointer -- invisible in
// the host, ungated at the bus, and impossible to state in a sentence about who
// may do what. As a weave it has an identity, an office, and a grant a reader can
// see in one place: it may report what it saw to whoever holds `zengine.builder`,
// and it may ask the Timer for its own beat. It cannot paint, cannot load a
// weave, cannot reach the Manager and cannot answer a stranger.
//
// WHAT IT WILL AND WILL NOT DO WITH A NAME. Its catalog comes from the host at
// construction. A `RunBuild` naming something in that catalog is carried out; a
// `RunBuild` naming anything else is REFUSED, by name, and nothing runs. There is
// no fallback, no PATH search of the name, no "if it looks like a target" -- the
// catalog is the complete set of things this program can build, and it is a set
// somebody wrote down.
//
// ---------------------------------------------------------------------------
// SINCE BLD-1 THE CATALOG IS AUTHORED RECIPES AND NOT COMMANDS, and this weave is
// where a recipe becomes one process. That is a widening of what it HOLDS and not
// of what it may DO, and the difference is worth being exact about:
//
//   what a recipe can name       a CMake build tree and a target; or a source
//                                file, some package prefixes and some exported
//                                target names to link
//   what a recipe cannot name    a program, an argument, a working directory, a
//                                shell line, an environment variable
//   what this weave puts in      the host's own CMake, by absolute path, handed
//   `BuildCommand::program`      to it at construction and unreachable from any
//                                file (`builder/generate.hpp::prepare`)
//
// So a maker's recipe file is not an execution-authority document in the sense a
// LOAD PLAN is: it selects inputs to a mechanism this package already had, and
// the mechanism is CMake either way.
//
// A SINGLE-SOURCE RECIPE ALSO WRITES TWO SMALL FILES before it runs anything --
// the generated CMake project and the driver that configures and builds it. That
// is materializing a command's subject, it happens synchronously inside the same
// handler that would otherwise have nothing to do, and a failure to write is
// reported as `BuildNotStarted` exactly as a failure to launch is. The text
// itself is `builder/generate.hpp`'s, which is a pure function of the recipe and
// is asserted on directly by the suite.
//
// ---------------------------------------------------------------------------
// CUSTODY IS THE PHASE (ASYNC-1), and it is one sentence:
//
//     the participant that possesses the external process capability
//     owns the unfinished work.
//
// Not Workshop, not the Kernel, not the Switchboard, not the Builder panel, and
// not an ambient registry of running things. This weave already had the only
// process authority in the program, so it is the only participant that can
// honestly say "I saw that child exit" -- and a bus stamp is exactly a record of
// who said something. The alternatives were considered and each one moves an OS
// handle somewhere that has no word for it; the Kernel's books are about
// artifacts and incarnations and contain nothing that is "a thing that is
// running".
//
// The shape of one build is therefore:
//
//     RunBuild            ->  start_recipe, keep the custody, say BuildStarted
//     the handler RETURNS.    Loom goes on delivering everything else.
//     an ordinary beat    ->  look at what is held; say what is newly true
//     ...
//     an ordinary beat    ->  it ended; reap it, say BuildFinished, let go
//
// NOTHING HERE WAITS, SLEEPS, OR RUNS ON ANOTHER THREAD. There is no scheduler,
// no queue, no worker and no callback from outside the bus: every line of this
// weave runs inside an ordinary handler, on the ordinary Loom execution thread,
// with an ordinary `Mail`. Many things may be happening; this weave still
// processes one observed fact at a time.
//
// POLLING LIVES HERE AND STOPS HERE. This weave polls its own operating-system
// handles, because that is what an OS handle offers; what leaves this weave is
// only newly observed FACTS. Nothing upstream -- not the tool, not Workshop, not
// the terminal -- ever asks "is it done yet?", and there is no shape in this
// package with which it could. That containment is the phase's architectural
// claim and it is checkable in one grep: `look()` appears in this file and in
// builder/run.hpp, and nowhere else in this repository.
//
// THE BEAT IS ASKED FOR AND GIVEN BACK. A repeating binding is established when
// the first operation starts and cancelled when the last one ends, so a Workshop
// with nothing building carries no Builder traffic. The one exception is
// deliberate and cheap: the binding is declared WANTING, so the Timer's arrival
// arms it once and the very first firing -- holding nothing -- cancels it. That
// single beat is the proof that the wiring exists, paid once per run.
//
// OPERATION IDENTITY, AND ITS SCOPE, DECIDED RATHER THAN DISCOVERED. `next_op_`
// is a plain counter in this weave's memory and the numbers it mints are
// meaningful within ONE LIVE INCARNATION of this runner. That limit is written
// here because Loom's own known-seams record already names the trap it belongs
// to -- a minted identity needs a namespace that outlives references to it -- and
// because in this program the limit costs nothing: this runner is mounted
// natively by its host (`bus.register_weave`), so the Kernel has no record of it,
// `Kernel::query_role` answers `holder == 0` for its office, and `unload_role`
// declines it as "held by a native weave that is not ours to unload". There is
// therefore no path by which a successor could exist to inherit a number. The
// day this runner becomes a loadable weave is the day that stops being true, and
// on that day the counter needs a surviving high-water mark -- not a bigger
// integer.
//
// A HOLDER THAT DIES TAKES ITS WORK WITH IT, AND SAYS NOTHING -- because there
// is nothing left to say it with. Destroying this weave destroys its held
// records, and each `RunningRecipe` destructor terminates and reaps its child:
// no orphan, no zombie, and no false completion. It is worth being exact about
// what that is and is not, because the two are easy to confuse:
//
//     the holder stopped holding      is what happens, and nothing publishes it
//     the external operation stopped  is a DIFFERENT claim, and is not made
//
// A destructor has no `Mail`, no bus and no stamped identity, so it could only
// author a fact by reaching around the one door a weave speaks through. It does
// not. In this program that path is reached exactly once -- when the maker quits
// and the whole Switchboard is torn down -- and a fact published to a bus that is
// being destroyed would be a fact with no reader anyway.
//
// WHAT THIS BOUNDARY IS AND IS NOT WORTH. An in-process weave shares the host's
// address space: the grant bounds what it may SAY, never what it may TOUCH
// (the Loom's own capabilities reference -- named rather than linked, because a
// comment's `*.md` path resolves against THIS repository's root and this
// repository is verified as a standalone clone with no sibling to look at), so
// any weave compiled into this program could call the same platform functions
// `run.hpp` calls. This split therefore buys reviewability rather than
// containment -- one place to look for process authority, one grant to read, one
// refusal to test -- and calling it containment would be exactly the overclaim
// the surrounding phases exist to refuse. Containment of a build is the isolation
// host's kind of question, and this is not that.

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

/// Everything in `buffer` that is now a COMPLETE line, taken OUT of it.
///
/// WHOLE LINES, SO A MAKER IS NEVER SHOWN HALF A PATH. A look lands wherever the
/// child happened to be writing, so the tail of a drain is routinely a fragment;
/// holding it back until its newline arrives costs one beat and buys a panel
/// that never shows a truncated diagnostic as though it were the whole one.
///
/// `ending` releases the remainder unconditionally, because at end of output
/// there is no later newline to wait for and a last line without one is still
/// something the build said. The length guard does the same for the pathological
/// case -- a child writing megabytes with no newline at all -- because past a
/// point "wait for the end of the line" stops being a bound.
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

/// The runner's own books. Nothing here is a capability -- the capability is the
/// catalog, and the catalog is not state a poke can reach (see below).
///
/// v2 (ASYNC-1): `live` and `looks` joined. `live` is how many operations this
/// runner is holding RIGHT NOW, which is the one number that makes custody
/// visible to an operator; `looks` is how many observation passes it has made,
/// which is what tells "the beat is running and finding nothing" apart from "the
/// beat is not running".
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
    /// THE CATALOG IS READ FROM ITS OWNER, WHICH IS THE HOST, and it is a plain
    /// member rather than part of the weave's state.
    ///
    /// That placement is deliberate and it is the same distinction Workshop
    /// draws between its document and its session: `ZEN_SHAPE` state is
    /// poke-writable by design (the operator's door), and a poke that could
    /// write a new program path into these recipes would be a door onto arbitrary
    /// execution wearing an inspection tool's clothes. So the recipes live where
    /// the substrate has no words for them, and what IS exposed is a tally that
    /// tells an operator how often this runner has run, refused and looked.
    ///
    /// ⚠ IT IS A READ AND NOT A COPY (PROJ-0), and the reference is the whole point.
    /// This weave used to take the completed catalog BY VALUE, which made it a second
    /// session-long owner of a truth the host had already completed once -- so a host
    /// that later replaced its catalog would have had to come and find this one. It
    /// now reads the one the host holds, which is what makes "the recipe this runner
    /// builds is the recipe this Workshop currently means" structural. What it reads
    /// is `const`: a runner that could write into somebody else's catalog would be a
    /// build procedure with no author.
    ///
    /// ⚠ THE OWNER MUST OUTLIVE THIS WEAVE, and an rvalue is refused below rather
    /// than left to a reader's care -- binding this to a temporary would be a
    /// dangling catalog whose first symptom is a build of nothing in particular.
    ///
    /// THE LIVE HANDLES ARE UNDER THE SAME ROOF, and for a sharper version of
    /// the same reason: a writable `pid` is that door with the lock already off.
    ///
    /// THE CMAKE ARRIVES THE SAME WAY AND FOR A SHARPER VERSION OF THE SAME REASON.
    /// It is the one program this weave will ever start, it is chosen by the party
    /// that composed the process, and no recipe, message or poke can reach it.
    BuildRunnerWeave(const std::vector<Recipe>& catalog, std::string cmake)
        : catalog_(catalog), cmake_(std::move(cmake)) {
        look_ = timers().repeat(std::string(kLookTimerId), std::chrono::milliseconds(kLookBeatMs),
                                &BuildRunnerWeave::on_look_beat);
    }

    /// THE WALL UNDER THE SENTENCE ABOVE. A caller that hands over a temporary catalog
    /// is not composing a runner, it is arranging a use-after-free, and the compiler is
    /// the only party that can catch it before a maker does.
    BuildRunnerWeave(std::vector<Recipe>&&, std::string) = delete;

    /// The one line of ceremony the binding layer cannot remove: this weave has
    /// its own `on` handlers, which would otherwise HIDE the binding layer's.
    using BuildRunnerBase::on;

    /// START A BUILD AND GO HOME.
    ///
    /// The handler returns as soon as the child exists. It does not read a byte,
    /// does not wait, and leaves behind an owned record that is sufficient to
    /// find the process again on any later beat.
    void on(const RunBuild& order, loom::Mail& mail) {
        const Recipe* recipe = recipe_named(catalog_, order.recipe);
        if (recipe == nullptr) {
            ++state_.refused;
            // A refusal names no operation, because none was created: `op` is 0
            // and the absence is the statement. A separate refusal shape would
            // give the tool two code paths for the same question -- "did a
            // process run for my ask?" -- whose answer is no either way.
            (void)mail.send_to_role(kBuilderRole,
                                    BuildNotStarted{0, order.recipe, std::string(),
                                                    "no recipe here is called `" + order.recipe +
                                                        "`"});
            return;
        }
        // THE RECIPE BECOMES A COMMAND HERE, and for a single-source recipe this is
        // also where the generated project is written. Both can fail for ordinary
        // reasons a maker can fix -- a build tree that was never configured, a
        // source file that has been moved, a workspace that cannot be created --
        // and all of them are "nothing ran", said with the reason.
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
    /// ONE UNFINISHED OPERATION, as the runner holds it.
    ///
    /// It is the smallest record that lets a later beat do its job: name the
    /// operation, name what it is building, describe what is running, keep the
    /// output that is not yet a whole line, and hold the process itself. There
    /// is no `phase` field, because this weave has exactly two conditions for an
    /// operation and they are "in this vector" and "not in this vector".
    struct Held {
        std::int64_t op = 0;
        std::string recipe;  ///< the authored recipe this operation is carrying out
        std::string command; ///< what is actually running, as one line
        std::string pending; ///< drained bytes that are not yet a complete line
        RunningRecipe process;
    };

    void on_look_beat(const zengine::timer::TimerFired&, loom::Mail& mail) { look_at_held(mail); }

    /// ONE BOUNDED PASS OVER EVERYTHING HELD.
    ///
    /// Every operation is looked at exactly once, each look is non-blocking and
    /// capped, and the whole pass is over in the time a handful of syscalls
    /// takes. A slow child does not make checking it slow, and a second
    /// operation does not wait behind the first.
    void look_at_held(loom::Mail& mail) {
        ++state_.looks;
        for (std::size_t i = 0; i < held_.size();) {
            Held& held = held_[i];
            const RunLook seen = held.process.look();
            held.pending += seen.fresh;
            const std::string ready = take_complete_lines(held.pending, seen.ended);
            if (!ready.empty()) {
                std::string text = tail_lines(ready, kAllLines);
                std::int64_t dropped = 0;
                if (text.size() > kMaxOutputChars) {
                    dropped = static_cast<std::int64_t>(text.size() - kMaxOutputChars);
                    text.erase(0, text.size() - kMaxOutputChars);
                }
                (void)mail.send_to_role(kBuilderRole,
                                        BuildOutput{held.op, held.recipe, text, dropped});
            }
            if (!seen.ended) {
                ++i;
                continue;
            }
            if (seen.never_ran) {
                // IT ENDED BECAUSE IT NEVER BEGAN. On POSIX this is the only
                // moment that fact can arrive (builder/run.hpp), and it carries
                // the operation's own number precisely because the operation was
                // already announced under it.
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

    /// THE OWNER'S CATALOG, NOT THIS WEAVE'S. Bound once at construction to the vector
    /// the host holds for the session, so a replacement of its CONTENTS is a
    /// replacement of what this runner builds -- with nothing here to find and update.
    const std::vector<Recipe>& catalog_;
    /// THE ONE PROGRAM THIS WEAVE STARTS, under the same roof as the catalog and for
    /// the same reason: a poke that could write a new program path here would be a
    /// door onto arbitrary execution wearing an inspection tool's clothes.
    std::string cmake_;
    std::vector<Held> held_;
    /// NEVER STATE, for the reason in this file's header: a poke that could
    /// rewind this would make the runner re-issue a number a published fact has
    /// already used.
    std::int64_t next_op_ = 0;
    Handle look_;
};

} // namespace zengine::builder

#endif // ZENGINE_BUILDER_RUNNER_HPP
