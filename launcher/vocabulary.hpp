// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_LAUNCHER_VOCABULARY_HPP
#define ZENGINE_LAUNCHER_VOCABULARY_HPP

// The launcher's words (launcher/docs/zengine.launcher.md): what may be started is a kind and a
// name in a catalog fixed when Zengine was configured, never a program, a path or an argument.
// Portable: no kernel, no loader.

#include <zen/weave/shape.hpp>

#include <cstdint>
#include <string>

namespace zengine::launcher {

/// The launcher's office.
inline constexpr const char* kLauncherRole = "zengine.launcher";

/// The kinds the catalog holds: an app in the project's `apps/` folder, started by the
/// `zengine-app` beside Workshop; and a demo, a setup the demo service starts.
inline constexpr const char* kKindApp = "app";
inline constexpr const char* kKindDemo = "demo";

/// The longest name a launch may give, in bytes.
inline constexpr std::size_t kMaxLaunchName = 64;

/// What an app the launcher started does next on this platform, as a start's notice says it: on
/// Windows it has a console of its own; on POSIX it reads none, so a row of its plan no console
/// has decided ends it, and its `app.log` says which.
#if defined(_WIN32)
inline constexpr const char* kStartedAppGoesOn =
    "its console asks what it may run, and it goes on without Workshop";
#else
inline constexpr const char* kStartedAppGoesOn =
    "it goes on without Workshop, with no console: an undecided row ends it, said in its app.log; "
    "run.sh in a terminal decides it";
#endif

/// An office -> `zengine.launcher`: start the catalog's `kind` named `name`. Answered `Launched`.
struct Launch {
    std::string kind;
    std::string name;
    ZEN_SHAPE(Launch, 1, ZEN_FIELD(kind), ZEN_FIELD(name));
};

/// `zengine.launcher` -> the asker: the process started, its id, or why nothing was. Started is
/// not running: the process is its own from then on, and the launcher keeps nothing of it.
struct Launched {
    bool ok = false;
    std::string kind;
    std::string name;
    std::int64_t pid = 0;
    std::string reason;
    ZEN_SHAPE(Launched, 1, ZEN_FIELD(ok), ZEN_FIELD(kind), ZEN_FIELD(name), ZEN_FIELD(pid),
              ZEN_FIELD(reason));
};

} // namespace zengine::launcher

#endif // ZENGINE_LAUNCHER_VOCABULARY_HPP
