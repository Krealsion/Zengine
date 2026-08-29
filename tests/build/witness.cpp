// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// THE BUILD WITNESS -- a host that builds one authored recipe and, when it works and
// the project already wanted it, realizes the result without restarting (BLD-1).
//
// ---- Why this exists and Workshop does not do instead --------------------------
//
// The claim BLD-1 has to prove end to end is:
//
//     one .cpp -> a generated CMake project -> the SUPPORTED Zengine package
//              -> a real compile and link -> a real loadable artifact
//              -> the RUNNING host's existing realization owner
//
// Every part of that except the compile is pinned in the ordinary suites. The compile
// cannot be: it needs an INSTALLED Zengine prefix, a toolchain, and two nested CMake
// projects, which is a lane and not a ctest entry (`tests/package/run.cmake`'s header
// says why at length, and this is the same line drawn for the same reason).
//
// Workshop itself is the obvious host to drive here and is the wrong one: it is an
// interactive terminal application whose whole surface is a picture, so a script
// driving it would be measuring a Skin and a key translation on the way to measuring
// a build. This host is Workshop's Builder wiring with the picture removed -- THE SAME
// weaves, THE SAME grants, THE SAME realization owner, THE SAME ordinary host loop --
// and it prints lines a script can read instead of painting cells a person can.
//
// ⚠ WHAT IT IS NOT A WITNESS OF. This program is built INSIDE Zengine's own tree and
// links Zengine's own targets, so nothing about IT is evidence of package purity. The
// purity claim is about the project it GENERATES, which is configured outside both
// repositories against an installed prefix and nothing else -- and `run.cmake` beside
// this file breaks that prefix on purpose to prove the claim can fail.
//
// ---- The loop is Workshop's, deliberately ---------------------------------------
//
//     while (!quit) { bus.drain_until_idle(); }
//
// That is the host boundary, and it is the entire scheduling this program contains --
// copied from `workshop.cpp` line for line, including the door a participant leaves by
// (`bus.stop()`). Nothing here counts turns for a build, waits for a load, or asks
// whether anything is finished: the runner polls its own OS handles because it owns
// them, the tool reacts to the runner's facts, the realization owner reacts to the
// load's answer, and this file reacts to nothing except a participant asking to stop.
//
// ⚠ THE PLAN LOADS A REAL TIMER SERVICE, and that is what makes the loop above honest
// rather than a spin. The runner asks the Timer for a beat while it holds an operation
// and gives it back when it does not; with no Timer deployed a host has to open the
// same hands itself, and a witness that did that would be measuring its own poll. So
// this host deploys one, exactly as the shipped Workshop plans do, and then does
// nothing at all while a real compiler runs.
//
// THERE IS NO TIMEOUT HERE, and the absence is deliberate: a running operation is
// simply running, and how long is too long is a judgement about a LANE rather than
// about a build. The driver beside this file bounds the wall clock, where that
// judgement belongs.

#include "builder/runner.hpp"
#include "builder/vocabulary.hpp"
#include "builder/weave.hpp"

#include "workshop/load_execute.hpp"
#include "workshop/load_persist.hpp"
#include "workshop/recipe_persist.hpp"
#include "workshop/recipes.hpp"

#include "operator/catalog.hpp"
#include "operator/host_surface.hpp"
#include "timer/vocabulary.hpp"

#include <zen/kernel/control.hpp>
#include <zen/kernel/kernel.hpp>
#include <zen/kernel/manager.hpp>
#include <zen/switchboard.hpp>
#include <zen/weave.hpp>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

namespace builder = zengine::builder;
namespace load = zengine::workshop::load;
namespace load_persist = zengine::workshop::load_persist;
namespace op = zengine::op;
namespace recipe_persist = zengine::workshop::recipe_persist;
namespace workshop = zengine::workshop;
namespace timer = zengine::timer;

/// Mount an in-process weave into an OFFICE, with the grant this host chose --
/// `workshop.cpp`'s helper, copied because the claim is that the wiring is the same.
template <class Weave, class... Args>
loom::WeaveId mount_in_office(loom::Switchboard& bus, loom::Grant grant, const char* office,
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

/// THE ONE RULE THAT SPELLS A STEM AS A FILE, and it is the host's -- `HostContext::so`
/// said again here rather than reached for, because this program deliberately does not
/// link Workshop's UI vocabulary.
std::string so_in(std::string_view dir, std::string_view stem) {
    return std::string(dir) + "/" + std::string(stem) +
#if defined(_WIN32)
           ".dll";
#else
           ".so";
#endif
}

/// WHAT THIS HOST IS WATCHING, AND THE ONLY THING THAT ENDS ITS LOOP.
///
/// It is an ORDINARY WEAVE that accepts the two publications the Builder makes, prints
/// them, and sets a flag when the conversation it is following has no more to say. It
/// commands nothing and asks nothing: a presentation, exactly as the Builder panel is.
struct ReporterState {
    std::int64_t heard = 0;
    ZEN_SHAPE(ReporterState, 1, ZEN_FIELD(heard));
};

class Reporter
    : public loom::WeaveBase<Reporter, ReporterState,
                             loom::Accept<builder::BuildStatus, builder::RecipeCatalog>,
                             loom::Emit<builder::BuildRequested>> {
public:
    /// `stop` IS THE HOST'S OWN DOOR, handed over exactly as `HostContext::request_stop`
    /// is: this weave decides that the conversation it was following is over, and the
    /// HOST decides what that means for the process.
    Reporter(std::string recipe, bool realize, std::function<void()> stop)
        : recipe_(std::move(recipe)), realize_(realize), stop_(std::move(stop)) {}

    /// THE CATALOG ARRIVES FIRST, and this is where the ask is made -- from inside a
    /// delivery, the way a panel's key press makes one, rather than from `main` before
    /// anything is alive.
    void on(const builder::RecipeCatalog& said, loom::Mail& mail) {
        if (asked_) {
            return;
        }
        std::printf("witness: recipes:");
        for (const builder::RecipeSummary& r : said.recipes) {
            std::printf(" %s->%s", r.recipe.c_str(), r.artifact.c_str());
        }
        std::printf("\n");
        asked_ = true;
        std::printf("witness: asking for `%s`%s\n", recipe_.c_str(),
                    realize_ ? " AND to realize it" : "");
        std::fflush(stdout);
        (void)mail.send_to_role(builder::kBuilderRole,
                                builder::BuildRequested{recipe_, realize_});
    }

    void on(const builder::BuildStatus& said, loom::Mail&) {
        ++state_.heard;
        // ⚠ IS THIS STATUS ABOUT THE ASK THIS HOST MADE? The tool publishes what it is
        // the moment it is asked, and at that moment it is about NOTHING -- an empty
        // recipe and `not built yet`. A witness that took the first status it heard as
        // its own answer would end its loop before the build had begun, which is
        // exactly the confusion between LEARNING a fact and WATCHING an event that the
        // Builder panel's `awaiting` latch exists to prevent.
        //
        // A REFUSED NAME IS THE ONE EXCEPTION, and it has to be: the tool never took
        // the ask, so the picture is still about nothing and there is nothing coming.
        if (said.outcome != builder::outcome::kUnknownRecipe && said.recipe != recipe_) {
            return;
        }
        // ONE LINE PER STATUS, and both outcomes on it. A script reading this needs to
        // be able to tell a build that worked whose realization was refused from a
        // build that failed, which is the whole reason there are two fields.
        std::printf("witness: build=%s artifact=%s exit=%lld realize=%s detail=%s\n",
                    builder::name_of_outcome(said.outcome), said.artifact.c_str(),
                    static_cast<long long>(said.status),
                    builder::name_of_realization(said.realization),
                    said.detail.c_str());
        if (!said.realized_detail.empty()) {
            std::printf("witness: realization: %s\n", said.realized_detail.c_str());
        }
        std::fflush(stdout);
        last = said;
        if (builder::still_going(said.outcome)) {
            return;
        }
        // THE BUILD IS OVER. Whether this conversation is over depends on whether a
        // second question was asked -- which is exactly the two-latch distinction the
        // Builder panel draws, said here without a screen.
        if (realize_ && said.realization != builder::realization::kRealized &&
            said.realization != builder::realization::kRefused) {
            return;
        }
        done = true;
        if (stop_) {
            stop_();
        }
    }

    builder::BuildStatus last{};
    bool done = false;

private:
    std::string recipe_;
    bool realize_ = false;
    bool asked_ = false;
    std::function<void()> stop_;
};

struct Arguments {
    std::string dir;       ///< where artifacts live and where a stem is resolved
    std::string plan;      ///< the authored load plan
    std::string recipes;   ///< the authored build recipes
    std::string recipe;    ///< which recipe to build
    bool realize = false;  ///< BUILD, or BUILD & REALIZE
};

bool parse(int argc, char** argv, Arguments& args) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--realize") {
            args.realize = true;
            continue;
        }
        if (i + 1 >= argc) {
            std::printf("witness: %s needs a value\n", arg.c_str());
            return false;
        }
        const std::string value = argv[++i];
        if (arg == "--dir") {
            args.dir = value;
        } else if (arg == "--load-plan") {
            args.plan = value;
        } else if (arg == "--recipes") {
            args.recipes = value;
        } else if (arg == "--build") {
            args.recipe = value;
        } else {
            std::printf("witness: unknown argument `%s`\n", arg.c_str());
            return false;
        }
    }
    if (args.dir.empty() || args.plan.empty() || args.recipes.empty() || args.recipe.empty()) {
        std::printf("witness: --dir, --load-plan, --recipes and --build are all required\n");
        return false;
    }
    return true;
}

} // namespace

int main(int argc, char** argv) {
    Arguments args;
    if (!parse(argc, argv, args)) {
        return 2;
    }

    // ---- The two authored files, read before anything is built ------------------
    const load_persist::LoadedPlan read_plan = load_persist::load_file(args.plan);
    if (!read_plan.outcome.accepted) {
        std::printf("witness: load plan refused: %s\n", read_plan.outcome.refusal.c_str());
        return 3;
    }
    recipe_persist::LoadedRecipes read_recipes = recipe_persist::load_file(args.recipes);
    if (!read_recipes.outcome.accepted) {
        std::printf("witness: build recipes refused: %s\n", read_recipes.outcome.refusal.c_str());
        return 5;
    }
    // THE ONE CURRENT COMPLETED CATALOG, held exactly as `workshop.cpp` holds it
    // (PROJ-0) -- declared before the bus, so it outlives every weave that reads it.
    workshop::CurrentRecipes current_recipes;
    {
        std::vector<builder::Recipe> recipes = std::move(read_recipes.recipes);
        // THE HOST'S TWO DEFAULTS, filled in exactly as `workshop.cpp` fills them. This
        // host completes no SOURCE, because it has no project to complete one against:
        // the lane authors absolute spellings, and guessing a base is the one thing
        // worse than the refusal (`recipe_persist::complete_recipes` says so at length).
        for (builder::Recipe& r : recipes) {
            if (r.artifact_dir.empty()) {
                r.artifact_dir = args.dir;
            }
            if (r.single_source.has_value() && r.single_source->workspace.empty()) {
                r.single_source->workspace = args.dir + "/build-workspace/" + r.id;
            }
        }
        current_recipes.hold(std::move(recipes), &so_in);
    }

    loom::Switchboard bus;
    op::Catalog operators;
    op::OperatorHostSurface operator_host(operators);
    loom::Kernel kernel(bus);
    const loom::WeaveId control = loom::mount_control(kernel, bus);
    const loom::WeaveId manager = loom::mount_manager(control, bus);

    // ---- The Builder's two offices, with the host's own grants ------------------
    loom::Grant run_builds;
    run_builds.allow_to_role(builder::BuildStarted::zen_name, builder::BuildStarted::zen_version,
                             builder::kBuilderRole);
    run_builds.allow_to_role(builder::BuildOutput::zen_name, builder::BuildOutput::zen_version,
                             builder::kBuilderRole);
    run_builds.allow_to_role(builder::BuildFinished::zen_name,
                             builder::BuildFinished::zen_version, builder::kBuilderRole);
    run_builds.allow_to_role(builder::BuildNotStarted::zen_name,
                             builder::BuildNotStarted::zen_version, builder::kBuilderRole);
    run_builds.allow_to_role(timer::EnsureTimer::zen_name, timer::EnsureTimer::zen_version,
                             timer::kTimerRole);
    run_builds.allow_to_role(timer::CancelTimer::zen_name, timer::CancelTimer::zen_version,
                             timer::kTimerRole);
    builder::BuildRunnerWeave* runner = nullptr;
    (void)mount_in_office<builder::BuildRunnerWeave>(bus, std::move(run_builds),
                                                     builder::kBuildRunnerRole, &runner,
                                                     current_recipes.all(),
                                                     std::string(ZENGINE_BUILDER_CMAKE));

    loom::Grant order_builds;
    order_builds.allow_to_role(builder::RunBuild::zen_name, builder::RunBuild::zen_version,
                               builder::kBuildRunnerRole);
    order_builds.allow_to_any(builder::BuildStatus::zen_name, builder::BuildStatus::zen_version);
    order_builds.allow_to_any(builder::RecipeCatalog::zen_name,
                              builder::RecipeCatalog::zen_version);
    order_builds.allow_to_any(builder::OfferArtifact::zen_name,
                              builder::OfferArtifact::zen_version);
    (void)mount_in_office<builder::BuilderWeave>(bus, std::move(order_builds),
                                                 builder::kBuilderRole,
                                                 static_cast<builder::BuilderWeave**>(nullptr),
                                                 current_recipes.views());

    // ---- The realization owner, its voice, and the one dangerous grant ----------
    loom::Grant operate;
    operate.allow(loom::LoadWeave::zen_name, loom::LoadWeave::zen_version, manager);
    operate.allow_to_any(builder::ArtifactRealized::zen_name,
                         builder::ArtifactRealized::zen_version);
    load::BootAnswers answers;
    auto speaker = std::make_unique<load::PlanBooter>(answers);
    load::PlanBooter& voice = *speaker;
    const loom::WeaveId booter = bus.register_weave(std::move(speaker), std::move(operate));
    voice.zen_set_self(booter);

    bool refused = false;
    load::PlanExecutor executor(
        bus, operators, operator_host, voice, manager, answers,
        [&args](const std::string& stem) { return so_in(args.dir, stem); },
        [&refused](const load::Executed& done) {
            for (const load::ResolvedArtifact& row : done.resolved) {
                std::printf("witness: loaded: %s%s\n", row.stem.c_str(),
                            row.weave_loaded ? " (weave)" : " (provider)");
            }
            if (!done.waiting_on.empty()) {
                std::printf("witness: waiting to be built: %s\n", done.waiting_on.c_str());
            }
            // ⚠ `ok` FALSE IS NOT A REFUSAL BY ITSELF (BLD-1a). Realization comes to
            // rest at a waiting row too, and that is neither completion nor failure --
            // a refusal is one because a layer below actually stated it, which is what
            // `refusal` carries. `workshop.cpp` draws the same line, in the same order.
            if (!done.ok && !done.refusal.empty()) {
                std::printf("witness: project refused: %s\n", done.refusal.c_str());
                refused = true;
            }
            std::fflush(stdout);
        },
        // IS THIS ROW WAITING ON THE MAKER? `workshop.cpp`'s predicate, said again:
        // the artifact is not on this disk AND some authored recipe can produce it.
        [&args, &current_recipes](const std::string& stem) {
            for (const builder::Recipe& r : current_recipes.all()) {
                if (r.artifact != stem) {
                    continue;
                }
                return !std::filesystem::exists(std::filesystem::path(so_in(args.dir, stem)));
            }
            return false;
        });

    // ---- The thing that asks, and the thing that ends the loop ------------------
    loom::Grant speak;
    speak.allow_to_role(builder::BuildRequested::zen_name, builder::BuildRequested::zen_version,
                        builder::kBuilderRole);
    speak.allow_to_role(builder::StatusRequested::zen_name,
                        builder::StatusRequested::zen_version, builder::kBuilderRole);
    Reporter* reporter = nullptr;
    (void)mount_in_office<Reporter>(bus, std::move(speak), "zengine.witness", &reporter,
                                    args.recipe, args.realize, [&bus] { bus.stop(); });

    executor.begin(read_plan.plan);

    // ASK THE TOOL WHAT IT IS, the way an opening panel does. Root-sent, because this
    // is the host and a host may put a message on its own bus.
    (void)bus.send_to_role(builder::kBuilderRole,
                           loom::Message(loom::to_value(builder::StatusRequested{})));

    // ---- THE HOST LOOP, AND IT IS THE WHOLE OF THIS PROGRAM'S SCHEDULING --------
    //
    // `workshop.cpp`'s loop, copied: everything runs inside `drain_until_idle()`, the
    // Timer service's nap paces it, and a participant asking to stop is what ends it.
    // A call that returns with an empty queue means nothing in this process will ever
    // speak again -- say so and leave rather than spin.
    //
    // NOTHING SEMANTIC TURNS THIS CRANK. The tool never asks whether its build is
    // done, the realization owner never asks whether its load settled, and neither
    // could: there is no shape in either vocabulary with which to ask.
    while (!reporter->done && !refused) {
        bus.drain_until_idle();
        if (!reporter->done && !refused && bus.pending() == 0) {
            std::printf("witness: the bus went quiet without an answer "
                        "(no timer service deployed?)\n");
            break;
        }
    }

    if (refused) {
        std::printf("witness: RESULT project-refused\n");
        return 4;
    }
    if (!reporter->done) {
        std::printf("witness: RESULT nothing-answered\n");
        return 6;
    }
    const builder::BuildStatus& last = reporter->last;
    std::printf("witness: RESULT build=%s realization=%s\n",
                builder::name_of_outcome(last.outcome),
                builder::name_of_realization(last.realization));
    std::printf("witness: artifact-present=%s\n",
                std::filesystem::exists(std::filesystem::path(
                    so_in(args.dir, last.artifact)))
                    ? "yes"
                    : "no");
    // ...AND WHETHER THE KERNEL ACTUALLY HOLDS IT, which is a different question with
    // a different owner and is the one that says a realization really happened.
    std::printf("witness: loaded=%s\n", kernel.is_loaded(last.artifact) ? "yes" : "no");
    std::fflush(stdout);
    return builder::artifact_produced(last.outcome) ? 0 : 1;
}
