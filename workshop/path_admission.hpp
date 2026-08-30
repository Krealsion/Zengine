// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_PATH_ADMISSION_HPP
#define ZENGINE_WORKSHOP_PATH_ADMISSION_HPP

// GETTING A FILESYSTEM PATH INTO THE ONLY PATH VOCABULARY THIS APPLICATION HAS.
//
// Every path in Workshop is a `std::string`. That is a real and deliberate limit -- one
// spelling, comparable by bytes, printable in a character medium -- and it means there are
// filesystem paths this application cannot say. This file is the ONE place that decides
// whether a given path is one of them, and the whole of what it exists to guarantee is:
//
//   ASKING IS NEVER FATAL. `std::filesystem::path`'s narrowing accessors THROW -- measured
//   on MSVC, where `string()`, `generic_string()` and even `u8string()` convert through the
//   active code page and raise `filesystem_error` for a name that has no mapping in it. A
//   working directory outside the ACP took the whole process down out of `main`; one
//   ill-formed UTF-16 filename on an NTFS volume took the Files browser down mid-listing.
//   Both were the SAME sentence arriving from the SAME kind of call, and neither is a
//   condition a maker can be asked to avoid: the first is where they were standing, the
//   second is a name somebody else's program wrote.
//
// SO THE ANSWER IS A VALUE AND NEVER AN EXCEPTION. A path this application cannot carry is
// a REFUSAL -- explicit, at the boundary, in the caller's own words -- and never a crash and
// never a different path silently substituted. Which of those two the caller then spends is
// the caller's law and not this file's: an absent launch directory is an absence every
// consumer already refuses in words, and an uncarriable filename is a row that is shown,
// marked, and not activatable.
//
// WHAT THIS IS NOT, and the absences are load-bearing:
//
//   NOT A PATH TYPE.        Nothing here wraps, subclasses or replaces `std::string` or
//                           `std::filesystem::path`. The vocabulary is unchanged; what is
//                           new is that failing to enter it has an answer.
//   NOT A UNICODE LAYER.    Nothing re-encodes, transliterates or widens Workshop's path
//                           custody. A name this platform cannot narrow is still a name
//                           this application cannot open -- that limit is untouched here,
//                           and `files.hpp` still owns which names it admits.
//   NOT A FILESYSTEM SERVICE. Nothing here touches a disk. Every operation is a conversion
//                           over a value the caller already holds, so nothing can block,
//                           race, or need an `error_code` of its own.
//   NOT A NORMALIZER.       `lexically_normal` and the base-joining rule belong to
//                           `persist::resolved_against`, which already owns them. Admission
//                           converts and reports; it does not decide what a path MEANS.
//
// ⚠ THE REUSE TRIGGER. Anything that later needs to name a filesystem location a maker did
// not type -- a generated origin, a remembered place, a location a tool reports -- asks
// `admit_path` rather than calling a narrowing accessor of its own. A second `generic_string()`
// somewhere else is a second way for this process to die, and it will be found by the same
// hostile directory that found the first two.
//
// ...AND THE TRIGGER HAS A SECOND ARM SINCE LOCATIONS BECAME DURABLE. Turning a stored
// narrow spelling back INTO a path is a conversion in the other direction, and it refuses
// on the same platform for the same reason -- so a spelling read out of a file asks
// `admit_location` rather than constructing a `std::filesystem::path` of its own. The
// three doors are the whole vocabulary: a path from the OS, one entry's name from the OS,
// and a spelling from a file.

#include <exception>
#include <filesystem>
#include <string>
#include <system_error>
#include <type_traits>

namespace zengine::workshop {

/// WHETHER A FILESYSTEM PATH COULD BE CARRIED, AND WHAT IT BECAME IF IT COULD.
///
/// `carried` is the whole answer. A false `carried` is not an error to log and continue
/// past -- it is the fact that this application has no way to say that path, and the caller
/// owes its own sentence about what that means where it stands.
struct AdmittedPath {
    /// Did the platform spell this path in Workshop's narrow path vocabulary?
    bool carried = false;
    /// The spelling, when it did. Empty when it did not -- never a guess, never a
    /// neighbouring path, never a lossy approximation of the one that was asked for.
    std::string spelling;
};

/// THE ONE ANSWER TO "CAN THIS FILESYSTEM PATH ENTER WORKSHOP?".
///
/// Returns the same `generic_string()` spelling every path in this application has always
/// been -- forward separators, so `a/b` and `a\b` are one spelling on Windows and every
/// notice reads the same on both platforms -- and returns it as a VALUE that says whether
/// the conversion happened, rather than as a call that can end the process.
///
/// IT NORMALIZES NOTHING, on purpose. `lexically_normal`, the base-joining rule and the
/// absolute/relative decision are `persist::resolved_against`'s, and a second party folding
/// `..` would be a second answer to a question that has an owner. This converts.
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
///
/// THE OTHER DIRECTION OF THE SAME MEASURED THROW, and it needed an owner the moment a
/// durable file began carrying locations. `admit_path` above answers "can the platform
/// SPELL this path"; this answers "can this build still MAKE a path out of a spelling it
/// is handed" -- and on MSVC that conversion refuses too, because the narrow spelling is
/// read back through the active code page and a byte sequence that is not valid in it has
/// no path to become. A durable file is the one place such bytes can arrive from: every
/// spelling this application writes round-trips by construction, and a hand-edited one
/// need not.
///
/// A LOCATION IS ABSOLUTE, BY DEFINITION HERE. Every place this application remembers --
/// where a maker was standing, where they asked to be able to come back to -- is an
/// absolute one, because a relative spelling means whatever the process's own footing
/// happens to be at the moment somebody spends it, which is the defect
/// `persist::resolved_against` exists to end. So a relative spelling is not a location
/// this application could not carry; it is not a location at all, and both answers are
/// the same empty.
///
/// IT NORMALIZES, and that is the one thing it does beyond converting: a remembered place
/// is compared to other remembered places by its bytes, so two spellings of one directory
/// must not be able to become two marks. `lexically_normal` is the same fold
/// `resolved_against` applies, over text, touching no disk -- and nothing here
/// canonicalizes, so a location reached through a link stays the location the maker
/// walked to.
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
///
/// `name` is always something -- a browser that dropped a row would be lying about what is
/// in the directory. `exact` is what says whether those bytes may ever be spent as
/// IDENTITY.
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
///
/// The narrower wrapper `enumerate_directory` spends, and it is narrower for a reason the
/// two callers do not share: a launch directory that cannot be carried is an ABSENCE -- the
/// run has no project and every consumer already refuses in words -- while a filename that
/// cannot be carried is still a row, because the entry exists and hiding it would make this
/// browser lie about the directory. So this one always answers with a name.
///
/// THE PROJECTION IS THE PROJECTION `shown_name` ALREADY MAKES, one layer earlier. When the
/// platform hands over bytes, the loss is marked at paint time and the exact bytes are kept.
/// When it will not hand over bytes at all -- an ill-formed UTF-16 name on NTFS, measured --
/// there is nothing to keep, so the same `?`-per-unit marking is applied to what the
/// platform DID report and `exact` is false forever after.
///
/// ⚠ A PROJECTION CAN BE ENTIRELY PRINTABLE ASCII. `lone?.txt` passes every byte test a
/// carriable name passes, so `exact` is the ONLY thing standing between a projection and
/// being opened as a path. A caller that tests the bytes and forgets `exact` has written
/// the one outcome worse than a refusal: a path that names a different file, or no file.
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
///
/// The project has always been the launch directory -- one install serves two projects by
/// being launched in two places -- and this is the whole of capturing it. It is a function
/// rather than six lines in `main` because it is the composition that has to be PROVED
/// non-fatal, and a proof about a copy of it would be a proof about a copy.
///
/// TWO WAYS TO HAVE NO PROJECT, AND THEY ARE THE SAME ABSENCE. The platform may decline to
/// report a working directory at all (`current_path`'s `error_code` form, so no exception),
/// or it may report one this application cannot carry (measured on MSVC: a directory named
/// outside the active code page). Either way the answer is empty, which is the absence
/// every consumer already refuses in words, and which the host says once on its banner.
///
/// ⚠ THERE IS DELIBERATELY NO FALLBACK. Not the executable's directory -- that would hand
/// two projects one root, the quiet wrong answer WUX-3 removed from the maker's own files --
/// and not a parent, an ancestor or a temporary directory. A path this application cannot
/// say is a project it does not have, and saying so is the only honest answer available.
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
