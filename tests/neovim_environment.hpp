// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_TESTS_NEOVIM_ENVIRONMENT_HPP
#define ZENGINE_TESTS_NEOVIM_ENVIRONMENT_HPP

// THE NEOVIM A CASE'S EDITOR WILL START, and where that Neovim keeps its state: the program, the
// profile and the XDG directories are set for the case and put back after it, so no case reads or
// writes the maker's own Neovim. Shared by the Neovim-backed Editor's suite files.

#include <cstdlib>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

inline void set_env(const char* name, const std::string& value) {
#if defined(_WIN32)
    (void)_putenv_s(name, value.c_str());
#else
    (void)setenv(name, value.c_str(), 1);
#endif
}

inline void unset_env(const char* name) {
#if defined(_WIN32)
    (void)_putenv_s(name, "");
#else
    (void)unsetenv(name);
#endif
}

/// THE NEOVIM THIS CASE'S EDITOR WILL START, and where that Neovim keeps its state -- set for the
/// case, and every variable put back as it was when the case ends.
class NeovimEnvironment {
public:
    NeovimEnvironment(const std::filesystem::path& root, const std::string& program, const char* fixture_mode = nullptr) {
        keep("ZENGINE_NEOVIM", program);
        keep("ZENGINE_NEOVIM_PROFILE", "clean");
        keep("ZENGINE_NEOVIM_FIXTURE_MODE", fixture_mode != nullptr ? fixture_mode : "");
        for (const char* v : {"XDG_CONFIG_HOME", "XDG_DATA_HOME", "XDG_STATE_HOME", "XDG_CACHE_HOME",
                              "XDG_RUNTIME_DIR", "LOCALAPPDATA"}) {
            const std::filesystem::path d = root / "neovim-env" / v;
            std::filesystem::create_directories(d);
            keep(v, d.string());
        }
        keep("NVIM_LOG_FILE", (root / "neovim-env" / "nvim.log").string());
    }
    ~NeovimEnvironment() {
        for (auto it = saved_.rbegin(); it != saved_.rend(); ++it) {
            if (it->second.has_value()) {
                set_env(it->first.c_str(), *it->second);
            } else {
                unset_env(it->first.c_str());
            }
        }
    }
    NeovimEnvironment(const NeovimEnvironment&) = delete;
    NeovimEnvironment& operator=(const NeovimEnvironment&) = delete;

private:
    void keep(const char* name, const std::string& value) {
        const char* was = std::getenv(name);
        saved_.emplace_back(name, was != nullptr ? std::optional<std::string>(was) : std::nullopt);
        if (value.empty()) {
            unset_env(name);
        } else {
            set_env(name, value);
        }
    }
    std::vector<std::pair<std::string, std::optional<std::string>>> saved_;
};

#endif // ZENGINE_TESTS_NEOVIM_ENVIRONMENT_HPP
