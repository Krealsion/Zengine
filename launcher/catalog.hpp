// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_LAUNCHER_CATALOG_HPP
#define ZENGINE_LAUNCHER_CATALOG_HPP

// WHAT THE LAUNCHER MAY START, judged before anything starts (launcher/docs/zengine.launcher.md):
// a kind and a name, each looked up in a catalog the configuration fixed. An app is a folder
// directly in the project's `apps/` holding a plan the load plan's codec reads, started by the
// `zengine-app` beside Workshop on that folder; a demo is a setup the demo service lists, started
// by the configured interpreter running the configured `demo.py` on a root of its own in the
// project's `demo-runs/`. No message names a program, a path or an argument.

#include "app/folder.hpp"
#include "app/vocabulary.hpp"
#include "launcher/start.hpp"
#include "launcher/vocabulary.hpp"
#include "workshop/load_persist.hpp"

#include <algorithm>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

namespace zengine::launcher {

/// The folder in the project a demo's root is made in.
inline constexpr const char* kDemoRunsFolder = "demo-runs";
/// The log an app started on POSIX appends its output to, in its own folder.
inline constexpr const char* kAppLogName = "app.log";

/// One setup the demo service lists: its name, and the directory holding its `setup.json`.
struct DemoSetup {
    std::string name;
    std::string dir;
};

/// WHAT THE CONFIGURATION FIXED: the app host's program, and the demo service's interpreter,
/// script, build and Loom, with the setups it lists. A demo service left unconfigured starts no
/// demo.
struct Catalog {
    std::string app_program;
    std::string python;
    std::string demo_script;
    std::string build;
    std::string loom_prefix;
    std::vector<DemoSetup> demos;
};

/// A launch judged: what to start, or why nothing will be.
struct LaunchJudged {
    bool ok = false;
    std::string refusal;
    StartPlan plan;
};

/// The setup a demo's name names in `catalog`, or none.
inline const DemoSetup* demo_named(const Catalog& catalog, const std::string& name) {
    const auto it = std::find_if(catalog.demos.begin(), catalog.demos.end(),
                                 [&](const DemoSetup& d) { return d.name == name; });
    return it == catalog.demos.end() ? nullptr : &*it;
}

/// JUDGE A LAUNCH in `project` against `catalog`, starting nothing.
inline LaunchJudged judge_launch(const Launch& asked, const std::string& project,
                                 const Catalog& catalog) {
    namespace fs = std::filesystem;
    LaunchJudged out;
    const auto refuse = [&out](std::string why) {
        out.refusal = std::move(why);
        return out;
    };
    if (asked.kind != kKindApp && asked.kind != kKindDemo) {
        return refuse("the launcher starts an app or a demo, not '" + asked.kind + "'");
    }
    if (asked.name.size() > kMaxLaunchName) {
        return refuse("a name of at most " + std::to_string(kMaxLaunchName) + " bytes");
    }
    if (project.empty()) {
        return refuse("no project directory: an app or a demo starts inside the project");
    }
    std::error_code ec;
    if (asked.kind == kKindApp) {
        if (const std::string why = app::folder_name_problem(asked.name); !why.empty()) {
            return refuse("an app " + why);
        }
        const fs::path apps = fs::path(project) / app::kAppsFolder;
        const fs::path folder = apps / asked.name;
        if (!fs::is_directory(folder, ec)) {
            return refuse("no app '" + asked.name + "' in " + apps.generic_string());
        }
        // ...directly in `apps/`, a link to anywhere else included.
        const fs::path real = fs::canonical(folder, ec);
        const fs::path real_apps = fs::canonical(apps, ec);
        if (ec || real.parent_path() != real_apps) {
            return refuse("'" + asked.name + "' is not a folder directly in " + apps.generic_string());
        }
        const auto plan = workshop::load_persist::load_file((real / app::kPlanName).string());
        if (!plan.outcome.accepted) {
            return refuse("the app '" + asked.name + "' has no plan this Workshop reads: " +
                          plan.outcome.refusal);
        }
        if (catalog.app_program.empty() || !fs::is_regular_file(catalog.app_program, ec)) {
            return refuse("no " + std::string(app::kAppProgram) + " beside this Workshop (" +
                          catalog.app_program + ")");
        }
        out.plan = StartPlan{catalog.app_program, {real.generic_string()}, real.generic_string(),
                             (real / kAppLogName).generic_string()};
        out.ok = true;
        return out;
    }
    const DemoSetup* setup = demo_named(catalog, asked.name);
    if (setup == nullptr) {
        return refuse(catalog.demos.empty()
                          ? std::string("this Workshop starts no demo: it was configured with no "
                                        "demo service")
                          : "no demo '" + asked.name + "' among the setups the demo service lists");
    }
    if (!fs::is_regular_file(catalog.python, ec) || !fs::is_regular_file(catalog.demo_script, ec)) {
        return refuse("the demo service this Workshop was configured with is not there (" +
                      catalog.python + ", " + catalog.demo_script + ")");
    }
    const fs::path runs = fs::path(project) / kDemoRunsFolder;
    fs::create_directories(runs, ec);
    if (ec) {
        return refuse("cannot make " + runs.generic_string() + ": " + ec.message());
    }
    out.plan = StartPlan{catalog.python,
                         {catalog.demo_script, "start", "--setup", setup->name, "--root",
                          (runs / setup->name).generic_string(), "--build", catalog.build,
                          "--loom-prefix", catalog.loom_prefix},
                         fs::path(project).generic_string(),
                         (runs / (setup->name + ".log")).generic_string()};
    out.ok = true;
    return out;
}

} // namespace zengine::launcher

#endif // ZENGINE_LAUNCHER_CATALOG_HPP
