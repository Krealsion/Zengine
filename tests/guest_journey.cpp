// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// THE GUEST JOURNEY ACROSS TWO REAL PROCESSES, the other half of what the guests suite proves in
// one: a REAL `zengine-workshop` launched headless with a guests file, and THIS process as the
// other host's participant -- admitted as the file's row, injecting the Pane Manager chord through
// an input session, fetching a picture of what the terminal Skin presents. It does not prove the
// SDL window's pixels (a machine with a display witnesses those) or the platform's input edge (an
// injected moment enters at the Input weave), and neither is inferred from a green here.

// And a weaver's own files: another Workshop, not isolated, whose roots and keymap stand in a
// folder named beyond ASCII, reads them, saves its session there and reopens it.

#include "input/vocabulary.hpp"
#include "surface/vocabulary.hpp"
#include "workshop/setup_control.hpp"

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
    std::fflush(stdout);
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

#if defined(_WIN32)
/// UTF-8 as UTF-16, whatever this program's own code page.
std::wstring wide(const std::string& utf8) {
    const int n = ::MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()),
                                        nullptr, 0);
    std::wstring out(static_cast<std::size_t>(n), L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), out.data(), n);
    return out;
}
#endif

struct Child {
#if defined(_WIN32)
    PROCESS_INFORMATION pi{};
    bool started = false;
    /// THE CHILD IS HANDED UTF-16, as a shell hands a program its command line, so the child's
    /// own code page alone decides how it reads what it was given.
    bool start(const std::string& exe, const std::vector<std::string>& args,
               const std::string& cwd, const std::string& out_path) {
        std::wstring line = L"\"" + wide(exe) + L"\"";
        for (const std::string& a : args) {
            line += L" \"" + wide(a) + L"\"";
        }
        SECURITY_ATTRIBUTES sa{};
        sa.nLength = sizeof(sa);
        sa.bInheritHandle = TRUE;
        HANDLE out = ::CreateFileW(wide(out_path).c_str(), GENERIC_WRITE, FILE_SHARE_READ, &sa,
                                   CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        STARTUPINFOW si{};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdOutput = out;
        si.hStdError = out;
        si.hStdInput = INVALID_HANDLE_VALUE;
        std::vector<wchar_t> buf(line.begin(), line.end());
        buf.push_back(L'\0');
        started = ::CreateProcessW(nullptr, buf.data(), nullptr, nullptr, TRUE,
                                   CREATE_NO_WINDOW, nullptr, wide(cwd).c_str(), &si, &pi) != 0;
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
    std::uint64_t ask(const char* far_role, const T& msg, bool settle = false) {
        const std::uint64_t c = next_correlation++;
        client->send_to_role(far_role, c, loom::serialize(loom::to_value(msg)), settle);
        client->flush();
        return c;
    }
    /// Wait for the host's `Settled` under `correlation`; true when it came.
    bool settled(std::uint64_t correlation, int timeout_ms = 10000) {
        return until(
            [&] {
                poll();
                for (const loom::BridgeEvent& e : events) {
                    if (e.kind == loom::BridgeEvent::Kind::Settled && e.correlation == correlation) {
                        return true;
                    }
                }
                return false;
            },
            timeout_ms);
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

/// The picture a capture answered, fetched by chunk; empty when any chunk did not come whole.
std::string fetch_picture(Guest& g, const surface::SurfaceCaptured& pic) {
    std::string picture;
    std::int64_t offset = 0;
    while (offset < pic.bytes) {
        const std::uint64_t c =
            g.ask(surface::kSkinRole, surface::SurfaceCaptureChunkRequested{pic.capture, offset});
        const std::optional<surface::SurfaceCaptureChunk> chunk =
            g.answer<surface::SurfaceCaptureChunk>(c);
        if (!chunk.has_value() || chunk->offset != offset || chunk->data.empty()) {
            return std::string();
        }
        picture.append(chunk->data.begin(), chunk->data.end());
        offset += static_cast<std::int64_t>(chunk->data.size());
    }
    return static_cast<std::int64_t>(picture.size()) == pic.bytes ? picture : std::string();
}

/// The path of a UTF-8 spelling, as the filesystem holds it, whatever the code page.
std::filesystem::path held(const std::string& utf8) {
    return std::filesystem::path(
        std::u8string(reinterpret_cast<const char8_t*>(utf8.data()), utf8.size()));
}

/// Set a variable for the children this program starts: on Windows as UTF-16, the way the
/// system keeps an environment.
void set_child_env(const char* name, const std::string& utf8) {
#if defined(_WIN32)
    ::SetEnvironmentVariableW(wide(name).c_str(), wide(utf8).c_str());
#else
    ::setenv(name, utf8.c_str(), 1);
#endif
}

} // namespace

// zengine-guest-journey <zengine-workshop exe> <headless load plan> <work dir>: exit 0 on PASS,
// 1 with the failed check named on stderr. The child is ended on the way out, whatever happened.
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

    // A picture BEFORE the chord, so the picture after it can be told apart from it.
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
    // SETTLED: the Workshop tells this session when everything the injection set in motion on
    // its bus -- the desktop's handling of the chord and every delivery it caused, the repaint
    // among them -- has been dispatched. The answer says only that the moments were published.
    const std::uint64_t inject = g.ask(input::kInputRole, batch, /*settle=*/true);
    const std::optional<input::InputInjected> injected = g.answer<input::InputInjected>(inject);
    check(injected.has_value() && injected->admitted == 2,
          "Ctrl+P was injected through the real Input weave (2 moments)");
    check(g.settled(inject),
          "...and the Workshop said what the injection set in motion has all been dispatched");

    const std::uint64_t inspect = g.ask("zengine.desktop", loom::DescribeAccepted{});
    const std::optional<loom::Value> shapes = g.answer_as(inspect, loom::accepted_shapes_schema());
    const std::size_t accepted = shapes.has_value() ? shapes->get("accepted")->as_list().size() : 0;
    check(shapes.has_value() && accepted > 0,
          "the desktop office described what it accepts (" + std::to_string(accepted) + " shapes)");

    // A picture asked for NOW, after the settlement: every paint the chord caused is behind it.
    const std::uint64_t capture = g.ask(surface::kSkinRole, surface::SurfaceCaptureRequested{});
    const std::optional<surface::SurfaceCaptured> pic = g.answer<surface::SurfaceCaptured>(capture, 10000);
    check(pic.has_value() && pic->ok && pic->frame > frame_before,
          "the Skin captured a frame the chord's paint produced (" +
              std::to_string(pic.has_value() ? pic->frame : -1) + " > " +
              std::to_string(frame_before) + ")");
    std::string picture;
    if (pic.has_value() && pic->ok) {
        picture = fetch_picture(g, *pic);
        check(!picture.empty(), "...and the whole picture was fetched by chunk (" +
                                    std::to_string(picture.size()) + " bytes, " + pic->format +
                                    ")");
        std::ofstream out(work + "/journey-capture.txt", std::ios::binary | std::ios::trunc);
        out << picture;
        // THE PICTURE IS WHAT THE TERMINAL SKIN PRESENTED: the Pane Manager the chord opened is in
        // it, and the Connections pane names this session by the host's word for it.
        check(picture.find("Pane Manager @zengine.desktop") != std::string::npos,
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
    // A FILE NAMING NO VERSION STILL MOUNTS, as version 1 on a weaver's host, and its row's losses
    // are said at launch beside the row.
    check(out.find("guests: the file is version 1, and this is a weaver's host") != std::string::npos,
          "a version-1 file mounted, and the launch said whose host this is");
    check(out.find("guests: 'agent' (version 1) does not reach here: the Builder's builds and loads") !=
              std::string::npos,
          "the launch said what the version-1 row's powers do not reach here");

    // 4. A WEAVER'S OWN FILES, IN A FOLDER NAMED BEYOND ASCII. Another Workshop, not isolated:
    //    its per-user roots and the keymap its command line names stand in a folder named with
    //    Western and Cyrillic letters and a space. It reads the keymap, writes its session there
    //    on quit, and the next launch reopens that session.
    std::printf("a weaver's own files, in a folder named beyond ASCII\n");
    const std::string folder = work + "/Zo\xC3\xAB \xD0\x96 Doe";
    std::error_code ec;
    std::filesystem::remove_all(held(folder), ec);
    std::filesystem::create_directories(held(folder), ec);
    bool made = !ec;
    {
        std::ofstream k(held(folder + "/keymap.json"), std::ios::trunc);
        k << R"({"zen":1,"schema":"WorkshopKeymap","version":2,"fields":{)"
          << R"("format":"zengine-workshop-keymap","format_version":"2","legend":"default",)"
          << R"("overrides":[{"action":"desktop.panes","gesture":"ctrl+q"}]}})";
        made = made && k.good();
    }
#if defined(_WIN32)
    set_child_env("APPDATA", folder + "/config");
    set_child_env("LOCALAPPDATA", folder + "/state");
#else
    set_child_env("XDG_CONFIG_HOME", folder + "/config");
    set_child_env("XDG_STATE_HOME", folder + "/state");
#endif
    const std::string own_guests = work + "/own-guests.json";
    const std::string own_port = work + "/own.port";
    {
        // A DEVELOPMENT HOST: this guest stands in for the weaver's own hand, so its `t` writes the
        // preference and its quit writes the session, as a weaver's would.
        std::ofstream o(own_guests, std::ios::trunc);
        o << R"({"version":"2","host":"development","listen":"127.0.0.1:0","port_file":"own.port",)"
          << R"("guests":[{"name":"weaver","credential":"open-sesame","may":["input","capture","demo"]}]})";
    }
    const std::vector<std::string> own_args = {"--load-plan", plan,    "--guests", own_guests,
                                               "--log",       "own.log", "--keymap",
                                               folder + "/keymap.json"};
    const std::string session_file = folder + "/state/zengine-workshop/workshop-session.json";
    const std::string prefs_file = folder + "/config/zengine-workshop/workshop-prefs.json";
    check(made && !std::filesystem::exists(held(session_file)),
          "the folder holds a keymap, and no session yet");
    // As far as an admitted guest, or a failed check saying where it stopped.
    const auto launch_own = [&](Child& own, Guest& w, const std::string& own_out) {
        std::remove(own_port.c_str());
        if (!check(own.start(workshop, own_args, work, own_out), "that Workshop started")) {
            return false;
        }
        std::uint16_t p = 0;
        (void)until(
            [&] {
                const std::string text = read_file(own_port);
                if (text.empty()) {
                    return !own.alive();
                }
                p = static_cast<std::uint16_t>(std::atoi(text.c_str()));
                return p != 0;
            },
            20000);
        if (!check(p != 0, "...and listens, its roots and its keymap read")) {
            std::fprintf(stderr, "--- workshop output ---\n%s\n", read_file(own_out).c_str());
            return false;
        }
        if (!check(w.connect(p, "weaver", "open-sesame"), "the weaver connects")) {
            return false;
        }
        (void)until(
            [&] {
                w.poll();
                return w.client->admitted() || w.client->denied() || w.client->disconnected();
            },
            5000);
        return check(w.client->admitted(), "...and is admitted");
    };
    const auto look = [](Guest& w) {
        const std::uint64_t asked = w.ask(surface::kSkinRole, surface::SurfaceCaptureRequested{});
        const std::optional<surface::SurfaceCaptured> taken =
            w.answer<surface::SurfaceCaptured>(asked, 10000);
        return taken.has_value() && taken->ok ? fetch_picture(w, *taken) : std::string();
    };
    // Asked to quit, it saves its session and ends. A pane still settling work refuses the quit
    // aloud and asks for it again, so a refused quit is asked again, ten times at most; its Ack
    // can go down with its link, so the ending is what is waited for.
    const auto quit = [](Child& own, Guest& w) {
        std::string answer = "nothing";
        for (int attempt = 0; attempt < 10 && own.alive(); ++attempt) {
            const std::uint64_t asked =
                w.ask("zengine.workshop", zengine::workshop::WorkshopQuitRequested{});
            bool refused = false;
            (void)until(
                [&] {
                    if (!own.alive()) {
                        return true;
                    }
                    w.poll();
                    const loom::BridgeEvent* said = w.delivered(asked);
                    if (said == nullptr) {
                        return false;
                    }
                    const loom::Unverified u = loom::parse(said->payload);
                    const loom::Admission no = loom::admit(u, loom::schema_of<loom::Refused>());
                    refused = no.ok();
                    answer = refused ? loom::from_value<loom::Refused>(no.value()).reason
                                     : u.claimed_name();
                    return refused;
                },
                30000);
            if (!refused) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
        if (own.alive()) {
            std::fprintf(stderr, "  the quit was answered: %s; Workshop is still running\n",
                         answer.c_str());
        }
        return !own.alive();
    };
    {
        Child own;
        Guest w;
        const std::string own_out = work + "/own.out";
        if (launch_own(own, w, own_out)) {
            const std::uint64_t asked = w.ask(input::kInputRole, input::InputSessionRequested{"own"});
            const std::optional<input::InputSessionOpened> session =
                w.answer<input::InputSessionOpened>(asked);
            // The legend spells the keymap in force: the chord is pressed once it says Ctrl+Q.
            std::string legend;
            check(until(
                      [&] {
                          legend = look(w);
                          return legend.find("^q panes") != std::string::npos;
                      },
                      10000),
                  "its legend says ^q panes: the keymap in that folder is in force");
            check(legend.find("Pane Manager @zengine.desktop") == std::string::npos,
                  "...and the Pane Manager is not on the desk yet");
            if (check(session.has_value(), "the Input weave opened a session")) {
                input::InjectInput chord;
                chord.session = session->session;
                input::InjectedEvent press;
                press.kind = "KeyPressed";
                press.scancode = input::scan::kQ;
                press.modifiers = input::mod::kCtrl;
                input::InjectedEvent release = press;
                release.kind = "KeyReleased";
                chord.events = {press, release};
                const std::uint64_t pressed = w.ask(input::kInputRole, chord, /*settle=*/true);
                check(w.answer<input::InputInjected>(pressed).has_value() && w.settled(pressed),
                      "Ctrl+Q was injected, and all it set in motion dispatched");
                const std::string seen = look(w);
                std::ofstream(work + "/own-capture.txt", std::ios::binary | std::ios::trunc) << seen;
                check(seen.find("Pane Manager @zengine.desktop") != std::string::npos,
                      "...and Ctrl+Q opens the Pane Manager");
                // Escape puts the Pane Manager down, and `t` hides the titles: a preference,
                // written under the configuration root in that folder.
                for (const std::int64_t key : {input::scan::kEscape, input::scan::kT}) {
                    input::InjectInput tap;
                    tap.session = session->session;
                    input::InjectedEvent key_down;
                    key_down.kind = "KeyPressed";
                    key_down.scancode = key;
                    input::InjectedEvent key_up = key_down;
                    key_up.kind = "KeyReleased";
                    tap.events = {key_down, key_up};
                    const std::uint64_t tapped = w.ask(input::kInputRole, tap, /*settle=*/true);
                    (void)w.answer<input::InputInjected>(tapped);
                    (void)w.settled(tapped);
                }
                check(until([&] { return std::filesystem::exists(held(prefs_file)); }, 5000),
                      "Escape, then t: the titles' preference is written under the configuration "
                      "root in that folder");
            }
            check(quit(own, w), "the weaver quit it, and it ended");
            check(std::filesystem::exists(held(session_file)),
                  "...writing its session under the state root in that folder");
            const std::string said = read_file(own_out);
            check(said.find("keymap: " + folder + "/keymap.json") != std::string::npos,
                  "its banner names the keymap in that folder, in UTF-8");
            check(said.find("last session: " + session_file) != std::string::npos &&
                      said.find("prefs: " + prefs_file) != std::string::npos,
                  "...and the session and the preferences under its two roots there");
        }
        own.end();
    }
    {
        // The restore's notice is drawn at startup, before a guest can ask for a picture: the
        // terminal Skin's own output is where it stands.
        Child again;
        Guest w;
        const std::string again_out = work + "/own-again.out";
        if (launch_own(again, w, again_out)) {
            check(until([&] {
                      return read_file(again_out).find("reopened your last desk") !=
                             std::string::npos;
                  }, 10000),
                  "the next launch reopened the desk it saved there");
            check(quit(again, w), "...and quit again");
        }
        again.end();
    }
    std::printf("%s (%d failure%s)\n", failures == 0 ? "PASS" : "FAIL", failures,
                failures == 1 ? "" : "s");
    if (failures != 0) {
        std::fprintf(stderr, "--- workshop output ---\n%s\n", out.c_str());
    }
    return failures == 0 ? 0 : 1;
}
