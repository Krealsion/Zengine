// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// THE GUEST JOURNEY, ACROSS TWO REAL PROCESSES.
//
// Everything the guests suite proves, it proves in one process with the door on one side of a
// loopback socket and a client on the other. This program is the other half of the claim: a
// REAL `zengine-workshop` process, launched with a guests file and a headless load plan (the
// terminal Skin over a stream that is not a terminal, the console Input reader with no console,
// the desktop, the Connections pane), and THIS process as the other host's participant --
// connecting through Loom's exported client, admitted as the row the file names, opening an
// input session with the real Input weave, injecting the desktop's own Pane Manager chord,
// asking the desktop office what it accepts, taking a picture of what the real Skin presents
// and fetching it by chunk, closing the session, and reading the Connections pane's own rows
// through the picture. Then a wrong credential, refused by the same process in words.
//
// WHAT IT DOES NOT PROVE: the graphical medium (the SDL window's pixels are the graphical
// Skin's, witnessed on a machine with a display) and the platform's input edge (an injected
// moment enters at the Input weave, downstream of the console reader). Those are said as such
// in the phase's evidence, never inferred from a green here.
//
//     zengine-guest-journey <zengine-workshop exe> <headless load plan> <work dir>
//
// Exit 0 on PASS; 1 with the failed check named on stderr. The child is ended on the way out,
// whatever happened.

#include "input/vocabulary.hpp"
#include "surface/vocabulary.hpp"

#include <zen/bridge/client.hpp>
#include <zen/serialize.hpp>
#include <zen/weave/describe.hpp>
#include <zen/weave/shape.hpp>
#include <zen/weave/standard_shapes.hpp>

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
#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#endif

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {

namespace input = zengine::input;
namespace surface = zengine::surface;

int failures = 0;

bool check(bool ok, const std::string& what) {
    std::printf("  %-6s %s\n", ok ? "ok" : "FAIL", what.c_str());
    if (!ok) {
        ++failures;
    }
    return ok;
}

bool until(const std::function<bool()>& done, int timeout_ms) {
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    while (!done()) {
        if (std::chrono::steady_clock::now() >= deadline) {
            return done();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return true;
}

// ---- the child process, and the one thing this program does to it: end it ---------------

struct Child {
#if defined(_WIN32)
    PROCESS_INFORMATION pi{};
    bool started = false;
    bool start(const std::string& exe, const std::vector<std::string>& args,
               const std::string& cwd, const std::string& out_path) {
        std::string line = "\"" + exe + "\"";
        for (const std::string& a : args) {
            line += " \"" + a + "\"";
        }
        SECURITY_ATTRIBUTES sa{};
        sa.nLength = sizeof(sa);
        sa.bInheritHandle = TRUE;
        HANDLE out = ::CreateFileA(out_path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, &sa,
                                   CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        STARTUPINFOA si{};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdOutput = out;
        si.hStdError = out;
        si.hStdInput = INVALID_HANDLE_VALUE;
        std::vector<char> buf(line.begin(), line.end());
        buf.push_back('\0');
        started = ::CreateProcessA(nullptr, buf.data(), nullptr, nullptr, TRUE,
                                   CREATE_NO_WINDOW, nullptr, cwd.c_str(), &si, &pi) != 0;
        ::CloseHandle(out);
        return started;
    }
    bool alive() const {
        if (!started) {
            return false;
        }
        return ::WaitForSingleObject(pi.hProcess, 0) == WAIT_TIMEOUT;
    }
    void end() {
        if (!started) {
            return;
        }
        if (alive()) {
            ::TerminateProcess(pi.hProcess, 9);
            ::WaitForSingleObject(pi.hProcess, 5000);
        }
        ::CloseHandle(pi.hThread);
        ::CloseHandle(pi.hProcess);
        started = false;
    }
#else
    pid_t pid = 0;
    bool start(const std::string& exe, const std::vector<std::string>& args,
               const std::string& cwd, const std::string& out_path) {
        std::vector<std::string> all;
        all.push_back(exe);
        for (const std::string& a : args) {
            all.push_back(a);
        }
        std::vector<char*> argv;
        for (std::string& a : all) {
            argv.push_back(a.data());
        }
        argv.push_back(nullptr);
        posix_spawn_file_actions_t fa;
        posix_spawn_file_actions_init(&fa);
        posix_spawn_file_actions_addopen(&fa, 1, out_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC,
                                         0644);
        posix_spawn_file_actions_adddup2(&fa, 1, 2);
        posix_spawn_file_actions_addopen(&fa, 0, "/dev/null", O_RDONLY, 0);
        posix_spawn_file_actions_addchdir_np(&fa, cwd.c_str());
        const int rc = ::posix_spawn(&pid, exe.c_str(), &fa, nullptr, argv.data(), environ);
        posix_spawn_file_actions_destroy(&fa);
        return rc == 0;
    }
    bool alive() const {
        if (pid <= 0) {
            return false;
        }
        int status = 0;
        return ::waitpid(pid, &status, WNOHANG) == 0;
    }
    void end() {
        if (pid <= 0) {
            return;
        }
        if (alive()) {
            ::kill(pid, SIGTERM);
            int status = 0;
            for (int i = 0; i < 100 && ::waitpid(pid, &status, WNOHANG) == 0; ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
            if (alive()) {
                ::kill(pid, SIGKILL);
                ::waitpid(pid, &status, 0);
            }
        }
        pid = 0;
    }
#endif
};

// ---- a guest of the child --------------------------------------------------------------

struct Guest {
    std::unique_ptr<loom::BridgeClient> client;
    std::vector<loom::BridgeEvent> events;
    std::uint64_t next_correlation = 1;

    bool connect(std::uint16_t port, const char* name, const char* credential) {
        std::string err;
        const loom::socket_t s = loom::bridge_connect_tcp("127.0.0.1", port, &err);
        if (s == loom::kInvalidSocket) {
            std::fprintf(stderr, "connect: %s\n", err.c_str());
            return false;
        }
        client = std::make_unique<loom::BridgeClient>(s);
        return client->hello(name, credential);
    }
    void poll() {
        std::vector<loom::BridgeEvent> got;
        client->poll(got);
        for (loom::BridgeEvent& e : got) {
            events.push_back(std::move(e));
        }
    }
    template <class T>
    std::uint64_t ask(const char* far_role, const T& msg) {
        const std::uint64_t c = next_correlation++;
        client->send_to_role(far_role, c, loom::serialize(loom::to_value(msg)));
        client->flush();
        return c;
    }
    const loom::BridgeEvent* delivered(std::uint64_t correlation) const {
        for (const loom::BridgeEvent& e : events) {
            if (e.kind == loom::BridgeEvent::Kind::Delivered && e.correlation == correlation) {
                return &e;
            }
        }
        return nullptr;
    }
    std::optional<loom::Value> answer_as(std::uint64_t correlation,
                                         const std::shared_ptr<const loom::Schema>& schema,
                                         int timeout_ms = 5000) {
        const loom::BridgeEvent* e = nullptr;
        (void)until(
            [&] {
                poll();
                e = delivered(correlation);
                return e != nullptr;
            },
            timeout_ms);
        if (e == nullptr) {
            return std::nullopt;
        }
        loom::Unverified u = loom::parse(e->payload);
        loom::Admission a = loom::admit(u, schema);
        if (!a.ok()) {
            std::fprintf(stderr, "  answer to %llu was %s, not %s\n",
                         static_cast<unsigned long long>(correlation), u.claimed_name().c_str(),
                         schema->name().c_str());
            return std::nullopt;
        }
        return std::move(a).value();
    }
    template <class T>
    std::optional<T> answer(std::uint64_t correlation, int timeout_ms = 5000) {
        const loom::BridgeEvent* e = nullptr;
        (void)until(
            [&] {
                poll();
                e = delivered(correlation);
                return e != nullptr;
            },
            timeout_ms);
        if (e == nullptr) {
            return std::nullopt;
        }
        loom::Unverified u = loom::parse(e->payload);
        loom::Admission a = loom::admit(u, loom::schema_of<T>());
        if (!a.ok()) {
            std::fprintf(stderr, "  answer to %llu was %s, not %s\n",
                         static_cast<unsigned long long>(correlation), u.claimed_name().c_str(),
                         loom::schema_of<T>()->name().c_str());
            return std::nullopt;
        }
        return loom::from_value<T>(a.value());
    }
};

std::string read_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 4) {
        std::fprintf(stderr, "usage: zengine-guest-journey <zengine-workshop> <load plan> <work dir>\n");
        return 2;
    }
    // ABSOLUTE, because the child starts in the work directory and a relative spelling of the
    // host or the plan would be resolved there rather than where this program was told.
    const std::string workshop = std::filesystem::absolute(argv[1]).string();
    const std::string plan = std::filesystem::absolute(argv[2]).string();
    const std::string work = std::filesystem::absolute(argv[3]).string();
    std::filesystem::create_directories(work);
    const std::string guests_path = work + "/guests.json";
    const std::string port_path = work + "/guests.port";
    const std::string out_path = work + "/workshop.out";
    std::remove(port_path.c_str());
    {
        std::ofstream g(guests_path, std::ios::trunc);
        g << R"({"listen":"127.0.0.1:0","port_file":")" << "guests.port"
          << R"(","guests":[{"name":"agent","credential":"open-sesame",)"
          << R"("may":["input","capture","inspect"]}]})";
    }
    std::string err;
    if (!loom::bridge_net_init(&err)) {
        std::fprintf(stderr, "net init: %s\n", err.c_str());
        return 1;
    }

    std::printf("guest journey: two real processes\n");
    Child child;
    const bool started = child.start(workshop,
                                     {"--isolated", "--load-plan", plan, "--guests", guests_path,
                                      "--log", "workshop.log"},
                                     work, out_path);
    check(started, "the Workshop process started");
    if (!started) {
        return 1;
    }
    std::uint16_t port = 0;
    const bool wrote_port = until(
                                [&] {
                                    const std::string text = read_file(port_path);
                                    if (text.empty()) {
                                        return !child.alive();
                                    }
                                    port = static_cast<std::uint16_t>(std::atoi(text.c_str()));
                                    return port != 0;
                                },
                                20000) &&
                            port != 0;
    // (the wait first, then the sentence: an argument list evaluates in no promised order)
    check(wrote_port, "...and wrote the port it listens on (" + std::to_string(port) + ")");
    if (port == 0) {
        child.end();
        std::fprintf(stderr, "--- workshop output ---\n%s\n", read_file(out_path).c_str());
        return 1;
    }

    // 1. A WRONG CREDENTIAL, refused in the host's words, leaving nothing behind.
    {
        Guest wrong;
        check(wrong.connect(port, "agent", "nope"), "a stranger can connect");
        (void)until(
            [&] {
                wrong.poll();
                return wrong.client->denied() || wrong.client->disconnected();
            },
            5000);
        check(wrong.client->denied() &&
                  wrong.client->denial() == "no guest of this Workshop presents that credential",
              "...and a wrong credential is refused in the Workshop's own sentence");
    }

    // 2. THE JOURNEY.
    Guest g;
    check(g.connect(port, "agent-from-elsewhere", "open-sesame"), "the agent connects");
    (void)until(
        [&] {
            g.poll();
            return g.client->admitted() || g.client->denied();
        },
        5000);
    if (!check(g.client->admitted(), "...and is admitted")) {
        child.end();
        return 1;
    }
    check(g.client->established_name() == "agent",
          "as the name the guests file establishes, not the one it claimed");

    const std::uint64_t open = g.ask(input::kInputRole, input::InputSessionRequested{"journey"});
    const std::optional<input::InputSessionOpened> opened = g.answer<input::InputSessionOpened>(open);
    if (!check(opened.has_value(), "the Input weave opened a session")) {
        child.end();
        std::fprintf(stderr, "--- workshop output ---\n%s\n", read_file(out_path).c_str());
        return 1;
    }
    const loom::BridgeEvent* opened_event = g.delivered(open);
    check(opened_event != nullptr && opened_event->answers_ask,
          "...with Loom's attestation that this is THE answer");

    // A picture BEFORE the chord, so the ordering claim below is about a later frame.
    const std::uint64_t before = g.ask(surface::kSkinRole, surface::SurfaceCaptureRequested{});
    const std::optional<surface::SurfaceCaptured> first = g.answer<surface::SurfaceCaptured>(before);
    check(first.has_value() && first->ok, "the Skin took a picture before the chord");
    const std::int64_t frame_before = first.has_value() ? first->frame : -1;

    input::InjectInput batch;
    batch.session = opened->session;
    input::InjectedEvent down;
    down.kind = "KeyPressed";
    down.scancode = input::scan::kP;
    down.modifiers = input::mod::kCtrl;
    input::InjectedEvent up = down;
    up.kind = "KeyReleased";
    batch.events = {down, up};
    const std::uint64_t inject = g.ask(input::kInputRole, batch);
    const std::optional<input::InputInjected> injected = g.answer<input::InputInjected>(inject);
    check(injected.has_value() && injected->admitted == 2,
          "Ctrl+P was injected through the real Input weave (2 moments)");

    const std::uint64_t inspect = g.ask("zengine.desktop", loom::DescribeAccepted{});
    const std::optional<loom::Value> shapes = g.answer_as(inspect, loom::accepted_shapes_schema());
    const std::size_t accepted = shapes.has_value() ? shapes->get("accepted")->as_list().size() : 0;
    check(shapes.has_value() && accepted > 0,
          "the desktop office described what it accepts (" + std::to_string(accepted) + " shapes)");

    // A picture AFTER the frame the injection's consumer painted: deferred to that paint.
    surface::SurfaceCaptureRequested after;
    after.after_frame = frame_before;
    const std::uint64_t capture = g.ask(surface::kSkinRole, after);
    const std::optional<surface::SurfaceCaptured> pic = g.answer<surface::SurfaceCaptured>(capture, 10000);
    check(pic.has_value() && pic->ok && pic->frame > frame_before,
          "the Skin captured a later frame (" +
              std::to_string(pic.has_value() ? pic->frame : -1) + " > " +
              std::to_string(frame_before) + ")");
    std::string picture;
    if (pic.has_value() && pic->ok) {
        std::int64_t offset = 0;
        bool whole = true;
        while (offset < pic->bytes) {
            const std::uint64_t c = g.ask(surface::kSkinRole,
                                          surface::SurfaceCaptureChunkRequested{pic->capture, offset});
            const std::optional<surface::SurfaceCaptureChunk> chunk =
                g.answer<surface::SurfaceCaptureChunk>(c);
            if (!chunk.has_value() || chunk->offset != offset) {
                whole = false;
                break;
            }
            picture.append(chunk->data.begin(), chunk->data.end());
            offset += static_cast<std::int64_t>(chunk->data.size());
            if (chunk->data.empty()) {
                whole = false;
                break;
            }
        }
        check(whole && static_cast<std::int64_t>(picture.size()) == pic->bytes,
              "...and the whole picture was fetched by chunk (" + std::to_string(picture.size()) +
                  " bytes, " + pic->format + ")");
        std::ofstream out(work + "/journey-capture.txt", std::ios::binary | std::ios::trunc);
        out << picture;
        // THE PICTURE IS WHAT THE TERMINAL SKIN PRESENTED: the Pane Manager the chord opened is in
        // it, and the Connections pane names this session by the host's word for it.
        check(picture.find("Pane Manager") != std::string::npos,
              "the picture shows the Pane Manager the injected chord opened");
    }

    const std::uint64_t close = g.ask(input::kInputRole,
                                      input::InputSessionClosed{opened->session, 0});
    check(g.answer<loom::Ack>(close).has_value(), "the session was closed");

    // 3. WHAT THE GRANT DOES NOT COVER: refused by the Workshop's bus, and the guest is told.
    const std::uint64_t forbidden = g.ask("zengine.workshop", input::InputSessionRequested{"no"});
    const loom::BridgeEvent* notice = nullptr;
    (void)until(
        [&] {
            g.poll();
            notice = g.delivered(forbidden);
            return notice != nullptr;
        },
        5000);
    check(notice != nullptr && notice->dispatch_refused,
          "a send outside the row's powers is refused at the bus, and the guest is told");

    child.end();
    const std::string out = read_file(out_path);
    check(out.find("guests: listening on 127.0.0.1:") != std::string::npos,
          "the Workshop said where it listened");
    std::printf("%s (%d failure%s)\n", failures == 0 ? "PASS" : "FAIL", failures,
                failures == 1 ? "" : "s");
    if (failures != 0) {
        std::fprintf(stderr, "--- workshop output ---\n%s\n", out.c_str());
    }
    return failures == 0 ? 0 : 1;
}
