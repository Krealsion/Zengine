// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_BUILDER_RUN_HPP
#define ZENGINE_BUILDER_RUN_HPP

// The one place in Zengine that starts an operating-system process.
//
// IT IS NOT A SHELL, AND THE INTERFACE IS WHAT MAKES THAT TRUE. `run_recipe`
// takes a PROGRAM and an ARGUMENT VECTOR that were chosen by the host, and there
// is no overload, no convenience and no field anywhere that takes a command
// LINE. Nothing composes a string that a shell then takes apart again, so the
// entire family of "a quote in the wrong place became an extra command" cannot
// occur here — not because the arguments are checked, but because there is no
// shell in the picture to check them for.
//
// That was a decision with a cheaper alternative. `popen()` is four lines and
// would have run this build perfectly well; it is also, exactly, a general shell
// capability, and BLD-0's whole job is to give Builder the smallest bounded
// mechanism it can be built on rather than the most convenient one. A later
// phase that genuinely needs a shell can add one and argue for it; it will not
// find one already sitting here having arrived as a side effect of a button.
//
// WHAT IT DOES NOT DO, and each absence is the same restraint:
//
//   - no environment authoring. The child inherits this process's environment.
//     Choosing what a child may see of it is the exec-boundary question Loom's
//     isolation host answers properly (Loom `docs/reference/capabilities.md`);
//     inventing a second, weaker answer here would be worse than inheriting
//     visibly.
//   - no isolation, no containment, no resource bound. This is an ordinary
//     child process of an ordinary host process, and it is exactly as
//     privileged as the Workshop that started it. Said out loud because that is
//     the honest description; the alternative — implying a boundary that is not
//     there — is the one thing this repository's phases keep refusing to do.
//   - no stdin. The child inherits it, and a build that decides to ask a
//     question will therefore wait for an answer that no maker can see it
//     asking. BLD-0's answer to that is to choose a recipe that does not ask
//     one, and to say here that nothing prevents it.
//   - no cancel and no timeout. `run_recipe` returns when the child exits.
//
// IT BLOCKS. The caller's thread is inside this function for as long as the
// build takes, and in BLD-0 that caller is a weave handler on the bus the
// Workshop is pumping — so the tool freezes for the duration. That is a real
// cost, it is the pressure this phase set out to feel, and it is measured in the
// report rather than papered over with a thread whose lifetime nobody has
// designed yet.

#include <cerrno>
#include <cstddef>
#include <cstdint>
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
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace zengine::builder {

/// One buildable thing, as the HOST describes it.
///
/// `target` is the NAME the rest of the world uses; everything else is the part
/// only the runner ever sees. A recipe is data, not a capability — holding one
/// does nothing — but the runner is the only weave that is given any, which is
/// the arrangement that makes "the tool holds a name, the runner holds a
/// command" structurally true instead of merely stated.
struct BuildRecipe {
    std::string target;             ///< what a maker and the tool call this
    std::string program;            ///< an executable, resolved by the host
    std::vector<std::string> args;  ///< its arguments, already separated
    std::string dir;                ///< the working directory to run it in ("" = inherit)

    /// The recipe as one readable line, for a maker who wants to know what a
    /// button actually did. Deliberately not a re-runnable command line: it is a
    /// description, and nothing parses it back.
    std::string as_line() const {
        std::string line = program;
        for (const std::string& a : args) {
            line += ' ';
            line += a;
        }
        return line;
    }
};

/// What a run came to.
///
/// `started` and `status` are two facts and not one, because "the compiler said
/// no" and "there is no compiler" are different things to tell a maker, and a
/// single non-zero number cannot say which happened.
struct RunResult {
    bool started = false;
    std::int64_t status = 0;
    std::string output; ///< the child's stdout and stderr, interleaved, bounded below
    std::string trouble; ///< why it never started, when it did not
};

/// The most of a child's output this keeps. A build is chatty and a panel shows
/// a handful of lines, so the whole of it is never needed; what IS needed is
/// that a talkative build cannot grow this process without bound. The OLDEST
/// bytes are dropped, because the end of a build's output is the part that says
/// what went wrong.
inline constexpr std::size_t kMaxCaptured = 64u * 1024u;

namespace detail {

inline void append_bounded(std::string& into, const char* data, std::size_t n) {
    into.append(data, n);
    if (into.size() > kMaxCaptured) {
        into.erase(0, into.size() - kMaxCaptured);
    }
}

#if defined(_WIN32)
/// One argument, as the Windows command-line convention needs it spelled.
///
/// Windows has no argument vector at the process boundary: `CreateProcess` takes
/// a string and every child takes it apart again with the same convention, so
/// composing that string correctly IS the argument vector on this platform. This
/// is the documented rule (backslashes are literal except before a quote, where
/// they double), applied to values the HOST chose — a path with a space in it is
/// the case that actually occurs.
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

/// Run one recipe to completion and answer what happened.
inline RunResult run_recipe(const BuildRecipe& recipe) {
    RunResult result;
    if (recipe.program.empty()) {
        result.trouble = "this recipe names no program";
        return result;
    }

#if defined(_WIN32)
    SECURITY_ATTRIBUTES inheritable{};
    inheritable.nLength = sizeof(inheritable);
    inheritable.bInheritHandle = TRUE;
    inheritable.lpSecurityDescriptor = nullptr;

    HANDLE read_end = nullptr;
    HANDLE write_end = nullptr;
    if (::CreatePipe(&read_end, &write_end, &inheritable, 0) == 0) {
        result.trouble = "could not make a pipe for the build's output";
        return result;
    }
    // The READ end is ours alone. Without this the child holds a copy, the pipe
    // never reaches end-of-file, and the read below waits forever for a writer
    // that is this very process.
    ::SetHandleInformation(read_end, HANDLE_FLAG_INHERIT, 0);

    std::string command = detail::windows_quote(recipe.program);
    for (const std::string& a : recipe.args) {
        command += ' ';
        command += detail::windows_quote(a);
    }
    std::vector<char> mutable_command(command.begin(), command.end());
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
        recipe.dir.empty() ? nullptr : recipe.dir.c_str(), &startup, &process);
    ::CloseHandle(write_end); // ours is closed either way: a child's copy is the writer now
    if (began == 0) {
        ::CloseHandle(read_end);
        result.trouble = "could not start `" + recipe.program + "` (error " +
                         std::to_string(static_cast<std::int64_t>(::GetLastError())) + ")";
        return result;
    }
    result.started = true;

    char buffer[4096];
    DWORD got = 0;
    while (::ReadFile(read_end, buffer, static_cast<DWORD>(sizeof(buffer)), &got, nullptr) != 0 &&
           got > 0) {
        detail::append_bounded(result.output, buffer, static_cast<std::size_t>(got));
    }
    ::CloseHandle(read_end);

    ::WaitForSingleObject(process.hProcess, INFINITE);
    DWORD code = 0;
    ::GetExitCodeProcess(process.hProcess, &code);
    result.status = static_cast<std::int64_t>(code);
    ::CloseHandle(process.hProcess);
    ::CloseHandle(process.hThread);
    return result;
#else
    int pipe_ends[2] = {-1, -1};
    if (::pipe(pipe_ends) != 0) {
        result.trouble = "could not make a pipe for the build's output";
        return result;
    }

    // The argument vector, built BEFORE the fork. Everything between fork and
    // exec runs in a child that shares this address space and must not allocate;
    // building the vector here is what keeps that stretch to dup2/close/chdir.
    std::vector<char*> argv;
    argv.reserve(recipe.args.size() + 2);
    std::string program = recipe.program;
    argv.push_back(program.data());
    std::vector<std::string> args = recipe.args;
    for (std::string& a : args) {
        argv.push_back(a.data());
    }
    argv.push_back(nullptr);
    const std::string dir = recipe.dir;
    const bool has_slash = program.find('/') != std::string::npos;

    const ::pid_t child = ::fork();
    if (child < 0) {
        ::close(pipe_ends[0]);
        ::close(pipe_ends[1]);
        result.trouble = "could not start a process for `" + recipe.program + "`";
        return result;
    }
    if (child == 0) {
        ::close(pipe_ends[0]);
        // Both streams go down the one pipe, interleaved as the child wrote
        // them: a compiler's error and the line of progress before it belong
        // together, and two pipes would let a panel show them in an order that
        // never happened.
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
    result.started = true;
    char buffer[4096];
    for (;;) {
        const ::ssize_t got = ::read(pipe_ends[0], buffer, sizeof(buffer));
        if (got > 0) {
            detail::append_bounded(result.output, buffer, static_cast<std::size_t>(got));
            continue;
        }
        if (got < 0 && errno == EINTR) {
            continue;
        }
        break;
    }
    ::close(pipe_ends[0]);

    // DRAINED FIRST, WAITED SECOND, and the order is the whole of why this does
    // not hang: a child that fills the pipe blocks writing, and a parent that
    // waited first would be waiting for a child that is waiting for the parent.
    int wait_status = 0;
    while (::waitpid(child, &wait_status, 0) < 0 && errno == EINTR) {
    }
    if (WIFEXITED(wait_status)) {
        result.status = static_cast<std::int64_t>(WEXITSTATUS(wait_status));
    } else if (WIFSIGNALED(wait_status)) {
        // A signalled child has no exit status, so it is given one that cannot
        // be mistaken for a build's own — the shell's convention, for the same
        // reason: 0 would read as success and there is no honest small number.
        result.status = 128 + static_cast<std::int64_t>(WTERMSIG(wait_status));
    } else {
        result.status = -1;
    }
    // 127 and 126 are the child's report that exec never happened. They arrive
    // as an exit status because that is the only channel a failed exec has, and
    // they are turned back into the fact they describe here rather than being
    // shown to a maker as a build that failed.
    if (result.status == 127) {
        result.started = false;
        result.trouble = "could not run `" + recipe.program + "` (not found, or not executable)";
    } else if (result.status == 126) {
        result.started = false;
        result.trouble = "could not enter `" + recipe.dir + "`";
    }
    return result;
#endif
}

} // namespace zengine::builder

#endif // ZENGINE_BUILDER_RUN_HPP
