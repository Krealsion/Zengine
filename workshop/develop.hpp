// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_DEVELOP_HPP
#define ZENGINE_WORKSHOP_DEVELOP_HPP

// THE DEVELOPMENT LAUNCH: how a maker working on Workshop's own panes starts the Workshop they
// work in, from outside it. `zengine-workshop-develop` (develop.cpp) is this file with the facts a
// configured build tree compiled in; CLion's shared "Develop Workshop" run configuration names that
// target, and a shell runs the same executable. Law: WL-CODE-08, agents/workshop/code.md.
//
// ONE ORDER, AND EVERY STEP CAN END IT WITH NOTHING STARTED. The tree must have staged a graphical
// plan; this launch must claim the runtime, which no other launch may then hold (WHO LAUNCHES A
// RUNTIME, below); no host from the runtime may be running; the runtime script must say the runtime
// is ready -- made now, or this tree's and still current; the project directory must be there or be
// made. Only then does the runtime's own host start, with the runtime's graphical plan and
// development catalog, in the project directory, and the claim is let go only after that host has
// exited. The build tree's host is never started, and a refused step is never followed by a second
// guess.

#include "builder/recipe.hpp"
#include "builder/run.hpp"

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <functional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#if !defined(_WIN32)
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace zengine::workshop::develop {

/// What configuration wrote down for this launch: the machine's own facts, compiled in by
/// `workshop/CMakeLists.txt` and never authored, so the shared run configuration names no path.
struct Facts {
    std::string cmake;   ///< the CMake that configured the build tree
    std::string script;  ///< that tree's generated development-runtime.cmake
    std::string build;   ///< the build tree: said, and never launched from
    std::string runtime; ///< the runtime directory when none is named
    std::string project; ///< the project directory when none is named
    std::string host;    ///< the host executable's file name
    std::string plan;    ///< the graphical load plan's file name; empty where none was staged
    std::string catalog; ///< the development catalog's file name
};

/// What one launch was asked for: the facts' two directories, or the ones a maker named.
struct Choice {
    bool ok = true;
    std::string complaint;
    std::string runtime;
    std::string project;
};

inline constexpr const char* kUsage =
    "usage: zengine-workshop-develop [--runtime <dir>] [--project <dir>]";

/// A named directory, absolute and without a trailing separator: the host starts in the project
/// directory, so a spelling relative to where the launch stands would name somewhere else there.
inline std::string absolute_directory(const std::string& named, std::string& trouble) {
    std::error_code ec;
    std::filesystem::path path = std::filesystem::absolute(named, ec);
    if (ec) {
        trouble = "cannot be made absolute: " + ec.message();
        return std::string();
    }
    path = path.lexically_normal();
    if (path.has_relative_path() && path.filename().empty()) {
        path = path.parent_path();
    }
    return path.generic_string();
}

// WL-CODE-08 -- agents/workshop/code.md
inline Choice choose(const std::vector<std::string>& args, const Facts& facts) {
    Choice choice;
    choice.runtime = facts.runtime;
    choice.project = facts.project;
    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string& arg = args[i];
        if (arg != "--runtime" && arg != "--project") {
            choice.ok = false;
            choice.complaint = "unknown argument `" + arg + "`";
            return choice;
        }
        if (i + 1 >= args.size() || args[i + 1].empty()) {
            choice.ok = false;
            choice.complaint = arg + " needs a directory";
            return choice;
        }
        std::string trouble;
        const std::string dir = absolute_directory(args[++i], trouble);
        if (!trouble.empty()) {
            choice.ok = false;
            choice.complaint = arg + " " + args[i] + " " + trouble;
            return choice;
        }
        (arg == "--runtime" ? choice.runtime : choice.project) = dir;
    }
    return choice;
}

/// The runtime script, asked about one directory: it makes the runtime, reuses it, or refuses.
// WL-CODE-08 -- agents/workshop/code.md
inline builder::BuildCommand runtime_command(const Facts& facts, const std::string& runtime) {
    builder::BuildCommand command;
    command.program = facts.cmake;
    command.args = {"-DZEN_RUNTIME=" + runtime, "-P", facts.script};
    return command;
}

/// The runtime's own host, with the runtime's graphical plan and development catalog, started in
/// the project directory.
// WL-CODE-08 -- agents/workshop/code.md
inline builder::BuildCommand host_command(const Facts& facts, const Choice& choice) {
    builder::BuildCommand command;
    command.program = choice.runtime + "/" + facts.host;
    command.args = {"--load-plan", choice.runtime + "/" + facts.plan, "--recipes",
                    choice.runtime + "/" + facts.catalog};
    command.dir = choice.project;
    return command;
}

/// IS THIS FILE AN IMAGE A RUNNING PROGRAM HOLDS? Asked by opening it for writing and closing it
/// again, which writes nothing. Windows answers a running image with a sharing violation, and a
/// Linux that denies writes to a running executable answers ETXTBSY; every other answer --
/// absent, opened, refused for another reason -- is "no", because this is asked only to refuse a
/// second launch.
// WL-CODE-08 -- agents/workshop/code.md
inline bool image_in_use(const std::string& path) {
#if defined(_WIN32)
    const HANDLE file = ::CreateFileA(path.c_str(), GENERIC_WRITE,
                                      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                      nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return ::GetLastError() == ERROR_SHARING_VIOLATION;
    }
    ::CloseHandle(file);
    return false;
#else
    const int fd = ::open(path.c_str(), O_WRONLY | O_CLOEXEC);
    if (fd < 0) {
        return errno == ETXTBSY;
    }
    ::close(fd);
    return false;
#endif
}

/// The project directory, made when it is absent: empty when it is there, else why it is not.
inline std::string project_directory(const std::string& dir) {
    std::error_code ec;
    if (std::filesystem::is_directory(dir, ec)) {
        return std::string();
    }
    if (std::filesystem::exists(dir, ec)) {
        return "is there and is not a directory";
    }
    std::filesystem::create_directories(dir, ec);
    if (ec) {
        return "could not be made: " + ec.message();
    }
    return std::string();
}

// ---- WHO LAUNCHES A RUNTIME ---------------------------------------------------------------------
//
// ONE LAUNCH HOLDS A RUNTIME, FROM BEFORE ITS PREPARATION UNTIL ITS HOST EXITS. Asking whether the
// runtime's host is running is a look, and two launches that look before either has started a host
// both see none: both prepare the one directory, and both start a Workshop over one runtime's
// images, promotions and project. Looking again just before the host starts would narrow that
// window, not close it. So the first thing a launch does is CLAIM the runtime -- ask the system for
// an object only one process can hold at a time -- and `launch` keeps the claim while it prepares,
// makes the project directory and runs the host, letting it go when it returns. A launch that
// cannot have the claim, because another holds it or because it could not be asked for, does
// nothing else.
//
// WHAT HOLDS IT, AND WHY NOTHING IS LEFT TO CLEAN UP. On Windows a named mutex in this logon
// session's namespace; on POSIX an exclusive `flock` on a lock file of this user's, in the per-user
// runtime directory `/run/user/<uid>` where the system made one, otherwise `/tmp`. The system lets
// go of either when the process holding it ends, however it ends, so a launch that crashed or was
// stopped leaves nothing held. The lock file stays behind and is never read for an answer, and it
// is never deleted: deleting a lock file another launch has open is how two launches come to hold
// "the" lock. It is touched when claimed, so a `/tmp` that ages out old files leaves it alone.
//
// WHAT NAMES IT: the runtime directory -- absolute, the part of it that exists made canonical, the
// rest normalized, no trailing separator, and on Windows case-folded as its file systems compare
// names -- hashed. So `C:/work/runtime`, `c:\WORK\runtime\` and `C:/work/x/../runtime` are one claim,
// and two runtimes are two. Not the launcher asking, not the build tree, and nothing written inside
// the runtime: a runtime not made yet is claimed without touching its directory, which the runtime
// script still finds exactly as it was -- absent, empty, or somebody else's files it refuses.
//
// WHAT IT DOES NOT COORDINATE. A host started from the runtime without this launch holds no claim:
// the in-use check `launch` makes right after claiming is what refuses one, and one a stopped launch
// left running. Another logon session's launch is another claim, and so is a spelling the standard
// library does not make canonical here: an 8.3 short name, a `subst` drive, a share by two names.

/// A claim a launch holds, let go when it is destroyed -- or by the system, if the process ends
/// first.
class RuntimeClaim {
public:
    RuntimeClaim() = default;
    explicit RuntimeClaim(std::function<void()> release) : release_(std::move(release)) {}
    RuntimeClaim(RuntimeClaim&& other) noexcept : release_(std::exchange(other.release_, nullptr)) {}
    RuntimeClaim& operator=(RuntimeClaim&& other) noexcept {
        if (this != &other) {
            let_go();
            release_ = std::exchange(other.release_, nullptr);
        }
        return *this;
    }
    RuntimeClaim(const RuntimeClaim&) = delete;
    RuntimeClaim& operator=(const RuntimeClaim&) = delete;
    ~RuntimeClaim() { let_go(); }

    bool held() const noexcept { return static_cast<bool>(release_); }

private:
    void let_go() noexcept {
        if (release_) {
            const std::function<void()> release = std::exchange(release_, nullptr);
            release();
        }
    }
    std::function<void()> release_;
};

/// What asking for a claim came to: held; held by another launch (`busy`); or not asked, and why.
struct ClaimAnswer {
    RuntimeClaim claim;
    bool busy = false;
    std::string trouble;
};

/// The runtime directory as one spelling: what a claim's name is made from.
inline std::string runtime_key(const std::string& runtime) {
    std::error_code ec;
    std::filesystem::path path = std::filesystem::absolute(std::filesystem::path(runtime), ec);
    if (ec) {
        path = std::filesystem::path(runtime);
    }
    std::error_code canonical_ec;
    const std::filesystem::path canonical = std::filesystem::weakly_canonical(path, canonical_ec);
    if (!canonical_ec) {
        path = canonical;
    }
    path = path.lexically_normal();
#if defined(_WIN32)
    std::wstring name = path.generic_wstring();
    while (name.size() > 1 && name.back() == L'/' && !(name.size() == 3 && name[1] == L':')) {
        name.pop_back();
    }
    // UPPER CASE BY THE SYSTEM'S OWN TABLE, not by a language's rules: how its file systems compare.
    if (!name.empty()) {
        std::wstring folded(name.size(), L'\0');
        const int length = ::LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_UPPERCASE, name.data(),
                                           static_cast<int>(name.size()), folded.data(),
                                           static_cast<int>(folded.size()), nullptr, nullptr, 0);
        if (length > 0) {
            folded.resize(static_cast<std::size_t>(length));
            name = folded;
        }
    }
    std::string key;
    const int bytes = ::WideCharToMultiByte(CP_UTF8, 0, name.data(), static_cast<int>(name.size()),
                                            nullptr, 0, nullptr, nullptr);
    if (bytes > 0) {
        key.resize(static_cast<std::size_t>(bytes));
        (void)::WideCharToMultiByte(CP_UTF8, 0, name.data(), static_cast<int>(name.size()),
                                    key.data(), bytes, nullptr, nullptr);
    }
    return key;
#else
    std::string key = path.generic_string();
    while (key.size() > 1 && key.back() == '/') {
        key.pop_back();
    }
    return key;
#endif
}

/// The name of the object that holds a runtime's claim -- a mutex's name on Windows, a lock file's
/// path on POSIX -- the same in every process for one directory, however it was spelled.
// WL-CODE-08 -- agents/workshop/code.md
inline std::string runtime_claim_name(const std::string& runtime) {
    std::uint64_t hash = 14695981039346656037ULL; // 64-bit FNV-1a: the same in every build
    for (const char c : runtime_key(runtime)) {
        hash ^= static_cast<std::uint64_t>(static_cast<unsigned char>(c));
        hash *= 1099511628211ULL;
    }
    char digits[17];
    (void)std::snprintf(digits, sizeof digits, "%016llx", static_cast<unsigned long long>(hash));
#if defined(_WIN32)
    return std::string("Local\\zengine-workshop-develop-") + digits;
#else
    const std::string uid = std::to_string(static_cast<unsigned long>(::geteuid()));
    std::string dir = "/tmp";
    struct stat about {};
    if (::stat(("/run/user/" + uid).c_str(), &about) == 0 && S_ISDIR(about.st_mode) &&
        about.st_uid == ::geteuid()) {
        dir = "/run/user/" + uid;
    }
    return dir + "/zengine-workshop-develop-" + uid + "-" + digits + ".lock";
#endif
}

// WL-CODE-08 -- agents/workshop/code.md
inline ClaimAnswer claim_runtime(const std::string& runtime) {
    ClaimAnswer answer;
    const std::string name = runtime_claim_name(runtime);
#if defined(_WIN32)
    const HANDLE mutex = ::CreateMutexA(nullptr, FALSE, name.c_str());
    if (mutex == nullptr) {
        answer.trouble = "the mutex " + name + " could not be made (error " +
                         std::to_string(static_cast<unsigned long>(::GetLastError())) + ")";
        return answer;
    }
    // WAIT_ABANDONED IS HELD TOO: the launch that held it ended without letting go, and the system
    // handed the mutex on. Whether a Workshop it started is still open is the in-use check's answer.
    const DWORD waited = ::WaitForSingleObject(mutex, 0);
    if (waited == WAIT_OBJECT_0 || waited == WAIT_ABANDONED) {
        answer.claim = RuntimeClaim([mutex] {
            (void)::ReleaseMutex(mutex);
            (void)::CloseHandle(mutex);
        });
        return answer;
    }
    const DWORD why = ::GetLastError();
    (void)::CloseHandle(mutex);
    if (waited == WAIT_TIMEOUT) {
        answer.busy = true;
    } else {
        answer.trouble = "the mutex " + name + " could not be waited on (error " +
                         std::to_string(static_cast<unsigned long>(why)) + ")";
    }
    return answer;
#else
    // CLOSED ON EXEC, so neither the runtime script nor the host shares the lock and outlives the
    // launch with it; NEVER THROUGH A LINK, and only a regular file of this user's, because `/tmp`
    // is everyone's.
    const int fd = ::open(name.c_str(), O_RDWR | O_CREAT | O_CLOEXEC | O_NOFOLLOW, 0600);
    if (fd < 0) {
        const int why = errno;
        answer.trouble = name + " could not be opened: " + std::strerror(why);
        return answer;
    }
    struct stat about {};
    if (::fstat(fd, &about) != 0 || !S_ISREG(about.st_mode) || about.st_uid != ::geteuid()) {
        (void)::close(fd);
        answer.trouble = name + " is not a lock file of this user's";
        return answer;
    }
    if (::flock(fd, LOCK_EX | LOCK_NB) != 0) {
        const int why = errno;
        (void)::close(fd);
        if (why == EWOULDBLOCK) {
            answer.busy = true;
        } else {
            answer.trouble = name + " could not be locked: " + std::strerror(why);
        }
        return answer;
    }
    (void)::futimens(fd, nullptr);
    answer.claim = RuntimeClaim([fd] {
        (void)::flock(fd, LOCK_UN);
        (void)::close(fd);
    });
    return answer;
#endif
}

/// What a launch needs from the world. `real_world` hands it the real doors; a suite hands it doors
/// of its own and reads what was asked of them.
struct World {
    std::function<ClaimAnswer(const std::string&)> claim;
    std::function<bool(const std::string&)> in_use;
    std::function<builder::RunResult(const builder::BuildCommand&)> run;
    std::function<std::string(const std::string&)> directory;
    std::function<void(const std::string&)> say;
};

/// THE REAL DOORS, COMPOSED ONCE. `zengine-workshop-develop` launches through them, and so does the
/// separate-process driver its suite starts two at a time (tests/launch_fixture_driver.cpp), so
/// what two of those meet is what two Runs meet.
// WL-CODE-08 -- agents/workshop/code.md
inline World real_world() {
    World world;
    world.claim = &claim_runtime;
    world.in_use = &image_in_use;
    world.run = [](const builder::BuildCommand& command) {
        return builder::run_forwarding(command, stdout);
    };
    world.directory = &project_directory;
    world.say = [](const std::string& line) {
        std::printf("zengine-workshop-develop - %s\n", line.c_str());
        std::fflush(stdout);
    };
    return world;
}

// WL-CODE-08 -- agents/workshop/code.md
inline int launch(const Facts& facts, const Choice& choice, const World& world) {
    if (!choice.ok) {
        world.say(choice.complaint + " -- nothing was prepared or launched");
        world.say(kUsage);
        return 2;
    }
    if (facts.plan.empty()) {
        world.say("the build tree " + facts.build + " staged no graphical load plan (it was "
                  "configured without the SDL skin) -- nothing was prepared or launched");
        return 1;
    }
    const builder::BuildCommand host = host_command(facts, choice);
    world.say("build tree: " + facts.build);
    world.say("runtime: " + choice.runtime);
    // THE CLAIM COMES BEFORE ANYTHING IS LOOKED AT OR PREPARED, and `owned` lives until this
    // function returns: after the host has exited, or at whichever step refused.
    const ClaimAnswer owned = world.claim(choice.runtime);
    if (owned.busy) {
        world.say("another launch holds the runtime " + choice.runtime + ": it is preparing it, or "
                  "the Workshop it started is still open. Use that Workshop, or quit it and launch "
                  "again -- nothing was prepared, launched or stopped");
        return 1;
    }
    if (!owned.claim.held()) {
        world.say("could not claim the runtime " + choice.runtime + " for this launch (" +
                  owned.trouble + ") -- nothing was prepared or launched");
        return 1;
    }
    if (world.in_use(host.program)) {
        world.say(host.program + " is running, and no launch holds its runtime: a Workshop started "
                  "from it some other way, or left open by a launch that was stopped. Quit it, then "
                  "launch again -- nothing was prepared, launched or stopped");
        return 1;
    }
    const builder::RunResult prepared = world.run(runtime_command(facts, choice.runtime));
    if (!prepared.started) {
        world.say("could not run " + facts.cmake + " to prepare the runtime (" + prepared.trouble +
                  ") -- nothing was launched");
        return 1;
    }
    if (prepared.status != 0) {
        world.say("the runtime was not prepared, for the reason above -- nothing was launched");
        return 1;
    }
    const std::string refused = world.directory(choice.project);
    if (!refused.empty()) {
        world.say("the project directory " + choice.project + " " + refused +
                  " -- nothing was launched");
        return 1;
    }
    world.say("project: " + choice.project + " (Workshop's project files)");
    world.say("launching " + host.as_line());
    const builder::RunResult ran = world.run(host);
    if (!ran.started) {
        world.say("the runtime's Workshop did not start: " + ran.trouble);
        return 1;
    }
    world.say("Workshop exited with " + std::to_string(ran.status));
    return static_cast<int>(ran.status);
}

} // namespace zengine::workshop::develop

#endif // ZENGINE_WORKSHOP_DEVELOP_HPP
