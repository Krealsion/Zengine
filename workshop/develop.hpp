// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_DEVELOP_HPP
#define ZENGINE_WORKSHOP_DEVELOP_HPP

// THE DEVELOPMENT LAUNCH: how a maker working on Workshop's own panes starts the Workshop they
// work in, from outside it. `zengine-workshop-develop` (develop.cpp) is this file with the facts a
// configured build tree compiled in; CLion's shared "Develop Workshop" run configuration names that
// target, and a shell runs the same executable. Law: WL-CODE-08, agents/workshop/code.md.
//
// ONE ORDER, AND EVERY STEP CAN END IT WITH NOTHING STARTED. The tree must have staged a graphical
// plan; the runtime's host must not already be running; the runtime script must say the runtime is
// ready -- made now, or this tree's and still current; the project directory must be there or be
// made. Only then does the runtime's own host start, with the runtime's graphical plan and
// development catalog, in the project directory. The build tree's host is never started, and a
// refused step is never followed by a second guess.

#include "builder/recipe.hpp"
#include "builder/run.hpp"

#include <cerrno>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <string>
#include <system_error>
#include <vector>

#if !defined(_WIN32)
#include <fcntl.h>
#include <unistd.h>
#endif

namespace zengine::workshop::develop {

/// What configuration wrote down for this launch: the machine's own facts, compiled in by
/// `workshop/CMakeLists.txt` and never authored, so the shared run configuration names no path.
struct Facts {
    std::string cmake;   ///< the CMake that configured the build tree
    std::string script;  ///< that tree's generated development-runtime.cmake
    std::string build;   ///< the build tree: said, and never launched from
    std::string runtime; ///< the runtime directory when none is named
    std::string project; ///< the project directory when none is named
    std::string host;    ///< the host executable's file name
    std::string plan;    ///< the graphical load plan's file name; empty where none was staged
    std::string catalog; ///< the development catalog's file name
};

/// What one launch was asked for: the facts' two directories, or the ones a maker named.
struct Choice {
    bool ok = true;
    std::string complaint;
    std::string runtime;
    std::string project;
};

inline constexpr const char* kUsage =
    "usage: zengine-workshop-develop [--runtime <dir>] [--project <dir>]";

/// A named directory, absolute and without a trailing separator: the host starts in the project
/// directory, so a spelling relative to where the launch stands would name somewhere else there.
inline std::string absolute_directory(const std::string& named, std::string& trouble) {
    std::error_code ec;
    std::filesystem::path path = std::filesystem::absolute(named, ec);
    if (ec) {
        trouble = "cannot be made absolute: " + ec.message();
        return std::string();
    }
    path = path.lexically_normal();
    if (path.has_relative_path() && path.filename().empty()) {
        path = path.parent_path();
    }
    return path.generic_string();
}

// WL-CODE-08 -- agents/workshop/code.md
inline Choice choose(const std::vector<std::string>& args, const Facts& facts) {
    Choice choice;
    choice.runtime = facts.runtime;
    choice.project = facts.project;
    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string& arg = args[i];
        if (arg != "--runtime" && arg != "--project") {
            choice.ok = false;
            choice.complaint = "unknown argument `" + arg + "`";
            return choice;
        }
        if (i + 1 >= args.size() || args[i + 1].empty()) {
            choice.ok = false;
            choice.complaint = arg + " needs a directory";
            return choice;
        }
        std::string trouble;
        const std::string dir = absolute_directory(args[++i], trouble);
        if (!trouble.empty()) {
            choice.ok = false;
            choice.complaint = arg + " " + args[i] + " " + trouble;
            return choice;
        }
        (arg == "--runtime" ? choice.runtime : choice.project) = dir;
    }
    return choice;
}

/// The runtime script, asked about one directory: it makes the runtime, reuses it, or refuses.
// WL-CODE-08 -- agents/workshop/code.md
inline builder::BuildCommand runtime_command(const Facts& facts, const std::string& runtime) {
    builder::BuildCommand command;
    command.program = facts.cmake;
    command.args = {"-DZEN_RUNTIME=" + runtime, "-P", facts.script};
    return command;
}

/// The runtime's own host, with the runtime's graphical plan and development catalog, started in
/// the project directory.
// WL-CODE-08 -- agents/workshop/code.md
inline builder::BuildCommand host_command(const Facts& facts, const Choice& choice) {
    builder::BuildCommand command;
    command.program = choice.runtime + "/" + facts.host;
    command.args = {"--load-plan", choice.runtime + "/" + facts.plan, "--recipes",
                    choice.runtime + "/" + facts.catalog};
    command.dir = choice.project;
    return command;
}

/// IS THIS FILE AN IMAGE A RUNNING PROGRAM HOLDS? Asked by opening it for writing and closing it
/// again, which writes nothing. Windows answers a running image with a sharing violation, and a
/// Linux that denies writes to a running executable answers ETXTBSY; every other answer --
/// absent, opened, refused for another reason -- is "no", because this is asked only to refuse a
/// second launch.
// WL-CODE-08 -- agents/workshop/code.md
inline bool image_in_use(const std::string& path) {
#if defined(_WIN32)
    const HANDLE file = ::CreateFileA(path.c_str(), GENERIC_WRITE,
                                      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                      nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return ::GetLastError() == ERROR_SHARING_VIOLATION;
    }
    ::CloseHandle(file);
    return false;
#else
    const int fd = ::open(path.c_str(), O_WRONLY | O_CLOEXEC);
    if (fd < 0) {
        return errno == ETXTBSY;
    }
    ::close(fd);
    return false;
#endif
}

/// The project directory, made when it is absent: empty when it is there, else why it is not.
inline std::string project_directory(const std::string& dir) {
    std::error_code ec;
    if (std::filesystem::is_directory(dir, ec)) {
        return std::string();
    }
    if (std::filesystem::exists(dir, ec)) {
        return "is there and is not a directory";
    }
    std::filesystem::create_directories(dir, ec);
    if (ec) {
        return "could not be made: " + ec.message();
    }
    return std::string();
}

/// What a launch needs from the world. `main` hands it the real doors; a suite hands it doors of
/// its own and reads what was asked of them.
struct World {
    std::function<bool(const std::string&)> in_use;
    std::function<builder::RunResult(const builder::BuildCommand&)> run;
    std::function<std::string(const std::string&)> directory;
    std::function<void(const std::string&)> say;
};

// WL-CODE-08 -- agents/workshop/code.md
inline int launch(const Facts& facts, const Choice& choice, const World& world) {
    if (!choice.ok) {
        world.say(choice.complaint + " -- nothing was prepared or launched");
        world.say(kUsage);
        return 2;
    }
    if (facts.plan.empty()) {
        world.say("the build tree " + facts.build + " staged no graphical load plan (it was "
                  "configured without the SDL skin) -- nothing was prepared or launched");
        return 1;
    }
    const builder::BuildCommand host = host_command(facts, choice);
    world.say("build tree: " + facts.build);
    world.say("runtime: " + choice.runtime);
    if (world.in_use(host.program)) {
        world.say(host.program + " is running: a Workshop launched from this runtime is still "
                  "open. Quit it from Workshop, then launch again -- nothing was prepared, "
                  "launched or stopped");
        return 1;
    }
    const builder::RunResult prepared = world.run(runtime_command(facts, choice.runtime));
    if (!prepared.started) {
        world.say("could not run " + facts.cmake + " to prepare the runtime (" + prepared.trouble +
                  ") -- nothing was launched");
        return 1;
    }
    if (prepared.status != 0) {
        world.say("the runtime was not prepared, for the reason above -- nothing was launched");
        return 1;
    }
    const std::string refused = world.directory(choice.project);
    if (!refused.empty()) {
        world.say("the project directory " + choice.project + " " + refused +
                  " -- nothing was launched");
        return 1;
    }
    world.say("project: " + choice.project + " (Workshop's project files)");
    world.say("launching " + host.as_line());
    const builder::RunResult ran = world.run(host);
    if (!ran.started) {
        world.say("the runtime's Workshop did not start: " + ran.trouble);
        return 1;
    }
    world.say("Workshop exited with " + std::to_string(ran.status));
    return static_cast<int>(ran.status);
}

} // namespace zengine::workshop::develop

#endif // ZENGINE_WORKSHOP_DEVELOP_HPP
