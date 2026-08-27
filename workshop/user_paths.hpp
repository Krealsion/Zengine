// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_USER_PATHS_HPP
#define ZENGINE_WORKSHOP_USER_PATHS_HPP

// WHERE A MAKER'S OWN FILES LIVE WHEN THE HOST DOES NOT SAY OTHERWISE (WUX-3).
//
// Until WUX-3, every durable default resolved against the launch directory, which is the
// right answer for exactly two of the six durable artifacts -- the document and the named
// setup describe a PROJECT, and a project is where you are standing -- and the wrong answer
// for the maker's own facts: launch Workshop from two directories and you had two keymaps,
// two sessions, and no way to say "my settings". This header is the whole of the repair: the
// conventional per-user roots, the precedence that decides one path per durable fact, and
// the one-time import that keeps a maker's pre-WUX-3 local files from being silently
// abandoned.
//
// TWO ROOTS, BECAUSE THEY ARE TWO KINDS OF FACT.
//
//   CONFIGURATION   the maker's hand: keymap overrides, presentation preferences. Durable
//                   preference, meaningful on any machine this maker sits at.
//   STATE           the maker's machine-local session: the desk in use, the viewport, and
//                   (since WUX-3) the window's desktop placement. A viewport describes THIS
//                   machine's window and a placement describes THIS machine's monitors, so
//                   putting state under a roaming root would ship one desk's coordinates to
//                   every desk -- machine-local is correctness here, not convention.
//
// THE ROOTS ARE THE PLATFORM'S OWN CONVENTIONS, nothing invented:
//
//   Windows   config  %APPDATA%\zengine-workshop         (the roaming application-data root)
//             state   %LOCALAPPDATA%\zengine-workshop    (the machine-local one)
//   else      config  $XDG_CONFIG_HOME/zengine-workshop, falling back to ~/.config/...
//             state   $XDG_STATE_HOME/zengine-workshop,  falling back to ~/.local/state/...
//                     (the XDG state directory's own worked example is exactly this class)
//
// `zengine-workshop` IS THE ARTIFACT'S OWN NAME -- the binary this repository ships -- and
// deliberately not a development machine's path, a repository name, or a nested
// vendor/product pair: one flat directory that matches the identity a maker already sees in
// their process list. Both platform spellings are plain functions over an ENVIRONMENT VALUE
// rather than reads of the live environment, so every lane pins both shapes; only
// `host_environment()` touches the real process environment, and only the host calls it.
//
// AN UNRESOLVABLE ROOT IS AN ABSENCE, NEVER A FALLBACK TO CWD. A hostile or bare
// environment (no %APPDATA%, no $HOME) yields an empty root, which resolves to an empty
// path, which is the weave's designed "no persistence: restore nothing, write nothing"
// (HostContext's contract since WUX-0). Falling back to the launch directory instead would
// silently reintroduce the per-CWD behaviour this phase exists to end -- a quiet wrong
// answer wearing a helpful face. The host says the absence once, on its banner.
//
// PRECEDENCE, PINNED (WUX-3): for each per-user durable fact,
//
//   1. an explicit path the maker typed (`--keymap`, `--prefs`, `--session`) is that fact's
//      path, exactly as typed -- more specific than any policy;
//   2. `--isolated` makes the fact ABSENT: this run reads and writes none of the maker's
//      ordinary per-user configuration or session state;
//   3. otherwise the per-user default under the root above.
//
// `--isolated` exists because the defaults flip INVERTS an accident. Before WUX-3, a
// witness harness or executor run launched from a scratch directory was isolated by
// accident (its CWD files were scratch files); after the flip, that same unflagged launch
// would read and write the REAL maker's settings. Isolation must therefore be explicit and
// whole-application, and it ships in the same phase as the flip. Explicit paths still win
// over it, so an isolated witness that needs scratch persistence names its scratch files.
// Project files (`--document`, `--setup`) are untouched by all of this: they follow the
// project, which is the launch directory or the path the maker typed.

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
///
/// Empty is the designed absence at every step: an isolated run resolves to no file, and so
/// does a default whose root this environment cannot supply. The caller distinguishes the
/// two only for its banner -- the weave's behaviour at an empty path is identical, which is
/// what makes `--isolated` a promise rather than a convention.
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

// ---- The one-time legacy import (WUX-3's bounded transition) ---------------------------
//
// The pre-WUX-3 behaviour -- keymap and session resolved against the launch directory --
// is shipped behaviour, and makers may have real settings in it. The transition is ONE
// rule, applied per file, at startup, only for a path that resolved to the per-user
// DEFAULT (an explicit path is the maker's own answer and an isolated run touches nothing):
//
//   the user-root file does not exist  AND  the old local file does
//       -> copy the local file's bytes to the user root (creating the directory), and say so
//
//   anything else
//       -> nothing. An existing user-root file is authoritative and is NEVER overwritten by
//          a legacy file's presence; the legacy file itself is NEVER deleted, moved, or
//          rewritten -- it simply stops being read, and the maker is told so.
//
// The import CONVERGES BY EXISTENCE: once the destination exists -- imported, or written by
// an ordinary save -- the rule never fires again, so repeated launches cannot re-import or
// overwrite current user data. The copy goes through `persist::write_file` (complete
// sibling, then rename), so a torn import cannot leave a half-file standing where the
// destination should be. The bytes are copied unjudged: what a file MEANS is its own
// loader's law, and a legacy file the loader would refuse is refused in the same words at
// the new path -- with the original still in place.

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
/// `legacy` is the pre-WUX-3 local file it replaces. `what` names the fact for the note
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
