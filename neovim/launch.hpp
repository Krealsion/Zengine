// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_NEOVIM_LAUNCH_HPP
#define ZENGINE_NEOVIM_LAUNCH_HPP

// WHICH NEOVIM, WITH WHICH CONFIGURATION, AND HOW IT IS SPOKEN TO.
//
// TWO ENVIRONMENT VARIABLES AND NOTHING ELSE, read when a Neovim is started:
//
//     ZENGINE_NEOVIM          the program: a path, or a name searched on PATH (default `nvim`)
//     ZENGINE_NEOVIM_PROFILE  `clean` (the default), `user`, or the path of an init file
//
//   clean   `--clean`, under NVIM_APPNAME `zengine-neovim-clean`: no user configuration, no
//           plugins, no shada, and swap files and logs kept apart from the maker's own Neovim.
//           A maker's personal setup is neither read nor written.
//   user    the maker's own startup, configuration and plugins, exactly as `nvim` would run --
//           opted into, never assumed.
//   <path>  `-u <path>`: that init file and nothing else from the user's configuration.
//
// ⚠ THE PROGRAM IS NEVER A MESSAGE. No shape, poke or plan row can name what runs; a maker names
// it in the environment of the process that hosts Neovim, the same place their PATH lives.
//
// TWO WAYS TO BE SPOKEN TO. Inside Workshop the owner attaches as Neovim's UI over the child's own
// pipes (`--embed`). From a baseline Loom there is no pane to draw into, so Neovim runs headless
// and listens (`--embed --headless --listen <address>`), and the maker attaches Neovim's own
// interface from a second terminal with `nvim --server <address> --remote-ui`.

#include "neovim/child.hpp"

#include <cstdlib>
#include <string>
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
