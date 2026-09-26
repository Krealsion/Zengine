// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_BUILDER_RUN_HPP
#define ZENGINE_BUILDER_RUN_HPP

// The one place in Zengine that starts an operating-system process, and holds it after the
// call that started it returns. Not a shell: `start_recipe` takes a program and an argument
// vector, and nothing anywhere takes a command line. `RunningRecipe` is the custody, move-only
// and reaped exactly once, and a `look()` is bounded and never blocks. No environment authoring,
// no isolation, no stdin, no cancel, no timeout: the child is exactly as privileged as the host.
// Builder law: agents/realization.md

// One platform asymmetry, kept: on Windows `CreateProcess` fails where it is called; on POSIX
// `execvp` fails inside a child whose only channel left is its exit status, so 127 (not found)
// and 126 (a directory it could not enter) are turned back into "never ran" here. A build that
// genuinely exits 127 therefore reads as never having started.

#include "builder/recipe.hpp"

#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <thread>
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
#else
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace zengine::builder {

/// What a run came to. `started` and `status` are two facts: "the compiler said no" and "there
/// is no compiler" are different things to tell a maker.
struct RunResult {
    bool started = false;
    std::int64_t status = 0;
    std::string output; ///< the child's stdout and stderr, interleaved, bounded below
    std::string trouble; ///< why it never started, when it did not
};

/// The most of a child's output `run_recipe` keeps; the oldest bytes go, since the end says what
/// went wrong.
inline constexpr std::size_t kMaxCaptured = 64u * 1024u;

/// The most one `look()` drains: it bounds a look, not a build, and what is left waits in the
/// pipe. Large enough to drain an ordinary build every look -- a pipe's own buffer is typically
/// 64 KiB, and a child that fills it blocks on its own write between looks.
inline constexpr std::size_t kMaxLookBytes = 64u * 1024u;

/// What one look at a held child saw, about that moment only. `fresh` is new: reading drains the
/// pipe, so no byte arrives twice. Empty `fresh` with `ended` false is a quiet build, not news.
struct RunLook {
    std::string fresh;        ///< output seen for the first time, this look
    bool ended = false;       ///< output has ENDED and the child has been reaped
    std::int64_t status = 0;  ///< its exit status; meaningful only when `ended`
    bool never_ran = false;   ///< it ended because the program was never entered
    std::string trouble;      ///< ...and why, when `never_ran`
};

namespace detail {

inline void append_bounded(std::string& into, const char* data, std::size_t n) {
    into.append(data, n);
    if (into.size() > kMaxCaptured) {
        into.erase(0, into.size() - kMaxCaptured);
    }
}

#if defined(_WIN32)
/// One argument spelled by the Windows command-line convention: `CreateProcess` takes one string
/// that every child takes apart by that rule (backslashes literal except before a quote, where
/// they double), so composing it correctly is the argument vector on this platform.
inline std::string windows_quote(const std::string& arg) {
    const bool needs = arg.empty() || arg.find_first_of(" \t\n\v\"") != std::string::npos;
    if (!needs) {
        return arg;
    }
    std::string out = "\"";
    for (std::size_t i = 0; i < arg.size(); ++i) {
        std::size_t slashes = 0;
        while (i < arg.size() && arg[i] == '\\') {
            ++slashes;
            ++i;
        }
        if (i == arg.size()) {
            out.append(slashes * 2, '\\');
            break;
        }
        if (arg[i] == '"') {
            out.append(slashes * 2 + 1, '\\');
        } else {
            out.append(slashes, '\\');
        }
        out.push_back(arg[i]);
    }
    out.push_back('"');
    return out;
}
#endif

} // namespace detail

struct RecipeStart;

/// Custody of one live child process: its handle and the read end of its output pipe, released
/// exactly once (move-only; a moved-from handle holds nothing). It does not need the stack frame
/// that made it, so a build outlives the handler that started it. Not a Zen shape and never
/// reachable from the bus: a poke that could write a pid would be process control.
class RunningRecipe {
public:
    RunningRecipe() = default;

    RunningRecipe(const RunningRecipe&) = delete;
    RunningRecipe& operator=(const RunningRecipe&) = delete;

    RunningRecipe(RunningRecipe&& other) noexcept { adopt(other); }
    RunningRecipe& operator=(RunningRecipe&& other) noexcept {
        if (this != &other) {
            abandon();
            adopt(other);
        }
        return *this;
    }

    /// Losing the holder loses the child: terminated and reaped, never orphaned or a zombie. It
    /// authors nothing; a destructor has no `Mail`.
    ~RunningRecipe() { abandon(); }

    /// Is there still a child here? False before a start, after an ending was
    /// observed, after `abandon()`, and in a moved-from handle.
    bool holds() const noexcept {
#if defined(_WIN32)
        return process_ != nullptr;
#else
        return child_ >= 0;
#endif
    }

    /// WHAT IS NEWLY TRUE RIGHT NOW. Never blocks; never waits for the child;
    /// never reports an ending it has not reaped.
    RunLook look() {
        RunLook seen;
        if (!holds()) {
            return seen;
        }
        drain(seen);
        if (output_open()) {
            // Still speaking: a failing build writes its last words before it exits, and an
            // ending taken now would be taken before they were read.
            return seen;
        }
        reap_if_done(seen);
        return seen;
    }

    /// End custody now: terminate, reap and close, exactly once. It says only that the holder
    /// stopped holding -- not that the build was cancelled, failed or finished; a caller wanting
    /// such a fact must author it while it can.
    void abandon() {
#if defined(_WIN32)
        if (out_ != nullptr) {
            ::CloseHandle(out_);
            out_ = nullptr;
        }
        if (process_ != nullptr) {
            (void)::TerminateProcess(process_, 1);
            (void)::WaitForSingleObject(process_, INFINITE);
            ::CloseHandle(process_);
            process_ = nullptr;
        }
#else
        if (out_ >= 0) {
            ::close(out_);
            out_ = -1;
        }
        if (child_ >= 0) {
            (void)::kill(child_, SIGKILL);
            int ignored = 0;
            while (::waitpid(child_, &ignored, 0) < 0 && errno == EINTR) {
            }
            child_ = -1;
        }
#endif
    }

private:
    friend RecipeStart start_recipe(const BuildCommand&);

    bool output_open() const noexcept {
#if defined(_WIN32)
        return out_ != nullptr;
#else
        return out_ >= 0;
#endif
    }

    void adopt(RunningRecipe& other) noexcept {
        program_ = std::move(other.program_);
        dir_ = std::move(other.dir_);
#if defined(_WIN32)
        process_ = other.process_;
        out_ = other.out_;
        other.process_ = nullptr;
        other.out_ = nullptr;
#else
        child_ = other.child_;
        out_ = other.out_;
        other.child_ = -1;
        other.out_ = -1;
#endif
    }

    void drain(RunLook& seen) {
        if (!output_open()) {
            return;
        }
        char buffer[4096];
        std::size_t taken = 0;
#if defined(_WIN32)
        while (taken < kMaxLookBytes) {
            DWORD available = 0;
            if (::PeekNamedPipe(out_, nullptr, 0, nullptr, &available, nullptr) == 0) {
                // The last writer let go: this is end of output, and it is the
                // only form of it an anonymous pipe reports on this platform.
                ::CloseHandle(out_);
                out_ = nullptr;
                return;
            }
            if (available == 0) {
                return; // nothing right now -- and asking again is the next look's job
            }
            const DWORD want = available < static_cast<DWORD>(sizeof(buffer))
                                   ? available
                                   : static_cast<DWORD>(sizeof(buffer));
            DWORD got = 0;
            if (::ReadFile(out_, buffer, want, &got, nullptr) == 0 || got == 0) {
                ::CloseHandle(out_);
                out_ = nullptr;
                return;
            }
            seen.fresh.append(buffer, static_cast<std::size_t>(got));
            taken += static_cast<std::size_t>(got);
        }
#else
        while (taken < kMaxLookBytes) {
            const ::ssize_t got = ::read(out_, buffer, sizeof(buffer));
            if (got > 0) {
                seen.fresh.append(buffer, static_cast<std::size_t>(got));
                taken += static_cast<std::size_t>(got);
                continue;
            }
            if (got < 0 && errno == EINTR) {
                continue;
            }
            if (got < 0) {
                return; // EAGAIN: the pipe is open and empty, which is not an ending
            }
            ::close(out_); // 0 bytes from a non-blocking read: every writer let go
            out_ = -1;
            return;
        }
#endif
    }

    void reap_if_done(RunLook& seen) {
#if defined(_WIN32)
        if (::WaitForSingleObject(process_, 0) != WAIT_OBJECT_0) {
            return; // finished writing, not yet exited
        }
        DWORD code = 0;
        (void)::GetExitCodeProcess(process_, &code);
        ::CloseHandle(process_);
        process_ = nullptr;
        seen.ended = true;
        seen.status = static_cast<std::int64_t>(code);
        // No never-ran translation here: CreateProcess had already failed, or
        // had not, before start_recipe returned. See this file's header.
#else
        int wait_status = 0;
        ::pid_t done = 0;
        while ((done = ::waitpid(child_, &wait_status, WNOHANG)) < 0 && errno == EINTR) {
        }
        if (done != child_) {
            return; // finished writing, not yet exited
        }
        child_ = -1;
        seen.ended = true;
        if (WIFEXITED(wait_status)) {
            seen.status = static_cast<std::int64_t>(WEXITSTATUS(wait_status));
        } else if (WIFSIGNALED(wait_status)) {
            // A signalled child has no exit status: 128 + the signal, the shell's convention,
            // since 0 would read as success.
            seen.status = 128 + static_cast<std::int64_t>(WTERMSIG(wait_status));
        } else {
            seen.status = -1;
        }
        // 127 and 126 are the child's report that exec never happened (the header says why).
        if (seen.status == 127) {
            seen.never_ran = true;
            seen.trouble = "could not run `" + program_ + "` (not found, or not executable)";
        } else if (seen.status == 126) {
            seen.never_ran = true;
            seen.trouble = "could not enter `" + dir_ + "`";
        }
#endif
    }

    std::string program_; ///< only ever used to write the sentence above
    std::string dir_;     ///< likewise
#if defined(_WIN32)
    HANDLE process_ = nullptr;
    HANDLE out_ = nullptr;
#else
    ::pid_t child_ = -1;
    int out_ = -1;
#endif
};

/// Did a process begin, and its custody if one did. `started` false leaves nothing running.
struct RecipeStart {
    bool started = false;
    std::string trouble; ///< why nothing began, when nothing did
    RunningRecipe process;
};

/// Start one recipe and HAND BACK CUSTODY of it. Returns as soon as the child
/// exists; it does not wait for a single byte.
inline RecipeStart start_recipe(const BuildCommand& command) {
    RecipeStart out;
    if (command.program.empty()) {
        out.trouble = "this command names no program";
        return out;
    }
    out.process.program_ = command.program;
    out.process.dir_ = command.dir;

#if defined(_WIN32)
    SECURITY_ATTRIBUTES inheritable{};
    inheritable.nLength = sizeof(inheritable);
    inheritable.bInheritHandle = TRUE;
    inheritable.lpSecurityDescriptor = nullptr;

    HANDLE read_end = nullptr;
    HANDLE write_end = nullptr;
    if (::CreatePipe(&read_end, &write_end, &inheritable, 0) == 0) {
        out.trouble = "could not make a pipe for the build's output";
        return out;
    }
    // The READ end is ours alone. Without this the child holds a copy, the pipe
    // never reaches end-of-file, and this build could never be observed to end.
    ::SetHandleInformation(read_end, HANDLE_FLAG_INHERIT, 0);

    std::string line = detail::windows_quote(command.program);
    for (const std::string& a : command.args) {
        line += ' ';
        line += detail::windows_quote(a);
    }
    std::vector<char> mutable_command(line.begin(), line.end());
    mutable_command.push_back('\0');

    STARTUPINFOA startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdOutput = write_end;
    startup.hStdError = write_end;
    startup.hStdInput = ::GetStdHandle(STD_INPUT_HANDLE);
    PROCESS_INFORMATION process{};

    const BOOL began = ::CreateProcessA(
        nullptr, mutable_command.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr,
        command.dir.empty() ? nullptr : command.dir.c_str(), &startup, &process);
    ::CloseHandle(write_end); // ours is closed either way: a child's copy is the writer now
    if (began == 0) {
        ::CloseHandle(read_end);
        out.trouble = "could not start `" + command.program + "` (error " +
                      std::to_string(static_cast<std::int64_t>(::GetLastError())) + ")";
        return out;
    }
    ::CloseHandle(process.hThread); // never used: this custody waits on the PROCESS
    out.process.process_ = process.hProcess;
    out.process.out_ = read_end;
    out.started = true;
    return out;
#else
    int pipe_ends[2] = {-1, -1};
    if (::pipe(pipe_ends) != 0) {
        out.trouble = "could not make a pipe for the build's output";
        return out;
    }

    // The argument vector, built BEFORE the fork. Everything between fork and
    // exec runs in a child that shares this address space and must not allocate;
    // building the vector here is what keeps that stretch to dup2/close/chdir.
    std::vector<char*> argv;
    argv.reserve(command.args.size() + 2);
    std::string program = command.program;
    argv.push_back(program.data());
    std::vector<std::string> args = command.args;
    for (std::string& a : args) {
        argv.push_back(a.data());
    }
    argv.push_back(nullptr);
    const std::string dir = command.dir;
    const bool has_slash = program.find('/') != std::string::npos;

    const ::pid_t child = ::fork();
    if (child < 0) {
        ::close(pipe_ends[0]);
        ::close(pipe_ends[1]);
        out.trouble = "could not start a process for `" + command.program + "`";
        return out;
    }
    if (child == 0) {
        ::close(pipe_ends[0]);
        // Both streams go down one pipe, interleaved as written: an error and the progress
        // line before it belong together.
        (void)::dup2(pipe_ends[1], 1);
        (void)::dup2(pipe_ends[1], 2);
        ::close(pipe_ends[1]);
        if (!dir.empty() && ::chdir(dir.c_str()) != 0) {
            ::_exit(126);
        }
        if (has_slash) {
            (void)::execv(argv[0], argv.data());
        } else {
            (void)::execvp(argv[0], argv.data());
        }
        ::_exit(127); // exec returned, so it failed; 127 is the shell's own word for it
    }

    ::close(pipe_ends[1]);
    // Non-blocking from the first look: otherwise a read on a quiet build waits for the build.
    const int flags = ::fcntl(pipe_ends[0], F_GETFL, 0);
    (void)::fcntl(pipe_ends[0], F_SETFL, (flags < 0 ? 0 : flags) | O_NONBLOCK);
    out.process.child_ = child;
    out.process.out_ = pipe_ends[0];
    out.started = true;
    return out;
#endif
}

/// Run one recipe to completion and answer what happened. It blocks, and nothing in production
/// calls it: it is the control the `builder` suite's blocking-runner canary drives, written over
/// the held primitive so the platform work has one implementation. The 1 ms nap makes it a
/// wait, not a spin.
inline RunResult run_recipe(const BuildCommand& command) {
    RunResult result;
    RecipeStart begun = start_recipe(command);
    if (!begun.started) {
        result.trouble = begun.trouble;
        return result;
    }
    result.started = true;
    for (;;) {
        const RunLook seen = begun.process.look();
        if (!seen.fresh.empty()) {
            detail::append_bounded(result.output, seen.fresh.data(), seen.fresh.size());
        }
        if (seen.ended) {
            result.status = seen.status;
            if (seen.never_ran) {
                result.started = false;
                result.trouble = seen.trouble;
            }
            return result;
        }
        if (seen.fresh.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}

/// Run one command to completion, handing its output on to `to` as it arrives and keeping none.
/// It blocks: for a process whose words belong to whoever watches this one -- the development
/// launch (`workshop/develop.hpp`), which is no weave. Its nap is a frame long, since what it
/// holds may run for hours.
inline RunResult run_forwarding(const BuildCommand& command, std::FILE* to) {
    RunResult result;
    RecipeStart begun = start_recipe(command);
    if (!begun.started) {
        result.trouble = begun.trouble;
        return result;
    }
    result.started = true;
    for (;;) {
        const RunLook seen = begun.process.look();
        if (!seen.fresh.empty()) {
            (void)std::fwrite(seen.fresh.data(), 1, seen.fresh.size(), to);
            (void)std::fflush(to);
        }
        if (seen.ended) {
            result.status = seen.status;
            if (seen.never_ran) {
                result.started = false;
                result.trouble = seen.trouble;
            }
            return result;
        }
        if (seen.fresh.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }
    }
}

} // namespace zengine::builder

#endif // ZENGINE_BUILDER_RUN_HPP
