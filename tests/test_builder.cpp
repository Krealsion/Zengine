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

#include <zen/schema.hpp>
#include <zen/switchboard.hpp>
#include <zen/weave.hpp>

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
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

/// A recipe that takes a while and says several things while it does.
///
/// THE DURATION IS THE POINT. "It did not block" measured against a child that
/// exits in a millisecond is a race dressed as a property; measured against a
/// child that is provably still alive several hundred deliveries later, it is a
/// measurement.
BuildRecipe slow(const std::string& target, int steps, const char* pause, bool fail = false) {
    BuildRecipe r;
    r.target = target;
    r.program = kCMake;
    r.args = {std::string("-DSTEPS=") + std::to_string(steps), std::string("-DPAUSE=") + pause};
    if (fail) {
        r.args.push_back("-DFAIL=1");
    }
    r.args.push_back("-P");
    r.args.push_back(kSlowScript);
    return r;
}

/// One recipe that always succeeds at once, named whatever the case wants.
BuildRecipe echoes(const std::string& target, const std::string& what) {
    return BuildRecipe{target, kCMake, {"-E", "echo", what}, std::string()};
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

class Listener : public loom::WeaveBase<Listener, HeardState, loom::Accept<BuildStatus>,
                                        loom::Emit<>> {
public:
    void on(const BuildStatus& s, loom::Mail&) {
        ++state_.heard;
        said.push_back(s);
    }
    const BuildStatus& last() const { return said.back(); }
    std::vector<BuildStatus> said;
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
    explicit BlockingRunnerWeave(BuildRecipe recipe) : recipe_(std::move(recipe)) {}

    void on(const RunBuild& order, loom::Mail& mail) {
        if (order.target != recipe_.target) {
            (void)mail.send_to_role(kBuilderRole,
                                    BuildNotStarted{0, order.target, std::string(), "no recipe"});
            return;
        }
        ++state_.ran;
        (void)mail.send_to_role(kBuilderRole, BuildStarted{1, recipe_.target, recipe_.as_line()});
        const RunResult run = run_recipe(recipe_); // <- the whole build, inside this handler
        if (!run.started) {
            (void)mail.send_to_role(
                kBuilderRole, BuildNotStarted{1, recipe_.target, recipe_.as_line(), run.trouble});
            return;
        }
        (void)mail.send_to_role(kBuilderRole, BuildFinished{1, recipe_.target, run.status});
    }

private:
    BuildRecipe recipe_;
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

loom::Grant tool_grant() {
    loom::Grant g;
    g.allow_to_role(RunBuild::zen_name, RunBuild::zen_version, kBuildRunnerRole);
    g.allow_to_any(BuildStatus::zen_name, BuildStatus::zen_version);
    return g;
}

/// The runner, watched directly by whoever holds the Builder office.
struct Bench {
    loom::Switchboard bus;
    BuildRunnerWeave* runner = nullptr;
    Foreman* foreman = nullptr;
    TimerClerk* clerk = nullptr;
    loom::WeaveId runner_id{};

    explicit Bench(std::vector<BuildRecipe> catalog) {
        runner_id = mount_office<BuildRunnerWeave>(bus, runner_grant(), kBuildRunnerRole, &runner,
                                                   std::move(catalog));
        mount_office<Foreman>(bus, loom::Grant{}, kBuilderRole, &foreman);
        mount_office<TimerClerk>(bus, loom::Grant{}, timer::kTimerRole, &clerk);
    }

    /// Order a build the way the tool does.
    void order(const std::string& target) {
        (void)bus.send_to_role(kBuildRunnerRole, loom::Message(loom::to_value(RunBuild{target})));
        bus.pump();
    }

    /// One beat, exactly as the Timer would deliver it.
    void beat() {
        (void)bus.send_to_role(
            kBuildRunnerRole,
            loom::Message(loom::to_value(timer::TimerFired{std::string(kLookTimerId)})));
        bus.pump();
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
    loom::Switchboard bus;
    BuilderWeave* tool = nullptr;
    BuildRunnerWeave* runner = nullptr;
    Listener* ears = nullptr;
    Bystander* bystander = nullptr;
    TimerClerk* clerk = nullptr;
    loom::WeaveId tool_id{};
    loom::WeaveId runner_id{};
    loom::WeaveId bystander_id{};

    explicit Live(std::string target, std::vector<BuildRecipe> catalog) {
        runner_id = mount_office<BuildRunnerWeave>(bus, runner_grant(), kBuildRunnerRole, &runner,
                                                   std::move(catalog));
        tool_id = mount_office<BuilderWeave>(bus, tool_grant(), kBuilderRole, &tool,
                                             std::move(target));
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
        bus.pump();
    }
    template <class T>
    void tell_runner(const T& msg) {
        (void)bus.send_to_role(kBuildRunnerRole, loom::Message(loom::to_value(msg)));
        bus.pump();
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
            bus.pump();
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
    const auto requested = SchemaBuilder("BuildRequested", 1).field("target", Kind::Text).build();
    CHECK(schema_of<BuildRequested>()->content_id() == requested->content_id());

    const auto order = SchemaBuilder("RunBuild", 1).field("target", Kind::Text).build();
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
    const auto started = SchemaBuilder("BuildStarted", 1)
                             .field("op", Kind::Int)
                             .field("target", Kind::Text)
                             .field("recipe", Kind::Text)
                             .build();
    CHECK(schema_of<BuildStarted>()->content_id() == started->content_id());

    const auto output = SchemaBuilder("BuildOutput", 1)
                            .field("op", Kind::Int)
                            .field("target", Kind::Text)
                            .field("text", Kind::Text)
                            .field("dropped", Kind::Int)
                            .build();
    CHECK(schema_of<BuildOutput>()->content_id() == output->content_id());

    const auto finished = SchemaBuilder("BuildFinished", 1)
                              .field("op", Kind::Int)
                              .field("target", Kind::Text)
                              .field("status", Kind::Int)
                              .build();
    CHECK(schema_of<BuildFinished>()->content_id() == finished->content_id());

    // AND THE FOURTH, WHICH IS THE ONE THAT IS NOT AN ENDING OF A BUILD. "There
    // is no compiler" and "the compiler said no" are different problems needing
    // different next actions, and BLD-0's `started` flag became this shape when
    // the two stopped arriving together.
    const auto never = SchemaBuilder("BuildNotStarted", 1)
                           .field("op", Kind::Int)
                           .field("target", Kind::Text)
                           .field("recipe", Kind::Text)
                           .field("trouble", Kind::Text)
                           .build();
    CHECK(schema_of<BuildNotStarted>()->content_id() == never->content_id());
}

// ============================================================================
// Tier 2 -- the held process, with no bus in sight
// ============================================================================

TEST_CASE("a started recipe is HELD: start returns, and the child is still alive") {
    RecipeStart begun = start_recipe(slow("slow", 4, "0.15"));
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
    RecipeStart begun = start_recipe(slow("slow", 3, "0.1"));
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
    RecipeStart begun = start_recipe(echoes("quick", "hello"));
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
    RecipeStart failing = start_recipe(slow("brk", 1, "0.05", /*fail=*/true));
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
        start_recipe(BuildRecipe{"gone", "zengine-no-such-program-async1", {"--version"},
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
    const RecipeStart empty = start_recipe(BuildRecipe{"no", std::string(), {}, std::string()});
    CHECK_FALSE(empty.started);
    CHECK(empty.trouble.find("no program") != std::string::npos);
}

TEST_CASE("abandoning custody does not wait for the build, and leaves nothing behind") {
    // A BUILD LONG ENOUGH THAT WAITING FOR IT WOULD BE VISIBLE. If `abandon()`
    // waited for the child on its own terms this would take ten seconds; it
    // terminates first and then reaps, so it takes milliseconds. The margin is
    // deliberately enormous, because a timing assertion with a tight one is a
    // flake waiting to happen.
    RecipeStart begun = start_recipe(slow("forever", 20, "0.5"));
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
    Bench bench({slow("slow", 5, "0.15")});
    bench.order("slow");

    // The pump that delivered `RunBuild` has drained. If the runner had built
    // inside its handler, this operation would already be finished.
    CHECK(bench.runner->live() == 1);
    CHECK(bench.runner->ran() == 1);
    REQUIRE(bench.foreman->started.size() == 1);
    CHECK(bench.foreman->started[0].op == 1);
    CHECK(bench.foreman->started[0].target == "slow");
    // WHAT IS RUNNING IS SAYABLE WHILE IT RUNS. BLD-0 could only ever describe a
    // command after it had finished, because that was the first moment anything
    // could speak.
    CHECK(bench.foreman->started[0].recipe.find("slow_build.cmake") != std::string::npos);
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
    Bench bench({slow("alpha", 4, "0.15"), slow("beta", 4, "0.15")});
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

    // EVERY OUTPUT FACT NAMES THE OPERATION IT BELONGS TO, and the two streams
    // did not mix: `alpha`'s output is about alpha and nothing else.
    for (const BuildOutput& o : bench.foreman->output) {
        CHECK((o.op == alpha || o.op == beta));
        CHECK(o.target == (o.op == alpha ? "alpha" : "beta"));
    }
    CHECK(bench.foreman->text_of(alpha).find("step 1 of 4") != std::string::npos);
    CHECK(bench.foreman->text_of(beta).find("step 1 of 4") != std::string::npos);
    CHECK(bench.foreman->finished[0].op != bench.foreman->finished[1].op);
}

TEST_CASE("an ending is published once, and looking again publishes nothing") {
    Bench bench({echoes("quick", "hello from ASYNC-1")});
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
    Bench bench({echoes("quick", "hi")});

    // The service arrives. The declared binding is reconciled -- one ask.
    (void)bench.bus.send_to_role(kBuildRunnerRole,
                                 loom::Message(loom::to_value(timer::TimerReady{})));
    bench.bus.pump();
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
    Bench bench({slow("slow", 2, "0.1")});
    bench.order("slow");
    REQUIRE(bench.runner->live() == 1);

    const std::int64_t looks_before = bench.runner->looks();
    int guard = 2000000;
    while (guard-- > 0 && bench.runner->live() > 0) {
        (void)bench.bus.send_to_role(kBuildRunnerRole,
                                     loom::Message(loom::to_value(LookAtBuilds{})));
        bench.bus.pump();
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
    bench.bus.pump();
    CHECK(bench.runner->ran() == ran);
    CHECK(bench.foreman->started.size() == 1);
}

TEST_CASE("a name the runner holds no recipe for names no operation at all") {
    Bench bench({echoes("quick", "hi")});
    bench.order("not-in-the-catalog");

    CHECK(bench.runner->ran() == 0);
    CHECK(bench.runner->refused() == 1);
    CHECK(bench.runner->live() == 0);
    REQUIRE(bench.foreman->never.size() == 1);
    // `op` IS 0 AND THE ABSENCE IS THE STATEMENT: nothing was ever held, so
    // there is nothing to name, and minting an identity for a thing that never
    // existed would be an identity that could never be referred to again.
    CHECK(bench.foreman->never[0].op == 0);
    CHECK(bench.foreman->never[0].recipe.empty());
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
    Live live("slow", {slow("slow", 5, "0.15")});
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
    // ...and the build was watched arriving, not merely found finished.
    CHECK(live.tool->known().chunks > 1);
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
                                      slow("slow", 5, "0.15"));

    Bystander* bystander = nullptr;
    const loom::WeaveId bystander_id = mount_plain<Bystander>(bus, loom::Grant{}, &bystander);

    (void)bus.send_to_role(kBuildRunnerRole, loom::Message(loom::to_value(RunBuild{"slow"})));
    bus.pump();

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
    bus.pump();
    CHECK(bystander->seen() == 1);
}

// ============================================================================
// Tier 5 -- the tool: a name, a memory, a running build, and a refusal
// ============================================================================

TEST_CASE("a fresh tool says which target it is and that nothing has been built") {
    Live live("greet", {echoes("greet", "ok")});
    live.tell_tool(StatusRequested{});

    REQUIRE(live.ears->said.size() == 1);
    const BuildStatus& said = live.ears->last();
    CHECK(said.target == "greet");
    CHECK(said.outcome == outcome::kNeverBuilt);
    CHECK(said.builds == 0);
    CHECK(said.op == 0);
    // IT HOLDS NO COMMAND, and this is where that is visible: the tool can say
    // what it is before anything has run, and it cannot say what would be run,
    // because it does not know.
    CHECK(said.recipe.empty());
    CHECK(live.runner->ran() == 0);
}

TEST_CASE("an ask for the known target reaches a real process, and the answer is the truth") {
    Live live("greet", {echoes("greet", "ASYNC-1 was here")});
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
    CHECK(done.detail.find("ASYNC-1 was here") != std::string::npos);
    // THE RECIPE ARRIVED WITH THE START, from the thing that started it.
    CHECK(done.recipe.find("-E") != std::string::npos);
    CHECK(live.runner->ran() == 1);
    CHECK(live.runner->refused() == 0);
}

TEST_CASE("a failing build is reported as a failure, with its own exit status") {
    Live live("brk", {slow("brk", 1, "0.05", /*fail=*/true)});
    live.tell_tool(BuildRequested{"brk"});
    live.carry_until_over();

    const BuildStatus& done = live.ears->last();
    CHECK(done.outcome == outcome::kFailed);
    CHECK(done.status != 0);
    CHECK(live.runner->ran() == 1);
    // THE LAST WORDS SURVIVED. A failing build's diagnostic is written just
    // before it exits, and the whole point of draining before reaping is that
    // this line is here.
    CHECK(done.detail.find("asked to fail") != std::string::npos);
}

TEST_CASE("a build that never starts is not a build that failed") {
    Live live("gone", {BuildRecipe{"gone", "zengine-no-such-program-async1", {}, std::string()}});
    live.tell_tool(BuildRequested{"gone"});
    live.carry_until_over();

    const BuildStatus& done = live.ears->last();
    CHECK(done.outcome == outcome::kNotStarted);
    CHECK_FALSE(done.detail.empty());
    // The runner DID try, which is the difference between this and the refusal
    // case below: one name was in the catalog and one was not.
    CHECK(live.runner->ran() == 1);
    CHECK(live.runner->refused() == 0);
    CHECK(live.runner->live() == 0);
}

TEST_CASE("the tool refuses a name it does not hold, and NOTHING is ordered") {
    Live live("greet", {echoes("greet", "ok")});
    live.tell_tool(BuildRequested{"something-else"});

    REQUIRE(live.ears->said.size() == 1);
    const BuildStatus& said = live.ears->last();
    CHECK(said.outcome == outcome::kUnknownTarget);
    CHECK(said.target == "greet"); // it says what it DOES know
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
    Live live("slow", {slow("slow", 5, "0.15")});
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
    Live live("slow", {slow("slow", 5, "0.15")});
    live.tell_tool(BuildRequested{"slow"});
    REQUIRE(live.runner->live() == 1);

    Listener* latecomer = nullptr;
    mount_plain<Listener>(live.bus, loom::Grant{}, &latecomer);
    live.tell_tool(StatusRequested{});

    REQUIRE(latecomer->said.size() == 1);
    CHECK(latecomer->said[0].outcome == outcome::kRunning);
    CHECK(latecomer->said[0].op == 1);
    CHECK(latecomer->said[0].target == "slow");
    CHECK(latecomer->said[0].recipe.find("slow_build.cmake") != std::string::npos);

    live.carry_until_over();
    CHECK(live.tool->known().outcome == outcome::kSucceeded);
}

TEST_CASE("an observation about somebody else's work is counted, never adopted") {
    Live live("greet", {echoes("greet", "ok")});
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
    Live live("greet", {echoes("greet", "ok")});

    Impostor* liar = nullptr;
    loom::Grant may_only_ask;
    may_only_ask.allow_to_role(BuildRequested::zen_name, BuildRequested::zen_version,
                               kBuilderRole);
    const loom::WeaveId id = mount_plain<Impostor>(live.bus, std::move(may_only_ask), &liar,
                                                   std::string("greet"));

    (void)live.bus.send(id, loom::Message(loom::to_value(loom::Ack{})));
    live.bus.pump();

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
    live.bus.pump();

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
    auto bus = std::make_unique<loom::Switchboard>();
    mount_office<Undertaker>(*bus, loom::Grant{}, kBuilderRole,
                             static_cast<Undertaker**>(nullptr), &ledger);
    BuildRunnerWeave* runner = nullptr;
    mount_office<BuildRunnerWeave>(*bus, runner_grant(), kBuildRunnerRole, &runner,
                                   std::vector<BuildRecipe>{slow("forever", 20, "0.5")});

    (void)bus->send_to_role(kBuildRunnerRole, loom::Message(loom::to_value(RunBuild{"forever"})));
    bus->pump();
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
