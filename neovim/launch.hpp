// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_NEOVIM_LAUNCH_HPP
#define ZENGINE_NEOVIM_LAUNCH_HPP

// Which Neovim, with which configuration, and how it is spoken to (WL-NVIM-09): two environment
// variables, read when a Neovim starts. `ZENGINE_NEOVIM` is the program, a path or a name on
// PATH (default `nvim`); `ZENGINE_NEOVIM_PROFILE` is `clean` (the default: `--clean` under
// NVIM_APPNAME `zengine-neovim-clean`, so a maker's own setup is neither read nor written),
// `user` (their own startup and plugins, opted into), or an init file (`-u <path>`).
// Workshop law: agents/workshop/neovim.md

// The program is never a message: no shape, poke or plan row can name what runs. Inside Workshop
// the owner attaches as Neovim's UI over the child's pipes (`--embed`); from a baseline Loom
// Neovim runs headless and listens (`--embed --headless --listen <address>`), and the maker
// attaches from a second terminal with `nvim --server <address> --remote-ui`.

#include "neovim/child.hpp"

#include <cstdlib>
#include <filesystem>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace zengine::neovim {

inline constexpr const char* kProgramVariable = "ZENGINE_NEOVIM";
inline constexpr const char* kProfileVariable = "ZENGINE_NEOVIM_PROFILE";
inline constexpr const char* kCleanAppName = "zengine-neovim-clean";

enum class UiMode : std::uint8_t {
    Embedded, ///< the owner attaches as the UI (Workshop's pane)
    Remote,   ///< headless and listening; a maker attaches `--remote-ui` (baseline Loom)
};

/// WHAT THE ENVIRONMENT CHOSE.
struct LaunchChoice {
    std::string program = "nvim";
    std::string profile = "clean";
};

inline LaunchChoice choice_from_environment() {
    LaunchChoice out;
    if (const char* p = std::getenv(kProgramVariable); p != nullptr && *p != '\0') {
        out.program = p;
    }
    if (const char* p = std::getenv(kProfileVariable); p != nullptr && *p != '\0') {
        out.profile = p;
    }
    return out;
}

/// What the profile names, judged before any Neovim starts: `clean` and `user`, or an init file.
/// A relative file resolves against the directory this process started in, where the maker set
/// the variable, never Neovim's working directory. A name that is neither spelling nor a file is
/// refused here: handed to Neovim as `-u <name>` it prints E282 into a prompt and runs with no
/// configuration at all (measured on 0.11.6), a third configuration nobody chose.
struct ProfileChoice {
    bool ok = true;
    std::string refusal;  ///< why not, naming the variable and where the file was looked for
    std::string resolved; ///< the absolute init file, for a file profile; empty for the two words
};

inline ProfileChoice check_profile(const LaunchChoice& c) {
    ProfileChoice out;
    if (c.profile == "clean" || c.profile == "user") {
        return out;
    }
    std::error_code where;
    const std::filesystem::path named(c.profile);
    const std::filesystem::path at =
        named.is_absolute() ? named : std::filesystem::current_path(where) / named;
    std::error_code is;
    if (!where && std::filesystem::is_regular_file(at, is)) {
        out.resolved = at.generic_string();
        return out;
    }
    out.ok = false;
    out.refusal = std::string(kProfileVariable) + " is `" + c.profile +
                  "`, which is neither `clean` nor `user` and names no init file (looked for " +
                  at.generic_string() + ") -- Neovim was not started";
    return out;
}

/// THE PROFILE IN ONE WORD, for a status row a maker reads while Neovim runs.
inline std::string profile_tag(const LaunchChoice& c) {
    if (c.profile == "clean" || c.profile == "user") {
        return c.profile;
    }
    return "init file";
}

/// The profile's words, for status and refusals.
inline std::string profile_words(const LaunchChoice& c) {
    if (c.profile == "clean") {
        return "clean (no user configuration)";
    }
    if (c.profile == "user") {
        return "user (the maker's own configuration)";
    }
    return "init file " + c.profile;
}

/// THE COMMAND LINE AND ENVIRONMENT for one Neovim.
inline LaunchSpec launch_spec(const LaunchChoice& choice, UiMode mode, const std::string& listen,
                              const std::string& cwd) {
    LaunchSpec spec;
    spec.program = choice.program;
    spec.cwd = cwd;
    spec.args.push_back("--embed");
    if (mode == UiMode::Remote) {
        spec.args.push_back("--headless");
        if (!listen.empty()) {
            spec.args.push_back("--listen");
            spec.args.push_back(listen);
        }
    }
    if (choice.profile == "clean") {
        spec.args.push_back("--clean");
        spec.set_env.emplace_back("NVIM_APPNAME", kCleanAppName);
    } else if (choice.profile != "user") {
        spec.args.push_back("-u");
        spec.args.push_back(choice.profile);
    }
    // A NEOVIM STARTED FROM INSIDE ANOTHER NEOVIM'S TERMINAL inherits `$NVIM`, which names the
    // parent's server; nothing here is that parent's child in any sense that matters.
    spec.unset_env.push_back("NVIM");
    spec.unset_env.push_back("NVIM_LISTEN_ADDRESS");
    return spec;
}

} // namespace zengine::neovim

#endif // ZENGINE_NEOVIM_LAUNCH_HPP
