// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_PATH_ADMISSION_HPP
#define ZENGINE_WORKSHOP_PATH_ADMISSION_HPP

// GETTING A FILESYSTEM PATH INTO THE ONLY PATH VOCABULARY THIS APPLICATION HAS.
// Workshop law: agents/workshop/files.md (+1 registers; agents/workshop.md routes)

#include <exception>
#include <filesystem>
#include <string>
#include <system_error>
#include <type_traits>

namespace zengine::workshop {

/// WHETHER A FILESYSTEM PATH COULD BE CARRIED, AND WHAT IT BECAME IF IT COULD.
// WL-FILES-11 -- agents/workshop/files.md
struct AdmittedPath {
    /// Did the platform spell this path in Workshop's narrow path vocabulary?
    bool carried = false;
    /// The spelling, when it did. Empty when it did not -- never a guess, never a
    /// neighbouring path, never a lossy approximation of the one that was asked for.
    std::string spelling;
};

/// THE ONE ANSWER TO "CAN THIS FILESYSTEM PATH ENTER WORKSHOP?".
// WL-FILES-11 -- agents/workshop/files.md
inline AdmittedPath admit_path(const std::filesystem::path& p) noexcept {
    AdmittedPath out;
    try {
        out.spelling = p.generic_string();
        out.carried = true;
    } catch (...) {
        // The platform will not narrow this path. That is the answer, not a failure to
        // report upward: `filesystem_error::what()` names an encoding, which is true and
        // is not what any caller here has to say to a maker.
        out.spelling.clear();
        out.carried = false;
    }
    return out;
}

/// A PATH SPELLING THIS APPLICATION WROTE DOWN EARLIER, COMING BACK IN.
// WL-FILES-09, WL-FILES-11 -- agents/workshop/files.md
inline std::string admit_location(const std::string& spelling) noexcept {
    if (spelling.empty()) {
        return std::string();
    }
    try {
        std::filesystem::path p = std::filesystem::path(spelling).lexically_normal();
        if (!p.is_absolute()) {
            return std::string();
        }
        // ⚠ AND ONE LOCATION MUST HAVE ONE SPELLING, WHICH `lexically_normal` ALONE DOES NOT
        // GIVE. MEASURED on both families: a path whose last element is `..` normalizes WITH a
        // trailing separator (`/a/b/sub/..` -> `/a/b/`), so the same directory could arrive
        // here as `/a/b` from one caller and `/a/b/` from another. Everything downstream
        // compares locations by BYTES -- a mark is the maker's own place exactly when its
        // spelling matches, and the browser's badge appears exactly when origin's does -- so a
        // second spelling is a place that silently stops being the place it is. A root keeps
        // its separator, because there it is the whole path rather than a trailing one.
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

/// ONE DIRECTORY ENTRY'S NAME, AND WHETHER THE BYTES ARE THE FILESYSTEM'S OWN.
// WL-FILES-11 -- agents/workshop/files.md
struct AdmittedName {
    /// The bytes this row holds: the filesystem's own UTF-8 when `exact`, and a
    /// `?`-per-unit projection of what the platform reported when not.
    std::string name;
    /// True when `name` is the filesystem's own bytes. FALSE means the platform refused to
    /// spell this name at all and these bytes are a PROJECTION -- they say that something
    /// is there and roughly what it looks like, and they name no file.
    bool exact = true;
};

/// TAKE ONE ENTRY'S FILENAME, DEFENSIVELY.
// WL-FILES-10, WL-FILES-11 -- agents/workshop/files.md
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
        // `native()` is the stored string and cannot throw -- it is the CONVERSION that
        // refuses, so what the platform reported is still readable one unit at a time.
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

/// WHERE THE MAKER IS STANDING, or the designed absence.
// WL-FILES-11 -- agents/workshop/files.md; WL-PROJ-01 -- agents/workshop/project.md
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

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_PATH_ADMISSION_HPP
