// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_NEOVIM_CHILD_HPP
#define ZENGINE_NEOVIM_CHILD_HPP

// CUSTODY OF ONE NEOVIM PROCESS, AND OF NOTHING ELSE.
//
// `builder/run.hpp` holds a build's process and reads its output. A Neovim is a CONVERSATION, so
// this holds a process it both writes to and reads from, and the rule the builder keeps is kept
// here in both directions: NOTHING WAITS UNLESS IT WAS ASKED TO WAIT FOR A BOUNDED TIME.
//
//     start_child(spec)     a running child in custody, or why nothing is running -- in words
//     pump(out, in, max)    write what the pipe takes NOW, read what is there NOW; never blocks
//     wait(ms)              the one bounded wait: output available, or the child gone
//     finish(grace)         a child that was asked to leave gets `grace` ms; then it is forced
//
// ⚠ A PARTIAL WRITE IS THE ORDINARY CASE. Neovim reads its input only when its loop runs, so a
// multi-megabyte request to a busy Neovim is accepted in pieces over many pumps (measured: 23
// partial sends of 5 MiB on Linux, and one overlapped write held pending on Windows). `pump`
// removes from `out` exactly what it handed to the pipe and leaves the rest.
//
// THE TWO PLATFORMS, and why each is spelled the way it is (both measured before written):
//
//   Windows   Named pipes whose ends on THIS side are OVERLAPPED, because an anonymous pipe's
//             write blocks when its buffer is full and has no non-blocking form. The child's ends
//             are inherited through PROC_THREAD_ATTRIBUTE_HANDLE_LIST -- only those three handles
//             -- so a Neovim cannot come to hold a pipe belonging to a concurrent build. The
//             process joins a kill-on-close JOB before its first instruction runs, so what it
//             starts ends with this custody too. The state lives on the heap: an overlapped
//             operation is identified by its OVERLAPPED's address, which must not move with the
//             `Child` that holds it.
//
//   POSIX     One socketpair for stdin and stdout, because `send` has MSG_NOSIGNAL and a pipe
//             write to an exited child raises SIGPIPE in the whole process. The child runs in its
//             own process group, closes every descriptor it was not given before exec, and on
//             Linux asks to be killed if the thread that started it dies first.
//
// ⚠ `exited` CAN BE SEEN BEFORE `output_ended`. A child's last words are written before it exits
// and read after; an owner that wants them keeps pumping until the output has ended.
//
// WHAT IT DOES NOT DO: decide what a Neovim is told, parse a byte of it, or claim anything about
// why a process ended beyond the status the operating system reported.

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <algorithm>
#include <atomic>
#include <cwchar>
#else
#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <ctime>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#if defined(__linux__)
#include <sys/prctl.h>
#include <sys/syscall.h>
#endif
extern char** environ;
#endif

namespace zengine::neovim {

/// WHAT TO START. The program is a path, or a bare name searched on PATH (the PATH `set_env`
/// gives, else this process's) BEFORE anything is spawned -- so "not installed" is said in words
/// at once rather than discovered later as an exit status.
struct LaunchSpec {
    std::string program;
    std::vector<std::string> args;                             ///< after the program
    std::string cwd;                                           ///< empty: inherit
    std::vector<std::pair<std::string, std::string>> set_env;  ///< added or replaced
    std::vector<std::string> unset_env;                        ///< removed
};

/// The most of a child's stderr kept: its END, because the end is what says why it left.
inline constexpr std::size_t kErrorTailBytes = 8u * 1024u;

namespace detail {

inline void keep_tail(std::string& tail, const char* data, std::size_t n) {
    tail.append(data, n);
    if (tail.size() > kErrorTailBytes) {
        tail.erase(0, tail.size() - kErrorTailBytes);
    }
}

/// ONE ARGUMENT AS THE WINDOWS COMMAND-LINE CONVENTION NEEDS IT SPELLED: the documented rule
/// (backslashes are literal except before a quote, where they double) that `builder/run.hpp`
/// applies. The two packages must not depend on each other, so each carries the rule, and the
/// `neovim` suite pins that both spell the same line for the same arguments. A template over the
/// character type and defined on every platform, so the rule itself is pinned on every lane.
template <class Char>
inline std::basic_string<Char> windows_quote(const std::basic_string<Char>& arg) {
    const Char specials[] = {Char(' '), Char('\t'), Char('\n'), Char('\v'), Char('"'), Char(0)};
    const bool needs = arg.empty() || arg.find_first_of(specials) != std::basic_string<Char>::npos;
    if (!needs) {
        return arg;
    }
    std::basic_string<Char> out(1, Char('"'));
    for (std::size_t i = 0; i < arg.size(); ++i) {
        std::size_t slashes = 0;
        while (i < arg.size() && arg[i] == Char('\\')) {
            ++slashes;
            ++i;
        }
        if (i == arg.size()) {
            out.append(slashes * 2, Char('\\'));
            break;
        }
        if (arg[i] == Char('"')) {
            out.append(slashes * 2 + 1, Char('\\'));
        } else {
            out.append(slashes, Char('\\'));
        }
        out.push_back(arg[i]);
    }
    out.push_back(Char('"'));
    return out;
}

#if defined(_WIN32)

inline std::wstring wide(const std::string& utf8) {
    if (utf8.empty()) {
        return std::wstring();
    }
    const int n = ::MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()),
                                        nullptr, 0);
    if (n <= 0) {
        return std::wstring();
    }
    std::wstring out(static_cast<std::size_t>(n), L'\0');
    (void)::MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), out.data(), n);
    return out;
}

inline int ordinal_compare(const std::wstring& a, const std::wstring& b) {
    return ::CompareStringOrdinal(a.c_str(), static_cast<int>(a.size()), b.c_str(),
                                  static_cast<int>(b.size()), TRUE);
}

/// The name part of one `NAME=value` entry. The hidden per-drive entries begin with '=', so the
/// name ends at the first '=' AFTER the first character.
inline std::wstring env_name(const std::wstring& entry) {
    const std::size_t eq = entry.find(L'=', entry.empty() ? 0 : 1);
    return eq == std::wstring::npos ? entry : entry.substr(0, eq);
}

/// This process's environment with `spec`'s changes, as the block CreateProcessW takes: sorted by
/// name, case-insensitively, in ordinal order, each entry NUL-terminated and one more NUL after.
inline std::wstring environment_block(const LaunchSpec& spec) {
    std::vector<std::wstring> entries;
    if (LPWCH block = ::GetEnvironmentStringsW()) {
        for (LPWCH p = block; *p != L'\0'; p += std::wcslen(p) + 1) {
            entries.emplace_back(p);
        }
        (void)::FreeEnvironmentStringsW(block);
    }
    const auto drop = [&entries](const std::wstring& name) {
        entries.erase(std::remove_if(entries.begin(), entries.end(),
                                     [&name](const std::wstring& e) {
                                         return ordinal_compare(env_name(e), name) == CSTR_EQUAL;
                                     }),
                      entries.end());
    };
    for (const std::string& name : spec.unset_env) {
        drop(wide(name));
    }
    for (const auto& kv : spec.set_env) {
        const std::wstring name = wide(kv.first);
        drop(name);
        entries.push_back(name + L"=" + wide(kv.second));
    }
    std::sort(entries.begin(), entries.end(), [](const std::wstring& a, const std::wstring& b) {
        return ordinal_compare(env_name(a), env_name(b)) == CSTR_LESS_THAN;
    });
    std::wstring out;
    for (const std::wstring& e : entries) {
        out += e;
        out.push_back(L'\0');
    }
    out.push_back(L'\0');
    return out;
}

inline std::wstring environment_value(const LaunchSpec& spec, const std::wstring& name) {
    for (const auto& kv : spec.set_env) {
        if (ordinal_compare(wide(kv.first), name) == CSTR_EQUAL) {
            return wide(kv.second);
        }
    }
    DWORD n = ::GetEnvironmentVariableW(name.c_str(), nullptr, 0);
    if (n == 0) {
        return std::wstring();
    }
    std::wstring value(n, L'\0');
    n = ::GetEnvironmentVariableW(name.c_str(), value.data(), n);
    value.resize(n);
    return value;
}

inline bool is_file(const std::wstring& path) {
    const DWORD a = ::GetFileAttributesW(path.c_str());
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

/// The program as a file CreateProcessW can open, or empty. A name without a directory is searched
/// on PATH; a name without an extension also tries `.exe` (and only that -- a `.cmd` or `.bat`
/// wrapper is not a program CreateProcessW runs).
inline std::wstring resolve(const LaunchSpec& spec) {
    const std::wstring program = wide(spec.program);
    const auto existing = [](const std::wstring& p) -> std::wstring {
        if (is_file(p)) {
            return p;
        }
        if (is_file(p + L".exe")) {
            return p + L".exe";
        }
        return std::wstring();
    };
    if (program.empty()) {
        return std::wstring();
    }
    if (program.find_first_of(L"\\/:") != std::wstring::npos) {
        return existing(program);
    }
    const std::wstring path = environment_value(spec, L"PATH");
    std::size_t start = 0;
    for (;;) {
        const std::size_t end = path.find(L';', start);
        std::wstring dir = path.substr(start, end == std::wstring::npos ? std::wstring::npos : end - start);
        if (!dir.empty()) {
            if (dir.back() != L'\\' && dir.back() != L'/') {
                dir.push_back(L'\\');
            }
            std::wstring found = existing(dir + program);
            if (!found.empty()) {
                return found;
            }
        }
        if (end == std::wstring::npos) {
            return std::wstring();
        }
        start = end + 1;
    }
}

inline std::string error_words(DWORD code) {
    wchar_t* buf = nullptr;
    const DWORD n = ::FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                                         FORMAT_MESSAGE_IGNORE_INSERTS,
                                     nullptr, code, 0, reinterpret_cast<wchar_t*>(&buf), 0, nullptr);
    std::string words;
    if (n > 0 && buf != nullptr) {
        const int len = ::WideCharToMultiByte(CP_UTF8, 0, buf, static_cast<int>(n), nullptr, 0,
                                              nullptr, nullptr);
        if (len > 0) {
            words.resize(static_cast<std::size_t>(len));
            (void)::WideCharToMultiByte(CP_UTF8, 0, buf, static_cast<int>(n), words.data(), len,
                                        nullptr, nullptr);
        }
        while (!words.empty() && (words.back() == '\n' || words.back() == '\r' ||
                                  words.back() == ' ' || words.back() == '.')) {
            words.pop_back();
        }
    }
    if (buf != nullptr) {
        (void)::LocalFree(buf);
    }
    const std::string number = "error " + std::to_string(static_cast<unsigned long>(code));
    return words.empty() ? number : words + " (" + number + ")";
}

/// The ways a pipe ENDS rather than fails: the other side closed it, or this side cancelled.
inline bool quiet_end(DWORD e) {
    return e == ERROR_BROKEN_PIPE || e == ERROR_PIPE_NOT_CONNECTED || e == ERROR_NO_DATA ||
           e == ERROR_OPERATION_ABORTED;
}

/// One pipe end on this side, with at most one overlapped operation in flight.
struct Endpoint {
    HANDLE pipe = INVALID_HANDLE_VALUE;
    HANDLE event = nullptr;
    OVERLAPPED ov{};
    bool pending = false;
    bool ended = false;

    void arm() {
        std::memset(&ov, 0, sizeof ov);
        ov.hEvent = event;
        (void)::ResetEvent(event);
    }

    void close() {
        if (pipe != INVALID_HANDLE_VALUE) {
            if (pending) {
                (void)::CancelIoEx(pipe, &ov);
                DWORD n = 0;
                (void)::GetOverlappedResult(pipe, &ov, &n, TRUE); // a cancelled operation completes promptly
                pending = false;
            }
            (void)::CloseHandle(pipe);
            pipe = INVALID_HANDLE_VALUE;
        }
        if (event != nullptr) {
            (void)::CloseHandle(event);
            event = nullptr;
        }
        ended = true;
    }
};

struct WindowsChild {
    HANDLE process = nullptr;
    HANDLE job = nullptr;
    Endpoint in;
    std::string in_chunk; ///< the bytes of the write in flight
    Endpoint out;
    Endpoint err;
    char out_buf[65536];
    char err_buf[4096];
};

#else

inline bool executable(const std::string& path) {
    struct stat st {};
    return ::stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode) && ::access(path.c_str(), X_OK) == 0;
}

inline std::string environment_value(const LaunchSpec& spec, const std::string& name) {
    for (const auto& kv : spec.set_env) {
        if (kv.first == name) {
            return kv.second;
        }
    }
    const char* v = std::getenv(name.c_str());
    return v != nullptr ? std::string(v) : std::string();
}

/// The program as an executable file, or empty. A name without a '/' is searched on PATH.
inline std::string resolve(const LaunchSpec& spec) {
    if (spec.program.empty()) {
        return std::string();
    }
    if (spec.program.find('/') != std::string::npos) {
        return executable(spec.program) ? spec.program : std::string();
    }
    const std::string path = environment_value(spec, "PATH");
    std::size_t start = 0;
    for (;;) {
        const std::size_t end = path.find(':', start);
        const std::string dir = path.substr(start, end == std::string::npos ? std::string::npos : end - start);
        const std::string candidate = (dir.empty() ? std::string(".") : dir) + "/" + spec.program;
        if (executable(candidate)) {
            return candidate;
        }
        if (end == std::string::npos) {
            return std::string();
        }
        start = end + 1;
    }
}

inline void close_quietly(int& fd) {
    if (fd >= 0) {
        (void)::close(fd);
        fd = -1;
    }
}

inline void set_flags(int fd, bool nonblocking) {
    const int fl = ::fcntl(fd, F_GETFD, 0);
    (void)::fcntl(fd, F_SETFD, (fl < 0 ? 0 : fl) | FD_CLOEXEC);
    if (nonblocking) {
        const int flags = ::fcntl(fd, F_GETFL, 0);
        (void)::fcntl(fd, F_SETFL, (flags < 0 ? 0 : flags) | O_NONBLOCK);
    }
}

#if defined(MSG_NOSIGNAL)
inline constexpr int kSendFlags = MSG_NOSIGNAL;
#else
inline constexpr int kSendFlags = 0; // SO_NOSIGPIPE is set on the socket instead
#endif

#endif

} // namespace detail

class Child;

/// What starting came to: custody of a running child, or the words for why nothing runs.
struct ChildStart;

/// START ONE CHILD with its stdin, stdout and stderr connected to this custody. Never blocks on
/// the child: it returns as soon as the process exists (or could not be made to).
inline ChildStart start_child(const LaunchSpec& spec);

class Child {
public:
    Child() = default;
    Child(const Child&) = delete;
    Child& operator=(const Child&) = delete;
    Child(Child&& other) noexcept { take(other); }
    Child& operator=(Child&& other) noexcept {
        if (this != &other) {
            (void)finish(0);
            take(other);
        }
        return *this;
    }
    /// LOSING THE HOLDER ENDS THE CHILD, forced. An owner that wants Neovim to leave cleanly (its
    /// swap files removed) asks it to quit and calls `finish` with a grace period first.
    ~Child() { (void)finish(0); }

    bool holds() const noexcept {
#if defined(_WIN32)
        return state_ != nullptr && state_->process != nullptr;
#else
        return pid_ > 0;
#endif
    }

    /// The operating system's process id, for words and witnesses only.
    std::int64_t pid() const noexcept { return pid_value_; }

    struct Pump {
        std::size_t wrote = 0;       ///< bytes the pipe accepted, completed during this pump
        std::size_t read = 0;        ///< bytes appended to `in`
        bool input_ended = false;    ///< the child no longer reads what it is sent
        bool output_ended = false;   ///< the child's stdout reached its end
        bool exited = false;         ///< the child has exited (this pump or before)
        std::int64_t exit_status = 0;
        std::string trouble;         ///< an I/O failure seen on this side, in words; empty when none
    };

    /// WRITE WHAT THE PIPE TAKES AND READ WHAT IS THERE, NOW. `out` loses exactly the bytes handed
    /// to the pipe; `in` gains at most `max_read` bytes; the stderr tail is refreshed. Never blocks.
    Pump pump(std::string& out, std::string& in, std::size_t max_read);

    /// THE ONE BOUNDED WAIT: returns when output may be available, the child has gone, or `ms`
    /// passed. True unless the time ran out.
    bool wait(int ms);

    const std::string& error_tail() const noexcept { return error_tail_; }

    struct Ended {
        bool was_running = false; ///< the child had not exited when `finish` was called
        bool forced = false;      ///< the grace ran out and the child was killed
        std::int64_t status = 0;
    };

    /// END CUSTODY. A child still running gets `grace_ms` to exit (it should already have been
    /// asked to); then it is forced, with what it started -- the job on Windows, the process group
    /// on POSIX -- and reaped. Every handle is released exactly once; a second call does nothing.
    Ended finish(int grace_ms);

    bool exited() const noexcept { return exited_; }
    std::int64_t exit_status() const noexcept { return exit_status_; }

private:
    friend ChildStart start_child(const LaunchSpec& spec);

    void take(Child& other) noexcept {
#if defined(_WIN32)
        state_ = std::move(other.state_);
#else
        pid_ = std::exchange(other.pid_, -1);
        io_ = std::exchange(other.io_, -1);
        err_ = std::exchange(other.err_, -1);
        input_ended_ = other.input_ended_;
        output_ended_ = other.output_ended_;
#endif
        pid_value_ = other.pid_value_;
        exited_ = other.exited_;
        exit_status_ = other.exit_status_;
        error_tail_ = std::move(other.error_tail_);
    }

#if defined(_WIN32)
    void read_from(detail::Endpoint& e, char* buf, DWORD size, std::string* into, std::size_t budget,
                   std::size_t& taken, std::string& trouble);
    std::unique_ptr<detail::WindowsChild> state_;
#else
    bool observe_exit() noexcept;
    ::pid_t pid_ = -1;
    int io_ = -1;
    int err_ = -1;
    bool input_ended_ = false;
    bool output_ended_ = false;
#endif
    std::int64_t pid_value_ = 0;
    bool exited_ = false;
    std::int64_t exit_status_ = 0;
    std::string error_tail_;
};

struct ChildStart {
    bool started = false;
    std::string trouble;
    Child child;
};

// =================================================================================================
#if defined(_WIN32)

inline void Child::read_from(detail::Endpoint& e, char* buf, DWORD size, std::string* into,
                             std::size_t budget, std::size_t& taken, std::string& trouble) {
    while (!e.ended && (into == nullptr || taken < budget)) {
        if (e.pending) {
            DWORD n = 0;
            if (!::GetOverlappedResult(e.pipe, &e.ov, &n, FALSE)) {
                const DWORD why = ::GetLastError();
                if (why == ERROR_IO_INCOMPLETE) {
                    return; // nothing more to read right now
                }
                e.pending = false;
                e.ended = true;
                if (!detail::quiet_end(why) && into != nullptr) {
                    trouble = "reading Neovim's output failed: " + detail::error_words(why);
                }
                return;
            }
            e.pending = false;
            if (into != nullptr) {
                into->append(buf, n);
                taken += n;
            } else {
                detail::keep_tail(error_tail_, buf, n);
            }
            continue;
        }
        DWORD want = size;
        if (into != nullptr && budget - taken < want) {
            want = static_cast<DWORD>(budget - taken);
        }
        e.arm();
        // Completed at once or pending, the count is collected the same way on the next turn.
        if (::ReadFile(e.pipe, buf, want, nullptr, &e.ov) || ::GetLastError() == ERROR_IO_PENDING) {
            e.pending = true;
            continue;
        }
        const DWORD why = ::GetLastError();
        e.ended = true;
        if (!detail::quiet_end(why) && into != nullptr) {
            trouble = "reading Neovim's output failed: " + detail::error_words(why);
        }
        return;
    }
}

inline Child::Pump Child::pump(std::string& out, std::string& in, std::size_t max_read) {
    Pump p;
    if (state_ == nullptr) {
        p.input_ended = true;
        p.output_ended = true;
        p.exited = exited_;
        p.exit_status = exit_status_;
        return p;
    }
    detail::WindowsChild& s = *state_;
    // WRITE: one overlapped write in flight at most, collected when done and never waited on.
    while (!s.in.ended) {
        if (s.in.pending) {
            DWORD n = 0;
            if (::GetOverlappedResult(s.in.pipe, &s.in.ov, &n, FALSE)) {
                s.in.pending = false;
                p.wrote += n;
                s.in_chunk.clear();
            } else {
                const DWORD why = ::GetLastError();
                if (why == ERROR_IO_INCOMPLETE) {
                    break;
                }
                s.in.pending = false;
                s.in.ended = true;
                if (!detail::quiet_end(why)) {
                    p.trouble = "writing to Neovim failed: " + detail::error_words(why);
                }
                break;
            }
        }
        if (out.empty()) {
            break;
        }
        const std::size_t take = out.size() < (std::size_t{1} << 20) ? out.size() : (std::size_t{1} << 20);
        s.in_chunk.assign(out, 0, take);
        out.erase(0, take);
        s.in.arm();
        if (::WriteFile(s.in.pipe, s.in_chunk.data(), static_cast<DWORD>(s.in_chunk.size()), nullptr,
                        &s.in.ov) ||
            ::GetLastError() == ERROR_IO_PENDING) {
            s.in.pending = true;
            continue;
        }
        const DWORD why = ::GetLastError();
        s.in.ended = true;
        if (!detail::quiet_end(why)) {
            p.trouble = "writing to Neovim failed: " + detail::error_words(why);
        }
    }
    std::size_t taken = 0;
    read_from(s.out, s.out_buf, sizeof s.out_buf, &in, max_read, taken, p.trouble);
    p.read = taken;
    std::size_t unused = 0;
    std::string ignored;
    read_from(s.err, s.err_buf, sizeof s.err_buf, nullptr, 0, unused, ignored);
    if (s.process != nullptr && !exited_ && ::WaitForSingleObject(s.process, 0) == WAIT_OBJECT_0) {
        DWORD code = 0;
        (void)::GetExitCodeProcess(s.process, &code);
        exit_status_ = static_cast<std::int64_t>(code);
        exited_ = true;
    }
    p.input_ended = s.in.ended;
    p.output_ended = s.out.ended;
    p.exited = exited_;
    p.exit_status = exit_status_;
    return p;
}

inline bool Child::wait(int ms) {
    if (state_ == nullptr) {
        return true;
    }
    detail::WindowsChild& s = *state_;
    if (!s.out.ended && !s.out.pending) {
        return true; // a read was cut short by its budget: there may be more already
    }
    HANDLE handles[3];
    DWORD count = 0;
    if (s.out.pending) {
        handles[count++] = s.out.event;
    }
    if (s.err.pending) {
        handles[count++] = s.err.event;
    }
    if (s.process != nullptr && !exited_) {
        handles[count++] = s.process;
    }
    if (count == 0) {
        return true;
    }
    const DWORD r = ::WaitForMultipleObjects(count, handles, FALSE, static_cast<DWORD>(ms < 0 ? 0 : ms));
    return r < WAIT_OBJECT_0 + count;
}

inline Child::Ended Child::finish(int grace_ms) {
    Ended e;
    if (state_ == nullptr) {
        return e;
    }
    detail::WindowsChild& s = *state_;
    if (s.process != nullptr) {
        e.was_running = ::WaitForSingleObject(s.process, 0) == WAIT_TIMEOUT;
        bool running = e.was_running;
        if (running && grace_ms > 0) {
            running = ::WaitForSingleObject(s.process, static_cast<DWORD>(grace_ms)) == WAIT_TIMEOUT;
        }
        if (running) {
            e.forced = true;
            if (s.job != nullptr) {
                (void)::TerminateJobObject(s.job, 1);
            } else {
                (void)::TerminateProcess(s.process, 1);
            }
            (void)::WaitForSingleObject(s.process, 5000);
        }
        DWORD code = 0;
        (void)::GetExitCodeProcess(s.process, &code);
        exit_status_ = static_cast<std::int64_t>(code);
        exited_ = true;
        (void)::CloseHandle(s.process);
        s.process = nullptr;
    }
    e.status = exit_status_;
    s.in.close();
    s.out.close();
    s.err.close();
    if (s.job != nullptr) {
        (void)::CloseHandle(s.job); // kill-on-close: whatever the child started and left ends here
        s.job = nullptr;
    }
    state_.reset();
    return e;
}

inline ChildStart start_child(const LaunchSpec& spec) {
    ChildStart r;
    const std::wstring program = detail::resolve(spec);
    if (program.empty()) {
        r.trouble = spec.program.empty() ? std::string("no Neovim program was named")
                                         : "`" + spec.program + "` was not found" +
                                               (spec.program.find_first_of("\\/:") == std::string::npos
                                                    ? " on PATH"
                                                    : "");
        return r;
    }
    struct Pair {
        HANDLE ours = INVALID_HANDLE_VALUE;
        HANDLE theirs = INVALID_HANDLE_VALUE;
        void close() {
            if (ours != INVALID_HANDLE_VALUE) {
                (void)::CloseHandle(ours);
                ours = INVALID_HANDLE_VALUE;
            }
            if (theirs != INVALID_HANDLE_VALUE) {
                (void)::CloseHandle(theirs);
                theirs = INVALID_HANDLE_VALUE;
            }
        }
    };
    // A NAME NOTHING ELSE HOLDS: this process, a high-resolution instant and a counter. The first
    // instance flag makes a collision a refusal rather than a shared pipe.
    static std::atomic<unsigned long> counter{0};
    LARGE_INTEGER instant{};
    (void)::QueryPerformanceCounter(&instant);
    const std::wstring stem = L"\\\\.\\pipe\\zengine-neovim-" +
                              std::to_wstring(static_cast<unsigned long>(::GetCurrentProcessId())) + L"-" +
                              std::to_wstring(static_cast<long long>(instant.QuadPart)) + L"-" +
                              std::to_wstring(++counter);
    SECURITY_ATTRIBUTES inheritable{};
    inheritable.nLength = sizeof inheritable;
    inheritable.bInheritHandle = TRUE;
    DWORD failure = 0;
    const auto make = [&](const wchar_t* which, bool child_reads) {
        const std::wstring name = stem + L"-" + which;
        Pair pair;
        pair.ours = ::CreateNamedPipeW(
            name.c_str(),
            static_cast<DWORD>(child_reads ? PIPE_ACCESS_OUTBOUND : PIPE_ACCESS_INBOUND) |
                FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE,
            PIPE_TYPE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS, 1, 1u << 16, 1u << 16, 0, nullptr);
        if (pair.ours == INVALID_HANDLE_VALUE) {
            failure = failure != 0 ? failure : ::GetLastError();
            return pair;
        }
        pair.theirs = ::CreateFileW(name.c_str(), child_reads ? GENERIC_READ : GENERIC_WRITE, 0,
                                    &inheritable, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (pair.theirs == INVALID_HANDLE_VALUE) {
            failure = failure != 0 ? failure : ::GetLastError();
        }
        return pair;
    };
    Pair in = make(L"in", true);
    Pair out = make(L"out", false);
    Pair err = make(L"err", false);
    const auto close_all = [&] {
        in.close();
        out.close();
        err.close();
    };
    if (in.theirs == INVALID_HANDLE_VALUE || out.theirs == INVALID_HANDLE_VALUE ||
        err.theirs == INVALID_HANDLE_VALUE) {
        close_all();
        r.trouble = "could not make the pipes to talk to Neovim: " + detail::error_words(failure);
        return r;
    }
    // ONLY THESE THREE HANDLES ARE INHERITED. Without the list a child inherits every inheritable
    // handle in this process; refusing is better than starting without it.
    HANDLE inherit[3] = {in.theirs, out.theirs, err.theirs};
    SIZE_T attr_size = 0;
    (void)::InitializeProcThreadAttributeList(nullptr, 1, 0, &attr_size);
    std::vector<char> attr_storage(attr_size);
    auto* attrs = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attr_storage.data());
    if (attr_size == 0 || !::InitializeProcThreadAttributeList(attrs, 1, 0, &attr_size)) {
        const DWORD why = ::GetLastError();
        close_all();
        r.trouble = "could not restrict what Neovim inherits: " + detail::error_words(why);
        return r;
    }
    if (!::UpdateProcThreadAttribute(attrs, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherit, sizeof inherit,
                                     nullptr, nullptr)) {
        const DWORD why = ::GetLastError();
        ::DeleteProcThreadAttributeList(attrs);
        close_all();
        r.trouble = "could not restrict what Neovim inherits: " + detail::error_words(why);
        return r;
    }
    std::wstring line = detail::windows_quote(program);
    for (const std::string& a : spec.args) {
        line += L' ';
        line += detail::windows_quote(detail::wide(a));
    }
    std::vector<wchar_t> mutable_line(line.begin(), line.end());
    mutable_line.push_back(L'\0');
    std::wstring environment = detail::environment_block(spec);
    const std::wstring cwd = detail::wide(spec.cwd);
    STARTUPINFOEXW startup{};
    startup.StartupInfo.cb = sizeof startup;
    startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    startup.StartupInfo.hStdInput = in.theirs;
    startup.StartupInfo.hStdOutput = out.theirs;
    startup.StartupInfo.hStdError = err.theirs;
    startup.lpAttributeList = attrs;
    PROCESS_INFORMATION process{};
    const BOOL began = ::CreateProcessW(
        program.c_str(), mutable_line.data(), nullptr, nullptr, TRUE,
        CREATE_NO_WINDOW | CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT | EXTENDED_STARTUPINFO_PRESENT,
        environment.data(), cwd.empty() ? nullptr : cwd.c_str(), &startup.StartupInfo, &process);
    const DWORD create_failure = began ? 0 : ::GetLastError();
    ::DeleteProcThreadAttributeList(attrs);
    // The child's copies are the child's now; ours would keep each pipe from ever reaching its end.
    (void)::CloseHandle(in.theirs);
    in.theirs = INVALID_HANDLE_VALUE;
    (void)::CloseHandle(out.theirs);
    out.theirs = INVALID_HANDLE_VALUE;
    (void)::CloseHandle(err.theirs);
    err.theirs = INVALID_HANDLE_VALUE;
    if (!began) {
        close_all();
        r.trouble = "could not start `" + spec.program + "`: " + detail::error_words(create_failure);
        return r;
    }
    // THE JOB BEFORE THE FIRST INSTRUCTION RUNS, so nothing the child starts escapes this custody.
    HANDLE job = ::CreateJobObjectW(nullptr, nullptr);
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (job == nullptr ||
        !::SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof limits) ||
        !::AssignProcessToJobObject(job, process.hProcess)) {
        const DWORD why = ::GetLastError();
        (void)::TerminateProcess(process.hProcess, 1);
        (void)::WaitForSingleObject(process.hProcess, 5000);
        (void)::CloseHandle(process.hThread);
        (void)::CloseHandle(process.hProcess);
        if (job != nullptr) {
            (void)::CloseHandle(job);
        }
        close_all();
        r.trouble = "could not place Neovim under this custody: " + detail::error_words(why);
        return r;
    }
    (void)::ResumeThread(process.hThread);
    (void)::CloseHandle(process.hThread);
    auto state = std::make_unique<detail::WindowsChild>();
    state->process = process.hProcess;
    state->job = job;
    state->in.pipe = std::exchange(in.ours, INVALID_HANDLE_VALUE);
    state->out.pipe = std::exchange(out.ours, INVALID_HANDLE_VALUE);
    state->err.pipe = std::exchange(err.ours, INVALID_HANDLE_VALUE);
    state->in.event = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);
    state->out.event = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);
    state->err.event = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);
    const bool events = state->in.event != nullptr && state->out.event != nullptr &&
                        state->err.event != nullptr;
    const DWORD event_failure = events ? 0 : ::GetLastError();
    r.child.state_ = std::move(state);
    r.child.pid_value_ = static_cast<std::int64_t>(process.dwProcessId);
    if (!events) {
        (void)r.child.finish(0);
        r.trouble = "could not make the events to wait on Neovim: " + detail::error_words(event_failure);
        return r;
    }
    r.started = true;
    return r;
}

// =================================================================================================
#else

inline bool Child::observe_exit() noexcept {
    if (pid_ <= 0 || exited_) {
        return exited_;
    }
    // WNOWAIT: SEE the exit without reaping it. The unreaped child keeps its pid -- and so its
    // process group's id -- from being reused until `finish` has ended what it started.
    ::siginfo_t info;
    std::memset(&info, 0, sizeof info);
    int r = 0;
    while ((r = ::waitid(P_PID, static_cast<::id_t>(pid_), &info, WEXITED | WNOHANG | WNOWAIT)) < 0 &&
           errno == EINTR) {
    }
    if (r == 0 && info.si_pid == pid_) {
        exited_ = true;
        exit_status_ = info.si_code == CLD_EXITED ? static_cast<std::int64_t>(info.si_status)
                                                  : 128 + static_cast<std::int64_t>(info.si_status);
    }
    return exited_;
}

inline Child::Pump Child::pump(std::string& out, std::string& in, std::size_t max_read) {
    Pump p;
    if (io_ >= 0 && !input_ended_ && !out.empty()) {
        std::size_t sent = 0;
        while (sent < out.size()) {
            const ::ssize_t n = ::send(io_, out.data() + sent, out.size() - sent, detail::kSendFlags);
            if (n > 0) {
                sent += static_cast<std::size_t>(n);
                continue;
            }
            if (n < 0 && errno == EINTR) {
                continue;
            }
            if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                break;
            }
            input_ended_ = true;
            if (n < 0 && errno != EPIPE && errno != ECONNRESET) {
                p.trouble = std::string("writing to Neovim failed: ") + std::strerror(errno);
            }
            break;
        }
        out.erase(0, sent);
        p.wrote = sent;
    }
    if (io_ >= 0 && !output_ended_) {
        char buf[65536];
        while (p.read < max_read) {
            const std::size_t want = max_read - p.read < sizeof buf ? max_read - p.read : sizeof buf;
            const ::ssize_t n = ::recv(io_, buf, want, 0);
            if (n > 0) {
                in.append(buf, static_cast<std::size_t>(n));
                p.read += static_cast<std::size_t>(n);
                continue;
            }
            if (n == 0) {
                output_ended_ = true;
                break;
            }
            if (errno == EINTR) {
                continue;
            }
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                output_ended_ = true;
                if (errno != ECONNRESET) {
                    p.trouble = std::string("reading Neovim's output failed: ") + std::strerror(errno);
                }
            }
            break;
        }
    }
    if (err_ >= 0) {
        char buf[4096];
        for (;;) {
            const ::ssize_t n = ::read(err_, buf, sizeof buf);
            if (n > 0) {
                detail::keep_tail(error_tail_, buf, static_cast<std::size_t>(n));
                continue;
            }
            if (n < 0 && errno == EINTR) {
                continue;
            }
            if (n == 0 || (errno != EAGAIN && errno != EWOULDBLOCK)) {
                detail::close_quietly(err_);
            }
            break;
        }
    }
    (void)observe_exit();
    p.input_ended = input_ended_ || io_ < 0;
    p.output_ended = output_ended_ || io_ < 0;
    p.exited = exited_;
    p.exit_status = exit_status_;
    return p;
}

inline bool Child::wait(int ms) {
    ::pollfd fds[2];
    ::nfds_t count = 0;
    if (io_ >= 0 && !output_ended_) {
        fds[count].fd = io_;
        fds[count].events = POLLIN;
        fds[count].revents = 0;
        ++count;
    }
    if (err_ >= 0) {
        fds[count].fd = err_;
        fds[count].events = POLLIN;
        fds[count].revents = 0;
        ++count;
    }
    if (count == 0) {
        return true;
    }
    int r = 0;
    while ((r = ::poll(fds, count, ms < 0 ? 0 : ms)) < 0 && errno == EINTR) {
    }
    return r > 0;
}

inline Child::Ended Child::finish(int grace_ms) {
    Ended e;
    if (pid_ > 0) {
        e.was_running = !observe_exit();
        if (e.was_running && grace_ms > 0) {
            ::timespec now{};
            (void)::clock_gettime(CLOCK_MONOTONIC, &now);
            const std::int64_t deadline =
                static_cast<std::int64_t>(now.tv_sec) * 1000 + now.tv_nsec / 1000000 + grace_ms;
            while (!observe_exit()) {
                (void)::clock_gettime(CLOCK_MONOTONIC, &now);
                if (static_cast<std::int64_t>(now.tv_sec) * 1000 + now.tv_nsec / 1000000 >= deadline) {
                    break;
                }
                const ::timespec step{0, 1000000L};
                (void)::nanosleep(&step, nullptr);
            }
        }
        if (!exited_) {
            e.forced = true;
        }
        // WHAT IT STARTED IN ITS GROUP ENDS WITH IT, forced or not. Neovim itself is unreaped here,
        // so the group's id cannot have been reused by anything this custody never held.
        (void)::kill(-pid_, SIGKILL);
        (void)::kill(pid_, SIGKILL);
        int status = 0;
        ::pid_t done = 0;
        while ((done = ::waitpid(pid_, &status, 0)) < 0 && errno == EINTR) {
        }
        if (done == pid_ && !exited_) {
            exit_status_ = WIFEXITED(status) ? static_cast<std::int64_t>(WEXITSTATUS(status))
                                             : 128 + static_cast<std::int64_t>(WTERMSIG(status));
        }
        exited_ = true;
        pid_ = -1;
    }
    e.status = exit_status_;
    detail::close_quietly(io_);
    detail::close_quietly(err_);
    return e;
}

inline ChildStart start_child(const LaunchSpec& spec) {
    ChildStart r;
    const std::string program = detail::resolve(spec);
    if (program.empty()) {
        r.trouble = spec.program.empty()
                        ? std::string("no Neovim program was named")
                        : "`" + spec.program + "` was not found" +
                              (spec.program.find('/') == std::string::npos ? " on PATH"
                                                                           : " or is not executable");
        return r;
    }
    int sv[2] = {-1, -1};
#if defined(SOCK_CLOEXEC)
    const int socket_type = SOCK_STREAM | SOCK_CLOEXEC;
#else
    const int socket_type = SOCK_STREAM;
#endif
    if (::socketpair(AF_UNIX, socket_type, 0, sv) != 0) {
        r.trouble = std::string("could not make the socket to talk to Neovim: ") + std::strerror(errno);
        return r;
    }
    int ep[2] = {-1, -1};
    if (::pipe(ep) != 0) {
        r.trouble = std::string("could not make the pipe for Neovim's errors: ") + std::strerror(errno);
        detail::close_quietly(sv[0]);
        detail::close_quietly(sv[1]);
        return r;
    }
    detail::set_flags(sv[0], true);
    detail::set_flags(sv[1], false);
    detail::set_flags(ep[0], true);
    detail::set_flags(ep[1], false);
#if defined(SO_NOSIGPIPE)
    const int one = 1;
    (void)::setsockopt(sv[0], SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof one);
#endif
    // EVERYTHING THE CHILD NEEDS IS BUILT BEFORE THE FORK. Between fork and exec the child shares
    // this address space and may not allocate, so that stretch is only prctl, getppid, setpgid,
    // dup2, close, chdir and execve.
    std::vector<std::string> argv_store;
    argv_store.push_back(program);
    for (const std::string& a : spec.args) {
        argv_store.push_back(a);
    }
    std::vector<char*> argv;
    for (std::string& a : argv_store) {
        argv.push_back(a.data());
    }
    argv.push_back(nullptr);
    std::vector<std::string> env_store;
    for (char** e = environ; e != nullptr && *e != nullptr; ++e) {
        const std::string entry(*e);
        const std::string name = entry.substr(0, entry.find('='));
        bool replaced = false;
        for (const std::string& u : spec.unset_env) {
            replaced = replaced || u == name;
        }
        for (const auto& kv : spec.set_env) {
            replaced = replaced || kv.first == name;
        }
        if (!replaced) {
            env_store.push_back(entry);
        }
    }
    for (const auto& kv : spec.set_env) {
        env_store.push_back(kv.first + "=" + kv.second);
    }
    std::vector<char*> envp;
    for (std::string& e : env_store) {
        envp.push_back(e.data());
    }
    envp.push_back(nullptr);
    const std::string cwd = spec.cwd;
    long open_max = ::sysconf(_SC_OPEN_MAX);
    if (open_max < 0 || open_max > 65536) {
        open_max = 65536;
    }
    const ::pid_t parent = ::getpid();
    const ::pid_t child = ::fork();
    if (child < 0) {
        r.trouble = std::string("could not start a process for Neovim: ") + std::strerror(errno);
        detail::close_quietly(sv[0]);
        detail::close_quietly(sv[1]);
        detail::close_quietly(ep[0]);
        detail::close_quietly(ep[1]);
        return r;
    }
    if (child == 0) {
#if defined(__linux__) && defined(PR_SET_PDEATHSIG)
        (void)::prctl(PR_SET_PDEATHSIG, SIGKILL);
        if (::getppid() != parent) {
            ::_exit(126); // the starter was already gone: nothing would ever hold this child
        }
#else
        (void)parent;
#endif
        (void)::setpgid(0, 0);
        if (::dup2(sv[1], 0) < 0 || ::dup2(sv[1], 1) < 0 || ::dup2(ep[1], 2) < 0) {
            ::_exit(126);
        }
#if defined(__linux__) && defined(SYS_close_range)
        if (::syscall(SYS_close_range, 3u, ~0u, 0u) != 0)
#endif
        {
            for (long fd = 3; fd < open_max; ++fd) {
                (void)::close(static_cast<int>(fd));
            }
        }
        if (!cwd.empty() && ::chdir(cwd.c_str()) != 0) {
            ::_exit(126);
        }
        (void)::execve(argv[0], argv.data(), envp.data());
        ::_exit(127);
    }
    // The same group from this side too, so a finish that comes before the child ran still
    // reaches it by its group (either call may lose the race; one of them does not).
    (void)::setpgid(child, child);
    detail::close_quietly(sv[1]);
    detail::close_quietly(ep[1]);
    r.child.pid_ = child;
    r.child.pid_value_ = static_cast<std::int64_t>(child);
    r.child.io_ = sv[0];
    r.child.err_ = ep[0];
    r.started = true;
    return r;
}

#endif

} // namespace zengine::neovim

#endif // ZENGINE_NEOVIM_CHILD_HPP
