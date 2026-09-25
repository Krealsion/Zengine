// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_PERSIST_HPP
#define ZENGINE_WORKSHOP_PERSIST_HPP

// The file doors every durable artifact shares: one safe write, one bounded read, and one way a
// relative spelling is resolved.
// Workshop law: agents/workshop/document-file.md (+3 registers; agents/workshop.md routes)

#include "property.hpp" // `Written`

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace zengine::workshop::persist {

/// The name the retired object document was saved under: kept so a launch can say that a file
/// under it is left exactly as it is (`workshop.cpp`), and never read as anything else.
inline constexpr const char* kRetiredDocumentName = "workshop.json";

// ---- The file itself -------------------------------------------------------

/// ONE SPELLING OF A PATH, RESOLVED AGAINST THE PLACE IT WAS MEANT RELATIVE TO.
// WL-EDIT-06 -- agents/workshop/editor.md; WL-PROJ-02 -- agents/workshop/project.md
inline std::string resolved_against(const std::string& base, const std::string& spelling) {
    if (spelling.empty()) {
        return spelling;
    }
    std::filesystem::path p(spelling);
    if (p.is_absolute()) {
        return p.lexically_normal().generic_string();
    }
    if (base.empty()) {
        return spelling;
    }
    return (std::filesystem::path(base) / p).lexically_normal().generic_string();
}

/// What reading a file produced.
struct FileText {
    Written outcome;
    std::string text;
};

/// The whole file, or why not. Refusals are the ordinary ones a maker meets:
/// the file is not there, it cannot be opened, it is too big to be what it
/// claims to be.
// WL-DOC-15 -- agents/workshop/document-file.md
inline FileText read_file(const std::string& path, std::uintmax_t most, const char* what) {
    std::error_code ec;
    const std::uintmax_t size = std::filesystem::file_size(path, ec);
    if (ec) {
        return FileText{Written::no("cannot read " + path + ": " + ec.message()), {}};
    }
    if (size > most) {
        return FileText{Written::no("cannot read " + path + ": " + std::to_string(size) +
                                    " bytes is larger than " + what + " can be"),
                        {}};
    }
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return FileText{Written::no("cannot read " + path), {}};
    }
    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (in.bad()) {
        return FileText{Written::no("cannot read " + path + ": the read failed part way"), {}};
    }
    return FileText{Written::ok(), std::move(text)};
}

/// Where the complete candidate is written before it becomes the document.
inline std::string pending_path(const std::string& path) { return path + ".saving"; }

/// Write text to a file WITHOUT putting the last good save at risk.
// WL-SESSION-13 -- agents/workshop/session.md; WL-DOC-15 -- agents/workshop/document-file.md
inline Written write_file(const std::string& path, const std::string& text) {
    const std::string pending = pending_path(path);
    {
        std::ofstream out(pending, std::ios::binary | std::ios::trunc);
        if (!out) {
            return Written::no("cannot write " + pending);
        }
        out.write(text.data(), static_cast<std::streamsize>(text.size()));
        out.close();
        if (!out) {
            std::error_code drop;
            std::filesystem::remove(pending, drop);
            return Written::no("could not finish writing " + pending +
                               " -- " + path + " is unchanged");
        }
    }
    std::error_code ec;
    std::filesystem::rename(pending, path, ec);
    if (ec) {
        std::error_code drop;
        std::filesystem::remove(pending, drop);
        return Written::no("cannot replace " + path + ": " + ec.message() +
                           " -- it is unchanged");
    }
    return Written::ok();
}

/// The same safe write, into a directory that may not exist yet: the per-user roots are created on
/// first write (the session, the prefs, the legacy import). Project files never are: a missing
/// directory there is a maker's typo, refused loudly.
// WL-SESSION-18 -- agents/workshop/session.md
inline Written write_file_making_room(const std::string& path, const std::string& text) {
    const std::filesystem::path parent = std::filesystem::path(path).parent_path();
    if (!parent.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(parent, ec);
        if (ec) {
            return Written::no("cannot create " + parent.string() + ": " + ec.message());
        }
    }
    return write_file(path, text);
}

} // namespace zengine::workshop::persist

#endif // ZENGINE_WORKSHOP_PERSIST_HPP
