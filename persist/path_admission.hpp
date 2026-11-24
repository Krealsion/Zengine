// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_PERSIST_PATH_ADMISSION_HPP
#define ZENGINE_PERSIST_PATH_ADMISSION_HPP

// Taking a filesystem path into the only path vocabulary this application has: a `std::string`
// spelling. Asking a path for its bytes can throw, and this header alone asks (WL-FILES-11, in
// Workshop's Files register): everything else holds the spelling it admitted.

#include <exception>
#include <filesystem>
#include <string>
#include <system_error>
#include <type_traits>

namespace zengine::persist {

/// Whether a filesystem path could be carried, and what it became if it could.
struct AdmittedPath {
    /// Did the platform spell this path in Workshop's narrow path vocabulary?
    bool carried = false;
    /// The spelling, when it did. Empty when it did not -- never a guess, never a
    /// neighbouring path, never a lossy approximation of the one that was asked for.
    std::string spelling;
};

/// The one answer to "can this filesystem path enter Workshop?": carried, with its generic
/// spelling, or not carried when the platform will not narrow it.
inline AdmittedPath admit_path(const std::filesystem::path& p) noexcept {
    AdmittedPath out;
    try {
        out.spelling = p.generic_string();
        out.carried = true;
    } catch (...) {
        // The platform will not narrow this path. That is the answer, not a failure to
        // report upward: `filesystem_error::what()` names an encoding, which is true and
        // is not what any caller here has to say to a weaver.
        out.spelling.clear();
        out.carried = false;
    }
    return out;
}

/// A path spelling this application wrote down earlier, coming back in. It returns the spelling
/// absolute, lexically normal and carried, or empty when it is not absolute or cannot be carried;
/// every durable spelling that comes back in goes through it (WL-FILES-09).
inline std::string admit_location(const std::string& spelling) noexcept {
    if (spelling.empty()) {
        return std::string();
    }
    try {
        std::filesystem::path p = std::filesystem::path(spelling).lexically_normal();
        if (!p.is_absolute()) {
            return std::string();
        }
        // One location has one spelling. `lexically_normal` keeps a trailing separator, the
        // spelling's own or one left where a final `..` was resolved, and everything downstream
        // compares locations by bytes, so it is dropped. A root keeps its separator, which there
        // is the whole path.
        if (p.filename().empty() && p != p.root_path()) {
            p = p.parent_path();
        }
        const AdmittedPath carried = admit_path(p);
        return carried.carried ? carried.spelling : std::string();
    } catch (...) {
        // The platform will not make a path out of these bytes. That is the answer.
        return std::string();
    }
}

/// One directory entry's name, and whether the bytes are the filesystem's own.
struct AdmittedName {
    /// The bytes this row holds: the filesystem's own UTF-8 when `exact`, and a
    /// `?`-per-unit projection of what the platform reported when not.
    std::string name;
    /// True when `name` is the filesystem's own bytes. False means the platform refused to spell
    /// this name at all, and these bytes are a projection: they say that something is there and
    /// roughly what it looks like, and they name no file.
    bool exact = true;
};

/// Takes one entry's file name: its UTF-8 bytes, exact, or when the platform will not convert it,
/// a projection with one `?` for each unit that is not printable ASCII. A name is held as
/// `std::string` bytes everywhere (WL-FILES-10).
inline AdmittedName admit_filename(const std::filesystem::path& filename) noexcept {
    AdmittedName out;
    try {
        const std::u8string u8 = filename.u8string();
        out.name.assign(reinterpret_cast<const char*>(u8.data()), u8.size());
        return out;
    } catch (...) {
        out.name.clear();
    }
    out.exact = false;
    try {
        // `native()` is the stored string and cannot throw: it is the conversion that refuses, so
        // what the platform reported is still readable one unit at a time.
        using Unit = std::filesystem::path::value_type;
        const std::basic_string<Unit>& native = filename.native();
        out.name.reserve(native.size());
        for (const Unit unit : native) {
            const std::make_unsigned_t<Unit> u = static_cast<std::make_unsigned_t<Unit>>(unit);
            out.name.push_back(u >= 0x20 && u <= 0x7E ? static_cast<char>(u) : '?');
        }
    } catch (...) {
        out.name.clear();
    }
    if (out.name.empty()) {
        // Nothing at all could be said about this entry's name. It is still there, so it
        // still gets a row: one mark, which is what every other unsayable unit becomes.
        out.name = "?";
    }
    return out;
}

/// Where the weaver is standing: the process's directory at launch, captured once by the host as
/// the project (WL-PROJ-01), or empty when it cannot be read or carried.

inline std::string launch_project_dir() {
    std::error_code cwd_ec;
    const std::filesystem::path cwd = std::filesystem::current_path(cwd_ec);
    if (cwd_ec) {
        return std::string();
    }
    const AdmittedPath admitted = admit_path(cwd);
    if (!admitted.carried) {
        return std::string();
    }
    return admitted.spelling;
}

} // namespace zengine::persist

#endif // ZENGINE_PERSIST_PATH_ADMISSION_HPP
