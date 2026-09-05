// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_MAKER_FILES_HPP
#define ZENGINE_MAKER_FILES_HPP

// THE TWO FILES ON DISK, read and written under the durable discipline the Workshop's
// persistence owns (workshop/persist.hpp): a read is size-capped before it is opened, so a
// file that is too large to be what it claims is refused rather than slurped; a write lands
// in a sibling and is renamed over the target, so the last good file is never at risk.
//
// RESTATED, NOT SHARED, on purpose: this package does not depend on workshop/, and the two
// functions are forty lines. A third owner of the same discipline is the trigger to hoist it.

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>

namespace zengine::maker {

/// The largest file this package will read as a definition or a state. Both are small by
/// construction -- a definition is a handful of descriptors and a graph, a state is one
/// value -- so a megabyte is generous and still a bound.
inline constexpr std::uintmax_t kMaxFileBytes = 1u << 20;

/// What reading a file produced.
struct FileBytes {
    bool ok = false;
    std::string reason;
    std::string bytes;
    explicit operator bool() const noexcept { return ok; }
};

/// The whole file, or why not: not there, too big, unreadable.
inline FileBytes read_file(const std::string& path, std::uintmax_t most = kMaxFileBytes) {
    FileBytes out;
    std::error_code ec;
    const std::uintmax_t size = std::filesystem::file_size(path, ec);
    if (ec) {
        out.reason = "cannot read " + path + ": " + ec.message();
        return out;
    }
    if (size > most) {
        out.reason = "cannot read " + path + ": " + std::to_string(size) +
                     " bytes is larger than a maker file can be";
        return out;
    }
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        out.reason = "cannot read " + path;
        return out;
    }
    out.bytes.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    if (in.bad()) {
        out.reason = "cannot read " + path + ": the read failed part way";
        out.bytes.clear();
        return out;
    }
    out.ok = true;
    return out;
}

/// Write bytes WITHOUT putting the last good file at risk: the complete candidate lands in
/// `<path>.saving`, then is renamed over `<path>`. Empty string on success, else the reason.
inline std::string write_file(const std::string& path, const std::string& bytes) {
    const std::string pending = path + ".saving";
    {
        std::ofstream out(pending, std::ios::binary | std::ios::trunc);
        if (!out) {
            return "cannot write " + pending;
        }
        out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        out.close();
        if (!out) {
            std::error_code drop;
            std::filesystem::remove(pending, drop);
            return "could not finish writing " + pending + " -- " + path + " is unchanged";
        }
    }
    std::error_code ec;
    std::filesystem::rename(pending, path, ec);
    if (ec) {
        std::error_code drop;
        std::filesystem::remove(pending, drop);
        return "cannot replace " + path + ": " + ec.message() + " -- it is unchanged";
    }
    return std::string();
}

} // namespace zengine::maker

#endif // ZENGINE_MAKER_FILES_HPP
