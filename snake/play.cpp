// zengine-snake — the playable host.
//
// The host is deliberately THIN: it owns the screen, the clock, and the pen —
// nothing else. It draws no board, knows no snake rules, performs no lifecycle
// itself, and — since the Input package arrived — reads no keys. Everything
// interesting crosses the bus:
//
//   keys      → the zengine-input weave (the Input package's producer, loaded
//               like any other weave) owns the platform's input side and
//               publishes KeyPressed/KeyReleased/Mouse*; the snake-controls
//               adapter turns steering keys into SnakeTurn; the host's own
//               operator weave accepts KeyPressed for the command keys. The
//               host merely root-sends PumpInput each lap so the producer has
//               execution time (the substrate has no timers yet).
//   time      → SnakeTick, root-sent to whoever holds the snake.world role
//   operating → zen.LoadWeave / zen.SwapWeave / zen.ReloadWeave / zen.ListLoaded,
//               sent by the operator weave whose grant reaches exactly the
//               Weave Manager, with every answer arriving back at it and
//               printed.
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

#include "input/vocabulary.hpp"

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
namespace input = zengine::input;
namespace scan = zengine::input::scan;

// ---- the screen --------------------------------------------------------------

/// The OUTPUT side of the terminal — alternate screen, hidden cursor, and (on
/// Windows) VT processing so the drawers' ANSI is real — restored whole on
/// destruction. The INPUT side (raw mode, key events) is deliberately not
/// here: it belongs to the Input weave now, engaged when it loads and restored
/// when it unloads. Degrades gracefully with no console (stdout redirected):
/// the game still runs; there is simply nothing to dress up.
class Screen {
public:
#if defined(_WIN32)
    Screen() {
        out_ = ::GetStdHandle(STD_OUTPUT_HANDLE);
        ok_ = out_ != INVALID_HANDLE_VALUE && ::GetConsoleMode(out_, &saved_out_) != 0;
        if (!ok_) {
            return;
        }
        ::SetConsoleMode(out_, saved_out_ | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        // Opt the console into UTF-8 output — held and restored like the mode.
        // The host's OWN strings are ASCII by rule (see status()), so the demo
        // never depends on this; the lever exists for what makers' weaves emit.
        saved_cp_ = ::GetConsoleOutputCP();
        ::SetConsoleOutputCP(CP_UTF8);
        enter();
    }
    ~Screen() {
        if (!ok_) {
            return;
        }
        leave();
        if (saved_cp_ != 0) {
            ::SetConsoleOutputCP(saved_cp_);
        }
        ::SetConsoleMode(out_, saved_out_);
    }
#else
    Screen() {
        ok_ = ::isatty(STDOUT_FILENO) == 1;
        if (ok_) {
            enter();
        }
    }
    ~Screen() {
        if (ok_) {
            leave();
        }
    }
#endif
    Screen(const Screen&) = delete;
    Screen& operator=(const Screen&) = delete;

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
    HANDLE out_ = nullptr;
    DWORD saved_out_ = 0;
    UINT saved_cp_ = 0;
#endif
    bool ok_ = false;
};

void status(const std::string& text) {
    // ASCII only in everything this host prints (and the drawers follow the
    // same rule): a demo's first impression must not depend on the console's
    // codepage or font. The screen layer still opts the console INTO UTF-8
    // (see Screen) so a maker's weave may emit what it likes.
    std::string out = "\x1b[1;1H\x1b[2K \x1b[36m[zen]\x1b[0m " + text +
                      "   \x1b[2m(wasd steer | 1 drawer | 2 score | 3 grow | r reload | "
                      "n new | l list | q quit)\x1b[0m";
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
    std::uint64_t next_corr = 1;
    int last_action = 0; ///< consumed by the loop after each pump
    bool quit = false;   ///< the q key, consumed by the loop
    bool block_drawer_in = false; ///< which skin holds the role right now
    bool world_is_v2 = false;     ///< which world generation holds it
    loom::WeaveId manager{};
    std::string dir; ///< where the host resolves loadable weaves (beside itself)

    std::string so(const char* stem) const;
};

/// The host's hand on the bus: it holds the reach (the manager, target-scoped
/// — the dangerous grant — plus the world's poke-reset door by role), issues
/// every lifecycle command, and hears every answer. Since the migration it is
/// also how the host LISTENS: the command keys arrive as published KeyPressed
/// messages from the Input weave — the host consumes input the same way snake
/// does, instead of reading the platform. Consumer obligation: answers are
/// matched against our own outstanding correlations; anything else is reported
/// as noise, acted on never.
struct OperatorState {
    std::int64_t answers = 0;
    ZEN_EXPOSE();
    ZEN_SHAPE(OperatorState, 1, ZEN_FIELD(answers));
};

class OperatorWeave
    : public loom::WeaveBase<OperatorWeave, OperatorState,
                             loom::Accept<loom::Result, loom::Ack, loom::Refused,
                                          input::KeyPressed>,
                             loom::Emit<loom::LoadWeave, loom::SwapWeave, loom::ReloadWeave,
                                        loom::ListLoaded, loom::PokeResetState>> {
public:
    explicit OperatorWeave(OperatorContext& ctx) : ctx_(&ctx) {}

    void on(const loom::Result& r, loom::Mail& mail) {
        answered(mail, "\x1b[32m->\x1b[0m " + r.value);
    }
    void on(const loom::Ack&, loom::Mail& mail) { answered(mail, "\x1b[32m-> done\x1b[0m"); }
    void on(const loom::Refused& r, loom::Mail& mail) {
        answered(mail, "\x1b[31m-> refused:\x1b[0m " + r.reason);
    }

    /// The command keys. Steering is not here — the snake-controls adapter
    /// owns it — and unknown keys are nobody's error.
    void on(const input::KeyPressed& k, loom::Mail& mail) {
        switch (k.scancode) {
        case scan::k1: {
            const char* stem =
                ctx_->block_drawer_in ? "snake-drawer-classic" : "snake-drawer-block";
            command(mail,
                    std::string("swap drawer -> ") +
                        (ctx_->block_drawer_in ? "classic" : "block"),
                    1, loom::SwapWeave{kDrawerRole, stem, ctx_->so(stem), /*graceful=*/false});
            break;
        }
        case scan::k2:
            command(mail, "load score weave (late)", 0,
                    loom::LoadWeave{"snake-score", ctx_->so("snake-score"), ""});
            break;
        case scan::k3:
            command(mail, "grow the world (graceful v1->v2)", 2,
                    loom::SwapWeave{kWorldRole, "snake-world-v2", ctx_->so("snake-world-v2"),
                                    /*graceful=*/true});
            break;
        case scan::kR: {
            const char* stem = ctx_->world_is_v2 ? "snake-world-v2" : "snake-world-v1";
            command(mail, "reload world in place (state rides the gate)", 0,
                    loom::ReloadWeave{stem, ctx_->so(stem)});
            break;
        }
        case scan::kL:
            command(mail, "loaded", 0, loom::ListLoaded{});
            break;
        case scan::kN: {
            const std::uint64_t corr = ctx_->next_corr++;
            ctx_->pending[corr] = Pending{"new game (poke reset)", 0};
            mail.send_to_role(kWorldRole, loom::PokeResetState{}, corr);
            status("new game (poke reset) ...");
            break;
        }
        case scan::kQ:
            ctx_->quit = true;
            break;
        case scan::kC:
            // V1 carries no modifiers, so Ctrl+C survives only as the backends'
            // dressed convenience name. Quitting on it is a courtesy the host
            // chooses to trust; the scancode stays the authority for the key.
            if (k.name == "Ctrl+C") {
                ctx_->quit = true;
            }
            break;
        default: break;
        }
    }

private:
    template <class Cmd>
    void command(loom::Mail& mail, std::string label, int action, const Cmd& cmd) {
        const std::uint64_t corr = ctx_->next_corr++;
        ctx_->pending[corr] = Pending{label, action};
        mail.send(ctx_->manager, cmd, corr);
        status(label + " ...");
    }

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

std::string OperatorContext::so(const char* stem) const { return dir + "/" + stem + kWeaveSuffix; }

} // namespace

int main() {
    // The honest line, before the alternate screen (it stays in scrollback):
    // this host isolates nothing anywhere, and on Windows it is the explicit
    // development/demo backend.
    std::printf("zengine-snake - containment: %s\n", loom::Kernel::containment_note());
    std::fflush(stdout);

    loom::Switchboard bus;
    loom::Kernel kernel(bus);
    const loom::WeaveId control = loom::mount_control(kernel, bus);
    const loom::WeaveId manager = loom::mount_manager(control, bus);

    OperatorContext ctx;
    ctx.manager = manager;
    ctx.dir = exe_dir();
    loom::Grant reach; // the manager (the dangerous grant, target-scoped)...
    reach.allow(loom::LoadWeave::zen_name, loom::LoadWeave::zen_version, manager);
    reach.allow(loom::SwapWeave::zen_name, loom::SwapWeave::zen_version, manager);
    reach.allow(loom::ReloadWeave::zen_name, loom::ReloadWeave::zen_version, manager);
    reach.allow(loom::ListLoaded::zen_name, loom::ListLoaded::zen_version, manager);
    // ...plus the world's reset door, by role (the n key), and nothing else.
    reach.allow_to_role(loom::PokeResetState::zen_name, loom::PokeResetState::zen_version,
                        kWorldRole);
    const loom::WeaveId op = loom::mount_granted<OperatorWeave>(bus, std::move(reach), ctx);

    // Startup commands are sent AS the operator (send_as stamps it and
    // authorizes against ITS grant — the host holds root but spends a real
    // capability), with the same in-flight bookkeeping its key commands use.
    const auto boot = [&](std::string label, const auto& cmd) {
        const std::uint64_t corr = ctx.next_corr++;
        ctx.pending[corr] = Pending{std::move(label), 0};
        bus.send_as(op, manager, loom::Message(loom::to_value(cmd), op, op, corr));
    };

    Screen screen;
    status("assembling the game ...");

    // Birth of the game: the same gesture as everything else — ask the steward.
    // The Input package's producer is loaded last, into its role: from that
    // moment the platform's keys belong to it, and everyone else just listens.
    boot("load world v1", loom::LoadWeave{"snake-world-v1", ctx.so("snake-world-v1"), kWorldRole});
    boot("load classic drawer",
         loom::LoadWeave{"snake-drawer-classic", ctx.so("snake-drawer-classic"), kDrawerRole});
    boot("load snake controls", loom::LoadWeave{"snake-controls", ctx.so("snake-controls"), ""});
    boot("load input weave",
         loom::LoadWeave{"zengine-input", ctx.so("zengine-input"), input::kInputRole});
    bus.pump();

    std::int64_t last_tick = monotonic_ms();

    while (!ctx.quit) {
        // -- input: give the producer its hands, deliver everything it said ----
        // (A missing producer refuses the send cleanly; the game runs keyless.)
        bus.send_to_role(input::kInputRole,
                         loom::Message(loom::to_value(input::PumpInput{})));
        bus.pump();

        // -- time -------------------------------------------------------------
        const std::int64_t now = monotonic_ms();
        if (now - last_tick >= 120) {
            last_tick = now;
            bus.send_to_role(kWorldRole, loom::Message(loom::to_value(SnakeTick{})));
        }

        bus.pump();

        // A successful moment flips the host's own trackers (applied after the
        // pump so the answer, not the wish, is what flips them).
        if (ctx.last_action == 1) {
            ctx.block_drawer_in = !ctx.block_drawer_in;
        } else if (ctx.last_action == 2) {
            ctx.world_is_v2 = true;
        }
        ctx.last_action = 0;

        nap_10ms();
    }
    return 0;
}
