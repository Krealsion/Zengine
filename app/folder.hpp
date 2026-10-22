// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_APP_FOLDER_HPP
#define ZENGINE_APP_FOLDER_HPP

// AN APP'S FOLDER, WRITTEN FROM A SAVED VIEW AND ITS FLOW (app/docs/app.md, "Export an app"):
// `<project>/apps/<name>/` holding the plan, `app.json`, the view's and the flow's files and the
// two launchers. Everything is judged before anything is written -- the view and the flow read
// back through their own gates, `app.json` through the app host's own reader -- and the folder's
// other files, the console's decisions among them, are never touched. It writes no decision and
// starts nothing.

#include "app/files.hpp"
#include "app/program.hpp"
#include "app/vocabulary.hpp"

#include "flow/project.hpp"
#include "graphics/vocabulary.hpp"
#include "maker/files.hpp"
#include "view/description.hpp"
#include "workshop/load_persist.hpp"

#include <zen/serialize.hpp>

#include <cstdint>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

namespace zengine::app {

/// The file the app host writes while it runs; an export never touches it.
inline constexpr const char* kSessionName = "app-session.json";
/// The two launchers an export writes.
inline constexpr const char* kLauncherCmd = "run.cmd";
inline constexpr const char* kLauncherSh = "run.sh";
/// Where an exported app's view stands in its window, and the margin around it, in pixels.
inline constexpr std::int64_t kViewInset = 8;
/// The skin an exported app's window paints its areas with.
inline constexpr const char* kExportSkin = "day";
/// The longest name an exported app may have, in bytes.
inline constexpr std::size_t kMaxAppName = 64;

/// THE PLAN AN EXPORTED APP LOADS: the operators its flow folds with, the app's own weave, the
/// graphics directory, the OS window and the timer, in that order.
inline workshop::load::LoadPlan export_plan() {
    namespace load = workshop::load;
    const auto provider = load::ProviderIntent{op::MountMode::Ordinary};
    const auto weave = [](const char* role) { return load::WeaveIntent{role}; };
    load::LoadPlan plan;
    plan.artifacts = {{"zengine-operators-basic", provider, std::nullopt},
                      {"zengine-app-weave", std::nullopt, weave(kAppRole)},
                      {"zengine-graphics", std::nullopt, weave("zengine.graphics")},
                      {"zengine-window-sdl", std::nullopt, weave("zengine.window.sdl")},
                      {"zengine-timer", provider, weave("zengine.timer")}};
    return plan;
}

/// A name an app's folder may have: letters, digits, `-`, `_` and `.`, not first, at most
/// `kMaxAppName` bytes. Empty when it is one, else why not.
inline std::string folder_name_problem(const std::string& name) {
    if (name.empty()) {
        return "names no app";
    }
    if (name.size() > kMaxAppName) {
        return "'" + name + "' is longer than " + std::to_string(kMaxAppName) + " bytes";
    }
    if (name.front() == '.') {
        return "'" + name + "' begins with a dot";
    }
    for (const char c : name) {
        const bool plain = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                           (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.';
        if (!plain) {
            return "'" + name + "' is not a plain folder name: letters, digits, '-', '_' and '.'";
        }
    }
    return {};
}

/// What an export is asked: the project, the saved view's file and the app host's program the
/// launchers start.
struct ExportAsked {
    std::string project;
    std::string view_file;
    std::string program;
};

/// AN EXPORT JUDGED, with nothing written: the app's name and folder, whether that folder is
/// already there, and every byte the export would write -- or why it would not.
struct ExportJudged {
    bool ok = false;
    std::string refusal;
    std::string name;
    std::string folder;
    std::string flow_file;
    bool exists = false;
    std::vector<std::string> stems; ///< the artifacts the plan names, which the console will ask
    std::string plan_text, app_text, view_bytes, flow_bytes, launcher_cmd, launcher_sh;
};

/// What an export wrote: each file, or why it stopped, naming what it had written by then.
struct Exported {
    bool ok = false;
    std::string refusal;
    std::vector<std::string> written;
};

/// `run.cmd`: the program, started in a console of its own on the folder the file stands in.
inline std::string launcher_cmd(const std::string& name, const std::string& program) {
    std::string path = std::filesystem::path(program).make_preferred().string();
    std::string escaped;
    for (const char c : path) {
        escaped += c;
        if (c == '%') {
            escaped += '%'; // a percent sign in a command file is written twice
        }
    }
    return "@start \"" + name + "\" \"" + escaped + "\" \"%~dp0.\"\r\n";
}

/// `run.sh`: the program on the folder the file stands in, in the terminal that ran it.
inline std::string launcher_sh(const std::string& program) {
    std::string quoted = "'";
    for (const char c : program) {
        quoted += c == '\'' ? std::string("'\\''") : std::string(1, c);
    }
    quoted += "'";
    return "#!/bin/sh\nexec " + quoted + " \"$(dirname \"$0\")\"\n";
}

/// JUDGE AN EXPORT, writing nothing: the view's file read back through its gate, its flow beside
/// it as `<stem>.flow` through its own, the name a plain folder name, the program present, and
/// `app.json` read back through the app host's reader.
inline ExportJudged judge_export(const ExportAsked& asked) {
    namespace fs = std::filesystem;
    ExportJudged out;
    const auto refuse = [&out](std::string why) {
        out.refusal = std::move(why);
        return out;
    };
    if (asked.project.empty()) {
        return refuse("no project directory: an app is written into the project's " +
                      std::string(kAppsFolder) + " folder");
    }
    if (asked.view_file.empty()) {
        return refuse("the view has no file: save it first, and the app is made from that file");
    }
    const fs::path view_path(asked.view_file);
    out.name = view_path.stem().string();
    if (const std::string why = folder_name_problem(out.name); !why.empty()) {
        return refuse("the app is named after the view's file, and " + why);
    }
    const maker::FileBytes view_read = maker::read_file(asked.view_file);
    if (!view_read) {
        return refuse(view_read.reason);
    }
    const view::Admitted view_admitted = view::read_description(view_read.bytes);
    if (!view_admitted) {
        return refuse(asked.view_file + " is not a view: " + view_admitted.reason);
    }
    fs::path flow_path = view_path;
    flow_path.replace_extension(".flow");
    out.flow_file = flow_path.generic_string();
    std::error_code ec;
    if (!fs::exists(flow_path, ec)) {
        return refuse("no " + flow_path.filename().string() + " beside " + asked.view_file +
                      ": export its flow from Flow as " + out.flow_file + " first");
    }
    const maker::FileBytes flow_read = maker::read_file(out.flow_file);
    if (!flow_read) {
        return refuse(flow_read.reason);
    }
    std::string flow_name;
    try {
        flow_name = flow::read_project(flow_read.bytes).definition.name;
    } catch (const std::exception& e) {
        return refuse(out.flow_file + " is not a flow Flow exported: " + e.what());
    }
    if (asked.program.empty() || !fs::is_regular_file(fs::path(asked.program), ec)) {
        return refuse("no " + std::string(kAppProgram) + " beside this Workshop (" + asked.program +
                      "): an app's launchers start it");
    }

    const view::Description& d = view_admitted.description;
    ZengineAppFile app;
    app.name = out.name;
    app.window.title = out.name;
    const std::int64_t line = 2 * graphics::kCellPx;
    app.window.width = d.width + 2 * kViewInset;
    app.window.height = d.height + 2 * kViewInset +
                        static_cast<std::int64_t>(view::kNoticeRows + 1) * line;
    app.window.skin = kExportSkin;
    app.views.push_back(ZengineAppView{out.name + ".view", d.name, kViewInset, kViewInset});
    app.flows.push_back(ZengineAppFlow{out.name + ".flow", flow_name});
    out.app_text = loom::compat::serialize(loom::to_value(app));
    if (const ReadApp back = app_from_text(out.app_text); !back.ok) {
        return refuse(back.refusal);
    }

    const workshop::load::LoadPlan plan = export_plan();
    for (const auto& row : plan.artifacts) {
        out.stems.push_back(row.stem);
    }
    out.plan_text = workshop::load_persist::to_text(plan);
    out.view_bytes = view_read.bytes;
    out.flow_bytes = flow_read.bytes;
    out.launcher_cmd = launcher_cmd(out.name, asked.program);
    out.launcher_sh = launcher_sh(asked.program);
    const fs::path folder = fs::path(asked.project) / kAppsFolder / out.name;
    out.folder = folder.generic_string();
    out.exists = fs::exists(folder, ec);
    if (out.exists && !fs::is_directory(folder, ec)) {
        return refuse(out.folder + " is there and is not a folder");
    }
    out.ok = true;
    return out;
}

/// WRITE WHAT AN EXPORT JUDGED: the folder made if it is not there, then each of the export's
/// files, written whole or not at all (`maker::write_file`); every other file in the folder is
/// left as it is.
inline Exported write_export(const ExportJudged& judged) {
    namespace fs = std::filesystem;
    Exported out;
    if (!judged.ok) {
        out.refusal = judged.refusal;
        return out;
    }
    std::error_code ec;
    fs::create_directories(fs::path(judged.folder), ec);
    if (ec) {
        out.refusal = "cannot make " + judged.folder + ": " + ec.message();
        return out;
    }
    const std::vector<std::pair<std::string, const std::string*>> files = {
        {kPlanName, &judged.plan_text},
        {kAppFileName, &judged.app_text},
        {judged.name + ".view", &judged.view_bytes},
        {judged.name + ".flow", &judged.flow_bytes},
        {kLauncherCmd, &judged.launcher_cmd},
        {kLauncherSh, &judged.launcher_sh}};
    for (const auto& [name, bytes] : files) {
        const std::string path = judged.folder + "/" + name;
        if (const std::string error = maker::write_file(path, *bytes); !error.empty()) {
            out.refusal = error;
            return out;
        }
        out.written.push_back(name);
    }
    // ...and the shell launcher made runnable where the file system keeps that.
    fs::permissions(fs::path(judged.folder) / kLauncherSh,
                    fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec,
                    fs::perm_options::add, ec);
    out.ok = true;
    return out;
}

} // namespace zengine::app

#endif // ZENGINE_APP_FOLDER_HPP
