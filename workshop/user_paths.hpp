// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_USER_PATHS_HPP
#define ZENGINE_WORKSHOP_USER_PATHS_HPP

// WHERE A MAKER'S OWN FILES LIVE WHEN THE HOST DOES NOT SAY OTHERWISE.
// Workshop law: agents/workshop/session.md

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>

#include "persist.hpp"

namespace zengine::workshop::user_paths {

/// The one directory name both roots end in -- the artifact's own identity.
inline constexpr const char* kAppDirName = "zengine-workshop";

/// THE ENVIRONMENT, AS A VALUE. The five variables the two platform spellings read,
/// captured once by the host (`host_environment`) and handed around as data -- so the
/// resolution rules are pure functions a suite can pin on every lane, and only one
/// function in this header can disagree between two runs.
struct Environment {
    std::string appdata;         ///< %APPDATA% (Windows roaming application data)
    std::string local_appdata;   ///< %LOCALAPPDATA% (Windows machine-local application data)
    std::string xdg_config_home; ///< $XDG_CONFIG_HOME
    std::string xdg_state_home;  ///< $XDG_STATE_HOME
    std::string home;            ///< $HOME (the XDG fallbacks hang off it)
};

/// The live process environment, read once. The only impure function here.
inline Environment host_environment() {
    const auto value = [](const char* name) {
        const char* v = std::getenv(name);
        return v != nullptr ? std::string(v) : std::string();
    };
    Environment env;
    env.appdata = value("APPDATA");
    env.local_appdata = value("LOCALAPPDATA");
    env.xdg_config_home = value("XDG_CONFIG_HOME");
    env.xdg_state_home = value("XDG_STATE_HOME");
    env.home = value("HOME");
    return env;
}

/// The Windows configuration root: %APPDATA%\zengine-workshop, or the absence.
inline std::string windows_config_root(const Environment& env) {
    return env.appdata.empty() ? std::string() : env.appdata + "/" + kAppDirName;
}

/// The Windows machine-local state root: %LOCALAPPDATA%\zengine-workshop, or the absence.
inline std::string windows_state_root(const Environment& env) {
    return env.local_appdata.empty() ? std::string() : env.local_appdata + "/" + kAppDirName;
}

/// The XDG configuration root: $XDG_CONFIG_HOME, else ~/.config, else the absence.
inline std::string xdg_config_root(const Environment& env) {
    if (!env.xdg_config_home.empty()) {
        return env.xdg_config_home + "/" + kAppDirName;
    }
    if (!env.home.empty()) {
        return env.home + "/.config/" + kAppDirName;
    }
    return std::string();
}

/// The XDG state root: $XDG_STATE_HOME, else ~/.local/state, else the absence.
inline std::string xdg_state_root(const Environment& env) {
    if (!env.xdg_state_home.empty()) {
        return env.xdg_state_home + "/" + kAppDirName;
    }
    if (!env.home.empty()) {
        return env.home + "/.local/state/" + kAppDirName;
    }
    return std::string();
}

/// This platform's two roots. The one `#if` in this header, and it selects between two
/// functions that exist and are pinned on every platform.
inline std::string config_root(const Environment& env) {
#if defined(_WIN32)
    return windows_config_root(env);
#else
    return xdg_config_root(env);
#endif
}

inline std::string state_root(const Environment& env) {
#if defined(_WIN32)
    return windows_state_root(env);
#else
    return xdg_state_root(env);
#endif
}

/// ONE PER-USER DURABLE FACT'S PATH, BY THE PINNED PRECEDENCE.
// WL-SESSION-01, WL-SESSION-02 -- agents/workshop/session.md
inline std::string resolve_durable_path(const std::string& explicit_path, bool isolated,
                                        const std::string& root, const char* default_name) {
    if (!explicit_path.empty()) {
        return explicit_path;
    }
    if (isolated) {
        return std::string();
    }
    if (root.empty()) {
        return std::string();
    }
    return root + "/" + default_name;
}

// ---- The one-time legacy import --------------------------------------------------------
// WL-SESSION-03 -- agents/workshop/session.md

/// What one file's transition did, in words a banner can print.
struct LegacyImport {
    bool imported = false;   ///< the legacy file's bytes now stand at the user root
    bool shadowed = false;   ///< a legacy file exists but the user root already has data
    std::string note;        ///< the human-readable sentence, empty when nothing happened
};

/// A ceiling for the import's read, spelled once: generous against every durable file's own
/// ceiling (largest is 64 KiB), tight against a hostile file chosen as a working directory
/// neighbour. The import moves bytes, not meaning, so the loaders' own ceilings still judge
/// the file at its new home.
inline constexpr std::uintmax_t kMaxImportBytes = 1u << 20;

/// Apply the transition rule for one durable fact. `destination` is the resolved per-user
/// DEFAULT path (the caller must not pass an explicit or isolated resolution here);
/// `legacy` is the older local file it replaces. `what` names the fact for the note
/// ("keymap", "session").
inline LegacyImport import_legacy_file(const std::string& destination,
                                       const std::string& legacy, const char* what) {
    LegacyImport result;
    if (destination.empty() || legacy.empty()) {
        return result;
    }
    std::error_code ec;
    if (!std::filesystem::exists(legacy, ec) || ec) {
        return result;
    }
    if (std::filesystem::exists(destination, ec) && !ec) {
        // The user root is authoritative. The legacy file is not read, not judged, and
        // not touched; the note says so, because a maker whose two files disagree is
        // entitled to know which one this Workshop is looking at.
        result.shadowed = true;
        result.note = std::string("your ") + what + " now lives at " + destination +
                      "; the older local " + legacy + " is not read (delete it to end this note)";
        return result;
    }
    const persist::FileText read = persist::read_file(legacy, kMaxImportBytes, what);
    if (!read.outcome.accepted) {
        result.note = std::string("found a local ") + what + " at " + legacy +
                      " but could not read it -- it was left untouched and nothing was imported";
        return result;
    }
    const Written written = persist::write_file_making_room(destination, read.text);
    if (!written.accepted) {
        result.note = std::string("found a local ") + what + " at " + legacy +
                      " but could not import it: " + written.refusal;
        return result;
    }
    result.imported = true;
    result.note = std::string("imported your local ") + what + " from " + legacy + " to " +
                  destination + " (the original was left in place)";
    return result;
}

} // namespace zengine::workshop::user_paths

#endif // ZENGINE_WORKSHOP_USER_PATHS_HPP
