// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_BUILDER_RUN_HPP
#define ZENGINE_BUILDER_RUN_HPP

// The one place in Zengine that starts an operating-system process, and the one
// place that HOLDS one after the call that started it has returned.
//
// IT IS NOT A SHELL, AND THE INTERFACE IS WHAT MAKES THAT TRUE. `start_recipe`
// takes a `BuildCommand` -- a PROGRAM and an ARGUMENT VECTOR -- and there is no
// overload, no convenience and no field anywhere that takes a command LINE.
// Nothing composes a string that a shell then takes apart again, so the entire
// family of "a quote in the wrong place became an extra command" cannot occur
// here -- not because the arguments are checked, but because there is no shell
// in the picture to check them for.
//
// WHERE THE PROGRAM COMES FROM MOVED ONCE, AND ONLY ONCE (BLD-1). It used to be
// a catalog entry the host wrote at configure time; it is now derived from an
// AUTHORED RECIPE by `builder/generate.hpp`, which still puts the host's own
// CMake in `program` and can put nothing else there. What a recipe file can name
// is INPUTS to a mechanism this package already holds -- never a program, never
// an argument vector, never a shell line -- so the authority this file exposes is
// exactly what it always was.
//
// That was a decision with a cheaper alternative. `popen()` is four lines and
// would have run this build perfectly well; it is also, exactly, a general shell
// capability, and BLD-0's whole job was to give Builder the smallest bounded
// mechanism it can be built on rather than the most convenient one. ASYNC-1
// widens the LIFETIME of that mechanism and not one inch of its authority: the
// same recipe struct, the same host-chosen program, the same absent shell.
//
// ---------------------------------------------------------------------------
// THE HELD LIFETIME (ASYNC-1), which is the whole of what this phase added here.
//
// BLD-0 had one verb -- `run recipe -> wait -> result` -- and its cost was
// measured rather than argued: the caller is a weave handler on the bus the
// Workshop is pumping, so the whole application stopped until the child exited.
// The replacement is three moments instead of one, and the middle one is the
// point:
//
//     start_recipe(recipe)   -> a RunningRecipe, OWNED by the caller
//     ...the caller returns. the handler's stack frame is gone...
//     process.look()         -> what is newly visible RIGHT NOW, never blocking
//     ...many ordinary turns later...
//     process.look()         -> ...ended, with a status, reaped
//
// `RunningRecipe` is the custody. It is MOVE-ONLY and it reaps in its
// destructor, so "exactly once" is a property of the type rather than a
// discipline its user has to keep. Nothing here polls on its own, owns a thread,
// sleeps, or knows what a Loom is: the holder decides when to look, and the
// holder is an ordinary weave (builder/runner.hpp).
//
// A LOOK IS BOUNDED AND NEVER BLOCKS, both halves deliberately. It reads at most
// `kMaxLookBytes` before answering, so a chatty build cannot make one look long;
// and the read end is non-blocking (O_NONBLOCK / PeekNamedPipe), so a quiet
// build cannot make one look slow. The whole cost of asking a running build what
// it has said is a handful of syscalls that return immediately.
//
// THE ENDING IS REPORTED ONLY AFTER THE OUTPUT HAS ENDED. `look()` will not say
// `ended` while the pipe is still open, even when the child has already exited
// -- because a child's last words are written before it exits and read after it,
// and an ending that raced the drain would throw away exactly the lines that say
// what went wrong. Output end first, reap second, ending last.
//
// WHAT IT DOES NOT DO, and each absence is the same restraint:
//
//   - no environment authoring. The child inherits this process's environment.
//     Choosing what a child may see of it is the exec-boundary question Loom's
//     isolation host answers properly (the Loom's own capabilities reference --
//     cited by NAME rather than by path, because a comment's `*.md` path resolves
//     against THIS repository's root and a stranger's clone has no sibling Loom);
//     inventing a second, weaker answer here would be worse than inheriting
//     visibly.
//   - no isolation, no containment, no resource bound. This is an ordinary
//     child process of an ordinary host process, and it is exactly as
//     privileged as the Workshop that started it. Said out loud because that is
//     the honest description; the alternative -- implying a boundary that is not
//     there -- is the one thing this repository's phases keep refusing to do.
//   - no stdin. The child inherits it, and a build that decides to ask a
//     question will therefore wait for an answer that no maker can see it
//     asking. The answer is still to choose a recipe that does not ask one, and
//     to say here that nothing prevents it.
//   - NO CANCEL, and `abandon()` is not one. It is the cleanup a destructor
//     needs: it terminates and reaps so that a holder going away leaves no child
//     of this process running and no zombie behind it. It makes no claim about
//     the build -- nothing here authors a fact, and "the holder stopped holding"
//     is not "the build was cancelled". builder/runner.hpp is where that
//     distinction is spent.
//   - no timeout. A running operation is simply running; how long is too long is
//     an observer's judgement about itself, and no observer here makes one.
//
// ---------------------------------------------------------------------------
// ONE PLATFORM ASYMMETRY, MEASURED AND NOT SMOOTHED OVER. On Windows,
// `CreateProcess` fails where it is called, so "this program is not there" is
// known before `start_recipe` returns. On POSIX, `fork` succeeds and `execvp`
// fails inside a child that has no way left to speak except its exit status --
// so the same fact arrives LATER, as an ending with status 127 (or 126 for a
// working directory that could not be entered), and `RunLook::never_ran` is
// where it is turned back into what it describes. The shell's own convention,
// used for the reason the shell uses it: those two numbers are the only channel
// a failed exec has. A build that genuinely exits 127 is therefore reported as
// never having started -- the cost of that channel being shared, and BLD-0's
// judgement kept rather than a new one invented.

#include "builder/recipe.hpp"

#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstddef>
#include <cstdint>
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

/// The most of a child's output `run_recipe` keeps. A build is chatty and a
/// panel shows a handful of lines, so the whole of it is never needed; what IS
/// needed is that a talkative build cannot grow this process without bound. The
/// OLDEST bytes are dropped, because the end of a build's output is the part
/// that says what went wrong.
inline constexpr std::size_t kMaxCaptured = 64u * 1024u;

/// The most one `look()` drains before answering.
///
/// IT BOUNDS A LOOK, NOT A BUILD. Whatever is left stays in the pipe and is
/// there at the next look, so nothing is lost -- what this buys is that the cost
/// of asking is a property of the ASKING and not of how loud the child is. It is
/// large enough that an ordinary build is fully drained every time (a pipe's own
/// buffer is typically 64 KiB, so a much smaller number here would let a chatty
/// child fill the pipe and block on its own write between looks), and small
/// enough that one look is a bounded number of immediate syscalls.
inline constexpr std::size_t kMaxLookBytes = 64u * 1024u;

/// WHAT ONE LOOK AT A HELD CHILD SAW -- an observation, and only ever about the
/// moment it was taken.
///
/// `fresh` is what has not been reported before: the pipe is drained BY reading
/// it, so bytes handed over here are gone from the pipe and cannot arrive twice.
/// An empty `fresh` with `ended` false is the ordinary answer while a build is
/// compiling quietly, and it is not news.
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
/// One argument, as the Windows command-line convention needs it spelled.
///
/// Windows has no argument vector at the process boundary: `CreateProcess` takes
/// a string and every child takes it apart again with the same convention, so
/// composing that string correctly IS the argument vector on this platform. This
/// is the documented rule (backslashes are literal except before a quote, where
/// they double), applied to values the HOST chose -- a path with a space in it is
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

struct RecipeStart;

/// CUSTODY OF ONE LIVE CHILD PROCESS -- the thing ASYNC-1 exists to give Builder.
///
/// It owns an operating-system process handle (a `pid_t` on POSIX, a process
/// HANDLE on Windows) and the read end of the one pipe its output arrives on,
/// and it releases both EXACTLY ONCE. That "exactly once" is the type's job and
/// not its user's: it is move-only, a moved-from handle holds nothing, and the
/// destructor does whatever is left to do.
///
/// IT DOES NOT NEED THE STACK FRAME THAT MADE IT. That is the whole property
/// this class exists for -- a `RunningRecipe` sitting in a weave's member vector
/// is a build that outlives the handler that started it, and looking at it later
/// is an ordinary member call on an ordinary object.
///
/// IT IS NOT ADDRESSABLE, INSPECTABLE OR WRITABLE FROM THE BUS, and that is
/// deliberate in the same way BLD-0's recipe catalog is: a poke that could write
/// a new pid into a `ZEN_SHAPE` field would be a door onto arbitrary process
/// control wearing an inspection tool's clothes. Nothing in this class is a Zen
/// shape, and the runner keeps its live handles in a plain member for that
/// reason.
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

    /// LOSING THE HOLDER LOSES THE CHILD, deliberately and completely. A weave
    /// that is destroyed, a vector that is cleared, a process that is quitting:
    /// each of them ends here, and each of them ends with the child terminated
    /// and reaped rather than orphaned or left a zombie. It authors nothing --
    /// there is no `Mail` in a destructor, and no fact is invented from one.
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
            // STILL SPEAKING. Refusing to look for an ending here is what keeps
            // the last words of a failing build: a child writes them before it
            // exits, and an ending taken now would be an ending taken before
            // they were read.
            return seen;
        }
        reap_if_done(seen);
        return seen;
    }

    /// End custody now: terminate what is still running, reap it, close
    /// everything, exactly once.
    ///
    /// THE SEMANTIC CLAIM IS NARROW AND IT IS WRITTEN HERE SO NOBODY HAS TO
    /// GUESS AT IT: this says the HOLDER stopped holding. It does not say the
    /// build was cancelled, that it failed, or that it finished -- no fact of
    /// any kind is produced, because there is nobody in a destructor to produce
    /// one to. A caller that wants an observation to exist must author it while
    /// it still can.
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
            // A signalled child has no exit status, so it is given one that
            // cannot be mistaken for a build's own -- the shell's convention,
            // for the same reason: 0 would read as success and there is no
            // honest small number.
            seen.status = 128 + static_cast<std::int64_t>(WTERMSIG(wait_status));
        } else {
            seen.status = -1;
        }
        // 127 and 126 are the child's report that exec never happened. They
        // arrive as an exit status because that is the only channel a failed
        // exec has, and they are turned back into the fact they describe here
        // rather than being shown to a maker as a build that failed.
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

/// The answer to "did a process begin?", and the custody if one did.
///
/// TWO FACTS AND NOT ONE, for the reason `RunResult` keeps them apart: a program
/// that is not there and a build that failed are different things to tell a
/// maker. `started` false means nothing was left running and `process` holds
/// nothing.
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
    // NON-BLOCKING FROM THE FIRST LOOK ONWARD. Without this a `read` on a quiet
    // build waits for the build, which is the exact thing this phase removed.
    const int flags = ::fcntl(pipe_ends[0], F_GETFL, 0);
    (void)::fcntl(pipe_ends[0], F_SETFL, (flags < 0 ? 0 : flags) | O_NONBLOCK);
    out.process.child_ = child;
    out.process.out_ = pipe_ends[0];
    out.started = true;
    return out;
#endif
}

/// Run one recipe TO COMPLETION and answer what happened. IT BLOCKS.
///
/// THIS IS BLD-0'S SHAPE, KEPT ON PURPOSE AND CALLED BY NOTHING IN PRODUCTION.
/// ASYNC-1's central claim -- that a build no longer stops the Workshop -- is
/// only a claim if the other answer can still be built and measured beside it,
/// so this stayed as the control the regression canary drives (the `builder`
/// suite's blocking-runner case). It is written OVER the held primitive rather
/// than beside it, so there is exactly one implementation of the platform work
/// and the canary cannot pass merely because two paths drifted apart.
///
/// The one-millisecond nap is what makes this a wait rather than a spin. It is
/// also the only sleep anywhere in this file, and it exists solely on the path
/// that is deliberately not used.
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

} // namespace zengine::builder

#endif // ZENGINE_BUILDER_RUN_HPP
