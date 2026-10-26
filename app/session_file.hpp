// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_APP_SESSION_FILE_HPP
#define ZENGINE_APP_SESSION_FILE_HPP

// AN APP'S SESSION FILE, `app-session.json` in its folder (app/docs/app.md, "While it runs"): the
// app host writes its own process id there once it holds the app's decisions, and removes the file
// when it ends. A file that stands names a process that ran the app; whether that process still
// runs is the reader's to ask the system, since a host that was killed leaves its file behind.
// Portable: no kernel, no loader.

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <system_error>
#include <utility>

#if defined(_WIN32)
#include <process.h>
#else
#include <unistd.h>
#endif

namespace zengine::app {

/// The file the app host writes while it runs; an export never touches it.
inline constexpr const char* kSessionName = "app-session.json";

/// This process's id.
inline std::int64_t this_process_id() {
#if defined(_WIN32)
    return static_cast<std::int64_t>(::_getpid());
#else
    return static_cast<std::int64_t>(::getpid());
#endif
}

/// What a session file says: `{"pid": <id>}`.
inline std::string session_text(std::int64_t pid) {
    return "{\"pid\": " + std::to_string(pid) + "}\n";
}

/// The process id `text` names, read as `session_text` writes it; none for anything else.
inline std::optional<std::int64_t> session_pid(const std::string& text) {
    const std::string key = "\"pid\":";
    const std::size_t at = text.find(key);
    if (at == std::string::npos) {
        return std::nullopt;
    }
    std::size_t i = at + key.size();
    while (i < text.size() && text[i] == ' ') {
        ++i;
    }
    std::int64_t pid = 0;
    std::size_t digits = 0;
    for (; i < text.size() && text[i] >= '0' && text[i] <= '9' && digits < 18; ++i, ++digits) {
        pid = pid * 10 + (text[i] - '0');
    }
    if (digits == 0 || pid <= 0) {
        return std::nullopt;
    }
    return pid;
}

/// The process id the session file in `folder` names, if one stands there and says one.
inline std::optional<std::int64_t> read_session(const std::filesystem::path& folder) {
    std::ifstream file(folder / kSessionName, std::ios::binary);
    if (!file) {
        return std::nullopt;
    }
    std::string text;
    text.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    if (text.size() > 64) {
        return std::nullopt;
    }
    return session_pid(text);
}

/// THE SESSION FILE, held while it lives: written with this process's id when made, removed when
/// it goes. A write that fails is said in `trouble` and leaves no file.
class SessionFile {
public:
    SessionFile() = default;
    explicit SessionFile(const std::filesystem::path& folder) : path_(folder / kSessionName) {
        std::ofstream file(path_, std::ios::binary | std::ios::trunc);
        file << session_text(this_process_id());
        file.close();
        if (!file) {
            trouble_ = "could not write " + path_.generic_string();
            std::error_code ec;
            std::filesystem::remove(path_, ec);
            path_.clear();
        }
    }
    ~SessionFile() { release(); }
    SessionFile(const SessionFile&) = delete;
    SessionFile& operator=(const SessionFile&) = delete;
    SessionFile(SessionFile&& other) noexcept
        : path_(std::move(other.path_)), trouble_(std::move(other.trouble_)) {
        other.path_.clear();
    }
    SessionFile& operator=(SessionFile&& other) noexcept {
        if (this != &other) {
            release();
            path_ = std::move(other.path_);
            trouble_ = std::move(other.trouble_);
            other.path_.clear();
        }
        return *this;
    }

    /// Whether the file stands, written by this holder.
    bool held() const noexcept { return !path_.empty(); }
    const std::string& trouble() const noexcept { return trouble_; }

    /// Remove the file now; idempotent, and done when the holder goes.
    void release() noexcept {
        if (!path_.empty()) {
            std::error_code ec;
            std::filesystem::remove(path_, ec);
            path_.clear();
        }
    }

private:
    std::filesystem::path path_;
    std::string trouble_;
};

} // namespace zengine::app

#endif // ZENGINE_APP_SESSION_FILE_HPP
