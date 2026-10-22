// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The launcher (launcher/docs/zengine.launcher.md): what it may start is judged against a catalog
// fixed at configure, nothing outside it starts, a demo starts with the configured interpreter,
// and an app starts detached as the process the answer names, running `zengine-app` on its folder.

#include "doctest.h"

#include "app/vocabulary.hpp"
#include "launcher/catalog.hpp"
#include "launcher/start.hpp"
#include "launcher/vocabulary.hpp"
#include "lifecycle_door.hpp"
#include "maker/weave.hpp"
#include "workshop/pane_seam_vocabulary.hpp"

#include <zen/kernel/kernel.hpp>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <thread>
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
#include <csignal>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace {

namespace launcher = zengine::launcher;
namespace ws = zengine::workshop;
namespace fs = std::filesystem;

bool has(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

[[maybe_unused]] std::string text_of(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

/// A project with the tally app in its `apps/`, and a file standing for an interpreter and a script.
struct Project {
    fs::path dir;
    Project() {
        dir = fs::temp_directory_path() /
              ("zengine-launcher-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        const fs::path tally = dir / zengine::app::kAppsFolder / "tally";
        fs::create_directories(tally);
        for (const char* file : {"app-plan.json", "app.json", "tally.view", "tally.flow"})
            fs::copy_file(fs::path(TALLY_APP_DIR) / file, tally / file);
        std::ofstream(dir / "python", std::ios::binary) << "an interpreter";
        std::ofstream(dir / "demo.py", std::ios::binary) << "a demo service";
    }
    ~Project() {
        std::error_code ec;
        fs::remove_all(dir, ec);
    }
    std::string path() const { return dir.generic_string(); }
    launcher::Catalog catalog() const {
        return launcher::Catalog{APP_PROGRAM, (dir / "python").generic_string(), (dir / "demo.py").generic_string(),
                                 "/the/build", "/the/loom", {{"values", (dir / "setups" / "values").generic_string()}}};
    }
};

/// Whether the process `pid` is running `program`'s file, and how its command line ends.
bool runs(std::int64_t pid, const std::string& program_name) {
#if defined(_WIN32)
    HANDLE h = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid));
    if (h == nullptr) return false;
    wchar_t image[MAX_PATH * 4];
    DWORD size = static_cast<DWORD>(std::size(image));
    const bool read = ::QueryFullProcessImageNameW(h, 0, image, &size) != 0;
    ::CloseHandle(h);
    const std::wstring name(image, read ? size : 0);
    return name.size() >= program_name.size() &&
           fs::path(name).filename().string() == program_name;
#else
    std::error_code ec;
    const fs::path exe = fs::read_symlink("/proc/" + std::to_string(pid) + "/exe", ec);
    return !ec && exe.filename().string() == program_name;
#endif
}

/// Wait up to `seconds` for the process to be gone; true when it is.
bool gone_within(std::int64_t pid, int seconds) {
    for (int i = 0; i < seconds * 10; ++i) {
#if defined(_WIN32)
        HANDLE h = ::OpenProcess(SYNCHRONIZE, FALSE, static_cast<DWORD>(pid));
        if (h == nullptr) return true;
        const DWORD waited = ::WaitForSingleObject(h, 100);
        ::CloseHandle(h);
        if (waited == WAIT_OBJECT_0) return true;
#else
        if (::kill(static_cast<pid_t>(pid), 0) != 0) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
#endif
    }
    return false;
}

void end(std::int64_t pid) {
#if defined(_WIN32)
    HANDLE h = ::OpenProcess(PROCESS_TERMINATE, FALSE, static_cast<DWORD>(pid));
    if (h != nullptr) {
        (void)::TerminateProcess(h, 9);
        ::CloseHandle(h);
    }
#else
    (void)::kill(static_cast<pid_t>(pid), SIGTERM);
#endif
}

} // namespace

TEST_CASE("a kind or a name outside the catalog is refused, and nothing is started") {
    Project p;
    const auto refused = [&](const launcher::Launch& l, const std::string& words) {
        const auto judged = launcher::judge_launch(l, p.path(), p.catalog());
        CHECK_FALSE(judged.ok);
        CHECK_MESSAGE(has(judged.refusal, words), judged.refusal);
        CHECK(judged.plan.program.empty());
    };
    refused({"shell", "tally"}, "starts an app or a demo, not 'shell'");
    refused({"app", "a/b"}, "not a plain folder name");
    refused({"app", "missing"}, "no app 'missing'");
    refused({"app", std::string(launcher::kMaxLaunchName + 1, 'a')}, "at most");
    refused({"demo", "unlisted"}, "no demo 'unlisted'");
    // A BUILD WITH NO DEMO SERVICE starts no demo.
    launcher::Catalog none = p.catalog();
    none.demos.clear();
    const auto no_service = launcher::judge_launch({"demo", "values"}, p.path(), none);
    CHECK_FALSE(no_service.ok);
    CHECK(has(no_service.refusal, "configured with no demo service"));
    // NO PROJECT: nothing starts outside one.
    CHECK(has(launcher::judge_launch({"app", "tally"}, "", p.catalog()).refusal, "no project directory"));
}

TEST_CASE("an app is a folder directly in the project's apps/ holding a plan the codec reads; a link out of it and a folder with no plan are refused") {
    // MUTATION: the launcher taking any folder a name reaches -- a link in apps/ to elsewhere starts.
    Project p;
    const fs::path apps = p.dir / zengine::app::kAppsFolder;
    fs::create_directories(p.dir / "elsewhere");
    fs::copy_file(apps / "tally" / "app-plan.json", p.dir / "elsewhere" / "app-plan.json");
    std::error_code linked;
    fs::create_directory_symlink(p.dir / "elsewhere", apps / "out", linked);
    if (!linked) {
        const auto out = launcher::judge_launch({"app", "out"}, p.path(), p.catalog());
        CHECK_FALSE(out.ok);
        CHECK(has(out.refusal, "not a folder directly in"));
    }
    fs::create_directories(apps / "planless");
    const auto planless = launcher::judge_launch({"app", "planless"}, p.path(), p.catalog());
    CHECK_FALSE(planless.ok);
    CHECK(has(planless.refusal, "has no plan this Workshop reads"));
    // ...and the app that is one: the app host beside Workshop, on the app's own folder.
    const auto tally = launcher::judge_launch({"app", "tally"}, p.path(), p.catalog());
    REQUIRE_MESSAGE(tally.ok, tally.refusal);
    CHECK(tally.plan.program == std::string(APP_PROGRAM));
    REQUIRE(tally.plan.args.size() == 1);
    CHECK(fs::equivalent(tally.plan.args[0], apps / "tally"));
    CHECK(fs::equivalent(tally.plan.dir, apps / "tally"));
}

TEST_CASE("a listed setup is started by the configured interpreter running the configured demo service on a root of its own in the project") {
    Project p;
    const auto judged = launcher::judge_launch({"demo", "values"}, p.path(), p.catalog());
    REQUIRE_MESSAGE(judged.ok, judged.refusal);
    CHECK(judged.plan.program == (p.dir / "python").generic_string());
    const std::vector<std::string> args{(p.dir / "demo.py").generic_string(), "start", "--setup", "values",
                                        "--root", (p.dir / "demo-runs" / "values").generic_string(),
                                        "--build", "/the/build", "--loom-prefix", "/the/loom"};
    CHECK(judged.plan.args == args);
    CHECK(fs::is_directory(p.dir / "demo-runs"));
}

TEST_CASE("an app starts detached as the process the launcher names: zengine-app on its folder, kept by nobody") {
    // MUTATION: a start that answers before the process exists -- the pid names no zengine-app.
    Project p;
    const auto judged = launcher::judge_launch({"app", "tally"}, p.path(), p.catalog());
    REQUIRE_MESSAGE(judged.ok, judged.refusal);
    const launcher::Started started = launcher::start_detached(judged.plan);
    REQUIRE_MESSAGE(started.ok, started.trouble);
    REQUIRE(started.pid > 0);
    CHECK(runs(started.pid, fs::path(APP_PROGRAM).filename().string()));
#if defined(_WIN32)
    // Its console is its own and waits on its weaver: ended here.
    end(started.pid);
    CHECK(gone_within(started.pid, 10));
#else
    // Its input is empty, so its console ends while the plan waits on it, by itself; what it said
    // is in the app's log.
    if (!gone_within(started.pid, 20)) end(started.pid);
    CHECK(has(text_of(p.dir / "apps" / "tally" / launcher::kAppLogName), "waiting on this console"));
#endif
    // A PROGRAM THAT IS NOT THERE never starts, and says so.
    launcher::StartPlan missing = judged.plan;
    missing.program = (p.dir / "nothing-here").generic_string();
    const launcher::Started never = launcher::start_detached(missing);
    CHECK_FALSE(never.ok);
    CHECK(has(never.trouble, "could not start"));
}

#if defined(__linux__)
TEST_CASE("a POSIX start hands on its three standard streams and nothing else of the starter's") {
    // MUTATION: a start that closes nothing -- the started process holds the starter's socket.
    // A LISTENING SOCKET the starter holds, open across an exec, as a door's may be.
    const int door = ::socket(AF_INET, SOCK_STREAM, 0);
    REQUIRE(door >= 0);
    sockaddr_in at{};
    at.sin_family = AF_INET;
    at.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    REQUIRE(::bind(door, reinterpret_cast<sockaddr*>(&at), sizeof(at)) == 0);
    REQUIRE(::listen(door, 1) == 0);
    socklen_t len = sizeof(at);
    REQUIRE(::getsockname(door, reinterpret_cast<sockaddr*>(&at), &len) == 0);

    const launcher::Started started = launcher::start_detached({"/bin/sleep", {"5"}, "", ""});
    REQUIRE_MESSAGE(started.ok, started.trouble);
    bool exec_done = false;
    for (int i = 0; i < 50 && !exec_done; ++i) {
        exec_done = runs(started.pid, "sleep");
        if (!exec_done) std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    REQUIRE(exec_done);
    // No descriptor of the started process is the starter's socket (its loader may still hold a
    // library of its own open, so what else it holds is not counted).
    std::error_code ec;
    const fs::path socket = fs::read_symlink("/proc/self/fd/" + std::to_string(door), ec);
    REQUIRE_FALSE(ec);
    std::vector<std::string> held;
    for (const auto& entry : fs::directory_iterator("/proc/" + std::to_string(started.pid) + "/fd", ec))
        held.push_back(fs::read_symlink(entry.path(), ec).string());
    CHECK_FALSE(held.empty());
    CHECK(std::find(held.begin(), held.end(), socket.string()) == held.end());

    // THE STARTER LETS ITS SOCKET GO, and its port is free while the started process runs on.
    ::close(door);
    const int again = ::socket(AF_INET, SOCK_STREAM, 0);
    REQUIRE(again >= 0);
    CHECK(::bind(again, reinterpret_cast<sockaddr*>(&at), sizeof(at)) == 0);
    ::close(again);
    end(started.pid);
    CHECK(gone_within(started.pid, 10));
}
#endif

namespace {
/// `zengine.project`: where the project is.
class ProjectStub final : public loom::Weave {
public:
    explicit ProjectStub(std::string dir) : dir_(std::move(dir)) {}
    loom::WeaveId self{};
    std::vector<std::shared_ptr<const loom::Schema>> accepted_schemas() const override {
        return {loom::schema_of<ws::ProjectRootRequested>()};
    }
    std::vector<std::shared_ptr<const loom::Schema>> emitted_schemas() const override {
        return {loom::schema_of<ws::ProjectRoot>()};
    }
    void handle(const loom::Message&, loom::Bus& bus) override {
        (void)bus.answer(loom::Message(loom::to_value(ws::ProjectRoot{dir_, ""}), self));
    }
    loom::Value snapshot() const override { return loom::Value(loom::make_schema("lt.Project", 1, {})); }
    loom::Value policy() const override { return zengine::maker::default_value(loom::lifecycle_policy_schema()); }
    void revive(const loom::Value&) override {}

private:
    std::string dir_;
};

/// An office that asks the launcher and keeps its answers.
class Asker final : public loom::Weave {
public:
    std::vector<launcher::Launched> heard;
    std::vector<std::shared_ptr<const loom::Schema>> accepted_schemas() const override {
        return {loom::schema_of<launcher::Launched>()};
    }
    void handle(const loom::Message& in, loom::Bus&) override {
        heard.push_back(loom::from_value<launcher::Launched>(in.payload));
    }
    loom::Value snapshot() const override { return loom::Value(loom::make_schema("lt.Asker", 1, {})); }
    loom::Value policy() const override { return zengine::maker::default_value(loom::lifecycle_policy_schema()); }
    void revive(const loom::Value&) override {}
};
} // namespace

TEST_CASE("the launcher weave starts nothing for an office that is not the View Builder or Files, and answers those two from the catalog") {
    Project p;
    loom::Switchboard bus;
    loom::Kernel kernel{bus, loom::trust_every_artifact("the suite loads the launcher")};
    auto stub = std::make_unique<ProjectStub>(p.path());
    ProjectStub* raw = stub.get();
    raw->self = bus.register_weave(std::move(stub), loom::Grant{}.allow_any(), ws::kProjectRole);
    const auto loaded = kernel.load("launcher", LAUNCHER_ARTIFACT, launcher::kLauncherRole, loom::Grant{}.allow_any());
    INFO(loaded.error);
    REQUIRE(loaded.ok);
    const loom::WeaveId door = zengine::testing::mount_door(bus);
    zengine::testing::order_activation(bus, door, loaded.id, 1);
    const auto pump = [&bus] {
        for (int n = 0; n < 64 && bus.pending() != 0; ++n) bus.pump_pending();
    };
    pump();
    std::uint64_t corr = 0;
    const auto ask = [&](const char* office, const launcher::Launch& l) {
        auto a = std::make_unique<Asker>();
        Asker* heard = a.get();
        const loom::WeaveId id = bus.register_weave(std::move(a), loom::Grant{}.allow_any(), office);
        REQUIRE(bus.office_send_to_role_as(id, office, launcher::kLauncherRole,
                                           loom::Message(loom::to_value(l), id, {}, ++corr)).valid());
        pump();
        REQUIRE(heard->heard.size() == 1);
        return heard->heard.back();
    };
    // ANOTHER OFFICE: refused, nothing started, though the app is there.
    const auto stranger = ask("zengine.stranger", {"app", "tally"});
    CHECK_FALSE(stranger.ok);
    CHECK(has(stranger.reason, "only the View Builder and Files"));
    // FILES, for what the catalog does not hold: refused in its words.
    const auto missing = ask("zengine.files", {"app", "missing"});
    CHECK_FALSE(missing.ok);
    CHECK(has(missing.reason, "no app 'missing'"));
    CHECK(missing.kind == "app");
    CHECK(missing.name == "missing");
}
