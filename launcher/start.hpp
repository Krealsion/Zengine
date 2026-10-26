// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_LAUNCHER_START_HPP
#define ZENGINE_LAUNCHER_START_HPP

// A PROCESS STARTED DETACHED AND KEPT BY NOBODY (launcher/docs/zengine.launcher.md): it outlives
// whoever started it. On Windows it has a console and a process group of its own, inherits no
// handle, and breaks away from a job where the job allows it; on POSIX it leads a session of its
// own, its input empty, its output appended to a log, and no other descriptor open. Not a shell: a
// program and an argument vector. The caller learns its id and nothing after.

#include <cerrno>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#if defined(__linux__)
#include <sys/syscall.h>
#endif
#endif

namespace zengine::launcher {

/// What a start came to: the process's id, or why it never ran.
struct Started {
    bool ok = false;
    std::int64_t pid = 0;
    std::string trouble;
};

/// What to start: the program, its arguments, the directory it starts in, and on POSIX the log its
/// output is appended to (a Windows start has a console of its own).
struct StartPlan {
    std::string program;
    std::vector<std::string> args;
    std::string dir;
    std::string log;
};

namespace detail {

#if defined(_WIN32)
inline std::wstring wide(const std::string& utf8) {
    if (utf8.empty()) {
        return {};
    }
    const int n = ::MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()),
                                        nullptr, 0);
    std::wstring out(static_cast<std::size_t>(n > 0 ? n : 0), L'\0');
    if (n > 0) {
        (void)::MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()),
                                    out.data(), n);
    }
    return out;
}

/// One argument by the Windows command-line convention: backslashes are literal except before a
/// quote, where they double.
inline std::wstring quoted(const std::wstring& arg) {
    if (!arg.empty() && arg.find_first_of(L" \t\"") == std::wstring::npos) {
        return arg;
    }
    std::wstring out = L"\"";
    std::size_t slashes = 0;
    for (const wchar_t c : arg) {
        if (c == L'\\') {
            ++slashes;
            continue;
        }
        if (c == L'"') {
            out.append(slashes * 2 + 1, L'\\');
        } else {
            out.append(slashes, L'\\');
        }
        slashes = 0;
        out += c;
    }
    out.append(slashes * 2, L'\\');
    out += L'"';
    return out;
}
#else
/// Closes every descriptor from 3 up but `keep`, between a fork and an exec.
inline void close_all_but(int keep, long open_max) {
#if defined(__linux__) && defined(SYS_close_range)
    const unsigned k = static_cast<unsigned>(keep);
    if ((k <= 3u || ::syscall(SYS_close_range, 3u, k - 1u, 0u) == 0) &&
        ::syscall(SYS_close_range, k + 1u, ~0u, 0u) == 0) {
        return;
    }
#endif
    for (long fd = 3; fd < open_max; ++fd) {
        if (fd != keep) {
            (void)::close(static_cast<int>(fd));
        }
    }
}
#endif

} // namespace detail

/// START `plan` DETACHED. Its id when it began; a program that could not run is said, on POSIX
/// from the exec's own error.
inline Started start_detached(const StartPlan& plan) {
    Started out;
#if defined(_WIN32)
    const std::wstring program = detail::wide(plan.program);
    std::wstring line = detail::quoted(program);
    for (const std::string& a : plan.args) {
        line += L' ';
        line += detail::quoted(detail::wide(a));
    }
    const std::wstring dir = detail::wide(plan.dir);
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    const DWORD own = CREATE_NEW_CONSOLE | CREATE_NEW_PROCESS_GROUP;
    const auto begin = [&](DWORD flags) {
        std::wstring writable = line; // CreateProcessW may write into its command line
        return ::CreateProcessW(program.c_str(), writable.data(), nullptr, nullptr, FALSE, flags,
                                nullptr, dir.empty() ? nullptr : dir.c_str(), &si, &pi);
    };
    // A job that forbids breaking away refuses the flag: then the process stays in it.
    BOOL began = begin(own | CREATE_BREAKAWAY_FROM_JOB);
    if (!began && ::GetLastError() == ERROR_ACCESS_DENIED) {
        began = begin(own);
    }
    if (!began) {
        out.trouble = "could not start " + plan.program + " (Windows error " +
                      std::to_string(::GetLastError()) + ")";
        return out;
    }
    out.ok = true;
    out.pid = static_cast<std::int64_t>(pi.dwProcessId);
    ::CloseHandle(pi.hThread);
    ::CloseHandle(pi.hProcess);
    return out;
#else
    std::vector<std::string> argv_store;
    argv_store.push_back(plan.program);
    argv_store.insert(argv_store.end(), plan.args.begin(), plan.args.end());
    std::vector<char*> argv;
    for (std::string& a : argv_store) {
        argv.push_back(a.data());
    }
    argv.push_back(nullptr);

    // One pipe back: the grandchild's id from the child, then nothing on a successful exec (the
    // write end closes with it) or the exec's errno.
    int back[2];
    if (::pipe(back) != 0) {
        out.trouble = std::string("could not make a pipe: ") + std::strerror(errno);
        return out;
    }
    (void)::fcntl(back[1], F_SETFD, FD_CLOEXEC);
    if (back[1] < 3) { // a starter with a standard stream closed: kept clear of the three
        const int moved = ::fcntl(back[1], F_DUPFD_CLOEXEC, 3);
        ::close(back[1]);
        back[1] = moved;
    }
    if (back[1] < 0) {
        ::close(back[0]);
        out.trouble = std::string("could not make a pipe: ") + std::strerror(errno);
        return out;
    }
    long open_max = ::sysconf(_SC_OPEN_MAX);
    if (open_max < 0 || open_max > 65536) {
        open_max = 65536;
    }
    const pid_t child = ::fork();
    if (child < 0) {
        ::close(back[0]);
        ::close(back[1]);
        out.trouble = std::string("could not fork: ") + std::strerror(errno);
        return out;
    }
    if (child == 0) {
        ::close(back[0]);
        (void)::setsid();
        const pid_t grandchild = ::fork();
        if (grandchild != 0) {
            const std::int64_t id = grandchild > 0 ? grandchild : -static_cast<std::int64_t>(errno);
            (void)!::write(back[1], &id, sizeof(id));
            ::_exit(0);
        }
        const int in = ::open("/dev/null", O_RDONLY);
        if (in >= 0) {
            (void)::dup2(in, 0);
        }
        const int log = plan.log.empty() ? -1
                                         : ::open(plan.log.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
        const int sink = log >= 0 ? log : ::open("/dev/null", O_WRONLY);
        if (sink >= 0) {
            (void)::dup2(sink, 1);
            (void)::dup2(sink, 2);
        }
        // Nothing of the starter's goes on but those three: the pipe back closes at the exec.
        detail::close_all_but(back[1], open_max);
        if (!plan.dir.empty() && ::chdir(plan.dir.c_str()) != 0) {
            const int e = errno;
            (void)!::write(back[1], &e, sizeof(e));
            ::_exit(127);
        }
        ::execv(plan.program.c_str(), argv.data());
        const int e = errno;
        (void)!::write(back[1], &e, sizeof(e));
        ::_exit(127);
    }
    ::close(back[1]);
    int status = 0;
    (void)::waitpid(child, &status, 0);
    std::int64_t id = 0;
    const ssize_t got = ::read(back[0], &id, sizeof(id));
    int failed = 0;
    const ssize_t said = got == static_cast<ssize_t>(sizeof(id)) ? ::read(back[0], &failed, sizeof(failed)) : 0;
    ::close(back[0]);
    if (got != static_cast<ssize_t>(sizeof(id)) || id <= 0) {
        out.trouble = "could not start " + plan.program + ": " +
                      std::strerror(id < 0 ? static_cast<int>(-id) : EAGAIN);
        return out;
    }
    if (said == static_cast<ssize_t>(sizeof(failed))) {
        out.trouble = "could not start " + plan.program + ": " + std::strerror(failed);
        return out;
    }
    out.ok = true;
    out.pid = id;
    return out;
#endif
}

/// DOES THE PROCESS `pid` RUN NOW, a program whose file is named `program_file`? Asked of the
/// system, never remembered: on Windows its exit is still to come and its image has that name; on
/// Linux its `/proc` entry's program has that name; elsewhere it answers a signal of none.
inline bool runs_program(std::int64_t pid, const std::string& program_file) {
    if (pid <= 0) {
        return false;
    }
#if defined(_WIN32)
    HANDLE h = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid));
    if (h == nullptr) {
        return false;
    }
    DWORD code = 0;
    const bool alive = ::GetExitCodeProcess(h, &code) != 0 && code == STILL_ACTIVE;
    wchar_t image[1024];
    DWORD size = static_cast<DWORD>(sizeof(image) / sizeof(image[0]));
    const bool read = ::QueryFullProcessImageNameW(h, 0, image, &size) != 0;
    ::CloseHandle(h);
    if (!alive || !read) {
        return false;
    }
    const std::wstring path(image, size);
    const std::size_t slash = path.find_last_of(L"\\/");
    const std::wstring name = slash == std::wstring::npos ? path : path.substr(slash + 1);
    const std::wstring wanted = detail::wide(program_file);
    return ::CompareStringOrdinal(name.c_str(), static_cast<int>(name.size()), wanted.c_str(),
                                  static_cast<int>(wanted.size()), TRUE) == CSTR_EQUAL;
#elif defined(__linux__)
    char target[4096];
    const std::string link = "/proc/" + std::to_string(pid) + "/exe";
    const ssize_t n = ::readlink(link.c_str(), target, sizeof(target) - 1);
    if (n <= 0) {
        return false;
    }
    const std::string path(target, static_cast<std::size_t>(n));
    const std::size_t slash = path.find_last_of('/');
    return (slash == std::string::npos ? path : path.substr(slash + 1)) == program_file;
#else
    (void)program_file;
    return ::kill(static_cast<pid_t>(pid), 0) == 0 || errno == EPERM;
#endif
}

} // namespace zengine::launcher

#endif // ZENGINE_LAUNCHER_START_HPP
