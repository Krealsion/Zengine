// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Builder suite -- the tool, the runner, the line between a NAME and a
// COMMAND, and (ASYNC-1) the line between a build and the turn that asked for it.
//
// This package is the first thing in Zengine whose subject is an EFFECT: a child
// process, with a real exit status, started by a real fork/exec or
// CreateProcess. So the suite is arranged around the questions that effect
// raises, and none of them is "does the panel look right" (that is the Workshop
// suite's, next door, and it is a different claim):
//
//   1. DOES A BUILD ACTUALLY HAPPEN, and is what comes back true? Success,
//      failure and never-started are three outcomes and the suite drives all
//      three against a real program -- the very CMake that configured this tree.
//   2. DOES IT OUTLIVE THE TURN THAT STARTED IT? A build that finishes inside
//      its own handler proves nothing about custody, so the cases that matter
//      here use a deliberately SLOW recipe and measure what the bus carried
//      while the child was demonstrably still alive -- with the old blocking
//      shape rebuilt beside it as the control, so the number means something.
//   3. WHO IS ALLOWED TO CAUSE ONE? The tool holds a name and the runner holds
//      the commands, and the cases below assert the negative half of that with
//      the positive control beside it: a weave with the reach a PRESENTATION is
//      given cannot make a process start, and the same case with the grant
//      widened proves the first half was a measurement rather than a fixture
//      that never fired.
//
// NO PROCESS IS STARTED EXCEPT THROUGH A RECIPE THE CASE ITSELF WROTE. Every
// recipe here names `cmake -E ...` or `cmake -P tests/slow_build.cmake`, so the
// suite needs no shell, no /bin/sh, no .bat and no assumption about what else is
// installed, on either platform this repository builds for.

// main() and the framework live in doctest_main.cpp -- the shared one that
// refuses a run selecting zero cases (POP-01).
#include "doctest.h"

#include "builder/run.hpp"
#include "builder/runner.hpp"
#include "builder/vocabulary.hpp"
#include "builder/weave.hpp"

#include "timer/vocabulary.hpp"

#include <zen/history/logger.hpp>
#include <zen/history/recorder.hpp>
#include <zen/schema.hpp>
#include <zen/switchboard.hpp>
#include <zen/weave.hpp>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

using namespace zengine::builder;
using loom::schema_of;
namespace timer = zengine::timer;

namespace {

/// The CMake that configured this build tree, handed over as a compile
/// definition exactly as the Workshop host's recipe is. A test that resolved
/// `cmake` from PATH would be testing this machine's PATH.
const char* const kCMake = ZENGINE_TEST_CMAKE;

/// The repository's own deliberately slow, deliberately chatty stand-in for a
/// build (tests/slow_build.cmake, which says why it exists).
const char* const kSlowScript = ZENGINE_TEST_SLOW_SCRIPT;

/// The fixture build tree a `CMakeTargetRecipe` can point at, and where its one
/// producing target puts the file it makes (tests/buildfixture/CMakeLists.txt).
const char* const kFixtureTree = ZENGINE_TEST_FIXTURE_TREE;
/// A SECOND CONFIGURED TREE, FOR THE ONE CASE THAT RUNS TWO BUILDS AT ONCE. A build
/// tree has one owner while a build is in it, so two operations that must genuinely
/// overlap need two of them -- see the case below, and tests/CMakeLists.txt for what
/// was measured.
const char* const kFixtureTreeB = ZENGINE_TEST_FIXTURE_TREE_B;
const char* const kFixtureArtifacts = ZENGINE_TEST_FIXTURE_ARTIFACTS;
const char* const kArtifactSuffix = ZENGINE_TEST_ARTIFACT_SUFFIX;

/// A COMMAND that takes a while and says several things while it does.
///
/// THE DURATION IS THE POINT. "It did not block" measured against a child that
/// exits in a millisecond is a race dressed as a property; measured against a
/// child that is provably still alive several hundred deliveries later, it is a
/// measurement.
///
/// It is a `BuildCommand` and NOT a recipe (BLD-1): a command is what one process
/// will be, and the cases that spend this one are the ones about the process
/// primitive itself -- `start_recipe`, `look`, custody, reaping. Everything above
/// that layer goes through an authored recipe, because that is what production
/// has.
BuildCommand slow(int steps, const char* pause, bool fail = false) {
    BuildCommand c;
    c.program = kCMake;
    c.args = {std::string("-DSTEPS=") + std::to_string(steps), std::string("-DPAUSE=") + pause};
    if (fail) {
        c.args.push_back("-DFAIL=1");
    }
    c.args.push_back("-P");
    c.args.push_back(kSlowScript);
    return c;
}

/// One command that always succeeds at once.
BuildCommand echoes(const std::string& what) {
    return BuildCommand{kCMake, {"-E", "echo", what}, std::string()};
}

/// AN AUTHORED RECIPE FOR ONE OF THE FIXTURE TREE'S TARGETS.
///
/// This is what the suite hands the runner now, and it is exactly what a maker's
/// recipe file produces: an identity, an artifact stem, where that artifact
/// lands, and a configured build tree plus a target in it. Nothing here is a
/// command, and nothing in this suite can make one reach the runner -- which is
/// the property BLD-1 added and this fixture exists to keep honest.
/// THE ARTIFACT DEFAULTS TO THE FIXTURE TARGET'S OWN NAME, because that is what the
/// fixture's succeeding targets produce -- so a recipe built from this pair really
/// does name the file its target really does write. The two-argument overload is the
/// deliberate mismatch: a recipe that names an artifact its target does not produce,
/// which is `outcome::kNoArtifact`'s whole subject.
Recipe cmake_recipe(const std::string& id, const std::string& target,
                    const std::string& artifact) {
    Recipe r;
    r.id = id;
    r.artifact = artifact;
    r.artifact_dir = kFixtureArtifacts;
    r.cmake_target = CMakeTargetRecipe{kFixtureTree, target, std::string()};
    return r;
}

Recipe cmake_recipe(const std::string& id, const std::string& target) {
    return cmake_recipe(id, target, target);
}

/// THE SAME RECIPE, IN A BUILD TREE OF ITS OWN. Nothing about the recipe kind changes --
/// a configured tree and a target in it, exactly as before; what changes is WHICH tree,
/// which is a thing a maker's recipe names too.
Recipe cmake_recipe_in(const char* tree, const std::string& id, const std::string& target) {
    Recipe r = cmake_recipe(id, target);
    r.cmake_target->build_dir = tree;
    return r;
}

/// The file a fixture recipe's artifact stem means, spelled the way a host spells
/// one. The suite owns this rule here for the same reason the host owns it there:
/// there is exactly one place either of them writes a platform suffix down.
std::string fixture_path(const std::string& artifact) {
    return std::string(kFixtureArtifacts) + "/" + artifact + kArtifactSuffix;
}

/// The tool's view of a recipe -- identity, artifact, and the one file it means.
RecipeView view_of(const Recipe& r) {
    return RecipeView{r.id, r.artifact, fixture_path(r.artifact)};
}

/// Drive one held process to its end the way nothing in production does -- by
/// looking as fast as it can -- and answer everything it said plus how many
/// looks it took.
struct Drained {
    std::string said;
    std::int64_t looks = 0;      ///< looks taken in total
    std::int64_t looks_that_spoke = 0; ///< how many of them produced new output
    bool ended = false;
    std::int64_t status = 0;
    bool never_ran = false;
    std::string trouble;
};

Drained drain(RunningRecipe& process, int guard = 2000000) {
    Drained out;
    while (guard-- > 0) {
        const RunLook seen = process.look();
        ++out.looks;
        if (!seen.fresh.empty()) {
            ++out.looks_that_spoke;
            out.said += seen.fresh;
        }
        if (seen.ended) {
            out.ended = true;
            out.status = seen.status;
            out.never_ran = seen.never_ran;
            out.trouble = seen.trouble;
            return out;
        }
    }
    return out;
}

/// Anything on this bus that accepts a BuildStatus. The Builder panel is one
/// such thing; this is another, and the tool cannot tell them apart, which is
/// the property that makes the panel a presentation rather than an owner.
struct HeardState {
    std::int64_t heard = 0;
    ZEN_SHAPE(HeardState, 1, ZEN_FIELD(heard));
};

class Listener
    : public loom::WeaveBase<Listener, HeardState,
                             loom::Accept<BuildStatus, RecipeCatalog, OfferArtifact>,
                             loom::Emit<>> {
public:
    void on(const BuildStatus& s, loom::Mail&) {
        ++state_.heard;
        said.push_back(s);
    }
    /// THE SECOND AND THIRD PUBLICATIONS, HEARD BY THE SAME ORDINARY LISTENER (BLD-1)
    /// -- which is the property, not the bookkeeping: nothing about `RecipeCatalog` or
    /// `OfferArtifact` is addressed to a panel, so anything on this bus that accepts
    /// them sees exactly what a panel sees.
    void on(const RecipeCatalog& c, loom::Mail&) { catalogs.push_back(c); }
    void on(const OfferArtifact& a, loom::Mail&) { built.push_back(a); }
    const BuildStatus& last() const { return said.back(); }
    std::vector<BuildStatus> said;
    std::vector<RecipeCatalog> catalogs;
    std::vector<OfferArtifact> built;
};

/// UNRELATED TRAFFIC, COUNTED. It is the whole falsifier of this phase: a build
/// that holds the pump carries none of this between its start and its end, and a
/// build that is merely held carries as much as anybody cares to send.
struct BystanderState {
    std::int64_t seen = 0;
    ZEN_SHAPE(BystanderState, 1, ZEN_FIELD(seen));
};

class Bystander : public loom::WeaveBase<Bystander, BystanderState, loom::Accept<loom::Ack>,
                                         loom::Emit<>> {
public:
    void on(const loom::Ack&, loom::Mail&) { ++state_.seen; }
    std::int64_t seen() const { return state_.seen; }
};

/// A stand-in for the Timer service that RECORDS rather than schedules.
///
/// The runner asks for a beat when it takes custody of something and gives the
/// beat back when it has nothing left to watch. That conversation is the
/// mechanism by which "polling is contained" is true, so it is measured here
/// rather than asserted -- and measuring it needs somebody holding the Timer's
/// office to hear it. It never fires anything: the cases drive the beat
/// themselves, so a firing arrives exactly when a case says it does.
struct ClerkState {
    std::int64_t asked = 0;
    std::int64_t cancelled = 0;
    ZEN_SHAPE(ClerkState, 1, ZEN_FIELD(asked), ZEN_FIELD(cancelled));
};

class TimerClerk
    : public loom::WeaveBase<TimerClerk, ClerkState,
                             loom::Accept<timer::EnsureTimer, timer::CancelTimer>, loom::Emit<>> {
public:
    void on(const timer::EnsureTimer& e, loom::Mail&) {
        ++state_.asked;
        ids.push_back(e.id);
    }
    void on(const timer::CancelTimer& c, loom::Mail&) {
        ++state_.cancelled;
        ids.push_back("cancel:" + c.id);
    }
    std::int64_t asked() const { return state_.asked; }
    std::int64_t cancelled() const { return state_.cancelled; }
    std::vector<std::string> ids;
};

/// Everything the runner said, held by whoever holds the Builder office.
///
/// It exists so a case can watch the RUNNER on its own, without a tool folding
/// four observations into one picture first -- which is exactly what a case about
/// two operations at once needs, since the tool deliberately follows one.
struct ForemanState {
    std::int64_t seen = 0;
    ZEN_SHAPE(ForemanState, 1, ZEN_FIELD(seen));
};

class Foreman : public loom::WeaveBase<Foreman, ForemanState,
                                       loom::Accept<BuildStarted, BuildOutput, BuildFinished,
                                                    BuildNotStarted>,
                                       loom::Emit<>> {
public:
    void on(const BuildStarted& s, loom::Mail&) {
        ++state_.seen;
        started.push_back(s);
    }
    void on(const BuildOutput& o, loom::Mail&) {
        ++state_.seen;
        output.push_back(o);
    }
    void on(const BuildFinished& f, loom::Mail&) {
        ++state_.seen;
        finished.push_back(f);
    }
    void on(const BuildNotStarted& n, loom::Mail&) {
        ++state_.seen;
        never.push_back(n);
    }

    /// Everything said about ONE operation, in order, joined -- the question
    /// "did this operation's output stay this operation's?" asked directly.
    std::string text_of(std::int64_t op) const {
        std::string all;
        for (const BuildOutput& o : output) {
            if (o.op == op) {
                all += o.text;
                all += '\n';
            }
        }
        return all;
    }

    std::vector<BuildStarted> started;
    std::vector<BuildOutput> output;
    std::vector<BuildFinished> finished;
    std::vector<BuildNotStarted> never;
};

/// A TALLY THAT OUTLIVES THE BUS.
///
/// The custodian-death case destroys the whole Switchboard and then asks what
/// was published during the teardown -- a question no weave can answer, because
/// every weave is destroyed with it. So the counting happens in memory the CASE
/// owns, handed to a weave by pointer, and reading it afterwards is reading the
/// test's own stack frame rather than a corpse.
struct Ledger {
    std::int64_t started = 0;
    std::int64_t endings = 0;
};

class Undertaker : public loom::WeaveBase<Undertaker, ForemanState,
                                          loom::Accept<BuildStarted, BuildOutput, BuildFinished,
                                                       BuildNotStarted>,
                                          loom::Emit<>> {
public:
    explicit Undertaker(Ledger* into) : into_(into) {}
    void on(const BuildStarted&, loom::Mail&) { ++into_->started; }
    void on(const BuildOutput&, loom::Mail&) {}
    void on(const BuildFinished&, loom::Mail&) { ++into_->endings; }
    void on(const BuildNotStarted&, loom::Mail&) { ++into_->endings; }

private:
    Ledger* into_;
};

/// BLD-0'S RUNNER, REBUILT AS THE CONTROL.
///
/// It builds inside its own handler, exactly as the shipped runner did before
/// ASYNC-1, and it exists for one reason: a claim that the new shape does not
/// stop the bus is only a measurement if the shape that DID stop it can be run
/// beside it and seen to. It calls `run_recipe`, which is the same platform code
/// the held path uses, driven to completion -- so the control cannot pass
/// because two implementations drifted.
struct BlockingState {
    std::int64_t ran = 0;
    ZEN_SHAPE(BlockingState, 1, ZEN_FIELD(ran));
};

class BlockingRunnerWeave
    : public loom::WeaveBase<BlockingRunnerWeave, BlockingState, loom::Accept<RunBuild>,
                             loom::Emit<BuildStarted, BuildFinished, BuildNotStarted>> {
public:
    BlockingRunnerWeave(std::string recipe, BuildCommand command)
        : recipe_(std::move(recipe)), command_(std::move(command)) {}

    void on(const RunBuild& order, loom::Mail& mail) {
        if (order.recipe != recipe_) {
            (void)mail.send_to_role(kBuilderRole,
                                    BuildNotStarted{0, order.recipe, std::string(), "no recipe"});
            return;
        }
        ++state_.ran;
        (void)mail.send_to_role(kBuilderRole, BuildStarted{1, recipe_, command_.as_line()});
        const RunResult run = run_recipe(command_); // <- the whole build, inside this handler
        if (!run.started) {
            (void)mail.send_to_role(
                kBuilderRole, BuildNotStarted{1, recipe_, command_.as_line(), run.trouble});
            return;
        }
        (void)mail.send_to_role(kBuilderRole, BuildFinished{1, recipe_, run.status});
    }

private:
    std::string recipe_;
    BuildCommand command_;
};

/// A weave that tries to ORDER THE RUNNER DIRECTLY when poked with an Ack.
///
/// It exists to answer one question with a measurement instead of a sentence:
/// can something that is only allowed to ASK the Builder make a process start?
/// The trigger is `loom::Ack` because the suite needs a shape to knock with and
/// this one carries nothing.
struct PushState {
    std::int64_t sent = 0;
    ZEN_SHAPE(PushState, 1, ZEN_FIELD(sent));
};

class Impostor : public loom::WeaveBase<Impostor, PushState, loom::Accept<loom::Ack>,
                                        loom::Emit<RunBuild>> {
public:
    explicit Impostor(std::string target) : target_(std::move(target)) {}
    void on(const loom::Ack&, loom::Mail& mail) {
        ++state_.sent;
        ++tried;
        (void)mail.send_to_role(kBuildRunnerRole, RunBuild{target_});
    }

    /// A plain member beside the state, because a case has to be able to say
    /// "it really did try" and the weave's ZEN_SHAPE state is not readable from
    /// outside without going through a poke.
    std::int64_t tried = 0;

private:
    std::string target_;
};

/// Mount a weave into an office with an explicit grant -- the host's own two
/// lines (`register_weave` with a role, then wire the self-id), because
/// `mount_granted` binds no role.
template <class Weave, class... Args>
loom::WeaveId mount_office(loom::Switchboard& bus, loom::Grant grant, const char* office,
                           Weave** out, Args&&... args) {
    auto weave = std::make_unique<Weave>(std::forward<Args>(args)...);
    Weave* raw = weave.get();
    const loom::WeaveId id =
        bus.register_weave(std::move(weave), std::move(grant), std::string(office));
    raw->zen_set_self(id);
    if (out != nullptr) {
        *out = raw;
    }
    return id;
}

template <class Weave, class... Args>
loom::WeaveId mount_plain(loom::Switchboard& bus, loom::Grant grant, Weave** out, Args&&... args) {
    auto weave = std::make_unique<Weave>(std::forward<Args>(args)...);
    Weave* raw = weave.get();
    const loom::WeaveId id = bus.register_weave(std::move(weave), std::move(grant));
    raw->zen_set_self(id);
    if (out != nullptr) {
        *out = raw;
    }
    return id;
}

/// THE RUNNER'S WHOLE REACH, written once so the suite and the host say the same
/// thing: four observations to the Builder office, and two sentences to the
/// Timer. Nothing else, to nobody else.
loom::Grant runner_grant() {
    loom::Grant g;
    g.allow_to_role(BuildStarted::zen_name, BuildStarted::zen_version, kBuilderRole);
    g.allow_to_role(BuildOutput::zen_name, BuildOutput::zen_version, kBuilderRole);
    g.allow_to_role(BuildFinished::zen_name, BuildFinished::zen_version, kBuilderRole);
    g.allow_to_role(BuildNotStarted::zen_name, BuildNotStarted::zen_version, kBuilderRole);
    g.allow_to_role(timer::EnsureTimer::zen_name, timer::EnsureTimer::zen_version,
                    timer::kTimerRole);
    g.allow_to_role(timer::CancelTimer::zen_name, timer::CancelTimer::zen_version,
                    timer::kTimerRole);
    return g;
}

/// THE HOST'S OWN GRANT FOR THE TOOL, and it is copied from `workshop.cpp` rather
/// than invented here: order the runner, publish what it knows, publish what can be
/// built at all, and (BLD-1) say that an artifact somebody asked to have realized is
/// on disk. Four rules, and the suite writes the same four the production host does
/// -- a fixture with a wider grant would be a fixture in which the phase's authority
/// claim is untested.
loom::Grant tool_grant() {
    loom::Grant g;
    g.allow_to_role(RunBuild::zen_name, RunBuild::zen_version, kBuildRunnerRole);
    g.allow_to_any(BuildStatus::zen_name, BuildStatus::zen_version);
    g.allow_to_any(RecipeCatalog::zen_name, RecipeCatalog::zen_version);
    g.allow_to_any(OfferArtifact::zen_name, OfferArtifact::zen_version);
    return g;
}

/// The runner, watched directly by whoever holds the Builder office.
///
/// THE CATALOG IS THE RIG'S, NOT THE RUNNER'S (PROJ-0), and the field order is the
/// lifetime: `catalog` is declared above the bus, so it outlives the weave that reads
/// it -- exactly as the host declares its owner above the bus for the same reason. The
/// runner takes a reference and keeps no copy, so a fixture that wants to change what
/// this runner can build changes `catalog` and nothing else.
struct Bench {
    std::vector<Recipe> catalog;
    loom::Switchboard bus;
    BuildRunnerWeave* runner = nullptr;
    Foreman* foreman = nullptr;
    TimerClerk* clerk = nullptr;
    loom::WeaveId runner_id{};

    explicit Bench(std::vector<Recipe> recipes) : catalog(std::move(recipes)) {
        runner_id = mount_office<BuildRunnerWeave>(bus, runner_grant(), kBuildRunnerRole, &runner,
                                                   catalog, std::string(kCMake));
        mount_office<Foreman>(bus, loom::Grant{}, kBuilderRole, &foreman);
        mount_office<TimerClerk>(bus, loom::Grant{}, timer::kTimerRole, &clerk);
    }

    /// Order a build the way the tool does.
    void order(const std::string& recipe) {
        (void)bus.send_to_role(kBuildRunnerRole, loom::Message(loom::to_value(RunBuild{recipe})));
        bus.drain_until_idle();
    }

    /// One beat, exactly as the Timer would deliver it.
    void beat() {
        (void)bus.send_to_role(
            kBuildRunnerRole,
            loom::Message(loom::to_value(timer::TimerFired{std::string(kLookTimerId)})));
        bus.drain_until_idle();
    }

    /// Beat until nothing is held any more.
    std::int64_t beat_until_idle(int guard = 2000000) {
        std::int64_t beats = 0;
        while (guard-- > 0 && runner->live() > 0) {
            beat();
            ++beats;
        }
        return beats;
    }
};

/// A live Builder: the tool in its office, the runner in its own, a listener, a
/// bystander to count what the bus carried, and a clerk holding the Timer's
/// office. The grants are the host's.
struct Live {
    std::vector<Recipe> catalog;
    std::vector<RecipeView> views;
    loom::Switchboard bus;
    BuilderWeave* tool = nullptr;
    BuildRunnerWeave* runner = nullptr;
    Listener* ears = nullptr;
    Bystander* bystander = nullptr;
    TimerClerk* clerk = nullptr;
    loom::WeaveId tool_id{};
    loom::WeaveId runner_id{};
    loom::WeaveId bystander_id{};

    /// ONE CATALOG, TWO VIEWS OF IT (BLD-1). The runner reads the authored recipes;
    /// the tool reads the reduced view -- identity, artifact, and the one file it
    /// means -- exactly as the host derives one from the other. A fixture that gave
    /// both halves the same object would be a fixture in which the split this package
    /// is built on had quietly stopped existing.
    ///
    /// BOTH ARE THE RIG'S OWN (PROJ-0), declared above the bus so they outlive the
    /// weaves that read them. Neither weave keeps a copy any more, which is what lets
    /// a case below change `catalog`/`views` in ONE place and ask what the two weaves
    /// then answer.
    explicit Live(std::vector<Recipe> recipes) : catalog(std::move(recipes)) {
        views.reserve(catalog.size());
        for (const Recipe& r : catalog) {
            views.push_back(view_of(r));
        }
        runner_id = mount_office<BuildRunnerWeave>(bus, runner_grant(), kBuildRunnerRole, &runner,
                                                   catalog, std::string(kCMake));
        tool_id = mount_office<BuilderWeave>(bus, tool_grant(), kBuilderRole, &tool, views);
        mount_office<TimerClerk>(bus, loom::Grant{}, timer::kTimerRole, &clerk);
        mount_plain<Listener>(bus, loom::Grant{}, &ears);
        bystander_id = mount_plain<Bystander>(bus, loom::Grant{}, &bystander);
    }

    /// Speak to the Builder office the way a presentation does -- as a root send,
    /// ungated, because what this suite is measuring here is the TOOL's answer
    /// and not the host's grant (which has its own case below).
    template <class T>
    void tell_tool(const T& msg) {
        (void)bus.send_to_role(kBuilderRole, loom::Message(loom::to_value(msg)));
        bus.drain_until_idle();
    }
    template <class T>
    void tell_runner(const T& msg) {
        (void)bus.send_to_role(kBuildRunnerRole, loom::Message(loom::to_value(msg)));
        bus.drain_until_idle();
    }

    void beat() { tell_runner(timer::TimerFired{std::string(kLookTimerId)}); }

    /// ORDINARY LIFE, CARRIED. Unrelated traffic and the runner's beat, over and
    /// over, until the tool says the build is over -- and the answer is how many
    /// unrelated deliveries were carried while it was not.
    std::int64_t carry_until_over(int guard = 2000000) {
        const std::int64_t before = bystander->seen();
        while (guard-- > 0 && still_going(tool->known().outcome)) {
            (void)bus.send(bystander_id, loom::Message(loom::to_value(loom::Ack{})));
            (void)bus.send_to_role(
                kBuildRunnerRole,
                loom::Message(loom::to_value(timer::TimerFired{std::string(kLookTimerId)})));
            bus.drain_until_idle();
        }
        return bystander->seen() - before;
    }
};

} // namespace

// ============================================================================
// Tier 1 -- the wire cannot spell a command, and an observation is one moment
// ============================================================================

TEST_CASE("contract: nothing a build conversation carries is a command") {
    using loom::Kind;
    using loom::SchemaBuilder;

    // THE HARD BOUNDARY OF BLD-0, asserted as a SHAPE rather than as a check,
    // and unchanged by ASYNC-1. Both shapes that can cause work carry exactly
    // one field, and it is a target NAME. There is no program, no argument list,
    // no working directory and no shell line anywhere on this wire -- so "the
    // panel sent a command" is not a sentence this vocabulary can express, and a
    // phase that wanted to make it one would have to change these declarations
    // and this case with them.
    const auto requested = SchemaBuilder("BuildRequested", 2)
                               .field("recipe", Kind::Text)
                               .field("realize", Kind::Bool)
                               .build();
    CHECK(schema_of<BuildRequested>()->content_id() == requested->content_id());

    const auto order = SchemaBuilder("RunBuild", 2).field("recipe", Kind::Text).build();
    CHECK(schema_of<RunBuild>()->content_id() == order->content_id());

    // The ask that opens a panel carries nothing at all: there is one tool at
    // the office and it has one condition, so the question has no parameters.
    const auto describe = SchemaBuilder("StatusRequested", 1).build();
    CHECK(schema_of<StatusRequested>()->content_id() == describe->content_id());

    // ...and neither does the door that makes the runner look at what it already
    // holds. Looking is not starting: there is no field here to name a thing to
    // start, which is what keeps this a diagnostic door rather than a second way
    // in.
    const auto look = SchemaBuilder("LookAtBuilds", 1).build();
    CHECK(schema_of<LookAtBuilds>()->content_id() == look->content_id());
}

TEST_CASE("contract: the three moments of a build are three shapes") {
    using loom::Kind;
    using loom::SchemaBuilder;

    // ASYNC-1'S OWN BOUNDARY. BLD-0 had one `BuildOutcome` carrying `started`
    // and `status` together, which was truthful only because both facts became
    // true in the same instant. They no longer do -- minutes can pass between a
    // process starting and it exiting -- so a single shape would have to either
    // wait for the end (the freeze this phase removed) or lie about a field.
    // Three shapes, three moments, and each one says what was seen when it was
    // seen.
    const auto started = SchemaBuilder("BuildStarted", 2)
                             .field("op", Kind::Int)
                             .field("recipe", Kind::Text)
                             .field("command", Kind::Text)
                             .build();
    CHECK(schema_of<BuildStarted>()->content_id() == started->content_id());

    const auto output = SchemaBuilder("BuildOutput", 2)
                            .field("op", Kind::Int)
                            .field("recipe", Kind::Text)
                            .field("text", Kind::Text)
                            .field("dropped", Kind::Int)
                            .build();
    CHECK(schema_of<BuildOutput>()->content_id() == output->content_id());

    const auto finished = SchemaBuilder("BuildFinished", 2)
                              .field("op", Kind::Int)
                              .field("recipe", Kind::Text)
                              .field("status", Kind::Int)
                              .build();
    CHECK(schema_of<BuildFinished>()->content_id() == finished->content_id());

    // AND THE FOURTH, WHICH IS THE ONE THAT IS NOT AN ENDING OF A BUILD. "There
    // is no compiler" and "the compiler said no" are different problems needing
    // different next actions, and BLD-0's `started` flag became this shape when
    // the two stopped arriving together.
    const auto never = SchemaBuilder("BuildNotStarted", 2)
                           .field("op", Kind::Int)
                           .field("recipe", Kind::Text)
                           .field("command", Kind::Text)
                           .field("trouble", Kind::Text)
                           .build();
    CHECK(schema_of<BuildNotStarted>()->content_id() == never->content_id());
}

// ============================================================================
// Tier 2 -- the held process, with no bus in sight
// ============================================================================

TEST_CASE("a started recipe is HELD: start returns, and the child is still alive") {
    RecipeStart begun = start_recipe(slow(4, "0.15"));
    REQUIRE(begun.started);
    // THE WHOLE PROPERTY, IN ONE LINE: the call that started it has returned and
    // the child has not finished. BLD-0 could not produce this state at all --
    // there was no moment between the two.
    CHECK(begun.process.holds());
    const RunLook first = begun.process.look();
    CHECK_FALSE(first.ended);
    CHECK(begun.process.holds());

    const Drained all = drain(begun.process);
    CHECK(all.ended);
    CHECK(all.status == 0);
    CHECK(all.said.find("step 1 of 4") != std::string::npos);
    CHECK(all.said.find("done") != std::string::npos);
    // OUTPUT ARRIVED IN PIECES, which is the second half of the same property: a
    // build whose output could only be read at the end would have spoken on
    // exactly one look.
    CHECK(all.looks_that_spoke > 1);
}

TEST_CASE("nothing already reported is reported again") {
    RecipeStart begun = start_recipe(slow(3, "0.1"));
    REQUIRE(begun.started);
    const Drained all = drain(begun.process);
    REQUIRE(all.ended);

    // The pipe is drained BY reading it, so a byte handed over cannot be handed
    // over twice. Counted rather than eyeballed: each of the script's lines is
    // unique and each must appear exactly once across every look.
    const auto count = [&all](const std::string& needle) {
        std::size_t n = 0;
        for (std::size_t at = all.said.find(needle); at != std::string::npos;
             at = all.said.find(needle, at + 1)) {
            ++n;
        }
        return n;
    };
    CHECK(count("step 1 of 3") == 1);
    CHECK(count("step 2 of 3") == 1);
    CHECK(count("step 3 of 3") == 1);
    CHECK(count("slow build: done") == 1);
}

TEST_CASE("an ending is observed once, and the custody is then empty") {
    RecipeStart begun = start_recipe(echoes("hello"));
    REQUIRE(begun.started);
    const Drained all = drain(begun.process);
    REQUIRE(all.ended);

    // REAPED EXACTLY ONCE. The handle holds nothing afterwards, so the
    // destructor has nothing to do and cannot wait on, signal, or reap a pid
    // that has already been collected -- which on a recycled pid would be a
    // signal sent to somebody else's process.
    CHECK_FALSE(begun.process.holds());
    const RunLook after = begun.process.look();
    CHECK_FALSE(after.ended);
    CHECK(after.fresh.empty());
}

TEST_CASE("a failing recipe ends with its own status, and a missing one never ran") {
    RecipeStart failing = start_recipe(slow(1, "0.05", /*fail=*/true));
    REQUIRE(failing.started);
    const Drained bad = drain(failing.process);
    CHECK(bad.ended);
    CHECK(bad.status != 0);
    CHECK_FALSE(bad.never_ran);
    // ...AND ITS LAST WORDS SURVIVED THE ENDING. The ending is only reported
    // after the output has ended, so a diagnostic written just before exit is
    // read before the exit is noticed. This is the case that would fail if
    // `look()` reaped first and drained second.
    CHECK(bad.said.find("asked to fail") != std::string::npos);

    // A program that is not there is a different answer entirely, and on POSIX
    // it can only arrive here -- at the ending -- because a failed exec has no
    // other channel (builder/run.hpp).
    RecipeStart absent =
        start_recipe(BuildCommand{"zengine-no-such-program-async1", {"--version"},
                                 std::string()});
#if defined(_WIN32)
    CHECK_FALSE(absent.started);
    CHECK_FALSE(absent.trouble.empty());
#else
    REQUIRE(absent.started);
    const Drained nothing = drain(absent.process);
    CHECK(nothing.ended);
    CHECK(nothing.never_ran);
    CHECK(nothing.trouble.find("not found") != std::string::npos);
#endif

    // A recipe with no program at all is refused before anything is attempted,
    // on every platform.
    const RecipeStart empty = start_recipe(BuildCommand{std::string(), {}, std::string()});
    CHECK_FALSE(empty.started);
    CHECK(empty.trouble.find("no program") != std::string::npos);
}

TEST_CASE("abandoning custody does not wait for the build, and leaves nothing behind") {
    // A BUILD LONG ENOUGH THAT WAITING FOR IT WOULD BE VISIBLE. If `abandon()`
    // waited for the child on its own terms this would take ten seconds; it
    // terminates first and then reaps, so it takes milliseconds. The margin is
    // deliberately enormous, because a timing assertion with a tight one is a
    // flake waiting to happen.
    RecipeStart begun = start_recipe(slow(20, "0.5"));
    REQUIRE(begun.started);
    REQUIRE(begun.process.holds());

    const auto before = std::chrono::steady_clock::now();
    begun.process.abandon();
    const auto took = std::chrono::duration_cast<std::chrono::milliseconds>(
                          std::chrono::steady_clock::now() - before)
                          .count();

    CHECK_FALSE(begun.process.holds());
    CHECK(took < 3000);
    // ...AND IT SAID NOTHING, because there is nothing here to say it with. The
    // handle produced no ending, no status and no trouble: "the holder stopped
    // holding" is not "the build stopped", and this type refuses to author the
    // second. A second abandon is a no-op, which is what makes the destructor
    // safe after an explicit one.
    begun.process.abandon();
    CHECK_FALSE(begun.process.holds());
}

TEST_CASE("the tail a panel is shown keeps whole lines, and keeps them APART") {
    // The first live run merged two of a build's lines into one sentence that
    // never happened -- `Built target SDL3-shared [100%] Built target
    // zengine-snake` -- because the newline became a space. The separator is
    // what fixed it, and this is what keeps it fixed.
    const std::string three = "first line\nsecond line\nthird line\n";
    CHECK(tail_lines(three, 3) == "first line | second line | third line");
    CHECK(tail_lines(three, 2) == "second line | third line");
    CHECK(tail_lines(three, 1) == "third line");
    CHECK(tail_lines(three, kAllLines) == "first line | second line | third line");
    CHECK(tail_lines("", 3).empty());
    CHECK(tail_lines("\n\n\n", 3).empty());
    // Trailing blank lines are not what a maker meant by "the last line".
    CHECK(tail_lines("only\n\n\n", 2) == "only");
}

TEST_CASE("a fragment waits for its newline, and the ending releases it") {
    // WHOLE LINES, SO A MAKER IS NEVER SHOWN HALF A PATH. A look lands wherever
    // the child happened to be writing, so the tail of a drain is routinely a
    // fragment -- and a panel that showed `/usr/include/foo` when the build said
    // `/usr/include/foobar.h: No such file` would be worse than one that waited
    // a beat.
    std::string buffer = "one\ntwo\nthr";
    CHECK(take_complete_lines(buffer, /*ending=*/false) == "one\ntwo\n");
    CHECK(buffer == "thr");
    CHECK(take_complete_lines(buffer, false).empty()); // still no newline: still held back
    CHECK(buffer == "thr");
    buffer += "ee";
    // ...UNTIL THERE WILL BE NO LATER NEWLINE. A last line without one is still
    // something the build said.
    CHECK(take_complete_lines(buffer, /*ending=*/true) == "three");
    CHECK(buffer.empty());
}

// ============================================================================
// Tier 3 -- the runner: custody, identity, and the beat
// ============================================================================

TEST_CASE("the handler that starts a build RETURNS, and the child is still running") {
    Bench bench({cmake_recipe("slow", "fixture-slow5")});
    bench.order("slow");

    // The pump that delivered `RunBuild` has drained. If the runner had built
    // inside its handler, this operation would already be finished.
    CHECK(bench.runner->live() == 1);
    CHECK(bench.runner->ran() == 1);
    REQUIRE(bench.foreman->started.size() == 1);
    CHECK(bench.foreman->started[0].op == 1);
    CHECK(bench.foreman->started[0].recipe == "slow");
    // WHAT IS RUNNING IS SAYABLE WHILE IT RUNS. BLD-0 could only ever describe a
    // command after it had finished, because that was the first moment anything
    // could speak.
    CHECK(bench.foreman->started[0].command.find("--target fixture-slow5") != std::string::npos);
    CHECK(bench.foreman->finished.empty());

    const std::int64_t beats = bench.beat_until_idle();
    CHECK(beats > 1); // it took more than one look, so it really was still going
    REQUIRE(bench.foreman->finished.size() == 1);
    CHECK(bench.foreman->finished[0].op == 1);
    CHECK(bench.foreman->finished[0].status == 0);
    CHECK(bench.runner->live() == 0);
}

TEST_CASE("output is attributed to its own operation, and two can run at once") {
    // ASYNC-0'S PROBE PROVED TWO HELD OPERATIONS CAN COEXIST; this is the same
    // claim about the shipped runner. It is driven at the RUNNER, not the tool,
    // because refusing a second build is the tool's product policy and this case
    // is about the mechanism underneath it -- the two must be able to disagree,
    // or the policy is a limitation wearing a policy's clothes.
    //
    // ONE TREE EACH, BECAUSE THE SUBJECT IS BUILDER'S CONCURRENCY AND NOT THE
    // GENERATOR'S. Both operations pointed at one build tree until a generator's own
    // bookkeeping was measured colliding there -- one child aborting before it built
    // anything, whose only output was that abort. Two trees is what a maker's two
    // recipes are anyway, and it leaves this case measuring what it names.
    Bench bench({cmake_recipe("alpha", "fixture-slow4"),
                 cmake_recipe_in(kFixtureTreeB, "beta", "fixture-slow4")});
    bench.order("alpha");
    bench.order("beta");
    CHECK(bench.runner->live() == 2);
    REQUIRE(bench.foreman->started.size() == 2);
    const std::int64_t alpha = bench.foreman->started[0].op;
    const std::int64_t beta = bench.foreman->started[1].op;
    CHECK(alpha != beta); // one identity per operation, and they are not shared

    bench.beat_until_idle();
    CHECK(bench.runner->live() == 0);
    REQUIRE(bench.foreman->finished.size() == 2);
    // BOTH OF THEM ACTUALLY BUILT SOMETHING, asserted before anything is asked about
    // their output. A child that died before reaching its target still ends, still ends
    // exactly once and is still attributed correctly -- so without this line the only
    // case that notices is the one below, and it reports a missing line rather than the
    // build that never happened.
    for (const BuildFinished& f : bench.foreman->finished) {
        CHECK(f.status == 0);
    }

    // EVERY OUTPUT FACT NAMES THE OPERATION IT BELONGS TO, and the two streams
    // did not mix: `alpha`'s output is about alpha and nothing else.
    for (const BuildOutput& o : bench.foreman->output) {
        CHECK((o.op == alpha || o.op == beta));
        CHECK(o.recipe == (o.op == alpha ? "alpha" : "beta"));
    }
    CHECK(bench.foreman->text_of(alpha).find("step 1 of 4") != std::string::npos);
    CHECK(bench.foreman->text_of(beta).find("step 1 of 4") != std::string::npos);
    CHECK(bench.foreman->finished[0].op != bench.foreman->finished[1].op);
}

TEST_CASE("an ending is published once, and looking again publishes nothing") {
    Bench bench({cmake_recipe("quick", "fixture-quick")});
    bench.order("quick");
    bench.beat_until_idle();
    REQUIRE(bench.foreman->finished.size() == 1);

    const std::size_t seen = bench.foreman->started.size() + bench.foreman->output.size() +
                             bench.foreman->finished.size() + bench.foreman->never.size();
    // TWENTY MORE BEATS OVER AN EMPTY TABLE SAY NOTHING AT ALL. This is the
    // "no continuous stream of still-not-done" claim, and the "reaped exactly
    // once" claim, measured with the same twenty beats.
    for (int i = 0; i < 20; ++i) {
        bench.beat();
    }
    const std::size_t after = bench.foreman->started.size() + bench.foreman->output.size() +
                              bench.foreman->finished.size() + bench.foreman->never.size();
    CHECK(after == seen);
    CHECK(bench.runner->live() == 0);
}

TEST_CASE("the runner asks for a beat when it takes custody, and gives it back") {
    // POLLING IS CONTAINED, AND THIS IS WHERE THAT IS A MEASUREMENT. The runner
    // polls its own handles -- but only while it is holding one, and this case
    // walks the whole conversation a live Workshop has: the Timer arrives, the
    // binding is established, the first beat finds nothing and hands it back,
    // a build establishes it again, and the ending hands it back again.
    Bench bench({cmake_recipe("quick", "fixture-quick")});

    // The service arrives. The declared binding is reconciled -- one ask.
    (void)bench.bus.send_to_role(kBuildRunnerRole,
                                 loom::Message(loom::to_value(timer::TimerReady{})));
    bench.bus.drain_until_idle();
    CHECK(bench.clerk->asked() == 1);
    CHECK(bench.clerk->cancelled() == 0);

    // ...and the very first firing, holding nothing, gives it straight back.
    // That single beat per run is the price of the binding being declared at
    // all, and it is what proves the wiring exists.
    bench.beat();
    CHECK(bench.clerk->cancelled() == 1);

    // Idle beats after that change nothing: there is no beat to cancel twice.
    bench.beat();
    bench.beat();
    CHECK(bench.clerk->cancelled() == 1);
    CHECK(bench.clerk->asked() == 1);

    // A build arrives: custody, therefore a beat.
    bench.order("quick");
    CHECK(bench.clerk->asked() == 2);
    bench.beat_until_idle();
    CHECK(bench.clerk->cancelled() == 2);
}

TEST_CASE("the direct door looks at what is held, and cannot start anything") {
    // `LookAtBuilds` is the same hands the beat opens, for suites, diagnostics
    // and hosts with no Timer service -- `zengine::input::PumpInput` is the
    // precedent, on the same kind of weave, for the same reason.
    Bench bench({cmake_recipe("slow", "fixture-slow2")});
    bench.order("slow");
    REQUIRE(bench.runner->live() == 1);

    const std::int64_t looks_before = bench.runner->looks();
    int guard = 2000000;
    while (guard-- > 0 && bench.runner->live() > 0) {
        (void)bench.bus.send_to_role(kBuildRunnerRole,
                                     loom::Message(loom::to_value(LookAtBuilds{})));
        bench.bus.drain_until_idle();
    }
    CHECK(bench.runner->looks() > looks_before);
    REQUIRE(bench.foreman->finished.size() == 1);

    // ...AND IT WIDENS NOTHING. A hundred looks at an empty table start nothing
    // and say nothing: there is no field on this shape with which to name a
    // thing to run.
    const std::int64_t ran = bench.runner->ran();
    for (int i = 0; i < 100; ++i) {
        (void)bench.bus.send_to_role(kBuildRunnerRole,
                                     loom::Message(loom::to_value(LookAtBuilds{})));
    }
    bench.bus.drain_until_idle();
    CHECK(bench.runner->ran() == ran);
    CHECK(bench.foreman->started.size() == 1);
}

TEST_CASE("a name the runner holds no recipe for names no operation at all") {
    Bench bench({cmake_recipe("quick", "fixture-quick")});
    bench.order("not-in-the-catalog");

    CHECK(bench.runner->ran() == 0);
    CHECK(bench.runner->refused() == 1);
    CHECK(bench.runner->live() == 0);
    REQUIRE(bench.foreman->never.size() == 1);
    // `op` IS 0 AND THE ABSENCE IS THE STATEMENT: nothing was ever held, so
    // there is nothing to name, and minting an identity for a thing that never
    // existed would be an identity that could never be referred to again.
    CHECK(bench.foreman->never[0].op == 0);
    CHECK(bench.foreman->never[0].command.empty());
    CHECK(bench.foreman->never[0].trouble.find("not-in-the-catalog") != std::string::npos);
    CHECK(bench.foreman->started.empty());

    // THE CANARY: a name that IS in the catalog runs, through the same door.
    bench.order("quick");
    CHECK(bench.runner->ran() == 1);
    CHECK(bench.foreman->started.size() == 1);
}

// ============================================================================
// Tier 4 -- ordinary Loom life goes on, and the control that proves it
// ============================================================================

TEST_CASE("unrelated deliveries continue while a real child runs") {
    Live live({cmake_recipe("slow", "fixture-slow5")});
    live.tell_tool(BuildRequested{"slow"});

    // THE HANDLER RETURNED BEFORE THE CHILD EXITED, said as a state rather than
    // as a duration: the tool has been told a process began and has not been
    // told it ended.
    CHECK(live.tool->known().outcome == outcome::kRunning);
    CHECK(live.tool->known().op == 1);
    CHECK(live.runner->live() == 1);

    const std::int64_t carried = live.carry_until_over();
    CHECK(live.tool->known().outcome == outcome::kSucceeded);
    CHECK(live.tool->known().status == 0);
    // HUNDREDS, AND BOUNDED ONLY BY HOW OFTEN ANYBODY LOOKS. The number itself
    // is a measurement of this machine and this cadence and must never be
    // quoted as a constant; what is asserted is that it is not small, because
    // the alternative shape can only ever produce zero.
    CHECK(carried > 50);
    // ...and the build was WATCHED, not merely found finished: at least one
    // observation about it was folded in before the ending arrived.
    //
    // ⚠ HOW MANY IS THE BUILD SYSTEM'S BUSINESS AND NOT ZENGINE'S, and BLD-1
    // measured that on two lanes rather than assuming it. Driving a REAL
    // `cmake --build --target` puts a generator between the script and the pipe:
    // Unix Makefiles streams a command's output as it is written (many chunks),
    // and Ninja hands the whole of one command's output over when that command
    // ends (exactly one). Neither is a fact about the runner, which publishes an
    // observation for every look that found new bytes.
    //
    // THE PROPERTY THAT IS ZENGINE'S -- output arriving in PIECES from a live
    // pipe -- is pinned where it belongs and platform-independently, in the
    // `RunningRecipe` tier above, which drives the slow script with no build
    // system in the way and requires more than one look to have spoken.
    CHECK(live.tool->known().chunks >= 1);
}

TEST_CASE("REGRESSION: the old blocking shape carries nothing at all") {
    // THE CANARY FOR THE WHOLE PHASE. It restores BLD-0's runner -- the same
    // platform code, driven to completion inside its own handler -- and measures
    // the same thing the case above measures. The two numbers are different
    // KINDS of number: the one above is a measurement that moves between runs,
    // and this one is STRUCTURAL. Nothing can be delivered between two
    // publications made in one handler, at any cadence, ever.
    loom::Switchboard bus;

    Foreman* foreman = nullptr;
    mount_office<Foreman>(bus, loom::Grant{}, kBuilderRole, &foreman);

    loom::Grant may_report;
    may_report.allow_to_role(BuildStarted::zen_name, BuildStarted::zen_version, kBuilderRole);
    may_report.allow_to_role(BuildFinished::zen_name, BuildFinished::zen_version, kBuilderRole);
    may_report.allow_to_role(BuildNotStarted::zen_name, BuildNotStarted::zen_version,
                             kBuilderRole);
    BlockingRunnerWeave* blocking = nullptr;
    mount_office<BlockingRunnerWeave>(bus, std::move(may_report), kBuildRunnerRole, &blocking,
                                      std::string("slow"), slow(5, "0.15"));

    Bystander* bystander = nullptr;
    const loom::WeaveId bystander_id = mount_plain<Bystander>(bus, loom::Grant{}, &bystander);

    (void)bus.send_to_role(kBuildRunnerRole, loom::Message(loom::to_value(RunBuild{"slow"})));
    bus.drain_until_idle();

    // ONE PUMP, AND THE BUILD IS ALREADY OVER. There was no moment in between to
    // deliver anything into: the start and the ending were published from the
    // same stack frame.
    REQUIRE(foreman->started.size() == 1);
    REQUIRE(foreman->finished.size() == 1);
    const std::int64_t carried = bystander->seen();
    CHECK(carried == 0);

    // THE CANARY'S OWN CANARY: this bus is not simply broken. The same bystander
    // receives ordinary traffic perfectly well once nothing is holding the pump.
    (void)bus.send(bystander_id, loom::Message(loom::to_value(loom::Ack{})));
    bus.drain_until_idle();
    CHECK(bystander->seen() == 1);
}

// ============================================================================
// Tier 5 -- the tool: a name, a memory, a running build, and a refusal
// ============================================================================

TEST_CASE("a fresh tool says what can be built here and that nothing has been") {
    Live live({cmake_recipe("greet", "fixture-quick")});
    live.tell_tool(StatusRequested{});

    REQUIRE(live.ears->said.size() == 1);
    const BuildStatus& said = live.ears->last();
    // THE FRESH TOOL NAMES NO RECIPE, and that is BLD-1 rather than an omission:
    // `recipe` is what the tool is BUILDING or last built, and a tool that had
    // never been asked for anything is about nothing. What a maker needs before
    // any build has happened is the CATALOG, which is the second shape below.
    CHECK(said.recipe.empty());
    CHECK(said.artifact.empty());
    CHECK(said.outcome == outcome::kNeverBuilt);
    CHECK(said.builds == 0);
    CHECK(said.op == 0);
    CHECK(said.realization == realization::kNotAsked);
    REQUIRE(live.ears->catalogs.size() == 1);
    REQUIRE(live.ears->catalogs[0].recipes.size() == 1);
    CHECK(live.ears->catalogs[0].recipes[0].recipe == "greet");
    CHECK(live.ears->catalogs[0].recipes[0].artifact == "fixture-quick");
    // IT HOLDS NO COMMAND, and this is where that is visible: the tool can say
    // what it is before anything has run, and it cannot say what would be run,
    // because it does not know.
    CHECK(said.command.empty());
    CHECK(live.runner->ran() == 0);
}

TEST_CASE("an ask for the known target reaches a real process, and the answer is the truth") {
    Live live({cmake_recipe("greet", "fixture-quick")});
    live.tell_tool(BuildRequested{"greet"});
    live.carry_until_over();

    // The publications read in the order the facts became true: asked, running,
    // what it said, then how it ended.
    REQUIRE(live.ears->said.size() >= 3);
    CHECK(live.ears->said[0].outcome == outcome::kAsked);
    CHECK(live.ears->said[1].outcome == outcome::kRunning);

    const BuildStatus& done = live.ears->last();
    CHECK(done.outcome == outcome::kSucceeded);
    CHECK(done.status == 0);
    CHECK(done.builds == 1);
    CHECK(done.op == 1);
    CHECK(done.detail.find("nothing to build, said quickly") != std::string::npos);
    // THE RECIPE ARRIVED WITH THE START, from the thing that started it.
    CHECK(done.command.find("--build") != std::string::npos);
    CHECK(live.runner->ran() == 1);
    CHECK(live.runner->refused() == 0);
}

TEST_CASE("a failing build is reported as a failure, with its own exit status") {
    Live live({cmake_recipe("brk", "fixture-broken")});
    live.tell_tool(BuildRequested{"brk"});
    live.carry_until_over();

    const BuildStatus& done = live.ears->last();
    CHECK(done.outcome == outcome::kFailed);
    CHECK(done.status != 0);
    CHECK(live.runner->ran() == 1);
    // THE BUILD SYSTEM'S OWN REFUSAL SURVIVED to the tool's bounded tail. The
    // build's INNERMOST last words -- the script's own `asked to fail` -- are
    // several lines further up than a three-row panel can hold once the build
    // system has added its own; that they reached the office at all is pinned
    // one layer down, at the Bench, where the whole stream is visible.
    CHECK_FALSE(done.detail.empty());
    CHECK(done.detail.find("Error") != std::string::npos);
    // AND NOTHING WAS OFFERED TO A PROJECT. Nobody asked for realization here,
    // so the second axis never moved -- which is the ordinary case and is worth
    // pinning beside the one where it does.
    CHECK(done.realization == realization::kNotAsked);
    CHECK(live.tool->known().offered == 0);
}

TEST_CASE("a failing build's OWN last words reach the office that asked (BLD-1)") {
    // THE SAME CLAIM ASYNC-1 MADE, MEASURED WHERE THE WHOLE STREAM IS. The tool
    // publishes a bounded tail for a panel with three rows; the `BuildOutput`
    // facts carry everything, and the office that receives them is where "the
    // diagnostic written just before the child exited was not lost" is checkable
    // without a row budget in the way.
    Bench bench({cmake_recipe("brk", "fixture-broken")});
    bench.order("brk");
    bench.beat_until_idle();

    REQUIRE(bench.foreman->finished.size() == 1);
    CHECK(bench.foreman->finished[0].status != 0);
    const std::int64_t op = bench.foreman->finished[0].op;
    CHECK(bench.foreman->text_of(op).find("asked to fail") != std::string::npos);
}

TEST_CASE("a build that never starts is not a build that failed") {
    // THE WAY A RECIPE FAILS TO BECOME A PROCESS CHANGED WITH BLD-1, and this case
    // followed it. A recipe cannot name a program, so "the program is not there" is
    // no longer reachable through one; what IS reachable, and is the ordinary maker
    // mistake, is a recipe pointing at a CMake build tree nobody ever configured.
    // It is refused before a child exists, with the path named, on every platform.
    Recipe nowhere = cmake_recipe("gone", "anything", "zengine-fixture-gone");
    nowhere.cmake_target->build_dir = std::string(kFixtureTree) + "-that-does-not-exist";
    Live live({nowhere});
    live.tell_tool(BuildRequested{"gone"});
    live.carry_until_over();

    const BuildStatus& done = live.ears->last();
    CHECK(done.outcome == outcome::kNotStarted);
    CHECK_FALSE(done.detail.empty());
    CHECK(done.detail.find("CMakeCache.txt") != std::string::npos);
    // NOTHING RAN AT ALL, which is the guarantee: `ran` counts processes this
    // runner tried to start, and a recipe that could not become a command never
    // reached that line.
    CHECK(live.runner->ran() == 0);
    CHECK(live.runner->live() == 0);
}

TEST_CASE("the tool refuses a name it does not hold, and NOTHING is ordered") {
    Live live({cmake_recipe("greet", "fixture-quick")});
    live.tell_tool(BuildRequested{"something-else"});

    REQUIRE(live.ears->said.size() == 1);
    const BuildStatus& said = live.ears->last();
    CHECK(said.outcome == outcome::kUnknownRecipe);
    CHECK(said.detail.find("no recipe called") != std::string::npos); // it says so
    CHECK(said.detail.find("it holds 1") != std::string::npos);       // ...and how many it has
    CHECK(said.builds == 0);
    // The guarantee is not "the outcome was a refusal" -- it is that no process
    // began. The runner was never even reached.
    CHECK(live.runner->ran() == 0);
    CHECK(live.runner->refused() == 0);

    // THE CANARY: the same tool, asked for the name it does hold, runs. Without
    // this the case above would be satisfied by a Live that could not build at
    // all.
    live.tell_tool(BuildRequested{"greet"});
    live.carry_until_over();
    CHECK(live.runner->ran() == 1);
    CHECK(live.ears->last().outcome == outcome::kSucceeded);
}

TEST_CASE("ONE BUILD AT A TIME is the tool's policy, and it refuses in its own voice") {
    Live live({cmake_recipe("slow", "fixture-slow5")});
    live.tell_tool(BuildRequested{"slow"});
    REQUIRE(live.tool->known().outcome == outcome::kRunning);

    live.tell_tool(BuildRequested{"slow"});
    const BuildStatus& refused = live.ears->last();
    // IT IS STILL RUNNING -- the refusal does not change what is true, it says
    // why nothing new happened, and it names the operation that is in the way.
    CHECK(refused.outcome == outcome::kRunning);
    CHECK(refused.detail.find("already running") != std::string::npos);
    CHECK(refused.detail.find("#1") != std::string::npos);
    // `builds` COUNTS ASKS THIS TOOL TOOK, not asks it turned down. A counter
    // that did both would answer two questions and get one of them wrong.
    CHECK(refused.builds == 1);
    // ...and, the part that matters: no second process began.
    CHECK(live.runner->ran() == 1);
    CHECK(live.runner->live() == 1);

    live.carry_until_over();
    CHECK(live.tool->known().outcome == outcome::kSucceeded);

    // THE CANARY: once it is over, the same ask is taken.
    live.tell_tool(BuildRequested{"slow"});
    CHECK(live.tool->known().builds == 2);
    CHECK(live.runner->ran() == 2);
    live.carry_until_over();
}

TEST_CASE("a presentation opened mid-build learns from the TOOL that one is running") {
    // THE PANEL/TOOL SPLIT, ASKED THE ONE QUESTION BLD-0 COULD NOT ASK. A
    // synchronous build had no observable middle, so "what is happening right
    // now" had no answer for the whole time it mattered. This is that question,
    // answered by the tool, to a reader that arrived after the ask.
    Live live({cmake_recipe("slow", "fixture-slow5")});
    live.tell_tool(BuildRequested{"slow"});
    REQUIRE(live.runner->live() == 1);

    Listener* latecomer = nullptr;
    mount_plain<Listener>(live.bus, loom::Grant{}, &latecomer);
    live.tell_tool(StatusRequested{});

    REQUIRE(latecomer->said.size() == 1);
    CHECK(latecomer->said[0].outcome == outcome::kRunning);
    CHECK(latecomer->said[0].op == 1);
    CHECK(latecomer->said[0].recipe == "slow");
    CHECK(latecomer->said[0].command.find("--target fixture-slow5") != std::string::npos);

    live.carry_until_over();
    CHECK(live.tool->known().outcome == outcome::kSucceeded);
}

TEST_CASE("an observation about somebody else's work is counted, never adopted") {
    Live live({cmake_recipe("greet", "fixture-quick")});
    live.tell_tool(BuildFinished{4, "a-different-target", 0});

    // Nothing was published, because nothing about this tool changed.
    CHECK(live.ears->said.empty());
    CHECK(live.tool->known().outcome == outcome::kNeverBuilt);
    CHECK(live.tool->known().recipe.empty());
    // ...and it is a number rather than a silence, so "this never happens" is
    // something an operator can check instead of something a comment asserts.
    CHECK(live.tool->known().stray == 1);

    // THE SECOND HALF OF THE SAME QUESTION: this tool's own target, but an
    // operation it is not following. Folding that in would mix two builds'
    // output into one picture.
    live.tell_tool(BuildOutput{99, "greet", "output from a build nobody here asked for", 0});
    CHECK(live.ears->said.empty());
    CHECK(live.tool->known().stray == 2);
}

// ============================================================================
// Tier 6 -- who may cause a process to start
// ============================================================================

TEST_CASE("a weave with a presentation's reach cannot make a process start") {
    // THE AUTHORITY MEASUREMENT OF THIS PHASE, UNCHANGED BY IT. Workshop is
    // granted the right to ASK the Builder for a build by name; it is not
    // granted the right to order the runner. This case asserts the second half
    // by giving a weave exactly that reach and having it try the thing it is not
    // allowed to do.
    //
    // It asserts the PROPERTY and not a copy of the host's grant list: a rule
    // that permits `BuildRequested` to the Builder office and nothing else does
    // not permit `RunBuild` to the runner. A second copy of the host's grant
    // here would be a second answer to a question the host already answers, and
    // it would go stale the first time the host's list changed.
    Live live({cmake_recipe("greet", "fixture-quick")});

    Impostor* liar = nullptr;
    loom::Grant may_only_ask;
    may_only_ask.allow_to_role(BuildRequested::zen_name, BuildRequested::zen_version,
                               kBuilderRole);
    const loom::WeaveId id = mount_plain<Impostor>(live.bus, std::move(may_only_ask), &liar,
                                                   std::string("greet"));

    (void)live.bus.send(id, loom::Message(loom::to_value(loom::Ack{})));
    live.bus.drain_until_idle();

    CHECK(liar->tried == 1); // it really did try
    CHECK(live.runner->ran() == 0); // and no process began
    CHECK(live.runner->refused() == 0);
    CHECK(live.runner->live() == 0);

    // THE CANARY: the identical weave, with the runner in its grant, DOES make a
    // process start. Without it, "no process began" would be satisfied by an
    // Impostor whose send never compiled into anything, a role that was never
    // held, or a runner that could not run.
    Impostor* honest = nullptr;
    loom::Grant may_order;
    may_order.allow_to_role(RunBuild::zen_name, RunBuild::zen_version, kBuildRunnerRole);
    const loom::WeaveId other = mount_plain<Impostor>(live.bus, std::move(may_order), &honest,
                                                      std::string("greet"));

    (void)live.bus.send(other, loom::Message(loom::to_value(loom::Ack{})));
    live.bus.drain_until_idle();

    CHECK(honest->tried == 1);
    CHECK(live.runner->ran() == 1);
    live.carry_until_over();
}

TEST_CASE("a runner destroyed while holding work takes it with it, and says nothing") {
    // ASYNC-0'S LANE C, AS A PRODUCTION QUESTION. A holder that dies reaps its
    // work and publishes nothing -- the operation did not complete, did not fail
    // and was not cancelled; it stopped existing. That is not a defect to fix
    // here, it is a semantics to state: a destructor has no `Mail`, and a fact
    // authored from one would have to reach around the only door a weave speaks
    // through.
    //
    // WHAT MUST NOT HAPPEN is the other thing: an orphan, a zombie, or a false
    // completion. This case is the one that would fail if any of the three
    // appeared.
    Ledger ledger; // the CASE's own memory: it outlives the bus below
    // ...AND SO DOES THE CATALOG (PROJ-0). The runner reads it and does not own it, so
    // it is declared here, above the bus that is deliberately destroyed mid-case. The
    // constructor refuses a temporary outright, which is what stops this line from
    // being written the dangling way.
    const std::vector<Recipe> catalog{cmake_recipe("forever", "fixture-forever")};
    auto bus = std::make_unique<loom::Switchboard>();
    mount_office<Undertaker>(*bus, loom::Grant{}, kBuilderRole,
                             static_cast<Undertaker**>(nullptr), &ledger);
    BuildRunnerWeave* runner = nullptr;
    mount_office<BuildRunnerWeave>(*bus, runner_grant(), kBuildRunnerRole, &runner, catalog,
                                   std::string(kCMake));

    (void)bus->send_to_role(kBuildRunnerRole, loom::Message(loom::to_value(RunBuild{"forever"})));
    bus->drain_until_idle();
    REQUIRE(runner->live() == 1);
    REQUIRE(ledger.started == 1);
    REQUIRE(ledger.endings == 0);

    const auto at = std::chrono::steady_clock::now();
    bus.reset(); // every weave destroyed, custody with them
    const auto took =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - at)
            .count();

    // IT DID NOT WAIT FOR THE BUILD (which had ten seconds left to run), and it
    // did not leave it running: the destructor terminated and reaped.
    CHECK(took < 3000);
    // AND NO ENDING WAS INVENTED. Not a completion, not a failure, not a
    // cancellation -- the operation stopped existing, and the only honest number
    // for facts published about that is zero.
    CHECK(ledger.endings == 0);
}

namespace {

/// The operation identity the runner published, read out of the retained
/// `BuildStarted` payload. It is a payload field and not a header, which is
/// exactly ASYNC-1's point.
std::int64_t started_op(const loom::HistoryRecord& started, const loom::Recorder& history) {
    const loom::PayloadLookup body = history.payload(started.record_seq);
    REQUIRE(body.state == loom::PayloadState::Retained);
    loom::Unverified u = loom::parse(body.bytes);
    loom::Admission a = loom::admit(u, loom::schema_of<BuildStarted>());
    REQUIRE(a.ok());
    return a.value().get("op")->as_int();
}

} // namespace

// ============================================================================
// Tier 5 -- what the HOST remembers about a build (RTH-1)
// ============================================================================
//
// The recorder is Loom's, and its own suite proves what a record holds. What
// belongs HERE is the claim only a real held build can make: that dispatch
// ancestry tells the truth about an operation that outlived the turn that asked
// for it, and that a burst of one shape does not cost another shape its memory.

TEST_CASE("RTH-1: a build's story survives the turns that produced it") {
    Live live({cmake_recipe("slow", "fixture-slow4")});
    loom::RecorderPolicy policy = loom::default_policy();
    // The host's shape, reduced to what this case needs: the beat takes no recent
    // context and keeps no bytes (but stays findable), and the four observations
    // get a deep last-call slot of their own.
    policy.rules.push_back(loom::RetentionRule{std::string(timer::TimerFired::zen_name),
                                               /*last_n=*/1, /*in_recent=*/false,
                                               /*retain_payload=*/false});
    for (const char* shape : {BuildStarted::zen_name, BuildOutput::zen_name,
                              BuildFinished::zen_name, RunBuild::zen_name}) {
        policy.rules.push_back(loom::RetentionRule{std::string(shape), 64, true, true});
    }
    loom::Recorder history(live.bus);
    history.apply_policy(policy);

    live.tell_tool(BuildRequested{"slow"});
    live.carry_until_over();

    const std::vector<loom::HistoryRecord> all = history.snapshot();
    const auto find_one = [&all](const char* shape) -> const loom::HistoryRecord* {
        for (const loom::HistoryRecord& r : all) {
            if (r.shape == shape) {
                return &r;
            }
        }
        return nullptr;
    };
    const loom::HistoryRecord* order = find_one(RunBuild::zen_name);
    const loom::HistoryRecord* started = find_one(BuildStarted::zen_name);
    const loom::HistoryRecord* output = find_one(BuildOutput::zen_name);
    const loom::HistoryRecord* finished = find_one(BuildFinished::zen_name);
    REQUIRE(order != nullptr);
    REQUIRE(started != nullptr);
    REQUIRE(output != nullptr);
    REQUIRE(finished != nullptr);

    // SYNCHRONOUS ANCESTRY IS EXACT. `BuildStarted` was authored from inside the
    // handling of the `RunBuild` that asked for it.
    CHECK(started->dispatch_parent == order->seq);

    // ...AND ASYNC ANCESTRY DOES NOT LIE ABOUT ITSELF. Output arrived on a LATER
    // beat, so its dispatch parent is that beat -- not the order, and not the
    // request. This is the field's whole discipline in one assertion: it says
    // what was being dispatched, never what a thing is "about".
    CHECK(output->dispatch_parent != order->seq);
    CHECK(output->dispatch_parent > started->seq);
    CHECK(finished->dispatch_parent != order->seq);

    // AND THE SEMANTIC RELATION IS INTACT, in the payload, where the operation
    // identity lives (ASYNC-1: `op` is not a correlation and not an ancestry).
    const loom::PayloadLookup body = history.payload(output->record_seq);
    REQUIRE(body.state == loom::PayloadState::Retained);
    loom::Unverified u = loom::parse(body.bytes);
    loom::Admission a = loom::admit(u, loom::schema_of<BuildOutput>());
    REQUIRE(a.ok());
    CHECK(a.value().get("op")->as_int() == started_op(*started, history));

    // THE BEATS THAT CARRIED IT TOOK NO RECENT CONTEXT AND ARE STILL FINDABLE
    // (RTH-1a). Under RTH-1's policy vocabulary this had to be said as "counted,
    // not kept", which also meant a maker could not ask whether a beat had ever
    // arrived. Both halves are asserted here because both matter: the build's
    // story is legible, AND the heartbeat did not become invisible to buy that.
    for (const loom::HistoryRecord& r : history.recent()) {
        CHECK(r.shape != timer::TimerFired::zen_name);
    }
    const loom::Lookup last_beat = history.last_of(timer::TimerFired::zen_name);
    REQUIRE(last_beat.horizon == loom::Horizon::Retained);
    CHECK(loom::held_in(last_beat.record->held, loom::Held::LastCall));
    CHECK(!loom::held_in(last_beat.record->held, loom::Held::Recent));
    CHECK(last_beat.record->payload == loom::PayloadDisposition::NotRetained);
    bool beats_seen = false;
    for (const loom::ShapeTally& t : history.tallies()) {
        if (t.shape == timer::TimerFired::zen_name) {
            beats_seen = true;
            CHECK(t.observed > 0);
            CHECK(t.last_call_held == 1);
        }
    }
    CHECK(beats_seen);
    // ...and the build's own observations DID take their place in recent context,
    // because the order they arrived in relative to everything else is the story.
    bool build_in_context = false;
    for (const loom::HistoryRecord& r : history.recent()) {
        if (r.shape == BuildFinished::zen_name) {
            build_in_context = true;
        }
    }
    CHECK(build_in_context);

    // THE POLICY THAT DECIDED ALL OF THAT IS ITSELF REMEMBERED, once, and no
    // message was sent to remember it.
    std::size_t policy_records = 0;
    for (const loom::HistoryRecord& r : all) {
        policy_records += r.kind == loom::RecordKind::RecorderPolicy ? 1u : 0u;
    }
    CHECK(policy_records == 1);
}

TEST_CASE("RTH-1: a burst of output does not cost the build its beginning") {
    // THE CLAIM A LAST-CALL SLOT MAKES, measured against a recent FIFO that is
    // deliberately too small. Without it, a talkative build erases the record of
    // its own start; with it, the start is still there when the maker looks.
    Live live({cmake_recipe("chatty", "fixture-chatty6")});
    loom::RecorderPolicy policy = loom::default_policy();
    policy.recent_capacity = 4;
    policy.rules.push_back(loom::RetentionRule{std::string(timer::TimerFired::zen_name), 1,
                                              false, false});
    policy.rules.push_back(
        loom::RetentionRule{std::string(BuildStarted::zen_name), 32, true, true});
    loom::Recorder history(live.bus);
    history.apply_policy(policy);

    live.tell_tool(BuildRequested{"chatty"});
    live.carry_until_over();

    std::size_t starts = 0;
    for (const loom::HistoryRecord& r : history.snapshot()) {
        starts += r.shape == BuildStarted::zen_name ? 1u : 0u;
    }
    CHECK(starts == 1);                    // still there
    CHECK(history.bounds().forgotten > 0); // ...and the recent window did lose things
}

// ============================================================================
// Tier 5b -- what the host CHOSE NOT TO FORGET about a build (RTH-1a)
// ============================================================================

TEST_CASE("RTH-1a: a finished build is durable; the thousand lines it printed are not") {
    // THE HOST'S OWN SELECTION, made falsifiable by a real held build. `Workshop`
    // adds exactly two application shapes to Loom's default -- the two a maker
    // asks about tomorrow -- and deliberately not `BuildOutput`, which is working
    // memory by the ton.
    Live live({cmake_recipe("chatty", "fixture-chatty6")});
    const std::string path = "zengine-rth1a-build.log";
    std::remove(path.c_str());

    loom::LoggerSelection selection = loom::default_selection();
    selection.shapes.push_back(loom::LogRule{std::string(BuildFinished::zen_name), 0});
    selection.shapes.push_back(loom::LogRule{std::string(BuildNotStarted::zen_name), 0});
    {
        loom::Logger journal(live.bus, std::move(selection));
        REQUIRE(journal.open(path));
        live.tell_tool(BuildRequested{"chatty"});
        live.carry_until_over();
        CHECK(journal.appended_of(BuildOutput::zen_name) == 0);
        CHECK(journal.appended_of(std::string(timer::TimerFired::zen_name)) == 0);
    }

    std::vector<loom::LogRecord> back;
    std::string error;
    REQUIRE(loom::Logger::read(path, &back, &error));
    std::size_t finished = 0;
    for (const loom::LogRecord& r : back) {
        CHECK(r.origin == loom::LogOrigin::BusObservation);
        // Nothing that was not selected reached the stream -- including every beat
        // that carried the build and every line it printed.
        CHECK(r.observation.shape != BuildOutput::zen_name);
        CHECK(r.observation.shape != timer::TimerFired::zen_name);
        finished += r.observation.shape == BuildFinished::zen_name ? 1u : 0u;
    }
    CHECK(finished == 1);
    std::remove(path.c_str());
}

// ============================================================================
// Tier 7 -- BLD-1: an AUTHORED RECIPE, the ARTIFACT it names, and the line
//                  between "the process was fine" and "the thing exists"
// ============================================================================

TEST_CASE("BLD-1 law: a recipe needs a name, an artifact and exactly one mechanism") {
    // THE NAME IS A NAME AND NOT A LOCATION. It is what a maker types, what a
    // message carries and what a refusal quotes back.
    CHECK(check_recipe_id("oven").empty());
    CHECK_FALSE(check_recipe_id("").empty());
    CHECK_FALSE(check_recipe_id("a b").empty());
    CHECK_FALSE(check_recipe_id("dir/oven").empty());
    CHECK_FALSE(check_recipe_id("dir\\oven").empty());
    CHECK_FALSE(check_recipe_id(std::string(kMaxRecipeIdLen + 1, 'x')).empty());

    // THE ARTIFACT IS A STEM, under the same five rules a load plan's stem meets --
    // restated in this package deliberately, because a rule enforced in one
    // execution-authority document and trusted in the other is a rule with a door
    // in it.
    CHECK(check_recipe_artifact("zengine-oven").empty());
    CHECK_FALSE(check_recipe_artifact("").empty());
    CHECK_FALSE(check_recipe_artifact("../oven").empty());
    CHECK_FALSE(check_recipe_artifact("lib/oven").empty());
    CHECK_FALSE(check_recipe_artifact("oven weave").empty());

    Recipe neither;
    neither.id = "empty";
    neither.artifact = "zengine-empty";
    CHECK(check_recipe(neither).find("names no build mechanism") != std::string::npos);

    Recipe both = neither;
    both.cmake_target = CMakeTargetRecipe{kFixtureTree, "fixture-quick", std::string()};
    both.single_source =
        SingleSourceRecipe{"/tmp/x.cpp", {}, {"loom::switchboard"}, std::string(), std::string()};
    CHECK(check_recipe(both).find("two build mechanisms") != std::string::npos);

    // A CMake recipe that names a tree and no target in it says nothing.
    Recipe headless = neither;
    headless.cmake_target = CMakeTargetRecipe{kFixtureTree, std::string(), std::string()};
    CHECK(check_recipe(headless).find("no target") != std::string::npos);

    // ...and a single-source recipe that links nothing would fail later and less
    // clearly, so it is refused here.
    Recipe unlinked = neither;
    unlinked.single_source =
        SingleSourceRecipe{"/tmp/x.cpp", {}, {}, std::string(), std::string()};
    CHECK(check_recipe(unlinked).find("links nothing") != std::string::npos);
}

TEST_CASE("BLD-1 law: a path may hold spaces, and a link target may not hold a flag") {
    // SPACES ARE LEGAL, and that is a decision about the platforms this repository
    // builds for rather than an oversight: a maker's checkout genuinely lives under
    // `C:/Users/Someone/My Weaves` on one of them, and every place a path is spent
    // here is one element of an argument vector or one quoted CMake string.
    CHECK(check_recipe_path("a source file", "/home/me/My Weaves/oven.cpp").empty());
    CHECK(check_recipe_path("a source file", "C:/Users/Someone/My Weaves/oven.cpp").empty());
    // A QUOTE AND A CONTROL CHARACTER ARE NOT. The first ends a string in a
    // generated file and the second ends a line in one, and neither is made safe by
    // escaping something a reader then has to trust.
    CHECK_FALSE(check_recipe_path("a source file", "/home/me/\"oven\".cpp").empty());
    CHECK_FALSE(check_recipe_path("a source file", "/home/me/oven\n.cpp").empty());
    CHECK_FALSE(check_recipe_path("a source file", "").empty());

    // A TARGET IS LINKED BY NAME. A flag, a path or a library file is not one, and
    // the refusal says so rather than passing it through to CMake.
    CHECK(check_link_target("zengine::timer").empty());
    CHECK(check_link_target("loom::switchboard").empty());
    CHECK(check_link_target("zengine::operator-consumer").empty());
    CHECK_FALSE(check_link_target("-lpthread").empty());
    CHECK_FALSE(check_link_target("/usr/lib/libfoo.so").empty());
    CHECK_FALSE(check_link_target("$(evil)").empty());
    CHECK_FALSE(check_link_target("a;b").empty());
}

TEST_CASE("BLD-1: two recipes coexist, and a name is a name") {
    const std::string twice = check_recipes(
        {cmake_recipe("one", "fixture-quick"), cmake_recipe("one", "fixture-slow2")});
    CHECK(twice.find("declared twice") != std::string::npos);

    // TWO RECIPES PRODUCING ONE ARTIFACT IS ORDINARY and is deliberately NOT
    // refused: the same weave built against two package prefixes is a thing a
    // project may want, and what asks for a build asks for a RECIPE.
    CHECK(check_recipes({cmake_recipe("debug", "fixture-quick", "zengine-oven"),
                         cmake_recipe("release", "fixture-slow2", "zengine-oven")})
              .empty());
}

TEST_CASE("BLD-1: selecting one recipe cannot build the other") {
    Live live({cmake_recipe("alpha", "fixture-quick"), cmake_recipe("beta", "fixture-neighbour")});
    live.tell_tool(BuildRequested{"beta"});
    live.carry_until_over();

    const BuildStatus& done = live.ears->last();
    CHECK(done.recipe == "beta");
    CHECK(done.artifact == "fixture-neighbour");
    CHECK(done.outcome == outcome::kSucceeded);
    // THE COMMAND NAMES THE ONE TARGET THAT WAS ASKED FOR, which is what makes
    // "the configured build action reaches the intended target" a measurement.
    CHECK(done.command.find("--target fixture-neighbour") != std::string::npos);
    CHECK(done.command.find("fixture-quick") == std::string::npos);
}

TEST_CASE("BLD-1: a process exiting zero is not an artifact") {
    // THE RECIPE NAMES A FILE ITS TARGET DOES NOT PRODUCE -- the ordinary maker
    // mistake, and the one a green build would otherwise hide completely.
    Live live({cmake_recipe("empty", "fixture-empty", "zengine-never-made")});
    live.tell_tool(BuildRequested{"empty"});
    live.carry_until_over();

    const BuildStatus& done = live.ears->last();
    CHECK(done.status == 0);                     // the build system was satisfied
    CHECK(done.outcome == outcome::kNoArtifact); // ...and the project is not
    CHECK(done.outcome != outcome::kSucceeded);
    CHECK(done.outcome != outcome::kFailed); // nothing FAILED, and saying so would send a
                                             // maker to read output that says everything
                                             // went fine
    CHECK(done.detail.find("zengine-never-made") != std::string::npos);
    CHECK_FALSE(artifact_produced(done.outcome));
    // AND NOTHING WAS OFFERED TO ANY PROJECT.
    CHECK(live.ears->built.empty());
}

TEST_CASE("BLD-1: a build succeeds only when the artifact its recipe names is there") {
    const std::string made = fixture_path("fixture-quick");
    std::remove(made.c_str());
    REQUIRE_FALSE(stamp_of(made).present);

    Live live({cmake_recipe("quick", "fixture-quick")});
    live.tell_tool(BuildRequested{"quick"});
    live.carry_until_over();

    const BuildStatus& done = live.ears->last();
    CHECK(done.outcome == outcome::kSucceeded);
    CHECK(artifact_produced(done.outcome));
    CHECK(stamp_of(made).present);
    // IT SAYS IT BUILT THE THING, by the stem the project would name it by.
    CHECK(done.detail.find("built fixture-quick") != std::string::npos);
}

TEST_CASE("BLD-1: an unrelated artifact beside the output satisfies nothing") {
    // A VALID-LOOKING NATIVE ARTIFACT, PRODUCED BY THE SAME BUILD, that the recipe
    // never named. There is no scan here and no newest-file heuristic: the recipe
    // says what it produces, and a neighbour is a neighbour.
    const std::string bystander = fixture_path("fixture-bystander");
    std::remove(bystander.c_str());

    Live live({cmake_recipe("neighbour", "fixture-neighbour", "zengine-not-the-neighbour")});
    live.tell_tool(BuildRequested{"neighbour"});
    live.carry_until_over();

    CHECK(stamp_of(bystander).present);                       // the neighbour really is there
    CHECK(live.ears->last().outcome == outcome::kNoArtifact); // ...and it counts for nothing
}

TEST_CASE("BLD-1: a stale artifact cannot make a failed build look successful") {
    // THE ORDER OF THE TWO CHECKS IS THE WHOLE GUARANTEE. The exit status is
    // consulted FIRST, so an artifact left at the destination by an earlier success
    // -- which is exactly what an incremental build tree looks like -- can never be
    // read as this operation's product.
    const std::string stale = fixture_path("fixture-broken");
    {
        std::ofstream leave(stale, std::ios::binary);
        leave << "a perfectly good artifact from a build that worked";
    }
    REQUIRE(stamp_of(stale).present);

    Live live({cmake_recipe("brk", "fixture-broken")});
    live.tell_tool(BuildRequested{"brk", /*realize=*/true});
    live.carry_until_over();

    const BuildStatus& done = live.ears->last();
    CHECK(done.outcome == outcome::kFailed);
    CHECK(done.status != 0);
    CHECK(stamp_of(stale).present); // the old file is still sitting there
    // ...AND NOTHING WAS OFFERED. A failed build never reaches the artifact
    // question at all, so a maker who asked for realization is told why rather
    // than left to wonder.
    CHECK(live.ears->built.empty());
    CHECK(done.realization == realization::kRefused);
    CHECK(done.realized_detail.find("build failed") != std::string::npos);
    std::remove(stale.c_str());
}

TEST_CASE("BLD-1: a build offers its artifact only when a maker asked it to") {
    // A PLAIN BUILD PRODUCES A FILE TOO, and says so -- and publishes no offer,
    // because an offer carries an INTENT that something be done with the result.
    Live plain({cmake_recipe("quick", "fixture-quick")});
    plain.tell_tool(BuildRequested{"quick", /*realize=*/false});
    plain.carry_until_over();
    CHECK(plain.ears->last().outcome == outcome::kSucceeded);
    CHECK(plain.ears->last().realization == realization::kNotAsked);
    CHECK(plain.ears->built.empty());

    Live asked({cmake_recipe("quick", "fixture-quick")});
    asked.tell_tool(BuildRequested{"quick", /*realize=*/true});
    asked.carry_until_over();
    const BuildStatus& done = asked.ears->last();
    CHECK(done.outcome == outcome::kSucceeded);
    CHECK(done.realize);
    CHECK(done.realization == realization::kOffered);
    REQUIRE(asked.ears->built.size() == 1);
    CHECK(asked.ears->built[0].recipe == "quick");
    CHECK(asked.ears->built[0].artifact == "fixture-quick");
    CHECK(asked.ears->built[0].op == done.op);
    // THE PATH TRAVELS SO NOTHING DOWNSTREAM HAS TO SPELL A STEM -- and it is the
    // file the tool actually looked at.
    CHECK(asked.ears->built[0].path == fixture_path("fixture-quick"));
    CHECK(asked.tool->known().offered == 1);
}

TEST_CASE("BLD-1: the project's answer is a SECOND outcome and never overwrites the first") {
    Live live({cmake_recipe("quick", "fixture-quick")});
    live.tell_tool(BuildRequested{"quick", /*realize=*/true});
    live.carry_until_over();
    REQUIRE(live.ears->last().outcome == outcome::kSucceeded);
    REQUIRE(live.ears->last().realization == realization::kOffered);

    // THE PROJECT REFUSES, IN ITS OWN WORDS. The build is untouched: it succeeded,
    // it still says so, and the two facts sit on two fields.
    live.tell_tool(
        ArtifactRealized{"fixture-quick", false, "already part of this running project"});
    const BuildStatus& after = live.ears->last();
    CHECK(after.outcome == outcome::kSucceeded);
    CHECK(after.status == 0);
    CHECK(after.realization == realization::kRefused);
    CHECK(after.realized_detail.find("already part of") != std::string::npos);

    // AN ANSWER ABOUT SOMEBODY ELSE'S ARTIFACT IS SOMEBODY ELSE'S CONVERSATION.
    const std::int64_t stray = live.tool->known().stray;
    live.tell_tool(ArtifactRealized{"some-other-artifact", true, "loaded"});
    CHECK(live.tool->known().stray == stray + 1);
    CHECK(live.tool->known().realization == realization::kRefused);
}

TEST_CASE("BLD-1 CANARY: break the recipe-to-artifact mapping and success stops being one") {
    // MUTATION A, IN COMMITTED FORM. `outcome::kSucceeded` is `exit 0 AND the file
    // the recipe names is there`; a recipe whose artifact no longer describes what
    // its target produces therefore cannot reach it, however green the process was.
    // If this ever passes as `kSucceeded`, the artifact half of the test has stopped
    // being applied and every artifact claim in this suite is void.
    Recipe broken = cmake_recipe("quick", "fixture-quick");
    broken.artifact = "zengine-a-name-nothing-produces";
    Live live({broken});
    live.tell_tool(BuildRequested{"quick", /*realize=*/true});
    live.carry_until_over();

    CHECK(live.ears->last().status == 0);
    CHECK(live.ears->last().outcome == outcome::kNoArtifact);
    CHECK(live.ears->built.empty());
}

// ---- The generated project: what a single-source recipe becomes ------------------

namespace {

/// One single-source recipe, fully resolved the way a host resolves one.
Recipe one_source(const std::string& workspace, const std::string& source) {
    Recipe r;
    r.id = "oven";
    r.artifact = "zengine-oven";
    r.artifact_dir = std::string(kFixtureArtifacts) + "/landing";
    r.single_source = SingleSourceRecipe{source,
                                         {"/opt/zengine", "/opt/loom"},
                                         {"zengine::timer", "loom::switchboard"},
                                         std::string(kFixtureTree),
                                         workspace};
    return r;
}

} // namespace

TEST_CASE("BLD-1: the generated project consumes the PACKAGE and nothing private") {
    const std::string text = generated_project(one_source("/tmp/ws", "/tmp/oven.cpp"));

    // THE WHOLE PURITY CLAIM, IN ONE LINE OF THE GENERATED FILE.
    CHECK(text.find("find_package(zengine 0.1 CONFIG REQUIRED)") != std::string::npos);
    CHECK(text.find("loom_weave_build_contract(zengine-oven)") != std::string::npos);
    CHECK(text.find("add_library(zengine-oven SHARED \"/tmp/oven.cpp\")") != std::string::npos);

    // ...AND WHAT IS NOT IN IT IS THE OTHER HALF OF THE SAME CLAIM. No include
    // directory into a source tree, no build-tree path, no named library file, no
    // compiler, no flag, no link line.
    //
    // WITH THE PROSE STRIPPED, which is this repository's own discipline for a
    // source tripwire (`test_operator_provider.cpp` reads `load_execute.hpp` the
    // same way). The generated file EXPLAINS what it refuses to contain -- it says
    // in a comment that there is no named `.so` or `.dll` in it -- and a scan that
    // could not tell a comment from a command would fail on the sentence saying the
    // thing is absent.
    std::string code;
    for (std::size_t at = 0; at < text.size();) {
        const std::size_t end = text.find('\n', at);
        const std::string line = text.substr(at, end == std::string::npos ? end : end - at);
        if (line.find_first_not_of(" \t") != std::string::npos &&
            line[line.find_first_not_of(" \t")] != '#') {
            code += line;
            code += '\n';
        }
        if (end == std::string::npos) {
            break;
        }
        at = end + 1;
    }
    for (const char* forbidden : {"include_directories", "target_compile_options", "-I", "-l",
                                  "g++", "clang++", "cl.exe", ".so", ".dll", "CMAKE_CXX_FLAGS"}) {
        CHECK(code.find(forbidden) == std::string::npos);
    }

    // ONLY WHAT WAS AUTHORED IS LINKED.
    CHECK(text.find("zengine::timer") != std::string::npos);
    CHECK(text.find("loom::switchboard") != std::string::npos);
    CHECK(text.find("zengine::surface") == std::string::npos);

    // CMAKE OWNS WHERE IT LANDS, INCLUDING THE PER-CONFIG VARIANTS a multi-config
    // generator would otherwise append a directory to.
    CHECK(text.find("LIBRARY_OUTPUT_DIRECTORY") != std::string::npos);
    CHECK(text.find("RUNTIME_OUTPUT_DIRECTORY") != std::string::npos);
    CHECK(text.find("ARCHIVE_OUTPUT_DIRECTORY") != std::string::npos);
    CHECK(text.find("CMAKE_CONFIGURATION_TYPES") != std::string::npos);
    CHECK(text.find("PREFIX \"\"") != std::string::npos);
}

TEST_CASE("BLD-1: the generated driver borrows a toolchain and tells two failures apart") {
    const std::string text = generated_driver(one_source("/tmp/ws", "/tmp/oven.cpp"));

    // THE TOOLCHAIN IS BORROWED WITH CMAKE'S OWN MECHANISM. No cache parser was
    // written in C++, and the policy is legible in a file a maker can open.
    CHECK(text.find("load_cache(") != std::string::npos);
    CHECK(text.find("CMAKE_GENERATOR") != std::string::npos);
    CHECK(text.find("CMAKE_CXX_COMPILER") != std::string::npos);
    CHECK(text.find("CMakeCache.txt") != std::string::npos);

    // TWO FAILURES, TWO SENTENCES. A configure that failed and a compile that
    // failed are different problems needing different next actions, and this script
    // is the only party that sees both exit codes.
    CHECK(text.find("CMake configure FAILED") != std::string::npos);
    CHECK(text.find("compile or link FAILED") != std::string::npos);
    // ...and both name the generated project, because that is where a maker has to
    // go to read why.
    CHECK(text.find("was not deleted") != std::string::npos);

    // THE PACKAGE PREFIXES ARRIVE AS ONE `-D`, with the list separator escaped so
    // it survives being written into a file that is itself CMake.
    CHECK(text.find("-DCMAKE_PREFIX_PATH=/opt/zengine\\;/opt/loom") != std::string::npos);

    // NO TOOLCHAIN AUTHORED IS A REAL ANSWER and is said to be a default.
    Recipe bare = one_source("/tmp/ws", "/tmp/oven.cpp");
    bare.single_source->toolchain_from.clear();
    const std::string plain = generated_driver(bare);
    CHECK(plain.find("load_cache(") == std::string::npos);
    CHECK(plain.find("NO TOOLCHAIN WAS BORROWED") != std::string::npos);
}

TEST_CASE("BLD-1: a path with spaces survives generation, and a dollar stays a dollar") {
    const Recipe spaced = one_source("/home/me/My Builds/oven", "/home/me/My Weaves/oven.cpp");
    const std::string project = generated_project(spaced);
    const std::string driver = generated_driver(spaced);

    CHECK(project.find("\"/home/me/My Weaves/oven.cpp\"") != std::string::npos);
    CHECK(driver.find("\"/home/me/My Builds/oven\"") != std::string::npos);

    // A WINDOWS PATH IS A PATH AND NOT A STRING FULL OF ESCAPES.
    CHECK(cmake_quoted("C:\\Users\\Someone\\oven.cpp") == "C:\\\\Users\\\\Someone\\\\oven.cpp");
    // ...AND A DOLLAR SIGN IS NOT A VARIABLE REFERENCE.
    CHECK(cmake_quoted("/tmp/$HOME/oven.cpp") == "/tmp/\\$HOME/oven.cpp");
}

TEST_CASE("BLD-1: a recipe becomes ONE process, and the two kinds become two commands") {
    // AN EXISTING CMAKE TARGET IS THE COMMAND A MAKER WOULD TYPE.
    const PreparedBuild target = prepare(cmake_recipe("quick", "fixture-quick"), kCMake);
    REQUIRE(target.ok);
    CHECK(target.command.program == std::string(kCMake));
    REQUIRE(target.command.args.size() == 4);
    CHECK(target.command.args[0] == "--build");
    CHECK(target.command.args[1] == std::string(kFixtureTree));
    CHECK(target.command.args[2] == "--target");
    CHECK(target.command.args[3] == "fixture-quick");

    // A MULTI-CONFIG GENERATOR'S CONFIGURATION RIDES ALONG when a recipe authors
    // one, and single-config generators accept and ignore it -- which is what makes
    // one recipe legal against either kind of tree.
    Recipe configured = cmake_recipe("quick", "fixture-quick");
    configured.cmake_target->config = "Debug";
    const PreparedBuild with_config = prepare(configured, kCMake);
    REQUIRE(with_config.ok);
    REQUIRE(with_config.command.args.size() == 6);
    CHECK(with_config.command.args[4] == "--config");
    CHECK(with_config.command.args[5] == "Debug");

    // A BUILD TREE NOBODY CONFIGURED IS REFUSED BY NAME, before a child exists.
    Recipe nowhere = cmake_recipe("quick", "fixture-quick");
    nowhere.cmake_target->build_dir = std::string(kFixtureTree) + "-absent";
    const PreparedBuild missing = prepare(nowhere, kCMake);
    CHECK_FALSE(missing.ok);
    CHECK(missing.trouble.find("CMakeCache.txt") != std::string::npos);

    // A SOURCE FILE THAT IS NOT THERE, likewise -- and NOTHING is generated for it.
    const std::string workspace = std::string(kFixtureArtifacts) + "/ws-absent";
    const PreparedBuild gone =
        prepare(one_source(workspace, "/tmp/no-such-oven-source.cpp"), kCMake);
    CHECK_FALSE(gone.ok);
    CHECK(gone.trouble.find("is not there") != std::string::npos);
    CHECK_FALSE(stamp_of(workspace + "/CMakeLists.txt").present);
}

TEST_CASE("BLD-1: a single-source recipe writes its project, and is one `cmake -P`") {
    // A REAL SOURCE FILE, so `prepare` gets past its one diagnostic preflight. It is
    // never compiled here -- what this case is about is the two files Zengine writes
    // and the single command it produces; a real compile against a real installed
    // package is `tests/build/run.cmake`, which is a lane and not a ctest entry.
    const std::string workspace = std::string(kFixtureArtifacts) + "/ws";
    const std::string source = std::string(kFixtureArtifacts) + "/oven.cpp";
    std::filesystem::create_directories(std::filesystem::path(kFixtureArtifacts));
    {
        std::ofstream write(source, std::ios::binary);
        write << "// not compiled by this case\n";
    }

    const PreparedBuild built = prepare(one_source(workspace, source), kCMake);
    REQUIRE(built.ok);
    CHECK(built.command.program == std::string(kCMake));
    REQUIRE(built.command.args.size() == 2);
    CHECK(built.command.args[0] == "-P");
    CHECK(built.command.args[1] == workspace + "/" + kGeneratedDriverFile);
    CHECK(built.command.dir == workspace);

    // BOTH FILES ARE ON DISK AND STAY THERE. A generated project that deleted
    // itself would take the diagnostics with it exactly when a maker needs them.
    CHECK(stamp_of(workspace + "/" + kGeneratedProjectFile).present);
    CHECK(stamp_of(workspace + "/" + kGeneratedDriverFile).present);

    // WRITING IT AGAIN WITH THE SAME RECIPE DOES NOT TOUCH IT, which is what keeps
    // the second build of an unchanged recipe incremental rather than a reconfigure.
    const ArtifactStamp first = stamp_of(workspace + "/" + kGeneratedProjectFile);
    CHECK(materialize(one_source(workspace, source)).empty());
    CHECK(stamp_of(workspace + "/" + kGeneratedProjectFile) == first);

    // ...AND A CHANGED RECIPE DOES.
    Recipe moved = one_source(workspace, source);
    moved.single_source->links.push_back("zengine::surface");
    CHECK(materialize(moved).empty());
    CHECK(stamp_of(workspace + "/" + kGeneratedProjectFile).size != first.size);
}

// ============================================================================
// Tier 9 -- custody: the catalog belongs to its owner, and both halves READ it
// ============================================================================

TEST_CASE("PROJ-0: neither build participant keeps a catalog of its own") {
    // ⭐ THE FALSIFIER FOR THE WHOLE CUSTODY CLAIM, and the only shape that can carry
    // it. Both weaves used to be HANDED the completed catalog -- the runner a whole
    // copy, the tool a copy of its reduced view -- and with nothing in the process ever
    // replacing either, a copy and a read answer identically forever. So this case does
    // the one thing that tells them apart: it changes the catalog in the ONE place that
    // owns it and asks both weaves what they now answer.
    //
    // ⚠ IT ADDS NO GESTURE AND NO POLICY. Nothing here says what a maker does to change
    // a catalog, what happens to a chosen row, or what an in-flight build makes of it;
    // those belong to the phase that adds a real catalog-change gesture. What is
    // measured is custody alone.
    Live live({cmake_recipe("greet", "fixture-quick")});

    // FIRST: THE OBJECTS THEMSELVES. Not "the same contents" -- the SAME OBJECT. A
    // participant holding a vector of its own would fail here before any behaviour was
    // consulted, which is why this line comes first.
    CHECK(&live.runner->catalog() == &live.catalog);
    CHECK(&live.tool->recipes() == &live.views);

    // ...AND NEITHER KNOWS A NAME THE CATALOG DOES NOT HOLD. The tool refuses by name
    // and counts what it holds; the runner refuses the same name in its own words.
    live.tell_tool(BuildRequested{"late"});
    CHECK(live.tool->known().outcome == outcome::kUnknownRecipe);
    CHECK(live.tool->known().detail.find("(it holds 1)") != std::string::npos);
    live.tell_runner(RunBuild{"late"});
    CHECK(live.runner->refused() == 1);
    CHECK(live.runner->ran() == 0);

    // ---- ONE PLACE CHANGES, AND IT IS NOT EITHER WEAVE ----------------------------
    //
    // The rig owns the catalog and the views the way the host owns its `CurrentRecipes`,
    // so this is the whole of what a replacement touches. The recipe names a build tree
    // that was never configured, deliberately: what is being measured is that the two
    // weaves can FIND it, and a recipe that could start a compiler would measure that
    // and a build.
    Recipe late = cmake_recipe("late", "anything", "zengine-fixture-late");
    late.cmake_target->build_dir = std::string(kFixtureTree) + "-that-does-not-exist";
    live.catalog.push_back(late);
    live.views.push_back(view_of(late));

    // THE TOOL ANSWERS THE NEW CATALOG. Its published picture is derived at the moment
    // it is asked, from the views it reads rather than from a list it kept.
    live.ears->catalogs.clear();
    live.tell_tool(StatusRequested{});
    REQUIRE(live.ears->catalogs.size() == 1);
    REQUIRE(live.ears->catalogs[0].recipes.size() == 2);
    CHECK(live.ears->catalogs[0].recipes[1].recipe == "late");
    CHECK(live.ears->catalogs[0].recipes[1].artifact == "zengine-fixture-late");

    // ...AND SO DOES THE RUNNER, WHICH IS THE HALF A PUBLISHED PICTURE CANNOT PROVE.
    // The ask goes the whole way now: the tool no longer refuses the name, the order
    // reaches the runner, and the refusal that comes back is the PREFLIGHT's -- a build
    // tree with no `CMakeCache.txt` -- which only a runner that found the recipe can
    // say. Nothing started, so nothing has to be waited for.
    live.tell_tool(BuildRequested{"late"});
    live.carry_until_over();
    const BuildStatus& done = live.ears->last();
    CHECK(done.recipe == "late");
    CHECK(done.outcome == outcome::kNotStarted);
    CHECK(done.detail.find("CMakeCache.txt") != std::string::npos);
    CHECK(live.runner->refused() == 2);
    CHECK(live.runner->ran() == 0);
}

TEST_CASE("PROJ-0: a build participant cannot be composed over a temporary catalog") {
    // THE LIFETIME WALL, ASSERTED AT COMPILE TIME because that is the only moment it can
    // be asserted at: a weave bound to a temporary catalog is a use-after-free whose
    // first symptom is a build of nothing in particular, and no runtime case can be
    // written that survives to check it. Both participants take a reference to somebody
    // else's catalog and both refuse an rvalue outright.
    static_assert(std::is_constructible_v<BuildRunnerWeave, const std::vector<Recipe>&,
                                          std::string>,
                  "the runner must read a catalog it does not own");
    static_assert(!std::is_constructible_v<BuildRunnerWeave, std::vector<Recipe>&&, std::string>,
                  "a runner over a temporary catalog is a dangling runner");
    static_assert(std::is_constructible_v<BuilderWeave, const std::vector<RecipeView>&>,
                  "the tool must read views it does not own");
    static_assert(!std::is_constructible_v<BuilderWeave, std::vector<RecipeView>&&>,
                  "a tool over temporary views is a dangling tool");
    // doctest wants a runtime assertion in every case, and this is the honest one: the
    // four claims above are already decided by the time this line runs.
    CHECK(true);
}

// ---- The scheduler audit, as a tripwire -----------------------------------------

namespace {

/// One source file, with its prose stripped.
///
/// THE PROSE GOES FIRST, because these files EXPLAIN what they refuse to do -- this
/// header says out loud that nothing here waits, sleeps or runs on another thread --
/// and a check that could not tell a sentence from a statement would forbid the
/// explanation. BOOT-0's tripwire over `load_execute.hpp` reads its subject exactly
/// this way, and this is that discipline applied to the two participants a build
/// conversation passes through.
std::string code_of(const char* path) {
    std::ifstream in(path);
    REQUIRE_MESSAGE(in.good(), "cannot read ", std::string(path));
    std::ostringstream all;
    all << in.rdbuf();
    const std::string text = all.str();
    std::string code;
    code.reserve(text.size());
    for (std::size_t i = 0; i < text.size();) {
        if (text.compare(i, 2, "//") == 0) {
            while (i < text.size() && text[i] != '\n') {
                ++i;
            }
            continue;
        }
        code.push_back(text[i]);
        ++i;
    }
    return code;
}

} // namespace

TEST_CASE("BLD-1: no semantic build consumer drives dispatch waiting for its own result") {
    // ⭐ THE SCHEDULER AUDIT, MECHANICALLY. The behavioural half is above -- a real
    // child runs while unrelated deliveries are carried, measured against the
    // blocking shape rebuilt beside it as a control. This is the tripwire, and it is
    // here because BLD-1 gave the TOOL two new reasons to want a loop: it now waits
    // for an artifact to appear, and it now waits for a project to answer. Neither is
    // a wait. Both are facts that arrive.
    const std::string tool = code_of(ZENGINE_TEST_TOOL_SOURCE);
    const std::string runner = code_of(ZENGINE_TEST_RUNNER_SOURCE);

    for (const char* verb : {"pump_pending", "drain_until_idle", "dispatch_at_most", "run_until",
                             "wait_until", "advance_until", "pump_until", "std::this_thread",
                             "sleep_for"}) {
        CHECK_MESSAGE(tool.find(verb) == std::string::npos, "builder/weave.hpp calls '", verb,
                      "', which makes the Builder tool its own scheduler");
        CHECK_MESSAGE(runner.find(verb) == std::string::npos, "builder/runner.hpp calls '", verb,
                      "', which makes the build runner its own scheduler");
    }
    for (const char* noun : {"std::future", "std::promise", "std::async", "std::thread",
                             "class Scheduler", "struct Scheduler", "co_await", "co_return"}) {
        CHECK_MESSAGE(tool.find(noun) == std::string::npos, "builder/weave.hpp declares '", noun,
                      "', which is a scheduler wearing another noun");
        CHECK_MESSAGE(runner.find(noun) == std::string::npos, "builder/runner.hpp declares '",
                      noun, "', which is a scheduler wearing another noun");
    }

    // ⚠ `look()` IS THE ONE POLL IN THIS PACKAGE AND IT LIVES WITH THE OS HANDLE.
    // The runner polls its own children because that is what an OS handle offers;
    // the TOOL must never learn the word, because a tool that could ask "is it done
    // yet?" is a tool that would eventually loop until it was.
    CHECK(runner.find(".look()") != std::string::npos);
    CHECK(tool.find("look()") == std::string::npos);

    // ...AND THE TOOL HOLDS NO PROCESS, NO HANDLE AND NO COMMAND. The three nouns
    // that would mean the split had quietly collapsed.
    for (const char* forbidden : {"RunningRecipe", "start_recipe", "builder/run.hpp",
                                  "builder/generate.hpp", "BuildCommand"}) {
        CHECK_MESSAGE(tool.find(forbidden) == std::string::npos, "builder/weave.hpp names '",
                      forbidden, "', which is process authority in the presentation's half");
    }

    // ...AND IT HOLDS NO REALIZATION AUTHORITY EITHER (BLD-1a). The seam runs the other
    // way too, and the rename is what made the asymmetry legible: this tool OFFERS an
    // artifact and the realization owner decides. A Builder that reached the Kernel, the
    // Weave Manager, the catalog or the plan executor directly would not be making an
    // offer -- it would be performing a load, and every eligibility rule the authored
    // plan owns would have been routed around rather than enforced.
    for (const char* forbidden : {"zen/kernel", "Kernel", "LoadWeave", "PlanExecutor",
                                  "mount_provider", "op::Catalog"}) {
        CHECK_MESSAGE(tool.find(forbidden) == std::string::npos, "builder/weave.hpp names '",
                      forbidden, "', which is realization authority in the BUILD half");
    }
}

TEST_CASE("PROJ-0: the two build participants declare no catalog storage of their own") {
    // DEFENCE IN DEPTH, AND SAID TO BE. The case above drives the real seam and would
    // go red the moment either weave kept a copy -- but it can only say that while a
    // fixture is there to change the catalog underneath it. This says the same thing
    // about the SOURCE, so a helpful future edit that puts a storing member back is
    // refused at the declaration rather than at the one case that happened to notice.
    //
    // ⚠ THE FORBIDDEN FORMS ARE DECLARATIONS, never bare words, and the prose is
    // stripped first for the reason `code_of` gives: both files EXPLAIN that they store
    // no catalog, and a check that could not tell a sentence from a member would report
    // the explanation as the defect.
    const std::string tool = code_of(ZENGINE_TEST_TOOL_SOURCE);
    const std::string runner = code_of(ZENGINE_TEST_RUNNER_SOURCE);

    // WHAT MUST BE THERE: a reference to somebody else's vector, in each file.
    CHECK(runner.find("const std::vector<Recipe>& catalog_;") != std::string::npos);
    CHECK(tool.find("const std::vector<RecipeView>& recipes_;") != std::string::npos);

    // WHAT MUST NOT: a vector of its own, under any of the spellings a by-value member
    // or a by-value constructor parameter would take.
    for (const char* forbidden : {"std::vector<Recipe> catalog_", "std::vector<Recipe> catalog)",
                                  "std::vector<Recipe> catalog,"}) {
        CHECK_MESSAGE(runner.find(forbidden) == std::string::npos, "builder/runner.hpp declares '",
                      forbidden, "', which is a second session-long catalog");
    }
    for (const char* forbidden : {"std::vector<RecipeView> recipes_",
                                  "std::vector<RecipeView> recipes)",
                                  "std::vector<RecipeView> recipes,"}) {
        CHECK_MESSAGE(tool.find(forbidden) == std::string::npos, "builder/weave.hpp declares '",
                      forbidden, "', which is a second session-long catalog of views");
    }

    // ...AND THE RVALUE DOOR IS SHUT IN BOTH, which is the compile-time half of the
    // lifetime claim the case two above asserts.
    CHECK(runner.find("BuildRunnerWeave(std::vector<Recipe>&&, std::string) = delete;") !=
          std::string::npos);
    CHECK(tool.find("BuilderWeave(std::vector<RecipeView>&&) = delete;") != std::string::npos);
}
