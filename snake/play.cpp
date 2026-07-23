// zengine-snake — the playable host.
//
// The host is deliberately THIN: it owns the terminal, the clock, and the pen —
// nothing else. It draws no board, knows no snake rules, and performs no
// lifecycle itself. Everything interesting crosses the bus:
//
//   time      → SnakeTick,  root-sent to whoever holds the snake.world role
//   keys      → SnakeTurn,  the same way (the host is the input hardware)
//   operating → zen.LoadWeave / zen.SwapWeave / zen.ReloadWeave / zen.ListLoaded,
//               sent AS a mounted operator weave whose grant reaches exactly the
//               Weave Manager (send_as: the bus stamps the operator and checks
//               ITS grant — the host holds root but spends a real capability),
//               with every answer arriving back at the operator and printed.
//
// The three phase moments are three keys:
//   1  swap the drawer (hard swap — the drawing code is unloaded, dlclosed,
//      and different drawing code takes the role, mid-game)
//   2  load the score weave into the already-running game
//   3  grow the world: a GRACEFUL swap — the v1 world is asked to write its
//      letter, the v2 heir claims it by role and migrates the state
// plus r (reload the world in place: same shape, state transplanted through
// the gate — snapshot/revive made playable), n (new game via the substrate's
// own poke-reset door), l (list what is loaded), q (quit).
//
// Terminal rows: 1 = this status line, 2 = the score weave's line, 3+ = the
// drawer's canvas. The host never writes below row 2.

#include "vocabulary.hpp"

#include <zen/kernel/control.hpp>
#include <zen/kernel/kernel.hpp>
#include <zen/kernel/manager.hpp>
#include <zen/switchboard.hpp>
#include <zen/weave.hpp>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <termios.h>
#include <time.h>
#include <unistd.h>
#endif

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <map>
#include <string>

namespace {

using namespace zengine::snake;

// ---- terminal ----------------------------------------------------------------

/// Raw mode, alternate screen, hidden cursor — restored whole on destruction.
/// Both backends degrade gracefully with NO console (stdin/stdout redirected):
/// the game still runs and draws; there is simply no key input to read.
///
/// POSIX: VMIN=0/VTIME=0 makes read() a poll — the loop owns its own cadence.
/// Win32: ENABLE_EXTENDED_FLAGS alone on input kills line-input/echo/processed
/// input AND quick-edit (whose text-selection would otherwise freeze output);
/// ENABLE_VIRTUAL_TERMINAL_PROCESSING on output makes the drawers' ANSI real.
class RawTerminal {
public:
#if defined(_WIN32)
    RawTerminal() {
        in_ = ::GetStdHandle(STD_INPUT_HANDLE);
        out_ = ::GetStdHandle(STD_OUTPUT_HANDLE);
        ok_ = in_ != INVALID_HANDLE_VALUE && out_ != INVALID_HANDLE_VALUE &&
              ::GetConsoleMode(in_, &saved_in_) != 0 && ::GetConsoleMode(out_, &saved_out_) != 0;
        if (!ok_) {
            return;
        }
        ::SetConsoleMode(in_, ENABLE_EXTENDED_FLAGS);
        ::SetConsoleMode(out_, saved_out_ | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        enter();
    }
    ~RawTerminal() {
        if (!ok_) {
            return;
        }
        leave();
        ::SetConsoleMode(in_, saved_in_);
        ::SetConsoleMode(out_, saved_out_);
    }

    /// Drain pending console key-DOWN events into the same byte vocabulary the
    /// POSIX path reads: arrows arrive as virtual keys here (no escape
    /// sequences), so they translate straight to wasd.
    std::size_t read_keys(unsigned char* keys, std::size_t cap) const {
        if (!ok_) {
            return 0;
        }
        std::size_t n = 0;
        DWORD pending = 0;
        while (n < cap && ::GetNumberOfConsoleInputEvents(in_, &pending) != 0 && pending > 0) {
            INPUT_RECORD rec;
            DWORD got = 0;
            if (::ReadConsoleInputA(in_, &rec, 1, &got) == 0 || got == 0) {
                break;
            }
            if (rec.EventType != KEY_EVENT || !rec.Event.KeyEvent.bKeyDown) {
                continue;
            }
            unsigned char out = 0;
            switch (rec.Event.KeyEvent.wVirtualKeyCode) {
            case VK_UP: out = 'w'; break;
            case VK_DOWN: out = 's'; break;
            case VK_LEFT: out = 'a'; break;
            case VK_RIGHT: out = 'd'; break;
            default: out = static_cast<unsigned char>(rec.Event.KeyEvent.uChar.AsciiChar); break;
            }
            if (out != 0) {
                keys[n++] = out;
            }
        }
        return n;
    }
#else
    RawTerminal() {
        ok_ = ::tcgetattr(STDIN_FILENO, &saved_) == 0;
        if (!ok_) {
            return;
        }
        termios raw = saved_;
        raw.c_lflag &= static_cast<tcflag_t>(~(ICANON | ECHO | ISIG));
        raw.c_iflag &= static_cast<tcflag_t>(~(IXON | ICRNL));
        raw.c_cc[VMIN] = 0;
        raw.c_cc[VTIME] = 0;
        ::tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
        enter();
    }
    ~RawTerminal() {
        if (!ok_) {
            return;
        }
        leave();
        ::tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved_);
    }

    std::size_t read_keys(unsigned char* keys, std::size_t cap) const {
        const ssize_t n = ::read(STDIN_FILENO, keys, cap);
        return n > 0 ? static_cast<std::size_t>(n) : 0;
    }
#endif
    RawTerminal(const RawTerminal&) = delete;
    RawTerminal& operator=(const RawTerminal&) = delete;

private:
    static void enter() {
        std::fputs("\x1b[?1049h\x1b[?25l\x1b[2J", stdout);
        std::fflush(stdout);
    }
    static void leave() {
        std::fputs("\x1b[?25h\x1b[?1049l", stdout);
        std::fflush(stdout);
    }

#if defined(_WIN32)
    HANDLE in_ = nullptr;
    HANDLE out_ = nullptr;
    DWORD saved_in_ = 0;
    DWORD saved_out_ = 0;
#else
    termios saved_{};
#endif
    bool ok_ = false;
};

void status(const std::string& text) {
    std::string out = "\x1b[1;1H\x1b[2K \x1b[36m[zen]\x1b[0m " + text +
                      "   \x1b[2m(wasd steer · 1 drawer · 2 score · 3 grow · r reload · "
                      "n new · l list · q quit)\x1b[0m";
    std::fwrite(out.data(), 1, out.size(), stdout);
    std::fflush(stdout);
}

// ---- the operator ------------------------------------------------------------

/// What the host remembers about a command in flight, so an answer can be
/// reported in the words of the question, and so a success can flip the host's
/// own trackers (which drawer is in, which world is current).
struct Pending {
    std::string label;
    int action = 0; // 0 none, 1 drawer-swapped, 2 world-migrated
};

struct OperatorContext {
    std::map<std::uint64_t, Pending> pending;
    int last_action = 0; ///< consumed by the loop after each pump
};

/// The host's hand on the bus: every lifecycle command is sent AS this weave
/// (send_as stamps it and authorizes against its grant — reaching the Manager
/// is a real, target-scoped capability, not host magic), and every answer
/// lands here and is printed. Consumer obligation: answers are matched against
/// our own outstanding correlations; anything else is reported as noise, acted
/// on never.
struct OperatorState {
    std::int64_t answers = 0;
    ZEN_EXPOSE();
    ZEN_SHAPE(OperatorState, 1, ZEN_FIELD(answers));
};

class OperatorWeave
    : public loom::WeaveBase<OperatorWeave, OperatorState,
                             loom::Accept<loom::Result, loom::Ack, loom::Refused>, loom::Emit<>> {
public:
    explicit OperatorWeave(OperatorContext& ctx) : ctx_(&ctx) {}

    void on(const loom::Result& r, loom::Mail& mail) {
        answered(mail, "\x1b[32m→\x1b[0m " + r.value);
    }
    void on(const loom::Ack&, loom::Mail& mail) { answered(mail, "\x1b[32m→ done\x1b[0m"); }
    void on(const loom::Refused& r, loom::Mail& mail) {
        answered(mail, "\x1b[31m→ refused:\x1b[0m " + r.reason);
    }

private:
    void answered(loom::Mail& mail, const std::string& outcome) {
        ++state_.answers;
        const auto it = ctx_->pending.find(mail.correlation());
        if (it == ctx_->pending.end()) {
            status("unsolicited answer from weave " + std::to_string(mail.sender().value) +
                   " " + outcome);
            return;
        }
        if (outcome.find("refused") == std::string::npos) {
            ctx_->last_action = it->second.action;
        }
        status(it->second.label + " " + outcome);
        ctx_->pending.erase(it);
    }

    OperatorContext* ctx_;
};

std::int64_t monotonic_ms() {
#if defined(_WIN32)
    return static_cast<std::int64_t>(::GetTickCount64());
#else
    timespec ts{};
    ::clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
#endif
}

void nap_10ms() {
#if defined(_WIN32)
    ::Sleep(10);
#else
    timespec nap{0, 10 * 1000000};
    ::nanosleep(&nap, nullptr);
#endif
}

std::string exe_dir() {
#if defined(_WIN32)
    char buf[MAX_PATH];
    const DWORD n = ::GetModuleFileNameA(nullptr, buf, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) {
        return ".";
    }
    std::string path(buf, n);
    const std::size_t slash = path.find_last_of("\\/");
    return slash == std::string::npos ? "." : path.substr(0, slash);
#else
    char buf[4096];
    const ssize_t n = ::readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n <= 0) {
        return ".";
    }
    buf[n] = '\0';
    std::string path(buf);
    const std::size_t slash = path.find_last_of('/');
    return slash == std::string::npos ? "." : path.substr(0, slash);
#endif
}

/// The platform's loadable-weave suffix. One spelling, used for every stem.
constexpr const char* kWeaveSuffix =
#if defined(_WIN32)
    ".dll";
#else
    ".so";
#endif

} // namespace

int main() {
    // The honest line, before the alternate screen (it stays in scrollback):
    // this host isolates nothing anywhere, and on Windows it is the explicit
    // development/demo backend.
    std::printf("zengine-snake — containment: %s\n", loom::Kernel::containment_note());
    std::fflush(stdout);

    const std::string dir = exe_dir();
    const auto so = [&dir](const char* stem) { return dir + "/" + stem + kWeaveSuffix; };

    loom::Switchboard bus;
    loom::Kernel kernel(bus);
    const loom::WeaveId control = loom::mount_control(kernel, bus);
    const loom::WeaveId manager = loom::mount_manager(control, bus);

    OperatorContext ctx;
    loom::Grant reach; // the manager, and nothing else — the dangerous grant, target-scoped
    reach.allow(loom::LoadWeave::zen_name, loom::LoadWeave::zen_version, manager);
    reach.allow(loom::SwapWeave::zen_name, loom::SwapWeave::zen_version, manager);
    reach.allow(loom::ReloadWeave::zen_name, loom::ReloadWeave::zen_version, manager);
    reach.allow(loom::ListLoaded::zen_name, loom::ListLoaded::zen_version, manager);
    const loom::WeaveId op = loom::mount_granted<OperatorWeave>(bus, std::move(reach), ctx);

    std::uint64_t next_corr = 1;
    const auto command = [&](std::string label, int action, const auto& cmd) {
        const std::uint64_t corr = next_corr++;
        ctx.pending[corr] = Pending{label, action};
        bus.send_as(op, manager, loom::Message(loom::to_value(cmd), op, op, corr));
        status(label + " …");
    };
    const auto to_world = [&](const auto& msg, loom::WeaveId reply_to = {},
                              std::uint64_t corr = 0) {
        bus.send_to_role(kWorldRole,
                         loom::Message(loom::to_value(msg), loom::WeaveId{}, reply_to, corr));
    };

    RawTerminal term;
    status("assembling the game …");

    // Birth of the game: the same gesture as everything else — ask the steward.
    command("load world v1", 0, loom::LoadWeave{"snake-world-v1", so("snake-world-v1"), kWorldRole});
    command("load classic drawer", 0,
            loom::LoadWeave{"snake-drawer-classic", so("snake-drawer-classic"), kDrawerRole});
    bus.pump();

    bool block_drawer_in = false; // which skin holds the role right now
    bool world_is_v2 = false;     // which world generation holds it
    std::int64_t last_tick = monotonic_ms();
    bool running = true;

    while (running) {
        // -- input ------------------------------------------------------------
        unsigned char keys[64];
        const std::size_t n = term.read_keys(keys, sizeof(keys));
        for (std::size_t i = 0; i < n; ++i) {
            std::int64_t steer = -1;
            switch (keys[i]) {
            case 'w': steer = kUp; break;
            case 'd': steer = kRight; break;
            case 's': steer = kDown; break;
            case 'a': steer = kLeft; break;
            case '\x1b': // arrow keys: ESC [ A/B/C/D
                if (i + 2 < n && keys[i + 1] == '[') {
                    switch (keys[i + 2]) {
                    case 'A': steer = kUp; break;
                    case 'B': steer = kDown; break;
                    case 'C': steer = kRight; break;
                    case 'D': steer = kLeft; break;
                    default: break;
                    }
                    if (steer >= 0) {
                        i += 2;
                    }
                }
                break;
            case '1': {
                const char* stem = block_drawer_in ? "snake-drawer-classic" : "snake-drawer-block";
                command(std::string("swap drawer → ") + (block_drawer_in ? "classic" : "block"),
                        1, loom::SwapWeave{kDrawerRole, stem, so(stem), /*graceful=*/false});
                break;
            }
            case '2':
                command("load score weave (late)", 0,
                        loom::LoadWeave{"snake-score", so("snake-score"), ""});
                break;
            case '3':
                command("grow the world (graceful v1→v2)", 2,
                        loom::SwapWeave{kWorldRole, "snake-world-v2", so("snake-world-v2"),
                                        /*graceful=*/true});
                break;
            case 'r': {
                const char* stem = world_is_v2 ? "snake-world-v2" : "snake-world-v1";
                command("reload world in place (state rides the gate)", 0,
                        loom::ReloadWeave{stem, so(stem)});
                break;
            }
            case 'l':
                command("loaded", 0, loom::ListLoaded{});
                break;
            case 'n': {
                const std::uint64_t corr = next_corr++;
                ctx.pending[corr] = Pending{"new game (poke reset)", 0};
                to_world(loom::PokeResetState{}, op, corr);
                break;
            }
            case 'q':
            case '\x03': // Ctrl-C (ISIG is off; quitting is ours to do cleanly)
                running = false;
                break;
            default: break;
            }
            if (steer >= 0) {
                to_world(SnakeTurn{steer});
            }
        }

        // -- time -------------------------------------------------------------
        const std::int64_t now = monotonic_ms();
        if (now - last_tick >= 120) {
            last_tick = now;
            to_world(SnakeTick{});
        }

        bus.pump();

        // A successful moment flips the host's own trackers (applied after the
        // pump so the answer, not the wish, is what flips them).
        if (ctx.last_action == 1) {
            block_drawer_in = !block_drawer_in;
        } else if (ctx.last_action == 2) {
            world_is_v2 = true;
        }
        ctx.last_action = 0;

        nap_10ms();
    }
    return 0;
}
