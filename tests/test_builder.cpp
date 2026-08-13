// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Builder suite — the tool, the runner, and the line between a NAME and a
// COMMAND.
//
// This package is the first thing in Zengine whose subject is an EFFECT: a child
// process, with a real exit status, started by a real fork/exec or
// CreateProcess. So the suite is arranged around the two questions that effect
// raises, and neither of them is "does the panel look right" (that is the
// Workshop suite's, next door, and it is a different claim):
//
//   1. DOES A BUILD ACTUALLY HAPPEN, and is what comes back true? Success,
//      failure and never-started are three outcomes and the suite drives all
//      three against a real program -- the very CMake that configured this tree,
//      asked to do the three smallest things it can do.
//   2. WHO IS ALLOWED TO CAUSE ONE? The tool holds a name and the runner holds
//      the commands, and the cases below assert the negative half of that with
//      the positive control beside it: a weave with the reach a PRESENTATION is
//      given cannot make a process start, and the same case with the grant
//      widened proves the first half was a measurement rather than a fixture
//      that never fired.
//
// NO PROCESS IS STARTED EXCEPT THROUGH A RECIPE THE CASE ITSELF WROTE. Every
// recipe here names `cmake -E ...`, which is CMake's own portable shim -- so the
// suite needs no shell, no /bin/sh, no .bat and no assumption about what else is
// installed, on either platform this repository builds for.

// main() and the framework live in doctest_main.cpp -- the shared one that
// refuses a run selecting zero cases (POP-01).
#include "doctest.h"

#include "builder/run.hpp"
#include "builder/runner.hpp"
#include "builder/vocabulary.hpp"
#include "builder/weave.hpp"

#include <zen/schema.hpp>
#include <zen/switchboard.hpp>
#include <zen/weave.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace zengine::builder;
using loom::schema_of;

namespace {

/// The CMake that configured this build tree, handed over as a compile
/// definition exactly as the Workshop host's recipe is. A test that resolved
/// `cmake` from PATH would be testing this machine's PATH.
const char* const kCMake = ZENGINE_TEST_CMAKE;

/// Mount a weave into an office with an explicit grant — the host's own two
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

struct HeardState {
    std::int64_t heard = 0;
    ZEN_SHAPE(HeardState, 1, ZEN_FIELD(heard));
};

/// Anything on this bus that accepts a BuildStatus. The Builder panel is one
/// such thing; this is another, and the tool cannot tell them apart, which is
/// the property that makes the panel a presentation rather than an owner.
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

struct PushState {
    std::int64_t sent = 0;
    ZEN_SHAPE(PushState, 1, ZEN_FIELD(sent));
};

/// A weave that tries to ORDER THE RUNNER DIRECTLY when poked with an Ack.
///
/// It exists to answer one question with a measurement instead of a sentence:
/// can something that is only allowed to ASK the Builder make a process start?
/// The trigger is `loom::Ack` because the suite needs a shape to knock with and
/// this one carries nothing.
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

/// One recipe that always succeeds, named whatever the case wants to call it.
BuildRecipe echoes(const std::string& target, const std::string& what) {
    return BuildRecipe{target, kCMake, {"-E", "echo", what}, std::string()};
}

/// A live Builder: the tool in its office, the runner in its own, a listener,
/// and the grants the host writes.
struct Live {
    loom::Switchboard bus;
    BuilderWeave* tool = nullptr;
    BuildRunnerWeave* runner = nullptr;
    Listener* ears = nullptr;
    loom::WeaveId tool_id{};
    loom::WeaveId runner_id{};

    explicit Live(std::string target, std::vector<BuildRecipe> catalog) {
        // THE RUNNER'S WHOLE REACH: report an outcome to the Builder office.
        loom::Grant run_builds;
        run_builds.allow_to_role(BuildOutcome::zen_name, BuildOutcome::zen_version, kBuilderRole);
        runner_id = mount_office<BuildRunnerWeave>(bus, std::move(run_builds), kBuildRunnerRole,
                                                  &runner, std::move(catalog));

        // THE TOOL'S: order the runner, and say what it knows.
        loom::Grant order_builds;
        order_builds.allow_to_role(RunBuild::zen_name, RunBuild::zen_version, kBuildRunnerRole);
        order_builds.allow_to_any(BuildStatus::zen_name, BuildStatus::zen_version);
        tool_id = mount_office<BuilderWeave>(bus, std::move(order_builds), kBuilderRole, &tool,
                                             std::move(target));

        ears = mount_listener();
    }

    Listener* mount_listener() {
        auto ears_owned = std::make_unique<Listener>();
        Listener* raw = ears_owned.get();
        const loom::WeaveId id = bus.register_weave(std::move(ears_owned), loom::Grant{});
        raw->zen_set_self(id);
        return raw;
    }

    /// Speak to the Builder office the way a presentation does — as a root send,
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
};

} // namespace

// ============================================================================
// Tier 1 — the wire cannot spell a command
// ============================================================================

TEST_CASE("contract: nothing a build conversation carries is a command") {
    using loom::Kind;
    using loom::SchemaBuilder;

    // THE HARD BOUNDARY OF BLD-0, asserted as a SHAPE rather than as a check.
    // Both shapes that can cause work carry exactly one field, and it is a
    // target NAME. There is no program, no argument list, no working directory
    // and no shell line anywhere on this wire -- so "the panel sent a command"
    // is not a sentence this vocabulary can express, and a phase that wanted to
    // make it one would have to change these declarations and this case with
    // them.
    const auto requested = SchemaBuilder("BuildRequested", 1).field("target", Kind::Text).build();
    CHECK(schema_of<BuildRequested>()->content_id() == requested->content_id());

    const auto order = SchemaBuilder("RunBuild", 1).field("target", Kind::Text).build();
    CHECK(schema_of<RunBuild>()->content_id() == order->content_id());

    // The ask that opens a panel carries nothing at all: there is one tool at
    // the office and it has one condition, so the question has no parameters.
    const auto describe = SchemaBuilder("StatusRequested", 1).build();
    CHECK(schema_of<StatusRequested>()->content_id() == describe->content_id());

    // What comes BACK does name what ran -- from the only thing that could
    // honestly say it, after it has said it.
    const auto outcome = SchemaBuilder("BuildOutcome", 1)
                             .field("target", Kind::Text)
                             .field("started", Kind::Bool)
                             .field("status", Kind::Int)
                             .field("recipe", Kind::Text)
                             .field("detail", Kind::Text)
                             .build();
    CHECK(schema_of<BuildOutcome>()->content_id() == outcome->content_id());
}

// ============================================================================
// Tier 2 — running a real process, and telling the truth about it
// ============================================================================

TEST_CASE("a recipe that succeeds: it started, it exited 0, and it said what it said") {
    const RunResult ok = run_recipe(echoes("greet", "hello from BLD-0"));
    CHECK(ok.started);
    CHECK(ok.status == 0);
    CHECK(ok.output.find("hello from BLD-0") != std::string::npos);
    CHECK(ok.trouble.empty());
}

TEST_CASE("a recipe that fails is a DIFFERENT answer from one that never started") {
    // `cmake -E false` is a real process that exits non-zero -- the smallest
    // honest failure there is, with no compiler and no build tree involved.
    const RunResult failed = run_recipe(BuildRecipe{"no", kCMake, {"-E", "false"}, std::string()});
    CHECK(failed.started);
    CHECK(failed.status != 0);

    // ...and a program that is not there never began. `started` is what tells
    // the two apart, and a single non-zero number could not: an exit status of
    // 127 from a failed exec would otherwise read as a build that ran and
    // failed, which is a different thing to tell a maker.
    const RunResult absent = run_recipe(
        BuildRecipe{"no", "zengine-no-such-program-bld0", {"--version"}, std::string()});
    CHECK_FALSE(absent.started);
    CHECK_FALSE(absent.trouble.empty());

    // A recipe with no program at all is refused before anything is attempted.
    const RunResult empty = run_recipe(BuildRecipe{"no", std::string(), {}, std::string()});
    CHECK_FALSE(empty.started);
    CHECK(empty.trouble.find("no program") != std::string::npos);
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
    CHECK(tail_lines("", 3).empty());
    CHECK(tail_lines("\n\n\n", 3).empty());
    // Trailing blank lines are not what a maker meant by "the last line".
    CHECK(tail_lines("only\n\n\n", 2) == "only");
}

// ============================================================================
// Tier 3 — the tool: a name, a memory, and a refusal
// ============================================================================

TEST_CASE("a fresh tool says which target it is and that nothing has been built") {
    Live live("greet", {echoes("greet", "ok")});
    live.tell_tool(StatusRequested{});

    REQUIRE(live.ears->said.size() == 1);
    const BuildStatus& said = live.ears->last();
    CHECK(said.target == "greet");
    CHECK(said.outcome == outcome::kNeverBuilt);
    CHECK(said.builds == 0);
    // IT HOLDS NO COMMAND, and this is where that is visible: the tool can say
    // what it is before anything has run, and it cannot say what would be run,
    // because it does not know.
    CHECK(said.recipe.empty());
    CHECK(live.runner->ran() == 0);
}

TEST_CASE("an ask for the known target reaches a real process, and the answer is the truth") {
    Live live("greet", {echoes("greet", "BLD-0 was here")});
    live.tell_tool(BuildRequested{"greet"});

    // Two publications: the tool said it had been asked, then said what came
    // back. The order is the order the facts became true.
    REQUIRE(live.ears->said.size() == 2);
    CHECK(live.ears->said[0].outcome == outcome::kAsked);

    const BuildStatus& done = live.ears->said[1];
    CHECK(done.outcome == outcome::kSucceeded);
    CHECK(done.status == 0);
    CHECK(done.builds == 1);
    CHECK(done.detail.find("BLD-0 was here") != std::string::npos);
    // THE RECIPE ARRIVES WITH THE OUTCOME, from the thing that ran it.
    CHECK(done.recipe.find("-E") != std::string::npos);
    CHECK(live.runner->ran() == 1);
    CHECK(live.runner->refused() == 0);
}

TEST_CASE("a failing build is reported as a failure, with its own exit status") {
    Live live("brk", {BuildRecipe{"brk", kCMake, {"-E", "false"}, std::string()}});
    live.tell_tool(BuildRequested{"brk"});

    REQUIRE(live.ears->said.size() == 2);
    const BuildStatus& done = live.ears->said.back();
    CHECK(done.outcome == outcome::kFailed);
    CHECK(done.status != 0);
    CHECK(live.runner->ran() == 1);
}

TEST_CASE("a build that never starts is not a build that failed") {
    Live live("gone", {BuildRecipe{"gone", "zengine-no-such-program-bld0", {}, std::string()}});
    live.tell_tool(BuildRequested{"gone"});

    REQUIRE(live.ears->said.size() == 2);
    const BuildStatus& done = live.ears->said.back();
    CHECK(done.outcome == outcome::kNotStarted);
    CHECK_FALSE(done.detail.empty());
    // The runner DID try, which is the difference between this and the refusal
    // case below: one name was in the catalog and one was not.
    CHECK(live.runner->ran() == 1);
    CHECK(live.runner->refused() == 0);
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
    CHECK(live.runner->ran() == 1);
    CHECK(live.ears->last().outcome == outcome::kSucceeded);
}

TEST_CASE("an outcome about somebody else's target is counted, never adopted") {
    Live live("greet", {echoes("greet", "ok")});
    live.tell_tool(BuildOutcome{"a-different-target", true, 0, "some recipe", "some detail"});

    // Nothing was published, because nothing about this tool changed.
    CHECK(live.ears->said.empty());
    CHECK(live.tool->known().outcome == outcome::kNeverBuilt);
    CHECK(live.tool->known().recipe.empty());
    // ...and it is a number rather than a silence, so "this never happens" is
    // something an operator can check instead of something a comment asserts.
    CHECK(live.tool->known().stray == 1);
}

// ============================================================================
// Tier 4 — who may cause a process to start
// ============================================================================

TEST_CASE("the runner turns down a name it holds no recipe for, and starts nothing") {
    Live live("greet", {echoes("greet", "ok")});
    live.tell_runner(RunBuild{"not-in-the-catalog"});

    CHECK(live.runner->ran() == 0);
    CHECK(live.runner->refused() == 1);
    // The refusal reaches the Builder office as an ordinary outcome, so the
    // tool has one code path for "what came back" rather than two.
    CHECK(live.tool->known().stray == 1); // it was about a target this tool does not hold

    // THE CANARY: a name that IS in the catalog runs, through the same door.
    live.tell_runner(RunBuild{"greet"});
    CHECK(live.runner->ran() == 1);
}

TEST_CASE("a weave with a presentation's reach cannot make a process start") {
    // THE AUTHORITY MEASUREMENT OF THIS PHASE. Workshop is granted the right to
    // ASK the Builder for a build by name; it is not granted the right to order
    // the runner. This case asserts the second half by giving a weave exactly
    // that reach and having it try the thing it is not allowed to do.
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
    auto weave = std::make_unique<Impostor>("greet");
    liar = weave.get();
    const loom::WeaveId id = live.bus.register_weave(std::move(weave), std::move(may_only_ask));
    liar->zen_set_self(id);

    (void)live.bus.send(id, loom::Message(loom::to_value(loom::Ack{})));
    live.bus.pump();

    CHECK(liar->tried == 1); // it really did try
    CHECK(live.runner->ran() == 0); // and no process began
    CHECK(live.runner->refused() == 0);

    // THE CANARY: the identical weave, with the runner in its grant, DOES make a
    // process start. Without it, "no process began" would be satisfied by an
    // Impostor whose send never compiled into anything, a role that was never
    // held, or a runner that could not run.
    Impostor* honest = nullptr;
    loom::Grant may_order;
    may_order.allow_to_role(RunBuild::zen_name, RunBuild::zen_version, kBuildRunnerRole);
    auto second = std::make_unique<Impostor>("greet");
    honest = second.get();
    const loom::WeaveId other =
        live.bus.register_weave(std::move(second), std::move(may_order));
    honest->zen_set_self(other);

    (void)live.bus.send(other, loom::Message(loom::to_value(loom::Ack{})));
    live.bus.pump();

    CHECK(honest->tried == 1);
    CHECK(live.runner->ran() == 1);
}
